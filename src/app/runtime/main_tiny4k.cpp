#include <windows.h>

extern "C" int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    WNDCLASSA windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcA;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "SLTiny4K";

    if (!RegisterClassA(&windowClass)) {
        return 1;
    }

    HWND windowHandle = CreateWindowExA(
        0,
        "SLTiny4K",
        "",
        WS_POPUP | WS_VISIBLE,
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!windowHandle) {
        return 2;
    }

    MSG message;
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return 0;
}
