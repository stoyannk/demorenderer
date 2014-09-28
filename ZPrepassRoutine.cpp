#include "precompiled.h"

#include "ZPrepassRoutine.h"
#include "Scene.h"
#include "ConstBufferTypes.h"
#include "GPUProfiling.h"

#include <Dx11/Rendering/VertexTypes.h>
#include <Dx11/Rendering/Mesh.h>
#include <Dx11/Rendering/Camera.h>
#include <Dx11/Rendering/MaterialShaderManager.h>

static const char* SHADER_NAME = "..\\Shaders\\ForwardDraw.hlsl";
static const char* SHADER_NAME_PROCEDURAL = "..\\Shaders\\ForwardDrawProcedural.hlsl";
static const char* VS_ENTRY = "VS";
static const char* VS_ENTRY_PROCEDURAL = "VS_Gen";

using namespace DirectX;

ZPrepassRoutine::ZPrepassRoutine()
{}

ZPrepassRoutine::~ZPrepassRoutine()
{}
 
bool ZPrepassRoutine::Initialize(Renderer* renderer, Camera* camera, Scene* scene, const XMFLOAT4X4& projection)
{
	DxRenderingRoutine::Initialize(renderer);

	m_Camera = camera;
	m_Scene = scene;
	m_Projection = projection;

	m_ShaderManager.reset(new MaterialShaderManager(m_Renderer->GetDevice()));

	if(!m_ShaderManager->CreateEasyConstantBuffer<PerSubsetBuffer>(m_PerSubsetBuffer.Receive()))
	{
		STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to create per-subset buffer"));
		return false;
	}

	if(!ReinitShading())
	{
		return false;
	}

	return true;
}

bool ZPrepassRoutine::ReinitShading()
{
	// Create a vs for the sake of the input layout - material variations of the shader DO NOT change the IL at this point
	{
		ID3DBlob* shader = nullptr;
		if (!m_ShaderManager->GetBlob(SHADER_NAME, VS_ENTRY, "vs_5_0", Material(), &shader))
		{
			return false;
		}
		ReleaseGuard<ID3DBlob> shaderGuard(shader);
		// Create the input layout
		auto hr = m_Renderer->GetDevice()->CreateInputLayout(StandardVertexLayout, ARRAYSIZE(StandardVertexLayout), shaderGuard.Get()->GetBufferPointer(),
			shaderGuard.Get()->GetBufferSize(), m_VertexLayout.Receive());
		if (FAILED(hr))
		{
			STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to create input layout"));
			return false;
		}
	}
	ShaderManager shaderManager(m_Renderer->GetDevice());
	{
		ReleaseGuard<ID3DBlob> shaderGuard;
		if (!shaderManager.CompileShaderFromFile("../Shaders/ForwardDrawProcedural.hlsl"
			, "VS_Gen"
			, "vs_5_0"
			, shaderGuard.Receive()))
		{
			return false;
		}

		m_VertexShaderProcedural.Set(shaderManager.CreateVertexShader(shaderGuard.Get(), nullptr));
		if (!m_VertexShaderProcedural.Get())
			return false;

		auto hr = m_Renderer->GetDevice()->CreateInputLayout(
			PositionNormalTextIndsVertexLayout,
			ARRAYSIZE(PositionNormalTextIndsVertexLayout),
			shaderGuard.Get()->GetBufferPointer(),
			shaderGuard.Get()->GetBufferSize(),
			m_VertexLayoutProcedural.Receive());
		if (FAILED(hr))
		{
			STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to create input layout - procedural"));
			return false;
		}
	}
	
	return true;
}

bool ZPrepassRoutine::Render(float deltaTime)
{	
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();
	#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.ZPrepassBegin[gGPUProfiling.CurrentIndex].Get());
	#endif

	context->OMSetRenderTargets(0, nullptr, m_Renderer->GetBackDepthStencilView());
	const float blFactors[] = {1, 1, 1, 1};
	context->OMSetBlendState(m_Renderer->GetStateHolder().GetBlendState(StateHolder::BLST_NoWrite), blFactors, 0xFFFFFFFF);

	context->IASetInputLayout(m_VertexLayout.Get());

	// Set shaders
	ID3D11Buffer* cbs[] = {m_Renderer->GetPerFrameConstantBuffer(), m_PerSubsetBuffer.Get()};
	context->VSSetConstantBuffers(0, 2, cbs);

    UINT stride = sizeof(StandardVertex);
    UINT offset = 0;

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto& entitiesToDraw = m_Scene->GetEntitiesForMainCamera();
	for (auto entity = entitiesToDraw.cbegin(); entity != entitiesToDraw.cend(); ++entity)
	{
		const auto& toDraw = entity->Subsets;
		ID3D11Buffer* buffer = entity->Geometry->GetVertexBuffer();
		context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

		ID3D11VertexShader* vs = nullptr;
		context->PSSetShader(nullptr, nullptr, 0);
		for (size_t i = 0; i < toDraw.size(); ++i)
		{
			SubsetPtr subset = toDraw[i];

			if (subset->GetMaterial().HasProperty(MP_AlphaMask)) // skip alpha masked subsets
				continue;

			vs = m_ShaderManager->GetVertexShader(SHADER_NAME, VS_ENTRY, "vs_5_0", Material());
			context->VSSetShader(vs, nullptr, 0);

			PerSubsetBuffer psbuffer;
			psbuffer.World = XMMatrixTranspose(entity->WorldMatrix);
			context->UpdateSubresource(m_PerSubsetBuffer.Get(), 0, nullptr, &psbuffer, 0, 0);

			// Set index buffer
			ID3D11Buffer* indexBuffer = subset->GetIndexBuffer();
			context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

			context->DrawIndexed(subset->GetIndicesCount(), 0, 0);
		}
	}

	const auto& genMeshes = m_Scene->GetProceduralEntitiesForMainCamera();
	if (genMeshes.size()) {
		context->RSSetState(m_Renderer->GetStateHolder().GetRasterState(StateHolder::RST_FrontCCW));
		context->IASetInputLayout(m_VertexLayoutProcedural.Get());
		context->VSSetShader(m_VertexShaderProcedural.Get(), nullptr, 0);
		context->PSSetShader(nullptr, nullptr, 0);

		UINT stride = sizeof(PositionNormalTextIndsVertex);
		ID3D11Buffer* vb[1];
		for (auto it = genMeshes.cbegin(); it != genMeshes.cend(); ++it)
		{
			PerSubsetBuffer psbuffer;
			psbuffer.World = XMMatrixTranspose(it->WorldMatrix);
			context->UpdateSubresource(m_PerSubsetBuffer.Get(), 0, nullptr, &psbuffer, 0, 0);

			vb[0] = it->Geometry->GetVertexBuffer();
			context->IASetVertexBuffers(0, 1, vb, &stride, &offset);
			context->IASetIndexBuffer(it->Geometry->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexedInstancedIndirect(it->Geometry->GetIndirectBuffer(), 0);
		}
		context->RSSetState(m_Renderer->GetStateHolder().GetRasterState(StateHolder::RST_FrontCW));
	}

	context->OMSetBlendState(nullptr, blFactors, 0xFFFFFFFF);
	
#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.ZPrepassEnd[gGPUProfiling.CurrentIndex].Get());
#endif

	return true;
}

