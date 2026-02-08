#include "ShaderLab/App/EditorApp.h"
#include <windows.h>
#include <stdio.h>

using namespace ShaderLab;

// Global app instance
static EditorApp* g_app = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_app) {
        return g_app->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    printf("ShaderLab Editor starting...\n");
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ShaderLabEditorClass";
    if (!RegisterClassExW(&wc)) {
        printf("Failed to register window class\n");
        return -1;
    }
    printf("Window class registered\n");

    // Create window
    const int width = 1920;
    const int height = 1080;

    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        L"ShaderLabEditorClass",
        L"ShaderLab - untitled",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        printf("Failed to create window\n");
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    printf("Window created\n");

    // Create and initialize app
    EditorApp app;
    g_app = &app;

    printf("Initializing application...\n");
    if (!app.Initialize(hwnd, width, height)) {
        printf("Failed to initialize application\n");
        MessageBoxW(hwnd, L"Failed to initialize application. Check that Direct3D 12 is available.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    printf("Application initialized successfully\n");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main loop
    MSG msg = {};
    LARGE_INTEGER frequency, lastTime, currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);

    bool running = true;
    while (running) {
        // Process messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!running) {
            break;
        }

        // Calculate delta time
        QueryPerformanceCounter(&currentTime);
        float deltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / 
                         static_cast<float>(frequency.QuadPart);
        lastTime = currentTime;

        // Update and render
        app.Update(deltaTime);
        app.Render();
    }

    // Cleanup
    app.Shutdown();
    g_app = nullptr;

    return static_cast<int>(msg.wParam);
}
