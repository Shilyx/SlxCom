#ifndef _SLX_COM_CONTEXT_MENU_H
#define _SLX_COM_CONTEXT_MENU_H

#include <ShlObj.h>

class FileInfo
{
public:
    BOOL bIsFile;
    BOOL bIsDll;
    BOOL bIsJpg;
    BOOL bIsPicture;
    BOOL bIsRar;
    BOOL bIsEncorated;
    BOOL bRegEventSucceed;
    TCHAR szPath[MAX_PATH];

    FileInfo()
    {
        bIsFile = FALSE;
        bIsDll = FALSE;
        bIsJpg = FALSE;
        bIsPicture = FALSE;
        bIsRar = FALSE;
        bIsEncorated = FALSE;
        bRegEventSucceed = FALSE;
        lstrcpyn(szPath, TEXT(""), sizeof(szPath) / sizeof(TCHAR));
    }
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

    static DWORD CALLBACK HashCalcProc(LPVOID lpParam);
    static INT_PTR CALLBACK PropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT CALLBACK PropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE pPsp); 

protected:
    volatile DWORD m_dwRefCount;
    FileInfo *m_pFiles;
    UINT m_uFileCount;
    BOOL m_bIsBackground;     //QueryContextMenu时且选中项数目为一时确定

    BOOL ConvertToShortPaths();
    BOOL ShouldAddPropSheet();
};

#endif