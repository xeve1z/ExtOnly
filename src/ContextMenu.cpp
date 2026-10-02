#include "ContextMenu.h"

#include <shobjidl.h>
#include <shlguid.h>
#include <exdisp.h>
#include <propkey.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <new>
#include <utility>

extern LONG g_objectCount;

namespace
{
#ifdef EXTONLY_TRACE

void AppendLog(std::wstring& log, const wchar_t* format, ...)
{
    wchar_t buffer[512];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);

    log += buffer;
    log += L"\r\n";
}

std::wstring WriteLogFile(const std::wstring& text)
{
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
        return std::wstring();

    std::wstring path = localAppData;
    CoTaskMemFree(localAppData);
    path += L"\\ExtOnly";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"\\extonly.log";

    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size = {};
        GetFileSizeEx(file, &size);

        DWORD written = 0;
        if (size.QuadPart == 0)
        {
            const unsigned char bom[2] = { 0xFF, 0xFE };
            WriteFile(file, bom, sizeof(bom), &written, nullptr);
        }

        WriteFile(file, text.c_str(), static_cast<DWORD>(text.size() * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(file);
    }

    return path;
}

std::wstring LogHeader(const wchar_t* category)
{
    SYSTEMTIME st = {};
    GetLocalTime(&st);

    std::wstring entry;
    AppendLog(entry, L"==== %04u-%02u-%02u %02u:%02u:%02u  category=%s ====",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, category);
    return entry;
}

#else

void AppendLog(std::wstring&, const wchar_t*, ...) {}
std::wstring WriteLogFile(const std::wstring&) { return std::wstring(); }
std::wstring LogHeader(const wchar_t*) { return std::wstring(); }

#endif

std::wstring ExtensionFromParsingName(LPCWSTR parsingName)
{
    LPCWSTR ext = PathFindExtensionW(parsingName);
    if (!ext || ext[0] != L'.' || ext[1] == L'\0')
        return std::wstring();

    std::wstring value(ext + 1);
    CharLowerBuffW(value.data(), static_cast<DWORD>(value.size()));
    return value;
}

PITEMID_CHILD CloneChildTerminated(PCUITEMID_CHILD child)
{
    const USHORT size = child->mkid.cb;
    auto* copy = static_cast<PITEMID_CHILD>(CoTaskMemAlloc(size + sizeof(USHORT)));
    if (!copy)
        return nullptr;

    memcpy(copy, child, size);
    *reinterpret_cast<USHORT*>(reinterpret_cast<BYTE*>(copy) + size) = 0;
    return copy;
}

bool IsFolderPidl(IShellFolder* psf, PCUITEMID_CHILD pidlChild)
{
    SFGAOF attributes = SFGAO_FOLDER | SFGAO_STREAM;
    if (FAILED(psf->GetAttributesOf(1, &pidlChild, &attributes)))
        return false;

    return (attributes & SFGAO_FOLDER) != 0 && (attributes & SFGAO_STREAM) == 0;
}

bool IsFolderItem(IShellItem* item)
{
    SFGAOF attributes = 0;
    if (FAILED(item->GetAttributes(SFGAO_FOLDER | SFGAO_STREAM, &attributes)))
        return false;

    return (attributes & SFGAO_FOLDER) != 0 && (attributes & SFGAO_STREAM) == 0;
}

void CollectCategoriesFromHDrop(IDataObject* pdtobj, std::vector<std::wstring>* categories, std::wstring& log)
{
    FORMATETC fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stm = {};
    const HRESULT hrGet = pdtobj->GetData(&fe, &stm);
    AppendLog(log, L"GetData(HDROP) hr=0x%08X", static_cast<unsigned>(hrGet));
    if (FAILED(hrGet))
        return;

    auto drop = static_cast<HDROP>(stm.hGlobal);
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    AppendLog(log, L"HDROP count=%u", count);

    for (UINT i = 0; i < count; ++i)
    {
        wchar_t path[MAX_PATH] = {};
        if (DragQueryFileW(drop, i, path, _countof(path)) == 0)
            continue;

        const DWORD attributes = GetFileAttributesW(path);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            categories->push_back(EXTONLY_FOLDER_CATEGORY);
            if (i < 30)
                AppendLog(log, L"[%u] folder %s", i, path);
            continue;
        }

        const std::wstring extension = ExtensionFromParsingName(path);
        if (!extension.empty())
            categories->push_back(extension);

        if (i < 30)
            AppendLog(log, L"[%u] file %s ext=%s", i, path, extension.c_str());
    }

    ReleaseStgMedium(&stm);
}

void CollectCategories(IDataObject* pdtobj, std::vector<std::wstring>* categories, std::wstring& log)
{
    const CLIPFORMAT cfShellIdList = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_SHELLIDLIST));
    FORMATETC fe = { cfShellIdList, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stm = {};
    const HRESULT hrGet = pdtobj->GetData(&fe, &stm);
    AppendLog(log, L"GetData(SHELLIDLIST) hr=0x%08X", static_cast<unsigned>(hrGet));
    if (FAILED(hrGet))
    {
        CollectCategoriesFromHDrop(pdtobj, categories, log);
        return;
    }

    auto* pida = static_cast<CIDA*>(GlobalLock(stm.hGlobal));
    if (!pida)
    {
        AppendLog(log, L"GlobalLock 失敗");
        ReleaseStgMedium(&stm);
        return;
    }

    AppendLog(log, L"cidl=%u", pida->cidl);

    if (pida->cidl >= 2 && categories)
    {
        PCIDLIST_ABSOLUTE pidlParent =
            reinterpret_cast<PCIDLIST_ABSOLUTE>(reinterpret_cast<BYTE*>(pida) + pida->aoffset[0]);

        IShellFolder* psf = nullptr;
        const HRESULT hrBind = SHBindToObject(nullptr, pidlParent, nullptr, IID_PPV_ARGS(&psf));
        AppendLog(log, L"SHBindToObject hr=0x%08X", static_cast<unsigned>(hrBind));

        if (SUCCEEDED(hrBind))
        {
            UINT logged = 0;

            for (UINT i = 0; i < pida->cidl; ++i)
            {
                PCUITEMID_CHILD rawChild =
                    reinterpret_cast<PCUITEMID_CHILD>(reinterpret_cast<BYTE*>(pida) + pida->aoffset[i + 1]);

                PITEMID_CHILD child = CloneChildTerminated(rawChild);
                if (!child)
                {
                    if (logged < 30)
                        AppendLog(log, L"[%u] clone 失敗", i);
                    logged++;
                    continue;
                }

                if (IsFolderPidl(psf, child))
                {
                    categories->push_back(EXTONLY_FOLDER_CATEGORY);
                    if (logged < 30)
                        AppendLog(log, L"[%u] folder", i);
                }
                else
                {
                    STRRET sr = {};
                    if (FAILED(psf->GetDisplayNameOf(child, SHGDN_FORPARSING, &sr)))
                    {
                        if (logged < 30)
                            AppendLog(log, L"[%u] 名前取得失敗", i);
                    }
                    else
                    {
                        PWSTR psz = nullptr;
                        if (SUCCEEDED(StrRetToStrW(&sr, child, &psz)) && psz)
                        {
                            const std::wstring extension = ExtensionFromParsingName(psz);
                            if (!extension.empty())
                                categories->push_back(extension);

                            if (logged < 30)
                                AppendLog(log, L"[%u] name=%s ext=%s", i, psz, extension.c_str());

                            CoTaskMemFree(psz);
                        }
                    }
                }

                CoTaskMemFree(child);
                logged++;
            }

            psf->Release();
        }
    }

    GlobalUnlock(stm.hGlobal);
    ReleaseStgMedium(&stm);
}

std::wstring ItemExtension(IShellItem* item)
{
    IShellItem2* item2 = nullptr;
    if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&item2))))
    {
        PWSTR value = nullptr;
        std::wstring result;
        if (SUCCEEDED(item2->GetString(PKEY_FileExtension, &value)) && value)
        {
            result = value;
            CoTaskMemFree(value);
        }
        item2->Release();

        if (!result.empty())
        {
            if (result[0] == L'.')
                result.erase(0, 1);

            CharLowerBuffW(result.data(), static_cast<DWORD>(result.size()));
            return result;
        }
    }

    PIDLIST_ABSOLUTE pidl = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetIDListFromObject(item, &pidl)) && pidl)
    {
        PWSTR name = nullptr;
        if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_DESKTOPABSOLUTEPARSING, &name)) && name)
        {
            result = ExtensionFromParsingName(name);
            CoTaskMemFree(name);
        }

        CoTaskMemFree(pidl);
    }

    return result;
}

bool IsDesktopWindowClass(const wchar_t* className)
{
    return _wcsicmp(className, L"Progman") == 0 || _wcsicmp(className, L"WorkerW") == 0;
}

IFolderView* GetFolderViewFromWindow(HWND hwnd, std::wstring& log)
{
    if (!hwnd)
    {
        AppendLog(log, L"cmd hwnd = NULL");
        return nullptr;
    }

    HWND top = GetAncestor(hwnd, GA_ROOT);
    if (!top)
        top = hwnd;

    wchar_t cmdClass[64] = {};
    wchar_t topClass[64] = {};
    GetClassNameW(hwnd, cmdClass, _countof(cmdClass));
    GetClassNameW(top, topClass, _countof(topClass));
    AppendLog(log, L"cmd hwnd = %p [%s]", static_cast<void*>(hwnd), cmdClass);
    AppendLog(log, L"cmd top  = %p [%s]", static_cast<void*>(top), topClass);

    IShellWindows* shellWindows = nullptr;
    const HRESULT hrCreate =
        CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&shellWindows));
    AppendLog(log, L"ShellWindows hr = 0x%08X", static_cast<unsigned>(hrCreate));
    if (FAILED(hrCreate) || !shellWindows)
        return nullptr;

    long count = 0;
    const HRESULT hrCount = shellWindows->get_Count(&count);
    AppendLog(log, L"shell window count = %ld (hr=0x%08X)", count, static_cast<unsigned>(hrCount));

    IFolderView* matched = nullptr;
    IFolderView* candidate = nullptr;
    int candidateCount = 0;

    for (long i = 0; i < count && !matched; ++i)
    {
        VARIANT index;
        VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;

        IDispatch* dispatch = nullptr;
        if (FAILED(shellWindows->Item(index, &dispatch)) || !dispatch)
        {
            AppendLog(log, L"[%ld] Item 取得失敗", i);
            continue;
        }

        IShellBrowser* browser = nullptr;
        IServiceProvider* provider = nullptr;
        if (SUCCEEDED(dispatch->QueryInterface(IID_PPV_ARGS(&provider))))
        {
            provider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
            provider->Release();
        }

        if (!browser)
        {
            AppendLog(log, L"[%ld] IShellBrowser なし", i);
            dispatch->Release();
            continue;
        }

        IShellView* view = nullptr;
        if (SUCCEEDED(browser->QueryActiveShellView(&view)) && view)
        {
            HWND viewWindow = nullptr;
            view->GetWindow(&viewWindow);
            HWND viewRoot = viewWindow ? GetAncestor(viewWindow, GA_ROOT) : nullptr;

            wchar_t viewRootClass[64] = {};
            if (viewRoot)
                GetClassNameW(viewRoot, viewRootClass, _countof(viewRootClass));

            IFolderView* folderView = nullptr;
            const HRESULT hrFolderView = view->QueryInterface(IID_PPV_ARGS(&folderView));
            AppendLog(log, L"[%ld] view=%p root=%p [%s] fv=0x%08X",
                      i,
                      static_cast<void*>(viewWindow),
                      static_cast<void*>(viewRoot), viewRootClass,
                      static_cast<unsigned>(hrFolderView));

            if (folderView)
            {
                if (viewRoot && viewRoot == top)
                {
                    matched = folderView;
                }
                else if (!IsDesktopWindowClass(viewRootClass))
                {
                    candidateCount++;
                    if (!candidate)
                        candidate = folderView;
                    else
                        folderView->Release();
                }
                else
                {
                    folderView->Release();
                }
            }

            view->Release();
        }
        else
        {
            AppendLog(log, L"[%ld] ShellView なし", i);
        }

        browser->Release();
        dispatch->Release();
    }

    shellWindows->Release();

    if (!matched && candidate && candidateCount == 1)
    {
        AppendLog(log, L"フォールバック: ビューが 1 つだけなので採用");
        matched = candidate;
        candidate = nullptr;
    }

    if (candidate)
        candidate->Release();

    AppendLog(log, L"view result: %s", matched ? L"OK" : L"NG");
    return matched;
}

PIDLIST_ABSOLUTE GetViewFolderPidl(IFolderView* view)
{
    IShellFolder* folder = nullptr;
    if (FAILED(view->GetFolder(IID_PPV_ARGS(&folder))) || !folder)
        return nullptr;

    PIDLIST_ABSOLUTE pidl = nullptr;
    IPersistFolder2* persist = nullptr;
    if (SUCCEEDED(folder->QueryInterface(IID_PPV_ARGS(&persist))))
    {
        persist->GetCurFolder(&pidl);
        persist->Release();
    }

    folder->Release();
    return pidl;
}

IFolderView* FindFolderViewForFolder(PCIDLIST_ABSOLUTE folderPidl, std::wstring& log)
{
    IShellWindows* shellWindows = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&shellWindows))))
        return nullptr;

    long count = 0;
    shellWindows->get_Count(&count);

    IFolderView* result = nullptr;

    for (long i = 0; i < count && !result; ++i)
    {
        VARIANT index;
        VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;

        IDispatch* dispatch = nullptr;
        if (FAILED(shellWindows->Item(index, &dispatch)) || !dispatch)
            continue;

        IShellBrowser* browser = nullptr;
        IServiceProvider* provider = nullptr;
        if (SUCCEEDED(dispatch->QueryInterface(IID_PPV_ARGS(&provider))))
        {
            provider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
            provider->Release();
        }

        if (browser)
        {
            IShellView* view = nullptr;
            if (SUCCEEDED(browser->QueryActiveShellView(&view)) && view)
            {
                IFolderView* folderView = nullptr;
                if (SUCCEEDED(view->QueryInterface(IID_PPV_ARGS(&folderView))) && folderView)
                {
                    PIDLIST_ABSOLUTE viewFolder = GetViewFolderPidl(folderView);
                    const bool same = viewFolder && ILIsEqual(viewFolder, folderPidl);
                    if (viewFolder)
                        CoTaskMemFree(viewFolder);

                    if (same)
                        result = folderView;
                    else
                        folderView->Release();
                }

                view->Release();
            }

            browser->Release();
        }

        dispatch->Release();
    }

    shellWindows->Release();

    if (!result)
        AppendLog(log, L"選択フォルダーのビューが見つからない");

    return result;
}

void CollectCategoriesFromView(IFolderView* view, std::vector<std::wstring>* categories)
{
    IShellItemArray* selection = nullptr;
    if (FAILED(view->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selection))) || !selection)
        return;

    DWORD count = 0;
    selection->GetCount(&count);

    const DWORD limit = 2000;

    for (DWORD i = 0; i < count && i < limit; ++i)
    {
        IShellItem* item = nullptr;
        if (FAILED(selection->GetItemAt(i, &item)) || !item)
            continue;

        if (IsFolderItem(item))
            categories->push_back(EXTONLY_FOLDER_CATEGORY);
        else
        {
            const std::wstring extension = ItemExtension(item);
            if (!extension.empty())
                categories->push_back(extension);
        }

        item->Release();
    }

    selection->Release();
}
}

CExtOnlyContextMenu::CExtOnlyContextMenu() : m_ref(1)
{
    InterlockedIncrement(&g_objectCount);
}

CExtOnlyContextMenu::~CExtOnlyContextMenu()
{
    if (m_folderPidl)
        CoTaskMemFree(m_folderPidl);

    InterlockedDecrement(&g_objectCount);
}

IFACEMETHODIMP CExtOnlyContextMenu::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == IID_IShellExtInit)
        *ppv = static_cast<IShellExtInit*>(this);
    else if (riid == IID_IContextMenu)
        *ppv = static_cast<IContextMenu*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) CExtOnlyContextMenu::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

IFACEMETHODIMP_(ULONG) CExtOnlyContextMenu::Release()
{
    const LONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0)
        delete this;

    return static_cast<ULONG>(ref);
}

IFACEMETHODIMP CExtOnlyContextMenu::Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY)
{
    if (m_folderPidl)
    {
        CoTaskMemFree(m_folderPidl);
        m_folderPidl = nullptr;
    }

    if (pidlFolder)
        m_folderPidl = ILClone(pidlFolder);

    m_categories.clear();

    WriteLogFile(LogHeader(L"(menu)") + std::wstring(L"Initialize called\r\n"));

    if (!pdtobj)
        return E_INVALIDARG;

    const DWORD startTick = GetTickCount();

    std::wstring log;
    CollectCategories(pdtobj, &m_categories, log);

    if (m_categories.size() > 1)
    {
        std::sort(m_categories.begin(), m_categories.end());
        m_categories.erase(std::unique(m_categories.begin(), m_categories.end()), m_categories.end());
    }

    AppendLog(log, L"menu categories = %u (elapsed=%u ms)",
              static_cast<unsigned>(m_categories.size()),
              static_cast<unsigned>(GetTickCount() - startTick));
    for (const auto& category : m_categories)
        AppendLog(log, L"  - %s", category.c_str());

    WriteLogFile(LogHeader(L"(menu)") + log);

    return S_OK;
}

IFACEMETHODIMP CExtOnlyContextMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    const HRESULT noItems = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);

    if (uFlags & (CMF_DEFAULTONLY | CMF_VERBSONLY))
        return noItems;

    RefreshCategoriesFromView();

    if (m_categories.size() < 2)
        return noItems;

    UINT count = static_cast<UINT>(m_categories.size());
    const UINT available = idCmdLast - idCmdFirst + 1;
    if (count > available)
        count = available;

    HMENU submenu = CreatePopupMenu();
    if (!submenu)
        return E_OUTOFMEMORY;

    for (UINT i = 0; i < count; ++i)
    {
        InsertMenuW(submenu, i, MF_BYPOSITION | MF_STRING, idCmdFirst + i, m_categories[i].c_str());
    }

    if (!InsertMenuW(hmenu, indexMenu, MF_BYPOSITION | MF_POPUP | MF_STRING,
                     reinterpret_cast<UINT_PTR>(submenu), EXTONLY_MENU_TEXT))
    {
        DestroyMenu(submenu);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, count);
}

IFACEMETHODIMP CExtOnlyContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    if (!pici)
        return E_INVALIDARG;

    if (!IS_INTRESOURCE(pici->lpVerb))
        return E_INVALIDARG;

    const UINT index = LOWORD(reinterpret_cast<UINT_PTR>(pici->lpVerb));
    if (index >= m_categories.size())
        return E_INVALIDARG;

    ApplyCategoryFilter(m_categories[index], pici->hwnd);
    return S_OK;
}

IFACEMETHODIMP CExtOnlyContextMenu::GetCommandString(UINT_PTR, UINT, UINT*, CHAR*, UINT)
{
    return E_NOTIMPL;
}

void CExtOnlyContextMenu::RefreshCategoriesFromView()
{
    std::wstring log;

    IFolderView* view = nullptr;

    HWND hwnd = GetActiveWindow();
    if (!hwnd)
        hwnd = GetForegroundWindow();

    AppendLog(log, L"Refresh: hwnd=%p", static_cast<void*>(hwnd));

    if (hwnd)
        view = GetFolderViewFromWindow(hwnd, log);

    if (!view && m_folderPidl)
        view = FindFolderViewForFolder(m_folderPidl, log);

    if (!view)
    {
        AppendLog(log, L"Refresh: ビュー取得失敗（データオブジェクトの種類を使います）");
        WriteLogFile(LogHeader(L"(menu)") + log);
        return;
    }

    std::vector<std::wstring> categories;
    CollectCategoriesFromView(view, &categories);
    view->Release();

    AppendLog(log, L"Refresh: view categories = %u", static_cast<unsigned>(categories.size()));

    if (categories.empty())
    {
        AppendLog(log, L"Refresh: ビューの選択が空");
        WriteLogFile(LogHeader(L"(menu)") + log);
        return;
    }

    std::sort(categories.begin(), categories.end());
    categories.erase(std::unique(categories.begin(), categories.end()), categories.end());

    m_categories = std::move(categories);

    for (const auto& category : m_categories)
        AppendLog(log, L"  - %s", category.c_str());

    WriteLogFile(LogHeader(L"(menu)") + log);
}

void CExtOnlyContextMenu::ApplyCategoryFilter(const std::wstring& category, HWND hwnd)
{
    std::wstring log;

    IFolderView* folderView = GetFolderViewFromWindow(hwnd, log);
    if (!folderView)
    {
#ifdef EXTONLY_TRACE
        const std::wstring logPath = WriteLogFile(LogHeader(category.c_str()) + log);

        std::wstring message = L"この画面では選択を変更できませんでした。\n\n【原因の手がかり】\n" + log;
        if (!logPath.empty())
            message += L"\nログ: " + logPath;
        if (message.size() > 1500)
            message = message.substr(0, 1500) + L"\n...(省略)";

        MessageBoxW(hwnd, message.c_str(), L"ExtOnly", MB_OK | MB_ICONWARNING);
#else
        MessageBoxW(hwnd, L"この画面では選択を変更できませんでした。", L"ExtOnly", MB_OK | MB_ICONWARNING);
#endif
        return;
    }

    IShellItemArray* selection = nullptr;
    HRESULT hr = folderView->Items(SVGIO_SELECTION, IID_PPV_ARGS(&selection));
    if (FAILED(hr) || !selection)
    {
        AppendLog(log, L"Items(SELECTION) hr=0x%08X", static_cast<unsigned>(hr));
        folderView->Release();
        WriteLogFile(LogHeader(category.c_str()) + log);
        return;
    }

    DWORD selectedCount = 0;
    selection->GetCount(&selectedCount);

    const bool wantFolder = (category == EXTONLY_FOLDER_CATEGORY);

    std::vector<IShellItem*> kept;
    for (DWORD i = 0; i < selectedCount; ++i)
    {
        IShellItem* item = nullptr;
        if (FAILED(selection->GetItemAt(i, &item)) || !item)
            continue;

        const bool isFolder = IsFolderItem(item);
        const bool keep = wantFolder ? isFolder : (!isFolder && ItemExtension(item) == category);

        if (keep)
            kept.push_back(item);
        else
            item->Release();
    }
    selection->Release();

    std::vector<PIDLIST_ABSOLUTE> absolutePidls;
    std::vector<PCUITEMID_CHILD> childPidls;

    for (IShellItem* item : kept)
    {
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHGetIDListFromObject(item, &pidl)) && pidl)
        {
            absolutePidls.push_back(pidl);
            childPidls.push_back(ILFindLastID(pidl));
        }

        item->Release();
    }

    AppendLog(log, L"selected=%lu kept=%u", selectedCount, static_cast<unsigned>(childPidls.size()));

    if (!childPidls.empty())
    {
        for (size_t i = 0; i < childPidls.size(); ++i)
        {
            DWORD flags = SVSI_SELECT | SVSI_ENSUREVISIBLE;
            if (i == 0)
                flags |= SVSI_DESELECTOTHERS;

            hr = folderView->SelectAndPositionItems(1, &childPidls[i], nullptr, flags);
        }

        AppendLog(log, L"SelectAndPositionItems hr=0x%08X", static_cast<unsigned>(hr));
    }

    for (PIDLIST_ABSOLUTE pidl : absolutePidls)
        CoTaskMemFree(pidl);

    folderView->Release();

    WriteLogFile(LogHeader(category.c_str()) + log);
}
