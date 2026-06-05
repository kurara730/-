#include "SweetsApp.h"
#include "DataTables.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

#include <filesystem>

namespace
{
// 特殊敵は出しすぎると爽快感よりストレスが勝つため、
// 通常Waveでは同時出現数を制限するために判定をまとめています。
bool IsEliteType(EnemyType type)
{
    return type == EnemyType::Healer
        || type == EnemyType::Barrier
        || type == EnemyType::Mirror
        || type == EnemyType::Teleport;
}

bool AssetExists(const std::wstring& relativePath)
{
    namespace fs = std::filesystem;
    const fs::path rel(relativePath);
    std::array<fs::path, 5> candidates{};
    candidates[0] = fs::current_path() / rel;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const fs::path exeDir = fs::path(modulePath).parent_path();
    candidates[1] = exeDir / rel;
    candidates[2] = exeDir.parent_path() / rel;
    candidates[3] = exeDir.parent_path().parent_path() / rel;
    candidates[4] = rel;

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (fs::exists(path, ec)) return true;
    }
    return false;
}
}

// ランを最初から作り直します。
// タイトルや難易度選択では呼ばず、実際にゲーム開始/リトライする時だけ呼ぶことで、
// 初回起動時の待ち時間を短くしています。
void SweetsApp::ResetGame()
{
    // 1Pは必ず有効。2P-4Pはキャラ選択画面で Off/AI/Pad が選ばれている時だけ参加します。
    ApplyLoadout(players_[0], loadoutIndex_, 0, false);
    player_.bombs = CurrentDifficulty().initialBombs;
    for (int i = 1; i < MaxPlayers; ++i)
    {
        if (coopSlotModes_[i] == CoopSlotMode::Off)
        {
            players_[i] = {};
            players_[i].index = i;
            players_[i].active = false;
            players_[i].ai = false;
            continue;
        }

        ApplyLoadout(players_[i], coopLoadoutIndices_[i], i, coopSlotModes_[i] == CoopSlotMode::AI);
        players_[i].bombs = CurrentDifficulty().initialBombs;
    }
    boss_ = {};
    enemies_.clear();
    enemies_.reserve(256);
    enemySerial_ = 0;
    shots_.clear();
    slashes_.clear();
    pickups_.clear();
    particles_.clear();
    effectPulses_.clear();
    swordEffectVisuals_.clear();
    obstacles_.clear();
    wave_ = 1;
    score_ = 0;
    reflectKills_ = 0;
    gameTime_ = 0.0f;
    stageTimer_ = 0.0f;
    shrinkRadius_ = ArenaRadius;
    pickupTimer_ = 4.0f;
    slowT_ = 0.0f;
    hitstopT_ = 0.0f;
    justZoomT_ = 0.0f;
    justZoomLife_ = 0.0f;
    shakeT_ = 0.0f;
    shakeMag_ = 0.0f;
    player_.blinkCharges = BlinkMaxCharges;
    player_.blinkRechargeT = 0.0f;
    for (auto& pl : players_) pl.grabbedT = 0.0f;
    clearTimer_ = 0.0f;
    hiddenIntroT_ = 0.0f;
    hiddenBossT_ = 0.0f;
    hiddenPatternCd_ = 0.0f;
    hiddenPatternStep_ = 0;
    hiddenBossPhase_ = -1;
    hiddenBossForm_ = 1;
    hiddenBossGaugeHp_ = HiddenBossBaseGaugeHp;
    hiddenBossTotalHp_ = HiddenBossBaseGaugeHp * HiddenBossGaugeCount;
    hiddenBossCoreOpenT_ = 0.0f;
    hiddenBossAuraBreakT_ = 0.0f;
    hiddenBossReflectCount_ = 0;
    hiddenBossCores_ = {};
    hiddenBossPhaseIntroT_ = 0.0f;
    hiddenBossPhaseIntroLife_ = 0.0f;
    hiddenBossDashWarnT_ = 0.0f;
    hiddenBossDashWarnLife_ = 0.0f;
    hiddenBossDashT_ = 0.0f;
    hiddenBossDashLife_ = 0.0f;
    hiddenBossDashRecoverT_ = 0.0f;
    hiddenBossDashChainLeft_ = 0;
    hiddenBossDashChainGapT_ = 0.0f;
    hiddenBossDashChainWarn_ = 0.0f;
    hiddenBossDashChainDuration_ = 0.0f;
    hiddenBossDashChainSpeed_ = 0.0f;
    hiddenBossDashGlobalCd_ = 0.0f;
    hiddenBossEdgePressureCd_ = 0.0f;
    hiddenBossReflectT_ = 0.0f;
    hiddenBossCloneCd_ = 0.0f;
    hiddenBossIdleBasePos_ = {};
    hiddenBossFloatAnchorT_ = 0.0f;
    hiddenBossDashVel_ = {};
    pendingHiddenBoss_ = false;
    message_ = L"";
    messageT_ = 0.0f;
    screenFlashT_ = 0.0f;
    combatNotices_.clear();
    worldTelegraphs_.clear();
    camera_.center = player_.pos;
    camera_.target = player_.pos;
    SyncAll3DState();
    StartWave();
}

// 難易度が決まった後のゲーム開始入口です。
// 素材がまだ読み終わっていない場合は、ゲーム画面へ入る前に専用ロード画面へ移します。
void SweetsApp::StartGameWithDifficulty(bool hiddenBossPractice)
{
    if (!gameplayAssetsLoaded_ || !effectAssetsLoaded_)
    {
        pendingStartHiddenBossPractice_ = hiddenBossPractice;
        gameplayLoadStep_ = 0;
        loadPhase_ = LoadPhase::GameplayAssets;
        loadPhaseElapsed_ = 0.0f;
        lastLoadStep_ = L"Preparing gameplay assets";
        screen_ = Screen::GameplayLoading;
        return;
    }

    EnsureGameplayAssetsReady();
    gameMode_ = hiddenBossPractice ? GameMode::HiddenBossPractice : pendingGameMode_;
    hiddenBossPractice_ = gameMode_ == GameMode::HiddenBossPractice;
    if (!hiddenBossPractice_)
    {
        difficulty_ = static_cast<Difficulty>(std::max(0, std::min(difficultyIndex_, 4)));
    }
    else
    {
        difficulty_ = Difficulty::Lunatic;
    }

    ResetGame();
    if (gameMode_ == GameMode::HiddenBossPractice)
    {
        StartHiddenBoss();
    }
    else
    {
        screen_ = Screen::Playing;
    }
}

// 1Pの選択キャラを現在のロードアウトに反映します。
// 複数人分の反映は ResetGame 内で ApplyLoadout(Player&, ...) を個別に呼びます。
void SweetsApp::ApplyLoadout()
{
    ApplyLoadout(player_, loadoutIndex_, 0, false);
}

// Waveごとにステージ形状と敵構成を作り直します。
// 3の倍数は通常ボス戦、それ以外は雑魚戦として扱います。
void SweetsApp::StartWave()
{
    waveStarted_ = true;
    // デバッグステージは常にボスウェーブ（雑魚なし・ボス即出現）。
    bossWave_ = (wave_ % 3 == 0) || gameMode_ == GameMode::BossOnlyDebug;
    enemies_.clear();
    shots_.clear();
    slashes_.clear();
    boss_ = {};
    BuildStage();
    if (bossWave_)
    {
        SpawnBoss();
        remainingToSpawn_ = 0;
        spawnTimer_ = 1.8f;
        pickupTimer_ = Rand(6.0f, 8.0f);
        message_ = L"ボスウェーブ";
    }
    else
    {
        remainingToSpawn_ = 7 + wave_ * 3;
        spawnTimer_ = 0.2f;
        pickupTimer_ = Rand(1.8f, 3.0f);
        std::wostringstream ss;
        ss << L"ウェーブ開始 - " << StageName(stage_);
        message_ = ss.str();
    }
    messageT_ = 2.0f;
}

// Waveクリア時の報酬と次画面への遷移です。
// StoryはFinalWaveで区切り、Endlessはそのまま次Waveへ進みます。
void SweetsApp::ClearWave()
{
    AddScore(750 + wave_ * 180, &player_);
    for (auto& p : players_)
    {
        if (!p.active) continue;
        if (bossWave_) GrantBossSkill(p);
        if (!p.downed)
        {
            p.hp = std::min(p.maxHp, p.hp + (bossWave_ ? 50.0f : 14.0f));
            p.ult = std::min(100.0f, p.ult + (bossWave_ ? 45.0f : 10.0f));
            if (bossWave_)
            {
                p.bombs = std::min(5, p.bombs + 1);
                p.fever = 100.0f;
                p.feverT = std::max(p.feverT, 6.0f);
            }
        }
    }
    if (wave_ >= FinalWave && gameMode_ == GameMode::Story)
    {
        pendingHiddenBoss_ = difficulty_ == Difficulty::Lunatic && !hiddenBossPractice_;
        if (pendingHiddenBoss_)
        {
            SaveProgress();
            message_ = L"Lunatic Clear";
        }
        else
        {
            message_ = L"Clear";
        }
        clearTimer_ = 0.0f;
        screen_ = Screen::Clear;
        messageT_ = 4.0f;
        return;
    }
    wave_++;
    StartWave();
}

// アイテムはPickupTypeごとに効果が違います。
// 表示は GameplayView.cpp 側で形も変え、敵と見分けやすくしています。
void SweetsApp::SpawnPickupAt(V2 pos)
{
    Pickup p{};
    p.pos = pos;
    ClampInside(p.pos, 1.0f);
    p.type = RandInt(0, 9);
    p.pickupType = static_cast<PickupType>(p.type);
    switch (p.pickupType)
    {
    case PickupType::Attack: p.color = Berry; break;
    case PickupType::Slow: p.color = Sky; break;
    case PickupType::Invincible: p.color = Cream; break;
    case PickupType::Magnet: p.color = Mint; break;
    case PickupType::BombDamage: p.color = Red; break;
    case PickupType::Heal: p.color = Mint; break;
    case PickupType::UltFull: p.color = Grape; break;
    case PickupType::Spread: p.color = Gold; break;
    case PickupType::Speed: p.color = Sky; break;
    default: p.color = Sky; break;
    }
    SyncPickup3D(p);
    pickups_.push_back(p);
}

void SweetsApp::SpawnPickup()
{
    SpawnPickupAt(RandInArena(1.5f));
}

// アプリ全体の更新入口です。
// screen_ ごとに必要な処理だけを動かすことで、メニュー背景でゲームが進まないようにしています。
void SweetsApp::Update(float dt)
{
    if (screen_ == Screen::BootLoading)
    {
        UpdateBootLoading(dt);
    }
    else if (screen_ == Screen::GameplayLoading)
    {
        UpdateGameplayLoading(dt);
    }
    else if (screen_ == Screen::Title)
    {
        UpdateTitle(dt);
    }
    else if (screen_ == Screen::Playing)
    {
        UpdatePlaying(dt);
    }
    else if (screen_ == Screen::Clear)
    {
        UpdateClear(dt);
    }
    else if (screen_ == Screen::HiddenBossIntro)
    {
        UpdateHiddenBossIntro(dt);
    }
    else if (screen_ == Screen::HiddenBoss)
    {
        UpdateHiddenBoss(dt);
    }
    else if (screen_ == Screen::Video)
    {
        UpdateEventVideo(dt);
    }
    UpdateEffectVisuals(dt);
    UpdateAudioForScreen();
}

// 起動直後のロード処理です。
// 1フレームで全部読むのではなく段階を分け、ロード画面をすぐ表示できるようにしています。
void SweetsApp::UpdateBootLoading(float dt)
{
    bootLoadElapsed_ += dt;
    loadPhaseElapsed_ += dt;

    switch (loadPhase_)
    {
    case LoadPhase::Boot:
        AdvanceLoadPhase(LoadPhase::Graphics, L"Graphics initialized");
        break;
    case LoadPhase::Graphics:
        LoadProgress();
        LoadCharacterTableFromCsv();
        ApplyAudioVolume();
        AdvanceLoadPhase(LoadPhase::TitleAssets, L"Save data and settings loaded");
        break;
    case LoadPhase::TitleAssets:
        LoadTitleAssets();
        AdvanceLoadPhase(LoadPhase::Audio, L"Title assets loaded");
        break;
    case LoadPhase::Audio:
        ApplyAudioVolume();
        audio_.LoadSoundEffect(SoundEffect::ChocoSlash, L"assets/audio/se/choco_slash.mp3");
        audio_.LoadSoundEffect(SoundEffect::UltimateSlash, L"assets/audio/se/ultimate_slash.mp3");
        audio_.LoadSoundEffect(SoundEffect::Reflect, L"assets/audio/se/reflect.mp3");
        AdvanceLoadPhase(LoadPhase::Ready, L"Audio streaming ready");
        break;
    case LoadPhase::Ready:
        AdvanceLoadPhase(LoadPhase::Done, L"Ready");
        titleVideoAttempted_ = false;
        titleVideoOpenDelay_ = 0.0f;
        screen_ = Screen::Title;
        break;
    case LoadPhase::Done:
    case LoadPhase::Count:
    default:
        screen_ = Screen::Title;
        break;
    }
}

// ロードフェーズを進め、デバッグ表示用に最後の処理名と経過時間を残します。
void SweetsApp::AdvanceLoadPhase(LoadPhase nextPhase, std::wstring step)
{
    const size_t index = static_cast<size_t>(loadPhase_);
    if (index < loadPhaseTimes_.size())
    {
        loadPhaseTimes_[index] += loadPhaseElapsed_;
    }

#if defined(_DEBUG)
    std::wostringstream ss;
    ss << L"[SweetsActionDX11 Load] " << LoadPhaseName(loadPhase_)
        << L" " << loadPhaseElapsed_ << L"s: " << step << L"\n";
    OutputDebugStringW(ss.str().c_str());
#endif

    loadPhase_ = nextPhase;
    loadPhaseElapsed_ = 0.0f;
    lastLoadStep_ = std::move(step);
}

// ゲーム開始直前のロードです。
// タイトルで不要なGameplay素材やEffekseer素材は、ここで初めて読み込みます。
void SweetsApp::UpdateGameplayLoading(float dt)
{
    loadPhaseElapsed_ += dt;

    if (gameplayLoadStep_ == 0)
    {
        LoadGameplayAssets();
        AdvanceLoadPhase(LoadPhase::Effects, L"Gameplay assets loaded");
        gameplayLoadStep_ = 1;
        return;
    }
    if (gameplayLoadStep_ == 1)
    {
        LoadEffectAssets();
        AdvanceLoadPhase(LoadPhase::Ready, L"Effect assets loaded");
        gameplayLoadStep_ = 2;
        return;
    }

    StartGameWithDifficulty(pendingStartHiddenBossPractice_);
}

// タイトル中はゲーム本体を動かさず、タイトル動画だけ準備できたら再生を始めます。
void SweetsApp::UpdateTitle(float dt)
{
    OpenTitleVideoIfReady(dt);
    titleVideo_.Update(dt);
}

void SweetsApp::OpenTitleVideoIfReady(float dt)
{
    if (!titleAssetsLoaded_ || titleVideoAttempted_)
    {
        return;
    }
    titleVideoOpenDelay_ += dt;
    if (titleVideoOpenDelay_ < 0.35f)
    {
        return;
    }

    titleVideoAttempted_ = true;
    if (!titleVideo_.Open(L"assets/video/title.mp4", true))
    {
        lastLoadWarning_ = titleVideo_.LastError();
    }
}

void SweetsApp::UpdateEventVideo(float dt)
{
    eventVideo_.Update(dt);
    if (!eventVideo_.IsOpen() || eventVideo_.Ended())
    {
        eventVideo_.Stop();
        eventVideoBitmap_.Reset();
        screen_ = eventVideoNextScreen_;
    }
}

void SweetsApp::UpdateDebugTiming(float dt)
{
#if defined(_DEBUG)
    debug_.frameMs = dt * 1000.0f;
    debug_.fpsAccum += dt;
    ++debug_.fpsFrames;
    if (debug_.fpsAccum >= 0.5f)
    {
        debug_.fps = static_cast<float>(debug_.fpsFrames) / debug_.fpsAccum;
        debug_.fpsAccum = 0.0f;
        debug_.fpsFrames = 0;
    }
#else
    (void)dt;
#endif
}

// 画面状態に合わせてBGMを切り替えます。
// 同じ曲への再要求はAudioSystem側で抑え、余計な再読み込みを避けています。
void SweetsApp::UpdateAudioForScreen()
{
    ApplyAudioVolume();
    switch (screen_)
    {
    case Screen::Title:
    case Screen::CharacterSelect:
    case Screen::DifficultySelect:
    case Screen::Credits:
        audio_.PlayLoop(MusicTrack::Title, L"assets/audio/333_BPM177.mp3");
        break;
    case Screen::Playing:
    case Screen::Paused:
        audio_.PlayLoop(MusicTrack::Gameplay, L"assets/audio/233_BPM163.mp3");
        break;
    case Screen::GameOver:
        audio_.PlayLoop(MusicTrack::GameOver, L"assets/audio/ruins.mp3");
        break;
    case Screen::HiddenBossIntro:
        audio_.PlayLoop(MusicTrack::HiddenBossGauge1, L"assets/audio/hidden_gauge1.mp3");
        break;
    case Screen::HiddenBoss:
        if (hiddenBossForm_ <= 1)
        {
            audio_.PlayLoop(MusicTrack::HiddenBossGauge1, L"assets/audio/hidden_gauge1.mp3");
        }
        else if (hiddenBossForm_ == 2)
        {
            audio_.PlayLoop(MusicTrack::HiddenBossGauge2, L"assets/audio/hidden_gauge2.mp3");
        }
        else
        {
            audio_.PlayLoop(MusicTrack::HiddenBossGauge3, L"assets/audio/hidden_gauge3.mp3");
        }
        break;
    case Screen::CompleteClear:
        if (AssetExists(L"assets/audio/hidden_clear.mp3"))
        {
            audio_.PlayLoop(MusicTrack::HiddenBossClear, L"assets/audio/hidden_clear.mp3");
        }
        else
        {
            audio_.PlayLoop(MusicTrack::Title, L"assets/audio/333_BPM177.mp3");
        }
        break;
    default:
        audio_.Stop();
        break;
    }
}

// セーブされた音量設定をAudioSystemへ反映します。
// 現時点で実音に効くのは主に Master と BGM/SE ですが、UI音量も将来用に保存します。
void SweetsApp::ApplyAudioVolume()
{
    audio_.SetVolume(ClampFloat(masterVolume_, 0.0f, 1.0f) * ClampFloat(bgmVolume_, 0.0f, 1.0f));
    audio_.SetSoundVolume(ClampFloat(masterVolume_, 0.0f, 1.0f) * ClampFloat(seVolume_, 0.0f, 1.0f));
}

// HPが0以下なのに動ける、という不整合を防ぐための保険処理です。
// 戦闘更新の最後に呼び、ソロまたは全員ダウンならゲームオーバーへ進めます。
void SweetsApp::NormalizePlayerLifeStates()
{
    for (auto& p : players_)
    {
        if (!p.active) continue;
        if (p.hp <= 0.0f)
        {
            p.hp = 0.0f;
            p.downed = true;
            p.alive = false;
            p.reviveT = 0.0f;
        }
    }
}

void SweetsApp::UpdateClear(float dt)
{
    clearTimer_ += dt;
    if (pendingHiddenBoss_ && clearTimer_ >= 1.0f)
    {
        screen_ = Screen::HiddenBossIntro;
        hiddenIntroT_ = 0.0f;
        Burst({ 0.0f, 0.0f }, Grape, 120);
    }
}

// 通常プレイ中の更新をまとめています。
// 入力、AI、ステージ、敵、弾、アイテム、演出の順で進めることで判定順を安定させています。
void SweetsApp::UpdatePlaying(float dt)
{
    // ジャスト回避の演出タイマーは実時間で進める。
    const float realDt = dt;
    if (hitstopT_ > 0.0f) hitstopT_ = std::max(0.0f, hitstopT_ - realDt);
    if (justZoomT_ > 0.0f) justZoomT_ = std::max(0.0f, justZoomT_ - realDt);
    if (shakeT_ > 0.0f) shakeT_ = std::max(0.0f, shakeT_ - realDt); // 画面シェイクは実時間で減衰
    // ヒットストップ中はゲーム内時間を強く減速（演出タイマーやカメラ追従はこの後の dt を使う）。
    if (hitstopT_ > 0.0f) dt = realDt * HitstopScale;

    gameTime_ += dt;
    if (messageT_ > 0.0f) messageT_ -= dt;
    if (slowT_ > 0.0f) slowT_ -= dt;
    if (player_.inv > 0.0f) player_.inv -= dt;
    if (player_.shieldT > 0.0f) player_.shieldT -= dt;
    if (player_.bombT > 0.0f) player_.bombT -= dt;
    if (player_.grazeFlash > 0.0f) player_.grazeFlash -= dt;
    if (player_.dmgBuffT > 0.0f) player_.dmgBuffT -= dt;
    if (player_.speedBuffT > 0.0f) player_.speedBuffT -= dt;
    if (player_.scoreDoubleT > 0.0f) player_.scoreDoubleT -= dt;
    if (player_.magnetT > 0.0f) player_.magnetT -= dt;
    if (player_.spreadT > 0.0f) player_.spreadT -= dt;
    if (player_.chargeCd > 0.0f) player_.chargeCd -= dt;
    if (player_.feverT > 0.0f) player_.feverT -= dt;
    else player_.fever = std::max(0.0f, player_.fever - dt * 16.0f);

    UpdateStage(dt);
    UpdatePlayer(dt);
    UpdateCoopPlayers(dt);
    UpdateEnemies(dt);
    UpdateBoss(dt);
    UpdateShots(dt);
    ReleaseCaughtIfNoBomb();
    UpdatePickups(dt);
    UpdateParticles(dt);
    UpdateCamera(dt);
    for (auto& s : slashes_)
    {
        s.ttl -= dt;
    }

    if (remainingToSpawn_ > 0)
    {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.0f)
        {
            int spawned = 1;
            if (!bossWave_ && remainingToSpawn_ >= 3 && Rand(0.0f, 1.0f) < 0.48f)
            {
                spawned = SpawnEnemyFormation();
            }
            else
            {
                SpawnEnemy();
            }
            remainingToSpawn_ = std::max(0, remainingToSpawn_ - spawned);
            spawnTimer_ = (bossWave_ ? Rand(1.4f, 2.4f) : Rand(0.38f, 0.82f)) * CurrentDifficulty().spawnIntervalMul;
        }
    }

    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return e.dead; }), enemies_.end());
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.dead || s.ttl <= 0.0f; }), shots_.end());
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return p.ttl <= 0.0f; }), pickups_.end());
    slashes_.erase(std::remove_if(slashes_.begin(), slashes_.end(), [](const Slash& s) { return s.ttl <= 0.0f; }), slashes_.end());
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());

    pickupTimer_ -= dt;
    const EncounterTuning& tuning = CurrentEncounterTuning();
    if (pickupTimer_ <= 0.0f && pickups_.size() < static_cast<size_t>(tuning.pickupMax))
    {
        SpawnPickup();
        pickupTimer_ = Rand(tuning.pickupIntervalMin, tuning.pickupIntervalMax);
    }

    if (!boss_.active && remainingToSpawn_ <= 0 && enemies_.empty())
    {
        ClearWave();
    }

    NormalizePlayerLifeStates();
    if (AllPlayersDown())
    {
        screen_ = Screen::GameOver;
        gameOverChoice_ = GameOverChoice::Retry;
        message_ = L"ゲームオーバー";
        messageT_ = 999.0f;
    }
}

const DifficultyDef& SweetsApp::CurrentDifficulty() const
{
    if (hiddenBossPractice_) return DifficultyDefs[static_cast<int>(Difficulty::Lunatic)];
    return DifficultyDefs[static_cast<int>(difficulty_)];
}

EncounterProfile SweetsApp::CurrentEncounterProfile() const
{
    if (screen_ == Screen::HiddenBoss || screen_ == Screen::HiddenBossIntro || boss_.bossType == BossType::HiddenBoss)
    {
        return EncounterProfile::HiddenBossSurvival;
    }
    if (bossWave_ || (boss_.active && screen_ == Screen::Playing))
    {
        return EncounterProfile::BossSkillCheck;
    }
    return EncounterProfile::MobRelease;
}

const EncounterTuning& SweetsApp::CurrentEncounterTuning() const
{
    return EncounterTunings[static_cast<size_t>(CurrentEncounterProfile())];
}

int SweetsApp::EliteEnemyCount() const
{
    return static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(), [](const Enemy& e)
    {
        return !e.dead && IsEliteType(e.type);
    }));
}

int SweetsApp::BossAddCount() const
{
    return static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(), [](const Enemy& e)
    {
        return !e.dead;
    }));
}

int SweetsApp::ScaledBulletCount(int base) const
{
    float count = static_cast<float>(base) * CurrentDifficulty().bulletCountMul;
    if (CurrentEncounterProfile() == EncounterProfile::MobRelease)
    {
        count *= 0.72f;
    }
    return std::max(1, static_cast<int>(std::round(count)));
}

int SweetsApp::DifficultyOptionCount() const
{
    // 末尾に「ボスのみ（デバッグ）」カードを1つ足す。
    return (hiddenBossUnlocked_ ? 6 : 5) + 1;
}

