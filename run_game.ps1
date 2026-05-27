$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exeCandidates = @(
    (Join-Path $root 'build\bin\Release\SweetsActionDX11.exe'),
    (Join-Path $root 'build\Release\SweetsActionDX11.exe'),
    (Join-Path $root 'build\bin\Debug\SweetsActionDX11.exe'),
    (Join-Path $root 'build\Debug\SweetsActionDX11.exe')
)

$exe = $exeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $exe) {
    Write-Host 'Executable not found. Building Release...'
    cmake -S $root -B (Join-Path $root 'build') -G 'Visual Studio 17 2022' -A x64
    cmake --build (Join-Path $root 'build') --config Release
    $exe = $exeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (-not $exe) {
    throw 'SweetsActionDX11.exe was not produced. Check the CMake/MSBuild output above.'
}

Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe)
