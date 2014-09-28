#pragma once

#include "PointLight.h"
#include "DirectionalLight.h"
#include "SharedRenderResources.h"
#include "MaterialTable.h"
#include <Dx11/Rendering/Entity.h>

class DxRenderer;
class Camera;
class FrustumCuller;

class Scene
{
public:
	Scene(DxRenderer* renderer, Camera* camera, const DirectX::XMFLOAT4X4& projection);

	bool Initialize();

	EntityVec& GetEntities()
	{
		return m_Entities;
	}
	EntityToDrawVec& GetEntitiesForMainCamera()
	{
		return m_MainCameraEntities;
	}
	ProceduralEntityToDrawVec& GetProceduralEntitiesForMainCamera()
	{
		return m_MainCameraProceduralEntities;
	}

	const std::vector<PointLight>& GetLights() const;
	const std::vector<MovingLight>& Scene::GetDynamicLights() const;
	const DirectionalLight& GetSun() const;

	const std::vector<GeneratedMeshPtr>& GetMeshesToGenerate() const
	{
		return m_MeshesToRegenerate;
	}
	void DidRegenerateAllMeshes();

	const ProceduralEntityVec& GetGeneratedMeshes() const
	{
		return m_GeneratedMeshes;
	}

	void Update(float dt);
	void ReloadProcedural();

	void FireLight();

private:
	bool ReloadProceduralFiles(std::vector<std::string>& code);
	void PopulateSubsetsToDraw();
	
	EntityVec m_Entities;
	EntityToDrawVec m_MainCameraEntities;
	ProceduralEntityToDrawVec m_MainCameraProceduralEntities;

	std::vector<GeneratedMeshPtr> m_MeshesToRegenerate;
	ProceduralEntityVec m_GeneratedMeshes;

#ifndef MINIMAL_SIZE
	MaterialTable m_ProceduralMeshesMaterials;
#endif

	DxRenderer* m_Renderer;

	Camera* m_Camera;

	std::unique_ptr<FrustumCuller> m_FrustumCuller;

	std::vector<PointLight>		m_Lights;
	std::vector<MovingLight>	m_DynamicLights;
	DirectionalLight			m_Sun;
};
