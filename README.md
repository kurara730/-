
- 透視カメラ付きのD3D11 3Dアリーナ
- WASD / 矢印キー移動
- マウス照準
- 左クリック / Spaceで通常弾、右クリック長押しでチャージ攻撃
- Q必殺技
- Shift低速移動、小さい当たり判定、グレイズ、X/Controlボム
- XAudio2 + Media Foundationによるプレイ中/ゲームオーバー/隠しボスBGM
- 5段階難易度とLunaticクリア後の隠しボス耐久戦
- タイトルBGM、Story/Endless開始項目、ゲームオーバー時のRetry/Title選択
- Debugビルド限定のF1クリック式デバッグパネル、FPS表示、RT/TAA確認、開発チート
- 反射倍率、反射キル、鏡敵、チーズ壁による敵弾反射
- 1Pキーボード/マウスだけで遊べるソロ優先
- 直感的な4キャラカード式の性能選択
- 8ステージ、8敵タイプ、6ボスタイプ、10アイテム
- 日本語DirectWrite HUD
- `assets/shaders` 配下の外部HLSLシェーダー
- `assets/textures` 配下の差し替え可能な仮PNGテクスチャ
- `assets/video/title.mp4` 配置時のタイトル動画再生

## 操作

| 入力 | 動作 |
| --- | --- |
| Enter | タイトル項目を決定 / 難易度選択で開始 / リザルト後に戻る |
| キャラカードをクリック | タイトル画面で性能を選択 |
| W / S または Up / Down | タイトル画面でStory/Endless/Creditsを選択 |
| Left / Right または A / D | タイトルのキャラ選択 / 難易度画面 / ゲームオーバー選択 |
| 難易度カードをクリック | 難易度を選択 |
| F1 | Debugビルド限定のクリック式デバッグパネルを開閉 |
| WASD / 矢印キー | 移動 |
| マウス | 照準 |
| 左クリック / Space | 小さい通常弾を連射 |
| 右クリック短押し | チーズ使用時にチーズ壁を設置 |
| 右クリック長押し | チャージ攻撃 |
| Shift | 低速移動と当たり判定/グレイズリング表示 |
| Q | 必殺ゲージ満タン時に必殺技 |
| X / Control | ボム |
| 1 | ショート |
| 2 | チョコ |
| 3 | チーズ |
| 4 | ロール |
| C | タイトル画面でクレジットを表示 |
| XInputゲームパッド | 将来のP2-P4ローカル協力用 |
| P / Esc | 一時停止。ポーズ画面でResume/Titleと音量を操作 |
| R | リスタート |

## ソース構成

| ファイル | 役割 |
| --- | --- |
| `src/main.cpp` | WinMain入口とメッセージ橋渡し |
| `src/SweetsApp.h/.cpp` | Win32アプリのライフサイクル、入力、共有状態 |
| `src/GameTypes.h` | 数学ヘルパー、ゲーム構造体、武器、ロードアウト |
| `src/GameDefs.h` | キャラ、敵、ボス、ステージ、アイテム定義 |
| `src/GameLogic.cpp` | ウェーブ、アイテム、弾、パーティクル、スコア |
| `src/PlayerController.cpp` | プレイヤー移動、攻撃、必殺、被弾 |
| `src/ChargeAttack.cpp` | キャラ別チャージ攻撃と分裂弾 |
| `src/CoopController.cpp` | 協力/ゲームパッド/AI対応コード。現状はソロ優先で未使用 |
| `src/EnemyController.cpp` | 敵とボスの生成、移動、攻撃、ダメージ |
| `src/ReflectionSystem.cpp` | 反射速度、反射ダメージ倍率 |
| `src/StageFactory.cpp` | ステージ選択、障害物、可動フィールド |
| `src/Renderer.cpp` | D3D11/D2D設定、メッシュ生成、描画、HUD |
| `src/AssetLoaders.cpp` | 標準テクスチャ/スプライト登録 |
| `src/AssetCatalog.h/.cpp` | 差し替え可能なビジュアルアセット管理 |
| `src/TextureLibrary.h/.cpp` | WICテクスチャ読み込みとSRV管理 |
| `src/SpriteLibrary.h/.cpp` | スプライトID、テクスチャID、UV矩形 |
| `src/ModelLibrary.h/.cpp` | 将来のFBX/glTF読み込み用モデル情報 |
| `assets/shaders/basic_lit.hlsl` | 3Dプリミティブ/モデル用ライティングシェーダー |
| `assets/shaders/sprite_unlit.hlsl` | スプライト/ビルボード用シェーダー |
| `assets/shaders/effect_additive.hlsl` | 弾、反射、ボム用の加算エフェクトシェーダー |
| `assets/shaders/postprocess.hlsl` | Scene/Additive/History RT合成とDebug TAA表示 |
| `assets/audio/333_BPM177.mp3` | タイトル/メニューBGM |
| `assets/audio/233_BPM163.mp3` | ゲームプレイ中BGM |
| `assets/audio/ruins.mp3` | ゲームオーバーBGM |
| `assets/audio/Lonery boy.wav` | 隠しボスBGM |
| `assets/video/title.mp4` | 任意配置のタイトル画面動画 |

## クレジット

- Gameplay BGM: 空想キャンパス - BGMer 様
- Game Over BGM: ruins - DOVA-SYNDROME 様

## アセット差し替え

`AssetCatalog` が見た目の差し替え窓口です。各ビジュアル役割は `texturePath`、`spritePath`、`modelPath` を持てます。`TextureLibrary` はWICでPNG/JPEG/BMPを読み込み、`SpriteLibrary` はUVを管理します。現在の描画は内蔵メッシュへフォールバックするため、画像や将来のFBX/glTFが未設定でもゲームは起動します。タイトル動画は `assets/video/title.mp4` を置くと右側メディア枠でループ再生され、無い場合はフォールバック表示になります。
