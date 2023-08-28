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
    SET_FILE,           // ���ļ����ļ��л����������һ�
    SET_BACKGROUND,     // ���ļ��пհ״��һ�
    SET_DRAGDROP,       // �Ҽ��϶�
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

    // У�� ����ҳ
    static DWORD CALLBACK HashCalcProc(LPVOID lpParam);
    static INT_PTR CALLBACK HashPropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT CALLBACK HashPropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp);

    // �ļ�ʱ�� ����ҳ
    static BOOL FileTimePropSheetDoSave(HWND hwndDlg, FileInfo *pFiles, UINT uFileCount);
    static INT_PTR CALLBACK FileTimePropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT CALLBACK FileTimePropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp);

protected:
    volatile DWORD m_dwRefCount;
    FileInfo *m_pFiles;
    UINT m_uFileCount;
    BOOL m_bIsBackground;     //QueryContextMenuʱ��ѡ������ĿΪһʱȷ��

    BOOL ConvertToShortPaths();
    BOOL ShouldAddHashPropSheet();
    BOOL ShouldAddFileTimePropSheet();
    BOOL ShouldAddPeInformationSheet();
    static void CallDriverAction(LPCWSTR lpAction, LPCWSTR lpFilePath);

protected:
    std::wstring m_strInputFolder;                  // ����Ŀ¼
    std::vector<std::wstring> m_vectorInputFiles;   // �����ļ��б�
    std::wstring m_strInputFile;                    // �����ļ���һ��
    ShellExtType m_shellExtType;
};

#endif