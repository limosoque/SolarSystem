#pragma once

#define NOMINMAX

#include <windows.h>
#include <wrl.h>
#include <d3d.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <SimpleMath.h>
#include <string>
#include <vector>
#include <chrono>

#include "DisplayWin32.h"
#include "InputDevice.h"
#include "GameComponent.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

class Game
{
public:
    Microsoft::WRL::ComPtr<ID3D11Device>            Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>      Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>           SwapChain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          backBuffer;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   RenderView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> RenderSRV;

    Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> DebugAnnotation;

    HINSTANCE hInstance;
    std::wstring Name;

    std::chrono::time_point<std::chrono::steady_clock> StartTime;
    std::chrono::time_point<std::chrono::steady_clock> PrevTime;
    float TotalTime;

    bool ScreenResized;

    DisplayWin32*               Display;
    InputDevice*                InputDev;
    std::vector<GameComponent*> Components;

    Game(LPCWSTR name, int screenWidth, int screenHeight);
    ~Game();

    void Initialize();
    void Run();
    void Exit();

    void CreateBackBuffer();
    void DestroyResources();
    void PrepareResources();

    void PrepareFrame();
    void Draw();
    void Update(float deltaTime);
    void UpdateInternal();
    void EndFrame();

    void RestoreTargets();

    LRESULT MessageHandler(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);

private:
    bool isExitRequested;
    int screenWidth;
    int screenHeight;

    //fps counter
    float fpsTimer;
    int   fpsCount;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);
};
