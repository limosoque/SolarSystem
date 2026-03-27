#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <WinUser.h>
#include <string>

class DisplayWin32
{
public:
    WNDCLASSEXW wc;
    HWND hWnd;
    HINSTANCE hInstance;
    int ClientWidth;
    int ClientHeight;
    HMODULE Module;

    DisplayWin32(LPCWSTR appName, int width, int height, WNDPROC wndProc);
};
