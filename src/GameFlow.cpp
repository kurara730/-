#include "SweetsApp.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

namespace
{
bool IsEliteType(EnemyType type)
{
    return type == EnemyType::Healer
        || type == EnemyType::Barrier
        || type == EnemyType::Mirror
        || type == EnemyType::Teleport;
}
}

void SweetsApp::ResetGame()
{
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
    clearTimer_ = 0.0f;
    hiddenIntroT_ = 0.0f;
    hiddenBossT_ = 0.0f;
    hiddenPatternCd_ = 0.0f;
    hiddenPatternStep_ = 0;
    hiddenBossPhase_ = -1;
    pendingHiddenBoss_ = false;
    message_ = L"";
    messageT_ = 0.0f;
    screenFlashT_ = 0.0f;
    SyncAll3DState();
    StartWave();
}

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

void SweetsApp::ApplyLoadout()
{
    ApplyLoadout(player_, loadoutIndex_, 0, false);
}

void SweetsApp::StartWave()
{
    waveStarted_ = true;
    bossWave_ = (wave_ % 3 == 0);
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
        ApplyAudioVolume();
        AdvanceLoadPhase(LoadPhase::TitleAssets, L"Save data and settings loaded");
        break;
    case LoadPhase::TitleAssets:
        LoadTitleAssets();
        AdvanceLoadPhase(LoadPhase::Audio, L"Title assets loaded");
        break;
    case LoadPhase::Audio:
        ApplyAudioVolume();
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
    case Screen::HiddenBoss:
        audio_.PlayOnce(MusicTrack::HiddenBoss, L"assets/audio/Lonery boy.wav");
        break;
    default:
        audio_.Stop();
        break;
    }
}

void SweetsApp::ApplyAudioVolume()
{
    audio_.SetVolume(ClampFloat(masterVolume_, 0.0f, 1.0f) * ClampFloat(bgmVolume_, 0.0f, 1.0f));
}

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

void SweetsApp::UpdatePlaying(float dt)
{
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
    UpdatePickups(dt);
    UpdateParticles(dt);
    for (auto& s : slashes_)
    {
        s.ttl -= dt;
    }

    if (remainingToSpawn_ > 0)
    {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.0f)
        {
            SpawnEnemy();
            --remainingToSpawn_;
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
    return hiddenBossUnlocked_ ? 6 : 5;
}

