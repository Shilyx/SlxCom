#include "SlxComExpandDirFiles.h"
#include "SlxComTools.h"
#include <Shlwapi.h>
#include <string>
#include <list>

using namespace std;

// list<fileName, isDir>
static list<pair<wstring, bool> > GetAllFileNames(const wstring &strDir) {
    WIN32_FIND_DATAW wfd = {0};
    HANDLE hFind = FindFirstFileW((strDir + L"\\*").c_str(), &wfd);
    list<pair<wstring, bool> > listResult;

    if (hFind != NULL && hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (lstrcmpiW(wfd.cFileName, L".") != 0 && lstrcmpiW(wfd.cFileName, L"..") != 0) {
                    listResult.push_front(make_pair(wfd.cFileName, true));
                }
            } else {
                listResult.push_back(make_pair(wfd.cFileName, false));
            }
        } while (FindNextFileW(hFind, &wfd));

        FindClose(hFind);
    }

    return listResult;
}

static bool IsDirInVolNTFS(LPCWSTR lpDir) {
    WCHAR szRootDir[MAX_PATH] = L"";
    WCHAR szFsName[1024] = L"";

    lstrcpynW(szRootDir, lpDir, RTL_NUMBER_OF(szRootDir));
    lstrcpynW(szRootDir + 1, L":\\", RTL_NUMBER_OF(szRootDir));

    GetVolumeInformationW(szRootDir, NULL, 0, NULL, NULL, NULL, szFsName, RTL_NUMBER_OF(szFsName));

    return lstrcmpiW(szFsName, L"NTFS") == 0;
}

BOOL EdfCanBeExpanded(LPCWSTR lpDir) {
    if (!IsDirInVolNTFS(lpDir)) {
        return FALSE;
    }

    list<pair<wstring, bool> > listFileNames = GetAllFileNames(lpDir);

    if (!listFileNames.empty()) {
        return !!listFileNames.begin()->second;
    }

    return FALSE;
}

BOOL EdfCanBeUnexpanded(LPCWSTR lpDir) {
    if (!IsDirInVolNTFS(lpDir)) {
        return FALSE;
    }

    list<pair<wstring, bool> > listFileNames = GetAllFileNames(lpDir);

    for (list<pair<wstring, bool> >::const_reverse_iterator it = listFileNames.rbegin(); it != listFileNames.rend(); ++it) {
        if (!it->second) {
            if (PathFileExistsW((wstring(lpDir) + L"\\" + it->first + L":edf").c_str())) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

void TouchFile(LPCWSTR lpFilePath) {
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

BOOL WINAPI apiCreateSymbolicLink(
  __in          LPCWSTR lpSymlinkFileName,
  __in          LPCWSTR lpTargetFileName,
  __in          DWORD dwFlags
) {
    static BOOL (WINAPI *s_pCreateSymbolicLinkW)(
        __in          LPCWSTR lpSymlinkFileName,
        __in          LPCWSTR lpTargetFileName,
        __in          DWORD dwFlags
        ) = NULL;

    if (s_pCreateSymbolicLinkW == NULL) {
        (PROC &)s_pCreateSymbolicLinkW = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateSymbolicLinkW");
    }

    if (s_pCreateSymbolicLinkW) {
        return s_pCreateSymbolicLinkW(lpSymlinkFileName, lpTargetFileName, dwFlags);
    }

    SetLastError(ERROR_INVALID_ADDRESS);
    return FALSE;
}

static bool EdfDoExpandInternal(const wstring &strBaseDir, const wstring &strTargetDir, const wstring &strPrefix, int nLevel) {
    list<pair<wstring, bool> > listFileNames = GetAllFileNames(strBaseDir);

    for (list<pair<wstring, bool> >::const_reverse_iterator it = listFileNames.rbegin(); it != listFileNames.rend(); ++it) {
        wstring strInputFilePath = strBaseDir + L"\\" + it->first;

        if (it->second) {
            EdfDoExpandInternal(strInputFilePath, strTargetDir, strPrefix + it->first + L"@", nLevel + 1);
        } else if (nLevel > 0) {            // 首层目录内的直接文件不再展开
            WCHAR szTargetFilePath[MAX_PATH] = L"";

            wnsprintfW(szTargetFilePath, RTL_NUMBER_OF(szTargetFilePath), L"%s\\%s[%d]#%s", strTargetDir.c_str(), strPrefix.c_str(), nLevel, it->first.c_str());
            CreateHardLinkW(szTargetFilePath, strInputFilePath.c_str(), NULL);
            DumpStringToFile(strInputFilePath, (wstring(szTargetFilePath) + L":edf").c_str());
        }
    }

    return true;
}

BOOL EdfDoExpand(LPCWSTR lpDir) {
    return !!EdfDoExpandInternal(lpDir, lpDir, L"@@", 0);
}

BOOL EdfDoUnexpand(LPCWSTR lpDir) {
    list<pair<wstring, bool> > listFileNames = GetAllFileNames(lpDir);

    for (list<pair<wstring, bool> >::const_reverse_iterator it = listFileNames.rbegin(); it != listFileNames.rend(); ++it) {
        wstring strFilePath = wstring(lpDir) + L"\\" + it->first;

        if (PathFileExistsW((strFilePath + L":edf").c_str())) {
            DeleteFileW(strFilePath.c_str());
        }
    }

    return TRUE;
}