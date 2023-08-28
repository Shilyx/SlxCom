#include "SlxComIconHandler.h"
#include <Shlwapi.h>
#include "SlxComTools.h"

static LPCWSTR GetDefaultDllFileIconPathIndex(int *pIndex)
{
    static WCHAR s_szFilePath[MAX_PATH] = L"";
    static int s_nIndex = -1;

    if (s_szFilePath[0])
    {
        if (pIndex)
        {
            *pIndex = s_nIndex;
        }

        return s_szFilePath;
    }

    WCHAR szData[1024] = L"";
    WCHAR szExpandedData[1024] = L"";
    DWORD dwRegType = REG_NONE;
    DWORD dwSize = sizeof(szData);

    SHGetValueW(HKEY_CLASSES_ROOT, L"dllfile\\DefaultIcon", szData, &dwRegType, szData, &dwSize);
    ExpandEnvironmentStringsW(szData, szExpandedData, RTL_NUMBER_OF(szExpandedData));

    BOOL bSigned = FALSE;
    LPWSTR lpDot = StrRChrW(szExpandedData, NULL, L',');

    if (lpDot)
    {
        *lpDot = 0;
        ++lpDot;
    }

    if (!PathFileExistsW(szExpandedData))
    {
        return NULL;
    }

    if (lpDot)
    {
        if (*lpDot == L'-')
        {
            ++lpDot;
            bSigned = TRUE;
        }

        s_nIndex = StrToIntW(lpDot);

        if (bSigned)
        {
            s_nIndex *= -1;
        }
    }

    lstrcpynW(s_szFilePath, szExpandedData, RTL_NUMBER_OF(s_szFilePath));

    return GetDefaultDllFileIconPathIndex(pIndex);
}

STDMETHODIMP CSlxComIconHandler::QueryInterface(REFIID riid, void **ppv)
{
    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IExtractIconW *>(this);
    }
    else if(riid == IID_IExtractIconW)
    {
        *ppv = static_cast<IExtractIconW *>(this);
    }
    else if(riid == IID_IPersistFile)
    {
        *ppv = static_cast<IPersistFile *>(this);
    }
    else if(riid == IID_IPersist)
    {
        *ppv = static_cast<IPersist *>(this);
    }
    else
    {
        *ppv = 0;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown *>(*ppv)->AddRef();

    return S_OK;
}

STDMETHODIMP_(ULONG) CSlxComIconHandler::AddRef()
{
    return InterlockedIncrement((volatile LONG *)&m_nRefCount);
}

STDMETHODIMP_(ULONG) CSlxComIconHandler::Release()
{
    LONG nRefCount = InterlockedDecrement(&m_nRefCount);

    if (nRefCount == 0)
    {
        delete this;
    }

    return nRefCount;
}

STDMETHODIMP CSlxComIconHandler::GetIconLocation(UINT uFlags, LPWSTR pszIconFile, UINT cchMax, int *piIndex, UINT *pwFlags)
{
    extern WCHAR g_szSlxComDllFullPath[];

    *pwFlags = 0;

    if (lstrcmpiW(m_szFilePath, g_szSlxComDllFullPath) == 0)
    {
        lstrcpynW(pszIconFile, g_szSlxComDllFullPath, cchMax);
        *piIndex = 1;
        return S_OK;
    }

    if (lstrcmpiW(PathFindExtensionW(m_szFilePath), L".dll") == 0)
    {
        LPCWSTR lpDefaultFilePath = GetDefaultDllFileIconPathIndex(piIndex);

        if (lpDefaultFilePath)
        {
            lstrcpynW(pszIconFile, lpDefaultFilePath, cchMax);
            return S_OK;
        }
    }

    return S_FALSE;
}

STDMETHODIMP CSlxComIconHandler::Extract(LPCWSTR pszFile, UINT nIconIndex, HICON *phiconLarge, HICON *phiconSmall, UINT nIconSize)
{
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CSlxComIconHandler::Load(LPCOLESTR pszFileName, DWORD dwMode)
{
    lstrcpynW(m_szFilePath, pszFileName, RTL_NUMBER_OF(m_szFilePath));
    return S_OK;
}
