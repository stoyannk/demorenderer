#include "precompiled.h"

#include "DebugLightsRoutine.h"
#include "ConstBufferTypes.h"
#include "SharedRenderResources.h"

#include <Dx11/Rendering/Camera.h>
#include <Dx11/Rendering/ShaderManager.h>

#include <Dx11/Rendering/ScreenQuad.h>

using namespace DirectX;

namespace {
	static const char* SHADER_NAME = "..\\Shaders\\DebugDraw.hlsl";
	static const char* VS_ENTRY = "VS";
	static const char* PS_ENTRY = "PS";
}

DebugLightsRoutine::DebugLightsRoutine()
: m_SQ(new ScreenQuad)
{}

DebugLightsRoutine::~DebugLightsRoutine()
{}
 
bool DebugLightsRoutine::Initialize(Renderer* renderer, Camera* camera, const XMFLOAT4X4& projection)
{
	DxRenderingRoutine::Initialize(renderer);

	m_Camera = camera;
	m_Projection = projection;
	
	if(!ReinitShading())
	{
		return false;
	}
	
	CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	if (FAILED(m_Renderer->GetDevice()->CreateDepthStencilState(&dsDesc, m_DSState.Receive())))
	{
		STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to create blend state for z-prepass"));
		return false;
	}

	return true;
}

bool DebugLightsRoutine::ReinitShading()
{
	if (!m_SQ->Initialize(m_Renderer,
		"..\\Shaders\\DebugDraw.hlsl",
		"VS", 
		"PS"))
	{
		return false;
	}

	return true;
}

bool DebugLightsRoutine::Render(float deltaTime)
{	
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();
	m_Renderer->SetViewport(context);

	ID3D11RenderTargetView* rts[] = { m_Renderer->GetBackBufferView() };
	context->OMSetRenderTargets(1, rts, m_Renderer->GetBackDepthStencilView());

	context->OMSetDepthStencilState(m_DSState.Get(), 0);

	ID3D11ShaderResourceView* srvs[] = { 
		gSharedRenderResources->LightsCulledSRV.Get(),
		gSharedRenderResources->LightsCulledCountSRV.Get(),
		gSharedRenderResources->PointLightsSRV.Get() };
	context->PSSetShaderResources(0, _countof(srvs), srvs);

	m_SQ->Draw(nullptr, 0);

	ID3D11ShaderResourceView* emptySrvs[] = { nullptr, nullptr, nullptr, nullptr };
	context->PSSetShaderResources(0, _countof(emptySrvs), emptySrvs);

	context->OMSetDepthStencilState(nullptr, 0);

	return true;
}

