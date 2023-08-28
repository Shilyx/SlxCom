#include "SlxComOverlay.h"
#include "resource.h"
#include "SlxComTools.h"
#include <shlwapi.h>

#define MAX_FILESIZE        (200 * 1024 * 1024)     //自动校验数字签名功能的文件大小限制，200M

extern HINSTANCE g_hinstDll;    //SlxCom.cpp
extern OSVERSIONINFO g_osi;     //SlxCom.cpp
WCHAR CSlxComOverlay::m_szIconFilePath[] = L"";
DWORD CSlxComOverlay::m_dwIcoFileSize = 0;

volatile
HANDLE CSlxComOverlay::m_hTaskEvent = NULL;
WCHAR  CSlxComOverlay::m_szTaskRegPath[1000] = L"";
SlxStringStatusCache CSlxComOverlay::m_cache(L"Software\\Slx\\StringStatusCache", IsExplorer());

CSlxComOverlay::CSlxComOverlay()
{
    m_dwRefCount = 0;
}

STDMETHODIMP CSlxComOverlay::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IShellIconOverlayIdentifier)
    {
        if (g_osi.dwMajorVersion > 5)
        {
        }
        else
        {
            HANDLE hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);

#ifdef _WIN64
            if (InterlockedCompareExchange64((volatile LONGLONG *)&m_hTaskEvent, (LONGLONG)hEvent, NULL) == NULL)
#else
            if (InterlockedCompareExchange((volatile LONG *)&m_hTaskEvent, (LONG)hEvent, NULL) == NULL)
#endif
            {
                wnsprintfW(m_szTaskRegPath, sizeof(m_szTaskRegPath) / sizeof(WCHAR), L"Software\\Slx\\Tasks\\%lu", GetCurrentProcessId());

                HANDLE hThread = CreateThread(NULL, 0, CheckTaskProc, NULL, 0, NULL);
                CloseHandle(hThread);
            }
            else
            {
                CloseHandle(hEvent);
            }
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
    WCHAR szString[MAX_PATH + 100];
    ULARGE_INTEGER uliFileSize = {0};

    if(BuildFileMarkString(pwszPath, szString, sizeof(szString) / sizeof(WCHAR), &uliFileSize))
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
        else
        {
            //仅校验大小不超过限制的文件的数字签名
            if (uliFileSize.QuadPart <= MAX_FILESIZE)
            {
                if (g_osi.dwMajorVersion > 5)
                {
                    if (IsFileSigned(pwszPath))
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
                else
                {
                    SHSetTempValue(HKEY_CURRENT_USER, m_szTaskRegPath, szString, REG_SZ, pwszPath, (lstrlenW(pwszPath) + 1) * sizeof(WCHAR));

                    if (m_hTaskEvent != NULL)
                    {
                        SetEvent(m_hTaskEvent);
                    }
                }
            }
        }

        return S_FALSE;
    }

    return S_FALSE;
}

STDMETHODIMP CSlxComOverlay::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags)
{
    WIN32_FILE_ATTRIBUTE_DATA wfad;

    if (!GetFileAttributesExW(m_szIconFilePath, GetFileExInfoStandard, &wfad) || wfad.nFileSizeLow != m_dwIcoFileSize)
    {
        GenericIconFile();
    }

    lstrcpynW(pwszIconFile, m_szIconFilePath, cchMax);

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
    WCHAR szPath[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA wfaa;

    GetTempPathW(MAX_PATH, szPath);
    PathAppendW(szPath, L"\\__SignMark_20120202.ico");

    if(SaveResourceToFile(L"RT_FILE", MAKEINTRESOURCEW(IDR_ICON), szPath))
    {
        if(GetFileAttributesExW(szPath, GetFileExInfoStandard, &wfaa))
        {
            if(wfaa.nFileSizeHigh == 0)
            {
                lstrcpynW(m_szIconFilePath, szPath, MAX_PATH);
                m_dwIcoFileSize = wfaa.nFileSizeLow;

                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOL CSlxComOverlay::BuildFileMarkString(LPCWSTR lpFilePath, LPWSTR lpMark, int nSize, ULARGE_INTEGER *puliFileSize)
{
    WIN32_FILE_ATTRIBUTE_DATA wfad;

    if(GetFileAttributesExW(lpFilePath, GetFileExInfoStandard, &wfad))
    {
        if((wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            ULARGE_INTEGER uliSize = {0};

            uliSize.HighPart = wfad.nFileSizeHigh;
            uliSize.LowPart = wfad.nFileSizeLow;

            wnsprintfW(lpMark, nSize, L"%s[%I64u]%I64u,%I64u",
                lpFilePath,
                uliSize.QuadPart,
                *(unsigned __int64 *)&wfad.ftCreationTime,
                *(unsigned __int64 *)&wfad.ftLastWriteTime);

            if(puliFileSize != NULL)
            {
                *puliFileSize = uliSize;
            }

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

            if(ERROR_SUCCESS == RegOpenKeyExW(HKEY_CURRENT_USER, m_szTaskRegPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey))
            {
                while(TRUE)
                {
                    WCHAR szFilePath[MAX_PATH] = L"";
                    DWORD dwFilePathSize = sizeof(szFilePath);
                    WCHAR szValueName[MAX_PATH + 100] = L"";
                    DWORD dwValueNameSize = sizeof(szValueName) / sizeof(WCHAR);
                    DWORD dwRegType = REG_NONE;

                    if(ERROR_SUCCESS == RegEnumValueW(hKey, 0, szValueName, &dwValueNameSize, NULL, &dwRegType, (LPBYTE)szFilePath, &dwFilePathSize))
                    {
                        if(dwRegType == REG_SZ)
                        {
                            if(PathFileExistsW(szFilePath))
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

                        RegDeleteValueW(hKey, szValueName);
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