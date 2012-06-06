#include "SlxComOverlay.h"
#include "resource.h"
#include "SlxComTools.h"
#include <shlwapi.h>

extern HINSTANCE g_hinstDll;    //SlxCom.cpp
TCHAR CSlxComOverlay::m_szIconFilePath[] = TEXT("");
DWORD CSlxComOverlay::m_dwIcoFileSize = 0;

volatile
HANDLE CSlxComOverlay::m_hTaskEvent = NULL;
TCHAR  CSlxComOverlay::m_szTaskRegPath[1000] = TEXT("");
SlxStringStatusCache CSlxComOverlay::m_cache(TEXT("Software\\Slx\\StringStatusCache"), IsExplorer());

CSlxComOverlay::CSlxComOverlay()
{
    m_dwRefCount = 0;
}

STDMETHODIMP CSlxComOverlay::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IShellIconOverlayIdentifier)
    {
        HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if(InterlockedCompareExchange((volatile LONG *)&m_hTaskEvent, (LONG)hEvent, NULL) != NULL)
        {
            wnsprintf(m_szTaskRegPath, sizeof(m_szTaskRegPath) / sizeof(TCHAR), TEXT("Software\\Slx\\Tasks\\%lu"), GetCurrentProcessId());

            HANDLE hThread = CreateThread(NULL, 0, CheckTaskProc, NULL, 0, NULL);
            CloseHandle(hThread);

            CloseHandle(hEvent);
        }

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
    TCHAR szString[MAX_PATH + 100];

    if(BuildFileMarkString(pwszPath, szString, sizeof(szString) / sizeof(TCHAR)))
    {
        StringStatus ssValue = SS_0;

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

        SHSetTempValue(HKEY_CURRENT_USER, m_szTaskRegPath, szString, REG_SZ, pwszPath, (lstrlen(pwszPath) + 1) * sizeof(TCHAR));
        SetEvent(m_hTaskEvent);

        return S_FALSE;
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

BOOL CSlxComOverlay::BuildFileMarkString(LPCTSTR lpFilePath, LPTSTR lpMark, int nSize)
{
    WIN32_FILE_ATTRIBUTE_DATA wfad;

    if(GetFileAttributesEx(lpFilePath, GetFileExInfoStandard, &wfad))
    {
        if((wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            ULARGE_INTEGER uliSize = {0};

            uliSize.HighPart = wfad.nFileSizeHigh;
            uliSize.LowPart = wfad.nFileSizeLow;

            wnsprintf(lpMark, nSize, TEXT("%s[%I64u]%I64u,%I64u"),
                lpFilePath,
                uliSize.QuadPart,
                *(unsigned __int64 *)&wfad.ftCreationTime,
                *(unsigned __int64 *)&wfad.ftLastWriteTime);

            return TRUE;
        }
    }

    return FALSE;
}

DWORD __stdcall CSlxComOverlay::CheckTaskProc(LPVOID lpParam)
{
    while(TRUE)
    {
        DWORD dwWaitResult = WaitForSingleObject(m_hTaskEvent, INFINITE);

        if(dwWaitResult == WAIT_OBJECT_0)
        {
            HKEY hKey = NULL;

            if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, m_szTaskRegPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey))
            {
                while(TRUE)
                {
                    TCHAR szFilePath[MAX_PATH] = TEXT("");
                    DWORD dwFilePathSize = sizeof(szFilePath);
                    TCHAR szValueName[MAX_PATH + 100] = TEXT("");
                    DWORD dwValueNameSize = sizeof(szValueName) / sizeof(TCHAR);
                    DWORD dwRegType = REG_NONE;

                    if(ERROR_SUCCESS == RegEnumValue(hKey, 0, szValueName, &dwValueNameSize, NULL, &dwRegType, (LPBYTE)szFilePath, &dwFilePathSize))
                    {
                        if(dwRegType == REG_SZ)
                        {
                            if(PathFileExists(szFilePath))
                            {
                                if(IsFileSigned(szFilePath))
                                {
                                    m_cache.AddCache(szValueName, SS_1);
                                }
                                else
                                {
                                    m_cache.AddCache(szValueName, SS_2);
                                }
                            }
                        }

                        RegDeleteValue(hKey, szValueName);
                    }
                    else
                    {
                        break;
                    }
                }

                RegCloseKey(hKey);
            }
        }
        else
        {
            break;
        }
    }

    return 0;
}