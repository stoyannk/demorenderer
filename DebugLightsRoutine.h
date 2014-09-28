#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>

class ScreenQuad;
class Camera;

class DebugLightsRoutine : public DxRenderingRoutine
{
public:
	DebugLightsRoutine();

	virtual ~DebugLightsRoutine();

	virtual bool Initialize(Renderer* renderer, Camera* camera, const DirectX::XMFLOAT4X4& projection);

	virtual bool Render(float deltaTime) override;

private:
	bool ReinitShading();

	Camera* m_Camera;
	DirectX::XMFLOAT4X4 m_Projection;

	ReleaseGuard<ID3D11DepthStencilState> m_DSState;

	std::unique_ptr<ScreenQuad> m_SQ;
};