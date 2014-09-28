#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>

class Scene;

class PolygonizeRoutine : public DxRenderingRoutine
{
public:
	PolygonizeRoutine();

	virtual bool Initialize(Renderer* renderer, Camera* camera, Scene* scene, const DirectX::XMFLOAT4X4& projection);

	virtual bool Render(float deltaTime) override;

private:
	ID3D11ComputeShader* GetShader(const std::string& generator);

	ReleaseGuard<ID3D11ComputeShader> m_InitCS;
	ReleaseGuard<ID3D11ComputeShader> m_FinalizeCS;

	std::unordered_map<std::string, ReleaseGuard<ID3D11ComputeShader>> m_Shaders;

	ReleaseGuard<ID3D11Buffer> m_CellDataBuffer;
	ReleaseGuard<ID3D11ShaderResourceView> m_CellDataSRV;
	ReleaseGuard<ID3D11Buffer> m_VertexDataBuffer;
	ReleaseGuard<ID3D11ShaderResourceView> m_VertexDataSRV;
	
	ReleaseGuard<ID3D11Buffer> m_PerFramePolygonizerBuffer;

	ReleaseGuard<ID3D11SamplerState> m_LinearSampler;

	TexturePtr m_RandomTexture;
	
	Scene* m_Scene;
	Camera* m_Camera;
	DirectX::XMFLOAT4X4 m_Projection;

	float m_TimeSinceStart;
};