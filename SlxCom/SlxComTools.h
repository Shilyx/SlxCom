#ifndef _SLX_COM_TOOLS_H
#define _SLX_COM_TOOLS_H

#include <Windows.h>
#include <CommCtrl.h>
#include <ShlObj.h>
#pragma warning(disable: 4786)
#include "lib/charconv.h"
#include <list>
#include <string>
#include <vector>

enum DRIVER_ACTION {
    DA_INSTALL,
    DA_START,
    DA_STOP,
    DA_UNINSTALL,
};

struct ListCtrlSortStruct {
    int nRet;
    HWND hListCtrl;
    int nSubItem;
};

VOID DrvAction(HWND hWindow, LPCWSTR lpFilePath, DRIVER_ACTION daValue);
DWORD SHSetTempValue(HKEY hRootKey, LPCWSTR pszSubKey, LPCWSTR pszValue, DWORD dwType, LPCVOID pvData, DWORD cbData);
BOOL IsFileSigned(LPCWSTR lpFilePath);
BOOL IsFileHasSignatureArea(LPCWSTR lpFilePath);
BOOL RemoveFileSignatrueArea(LPCWSTR lpFilePath);
BOOL CombineFile(LPCWSTR lpRarPath, LPCWSTR lpJpgPath, LPWSTR lpResultPath, int nLength);
BOOL SaveResourceToFile(LPCWSTR lpResType, LPCWSTR lpResName, LPCWSTR lpFilePath);
BOOL SetClipboardText(LPCWSTR lpText);
BOOL RunCommand(LPWSTR lpCommandLine, LPCWSTR lpCurrentDirectory = NULL);
BOOL RunCommandWithArguments(LPCWSTR lpFile);
BOOL RunCommandEx(LPCWSTR lpApplication, LPCWSTR lpArguments, LPCWSTR lpCurrentDirectory, BOOL bElevate);
BOOL BrowseForRegPath(LPCWSTR lpRegPath);
BOOL BrowseForFile(LPCWSTR lpFile);
BOOL IsExplorer();
BOOL TryUnescapeFileName(LPCWSTR lpFilePath, WCHAR szUnescapedFilePath[], int nSize);
BOOL ResolveShortcut(LPCWSTR lpLinkFilePath, WCHAR szResolvedPath[], UINT nSize);
// BOOL RegisterClipboardFile(LPCWSTR lpFileList, BOOL bCopy);
VOID ShowErrorMessage(HWND hWindow, DWORD dwErrorCode);
int MessageBoxFormat(HWND hWindow, LPCWSTR lpCaption, UINT uType, LPCWSTR lpFormat, ...);
BOOL EnableDebugPrivilege(BOOL bEnable);
BOOL IsFileDenyed(LPCWSTR lpFilePath);
BOOL CloseRemoteHandle(DWORD dwProcessId, HANDLE hRemoteHandle);
BOOL IsSameFilePath(LPCWSTR lpFilePath1, LPCWSTR lpFilePath2);
BOOL IsPathDesktop(LPCWSTR lpPath);
BOOL IsWow64ProcessHelper(HANDLE hProcess);
BOOL DisableWow64FsRedirection();
BOOL ResetExplorer();
BOOL SetClipboardPicturePathsByHtml(LPCWSTR lpPaths);
void SafeDebugMessage(LPCWSTR pFormat, ...);
HKEY ParseRegPath(LPCWSTR lpWholePath, LPCWSTR *lppRegPath);
BOOL IsRegPathExists(HKEY hRootKey, LPCWSTR lpRegPath);
BOOL TouchRegPath(HKEY hRootKey, LPCWSTR lpRegPath);
HFONT GetMenuDefaultFont();
UINT GetDrawTextSizeInDc(HDC hdc, LPCWSTR lpText, UINT *puHeight);
BOOL PathCompactPathHelper(HDC hdc, LPWSTR lpText, UINT dx);
HWND GetTrayNotifyWndInProcess();
BOOL GetVersionString(HINSTANCE hModule, WCHAR szVersionString[], int nSize);
BOOL IsAdminMode();
BOOL WriteFileHelper(LPCWSTR lpFilePath, LPCVOID lpBuffer, DWORD dwBytesToWrite);
BOOL WriteFileHelper(HANDLE hFile, LPCVOID lpBuffer, DWORD dwBytesToWrite);
HICON GetWindowIcon(HWND hWindow);
HICON GetWindowIconSmall(HWND hWindow);
DWORD RegGetDWORD(HKEY hRootKey, LPCWSTR lpRegPath, LPCWSTR lpRegValue, DWORD dwDefault);
DWORD CheckMenuItemHelper(HMENU hMenu, UINT uId, UINT uFlags, BOOL bChecked);
DWORD EnableMenuItemHelper(HMENU hMenu, UINT uId, UINT uFlags, BOOL bEnabled);
BOOL IsWindowTopMost(HWND hWindow);
unsigned __int64 StrToInt64Def(LPCWSTR lpString, unsigned __int64 nDefault);
BOOL AdvancedSetForegroundWindow(HWND hWindow);
BOOL GetWindowImageFileName(HWND hWindow, LPWSTR lpBuffer, UINT uBufferSize);
BOOL IsWindowFullScreen(HWND hWindow);
BOOL KillProcess(DWORD dwProcessId);
BOOL GetProcessNameById(DWORD dwProcessId, WCHAR szProcessName[], DWORD dwBufferSize);
LPCVOID GetResourceBuffer(HINSTANCE hInstance, LPCWSTR lpResType, LPCWSTR lpResName, LPDWORD lpResSize = NULL);
void ExpandTreeControlForLevel(HWND hControl, HTREEITEM htiBegin, int nLevel);
bool IsPathDirectoryW(LPCWSTR lpPath);
bool IsPathFileW(LPCWSTR lpPath);
std::wstring GetEscapedFilePathInDirectoryW(LPCWSTR lpFileName, LPCWSTR lpDestDirectory);
bool CreateHardLinkHelperW(LPCWSTR lpSrcFilePath, LPCWSTR lpDestDirectory);
bool CreateSoftLinkHelperW(LPCWSTR lpSrcFilePath, LPCWSTR lpDestDirectory);
void AutoCloseMessageBoxForThreadInSeconds(DWORD dwThreadId, int nSeconds);
HRESULT SHGetNameFromIDListHelper(PCIDLIST_ABSOLUTE pidl, SIGDN sigdnName, PWSTR* ppszName);
BOOL PathIsExistingDirectory(LPCWSTR lpFilePath);
BOOL PathIsExistingFile(LPCWSTR lpFilePath);
BOOL PathGetFileSize(LPCWSTR lpFilePath, ULONGLONG* pFileSizeHigh);
BOOL TextIsBase64W(LPCWSTR lpText); // allow \r \n
BOOL TextIsHexW(LPCWSTR lpText); // allow \r \n \t space

enum WindowUnalphaValue {
    WUV100 = 255,
    WUV80 = 204,
    WUV60 = 153,
    WUV40 = 102,
    WUV20 = 51,
};

BOOL SetWindowUnalphaValue(HWND hWindow, WindowUnalphaValue nValue);

bool DumpStringToFile(const std::wstring &strText, LPCWSTR lpFilePath);
std::wstring GetStringFromFileMost4kBytes(LPCWSTR lpFilePath);

std::wstring GetCurrentTimeString();
std::wstring &AssignString(std::wstring &str, LPCWSTR lpText);
std::wstring GetDesktopName(HDESK hDesktop);
std::string GetFriendlyFileSizeA(unsigned __int64 nSize);
std::wstring fmtW(const wchar_t *fmt, ...);
LPSYSTEMTIME Time1970ToLocalTime(DWORD dwTime1970, LPSYSTEMTIME lpSt);
HRESULT SHGetPropertyStoreForWindowHelper(HWND hwnd, REFIID riid, void** ppv);

void InitGdiplus();
BOOL GetEncoderClsid(LPCWSTR lpFormat, CLSID* pClsid);
BOOL ClipboardDataExist(BOOL& bHasImage);
BOOL SaveClipboardImageAsPng(HWND hWindow, LPCWSTR lpImageFilePath);
BOOL SaveClipboardImageAsPngToDir(HWND hWindow, LPCWSTR lpDirectory, std::wstring& strFilePath);

std::string HexEncode(const std::string& strInput);
std::string HexDecode_Throw(const std::string& strInput); // allow \r \n \t space

template <class T>
std::wstring AnyTypeToString(const T &value) {
    std::wstringstream ss;

    ss<<value;

    return ss.str();
}

template <>
inline std::wstring AnyTypeToString(const std::wstring &value) {
    return value;
}

// 功能：以patten为分隔符将字符串str分隔并置入result中，并返回result的大小，ignore_blank标识是否忽略空白部分
// 注意：只要函数被调用，result都将首先被置空
// 注意：原始字符串首尾的空白始终不被置入result中
// 例如：以*分隔“a*b”和“*a*b”和“a*b*”和“*a*b*”时无论ignore_blank为何得到的结果总是两部分
template <class T>
size_t SplitString(const std::basic_string<T> &str, const std::basic_string<T> &patten, std::vector<std::basic_string<T> > &result, bool ignore_blank = true) {
    std::basic_string<T>::size_type pos = str.find(patten);
    std::basic_string<T>::size_type begin = 0;
    bool first = true;

    result.clear();

    while (true) {
        if (pos == str.npos) {
            if (begin < str.size()) {
                result.push_back(str.substr(begin));
            }

            return result.size();
        } else {
            if (pos > begin || (!first && !ignore_blank)) {
                result.push_back(str.substr(begin, pos - begin));
            }

            begin = pos + patten.size();
            pos = str.find(patten, begin);

            first = false;
        }
    }
}

template <class T>
void Replace(std::basic_string<T> &str, const std::basic_string<T> &old_str, const std::basic_string<T> &new_str) {
    std::basic_string<T>::size_type offset = str.find(old_str);

    while (offset != str.npos) {
        str.replace(offset, old_str.length(), new_str);

        offset = str.find(old_str, offset + new_str.length());
    }
}

std::list<HWND> GetDoubtfulDesktopWindowsInSelfProcess();

std::wstring RegGetString(HKEY hKey, LPCWSTR lpRegPath, LPCWSTR lpRegValue, LPCWSTR lpDefaultData = L"");

//////////////////////////////////////////////////////////////////////////
#define LOCAL_BEGIN class __Local           \
{                                           \
public:                                     \
    void __Temp()                           \
    {                                       \
        m___nTemp = GetTickCount();         \
    }                                       \
    volatile int m___nTemp

#define LOCAL_END }
#define LOCAL __Local::

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD (0x160c)
#endif

#endif
