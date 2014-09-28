#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>
#include <Dx11/Rendering/Subset.h>
#include <Dx11/Rendering/Providers.h>

class Camera;
class Scene;
class Mesh;
class MaterialShaderManager;

class DrawRoutine : public DxRenderingRoutine
{
public:
	DrawRoutine();

	virtual ~DrawRoutine();

	virtual bool Initialize(Renderer* renderer, Camera* camera, Scene* scene, const DirectX::XMFLOAT4X4& projection);

	virtual bool Render(float deltaTime) override;

	void SetWireframe(bool w)
	{
		m_Wireframe = w;
	}

private:
	bool ReinitShading();
	void DrawLights();

	Camera* m_Camera;
	Scene* m_Scene;
	DirectX::XMFLOAT4X4 m_Projection;

	bool m_Wireframe;

	std::unique_ptr<MaterialShaderManager> m_ShaderManager;
	
	// Shaders
	ReleaseGuard<ID3D11VertexShader> m_VertexShaderLights;
	ReleaseGuard<ID3D11PixelShader> m_PixelShaderLights;
	ReleaseGuard<ID3D11VertexShader> m_VertexShaderProcedural;
	ReleaseGuard<ID3D11PixelShader> m_PixelShaderProcedural;
	
	// Constant Buffers
	ReleaseGuard<ID3D11Buffer> m_PerSubsetBuffer;
	ReleaseGuard<ID3D11Buffer> m_GlobalPropsBuffer;

	// Samplers
	ReleaseGuard<ID3D11SamplerState> m_LinearSampler;
	ReleaseGuard<ID3D11SamplerState> m_PointSampler;

	ReleaseGuard<ID3D11InputLayout> m_VertexLayout;
	ReleaseGuard<ID3D11InputLayout> m_VertexLayoutProcedural;

#ifndef MINIMAL_SIZE
	std::shared_ptr<Mesh> m_LightMesh;
#endif
};