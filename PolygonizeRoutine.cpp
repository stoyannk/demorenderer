#include "precompiled.h"

#include "PolygonizeRoutine.h"

#include <Dx11/Rendering/ShaderManager.h>
#include <Dx11/Rendering/Camera.h>

#include <Dx11/Rendering/VertexTypes.h>

#include "Transvoxel.inl"
#include "Scene.h"
#include "GPUProfiling.h"

using namespace DirectX;

struct PerFramePolygonizeBuffer
{
	XMFLOAT4 Time;
};

PolygonizeRoutine::PolygonizeRoutine()
	: m_TimeSinceStart(0)
{}

bool PolygonizeRoutine::Initialize(Renderer* renderer, Camera* camera, Scene* scene, const DirectX::XMFLOAT4X4& projection)
{
	DxRenderingRoutine::Initialize(renderer);
		
	m_Camera = camera;
	m_Projection = projection;
	m_Scene = scene;

	if (!ReinitShading())
		return false;
	
	ShaderManager shaderManager(m_Renderer->GetDevice());
	
	if (!shaderManager.CreateStructuredBuffer(sizeof(RegularCellData), 16, m_CellDataBuffer.Receive(), nullptr, m_CellDataSRV.Receive()))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to create cell data buffer");
		return false;
	}
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();
	context->UpdateSubresource(m_CellDataBuffer.Get(), 0, nullptr, regularCellData, 0, 0);

	if (!shaderManager.CreateStructuredBuffer(sizeof(unsigned), 3072, m_VertexDataBuffer.Receive(), nullptr, m_VertexDataSRV.Receive()))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to create vertex data buffer");
		return false;
	}
	context->UpdateSubresource(m_VertexDataBuffer.Get(), 0, nullptr, regularVertexData, 0, 0);
	
	if (!shaderManager.CreateEasyConstantBuffer<PerFramePolygonizeBuffer>(m_PerFramePolygonizerBuffer.Receive(), false))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to create per-frame polygonizer buffer");
		return false;
	}

	const char* toCompile[] = {
		"InitCounters",
		"FinalizeCounters"
	};
	ReleaseGuard<ID3D11ComputeShader>* shaders[] = {
		&m_InitCS,
		&m_FinalizeCS
	};
	static_assert(_countof(toCompile) == _countof(shaders), "Count of funcs and receiving shaders must coincide");

	for (auto i = 0; i < _countof(toCompile); ++i) {
		ReleaseGuard<ID3DBlob> compiled;
		if (!shaderManager.CompileShaderFromFile("../Shaders/PolygonizerHelpers.hlsl",
			toCompile[i],
			"cs_5_0",
			compiled.Receive()))
		{
			SLOG(Sev_Error, Fac_Rendering, "Unable to compile polygonizer func ", toCompile[i]);
			return false;
		}
		shaders[i]->Set(shaderManager.CreateComputeShader(compiled.Get(), nullptr));
		if (!shaders[i]->Get()) {
			SLOG(Sev_Error, Fac_Rendering, "Unable to create polygonizer shader!");
			return false;
		}
	}

	auto& textureManager = m_Renderer->GetTextureManager();
	m_RandomTexture = textureManager.Load("../media/textures/random.png");
	if (!m_RandomTexture)
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load random texture!");
		return false;
	}

	m_LinearSampler.Set(textureManager.MakeSampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR));

	return true;
}

ID3D11ComputeShader* PolygonizeRoutine::GetShader(const std::string& generator)
{
	auto found = m_Shaders.find(generator);

	if (found != m_Shaders.end()) {
		return found->second.Get();
	}

	// Recompile
	ShaderManager shaderManager(m_Renderer->GetDevice());
	ReleaseGuard<ID3DBlob> compiled;
	if (!shaderManager.CompileShaderFromFile("../Shaders/Polygonizer.hlsl",
		"PolygonizerCS",
		"cs_5_0",
		compiled.Receive(),
		generator))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to compile polygonizer shader with generator ", generator);
		return nullptr;
	}
	auto shader = shaderManager.CreateComputeShader(compiled.Get(), nullptr);
	if (!shader) {
		SLOG(Sev_Error, Fac_Rendering, "Unable to create polygonizer shader with generator ", generator);
		return nullptr;
	}
	m_Shaders.insert(std::make_pair(generator, ReleaseGuard<ID3D11ComputeShader>(shader)));

	return shader;
}

bool PolygonizeRoutine::Render(float deltaTime)
{
	m_TimeSinceStart += deltaTime;

	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();

#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.PolygonizeBegin[gGPUProfiling.CurrentIndex].Get());
#endif

	const auto& genMeshes = m_Scene->GetMeshesToGenerate();
	if (genMeshes.size()) {
		PerFramePolygonizeBuffer pfb;
		pfb.Time.x = m_TimeSinceStart;
		pfb.Time.y = deltaTime;
		context->UpdateSubresource(m_PerFramePolygonizerBuffer.Get(), 0, nullptr, &pfb, 0, 0);

		context->CSSetConstantBuffers(1, 1, m_PerFramePolygonizerBuffer.GetConstPP());
		context->CSSetSamplers(0, 1, m_LinearSampler.GetConstPP());

		for (auto it = genMeshes.cbegin(); it != genMeshes.cend(); ++it)
		{
			auto& mesh = (*it);
			auto shader = GetShader(mesh->GetGeneratingFunction());

			if (!shader)
				continue;

			ID3D11UnorderedAccessView* uavs[] = { mesh->GetVertexBufferUAV(), mesh->GetIndexBufferUAV(), mesh->GetIndirectBufferUAV(), mesh->GetCountersBufferUAV() };
			ID3D11ShaderResourceView* cellSRV[] = { m_CellDataSRV.Get(), m_VertexDataSRV.Get(), m_RandomTexture->GetSHRV() };
			context->CSSetUnorderedAccessViews(0, _countof(uavs), uavs, nullptr);
			context->CSSetShaderResources(0, _countof(cellSRV), cellSRV);

			// Init
			context->CSSetShader(m_InitCS.Get(), nullptr, 0);
			context->Dispatch(1, 1, 1);

			// Execute
			context->CSSetShader(shader, nullptr, 0);
			const auto& dispatch = mesh->GetDispatch();
			context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

			// Finalize
			context->CSSetShader(m_FinalizeCS.Get(), nullptr, 0);
			context->Dispatch(1, 1, 1);
		}
		ID3D11UnorderedAccessView* emptyUAV[] = { nullptr, nullptr, nullptr, nullptr };
		context->CSSetUnorderedAccessViews(0, _countof(emptyUAV), emptyUAV, nullptr);

		m_Scene->DidRegenerateAllMeshes();
	}

#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.PolygonizeEnd[gGPUProfiling.CurrentIndex].Get());
#endif
	return true;
}
