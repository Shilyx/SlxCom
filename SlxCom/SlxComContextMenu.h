#ifndef _SLX_COM_CONTEXT_MENU_H
#define _SLX_COM_CONTEXT_MENU_H

#include <ShlObj.h>
#include <vector>
#include <string>

class FileInfo
{
public:
    BOOL bIsFile;
    BOOL bIsDll;
    BOOL bIsJpg;
    BOOL bIsPicture;
    BOOL bIsRar;
    WCHAR szPath[MAX_PATH];

    FileInfo()
    {
        bIsFile = FALSE;
        bIsDll = FALSE;
        bIsJpg = FALSE;
        bIsPicture = FALSE;
        bIsRar = FALSE;
        lstrcpynW(szPath, L"", sizeof(szPath) / sizeof(WCHAR));
    }
};

enum ShellExtType
{
    SET_FILE,           // 在文件、文件夹或驱动器上右击
    SET_BACKGROUND,     // 在文件夹空白处右击
    SET_DRAGDROP,       // 右键拖动
};

class CSlxComContextMenu : public IContextMenu, public IShellExtInit, public IShellPropSheetExt
{
public:
    CSlxComContextMenu();
    ~CSlxComContextMenu();

    //IUnknow Method
    STDMETHOD(QueryInterface)(REFIID, void**);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    //IShellExtInit
    STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID);

    //IContextMenu
    STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
    STDMETHOD(GetCommandString)(UINT_PTR idCmd, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax);
    STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO pici);

    //IShellPropSheetExt
    STDMETHOD(AddPages)(LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam);
    STDMETHOD(ReplacePage)(UINT uPageID, LPFNADDPROPSHEETPAGE pfnReplacePage, LPARAM lParam);

    static BOOL CALLBACK EnumChildSetFontProc(HWND hwnd, LPARAM lParam);

    // 校验 属性页
    static DWORD CALLBACK HashCalcProc(LPVOID lpParam);
    static INT_PTR CALLBACK HashPropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT CALLBACK HashPropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp);

    // 文件时间 属性页
    static BOOL FileTimePropSheetDoSave(HWND hwndDlg, FileInfo *pFiles, UINT uFileCount);
    static INT_PTR CALLBACK FileTimePropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT CALLBACK FileTimePropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp);

protected:
    volatile DWORD m_dwRefCount;
    FileInfo *m_pFiles;
    UINT m_uFileCount;
    BOOL m_bIsBackground;     //QueryContextMenu时且选中项数目为一时确定

    BOOL ConvertToShortPaths();
    BOOL ShouldAddHashPropSheet();
    BOOL ShouldAddFileTimePropSheet();
    BOOL ShouldAddPeInformationSheet();
    static void CallDriverAction(LPCWSTR lpAction, LPCWSTR lpFilePath);

protected:
    std::wstring m_strInputFolder;                  // 输入目录
    std::vector<std::wstring> m_vectorInputFiles;   // 输入文件列表
    std::wstring m_strInputFile;                    // 输入文件第一个
    ShellExtType m_shellExtType;
};

#endif