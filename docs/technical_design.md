# スイーツパニック DX11 技術設計書

## 1. 目的

この文書は、企画仕様をDirect3D 11 / Win32実装へ落とし込むための技術設計である。現行プロジェクトの分割方針を維持し、ゲームロジック、描画、入力、敵制御、アセット差し替え、シェーダーを独立した責務として扱う。

初期実装はプリミティブメッシュで成立させる。後から画像、スプライト、テクスチャ、FBX/glTFモデルへ差し替えられるよう、ゲームロジックは描画アセットの実体を直接参照しない。

## 2. 現行構成

主要ファイルと責務は以下。

| ファイル | 責務 |
| --- | --- |
| `src/main.cpp` | WinMain、Win32メッセージ入口 |
| `src/SweetsApp.h/.cpp` | アプリ状態、入力、メインループ、共有状態 |
| `src/GameTypes.h` | 数学ヘルパー、ゲーム用構造体、武器、ロードアウト |
| `src/GameLogic.cpp` | ウェーブ、ステージ、弾、ピックアップ、スコア |
| `src/PlayerController.cpp` | プレイヤー移動、攻撃、必殺、ボム、被弾 |
| `src/EnemyController.cpp` | 敵生成、敵AI、ボスAI、ダメージ処理 |
| `src/Renderer.cpp` | D3D11/D2D/DWrite初期化、3D描画、HUD |
| `src/AssetCatalog.h/.cpp` | 見た目差し替え用のビジュアル登録 |
| `src/TextureLibrary.h/.cpp` | テクスチャ情報と将来のSRV管理 |
| `src/SpriteLibrary.h/.cpp` | スプライト情報、UV、テクスチャID |
| `src/ModelLibrary.h/.cpp` | 将来のFBX/glTFモデル情報 |
| `assets/shaders/basic_lit.hlsl` | 頂点/ピクセルシェーダー |

この分割を維持し、機能追加時も「ゲームルールはロジック側」「見た目はRenderer/AssetCatalog側」に分ける。

## 3. 主要データ設計

### 既存構造体

| 構造体 | 用途 |
| --- | --- |
| `Player` | 位置、HP、速度、武器、必殺、ボム、集中移動、グレイズ状態 |
| `Enemy` | 通常敵の位置、HP、速度、攻撃、種類、AIタイマー |
| `Boss` | ボス位置、HP、フェーズ、弾幕タイマー、種類 |
| `Shot` | プレイヤー弾と敵弾。速度、半径、ダメージ、貫通、反射、曲げ、加減速を持つ |
| `Slash` | チョコの近接攻撃判定 |
| `Pickup` | アイテム位置、種類、寿命 |
| `Obstacle` | 障害物、壁、反射地形 |
| `LoadoutPreset` | キャラ選択時の性能プリセット |
| `WeaponDef` | 武器の基本性能 |

### 追加すべき列挙型

現行実装では一部が `int kind` や `Weapon` で表現されている。仕様を安定させるため、次の列挙型を追加する。

```cpp
enum class CharacterType { Shortcake, Chocolate, Cheese, Roll };
enum class EnemyType { Normal, Shield, Split, Healer, Barrier, Mirror, Mine, Teleport };
enum class BossType { Demon, DonutKing, MirrorMacaron, GravityPudding, TerritoryCake, DemonParfait };
enum class StageType { Donut, TwinIsland, Pinball, RingCorridor, FourPillars, MovingIsland, ShrinkRing, BossArena };
enum class PickupType { Attack, Slow, Invincible, Magnet, BombDamage, Heal, UltFull, Spread, Speed, ScoreDouble };
```

移行時は既存の `Enemy::kind`, `Boss::type`, `Pickup::type`, `Obstacle::kind` を段階的に置き換える。保存データがないため、互換変換は不要。

## 4. ゲーム状態遷移

画面状態は `Screen` で管理する。

- `Title`: キャラ/性能選択
- `Playing`: 戦闘中
- `Paused`: 一時停止
- `GameOver`: 結果表示

`SweetsApp::Update` は画面状態ごとに更新先を分ける。戦闘中は以下の順で処理する。

1. 経過時間、無敵、バフ、メッセージなどのタイマー更新
2. プレイヤー更新
3. 敵更新
4. ボス更新
5. 弾更新
6. ピックアップ更新
7. パーティクル更新
8. 敵生成とウェーブクリア判定
9. ゲームオーバー判定

この順序により、入力反映後に敵と弾が動き、最後に死亡やクリアを確定する。

## 5. プレイヤー設計

### 入力

`SweetsApp` がWin32メッセージとキー配列を管理し、`PlayerController.cpp` がゲーム用入力に変換する。

- 移動: WASD / 方向キー
- 照準: `ScreenToWorld(mouseX, mouseY)`
- 通常攻撃: 左クリック / Space
- 必殺: Q
- ボム: X / Control
- 集中移動: Shift

4人協力へ拡張する際は、`Player player_` を `std::vector<Player> players_` または固定長 `std::array<Player, 4>` に変更する。入力元は `InputSlot` として分離し、人間/AI/未参加を切り替える。

### 集中移動と当たり判定

`Player` は見た目用 `radius` と被弾用 `hitboxRadius` を分ける。敵弾との接触は `hitboxRadius` を使い、集中移動中はRendererが小さい当たり判定リングを描画する。

### グレイズ

`Shot::enemy == true` の弾に対して、以下で判定する。

- `distance < shot.radius + player.grazeRadius`
- `distance >= shot.radius + player.hitboxRadius`
- `Shot::grazed == false`

成立時は `Shot::grazed = true`、グレイズ数、連続グレイズ数、スコア、必殺ゲージを加算する。被弾時は連続グレイズ数をリセットする。

### ボム

`UseBomb()` は以下を行う。

1. 戦闘中、ボム残数あり、ボムクールダウンなしを確認
2. ボム残数を1減らす
3. 短時間無敵とシールドを付与
4. 敵弾を消す
5. 周囲の敵とボスへダメージ
6. 消した弾数に応じてスコア加算
7. D2D/HUDメッセージと3Dエフェクトを出す

将来的にキャラ別ボムへ拡張する場合も、共通処理は `UseBomb()` に残し、効果部分のみ `CharacterType` で分岐する。

## 6. 攻撃と反射設計

### Shot

`Shot` はプレイヤー弾と敵弾を同じ構造体で扱う。

- `pos`, `vel`: 平面上の位置と速度
- `radius`: 当たり判定半径
- `damage`: 基本ダメージ
- `ttl`: 寿命
- `pierce`: 貫通残数
- `bounce`: 反射残数
- `angularVel`: 軌道を曲げる角速度
- `accel`: 加速度
- `enemy`: 敵弾かどうか
- `grazed`: グレイズ済みかどうか

反射倍率を厳密に扱うため、次工程で `int reflectedCount` と `bool reflected` を追加する。ダメージは `damage * min(pow(1.5f, reflectedCount), 5.0f)` で計算する。

### 反射処理

外周または障害物へ衝突したら、法線 `n` に対して速度を反射する。

```cpp
velocity -= n * (2.0f * Dot(velocity, n));
```

反射時に行う処理:

- 位置を衝突面の外へ押し戻す
- `bounce` を減らす
- `reflectedCount` を増やす
- 反射エフェクトを出す
- プレイヤー弾なら威力倍率を更新する
- 敵弾をチーズ壁が受けた場合はプレイヤー弾へ変換する

### キャラ別攻撃実装

| CharacterType | 実装場所 | 要点 |
| --- | --- | --- |
| `Shortcake` | `FirePrimary`, `ChargeAttack`, `UseUltimate` | 追尾処理は `Shot` にターゲットIDまたは誘導係数を追加する |
| `Chocolate` | `DoMelee`, `ChargeAttack` | 近接は `Slash`、斬撃波は `Shot` または専用 `WaveAttack` |
| `Cheese` | `Obstacle` / `CheeseWall` | 壁は耐久、反射所有者、寿命を持つ |
| `Roll` | `Shot` / `PlayerDash` | ロール弾と突進に反射回数と連続ヒット制限を持たせる |

チャージ攻撃用に `Player` へ `chargeT`, `charging`, `chargeReady` を追加する。入力は押下開始、継続、離しで管理する。

## 7. 敵・ボス設計

### 通常敵

現行の `Enemy::kind` を `EnemyType` に置き換える。敵AIは `UpdateEnemies` 内の巨大な分岐から、将来的には種類別関数へ分離する。

推奨分割:

- `UpdateEnemyNormal`
- `UpdateEnemyShield`
- `UpdateEnemySplit`
- `UpdateEnemyHealer`
- `UpdateEnemyBarrier`
- `UpdateEnemyMirror`
- `UpdateEnemyMine`
- `UpdateEnemyTeleport`

支援敵は周囲検索を行うため、更新順は「支援効果を計算」「移動と攻撃」「死亡処理」の2段階に分けると安定する。

### ボス

`Boss` は `BossType` と `phase` を持つ。HP割合に応じてフェーズを更新する。

- phase 1: 基本弾幕
- phase 2: 弾数増加
- phase 3: 曲がる弾、追加弾幕
- phase 4: 複合弾幕、召喚

ボス弾は `SpawnEnemyShot` を統一入口にする。弾幕パターンは以下の関数へ分離する。

- `SpawnBossRadialPattern`
- `SpawnBossAimedFanPattern`
- `SpawnBossSpiralPattern`
- `SpawnBossCurvePattern`
- `SpawnBossSummonPattern`

これにより `BossType` ごとの組み合わせを作りやすくする。

## 8. ステージ設計

`BuildStage()` は `StageType` を受け取り、障害物とギミックを配置する。

初期は `Obstacle` で島、柱、バンパー、壁を表現する。将来的に以下を追加する。

- `moving`: 動く島かどうか
- `velocity`: 障害物移動速度
- `reflectPower`: 反射時の速度/威力補正
- `damageField`: 危険床かどうか
- `owner`: チーズ壁などプレイヤー設置物の所有者

ボス戦は弾幕視認性を優先し、基本は障害物を置かない `BossArena` とする。特殊ボスのみ床ギミックや遮蔽を追加する。

## 9. UI設計

### キャラ選択UI

`DrawLoadoutSelection()` を拡張し、4キャラカードを横並びまたは2x2で描画する。カードはD2D/DWriteで描き、選択中カードのみ3Dプレビューのリングや発光を強くする。

カード内要素:

- キャラ名
- 役割アイコン
- 通常攻撃、チャージ、必殺の短文
- HP、速度、攻撃、連射、反射のステータスバー
- 人間/AI切り替え
- 操作プレビュー

ステータスバーは数値よりも視覚を優先する。プレイヤーが説明文を読まなくても性能差を判別できる状態を合格基準とする。

### 戦闘HUD

`DrawHud()` は以下を表示する。

- HP
- 必殺ゲージ
- ボム残数
- グレイズ数
- レベル/EXP
- スコア
- ウェーブ
- ボスHP
- 一時メッセージ

4人協力時は画面端にプレイヤー別の小HUDを並べ、中央上部にウェーブ/ボス情報をまとめる。

## 10. 描画・シェーダー設計

### Renderer

`Renderer.cpp` は以下を担当する。

- D3D11デバイス、スワップチェーン、深度バッファ
- HLSLコンパイル
- 基本メッシュ生成
- 3Dシーン描画
- D2D/DWriteによるHUD描画

ゲームロジックから直接D3Dリソースを参照しない。描画に必要な情報は `Player`, `Enemy`, `Shot`, `Obstacle`, `Pickup`, `Boss` の状態から読み取る。

### シェーダー

現行の `assets/shaders/basic_lit.hlsl` を基本シェーダーとする。

初期シェーダー責務:

- 頂点変換
- 法線による簡易ライティング
- オブジェクトカラーの反映
- アルファ付きリングや弾の発光表現

拡張時に追加するシェーダー:

- `sprite_unlit.hlsl`: ビルボード/2Dスプライト用
- `model_lit.hlsl`: 法線、UV、テクスチャ付き3Dモデル用
- `effect_additive.hlsl`: 弾幕、反射、ボム、グレイズの加算エフェクト用

HLSLファイルは `assets/shaders/` に分け、CMakeではビルド時に実行ファイル横へコピーする。

## 11. アセット差し替え設計

`AssetCatalog` を正式な見た目差し替え窓口にする。ゲームロジックは `VisualRole` のみを知り、実際の見た目は `AssetCatalog` が返す。

### VisualAsset

`VisualAsset` は以下を持つ。

- `role`: プレイヤー、敵、ボス、弾、アイテム、障害物などの用途
- `meshKind`: プリミティブ fallback
- `fallbackColor`: 外部アセット未設定時の色
- `label`: デバッグ表示名
- `texturePath`: テクスチャパス
- `spritePath`: スプライトパス
- `modelPath`: モデルパス

### テクスチャ

`TextureLibrary` はWIC経由でPNG/JPEGを読み込み、`ID3D11ShaderResourceView` を管理する拡張を行う。初期段階ではパス登録のみでもよい。

### スプライト

`SpriteLibrary` は以下を管理する。

- テクスチャID
- UV矩形
- ピボット
- 表示サイズ
- ビルボード描画フラグ

弾、アイテム、UIプレビューはスプライト差し替え候補である。

### FBX/glTFモデル

`ModelLibrary` はモデルID、パス、メッシュ、マテリアル、テクスチャ参照を管理する。FBXを直接扱う場合はFBX SDK、軽量運用ではglTFへ変換して読み込む方針にする。

モデル読み込み後も、ゲーム側は `VisualRole::Player` や `VisualRole::Boss` を指定するだけにする。

## 12. ファイル追加方針

今後の実装で肥大化を避けるため、以下の分割を推奨する。

| 追加ファイル | 用途 |
| --- | --- |
| `src/CharacterTypes.h` | `CharacterType` とキャラ別定義 |
| `src/StageTypes.h` | `StageType` と地形生成パラメータ |
| `src/EnemyTypes.h` | `EnemyType`, `BossType` と敵定義 |
| `src/PickupTypes.h` | `PickupType` と効果定義 |
| `src/ReflectionSystem.cpp` | 反射判定、倍率、反射キル処理 |
| `src/ChargeAttack.cpp` | チャージ入力とキャラ別チャージ攻撃 |
| `src/CoopController.cpp` | 2から4人プレイ、AI参加枠、救助 |
| `src/AssetLoaders.cpp` | WIC、スプライト、モデル読み込み |

ただし、現時点で小さい機能は既存ファイルに残してよい。分割は責務が明確に増えた段階で行う。

## 13. 実装順序

1. 列挙型を導入し、`int kind` を段階的に置き換える。
2. 反射回数、反射倍率、反射キル判定を `Shot` とダメージ処理へ追加する。
3. チャージ攻撃入力と4キャラ分のチャージ処理を実装する。
4. ステージタイプを `BuildStage(StageType)` へ拡張する。
5. 敵タイプとボスタイプを仕様どおりに増やす。
6. キャラ選択UIをカード型へ改善する。
7. アイテム10種と成長選択UIを追加する。
8. `AssetCatalog` 経由でテクスチャ/スプライト/モデルを読み込めるようにする。
9. 2から4人協力、AI、救助を追加する。

## 14. テスト計画

### ビルド

- CMake configure
- Debugビルド
- Releaseビルド
- `SweetsActionDX11_Game.sln` からVisual Studioデバッグ起動

### ゲームプレイ確認

- 4キャラを選択できる
- 通常攻撃がキャラごとに違う
- チャージ攻撃が押下/離しで発動する
- 反射で弾の向きが変わり、威力とスコアが上がる
- 鏡敵が反射済み攻撃でのみ倒れる
- Shift集中移動で当たり判定が表示される
- 敵弾をかすめるとグレイズが増える
- X/Controlボムで敵弾が消え、無敵とダメージが入る
- ボスフェーズごとに弾幕が増える
- アイテム10種が正しく発動する
- ステージタイプごとに反射地形が変わる

### UI確認

- キャラカードの文字が画面内に収まる
- ステータスバーで性能差が直感的に分かる
- マウス、キーボード、ゲームパッドで選択できる
- 戦闘HUDが弾幕や敵に隠れすぎない

### アセット差し替え確認

- 外部アセット未設定時はfallbackメッシュで起動する
- `AssetCatalog` に画像パスを入れてもゲームロジックが変わらない
- シェーダーファイルが実行ファイル横へコピーされる
- モデル読み込み失敗時はfallback表示へ戻る

