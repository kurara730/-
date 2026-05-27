#include "SweetsApp.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

void SweetsApp::ResetGame()
{
    ApplyLoadout(players_[0], loadoutIndex_, 0, false);
    player_.bombs = CurrentDifficulty().initialBombs;
    for (int i = 1; i < MaxPlayers; ++i)
    {
        players_[i] = {};
        players_[i].index = i;
        players_[i].active = false;
        players_[i].ai = false;
    }
    boss_ = {};
    enemies_.clear();
    enemies_.reserve(256);
    shots_.clear();
    slashes_.clear();
    pickups_.clear();
    particles_.clear();
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
    pendingHiddenBoss_ = false;
    message_ = L"";
    messageT_ = 0.0f;
    StartWave();
}

void SweetsApp::StartGameWithDifficulty(bool hiddenBossPractice)
{
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

void SweetsApp::StartHiddenBoss()
{
    enemies_.clear();
    shots_.clear();
    slashes_.clear();
    pickups_.clear();
    obstacles_.clear();
    particles_.clear();
    stage_ = StageType::BossArena;
    stageTimer_ = 0.0f;
    shrinkRadius_ = ArenaRadius;
    boss_ = {};
    boss_.active = true;
    boss_.bossType = BossType::HiddenBoss;
    boss_.type = static_cast<int>(BossType::HiddenBoss);
    boss_.pos = { 0.0f, -4.8f };
    boss_.radius = 1.05f;
    boss_.maxHp = 999999.0f;
    boss_.hp = boss_.maxHp;
    boss_.speed = 0.35f;
    boss_.atk = 15.0f;
    boss_.phase = 1;
    boss_.attackCd = 0.0f;
    hiddenBossT_ = 0.0f;
    hiddenPatternCd_ = 0.0f;
    hiddenPatternStep_ = 0;
    screen_ = Screen::HiddenBoss;
    message_ = L"隠しボス: 曲が終わるまで耐えろ";
    messageT_ = 3.0f;
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
        message_ = L"ボスウェーブ";
    }
    else
    {
        remainingToSpawn_ = 5 + wave_ * 2;
        spawnTimer_ = 0.2f;
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
            p.hp = std::min(p.maxHp, p.hp + (bossWave_ ? 35.0f : 18.0f));
            p.ult = std::min(100.0f, p.ult + (bossWave_ ? 30.0f : 12.0f));
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

void SweetsApp::SpawnPickup()
{
    Pickup p{};
    p.pos = RandInArena(1.5f);
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
    pickups_.push_back(p);
}

void SweetsApp::Update(float dt)
{
    if (screen_ == Screen::Title)
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
    UpdateAudioForScreen();
}

void SweetsApp::UpdateTitle(float dt)
{
    titleVideo_.Update(dt);
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

void SweetsApp::UpdateHiddenBossIntro(float dt)
{
    hiddenIntroT_ += dt;
    boss_.active = true;
    boss_.bossType = BossType::HiddenBoss;
    boss_.pos = { 0.0f, -4.8f + ClampFloat(hiddenIntroT_ / 2.2f, 0.0f, 1.0f) * 2.6f };
    boss_.radius = 1.05f;
    UpdateParticles(dt);
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());
    if (hiddenIntroT_ >= 2.2f)
    {
        StartHiddenBoss();
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

    if (remainingToSpawn_ > 0)
    {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.0f)
        {
            SpawnEnemy();
            --remainingToSpawn_;
            spawnTimer_ = (bossWave_ ? Rand(1.4f, 2.4f) : Rand(0.55f, 1.15f)) * CurrentDifficulty().spawnIntervalMul;
        }
    }

    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return e.dead; }), enemies_.end());
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.dead || s.ttl <= 0.0f; }), shots_.end());
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return p.ttl <= 0.0f; }), pickups_.end());
    slashes_.erase(std::remove_if(slashes_.begin(), slashes_.end(), [](const Slash& s) { return s.ttl <= 0.0f; }), slashes_.end());
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());

    pickupTimer_ -= dt;
    if (pickupTimer_ <= 0.0f && pickups_.size() < 3)
    {
        SpawnPickup();
        pickupTimer_ = Rand(6.0f, 10.0f);
    }

    if (!boss_.active && remainingToSpawn_ <= 0 && enemies_.empty())
    {
        ClearWave();
    }

    if (AllPlayersDown())
    {
        screen_ = Screen::GameOver;
        gameOverChoice_ = GameOverChoice::Retry;
        message_ = L"ゲームオーバー";
        messageT_ = 999.0f;
    }
}

void SweetsApp::UpdateHiddenBoss(float dt)
{
    gameTime_ += dt;
    hiddenBossT_ += dt;
    if (messageT_ > 0.0f) messageT_ -= dt;
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

    boss_.spin += dt * 1.35f;
    boss_.pos.x = std::sin(hiddenBossT_ * 0.55f) * 2.7f;
    boss_.pos.z = -3.0f + std::sin(hiddenBossT_ * 0.37f) * 0.55f;
    if (boss_.flash > 0.0f) boss_.flash -= dt;

    UpdateStage(dt);
    UpdatePlayer(dt);
    UpdateCoopPlayers(dt);

    hiddenPatternCd_ -= dt;
    if (hiddenPatternCd_ <= 0.0f)
    {
        const int enemyBullets = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.enemy && !s.dead; }));
        int remaining = std::max(0, HiddenBossBulletCap - enemyBullets);
        auto spawn = [&](float angle, float speed, float radius, Color color, float ttl, float curve = 0.0f, float accel = 0.0f)
        {
            if (remaining <= 0) return;
            SpawnEnemyShot(boss_.pos + FromAngle(angle) * (boss_.radius + 0.18f), angle, speed, boss_.atk * 0.62f, radius, color, ttl, curve, accel);
            --remaining;
        };

        const int phase = std::min(4, static_cast<int>(hiddenBossT_ / 27.0f));
        const float aimed = AngleOf(player_.pos - boss_.pos);
        if (phase == 0)
        {
            for (int i = -2; i <= 2; ++i) spawn(aimed + i * 0.13f, 3.6f + 0.12f * i, 0.075f, Sky, 5.8f);
            for (int i = 0; i < 12; ++i) spawn(boss_.spin + TwoPi * i / 12.0f, 2.25f, 0.070f, Grape, 6.2f, 0.05f);
            hiddenPatternCd_ = 0.58f;
        }
        else if (phase == 1)
        {
            for (int arm = 0; arm < 4; ++arm)
            {
                const float a = boss_.spin * 1.6f + arm * Pi * 0.5f;
                spawn(a, 2.8f + 0.08f * (hiddenPatternStep_ % 5), 0.072f, Gold, 6.4f, (arm & 1) ? -0.16f : 0.16f, 0.03f);
                spawn(a + 0.18f, 3.25f, 0.070f, Mint, 5.4f, (arm & 1) ? -0.10f : 0.10f);
            }
            if ((hiddenPatternStep_ % 4) == 0)
            {
                for (int i = -3; i <= 3; ++i) spawn(aimed + i * 0.09f, 4.2f, 0.068f, Red, 4.6f, 0.0f, -0.04f);
            }
            hiddenPatternCd_ = 0.32f;
        }
        else if (phase == 2)
        {
            const float gap = std::sin(hiddenBossT_ * 1.2f) * 0.9f;
            for (int i = 0; i < 20; ++i)
            {
                const float a = -Pi * 0.92f + i * (Pi * 1.84f / 19.0f);
                if (std::fabs(a - gap) < 0.20f) continue;
                spawn(a, 2.55f + (i % 3) * 0.18f, 0.066f, Cream, 6.8f, 0.03f * std::sin(i * 1.7f));
            }
            for (int i = -1; i <= 1; ++i) spawn(aimed + i * 0.16f, 4.55f, 0.072f, Red, 4.2f);
            hiddenPatternCd_ = 0.72f;
        }
        else if (phase == 3)
        {
            for (int i = 0; i < 28; ++i)
            {
                const float a = boss_.spin * 0.7f + TwoPi * i / 28.0f;
                if ((i + hiddenPatternStep_) % 7 == 0) continue;
                spawn(a, 2.45f + (i % 4) * 0.17f, 0.067f, Grape, 6.5f, (i & 1) ? 0.09f : -0.09f);
            }
            for (int i = -2; i <= 2; ++i) spawn(aimed + i * 0.11f, 4.15f, 0.070f, Sky, 4.8f);
            hiddenPatternCd_ = 0.82f;
        }
        else
        {
            for (int i = 0; i < 10; ++i)
            {
                const float a = boss_.spin * 2.0f + i * 0.63f;
                spawn(a, 3.0f + 0.12f * i, 0.066f, (i & 1) ? Gold : Grape, 5.4f, (i & 1) ? 0.18f : -0.18f, 0.04f);
            }
            for (int i = -3; i <= 3; ++i) spawn(aimed + i * 0.08f, 4.75f, 0.066f, Red, 4.0f, 0.0f, -0.03f);
            hiddenPatternCd_ = 0.36f;
        }
        ++hiddenPatternStep_;
    }

    UpdateShots(dt);
    UpdatePickups(dt);
    UpdateParticles(dt);

    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.dead || s.ttl <= 0.0f; }), shots_.end());
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return p.ttl <= 0.0f; }), pickups_.end());
    slashes_.erase(std::remove_if(slashes_.begin(), slashes_.end(), [](const Slash& s) { return s.ttl <= 0.0f; }), slashes_.end());
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());

    if (hiddenBossT_ >= HiddenBossDurationSeconds)
    {
        SaveProgress();
        AddScore(50000, &player_);
        shots_.clear();
        boss_.active = false;
        screen_ = Screen::CompleteClear;
        message_ = L"完全クリア";
        messageT_ = 999.0f;
    }
    else if (AllPlayersDown())
    {
        screen_ = Screen::GameOver;
        gameOverChoice_ = GameOverChoice::Retry;
        message_ = L"ゲームオーバー";
        messageT_ = 999.0f;
    }
}

void SweetsApp::UpdateShots(float dt)
{
    for (auto& s : shots_)
    {
        if (s.dead) continue;
        s.ttl -= dt;
        if (s.ttl <= 0.0f) continue;

        if (s.enemy && (std::fabs(s.angularVel) > 0.0001f || std::fabs(s.accel) > 0.0001f))
        {
            float speed = Len(s.vel);
            float angle = AngleOf(s.vel) + s.angularVel * dt;
            speed = std::max(0.3f, speed + s.accel * dt);
            s.vel = FromAngle(angle) * speed;
        }
        else if (!s.enemy && s.homingStrength > 0.0f)
        {
            const V2 target = FindNearestEnemyOrBoss(s.pos);
            const V2 desired = Normalize(target - s.pos);
            if (LenSq(desired) > 0.001f)
            {
                const float speed = Len(s.vel);
                const V2 blended = Normalize(Normalize(s.vel) + desired * s.homingStrength * dt);
                s.vel = blended * speed;
            }
        }

        s.pos += s.vel * dt;
        const float dist = Len(s.pos);
        if (dist > ArenaRadius - s.radius)
        {
            if (s.bounce > 0)
            {
                V2 n = Normalize(s.pos);
                s.pos = n * (ArenaRadius - s.radius);
                ApplyShotReflection(s, n, 1.0f);
                --s.bounce;
            }
            else
            {
                s.dead = true;
                continue;
            }
        }

        for (const auto& o : obstacles_)
        {
            V2 d = s.pos - o.pos;
            const float l = Len(d);
            if (l < s.radius + o.radius)
            {
                V2 n = Normalize(d);
                s.pos = o.pos + n * (s.radius + o.radius + 0.01f);
                if (s.enemy && o.cheeseWall)
                {
                    ApplyShotReflection(s, n, std::max(1.15f, o.reflectPower));
                    s.enemy = false;
                    s.ownerIndex = std::max(0, o.ownerIndex);
                    s.sourceCharacter = CharacterType::Cheese;
                    s.color = Gold;
                    s.damage *= 1.25f;
                    s.bounce = std::max(s.bounce, 2);
                }
                else if (s.bounce > 0 || s.enemy)
                {
                    ApplyShotReflection(s, n, o.reflectPower);
                    if (s.bounce > 0) --s.bounce;
                }
                else
                {
                    s.dead = true;
                }
                break;
            }
        }

        if (s.dead) continue;

        if (s.enemy)
        {
            for (auto& p : players_)
            {
                if (!p.active || p.downed) continue;
                const float hitDist = s.radius + p.hitboxRadius;
                const float grazeDist = s.radius + p.grazeRadius;
                const float d = Len(s.pos - p.pos);
                if (!s.grazed && d < grazeDist && d >= hitDist)
                {
                    s.grazed = true;
                    ++p.graze;
                    ++p.grazeChain;
                    p.grazeFlash = 0.18f;
                    p.ult = std::min(100.0f, p.ult + 0.45f);
                    AddScore(12 + std::min(p.grazeChain, 80), &p);
                }
                if (d < hitDist)
                {
                    ResolvePlayerHit(p, s.damage, AngleOf(p.pos - s.pos));
                    s.dead = true;
                    break;
                }
            }
        }
        else
        {
            for (auto& e : enemies_)
            {
                if (e.dead) continue;
                if (Len(s.pos - e.pos) < s.radius + e.radius)
                {
                    const bool wasChargedSplit = s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0;
                    DamageEnemy(e, ReflectedDamage(s), s.pos, 1.0f, s.reflected, s.ownerIndex);
                    if (e.type == EnemyType::Mirror && !s.reflected)
                    {
                        Burst(e.pos, Sky, 10);
                    }
                    if (wasChargedSplit) SpawnSplitShots(s, e.pos);
                    if (s.pierce > 0) --s.pierce;
                    else s.dead = true;
                    break;
                }
            }
            if (!s.dead && boss_.active && Len(s.pos - boss_.pos) < s.radius + boss_.radius)
            {
                if (s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0) SpawnSplitShots(s, boss_.pos);
                DamageBoss(ReflectedDamage(s), s.reflected, s.ownerIndex);
                if (s.pierce > 0) --s.pierce;
                else s.dead = true;
            }
        }
    }
}

void SweetsApp::UpdatePickups(float dt)
{
    for (auto& item : pickups_)
    {
        item.ttl -= dt;
        if (item.ttl <= 0.0f) continue;

        Player* taker = nullptr;
        float bestD = 99999.0f;
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            const float d = Len(p.pos - item.pos);
            if (p.magnetT > 0.0f && d < 4.6f)
            {
                item.pos += Normalize(p.pos - item.pos) * (dt * 6.5f);
            }
            if (d < item.radius + p.radius && d < bestD)
            {
                bestD = d;
                taker = &p;
            }
        }

        if (taker)
        {
            Player& p = *taker;
            switch (item.pickupType)
            {
            case PickupType::Heal:
                p.hp = std::min(p.maxHp, p.hp + 45.0f);
                message_ = L"回復アイテム";
                break;
            case PickupType::Attack:
                p.dmgBuffT = 8.0f;
                message_ = L"攻撃力アップ";
                break;
            case PickupType::Slow:
                slowT_ = 7.0f;
                message_ = L"敵スロー";
                break;
            case PickupType::Invincible:
                p.shieldT = 5.0f;
                p.inv = 2.0f;
                message_ = L"無敵";
                break;
            case PickupType::Magnet:
                p.magnetT = 10.0f;
                message_ = L"マグネット";
                break;
            case PickupType::BombDamage:
                for (auto& e : enemies_) DamageEnemy(e, 95.0f + wave_ * 6.0f, p.pos, 2.0f, false, p.index);
                if (boss_.active) DamageBoss(260.0f + wave_ * 18.0f, false, p.index);
                Burst(p.pos, Red, 60);
                message_ = L"爆発アイテム";
                break;
            case PickupType::UltFull:
                p.ult = 100.0f;
                message_ = L"必殺ゲージ満タン";
                break;
            case PickupType::Spread:
                p.spreadT = 10.0f;
                message_ = L"拡散ショット";
                break;
            case PickupType::Speed:
                p.speedBuffT = 8.0f;
                message_ = L"速度アップ";
                break;
            case PickupType::ScoreDouble:
                p.scoreDoubleT = 10.0f;
                message_ = L"スコア2倍";
                break;
            default:
                p.bombs = std::min(5, p.bombs + 1);
                p.inv = std::max(p.inv, 1.0f);
                message_ = L"ボム+1";
                break;
            }
            Burst(item.pos, item.color, 28);
            messageT_ = 2.0f;
            item.ttl = 0.0f;
        }
    }
}

void SweetsApp::UpdateParticles(float dt)
{
    for (auto& p : particles_)
    {
        p.ttl -= dt;
        p.pos += p.vel * dt;
        p.y += p.vy * dt;
        p.vy -= 6.0f * dt;
    }
}

void SweetsApp::SpawnEnemyShot(V2 pos, float angle, float speed, float damage, float radius, Color color, float ttl, float angularVel, float accel)
{
    if (screen_ == Screen::HiddenBoss)
    {
        const int enemyBullets = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.enemy && !s.dead; }));
        if (enemyBullets >= HiddenBossBulletCap) return;
        radius *= 1.14f;
    }
    else
    {
        const DifficultyDef& diff = CurrentDifficulty();
        speed *= diff.bulletSpeedMul;
        radius *= diff.enemyShotRadiusMul;
    }

    Shot s{};
    s.enemy = true;
    s.pos = pos + FromAngle(angle) * 0.12f;
    s.vel = FromAngle(angle) * speed;
    s.radius = radius;
    s.damage = damage;
    s.ttl = ttl;
    s.color = color;
    s.angularVel = angularVel;
    s.accel = accel;
    s.ownerIndex = -1;
    shots_.push_back(s);
}

void SweetsApp::Burst(V2 p, Color c, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const float a = Rand(0.0f, TwoPi);
        const float spd = Rand(0.6f, 3.0f);
        Particle q{};
        q.pos = p;
        q.vel = FromAngle(a) * spd;
        q.y = Rand(0.12f, 0.8f);
        q.vy = Rand(1.0f, 4.2f);
        q.ttl = Rand(0.35f, 0.9f);
        q.color = c;
        particles_.push_back(q);
    }
}

const DifficultyDef& SweetsApp::CurrentDifficulty() const
{
    if (hiddenBossPractice_) return DifficultyDefs[static_cast<int>(Difficulty::Lunatic)];
    return DifficultyDefs[static_cast<int>(difficulty_)];
}

int SweetsApp::ScaledBulletCount(int base) const
{
    return std::max(1, static_cast<int>(std::round(base * CurrentDifficulty().bulletCountMul)));
}

int SweetsApp::DifficultyOptionCount() const
{
    return hiddenBossUnlocked_ ? 6 : 5;
}
