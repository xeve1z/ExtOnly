param(
    [switch]$RestartExplorer
)

$ErrorActionPreference = 'Stop'

$clsid = '{B06D4875-833C-4F8E-85A7-8811748382EE}'
$dll = Join-Path $PSScriptRoot 'build\ExtOnly.dll'

if (-not (Test-Path -LiteralPath $dll)) {
    throw "DLL が見つかりません。先に build.ps1 を実行してください: $dll"
}

$classes = 'HKCU:\Software\Classes'

$handlerKey = "$classes\*\shellex\ContextMenuHandlers\ExtOnly"
New-Item -Path $handlerKey -Force | Out-Null
Set-ItemProperty -LiteralPath $handlerKey -Name '(Default)' -Value $clsid

$clsidKey = "$classes\CLSID\$clsid"
New-Item -Path $clsidKey -Force | Out-Null
Set-ItemProperty -LiteralPath $clsidKey -Name '(Default)' -Value 'ExtOnly'

$inprocKey = "$clsidKey\InprocServer32"
New-Item -Path $inprocKey -Force | Out-Null
Set-ItemProperty -LiteralPath $inprocKey -Name '(Default)' -Value $dll
Set-ItemProperty -LiteralPath $inprocKey -Name 'ThreadingModel' -Value 'Apartment'

Write-Host "登録しました (CLSID: $clsid)"
Write-Host "DLL    : $dll"

if ($RestartExplorer) {
    Write-Host 'エクスプローラーを再起動しています...'
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Start-Process explorer.exe
    Write-Host '再起動しました。'
} else {
    Write-Host 'メニューに反映されない場合は、-RestartExplorer を付けて再実行してください。'
}
