#pragma once

#include <array>

constexpr int MaxPlayers = 4;

// スペースキーのブリンク（短距離テレポート回避）の調整値です。
constexpr float BlinkDistance = 3.8f;    // 通常ブリンク（ジャスト回避でない）の移動距離
constexpr float BlinkJustDistance = 5.8f; // ジャスト回避成功時の移動距離（成功を見分けやすく長め）
constexpr float BlinkInvuln = 0.35f;     // ブリンク直後の無敵時間（弾を抜けられる）
constexpr int BlinkMaxCharges = 2;       // 連続で使える最大回数（チャージ）
constexpr float BlinkChargeCooldown = 10.0f; // チャージ1回あたりの回復時間（秒）
constexpr float BlinkAttackLock = 0.3f;  // ブリンク直後に攻撃を出せない時間（攻撃とブリンクの同時発動を禁止）

// ジャスト回避（危険な状況でブリンク成功）時の演出。ヒットストップ＋自キャラへズーム。
constexpr float HitstopTime = 0.22f;     // ヒットストップ（強スロー）の実時間長
constexpr float HitstopScale = 0.06f;    // ヒットストップ中のゲーム内時間倍率
constexpr float JustZoomLife = 0.55f;    // ズーム演出の長さ（実時間）
constexpr float JustZoomPeak = 0.30f;    // ズーム量のピーク（1.0 + これ倍）
constexpr float JustDodgeBulletRange = 1.35f; // この距離以内に迫る敵弾があればジャスト判定

// ボスの貫通ビーム（パリィ不可・低頻度の強攻撃）の調整値です。
// 予兆(Warn)で地面に線を出して避けさせ、照射(Active)中に線分上のプレイヤーへダメージします。
constexpr float BossBeamWarnTime = 1.5f;      // 予兆時間（回避猶予）
constexpr float BossBeamActiveTime = 1.5f;    // 照射時間
constexpr float BossBeamHalfWidth = 1.24f;    // ビーム半幅（当たり判定）
constexpr float BossBeamLength = 26.0f;       // ビーム長（アリーナを貫く）
constexpr float BossBeamDamageMul = 2.2f;     // boss_.atk への倍率（強攻撃）
constexpr float BossBeamCooldownMin = 12.0f;  // 次のビームまでの最短秒数（発動頻度・低め）
constexpr float BossBeamCooldownVar = 5.0f;   // 上記に加算される乱数幅

// どの攻撃（弾幕・特殊技）も、終わった後この秒数は次の攻撃を始めない（攻撃の重なり防止＝間合い確保）。
constexpr float BossAttackRest = 0.7f;

// 貫通ビームの反射＆ブレイク（崩し）システム。
// チョコウォール（右クリックの壁）をビームの軌道上に置くと、ビームを反射してボスへダメージ。
// 反射ダメージは「ブレイクゲージ」に蓄積し、満タンでボスが一定時間動けなくなる（ブレイク）。
constexpr float BossBeamReflectDps = 1.6f;     // 反射中、ボスへ与える毎秒ダメージ（boss.atk倍率）
constexpr float BossBreakGaugeMax = 90.0f;     // 崩しゲージ初期最大値（反射ダメージの蓄積で満タン）
constexpr float BossBreakGaugeGrowth = 1.6f;   // 崩すたびに次の必要量がこの倍率で増える（だんだん崩しづらく）
constexpr float BossBreakDuration = 8.0f;      // 崩し（弱点露出）の継続時間（秒）
// 崩し中：停止せず弱点が露出。その代わり行動が活発化（攻撃頻度・移動速度UP）し、被ダメージが増える。
constexpr float BossBreakWeakDamageMul = 2.2f; // 崩し中の弱点ダメージ倍率
constexpr float BossBreakAggroCdMul = 0.5f;    // 崩し中の攻撃クールダウン短縮（小さいほど活発）
constexpr float BossBreakSpeedMul = 1.35f;     // 崩し中の移動速度倍率

// ボスのフェーズ（SAO風の分割HPバー）。HPを BossGaugeCount 本のゲージに分割し、
// 1本削り切るたびにフェーズが上がり、行動が激化する（移動・攻撃頻度の上昇＋弾消し＋演出）。
constexpr int BossGaugeCount = 4;              // HPバーの分割数＝フェーズ数
constexpr float BossPhaseAggroPerPhase = 0.30f;// フェーズ毎の攻撃頻度上昇（特殊技CD短縮）
constexpr float BossPhaseSpeedPerPhase = 0.15f;// フェーズ毎の移動速度上昇
constexpr float BossPhaseIntroTime = 0.9f;     // フェーズ移行時の小休止（無防備の溜め＝ピーク演出）

// ブレイクコンボ：ブレイク中だけ有効。ヒットを重ねるほど与ダメージ倍率が上がり、ブレイク終了で必ずリセット。
constexpr float BreakComboDamagePerHit = 0.06f;// 1ヒットごとの倍率上昇（+6%）
constexpr float BreakComboMaxMul = 3.0f;       // 倍率の上限

// リフレクションコア：全キャラ共通。敵に攻撃を当ててチャージ→満タンで右クリック設置（時間では消えず壊れるまで残る）。
constexpr float ReflectionCoreCost = 100.0f;        // 設置に必要なチャージ量
constexpr float ReflectionCoreChargePerDamage = 0.7f;// 与ダメージ1あたりのチャージ獲得
constexpr int ReflectionCoreMax = 3;                // 同時設置できる最大数（超えると古い順に消える）
constexpr float ReflectionCoreHp = 140.0f;          // コアの耐久（敵に壊されると消える）

// 反射シールド（全キャラ共通）：左クリックで前方にシールドを展開し、当たったボス攻撃を反射する。
// 反射の「効果」だけキャラごとに異なる（チョコ＝増殖、など）。
constexpr float ReflectShieldActive = 0.45f;     // 展開している時間
constexpr float ReflectShieldCooldown = 0.6f;    // 再展開までのクールダウン
constexpr float ReflectShieldRange = 1.9f;       // 自機からシールドの届く距離
constexpr float ReflectShieldArc = 1.8f;         // シールドの開き角（全角・ラジアン）

// シールドスタミナ：張りっぱなしを防ぐ。構え続けると減り、下ろすと回復。0で一旦構えられない。
constexpr float ShieldStaminaMax = 100.0f;
constexpr float ShieldStaminaDrainPerSec = 32.0f; // 構え中の消費（毎秒）
constexpr float ShieldStaminaRegenPerSec = 26.0f; // 下ろし中の回復（毎秒）
constexpr float ShieldStaminaMinToRaise = 20.0f;  // 切らした後、これだけ回復するまで再展開できない
// パーフェクト反射：構え直した直後の短い窓で反射すると強化（崩しゲージ多め＋増殖＋演出）。
constexpr float PerfectReflectWindow = 0.18f;     // 構え直後にパーフェクト扱いになる時間
constexpr float PerfectReflectBreakMul = 2.2f;    // パーフェクト時の崩しゲージ倍率

// ブレイク中だけ使える攻撃（左クリック）。崩した相手を殴ってブレイクコンボで火力を伸ばす。
constexpr float BreakAttackDamage = 26.0f;       // 1発のダメージ（ブレイクコンボ倍率が乗る）
constexpr float BreakAttackInterval = 0.10f;     // 連射間隔
constexpr float BreakAttackBoltSpeed = 18.0f;    // 弾速

// チョコの「増殖反射」：左クリックのチョコボムが飛行中、敵弾やボスのビームを巻き取り、
// 数を増やしてボスへ撃ち返す。チョコの報酬寄り＝手数でブレイクゲージを稼ぐ。
constexpr int ChocoReflectMultiply = 4;          // 1回の反射で撃ち返す弾数
constexpr float ChocoReflectBoltSpeed = 16.0f;   // 反射弾の速度
constexpr float ChocoReflectBoltDamage = 16.0f;  // 反射弾1発のダメージ
constexpr float ChocoReflectBeamCd = 0.28f;      // ビーム巻き取りの間隔（連続発生防止）
constexpr float ChocoReflectBreakPerCatch = 6.0f;// 1回の反射で溜まるブレイクゲージ

// シールド反射のキャラ別個性。共通モーション（前方シールド展開）で受けた弾を、
// キャラごとに違う「返し方」へ変換する。チョコ=増殖 / ショート=威力 / チーズ=連鎖 / ロール=集束。
// ショート（威力）：少数の極太・高威力弾でまとめて削る。
constexpr int   ShortReflectCount = 1;            // 撃ち返す弾数
constexpr float ShortReflectDamageMul = 2.6f;     // 威力倍率（重い一撃）
constexpr float ShortReflectSpeedMul = 0.95f;     // 速度倍率
constexpr float ShortReflectRadius = 0.44f;       // 弾の太さ
constexpr int   ShortReflectPierce = 2;           // 貫通回数
// チーズ（連鎖）：反射弾がボス→タレット→分身へ自動で飛び移る稲妻型。複数の的に同時に効く。
constexpr int   CheeseReflectCount = 1;           // 撃ち返す弾数
constexpr float CheeseReflectDamageMul = 1.15f;   // 威力倍率
constexpr float CheeseReflectSpeedMul = 1.15f;    // 速度倍率
constexpr float CheeseReflectRadius = 0.22f;      // 弾の太さ
constexpr int   CheeseReflectChainJumps = 4;      // 飛び移り回数（連鎖）
constexpr float CheeseReflectChainGain = 1.12f;   // 1ホップごとの威力倍率
constexpr float CheeseReflectChainSpeed = 17.0f;  // ホップ時の弾速
// ロール（渦）：受けた敵弾を渦に巻き取ってストックし、満タン/パーフェクトで
// 回転しながらスパイラル弾幕として一気に放出する。
constexpr int   RollVortexStockMax = 14;          // 巻き取れる最大ストック数
constexpr int   RollVortexThreshold = 8;          // この数まで貯まると自動で放出
constexpr int   RollVortexBoltsPerStock = 2;      // ストック1つあたり放出する弾数
constexpr float RollVortexSpiralTurns = 1.4f;     // スパイラルの巻き幅（大きいほど広く渦巻く）
constexpr float RollVortexBoltSpeed = 15.0f;      // 放出弾の速度
constexpr float RollVortexBoltDamage = 13.0f;     // 放出弾1発のダメージ
constexpr float RollVortexBoltAngular = 2.4f;     // 放出弾のコークスクリュー回転（rad/s）＝渦の見た目

// ネガポジ：ジャスト回避を一定回数ためると突入。発動中は世界が反転する。
// 被ダメージ→自分が回復＆蓄積し、終了時にまとめてボスへお返し。攻撃すると逆に敵が回復するので受けに徹する。
// お返し倍率は「ネガポジに入った累計回数」で伸びる（恒久成長）。
constexpr int NegaPosiReflectReq = 2;          // 突入に必要なリフレクションコアの反射成功回数（デバッグ用に2）
constexpr float NegaPosiDuration = 7.0f;       // 発動時間（秒）
constexpr float NegaPosiPaybackBase = 1.0f;    // 1回目のお返し倍率
constexpr float NegaPosiPaybackPerCount = 0.5f;// 累計発動回数ごとのお返し倍率上乗せ
constexpr float NegaPosiAccumMax = 800.0f;     // 蓄積ダメージの上限（一撃必殺になりすぎないように）

// ボスの極太回転ビーム薙ぎ払い（パリィ不可）。極太ビームを出しながらゆっくり回す。
constexpr float BossMegaBeamWarnTime = 1.6f;
constexpr float BossMegaBeamActiveTime = 3.2f;   // 回しながら照射する時間
constexpr float BossMegaBeamHalfWidth = 2.1f;    // 極太
constexpr float BossMegaBeamLength = 28.0f;
constexpr float BossMegaBeamDamageMul = 1.5f;
constexpr float BossMegaBeamRotateSpeed = 0.55f; // 回転速度（ラジアン/秒）
constexpr float BossMegaBeamCooldownMin = 17.0f;
constexpr float BossMegaBeamCooldownVar = 6.0f;

// 分身：本体＋分身が反射可能な弾をまとめて撃つ攻撃。
constexpr int BossCloneMax = 2;                 // 同時に出す分身の数
constexpr float BossCloneWarnTime = 1.0f;       // 出現予兆
constexpr float BossCloneActiveTime = 1.2f;     // 分身が残る時間（演出）
constexpr float BossCloneCooldownMin = 9.0f;
constexpr float BossCloneCooldownVar = 4.0f;
constexpr int BossCloneBulletCount = 5;         // 1体が撃つ弾数（扇）
constexpr float BossCloneBulletSpeed = 6.0f;
constexpr float BossCloneBulletSpread = 0.5f;   // 扇の全角（ラジアン）
constexpr float BossCloneDamageMul = 1.0f;      // 弾のダメージ倍率（boss.atk基準）
constexpr float BossCloneOffset = 3.2f;         // 本体からの分身配置距離

// 分裂：HP1/4ほどの分身を出し、一定時間or撃破まで反射可能弾を撃たせる通常技。
constexpr int   BossSplitCount = 2;             // 1回の発動で出す分身数
constexpr int   BossSplitMax = 2;               // 同時に存在できる分身の上限
constexpr float BossSplitHpRatio = 0.25f;       // 分身HP＝ボス最大HPの割合（1/4）
constexpr float BossSplitLifetime = 30.0f;      // 寿命（秒）。これか撃破で消える
constexpr float BossSplitCooldownMin = 16.0f;   // 次の分裂までの最短
constexpr float BossSplitCooldownVar = 6.0f;    // 上乗せ乱数
constexpr float BossSplitRadius = 0.78f;        // 分身の当たり/見た目半径（ミニボス感）
constexpr float BossSplitSpeed = 2.1f;          // 分身の移動速度
constexpr float BossSplitFireInterval = 1.5f;   // 反射可能弾の発射間隔
constexpr int   BossSplitBulletCount = 3;       // 1回の発射数（扇）
constexpr float BossSplitBulletSpeed = 5.5f;    // 弾速
constexpr float BossSplitBulletSpread = 0.4f;   // 扇の全角（ラジアン）
constexpr float BossSplitDamageMul = 0.7f;      // 弾ダメージ（boss.atk基準）

// 扇状斬撃：壁で3回跳ね返り、跳ねるほど巨大化＆強化する斬撃を扇状に飛ばす通常技。
// 本体（自分）かプレイヤー（敵）に当たると消滅。反射可能。
constexpr float BossFanSlashCooldownMin = 9.0f;
constexpr float BossFanSlashCooldownVar = 4.0f;
constexpr int   BossFanSlashCount = 1;          // 飛ばす斬撃数（三日月を1つ）
constexpr float BossFanSlashSpread = 0.7f;      // 扇の全角（ラジアン・複数時のみ使用）
constexpr float BossFanSlashSpeed = 6.5f;       // 初速
constexpr float BossFanSlashRadius = 0.5f;      // 初期サイズ（シールドの大きさ）
constexpr int   BossFanSlashBounce = 3;         // 壁反射回数
constexpr float BossFanSlashGrowth = 1.25f;     // 1反射ごとのサイズ/威力倍率
constexpr float BossFanSlashDamageMul = 0.9f;   // ダメージ（boss.atk基準）
constexpr float BossFanSlashTtl = 6.0f;         // 寿命（秒）

// チャージ衝撃波：チャージ後、フィールド1/2ほどの範囲円へ衝撃波を放つ通常技。
// 衝撃波（＝ボス）から遠いほどダメージが上がる＝ボスに近づくほど安全（高リスク誘導）。
constexpr float BossShockwaveCooldownMin = 11.0f;
constexpr float BossShockwaveCooldownVar = 5.0f;
constexpr float BossShockwaveChargeTime = 1.3f;    // チャージ（予兆）時間。この間に近づいて回避
constexpr float BossShockwaveActiveTime = 0.55f;   // 衝撃波が広がりきるまでの時間
constexpr float BossShockwaveRangeRatio = 0.5f;    // 到達半径＝アリーナ半径×これ（1/2フィールド）
constexpr float BossShockwaveMinDamageMul = 0.15f; // 中心付近（近い）の最小ダメージ（boss.atk基準）
constexpr float BossShockwaveMaxDamageMul = 1.6f;  // 最遠（範囲端）の最大ダメージ（boss.atk基準）

// 隕石落下（大技・反射不可）：一定時間、エリアのランダム位置に隕石を落とし続ける。
// 着弾には予兆（落下位置の表示）があり、踏まなければ回避できる。
constexpr float BossMeteorCooldownMin = 13.0f;
constexpr float BossMeteorCooldownVar = 5.0f;
constexpr float BossMeteorDuration = 5.0f;      // 降らせ続ける時間
constexpr float BossMeteorDropInterval = 0.42f; // 落下の間隔
constexpr float BossMeteorWarnTime = 0.85f;     // 着弾予兆（落下位置表示）の時間
constexpr float BossMeteorImpactTime = 0.32f;   // 着弾エフェクト/判定の時間
constexpr float BossMeteorRadius = 1.7f;        // 着弾の半径
constexpr float BossMeteorDamageMul = 1.4f;     // 着弾ダメージ（boss.atk基準）
constexpr int   BossMeteorPerDrop = 1;          // 1回の落下数

// 突進追走（大技）：高速で走り回り、接触で確定つかみ→引きずりダメージ。
// ※無敵ではない＝この間も反射などでダメージは通る。
constexpr float BossRushCooldownMin = 14.0f;
constexpr float BossRushCooldownVar = 5.0f;
constexpr float BossRushDuration = 6.0f;        // 走り回る時間
constexpr float BossRushSpeedMul = 3.6f;        // 通常移動速度に対する突進速度倍率
constexpr float BossRushDragTime = 1.6f;        // 1回のつかみ引きずり時間
constexpr float BossRushDragDamageMul = 0.6f;   // 引きずり中の周期ダメージ（boss.atk基準）

// タレット：合間にボスが設置する反射可能な砲台。反射弾で壊せる。
constexpr int BossTurretMax = 3;                // 同時設置できる数
constexpr float BossTurretSpawnInterval = 5.0f; // 設置の間隔
constexpr float BossTurretHp = 60.0f;           // 砲台HP（反射弾で削る・tierで増える）
constexpr float BossTurretFireInterval = 1.6f;  // 発射間隔（tierで短縮）
constexpr float BossTurretBulletSpeed = 6.5f;   // 弾速（tierで上昇）
constexpr float BossTurretDamageMul = 0.8f;     // 弾ダメージ（boss.atk基準）
constexpr float BossTurretRadius = 0.6f;        // 当たり/見た目半径
// tier（強さ段階）：フェーズが進むほど強い砲台が出る。tier2はビーム（高速連射ストリーム）。
constexpr int BossTurretBeamBurst = 3;          // ビームタレットの1発あたり連射数

// ボスの腕（ダメージ床）。ボス本体の一部として最寄りプレイヤー方向へ伸び、触れると継続ダメージ。
// 腕に触れた瞬間、つかみ(下記)が可能ならそのプレイヤーを捕獲する。
constexpr float BossArmRadius = 1.1f;        // 腕先端（赤＝掴み判定）の半径
constexpr float BossArmReach = 3.6f;         // ボス中心から腕先端（赤）までの距離
constexpr float BossArmTrackSpeed = 2.2f;    // 腕がプレイヤー方向へ向き直る速さ（ラジアン/秒）
constexpr float BossArmChipPerSec = 8.0f;    // 腕に触れている間の継続ダメージ（毎秒・従来のダメージ床と同等）
constexpr float BossArmSpread = 0.7f;        // 左右2本の腕の開き角の最大値（ラジアン）
constexpr float BossArmSpreadMinRatio = 0.3f;// 開き角の最小＝最大×この割合（この範囲で開閉する）
constexpr float BossArmSpreadSpeed = 1.2f;   // 開閉のゆっくりした速さ（ラジアン/秒）
constexpr float BossArmHpRatio = 0.10f;      // 腕のHP＝ボス最大HPのこの割合。これだけ削ると腕が消滅
constexpr float BossArmDestroyTime = 20.0f;  // 腕が消滅してから復活するまでの時間（秒）

// ボスのつかみ攻撃（回避専用）。腕に触れると捕獲し、拘束ダメージ→解放。
constexpr float BossGrabWarnTime = 0.9f;
constexpr float BossGrabReachTime = 0.35f;       // 手を伸ばす判定時間（外したら終了）
constexpr float BossGrabRange = 6.5f;
constexpr float BossGrabArc = 0.7f;              // 正面コーンの全角（ラジアン）
constexpr float BossGrabHoldTime = 2.2f;         // 拘束時間
constexpr float BossGrabTickDamageMul = 0.275f;  // 拘束中の周期ダメージ（i-frameで約0.45s間引き／旧0.55の1/2）
constexpr float BossGrabCooldownMin = 6.0f;
constexpr float BossGrabCooldownVar = 2.0f;
// つかみ攻撃で腕を「伸ばす」動き。
constexpr float BossGrabTriggerRange = 9.5f;     // この距離以内にプレイヤーがいると掴みを試みる
constexpr float BossGrabReachWarn = 0.55f;       // 腕を引いて溜める予兆時間
constexpr float BossGrabThrustTime = 0.42f;      // 腕を突き出す時間
constexpr float BossGrabReachMax = 9.0f;         // 突き出した腕先（赤）の最大到達距離

// ボスの飛行必殺（フィールドを一周してから大範囲円に強攻撃）。
constexpr float BossFlyWarnTime = 1.2f;          // 飛び立つ前の予兆
constexpr float BossFlyLapTime = 3.0f;           // フィールドを一周する時間
constexpr float BossFlyStrikeWarnTime = 1.5f;    // 着弾円の予兆
constexpr float BossFlyStrikeTime = 0.6f;        // 着弾（判定/演出）
constexpr float BossFlyStrikeRadius = 5.9f;      // 着弾円の半径（フィールドの約1/3）
constexpr float BossFlyDamageMul = 3.0f;         // 必殺の威力
constexpr float BossFlyCooldownMin = 22.0f;
constexpr float BossFlyCooldownVar = 6.0f;

// ボスの薙ぎ払い（近接・回避専用＝パリィ不可）の調整値です。
// プレイヤーが近いときに前方の扇を予告→振り抜く。接近を咎める対近接攻撃。
constexpr float BossSweepWarnTime = 0.85f;     // 予兆（短め＝近接の圧）
constexpr float BossSweepActiveTime = 0.4f;    // 振り抜きの見た目・判定が残る時間
constexpr float BossSweepRange = 5.6f;         // 扇の半径（リーチ）
constexpr float BossSweepArc = 2.7f;           // 扇の全角（ラジアン, 約155度）
constexpr float BossSweepDamageMul = 1.8f;     // boss_.atk への倍率
constexpr float BossSweepCooldownMin = 6.0f;   // 最短クールダウン
constexpr float BossSweepCooldownVar = 2.5f;   // 加算乱数幅
constexpr float BossSweepTriggerRange = 6.6f;  // この距離以内にプレイヤーがいると発動

// ボスの地中突き上げ（Burrow→Eruption・回避専用＝パリィ不可）の調整値です。
// 潜行予兆→地中に潜って無敵→各プレイヤー足元の予測円が追尾→ロック→地面から噴出。
constexpr float BossBurrowWarnTime = 0.8f;     // 潜る前の予兆（地上で発光）
constexpr float BossBurrowSubmergeTime = 2.2f; // 潜行（無敵・非表示）の時間
constexpr float BossBurrowLockTime = 0.7f;     // 潜行残りこの秒数で予測円をロック（以降は追尾しない）
constexpr float BossBurrowEruptTime = 0.5f;    // 噴出の判定/演出が残る時間
constexpr float BossBurrowRadius = 2.3f;       // 噴出1発の半径
constexpr float BossBurrowDamageMul = 2.0f;    // boss_.atk への倍率（強攻撃）
constexpr float BossBurrowCooldownMin = 14.0f; // 最短クールダウン（低頻度）
constexpr float BossBurrowCooldownVar = 4.0f;  // 加算乱数幅

// キャラクター、敵、ボス、ステージ、アイテムなどの分類を enum で固定します。
// int のまま扱うと「0 が何を意味するか」が分かりにくいため、コード上では
// CharacterType::Chocolate のように名前で読める形へ寄せています。
enum class CharacterType
{
    Shortcake = 0,
    Chocolate = 1,
    Cheese = 2,
    Roll = 3
};

enum class EnemyType
{
    Normal = 0,
    Shield,
    Split,
    Healer,
    Barrier,
    Mirror,
    Mine,
    Teleport
};

enum class BossType
{
    Demon = 0,
    DonutKing,
    MirrorMacaron,
    GravityPudding,
    TerritoryCake,
    DemonParfait,
    ThunderCaptain,
    HiddenBoss
};

// 各ボスが1つだけ持つ大技（反射不可）。SpawnBossでランダム/ラッシュ順に付与。
enum class BossBigMove
{
    MegaBeam = 0,       // 極太レーザー薙ぎ払い（既存）
    Meteor,             // 隕石落下（新）
    InvincibleChase,    // 無敵で走り回り接触で確定つかみ（新）
    Count
};

// ボスラッシュ（連続戦）の体数。
constexpr int GauntletBossCount = 3;

enum class StageType
{
    Donut = 0,
    TwinIsland,
    Pinball,
    RingCorridor,
    FourPillars,
    MovingIsland,
    ShrinkRing,
    BossArena
};

enum class FieldShape
{
    Circle = 0,
    Rectangle,
    Octagon,
    Corridor,
    Ring,
    ShrinkCircle
};

enum class PickupType
{
    Attack = 0,
    Slow,
    Invincible,
    Magnet,
    BombDamage,
    Heal,
    UltFull,
    Spread,
    Speed,
    ScoreDouble
};

enum class CoopSlotMode
{
    Off = 0,
    AI,
    Pad
};

// 攻撃方向の決め方です。
// Cursor はマウス照準、MoveDirection は移動方向、AutoTarget は近い敵を自動で向きます。
enum class AimMode
{
    Cursor = 0,
    MoveDirection,
    AutoTarget
};

enum class BossDamageKind
{
    NormalShot = 0,
    ChargeShot,
    ChocolateCharge,
    Melee,
    Bomb,
    Ultimate,
    ReflectedShot,
    HiddenBossAuraKey
};

enum class BossPatternId
{
    Radial = 0,
    Aimed,
    Spiral,
    Curve,
    Seal,
    GuardRing,
    MirrorSplit,
    GravityWell,
    TerritoryZone,
    Beam,
    SkyLaser
};

// UI表示用の短い名前を返すヘルパーです。
// ここで表示名をまとめておくと、ポーズ画面など複数箇所で同じ表記を使えます。
inline constexpr const wchar_t* AimModeName(AimMode mode)
{
    switch (mode)
    {
    case AimMode::Cursor: return L"マウス";
    case AimMode::AutoTarget: return L"近い敵";
    case AimMode::MoveDirection:
    default: return L"移動方向";
    }
}

struct CharacterText
{
    const wchar_t* jpName;
    const wchar_t* roleIcon;
    const wchar_t* normal;
    const wchar_t* charge;
    const wchar_t* ultimate;
};

// キャラごとの説明文です。
// キャラ選択画面ではここを参照し、性能値は GameStateTypes.h の Loadouts を参照します。
inline constexpr std::array<CharacterText, 4> CharacterTexts{ {
    { L"ショート", L"ST", L"誘導いちご弾", L"苺リコシェ場", L"巨大メテオ" },
    { L"チョコ", L"CH", L"チャージボム", L"チョコウォール", L"時計斬り" },
    { L"チーズ", L"CZ", L"チーズショット", L"ストレッチダッシュ", L"無敵要塞" },
    { L"ロール", L"RL", L"バウンドロール弾", L"最大溜め突進", L"全画面叩きつけ" },
} };

// ステージ名やボス名は、ゲーム内メッセージやHUD表示で使います。
// 実装側では StageType / BossType だけを渡し、文字列はここで一元管理します。
inline constexpr const wchar_t* StageName(StageType type)
{
    switch (type)
    {
    case StageType::Donut: return L"ドーナツ";
    case StageType::TwinIsland: return L"双子島";
    case StageType::Pinball: return L"ピンボール";
    case StageType::RingCorridor: return L"リング回廊";
    case StageType::FourPillars: return L"四隅の柱";
    case StageType::MovingIsland: return L"動く島";
    case StageType::ShrinkRing: return L"収縮リング";
    case StageType::BossArena: return L"ボス円形";
    default: return L"不明";
    }
}

inline constexpr const wchar_t* BossName(BossType type)
{
    switch (type)
    {
    case BossType::Demon: return L"大ボス";
    case BossType::DonutKing: return L"ドーナツキング";
    case BossType::MirrorMacaron: return L"ミラーマカロン";
    case BossType::GravityPudding: return L"グラビティプリン";
    case BossType::TerritoryCake: return L"テリトリーケーキ";
    case BossType::DemonParfait: return L"魔王パフェ";
    case BossType::ThunderCaptain: return L"キャプテンサンダー";
    case BossType::HiddenBoss: return L"隠しボス";
    default: return L"ボス";
    }
}
