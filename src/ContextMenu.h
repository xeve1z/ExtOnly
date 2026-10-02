#pragma once

#include "ExtOnly.h"

class CExtOnlyContextMenu final : public IContextMenu, public IShellExtInit
{
public:
    CExtOnlyContextMenu();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) override;

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
