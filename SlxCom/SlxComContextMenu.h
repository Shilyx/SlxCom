#ifndef _SLX_COM_CONTEXT_MENU_H
#define _SLX_COM_CONTEXT_MENU_H

#include <ShlObj.h>

typedef TCHAR PATH[MAX_PATH];

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
    PATH *m_pszFiles;
    UINT m_uFileCount;
};

#endif