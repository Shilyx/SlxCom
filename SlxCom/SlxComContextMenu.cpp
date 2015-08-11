#define _WIN32_WINNT 0x500
#include "SlxComContextMenu.h"
#include <shlwapi.h>
#include <CommCtrl.h>
#include "SlxComOverlay.h"
#include "SlxComTools.h"
#include "SlxComPeTools.h"
#include "resource.h"
#include "SlxCrypto.h"
#include "SlxComAbout.h"
#include "SlxManualCheckSignature.h"
#include "SlxElevateBridge.h"
#pragma warning(disable: 4786)
#include "lib/charconv.h"
#include <list>
#include <map>
#include <vector>

using namespace std;

extern HINSTANCE g_hinstDll;    //SlxCom.cpp
extern OSVERSIONINFO g_osi;     //SlxCom.cpp

extern HBITMAP g_hInstallBmp;
extern HBITMAP g_hUninstallBmp;
extern HBITMAP g_hCombineBmp;
extern HBITMAP g_hCopyFullPathBmp;
extern HBITMAP g_hAddToCopyBmp;
extern HBITMAP g_hAddToCutBmp;
extern HBITMAP g_hTryRunBmp;
extern HBITMAP g_hTryRunWithArgumentsBmp;
extern HBITMAP g_hRunCmdHereBmp;
extern HBITMAP g_hOpenWithNotepadBmp;
extern HBITMAP g_hKillExplorerBmp;
extern HBITMAP g_hManualCheckSignatureBmp;
extern HBITMAP g_hUnescapeBmp;
extern HBITMAP g_hAppPathBmp;
extern HBITMAP g_hDriverBmp;
extern HBITMAP g_hUnlockFileBmp;
extern HBITMAP g_hCopyPictureHtmlBmp;
extern BOOL g_bVistaLater;
extern BOOL g_bElevated;

volatile HANDLE m_hManualCheckSignatureThread = NULL;
static HANDLE g_hManualCheckSignatureMutex = CreateMutex(NULL, FALSE, NULL);

CSlxComContextMenu::CSlxComContextMenu()
{
    m_dwRefCount = 0;
    m_bIsBackground = FALSE;

    m_pFiles = new FileInfo[1];
    m_uFileCount = 1;
    lstrcpy(m_pFiles[0].szPath, TEXT("1:\\"));
}

CSlxComContextMenu::~CSlxComContextMenu()
{
    delete []m_pFiles;
}

STDMETHODIMP CSlxComContextMenu::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IShellExtInit)
    {
        *ppv = static_cast<IShellExtInit *>(this);
    }
    else if(riid == IID_IContextMenu)
    {
        *ppv = static_cast<IContextMenu *>(this);
    }
    else if(riid == IID_IShellPropSheetExt)
    {
        *ppv = static_cast<IShellPropSheetExt *>(this);
    }
    else if(riid == IID_IUnknown)
    {
        *ppv = static_cast<IShellExtInit *>(this);
    }
    else
    {
        *ppv = 0;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown *>(*ppv)->AddRef();

    return S_OK;
}

STDMETHODIMP_(ULONG) CSlxComContextMenu::AddRef()
{
    return InterlockedIncrement((volatile LONG *)&m_dwRefCount);
}

STDMETHODIMP_(ULONG) CSlxComContextMenu::Release()
{
    DWORD dwRefCount = InterlockedDecrement((volatile LONG *)&m_dwRefCount);

    if(dwRefCount == 0)
    {
        delete this;
    }

    return dwRefCount;
}

//IShellExtInit
STDMETHODIMP CSlxComContextMenu::Initialize(LPCITEMIDLIST pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID)
{
    if(pdtobj == NULL)
    {
        if(pidlFolder == NULL)
        {
            return E_INVALIDARG;
        }
        else
        {
            TCHAR szPath[MAX_PATH];

            if(SHGetPathFromIDList(pidlFolder, szPath))
            {
                m_uFileCount = 1;

                delete []m_pFiles;

                m_pFiles = new FileInfo[m_uFileCount];
                lstrcpyn(m_pFiles[0].szPath, szPath, MAX_PATH);

                return S_OK;
            }
            else
            {
                return E_INVALIDARG;
            }
        }
    }
    else
    {
        FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stg = {TYMED_HGLOBAL};
        HDROP hDrop;
        HRESULT hResult = E_INVALIDARG;

        if(SUCCEEDED(pdtobj->GetData(&fmt, &stg)))
        {
            hDrop = (HDROP)GlobalLock(stg.hGlobal);

            if(hDrop != NULL)
            {
                UINT uIndex;
                UINT uNumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

                if(uNumFiles > 0)
                {
                    delete []m_pFiles;

                    m_pFiles = new FileInfo[uNumFiles];
                    m_uFileCount = uNumFiles;

                    for(uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                    {
                        DragQueryFile(hDrop, uIndex, m_pFiles[uIndex].szPath, MAX_PATH);

                        DWORD dwAttribute = GetFileAttributes(m_pFiles[uIndex].szPath);

                        if(dwAttribute != INVALID_FILE_ATTRIBUTES && (dwAttribute & FILE_ATTRIBUTE_DIRECTORY) == 0)
                        {
                            LPCTSTR lpExtension = PathFindExtension(m_pFiles[uIndex].szPath);

                            if(lstrcmpi(lpExtension, TEXT(".dll")) == 0 || lstrcmpi(lpExtension, TEXT(".ocx")) == 0)
                            {
                                m_pFiles[uIndex].bIsDll = TRUE;
                            }
                            else if(lstrcmpi(lpExtension, TEXT(".jpg")) == 0 || lstrcmpi(lpExtension, TEXT(".jpeg")) == 0)
                            {
                                m_pFiles[uIndex].bIsJpg = TRUE;
                                m_pFiles[uIndex].bIsPicture = TRUE;
                            }
                            else if (lstrcmpi(lpExtension, TEXT(".gif")) == 0 ||
                                     lstrcmpi(lpExtension, TEXT(".bmp")) == 0 ||
                                     lstrcmpi(lpExtension, TEXT(".png")) == 0 ||
                                     lstrcmpi(lpExtension, TEXT(".dib")) == 0 ||
                                     lstrcmpi(lpExtension, TEXT(".tif")) == 0 ||
                                     lstrcmpi(lpExtension, TEXT(".tiff")) == 0)
                            {
                                m_pFiles[uIndex].bIsPicture = TRUE;
                            }
                            else if(lstrcmpi(lpExtension, TEXT(".rar")) == 0)
                            {
                                m_pFiles[uIndex].bIsRar = TRUE;
                            }

                            m_pFiles[uIndex].bIsFile = TRUE;
                        }
                    }

                    hResult = S_OK;
                }
            }

            ReleaseStgMedium(&stg);
        }

        return hResult;
    }
}

#define ID_REGISTER             1
#define ID_UNREGISTER           2
#define ID_COMBINE              3
#define ID_COPYFULLPATH         4
#define ID_APPPATH              5
#define ID_UNLOCKFILE           6
#define ID_TRYRUN               7
#define ID_TRYRUNWITHARGUMENTS  8
#define ID_RUNCMDHERE           9
#define ID_OPENWITHNOTEPAD      10
#define ID_KILLEXPLORER         11
#define ID_MANUALCHECKSIGNATURE 12
#define ID_UNESCAPE             13
#define ID_DRV_INSTALL          14
#define ID_DRV_START            15
#define ID_DRV_STOP             16
#define ID_DRV_UNINSTALL        17
#define ID_COPY_PICTURE_HTML    18
#define IDCOUNT                 19

//IContextMenu
STDMETHODIMP CSlxComContextMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    UINT uMenuIndex = 0;
    UINT uFileIndex;

    if(m_uFileCount == 0)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }
    else if (m_uFileCount == 1)
    {
        int nCount = GetMenuItemCount(hmenu);
        int nMenuIndex = 0;

        for (; nMenuIndex < nCount; nMenuIndex += 1)
        {
            TCHAR szText[100] = TEXT("");

            GetMenuString(hmenu, nMenuIndex, szText, RTL_NUMBER_OF(szText), MF_BYPOSITION);

            if (lstrcmpi(szText, TEXT("刷新(&E)")) == 0)
            {
                m_bIsBackground = TRUE;
                break;
            }
        }
    }

    BOOL bShiftDown = GetKeyState(VK_LSHIFT) < 0 || GetKeyState(VK_RSHIFT) < 0;

    //Check Dll And File
    BOOL bExistDll = FALSE;
    BOOL bExistFile = FALSE;
    BOOL bExistCom = FALSE;
    BOOL bExistPicture = FALSE;

    for(uFileIndex = 0; uFileIndex < m_uFileCount; uFileIndex += 1)
    {
        if(!bExistDll && m_pFiles[uFileIndex].bIsDll)
        {
            bExistDll = TRUE;
        }

        if(!bExistFile && m_pFiles[uFileIndex].bIsFile)
        {
            bExistFile = TRUE;
        }

        if (!bExistCom && PeIsCOMModule(m_pFiles[uFileIndex].szPath))
        {
            bExistCom = TRUE;
        }

        if (!bExistPicture && m_pFiles[uFileIndex].bIsPicture)
        {
            bExistPicture = TRUE;
        }

        if(bExistDll * bExistFile * bExistCom * bExistPicture)
        {
            break;
        }
    }

    //clipboard functions
    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, TEXT(""));

    //CopyFilePath
    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COPYFULLPATH, TEXT("复制完整路径"));
    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COPYFULLPATH, MF_BYCOMMAND, g_hCopyFullPathBmp, g_hCopyFullPathBmp);

    //CopyPictureAsHtml
    if (bExistPicture)
    {
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COPY_PICTURE_HTML, TEXT("复制图片（QQ）"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COPY_PICTURE_HTML, MF_BYCOMMAND, g_hCopyPictureHtmlBmp, g_hCopyPictureHtmlBmp);
    }

    //Dll Register
    if(bExistCom)
    {
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_REGISTER, TEXT("注册组件(&R)"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_REGISTER, MF_BYCOMMAND, g_hInstallBmp, g_hInstallBmp);

        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNREGISTER, TEXT("取消注册组件(&U)"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNREGISTER, MF_BYCOMMAND, g_hUninstallBmp, g_hUninstallBmp);
    }

    //File Combine
    if(TRUE)
    {
        BOOL bCouldCombine = FALSE;

        if(m_uFileCount == 1)
        {
            bCouldCombine = m_pFiles[0].bIsRar;
        }
        else if(m_uFileCount == 2)
        {
            bCouldCombine = m_pFiles[0].bIsJpg && m_pFiles[1].bIsRar || m_pFiles[0].bIsRar && m_pFiles[1].bIsJpg;
        }

        if(bCouldCombine)
        {
            InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COMBINE, TEXT("文件合成(&C)"));
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COMBINE, MF_BYCOMMAND, g_hCombineBmp, g_hCombineBmp);
        }
    }

    //Check Signature
    if(bExistFile)
    {
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_MANUALCHECKSIGNATURE, TEXT("校验数字签名"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_MANUALCHECKSIGNATURE, MF_BYCOMMAND, g_hManualCheckSignatureBmp, g_hManualCheckSignatureBmp);
    }

    if(m_uFileCount == 1)
    {
        DWORD dwFileAttribute = GetFileAttributes(m_pFiles[0].szPath);

        if(dwFileAttribute != INVALID_FILE_ATTRIBUTES)
        {
            if((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                if(bShiftDown)
                {
                    HMENU hPopupMenu = CreatePopupMenu();

                    if(hPopupMenu != NULL)
                    {
                        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)hPopupMenu, TEXT("尝试运行"));
                        SetMenuItemBitmaps(hmenu, indexMenu + uMenuIndex - 1, MF_BYPOSITION, g_hTryRunBmp, g_hTryRunBmp);

                        //Try to run
                        InsertMenu(hPopupMenu, 1, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_TRYRUN, TEXT("直接运行"));
                        SetMenuItemBitmaps(hPopupMenu, idCmdFirst + ID_TRYRUN, MF_BYCOMMAND, g_hTryRunBmp, g_hTryRunBmp);

                        InsertMenu(hPopupMenu, 2, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_TRYRUNWITHARGUMENTS, TEXT("附带参数运行(&P)"));
                        SetMenuItemBitmaps(hPopupMenu, idCmdFirst + ID_TRYRUNWITHARGUMENTS, MF_BYCOMMAND, g_hTryRunWithArgumentsBmp, g_hTryRunWithArgumentsBmp);

                        DestroyMenu(hPopupMenu);
                    }
                }

                //OpenWithNotepad
                InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_OPENWITHNOTEPAD, TEXT("用记事本打开"));
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_OPENWITHNOTEPAD, MF_BYCOMMAND, g_hOpenWithNotepadBmp, g_hOpenWithNotepadBmp);

                //Unescape
                TCHAR szUnescapedFileName[MAX_PATH];

                if(TryUnescapeFileName(m_pFiles[0].szPath, szUnescapedFileName, sizeof(szUnescapedFileName) / sizeof(TCHAR)))
                {
                    TCHAR szCommandText[MAX_PATH + 100];

                    wnsprintf(
                        szCommandText,
                        sizeof(szCommandText) / sizeof(TCHAR),
                        TEXT("重命名为“%s”"),
                        PathFindFileName(szUnescapedFileName)
                        );

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNESCAPE, szCommandText);
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNESCAPE, MF_BYCOMMAND, g_hUnescapeBmp, g_hUnescapeBmp);
                }

                //App path
                TCHAR szCommandInReg[MAX_PATH] = TEXT("");

                ModifyAppPath_GetFileCommand(m_pFiles[0].szPath, szCommandInReg, sizeof(szCommandInReg) / sizeof(TCHAR));

                if (bShiftDown ||
                    lstrcmpi(PathFindExtension(m_pFiles[0].szPath), TEXT(".exe")) == 0 ||
                    lstrlen(szCommandInReg) > 0
                    )
                {
                    TCHAR szMenuText[MAX_PATH + 1000] = TEXT("添加快捷短语");

                    if (lstrlen(szCommandInReg) > 0)
                    {
                        wnsprintf(
                            szMenuText,
                            sizeof(szMenuText) / sizeof(TCHAR),
                            TEXT("修改快捷短语“%s”"),
                            szCommandInReg
                            );
                    }

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_APPPATH, szMenuText);
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_APPPATH, MF_BYCOMMAND, g_hAppPathBmp, g_hAppPathBmp);
                }

                //Driver
                if (bShiftDown &&
                    lstrcmpi(PathFindExtension(m_pFiles[0].szPath), TEXT(".dll")) == 0 ||
                    lstrcmpi(PathFindExtension(m_pFiles[0].szPath), TEXT(".sys")) == 0
                    )
                {
                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_INSTALL, TEXT("驱动程序-安装"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_INSTALL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_START, TEXT("驱动程序-启动"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_START, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_STOP, TEXT("驱动程序-停止"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_STOP, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_UNINSTALL, TEXT("驱动程序-卸载"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_UNINSTALL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);
                }
            }

            if((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                //RunCmdHere
                InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_RUNCMDHERE, TEXT("在此处运行命令行"));
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_RUNCMDHERE, MF_BYCOMMAND, g_hRunCmdHereBmp, g_hRunCmdHereBmp);
            }

            if ((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0 && bShiftDown ||
                (dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) == 0 && (bShiftDown || IsFileDenyed(m_pFiles[0].szPath)))
            {
                //UnlockFile
                InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNLOCKFILE, TEXT("查看锁定情况"));
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNLOCKFILE, MF_BYCOMMAND, g_hUnlockFileBmp, g_hUnlockFileBmp);
            }
        }
    }

    if(bShiftDown)
    {
        //结束Explorer
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_KILLEXPLORER, TEXT("结束Explorer进程"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_KILLEXPLORER, MF_BYCOMMAND, g_hKillExplorerBmp, g_hKillExplorerBmp);
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, IDCOUNT);
}

STDMETHODIMP CSlxComContextMenu::GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
    LPCSTR lpText = "";

    if(idCmd == ID_REGISTER)
    {
        lpText = "注册组件。";
    }
    else if(idCmd == ID_UNREGISTER)
    {
        lpText = "取消注册组件。";
    }
    else if(idCmd == ID_COMBINE)
    {
        if(m_uFileCount == 1)
        {
            lpText = "将选定的rar文件附加到默认jpg文件之后。";
        }
        else
        {
            lpText = "将选定的rar文件附加到选定的jpg文件之后。";
        }
    }
    else if(idCmd == ID_COPYFULLPATH)
    {
        lpText = "复制选中的文件或文件夹的完整路径到剪贴板。";
    }
    else if(idCmd == ID_APPPATH)
    {
        lpText = "维护在“运行”对话框中快速启动的条目。";
    }
    else if(idCmd == ID_UNLOCKFILE)
    {
        lpText = "查看文件夹或文件是否锁定和解除这些锁定。";
    }
    else if(idCmd == ID_TRYRUN)
    {
        lpText = "尝试将文件作为可执行文件运行。";
    }
    else if(idCmd == ID_TRYRUNWITHARGUMENTS)
    {
        lpText = "尝试将文件作为可执行文件运行，并附带命令行参数。";
    }
    else if(idCmd == ID_RUNCMDHERE)
    {
        lpText = "在当前目录启动命令行。";
    }
    else if(idCmd == ID_OPENWITHNOTEPAD)
    {
        lpText = "在记事本打开当前文件。";
    }
    else if(idCmd == ID_KILLEXPLORER)
    {
        lpText = "结束Explorer，随后系统将自动重新启动Explorer。";
    }
    else if(idCmd == ID_MANUALCHECKSIGNATURE)
    {
        lpText = "手动校验选中的文件的数字签名。";
    }
    else if(idCmd == ID_UNESCAPE)
    {
        lpText = "转化文件名为较合适的形式。";
    }
    else if (idCmd == ID_COPY_PICTURE_HTML)
    {
        lpText = "将图片内容复制到剪贴板，支持在QQ上粘贴。";
    }

    if(uFlags & GCS_UNICODE)
    {
        wnsprintfW((LPWSTR)pszName, cchMax, L"%hs", lpText);
    }
    else
    {
        wnsprintfA(pszName, cchMax, "%hs", lpText);
    }

    return S_OK;
}

STDMETHODIMP CSlxComContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    if(0 != HIWORD(pici->lpVerb))
    {
        return E_INVALIDARG;
    }

    if((GetKeyState(VK_LSHIFT) < 0 || GetKeyState(VK_RSHIFT) < 0))
    {
        ConvertToShortPaths();
    }

    switch(LOWORD(pici->lpVerb))
    {
    case ID_REGISTER:
    case ID_UNREGISTER:
    {
        if (m_uFileCount == 1)
        {
            TCHAR szArguments[MAX_PATH * 2];

            wnsprintf(
                szArguments,
                RTL_NUMBER_OF(szArguments),
                TEXT("%s \"%s\""),
                LOWORD(pici->lpVerb) == ID_REGISTER ? TEXT("") : TEXT("/u"),
                m_pFiles[0].szPath);

            ElevateAndRun(TEXT("regsvr32.exe"), szArguments, NULL, SW_SHOW);
        }
        else
        {
            tstringstream ssArguments;

            TCHAR szDllpath[MAX_PATH];

            GetModuleFileName(g_hinstDll, szDllpath, RTL_NUMBER_OF(szDllpath));
            PathQuoteSpaces(szDllpath);

            ssArguments<<szDllpath;

            if (LOWORD(pici->lpVerb) == ID_REGISTER)
            {
                ssArguments<<TEXT(" BatchRegisterDlls");
            }
            else
            {
                ssArguments<<TEXT(" BatchUnregisterDlls");
            }

            for (DWORD dwIndex = 0; dwIndex < m_uFileCount; ++dwIndex)
            {
                ssArguments<<TEXT(" \"");
                ssArguments<<m_pFiles[dwIndex].szPath;
                ssArguments<<TEXT("\"");
            }

            ElevateAndRun(TEXT("rundll32.exe"), ssArguments.str().c_str(), NULL, SW_SHOW);
        }

        break;
    }

    case ID_COMBINE:
    {
        BOOL bSucceed = FALSE;
        TCHAR szResultPath[MAX_PATH];

        if(m_uFileCount == 1)
        {
            bSucceed = CombineFile(m_pFiles[0].szPath, NULL, szResultPath, MAX_PATH);
        }
        else if(m_uFileCount == 2)
        {
            if(m_pFiles[0].bIsRar)
            {
                bSucceed = CombineFile(m_pFiles[0].szPath, m_pFiles[1].szPath, szResultPath, MAX_PATH);
            }
            else
            {
                bSucceed = CombineFile(m_pFiles[1].szPath, m_pFiles[0].szPath, szResultPath, MAX_PATH);
            }
        }

        if(!bSucceed)
        {
            MessageBox(pici->hwnd, TEXT("合成文件出错"), NULL, MB_ICONERROR | MB_TOPMOST);
        }

        break;
    }

    case ID_APPPATH:
        ModifyAppPath(m_pFiles[0].szPath);
        break;

    case ID_UNLOCKFILE:
    {
        TCHAR szDllPath[MAX_PATH];
        TCHAR szCommand[MAX_PATH * 2];
        STARTUPINFO si = {sizeof(si)};
        PROCESS_INFORMATION pi;

        GetModuleFileName(g_hinstDll, szDllPath, sizeof(szDllPath) / sizeof(TCHAR));
        wnsprintf(
            szCommand,
            sizeof(szCommand) / sizeof(TCHAR),
            TEXT("rundll32.exe \"%s\" UnlockFileFromPath %s"),
            szDllPath,
            m_pFiles[0].szPath
            );

        if (CreateProcess(
            NULL,
            szCommand,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi
            ))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else
        {
            MessageBoxFormat(
                pici->hwnd,
                NULL,
                MB_ICONERROR,
                TEXT("启动外部程序失败，命令行：\r\n\r\n%s"),
                szCommand
                );
        }

        break;
    }

    case ID_COPYFULLPATH:
    {
        if(m_uFileCount == 1)
        {
            SetClipboardText(m_pFiles[0].szPath);
        }
        else if(m_uFileCount > 1)
        {
            TCHAR *szText = new TCHAR[MAX_PATH * m_uFileCount];

            if(szText != NULL)
            {
                UINT uFileIndex = 0;

                szText[0] = TEXT('\0');

                for(; uFileIndex < m_uFileCount; uFileIndex += 1)
                {
                    lstrcat(szText, m_pFiles[uFileIndex].szPath);
                    lstrcat(szText, TEXT("\r\n"));
                }

                SetClipboardText(szText);

                delete []szText;
            }
        }

        break;
    }

    case ID_TRYRUN:
    {
        TCHAR szCommandLine[MAX_PATH + 100];

        wnsprintf(szCommandLine, sizeof(szCommandLine) / sizeof(TCHAR), TEXT("\"%s\""), m_pFiles[0].szPath);

        if(IDYES == MessageBox(pici->hwnd, TEXT("确定要尝试将此文件作为可执行文件运行吗？"), TEXT("请确认"), MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3))
        {
            if(!RunCommand(szCommandLine))
            {
                TCHAR szErrorMessage[MAX_PATH + 300];

                wnsprintf(szErrorMessage, sizeof(szErrorMessage) / sizeof(TCHAR), TEXT("无法启动进程，错误码%lu\r\n\r\n%s"),
                    GetLastError(), szCommandLine);

                MessageBox(pici->hwnd, szErrorMessage, NULL, MB_ICONERROR | MB_TOPMOST);
            }
        }

        break;
    }

    case ID_TRYRUNWITHARGUMENTS:
    {
        RunCommandWithArguments(m_pFiles[0].szPath);

        break;
    }

    case ID_RUNCMDHERE:
    {
        TCHAR szApplication[MAX_PATH * 2] = TEXT("");
        TCHAR szArgument[MAX_PATH * 2] = TEXT("/k pushd ");

        GetEnvironmentVariable(TEXT("ComSpec"), szApplication, MAX_PATH);
        StrCatBuff(szArgument, m_pFiles[0].szPath, RTL_NUMBER_OF(szArgument));

        if (GetKeyState(VK_CONTROL) < 0 && g_bVistaLater && !g_bElevated)
        {
            RunCommandEx(szApplication, szArgument, NULL, TRUE);
        }
        else
        {
            RunCommandEx(szApplication, szArgument, NULL, FALSE);
        }

        break;
    }

    case ID_OPENWITHNOTEPAD:
    {
        TCHAR szCommandLine[MAX_PATH * 2] = TEXT("");

        GetSystemDirectory(szCommandLine, MAX_PATH);
        PathAppend(szCommandLine, TEXT("\\notepad.exe"));

        if(GetFileAttributes(szCommandLine) == INVALID_FILE_ATTRIBUTES)
        {
            GetWindowsDirectory(szCommandLine, MAX_PATH);
            PathAppend(szCommandLine, TEXT("\\notepad.exe"));
        }

        lstrcat(szCommandLine, TEXT(" \""));
        lstrcat(szCommandLine, m_pFiles[0].szPath);
        lstrcat(szCommandLine, TEXT("\""));

        if(!RunCommand(szCommandLine))
        {
            TCHAR szErrorMessage[MAX_PATH + 300];

            wnsprintf(szErrorMessage, sizeof(szErrorMessage) / sizeof(TCHAR), TEXT("无法启动进程，错误码%lu\r\n\r\n%s"),
                GetLastError(), szCommandLine);

            MessageBox(pici->hwnd, szErrorMessage, NULL, MB_ICONERROR | MB_TOPMOST);
        }

        break;
    }

    case ID_KILLEXPLORER:
    {
        DWORD dwTimeMark = GetTickCount();
        TCHAR szPath[MAX_PATH];

        lstrcpyn(szPath, m_pFiles[0].szPath, RTL_NUMBER_OF(szPath));

        if (m_bIsBackground)
        {
            PathAppend(szPath, TEXT("\\*"));

            BOOL bFound = FALSE;
            WIN32_FIND_DATA wfd;
            HANDLE hFind = FindFirstFile(szPath, &wfd);

            if (hFind != NULL && hFind != INVALID_HANDLE_VALUE)
            {
                do 
                {
                    if (lstrcmpi(wfd.cFileName, TEXT(".")) != 0 &&
                        lstrcmpi(wfd.cFileName, TEXT("..")) != 0
                        )
                    {
                        PathRemoveFileSpec(szPath);
                        StrCatBuff(szPath, TEXT("\\"), RTL_NUMBER_OF(szPath));
                        StrCatBuff(szPath, wfd.cFileName, RTL_NUMBER_OF(szPath));
                        bFound = TRUE;

                        break;
                    }

                } while (FindNextFile(hFind, &wfd));

                FindClose(hFind);
            }

            if (!bFound)
            {
                PathRemoveFileSpec(szPath);
            }
        }

        SHSetTempValue(
            HKEY_CURRENT_USER,
            TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"),
            TEXT("SlxLastPath"),
            REG_SZ,
            szPath,
            (lstrlen(szPath) + 1) * sizeof(TCHAR)
            );

        SHSetTempValue(
            HKEY_CURRENT_USER,
            TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"),
            TEXT("SlxLastPathTime"),
            REG_DWORD,
            &dwTimeMark,
            sizeof(dwTimeMark)
            );

        ResetExplorer();

        break;
    }

    case ID_MANUALCHECKSIGNATURE:
    {
        if(m_hManualCheckSignatureThread == NULL)
        {
            WaitForSingleObject(g_hManualCheckSignatureMutex, INFINITE);

            if(m_hManualCheckSignatureThread == NULL)
            {
                m_hManualCheckSignatureThread = CreateThread(NULL, 0, ManualCheckSignatureThreadProc, NULL, 0, NULL);
            }

            ReleaseMutex(g_hManualCheckSignatureMutex);
        }

        if(m_hManualCheckSignatureThread == NULL)
        {
            MessageBox(pici->hwnd, TEXT("启动数字签名校验组件失败。"), NULL, MB_ICONERROR);
        }
        else
        {
#if _MSC_VER > 1200
            map<tstring, HWND> *pMapPaths = new (std::nothrow) map<tstring, HWND>;
#else
            map<tstring, HWND> *pMapPaths = new map<tstring, HWND>;
#endif
            if(pMapPaths != NULL)
            {
                for(DWORD dwIndex = 0; dwIndex < m_uFileCount; dwIndex += 1)
                {
                    if(m_pFiles[dwIndex].bIsFile)
                    {
                        pMapPaths->insert(make_pair(tstring(m_pFiles[dwIndex].szPath), HWND(NULL)));
                    }
                }

                if(pMapPaths->empty())
                {
                    delete pMapPaths;

                    MessageBox(pici->hwnd, TEXT("选中的项目不直接包含任何文件。"), NULL, MB_ICONERROR);
                }
                else
                {
                    DialogBoxParam(
                        g_hinstDll,
                        MAKEINTRESOURCE(IDD_MANUALCHECKSIGNATURE_DIALOG),
                        pici->hwnd,
                        ManualCheckSignatureDialogProc,
                        (LPARAM)pMapPaths
                        );
                }
            }
        }

        break;
    }

    case ID_UNESCAPE:
    {
        TCHAR szUnescapedFileName[MAX_PATH];
        TCHAR szMsgText[MAX_PATH + 1000];

        if(TryUnescapeFileName(m_pFiles[0].szPath, szUnescapedFileName, sizeof(szUnescapedFileName) / sizeof(TCHAR)))
        {
            if(PathFileExists(szUnescapedFileName))
            {
                wnsprintf(
                    szMsgText,
                    sizeof(szMsgText) / sizeof(TCHAR),
                    TEXT("“%s”已存在，是否要先删除？"),
                    szUnescapedFileName
                    );

                if(IDYES == MessageBox(
                    pici->hwnd,
                    szMsgText,
                    TEXT("请确认"),
                    MB_ICONQUESTION | MB_YESNOCANCEL
                    ))
                {
                    if(!DeleteFile(szUnescapedFileName))
                    {
                        MessageBox(pici->hwnd, TEXT("删除失败。"), NULL, MB_ICONERROR);

                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if(!MoveFile(m_pFiles[0].szPath, szUnescapedFileName))
            {
                wnsprintf(
                    szMsgText,
                    sizeof(szMsgText) / sizeof(TCHAR),
                    TEXT("操作失败。\r\n\r\n错误码：%lu"),
                    GetLastError()
                    );

                MessageBox(pici->hwnd, szMsgText, NULL, MB_ICONERROR);
            }
        }

        break;
    }

    case ID_DRV_INSTALL:
        DrvAction(pici->hwnd, m_pFiles[0].szPath, DA_INSTALL);
        break;

    case ID_DRV_START:
        DrvAction(pici->hwnd, m_pFiles[0].szPath, DA_START);
        break;

    case ID_DRV_STOP:
        DrvAction(pici->hwnd, m_pFiles[0].szPath, DA_STOP);
        break;

    case ID_DRV_UNINSTALL:
        DrvAction(pici->hwnd, m_pFiles[0].szPath, DA_UNINSTALL);
        break;

    case ID_COPY_PICTURE_HTML:
    {
        TCHAR *lpBuffer = (TCHAR *)malloc(m_uFileCount * MAX_PATH * sizeof(TCHAR));

        if (lpBuffer != NULL)
        {
            UINT uFileIndex = 0;
            TCHAR *lpPaths = lpBuffer;

            for (; uFileIndex < m_uFileCount; uFileIndex += 1)
            {
                if (m_pFiles[uFileIndex].bIsPicture)
                {
                    lstrcpyn(lpPaths, m_pFiles[uFileIndex].szPath, MAX_PATH);
                    lpPaths += (lstrlen(lpPaths) + 1);
                }
            }

            lpPaths[0] = TEXT('\0');

            SetClipboardPicturePathsByHtml(lpBuffer);

            free((void *)lpBuffer);
        }

        break;
    }

    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
#define DEFAULT_MAP_SIZE    (4 * 1024 * 1024)
#define STATUS_COMPUTING    TEXT("计算中...")
#define STATUS_HALTED       TEXT("计算过程已被中止。")
#define STATUS_ERROR        TEXT("计算中遇到错误。")

enum
{
    WM_ADDCOMPARE = WM_USER + 24,
    WM_STARTCALC,
};

struct HashPropSheetDlgParam
{
    TCHAR szFilePath[MAX_PATH];
    TCHAR szFilePath_2[MAX_PATH];

    HashPropSheetDlgParam()
    {
        ZeroMemory(this, sizeof(*this));
    }
};

struct HashCalcProcParam
{
    TCHAR szFile[MAX_PATH];
    HWND hwndDlg;
    HWND hwndFileSize;
    HWND hwndMd5;
    HWND hwndSha1;
    HWND hwndCrc32;
    HWND hwndStop;
};

DWORD CALLBACK CSlxComContextMenu::HashCalcProc(LPVOID lpParam)
{
    TCHAR szFilePath_Debug[MAX_PATH] = TEXT("");
    HashCalcProcParam *pCalcParam = (HashCalcProcParam *)lpParam;

    if (pCalcParam != NULL &&
        IsWindow(pCalcParam->hwndStop) &&
        GetWindowLongPtr(pCalcParam->hwndStop, GWLP_USERDATA) == GetCurrentThreadId()
        )
    {
        //
        lstrcpyn(szFilePath_Debug, pCalcParam->szFile, RTL_NUMBER_OF(szFilePath_Debug));

        //提取文件大小
        WIN32_FILE_ATTRIBUTE_DATA wfad = {0};
        ULARGE_INTEGER uliFileSize;
        TCHAR szSizeStringWithBytes[128];

        GetFileAttributesEx(pCalcParam->szFile, GetFileExInfoStandard, &wfad);
        uliFileSize.HighPart = wfad.nFileSizeHigh;
        uliFileSize.LowPart = wfad.nFileSizeLow;

        if (uliFileSize.QuadPart == 0)
        {
            SetWindowText(pCalcParam->hwndFileSize, TEXT("0 字节"));
        }
        else
        {
            lstrcpyn(szSizeStringWithBytes + 64, TEXT(" 字节"), 64);
            TCHAR *pChar = szSizeStringWithBytes + 64 - 1;
            int index = 0;

            while (TRUE)
            {
                *pChar-- = TEXT('0') + uliFileSize.QuadPart % 10;
                uliFileSize.QuadPart /= 10;
                index += 1;

                if (uliFileSize.QuadPart == 0)
                {
                    break;
                }

                if (index % 3 == 0)
                {
                    *pChar-- = TEXT(',');
                }
            }

            SetWindowText(pCalcParam->hwndFileSize, pChar + 1);
        }

        uliFileSize.HighPart = wfad.nFileSizeHigh;
        uliFileSize.LowPart = wfad.nFileSizeLow;

        //设定初始显示文本
        TCHAR szMd5[100] = STATUS_COMPUTING;
        TCHAR szSha1[100] = STATUS_COMPUTING;
        TCHAR szCrc32[100] = STATUS_COMPUTING;

        SetWindowText(pCalcParam->hwndMd5, szMd5);
        SetWindowText(pCalcParam->hwndSha1, szSha1);
        SetWindowText(pCalcParam->hwndCrc32, szCrc32);

        //设置停止按钮文本，超过一定时间未计算完毕则显示按钮
        BOOL bShowStop = FALSE;
        DWORD dwTickCountMark = GetTickCount();
        const DWORD dwTimeToShowStop = 987;

        SetWindowText(pCalcParam->hwndStop, TEXT("停止计算(已完成0%)"));

        //密码库Init
        md5_ctx md5;
        sha1_ctx sha1;
        crc32_ctx crc32;

        md5_init(&md5);
        sha1_init(&sha1);
        crc32_init(&crc32);

        //开始计算
        BOOL bError = FALSE;
        if (uliFileSize.QuadPart > 0)
        {
            HANDLE hFile = CreateFile(pCalcParam->szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

            if (hFile != INVALID_HANDLE_VALUE)
            {
                HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

                if (hFileMapping != NULL)
                {
                    ULARGE_INTEGER uliOffset = {0};
                    ULARGE_INTEGER uliBytesLeft = {0};

                    uliBytesLeft.QuadPart = uliFileSize.QuadPart;

                    while (uliOffset.QuadPart < uliFileSize.QuadPart)
                    {
                        DWORD dwAllocationBaseSize = DEFAULT_MAP_SIZE;
                        DWORD dwBytesToMap = dwAllocationBaseSize;

                        if (uliBytesLeft.QuadPart < dwBytesToMap)
                        {
                            dwBytesToMap = uliBytesLeft.LowPart;
                        }

                        LPVOID lpBuffer = MapViewOfFile(
                            hFileMapping,
                            FILE_MAP_READ,
                            uliOffset.HighPart,
                            uliOffset.LowPart,
                            dwBytesToMap
                            );

                        if (lpBuffer == NULL)
                        {
                            bError = TRUE;
                            break;
                        }

                        md5_update(&md5, (const unsigned char *)lpBuffer, dwBytesToMap);
                        sha1_update(&sha1, (const unsigned char *)lpBuffer, dwBytesToMap);
                        crc32_update(&crc32, (const unsigned char *)lpBuffer, dwBytesToMap);

                        UnmapViewOfFile(lpBuffer);

                        uliOffset.QuadPart += dwBytesToMap;
                        uliBytesLeft.QuadPart -= dwBytesToMap;

                        //进度信息
                        if (IsWindow(pCalcParam->hwndStop) &&
                            GetWindowLongPtr(pCalcParam->hwndStop, GWLP_USERDATA) == GetCurrentThreadId()
                            )
                        {
                            TCHAR szCtrlText[100] = TEXT("");

                            wnsprintf(szCtrlText, RTL_NUMBER_OF(szCtrlText), TEXT("停止计算(已完成%u%%)"), uliOffset.QuadPart * 100 / uliFileSize.QuadPart);
                            SetWindowText(pCalcParam->hwndStop, szCtrlText);

                            if (!bShowStop && GetTickCount() - dwTickCountMark > dwTimeToShowStop)
                            {
                                bShowStop = TRUE;
                                ShowWindow(pCalcParam->hwndStop, SW_SHOW);
                            }
                        }
                        else
                        {
                            bError = TRUE;
                            break;
                        }
                    }

                    CloseHandle(hFileMapping);
                }
                else
                {
                    bError = TRUE;
                }

                CloseHandle(hFile);
            }
            else
            {
                bError = TRUE;
            }
        }

        //密码库Final
        if (!bError)
        {
            int index;
            uint8_t uMd5[16];
            uint8_t uSha1[20];
            uint32_t uCrc32;

            md5_final(uMd5, &md5);
            sha1_final(uSha1, &sha1);
            crc32_final(&uCrc32, &crc32);

            for (index = 0; index < RTL_NUMBER_OF(uMd5); index += 1)
            {
                wnsprintf(szMd5 + index * 2, 3, TEXT("%02x"), uMd5[index]);
            }

            for (index = 0; index < RTL_NUMBER_OF(uSha1); index += 1)
            {
                wnsprintf(szSha1 + index * 2, 3, TEXT("%02x"), uSha1[index]);
            }

            wnsprintf(szCrc32, RTL_NUMBER_OF(szCrc32), TEXT("%08x"), uCrc32);
        }
        else
        {
            lstrcpyn(szMd5, STATUS_ERROR, RTL_NUMBER_OF(szMd5));
            lstrcpyn(szSha1, STATUS_ERROR, RTL_NUMBER_OF(szSha1));
            lstrcpyn(szCrc32, STATUS_ERROR, RTL_NUMBER_OF(szCrc32));
        }

        if (IsWindow(pCalcParam->hwndStop) &&
            GetWindowLongPtr(pCalcParam->hwndStop, GWLP_USERDATA) == GetCurrentThreadId()
            )
        {
            if (!!GetWindowLongPtr(pCalcParam->hwndDlg, GWLP_USERDATA))
            {
                CharUpperBuff(szMd5, RTL_NUMBER_OF(szMd5));
                CharUpperBuff(szSha1, RTL_NUMBER_OF(szSha1));
                CharUpperBuff(szCrc32, RTL_NUMBER_OF(szCrc32));
            }

            SetWindowText(pCalcParam->hwndMd5, szMd5);
            SetWindowText(pCalcParam->hwndSha1, szSha1);
            SetWindowText(pCalcParam->hwndCrc32, szCrc32);
            ShowWindow(pCalcParam->hwndStop, SW_HIDE);
        }

        free(pCalcParam);
    }

    SafeDebugMessage(TEXT("HashCalcProc线程%lu结束，文件“%s”\n"), GetCurrentThreadId(), szFilePath_Debug);
    return 0;
}

INT_PTR CALLBACK CSlxComContextMenu::HashPropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RECT rect;
        LPPROPSHEETPAGE pPsp = (LPPROPSHEETPAGE)lParam;
        HashPropSheetDlgParam *pPropSheetDlgParam = (HashPropSheetDlgParam *)pPsp->lParam;

        //
        HWND hParent = GetParent(hwndDlg);

        if (IsWindow(hParent))
        {
            HFONT hFont = (HFONT)SendMessage(hParent, WM_GETFONT, 0, 0);

            EnumChildWindows(hwndDlg, EnumChildSetFontProc, (LPARAM)hFont);
        }

        //
        SetDlgItemText(hwndDlg, IDC_FILEPATH, pPropSheetDlgParam->szFilePath);

        if (lstrlen(pPropSheetDlgParam->szFilePath) > 0)
        {
            HashCalcProcParam *pCalcParam = (HashCalcProcParam *)malloc(sizeof(HashCalcProcParam));

            if (pCalcParam != NULL)
            {
                lstrcpyn(pCalcParam->szFile, pPropSheetDlgParam->szFilePath, RTL_NUMBER_OF(pCalcParam->szFile));
                pCalcParam->hwndDlg = hwndDlg;
                pCalcParam->hwndFileSize = GetDlgItem(hwndDlg, IDC_FILESIZE);
                pCalcParam->hwndMd5 = GetDlgItem(hwndDlg, IDC_MD5);
                pCalcParam->hwndSha1 = GetDlgItem(hwndDlg, IDC_SHA1);
                pCalcParam->hwndCrc32 = GetDlgItem(hwndDlg, IDC_CRC32);
                pCalcParam->hwndStop = GetDlgItem(hwndDlg, IDC_STOP);

                PostMessage(hwndDlg, WM_STARTCALC, 0, (LPARAM)pCalcParam);
            }
        }

        if (lstrlen(pPropSheetDlgParam->szFilePath_2) > 0)
        {
            SendMessage(hwndDlg, WM_ADDCOMPARE, (WPARAM)pPropSheetDlgParam->szFilePath_2, 0);
        }

        //IDC_ABOUT的用户数据用来标识对话框的本来宽度
        //hwndDlg的用户数据用来标识哈希值的大小写
        GetClientRect(hwndDlg, &rect);
        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, rect.right - rect.left);
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, FALSE);

        return FALSE;
    }

    case WM_SIZE:
        if (IsWindow(GetDlgItem(hwndDlg, IDC_ABOUT)))
        {
            RECT rectDlg;
            int nOldWidth = (int)GetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA);
            int nAddPart = 0;

            GetClientRect(hwndDlg, &rectDlg);
            nAddPart = rectDlg.right - rectDlg.left - nOldWidth;

            if (nOldWidth != 0 && nAddPart != 0)
            {
                int nIds[] = {
                    IDC_FILEPATH,
                    IDC_FILESIZE,
                    IDC_MD5,
                    IDC_SHA1,
                    IDC_CRC32,
                    IDC_STOP,
                    IDC_FILEPATH_2,
                    IDC_FILESIZE_2,
                    IDC_MD5_2,
                    IDC_SHA1_2,
                    IDC_CRC32_2,
                    IDC_STOP_2,
                    IDC_COMPARE,
                    IDC_ABOUT,
                };
                HDWP hDwp = BeginDeferWindowPos(RTL_NUMBER_OF(nIds));
                RECT rect;

                if (hDwp != NULL)
                {
                    for (int index = 0; index < RTL_NUMBER_OF(nIds); index += 1)
                    {
                        HWND hCtrl = GetDlgItem(hwndDlg, nIds[index]);

                        GetWindowRect(hCtrl, &rect);
                        ScreenToClient(hwndDlg, (LPPOINT)&rect);
                        ScreenToClient(hwndDlg, (LPPOINT)&rect + 1);

                        if (nIds[index] == IDC_COMPARE || nIds[index] == IDC_ABOUT)
                        {
                            DeferWindowPos(hDwp, hCtrl, NULL, rect.left + nAddPart, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                        }
                        else
                        {
                            DeferWindowPos(hDwp, hCtrl, NULL, 0, 0, rect.right - rect.left + nAddPart, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE);
                        }
                    }

                    EndDeferWindowPos(hDwp);
                }
            }
        }
        break;

    case WM_STARTCALC:
    {
        HashCalcProcParam *pCalcParam = (HashCalcProcParam *)lParam;

        if (pCalcParam != NULL)
        {
            DWORD dwThreadId = 0;
            HANDLE hThread = CreateThread(NULL, 0, HashCalcProc, (LPVOID)pCalcParam, CREATE_SUSPENDED, &dwThreadId);

            SetWindowLongPtr(pCalcParam->hwndStop, GWLP_USERDATA, dwThreadId);
            ResumeThread(hThread);
        }

        break;
    }

    case WM_ADDCOMPARE:
    {
        LPCTSTR lpCompareFile = (LPCTSTR)wParam;
        HashCalcProcParam *pCalcParam = (HashCalcProcParam *)malloc(sizeof(HashCalcProcParam));

        SetDlgItemText(hwndDlg, IDC_FILEPATH_2, lpCompareFile);

        if (pCalcParam != NULL)
        {
            lstrcpyn(pCalcParam->szFile, lpCompareFile, RTL_NUMBER_OF(pCalcParam->szFile));
            pCalcParam->hwndDlg = hwndDlg;
            pCalcParam->hwndFileSize = GetDlgItem(hwndDlg, IDC_FILESIZE_2);
            pCalcParam->hwndMd5 = GetDlgItem(hwndDlg, IDC_MD5_2);
            pCalcParam->hwndSha1 = GetDlgItem(hwndDlg, IDC_SHA1_2);
            pCalcParam->hwndCrc32 = GetDlgItem(hwndDlg, IDC_CRC32_2);
            pCalcParam->hwndStop = GetDlgItem(hwndDlg, IDC_STOP_2);

            PostMessage(hwndDlg, WM_STARTCALC, 0, (LPARAM)pCalcParam);
        }

        //更新到双文件界面
        HWND hBegin = GetWindow(GetDlgItem(hwndDlg, IDC_COMPARE), GW_HWNDNEXT);
        HWND hEnd = GetDlgItem(hwndDlg, IDC_STOP_2);

        while (hBegin != hEnd)
        {
            ShowWindow(hBegin, SW_SHOW);
            hBegin = GetWindow(hBegin, GW_HWNDNEXT);
        }

        SetDlgItemText(hwndDlg, IDC_COMPARE, TEXT("更换文件对比(&C)..."));

        break;
    }

    case WM_NOTIFY:
    {
        NMHDR *phdr = (NMHDR *)lParam;

        if (phdr->code == PSN_APPLY)
        {
            OutputDebugString(TEXT("PSN_APPLY"));
        }

        break;
    }

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE)
        {
            InvalidateRect(hwndDlg, NULL, TRUE);
        }

        switch (LOWORD(wParam))
        {
        case IDC_COMPARE:
        {
            OPENFILENAME ofn = {OPENFILENAME_SIZE_VERSION_400};
            TCHAR szFilter[128];
            TCHAR szFilePath[MAX_PATH] = TEXT("");

            wnsprintf(
                szFilter,
                RTL_NUMBER_OF(szFilter),
                TEXT("All files (*.*)%c*.*%c"),
                TEXT('\0'),
                TEXT('\0')
                );

            ofn.hwndOwner = hwndDlg;
            ofn.Flags = OFN_HIDEREADONLY;
            ofn.hInstance = g_hinstDll;
            ofn.lpstrFilter = szFilter;
            ofn.lpstrTitle = TEXT("打开要对比的文件");
            ofn.lpstrFile = szFilePath;
            ofn.nMaxFile = RTL_NUMBER_OF(szFilePath);

            if (GetOpenFileName(&ofn))
            {
                SendMessage(hwndDlg, WM_ADDCOMPARE, (WPARAM)szFilePath, 0);
            }
            break;
        }

        case IDC_STOP:
            SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_STOP), GWLP_USERDATA, 0);
            ShowWindow(GetDlgItem(hwndDlg, IDC_STOP), SW_HIDE);
            SetWindowText(GetDlgItem(hwndDlg, IDC_MD5), STATUS_HALTED);
            SetWindowText(GetDlgItem(hwndDlg, IDC_SHA1), STATUS_HALTED);
            SetWindowText(GetDlgItem(hwndDlg, IDC_CRC32), STATUS_HALTED);
            break;

        case IDC_STOP_2:
            SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_STOP_2), GWLP_USERDATA, 0);
            ShowWindow(GetDlgItem(hwndDlg, IDC_STOP_2), SW_HIDE);
            SetWindowText(GetDlgItem(hwndDlg, IDC_MD5_2), STATUS_HALTED);
            SetWindowText(GetDlgItem(hwndDlg, IDC_SHA1_2), STATUS_HALTED);
            SetWindowText(GetDlgItem(hwndDlg, IDC_CRC32_2), STATUS_HALTED);
            break;

        case IDC_ABOUT:
            SlxComAbout(hwndDlg);
            break;

        default:
            break;
        }
        break;

    case WM_DROPFILES:
        if (wParam != 0)
        {
            HDROP hDrop = (HDROP)wParam;
            int nFileCount = DragQueryFile(hDrop, -1, NULL, 0);

            if (nFileCount != 1)
            {
                MessageBox(hwndDlg, TEXT("仅支持同单个文件进行比较。"), NULL, MB_ICONERROR);
            }
            else
            {
                TCHAR szFilePath[MAX_PATH] = TEXT("");
                DWORD dwAttributes;

                DragQueryFile(hDrop, 0, szFilePath, RTL_NUMBER_OF(szFilePath));
                dwAttributes = GetFileAttributes(szFilePath);

                if (dwAttributes == INVALID_FILE_ATTRIBUTES)
                {
                    MessageBox(hwndDlg, TEXT("待比较的文件不存在。"), NULL, MB_ICONERROR);
                }
                else if (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    MessageBox(hwndDlg, TEXT("仅支持同文件进行对比，而不是文件夹。"), NULL, MB_ICONERROR);
                }
                else
                {
                    SendMessage(hwndDlg, WM_ADDCOMPARE, (WPARAM)szFilePath, 0);
                }
            }

            DragFinish(hDrop);
        }
        return TRUE;

    case WM_LBUTTONDBLCLK:
    {
        int nIds[] = {
            IDC_MD5,
            IDC_SHA1,
            IDC_CRC32,
            IDC_MD5_2,
            IDC_SHA1_2,
            IDC_CRC32_2,
        };
        DWORD (WINAPI *CharChangeBuff[])(LPTSTR lpsz, DWORD cchLength) = {CharUpperBuff, CharLowerBuff};
        BOOL bCaseStatus = !!GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
        TCHAR szBuffer[128];
        int index = 0;

        for (; index < RTL_NUMBER_OF(nIds); index += 1)
        {
            GetDlgItemText(hwndDlg, nIds[index], szBuffer, RTL_NUMBER_OF(szBuffer));
            CharChangeBuff[bCaseStatus](szBuffer, RTL_NUMBER_OF(szBuffer));
            SetDlgItemText(hwndDlg, nIds[index], szBuffer);
        }

        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, !bCaseStatus);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        int nIds[] = {
            IDC_FILESIZE,
            IDC_FILESIZE_2,
            IDC_MD5,
            IDC_MD5_2,
            IDC_SHA1,
            IDC_SHA1_2,
            IDC_CRC32,
            IDC_CRC32_2,
        };
        TCHAR szText[256], szText2[256];
        HDC hDc = (HDC)wParam;
        int index = 0, index2;

        for (; index < RTL_NUMBER_OF(nIds); index += 1)
        {
            if (lParam == (LPARAM)GetDlgItem(hwndDlg, nIds[index]))
            {
                index = index / 2 * 2;
                index2 = index + 1;

                GetDlgItemText(hwndDlg, nIds[index], szText, RTL_NUMBER_OF(szText));
                GetDlgItemText(hwndDlg, nIds[index2], szText2, RTL_NUMBER_OF(szText2));

                if (IsWindowVisible(GetDlgItem(hwndDlg, nIds[index2])) && 0 != lstrcmpi(szText, szText2))
                {
                    SetTextColor(hDc, RGB(0, 0, 99));
                    SetBkMode(hDc, TRANSPARENT);
                }

                return (BOOL)GetStockObject(WHITE_BRUSH);
            }
        }

        break;
    }

    case WM_DESTROY:
        break;

    default:
        break;
    }

    return FALSE;
}

UINT CALLBACK CSlxComContextMenu::HashPropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE pPsp)
{
    switch (uMsg)
    {
    case PSPCB_ADDREF:
        break;

    case PSPCB_CREATE:
        break;

    case PSPCB_RELEASE:
        delete (HashPropSheetDlgParam *)pPsp->lParam;
        break;

    default:
        break;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////
struct FileTimePropSheetDlgParam
{
    FileInfo *pFiles;
    UINT uFileCount;

    FileTimePropSheetDlgParam(FileInfo *pFiles, UINT uFileCount)
    {
        if (uFileCount > 0)
        {
            this->pFiles = (FileInfo *)malloc(uFileCount * sizeof(pFiles[0]));
            this->uFileCount = uFileCount;

            if (this->pFiles != NULL && pFiles != NULL)
            {
                memcpy(this->pFiles, pFiles, uFileCount * sizeof(pFiles[0]));
            }
        }
        else
        {
            pFiles = NULL;
            uFileCount = 0;
        }
    }

    ~FileTimePropSheetDlgParam()
    {
        if (pFiles != NULL)
        {
            free((void *)pFiles);
        }
    }
};

void FileTimeSetToFileObject(
    LPCTSTR lpPath,
    BOOL bIsFile,
    BOOL bSubDirIncluded,
    const FILETIME *pftCreated,
    const FILETIME *pftModified,
    const FILETIME *pftAccessed,
    tstringstream &ssError
    )
{
    if (pftCreated == NULL || pftModified == NULL || pftAccessed == NULL)
    {
        return;
    }

    list<tstring> listDirs;
    list<tstring> listFiles;
    list<tstring>::const_iterator it;

    listFiles.push_back(lpPath);

    if (!bIsFile && bSubDirIncluded)
    {
        tstring strPath = lpPath;
        WIN32_FIND_DATA wfd;
        HANDLE hFind = FindFirstFile((strPath + TEXT("\\*")).c_str(), &wfd);

        if (hFind != NULL && hFind != INVALID_HANDLE_VALUE)
        {
            do 
            {
                if (lstrcmpi(wfd.cFileName, TEXT("..")) != 0 &&
                    lstrcmpi(wfd.cFileName, TEXT(".")) != 0
                    )
                {
                    if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        listDirs.push_back(strPath + TEXT("\\") + wfd.cFileName);
                    }
                    else
                    {
                        listFiles.push_back(strPath + TEXT("\\") + wfd.cFileName);
                    }
                }

            } while (FindNextFile(hFind, &wfd));

            FindClose(hFind);
        }
    }

    for (it = listDirs.begin(); it != listDirs.end(); ++it)
    {
        FileTimeSetToFileObject(
            it->c_str(),
            FALSE,
            bSubDirIncluded,
            pftCreated,
            pftModified,
            pftAccessed,
            ssError
            );
    }

    for (it = listFiles.begin(); it != listFiles.end(); ++it)
    {
        HANDLE hFileObject = INVALID_HANDLE_VALUE;
        FILETIME ftCreated = {0};
        FILETIME ftModified = {0};
        FILETIME ftAccessed = {0};

        if (!bIsFile && it == listFiles.begin())
        {
            hFileObject = CreateFile(it->c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        }
        else
        {
            hFileObject = CreateFile(it->c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        }

        if (hFileObject == INVALID_HANDLE_VALUE)
        {
            ssError<<TEXT("“")<<it->c_str()<<TEXT("”打开文件失败，错误码")<<GetLastError()<<TEXT("\r\n");
        }
        else
        {
            do
            {
                if (pftCreated == NULL || pftModified == NULL || pftAccessed == NULL)
                {
                    if (!GetFileTime(hFileObject, &ftCreated, &ftModified, &ftAccessed))
                    {
                        ssError<<TEXT("“")<<it->c_str()<<TEXT("”获取文件原时间失败，错误码")<<GetLastError()<<TEXT("\r\n");
                        break;
                    }
                }

                if (pftCreated == NULL)
                {
                    pftCreated = &ftCreated;
                }

                if (pftModified == NULL)
                {
                    pftModified = &ftModified;
                }

                if (pftAccessed == NULL)
                {
                    pftAccessed = &ftAccessed;
                }

                if (!SetFileTime(hFileObject, pftCreated, pftModified, pftAccessed))
                {
                    ssError<<TEXT("“")<<it->c_str()<<TEXT("”设置文件新时间失败，错误码")<<GetLastError()<<TEXT("\r\n");
                }

            } while (FALSE);

            CloseHandle(hFileObject);
        }
    }
}

BOOL CSlxComContextMenu::FileTimePropSheetDoSave(HWND hwndDlg, FileInfo *pFiles, UINT uFileCount)
{
    BOOL bSubDirIncluded = IsDlgButtonChecked(hwndDlg, IDC_SUBDIR);
    SYSTEMTIME stNow, stCreated, stModified, stAccessed, stForTime;
    FILETIME ftNow, ftCreated, ftModified, ftAccessed;
    FILETIME *pftCreated = NULL;
    FILETIME *pftModified = NULL;
    FILETIME *pftAccessed = NULL;
    UINT uFileIndex = 0;
    tstringstream ssError;

    GetSystemTime(&stNow);
    SystemTimeToFileTime(&stNow, &ftNow);

#define INIT_FILETIME_POINTER(str1, str2) do                                                                        \
    {                                                                                                               \
        if (IsDlgButtonChecked(hwndDlg, IDC_NOW_##str1))                                                            \
        {                                                                                                           \
            pft##str2 = &ftNow;                                                                                     \
        }                                                                                                           \
        else if (GDT_VALID == SendDlgItemMessage(hwndDlg, IDC_DATE_##str1, DTM_GETSYSTEMTIME, 0, (LPARAM)&st##str2))\
        {                                                                                                           \
            SendDlgItemMessage(hwndDlg, IDC_TIME_##str1, DTM_GETSYSTEMTIME, 0, (LPARAM)&stForTime);                 \
                                                                                                                    \
            st##str2.wHour = stForTime.wHour;                                                                       \
            st##str2.wMinute = stForTime.wMinute;                                                                   \
            st##str2.wSecond = stForTime.wSecond;                                                                   \
            st##str2.wMilliseconds = stForTime.wMilliseconds;                                                       \
                                                                                                                    \
            SystemTimeToFileTime(&st##str2, &ft##str2);                                                             \
            LocalFileTimeToFileTime(&ft##str2, &ft##str2);                                                          \
                                                                                                                    \
            pft##str2 = &ft##str2;                                                                                  \
        }                                                                                                           \
    } while (FALSE)

    INIT_FILETIME_POINTER(CREATED, Created);
    INIT_FILETIME_POINTER(MODIFIED, Modified);
    INIT_FILETIME_POINTER(ACCESSED, Accessed);

    for (; uFileIndex < uFileCount; uFileIndex += 1)
    {
        FileTimeSetToFileObject(
            pFiles[uFileIndex].szPath,
            pFiles[uFileIndex].bIsFile,
            bSubDirIncluded,
            pftCreated,
            pftModified,
            pftAccessed,
            ssError
            );
    }

    if (!ssError.str().empty())
    {
        if (IDYES == MessageBox(
            hwndDlg,
            TEXT("任务执行完毕，一部分文件时间没有修改成功。\r\n\r\n")
            TEXT("选择“是”查看错误列表。"),
            NULL,
            MB_ICONERROR | MB_YESNO
            ))
        {
            tstring strError = ssError.str();

            if (strError.size() < 1000)
            {
                MessageBox(hwndDlg, strError.c_str(), TEXT("错误信息"), MB_ICONINFORMATION);
            }
            else
            {
                TCHAR szTempPath[MAX_PATH] = TEXT("");
                TCHAR szTempFilePath[MAX_PATH] = TEXT("");

                GetTempPath(RTL_NUMBER_OF(szTempPath), szTempPath);
                GetTempFileName(szTempPath, TEXT("SlxCom"), 0, szTempFilePath);

                BOOL bWriteSucceed = FALSE;
                HANDLE hFile = CreateFile(szTempFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hFile != INVALID_HANDLE_VALUE)
                {
                    unsigned char bom[] = {0xef, 0xbb, 0xbf};
                    strutf8 strErrorUtf8 = TtoU(strError);

                    bWriteSucceed = WriteFileHelper(hFile, bom, sizeof(bom)) && WriteFileHelper(hFile, strErrorUtf8.c_str(), (DWORD)strErrorUtf8.size());
                    CloseHandle(hFile);
                }

                if (!bWriteSucceed)
                {
                    MessageBox(hwndDlg, TEXT("错误信息保存到文件失败"), NULL, MB_ICONERROR);
                }
                else
                {
                    TCHAR szCommand[MAX_PATH + 1024];
                    STARTUPINFO si = {sizeof(si)};
                    PROCESS_INFORMATION pi;

                    PathQuoteSpaces(szTempFilePath);
                    wnsprintf(szCommand, RTL_NUMBER_OF(szCommand), TEXT("notepad %s"), szTempFilePath);

                    if (CreateProcess(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                    {
                        WaitForInputIdle(pi.hProcess, 10 * 1000);

                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    else
                    {
                        MessageBox(hwndDlg, TEXT("启动记事本查看错误信息失败"), NULL, MB_ICONERROR);
                    }
                }

                DeleteFile(szTempFilePath);
            }
        }
    }

    return TRUE;
}

INT_PTR CALLBACK CSlxComContextMenu::FileTimePropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_COMMAND && (
        LOWORD(wParam) == IDC_NOW_CREATED ||
        LOWORD(wParam) == IDC_NOW_MODIFIED ||
        LOWORD(wParam) == IDC_NOW_ACCESSED) ||
        uMsg == WM_NOTIFY &&
        lParam != 0 &&
        DTN_DATETIMECHANGE == ((LPNMHDR)lParam)->code
        )
    {
        SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, NULL);
        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, TRUE);
    }

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        LPPROPSHEETPAGE pPsp = (LPPROPSHEETPAGE)lParam;
        FileTimePropSheetDlgParam *pPropSheetDlgParam = (FileTimePropSheetDlgParam *)pPsp->lParam;

        //
        HWND hParent = GetParent(hwndDlg);

        if (IsWindow(hParent))
        {
            HFONT hFont = (HFONT)SendMessage(hParent, WM_GETFONT, 0, 0);
            EnumChildWindows(hwndDlg, EnumChildSetFontProc, (LPARAM)hFont);
        }

        //
        DWORD dwFileCount = 0;
        DWORD dwDirectoryCount = 0;

        if (pPropSheetDlgParam->uFileCount == 1)
        {
            SetDlgItemText(hwndDlg, IDC_FILEPATH, pPropSheetDlgParam->pFiles[0].szPath);

            if (pPropSheetDlgParam->pFiles[0].bIsFile)
            {
                dwFileCount += 1;
            }
            else
            {
                dwDirectoryCount += 1;
            }
        }
        else
        {
            UINT uIndex = 0;
            TCHAR szText[MAX_PATH];

            for (; uIndex < pPropSheetDlgParam->uFileCount; uIndex += 1)
            {
                if (pPropSheetDlgParam->pFiles[uIndex].bIsFile)
                {
                    dwFileCount += 1;
                }
                else
                {
                    dwDirectoryCount += 1;
                }
            }

            wnsprintf(szText, RTL_NUMBER_OF(szText), TEXT("%lu个文件，%lu个文件夹"), dwFileCount, dwDirectoryCount);
            SetDlgItemText(hwndDlg, IDC_FILEPATH, szText);
        }

        if (dwDirectoryCount == 0)
        {
            ShowWindow(GetDlgItem(hwndDlg, IDC_SUBDIR), SW_HIDE);
        }
        else if (dwFileCount == 0)
        {
            SendDlgItemMessage(hwndDlg, IDC_SUBDIR, BM_SETCHECK, BST_CHECKED, 0);
        }

        // hwndDlg的用户数据为pPropSheetDlgParam
        // IDC_ABOUT的用户数据为修改标识
        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, FALSE);
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)pPropSheetDlgParam);

        return FALSE;
    }

//     case WM_LBUTTONDOWN:
//         SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, NULL);
//         SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, TRUE);
//         break;
// 
//     case WM_RBUTTONDOWN:
//         SendMessage(GetParent(hwndDlg), PSM_UNCHANGED, (WPARAM)hwndDlg, NULL);
//         SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, FALSE);
//         break;

    case WM_NOTIFY:
        if (lParam != 0)
        {
            NMHDR *phdr = (NMHDR *)lParam;

            if (phdr->code == PSN_APPLY)
            {
                if (GetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA))
                {
                    FileTimePropSheetDlgParam *pPropSheetDlgParam = (FileTimePropSheetDlgParam *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

                    if (FileTimePropSheetDoSave(hwndDlg, pPropSheetDlgParam->pFiles, pPropSheetDlgParam->uFileCount))
                    {
                        SendMessage(GetParent(hwndDlg), PSM_UNCHANGED, (WPARAM)hwndDlg, NULL);
                        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, FALSE);
                    }
                    else
                    {
                        SendMessage(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, NULL);
                        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, TRUE);
                    }
                }
                else
                {
                    SendMessage(GetParent(hwndDlg), PSM_UNCHANGED, (WPARAM)hwndDlg, NULL);
                }
            }
            else if (phdr->code == DTN_DATETIMECHANGE)
            {
                LPNMDATETIMECHANGE lpChange = (LPNMDATETIMECHANGE)phdr;

                switch (phdr->idFrom)
                {
                case IDC_DATE_CREATED:
                    if (lpChange->dwFlags == GDT_NONE)
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_CREATED), FALSE);
                    }
                    else
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_CREATED), TRUE);
                        SendDlgItemMessage(hwndDlg, IDC_NOW_CREATED, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    break;

                case IDC_DATE_MODIFIED:
                    if (lpChange->dwFlags == GDT_NONE)
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_MODIFIED), FALSE);
                    }
                    else
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_MODIFIED), TRUE);
                        SendDlgItemMessage(hwndDlg, IDC_NOW_MODIFIED, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    break;

                case IDC_DATE_ACCESSED:
                    if (lpChange->dwFlags == GDT_NONE)
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_ACCESSED), FALSE);
                    }
                    else
                    {
                        EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_ACCESSED), TRUE);
                        SendDlgItemMessage(hwndDlg, IDC_NOW_ACCESSED, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    break;

                default:
                    break;
                }
            }
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_ABOUT:
            SlxComAbout(hwndDlg);
            break;

        case IDC_NOW_CREATED:
            SendDlgItemMessage(hwndDlg, IDC_DATE_CREATED, DTM_SETSYSTEMTIME, GDT_NONE, 0);
            EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_CREATED), FALSE);
            break;

        case IDC_NOW_MODIFIED:
            SendDlgItemMessage(hwndDlg, IDC_DATE_MODIFIED, DTM_SETSYSTEMTIME, GDT_NONE, 0);
            EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_MODIFIED), FALSE);
            break;

        case IDC_NOW_ACCESSED:
            SendDlgItemMessage(hwndDlg, IDC_DATE_ACCESSED, DTM_SETSYSTEMTIME, GDT_NONE, 0);
            EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_ACCESSED), FALSE);
            break;

        default:
            break;
        }
        break;

    case WM_DESTROY:
        break;

    default:
        break;
    }

    return FALSE;
}

UINT CALLBACK CSlxComContextMenu::FileTimePropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE pPsp)
{
    switch (uMsg)
    {
    case PSPCB_ADDREF:
        break;

    case PSPCB_CREATE:
        break;

    case PSPCB_RELEASE:
        delete (HashPropSheetDlgParam *)pPsp->lParam;
        break;

    default:
        break;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSlxComContextMenu::AddPages(LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam)
{
    HRESULT hr = E_NOTIMPL;

    if (ShouldAddFileTimePropSheet())
    {
        PROPSHEETPAGE psp = {sizeof(psp)};
        HPROPSHEETPAGE hPage;
        FileTimePropSheetDlgParam *pPropSheetDlgParam = new FileTimePropSheetDlgParam(m_pFiles, m_uFileCount);

        psp.dwFlags     = PSP_USETITLE | PSP_DEFAULT | PSP_USECALLBACK;
        psp.hInstance   = g_hinstDll;
        psp.pszTemplate = MAKEINTRESOURCE(IDD_FILETIMEPAGE);
        psp.pszTitle    = TEXT("文件时间");
        psp.pfnDlgProc  = FileTimePropSheetDlgProc;
        psp.lParam      = (LPARAM)pPropSheetDlgParam;
        psp.pfnCallback = FileTimePropSheetCallback;

        hPage = CreatePropertySheetPage(&psp);

        if (NULL != hPage)
        {
            if (!pfnAddPage(hPage, lParam))
            {
                DestroyPropertySheetPage(hPage);
            }
        }

        hr = S_OK;
    }

    if (ShouldAddHashPropSheet())
    {
        PROPSHEETPAGE psp = {sizeof(psp)};
        HPROPSHEETPAGE hPage;
        HashPropSheetDlgParam *pPropSheetDlgParam = new HashPropSheetDlgParam;

        lstrcpyn(pPropSheetDlgParam->szFilePath, m_pFiles[0].szPath, RTL_NUMBER_OF(pPropSheetDlgParam->szFilePath));

        if (m_uFileCount > 1)
        {
            lstrcpyn(pPropSheetDlgParam->szFilePath_2, m_pFiles[1].szPath, RTL_NUMBER_OF(pPropSheetDlgParam->szFilePath_2));
        }

        psp.dwFlags     = PSP_USETITLE | PSP_DEFAULT | PSP_USECALLBACK;
        psp.hInstance   = g_hinstDll;
        psp.pszTemplate = MAKEINTRESOURCE(IDD_HASHPAGE);
        psp.pszTitle    = TEXT("校验");
        psp.pfnDlgProc  = HashPropSheetDlgProc;
        psp.lParam      = (LPARAM)pPropSheetDlgParam;
        psp.pfnCallback = HashPropSheetCallback;

        hPage = CreatePropertySheetPage(&psp);

        if (NULL != hPage)
        {
            if (!pfnAddPage(hPage, lParam))
            {
                DestroyPropertySheetPage(hPage);
            }
        }

        hr = S_OK;
    }

    return hr;
}

STDMETHODIMP CSlxComContextMenu::ReplacePage(UINT uPageID, LPFNADDPROPSHEETPAGE pfnReplacePage, LPARAM lParam)
{
    return E_NOTIMPL;
}

BOOL CALLBACK CSlxComContextMenu::EnumChildSetFontProc(HWND hwnd, LPARAM lParam)
{
    SendMessage(hwnd, WM_SETFONT, lParam, 0);
    return TRUE;
}

BOOL CSlxComContextMenu::ConvertToShortPaths()
{
    TCHAR szFilePathTemp[MAX_PATH];

    for(UINT uFileIndex = 0; uFileIndex < m_uFileCount; uFileIndex += 1)
    {
        GetShortPathName(m_pFiles[uFileIndex].szPath, szFilePathTemp, sizeof(szFilePathTemp) / sizeof(TCHAR));
        lstrcpyn(m_pFiles[uFileIndex].szPath, szFilePathTemp, sizeof(szFilePathTemp) / sizeof(TCHAR));
    }

    return TRUE;
}

BOOL CSlxComContextMenu::ShouldAddHashPropSheet()
{
    if (m_uFileCount == 1 && m_pFiles[0].bIsFile)
    {
        return TRUE;
    }

    if (m_uFileCount == 2 && m_pFiles[0].bIsFile && m_pFiles[1].bIsFile)
    {
        return TRUE;
    }

    return FALSE;
}

BOOL CSlxComContextMenu::ShouldAddFileTimePropSheet()
{
    return m_uFileCount > 0 && !(g_osi.dwMajorVersion == 5 && g_osi.dwMinorVersion == 0);
}

void BatchRegisterOrUnregisterDllsW(LPCWSTR lpMarkArgument, LPCWSTR lpArguments)
{
    if (lpMarkArgument == NULL || lpArguments == NULL)
    {
        return;
    }

    int nArgCount = 0;
    LPWSTR *lpArgs = CommandLineToArgvW(lpArguments, &nArgCount);

    if (nArgCount == 0)
    {
        return;
    }

    vector<wstring> vectorFiles(lpArgs, lpArgs + nArgCount);

    TCHAR szCmd[MAX_PATH + 1024];
    WCHAR szPath[MAX_PATH];
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    vector<HANDLE> vectorProcesses;
    vector<DWORD> vectorExitCodes;

    for (vector<wstring>::const_iterator it = vectorFiles.begin(); it != vectorFiles.end(); ++it)
    {
        lstrcpynW(szPath, it->c_str(), RTL_NUMBER_OF(szPath));
        PathQuoteSpaces(szPath);
        wnsprintf(szCmd, RTL_NUMBER_OF(szCmd), TEXT("regsvr32 %ls %ls"), lpMarkArgument,szPath);

        if (CreateProcess(NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(pi.hThread);
            vectorProcesses.push_back(pi.hProcess);
        }
        else
        {
            MessageBoxFormat(NULL, NULL, MB_ICONERROR | MB_TOPMOST, TEXT("启动regsvr32失败，错误码%lu"), GetLastError());
            return;
        }
    }

    for (vector<HANDLE>::const_iterator it2 = vectorProcesses.begin(); it2 != vectorProcesses.end(); ++it2)
    {
        DWORD dwExitCodeProcess = 0;

        WaitForSingleObject(*it2, INFINITE);
        GetExitCodeProcess(*it2, &dwExitCodeProcess);

        vectorExitCodes.push_back(dwExitCodeProcess);
    }

    wstringstream ssResults;

    for (size_t i = 0; i < vectorExitCodes.size(); ++i)
    {
        if (i > 0)
        {
            ssResults<<L"\r\n";
        }

        if (vectorExitCodes[i] == 0)
        {
            ssResults<<L"[成功]";
        }
        else
        {
            ssResults<<L"[失败]";
        }

        ssResults<<vectorFiles.at(i);
    }

    MessageBoxW(NULL, ssResults.str().c_str(), TEXT("RegSvr32 执行结果"), MB_ICONINFORMATION | MB_TOPMOST);
}

void WINAPI BatchRegisterDllsW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    BatchRegisterOrUnregisterDllsW(TEXT("/s"), lpszCmdLine);
}

void WINAPI BatchUnregisterDllsW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    BatchRegisterOrUnregisterDllsW(TEXT("/s /u"), lpszCmdLine);
}