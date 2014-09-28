#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>
#include <Dx11/Rendering/Subset.h>
#include <Dx11/Rendering/Providers.h>

class Camera;
class Scene;
class MaterialShaderManager;

class ZPrepassRoutine : public DxRenderingRoutine
{
public:
	ZPrepassRoutine();

	virtual ~ZPrepassRoutine();

	virtual bool Initialize(Renderer* renderer, Camera* camera, Scene* scene, const DirectX::XMFLOAT4X4& projection);

	virtual bool Render(float deltaTime) override;

private:
	bool ReinitShading();

	Camera* m_Camera;
	Scene* m_Scene;
	DirectX::XMFLOAT4X4 m_Projection;

	std::unique_ptr<MaterialShaderManager> m_ShaderManager;

	ReleaseGuard<ID3D11InputLayout> m_VertexLayout;
	ReleaseGuard<ID3D11Buffer> m_PerSubsetBuffer;

	ReleaseGuard<ID3D11VertexShader> m_VertexShaderProcedural;
	ReleaseGuard<ID3D11InputLayout> m_VertexLayoutProcedural;
};