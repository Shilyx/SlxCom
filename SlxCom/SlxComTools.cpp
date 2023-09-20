#include "SlxComTools.h"
#include <wintrust.h>
#include <Softpub.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <WinTrust.h>
#include <ImageHlp.h>
#include "resource.h"
#include "lib/deelx.h"
#include <TlHelp32.h>
#include <PsApi.h>
#include <shlobj.h>
#pragma warning(disable: 4786)
#include <set>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "lib/charconv.h"
#include "lib/DummyWinTrustDll.h"
#include "SlxElevateBridge.h"

using namespace std;

#pragma comment(lib, "Wintrust.lib")
#pragma comment(lib, "PsApi.lib")
#pragma comment(lib, "ImageHlp.lib")

extern HINSTANCE g_hinstDll;    // SlxCom.cpp
extern BOOL g_bVistaLater;      // SlxCom.cpp
extern BOOL g_bXPLater;         // SlxCom.cpp
extern BOOL g_bElevated;        // SlxCom.cpp

DWORD SHSetTempValue(HKEY hRootKey, LPCWSTR pszSubKey, LPCWSTR pszValue, DWORD dwType, LPCVOID pvData, DWORD cbData) {
    HKEY hKey = NULL;
    DWORD dwRet = RegCreateKeyExW(hRootKey, pszSubKey, 0, NULL, REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if (ERROR_SUCCESS == dwRet) {
        dwRet = RegSetValueExW(hKey, pszValue, 0, dwType, (const unsigned char *)pvData, cbData);

        RegCloseKey(hKey);
    }

    return dwRet;
}

BOOL IsFileSignedBySelf(LPCWSTR lpFilePath) {
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

BOOL GetFileHash(LPCWSTR lpFilePath, BYTE btHash[], DWORD *pcbSize) {
    BOOL bResult = FALSE;
    HANDLE hFile = CreateFileW(
        lpFilePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
        );

    if (hFile != INVALID_HANDLE_VALUE) {
        __try {
            if (CryptCATAdminCalcHashFromFileHandle(hFile, pcbSize, btHash, 0)) {
                bResult = TRUE;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }

        CloseHandle(hFile);
    }

    return bResult;
}

BOOL IsFileSignedByCatlog(LPCWSTR lpFilePath) {
    BOOL bResult = FALSE;
    BYTE btHash[100];
    DWORD dwHashSize = sizeof(btHash);

    if (GetFileHash(lpFilePath, btHash, &dwHashSize)) {
        HANDLE hCatHandle = NULL;

        CryptCATAdminAcquireContext(&hCatHandle, NULL, 0);

        if (hCatHandle != NULL) {
            __try {
                HANDLE hCatalogContext = CryptCATAdminEnumCatalogFromHash(hCatHandle, btHash, dwHashSize, 0, NULL);

                if (hCatalogContext != NULL) {
                    CryptCATAdminReleaseCatalogContext(hCatHandle, hCatalogContext, 0);

                    bResult = TRUE;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) { }

            CryptCATAdminReleaseContext(hCatHandle, 0);
        }
    }

    return bResult;
}

BOOL IsFileSigned(LPCWSTR lpFilePath) {
    return IsFileSignedBySelf(lpFilePath) || IsFileSignedByCatlog(lpFilePath);
}

BOOL IsFileHasSignatureArea(LPCWSTR lpFilePath) {
    BOOL bRet = FALSE;
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwCertCount = 0;
        if (ImageEnumerateCertificates(hFile, CERT_SECTION_TYPE_ANY, &dwCertCount, NULL, 0) && dwCertCount > 0) {
            bRet = TRUE;
        }

        CloseHandle(hFile);
    }

    return bRet;
}

BOOL RemoveFileSignatrueArea(LPCWSTR lpFilePath) {
    if (!IsFileHasSignatureArea(lpFilePath)) {
        return FALSE;
    }

    DWORD dwCertCount = 0;
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    if (!ImageEnumerateCertificates(hFile, CERT_SECTION_TYPE_ANY, &dwCertCount, NULL, 0) || dwCertCount == 0) {
        CloseHandle(hFile);
        SetLastError(ERROR_EMPTY);
        return FALSE;
    }

    for (DWORD i = 0; i < dwCertCount; i++) {
        if (!ImageRemoveCertificate(hFile, i)) {
            CloseHandle(hFile);
            return FALSE;
        }
    }

    CloseHandle(hFile);
    return TRUE;
}

BOOL SaveResourceToFile(LPCWSTR lpResType, LPCWSTR lpResName, LPCWSTR lpFilePath) {
    BOOL bSucceed = FALSE;
    DWORD dwResSize = 0;

    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        HRSRC hRsrc = FindResourceW(g_hinstDll, lpResName, lpResType);

        if (hRsrc != NULL) {
            HGLOBAL hGlobal = LoadResource(g_hinstDll, hRsrc);

            if (hGlobal != NULL) {
                LPCSTR lpBuffer = (LPCSTR)LockResource(hGlobal);
                dwResSize = SizeofResource(g_hinstDll, hRsrc);

                if (lpBuffer != NULL && dwResSize > 0) {
                    bSucceed = WriteFileHelper(hFile, lpBuffer, dwResSize);
                }
            }
        }

        CloseHandle(hFile);
    }

    return bSucceed;
}

BOOL GetResultPath(LPCWSTR lpRarPath, LPWSTR lpResultPath, int nLength) {
    if (nLength >= MAX_PATH) {
        WCHAR szJpgPath[MAX_PATH];

        lstrcpynW(szJpgPath, lpRarPath, MAX_PATH);
        PathRenameExtensionW(szJpgPath, L".jpg");

        return PathYetAnotherMakeUniqueName(lpResultPath, szJpgPath, NULL, NULL);
    }

    return FALSE;
}

BOOL DoCombine(LPCWSTR lpFile1, LPCWSTR lpFile2, LPCWSTR lpFileNew) {
    IStream *pStream1 = NULL;
    IStream *pStream2 = NULL;
    IStream *pStreamNew = NULL;

    if (S_OK == SHCreateStreamOnFileW(lpFile1, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream1)) {
        if (S_OK == SHCreateStreamOnFileW(lpFile2, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream2)) {
            if (S_OK == SHCreateStreamOnFileW(lpFileNew, STGM_WRITE | STGM_SHARE_DENY_NONE | STGM_CREATE, &pStreamNew)) {
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

BOOL CombineFile(LPCWSTR lpRarPath, LPCWSTR lpJpgPath, LPWSTR lpResultPath, int nLength) {
    //Get jpg file path
    WCHAR szJpgPath[MAX_PATH];

    if (GetFileAttributesW(lpJpgPath) == INVALID_FILE_ATTRIBUTES) {
        GetModuleFileNameW(g_hinstDll, szJpgPath, MAX_PATH);
        PathAppendW(szJpgPath, L"\\..\\Default.jpg");

        if (GetFileAttributesW(szJpgPath) == INVALID_FILE_ATTRIBUTES) {
            SaveResourceToFile(L"RT_FILE", MAKEINTRESOURCEW(IDR_DEFAULTJPG), szJpgPath);
        }
    } else {
        lstrcpynW(szJpgPath, lpJpgPath, MAX_PATH);
    }

    if (GetFileAttributesW(szJpgPath) != INVALID_FILE_ATTRIBUTES) {
        if (GetResultPath(lpRarPath, lpResultPath, nLength)) {
            return DoCombine(szJpgPath, lpRarPath, lpResultPath);
        }
    }

    return FALSE;
}

BOOL SetClipboardText(LPCWSTR lpText) {
    BOOL bResult = FALSE;
    HGLOBAL hGlobal = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, (lstrlenW(lpText) + 1) * sizeof(WCHAR));

    if (hGlobal != NULL) {
        LPWSTR lpBuffer = (LPWSTR)GlobalLock(hGlobal);

        if (lpBuffer != NULL) {
            lstrcpyW(lpBuffer, lpText);

            GlobalUnlock(hGlobal);

            if (OpenClipboard(NULL)) {
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

    if (!bResult && hGlobal != NULL) {
        GlobalFree(hGlobal);
    }

    return bResult;
}

BOOL RunCommand(LPWSTR lpCommandLine, LPCWSTR lpCurrentDirectory) {
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    si.wShowWindow = SW_SHOW;   //no use

    if (CreateProcessW(NULL, lpCommandLine, NULL, NULL, FALSE, 0, NULL, lpCurrentDirectory, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK RunCommandWithArgumentsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HICON hIcon = NULL;

    switch(uMsg) {
    case WM_INITDIALOG:
        if (hIcon == NULL) {
            hIcon = LoadIconW(g_hinstDll, MAKEINTRESOURCEW(IDI_CONFIG_ICON));
        }

        if (hIcon != NULL) {
            SendMessageW(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        SetDlgItemTextW(hwndDlg, IDC_FILE, (LPCWSTR)lParam);
        return FALSE;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE) {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_RUN) {
            if (IDYES == MessageBoxW(hwndDlg, L"确定要尝试将此文件作为可执行文件运行吗？", L"请确认", MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3)) {
                BOOL bSucceed = FALSE;
                INT_PTR nLength = 0;
                INT_PTR nOffset = 0;

                nLength += SendDlgItemMessageW(hwndDlg, IDC_FILE, WM_GETTEXTLENGTH, 0, 0);
                nLength += SendDlgItemMessageW(hwndDlg, IDC_ARGUMENTS, WM_GETTEXTLENGTH, 0, 0);
                nLength += 200;

                WCHAR *szCommand = new WCHAR[nLength];

                if (szCommand != NULL) {
                    lstrcpyW(szCommand, L"\"");
                    nOffset += 1;

                    nOffset += SendDlgItemMessageW(hwndDlg, IDC_FILE, WM_GETTEXT, nLength - nOffset, (LPARAM)(szCommand + nOffset));

                    lstrcatW(szCommand, L"\" ");
                    nOffset += 2;

                    SendDlgItemMessageW(hwndDlg, IDC_ARGUMENTS, WM_GETTEXT, nLength - nOffset, (LPARAM)(szCommand + nOffset));

                    bSucceed = RunCommand(szCommand);

                    delete []szCommand;
                }

                if (bSucceed) {
                    EndDialog(hwndDlg, 1);
                } else {
                    WCHAR szErrorMessage[200];

                    wnsprintfW(szErrorMessage, RTL_NUMBER_OF(szErrorMessage), L"无法启动进程，错误码%lu", GetLastError());

                    MessageBoxW(hwndDlg, szErrorMessage, NULL, MB_ICONERROR);

                    SendDlgItemMessageW(hwndDlg, IDC_FILE, EM_SETSEL, 0, -1);
                    SendDlgItemMessageW(hwndDlg, IDC_FILE, EM_SETREADONLY, FALSE, 0);
                    SetFocus(GetDlgItem(hwndDlg, IDC_FILE));
                }
            } else {
                SendDlgItemMessageW(hwndDlg, IDC_ARGUMENTS, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwndDlg, IDC_ARGUMENTS));
            }
        } else if (LOWORD(wParam) == IDC_CANCEL) {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (INT_PTR)GetStockObject(WHITE_BRUSH);

    default:
        break;
    }

    return FALSE;
}

BOOL RunCommandWithArguments(LPCWSTR lpFile) {
    return 0 != DialogBoxParamW(g_hinstDll, MAKEINTRESOURCEW(IDD_RUNWITHARGUMENTS), NULL, RunCommandWithArgumentsProc, (LPARAM)lpFile);
}

BOOL RunCommandEx(LPCWSTR lpApplication, LPCWSTR lpArguments, LPCWSTR lpCurrentDirectory, BOOL bElevate) {
    return (int)(INT_PTR)ShellExecuteW(
        NULL,
        bElevate ? L"runas" : L"open",
        lpApplication,
        lpArguments,
        lpCurrentDirectory,
        SW_SHOW) > 32;
}

struct BrowserForRegPathParam {
    HANDLE hProcess;
    HWND hRegEdit;
    HWND hTreeCtrl;
    LPCWSTR lpRegPath;
};

DWORD CALLBACK BrowserForRegPathProc(LPVOID lpParam) {
    BrowserForRegPathParam *pParam = (BrowserForRegPathParam *)lpParam;

    SendMessageW(pParam->hRegEdit, WM_COMMAND, 0x10288, 0);
    WaitForInputIdle(pParam->hProcess, INFINITE);

    for (int i = 0; i < 40; i += 1) {
        SendMessageW(pParam->hTreeCtrl, WM_KEYDOWN, VK_LEFT, 0);
        WaitForInputIdle(pParam->hProcess, INFINITE);
    }

    SendMessageW(pParam->hTreeCtrl, WM_KEYDOWN, VK_RIGHT, 0);
    WaitForInputIdle(pParam->hProcess, INFINITE);

    LPCWSTR lpChar = pParam->lpRegPath;
    while (*lpChar != L'\0') {
        if (*lpChar == L'\\') {
            SendMessageW(pParam->hTreeCtrl, WM_KEYDOWN, VK_RIGHT, 0);
        } else {
            SendMessageW(pParam->hTreeCtrl, WM_CHAR, *lpChar, 0);
        }

        WaitForInputIdle(pParam->hProcess, INFINITE);
        lpChar += 1;
    }

    return 0;
}

BOOL BrowseForRegPathInProcess(LPCWSTR lpRegPath) {
    if (lpRegPath == NULL || *lpRegPath == L'\0') {
        return FALSE;
    }

    HWND hRegEdit = NULL;
    DWORD dwRegEditProcessId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != NULL && hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32 = {sizeof(pe32)};

        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ParentProcessID == GetCurrentProcessId()) {
                    if (0 == lstrcmpiW(pe32.szExeFile, L"regedit.exe")) {
                        dwRegEditProcessId = pe32.th32ProcessID;
                        break;
                    }
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
    }

    if (dwRegEditProcessId == 0) {
        WCHAR szCommand[] = L"regedit -m";
        PROCESS_INFORMATION pi;
        STARTUPINFOW si = {sizeof(si)};

        if (CreateProcessW(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            WaitForInputIdle(pi.hProcess, 2212);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            dwRegEditProcessId = pi.dwProcessId;
        }
    }

    if (dwRegEditProcessId != 0) {
        LPCWSTR lpClassName = L"RegEdit_RegEdit";
        HWND hWindow = FindWindowExW(NULL, NULL, lpClassName, NULL);

        while (IsWindow(hWindow)) {
            DWORD dwProcessId = 0;

            GetWindowThreadProcessId(hWindow, &dwProcessId);

            if (dwProcessId == dwRegEditProcessId) {
                hRegEdit = hWindow;
                break;
            }

            hWindow = FindWindowExW(NULL, hWindow, lpClassName, NULL);
        }
    }

    if (IsWindow(hRegEdit)) {
        HWND hTreeCtrl = FindWindowExW(hRegEdit, NULL, L"SysTreeView32", NULL);

        if (IsWindow(hTreeCtrl)) {
            BrowserForRegPathParam param;

            param.hProcess = OpenProcess(SYNCHRONIZE, FALSE, dwRegEditProcessId);
            param.hRegEdit = hRegEdit;
            param.hTreeCtrl = hTreeCtrl;
            param.lpRegPath = lpRegPath;

            if (param.hProcess != NULL) {
                if (IsIconic(hRegEdit)) {
                    ShowWindow(hRegEdit, SW_RESTORE);
                }

                BringWindowToTop(hRegEdit);

                HANDLE hThread = CreateThread(NULL, 0, BrowserForRegPathProc, (LPVOID)&param, 0, NULL);

                if (WAIT_TIMEOUT == WaitForSingleObject(hThread, 9876)) {
                    TerminateThread(hThread, 0);
                }

                CloseHandle(hThread);
                CloseHandle(param.hProcess);
            }
        }
    }

    return TRUE;
}

void WINAPI SlxBrowseForRegPathW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow) {
    BrowseForRegPathInProcess(WtoW(lpszCmdLine).c_str());
}

BOOL BrowseForRegPath(LPCWSTR lpRegPath) {
    if (g_bVistaLater && !g_bElevated) {
        WCHAR szRundll32[MAX_PATH] = L"";
        WCHAR szDllPath[MAX_PATH] = L"";
        WCHAR szArguments[MAX_PATH + 20] = L"";

        GetSystemDirectoryW(szRundll32, RTL_NUMBER_OF(szRundll32));
        PathAppendW(szRundll32, L"\\rundll32.exe");

        GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
        PathQuoteSpacesW(szDllPath);

        wnsprintfW(szArguments, RTL_NUMBER_OF(szArguments), L"%s SlxBrowseForRegPath %s", szDllPath, lpRegPath);

        return ElevateAndRunW(szRundll32, szArguments, NULL, SW_SHOW);
    } else {
        return BrowseForRegPathInProcess(lpRegPath);
    }
}

BOOL SHOpenFolderAndSelectItems(LPCWSTR lpFile) {
    typedef HRESULT (__stdcall *PSHOpenFolderAndSelectItems)(LPCITEMIDLIST, UINT, LPCITEMIDLIST *, DWORD);

    BOOL bResult = FALSE;
    LPCWSTR lpDllName = L"Shell32.dll";
    HMODULE hShell32 = GetModuleHandleW(lpDllName);
    PSHOpenFolderAndSelectItems pSHOpenFolderAndSelectItems;

    if (hShell32 == NULL) {
        hShell32 = LoadLibraryW(lpDllName);
    }

    if (hShell32 == NULL) {
        return bResult;
    }

    (PROC&)pSHOpenFolderAndSelectItems = GetProcAddress(hShell32, "SHOpenFolderAndSelectItems");

    if (pSHOpenFolderAndSelectItems != NULL) {
        IShellFolder* pDesktopFolder = NULL;

        if (SUCCEEDED(SHGetDesktopFolder(&pDesktopFolder))) {
            LPCITEMIDLIST pidlFile = NULL;
            LPITEMIDLIST pidl = NULL;
            WCHAR szFile[MAX_PATH] = {0};

            lstrcpynW(szFile, WtoW(lpFile).c_str(), RTL_NUMBER_OF(szFile));

            if (SUCCEEDED(pDesktopFolder->ParseDisplayName(NULL, NULL, szFile, NULL, &pidl, NULL))) {
                pidlFile = pidl;

                int nTryCount = 0;
                while (!IsWindow(GetTrayNotifyWndInProcess())) {
                    Sleep(107);
                    nTryCount += 1;

                    if (nTryCount > 17) {
                        break;
                    }
                }

                CoInitialize(NULL);
                bResult = SUCCEEDED(pSHOpenFolderAndSelectItems(pidlFile, 0, NULL, 0));
            }

            if (pidlFile != NULL) {
                CoTaskMemFree((void*)pidlFile);
            }

            pDesktopFolder->Release();
        }
    }

    return bResult;
}

BOOL BrowseForFile(LPCWSTR lpFile) {
    if (SHOpenFolderAndSelectItems(lpFile)) {
        return TRUE;
    } else {
        WCHAR szExplorerPath[MAX_PATH];
        WCHAR szCommandLine[MAX_PATH * 2 + 100];

        GetWindowsDirectoryW(szExplorerPath, MAX_PATH);
        PathAppendW(szExplorerPath, L"\\Explorer.exe");
        wnsprintfW(szCommandLine, RTL_NUMBER_OF(szCommandLine), L"%s /n,/select,\"%s\"", szExplorerPath, lpFile);

        OutputDebugStringW(L"BrowseForFile by starting new explorer.exe\r\n");

        return RunCommand(szCommandLine, NULL);
    }
}

// void WINAPI ResetExplorer(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
// {
//     int nCmdLineLength = lstrlenA(lpszCmdLine);
//
//     if (nCmdLineLength > 0)
//     {
//         WCHAR *szBuffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (nCmdLineLength + 100) * sizeof(WCHAR));
//
//         if (szBuffer != NULL)
//         {
//             if (wnsprintfW(szBuffer, (nCmdLineLength + 100), L"%S", lpszCmdLine) > 0)
//             {
//                 int nArgc = 0;
//                 LPWSTR *ppArgv = CommandLineToArgvW(szBuffer, &nArgc);
//
//                 if (ppArgv != NULL)
//                 {
//                     if (nArgc >= 1)
//                     {
//                         DWORD dwProcessId = StrToIntW(ppArgv[0]);
//
//                         if (dwProcessId != 0)
//                         {
//                             do
//                             {
//                                 HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, dwProcessId);
//
//                                 if (hProcess == NULL)
//                                 {
//                                     MessageBoxW(hwndStub, L"打开进程失败！", NULL, MB_ICONERROR | MB_TOPMOST);
//
//                                     break;
//                                 }
//
//                                 TerminateProcess(hProcess, 0);
//
//                                 WaitForSingleObject(hProcess, 5000);
//
//                                 DWORD dwExitCode = 0;
//                                 GetExitCodeProcess(hProcess, &dwExitCode);
//
//                                 if (dwExitCode == STILL_ACTIVE)
//                                 {
//                                     MessageBoxW(hwndStub, L"无法终止进程！", NULL, MB_ICONERROR | MB_TOPMOST);
//
//                                     break;
//                                 }
//
//                                 CloseHandle(hProcess);
//
//                                 STARTUPINFOW si = {sizeof(si)};
//                                 PROCESS_INFORMATION pi;
//                                 WCHAR szExplorerPath[MAX_PATH];
//
//                                 GetWindowsDirectoryW(szExplorerPath, MAX_PATH);
//                                 PathAppendW(szExplorerPath, L"\\Explorer.exe");
//
//                                 if (CreateProcessW(NULL, szExplorerPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
//                                 {
//                                     if (nArgc >= 2 && lstrlenW(ppArgv[1]) < MAX_PATH)
//                                     {
//                                         WaitForInputIdle(pi.hProcess, 10 * 1000);
//
//                                         WCHAR szFilePath[MAX_PATH];
//                                         DWORD dwFileAttributes = INVALID_FILE_ATTRIBUTES;
//
//                                         wnsprintfW(szFilePath, RTL_NUMBER_OF(szFilePath), L"%ls", ppArgv[1]);
//
//                                         dwFileAttributes = GetFileAttributesW(szFilePath);
//
//                                         if (dwFileAttributes != INVALID_FILE_ATTRIBUTES)
//                                         {
//                                             if (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
//                                             {
//                                                 WCHAR szFindMark[MAX_PATH];
//                                                 WIN32_FIND_DATAW wfd;
//
//                                                 lstrcpynW(szFindMark, szFilePath, MAX_PATH);
//                                                 PathAppendW(szFindMark, L"\\*");
//
//                                                 HANDLE hFind = FindFirstFileW(szFindMark, &wfd);
//
//                                                 if (hFind != INVALID_HANDLE_VALUE)
//                                                 {
//                                                     do
//                                                     {
//                                                         if (lstrcmpW(wfd.cFileName, L".") != 0 && lstrcmpW(wfd.cFileName, L"..") != 0)
//                                                         {
//                                                             break;
//                                                         }
//
//                                                     } while (FindNextFileW(hFind, &wfd));
//
//                                                     if (lstrcmpW(wfd.cFileName, L".") != 0 && lstrcmpW(wfd.cFileName, L"..") != 0)
//                                                     {
//                                                         PathAddBackslash(szFilePath);
//                                                         lstrcatW(szFilePath, wfd.cFileName);
//                                                     }
//
//                                                     FindClose(hFind);
//                                                 }
//                                             }
//
//                                             if (PathFileExistsW(szFilePath))
//                                             {
//                                                 BrowseForFile(szFilePath);
//                                             }
//                                         }
//                                     }
//
//                                     CloseHandle(pi.hThread);
//                                     CloseHandle(pi.hProcess);
//                                 }
//                                 else
//                                 {
//                                     MessageBoxW(hwndStub, L"无法启动Explorer！", NULL, MB_ICONERROR | MB_TOPMOST);
//
//                                     break;
//                                 }
//                             }
//                             while (FALSE);
//                         }
//                     }
//
//                     LocalFree(ppArgv);
//                 }
//             }
//
//             HeapFree(GetProcessHeap(), 0, szBuffer);
//         }
//     }
// }

BOOL IsExplorer() {
    WCHAR szExePath[MAX_PATH] = L"";

    GetModuleFileNameW(GetModuleHandleW(NULL), szExePath, MAX_PATH);

    return lstrcmpiW(PathFindFileNameW(szExePath), L"explorer.exe") == 0;
}

BOOL TryUnescapeFileName(LPCWSTR lpFilePath, WCHAR szUnescapedFilePath[], int nSize) {
    BOOL bChanged = FALSE;

    if (lstrlenW(lpFilePath) + 1 > nSize) {
        return bChanged;
    }

    lstrcpynW(szUnescapedFilePath, lpFilePath, nSize);

    // win7
    // (.+?\\)++.+?(?<kill>( \- 副本(( \(([1-9][0-9]{2}|[1-9][0-9]|[1-9])\))|))+)\..+
    // xp
    // (.+?\\)++(?<kill>复件( \(([1-9][0-9]|[1-9])\))? )[^\.\\\n]+\.[^\.\\\n]+
    // 点前加(数字)或[数字]
    // (.+?\\)++.+?((?<kill> *\(([1-9][0-9]|[1-9])\))|(?<kill> *\[([1-9][0-9]|[1-9])\]))\..+
    static CRegexpT<WCHAR> regWin7    (L"(.+?\\\\)++.+?(?<kill>( \\- 副本(( \\(([1-9][0-9]{2}|[1-9][0-9]|[1-9])\\))|))+)\\..+");
    static CRegexpT<WCHAR> regXp      (L"(.+?\\\\)++(?<kill>复件( \\(([1-9][0-9]|[1-9])\\))? )[^\\.\\\\\\n]+\\.[^\\.\\\\\\n]+");
    static CRegexpT<WCHAR> regDownload(L"(.+?\\\\)++.+?((?<kill> *\\(([1-9][0-9]|[1-9])\\))|(?<kill> *\\[([1-9][0-9]|[1-9])\\]))\\..+");

    static int nGroupKillWin7     = regWin7.    GetNamedGroupNumber(L"kill");
    static int nGroupKillXp       = regXp.      GetNamedGroupNumber(L"kill");
    static int nGroupKillDownload = regDownload.GetNamedGroupNumber(L"kill");

    static CRegexpT<WCHAR>* pRegs[] = {
        &regWin7,
        &regXp,
        &regDownload,
        };
    static int pGroupKills[] = {
        nGroupKillWin7,
        nGroupKillXp,
        nGroupKillDownload,
    };

    for (int nIndex = 0; nIndex < sizeof(pRegs) / sizeof(pRegs[0]); nIndex += 1) {
        while (TRUE) {
            MatchResult result = pRegs[nIndex]->Match(szUnescapedFilePath);

            if (result.IsMatched()) {
                int nGroupBegin = result.GetGroupStart(pGroupKills[nIndex]);
                int nGroupEnd = result.GetGroupEnd(pGroupKills[nIndex]);

                if (nGroupBegin >= 0 && nGroupEnd > nGroupBegin) {
                    MoveMemory(
                        szUnescapedFilePath + nGroupBegin,
                        szUnescapedFilePath + nGroupEnd,
                        (lstrlenW(szUnescapedFilePath + nGroupEnd) + 1) * sizeof(WCHAR)
                        );

                    bChanged = TRUE;
                }
            } else {
                break;
            }
        }
    }

    WCHAR szTailMark[] = L".重命名";
    int nResultLength = lstrlenW(szUnescapedFilePath);
    int nMarkLength = lstrlenW(szTailMark);

    if (nResultLength > nMarkLength) {
        if (lstrcmpW(szUnescapedFilePath + nResultLength - nMarkLength, szTailMark) == 0) {
            *(szUnescapedFilePath + nResultLength - nMarkLength) = L'\0';

            bChanged = TRUE;
        }
    }

    return bChanged;
}

BOOL ResolveShortcut(LPCWSTR lpLinkFilePath, WCHAR szResolvedPath[], UINT nSize) {
    WCHAR szLinkFilePath[MAX_PATH];
    IShellLinkW *pShellLink = NULL;
    IPersistFile *pPersistFile = NULL;
    WIN32_FIND_DATAW wfd;
    BOOL bResult = FALSE;

    if (PathFileExistsW(lpLinkFilePath)) {
        wnsprintfW(szLinkFilePath, RTL_NUMBER_OF(szLinkFilePath), L"%ls", lpLinkFilePath);

        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink))) {
            if (SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile))) {
                if (SUCCEEDED(pPersistFile->Load(szLinkFilePath, STGM_READ))) {
                    bResult = SUCCEEDED(pShellLink->GetPath(szResolvedPath, nSize, &wfd, 0));
                }

                pPersistFile->Release();
            }

            pShellLink->Release();
        }
    }

    return bResult;
}

VOID ShowErrorMessage(HWND hWindow, DWORD dwErrorCode) {
    LPWSTR lpMessage = NULL;
    WCHAR szMessage[2000];
    UINT uType = MB_ICONERROR;

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMessage,
        0,
        NULL
        );

    if (lpMessage != NULL) {
        wnsprintfW(
            szMessage,
            RTL_NUMBER_OF(szMessage),
            L"错误码：%lu\r\n\r\n参考意义：%s",
            dwErrorCode,
            lpMessage
            );

        LocalFree(lpMessage);
    }

    if (dwErrorCode == 0) {
        uType = MB_ICONINFORMATION;
    }

    MessageBoxW(hWindow, szMessage, L"信息", uType);
}

struct AutoCloseMessageBoxParam {
    DWORD dwThreadIdToCheck;
    int nCloseAfterSeconds;
};

static BOOL CALLBACK EnumMessageBoxProc(HWND hwnd, LPARAM lParam) {
    HWND &hTargetWnd = *(HWND *)lParam;
    WCHAR szClassName[128] = L"";

    GetClassNameW(hwnd, szClassName, RTL_NUMBER_OF(szClassName));

    if (lstrcmpiW(szClassName, L"#32770") == 0 &&
        IsWindow(FindWindowExW(hwnd, NULL, L"Static", NULL)) &&
        IsWindow(FindWindowExW(hwnd, NULL, L"Button", NULL))) {
        hTargetWnd = hwnd;
        return FALSE;
    }

    return TRUE;
}

static DWORD CALLBACK AutoCloseMessageBoxProc(LPVOID lpParam) {
    AutoCloseMessageBoxParam param = *(AutoCloseMessageBoxParam *)lpParam;

    delete (AutoCloseMessageBoxParam *)lpParam;

    HWND hTargetWnd = NULL;
    WCHAR szOldText[128];
    WCHAR szNewText[128];

    // 最多检测30秒，30秒内仍没有MessageBox弹出，将不再检测
    for (int i = 0; i < 3 * 30; ++i) {
        EnumThreadWindows(param.dwThreadIdToCheck, EnumMessageBoxProc, (LPARAM)&hTargetWnd);

        if (IsWindow(hTargetWnd)) {
            GetWindowTextW(hTargetWnd, szOldText, RTL_NUMBER_OF(szOldText));

            if (lstrlenW(szOldText) > 4) {
                lstrcpynW(szOldText + 4, L"...", 10);
            }

            break;
        }

        Sleep(333);
    }

    for (int i = param.nCloseAfterSeconds; i > 0 && IsWindow(hTargetWnd); --i) {
        if (i > 60) {
            wnsprintfW(szNewText, RTL_NUMBER_OF(szNewText), L"%s 约%d分钟后自动关闭", szOldText, i / 60);
        } else {
            wnsprintfW(szNewText, RTL_NUMBER_OF(szNewText), L"%s %d秒后自动关闭", szOldText, i);
        }

        SetWindowTextW(hTargetWnd, szNewText);

        Sleep(1000);
    }

    if (IsWindow(hTargetWnd)) {
        SetWindowTextW(hTargetWnd, L"关闭中...");
        EndDialog(hTargetWnd, 0);
    }

    return 0;
}

void AutoCloseMessageBoxForThreadInSeconds(DWORD dwThreadId, int nSeconds) {
    if (nSeconds < 0) {
        nSeconds = 0;
    }

    AutoCloseMessageBoxParam *param = new AutoCloseMessageBoxParam;

    param->dwThreadIdToCheck = dwThreadId;
    param->nCloseAfterSeconds = nSeconds;

    CloseHandle(CreateThread(NULL, 0, AutoCloseMessageBoxProc, (LPVOID)param, 0, NULL));
}

VOID DrvAction(HWND hWindow, LPCWSTR lpFilePath, DRIVER_ACTION daValue) {
    if (daValue != DA_INSTALL   &&
        daValue != DA_UNINSTALL &&
        daValue != DA_START     &&
        daValue != DA_STOP
        ) {
        return;
    }

    SC_HANDLE hService = NULL;
    SC_HANDLE hScManager = OpenSCManager(NULL, NULL, GENERIC_READ | GENERIC_WRITE);

    if (hScManager == NULL) {
        return;
    }

    WCHAR szServiceName[MAX_PATH] = L"_slxcom_";

    StrCatBuffW(szServiceName, PathFindFileNameW(lpFilePath), RTL_NUMBER_OF(szServiceName));
    PathRemoveExtensionW(szServiceName);

    for (int i = 0; i < lstrlenW(szServiceName); i++) {
        if (szServiceName[i] == L' ') {
            szServiceName[i] = L'_';
        }
    }

    if (daValue == DA_INSTALL) {
        hService = CreateServiceW(
            hScManager,
            szServiceName,
            szServiceName,
            SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG,
            SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_IGNORE,
            lpFilePath,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            );
    } else {
        hService = OpenServiceW(hScManager, szServiceName, SERVICE_ALL_ACCESS);

        if (hService != NULL) {
            if (daValue == DA_UNINSTALL) {
                DeleteService(hService);
            } else if (daValue == DA_START) {
                StartService(hService, 0, NULL);
            } else if (daValue == DA_STOP) {
                SERVICE_STATUS status;
                ControlService(hService, SERVICE_CONTROL_STOP, &status);
            }
        }
    }

    ShowErrorMessage(hWindow, GetLastError());

    if (hService != NULL) {
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hScManager);
}

void WINAPI DriverActionW(HWND hwndStub, HINSTANCE hAppInstance, LPWSTR lpszCmdLine, int nCmdShow) {
    int nArgc = 0;
    LPWSTR *lpArgv = CommandLineToArgvW(lpszCmdLine, &nArgc);

    if (nArgc < 2) {
        return;
    }

    AutoCloseMessageBoxForThreadInSeconds(GetCurrentThreadId(), 6);

    if (lstrcmpiW(lpArgv[0], L"install") == 0) {
        DrvAction(hwndStub, lpArgv[1], DA_INSTALL);
    } else if (lstrcmpiW(lpArgv[0], L"start") == 0) {
        DrvAction(hwndStub, lpArgv[1], DA_START);
    } else if (lstrcmpiW(lpArgv[0], L"stop") == 0) {
        DrvAction(hwndStub, lpArgv[1], DA_STOP);
    } else if (lstrcmpiW(lpArgv[0], L"uninstall") == 0) {
        DrvAction(hwndStub, lpArgv[1], DA_UNINSTALL);
    } else {

    }
}

int MessageBoxFormat(HWND hWindow, LPCWSTR lpCaption, UINT uType, LPCWSTR lpFormat, ...) {
    WCHAR szText[2000];
    va_list val;

    va_start(val, lpFormat);
    wvnsprintfW(szText, RTL_NUMBER_OF(szText), lpFormat, val);
    va_end(val);

    return MessageBoxW(hWindow, szText, lpCaption, uType);
}

void WINAPI BrowserLinkFilePosition(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow) {
    WCHAR szLinkFilePath[MAX_PATH];
    WCHAR szTargetFilePath[MAX_PATH];

    CoInitialize(NULL);

    wnsprintfW(szLinkFilePath, RTL_NUMBER_OF(szLinkFilePath), L"%hs", lpszCmdLine);
    PathUnquoteSpacesW(szLinkFilePath);

    if (ResolveShortcut(szLinkFilePath, szTargetFilePath, RTL_NUMBER_OF(szTargetFilePath))) {
        BrowseForFile(szTargetFilePath);
    }
}

BOOL EnableDebugPrivilege(BOOL bEnable) {
    BOOL bResult = FALSE;
    HANDLE hToken;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        TOKEN_PRIVILEGES tp;

        tp.PrivilegeCount = 1;
        LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
        tp.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);

        bResult = GetLastError() == ERROR_SUCCESS;

        CloseHandle(hToken);
    }

    return bResult;
}

BOOL IsFileDenyed(LPCWSTR lpFilePath) {
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD dwLastError = GetLastError();

    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }

    return dwLastError == ERROR_SHARING_VIOLATION;
}

BOOL CloseRemoteHandle(DWORD dwProcessId, HANDLE hRemoteHandle) {
    BOOL bResult = FALSE;
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessId);

    if (hProcess != NULL) {
        HANDLE hLocalHandle = NULL;

        if (DuplicateHandle(
            hProcess,
            hRemoteHandle,
            GetCurrentProcess(),
            &hLocalHandle,
            0,
            FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE
            )) {
            bResult = TRUE;
            CloseHandle(hLocalHandle);
        }

        CloseHandle(hProcess);
    }

    return bResult;
}

BOOL IsSameFilePath(LPCWSTR lpFilePath1, LPCWSTR lpFilePath2) {
    WCHAR szFilePath1[MAX_PATH];
    WCHAR szFilePath2[MAX_PATH];
    WCHAR szLongFilePath1[MAX_PATH];
    WCHAR szLongFilePath2[MAX_PATH];

    lstrcpynW(szFilePath1, lpFilePath1, RTL_NUMBER_OF(szFilePath1));
    lstrcpynW(szFilePath2, lpFilePath2, RTL_NUMBER_OF(szFilePath2));

    if (0 == GetLongPathNameW(szFilePath1, szLongFilePath1, RTL_NUMBER_OF(szLongFilePath1))) {
        lstrcpynW(szLongFilePath1, lpFilePath1, RTL_NUMBER_OF(szLongFilePath1));
    }

    if (0 == GetLongPathNameW(szFilePath2, szLongFilePath2, RTL_NUMBER_OF(szLongFilePath2))) {
        lstrcpynW(szLongFilePath2, lpFilePath2, RTL_NUMBER_OF(szLongFilePath2));
    }

    return lstrcmpiW(szLongFilePath1, szLongFilePath2) == 0;
}

BOOL IsPathDesktop(LPCWSTR lpPath) {
    WCHAR szDesktopPath[MAX_PATH];

    if (SHGetSpecialFolderPathW(NULL, szDesktopPath, CSIDL_DESKTOP, FALSE)) {
        return IsSameFilePath(lpPath, szDesktopPath);
    }

    return FALSE;
}

BOOL IsWow64ProcessHelper(HANDLE hProcess) {
    BOOL bIsWow64 = FALSE;
    BOOL (WINAPI *pIsWow64Process)(HANDLE, PBOOL) = NULL;

    (PROC &)pIsWow64Process = GetProcAddress(
        GetModuleHandleW(L"kernel32"),
        "IsWow64Process"
        );

    if (pIsWow64Process != NULL) {
        pIsWow64Process(hProcess, &bIsWow64);
    }

    return bIsWow64;
}

BOOL DisableWow64FsRedirection() {
    BOOL bResult = TRUE;
    BOOL (WINAPI *pWow64DisableWow64FsRedirection)(PVOID *) = NULL;

    (PROC &)pWow64DisableWow64FsRedirection = GetProcAddress(
        GetModuleHandleW(L"kernel32"),
        "Wow64DisableWow64FsRedirection"
        );

    if (pWow64DisableWow64FsRedirection != NULL) {
        PVOID pOldValue = NULL;
        bResult = pWow64DisableWow64FsRedirection(&pOldValue);
    }

    return bResult;
}

void InvokeDesktopRefresh() {
    std::list<HWND> listDesktopWindows = GetDoubtfulDesktopWindowsInSelfProcess();

    for (std::list<HWND>::iterator it = listDesktopWindows.begin(); it != listDesktopWindows.end(); ++it) {
        HWND hSHELLDLL_DefView = FindWindowExW(*it, NULL, L"SHELLDLL_DefView", L"");

        if (IsWindow(hSHELLDLL_DefView)) {
            HWND hSysListView32 = FindWindowExW(hSHELLDLL_DefView, NULL, L"SysListView32", L"FolderView");

            if (!IsWindow(hSysListView32)) {
                hSysListView32 = FindWindowExW(hSHELLDLL_DefView, NULL, L"SysListView32", NULL);
            }

            if (IsWindow(hSysListView32)) {
                ::PostMessageW(hSysListView32, WM_KEYDOWN, VK_F5, 0);
                Sleep(888);
            }
        }
    }
}

BOOL KillAllExplorers() {
    set<DWORD> setExplorers;
    BOOL bKillSelf = FALSE;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != INVALID_HANDLE_VALUE && hSnapshot != NULL) {
        PROCESSENTRY32W pe32 = {sizeof(pe32)};

        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (lstrcmpiW(pe32.szExeFile, L"explorer.exe") == 0) {
                    if (pe32.th32ProcessID == GetCurrentProcessId()) {
                        bKillSelf = TRUE;
                    } else {
                        setExplorers.insert(pe32.th32ProcessID);
                    }
                } else if (lstrcmpiW(pe32.szExeFile, L"clover.exe") == 0 &&
                    (GetModuleHandleW(L"TabHelper32.dll") != NULL || GetModuleHandleW(L"TabHelper64.dll") != NULL)) {
                    setExplorers.insert(pe32.th32ProcessID);
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
    }

    set<DWORD>::iterator it = setExplorers.begin();
    for (; it != setExplorers.end(); ++it) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, *it);

        if (hProcess != NULL) {
            TerminateProcess(hProcess, 0);
        }
    }

    if (bKillSelf) {
        TerminateProcess(GetCurrentProcess(), 0);
    }

    return TRUE;
}

static DWORD CALLBACK ResetExplorerProc(LPVOID lpParam) {
    InvokeDesktopRefresh();
    KillAllExplorers();

    return 0;
}

BOOL ResetExplorer() {
    HANDLE hThread = CreateThread(NULL, 0, ResetExplorerProc, NULL, 0, NULL);
    CloseHandle(hThread);

    return hThread != NULL;
}

BOOL SetClipboardHtml(const char *html) {
    static UINT nCbFormat = 0;
    BOOL bResult = FALSE;

    if (nCbFormat == 0) {
        nCbFormat = RegisterClipboardFormatA("HTML Format");

        if (nCbFormat == 0) {
            return bResult;
        }
    }

    if (html == NULL) {
        return bResult;
    }

    int nSize = lstrlenA(html) + 500;
    char *buffer = (char *)malloc(nSize);

    if (buffer == NULL) {
        return bResult;
    }

    int nStartHtml = 0;
    int nEndHtml = 0;
    int nStartFragment = 0;
    int nEndFragment = 0;
    const char *fmt =
        "Version:0.9\r\n"
        "StartHTML:%08d\r\n"
        "EndHTML:%08d\r\n"
        "StartFragment:%08d\r\n"
        "EndFragment:%08d\r\n"
        "<!doctype html><html><body>\r\n"
        "<!--StartFragment-->"
        "%s"
        "<!--EndFragment-->\r\n"
        "</body>\r\n"
        "</html>";

    nEndHtml = wnsprintfA(
        buffer,
        nSize,
        fmt,
        nStartHtml,
        nEndHtml,
        nStartFragment,
        nEndFragment,
        html
        );

    nStartHtml = (int)(INT_PTR)(StrStrA(buffer, "<html") - buffer);
    nStartFragment = (int)(INT_PTR)(StrStrA(buffer, "<!--StartFrag") - buffer);
    nEndFragment = (int)(INT_PTR)(StrStrA(buffer, "<!--EndFragment") - buffer);

    if (nStartHtml != 0 &&
        nEndHtml != 0 &&
        nStartFragment != 0 &&
        nEndFragment != 0
        ) {
        nSize = wnsprintfA(
            buffer,
            nSize,
            fmt,
            nStartHtml,
            nEndHtml,
            nStartFragment,
            nEndFragment,
            html
            );

        if (OpenClipboard(0)) {
            EmptyClipboard();

            HGLOBAL hText = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, nSize + 10);

            if (hText != NULL) {
                char *lpText = (char *)GlobalLock(hText);

                if (lpText != NULL) {
                    lstrcpynA(lpText, buffer, nSize + 10);

                    GlobalUnlock(hText);
                }

                bResult = SetClipboardData(nCbFormat, hText) != NULL;

                GlobalFree(hText);
            }

            CloseClipboard();
        }
    }

    free((void *)buffer);

    return bResult;
}

BOOL SetClipboardPicturePathsByHtml(LPCWSTR lpPaths) {
    if (lpPaths == NULL) {
        return FALSE;
    }

    int nLength = lstrlenW(lpPaths);

    stringstream ss;
    WCHAR szShortPath[MAX_PATH];

    ss<<"<DIV>"<<endl;

    while (nLength > 0) {
        if (GetShortPathNameW(lpPaths, szShortPath, RTL_NUMBER_OF(szShortPath)) > 0) {
            ss<<"<IMG src=\"file"<<":///"<<WtoU(szShortPath)<<"\" >"<<endl;
        }

        lpPaths += (nLength + 1);
        nLength = lstrlenW(lpPaths);
    }

    ss<<"</DIV>";

    return SetClipboardHtml(ss.str().c_str());
}

void SafeDebugMessage(LPCWSTR pFormat, ...) {
    WCHAR szBuffer[2000];
    va_list pArg;

    va_start(pArg, pFormat);
    wvnsprintfW(szBuffer, RTL_NUMBER_OF(szBuffer), pFormat, pArg);
    va_end(pArg);

    OutputDebugStringW(szBuffer);
}

HKEY ParseRegPath(LPCWSTR lpWholePath, LPCWSTR *lppRegPath) {
    HKEY hResult = NULL;
    WCHAR szRootKey[100] = L"";

    lstrcpynW(szRootKey, lpWholePath, RTL_NUMBER_OF(szRootKey));
    LPWSTR lpBackSlash = StrStrW(szRootKey, L"\\");

    if (lpBackSlash != NULL) {
        *lpBackSlash = L'\0';

        if (lstrcmpiW(szRootKey, L"HKEY_CLASSES_ROOT") == 0 || lstrcmpiW(szRootKey, L"HKCR") == 0) {
            hResult = HKEY_CLASSES_ROOT;
        } else if (lstrcmpiW(szRootKey, L"HKEY_CURRENT_USER") == 0 || lstrcmpiW(szRootKey, L"HKCU") == 0) {
            hResult = HKEY_CURRENT_USER;
        } else if (lstrcmpiW(szRootKey, L"HKEY_LOCAL_MACHINE") == 0 || lstrcmpiW(szRootKey, L"HKLM") == 0) {
            hResult = HKEY_LOCAL_MACHINE;
        } else if (lstrcmpiW(szRootKey, L"HKEY_USERS") == 0 || lstrcmpiW(szRootKey, L"HKU") == 0) {
            hResult = HKEY_USERS;
        } else if (lstrcmpiW(szRootKey, L"HKEY_CURRENT_CONFIG") == 0 || lstrcmpiW(szRootKey, L"HKCC") == 0) {
            hResult = HKEY_CURRENT_CONFIG;
        } else if (lstrcmpiW(szRootKey, L"HKEY_PERFORMANCE_DATA") == 0) {
            hResult = HKEY_PERFORMANCE_DATA;
        } else if (lstrcmpiW(szRootKey, L"HKEY_PERFORMANCE_TEXT") == 0) {
            hResult = HKEY_PERFORMANCE_TEXT;
        } else if (lstrcmpiW(szRootKey, L"HKEY_PERFORMANCE_NLSTEXT") == 0) {
            hResult = HKEY_PERFORMANCE_NLSTEXT;
        } else if (lstrcmpiW(szRootKey, L"HKEY_DYN_DATA") == 0) {
            hResult = HKEY_DYN_DATA;
        }
    }

    if (hResult != NULL) {
        lpBackSlash = StrStrW(lpWholePath, L"\\");

        if (lpBackSlash != NULL) {
            while (*lpBackSlash == L'\\') {
                lpBackSlash += 1;
            }

            if (lppRegPath != NULL) {
                *lppRegPath = lpBackSlash;
            }
        }
    }

    return hResult;
}

BOOL IsRegPathExists(HKEY hRootKey, LPCWSTR lpRegPath) {
    HKEY hKey = NULL;

    RegOpenKeyExW(hRootKey, lpRegPath, 0, READ_CONTROL, &hKey);

    if (hKey != NULL) {
        RegCloseKey(hKey);
    }

    return hKey != NULL;
}

BOOL TouchRegPath(HKEY hRootKey, LPCWSTR lpRegPath) {
    HKEY hKey = NULL;

    RegCreateKeyExW(hRootKey, lpRegPath, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_QUERY_VALUE, NULL, &hKey, NULL);

    if (hKey != NULL) {
        RegCloseKey(hKey);
    }

    return hKey != NULL;
}

HFONT GetMenuDefaultFont() {
    HFONT hFont = NULL;
    NONCLIENTMETRICSW ncm = {sizeof(ncm)};

    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        hFont = CreateFontIndirectW(&ncm.lfMenuFont);
    }

    return hFont;
}

UINT GetDrawTextSizeInDc(HDC hdc, LPCWSTR lpText, UINT *puHeight) {
    SIZE size = {0};

    GetTextExtentPoint32W(hdc, lpText, lstrlenW(lpText), &size);

    if (puHeight != NULL) {
        *puHeight = (UINT)size.cy;
    }

    return (UINT)size.cx;
}

BOOL PathCompactPathHelper(HDC hdc, LPWSTR lpText, UINT dx) {
    if (lpText == NULL || *lpText == L'\0') {
        return FALSE;
    } else if (GetDrawTextSizeInDc(hdc, lpText, NULL) <= dx) {
        return TRUE;
    } else if (dx == 0) {
        *lpText = L'\0';
        return TRUE;
    } else {
        int nLength = lstrlenW(lpText);
        BOOL bBackSlashExist = StrStrW(lpText, L"\\") != NULL;
        BOOL bRet = PathCompactPathW(hdc, lpText, dx);

        if (!bRet) {
            return FALSE;
        } else if (GetDrawTextSizeInDc(hdc, lpText, NULL) <= dx) {
            return TRUE;
        } else {
            LPWSTR lpEnd = NULL;

            lstrcpynW(lpText, L"..........................", nLength + 1);
            lpEnd = lpText + lstrlenW(lpText);

            if (bBackSlashExist && GetDrawTextSizeInDc(hdc, L"...\\...", NULL) < dx) {
                lpText[3] = L'\\';
            }

            while (lpEnd >= lpText) {
                if (GetDrawTextSizeInDc(hdc, lpText, NULL) < dx) {
                    return TRUE;
                }

                lpEnd -= 1;
                *lpEnd = L'\0';
            }

            return FALSE;
        }
    }
}

HWND GetTrayNotifyWndInProcess() {
    DWORD dwPid = GetCurrentProcessId();
    HWND hShell_TrayWnd = FindWindowExW(NULL, NULL, L"Shell_TrayWnd", NULL);

    while (IsWindow(hShell_TrayWnd)) {
        DWORD dwWindowPid = 0;

        GetWindowThreadProcessId(hShell_TrayWnd, &dwWindowPid);

        if (dwWindowPid == dwPid) {
            HWND hTrayNotifyWnd = FindWindowExW(hShell_TrayWnd, NULL, L"TrayNotifyWnd", NULL);

            if (IsWindow(hTrayNotifyWnd)) {
                return hTrayNotifyWnd;
            }
        }

        hShell_TrayWnd = FindWindowExW(NULL, hShell_TrayWnd, L"Shell_TrayWnd", NULL);
    }

    return NULL;
}

BOOL GetVersionString(HINSTANCE hModule, WCHAR szVersionString[], int nSize) {
    WCHAR szPath[MAX_PATH];
    char verBuffer[1000];
    VS_FIXEDFILEINFO *pVerFixedInfo = {0};
    UINT uLength;

    GetModuleFileNameW(hModule, szPath, RTL_NUMBER_OF(szPath));

    if (GetFileVersionInfoW(szPath, 0, sizeof(verBuffer), verBuffer)) {
        if (VerQueryValueW(verBuffer, L"\\", (LPVOID *)&pVerFixedInfo, &uLength)) {
            wnsprintfW(
                szVersionString,
                nSize,
                L"%d.%d.%d.%d",
                pVerFixedInfo->dwFileVersionMS >> 16,
                pVerFixedInfo->dwFileVersionMS & 0xffff,
                pVerFixedInfo->dwFileVersionLS >> 16,
                pVerFixedInfo->dwFileVersionLS & 0xffff
                );

            return TRUE;
        }
    }

    return FALSE;
}

BOOL IsAdminMode() {
    BOOL bIsElevated = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        DWORD dwReturnLength = 0;
        struct {
            DWORD TokenIsElevated;
        } te;

        if (GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)20, &te, sizeof(te), &dwReturnLength)) {
            if (dwReturnLength == sizeof(te)) {
                bIsElevated = !!te.TokenIsElevated;
            }
        }

        CloseHandle(hToken);
    }

    return bIsElevated;
}

BOOL WriteFileHelper(HANDLE hFile, LPCVOID lpBuffer, DWORD dwBytesToWrite) {
    DWORD dwBytesWritten = 0;

    while (dwBytesToWrite > 0) {
        if (!WriteFile(hFile, lpBuffer, dwBytesToWrite, &dwBytesWritten, NULL)) {
            return FALSE;
        }

        (LPCSTR &)lpBuffer += dwBytesWritten;
        dwBytesToWrite -= dwBytesWritten;
    }

    return TRUE;
}

HICON GetWindowIcon(HWND hWindow) {
    HICON hIcon = NULL;

    SendMessageTimeout(hWindow, WM_GETICON, ICON_BIG, 0, SMTO_BLOCK | SMTO_ABORTIFHUNG, 3333, (DWORD_PTR *)&hIcon);

    if (hIcon == NULL) {
        hIcon = (HICON)GetClassLongPtr(hWindow, GCLP_HICON);
    }

    return hIcon;
}

HICON GetWindowIconSmall(HWND hWindow) {
    HICON hIcon = NULL;

    SendMessageTimeout(hWindow, WM_GETICON, ICON_SMALL, 0, SMTO_BLOCK | SMTO_ABORTIFHUNG, 3333, (DWORD_PTR *)&hIcon);

    if (hIcon == NULL) {
        hIcon = (HICON)GetClassLongPtr(hWindow, GCLP_HICONSM);
    }

    return hIcon;
}

DWORD RegGetDWORD(HKEY hRootKey, LPCWSTR lpRegPath, LPCWSTR lpRegValue, DWORD dwDefault) {
    DWORD dwSize = sizeof(dwDefault);

    SHGetValueW(hRootKey, lpRegPath, lpRegValue, NULL, &dwDefault, &dwSize);

    return dwDefault;
}

DWORD CheckMenuItemHelper(HMENU hMenu, UINT uId, UINT uFlags, BOOL bChecked) {
    UINT uFlags2[] = {MF_UNCHECKED, MF_CHECKED};

    return CheckMenuItem(hMenu, uId, uFlags | uFlags2[!!bChecked]);
}

DWORD EnableMenuItemHelper(HMENU hMenu, UINT uId, UINT uFlags, BOOL bEnabled) {
    UINT uFlags2[] = {MF_GRAYED, MF_ENABLED};

    return EnableMenuItem(hMenu, uId, uFlags | uFlags2[!!bEnabled]);
}

BOOL IsWindowTopMost(HWND hWindow) {
    return (GetWindowLongPtr(hWindow, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

unsigned __int64 StrToInt64Def(LPCWSTR lpString, unsigned __int64 nDefault) {
    if (lpString == NULL || *lpString == 0) {
        return 0;
    }

    unsigned __int64 nResult = 0;

    for (int i = 0; i < 20; i += 1) {
        WCHAR ch = *lpString++;

        if (ch == L'\0') {
            break;
        }

        if (ch >= L'0' && ch <= L'9') {
            nResult *= 10;
            nResult += (ch - L'0');
        } else {
            return nDefault;
        }
    }

    return nResult;
}

BOOL AdvancedSetForegroundWindow(HWND hWindow) {
    HWND hForegroundWindow = GetForegroundWindow();

    if (hWindow == hForegroundWindow) {
        return TRUE;
    }

    if (!IsWindow(hForegroundWindow) || !IsWindow(hWindow)) {
        return FALSE;
    }

    BOOL bSucceed = FALSE;
    DWORD dwSrcProcessId = 0;
    DWORD dwDestProcessId = 0;
    DWORD dwSrcThreadId = GetWindowThreadProcessId(hWindow, &dwSrcProcessId);
    DWORD dwDestThreadId = GetWindowThreadProcessId(hForegroundWindow, &dwDestProcessId);

    if (dwSrcThreadId == 0 || dwDestThreadId == 0) {
        return FALSE;
    }

    if (dwSrcProcessId != dwDestProcessId) {
        AttachThreadInput(dwSrcThreadId, dwDestThreadId, TRUE);
    }

    bSucceed = SetForegroundWindow(hWindow);

    if (dwSrcProcessId != dwDestProcessId) {
        AttachThreadInput(dwSrcThreadId, dwDestThreadId, FALSE);
    }

    return bSucceed;
}

BOOL GetWindowImageFileName(HWND hWindow, LPWSTR lpBuffer, UINT uBufferSize) {
    DWORD dwProcessId = 0;

    GetWindowThreadProcessId(hWindow, &dwProcessId);

    if (dwProcessId == 0) {
        return FALSE;
    }

    BOOL bRet = FALSE;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);

    if (hProcess != NULL) {
        bRet = GetModuleFileNameExW(hProcess, NULL, lpBuffer, uBufferSize) > 0;

        CloseHandle(hProcess);
    }

    return bRet;
}

BOOL IsWindowFullScreen(HWND hWindow) {
    RECT rect = {0};

    if (!IsWindow(hWindow)) {
        return FALSE;
    }

    GetWindowRect(hWindow, &rect);

    if (GetSystemMetrics(SM_CXSCREEN) == rect.right - rect.left &&
        GetSystemMetrics(SM_CYSCREEN) == rect.bottom - rect.top) {
        return TRUE;
    }

    return FALSE;
}

BOOL KillProcess(DWORD dwProcessId) {
    BOOL bRet = FALSE;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);

    if (hProcess != NULL) {
        bRet = TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }

    if (!bRet && g_bVistaLater && !g_bElevated) {
        WCHAR szRundll32[MAX_PATH] = L"";
        WCHAR szDllPath[MAX_PATH] = L"";
        WCHAR szArguments[MAX_PATH + 20] = L"";

        GetSystemDirectoryW(szRundll32, RTL_NUMBER_OF(szRundll32));
        PathAppendW(szRundll32, L"\\rundll32.exe");

        GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
        PathQuoteSpacesW(szDllPath);

        wnsprintfW(szArguments, RTL_NUMBER_OF(szArguments), L"%s K_Process %lu", szDllPath, dwProcessId);

        bRet = (int)(INT_PTR)ShellExecuteW(NULL, L"runas", szRundll32, szArguments, NULL, SW_SHOW) > 32;
    }

    return bRet;
}

void WINAPI KillProcessByRundll32(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow) {
    if (lpszCmdLine != NULL) {
        DWORD dwProcessId = StrToIntW(lpszCmdLine);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);

        if (hProcess != NULL) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
}

BOOL GetProcessNameById(DWORD dwProcessId, WCHAR szProcessName[], DWORD dwBufferSize) {
    BOOL bRet = FALSE;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != NULL && hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32 = {sizeof(pe32)};

        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == dwProcessId) {
                    lstrcpynW(szProcessName, pe32.szExeFile, dwBufferSize);
                    bRet = TRUE;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
    }

    return bRet;
}

HRESULT SHGetNameFromIDListHelper(PCIDLIST_ABSOLUTE pidl, SIGDN sigdnName, PWSTR* ppszName) {
    typedef HRESULT(WINAPI *FuncSHGetNameFromIDList)(
        PCIDLIST_ABSOLUTE pidl,
        SIGDN             sigdnName,
        PWSTR             *ppszName
        );

    HMODULE hModule = GetModuleHandleW(L"Shell32.dll");
    if (hModule == NULL) {
        hModule = LoadLibraryW(L"Shell32.dll");
    }
    if (hModule == NULL) {
        return ERROR_CALL_NOT_IMPLEMENTED;
    }

    FuncSHGetNameFromIDList func = NULL;
    (PROC&)func = GetProcAddress(hModule, "SHGetNameFromIDList");

    if (func == NULL) {
        return ERROR_CALL_NOT_IMPLEMENTED;
    }

    return func(pidl, sigdnName, ppszName);
}

BOOL SetWindowUnalphaValue(HWND hWindow, WindowUnalphaValue nValue) {
    DWORD dwExStyle = (DWORD)GetWindowLongPtr(hWindow, GWL_EXSTYLE);

    if ((dwExStyle & WS_EX_LAYERED) == 0) {
        SetWindowLongPtr(hWindow, GWL_EXSTYLE, dwExStyle | WS_EX_LAYERED);
        dwExStyle = (DWORD)GetWindowLongPtr(hWindow, GWL_EXSTYLE);
    }

    if ((dwExStyle & WS_EX_LAYERED) == 0) {
        return FALSE;
    }

    return SetLayeredWindowAttributes(hWindow, 0, (BYTE)nValue, LWA_ALPHA);
}

bool DumpStringToFile(const std::wstring &strText, LPCWSTR lpFilePath) {
    bool bRet = false;
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwBytesWritten = 0;
        std::string strTextUtf8 = WtoU(strText);

        bRet = !!WriteFile(hFile, strTextUtf8.c_str(), (DWORD)strTextUtf8.size(), &dwBytesWritten, NULL);
        CloseHandle(hFile);
    }

    return bRet;
}

std::wstring GetStringFromFileMost4kBytes(LPCWSTR lpFilePath) {
    std::wstring strResult;
    char szBuffer[4096];
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwBytesRead = 0;

        if (ReadFile(hFile, szBuffer, sizeof(szBuffer), &dwBytesRead, NULL)) {
            strResult = UtoW(string(szBuffer, dwBytesRead));
        }

        CloseHandle(hFile);
    }

    return strResult;
}

wstring GetCurrentTimeString() {
    WCHAR szTime[32] = L"";
    SYSTEMTIME st;

    GetLocalTime(&st);
    wnsprintfW(
        szTime,
        RTL_NUMBER_OF(szTime),
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds
        );

    return szTime;
}

wstring &AssignString(wstring &str, LPCWSTR lpText) {
    if (lpText != NULL) {
        str = lpText;
    } else {
        str.erase(str.begin(), str.end());
    }

    return str;
}

wstring GetDesktopName(HDESK hDesktop) {
    WCHAR szDesktopName[4096];

    GetUserObjectInformation(hDesktop, UOI_NAME, szDesktopName, sizeof(szDesktopName), NULL);

    return szDesktopName;
}

std::list<HWND> GetDoubtfulDesktopWindowsInSelfProcess() {
    LOCAL_BEGIN;
    static void GetDesktopWindows(LPCWSTR lpClassName, LPCWSTR lpWindowText, list<HWND> &listWindows) {
        HWND hWindow = FindWindowExW(NULL, NULL, lpClassName, lpWindowText);

        while (IsWindow(hWindow)) {
            DWORD dwProcessId = 0;

            GetWindowThreadProcessId(hWindow, &dwProcessId);

            if (dwProcessId == GetCurrentProcessId()) {
                listWindows.push_back(hWindow);
            }

            hWindow = FindWindowExW(NULL, hWindow, lpClassName, lpWindowText);
        }
    }
    LOCAL_END;

    list<HWND> listWindows;

    LOCAL GetDesktopWindows(L"Progman", L"Program Manager", listWindows);
    LOCAL GetDesktopWindows(L"WorkerW", L"", listWindows);

    return listWindows;
}

std::wstring RegGetString(HKEY hKey, LPCWSTR lpRegPath, LPCWSTR lpRegValue, LPCWSTR lpDefaultData /*= L""*/) {
    if (lpDefaultData == NULL) {
        lpDefaultData = L"";
    }

    wstring strResult = lpDefaultData;
    WCHAR szBuffer[4096] = L"";
    DWORD dwSize = sizeof(szBuffer);
    DWORD dwRegType = REG_NONE;

    LSTATUS nResult = SHGetValueW(hKey, lpRegPath, lpRegValue, &dwRegType, szBuffer, &dwSize);

    if (dwRegType != REG_SZ && dwRegType != REG_EXPAND_SZ) {
        //
    } else if (nResult == ERROR_SUCCESS) {
        strResult = szBuffer;
    } else if (nResult == ERROR_MORE_DATA) {
        WCHAR *lpBuffer = (WCHAR *)malloc(dwSize);

        if (lpBuffer != NULL) {
            ZeroMemory(lpBuffer, dwSize);
            SHGetValueW(hKey, lpRegPath, lpRegValue, NULL, lpBuffer, &dwSize);
            strResult = lpBuffer;
            free(lpBuffer);
        }
    }

    return strResult;
}

LPCVOID GetResourceBuffer(HINSTANCE hInstance, LPCWSTR lpResType, LPCWSTR lpResName, LPDWORD lpResSize /*= NULL*/) {
    HRSRC hRes = FindResourceW(hInstance, lpResName, lpResType);

    if (hRes == NULL) {
        return NULL;
    }

    HGLOBAL hGlobal = LoadResource(hInstance, hRes);
    DWORD dwSize = SizeofResource(hInstance, hRes);

    if (hGlobal == NULL) {
        return NULL;
    }

    LPCVOID lpBuffer = LockResource(hGlobal);

    if (lpBuffer == NULL) {
        return NULL;
    }

    if (lpResSize != NULL) {
        *lpResSize = dwSize;
    }

    return lpBuffer;
}

void ExpandTreeControlForLevel(HWND hControl, HTREEITEM htiBegin, int nLevel) {
    if (nLevel < 1) {
        return;
    }

    if (htiBegin == NULL) {
        htiBegin = (HTREEITEM)SendMessageW(hControl, TVM_GETNEXTITEM, TVGN_ROOT, NULL);
    }

    SendMessageW(hControl, TVM_EXPAND, TVE_EXPAND, (LPARAM)htiBegin);

    if (nLevel > 1) {
        HTREEITEM hChild = (HTREEITEM)SendMessageW(hControl, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)htiBegin);

        while (hChild != NULL) {
            ExpandTreeControlForLevel(hControl, hChild, nLevel - 1);
            hChild = (HTREEITEM)SendMessageW(hControl, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hChild);
        }
    }
}

bool IsPathDirectoryW(LPCWSTR lpPath) {
    DWORD dwFileAttributes = GetFileAttributesW(lpPath);

    return dwFileAttributes != INVALID_FILE_ATTRIBUTES && (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsPathFileW(LPCWSTR lpPath) {
    DWORD dwFileAttributes = GetFileAttributesW(lpPath);

    return dwFileAttributes != INVALID_FILE_ATTRIBUTES && (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring GetEscapedFilePathInDirectoryW(LPCWSTR lpFileName, LPCWSTR lpDestDirectory) {
    WCHAR szInputFileName[MAX_PATH] = L"";
    WCHAR szInputFileExt[MAX_PATH] = L"";
    WCHAR szDestFilePath[MAX_PATH] = L"";
    int nIndex = 1;

    lstrcpynW(szInputFileName, PathFindFileNameW(lpFileName), RTL_NUMBER_OF(szInputFileName));
    lstrcpynW(szInputFileExt, PathFindExtensionW(szInputFileName), RTL_NUMBER_OF(szInputFileExt));
    PathRemoveExtensionW(szInputFileName);

    wnsprintfW(szDestFilePath, RTL_NUMBER_OF(szDestFilePath), L"%ls\\%ls%ls", lpDestDirectory, szInputFileName, szInputFileExt);

    while (PathFileExistsW(szDestFilePath)) {
        wnsprintfW(szDestFilePath, RTL_NUMBER_OF(szDestFilePath), L"%ls\\%ls[%d]%ls", lpDestDirectory, szInputFileName, nIndex++, szInputFileExt);
    }

    return szDestFilePath;
}

bool CreateHardLinkHelperW(LPCWSTR lpSrcFilePath, LPCWSTR lpDestDirectory) {
    std::wstring strDestFilePath = GetEscapedFilePathInDirectoryW(lpSrcFilePath, lpDestDirectory);
    std::wstring strArguments = fmtW(L"/c mklink /h \"%ls\" \"%ls\"", strDestFilePath.c_str(), lpSrcFilePath);

    return !!ElevateAndRunW(L"cmd", strArguments.c_str(), lpDestDirectory, SW_HIDE);
}

bool CreateSoftLinkHelperW(LPCWSTR lpSrcFilePath, LPCWSTR lpDestDirectory) {
    std::wstring strDestFilePath = GetEscapedFilePathInDirectoryW(lpSrcFilePath, lpDestDirectory);
    std::wstring strArguments = fmtW(L"/c mklink /d /j \"%ls\" \"%ls\"", strDestFilePath.c_str(), lpSrcFilePath);

    return !!ElevateAndRunW(L"cmd", strArguments.c_str(), lpDestDirectory, SW_HIDE);
}

std::string GetFriendlyFileSizeA(unsigned __int64 nSize) {
    const unsigned __int64 nTimes = 1024;           // 单位之间的倍数
    const unsigned __int64 nKB = nTimes;
    const unsigned __int64 nMB = nTimes * nKB;
    const unsigned __int64 nGB = nTimes * nMB;
    const unsigned __int64 nTB = nTimes * nGB;
    const char *lpUnit = NULL;
    unsigned __int64 nLeftValue = 0;                // 小数点左边的数字
    unsigned __int64 nRightValue = 0;               // 小数点右边的数字
    stringstream ssResult;

    if (nSize < nKB) {
        ssResult<<(unsigned)nSize<<" B";
    } else {
        if (nSize >= nTB) {
            nLeftValue = nSize / nTB;
            nRightValue = nSize % nTB * 100 / nGB % 100;
            lpUnit = " TB";
        } else if (nSize >= nGB) {
            nLeftValue = nSize / nGB;
            nRightValue = nSize % nGB * 100 / nMB % 100;
            lpUnit = " GB";
        } else if (nSize >= nMB) {
            nLeftValue = nSize / nMB;
            nRightValue = nSize % nMB * 100 / nKB % 100;
            lpUnit = " MB";
        } else {
            nLeftValue = nSize / nKB;
            nRightValue = nSize % nKB * 100 / 1 % 100;
            lpUnit = " KB";
        }

        // 每三位数用逗号分隔
        list<unsigned __int64> listValues;

        while (nLeftValue >= 1000) {
            listValues.push_back(nLeftValue % 1000);
            nLeftValue /= 1000;
        }

        if (nLeftValue > 0) {
            listValues.push_back(nLeftValue);
        }

        stringstream ssLeftValue;
        list<unsigned __int64>::reverse_iterator it = listValues.rbegin();
        for (; it != listValues.rend(); ++it) {
            if (it == listValues.rbegin()) {
                ssLeftValue<<(unsigned)*it;
            } else {
                ssLeftValue<<setw(3)<<setfill('0')<<(unsigned)*it;
            }
        }

        ssResult<<ssLeftValue.str()<<'.'<<setw(2)<<setfill('0')<<(unsigned)nRightValue<<lpUnit;
    }

    return ssResult.str();
}

std::wstring fmtW(const wchar_t *fmt, ...) {
    wchar_t szText[1024 * 10];
    va_list val;

    szText[0] = L'\0';
    va_start(val, fmt);
    wvnsprintfW(szText, RTL_NUMBER_OF(szText), fmt, val);
    va_end(val);

    return szText;
}

LPSYSTEMTIME Time1970ToLocalTime(DWORD dwTime1970, LPSYSTEMTIME lpSt) {
    SYSTEMTIME st1970 = {0};
    FILETIME ft1970;
    FILETIME ftNewTime;

    st1970.wYear = 1970;
    st1970.wMonth = 1;
    st1970.wDay = 1;

    SystemTimeToFileTime(&st1970, &ft1970);
    FileTimeToLocalFileTime(&ft1970, &ftNewTime);
    ((PULARGE_INTEGER)&ftNewTime)->QuadPart += (unsigned __int64)dwTime1970 * 1000 * 1000 * 10;
    FileTimeToSystemTime(&ftNewTime, lpSt);

    return lpSt;
}

HRESULT SHGetPropertyStoreForWindowHelper(HWND hwnd, REFIID riid, void** ppv) {
    typedef HRESULT(WINAPI *FuncSHGetPropertyStoreForWindow)(HWND hwnd, REFIID riid, void** ppv);
    static FuncSHGetPropertyStoreForWindow funcSHGetPropertyStoreForWindow = NULL;

    if (funcSHGetPropertyStoreForWindow == NULL) {
        HMODULE hModule = GetModuleHandleW(L"shell32.dll");
        if (hModule == NULL) {
            hModule = LoadLibraryW(L"shell32.dll");
        }
        if (hModule == NULL) {
            return E_FAIL;
        }
        (PROC&)funcSHGetPropertyStoreForWindow = GetProcAddress(hModule, "SHGetPropertyStoreForWindow");
    }

    if (funcSHGetPropertyStoreForWindow == NULL) {
        return E_FAIL;
    }

    return funcSHGetPropertyStoreForWindow(hwnd, riid, ppv);
}

void WINAPI T2(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow) {
    BrowseForFile(L"C:\\Windows\\system32\\cmd.exe");

    return ;
}
