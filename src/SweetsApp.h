#pragma once

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
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
#include "EffekseerSystem.h"
#include "GameTypes.h"
#include "ModelLibrary.h"
#include "SpriteLibrary.h"
#include "SpriteCanvas.h"
#include "TextureLibrary.h"
#include "VideoSystem.h"

class SweetsApp
{
public:
    bool Initialize(HINSTANCE instance, int showCmd);
    int Run();
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void CreateDevice();
    void CreateFrameTargets();
    void ReleaseFrameTargets();
    void CreateShadersAndStates();
    void CreateOffscreenTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& rtv, ComPtr<ID3D11ShaderResourceView>& srv, DXGI_FORMAT format);
    void CreateMeshes();
    void LoadAssets();
    void LoadTitleAssets();
    void LoadGameplayAssets();
    void LoadEffectAssets();
    void EnsureGameplayAssetsReady();
    Mesh CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void Resize(UINT w, UINT h);

    void ResetGame();
    void StartGameWithDifficulty(bool hiddenBossPractice);
    void StartHiddenBoss();
    void PrepareHiddenBossResources();
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
    bool Use3DRules() const;
    void SetGameplayDimension(GameplayDimension dimension);
    void SyncAll3DState();
    void SyncPlayer3D(Player& p);
    void SyncEnemy3D(Enemy& e);
    void SyncBoss3D(Boss& b);
    void SyncShot3D(Shot& s);
    void SyncPickup3D(Pickup& p);
    void SyncObstacle3D(Obstacle& o);
    void SyncSlash3D(Slash& s);
    V3 Grounded3D(V2 pos, float y) const;
    float RuleDistance(V2 a, float ay, V2 b, float by) const;
    float RuleDistance(const Shot& s, const Player& p) const;
    float RuleDistance(const Shot& s, const Enemy& e) const;
    float RuleDistance(const Shot& s, const Boss& b) const;
    float RuleDistance(const Player& p, const Enemy& e) const;
    float RuleDistance(const Player& p, const Pickup& item) const;
    bool RuleCircleHit(V2 a, float ay, float ar, V2 b, float by, float br) const;
    float AddScore(int base, const Player* source = nullptr);
    void GrantBossSkill(Player& p);

    void Update(float dt);
    void UpdateBootLoading(float dt);
    void UpdateGameplayLoading(float dt);
    void UpdateTitle(float dt);
    void OpenTitleVideoIfReady(float dt);
    void AdvanceLoadPhase(LoadPhase nextPhase, std::wstring step);
    void UpdateDebugTiming(float dt);
    void UpdateClear(float dt);
    void UpdateHiddenBossIntro(float dt);
    void UpdateHiddenBoss(float dt);
    void UpdateEventVideo(float dt);
    void UpdatePlaying(float dt);
    void UpdateAudioForScreen();
    void ApplyAudioVolume();
    void NormalizePlayerLifeStates();
    void UpdatePlayer(float dt);
    void UpdateEnemies(float dt);
    void UpdateBoss(float dt);
    void UpdateShots(float dt);
    void UpdatePickups(float dt);
    void UpdateParticles(float dt);
    void UpdateEffectVisuals(float dt);
    void ResolvePlayerHit(float dmg, float angle);
    void ResolvePlayerHit(Player& p, float dmg, float angle);
    void DamageEnemy(Enemy& e, float dmg, V2 from, float knock);
    void DamageEnemy(Enemy& e, float dmg, V2 from, float knock, bool reflected, int ownerIndex);
    void DamageBoss(float dmg);
    void DamageBoss(float dmg, bool reflected, int ownerIndex);
    void Burst(V2 p, Color c, int count);
    void PlayCombatEffect(const std::wstring& id, V2 position, float y, float rotationY, float scale, Color fallbackColor, int fallbackCount);
    void SpawnEnemyShot(V2 pos, float angle, float speed, float damage, float radius, Color color, float ttl = 5.0f, float angularVel = 0.0f, float accel = 0.0f);
    int ScaledBulletCount(int base) const;
    const DifficultyDef& CurrentDifficulty() const;
    EncounterProfile CurrentEncounterProfile() const;
    const EncounterTuning& CurrentEncounterTuning() const;
    int EliteEnemyCount() const;
    int BossAddCount() const;
    void LoadProgress();
    void SaveProgress();
    int DifficultyOptionCount() const;
    void FirePrimary();
    void FirePrimaryFor(Player& p, int ownerIndex, float aim);
    void FireCharged(Player& p, int ownerIndex, float aim, V2 aimPoint);
    void SpawnSplitShots(const Shot& source, V2 at);
    void DoMelee(float aim);
    void DoMeleeFor(Player& p, int ownerIndex, float aim);

    void PresentFrame();
    void DrawScene();
    void DrawGameplay3D();
    void DrawAdditiveScene();
    void CompositeScene();
    void DrawHud();
    void DrawDebugHud();
    void DrawBootLoading();
    void DrawScreenFlashOverlay();
    void DrawCharacterSelect();
    void DrawPauseMenu();
    void DrawVideoScreen();
    void DrawTitleMediaFrame(const D2D1_RECT_F& rect);
    void DrawBitmapCover(ID2D1Bitmap1* bitmap, const D2D1_RECT_F& rect, float opacity);
    void LoadTitleImageBitmap();
    void DrawLoadoutSelection();
    void DrawCoopSlotSelection();
    void DrawDifficultySelection();
    void DrawClearScreen();
    void DrawHiddenBossIntro();
    void DrawCredits();
    void DrawMesh(const Mesh& mesh, const XMMATRIX& world, Color tint);
    void DrawSphere(V2 p, float y, float r, Color c);
    void DrawCylinder(V2 p, float radius, float height, Color c);
    void DrawPickupShape(const Pickup& p);
    void DrawPickupShape3D(const Pickup& p);
    void DrawSector(const Slash& s);
    void DrawSector3D(const Slash& s);
    void DrawUltimatePreview(const Player& p, int ownerIndex);
    void DrawSprite2D(const std::wstring& spriteId, V2 pos, V2 size, float rotation, Color tint, float depth = 0.5f);
    V2 ScreenToWorld(float sx, float sy) const;

    void OnKeyDown(WPARAM key);
    bool HandleDebugKey(WPARAM key);
    bool HandleDebugClick(float sx, float sy);
    bool DebugPanelContains(float sx, float sy) const;
    void ExecuteDebugAction(int action);
    void RestartCurrentRun();
    void StartSelectedTitleItem();
    void ActivatePauseMenuItem();
    bool HandlePauseClick(float sx, float sy);
    bool HandlePauseDrag(float sx, float sy);
    void SetVolumeSlider(int index, float value, bool save);
    float VolumeSliderValue(int index) const;
    float* MutableVolumeSliderValue(int index);
    void SaveSettings();
    void PlayVideo(const std::wstring& relativePath, Screen nextScreen, bool skippable);
    bool SelectLoadoutAt(float sx, float sy);
    bool SelectCoopSlotAt(float sx, float sy);
    bool SelectTitleMenuAt(float sx, float sy);
    bool SelectDifficultyAt(float sx, float sy);
    bool SelectCreditsAt(float sx, float sy);
    bool SelectGameOverAt(float sx, float sy);
    bool SelectClearAt(float sx, float sy);
    bool KeyDown(int key) const;
    float Rand(float a, float b);
    int RandInt(int a, int b);
    V2 RandInArena(float margin);
    void ClampInside(V2& p, float radius) const;
    void ClampInside(V3& p, float radius) const;

private:
    HWND hwnd_ = nullptr;
    UINT width_ = 1280;
    UINT height_ = 800;
    bool comInitialized_ = false;

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11Texture2D> backBufferTex_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11Texture2D> sceneColorTex_;
    ComPtr<ID3D11RenderTargetView> sceneColorRtv_;
    ComPtr<ID3D11ShaderResourceView> sceneColorSrv_;
    ComPtr<ID3D11Texture2D> additiveTex_;
    ComPtr<ID3D11RenderTargetView> additiveRtv_;
    ComPtr<ID3D11ShaderResourceView> additiveSrv_;
    ComPtr<ID3D11Texture2D> historyTex_;
    ComPtr<ID3D11RenderTargetView> historyRtv_;
    ComPtr<ID3D11ShaderResourceView> historySrv_;
    ComPtr<ID3D11Texture2D> resolvedTex_;
    ComPtr<ID3D11RenderTargetView> resolvedRtv_;
    ComPtr<ID3D11ShaderResourceView> resolvedSrv_;
    ComPtr<ID3D11Texture2D> depthTex_;
    ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11VertexShader> postVs_;
    ComPtr<ID3D11PixelShader> postPs_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11Buffer> frameCB_;
    ComPtr<ID3D11Buffer> objectCB_;
    ComPtr<ID3D11Buffer> postCB_;
    ComPtr<ID3D11RasterizerState> rasterState_;
    ComPtr<ID3D11DepthStencilState> depthState_;
    ComPtr<ID3D11BlendState> alphaBlend_;
    ComPtr<ID3D11BlendState> additiveBlend_;
    ComPtr<ID3D11SamplerState> postSampler_;

    ComPtr<ID2D1Factory1> d2dFactory_;
    ComPtr<ID2D1Device> d2dDevice_;
    ComPtr<ID2D1DeviceContext> d2dContext_;
    ComPtr<ID2D1Bitmap1> d2dTarget_;
    ComPtr<ID2D1Bitmap1> titleVideoBitmap_;
    ComPtr<ID2D1Bitmap1> titleImageBitmap_;
    ComPtr<ID2D1Bitmap1> eventVideoBitmap_;
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
    SpriteCanvas spriteCanvas_;

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

    Screen screen_ = Screen::BootLoading;
    std::array<Player, MaxPlayers> players_{};
    Player& player_ = players_[0];
    Boss boss_;
    std::vector<Enemy> enemies_;
    std::vector<Shot> shots_;
    std::vector<Slash> slashes_;
    std::vector<Pickup> pickups_;
    std::vector<Obstacle> obstacles_;
    std::vector<Particle> particles_;
    std::vector<EffectPulse> effectPulses_;
    std::vector<SwordEffectVisual> swordEffectVisuals_;
    AssetCatalog assetCatalog_;
    EffekseerSystem effekseer_;
    AudioSystem audio_;
    VideoSystem titleVideo_;
    VideoSystem eventVideo_;
    TextureLibrary textureLibrary_;
    SpriteLibrary spriteLibrary_;
    ModelLibrary modelLibrary_;
    ComPtr<IWICImagingFactory> wicFactory_;

    int wave_ = 1;
    int score_ = 0;
    int reflectKills_ = 0;
    int remainingToSpawn_ = 0;
    int loadoutIndex_ = 0;
    std::array<int, MaxPlayers> coopLoadoutIndices_{ 0, 1, 2, 3 };
    std::array<CoopSlotMode, MaxPlayers> coopSlotModes_{ CoopSlotMode::Pad, CoopSlotMode::Off, CoopSlotMode::Off, CoopSlotMode::Off };
    int titleMenuIndex_ = 0;
    int difficultyIndex_ = 1;
    int pauseMenuIndex_ = 0;
    int draggingVolume_ = -1;
    Difficulty difficulty_ = Difficulty::Normal;
    GameMode gameMode_ = GameMode::Story;
    GameMode pendingGameMode_ = GameMode::Story;
    GameOverChoice gameOverChoice_ = GameOverChoice::Retry;
    GameplayDimension gameplayDimension_ = GameplayDimension::TwoD;
    DebugState debug_;
    LoadPhase loadPhase_ = LoadPhase::Boot;
    bool hiddenBossUnlocked_ = false;
    bool hiddenBossPractice_ = false;
    bool pendingHiddenBoss_ = false;
    bool titleAssetsLoaded_ = false;
    bool gameplayAssetsLoaded_ = false;
    bool effectAssetsLoaded_ = false;
    bool titleVideoAttempted_ = false;
    bool pendingStartHiddenBossPractice_ = false;
    int gameplayLoadStep_ = 0;
    bool eventVideoSkippable_ = true;
    bool bossWave_ = false;
    bool waveStarted_ = false;
    StageType stage_ = StageType::Donut;
    float stageTimer_ = 0.0f;
    float shrinkRadius_ = ArenaRadius;
    float spawnTimer_ = 0.0f;
    float pickupTimer_ = 5.0f;
    float slowT_ = 0.0f;
    float gameTime_ = 0.0f;
    float clearTimer_ = 0.0f;
    float hiddenIntroT_ = 0.0f;
    float hiddenBossT_ = 0.0f;
    float hiddenPatternCd_ = 0.0f;
    int hiddenPatternStep_ = 0;
    int hiddenBossPhase_ = -1;
    int hiddenBossForm_ = 1;
    float hiddenBossPhaseIntroT_ = 0.0f;
    float hiddenBossPhaseIntroLife_ = 0.0f;
    float messageT_ = 0.0f;
    float bootLoadElapsed_ = 0.0f;
    float loadPhaseElapsed_ = 0.0f;
    float titleVideoOpenDelay_ = 0.0f;
    std::array<float, static_cast<size_t>(LoadPhase::Count)> loadPhaseTimes_{};
    float screenFlashT_ = 0.0f;
    float screenFlashLife_ = 0.01f;
    Color screenFlashColor_ = Cream;
    float masterVolume_ = 1.0f;
    float bgmVolume_ = 1.0f;
    float seVolume_ = 1.0f;
    float uiVolume_ = 1.0f;
    uint64_t titleVideoSerial_ = 0;
    uint64_t eventVideoSerial_ = 0;
    Screen eventVideoNextScreen_ = Screen::Title;
    std::wstring message_;
    std::wstring lastLoadStep_ = L"Boot";
    std::wstring lastLoadWarning_;

    std::mt19937 rng_{ std::random_device{}() };
    std::chrono::steady_clock::time_point lastTick_{};
};
