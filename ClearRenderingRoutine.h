#pragma once

#include <Dx11/Rendering/DxRenderingRoutine.h>

class ClearRenderingRoutine : public DxRenderingRoutine
{
public:
	virtual bool Render(float deltaTime) override;
};