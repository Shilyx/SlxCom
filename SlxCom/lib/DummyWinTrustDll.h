#ifndef _DUMMYWINTRUSTDLL_H
#define _DUMMYWINTRUSTDLL_H

#include <Windows.h>
#include <WinTrust.h>

typedef HANDLE HCATADMIN;
typedef HANDLE HCATINFO;

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI CryptCATAdminAcquireContext(
    HCATADMIN* phCatAdmin,
    const GUID* pgSubsystem,
    DWORD dwFlags
    );

BOOL WINAPI CryptCATAdminReleaseContext(
    HCATADMIN hCatAdmin,
    DWORD dwFlags
    );

BOOL WINAPI CryptCATAdminCalcHashFromFileHandle(
    HANDLE hFile,
    DWORD* pcbHash,
    BYTE* pbHash,
    DWORD dwFlags
    );

HCATINFO WINAPI CryptCATAdminEnumCatalogFromHash(
    HCATADMIN hCatAdmin,
    BYTE* pbHash,
    DWORD cbHash,
    DWORD dwFlags,
    HCATINFO* phPrevCatInfo
    );

BOOL WINAPI CryptCATAdminReleaseCatalogContext(
    HCATADMIN hCatAdmin,
    HCATINFO hCatInfo,
    DWORD dwFlags
    );

#ifdef __cplusplus
};
#endif

#endif /* _DUMMYWINTRUSTDLL_H */