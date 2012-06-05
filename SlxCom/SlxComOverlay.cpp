#include "SlxComOverlay.h"
#include "resource.h"
#include "SlxComTools.h"
#include <shlwapi.h>

extern HINSTANCE g_hinstDll;
TCHAR CSlxComOverlay::m_szIconFilePath[] = TEXT("");
DWORD CSlxComOverlay::m_dwIcoFileSize = 0;

SlxStringStatusCache CSlxComOverlay::m_cache(TEXT("Software\\Slx\\StringStatusCache"), IsExplorer());

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
    WIN32_FILE_ATTRIBUTE_DATA wfad;

    if(GetFileAttributesEx(pwszPath, GetFileExInfoStandard, &wfad))
    {
        if((wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            TCHAR szString[MAX_PATH + 100];
            ULARGE_INTEGER uliSize = {0};
            StringStatus ssValue;

            uliSize.HighPart = wfad.nFileSizeHigh;
            uliSize.LowPart = wfad.nFileSizeLow;

            wnsprintf(szString, sizeof(szString) / sizeof(TCHAR), TEXT("%s[%I64u]%I64u,%I64u"),
                pwszPath,
                uliSize.QuadPart,
                *(unsigned __int64 *)&wfad.ftCreationTime,
                *(unsigned __int64 *)&wfad.ftLastWriteTime);

            if(m_cache.CheckCache(szString, &ssValue))
            {
                if(ssValue == SS_1)
                {
                    return S_OK;
                }
                else if(ssValue == SS_2)
                {
                    return S_FALSE;
                }
                else
                {

                }
            }

            if(IsFileSigned(pwszPath))
            {
                m_cache.AddCache(szString, SS_1);

                return S_OK;
            }
            else
            {
                m_cache.AddCache(szString, SS_2);

                return S_FALSE;
            }
        }
    }

    return S_FALSE;
}

STDMETHODIMP CSlxComOverlay::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags)
{
    WIN32_FILE_ATTRIBUTE_DATA wfad;
    DWORD dwIconFileAttribute = GetFileAttributesEx(m_szIconFilePath, GetFileExInfoStandard, &wfad);

    if(dwIconFileAttribute == INVALID_FILE_ATTRIBUTES || wfad.nFileSizeLow != m_dwIcoFileSize)
    {
        GenericIconFile();
    }

    lstrcpyn(pwszIconFile, m_szIconFilePath, cchMax);

    *pdwFlags = ISIOI_ICONFILE;

    return S_OK;
}

STDMETHODIMP CSlxComOverlay::GetPriority(int *pPriority)
{
    if(pPriority != NULL)
    {
        *pPriority = 0;
    }

    return S_OK;
}

BOOL CSlxComOverlay::GenericIconFile()
{
    TCHAR szPath[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA wfaa;

    GetTempPath(MAX_PATH, szPath);
    PathAppend(szPath, TEXT("\\__SignMark_20120202.ico"));

    if(SaveResourceToFile(TEXT("RT_FILE"), MAKEINTRESOURCE(IDR_ICON), szPath))
    {
        if(GetFileAttributesEx(szPath, GetFileExInfoStandard, &wfaa))
        {
            if(wfaa.nFileSizeHigh == 0)
            {
                lstrcpyn(m_szIconFilePath, szPath, MAX_PATH);
                m_dwIcoFileSize = wfaa.nFileSizeLow;

                return TRUE;
            }
        }
    }

    return FALSE;
}