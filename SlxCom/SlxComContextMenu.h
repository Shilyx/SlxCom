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

class CSlxComContextMenu : public IContextMenu, public IShellExtInit
{
public:
    CSlxComContextMenu();

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

protected:
    volatile DWORD m_dwRefCount;
    FileInfo *m_pFiles;
    UINT m_uFileCount;

    BOOL ConvertToShortPaths();
};

#endif