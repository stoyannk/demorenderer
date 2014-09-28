#include "precompiled.h"

#include "PresentRoutine.h"
#include "GPUProfiling.h"

bool PresentRoutine::Render(float deltaTime)
{
	auto context = m_Renderer->GetImmediateContext();
#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.PresentBegin[gGPUProfiling.CurrentIndex].Get());
#endif

	if(FAILED(m_Renderer->GetSwapChain()->Present(m_VSync, 0)))
	{
		STLOG(Logging::Sev_Error, Logging::Fac_Rendering, std::make_tuple("Unable to present"));
		return false;
	}

#if defined(ENABLE_GPU_PROFILING)
	context->End(gGPUProfiling.PresentEnd[gGPUProfiling.CurrentIndex].Get());
#endif
	return true;
}
