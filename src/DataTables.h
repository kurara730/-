#pragma once

// 外部データ(CSV)からゲームのパラメータを読み込むための関数群。
//
// 方針:
//   - キャラ性能などの「調整したい数値」を assets/data/*.csv に置き、起動時に読み込む。
//   - CSV が存在しない/壊れている場合は、コード内の既定値(GameStateTypes.h の初期値)を
//     そのまま使う。よってデータファイルが無くてもゲームは必ず起動できる。
//   - Excel / Google スプレッドシートで編集 → Google Drive 同期 → 反映、という運用を想定。
//
// 再読込:
//   デバッグパネルの「データ再読込」ボタンからも同じ関数を呼べるので、
//   ゲームを起動したまま CSV を保存し直して即反映できる。

// assets/data/characters.csv を読み込み、Loadouts[] を上書きする。
// 失敗時は何もしない(既定値を維持)。
void LoadCharacterTableFromCsv();
