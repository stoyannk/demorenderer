#include "precompiled.h"

#include "DemoRendererApplication.h"

#include <Dx11/Rendering/Mesh.h>
#include <Dx11/Rendering/FrustumCuller.h>

#include "Scene.h"

#include "ClearRenderingRoutine.h"
#include "PolygonizeRoutine.h"
#include "SharedRenderResources.h"
#include "ZPrepassRoutine.h"
#include "TileLightsRoutine.h"
#include "DrawRoutine.h"
#include "PresentRoutine.h"
#include "DebugLightsRoutine.h"

#include "GPUProfiling.h"

GPUProfiling gGPUProfiling;

using namespace DirectX;

SharedRenderResources* gSharedRenderResources = nullptr;

DemoRendererApplication::DemoRendererApplication(HINSTANCE instance)
	: DxGraphicsApplication(instance)
	, m_LastMouseX(0)
	, m_LastMouseY(0)
	, m_IsRightButtonDown(false)
	, m_CurrentRoutines(RS_Draw)
	, m_Wireframe(false)
	, m_BoostSpeed(1.f)
{
	::memset(m_Keys, 0, 255);
}

bool DemoRendererApplication::Initiate(char* className,
	char* windowName,
	unsigned width,
	unsigned height,
	bool fullscreen,
	WNDPROC winProc,
	bool sRGBRT,
	int samplesCnt)
{
	bool result = true;
	result &= DxGraphicsApplication::Initiate(className, windowName, width, height, fullscreen, winProc, sRGBRT);

	static const float NEAR_PLANE = 1.0f;
	static const float FAR_PLANE = 5000.0f;

	SetProjection(XM_PI / 3, float(GetWidth()) / GetHeight(), NEAR_PLANE, FAR_PLANE);

	GetMainCamera()->SetLookAt(XMFLOAT3(0, 0, -4)
								, XMFLOAT3(0, 0, 5)
								, XMFLOAT3(0, 1, 0));

	DxRenderer* renderer = static_cast<DxRenderer*>(GetRenderer());
	
	m_SharedRenderResources.reset(new SharedRenderResources);
	gSharedRenderResources = m_SharedRenderResources.get();

	if (!InitializeGPUProfiling())
		return false;

	m_Scene.reset(new Scene(renderer, GetMainCamera(), GetProjection()));
	ReturnUnless(m_Scene->Initialize(), false);

	m_ClearRoutine.reset(new ClearRenderingRoutine());
	ReturnUnless(m_ClearRoutine->Initialize(renderer), false);

	m_PolygonizeRoutine.reset(new PolygonizeRoutine());
	ReturnUnless(m_PolygonizeRoutine->Initialize(renderer, GetMainCamera(), m_Scene.get(), GetProjection()), false);

	m_ZPrepassRoutine.reset(new ZPrepassRoutine());
	ReturnUnless(m_ZPrepassRoutine->Initialize(renderer, GetMainCamera(), m_Scene.get(), GetProjection()), false);

	m_TileLightsRoutine.reset(new TileLightsRoutine());
	ReturnUnless(m_TileLightsRoutine->Initialize(renderer, GetMainCamera(), m_Scene.get(), GetProjection()), false);

	m_DrawRoutine.reset(new DrawRoutine());
	ReturnUnless(m_DrawRoutine->Initialize(renderer, GetMainCamera(), m_Scene.get(), GetProjection()), false);
	m_DrawRoutine->SetWireframe(m_Wireframe);

	m_PresentRoutine.reset(new PresentRoutine());
	ReturnUnless(m_PresentRoutine->Initialize(renderer), false);

	m_DebugLightsRoutine.reset(new DebugLightsRoutine());
	ReturnUnless(m_DebugLightsRoutine->Initialize(renderer, GetMainCamera(), GetProjection()), false);

	SetDrawRoutines();

	return result;
}

void DemoRendererApplication::SetDrawRoutines()
{
	auto renderer = GetRenderer();
	renderer->ClearRoutines();

	renderer->AddRoutine(m_ClearRoutine.get());
	renderer->AddRoutine(m_PolygonizeRoutine.get());
	renderer->AddRoutine(m_ZPrepassRoutine.get());
	renderer->AddRoutine(m_TileLightsRoutine.get());
	renderer->AddRoutine(m_DrawRoutine.get());
	renderer->AddRoutine(m_PresentRoutine.get());
}

void DemoRendererApplication::SetDebugRoutines()
{
	auto renderer = GetRenderer();
	renderer->ClearRoutines();

	renderer->AddRoutine(m_ClearRoutine.get());
	renderer->AddRoutine(m_PolygonizeRoutine.get());
	renderer->AddRoutine(m_ZPrepassRoutine.get());
	renderer->AddRoutine(m_TileLightsRoutine.get());
	renderer->AddRoutine(m_DebugLightsRoutine.get());
	renderer->AddRoutine(m_PresentRoutine.get());
}

namespace {
	static const float MV_SPEED = 0.5f * 50;
	static const float ROT_SPEED = 0.5f * 10;
	static const float MOUSE_SPEED = 0.005f * 10;
	static const float BOOST_COEFF = 4.f;
	static const float lightChange = 0.05f;
}

void DemoRendererApplication::Update(float delta)
{
	XMFLOAT3A sd;
	for (int key = 0; key < 255; ++key)
	{
		if (!m_Keys[key]) continue;

		switch (key)
		{
		case VK_UP:
			m_MainCamera.Move(MV_SPEED * delta * m_BoostSpeed);
			break;
		case VK_DOWN:
			m_MainCamera.Move(-MV_SPEED * delta * m_BoostSpeed);
			break;
		case VK_LEFT:
			m_MainCamera.Yaw(-ROT_SPEED * delta * m_BoostSpeed);
			break;
		case VK_RIGHT:
			m_MainCamera.Yaw(ROT_SPEED * delta * m_BoostSpeed);
			break;
		case VK_INSERT:
			m_MainCamera.Up(MV_SPEED * delta);
			break;
		case VK_DELETE:
			m_MainCamera.Up(-MV_SPEED * delta);
			break;
		case VK_HOME:
			m_MainCamera.Pitch(-ROT_SPEED * delta);
			break;
		case VK_END:
			m_MainCamera.Pitch(ROT_SPEED * delta);
			break;
		default:
			break;
		}
	}
	m_Scene->Update(delta);
}

void DemoRendererApplication::KeyDown(unsigned int key)
{
	m_Keys[key] = true;

	switch (key)
	{
	case VK_NUMPAD1:
	{
		m_CurrentRoutines = RoutineSet((m_CurrentRoutines + 1) % RS_Count);
		switch (m_CurrentRoutines)
		{
		case RS_Draw:
			SetDrawRoutines();
			break;
		case RS_Debug:
			SetDebugRoutines();
			break;
		}
	}
		break;
	case VK_F1:
		m_TileLightsRoutine->ToggleDebug();
		break;
	case VK_F2:
		m_Wireframe = !m_Wireframe;
		m_DrawRoutine->SetWireframe(m_Wireframe);
		break;
	case VK_F3:
		m_PresentRoutine->ToggleVSync();
		break;
	case VK_F5:
		m_Scene->ReloadProcedural();
		break;
	case VK_SPACE:
		m_Scene->FireLight();
		break;
	case VK_NUMPAD9:
		GetRenderer()->ReinitRoutineShading();
		break;
	case VK_NUMPAD8:
		m_Scene->GetEntities()[0].Position.y += 1.0f;
		break;
	case VK_NUMPAD2:
		m_Scene->GetEntities()[0].Position.y -= 1.0f;
		break;
	case VK_NUMPAD4:
		m_Scene->GetEntities()[0].Position.x -= 1.0f;
		break;
	case VK_NUMPAD6:
		m_Scene->GetEntities()[0].Position.x += 1.0f;
		break;
	case VK_SHIFT:
		m_BoostSpeed = BOOST_COEFF;
		break;
	}
}

void DemoRendererApplication::KeyUp(unsigned int key)
{
	m_Keys[key] = false;

	switch (key)
	{
	case VK_SHIFT:
		m_BoostSpeed = 1.f;
		break;
	}
}

void DemoRendererApplication::MouseButtonUp(MouseBtn button, int x, int y)
{
	switch (button)
	{
	case MBT_Right:
		m_IsRightButtonDown = false;
		break;
	default:
		break;
	}
}

void DemoRendererApplication::MouseButtonDown(MouseBtn button, int x, int y)
{
	switch (button)
	{
	case MBT_Right:
		m_IsRightButtonDown = true;
		break;
	default:
		break;
	}
}

void DemoRendererApplication::MouseMove(int x, int y)
{
	if (m_IsRightButtonDown) {
		const int dX = m_LastMouseX - x;
		const int dY = m_LastMouseY - y;

		m_MainCamera.Yaw(-dX*MOUSE_SPEED);
		m_MainCamera.Pitch(-dY*MOUSE_SPEED);
	}

	m_LastMouseX = x;
	m_LastMouseY = y;
}

#define CREATE_QUERY(OBJECT) \
	if (FAILED(device->CreateQuery(&desc, OBJECT.Receive()))) \
	{ \
		SLOG(Sev_Error, Fac_Rendering, "Unable to create timestamp query");	\
		return false; \
	}

bool DemoRendererApplication::InitializeGPUProfiling()
{
#if defined(ENABLE_GPU_PROFILING)
	m_ProfileFile.open("gpu_profile.log");
	if (!m_ProfileFile.is_open())
	{
		SLOG(Sev_Error, Fac_Rendering, "Unable to open file for GPU profile");
		return false;
	}
	
	gGPUProfiling.FrameCounter = 0;
	gGPUProfiling.CurrentIndex = 0;

	auto device = m_Renderer->GetDevice();

	for (auto i = 0; i < GPUProfiling::QUERIES_COUNT; ++i) {
		D3D11_QUERY_DESC desc;
		::memset(&desc, 0, sizeof(desc));
		desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		CREATE_QUERY(gGPUProfiling.FrameDisjoint[i]);
		
		desc.Query = D3D11_QUERY_TIMESTAMP;
		
		CREATE_QUERY(gGPUProfiling.FrameBegin[i]);
		CREATE_QUERY(gGPUProfiling.FrameEnd[i]);
		CREATE_QUERY(gGPUProfiling.ZPrepassBegin[i]);
		CREATE_QUERY(gGPUProfiling.ZPrepassEnd[i]);
		CREATE_QUERY(gGPUProfiling.PolygonizeBegin[i]);
		CREATE_QUERY(gGPUProfiling.PolygonizeEnd[i]);
		CREATE_QUERY(gGPUProfiling.TileLightsBegin[i]);
		CREATE_QUERY(gGPUProfiling.TileLightsEnd[i]);
		CREATE_QUERY(gGPUProfiling.DrawBegin[i]);
		CREATE_QUERY(gGPUProfiling.DrawEnd[i]);
		CREATE_QUERY(gGPUProfiling.PresentBegin[i]);
		CREATE_QUERY(gGPUProfiling.PresentEnd[i]);
	}
#endif
	return true;
}

void DemoRendererApplication::PreRender()
{
#if defined(ENABLE_GPU_PROFILING)
	auto ctx = m_Renderer->GetImmediateContext();
	ctx->Begin(gGPUProfiling.FrameDisjoint[gGPUProfiling.CurrentIndex].Get());
	ctx->End(gGPUProfiling.FrameBegin[gGPUProfiling.CurrentIndex].Get());
#endif
}

void DemoRendererApplication::PostRender()
{
#if defined(ENABLE_GPU_PROFILING)
	auto ctx = m_Renderer->GetImmediateContext();
	ctx->End(gGPUProfiling.FrameDisjoint[gGPUProfiling.CurrentIndex].Get());
	ctx->End(gGPUProfiling.FrameEnd[gGPUProfiling.CurrentIndex].Get());

	const auto lastFrameIndex = (gGPUProfiling.CurrentIndex + 1) % GPUProfiling::QUERIES_COUNT;
	gGPUProfiling.CurrentIndex = (gGPUProfiling.CurrentIndex + 1) % GPUProfiling::QUERIES_COUNT;

	// Give times to queries to end
	if (gGPUProfiling.FrameCounter++ < GPUProfiling::QUERIES_COUNT - 1)
		return;

	std::ostringstream line;
	const auto frameId = gGPUProfiling.FrameCounter - (GPUProfiling::QUERIES_COUNT - 1);
	line << "Frame " << frameId << ": ";

	while (ctx->GetData(gGPUProfiling.FrameDisjoint[lastFrameIndex].Get(), nullptr, 0, 0) == S_FALSE)
	{
		line << "Query not ready! Sleeping!" << std::endl;
		::Sleep(1);
	}

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
	ctx->GetData(gGPUProfiling.FrameDisjoint[lastFrameIndex].Get(), &disjoint, sizeof(disjoint), 0);

	// Even if the the query is disjoint we get all data, otherwise
	// D3D11 complains with a warning
	if (disjoint.Disjoint)
	{
		line << "Disjoint frame data detected!";
	}

	line << std::endl;
	
	auto printTimestamp = [&](ID3D11Query* startQuery, ID3D11Query* endQuery, const char* prettyStr) {
		UINT64 start, end;
		ctx->GetData(startQuery, &start, sizeof(start), 0);
		ctx->GetData(endQuery, &end, sizeof(end), 0);
		if (!disjoint.Disjoint)
			line << prettyStr << std::fixed << (double(end - start) / (double(disjoint.Frequency)) * 1000.0) << "; ";
	};

	// Get all timestamps
	printTimestamp(gGPUProfiling.FrameBegin[lastFrameIndex].Get(),
		gGPUProfiling.FrameEnd[lastFrameIndex].Get(),
		"Total frame time: ");
	printTimestamp(gGPUProfiling.PolygonizeBegin[lastFrameIndex].Get(),
		gGPUProfiling.PolygonizeEnd[lastFrameIndex].Get(),
		"Polygonize: ");
	printTimestamp(gGPUProfiling.ZPrepassBegin[lastFrameIndex].Get(),
		gGPUProfiling.ZPrepassEnd[lastFrameIndex].Get(),
		"Z Prepass: ");
	printTimestamp(gGPUProfiling.TileLightsBegin[lastFrameIndex].Get(),
		gGPUProfiling.TileLightsEnd[lastFrameIndex].Get(),
		"Tile lights: ");
	printTimestamp(gGPUProfiling.DrawBegin[lastFrameIndex].Get(),
		gGPUProfiling.DrawEnd[lastFrameIndex].Get(),
		"Draw: ");
	printTimestamp(gGPUProfiling.PresentBegin[lastFrameIndex].Get(),
		gGPUProfiling.PresentEnd[lastFrameIndex].Get(),
		"Present: ");
	
	line << std::endl;
	
	m_ProfileFile << line.str() << std::endl;
#endif
}