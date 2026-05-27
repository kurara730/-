
- 透視カメラ付きのD3D11 3Dアリーナ
- WASD / 矢印キー移動
- マウス照準
- 左クリック / Space長押しで通常弾を撃ちながらチャージ、離すとチャージ攻撃
- Q必殺技
- Shift低速移動、小さい当たり判定、グレイズ、X/Controlボム
- 反射倍率、反射キル、鏡敵、チーズ壁による敵弾反射
- 1Pキーボード/マウスだけで遊べるソロ優先
- 直感的な4キャラカード式の性能選択
- 8ステージ、8敵タイプ、6ボスタイプ、10アイテム
- 日本語DirectWrite HUD
- `assets/shaders` 配下の外部HLSLシェーダー
- `assets/textures` 配下の差し替え可能な仮PNGテクスチャ

## 操作

| 入力 | 動作 |
| --- | --- |
| Enter | タイトルから開始 / ゲームオーバー後に再開 |
| キャラカードをクリック | タイトル画面で性能を選択 |
| Left / Right または A / D | タイトル画面で選択を移動 |
| WASD / 矢印キー | 移動 |
| マウス | 照準 |
| 左クリック / Space | 長押し中は小さい通常弾、離すとチャージ攻撃 |
| Shift | 低速移動と当たり判定/グレイズリング表示 |
| Q | 必殺ゲージ満タン時に必殺技 |
| X / Control | ボム |
| 右クリック | チーズ使用時にチーズ壁を設置 |
| 1 | ショート |
| 2 | チョコ |
| 3 | チーズ |
| 4 | ロール |
| XInputゲームパッド | 将来のP2-P4ローカル協力用 |
| P / Esc | 一時停止 |
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

## アセット差し替え

`AssetCatalog` が見た目の差し替え窓口です。各ビジュアル役割は `texturePath`、`spritePath`、`modelPath` を持てます。`TextureLibrary` はWICでPNG/JPEG/BMPを読み込み、`SpriteLibrary` はUVを管理します。現在の描画は内蔵メッシュへフォールバックするため、画像や将来のFBX/glTFが未設定でもゲームは起動します。