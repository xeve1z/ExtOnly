#pragma once

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <string>
#include <vector>

// ExtOnly 右クリックメニュー拡張の CLSID。
// register.ps1 / unregister.ps1 と値を揃えること。
static const GUID CLSID_ExtOnlyContextMenu =
{ 0xB06D4875, 0x833C, 0x4F8E, { 0x85, 0xA7, 0x88, 0x11, 0x74, 0x83, 0x82, 0xEE } };

#define EXTONLY_MENU_TEXT L"選択"

// フォルダーを表すメニュー項目名
#define EXTONLY_FOLDER_CATEGORY L"folder"
