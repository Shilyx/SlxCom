#include "SlxComOverlay.h"

CSlxComOverlay::CSlxComOverlay()
{
    m_dwRefCount = 0;
}

STDMETHODIMP CSlxComOverlay::QueryInterface(REFIID riid, void **ppv)
{
    *ppv = NULL;

    if(riid == IID_IUnknown || riid == IID_IShellIconOverlayIdentifier)
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