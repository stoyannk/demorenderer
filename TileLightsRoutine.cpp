#include "precompiled.h"

#include "TileLightsRoutine.h"
#include "Scene.h"
#include "ConstBufferTypes.h"
#include "SharedRenderResources.h"
#include "GPUProfiling.h"

#include <Dx11/Rendering/Camera.h>
#include <Dx11/Rendering/ShaderManager.h>

using namespace DirectX;

struct TilingData
{
	XMMATRIX InvProjection;
	unsigned LightsCount;
};

namespace {
static const char* SHADER_NAME = "..\\Shaders\\TileLights.hlsl";
static const char* ENTRY_POINT = "CSTileLights";
}

TileLightsRoutine::TileLightsRoutine()
: m_Debug(false)
{}

TileLightsRoutine::~TileLightsRoutine()
{}
 
bool TileLightsRoutine::Initialize(Renderer* renderer, Camera* camera, Scene* scene, const XMFLOAT4X4& projection)
{
	DxRenderingRoutine::Initialize(renderer);

	m_Camera = camera;
	m_Scene = scene;
	m_Projection = projection;
	
	if(!ReinitShading())
	{
		return false;
	}
	
	ShaderManager shaderManager(m_Renderer->GetDevice());

	if (!shaderManager.CreateEasyConstantBuffer<TilingData>(m_TilingDataBuffer.Receive(), false))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to create tiling data buffer");
		return false;
	}
	
	auto context = m_Renderer->GetImmediateContext();
	{
		TilingData td;
		td.InvProjection = XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_Projection)));
		td.LightsCount = m_Scene->GetLights().size();
		context->UpdateSubresource(m_TilingDataBuffer.Get(), 0, nullptr, &td, 0, 0);
	}

	if (!shaderManager.CreateStructuredBuffer(sizeof(CSPointLightProperties),
			MAX_LIGHTS_IN_SCENE,
			gSharedRenderResources->PointLightsBuffer.Receive(),
			nullptr,
			gSharedRenderResources->PointLightsSRV.Receive()))
		return false;

	m_TileCountX = unsigned(ceil(m_Renderer->GetBackBufferWidth() / LIGHTS_TILE_SIZE));
	m_TileCountY = unsigned(ceil(m_Renderer->GetBackBufferHeight() / LIGHTS_TILE_SIZE));

	if (!shaderManager.CreateStructuredBuffer(
		sizeof(CSCulledLight),
		m_TileCountX * m_TileCountY * MAX_LIGHTS_PER_TILE,
		gSharedRenderResources->LightsCulledBuffer.Receive(),
		gSharedRenderResources->LightsCulledUAV.Receive(),
		gSharedRenderResources->LightsCulledSRV.Receive()))
		return false;

	if (!shaderManager.CreateStructuredBuffer(
		sizeof(unsigned),
		m_TileCountX * m_TileCountY,
		gSharedRenderResources->LightsCulledCountBuffer.Receive(),
		gSharedRenderResources->LightsCulledCountUAV.Receive(),
		gSharedRenderResources->LightsCulledCountSRV.Receive()))
		return false;

	// populate lights
	{
		auto& lights = m_Scene->GetLights();

		if (lights.size() >= MAX_LIGHTS_IN_SCENE) {
			SLOG(Sev_Error, Fac_Rendering, "Too many lights!");
			return false;
		}

		CSPointLightProperties cs_lights[MAX_LIGHTS_IN_SCENE];
		auto ptr = cs_lights;
		::memset(ptr, 0, MAX_LIGHTS_IN_SCENE * sizeof(CSPointLightProperties));
		for (auto l : lights) {
			ptr->PositionAndRadius = XMFLOAT4(l.Position.x,
				l.Position.y,
				l.Position.z,
				l.Radius);
			ptr->Color = XMFLOAT4(l.Color.x,
				l.Color.y,
				l.Color.z,
				0);
			ptr += 1;
		}
		context->UpdateSubresource(gSharedRenderResources->PointLightsBuffer.Get(), 0, nullptr, cs_lights, 0, 0);
	}

	return true;
}

bool TileLightsRoutine::ReinitShading()
{
	ShaderManager shaderManager(m_Renderer->GetDevice());

	std::string multisampleDefine;
	if (m_Renderer->SamplesCount() > 1) {
		multisampleDefine = "#define MULTISAMPLING\n";
	}

	m_TileLightCuller.Set(shaderManager.CompileComputeShader(
		SHADER_NAME,
		ENTRY_POINT,
		"cs_5_0",
		multisampleDefine));
	if(!m_TileLightCuller.Get()) {
		SLOG(Sev_Error, Fac_Rendering, "Unable to create compute shader for lights culling");
		return false;
	}

	m_TileLightCullerDebug.Set(shaderManager.CompileComputeShader(
		SHADER_NAME,
		ENTRY_POINT,
		"cs_5_0",
		"#define DEBUG_SPHERES\n" + multisampleDefine));
	if (!m_TileLightCullerDebug.Get()) {
		SLOG(Sev_Error, Fac_Rendering, "Unable to create compute shader for lights culling - debug");
		return false;
	}
	
	return true;
}

void TileLightsRoutine::UpdateLights(ID3D11DeviceContext* context)
{
	const auto staticLightsCnt = m_Scene->GetLights().size();
	auto& dynLights = m_Scene->GetDynamicLights();
	const auto totalLights = staticLightsCnt + dynLights.size();

	if (totalLights >= MAX_LIGHTS_IN_SCENE) {
		SLOG(Sev_Error, Fac_Rendering, "Too many lights in scene! Unable to update!");
		return;
	}

	TilingData td;
	td.InvProjection = XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_Projection)));
	td.LightsCount = totalLights;
	context->UpdateSubresource(m_TilingDataBuffer.Get(), 0, nullptr, &td, 0, 0);

	if (dynLights.empty())
		return;

	CSPointLightProperties cs_dyn_lights[MAX_LIGHTS_IN_SCENE];
	for (auto lid = 0u; lid < dynLights.size(); ++lid)
	{
		auto& l = dynLights[lid];
		cs_dyn_lights[lid].PositionAndRadius = XMFLOAT4(l.Position.x,
			l.Position.y,
			l.Position.z,
			l.Radius);
		cs_dyn_lights[lid].Color = XMFLOAT4(l.Color.x,
			l.Color.y,
			l.Color.z,
			0);
	}

	D3D11_BOX dest;
	dest.left = staticLightsCnt * sizeof(CSPointLightProperties);
	dest.right = totalLights * sizeof(CSPointLightProperties);
	dest.top = 0;
	dest.bottom = 1;
	dest.back = 1;
	dest.front = 0;
	context->UpdateSubresource(gSharedRenderResources->PointLightsBuffer.Get(), 0, &dest, cs_dyn_lights, 0, 0);
}

bool TileLightsRoutine::Render(float deltaTime)
{	
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();
#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.TileLightsBegin[gGPUProfiling.CurrentIndex].Get());
#endif

	context->OMSetRenderTargets(0, nullptr, nullptr);

	UpdateLights(context);

	ID3D11UnorderedAccessView* uavs[] = { gSharedRenderResources->LightsCulledUAV.Get(),
		gSharedRenderResources->LightsCulledCountUAV.Get() };
	ID3D11ShaderResourceView* srvs[] = { gSharedRenderResources->PointLightsSRV.Get(), m_Renderer->GetBackDepthStencilShaderView() };
	ID3D11Buffer* cbs[] = { m_Renderer->GetPerFrameConstantBuffer(), m_TilingDataBuffer.Get() };

	if (m_Debug) {
		context->CSSetShader(m_TileLightCullerDebug.Get(), nullptr, 0);
	}
	else {
		context->CSSetShader(m_TileLightCuller.Get(), nullptr, 0);
	}
	context->CSSetUnorderedAccessViews(0, _countof(uavs), uavs, nullptr);
	context->CSSetShaderResources(0, _countof(srvs), srvs);
	context->CSSetConstantBuffers(0, _countof(cbs), cbs);

	context->Dispatch(m_TileCountX, m_TileCountY, 1);

	ID3D11UnorderedAccessView* emptyUAV[] = { nullptr, nullptr, nullptr, nullptr };
	context->CSSetUnorderedAccessViews(0, _countof(emptyUAV), emptyUAV, nullptr);
	ID3D11ShaderResourceView* emptySrvs[] = { nullptr, nullptr, nullptr, nullptr };
	context->CSSetShaderResources(0, _countof(emptySrvs), emptySrvs);

#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.TileLightsEnd[gGPUProfiling.CurrentIndex].Get());
#endif
	return true;
}

