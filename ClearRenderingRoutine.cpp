#include "precompiled.h"

#include "ClearRenderingRoutine.h"

bool ClearRenderingRoutine::Render(float deltaTime)
{
	ID3D11DeviceContext* context = m_Renderer->GetImmediateContext();
	
	ID3D11RenderTargetView* bbufferRTV = m_Renderer->GetBackBufferView();
	context->OMSetRenderTargets(1, &bbufferRTV, m_Renderer->GetBackDepthStencilView());

    float ClearColor[4] = { 0.5f, 0.125f, 0.3f, 1.0f };
	context->ClearRenderTargetView(m_Renderer->GetBackBufferView(), ClearColor);
	context->ClearDepthStencilView(m_Renderer->GetBackDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);
		
	return true;
}