#pragma once

#include <Dx11/AppGraphics/DxGraphicsApplication.h>

class Scene;

class ClearRenderingRoutine;
class PresentRoutine;
class PolygonizeRoutine;
class DrawRoutine;
class TileLightsRoutine;
class ZPrepassRoutine;
class DebugLightsRoutine;

struct SharedRenderResources;

class DemoRendererApplication : public DxGraphicsApplication
{
public:
	DemoRendererApplication (HINSTANCE instance);
	bool Initiate(char* className, char* windowName, unsigned width, unsigned height, bool fullscreen, WNDPROC winProc, bool sRGBRT, int samplesCnt);
	virtual void Update(float delta) override;
	virtual void PreRender() override;
	virtual void PostRender() override;
	virtual void KeyDown(unsigned int key) override;
	virtual void KeyUp(unsigned int key) override;
	virtual void MouseMove(int x, int y) override;
	virtual void MouseButtonDown(MouseBtn button, int x, int y) override;
	virtual void MouseButtonUp(MouseBtn button, int x, int y) override;

private:
	void SetDrawRoutines();
	void SetDebugRoutines();
	bool InitializeGPUProfiling();

	#if defined(ENABLE_GPU_PROFILING)
	std::ofstream m_ProfileFile;
	#endif
	std::unique_ptr<Scene> m_Scene;

	std::unique_ptr<ClearRenderingRoutine> m_ClearRoutine;
	std::unique_ptr<ZPrepassRoutine> m_ZPrepassRoutine;
	std::unique_ptr<PolygonizeRoutine> m_PolygonizeRoutine;
	std::unique_ptr<TileLightsRoutine> m_TileLightsRoutine;
	std::unique_ptr<DrawRoutine> m_DrawRoutine;
	std::unique_ptr<PresentRoutine> m_PresentRoutine;
	std::unique_ptr<DebugLightsRoutine> m_DebugLightsRoutine;

	std::unique_ptr<SharedRenderResources> m_SharedRenderResources;

	bool m_Keys[255];

	int m_LastMouseX;
	int m_LastMouseY;
	bool m_IsRightButtonDown;

	bool m_Wireframe;
	float m_BoostSpeed;

	enum RoutineSet
	{
		RS_Draw,
		RS_Debug,

		RS_Count
	};
	RoutineSet m_CurrentRoutines;
};

