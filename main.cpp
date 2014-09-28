#include "precompiled.h"

#include "DemoRendererApplication.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch(message)
    {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	Logging::Logger::Initialize();
	Logging::Logger::Get().AddTarget(new std::ofstream("DemoRendererApplication_App.log"));

	auto app = std::unique_ptr<DemoRendererApplication>(new DemoRendererApplication(hInstance));

	unsigned width = 1280;
	unsigned height = 720;
	std::wstring cmdLine(lpCmdLine);
	if(cmdLine.size())
	{
		std::wistringstream sin(cmdLine);
		sin >> width;
		sin >> height;
	}

	const int samplesCount = 1;
	if (app->Initiate("DemoRendererApp", "DemoRenderer application", width, height, false, &WndProc, true, samplesCount))
	{
		app->DoMessageLoop();

		app.reset();
	}

	Logging::Logger::Deinitialize();

	return 0;
}