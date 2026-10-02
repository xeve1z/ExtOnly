#pragma once

#include "ExtOnly.h"

// エクスプローラーの右クリックメニューに「選択 ▸ 拡張子 / folder」を追加する。
// 異なる種類（拡張子またはフォルダー）が 2 つ以上あるときだけメニューを出す。
class CExtOnlyContextMenu final : public IContextMenu, public IShellExtInit
{
public:
    CExtOnlyContextMenu();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) override;

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags) override;
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici) override;
    IFACEMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pReserved, CHAR* pszName, UINT cchMax) override;

private:
    ~CExtOnlyContextMenu();

    void ApplyCategoryFilter(const std::wstring& category, HWND hwnd);
    void RefreshCategoriesFromView();

    LONG m_ref;
    PIDLIST_ABSOLUTE m_folderPidl = nullptr;
    std::vector<std::wstring> m_categories;
};
