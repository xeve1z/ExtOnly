param(
    [switch]$Debug
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot

if ($Debug) {
    & (Join-Path $root 'build.ps1') -Debug
} else {
    & (Join-Path $root 'build.ps1')
}

$dllPath = Join-Path $root 'build\ExtOnly.dll'
if (-not (Test-Path -LiteralPath $dllPath)) { throw "DLL が見つかりません: $dllPath" }

$candidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 5\ISCC.exe')
)
$iscc = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup (ISCC.exe) が見つかりません。Inno Setup 6 をインストールしてください。' }

Write-Host 'インストーラーを作成中...'
& $iscc (Join-Path $root 'installer.iss')
if ($LASTEXITCODE -ne 0) { throw "インストーラーの作成に失敗しました (exit $LASTEXITCODE)" }

Write-Host "完成: $(Join-Path $root 'dist\ExtOnlySetup.exe')"
