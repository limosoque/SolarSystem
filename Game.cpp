#include "Game.h"
#include <iostream>

//game pointer for static WndProc
static Game* g_Game = nullptr;

LRESULT CALLBACK Game::WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
    if (g_Game)
        return g_Game->MessageHandler(hwnd, umessage, wparam, lparam);
    return DefWindowProc(hwnd, umessage, wparam, lparam);
}

//process system messages
LRESULT Game::MessageHandler(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
    switch (umessage)
    {
    case WM_INPUT:
    {
        UINT dwSize = 0;
        GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lparam),
            RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

        if (dwSize == 0) break;

        auto lpb = std::make_unique<BYTE[]>(dwSize);
        if (GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lparam),
            RID_INPUT, lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
            break;

        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.get());

        if (raw->header.dwType == RIM_TYPEKEYBOARD && InputDev)
        {
            InputDev->OnKeyDown(raw->data.keyboard);

            //Escape
            if (raw->data.keyboard.VKey == VK_ESCAPE &&
                !(raw->data.keyboard.Flags & RI_KEY_BREAK))
            {
                PostQuitMessage(0);
            }
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE && InputDev)
        {
            InputDev->OnMouseMove(raw->data.mouse);
        }

        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, umessage, wparam, lparam);
    }
    return 0;
}
Game::Game(LPCWSTR name, int width, int height)
    : Name(name), 
      screenWidth(width), 
      screenHeight(height),
      Display(nullptr), 
      InputDev(nullptr),
      TotalTime(0.0f), 
      ScreenResized(false), 
      isExitRequested(false),
      fpsTimer(0.0f), 
      fpsCount(0)
{
    g_Game = this;
    hInstance = GetModuleHandle(nullptr);
}

Game::~Game()
{
    DestroyResources();

    for (auto* comp : Components)
        delete comp;
    Components.clear();

    delete InputDev;
    delete Display;

    g_Game = nullptr;
}

void Game::Initialize()
{
    Display = new DisplayWin32(Name.c_str(), screenWidth, screenHeight, WndProc);

    InputDev = new InputDevice(this);

    D3D_FEATURE_LEVEL featureLevel[] = { D3D_FEATURE_LEVEL_11_1 };

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.BufferCount                 = 2;
    swapDesc.BufferDesc.Width            = screenWidth;
    swapDesc.BufferDesc.Height           = screenHeight;
    swapDesc.BufferDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate      = { 60, 1 };
    swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    swapDesc.BufferUsage                 = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow                = Display->hWnd;
    swapDesc.Windowed                    = TRUE;
    swapDesc.SwapEffect                  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Flags                       = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapDesc.SampleDesc                  = { 1, 0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        featureLevel, 1, D3D11_SDK_VERSION,
        &swapDesc, &SwapChain, &Device, nullptr, &Context);

    if (FAILED(hr))
    {
        // Без Debug-слоя
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, featureLevel, 1, D3D11_SDK_VERSION,
            &swapDesc, &SwapChain, &Device, nullptr, &Context);
    }

    if (FAILED(hr))
    {
        std::cerr << "Failed to create D3D11 device!" << std::endl;
        return;
    }

    Context.As(&DebugAnnotation);

    CreateBackBuffer();

    for (auto* comp : Components)
        comp->Initialize();

    StartTime = std::chrono::steady_clock::now();
    PrevTime  = StartTime;
}

void Game::CreateBackBuffer()
{
    SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &RenderView);
}

void Game::DestroyResources()
{
    for (auto* comp : Components)
        comp->DestroyResources();

    RenderView.Reset();
    backBuffer.Reset();
}

void Game::Run()
{
    MSG msg = {};
    isExitRequested = false;

    while (!isExitRequested)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT)
        {
            isExitRequested = true;
            break;
        }

        UpdateInternal();
    }
}

void Game::UpdateInternal()
{
    auto curTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(curTime - PrevTime).count();
    PrevTime = curTime;
    TotalTime += deltaTime;

    //update fps counter
    fpsTimer += deltaTime;
    fpsCount++;
    if (fpsTimer >= 1.0f)
    {
        float fps = fpsCount / fpsTimer;
        wchar_t title[256];
        swprintf_s(title, L"%s - FPS: %.0f", Name.c_str(), fps);
        SetWindowText(Display->hWnd, title);
        fpsTimer -= 1.0f;
        fpsCount = 0;
    }

    Update(deltaTime);

    //Render
    PrepareFrame();
    Draw();
    EndFrame();

    if (InputDev) InputDev->ResetMouseOffset();
}

void Game::Update(float deltaTime)
{
    for (auto* comp : Components)
        comp->Update(deltaTime);
}

void Game::PrepareFrame()
{
    Context->ClearState();
    RestoreTargets();

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(screenWidth);
    vp.Height   = static_cast<float>(screenHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &vp);

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    Context->ClearRenderTargetView(RenderView.Get(), clearColor);
}

void Game::Draw()
{
    for (auto* comp : Components)
        comp->Draw();
}

void Game::RestoreTargets()
{
    ID3D11RenderTargetView* rtvs[] = { RenderView.Get() };
    Context->OMSetRenderTargets(1, rtvs, nullptr);
}

void Game::EndFrame()
{
    Context->OMSetRenderTargets(0, nullptr, nullptr);
    //show frame
    SwapChain->Present(1, 0);
}

void Game::Exit()
{
    isExitRequested = true;
}
