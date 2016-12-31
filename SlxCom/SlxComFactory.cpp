#include "SlxComFactory.h"
#include "SlxComOverlay.h"
#include "SlxComContextMenu.h"
#include "SlxComIconHandler.h"

CSlxComFactory::CSlxComFactory()
{
    m_dwRefCount = 0;
}

STDMETHODIMP CSlxComFactory::QueryInterface(REFIID riid, void **ppv)
{
    *ppv = NULL;

    if(riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *ppv = this;
        AddRef();

        return S_OK;
    }
    else
    {
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CSlxComFactory::AddRef()
{
    return InterlockedIncrement((volatile LONG *)&m_dwRefCount);
}

STDMETHODIMP_(ULONG) CSlxComFactory::Release()
{
    DWORD dwRefCount = InterlockedDecrement((volatile LONG *)&m_dwRefCount);

    if(dwRefCount == 0)
    {
        delete this;
    }

    return dwRefCount;
}

//IClassFactory Method
STDMETHODIMP CSlxComFactory::CreateInstance(IUnknown * pUnkOuter, REFIID riid, void **ppv)
{
    *ppv=NULL;

    if(pUnkOuter != NULL)
    {
        return CLASS_E_NOAGGREGATION;
    }

    try
    {
        if (riid == IID_IShellIconOverlayIdentifier)
        {
            CSlxComOverlay *pOverlay = new CSlxComOverlay;
            HRESULT hr = pOverlay->QueryInterface(riid, ppv);

            if (FAILED(hr))
            {
                delete pOverlay;
            }

            return hr;
        }
        else if (riid == IID_IShellExtInit || riid == IID_IContextMenu || riid == IID_IShellPropSheetExt)
        {
            CSlxComContextMenu *pContextMenu = new CSlxComContextMenu;
            HRESULT hr = pContextMenu->QueryInterface(riid, ppv);

            if (FAILED(hr))
            {
                delete pContextMenu;
            }

            return hr;
        }
        else if (riid == IID_IPersistFile || riid == IID_IExtractIconW)
        {
            CSlxComIconHandler *pIconHandler = new CSlxComIconHandler;
            HRESULT hr = pIconHandler->QueryInterface(riid, ppv);

            if (FAILED(hr))
            {
                delete pIconHandler;
            }

            return hr;
        }
    }
    catch (...)
    {
        return E_OUTOFMEMORY;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP CSlxComFactory::LockServer(BOOL fLock)
{
    return E_NOTIMPL;
}