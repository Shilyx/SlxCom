#include "SlxComTools.h"
#include <wintrust.h>
#include <Softpub.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <WinTrust.h>
#include "resource.h"
#include "lib/deelx.h"

#pragma comment(lib, "Wintrust.lib")

typedef HANDLE HCATADMIN;
typedef HANDLE HCATINFO;

typedef BOOL (WINAPI* PFN_CryptCATAdminAcquireContext)(
    HCATADMIN* phCatAdmin,
    const GUID* pgSubsystem,
    DWORD dwFlags
    );

typedef BOOL (WINAPI* PFN_CryptCATAdminReleaseContext)(
    HCATADMIN hCatAdmin,
    DWORD dwFlags
    );

typedef BOOL (WINAPI* PFN_CryptCATAdminCalcHashFromFileHandle)(
    HANDLE hFile,
    DWORD* pcbHash,
    BYTE* pbHash,
    DWORD dwFlags
    );

typedef HCATINFO (WINAPI* PFN_CryptCATAdminEnumCatalogFromHash)(
    HCATADMIN hCatAdmin,
    BYTE* pbHash,
    DWORD cbHash,
    DWORD dwFlags,
    HCATINFO* phPrevCatInfo
    );

typedef BOOL (WINAPI* PFN_CryptCATAdminReleaseCatalogContext)(
    HCATADMIN hCatAdmin,
    HCATINFO hCatInfo,
    DWORD dwFlags
    );

typedef LONG (WINAPI* PFN_WinVerifyTrust)(
    HWND hWnd,
    GUID* pgActionID,
    WINTRUST_DATA* pWinTrustData
    );

static PFN_CryptCATAdminAcquireContext          g_pfnCryptCATAdminAcquireContext            = NULL;
static PFN_CryptCATAdminReleaseContext          g_pfnCryptCATAdminReleaseContext            = NULL;
static PFN_CryptCATAdminCalcHashFromFileHandle  g_pfnCryptCATAdminCalcHashFromFileHandle    = NULL;
static PFN_CryptCATAdminEnumCatalogFromHash     g_pfnCryptCATAdminEnumCatalogFromHash       = NULL;
static PFN_CryptCATAdminReleaseCatalogContext   g_pfnCryptCATAdminReleaseCatalogContext     = NULL;
static PFN_WinVerifyTrust                       g_pfnWinVerifyTrust                         = NULL;

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

BOOL LoadWinTrustDll()
{
    static BOOL bResult = FALSE;

    if(bResult)
    {
        return bResult;
    }

    HMODULE hModule = GetModuleHandle(SP_POLICY_PROVIDER_DLL_NAME);

    if(hModule == NULL)
    {
        hModule = LoadLibrary(SP_POLICY_PROVIDER_DLL_NAME);
    }

    if(hModule != NULL)
    {
        do 
        {
            (FARPROC&)g_pfnCryptCATAdminAcquireContext = GetProcAddress(hModule, "CryptCATAdminAcquireContext");
            if(g_pfnCryptCATAdminAcquireContext == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminReleaseContext = GetProcAddress(hModule, "CryptCATAdminReleaseContext");
            if(g_pfnCryptCATAdminReleaseContext == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminCalcHashFromFileHandle = GetProcAddress(hModule, "CryptCATAdminCalcHashFromFileHandle");
            if(g_pfnCryptCATAdminCalcHashFromFileHandle == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminEnumCatalogFromHash = GetProcAddress(hModule, "CryptCATAdminEnumCatalogFromHash");
            if(g_pfnCryptCATAdminEnumCatalogFromHash == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminReleaseCatalogContext = GetProcAddress(hModule, "CryptCATAdminReleaseCatalogContext");
            if(g_pfnCryptCATAdminReleaseCatalogContext == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnWinVerifyTrust = GetProcAddress(hModule, "WinVerifyTrust");
            if(g_pfnWinVerifyTrust == NULL)
            {
                break;
            }

            bResult = TRUE;

        } while (FALSE);
    }

    return bResult;
}

DWORD SHSetTempValue(HKEY hRootKey, LPCTSTR pszSubKey, LPCTSTR pszValue, DWORD dwType, LPCVOID pvData, DWORD cbData)
{
    HKEY hKey = NULL;
    DWORD dwRet = RegCreateKeyEx(hRootKey, pszSubKey, 0, NULL, REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if(ERROR_SUCCESS == dwRet)
    {
        dwRet = RegSetValueEx(hKey, pszValue, 0, dwType, (const unsigned char *)pvData, cbData);

        RegCloseKey(hKey);
    }

    return dwRet;
}

BOOL IsFileSignedBySelf(LPCTSTR lpFilePath)
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

BOOL GetFileHash(LPCTSTR lpFilePath, BYTE btHash[], DWORD *pcbSize)
{
    BOOL bResult = FALSE;
    HANDLE hFile = CreateFile(
        lpFilePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
        );

    if(hFile != INVALID_HANDLE_VALUE)
    {
        __try
        {
            if(g_pfnCryptCATAdminCalcHashFromFileHandle(hFile, pcbSize, btHash, 0))
            {
                bResult = TRUE;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
        }

        CloseHandle(hFile);
    }

    return bResult;
}

BOOL IsFileSignedByCatlog(LPCTSTR lpFilePath)
{
    BOOL bResult = FALSE;

    if(LoadWinTrustDll())
    {
        BYTE btHash[100];
        DWORD dwHashSize = sizeof(btHash);

        if(GetFileHash(lpFilePath, btHash, &dwHashSize))
        {
            HANDLE hCatHandle = NULL;

            g_pfnCryptCATAdminAcquireContext(&hCatHandle, NULL, 0);

            if(hCatHandle != NULL)
            {
                __try
                {
                    HANDLE hCatalogContext = g_pfnCryptCATAdminEnumCatalogFromHash(hCatHandle, btHash, dwHashSize, 0, NULL);

                    if(hCatalogContext != NULL)
                    {
                        g_pfnCryptCATAdminReleaseCatalogContext(hCatHandle, hCatalogContext, 0);

                        bResult = TRUE;
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                }

                g_pfnCryptCATAdminReleaseContext(hCatHandle, 0);
            }
        }
    }

    return bResult;
}

BOOL IsFileSigned(LPCTSTR lpFilePath)
{
    return IsFileSignedBySelf(lpFilePath) || IsFileSignedByCatlog(lpFilePath);
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

// UINT GetDropEffectFormat()
// {
//     static UINT uDropEffect = RegisterClipboardFormat(TEXT("Preferred DropEffect"));
// 
//     return uDropEffect;
// }
// 
// LPCTSTR GetClipboardFiles(DWORD *pdwEffect, UINT *puFileCount)
// {
//     TCHAR *szFiles = NULL;
// 
//     if(puFileCount != NULL)
//     {
//         *puFileCount = 0;
//     }
// 
//     if(pdwEffect != NULL)
//     {
//         *pdwEffect = 0;
// 
//         if(OpenClipboard(NULL))
//         {
//             if(IsClipboardFormatAvailable(GetDropEffectFormat()) && IsClipboardFormatAvailable(CF_HDROP))
//             {
//                 HGLOBAL hGlobalEffect = GetClipboardData(GetDropEffectFormat());
//                 HGLOBAL hGlobalDrop = GetClipboardData(CF_HDROP);
// 
//                 if(hGlobalEffect != NULL && hGlobalDrop != NULL)
//                 {
//                     LPCVOID lpBuffer = (LPCVOID)GlobalLock(hGlobalEffect);
// 
//                     if(lpBuffer != NULL)
//                     {
//                         *pdwEffect = *(DWORD *)lpBuffer;
// 
//                         GlobalUnlock(hGlobalEffect);
//                     }
// 
//                     if((*pdwEffect & DROPEFFECT_COPY) || (*pdwEffect & DROPEFFECT_MOVE))
//                     {
//                         HDROP hDrop = (HDROP)GlobalLock(hGlobalDrop);
// 
//                         if(hDrop != NULL)
//                         {
//                             UINT uCount = DragQueryFile(hDrop, 0xffffffff, NULL, 0);
// 
//                             if(puFileCount != NULL)
//                             {
//                                 *puFileCount = uCount;
//                             }
// 
//                             if(uCount > 0)
//                             {
//                                 UINT uOffset = 0;
//                                 szFiles = new TCHAR[MAX_PATH * uCount];
// 
//                                 if(szFiles != NULL)
//                                 {
//                                     ZeroMemory(szFiles, MAX_PATH * uCount);
// 
//                                     for(UINT uIndex = 0; uIndex < uCount; uIndex += 1)
//                                     {
//                                         uOffset += DragQueryFile(hDrop, uIndex, szFiles + uOffset, MAX_PATH) + 1;
//                                     }
//                                 }
//                             }
// 
//                             GlobalUnlock(hGlobalDrop);
//                         }
//                     }
//                 }
//             }
// 
//             CloseClipboard();
//         }
//     }
// 
//     return szFiles;
// }
// 
// BOOL RegisterClipboardFile(LPCTSTR lpFileList, BOOL bCopy)
// {
//     DWORD dwBufferSize = 0;
// 
//     while(true)
//     {
//         if(lpFileList[dwBufferSize] == TEXT('\0') && lpFileList[dwBufferSize + 1] == TEXT('\0'))
//         {
//             dwBufferSize += 2;
//             break;
//         }
// 
//         dwBufferSize += 1;
//     }
// 
//     dwBufferSize *= sizeof(TCHAR);
//     dwBufferSize += sizeof(DROPFILES);
// 
//     char *lpBufferAll = new char[dwBufferSize];
// 
//     if(lpBufferAll != NULL)
//     {
//         DROPFILES *pDropFiles = (DROPFILES *)lpBufferAll;
// 
//         pDropFiles->fNC = FALSE;
//         pDropFiles->pt.x = 0;
//         pDropFiles->pt.y = 0;
//         pDropFiles->fWide = TRUE;
//         pDropFiles->pFiles = sizeof(DROPFILES);
// 
//         memcpy(lpBufferAll + sizeof(DROPFILES), lpFileList, dwBufferSize - sizeof(DROPFILES));
// 
//         HGLOBAL hGblFiles = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, dwBufferSize);
// 
//         if(hGblFiles != NULL)
//         {
//             LPSTR lpGblData = (char *)GlobalLock(hGblFiles);
// 
//             if(lpGblData != NULL)
//             {
//                 memcpy(lpGblData, lpBufferAll, dwBufferSize);
//                 GlobalUnlock(hGblFiles);
//             }
//         }
// 
//         HGLOBAL hGblEffect = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DWORD));
// 
//         if(hGblEffect != NULL)
//         {
//             DWORD *pEffect = (DWORD *)GlobalLock(hGblEffect);
// 
//             if(pEffect != NULL)
//             {
//                 *pEffect = bCopy ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
//                 GlobalUnlock(hGblEffect);
//             }
//         }
// 
//         if(OpenClipboard(NULL))
//         {
//             EmptyClipboard();
// 
//             SetClipboardData(CF_HDROP, hGblFiles);
//             SetClipboardData(GetDropEffectFormat(), hGblEffect);
// 
//             CloseClipboard();
//         }
// 
//         delete []lpBufferAll;
//     }
// 
//     return TRUE;
// }

BOOL SetClipboardText(LPCTSTR lpText)
{
    BOOL bResult = FALSE;
    HGLOBAL hGlobal = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, (lstrlen(lpText) + 1) * sizeof(TCHAR));

    if(hGlobal != NULL)
    {
        LPTSTR lpBuffer = (LPTSTR)GlobalLock(hGlobal);

        if(lpBuffer != NULL)
        {
            lstrcpy(lpBuffer, lpText);

            GlobalUnlock(hGlobal);

            if(OpenClipboard(NULL))
            {
                EmptyClipboard();
#ifdef UNICODE
                SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
                SetClipboardData(CF_TEXT, hGlobal);
#endif
                CloseClipboard();

                bResult = TRUE;
            }
        }
    }

    if(!bResult && hGlobal != NULL)
    {
        GlobalFree(hGlobal);
    }

    return bResult;
}

BOOL RunCommand(LPTSTR lpCommandLine, LPCTSTR lpCurrentDirectory)
{
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    si.wShowWindow = SW_SHOW;

    if(CreateProcess(NULL, lpCommandLine, NULL, NULL, FALSE, 0, NULL, lpCurrentDirectory, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return TRUE;
    }

    return FALSE;
}

INT_PTR __stdcall RunCommandWithArgumentsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        SetDlgItemText(hwndDlg, IDC_FILE, (LPCTSTR)lParam);
        return FALSE;

    case WM_SYSCOMMAND:
        if(wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_COMMAND:
        if(LOWORD(wParam) == IDC_RUN)
        {
            if(IDYES == MessageBox(hwndDlg, TEXT("ȷ��Ҫ���Խ����ļ���Ϊ��ִ���ļ�������"), TEXT("��ȷ��"), MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3))
            {
                BOOL bSucceed = FALSE;
                INT_PTR nLength = 0;
                INT_PTR nOffset = 0;

                nLength += SendDlgItemMessage(hwndDlg, IDC_FILE, WM_GETTEXTLENGTH, 0, 0);
                nLength += SendDlgItemMessage(hwndDlg, IDC_ARGUMENTS, WM_GETTEXTLENGTH, 0, 0);
                nLength += 200;

                TCHAR *szCommand = new TCHAR[nLength];

                if(szCommand != NULL)
                {
                    lstrcpy(szCommand, TEXT("\""));
                    nOffset += 1;

                    nOffset += SendDlgItemMessage(hwndDlg, IDC_FILE, WM_GETTEXT, nLength - nOffset, (LPARAM)(szCommand + nOffset));

                    lstrcat(szCommand, TEXT("\" "));
                    nOffset += 2;

                    SendDlgItemMessage(hwndDlg, IDC_ARGUMENTS, WM_GETTEXT, nLength - nOffset, (LPARAM)(szCommand + nOffset));

                    bSucceed = RunCommand(szCommand);

                    delete []szCommand;
                }

                if(bSucceed)
                {
                    EndDialog(hwndDlg, 1);
                }
                else
                {
                    TCHAR szErrorMessage[200];

                    wnsprintf(szErrorMessage, sizeof(szErrorMessage) / sizeof(TCHAR), TEXT("�޷��������̣�������%lu"), GetLastError());

                    MessageBox(hwndDlg, szErrorMessage, NULL, MB_ICONERROR);

                    SendDlgItemMessage(hwndDlg, IDC_FILE, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hwndDlg, IDC_FILE, EM_SETREADONLY, FALSE, 0);
                    SetFocus(GetDlgItem(hwndDlg, IDC_FILE));
                }
            }
            else
            {
                SendDlgItemMessage(hwndDlg, IDC_ARGUMENTS, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwndDlg, IDC_ARGUMENTS));
            }
        }
        else if(LOWORD(wParam) == IDC_CANCEL)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (BOOL)GetStockObject(WHITE_BRUSH);

    default:
        break;
    }

    return FALSE;
}

BOOL RunCommandWithArguments(LPCTSTR lpFile)
{
    return 0 != DialogBoxParam(g_hinstDll, MAKEINTRESOURCE(IDD_RUNWITHARGUMENTS), NULL, RunCommandWithArgumentsProc, (LPARAM)lpFile);
}

BOOL BrowseForFile(LPCTSTR lpFile)
{
    TCHAR szExplorerPath[MAX_PATH];
    TCHAR szCommandLine[MAX_PATH * 2 + 100];

    GetWindowsDirectory(szExplorerPath, MAX_PATH);
    PathAppend(szExplorerPath, TEXT("\\Explorer.exe"));

    wnsprintf(szCommandLine, sizeof(szCommandLine) / sizeof(TCHAR), TEXT("%s /n,/select,\"%s\""), szExplorerPath, lpFile);

    return RunCommand(szCommandLine, NULL);
}

void WINAPI ResetExplorer(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    int nCmdLineLength = lstrlenA(lpszCmdLine);

    if(nCmdLineLength > 0)
    {
        WCHAR *szBuffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (nCmdLineLength + 100) * sizeof(WCHAR));

        if(szBuffer != NULL)
        {
            if(wnsprintf(szBuffer, (nCmdLineLength + 100), L"%S", lpszCmdLine) > 0)
            {
                int nArgc = 0;
                LPWSTR *ppArgv = CommandLineToArgvW(szBuffer, &nArgc);

                if(ppArgv != NULL)
                {
                    if(nArgc >= 1)
                    {
                        DWORD dwProcessId = StrToInt(ppArgv[0]);

                        if(dwProcessId != 0)
                        {
                            do
                            {
                                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, dwProcessId);

                                if(hProcess == NULL)
                                {
                                    MessageBox(hwndStub, TEXT("�򿪽���ʧ�ܣ�"), NULL, MB_ICONERROR | MB_TOPMOST);

                                    break;
                                }

                                TerminateProcess(hProcess, 0);

                                WaitForSingleObject(hProcess, 5000);

                                DWORD dwExitCode = 0;
                                GetExitCodeProcess(hProcess, &dwExitCode);

                                if(dwExitCode == STILL_ACTIVE)
                                {
                                    MessageBox(hwndStub, TEXT("�޷���ֹ���̣�"), NULL, MB_ICONERROR | MB_TOPMOST);

                                    break;
                                }

                                CloseHandle(hProcess);

                                STARTUPINFO si = {sizeof(si)};
                                PROCESS_INFORMATION pi;
                                TCHAR szExplorerPath[MAX_PATH];

                                GetWindowsDirectory(szExplorerPath, MAX_PATH);
                                PathAppend(szExplorerPath, TEXT("\\Explorer.exe"));

                                if(CreateProcess(NULL, szExplorerPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                                {
                                    if(nArgc >= 2 && lstrlenW(ppArgv[1]) < MAX_PATH)
                                    {
                                        WaitForInputIdle(pi.hProcess, 10 * 1000);

                                        TCHAR szFilePath[MAX_PATH];
                                        DWORD dwFileAttributes = INVALID_FILE_ATTRIBUTES;

                                        wnsprintf(szFilePath, sizeof(szFilePath) / sizeof(TCHAR), TEXT("%ls"), ppArgv[1]);

                                        dwFileAttributes = GetFileAttributes(szFilePath);

                                        if(dwFileAttributes != INVALID_FILE_ATTRIBUTES)
                                        {
                                            if(dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                                            {
                                                TCHAR szFindMark[MAX_PATH];
                                                WIN32_FIND_DATA wfd;

                                                lstrcpyn(szFindMark, szFilePath, MAX_PATH);
                                                PathAppend(szFindMark, TEXT("\\*"));

                                                HANDLE hFind = FindFirstFile(szFindMark, &wfd);

                                                if(hFind != INVALID_HANDLE_VALUE)
                                                {
                                                    do 
                                                    {
                                                        if(lstrcmp(wfd.cFileName, TEXT(".")) != 0 && lstrcmp(wfd.cFileName, TEXT("..")) != 0)
                                                        {
                                                            break;
                                                        }

                                                    } while (FindNextFile(hFind, &wfd));

                                                    if(lstrcmp(wfd.cFileName, TEXT(".")) != 0 && lstrcmp(wfd.cFileName, TEXT("..")) != 0)
                                                    {
                                                        PathAddBackslash(szFilePath);
                                                        lstrcat(szFilePath, wfd.cFileName);
                                                    }

                                                    FindClose(hFind);
                                                }
                                            }

                                            if(PathFileExists(szFilePath))
                                            {
                                                BrowseForFile(szFilePath);
                                            }
                                        }
                                    }

                                    CloseHandle(pi.hThread);
                                    CloseHandle(pi.hProcess);
                                }
                                else
                                {
                                    MessageBox(hwndStub, TEXT("�޷�����Explorer��"), NULL, MB_ICONERROR | MB_TOPMOST);

                                    break;
                                }
                            }
                            while(FALSE);
                        }
                    }

                    LocalFree(ppArgv);
                }
            }

            HeapFree(GetProcessHeap(), 0, szBuffer);
        }
    }
}

BOOL IsExplorer()
{
    TCHAR szExePath[MAX_PATH] = TEXT("");

    GetModuleFileName(GetModuleHandle(NULL), szExePath, MAX_PATH);

    return lstrcmpi(PathFindFileName(szExePath), TEXT("explorer.exe")) == 0;
}

BOOL TryUnescapeFileName(LPCTSTR lpFilePath, TCHAR szUnescapedFilePath[], int nSize)
{
    BOOL bChanged = FALSE;

    if(lstrlen(lpFilePath) + 1 > nSize)
    {
        return bChanged;
    }

    lstrcpyn(szUnescapedFilePath, lpFilePath, nSize);

    // win7
    // (.+?\\)++.+?(?<kill>( \- ����(( \(([1-9][0-9]{2}|[1-9][0-9]|[1-9])\))|))+)\..+
    // xp
    // (.+?\\)++(?<kill>����( \(([1-9][0-9]|[1-9])\))? )[^\.\\\n]+\.[^\.\\\n]+
    // ��ǰ��(����)��[����]
    // (.+?\\)++.+?((?<kill>\(([1-9][0-9]|[1-9])\))|(?<kill>\[([1-9][0-9]|[1-9])\]))\..+
    static CRegexpT<TCHAR> regWin7    (TEXT("(.+?\\\\)++.+?(?<kill>( \\- ����(( \\(([1-9][0-9]{2}|[1-9][0-9]|[1-9])\\))|))+)\\..+"));
    static CRegexpT<TCHAR> regXp      (TEXT("(.+?\\\\)++(?<kill>����( \\(([1-9][0-9]|[1-9])\\))? )[^\\.\\\\\\n]+\\.[^\\.\\\\\\n]+"));
    static CRegexpT<TCHAR> regDownload(TEXT("(.+?\\\\)++.+?((?<kill>\\(([1-9][0-9]|[1-9])\\))|(?<kill>\\[([1-9][0-9]|[1-9])\\]))\\..+"));

    static int nGroupKillWin7     = regWin7.    GetNamedGroupNumber(TEXT("kill"));
    static int nGroupKillXp       = regXp.      GetNamedGroupNumber(TEXT("kill"));
    static int nGroupKillDownload = regDownload.GetNamedGroupNumber(TEXT("kill"));

    static CRegexpT<TCHAR>* pRegs[] = {
        &regWin7,
        &regXp,
        &regDownload,
        };
    static int pGroupKills[] = {
        nGroupKillWin7,
        nGroupKillXp,
        nGroupKillDownload,
    };

    for(int nIndex = 0; nIndex < sizeof(pRegs) / sizeof(pRegs[0]); nIndex += 1)
    {
        while(TRUE)
        {
            MatchResult result = pRegs[nIndex]->Match(szUnescapedFilePath);

            if(result.IsMatched())
            {
                int nGroupBegin = result.GetGroupStart(pGroupKills[nIndex]);
                int nGroupEnd = result.GetGroupEnd(pGroupKills[nIndex]);

                if(nGroupBegin >= 0 && nGroupEnd > nGroupBegin)
                {
                    MoveMemory(
                        szUnescapedFilePath + nGroupBegin,
                        szUnescapedFilePath + nGroupEnd,
                        (lstrlen(szUnescapedFilePath + nGroupEnd) + 1) * sizeof(TCHAR)
                        );

                    bChanged = TRUE;
                }
            }
            else
            {
                break;
            }
        }
    }

    TCHAR szTailMark[] = TEXT(".������");
    int nResultLength = lstrlen(szUnescapedFilePath);
    int nMarkLength = lstrlen(szTailMark);

    if(nResultLength > nMarkLength)
    {
        if(lstrcmp(szUnescapedFilePath + nResultLength - nMarkLength, szTailMark) == 0)
        {
            *(szUnescapedFilePath + nResultLength - nMarkLength) = TEXT('\0');

            bChanged = TRUE;
        }
    }

    return bChanged;
}

BOOL ResolveShortcut(LPCTSTR lpLinkFilePath, TCHAR szResolvedPath[], UINT nSize)
{
    WCHAR szLinkFilePath[MAX_PATH];
    IShellLink *pShellLink = NULL;
    IPersistFile *pPersistFile = NULL;
    WIN32_FIND_DATA wfd;
    BOOL bResult = FALSE;

    if(PathFileExists(lpLinkFilePath))
    {
#ifdef UNICODE
        wnsprintfW(szLinkFilePath, sizeof(szLinkFilePath) / sizeof(WCHAR), L"%s", lpLinkFilePath);
#else
        wnsprintfW(szLinkFilePath, sizeof(szLinkFilePath) / sizeof(WCHAR), L"%S", lpLinkFilePath);
#endif

        if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink)))
        {
            if(SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile)))
            {
                if(SUCCEEDED(pPersistFile->Load(szLinkFilePath, STGM_READ)))
                {
                    bResult = SUCCEEDED(pShellLink->GetPath(szResolvedPath, nSize, &wfd, 0));
                }

                pPersistFile->Release();
            }

            pShellLink->Release();
        }
    }

    return bResult;
} 

void WINAPI T2(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    const TCHAR *sz[] = {
        TEXT("D:\\����\\�½��ļ���\\���� aaa.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� a(88).aa.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� aa[5].a.txt"),
        TEXT("D:\\����\\�½��ļ���\\������ - ����.txt"),
        TEXT("D:\\����\\�½��ļ���\\������ - ���� (10).txt"),
        TEXT("D:\\����\\�½��ļ���\\������ - ���� (8).txt"),
        TEXT("D:\\����\\�½��ļ���\\������ - ���� (7) - ����.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� (14) aa a.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� (14) aaa.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� (15) aa a.txt"),
        TEXT("D:\\����\\�½�(8).�ļ���\\���� (15) aaa.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� (16) aa a.txt"),
        TEXT("D:\\����\\�½��ļ���\\���� (26) aaa.txt"),
    };
    TCHAR szO[MAX_PATH];

    for(int nIndex = 0; nIndex < sizeof(sz) / sizeof(sz[0]); nIndex += 1)
    {
        if(TryUnescapeFileName(sz[nIndex], szO, MAX_PATH))
        {
            TCHAR szText[1000];

            wsprintf(szText, TEXT("%lu %s -> %s\r\n"), nIndex, sz[nIndex], szO);

            OutputDebugString(szText);
        }
    }
}