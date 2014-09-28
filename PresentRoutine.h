#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>

class PresentRoutine : public DxRenderingRoutine
{
public:
	PresentRoutine()
		: m_VSync(true)
	{}

	void ToggleVSync()
	{
		m_VSync = !m_VSync;
	}

	virtual bool Render(float deltaTime);

private:
	bool m_VSync;
};