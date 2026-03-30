#include "SolarSystemComponent.h"
#include "Game.h"
#include "InputDevice.h"
#include <stdexcept>
#include <iostream>
#include <cmath>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//for clamp work
template<typename T>
static T Clamp(T val, T lo, T hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

SolarSystemComponent::SolarSystemComponent(Game* owner, std::wstring shaderPath)
    : GameComponent(owner)
    , shaderPath_(std::move(shaderPath))
{
}

// ─────────────────────────────────────────────────────────────────────────────
//  Initialize
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::Initialize()
{
    BuildPipeline();
    BuildDepthBuffer();
    BuildMeshes();
    BuildScene();

    GetCursorPos(&lastMouse_);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pipeline (shaders, CB, rasterizer)
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::BuildPipeline()
{
    auto* dev = game->Device.Get();
    HRESULT hr;
    ComPtr<ID3DBlob> vsBlob, psBlob, errors;

    // --- Vertex shader ---
    hr = D3DCompileFromFile(
        shaderPath_.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, vsBlob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[VS] " << (char*)errors->GetBufferPointer() << '\n';
        throw std::runtime_error("VS compilation failed.");
    }

    hr = dev->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, vs_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed.");

    // --- Pixel shader ---
    hr = D3DCompileFromFile(
        shaderPath_.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, psBlob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[PS] " << (char*)errors->GetBufferPointer() << '\n';
        throw std::runtime_error("PS compilation failed.");
    }

    hr = dev->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, ps_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed.");

    // --- Input layout: Position(3) Normal(3) Color(4) ---
    D3D11_INPUT_ELEMENT_DESC elems[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  12,                           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  24,                           D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = dev->CreateInputLayout(elems, static_cast<UINT>(std::size(elems)),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        layout_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed.");

    // --- Constant buffer ---
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.ByteWidth = sizeof(CBPerObject);
    hr = dev->CreateBuffer(&cbd, nullptr, cbPerObject_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer (CB) failed.");

    // --- Rasterizer ---
    CD3D11_RASTERIZER_DESC rd(D3D11_DEFAULT);
    rd.CullMode = D3D11_CULL_BACK;
    rd.FillMode = D3D11_FILL_WIREFRAME;
    rd.FrontCounterClockwise = FALSE;
    hr = dev->CreateRasterizerState(&rd, rastState_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRasterizerState failed.");

    // --- Depth stencil state ---
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    hr = dev->CreateDepthStencilState(&dsd, dss_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilState failed.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Depth buffer
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::BuildDepthBuffer()
{
    auto* dev = game->Device.Get();

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = static_cast<UINT>(game->Display->ClientWidth);
    td.Height = static_cast<UINT>(game->Display->ClientHeight);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = dev->CreateTexture2D(&td, nullptr, dsTexture_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateTexture2D (depth) failed.");

    hr = dev->CreateDepthStencilView(dsTexture_.Get(), nullptr, dsv_.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilView failed.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mesh generation
// ─────────────────────────────────────────────────────────────────────────────

Mesh SolarSystemComponent::CreateSphereMesh(UINT stacks, UINT slices, float radius, XMFLOAT4 color)
{
    std::vector<Vertex> verts;
    std::vector<UINT>   idxs;

    for (UINT i = 0; i <= stacks; ++i)
    {
		//latitude (phi) goes from 0 at top to pi at bottom, so cos(phi) is Y normal, sin(phi) is XZ radius
        float phi = XM_PI * static_cast<float>(i) / static_cast<float>(stacks);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (UINT j = 0; j <= slices; ++j)
        {
			//longitude (theta) goes from 0 to 2pi, so cos(theta) is X normal, sin(theta) is Z normal
            float theta = XM_2PI * static_cast<float>(j) / static_cast<float>(slices);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            //translation from spherical to cartesian
            Vertex v;
			v.Normal = { sinPhi * cosTheta, cosPhi, sinPhi * sinTheta }; 
            v.Position = { v.Normal.x * radius, v.Normal.y * radius, v.Normal.z * radius };

            v.Color = color;
            verts.push_back(v);
        }
    }

    for (UINT i = 0; i < stacks; ++i)
    {
        for (UINT j = 0; j < slices; ++j)
        {
            UINT a = i * (slices + 1) + j;
            UINT b = a + slices + 1;
            idxs.push_back(a);     idxs.push_back(b);     idxs.push_back(a + 1);
            idxs.push_back(b);     idxs.push_back(b + 1); idxs.push_back(a + 1);
        }
    }

    Mesh mesh;
    auto* dev = game->Device.Get();

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * verts.size());
    D3D11_SUBRESOURCE_DATA vsd = { verts.data() };
    dev->CreateBuffer(&vbd, &vsd, mesh.VertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = static_cast<UINT>(sizeof(UINT) * idxs.size());
    D3D11_SUBRESOURCE_DATA isd = { idxs.data() };
    dev->CreateBuffer(&ibd, &isd, mesh.IndexBuffer.GetAddressOf());

    mesh.IndexCount = static_cast<UINT>(idxs.size());
    return mesh;
}

Mesh SolarSystemComponent::CreateBoxMesh(float w, float h, float d, XMFLOAT4 color)
{
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

    // 6 faces, 4 verts each, with proper normals
    XMFLOAT3 normals[6] = {
        { 0, 0,-1}, { 0, 0, 1},
        {-1, 0, 0}, { 1, 0, 0},
        { 0,-1, 0}, { 0, 1, 0}
    };
    // Each face as two triangles (6 vertices)
    // We'll use indexed: 4 verts per face, 6 indices per face
    struct FaceCorner { float x, y, z; };
    FaceCorner faceVerts[6][4] = {
        // -Z
        {{-hw,-hh,-hd},{-hw, hh,-hd},{ hw, hh,-hd},{ hw,-hh,-hd}},
        // +Z
        {{ hw,-hh, hd},{ hw, hh, hd},{-hw, hh, hd},{-hw,-hh, hd}},
        // -X
        {{-hw,-hh, hd},{-hw, hh, hd},{-hw, hh,-hd},{-hw,-hh,-hd}},
        // +X
        {{ hw,-hh,-hd},{ hw, hh,-hd},{ hw, hh, hd},{ hw,-hh, hd}},
        // -Y
        {{-hw,-hh, hd},{-hw,-hh,-hd},{ hw,-hh,-hd},{ hw,-hh, hd}},
        // +Y
        {{-hw, hh,-hd},{-hw, hh, hd},{ hw, hh, hd},{ hw, hh,-hd}},
    };

    std::vector<Vertex> verts;
    std::vector<UINT>   idxs;
    for (int f = 0; f < 6; ++f)
    {
        UINT base = static_cast<UINT>(verts.size());
        for (int v = 0; v < 4; ++v)
        {
            Vertex vtx;
            vtx.Position = { faceVerts[f][v].x, faceVerts[f][v].y, faceVerts[f][v].z };
            vtx.Normal = normals[f];
            vtx.Color = color;
            verts.push_back(vtx);
        }
        idxs.push_back(base + 0); idxs.push_back(base + 1); idxs.push_back(base + 2);
        idxs.push_back(base + 0); idxs.push_back(base + 2); idxs.push_back(base + 3);
    }

    Mesh mesh;
    auto* dev = game->Device.Get();

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * verts.size());
    D3D11_SUBRESOURCE_DATA vsd = { verts.data() };
    dev->CreateBuffer(&vbd, &vsd, mesh.VertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = static_cast<UINT>(sizeof(UINT) * idxs.size());
    D3D11_SUBRESOURCE_DATA isd = { idxs.data() };
    dev->CreateBuffer(&ibd, &isd, mesh.IndexBuffer.GetAddressOf());

    mesh.IndexCount = static_cast<UINT>(idxs.size());
    return mesh;
}

void SolarSystemComponent::BuildMeshes()
{
    // Mesh 0 = sphere (unit radius, scaled per body)
    mSphereMesh_ = CreateSphereMesh(24, 24, 1.0f, { 1,1,1,1 });
    // Mesh 1 = box (unit box, scaled per body)
    mBoxMesh_ = CreateBoxMesh(1.0f, 1.0f, 1.0f, { 1,1,1,1 });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scene graph — Solar System layout
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::BuildScene()
{
    // Index layout:
    //  0 = Sun
    //  1 = Mercury    (planet)
    //  2 = Venus      (planet)
    //  3 = Earth      (planet)
    //  4 = Moon       (moon of Earth)
    //  5 = Mars       (planet)
    //  6 = Phobos     (moon of Mars, box)
    //  7 = Deimos     (moon of Mars, box)
    //  8 = Jupiter    (planet)
    //  9 = Io         (moon of Jupiter)
    // 10 = Europa     (moon of Jupiter)
    // 11 = Saturn     (planet)
    // 12 = Titan      (moon of Saturn)
    // 13 = Uranus     (planet)
    // 14 = Uranus ring-station (box, moon of Uranus)
    // 15 = Neptune    (planet)
    // 16 = Triton     (moon of Neptune)

    bodies_.clear();
    bodies_.reserve(17);

    // Helper lambda
    auto add = [&](const char* name, BodyShape shape, XMFLOAT4 col,
        int parent, float orbitR, float orbitSpd, float spinSpd,
        float scale, float initAngle = 0.0f)
        {
            OrbitalBody b;
            b.Name = name;
            b.Shape = shape;
            b.Color = col;
            b.ParentIndex = parent;
            b.OrbitRadius = orbitR;
            b.OrbitSpeed = orbitSpd;
            b.SpinSpeed = spinSpd;
            b.Scale = scale;
            b.OrbitAngle = initAngle;
            b.MeshIndex = (shape == BodyShape::Sphere) ? 0 : 1;
            bodies_.push_back(b);
        };

    //                  Name        Shape              Color                    Par  Orbit  OSpd   Spin   Scale  Phase
    add("Sun", BodyShape::Sphere, { 1.0f,0.9f,0.1f,1 }, -1, 0.0f, 0.00f, 0.20f, 2.2f, 0.0f); // 0
    add("Mercury", BodyShape::Sphere, { 0.6f,0.5f,0.5f,1 }, 0, 3.8f, 1.60f, 0.60f, 0.22f, 0.3f); // 1
    add("Venus", BodyShape::Sphere, { 0.9f,0.7f,0.3f,1 }, 0, 5.5f, 1.17f, 0.40f, 0.55f, 1.1f); // 2
    add("Earth", BodyShape::Sphere, { 0.2f,0.5f,1.0f,1 }, 0, 7.5f, 1.00f, 1.00f, 0.60f, 2.4f); // 3
    add("Moon", BodyShape::Sphere, { 0.7f,0.7f,0.7f,1 }, 3, 1.2f, 3.50f, 0.50f, 0.17f, 0.0f); // 4
    add("Mars", BodyShape::Sphere, { 0.8f,0.3f,0.1f,1 }, 0, 9.8f, 0.80f, 0.97f, 0.45f, 0.7f); // 5
    add("Phobos", BodyShape::Box, { 0.5f,0.4f,0.4f,1 }, 5, 0.9f, 5.00f, 2.00f, 0.10f, 0.0f); // 6
    add("Deimos", BodyShape::Box, { 0.4f,0.4f,0.3f,1 }, 5, 1.4f, 3.20f, 1.50f, 0.08f, 1.6f); // 7
    add("Jupiter", BodyShape::Sphere, { 0.8f,0.6f,0.4f,1 }, 0, 13.5f, 0.43f, 2.40f, 1.10f, 1.8f); // 8
    add("Io", BodyShape::Sphere, { 0.9f,0.8f,0.1f,1 }, 8, 1.8f, 4.00f, 1.50f, 0.20f, 0.5f); // 9
    add("Europa", BodyShape::Sphere, { 0.7f,0.8f,0.9f,1 }, 8, 2.6f, 2.80f, 1.20f, 0.17f, 2.5f); //10
    add("Saturn", BodyShape::Sphere, { 0.9f,0.8f,0.5f,1 }, 0, 18.0f, 0.32f, 2.20f, 0.95f, 3.5f); //11
    add("Titan", BodyShape::Sphere, { 0.8f,0.6f,0.2f,1 }, 11, 2.2f, 1.80f, 1.00f, 0.25f, 1.0f); //12
    add("Uranus", BodyShape::Sphere, { 0.5f,0.8f,0.9f,1 }, 0, 22.5f, 0.23f, 1.70f, 0.75f, 0.9f); //13
    add("RingStation", BodyShape::Box, { 0.6f,0.9f,0.6f,1 }, 13, 1.5f, 2.50f, 3.00f, 0.18f, 0.0f); //14
    add("Neptune", BodyShape::Sphere, { 0.2f,0.3f,0.9f,1 }, 0, 26.5f, 0.18f, 1.60f, 0.73f, 5.2f); //15
    add("Triton", BodyShape::Sphere, { 0.5f,0.6f,0.7f,1 }, 15, 1.8f, 2.20f, 0.90f, 0.18f, 3.1f); //16
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::Update(float dt)
{
    HandleInput(dt);
    UpdateCamera(dt);

    if (!isPaused_) {
        for (auto& body : bodies_)
        {
            body.OrbitAngle += body.OrbitSpeed * dt;
            body.SpinAngle += body.SpinSpeed * dt;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Input handling
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::HandleInput(float dt)
{
    auto* input = game->InputDev;

    // ── Toggle camera mode: C ──────────────────────────────────────────────
    bool curC = input->IsKeyDown('C');
    if (curC && !prevC_)
    {
        cameraMode_ = (cameraMode_ == CameraMode::Orbital)
            ? CameraMode::FPS
            : CameraMode::Orbital;
        std::cout << "Camera mode: "
            << (cameraMode_ == CameraMode::FPS ? "FPS" : "Orbital") << '\n';
    }
    prevC_ = curC;

    // ── Cycle projection: P ────────────────────────────────────────────────
    bool curP = input->IsKeyDown('P');
    if (curP && !prevP_)
    {
        projPreset_ = static_cast<ProjectionPreset>(
            (static_cast<int>(projPreset_) + 1) % 4);
        const char* names[] = { "Normal", "NarrowFOV", "WideFOV", "Orthographic" };
        std::cout << "Projection: " << names[static_cast<int>(projPreset_)] << '\n';
    }
    prevP_ = curP;

	// ── Toggle pause: Space ───────────────────────────────────────────────
	bool curSpace = input->IsKeyDown(VK_SPACE);
	if (curSpace && !prevSpace_)
    {
        isPaused_ = !isPaused_;
        std::cout << "Simulation: " << (isPaused_ ? "Paused\n" : "Running\n");
    }
	prevSpace_ = curSpace;

    // ── Mouse delta ────────────────────────────────────────────────────────
    POINT cur;
    GetCursorPos(&cur);
    float dx = static_cast<float>(cur.x - lastMouse_.x);
    float dy = static_cast<float>(cur.y - lastMouse_.y);
    lastMouse_ = cur;

    bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    if (cameraMode_ == CameraMode::Orbital)
    {
        // Orbit: RMB drag to rotate, scroll to zoom
        if (rmb)
        {
            camYaw_ += dx * 0.005f;
            camPitch_ += dy * 0.005f;
            camPitch_ = Clamp(camPitch_, -XM_PIDIV2 + 0.05f, XM_PIDIV2 - 0.05f);
        }
        // Zoom: W/S
        if (input->IsKeyDown('W')) camDist_ -= 15.0f * dt;
        if (input->IsKeyDown('S')) camDist_ += 15.0f * dt;
        camDist_ = Clamp(camDist_, 3.0f, 60.0f);
    }
    else // FPS
    {
        // RMB drag → look
        if (rmb)
        {
            fpsYaw_ += dx * 0.003f;
            fpsPitch_ += dy * 0.003f;
            fpsPitch_ = Clamp(fpsPitch_, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);
        }
        // WASD move
        float speed = 12.0f * dt;
        XMVECTOR fwd = XMVector3Normalize(
            XMVectorSet(std::sin(fpsYaw_), 0, std::cos(fpsYaw_), 0));
        XMVECTOR right = XMVector3Normalize(
            XMVectorSet(std::cos(fpsYaw_), 0, -std::sin(fpsYaw_), 0));

        XMVECTOR pos = XMLoadFloat3(&fpsPos_);
        if (input->IsKeyDown('W')) pos = XMVectorAdd(pos, XMVectorScale(fwd, speed));
        if (input->IsKeyDown('S')) pos = XMVectorAdd(pos, XMVectorScale(fwd, -speed));
        if (input->IsKeyDown('A')) pos = XMVectorAdd(pos, XMVectorScale(right, -speed));
        if (input->IsKeyDown('D')) pos = XMVectorAdd(pos, XMVectorScale(right, speed));
        if (input->IsKeyDown('Q')) pos = XMVectorAdd(pos, XMVectorSet(0, speed, 0, 0));
        if (input->IsKeyDown('E')) pos = XMVectorAdd(pos, XMVectorSet(0, -speed, 0, 0));
        XMStoreFloat3(&fpsPos_, pos);
    }
}

void SolarSystemComponent::UpdateCamera(float /*dt*/)
{
    // Nothing extra needed — view matrix is rebuilt each Draw()
}

// ─────────────────────────────────────────────────────────────────────────────
//  Camera / Projection matrices
// ─────────────────────────────────────────────────────────────────────────────

XMMATRIX SolarSystemComponent::GetViewMatrix() const
{
    if (cameraMode_ == CameraMode::Orbital)
    {
        // Spherical orbit around camTarget_
        float x = camDist_ * std::cos(camPitch_) * std::sin(camYaw_);
        float y = camDist_ * std::sin(camPitch_);
        float z = camDist_ * std::cos(camPitch_) * std::cos(camYaw_);
        XMVECTOR eye = XMVectorSet(x, y, z, 1);
        XMVECTOR target = XMLoadFloat3(&camTarget_);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        return XMMatrixLookAtLH(eye, target, up);
    }
    else
    {
        // FPS: yaw+pitch
        XMVECTOR fwd = XMVectorSet(
            std::sin(fpsYaw_) * std::cos(fpsPitch_),
            -std::sin(fpsPitch_),
            std::cos(fpsYaw_) * std::cos(fpsPitch_), 0);
        XMVECTOR eye = XMLoadFloat3(&fpsPos_);
        return XMMatrixLookAtLH(eye, XMVectorAdd(eye, fwd), XMVectorSet(0, 1, 0, 0));
    }
}

XMMATRIX SolarSystemComponent::GetProjectionMatrix() const
{
    float w = static_cast<float>(game->Display->ClientWidth);
    float h = static_cast<float>(game->Display->ClientHeight);
    float aspect = w / h;
    float nearZ = 0.1f, farZ = 300.0f;

    switch (projPreset_)
    {
    case ProjectionPreset::NarrowFOV:
        return XMMatrixPerspectiveFovLH(XMConvertToRadians(30.0f), aspect, nearZ, farZ);
    case ProjectionPreset::WideFOV:
        return XMMatrixPerspectiveFovLH(XMConvertToRadians(110.0f), aspect, nearZ, farZ);
    case ProjectionPreset::Orthographic:
        // Scale ortho so scene is visible
        return XMMatrixOrthographicLH(camDist_ * aspect, camDist_, nearZ, farZ);
    default: // Normal
        return XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, nearZ, farZ);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Draw
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::Draw()
{
    auto* ctx = game->Context.Get();

    // Bind RTV + DSV
    ID3D11RenderTargetView* rtv = game->RenderView.Get();
    ctx->OMSetRenderTargets(1, &rtv, dsv_.Get());

    // Clear depth
    ctx->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    ctx->OMSetDepthStencilState(dss_.Get(), 0);
    ctx->RSSetState(rastState_.Get());
    ctx->IASetInputLayout(layout_.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, cbPerObject_.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, cbPerObject_.GetAddressOf());

    XMMATRIX view = GetViewMatrix();
    XMMATRIX proj = GetProjectionMatrix();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    // Compute world position for each body.
    // We need world centers to resolve parent transforms.
    std::vector<XMMATRIX> worldTransforms(bodies_.size(), XMMatrixIdentity());

    // First pass: compute world positions (translation only, for orbital reference)
    // We separate the spin from orbit so moons orbit around the parent's center
    std::vector<XMVECTOR> worldPositions(bodies_.size(), XMVectorZero());

    for (size_t i = 0; i < bodies_.size(); ++i)
    {
        const auto& body = bodies_[i];
        float cx = body.OrbitRadius * std::cos(body.OrbitAngle);
        float cz = body.OrbitRadius * std::sin(body.OrbitAngle);
        XMVECTOR localPos = XMVectorSet(cx, 0, cz, 0);

        if (body.ParentIndex >= 0)
            localPos = XMVectorAdd(localPos, worldPositions[body.ParentIndex]);

        worldPositions[i] = localPos;
    }

    // Second pass: build full world matrix (translate to world pos, spin, scale)
    for (size_t i = 0; i < bodies_.size(); ++i)
    {
        const auto& body = bodies_[i];
        XMMATRIX S = XMMatrixScaling(body.Scale, body.Scale, body.Scale);
        XMMATRIX R = XMMatrixRotationY(body.SpinAngle);

        XMFLOAT3 wpos;
        XMStoreFloat3(&wpos, worldPositions[i]);
        XMMATRIX T = XMMatrixTranslation(wpos.x, wpos.y, wpos.z);

        worldTransforms[i] = XMMatrixMultiply(XMMatrixMultiply(S, R), T);
    }

    // Third pass: draw
    for (size_t i = 0; i < bodies_.size(); ++i)
    {
        const auto& body = bodies_[i];
        UpdateCB(worldTransforms[i], viewProj, body.Color);
        const Mesh& mesh = (body.MeshIndex == 0) ? mSphereMesh_ : mBoxMesh_;
        DrawMesh(mesh);
    }
}

void SolarSystemComponent::DrawMesh(const Mesh& mesh)
{
    auto* ctx = game->Context.Get();
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, mesh.VertexBuffer.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(mesh.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->DrawIndexed(mesh.IndexCount, 0, 0);
}

void SolarSystemComponent::UpdateCB(const XMMATRIX& world,
    const XMMATRIX& vp,
    XMFLOAT4        color)
{
    auto* ctx = game->Context.Get();
    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(cbPerObject_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    auto* cb = reinterpret_cast<CBPerObject*>(mapped.pData);
    cb->World = XMMatrixTranspose(world);
    cb->ViewProj = XMMatrixTranspose(vp);
    cb->BaseColor = color;
    ctx->Unmap(cbPerObject_.Get(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cleanup
// ─────────────────────────────────────────────────────────────────────────────

void SolarSystemComponent::DestroyResources()
{
    dss_.Reset();
    dsv_.Reset();
    dsTexture_.Reset();
    rastState_.Reset();
    cbPerObject_.Reset();
    layout_.Reset();
    ps_.Reset();
    vs_.Reset();

    mSphereMesh_.VertexBuffer.Reset();
    mSphereMesh_.IndexBuffer.Reset();
    mBoxMesh_.VertexBuffer.Reset();
    mBoxMesh_.IndexBuffer.Reset();
}