#include "DisplayWin32.h"

DisplayWin32::DisplayWin32(LPCWSTR appName, int width, int height, WNDPROC wndProc)
    : ClientWidth(width), 
      ClientHeight(height), 
      hWnd(nullptr), 
      Module(nullptr)
{
    hInstance = GetModuleHandle(nullptr);

    wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wndProc;
    wc.hInstance      = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm       = wc.hIcon;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = appName;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;

    RegisterClassEx(&wc);

    RECT rc = { 0, 0, static_cast<LONG>(ClientWidth), static_cast<LONG>(ClientHeight) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    //center screen
    int posX = (GetSystemMetrics(SM_CXSCREEN) - ClientWidth) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - ClientHeight) / 2;

    hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        appName, appName,
        WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME,
        posX, posY,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);
    ShowCursor(true);
}
