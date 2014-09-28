#include "precompiled.h"

#include "Scene.h"

#include <Dx11/Rendering/Mesh.h>
#include <Dx11/Rendering/DxRenderer.h>
#include <Dx11/Rendering/Camera.h>
#include <Dx11/Rendering/MeshLoader.h>
#include <Dx11/Rendering/FrustumCuller.h>

#include <Utilities/Random.h>

//#include <Dx11/Rendering/MeshSaver.h>

using namespace DirectX;

#define SURFACE_GENERATOR_EXTENT 10
#define SURFACE_BUFF_SIZE 250000

Scene::Scene(DxRenderer* renderer, Camera* camera, const XMFLOAT4X4& projection)
	: m_Renderer(renderer)
	, m_Camera(camera)
	, m_Sun(XMFLOAT4(-1, -1, 1, 0.3f), XMFLOAT3(0.77f, 0.901f, 0.929f))
{
	m_FrustumCuller.reset(new FrustumCuller(camera->GetViewMatrix(), projection));
}

bool Scene::Initialize()
{
	std::string errors;

	Entity sponza;
	sponza.Position = XMFLOAT3A(0, 0, 0);
	sponza.Scale = 1.0f;
	sponza.Rotation = XMQuaternionIdentity();
	sponza.Mesh.reset(MeshLoader::LoadMesh(static_cast<DxRenderer*>(m_Renderer), "..\\..\\media\\saves\\sponza.rmesh", errors));
	//m_Sponza.reset(MeshLoader::LoadMesh(static_cast<DxRenderer*>(m_Renderer), "..\\..\\media\\sponza.obj", errors));
	if (!sponza.Mesh.get())
	{
		STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::tie("Unable to load sponza mesh: ", errors));
		return false;
	}
	else
	{
		STLOG(Logging::Sev_Warning, Logging::Fac_Rendering, std::tie("Sponza model loaded (warnings): ", errors));
	}

	//MeshSaver::SaveMesh(static_cast<DxRenderer*>(m_Renderer), m_Sponza.get(), "..\\..\\media\\saves\\sponza.rmesh", errors);
	
	m_Entities.push_back(std::move(sponza));

	// Set camera
	m_Camera->SetLookAt(XMFLOAT3(0.f, 0.f, -5.f)
						, XMFLOAT3(0.f, 0.f, 1.f)
						, XMFLOAT3(0.f, 1.f, 0.f));

	//m_Lights.push_back(PointLight(0, 0, 1, 30, 1.f, 0.f, 0.f, 50));
	//m_Lights.push_back(PointLight(-2, 0, 1, 30, 0.f, 1.f, 0.f, 100));

	static const int LIGHTS_COUNT = 30;
	static const int LIGHTS_COUNT_ON_WALLS = 30;
	static_assert(MAX_LIGHTS_IN_SCENE >= LIGHTS_COUNT + LIGHTS_COUNT_ON_WALLS, "Lights exceed the max count specified");
	static const unsigned SEED = 1;
	Random::Seed(SEED);
	m_Lights.reserve(LIGHTS_COUNT + LIGHTS_COUNT_ON_WALLS);

	/*m_Lights.push_back(
		PointLight(0, 25, 0
		, 100
		, 1, 1, 1));*/
	
	for(int i = 0; i < LIGHTS_COUNT; ++i)
	{
		m_Lights.push_back(
			PointLight(Random::RandomBetween(-1400, 1400), 5, Random::RandomBetween(-150, 150)
						, Random::RandomBetween(250, 350)
						, Random::RandomNumber(), Random::RandomNumber(), Random::RandomNumber()));
	}
	// Wall 1
	for(int i = 0; i < LIGHTS_COUNT_ON_WALLS/2; ++i)
	{
		m_Lights.push_back(
			PointLight(Random::RandomBetween(-1000, 1000), Random::RandomBetween(250, 1000), Random::RandomBetween(-240, -220)
						, Random::RandomBetween(150, 250)
						, Random::RandomNumber(), Random::RandomNumber(), Random::RandomNumber()));
	}
	// Wall 2
	for(int i = 0; i < LIGHTS_COUNT_ON_WALLS/2; ++i)
	{
		m_Lights.push_back(
			PointLight(Random::RandomBetween(-1000, 1000), Random::RandomBetween(250, 1000), Random::RandomBetween(150, 170)
						, Random::RandomBetween(150, 250)
						, Random::RandomNumber(), Random::RandomNumber(), Random::RandomNumber()));
	}

	// Generated stuff
	std::vector<std::string> code;
	if (!ReloadProceduralFiles(code))
		return false;

	ProceduralEntity procedural;
	procedural.Mesh = GeneratedMesh::Create(m_Renderer->GetDevice(), SURFACE_BUFF_SIZE, code[0], XMINT3(SURFACE_GENERATOR_EXTENT, SURFACE_GENERATOR_EXTENT, SURFACE_GENERATOR_EXTENT));
	procedural.Position = XMFLOAT3A(0, 100, 0);
	procedural.Scale = 15.0f;
	procedural.Rotation = XMQuaternionIdentity();

#ifndef MINIMAL_SIZE
	if (!m_ProceduralMeshesMaterials.Load("../media/materials.json"))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load material table");
		return false;
	}

	Material proceduralMeshMaterial;
	if (!GenerateMaterialFromTable(m_ProceduralMeshesMaterials,
		"../media/textures",
		m_Renderer->GetTextureManager(),
		proceduralMeshMaterial))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load procedural mesh material");
		return false;
	}
#else
	Material proceduralMeshMaterial;
	std::vector<std::string> fileListDiffuse = {
		"grass.png",
		"Snow.jpg",
		"canyon.jpg",
		"Deep-Freeze.jpg",
		"dry_grass.jpg",
		"sand.jpg",
		"sand_stone.jpg",
		"grey_stone.jpg",
		"calcare.jpg",
	};
	if (!GenerateMaterialFromList(
		"../media/textures",
		fileListDiffuse,
		m_Renderer->GetTextureManager(),
		proceduralMeshMaterial))
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load procedural mesh material");
		return false;
	}
#endif
	proceduralMeshMaterial.SetSpecularPower(10.0f);
	procedural.Mesh->SetMaterial(proceduralMeshMaterial);
	procedural.Mesh->SetDynamic(true);

	if (!procedural.Mesh->IsDynamic())
		m_MeshesToRegenerate.push_back(procedural.Mesh);
	m_GeneratedMeshes.push_back(procedural);
	
	return true;
}

bool Scene::ReloadProceduralFiles(std::vector<std::string>& code)
{
	std::ifstream surfaceGen("../Shaders/Generators/Surface.hlsl");
	if (!surfaceGen.is_open())
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to load procedural generator file");
		return false;
	}

	std::string generator((std::istreambuf_iterator<char>(surfaceGen)),
						std::istreambuf_iterator<char>());

	code.push_back(std::move(generator));

	return true;
}

void Scene::ReloadProcedural()
{
	std::vector<std::string> code;
	if (!ReloadProceduralFiles(code))
		return;

	auto& mesh = m_GeneratedMeshes[0].Mesh;
	mesh->SetGenerator(code[0], XMINT3(SURFACE_GENERATOR_EXTENT, SURFACE_GENERATOR_EXTENT, SURFACE_GENERATOR_EXTENT));
	if (!mesh->IsDynamic())
		m_MeshesToRegenerate.push_back(mesh);
}

void Scene::PopulateSubsetsToDraw()
{
	m_FrustumCuller->SetView(m_Camera->GetViewMatrix());

	m_MainCameraEntities.clear();
	m_FrustumCuller->Cull(m_Entities, m_MainCameraEntities);

	m_MainCameraProceduralEntities.clear();
	for (auto entity : m_GeneratedMeshes)
	{
		ProceduralEntityToDraw toDraw;
		toDraw.WorldMatrix = XMMatrixAffineTransformation(
			XMVectorReplicate(entity.Scale),
			XMVectorZero(),
			entity.Rotation,
			XMLoadFloat3A(&entity.Position));
		toDraw.Geometry = entity.Mesh.get();
		m_MainCameraProceduralEntities.emplace_back(toDraw);
	}
}

const std::vector<PointLight>& Scene::GetLights() const
{
	return m_Lights;
}

const std::vector<MovingLight>& Scene::GetDynamicLights() const
{
	return m_DynamicLights;
}

const DirectionalLight& Scene::GetSun() const
{
	return m_Sun;
}

void Scene::FireLight()
{
	XMFLOAT3 camPos = m_Camera->GetPos();
	MovingLight light = MovingLight(camPos.x, camPos.y, camPos.z
								, Random::RandomBetween(50, 100)
								, Random::RandomNumber(), Random::RandomNumber(), Random::RandomNumber());

	light.Direction = m_Camera->GetAxisZ();

	m_DynamicLights.push_back(light);
}
 
void Scene::Update(float dt)
{
	std::for_each(m_DynamicLights.begin(), m_DynamicLights.end(), std::bind(&MovingLight::Update, std::placeholders::_1, dt));

	m_DynamicLights.erase(std::remove_if(m_DynamicLights.begin(), m_DynamicLights.end(), [](const MovingLight& l) -> bool 
		{ 
			static const float LIFETIME = 15;
			return l.LifeTime >= LIFETIME; 
		} ), m_DynamicLights.end());

	PopulateSubsetsToDraw();

	// Enqueue all dynamic procedural meshes for re-calculation
	std::for_each(m_GeneratedMeshes.cbegin(), 
		m_GeneratedMeshes.cend(), [&](const ProceduralEntity& entity)
	{
		if (entity.Mesh->IsDynamic())
			m_MeshesToRegenerate.push_back(entity.Mesh);
	});

	// TEST - rotating procedural
	/*static float time = 0;
	time += dt;
	m_GeneratedMeshes[0].Rotation = XMQuaternionRotationRollPitchYaw(time, 0, 0);
	*/
}

void Scene::DidRegenerateAllMeshes()
{
	m_MeshesToRegenerate.clear();
}
