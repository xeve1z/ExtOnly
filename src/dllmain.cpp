#include "ContextMenu.h"

#include <new>

HINSTANCE g_hInst = nullptr;
LONG g_objectCount = 0;
LONG g_serverLocks = 0;

namespace
{
class CClassFactory final : public IClassFactory
{
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return 2;
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        return 1;
    }

    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;

        CExtOnlyContextMenu* object = new (std::nothrow) CExtOnlyContextMenu();
        if (!object)
            return E_OUTOFMEMORY;

        const HRESULT hr = object->QueryInterface(riid, ppv);
        object->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL lock) override
    {
        if (lock)
            InterlockedIncrement(&g_serverLocks);
        else
            InterlockedDecrement(&g_serverLocks);

        return S_OK;
    }
};
}

extern "C"
{
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (rclsid != CLSID_ExtOnlyContextMenu)
        return CLASS_E_CLASSNOTAVAILABLE;

    static CClassFactory factory;
    return factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return (g_objectCount == 0 && g_serverLocks == 0) ? S_OK : S_FALSE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }

    return TRUE;
}
}
