#include "SlxComOverlay.h"

CSlxComOverlay::CSlxComOverlay()
{
    m_dwRefCount = 0;
}

STDMETHODIMP CSlxComOverlay::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IShellIconOverlayIdentifier)
    {
        *ppv = static_cast<IShellIconOverlayIdentifier *>(this);
    }
    else if(riid == IID_IUnknown)
    {
        *ppv = static_cast<IShellIconOverlayIdentifier *>(this);
    }
    else
    {
        *ppv = 0;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown *>(*ppv)->AddRef();

    return S_OK;
}

STDMETHODIMP_(ULONG) CSlxComOverlay::AddRef()
{
    return InterlockedIncrement((volatile LONG *)&m_dwRefCount);
}

STDMETHODIMP_(ULONG) CSlxComOverlay::Release()
{
    DWORD dwRefCount = InterlockedDecrement((volatile LONG *)&m_dwRefCount);

    if(dwRefCount == 0)
    {
        delete this;
    }

    return dwRefCount;
}

//IShellIconOverlayIdentifier Method
STDMETHODIMP CSlxComOverlay::IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib)
{
    return S_FALSE;
}

STDMETHODIMP CSlxComOverlay::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags)
{
    return S_OK;
}

STDMETHODIMP CSlxComOverlay::GetPriority(int *pPriority)
{
    return S_OK;
}