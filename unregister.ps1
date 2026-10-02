$ErrorActionPreference = 'Stop'

# src/ExtOnly.h の CLSID_ExtOnlyContextMenu と同じ値にすること
$clsid = '{B06D4875-833C-4F8E-85A7-8811748382EE}'
$classes = 'HKCU:\Software\Classes'

Remove-Item -LiteralPath "$classes\*\shellex\ContextMenuHandlers\ExtOnly" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue

# 空になった親キーを掃除する（他社の拡張が入っていれば残る）
Remove-Item -LiteralPath "$classes\*\shellex\ContextMenuHandlers" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$classes\*\shellex" -Force -ErrorAction SilentlyContinue

Write-Host '登録を解除しました。エクスプローラーを再起動すると完全に反映されます。'
