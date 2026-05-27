#pragma once

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <DirectXMath.h>

#include <chrono>
#include <cstdint>
#include <cwchar>
#include <exception>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "AudioSystem.h"
#include "AssetCatalog.h"
#include "GameTypes.h"
#include "ModelLibrary.h"
#include "SpriteLibrary.h"
#include "TextureLibrary.h"

class SweetsApp
{
public:
    bool Initialize(HINSTANCE instance, int showCmd);
    int Run();
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void CreateDevice();
    void CreateRenderTargets();
    void ReleaseRenderTargets();
    void CreateShadersAndStates();
    void CreateMeshes();
    void LoadAssets();
    Mesh CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void Resize(UINT w, UINT h);

    void ResetGame();
    void StartWave();
    void ClearWave();
    void SpawnEnemy();
    void SpawnBoss();
    void BuildStage();
    void UpdateStage(float dt);
    void SpawnPickup();
    void UseUltimate();
    void UseUltimateFor(Player& p, int ownerIndex);
    void UseBomb();
    void UseBombFor(Player& p, int ownerIndex);
    void ApplyLoadout();
    void ApplyLoadout(Player& p, int loadoutIndex, int playerIndex, bool ai);
    void UpdateCoopPlayers(float dt);
    void UpdateAiPlayer(Player& p, int playerIndex, float dt);
    void UpdateGamepadPlayer(Player& p, int playerIndex, float dt);
    void TryRevivePlayers(float dt);
    bool AllPlayersDown() const;
    Player* FindNearestPlayer(V2 pos);
    const Player* FindNearestPlayer(V2 pos) const;
    V2 FindNearestEnemyOrBoss(V2 pos) const;
    float AddScore(int base, const Player* source = nullptr);
    void GrantBossSkill(Player& p);

    void Update(float dt);
    void UpdateTitle(float dt);
    void UpdatePlaying(float dt);
    void UpdateAudioForScreen();
    void UpdatePlayer(float dt);
    void UpdateEnemies(float dt);
    void UpdateBoss(float dt);
    void UpdateShots(float dt);
    void UpdatePickups(float dt);
    void UpdateParticles(float dt);
    void ResolvePlayerHit(float dmg, float angle);
    void ResolvePlayerHit(Player& p, float dmg, float angle);
    void DamageEnemy(Enemy& e, float dmg, V2 from, float knock);
    void DamageEnemy(Enemy& e, float dmg, V2 from, float knock, bool reflected, int ownerIndex);
    void DamageBoss(float dmg);
    void DamageBoss(float dmg, bool reflected, int ownerIndex);
    void Burst(V2 p, Color c, int count);
    void SpawnEnemyShot(V2 pos, float angle, float speed, float damage, float radius, Color color, float ttl = 5.0f, float angularVel = 0.0f, float accel = 0.0f);
    void FirePrimary();
    void FirePrimaryFor(Player& p, int ownerIndex, float aim);
    void FireCharged(Player& p, int ownerIndex, float aim, V2 aimPoint);
    void SpawnSplitShots(const Shot& source, V2 at);
    void DoMelee(float aim);
    void DoMeleeFor(Player& p, int ownerIndex, float aim);

    void Render();
    void DrawScene();
    void DrawHud();
    void DrawLoadoutSelection();
    void DrawCredits();
    void DrawMesh(const Mesh& mesh, const XMMATRIX& world, Color tint);
    void DrawSphere(V2 p, float y, float r, Color c);
    void DrawCylinder(V2 p, float radius, float height, Color c);
    void DrawSector(const Slash& s);
    V2 ScreenToWorld(float sx, float sy) const;

    void OnKeyDown(WPARAM key);
    bool SelectLoadoutAt(float sx, float sy);
    bool KeyDown(int key) const;
    float Rand(float a, float b);
    int RandInt(int a, int b);
    V2 RandInArena(float margin);
    void ClampInside(V2& p, float radius) const;

private:
    HWND hwnd_ = nullptr;
    UINT width_ = 1280;
    UINT height_ = 800;
    bool comInitialized_ = false;

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11Texture2D> depthTex_;
    ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11Buffer> frameCB_;
    ComPtr<ID3D11Buffer> objectCB_;
    ComPtr<ID3D11RasterizerState> rasterState_;
    ComPtr<ID3D11DepthStencilState> depthState_;
    ComPtr<ID3D11BlendState> alphaBlend_;

    ComPtr<ID2D1Factory1> d2dFactory_;
    ComPtr<ID2D1Device> d2dDevice_;
    ComPtr<ID2D1DeviceContext> d2dContext_;
    ComPtr<ID2D1Bitmap1> d2dTarget_;
    ComPtr<ID2D1SolidColorBrush> textBrush_;
    ComPtr<IDWriteFactory> writeFactory_;
    ComPtr<IDWriteTextFormat> hudFormat_;
    ComPtr<IDWriteTextFormat> titleFormat_;
    ComPtr<IDWriteTextFormat> smallFormat_;

    Mesh sphereMesh_;
    Mesh cylinderMesh_;
    Mesh floorMesh_;
    Mesh ringMesh_;
    Mesh cubeMesh_;
    Mesh wedgeMesh_;

    XMMATRIX view_{ XMMatrixIdentity() };
    XMMATRIX proj_{ XMMatrixIdentity() };
    XMFLOAT3 cameraPos_{ 0.0f, 15.5f, -18.5f };

    bool keys_[MaxKeys]{};
    bool mouseLeft_ = false;
    bool prevMouseLeft_ = false;
    bool mouseRight_ = false;
    bool mouseRightReleased_ = false;
    float mouseX_ = 640.0f;
    float mouseY_ = 400.0f;

    Screen screen_ = Screen::Title;
    std::array<Player, MaxPlayers> players_{};
    Player& player_ = players_[0];
    Boss boss_;
    std::vector<Enemy> enemies_;
    std::vector<Shot> shots_;
    std::vector<Slash> slashes_;
    std::vector<Pickup> pickups_;
    std::vector<Obstacle> obstacles_;
    std::vector<Particle> particles_;
    AssetCatalog assetCatalog_;
    AudioSystem audio_;
    TextureLibrary textureLibrary_;
    SpriteLibrary spriteLibrary_;
    ModelLibrary modelLibrary_;

    int wave_ = 1;
    int score_ = 0;
    int reflectKills_ = 0;
    int remainingToSpawn_ = 0;
    int loadoutIndex_ = 0;
    bool bossWave_ = false;
    bool waveStarted_ = false;
    StageType stage_ = StageType::Donut;
    float stageTimer_ = 0.0f;
    float shrinkRadius_ = ArenaRadius;
    float spawnTimer_ = 0.0f;
    float pickupTimer_ = 5.0f;
    float slowT_ = 0.0f;
    float gameTime_ = 0.0f;
    float messageT_ = 0.0f;
    std::wstring message_;

    std::mt19937 rng_{ std::random_device{}() };
    std::chrono::steady_clock::time_point lastTick_{};
};
