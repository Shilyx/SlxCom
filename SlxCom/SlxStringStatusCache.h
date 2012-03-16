#ifndef _SLX_STRING_STATUS_CACHE_H
#define _SLX_STRING_STATUS_CACHE_H

#include <Windows.h>

#define SS_MASK     0xf

enum StringStatus
{
    SS_0,
    SS_1,
    SS_2,
    SS_3,
    SS_4,
    SS_5,
    SS_6,
    SS_7,
    SS_8,
    SS_9,
    SS_10,
    SS_11,
    SS_12,
    SS_13,
    SS_14,
    SS_15,
};

class SlxStringStatusCache
{
protected:
    HANDLE m_hThreadClean;
    BOOL m_bAutoClean;
//     TCHAR m_szRegPath[1000];
    HKEY m_hRegKey;

    DWORD GetCacheSize() const;
    static DWORD __stdcall AutoCleanThread(LPVOID lpParam);
    BOOL IsCleaning() const ;
    BOOL UpdateCache(LPCTSTR lpString, StringStatus ssValue);

public:
    BOOL CheckCache(LPCTSTR lpString, StringStatus *pssValue);
    BOOL AddCache(LPCTSTR lpString, StringStatus ssValue);

public:
    SlxStringStatusCache(LPCTSTR lpRegPath, BOOL bAutoClean = FALSE);
    ~SlxStringStatusCache();
};

#endif