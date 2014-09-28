#include "precompiled.h"

#include "DrawRoutine.h"
#include "Scene.h"
#include "ConstBufferTypes.h"

#include "SharedRenderResources.h"
#include "GPUProfiling.h"

#include <Dx11/Rendering/ShaderManager.h>
#include <Dx11/Rendering/Camera.h>
#include <Dx11/Rendering/VertexTypes.h>
#include <Dx11/Rendering/Mesh.h>
#include <Dx11/Rendering/GeneratedMesh.h>
#include <Dx11/Rendering/MaterialShaderManager.h>
#ifndef MINIMAL_SIZE
#include <Dx11/Rendering/MeshLoader.h>
#endif
//#define DEBUG_LIGHTS

using namespace DirectX;

namespace {
	static const char* SHADER_NAME = "..\\Shaders\\ForwardDraw.hlsl";
	static const char* VS_ENTRY = "VS";
	static const char* PS_ENTRY = "PS";

	static const float DEFAULT_SPECULAR_POWER = 10;

	struct GlobalPropertiesBuffer
	{
		XMFLOAT4 CameraPosition;
		XMFLOAT4 GlobalLightDirInt;
		XMFLOAT4 GlobalLightColor;
	};
}

DrawRoutine::DrawRoutine()
	: m_Wireframe(false)
{}

DrawRoutine::~DrawRoutine()
{}
 
bool DrawRoutine::Initialize(Renderer* renderer, Camera* camera, Scene* scene, const XMFLOAT4X4& projection)
{
	DxRenderingRoutine::Initialize(renderer);

	m_Camera = camera;
	m_Scene = scene;
	m_Projection = projection;
	
	m_ShaderManager.reset(new MaterialShaderManager(m_Renderer->GetDevice()));

	if(!ReinitShading())
	{
		return false;
	}

	ShaderManager shaderManager(m_Renderer->GetDevice());
	// Create PerSubset buffer
	if(!shaderManager.CreateEasyConstantBuffer<PerSubsetBuffer>(m_PerSubsetBuffer.Receive(), true))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to create per-subset buffer");
		return false;
	}
	if (!shaderManager.CreateEasyConstantBuffer<GlobalPropertiesBuffer>(m_GlobalPropsBuffer.Receive(), true))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to create global properties buffer");
		return false;
	}
		
	auto& texManager = m_Renderer->GetTextureManager();
	// Create samplers
	m_LinearSampler.Set(texManager.MakeSampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR));
	if(!m_LinearSampler.Get()) return false;

	m_PointSampler.Set(texManager.MakeSampler(D3D11_FILTER_MIN_MAG_MIP_POINT));
	if(!m_PointSampler.Get()) return false;

#ifndef MINIMAL_SIZE
	// Mesh for lights
	std::string errors;
	m_LightMesh.reset(MeshLoader::LoadMesh(static_cast<DxRenderer*>(m_Renderer), "..\\..\\media\\cube.obj", errors));
	if (!m_LightMesh.get())
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load cube mesh: ", errors);
		return false;
	}
#endif

	return true;
}

bool DrawRoutine::ReinitShading()
{
	// Create a vs for the sake of the input layout - material variations of the shader DO NOT change the IL at this point
	ID3DBlob* shader = nullptr;
	if(!m_ShaderManager->GetBlob(SHADER_NAME, VS_ENTRY, "vs_5_0", Material(), &shader))
	{
		return false;
	}
	ReleaseGuard<ID3DBlob> shaderGuard(shader);
	// Define the input layout
    HRESULT hr = S_OK;
    // Create the input layout
	ID3D11InputLayout* vertexLayout = nullptr;
	hr = m_Renderer->GetDevice()->CreateInputLayout(StandardVertexLayout, ARRAYSIZE(StandardVertexLayout), shaderGuard.Get()->GetBufferPointer(),
		shaderGuard.Get()->GetBufferSize(), m_VertexLayout.Receive());
	if(FAILED(hr))
	{
		STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to create input layout"));
		return false;
	}

	ShaderManager shaderManager(m_Renderer->GetDevice());
	{
		ShaderManager::CompilationOutput compilationResult;
		// Create shaders
		if (!shaderManager.CompileShaderDuo("../Shaders/DrawLight.hlsl"
			, "VSLight"
			, "vs_5_0"
			, "PSLight"
			, "ps_5_0"
			, compilationResult))
		{
			compilationResult.ReleaseAll();
			return false;
		}

		ReleaseGuard<ID3DBlob> vsGuard(compilationResult.vsBlob);
		ReleaseGuard<ID3DBlob> psGuard(compilationResult.psBlob);
		m_VertexShaderLights.Set(compilationResult.vertexShader);
		m_PixelShaderLights.Set(compilationResult.pixelShader);
	}
	// Shaders for procedural content
	{
		ShaderManager::CompilationOutput compilationResult;
		if (!shaderManager.CompileShaderDuo("../Shaders/ForwardDrawProcedural.hlsl"
			, "VS_Gen"
			, "vs_5_0"
			, "PS_Gen"
			, "ps_5_0"
			, compilationResult))
		{
			compilationResult.ReleaseAll();
			return false;
		}

		ReleaseGuard<ID3DBlob> vsGuard(compilationResult.vsBlob);
		ReleaseGuard<ID3DBlob> psGuard(compilationResult.psBlob);
		m_VertexShaderProcedural.Set(compilationResult.vertexShader);
		m_PixelShaderProcedural.Set(compilationResult.pixelShader);

		hr = m_Renderer->GetDevice()->CreateInputLayout(
			PositionNormalTextIndsVertexLayout,
			ARRAYSIZE(PositionNormalTextIndsVertexLayout),
			vsGuard.Get()->GetBufferPointer(),
			vsGuard.Get()->GetBufferSize(),
			m_VertexLayoutProcedural.Receive());
		if (FAILED(hr))
		{
			STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to create input layout for procedural meshes"));
			return false;
		}
	}

	return true;
}

bool DrawRoutine::Render(float deltaTime)
{
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();
#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.DrawBegin[gGPUProfiling.CurrentIndex].Get());
#endif

	m_Renderer->SetViewport(context);

	ID3D11RenderTargetView* rts[] = {m_Renderer->GetBackBufferView()};
	context->OMSetRenderTargets(1, rts, m_Renderer->GetBackDepthStencilView());
	
	context->IASetInputLayout(m_VertexLayout.Get());

	context->OMSetDepthStencilState(m_Renderer->GetStateHolder().GetDepthState(StateHolder::DSST_NoWriteLE), 0);
	const float blFactors[] = { 1, 1, 1, 1 };
	context->OMSetBlendState(m_Renderer->GetStateHolder().GetBlendState(StateHolder::BLST_WrOneAddAtoCov), blFactors, 0xFFFFFFFF);

	D3D11_MAPPED_SUBRESOURCE mappedCB = { 0 };
	if (FAILED(context->Map(m_GlobalPropsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to map global props constant buffer");
	}
	GlobalPropertiesBuffer* globBuffer = static_cast<GlobalPropertiesBuffer*>(mappedCB.pData);
	globBuffer->CameraPosition = XMFLOAT4(m_Camera->GetPos().x,
		m_Camera->GetPos().y,
		m_Camera->GetPos().z,
		0);
	const auto& sun = m_Scene->GetSun();
	globBuffer->GlobalLightDirInt = sun.Properties;
	globBuffer->GlobalLightColor = XMFLOAT4(sun.Color.x, sun.Color.y, sun.Color.z, 0.f);
	context->Unmap(m_GlobalPropsBuffer.Get(), 0);

	// Set shaders
	ID3D11Buffer* cbs[] = { m_Renderer->GetPerFrameConstantBuffer(), m_PerSubsetBuffer.Get(), m_GlobalPropsBuffer.Get() };
	context->VSSetConstantBuffers(0, _countof(cbs), cbs);
	context->PSSetConstantBuffers(0, _countof(cbs), cbs);

    UINT stride = sizeof(StandardVertex);
    UINT offset = 0;
	ID3D11ShaderResourceView* textures[7];
	textures[4] = gSharedRenderResources->LightsCulledSRV.Get();
	textures[5] = gSharedRenderResources->LightsCulledCountSRV.Get();
	textures[6] = gSharedRenderResources->PointLightsSRV.Get();

	context->RSSetState(m_Renderer->GetStateHolder().GetRasterState(m_Wireframe ? StateHolder::RST_FrontCWWire : StateHolder::RST_FrontCW));
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set samplers
	context->PSSetSamplers(0, 1, m_LinearSampler.GetConstPP());
	context->PSSetSamplers(1, 1, m_PointSampler.GetConstPP());

	auto& entitiesToDraw = m_Scene->GetEntitiesForMainCamera();
	for (auto entity = entitiesToDraw.begin(); entity != entitiesToDraw.end(); ++entity)
	{
		auto& toDraw = entity->Subsets;

		ID3D11Buffer* buffer = entity->Geometry->GetVertexBuffer();
		context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

		unsigned lastMaterialProps = unsigned(~0);
		ID3D11VertexShader* vs = nullptr;
		ID3D11PixelShader* ps = nullptr;

		auto firstAlphaSubset = std::partition(toDraw.begin(), toDraw.end(), [](SubsetPtr& ptr) {
			return !ptr->GetMaterial().HasProperty(MP_AlphaMask);
		});

		for (auto it = toDraw.cbegin(); it != toDraw.cend(); ++it)
		{
			if (it == firstAlphaSubset) {
				context->OMSetDepthStencilState(nullptr, 0);
			}

			const SubsetPtr& subset = *it;

			const Material& material = subset->GetMaterial();
			if (lastMaterialProps != material.GetProperties())
			{
				vs = m_ShaderManager->GetVertexShader(SHADER_NAME, VS_ENTRY, "vs_5_0", subset->GetMaterial());
				context->VSSetShader(vs, nullptr, 0);
				ps = m_ShaderManager->GetPixelShader(SHADER_NAME, PS_ENTRY, "ps_5_0", subset->GetMaterial());
				context->PSSetShader(ps, nullptr, 0);

				lastMaterialProps = material.GetProperties();
			}

			TexturePtr texture = material.GetDiffuse();
			textures[0] = texture.get() ? texture->GetSHRV() : nullptr;
			TexturePtr normals = material.GetNormalMap();
			textures[1] = normals.get() ? normals->GetSHRV() : nullptr;
			TexturePtr alphaMask = material.GetAlphaMask();
			textures[2] = alphaMask.get() ? alphaMask->GetSHRV() : nullptr;
			TexturePtr specular = material.GetSpecularMap();
			textures[3] = specular.get() ? specular->GetSHRV() : nullptr;

			context->PSSetShaderResources(0, _countof(textures), textures);

			if (FAILED(context->Map(m_PerSubsetBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
			{
				STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to map subset constant buffer"));
				continue;
			}
			PerSubsetBuffer* psbuffer = static_cast<PerSubsetBuffer*>(mappedCB.pData);
			psbuffer->World = XMMatrixTranspose(entity->WorldMatrix);
			psbuffer->Properties.x = DEFAULT_SPECULAR_POWER;
			if (material.HasProperty(MP_SpecularPower)) {
				psbuffer->Properties.x = material.GetSpecularPower();
			}
			context->Unmap(m_PerSubsetBuffer.Get(), 0);

			// Set index buffer
			ID3D11Buffer* indexBuffer = subset->GetIndexBuffer();
			context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

			context->DrawIndexed(subset->GetIndicesCount(), 0, 0);
		}
	}
	// Draw generated meshes
	const auto& genMeshes = m_Scene->GetProceduralEntitiesForMainCamera();
	if (genMeshes.size()) {
		context->OMSetDepthStencilState(m_Renderer->GetStateHolder().GetDepthState(StateHolder::DSST_NoWriteLE), 0);

		ReleaseGuard<ID3D11RasterizerState> rsState;
		context->RSGetState(rsState.Receive());

		context->IASetInputLayout(m_VertexLayoutProcedural.Get());
		context->VSSetShader(m_VertexShaderProcedural.Get(), nullptr, 0);
		context->PSSetShader(m_PixelShaderProcedural.Get(), nullptr, 0);
		context->RSSetState(m_Renderer->GetStateHolder().GetRasterState(m_Wireframe ? StateHolder::RST_FrontCCWWire : StateHolder::RST_FrontCCW));

		// set light params
		context->PSSetShaderResources(4, 3, textures + 4);

		UINT stride = sizeof(PositionNormalTextIndsVertex);
		ID3D11Buffer* vb[1];
		for (auto it = genMeshes.cbegin(); it != genMeshes.cend(); ++it)
		{
			GeneratedMesh* geometry = it->Geometry;
			const Material& material = geometry->GetMaterial();

			if (FAILED(context->Map(m_PerSubsetBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
			{
				SLOG(Sev_Error, Fac_Rendering, "Unable to map subset constant buffer");
			}
			PerSubsetBuffer* psbuffer = static_cast<PerSubsetBuffer*>(mappedCB.pData);
			psbuffer->World = XMMatrixTranspose(it->WorldMatrix);
			psbuffer->Properties.x = DEFAULT_SPECULAR_POWER;
			if (material.HasProperty(MP_SpecularPower)) {
				psbuffer->Properties.x = material.GetSpecularPower();
			}
			context->Unmap(m_PerSubsetBuffer.Get(), 0);

			TexturePtr texture = material.GetDiffuse();
			textures[0] = texture.get() ? texture->GetSHRV() : nullptr;
			TexturePtr normals = material.GetNormalMap();
			textures[1] = normals.get() ? normals->GetSHRV() : nullptr;
			context->PSSetShaderResources(0, 2, textures);

			vb[0] = geometry->GetVertexBuffer();
			context->IASetVertexBuffers(0, 1, vb, &stride, &offset);
			context->IASetIndexBuffer(geometry->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexedInstancedIndirect(geometry->GetIndirectBuffer(), 0);
		}
		ID3D11Buffer* nullBuffs[] = { nullptr };
		context->IASetVertexBuffers(0, _countof(nullBuffs), nullBuffs, &stride, &offset);
		context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);

		context->RSSetState(rsState.Get());
	}

	::memset(textures, 0, sizeof(textures));
	context->PSSetShaderResources(0, _countof(textures), textures);

	context->OMSetDepthStencilState(nullptr, 0);
	context->OMSetBlendState(nullptr, blFactors, 0xFFFFFFFF);
	context->RSSetState(m_Renderer->GetStateHolder().GetRasterState(StateHolder::RST_FrontCW));

#ifdef DEBUG_LIGHTS
	DrawLights();
#endif

#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.DrawEnd[gGPUProfiling.CurrentIndex].Get());
#endif

	return true;
}

#ifndef MINIMAL_SIZE
void DrawRoutine::DrawLights()
{
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();

	const auto& subset = m_LightMesh->GetSubset(0);
	ID3D11Buffer* indexBuffer = subset->GetIndexBuffer();
	context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	ID3D11RenderTargetView* rts[] = { m_Renderer->GetBackBufferView() };
	context->OMSetRenderTargets(1, rts, m_Renderer->GetBackDepthStencilView());
	context->IASetInputLayout(m_VertexLayout.Get());
	
	// Set vertex buffer
	UINT stride = sizeof(StandardVertex);
	UINT offset = 0;
	// Set primitive topology
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11Buffer* buffer = m_LightMesh->GetVertexBuffer();
	context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

	const auto& lights = m_Scene->GetLights();

	for (size_t i = 0; i < lights.size(); ++i)
	{
		D3D11_MAPPED_SUBRESOURCE mappedCB = { 0 };
		if (FAILED(context->Map(m_PerSubsetBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			SLOG(Sev_Error, Fac_Rendering, "Unable to map subset constant buffer");
			continue;
		}
		PerSubsetBuffer* psbuffer = static_cast<PerSubsetBuffer*>(mappedCB.pData);
		XMMATRIX world = XMMatrixTranslation(-0.5f, -0.5f, -0.5f)
			* XMMatrixScaling(lights[i].Radius * 2, lights[i].Radius * 2, lights[i].Radius * 2)
			* XMMatrixTranslation(lights[i].Position.x, lights[i].Position.y, lights[i].Position.z);
		psbuffer->World = XMMatrixTranspose(world);
		context->Unmap(m_PerSubsetBuffer.Get(), 0);

		context->VSSetShader(m_VertexShaderLights.Get(), nullptr, 0);
		context->PSSetShader(m_PixelShaderLights.Get(), nullptr, 0);

		// Set shaders
		ID3D11Buffer* cbs[] = { m_Renderer->GetPerFrameConstantBuffer(), m_PerSubsetBuffer.Get() };
		context->VSSetConstantBuffers(0, 2, cbs);

		context->DrawIndexed( subset->GetIndicesCount(), 0, 0);
	}
}
#else
void DrawRoutine::DrawLights()
{}
#endif