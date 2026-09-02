#include "MainApp.h"

#include <Windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command)
{
    auto main_application = Client::MainApp::create(instance, show_command);
    if (!main_application)
    {
        MessageBoxW(nullptr, L"WBOIT benchmark initialization failed.", L"Initialization Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    return main_application->run();
}
