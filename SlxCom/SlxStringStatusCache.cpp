#include "SlxStringStatusCache.h"
#include <shlwapi.h>
#pragma warning(disable: 4786)
#include "lib/charconv.h"
#include <map>

using namespace std;

#pragma comment(lib, "Shlwapi.lib")

#define SSC_MAXSIZE     20000
#define SSC_CLEANTOSIZE 10000

SlxStringStatusCache::SlxStringStatusCache(LPCWSTR lpRegPath, BOOL bAutoClean) {
    m_bAutoClean = bAutoClean;
    m_hThreadClean = NULL;

    RegCreateKeyExW(HKEY_CURRENT_USER, lpRegPath, 0, NULL, REG_OPTION_VOLATILE, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &m_hRegKey, NULL);
}

SlxStringStatusCache::~SlxStringStatusCache(void) {
    if (m_hThreadClean != NULL) {
        DWORD dwExitCode = 0;

        WaitForSingleObject(m_hThreadClean, 500);

        GetExitCodeThread(m_hThreadClean, &dwExitCode);

        if (dwExitCode == STILL_ACTIVE) {
            TerminateThread(m_hThreadClean, 0);
        }

        CloseHandle(m_hThreadClean);
    }

    if (m_hRegKey != NULL) {
        RegCloseKey(m_hRegKey);
    }
}

DWORD SlxStringStatusCache::GetCacheSize() const {
    DWORD dwCount = 0;
    HKEY hKey = NULL;

    if (m_hRegKey != NULL) {
        RegQueryInfoKey(m_hRegKey, NULL, NULL, NULL, NULL, NULL, NULL, &dwCount, NULL, NULL, NULL, NULL);
    }

    return dwCount;
}

DWORD CALLBACK SlxStringStatusCache::AutoCleanThread(LPVOID lpParam) {
    HKEY hRegKey = (HKEY)lpParam;

    if (hRegKey != NULL) {
        DWORD dwIndex = 0;
        DWORD dwCacheSize = 0;
        DWORD dwStringMaxSize = 0;

        if (ERROR_SUCCESS == RegQueryInfoKey(hRegKey, NULL, NULL, NULL, NULL, NULL, NULL, &dwCacheSize, &dwStringMaxSize, NULL, NULL, NULL)) {
            if (dwStringMaxSize > 0 && dwCacheSize > SSC_MAXSIZE) {
                dwStringMaxSize += 100;

                WCHAR *szString = new WCHAR[dwStringMaxSize];

                if (szString != NULL) {
                    multimap<DWORD, wstring> mapAllStrings;

                    while (TRUE) {
                        DWORD dwStringSize = dwStringMaxSize;
                        DWORD dwRegType = REG_NONE;
                        DWORD dwData = 0;
                        DWORD dwDataSize = sizeof(dwData);

                        if (ERROR_SUCCESS == RegEnumValueW(hRegKey, dwIndex, szString, &dwStringSize, NULL, &dwRegType, (LPBYTE)&dwData, &dwDataSize)) {
                            mapAllStrings.insert(make_pair(dwData, szString));
                        } else {
                            break;
                        }

                        dwIndex += 1;
                    }

                    delete []szString;

                    if (mapAllStrings.size() > SSC_MAXSIZE) {
                        DWORD dwCount = (DWORD)mapAllStrings.size() - SSC_MAXSIZE;
                        multimap<DWORD, wstring>::iterator it = mapAllStrings.begin();

                        for (dwIndex = 0; dwIndex < dwCount; dwIndex += 1, it++) {
                            if (it == mapAllStrings.end()) {
                                break;
                            }

                            RegDeleteValueW(hRegKey, it->second.c_str());
                        }
                    }
                }
            }
        }

        RegCloseKey(hRegKey);
    }

    return 0;
}

BOOL SlxStringStatusCache::IsCleaning() const {
    if (m_hThreadClean != NULL) {
        DWORD dwExitCode = 0;

        GetExitCodeThread(m_hThreadClean, &dwExitCode);

        if (dwExitCode == STILL_ACTIVE) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL SlxStringStatusCache::UpdateCache(LPCWSTR lpString, StringStatus ssValue) {
    if (m_hRegKey != NULL) {
        DWORD dwData = (GetTickCount() & ~(DWORD)SS_MASK) | ssValue;

        return ERROR_SUCCESS == RegSetValueExW(m_hRegKey, lpString, 0, REG_DWORD, (LPCBYTE)&dwData, sizeof(dwData));
    }

    return FALSE;
}

BOOL SlxStringStatusCache::CheckCache(LPCWSTR lpString, StringStatus *pssValue) {
    DWORD dwRegData = 0;
    DWORD dwRegType = REG_NONE;
    DWORD dwDataSize = sizeof(dwRegData);

    if (m_hRegKey != NULL) {
        if (ERROR_SUCCESS == RegQueryValueExW(m_hRegKey, lpString, NULL, &dwRegType, (LPBYTE)&dwRegData, &dwDataSize)) {
            *pssValue = (StringStatus)(dwRegData & SS_MASK);

// #ifdef _DEBUG
//             WCHAR szDebugText[1000];
//
//             wnsprintfW(szDebugText, RTL_NUMBER_OF(szDebugText), L"SlxCom!命中%s\r\n", lpString);
//             OutputDebugStringW(szDebugText);
// #endif

            return UpdateCache(lpString, *pssValue);
        }
    }

    return FALSE;
}

BOOL SlxStringStatusCache::AddCache(LPCWSTR lpString, StringStatus ssValue) {
    if (m_hRegKey) {
        if (GetCacheSize() > SSC_MAXSIZE) {
            if (!IsCleaning()) {
                HKEY hCopyedKey = NULL;

                DuplicateHandle(GetCurrentProcess(), m_hRegKey, GetCurrentProcess(), (HANDLE *)&hCopyedKey, KEY_QUERY_VALUE | KEY_SET_VALUE, FALSE, 0);

                if (hCopyedKey != NULL) {
#ifdef _DEBUG
                    OutputDebugStringW(L"SlxCom!开始清理\r\n");
#endif

                    m_hThreadClean = CreateThread(NULL, 0, AutoCleanThread, (LPVOID)hCopyedKey, 0, NULL);
                }
            }
        }
    }

// #ifdef _DEBUG
//     WCHAR szDebugText[1000];
//
//     wnsprintfW(szDebugText, RTL_NUMBER_OF(szDebugText), L"SlxCom!添加%s\r\n", lpString);
//     OutputDebugStringW(szDebugText);
// #endif

    return UpdateCache(lpString, ssValue);
}