#define _WIN32_WINNT 0x500
#include "SlxComContextMenu.h"
#include <shlwapi.h>
#include <CommCtrl.h>
#include "lib/GeneralTreeItemW.h"
#include "SlxComOverlay.h"
#include "SlxComTools.h"
#include "SlxComPeTools.h"
#include "resource.h"
#include "SlxCrypto.h"
#include "SlxComAbout.h"
#include "SlxManualCheckSignature.h"
#include "SlxElevateBridge.h"
#include "SlxAppPath.h"
#include "SlxPeInformation.h"
#include "SlxComExpandDirFiles.h"
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
extern HBITMAP g_hCreateLinkBmp;
extern HBITMAP g_hEdfBmp;
extern HBITMAP g_hLocateBmp;
extern BOOL g_bVistaLater;
extern BOOL g_bElevated;
extern BOOL g_bHasWin10Bash;

volatile HANDLE m_hManualCheckSignatureThread = NULL;
static HANDLE g_hManualCheckSignatureMutex = CreateMutex(NULL, FALSE, NULL);

CSlxComContextMenu::CSlxComContextMenu()
{
    m_dwRefCount = 0;
    m_bIsBackground = FALSE;

    m_pFiles = new FileInfo[1];
    m_uFileCount = 1;
    lstrcpyW(m_pFiles[0].szPath, L"1:\\");
}

CSlxComContextMenu::~CSlxComContextMenu()
{
    delete []m_pFiles;
}

STDMETHODIMP CSlxComContextMenu::QueryInterface(REFIID riid, void **ppv)
{
    if (riid == IID_IShellExtInit)
    {
        *ppv = static_cast<IShellExtInit *>(this);
    }
    else if (riid == IID_IContextMenu)
    {
        *ppv = static_cast<IContextMenu *>(this);
    }
    else if (riid == IID_IShellPropSheetExt)
    {
        *ppv = static_cast<IShellPropSheetExt *>(this);
    }
    else if (riid == IID_IUnknown)
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

    if (dwRefCount == 0)
    {
        delete this;
    }

    return dwRefCount;
}

//IShellExtInit
STDMETHODIMP CSlxComContextMenu::Initialize(LPCITEMIDLIST pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID)
{
    if (pidlFolder != NULL)
    {
        WCHAR szFilePath[MAX_PATH] = L"";

        SHGetPathFromIDListW(pidlFolder, szFilePath);
        m_strInputFolder = szFilePath;
    }

    if (pdtobj != NULL)
    {
        FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stg = {TYMED_HGLOBAL};
        HDROP hDrop;
        HRESULT hResult = E_INVALIDARG;

        if (SUCCEEDED(pdtobj->GetData(&fmt, &stg)))
        {
            hDrop = (HDROP)GlobalLock(stg.hGlobal);

            if (hDrop != NULL)
            {
                UINT uNumFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

                for (UINT uIndex = 0; uIndex < uNumFiles; ++uIndex)
                {
                    WCHAR szFilePath[MAX_PATH] = L"";

                    DragQueryFileW(hDrop, uIndex, szFilePath, RTL_NUMBER_OF(szFilePath));

                    if (PathFileExistsW(szFilePath))
                    {
                        m_vectorInputFiles.push_back(szFilePath);

                        if (m_strInputFile.empty())
                        {
                            m_strInputFile = szFilePath;
                        }
                    }
                }
            }

            ReleaseStgMedium(&stg);
        }
    }

    if (m_strInputFolder.empty() && m_vectorInputFiles.empty())
    {
        return E_INVALIDARG;
    }

    if (m_strInputFolder.empty())
    {
        m_shellExtType = SET_FILE;
    }
    else if (m_vectorInputFiles.empty())
    {
        m_shellExtType = SET_BACKGROUND;
    }
    else
    {
        m_shellExtType = SET_DRAGDROP;
    }

    // 旧处理逻辑
    // todo: 基于新逻辑重新实现
    if (pdtobj == NULL)
    {
        if (pidlFolder == NULL)
        {
            return E_INVALIDARG;
        }
        else
        {
            WCHAR szPath[MAX_PATH];

            if (SHGetPathFromIDListW(pidlFolder, szPath))
            {
                m_uFileCount = 1;

                delete []m_pFiles;

                m_pFiles = new FileInfo[m_uFileCount];
                lstrcpynW(m_pFiles[0].szPath, szPath, MAX_PATH);

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

        if (SUCCEEDED(pdtobj->GetData(&fmt, &stg)))
        {
            hDrop = (HDROP)GlobalLock(stg.hGlobal);

            if (hDrop != NULL)
            {
                UINT uIndex;
                UINT uNumFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

                if (uNumFiles > 0)
                {
                    delete []m_pFiles;

                    m_pFiles = new FileInfo[uNumFiles];
                    m_uFileCount = uNumFiles;

                    for (uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                    {
                        DragQueryFileW(hDrop, uIndex, m_pFiles[uIndex].szPath, MAX_PATH);

                        DWORD dwAttribute = GetFileAttributesW(m_pFiles[uIndex].szPath);

                        if (dwAttribute != INVALID_FILE_ATTRIBUTES && (dwAttribute & FILE_ATTRIBUTE_DIRECTORY) == 0)
                        {
                            LPCWSTR lpExtension = PathFindExtensionW(m_pFiles[uIndex].szPath);

                            if (lstrcmpiW(lpExtension, L".dll") == 0 || lstrcmpiW(lpExtension, L".ocx") == 0)
                            {
                                m_pFiles[uIndex].bIsDll = TRUE;
                            }
                            else if (lstrcmpiW(lpExtension, L".jpg") == 0 || lstrcmpiW(lpExtension, L".jpeg") == 0)
                            {
                                m_pFiles[uIndex].bIsJpg = TRUE;
                                m_pFiles[uIndex].bIsPicture = TRUE;
                            }
                            else if (lstrcmpiW(lpExtension, L".gif") == 0 ||
                                     lstrcmpiW(lpExtension, L".bmp") == 0 ||
                                     lstrcmpiW(lpExtension, L".png") == 0 ||
                                     lstrcmpiW(lpExtension, L".dib") == 0 ||
                                     lstrcmpiW(lpExtension, L".tif") == 0 ||
                                     lstrcmpiW(lpExtension, L".tiff") == 0)
                            {
                                m_pFiles[uIndex].bIsPicture = TRUE;
                            }
                            else if (lstrcmpiW(lpExtension, L".rar") == 0)
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

enum
{
    ID_REGISTER = 33,
    ID_UNREGISTER,
    ID_COMBINE,
    ID_COPYFULLPATH,
    ID_APPPATH,
    ID_UNLOCKFILE,
    ID_TRYRUN,
    ID_TRYRUNWITHARGUMENTS,
    ID_RUNCMDHERE,
    ID_RUNBASHHERE,
    ID_OPENWITHNOTEPAD,
    ID_KILLEXPLORER,
    ID_MANUALCHECKSIGNATURE,
    ID_UNESCAPE,
    ID_DRV_INSTALL,
    ID_DRV_START,
    ID_DRV_STOP,
    ID_DRV_UNINSTALL,
    ID_COPY_PICTURE_HTML,
    ID_CREATE_HARD_LINK,
    ID_CREATE_SOFT_LINK,
    ID_LOAD_DLL,
    ID_EXPAND_DIR_FILES,
    ID_UNEXPAND_DIR_FILES,
    ID_LOCATE_EXPAND_SOURCE_FILE,
    IDCOUNT,
};

//IContextMenu
STDMETHODIMP CSlxComContextMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    UINT uMenuIndex = 0;
    UINT uFileIndex;

    if (m_uFileCount == 0)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }
    else if (m_uFileCount == 1)
    {
        int nCount = GetMenuItemCount(hmenu);
        int nMenuIndex = 0;

        for (; nMenuIndex < nCount; nMenuIndex += 1)
        {
            WCHAR szText[100] = L"";

            GetMenuStringW(hmenu, nMenuIndex, szText, RTL_NUMBER_OF(szText), MF_BYPOSITION);

            if (lstrcmpiW(szText, L"刷新(&E)") == 0)
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

    for (uFileIndex = 0; uFileIndex < m_uFileCount; uFileIndex += 1)
    {
        if (!bExistDll && m_pFiles[uFileIndex].bIsDll)
        {
            bExistDll = TRUE;
        }

        if (!bExistFile && m_pFiles[uFileIndex].bIsFile)
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

        if (bExistDll * bExistFile * bExistCom * bExistPicture)
        {
            break;
        }
    }

    if (m_shellExtType != SET_DRAGDROP)
    {
        //clipboard functions
        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, L"");

        //CopyFilePath
        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COPYFULLPATH, L"复制完整路径");
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COPYFULLPATH, MF_BYCOMMAND, g_hCopyFullPathBmp, g_hCopyFullPathBmp);

        //CopyPictureAsHtml
        if (bExistPicture)
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COPY_PICTURE_HTML, L"复制图片（QQ）");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COPY_PICTURE_HTML, MF_BYCOMMAND, g_hCopyPictureHtmlBmp, g_hCopyPictureHtmlBmp);
        }

        //Dll Register
        if (bExistCom)
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_REGISTER, L"注册组件(&R)");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_REGISTER, MF_BYCOMMAND, g_hInstallBmp, g_hInstallBmp);

            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNREGISTER, L"取消注册组件(&U)");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNREGISTER, MF_BYCOMMAND, g_hUninstallBmp, g_hUninstallBmp);
        }

        //File Combine
        if (TRUE)
        {
            BOOL bCouldCombine = FALSE;

            if (m_uFileCount == 1)
            {
                bCouldCombine = m_pFiles[0].bIsRar;
            }
            else if (m_uFileCount == 2)
            {
                bCouldCombine = m_pFiles[0].bIsJpg && m_pFiles[1].bIsRar || m_pFiles[0].bIsRar && m_pFiles[1].bIsJpg;
            }

            if (bCouldCombine)
            {
                InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COMBINE, L"文件合成(&C)");
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COMBINE, MF_BYCOMMAND, g_hCombineBmp, g_hCombineBmp);
            }
        }

        //Check Signature
        if (bExistFile)
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_MANUALCHECKSIGNATURE, L"校验数字签名");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_MANUALCHECKSIGNATURE, MF_BYCOMMAND, g_hManualCheckSignatureBmp, g_hManualCheckSignatureBmp);
        }

        if (m_uFileCount == 1)
        {
            DWORD dwFileAttribute = GetFileAttributesW(m_pFiles[0].szPath);

            if (dwFileAttribute != INVALID_FILE_ATTRIBUTES)
            {
                if ((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    if (bShiftDown)
                    {
                        HMENU hPopupMenu = CreatePopupMenu();

                        if (hPopupMenu != NULL)
                        {
                            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)hPopupMenu, L"尝试运行");
                            SetMenuItemBitmaps(hmenu, indexMenu + uMenuIndex - 1, MF_BYPOSITION, g_hTryRunBmp, g_hTryRunBmp);

                            //Try to run
                            InsertMenuW(hPopupMenu, 1, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_TRYRUN, L"直接运行");
                            SetMenuItemBitmaps(hPopupMenu, idCmdFirst + ID_TRYRUN, MF_BYCOMMAND, g_hTryRunBmp, g_hTryRunBmp);

                            InsertMenuW(hPopupMenu, 2, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_TRYRUNWITHARGUMENTS, L"附带参数运行(&P)");
                            SetMenuItemBitmaps(hPopupMenu, idCmdFirst + ID_TRYRUNWITHARGUMENTS, MF_BYCOMMAND, g_hTryRunWithArgumentsBmp, g_hTryRunWithArgumentsBmp);

                            DestroyMenu(hPopupMenu);
                        }
                    }

                    //OpenWithNotepad
                    InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_OPENWITHNOTEPAD, L"用记事本打开");
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_OPENWITHNOTEPAD, MF_BYCOMMAND, g_hOpenWithNotepadBmp, g_hOpenWithNotepadBmp);

                    //Unescape
                    WCHAR szUnescapedFileName[MAX_PATH];

                    if (TryUnescapeFileName(m_pFiles[0].szPath, szUnescapedFileName, sizeof(szUnescapedFileName) / sizeof(WCHAR)))
                    {
                        WCHAR szCommandText[MAX_PATH + 100];

                        wnsprintfW(
                            szCommandText,
                            sizeof(szCommandText) / sizeof(WCHAR),
                            L"重命名为“%s”",
                            PathFindFileNameW(szUnescapedFileName)
                            );

                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNESCAPE, szCommandText);
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNESCAPE, MF_BYCOMMAND, g_hUnescapeBmp, g_hUnescapeBmp);
                    }

                    //App path
                    WCHAR szCommandInReg[MAX_PATH] = L"";

                    ModifyAppPath_GetFileCommand(m_pFiles[0].szPath, szCommandInReg, sizeof(szCommandInReg) / sizeof(WCHAR));

                    if (bShiftDown ||
                        lstrcmpiW(PathFindExtensionW(m_pFiles[0].szPath), L".exe") == 0 ||
                        lstrlenW(szCommandInReg) > 0
                        )
                    {
                        WCHAR szMenuText[MAX_PATH + 1000] = L"添加快捷短语";

                        if (lstrlenW(szCommandInReg) > 0)
                        {
                            wnsprintfW(
                                szMenuText,
                                sizeof(szMenuText) / sizeof(WCHAR),
                                L"修改快捷短语“%s”",
                                szCommandInReg
                                );
                        }

                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_APPPATH, szMenuText);
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_APPPATH, MF_BYCOMMAND, g_hAppPathBmp, g_hAppPathBmp);
                    }

                    //Driver
                    if (bShiftDown &&
                        lstrcmpiW(PathFindExtensionW(m_pFiles[0].szPath), L".dll") == 0 ||
                        lstrcmpiW(PathFindExtensionW(m_pFiles[0].szPath), L".sys") == 0
                        )
                    {
                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_INSTALL, L"驱动程序-安装");
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_INSTALL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_START, L"驱动程序-启动");
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_START, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_STOP, L"驱动程序-停止");
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_STOP, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_UNINSTALL, L"驱动程序-卸载");
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_UNINSTALL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);
                    }
                }

                if ((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    //RunCmdHere
                    InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_RUNCMDHERE, L"在此处运行命令行");
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_RUNCMDHERE, MF_BYCOMMAND, g_hRunCmdHereBmp, g_hRunCmdHereBmp);

                    if (g_bHasWin10Bash)
                    {
                        // Win10BashHere
                        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_RUNBASHHERE, L"在此处运行Bash");
                        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_RUNBASHHERE, MF_BYCOMMAND, g_hRunCmdHereBmp, g_hRunCmdHereBmp);
                    }
                }

                if ((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0 && bShiftDown ||
                    (dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) == 0 && (bShiftDown || IsFileDenyed(m_pFiles[0].szPath)))
                {
                    //UnlockFile
                    InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNLOCKFILE, L"查看锁定情况");
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNLOCKFILE, MF_BYCOMMAND, g_hUnlockFileBmp, g_hUnlockFileBmp);
                }
            }
        }
    }

    // 右键拖动
    if (m_shellExtType == SET_DRAGDROP)
    {
        // 文件软硬链接
        if (g_osi.dwMajorVersion >= 6 && m_vectorInputFiles.size() == 1)
        {
            if (IsPathDirectoryW(m_strInputFile.c_str()))
            {
                InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_CREATE_SOFT_LINK, L"在此处创建软链接");
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_CREATE_SOFT_LINK, MF_BYCOMMAND, g_hCreateLinkBmp, g_hCreateLinkBmp);
            }

            if (IsPathFileW(m_strInputFile.c_str()))
            {
                WCHAR chDriverLetter1 = *m_strInputFolder.begin() | 32;
                WCHAR chDriverLetter2 = *m_strInputFile.begin() | 32;

                if (chDriverLetter1 == chDriverLetter2)
                {
                    InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_CREATE_HARD_LINK, L"在此处创建硬链接");
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_CREATE_HARD_LINK, MF_BYCOMMAND, g_hCreateLinkBmp, g_hCreateLinkBmp);
                }
            }
        }
    }

    if (bShiftDown)
    {
        //结束Explorer
        InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_KILLEXPLORER, L"结束Explorer进程");
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_KILLEXPLORER, MF_BYCOMMAND, g_hKillExplorerBmp, g_hKillExplorerBmp);
    }

    extern BOOL g_bDebugMode;
    if (bShiftDown && g_bDebugMode)
    {
        HMODULE hModule = GetModuleHandleW(m_strInputFile.c_str());

        // 加载dll
        if (lstrcmpiW(PathFindExtensionW(m_strInputFile.c_str()), L".dll") == 0)
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_LOAD_DLL, hModule ? L"卸载此dll" : L"加载此dll");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_LOAD_DLL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);
        }
    }

    // 定位展开的文件
    if (!m_strInputFile.empty() && m_uFileCount == 1)
    {
        if (PathFileExistsW((m_strInputFile + L":edf").c_str()))
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_LOCATE_EXPAND_SOURCE_FILE, L"打开展开前文件位置");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_LOCATE_EXPAND_SOURCE_FILE, MF_BYCOMMAND, g_hLocateBmp, g_hLocateBmp);
        }
    }

    // 展开与取消展开文件
    if (m_bIsBackground && g_osi.dwMajorVersion >= 6)
    {
        if (EdfCanBeUnexpanded(m_strInputFolder.c_str()))
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNEXPAND_DIR_FILES, L"取消展开这里的所有目录");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_UNEXPAND_DIR_FILES, MF_BYCOMMAND, g_hEdfBmp, g_hEdfBmp);
        }
        else if (bShiftDown && EdfCanBeExpanded(m_strInputFolder.c_str()))
        {
            InsertMenuW(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_EXPAND_DIR_FILES, L"展开这里的所有目录");
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_EXPAND_DIR_FILES, MF_BYCOMMAND, g_hEdfBmp, g_hEdfBmp);
        }
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, IDCOUNT);
}

STDMETHODIMP CSlxComContextMenu::GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
    if (uFlags & GCS_UNICODE)
    {
        memset(pszName, 0, cchMax * 2);
    }
    else
    {
        memset(pszName, 0, cchMax);
    }

    return S_OK;


    LPCSTR lpText = "";

    if (idCmd == ID_REGISTER)
    {
        lpText = "注册组件。";
    }
    else if (idCmd == ID_UNREGISTER)
    {
        lpText = "取消注册组件。";
    }
    else if (idCmd == ID_COMBINE)
    {
        if (m_uFileCount == 1)
        {
            lpText = "将选定的rar文件附加到默认jpg文件之后。";
        }
        else
        {
            lpText = "将选定的rar文件附加到选定的jpg文件之后。";
        }
    }
    else if (idCmd == ID_COPYFULLPATH)
    {
        lpText = "复制选中的文件或文件夹的完整路径到剪贴板。";
    }
    else if (idCmd == ID_APPPATH)
    {
        lpText = "维护在“运行”对话框中快速启动的条目。";
    }
    else if (idCmd == ID_UNLOCKFILE)
    {
        lpText = "查看文件夹或文件是否锁定和解除这些锁定。";
    }
    else if (idCmd == ID_TRYRUN)
    {
        lpText = "尝试将文件作为可执行文件运行。";
    }
    else if (idCmd == ID_TRYRUNWITHARGUMENTS)
    {
        lpText = "尝试将文件作为可执行文件运行，并附带命令行参数。";
    }
    else if (idCmd == ID_RUNCMDHERE)
    {
        lpText = "在当前目录启动命令行。";
    }
    else if (idCmd == ID_RUNBASHHERE)
    {
        lpText = "在当前目录启动Win10 linux子系统bash。";
    }
    else if (idCmd == ID_OPENWITHNOTEPAD)
    {
        lpText = "在记事本打开当前文件。";
    }
    else if (idCmd == ID_KILLEXPLORER)
    {
        lpText = "结束Explorer，随后系统将自动重新启动Explorer。";
    }
    else if (idCmd == ID_MANUALCHECKSIGNATURE)
    {
        lpText = "手动校验选中的文件的数字签名。";
    }
    else if (idCmd == ID_UNESCAPE)
    {
        lpText = "转化文件名为较合适的形式。";
    }
    else if (idCmd == ID_COPY_PICTURE_HTML)
    {
        lpText = "将图片内容复制到剪贴板，支持在QQ上粘贴。";
    }

    if (uFlags & GCS_UNICODE)
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
    if (0 != HIWORD(pici->lpVerb))
    {
        return E_INVALIDARG;
    }

    if ((GetKeyState(VK_LSHIFT) < 0 || GetKeyState(VK_RSHIFT) < 0))
    {
        ConvertToShortPaths();
    }

    switch(LOWORD(pici->lpVerb))
    {
    case ID_REGISTER:
    case ID_UNREGISTER:
    {
        // 始终采用多文件处理方式
//         if (m_uFileCount == 1)
//         {
//             WCHAR szArguments[MAX_PATH * 2];
// 
//             wnsprintfW(
//                 szArguments,
//                 RTL_NUMBER_OF(szArguments),
//                 L"%s \"%s\"",
//                 LOWORD(pici->lpVerb) == ID_REGISTER ? L"" : L"/u",
//                 m_pFiles[0].szPath);
// 
//             ElevateAndRunW(L"regsvr32.exe", szArguments, NULL, SW_SHOW);
//         }
//         else
        {
            wstringstream ssArguments;

            WCHAR szDllpath[MAX_PATH];

            GetModuleFileNameW(g_hinstDll, szDllpath, RTL_NUMBER_OF(szDllpath));
            PathQuoteSpacesW(szDllpath);

            ssArguments<<szDllpath;

            if (LOWORD(pici->lpVerb) == ID_REGISTER)
            {
                ssArguments<<L" BatchRegisterDlls";
            }
            else
            {
                ssArguments<<L" BatchUnregisterDlls";
            }

            for (DWORD dwIndex = 0; dwIndex < m_uFileCount; ++dwIndex)
            {
                ssArguments<<L" \"";
                ssArguments<<m_pFiles[dwIndex].szPath;
                ssArguments<<L"\"";
            }

            ElevateAndRunW(L"rundll32.exe", ssArguments.str().c_str(), NULL, SW_SHOW);
        }

        break;
    }

    case ID_COMBINE:
    {
        BOOL bSucceed = FALSE;
        WCHAR szResultPath[MAX_PATH];

        if (m_uFileCount == 1)
        {
            bSucceed = CombineFile(m_pFiles[0].szPath, NULL, szResultPath, MAX_PATH);
        }
        else if (m_uFileCount == 2)
        {
            if (m_pFiles[0].bIsRar)
            {
                bSucceed = CombineFile(m_pFiles[0].szPath, m_pFiles[1].szPath, szResultPath, MAX_PATH);
            }
            else
            {
                bSucceed = CombineFile(m_pFiles[1].szPath, m_pFiles[0].szPath, szResultPath, MAX_PATH);
            }
        }

        if (!bSucceed)
        {
            MessageBoxW(pici->hwnd, L"合成文件出错", NULL, MB_ICONERROR | MB_TOPMOST);
        }

        break;
    }

    case ID_APPPATH:
        ModifyAppPath(m_pFiles[0].szPath);
        break;

    case ID_UNLOCKFILE:
    {
        WCHAR szDllPath[MAX_PATH];
        WCHAR szCommand[MAX_PATH * 2];
        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi;

        GetModuleFileNameW(g_hinstDll, szDllPath, sizeof(szDllPath) / sizeof(WCHAR));
        wnsprintfW(
            szCommand,
            sizeof(szCommand) / sizeof(WCHAR),
            L"rundll32.exe \"%s\" UnlockFileFromPath %s",
            szDllPath,
            m_pFiles[0].szPath
            );

        if (CreateProcessW(
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
                L"启动外部程序失败，命令行：\r\n\r\n%s",
                szCommand
                );
        }

        break;
    }

    case ID_COPYFULLPATH:
    {
        if (m_uFileCount == 1)
        {
            SetClipboardText(m_pFiles[0].szPath);
        }
        else if (m_uFileCount > 1)
        {
            WCHAR *szText = new WCHAR[MAX_PATH * m_uFileCount];

            if (szText != NULL)
            {
                UINT uFileIndex = 0;

                szText[0] = L'\0';

                for (; uFileIndex < m_uFileCount; uFileIndex += 1)
                {
                    lstrcatW(szText, m_pFiles[uFileIndex].szPath);
                    lstrcatW(szText, L"\r\n");
                }

                SetClipboardText(szText);

                delete []szText;
            }
        }

        break;
    }

    case ID_TRYRUN:
    {
        WCHAR szCommandLine[MAX_PATH + 100];

        wnsprintfW(szCommandLine, sizeof(szCommandLine) / sizeof(WCHAR), L"\"%s\"", m_pFiles[0].szPath);

        if (IDYES == MessageBoxW(pici->hwnd, L"确定要尝试将此文件作为可执行文件运行吗？", L"请确认", MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3))
        {
            if (!RunCommand(szCommandLine))
            {
                WCHAR szErrorMessage[MAX_PATH + 300];

                wnsprintfW(szErrorMessage, sizeof(szErrorMessage) / sizeof(WCHAR), L"无法启动进程，错误码%lu\r\n\r\n%s",
                    GetLastError(), szCommandLine);

                MessageBoxW(pici->hwnd, szErrorMessage, NULL, MB_ICONERROR | MB_TOPMOST);
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
        WCHAR szApplication[MAX_PATH * 2] = L"";
        WCHAR szArgument[MAX_PATH * 2] = L"/k pushd ";

        GetEnvironmentVariableW(L"ComSpec", szApplication, MAX_PATH);
        StrCatBuffW(szArgument, m_pFiles[0].szPath, RTL_NUMBER_OF(szArgument));

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

    case ID_RUNBASHHERE:
    {
        WCHAR szApplication[MAX_PATH * 2] = L"";
        WCHAR szArgument[MAX_PATH * 2] = L"/c pushd \"";

        GetEnvironmentVariableW(L"ComSpec", szApplication, MAX_PATH);
        StrCatBuffW(szArgument, m_pFiles[0].szPath, RTL_NUMBER_OF(szArgument));
        StrCatBuffW(szArgument, L"\" && bash", RTL_NUMBER_OF(szArgument));

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
        WCHAR szCommandLine[MAX_PATH * 2] = L"";

        GetSystemDirectoryW(szCommandLine, MAX_PATH);
        PathAppendW(szCommandLine, L"\\notepad.exe");

        if (GetFileAttributesW(szCommandLine) == INVALID_FILE_ATTRIBUTES)
        {
            GetWindowsDirectoryW(szCommandLine, MAX_PATH);
            PathAppendW(szCommandLine, L"\\notepad.exe");
        }

        lstrcatW(szCommandLine, L" \"");
        lstrcatW(szCommandLine, m_pFiles[0].szPath);
        lstrcatW(szCommandLine, L"\"");

        if (!RunCommand(szCommandLine))
        {
            WCHAR szErrorMessage[MAX_PATH + 300];

            wnsprintfW(szErrorMessage, sizeof(szErrorMessage) / sizeof(WCHAR), L"无法启动进程，错误码%lu\r\n\r\n%s",
                GetLastError(), szCommandLine);

            MessageBoxW(pici->hwnd, szErrorMessage, NULL, MB_ICONERROR | MB_TOPMOST);
        }

        break;
    }

    case ID_KILLEXPLORER:
    {
        DWORD dwTimeMark = GetTickCount();
        WCHAR szPath[MAX_PATH];

        lstrcpynW(szPath, m_pFiles[0].szPath, RTL_NUMBER_OF(szPath));

        if (m_bIsBackground)
        {
            PathAppendW(szPath, L"\\*");

            BOOL bFound = FALSE;
            WIN32_FIND_DATAW wfd;
            HANDLE hFind = FindFirstFileW(szPath, &wfd);

            if (hFind != NULL && hFind != INVALID_HANDLE_VALUE)
            {
                do 
                {
                    if (lstrcmpiW(wfd.cFileName, L".") != 0 &&
                        lstrcmpiW(wfd.cFileName, L"..") != 0
                        )
                    {
                        PathRemoveFileSpecW(szPath);
                        StrCatBuffW(szPath, L"\\", RTL_NUMBER_OF(szPath));
                        StrCatBuffW(szPath, wfd.cFileName, RTL_NUMBER_OF(szPath));
                        bFound = TRUE;

                        break;
                    }

                } while (FindNextFileW(hFind, &wfd));

                FindClose(hFind);
            }

            if (!bFound)
            {
                PathRemoveFileSpecW(szPath);
            }
        }

        SHSetTempValue(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
            L"SlxLastPath",
            REG_SZ,
            szPath,
            (lstrlenW(szPath) + 1) * sizeof(WCHAR)
            );

        SHSetTempValue(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
            L"SlxLastPathTime",
            REG_DWORD,
            &dwTimeMark,
            sizeof(dwTimeMark)
            );

        ResetExplorer();

        break;
    }

    case ID_MANUALCHECKSIGNATURE:
    {
        if (m_hManualCheckSignatureThread == NULL)
        {
            WaitForSingleObject(g_hManualCheckSignatureMutex, INFINITE);

            if (m_hManualCheckSignatureThread == NULL)
            {
                m_hManualCheckSignatureThread = CreateThread(NULL, 0, ManualCheckSignatureThreadProc, NULL, 0, NULL);
            }

            ReleaseMutex(g_hManualCheckSignatureMutex);
        }

        if (m_hManualCheckSignatureThread == NULL)
        {
            MessageBoxW(pici->hwnd, L"启动数字签名校验组件失败。", NULL, MB_ICONERROR);
        }
        else
        {
#if _MSC_VER > 1200
            map<wstring, HWND> *pMapPaths = new (std::nothrow) map<wstring, HWND>;
#else
            map<wstring, HWND> *pMapPaths = new map<wstring, HWND>;
#endif
            if (pMapPaths != NULL)
            {
                for (DWORD dwIndex = 0; dwIndex < m_uFileCount; dwIndex += 1)
                {
                    if (m_pFiles[dwIndex].bIsFile)
                    {
                        pMapPaths->insert(make_pair(wstring(m_pFiles[dwIndex].szPath), HWND(NULL)));
                    }
                }

                if (pMapPaths->empty())
                {
                    delete pMapPaths;

                    MessageBoxW(pici->hwnd, L"选中的项目不直接包含任何文件。", NULL, MB_ICONERROR);
                }
                else
                {
                    DialogBoxParamW(
                        g_hinstDll,
                        MAKEINTRESOURCEW(IDD_MANUALCHECKSIGNATURE_DIALOG),
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
        WCHAR szUnescapedFileName[MAX_PATH];
        WCHAR szMsgText[MAX_PATH + 1000];

        if (TryUnescapeFileName(m_pFiles[0].szPath, szUnescapedFileName, sizeof(szUnescapedFileName) / sizeof(WCHAR)))
        {
            if (PathFileExistsW(szUnescapedFileName))
            {
                wnsprintfW(
                    szMsgText,
                    sizeof(szMsgText) / sizeof(WCHAR),
                    L"“%s”已存在，是否要先删除？",
                    szUnescapedFileName
                    );

                if (IDYES == MessageBoxW(
                    pici->hwnd,
                    szMsgText,
                    L"请确认",
                    MB_ICONQUESTION | MB_YESNOCANCEL
                    ))
                {
                    if (!DeleteFileW(szUnescapedFileName))
                    {
                        MessageBoxW(pici->hwnd, L"删除失败。", NULL, MB_ICONERROR);

                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if (!MoveFileW(m_pFiles[0].szPath, szUnescapedFileName))
            {
                wnsprintfW(
                    szMsgText,
                    sizeof(szMsgText) / sizeof(WCHAR),
                    L"操作失败。\r\n\r\n错误码：%lu",
                    GetLastError()
                    );

                MessageBoxW(pici->hwnd, szMsgText, NULL, MB_ICONERROR);
            }
        }

        break;
    }

    case ID_DRV_INSTALL:
        CallDriverAction(L"install", m_strInputFile.c_str());
        break;

    case ID_DRV_START:
        CallDriverAction(L"start", m_strInputFile.c_str());
        break;

    case ID_DRV_STOP:
        CallDriverAction(L"stop", m_strInputFile.c_str());
        break;

    case ID_DRV_UNINSTALL:
        CallDriverAction(L"uninstall", m_strInputFile.c_str());
        break;

    case ID_COPY_PICTURE_HTML:
    {
        WCHAR *lpBuffer = (WCHAR *)malloc(m_uFileCount * MAX_PATH * sizeof(WCHAR));

        if (lpBuffer != NULL)
        {
            UINT uFileIndex = 0;
            WCHAR *lpPaths = lpBuffer;

            for (; uFileIndex < m_uFileCount; uFileIndex += 1)
            {
                if (m_pFiles[uFileIndex].bIsPicture)
                {
                    lstrcpynW(lpPaths, m_pFiles[uFileIndex].szPath, MAX_PATH);
                    lpPaths += (lstrlenW(lpPaths) + 1);
                }
            }

            lpPaths[0] = L'\0';

            SetClipboardPicturePathsByHtml(lpBuffer);

            free((void *)lpBuffer);
        }

        break;
    }

    case ID_CREATE_HARD_LINK:
        CreateHardLinkHelperW(m_strInputFile.c_str(), m_strInputFolder.c_str());
        break;

    case ID_CREATE_SOFT_LINK:
        CreateSoftLinkHelperW(m_strInputFile.c_str(), m_strInputFolder.c_str());
        break;

    case ID_LOAD_DLL:
        {
            HMODULE hModule = GetModuleHandleW(m_strInputFile.c_str());

            AutoCloseMessageBoxForThreadInSeconds(GetCurrentThreadId(), 6);

            if (hModule)
            {
                FreeLibrary(hModule);
                ShowErrorMessage(NULL, GetLastError());
            }
            else
            {
                hModule = LoadLibraryW(m_strInputFile.c_str());

                if (!hModule)
                {
                    ShowErrorMessage(NULL, GetLastError());
                }
                else
                {
                    MessageBoxFormat(NULL, L"info", MB_ICONINFORMATION | MB_TOPMOST, L"成功加载dll到0x%p位置", hModule);
                }
            }
        }
        break;

    case ID_LOCATE_EXPAND_SOURCE_FILE:
        {
            wstring strOldFilePath = GetStringFromFileMost4kBytes((m_strInputFile + L":edf").c_str());

            if (strOldFilePath.empty())
            {
                ShowErrorMessage(pici->hwnd, ERROR_FILE_NOT_FOUND);
            }
            else
            {
                BrowseForFile(strOldFilePath.c_str());
            }
        }
        break;

    case ID_EXPAND_DIR_FILES:
        EdfDoExpand(m_strInputFolder.c_str());
        break;

    case ID_UNEXPAND_DIR_FILES:
        if (IDYES == MessageBoxW(
            pici->hwnd,
            L"注意：这会删除所有前段时间展开的文件（名如“@@...”）！\r\n"
            L"复制、移动、改名的文件也会受到影响，其他目录或子目录内的文件不受影响。\r\n"
            L"\r\n"
            L"是否继续？",
            L"请确认",
            MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3))
        {
            EdfDoUnexpand(m_strInputFolder.c_str());
        }
        break;

    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
#define DEFAULT_MAP_SIZE    (4 * 1024 * 1024)
#define STATUS_COMPUTING    L"计算中..."
#define STATUS_HALTED       L"计算过程已被中止。"
#define STATUS_ERROR        L"计算中遇到错误。"

enum
{
    WM_ADDCOMPARE = WM_USER + 24,
    WM_STARTCALC,
};

struct HashPropSheetDlgParam
{
    WCHAR szFilePath[MAX_PATH];
    WCHAR szFilePath_2[MAX_PATH];

    HashPropSheetDlgParam()
    {
        ZeroMemory(this, sizeof(*this));
    }
};

struct HashCalcProcParam
{
    WCHAR szFile[MAX_PATH];
    HWND hwndDlg;
    HWND hwndFileSize;
    HWND hwndMd5;
    HWND hwndSha1;
    HWND hwndCrc32;
    HWND hwndStop;
};

DWORD CALLBACK CSlxComContextMenu::HashCalcProc(LPVOID lpParam)
{
    WCHAR szFilePath_Debug[MAX_PATH] = L"";
    HashCalcProcParam *pCalcParam = (HashCalcProcParam *)lpParam;

    if (pCalcParam != NULL &&
        IsWindow(pCalcParam->hwndStop) &&
        GetWindowLongPtr(pCalcParam->hwndStop, GWLP_USERDATA) == GetCurrentThreadId()
        )
    {
        //
        lstrcpynW(szFilePath_Debug, pCalcParam->szFile, RTL_NUMBER_OF(szFilePath_Debug));

        //提取文件大小
        WIN32_FILE_ATTRIBUTE_DATA wfad = {0};
        ULARGE_INTEGER uliFileSize;
        WCHAR szSizeStringWithBytes[128];

        GetFileAttributesExW(pCalcParam->szFile, GetFileExInfoStandard, &wfad);
        uliFileSize.HighPart = wfad.nFileSizeHigh;
        uliFileSize.LowPart = wfad.nFileSizeLow;

        if (uliFileSize.QuadPart == 0)
        {
            SetWindowTextW(pCalcParam->hwndFileSize, L"0 字节");
        }
        else
        {
            lstrcpynW(szSizeStringWithBytes + 64, L" 字节", 64);
            WCHAR *pChar = szSizeStringWithBytes + 64 - 1;
            int index = 0;

            while (TRUE)
            {
                *pChar-- = L'0' + uliFileSize.QuadPart % 10;
                uliFileSize.QuadPart /= 10;
                index += 1;

                if (uliFileSize.QuadPart == 0)
                {
                    break;
                }

                if (index % 3 == 0)
                {
                    *pChar-- = L',';
                }
            }

            SetWindowTextW(pCalcParam->hwndFileSize, pChar + 1);
        }

        uliFileSize.HighPart = wfad.nFileSizeHigh;
        uliFileSize.LowPart = wfad.nFileSizeLow;

        //设定初始显示文本
        WCHAR szMd5[100] = STATUS_COMPUTING;
        WCHAR szSha1[100] = STATUS_COMPUTING;
        WCHAR szCrc32[100] = STATUS_COMPUTING;

        SetWindowTextW(pCalcParam->hwndMd5, szMd5);
        SetWindowTextW(pCalcParam->hwndSha1, szSha1);
        SetWindowTextW(pCalcParam->hwndCrc32, szCrc32);

        //设置停止按钮文本，超过一定时间未计算完毕则显示按钮
        BOOL bShowStop = FALSE;
        DWORD dwTickCountMark = GetTickCount();
        const DWORD dwTimeToShowStop = 987;

        SetWindowTextW(pCalcParam->hwndStop, L"停止计算(已完成0%)");

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
            HANDLE hFile = CreateFileW(pCalcParam->szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

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
                            WCHAR szCtrlText[100] = L"";

                            wnsprintfW(szCtrlText, RTL_NUMBER_OF(szCtrlText), L"停止计算(已完成%u%%)", uliOffset.QuadPart * 100 / uliFileSize.QuadPart);
                            SetWindowTextW(pCalcParam->hwndStop, szCtrlText);

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
                wnsprintfW(szMd5 + index * 2, 3, L"%02x", uMd5[index]);
            }

            for (index = 0; index < RTL_NUMBER_OF(uSha1); index += 1)
            {
                wnsprintfW(szSha1 + index * 2, 3, L"%02x", uSha1[index]);
            }

            wnsprintfW(szCrc32, RTL_NUMBER_OF(szCrc32), L"%08x", uCrc32);
        }
        else
        {
            lstrcpynW(szMd5, STATUS_ERROR, RTL_NUMBER_OF(szMd5));
            lstrcpynW(szSha1, STATUS_ERROR, RTL_NUMBER_OF(szSha1));
            lstrcpynW(szCrc32, STATUS_ERROR, RTL_NUMBER_OF(szCrc32));
        }

        if (IsWindow(pCalcParam->hwndStop) &&
            GetWindowLongPtr(pCalcParam->hwndStop, GWLP_USERDATA) == GetCurrentThreadId()
            )
        {
            if (!!GetWindowLongPtr(pCalcParam->hwndDlg, GWLP_USERDATA))
            {
                CharUpperBuffW(szMd5, RTL_NUMBER_OF(szMd5));
                CharUpperBuffW(szSha1, RTL_NUMBER_OF(szSha1));
                CharUpperBuffW(szCrc32, RTL_NUMBER_OF(szCrc32));
            }

            SetWindowTextW(pCalcParam->hwndMd5, szMd5);
            SetWindowTextW(pCalcParam->hwndSha1, szSha1);
            SetWindowTextW(pCalcParam->hwndCrc32, szCrc32);
            ShowWindow(pCalcParam->hwndStop, SW_HIDE);
        }

        free(pCalcParam);
    }

    SafeDebugMessage(L"HashCalcProc线程%lu结束，文件“%s”\n", GetCurrentThreadId(), szFilePath_Debug);
    return 0;
}

INT_PTR CALLBACK CSlxComContextMenu::HashPropSheetDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RECT rect;
        LPPROPSHEETPAGEW pPsp = (LPPROPSHEETPAGEW)lParam;
        HashPropSheetDlgParam *pPropSheetDlgParam = (HashPropSheetDlgParam *)pPsp->lParam;

        //
        HWND hParent = GetParent(hwndDlg);

        if (IsWindow(hParent))
        {
            HFONT hFont = (HFONT)SendMessageW(hParent, WM_GETFONT, 0, 0);

            EnumChildWindows(hwndDlg, EnumChildSetFontProc, (LPARAM)hFont);
        }

        //
        SetDlgItemTextW(hwndDlg, IDC_FILEPATH, pPropSheetDlgParam->szFilePath);

        if (lstrlenW(pPropSheetDlgParam->szFilePath) > 0)
        {
            HashCalcProcParam *pCalcParam = (HashCalcProcParam *)malloc(sizeof(HashCalcProcParam));

            if (pCalcParam != NULL)
            {
                lstrcpynW(pCalcParam->szFile, pPropSheetDlgParam->szFilePath, RTL_NUMBER_OF(pCalcParam->szFile));
                pCalcParam->hwndDlg = hwndDlg;
                pCalcParam->hwndFileSize = GetDlgItem(hwndDlg, IDC_FILESIZE);
                pCalcParam->hwndMd5 = GetDlgItem(hwndDlg, IDC_MD5);
                pCalcParam->hwndSha1 = GetDlgItem(hwndDlg, IDC_SHA1);
                pCalcParam->hwndCrc32 = GetDlgItem(hwndDlg, IDC_CRC32);
                pCalcParam->hwndStop = GetDlgItem(hwndDlg, IDC_STOP);

                PostMessageW(hwndDlg, WM_STARTCALC, 0, (LPARAM)pCalcParam);
            }
        }

        if (lstrlenW(pPropSheetDlgParam->szFilePath_2) > 0)
        {
            SendMessageW(hwndDlg, WM_ADDCOMPARE, (WPARAM)pPropSheetDlgParam->szFilePath_2, 0);
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
        LPCWSTR lpCompareFile = (LPCWSTR)wParam;
        HashCalcProcParam *pCalcParam = (HashCalcProcParam *)malloc(sizeof(HashCalcProcParam));

        SetDlgItemTextW(hwndDlg, IDC_FILEPATH_2, lpCompareFile);

        if (pCalcParam != NULL)
        {
            lstrcpynW(pCalcParam->szFile, lpCompareFile, RTL_NUMBER_OF(pCalcParam->szFile));
            pCalcParam->hwndDlg = hwndDlg;
            pCalcParam->hwndFileSize = GetDlgItem(hwndDlg, IDC_FILESIZE_2);
            pCalcParam->hwndMd5 = GetDlgItem(hwndDlg, IDC_MD5_2);
            pCalcParam->hwndSha1 = GetDlgItem(hwndDlg, IDC_SHA1_2);
            pCalcParam->hwndCrc32 = GetDlgItem(hwndDlg, IDC_CRC32_2);
            pCalcParam->hwndStop = GetDlgItem(hwndDlg, IDC_STOP_2);

            PostMessageW(hwndDlg, WM_STARTCALC, 0, (LPARAM)pCalcParam);
        }

        //更新到双文件界面
        HWND hBegin = GetWindow(GetDlgItem(hwndDlg, IDC_COMPARE), GW_HWNDNEXT);
        HWND hEnd = GetDlgItem(hwndDlg, IDC_STOP_2);

        while (hBegin != hEnd)
        {
            ShowWindow(hBegin, SW_SHOW);
            hBegin = GetWindow(hBegin, GW_HWNDNEXT);
        }

        SetDlgItemTextW(hwndDlg, IDC_COMPARE, L"更换文件对比(&C)...");

        break;
    }

    case WM_NOTIFY:
    {
        NMHDR *phdr = (NMHDR *)lParam;

        if (phdr->code == PSN_APPLY)
        {
            OutputDebugStringW(L"PSN_APPLY");
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
            OPENFILENAMEW ofn = {OPENFILENAME_SIZE_VERSION_400W};
            WCHAR szFilter[128];
            WCHAR szFilePath[MAX_PATH] = L"";

            wnsprintfW(
                szFilter,
                RTL_NUMBER_OF(szFilter),
                L"All files (*.*)%c*.*%c",
                L'\0',
                L'\0'
                );

            ofn.hwndOwner = hwndDlg;
            ofn.Flags = OFN_HIDEREADONLY;
            ofn.hInstance = g_hinstDll;
            ofn.lpstrFilter = szFilter;
            ofn.lpstrTitle = L"打开要对比的文件";
            ofn.lpstrFile = szFilePath;
            ofn.nMaxFile = RTL_NUMBER_OF(szFilePath);

            if (GetOpenFileNameW(&ofn))
            {
                SendMessageW(hwndDlg, WM_ADDCOMPARE, (WPARAM)szFilePath, 0);
            }
            break;
        }

        case IDC_STOP:
            SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_STOP), GWLP_USERDATA, 0);
            ShowWindow(GetDlgItem(hwndDlg, IDC_STOP), SW_HIDE);
            SetWindowTextW(GetDlgItem(hwndDlg, IDC_MD5), STATUS_HALTED);
            SetWindowTextW(GetDlgItem(hwndDlg, IDC_SHA1), STATUS_HALTED);
            SetWindowTextW(GetDlgItem(hwndDlg, IDC_CRC32), STATUS_HALTED);
            break;

        case IDC_STOP_2:
            SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_STOP_2), GWLP_USERDATA, 0);
            ShowWindow(GetDlgItem(hwndDlg, IDC_STOP_2), SW_HIDE);
            SetWindowTextW(GetDlgItem(hwndDlg, IDC_MD5_2), STATUS_HALTED);
            SetWindowTextW(GetDlgItem(hwndDlg, IDC_SHA1_2), STATUS_HALTED);
            SetWindowTextW(GetDlgItem(hwndDlg, IDC_CRC32_2), STATUS_HALTED);
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
            int nFileCount = DragQueryFileW(hDrop, -1, NULL, 0);

            if (nFileCount != 1)
            {
                MessageBoxW(hwndDlg, L"仅支持同单个文件进行比较。", NULL, MB_ICONERROR);
            }
            else
            {
                WCHAR szFilePath[MAX_PATH] = L"";
                DWORD dwAttributes;

                DragQueryFileW(hDrop, 0, szFilePath, RTL_NUMBER_OF(szFilePath));
                dwAttributes = GetFileAttributesW(szFilePath);

                if (dwAttributes == INVALID_FILE_ATTRIBUTES)
                {
                    MessageBoxW(hwndDlg, L"待比较的文件不存在。", NULL, MB_ICONERROR);
                }
                else if (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    MessageBoxW(hwndDlg, L"仅支持同文件进行对比，而不是文件夹。", NULL, MB_ICONERROR);
                }
                else
                {
                    SendMessageW(hwndDlg, WM_ADDCOMPARE, (WPARAM)szFilePath, 0);
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
        DWORD (WINAPI *CharChangeBuff[])(LPWSTR lpsz, DWORD cchLength) = {CharUpperBuffW, CharLowerBuffW};
        BOOL bCaseStatus = !!GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
        WCHAR szBuffer[128];
        int index = 0;

        for (; index < RTL_NUMBER_OF(nIds); index += 1)
        {
            GetDlgItemTextW(hwndDlg, nIds[index], szBuffer, RTL_NUMBER_OF(szBuffer));
            CharChangeBuff[bCaseStatus](szBuffer, RTL_NUMBER_OF(szBuffer));
            SetDlgItemTextW(hwndDlg, nIds[index], szBuffer);
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
        WCHAR szText[256], szText2[256];
        HDC hDc = (HDC)wParam;
        int index = 0, index2;

        for (; index < RTL_NUMBER_OF(nIds); index += 1)
        {
            if (lParam == (LPARAM)GetDlgItem(hwndDlg, nIds[index]))
            {
                index = index / 2 * 2;
                index2 = index + 1;

                GetDlgItemTextW(hwndDlg, nIds[index], szText, RTL_NUMBER_OF(szText));
                GetDlgItemTextW(hwndDlg, nIds[index2], szText2, RTL_NUMBER_OF(szText2));

                if (IsWindowVisible(GetDlgItem(hwndDlg, nIds[index2])) && 0 != lstrcmpiW(szText, szText2))
                {
                    SetTextColor(hDc, RGB(0, 0, 99));
                    SetBkMode(hDc, TRANSPARENT);
                }

                return (INT_PTR)GetStockObject(WHITE_BRUSH);
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

UINT CALLBACK CSlxComContextMenu::HashPropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp)
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
    LPCWSTR lpPath,
    BOOL bIsFile,
    BOOL bSubDirIncluded,
    const FILETIME *pftCreated,
    const FILETIME *pftModified,
    const FILETIME *pftAccessed,
    wstringstream &ssError
    )
{
    if (pftCreated == NULL || pftModified == NULL || pftAccessed == NULL)
    {
        return;
    }

    list<wstring> listDirs;
    list<wstring> listFiles;
    list<wstring>::const_iterator it;

    listFiles.push_back(lpPath);

    if (!bIsFile && bSubDirIncluded)
    {
        wstring strPath = lpPath;
        WIN32_FIND_DATAW wfd;
        HANDLE hFind = FindFirstFileW((strPath + L"\\*").c_str(), &wfd);

        if (hFind != NULL && hFind != INVALID_HANDLE_VALUE)
        {
            do 
            {
                if (lstrcmpiW(wfd.cFileName, L"..") != 0 &&
                    lstrcmpiW(wfd.cFileName, L".") != 0
                    )
                {
                    if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        listDirs.push_back(strPath + L"\\" + wfd.cFileName);
                    }
                    else
                    {
                        listFiles.push_back(strPath + L"\\" + wfd.cFileName);
                    }
                }

            } while (FindNextFileW(hFind, &wfd));

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
            hFileObject = CreateFileW(it->c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        }
        else
        {
            hFileObject = CreateFileW(it->c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        }

        if (hFileObject == INVALID_HANDLE_VALUE)
        {
            ssError<<L"“"<<it->c_str()<<L"”打开文件失败，错误码"<<GetLastError()<<L"\r\n";
        }
        else
        {
            do
            {
                if (pftCreated == NULL || pftModified == NULL || pftAccessed == NULL)
                {
                    if (!GetFileTime(hFileObject, &ftCreated, &ftModified, &ftAccessed))
                    {
                        ssError<<L"“"<<it->c_str()<<L"”获取文件原时间失败，错误码"<<GetLastError()<<L"\r\n";
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
                    ssError<<L"“"<<it->c_str()<<L"”设置文件新时间失败，错误码"<<GetLastError()<<L"\r\n";
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
    wstringstream ssError;

    GetSystemTime(&stNow);
    SystemTimeToFileTime(&stNow, &ftNow);

#define INIT_FILETIME_POINTER(str1, str2) do                                                                         \
    {                                                                                                                \
        if (IsDlgButtonChecked(hwndDlg, IDC_NOW_##str1))                                                             \
        {                                                                                                            \
            pft##str2 = &ftNow;                                                                                      \
        }                                                                                                            \
        else if (GDT_VALID == SendDlgItemMessageW(hwndDlg, IDC_DATE_##str1, DTM_GETSYSTEMTIME, 0, (LPARAM)&st##str2))\
        {                                                                                                            \
            SendDlgItemMessageW(hwndDlg, IDC_TIME_##str1, DTM_GETSYSTEMTIME, 0, (LPARAM)&stForTime);                 \
                                                                                                                     \
            st##str2.wHour = stForTime.wHour;                                                                        \
            st##str2.wMinute = stForTime.wMinute;                                                                    \
            st##str2.wSecond = stForTime.wSecond;                                                                    \
            st##str2.wMilliseconds = stForTime.wMilliseconds;                                                        \
                                                                                                                     \
            SystemTimeToFileTime(&st##str2, &ft##str2);                                                              \
            LocalFileTimeToFileTime(&ft##str2, &ft##str2);                                                           \
                                                                                                                     \
            pft##str2 = &ft##str2;                                                                                   \
        }                                                                                                            \
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
        if (IDYES == MessageBoxW(
            hwndDlg,
            L"任务执行完毕，一部分文件时间没有修改成功。\r\n\r\n"
            L"选择“是”查看错误列表。",
            NULL,
            MB_ICONERROR | MB_YESNO
            ))
        {
            wstring strError = ssError.str();

            if (strError.size() < 1000)
            {
                MessageBoxW(hwndDlg, strError.c_str(), L"错误信息", MB_ICONINFORMATION);
            }
            else
            {
                WCHAR szTempPath[MAX_PATH] = L"";
                WCHAR szTempFilePath[MAX_PATH] = L"";

                GetTempPathW(RTL_NUMBER_OF(szTempPath), szTempPath);
                GetTempFileNameW(szTempPath, L"SlxCom", 0, szTempFilePath);

                BOOL bWriteSucceed = FALSE;
                HANDLE hFile = CreateFileW(szTempFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hFile != INVALID_HANDLE_VALUE)
                {
                    unsigned char bom[] = {0xef, 0xbb, 0xbf};
                    strutf8 strErrorUtf8 = WtoU(strError);

                    bWriteSucceed = WriteFileHelper(hFile, bom, sizeof(bom)) && WriteFileHelper(hFile, strErrorUtf8.c_str(), (DWORD)strErrorUtf8.size());
                    CloseHandle(hFile);
                }

                if (!bWriteSucceed)
                {
                    MessageBoxW(hwndDlg, L"错误信息保存到文件失败", NULL, MB_ICONERROR);
                }
                else
                {
                    WCHAR szCommand[MAX_PATH + 1024];
                    STARTUPINFOW si = {sizeof(si)};
                    PROCESS_INFORMATION pi;

                    PathQuoteSpacesW(szTempFilePath);
                    wnsprintfW(szCommand, RTL_NUMBER_OF(szCommand), L"notepad %s", szTempFilePath);

                    if (CreateProcessW(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                    {
                        WaitForInputIdle(pi.hProcess, 10 * 1000);

                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    else
                    {
                        MessageBoxW(hwndDlg, L"启动记事本查看错误信息失败", NULL, MB_ICONERROR);
                    }
                }

                DeleteFileW(szTempFilePath);
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
        SendMessageW(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, NULL);
        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, TRUE);
    }

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        LPPROPSHEETPAGEW pPsp = (LPPROPSHEETPAGEW)lParam;
        FileTimePropSheetDlgParam *pPropSheetDlgParam = (FileTimePropSheetDlgParam *)pPsp->lParam;

        //
        HWND hParent = GetParent(hwndDlg);

        if (IsWindow(hParent))
        {
            HFONT hFont = (HFONT)SendMessageW(hParent, WM_GETFONT, 0, 0);
            EnumChildWindows(hwndDlg, EnumChildSetFontProc, (LPARAM)hFont);
        }

        //
        DWORD dwFileCount = 0;
        DWORD dwDirectoryCount = 0;

        if (pPropSheetDlgParam->uFileCount == 1)
        {
            SetDlgItemTextW(hwndDlg, IDC_FILEPATH, pPropSheetDlgParam->pFiles[0].szPath);

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
            WCHAR szText[MAX_PATH];

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

            wnsprintfW(szText, RTL_NUMBER_OF(szText), L"%lu个文件，%lu个文件夹", dwFileCount, dwDirectoryCount);
            SetDlgItemTextW(hwndDlg, IDC_FILEPATH, szText);
        }

        if (dwDirectoryCount == 0)
        {
            ShowWindow(GetDlgItem(hwndDlg, IDC_SUBDIR), SW_HIDE);
        }
        else if (dwFileCount == 0)
        {
            SendDlgItemMessageW(hwndDlg, IDC_SUBDIR, BM_SETCHECK, BST_CHECKED, 0);
        }

        // hwndDlg的用户数据为pPropSheetDlgParam
        // IDC_ABOUT的用户数据为修改标识
        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, FALSE);
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)pPropSheetDlgParam);

        return FALSE;
    }

//     case WM_LBUTTONDOWN:
//         SendMessageW(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, NULL);
//         SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, TRUE);
//         break;
// 
//     case WM_RBUTTONDOWN:
//         SendMessageW(GetParent(hwndDlg), PSM_UNCHANGED, (WPARAM)hwndDlg, NULL);
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
                        SendMessageW(GetParent(hwndDlg), PSM_UNCHANGED, (WPARAM)hwndDlg, NULL);
                        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, FALSE);
                    }
                    else
                    {
                        SendMessageW(GetParent(hwndDlg), PSM_CHANGED, (WPARAM)hwndDlg, NULL);
                        SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_ABOUT), GWLP_USERDATA, TRUE);
                    }
                }
                else
                {
                    SendMessageW(GetParent(hwndDlg), PSM_UNCHANGED, (WPARAM)hwndDlg, NULL);
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
                        SendDlgItemMessageW(hwndDlg, IDC_NOW_CREATED, BM_SETCHECK, BST_UNCHECKED, 0);
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
                        SendDlgItemMessageW(hwndDlg, IDC_NOW_MODIFIED, BM_SETCHECK, BST_UNCHECKED, 0);
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
                        SendDlgItemMessageW(hwndDlg, IDC_NOW_ACCESSED, BM_SETCHECK, BST_UNCHECKED, 0);
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
            SendDlgItemMessageW(hwndDlg, IDC_DATE_CREATED, DTM_SETSYSTEMTIME, GDT_NONE, 0);
            EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_CREATED), FALSE);
            break;

        case IDC_NOW_MODIFIED:
            SendDlgItemMessageW(hwndDlg, IDC_DATE_MODIFIED, DTM_SETSYSTEMTIME, GDT_NONE, 0);
            EnableWindow(GetDlgItem(hwndDlg, IDC_TIME_MODIFIED), FALSE);
            break;

        case IDC_NOW_ACCESSED:
            SendDlgItemMessageW(hwndDlg, IDC_DATE_ACCESSED, DTM_SETSYSTEMTIME, GDT_NONE, 0);
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

UINT CALLBACK CSlxComContextMenu::FileTimePropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp)
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
class CPeInformationPage
{
public:
    CPeInformationPage(const wstring &strFilePath)
        : m_strFilePath(strFilePath)
        , m_hwndDlg(NULL)
    {
    }

    ~CPeInformationPage()
    {
        if (IsWindow(m_hwndDlg))
        {
            DestroyWindow(m_hwndDlg);
        }
    }

public:
    static INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        const LPCWSTR lpWndPropName = L"__CPeInformationPage_PropName";

        if (uMsg == WM_INITDIALOG)
        {
            LPPROPSHEETPAGEW pPsp = (LPPROPSHEETPAGEW)lParam;
            CPeInformationPage *pPeInformationPage = (CPeInformationPage *)pPsp->lParam;

            SetPropW(hwndDlg, lpWndPropName, (HANDLE)pPeInformationPage);
        }

        CPeInformationPage *pCPeInformationPage = (CPeInformationPage *)GetPropW(hwndDlg, lpWndPropName);

        if (pCPeInformationPage != NULL)
        {
            return pCPeInformationPage->PrivateDlgProc(hwndDlg, uMsg, wParam, lParam);
        }

        if (uMsg == WM_DESTROY)
        {
            RemovePropW(hwndDlg, lpWndPropName);
        }

        return FALSE;
    }

    static UINT CALLBACK PropSheetCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGEW pPsp)
    {
        switch (uMsg)
        {
        case PSPCB_ADDREF:
            break;

        case PSPCB_CREATE:
            break;

        case PSPCB_RELEASE:
            delete (CPeInformationPage *)pPsp->lParam;
            break;

        default:
            break;
        }

        return TRUE;
    }

private:
    struct WorkThreadParam
    {
        wstring strFilePath;
        HWND hwndDlg;

    public:
        WorkThreadParam(const wstring &strFilePath, HWND hwndDlg)
        {
            this->strFilePath = strFilePath;
            this->hwndDlg = hwndDlg;
        }
    };

    static DWORD CALLBACK WorkProc(LPVOID lpParam)
    {
        // 
        WorkThreadParam *pParam = (WorkThreadParam *)lpParam;
        WorkThreadParam param = *pParam;

        delete pParam;

        // 
        WNDCLASSEXW wcex = {sizeof(wcex)};
        LPCWSTR lpClassName = L"__PeInformationPageMiddleWnd";

        wcex.hInstance = g_hinstDll;
        wcex.lpszClassName = lpClassName;
        wcex.lpfnWndProc = DefWindowProc;

        RegisterClassExW(&wcex);

        HWND hMiddleWindow = CreateWindowW(lpClassName, NULL, WS_OVERLAPPEDWINDOW, 0, 0, 1555, 100, NULL, NULL, g_hinstDll, NULL);

        if (!IsWindow(hMiddleWindow))
        {
            return 0;
        }

        // 
        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        WCHAR szCmd[MAX_PATH * 2 + 1024];
        HANDLE hWorkProcess = NULL;
        WCHAR szDllPath[MAX_PATH] = L"";

        GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
        wnsprintfW(szCmd, RTL_NUMBER_OF(szCmd), L"rundll32.exe \"%s\" GetPeInformationAndReport %I64u \"%ls\"", szDllPath, (unsigned __int64)hMiddleWindow, param.strFilePath.c_str());

        if (CreateProcessW(NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            while (IsWindow(hMiddleWindow))
            {
                DWORD dwWaitResult = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLINPUT);

                if (dwWaitResult == WAIT_OBJECT_0)
                {
                    break;
                }
                else if (dwWaitResult = WAIT_OBJECT_0 + 1)
                {
                    MSG msg;

                    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&msg);
                        DispatchMessageW(&msg);
                    }
                }
                else
                {
                    break;
                }
            }

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }

        // 获取结果
        if (IsWindow(param.hwndDlg))
        {
            DWORD dwWindowTextLength = GetWindowTextLength(hMiddleWindow);

            if (dwWindowTextLength > 0)
            {
                try
                {
                    WCHAR *szText = new WCHAR[dwWindowTextLength + 10];

                    if (GetWindowTextW(hMiddleWindow, szText, dwWindowTextLength + 10) > 0)
                    {
                        if (IsWindow(param.hwndDlg))
                        {
                            HWND hTreeControl = GetDlgItem(param.hwndDlg, IDC_TREE);

                            if (IsWindow(hTreeControl))
                            {
                                SendDlgItemMessageW(param.hwndDlg, IDC_TREE, TVM_DELETEITEM, 0, 0);
                                AttachToTreeCtrlW(szText, hTreeControl);
                                ExpandTreeControlForLevel(hTreeControl, NULL, 2);
                            }
                        }
                    }

                    delete []szText;
                }
                catch (bad_alloc &)
                {
                }
            }
        }

        return 0;
    }

    INT_PTR PrivateDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_INITDIALOG:
            m_hwndDlg = hwndDlg;
            CloseHandle(CreateThread(NULL, 0, WorkProc, new WorkThreadParam(m_strFilePath, m_hwndDlg), 0, NULL));
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_ABOUT:
                SlxComAbout(hwndDlg);
                break;

            default:
                break;
            }
            break;

        default:
            break;
        }

        return FALSE;
    }

private:
    wstring m_strFilePath;
    HWND m_hwndDlg;
};

//////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSlxComContextMenu::AddPages(LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam)
{
    HRESULT hr = E_NOTIMPL;

    if (ShouldAddFileTimePropSheet())
    {
        PROPSHEETPAGEW psp = {sizeof(psp)};
        HPROPSHEETPAGE hPage;
        FileTimePropSheetDlgParam *pPropSheetDlgParam = new FileTimePropSheetDlgParam(m_pFiles, m_uFileCount);

        psp.dwFlags     = PSP_USETITLE | PSP_DEFAULT | PSP_USECALLBACK;
        psp.hInstance   = g_hinstDll;
        psp.pszTemplate = MAKEINTRESOURCEW(IDD_FILETIMEPAGE);
        psp.pszTitle    = L"文件时间";
        psp.pfnDlgProc  = FileTimePropSheetDlgProc;
        psp.lParam      = (LPARAM)pPropSheetDlgParam;
        psp.pfnCallback = FileTimePropSheetCallback;

        hPage = CreatePropertySheetPageW(&psp);

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
        PROPSHEETPAGEW psp = {sizeof(psp)};
        HPROPSHEETPAGE hPage;
        HashPropSheetDlgParam *pPropSheetDlgParam = new HashPropSheetDlgParam;

        lstrcpynW(pPropSheetDlgParam->szFilePath, m_pFiles[0].szPath, RTL_NUMBER_OF(pPropSheetDlgParam->szFilePath));

        if (m_uFileCount > 1)
        {
            lstrcpynW(pPropSheetDlgParam->szFilePath_2, m_pFiles[1].szPath, RTL_NUMBER_OF(pPropSheetDlgParam->szFilePath_2));
        }

        psp.dwFlags     = PSP_USETITLE | PSP_DEFAULT | PSP_USECALLBACK;
        psp.hInstance   = g_hinstDll;
        psp.pszTemplate = MAKEINTRESOURCEW(IDD_HASHPAGE);
        psp.pszTitle    = L"校验";
        psp.pfnDlgProc  = HashPropSheetDlgProc;
        psp.lParam      = (LPARAM)pPropSheetDlgParam;
        psp.pfnCallback = HashPropSheetCallback;

        hPage = CreatePropertySheetPageW(&psp);

        if (NULL != hPage)
        {
            if (!pfnAddPage(hPage, lParam))
            {
                DestroyPropertySheetPage(hPage);
            }
        }

        hr = S_OK;
    }

    if (ShouldAddPeInformationSheet())
    {
        PROPSHEETPAGEW psp = {sizeof(psp)};
        HPROPSHEETPAGE hPage;
        CPeInformationPage *pPropSheetPage = new CPeInformationPage(WtoW(m_pFiles[0].szPath).c_str());

        psp.dwFlags     = PSP_USETITLE | PSP_DEFAULT | PSP_USECALLBACK;
        psp.hInstance   = g_hinstDll;
        psp.pszTemplate = MAKEINTRESOURCEW(IDD_PEINFORMATIONPAGE);
        psp.pszTitle    = L"PE信息";
        psp.pfnDlgProc  = CPeInformationPage::DlgProc;
        psp.lParam      = (LPARAM)pPropSheetPage;
        psp.pfnCallback = CPeInformationPage::PropSheetCallback;

        hPage = CreatePropertySheetPageW(&psp);

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
    SendMessageW(hwnd, WM_SETFONT, lParam, 0);
    return TRUE;
}

BOOL CSlxComContextMenu::ConvertToShortPaths()
{
    WCHAR szFilePathTemp[MAX_PATH];

    for (UINT uFileIndex = 0; uFileIndex < m_uFileCount; uFileIndex += 1)
    {
        GetShortPathNameW(m_pFiles[uFileIndex].szPath, szFilePathTemp, sizeof(szFilePathTemp) / sizeof(WCHAR));
        lstrcpynW(m_pFiles[uFileIndex].szPath, szFilePathTemp, sizeof(szFilePathTemp) / sizeof(WCHAR));
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

BOOL CSlxComContextMenu::ShouldAddPeInformationSheet()
{
    return m_uFileCount == 1 && m_pFiles[0].bIsFile && IsFileLikePeFile(WtoW(m_pFiles[0].szPath).c_str());
}

void CSlxComContextMenu::CallDriverAction(LPCWSTR lpAction, LPCWSTR lpFilePath)
{
    WCHAR szDllPath[MAX_PATH];
    WCHAR szArguments[MAX_PATH * 2 + 1024];

    GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
    PathQuoteSpacesW(szDllPath);

    wnsprintfW(szArguments, RTL_NUMBER_OF(szArguments), L"%ls DriverAction %ls \"%ls\"", szDllPath, lpAction, lpFilePath);
    ElevateAndRunW(L"rundll32.exe", szArguments, NULL, SW_HIDE);
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

    AutoCloseMessageBoxForThreadInSeconds(GetCurrentThreadId(), 6);

    vector<wstring> vectorFiles(lpArgs, lpArgs + nArgCount);

    WCHAR szCmd[MAX_PATH + 1024];
    WCHAR szPath[MAX_PATH];
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    vector<HANDLE> vectorProcesses;
    vector<DWORD> vectorExitCodes;

    for (vector<wstring>::const_iterator it = vectorFiles.begin(); it != vectorFiles.end(); ++it)
    {
        lstrcpynW(szPath, it->c_str(), RTL_NUMBER_OF(szPath));
        PathQuoteSpacesW(szPath);
        wnsprintfW(szCmd, RTL_NUMBER_OF(szCmd), L"regsvr32 %ls %ls", lpMarkArgument,szPath);

        if (CreateProcessW(NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(pi.hThread);
            vectorProcesses.push_back(pi.hProcess);
        }
        else
        {
            MessageBoxFormat(NULL, NULL, MB_ICONERROR | MB_TOPMOST, L"启动regsvr32失败，错误码%lu", GetLastError());
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
            ssResults<<L"[成功] ";
        }
        else
        {
            ssResults<<L"[失败] ";
        }

        ssResults<<vectorFiles.at(i);
    }

    MessageBoxW(NULL, ssResults.str().c_str(), L"执行结果", MB_ICONINFORMATION | MB_TOPMOST);
}

void WINAPI BatchRegisterDllsW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    BatchRegisterOrUnregisterDllsW(L"/s", lpszCmdLine);
}

void WINAPI BatchUnregisterDllsW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    BatchRegisterOrUnregisterDllsW(L"/s /u", lpszCmdLine);
}