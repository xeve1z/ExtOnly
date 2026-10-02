$ErrorActionPreference = 'Stop'

$clsid = '{B06D4875-833C-4F8E-85A7-8811748382EE}'
$classes = 'HKCU:\Software\Classes'

Remove-Item -LiteralPath "$classes\*\shellex\ContextMenuHandlers\ExtOnly" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue

foreach ($target in @('*', 'Directory', 'Folder')) {
    Remove-Item -LiteralPath "$classes\$target\shellex\ContextMenuHandlers\ExtOnly" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath "$classes\$target\shellex\ContextMenuHandlers" -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath "$classes\$target\shellex" -Force -ErrorAction SilentlyContinue
}

Write-Host '登録を解除しました。エクスプローラーを再起動すると完全に反映されます。'
