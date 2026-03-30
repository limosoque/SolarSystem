#pragma once
#include "GameComponent.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ─── Constant buffer layout ───────────────────────────────────────────────────

struct alignas(16) CBPerObject
{
    XMMATRIX World;
    XMMATRIX ViewProj;
    XMFLOAT4 BaseColor;
};

// ─── Mesh data ────────────────────────────────────────────────────────────────

struct Vertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
};

struct Mesh
{
    ComPtr<ID3D11Buffer> VertexBuffer;
    ComPtr<ID3D11Buffer> IndexBuffer;
    UINT                 IndexCount = 0;
};

// ─── Orbital body description ─────────────────────────────────────────────────

enum class BodyShape { Sphere, Box };

struct OrbitalBody
{
    std::string  Name;
    BodyShape    Shape = BodyShape::Sphere;
    XMFLOAT4     Color = { 1,1,1,1 };

    // Hierarchy
    int          ParentIndex = -1;       // -1 = root (sun)

    // Orbit
    float        OrbitRadius = 0.0f;     // distance from parent
    float        OrbitSpeed = 0.0f;     // radians/sec
    float        OrbitAngle = 0.0f;     // current angle (runtime)

    // Self-rotation
    float        SpinSpeed = 1.0f;     // radians/sec
    float        SpinAngle = 0.0f;     // current angle (runtime)

    // Scale
    float        Scale = 1.0f;

    // Mesh index (into SolarSystemComponent::mMeshes)
    int          MeshIndex = 0;        // 0 = sphere, 1 = box
};

// ─── Camera modes ─────────────────────────────────────────────────────────────

enum class CameraMode { Orbital, FPS };

// ─── Projection presets ───────────────────────────────────────────────────────

enum class ProjectionPreset { Normal, NarrowFOV, WideFOV, Orthographic };

// ─── SolarSystemComponent ────────────────────────────────────────────────────

class SolarSystemComponent : public GameComponent
{
public:
    explicit SolarSystemComponent(Game* owner,
        std::wstring shaderPath = L"./Shaders/SolarSystem.hlsl");

    void Initialize()       override;
    void Update(float dt)   override;
    void Draw()             override;
    void DestroyResources() override;

private:
    // ── Shader / Pipeline ──────────────────────────────────────────────────
    std::wstring shaderPath_;

    ComPtr<ID3D11VertexShader>    vs_;
    ComPtr<ID3D11PixelShader>     ps_;
    ComPtr<ID3D11InputLayout>     layout_;
    ComPtr<ID3D11Buffer>          cbPerObject_;
    ComPtr<ID3D11RasterizerState> rastState_;
    ComPtr<ID3D11DepthStencilView>   dsv_;
    ComPtr<ID3D11Texture2D>          dsTexture_;
    ComPtr<ID3D11DepthStencilState>  dss_;

    // ── Meshes ─────────────────────────────────────────────────────────────
    Mesh mSphereMesh_;
    Mesh mBoxMesh_;

    // ── Scene graph ────────────────────────────────────────────────────────
    std::vector<OrbitalBody> bodies_;

    // ── Camera ────────────────────────────────────────────────────────────
    CameraMode   cameraMode_ = CameraMode::Orbital;

    // Orbital camera
    float        camYaw_ = 0.0f;
    float        camPitch_ = 0.3f;
    float        camDist_ = 20.0f;
    XMFLOAT3     camTarget_ = { 0,0,0 };

    // FPS camera
    XMFLOAT3     fpsPos_ = { 0, 5, -25 };
    float        fpsYaw_ = 0.0f;
    float        fpsPitch_ = 0.0f;

    // ── Projection ────────────────────────────────────────────────────────
    ProjectionPreset projPreset_ = ProjectionPreset::Normal;

    // ── Input state ────────────────────────────────────────────────────────
    POINT        lastMouse_ = {};
    bool         mouseTracking_ = false;
	bool         isPaused_ = false;

    // ── Key-repeat guard ──────────────────────────────────────────────────
    bool prevC_ = false;
    bool prevP_ = false;
	bool prevSpace_ = false;

    // ── Helpers ────────────────────────────────────────────────────────────
    void BuildPipeline();
    void BuildDepthBuffer();
    void BuildMeshes();
    void BuildScene();

    Mesh CreateSphereMesh(UINT stacks, UINT slices, float radius, XMFLOAT4 color);
    Mesh CreateBoxMesh(float w, float h, float d, XMFLOAT4 color);

    void DrawBody(const OrbitalBody& body,
        const XMMATRIX& parentWorld,
        const XMMATRIX& viewProj);

    XMMATRIX GetViewMatrix()       const;
    XMMATRIX GetProjectionMatrix() const;

    void UpdateCamera(float dt);
    void HandleInput(float dt);

    void DrawMesh(const Mesh& mesh);
    void UpdateCB(const XMMATRIX& world, const XMMATRIX& vp, XMFLOAT4 color);
};