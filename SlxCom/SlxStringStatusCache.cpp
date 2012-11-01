#include "SlxStringStatusCache.h"
#include <shlwapi.h>
#include "SlxString.h"
#pragma warning(disable: 4786)
#include <map>

using namespace std;

#pragma comment(lib, "Shlwapi.lib")

#define SSC_MAXSIZE     20000
#define SSC_CLEANTOSIZE 10000

SlxStringStatusCache::SlxStringStatusCache(LPCTSTR lpRegPath, BOOL bAutoClean)
{
    m_bAutoClean = bAutoClean;
    m_hThreadClean = NULL;

    RegCreateKeyEx(HKEY_CURRENT_USER, lpRegPath, 0, NULL, REG_OPTION_VOLATILE, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &m_hRegKey, NULL);
}

SlxStringStatusCache::~SlxStringStatusCache(void)
{
    if(m_hThreadClean != NULL)
    {
        DWORD dwExitCode = 0;

        WaitForSingleObject(m_hThreadClean, 500);

        GetExitCodeThread(m_hThreadClean, &dwExitCode);

        if(dwExitCode == STILL_ACTIVE)
        {
            TerminateThread(m_hThreadClean, 0);
        }

        CloseHandle(m_hThreadClean);
    }

    if(m_hRegKey != NULL)
    {
        RegCloseKey(m_hRegKey);
    }
}

DWORD SlxStringStatusCache::GetCacheSize() const
{
    DWORD dwCount = 0;
    HKEY hKey = NULL;

    if(m_hRegKey != NULL)
    {
        RegQueryInfoKey(m_hRegKey, NULL, NULL, NULL, NULL, NULL, NULL, &dwCount, NULL, NULL, NULL, NULL);
    }

    return dwCount;
}

DWORD __stdcall SlxStringStatusCache::AutoCleanThread(LPVOID lpParam)
{
    HKEY hRegKey = (HKEY)lpParam;

    if(hRegKey != NULL)
    {
        DWORD dwIndex = 0;
        DWORD dwCacheSize = 0;
        DWORD dwStringMaxSize = 0;

        if(ERROR_SUCCESS == RegQueryInfoKey(hRegKey, NULL, NULL, NULL, NULL, NULL, NULL, &dwCacheSize, &dwStringMaxSize, NULL, NULL, NULL))
        {
            if(dwStringMaxSize > 0 && dwCacheSize > SSC_MAXSIZE)
            {
                dwStringMaxSize += 100;

                TCHAR *szString = new TCHAR[dwStringMaxSize];

                if(szString != NULL)
                {
                    multimap<DWORD, tstring> mapAllStrings;

                    while(TRUE)
                    {
                        DWORD dwStringSize = dwStringMaxSize;
                        DWORD dwRegType = REG_NONE;
                        DWORD dwData = 0;
                        DWORD dwDataSize = sizeof(dwData);

                        if(ERROR_SUCCESS == RegEnumValue(hRegKey, dwIndex, szString, &dwStringSize, NULL, &dwRegType, (LPBYTE)&dwData, &dwDataSize))
                        {
                            mapAllStrings.insert(make_pair(dwData, szString));
                        }
                        else
                        {
                            break;
                        }

                        dwIndex += 1;
                    }

                    delete []szString;

                    if(mapAllStrings.size() > SSC_MAXSIZE)
                    {
                        DWORD dwCount = (DWORD)mapAllStrings.size() - SSC_MAXSIZE;
                        multimap<DWORD, tstring>::iterator it = mapAllStrings.begin();

                        for(dwIndex = 0; dwIndex < dwCount; dwIndex += 1, it++)
                        {
                            if(it == mapAllStrings.end())
                            {
                                break;
                            }

                            RegDeleteValue(hRegKey, it->second.c_str());
                        }
                    }
                }
            }
        }

        RegCloseKey(hRegKey);
    }

    return 0;
}

BOOL SlxStringStatusCache::IsCleaning() const
{
    if(m_hThreadClean != NULL)
    {
        DWORD dwExitCode = 0;

        GetExitCodeThread(m_hThreadClean, &dwExitCode);

        if(dwExitCode == STILL_ACTIVE)
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL SlxStringStatusCache::UpdateCache(LPCTSTR lpString, StringStatus ssValue)
{
    if(m_hRegKey != NULL)
    {
        DWORD dwData = (GetTickCount() & ~(DWORD)SS_MASK) | ssValue;

        return ERROR_SUCCESS == RegSetValueEx(m_hRegKey, lpString, 0, REG_DWORD, (LPCBYTE)&dwData, sizeof(dwData));
    }

    return FALSE;
}

BOOL SlxStringStatusCache::CheckCache(LPCTSTR lpString, StringStatus *pssValue)
{
    DWORD dwRegData = 0;
    DWORD dwRegType = REG_NONE;
    DWORD dwDataSize = sizeof(dwRegData);

    if(m_hRegKey != NULL)
    {
        if(ERROR_SUCCESS == RegQueryValueEx(m_hRegKey, lpString, NULL, &dwRegType, (LPBYTE)&dwRegData, &dwDataSize))
        {
            *pssValue = (StringStatus)(dwRegData & SS_MASK);

#ifdef _DEBUG
            TCHAR szDebugText[1000];

            wnsprintf(szDebugText, sizeof(szDebugText) / sizeof(TCHAR), TEXT("SlxCom!命中%s\r\n"), lpString);
            OutputDebugString(szDebugText);
#endif

            return UpdateCache(lpString, *pssValue);
        }
    }

    return FALSE;
}

BOOL SlxStringStatusCache::AddCache(LPCTSTR lpString, StringStatus ssValue)
{
    if(m_hRegKey)
    {
        if(GetCacheSize() > SSC_MAXSIZE)
        {
            if(!IsCleaning())
            {
                HKEY hCopyedKey = NULL;

                DuplicateHandle(GetCurrentProcess(), m_hRegKey, GetCurrentProcess(), (HANDLE *)&hCopyedKey, KEY_QUERY_VALUE | KEY_SET_VALUE, FALSE, 0);

                if(hCopyedKey != NULL)
                {
#ifdef _DEBUG
                    OutputDebugString(TEXT("SlxCom!开始清理\r\n"));
#endif

                    m_hThreadClean = CreateThread(NULL, 0, AutoCleanThread, (LPVOID)hCopyedKey, 0, NULL);
                }
            }
        }
    }

#ifdef _DEBUG
    TCHAR szDebugText[1000];

    wnsprintf(szDebugText, sizeof(szDebugText) / sizeof(TCHAR), TEXT("SlxCom!添加%s\r\n"), lpString);
    OutputDebugString(szDebugText);
#endif

    return UpdateCache(lpString, ssValue);
}