param(
    [switch]$Debug
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$srcDir = Join-Path $root 'src'
$outDir = Join-Path $root 'build'

if (-not (Test-Path -LiteralPath $srcDir)) { throw "src フォルダが見つかりません: $srcDir" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw "vswhere.exe が見つかりません（Visual Studio が入っていない可能性があります）" }

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "C++ ワークロード入りの Visual Studio が見つかりません" }

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvars64.bat が見つかりません: $vcvars" }

$optimize = if ($Debug) { '/Od /Zi /DEBUG /DEXTONLY_TRACE' } else { '/O2 /DNDEBUG' }

$sources = Get-ChildItem -LiteralPath $srcDir -Filter '*.cpp' |
    Sort-Object Name |
    ForEach-Object { '"' + $_.FullName + '"' }

$defFile = Join-Path $root 'ExtOnly.def'

$cl = 'cl /nologo /std:c++17 /W4 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 {0} /LD {1} /Fe:ExtOnly.dll /link /DEF:"{2}" /DLL shell32.lib shlwapi.lib ole32.lib oleaut32.lib uuid.lib user32.lib' -f `
    $optimize, ($sources -join ' '), $defFile

$batchPath = Join-Path $outDir '_build.bat'
$batch = @()
$batch += '@echo off'
$batch += ('call "{0}" >nul' -f $vcvars)
$batch += ('cd /d "{0}"' -f $outDir)
$batch += $cl
Set-Content -LiteralPath $batchPath -Value $batch -Encoding Oem

$wasLoaded = $false
foreach ($process in (Get-Process explorer -ErrorAction SilentlyContinue))
{
    try
    {
        foreach ($module in $process.Modules)
        {
            if ($module.ModuleName -ieq 'ExtOnly.dll') { $wasLoaded = $true }
        }
    }
    catch { }
}

if ($wasLoaded)
{
    Write-Host 'エクスプローラーが ExtOnly.dll を使用中のため、再起動してからビルドします...'
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

Write-Host 'ビルド中...'
& cmd /c $batchPath
$buildExit = $LASTEXITCODE

if ($wasLoaded -and -not (Get-Process explorer -ErrorAction SilentlyContinue))
{
    Start-Process explorer.exe
}

if ($buildExit -ne 0) { throw "ビルドに失敗しました (exit $buildExit)" }

$dllPath = Join-Path $outDir 'ExtOnly.dll'
if (-not (Test-Path -LiteralPath $dllPath)) { throw "DLL が生成されませんでした: $dllPath" }

Write-Host "ビルド成功: $dllPath"
