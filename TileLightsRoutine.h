#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>
#include <Dx11/Rendering/Subset.h>

class Camera;
class Scene;

class TileLightsRoutine : public DxRenderingRoutine
{
public:
	TileLightsRoutine();

	virtual ~TileLightsRoutine();

	virtual bool Initialize(Renderer* renderer, Camera* camera, Scene* scene, const DirectX::XMFLOAT4X4& projection);

	virtual bool Render(float deltaTime) override;

	void ToggleDebug() { m_Debug = !m_Debug; }

private:
	bool ReinitShading();
	void UpdateLights(ID3D11DeviceContext* context);

	Camera* m_Camera;
	Scene* m_Scene;
	DirectX::XMFLOAT4X4 m_Projection;

	bool m_Debug;

	unsigned m_TileCountX;
	unsigned m_TileCountY;

	ReleaseGuard<ID3D11ComputeShader> m_TileLightCuller;
	ReleaseGuard<ID3D11ComputeShader> m_TileLightCullerDebug;

	ReleaseGuard<ID3D11Buffer> m_TilingDataBuffer;
};