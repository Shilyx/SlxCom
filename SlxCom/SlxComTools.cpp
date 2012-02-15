#include "SlxComTools.h"
#include <wintrust.h>
#include <Softpub.h>
#include <shlwapi.h>
#include <shlobj.h>
#include "resource.h"

#pragma comment(lib, "Wintrust.lib")

extern HINSTANCE g_hinstDll;

BOOL IsFileSigned(LPCTSTR lpFilePath)
{
    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_FILE_INFO sWintrustFileInfo;
    WINTRUST_DATA      sWintrustData;

    memset((void*)&sWintrustFileInfo, 0x00, sizeof(WINTRUST_FILE_INFO));
    memset((void*)&sWintrustData, 0x00, sizeof(WINTRUST_DATA));

    sWintrustFileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    sWintrustFileInfo.pcwszFilePath = lpFilePath;
    sWintrustFileInfo.hFile = NULL;

    sWintrustData.cbStruct            = sizeof(WINTRUST_DATA);
    sWintrustData.dwUIChoice          = WTD_UI_NONE;
    sWintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    sWintrustData.dwUnionChoice       = WTD_CHOICE_FILE;
    sWintrustData.pFile               = &sWintrustFileInfo;
    sWintrustData.dwStateAction       = WTD_STATEACTION_VERIFY;

    return S_OK == WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);
}

BOOL SaveResourceToFile(LPCTSTR lpResType, LPCTSTR lpResName, LPCTSTR lpFilePath)
{
    BOOL bSucceed = FALSE;
    DWORD dwResSize = 0;

    HANDLE hFile = CreateFile(lpFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if(hFile != INVALID_HANDLE_VALUE)
    {
        HRSRC hRsrc = FindResource(g_hinstDll, lpResName, lpResType);

        if(hRsrc != NULL)
        {
            HGLOBAL hGlobal = LoadResource(g_hinstDll, hRsrc);

            if(hGlobal != NULL)
            {
                LPCSTR lpBuffer = (LPCSTR)LockResource(hGlobal);
                dwResSize = SizeofResource(g_hinstDll, hRsrc);

                if(lpBuffer != NULL && dwResSize > 0)
                {
                    DWORD dwBytesWritten = 0;

                    bSucceed = WriteFile(hFile, lpBuffer, dwResSize, &dwBytesWritten, NULL);
                }
            }
        }

        CloseHandle(hFile);
    }

    return bSucceed;
}

BOOL GetResultPath(LPCTSTR lpRarPath, LPTSTR lpResultPath, int nLength)
{
    if(nLength >= MAX_PATH)
    {
        TCHAR szJpgPath[MAX_PATH];

        lstrcpyn(szJpgPath, lpRarPath, MAX_PATH);
        PathRenameExtension(szJpgPath, TEXT(".jpg"));

        return PathYetAnotherMakeUniqueName(lpResultPath, szJpgPath, NULL, NULL);
    }

    return FALSE;
}

BOOL DoCombine(LPCTSTR lpFile1, LPCTSTR lpFile2, LPCTSTR lpFileNew)
{
    IStream *pStream1 = NULL;
    IStream *pStream2 = NULL;
    IStream *pStreamNew = NULL;

    if(S_OK == SHCreateStreamOnFile(lpFile1, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream1))
    {
        if(S_OK == SHCreateStreamOnFile(lpFile2, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream2))
        {
            if(S_OK == SHCreateStreamOnFile(lpFileNew, STGM_WRITE | STGM_SHARE_DENY_NONE | STGM_CREATE, &pStreamNew))
            {
                ULARGE_INTEGER uliSize1, uliSize2;
                STATSTG stat;

                pStream1->Stat(&stat, STATFLAG_NONAME);
                uliSize1.QuadPart = stat.cbSize.QuadPart;

                pStream2->Stat(&stat, STATFLAG_NONAME);
                uliSize2.QuadPart = stat.cbSize.QuadPart;

                pStream1->CopyTo(pStreamNew, uliSize1, NULL, NULL);
                pStream2->CopyTo(pStreamNew, uliSize2, NULL, NULL);

                pStreamNew->Release();
            }

            pStream2->Release();
        }

        pStream1->Release();
    }

    return TRUE;
}

BOOL CombineFile(LPCTSTR lpRarPath, LPCTSTR lpJpgPath, LPTSTR lpResultPath, int nLength)
{
    //Get jpg file path
    TCHAR szJpgPath[MAX_PATH];

    if(GetFileAttributes(lpJpgPath) == INVALID_FILE_ATTRIBUTES)
    {
        GetModuleFileName(g_hinstDll, szJpgPath, MAX_PATH);
        PathAppend(szJpgPath, TEXT("\\..\\Default.jpg"));

        if(GetFileAttributes(szJpgPath) == INVALID_FILE_ATTRIBUTES)
        {
            SaveResourceToFile(TEXT("RT_FILE"), MAKEINTRESOURCE(IDR_DEFAULTJPG), szJpgPath);
        }
    }
    else
    {
        lstrcpyn(szJpgPath, lpJpgPath, MAX_PATH);
    }

    if(GetFileAttributes(szJpgPath) != INVALID_FILE_ATTRIBUTES)
    {
        if(GetResultPath(lpRarPath, lpResultPath, nLength))
        {
            return DoCombine(szJpgPath, lpRarPath, lpResultPath);
        }
    }

    return FALSE;
}

void WINAPI T(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    TCHAR szResultPath[MAX_PATH];
    CombineFile(TEXT("D:\\×ÀÃæ\\StatusStr.rar"), NULL, szResultPath, MAX_PATH);

    MessageBox(hwndStub, NULL, NULL, MB_TOPMOST);
}