# ============================================================
#  sync-data.ps1
#  Google ドライブの編集用フォルダ <-> ゲームの assets\data を同期します。
#  （方式A：Google Drive for desktop のローカル同期フォルダを使う）
#
#  使い方:
#    1. 下の $DriveDataDir を、自分の編集用フォルダのパスに書き換える。
#    2. PowerShell でこのスクリプトを実行:
#         pwsh -File tools\sync-data.ps1            # ドライブ -> ゲーム（取り込み）
#         pwsh -File tools\sync-data.ps1 -Push      # ゲーム -> ドライブ（書き出し）
# ============================================================

param(
    # -Push を付けると「ゲーム -> ドライブ」へコピー（既定はドライブ -> ゲーム）
    [switch]$Push
)

$ErrorActionPreference = 'Stop'

# ▼▼▼ ここを自分の環境に合わせて書き換える ▼▼▼
$DriveDataDir = 'G:\マイドライブ\19project_data'
# ▲▲▲ 例: G:\マイドライブ\19project_data ▲▲▲

# このスクリプトの場所からゲームの assets\data を求める（tools\ の一つ上が repo ルート）
$repoRoot   = Split-Path -Parent $PSScriptRoot
$gameDataDir = Join-Path $repoRoot 'assets\data'

if (-not (Test-Path $DriveDataDir)) {
    Write-Host "編集用フォルダが見つかりません: $DriveDataDir" -ForegroundColor Red
    Write-Host "スクリプト先頭の `$DriveDataDir を正しいパスに直してください。" -ForegroundColor Yellow
    exit 1
}
if (-not (Test-Path $gameDataDir)) {
    Write-Host "ゲームの assets\data が見つかりません: $gameDataDir" -ForegroundColor Red
    exit 1
}

if ($Push) {
    $src = $gameDataDir; $dst = $DriveDataDir
    Write-Host "ゲーム -> ドライブ へ CSV を書き出します" -ForegroundColor Cyan
} else {
    $src = $DriveDataDir; $dst = $gameDataDir
    Write-Host "ドライブ -> ゲーム へ CSV を取り込みます" -ForegroundColor Cyan
}

# CSV だけをコピー（README などは対象外）
$files = Get-ChildItem -Path $src -Filter *.csv -File
if (-not $files) {
    Write-Host "コピー対象の .csv が $src にありません。" -ForegroundColor Yellow
    exit 0
}
foreach ($f in $files) {
    Copy-Item -Path $f.FullName -Destination (Join-Path $dst $f.Name) -Force
    Write-Host ("  copied: {0}" -f $f.Name)
}
Write-Host "完了。" -ForegroundColor Green
Write-Host "（取り込み後はゲームの『データ再読込』ボタン、または再起動で反映されます）"
