#define _WIN32_WINNT 0x500
#include "SlxComContextMenu.h"
#include <shlwapi.h>
#include <CommCtrl.h>
#include "SlxComOverlay.h"
#include "SlxComTools.h"
#include "resource.h"
#include "SlxString.h"
#pragma warning(disable: 4786)
#include <map>

using namespace std;

extern HINSTANCE g_hinstDll;

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

volatile HANDLE CSlxComContextMenu::m_hManualCheckSignatureThread = NULL;
static HANDLE g_hManualCheckSignatureMutex = CreateMutex(NULL, FALSE, NULL);

CSlxComContextMenu::CSlxComContextMenu()
{
    m_dwRefCount = 0;

    m_pFiles = new FileInfo[1];
    m_uFileCount = 1;
    lstrcpy(m_pFiles[0].szPath, TEXT("1:\\"));
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
#define ID_ID6                  6
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
#define ID_18                   18
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

    BOOL bShiftDown = GetKeyState(VK_LSHIFT) < 0 || GetKeyState(VK_RSHIFT) < 0;

    //Check Dll And File
    BOOL bExistDll = FALSE;
    BOOL bExistFile = FALSE;

    for(uFileIndex = 0; uFileIndex < m_uFileCount; uFileIndex += 1)
    {
        if(m_pFiles[uFileIndex].bIsDll)
        {
            bExistDll = TRUE;
        }

        if(m_pFiles[uFileIndex].bIsFile)
        {
            bExistFile = TRUE;
        }

        if(bExistDll * bExistFile)
        {
            break;
        }
    }

    //clipboard functions
    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, TEXT(""));

    //CopyFilePath
    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COPYFULLPATH, TEXT("��������·��"));
    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COPYFULLPATH, MF_BYCOMMAND, g_hCopyFullPathBmp, g_hCopyFullPathBmp);

    //Dll Register
    if(bExistDll)
    {
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_REGISTER, TEXT("ע�����(&R)"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_REGISTER, MF_BYCOMMAND, g_hInstallBmp, g_hInstallBmp);

        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNREGISTER, TEXT("ȡ��ע�����(&U)"));
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
            InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_COMBINE, TEXT("�ļ��ϳ�(&C)"));
            SetMenuItemBitmaps(hmenu, idCmdFirst + ID_COMBINE, MF_BYCOMMAND, g_hCombineBmp, g_hCombineBmp);
        }
    }

    //Check Signature
    if(bExistFile)
    {
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_MANUALCHECKSIGNATURE, TEXT("У������ǩ��"));
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
                        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)hPopupMenu, TEXT("��������"));
                        SetMenuItemBitmaps(hmenu, indexMenu + uMenuIndex - 1, MF_BYPOSITION, g_hTryRunBmp, g_hTryRunBmp);

                        //Try to run
                        InsertMenu(hPopupMenu, 1, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_TRYRUN, TEXT("ֱ������"));
                        SetMenuItemBitmaps(hPopupMenu, idCmdFirst + ID_TRYRUN, MF_BYCOMMAND, g_hTryRunBmp, g_hTryRunBmp);

                        InsertMenu(hPopupMenu, 2, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_TRYRUNWITHARGUMENTS, TEXT("������������(&P)"));
                        SetMenuItemBitmaps(hPopupMenu, idCmdFirst + ID_TRYRUNWITHARGUMENTS, MF_BYCOMMAND, g_hTryRunWithArgumentsBmp, g_hTryRunWithArgumentsBmp);

                        DestroyMenu(hPopupMenu);
                    }
                }

                //OpenWithNotepad
                InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_OPENWITHNOTEPAD, TEXT("�ü��±���"));
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_OPENWITHNOTEPAD, MF_BYCOMMAND, g_hOpenWithNotepadBmp, g_hOpenWithNotepadBmp);

                //Unescape
                TCHAR szUnescapedFileName[MAX_PATH];

                if(TryUnescapeFileName(m_pFiles[0].szPath, szUnescapedFileName, sizeof(szUnescapedFileName) / sizeof(TCHAR)))
                {
                    TCHAR szCommandText[MAX_PATH + 100];

                    wnsprintf(
                        szCommandText,
                        sizeof(szCommandText) / sizeof(TCHAR),
                        TEXT("������Ϊ��%s��"),
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
                    TCHAR szMenuText[MAX_PATH + 1000] = TEXT("��ӿ�ݶ���");

                    if (lstrlen(szCommandInReg) > 0)
                    {
                        wnsprintf(
                            szMenuText,
                            sizeof(szMenuText) / sizeof(TCHAR),
                            TEXT("�޸Ŀ�ݶ��%s��"),
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
                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_INSTALL, TEXT("��������-��װ"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_INSTALL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_START, TEXT("��������-����"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_START, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_STOP, TEXT("��������-ֹͣ"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_STOP, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);

                    InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_DRV_UNINSTALL, TEXT("��������-ж��"));
                    SetMenuItemBitmaps(hmenu, idCmdFirst + ID_DRV_UNINSTALL, MF_BYCOMMAND, g_hDriverBmp, g_hDriverBmp);
                }
            }

            if((dwFileAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                //RunCmdHere
                InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_RUNCMDHERE, TEXT("�ڴ˴�����������"));
                SetMenuItemBitmaps(hmenu, idCmdFirst + ID_RUNCMDHERE, MF_BYCOMMAND, g_hRunCmdHereBmp, g_hRunCmdHereBmp);
            }
        }
    }

    if(bShiftDown)
    {
        //����Explorer
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_KILLEXPLORER, TEXT("����Explorer����"));
        SetMenuItemBitmaps(hmenu, idCmdFirst + ID_KILLEXPLORER, MF_BYCOMMAND, g_hKillExplorerBmp, g_hKillExplorerBmp);
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, IDCOUNT);
}

STDMETHODIMP CSlxComContextMenu::GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
    LPCSTR lpText = "";

    if(idCmd == ID_REGISTER)
    {
        lpText = "ע�������";
    }
    else if(idCmd == ID_UNREGISTER)
    {
        lpText = "ȡ��ע�������";
    }
    else if(idCmd == ID_COMBINE)
    {
        if(m_uFileCount == 1)
        {
            lpText = "��ѡ����rar�ļ����ӵ�Ĭ��jpg�ļ�֮��";
        }
        else
        {
            lpText = "��ѡ����rar�ļ����ӵ�ѡ����jpg�ļ�֮��";
        }
    }
    else if(idCmd == ID_COPYFULLPATH)
    {
        lpText = "����ѡ�е��ļ����ļ��е�����·���������塣";
    }
    else if(idCmd == ID_APPPATH)
    {
        lpText = "ά���ڡ����С��Ի����п�����������Ŀ��";
    }
    else if(idCmd == ID_ID6)
    {
        lpText = "";
    }
    else if(idCmd == ID_TRYRUN)
    {
        lpText = "���Խ��ļ���Ϊ��ִ���ļ����С�";
    }
    else if(idCmd == ID_TRYRUNWITHARGUMENTS)
    {
        lpText = "���Խ��ļ���Ϊ��ִ���ļ����У������������в�����";
    }
    else if(idCmd == ID_RUNCMDHERE)
    {
        lpText = "�ڵ�ǰĿ¼���������С�";
    }
    else if(idCmd == ID_OPENWITHNOTEPAD)
    {
        lpText = "�ڼ��±��򿪵�ǰ�ļ���";
    }
    else if(idCmd == ID_KILLEXPLORER)
    {
        lpText = "����Explorer��";
    }
    else if(idCmd == ID_MANUALCHECKSIGNATURE)
    {
        lpText = "�ֶ�У��ѡ�е��ļ�������ǩ����";
    }
    else if(idCmd == ID_UNESCAPE)
    {
        lpText = "ת���ļ���Ϊ�Ϻ��ʵ���ʽ��";
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
        LPCTSTR lpExe = TEXT("regsvr32");
        TCHAR szCommand[MAX_PATH * 2];
        STARTUPINFO si = {sizeof(si)};
        PROCESS_INFORMATION pi;

        if(m_uFileCount == 1)
        {
            if(LOWORD(pici->lpVerb) == ID_REGISTER)
            {
                lpExe = TEXT("regsvr32");
            }
            else
            {
                lpExe = TEXT("regsvr32 /u");
            }

            wnsprintf(szCommand, sizeof(szCommand) / sizeof(TCHAR), TEXT("%s \"%s\""), lpExe, m_pFiles[0].szPath);

            if(CreateProcess(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
        else
        {
            UINT uIndex;

            if(LOWORD(pici->lpVerb) == ID_REGISTER)
            {
                lpExe = TEXT("regsvr32 /s");
            }
            else
            {
                lpExe = TEXT("regsvr32 /u /s");
            }

            HANDLE *pProcessHandle = new HANDLE[m_uFileCount];

            if(pProcessHandle != NULL)
            {
                for(uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                {
                    wnsprintf(szCommand, sizeof(szCommand) / sizeof(TCHAR), TEXT("%s \"%s\""), lpExe, m_pFiles[uIndex].szPath);

                    if(CreateProcess(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                    {
                        CloseHandle(pi.hThread);

                        pProcessHandle[uIndex] = pi.hProcess;
                    }
                    else
                    {
                        pProcessHandle[uIndex] = NULL;
                    }
                }

                while(TRUE)
                {
                    DWORD dwExitCodeProcess = 0;

                    for(uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                    {
                        GetExitCodeProcess(pProcessHandle[uIndex], &dwExitCodeProcess);

                        if(dwExitCodeProcess == STILL_ACTIVE)
                        {
                            break;
                        }
                        else
                        {
                            m_pFiles[uIndex].bRegEventSucceed = dwExitCodeProcess == 0;
                        }
                    }

                    if(uIndex == m_uFileCount)
                    {
                        break;
                    }
                    else
                    {
                        Sleep(10);
                    }
                }

                for(uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                {
                    CloseHandle(pProcessHandle[uIndex]);
                }

                TCHAR *szText = new TCHAR[(MAX_PATH + 20) * m_uFileCount];
                if(szText != NULL)
                {
                    szText[0] = TEXT('\0');

                    for(uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                    {
                        if(m_pFiles[uIndex].bRegEventSucceed)
                        {
                            lstrcat(szText, TEXT("[�ɹ�]"));
                        }
                        else
                        {
                            lstrcat(szText, TEXT("[ʧ��]"));
                        }

                        lstrcat(szText, m_pFiles[uIndex].szPath);
                        lstrcat(szText, TEXT("\r\n"));
                    }

                    MessageBox(pici->hwnd, szText, TEXT("RegSvr32 ִ�н��"), MB_ICONINFORMATION | MB_TOPMOST);

                    delete []szText;
                }

                delete []pProcessHandle;
            }
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
            MessageBox(pici->hwnd, TEXT("�ϳ��ļ�����"), NULL, MB_ICONERROR | MB_TOPMOST);
        }

        break;
    }

    case ID_APPPATH:
        ModifyAppPath(m_pFiles[0].szPath);
        break;

    case ID_ID6:
        break;

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

        if(IDYES == MessageBox(pici->hwnd, TEXT("ȷ��Ҫ���Խ����ļ���Ϊ��ִ���ļ�������"), TEXT("��ȷ��"), MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3))
        {
            if(!RunCommand(szCommandLine))
            {
                TCHAR szErrorMessage[MAX_PATH + 300];

                wnsprintf(szErrorMessage, sizeof(szErrorMessage) / sizeof(TCHAR), TEXT("�޷��������̣�������%lu\r\n\r\n%s"),
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
        TCHAR szCommand[MAX_PATH * 2] = TEXT("");

        GetEnvironmentVariable(TEXT("ComSpec"), szCommand, MAX_PATH);

        lstrcat(szCommand, TEXT(" /k pushd "));
        lstrcat(szCommand, m_pFiles[0].szPath);

        RunCommand(szCommand, NULL);

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

            wnsprintf(szErrorMessage, sizeof(szErrorMessage) / sizeof(TCHAR), TEXT("�޷��������̣�������%lu\r\n\r\n%s"),
                GetLastError(), szCommandLine);

            MessageBox(pici->hwnd, szErrorMessage, NULL, MB_ICONERROR | MB_TOPMOST);
        }

        break;
    }

    case ID_KILLEXPLORER:
    {
        DWORD dwTimeMark = GetTickCount();

        SHSetTempValue(HKEY_CURRENT_USER,
            TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"), TEXT("SlxLastPath"),
            REG_SZ, m_pFiles[0].szPath, (lstrlen(m_pFiles[0].szPath) + 1) * sizeof(TCHAR));

        SHSetTempValue(HKEY_CURRENT_USER,
            TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"), TEXT("SlxLastPathTime"),
            REG_DWORD, &dwTimeMark, sizeof(dwTimeMark));

        TerminateProcess(GetCurrentProcess(), 0);

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
            MessageBox(pici->hwnd, TEXT("��������ǩ��У�����ʧ�ܡ�"), NULL, MB_ICONERROR);
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

                    MessageBox(pici->hwnd, TEXT("ѡ�е���Ŀ��ֱ�Ӱ����κ��ļ���"), NULL, MB_ICONERROR);
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
                    TEXT("��%s���Ѵ��ڣ��Ƿ�Ҫ��ɾ����"),
                    szUnescapedFileName
                    );

                if(IDYES == MessageBox(
                    pici->hwnd,
                    szMsgText,
                    TEXT("��ȷ��"),
                    MB_ICONQUESTION | MB_YESNOCANCEL
                    ))
                {
                    if(!DeleteFile(szUnescapedFileName))
                    {
                        MessageBox(pici->hwnd, TEXT("ɾ��ʧ�ܡ�"), NULL, MB_ICONERROR);

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
                    TEXT("����ʧ�ܡ�\r\n\r\n�����룺%lu"),
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

    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

#define WM_SIGNRESULT (WM_USER + 101)

DWORD __stdcall CSlxComContextMenu::ManualCheckSignatureThreadProc(LPVOID lpParam)
{
    if(lpParam == 0)
    {
        while(TRUE)
        {
            SleepEx(INFINITE, TRUE);
        }
    }
    else
    {
        map<tstring, HWND> *pMapPaths = (map<tstring, HWND> *)lpParam;

        if(pMapPaths != NULL && !pMapPaths->empty())
        {
            map<tstring, HWND>::iterator it = pMapPaths->begin();
            HWND hTargetWindow = it->second;
            DWORD dwFileIndex = 0;

            if(IsWindow(hTargetWindow))             //ͨ���Ŷӣ����ڿ����Ѿ���ȡ��
            {
                for(; it != pMapPaths->end(); it++, dwFileIndex++)
                {
                    BOOL bSigned = FALSE;

                    if(PathFileExists(it->first.c_str()))
                    {
                        TCHAR szString[MAX_PATH + 100];

                        if(CSlxComOverlay::BuildFileMarkString(
                            it->first.c_str(),
                            szString,
                            sizeof(szString) / sizeof(TCHAR)
                            ))
                        {
                            if(IsFileSigned(it->first.c_str()))
                            {
                                CSlxComOverlay::m_cache.AddCache(szString, SS_1);

                                bSigned = TRUE;
                            }
                            else
                            {
                                CSlxComOverlay::m_cache.AddCache(szString, SS_2);
                            }
                        }
                    }

                    if(IsWindow(hTargetWindow))
                    {
                        PostMessage(hTargetWindow, WM_SIGNRESULT, dwFileIndex, bSigned);
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

#define LVH_INDEX       0
#define LVH_PATH        1
#define LVH_RESULT      2

struct ListCtrlSortStruct
{
    int nRet;
    HWND hListCtrl;
    int nSubItem;
};

int CALLBACK ListCtrlCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    ListCtrlSortStruct *pLcss = (ListCtrlSortStruct *)lParamSort;
    TCHAR szText1[1000] = TEXT("");
    TCHAR szText2[1000] = TEXT("");

    ListView_GetItemText(pLcss->hListCtrl, lParam1, pLcss->nSubItem, szText1, sizeof(szText1) / sizeof(TCHAR));
    ListView_GetItemText(pLcss->hListCtrl, lParam2, pLcss->nSubItem, szText2, sizeof(szText2) / sizeof(TCHAR));

    if(lstrcmp(szText1, szText2) == 0)
    {
        return 0;
    }

    if(pLcss->nSubItem == LVH_INDEX)
    {
        int n1 = StrToInt(szText1);
        int n2 = StrToInt(szText2);

        if(n1 > n2)
        {
            return pLcss->nRet;
        }
        else
        {
            return -pLcss->nRet;
        }
    }
    else if(pLcss->nSubItem == LVH_PATH)
    {
        return lstrcmp(szText1, szText2) * pLcss->nRet;
    }
    else if(pLcss->nSubItem == LVH_RESULT)
    {
        return lstrcmp(szText1, szText2) * pLcss->nRet;
    }
    else
    {
        return pLcss->nRet;
    }
}

INT_PTR __stdcall CSlxComContextMenu::ManualCheckSignatureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;
    BOOL bDlgProcResult = FALSE;
    HWND hFileList = GetDlgItem(hwndDlg, IDC_FILELIST);

    if(uMsg == WM_INITDIALOG)
    {
        //�趨�Ի���ͼ��
        if(hIcon == NULL)
        {
            hIcon = LoadIcon(g_hinstDll, MAKEINTRESOURCE(IDI_CONFIG_ICON));
        }

        if(hIcon != NULL)
        {
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        //����ȫѡ���
        ListView_SetExtendedListViewStyleEx(hFileList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        //����б�
        LVCOLUMN col = {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT};

        col.fmt = LVCFMT_LEFT;

        col.cx = 35;
        col.pszText = TEXT("#");
        ListView_InsertColumn(hFileList, LVH_INDEX, &col);

        col.cx = 345;
        col.pszText = TEXT("·��");
        ListView_InsertColumn(hFileList, LVH_PATH, &col);

        col.cx = 50;
        col.pszText = TEXT("���");
        ListView_InsertColumn(hFileList, LVH_RESULT, &col);

        //��ʼ����У�����
        map<tstring, HWND> *pMapPaths = (map<tstring, HWND> *)lParam;

        if(pMapPaths != NULL && !pMapPaths->empty())
        {
            //����б�
            TCHAR szText[1000];
            DWORD dwIndex = 0;
            LVITEM item = {LVIF_TEXT};
            map<tstring, HWND>::iterator it = pMapPaths->begin();

            it->second = hwndDlg;

            for(; it != pMapPaths->end(); it++, dwIndex++)
            {
                item.iItem = dwIndex;
                item.pszText = szText;

                wnsprintf(szText, sizeof(szText) / sizeof(TCHAR), TEXT("%lu"), dwIndex + 1);
                ListView_InsertItem(hFileList, &item);

                lstrcpyn(szText, it->first.c_str(), sizeof(szText) / sizeof(TCHAR));
                ListView_SetItemText(hFileList, dwIndex, LVH_PATH, szText);

                lstrcpyn(szText, TEXT(""), sizeof(szText) / sizeof(TCHAR));
                ListView_SetItemText(hFileList, dwIndex, LVH_RESULT, szText);
            }

            //��¼���ļ�����
            TCHAR szWindowText[100];

            wnsprintf(szWindowText, sizeof(szWindowText) / sizeof(TCHAR), TEXT("У������ǩ�� 0/%lu(0.00%%)"), pMapPaths->size());
            SetWindowText(hwndDlg, szWindowText);

            //��ʼУ��
            QueueUserAPC(
                (PAPCFUNC)ManualCheckSignatureThreadProc,
                m_hManualCheckSignatureThread,
                (DWORD)pMapPaths
                );
        }

        //������ʱ��
        SetTimer(hwndDlg, 1, 40, NULL);

        bDlgProcResult = TRUE;
    }
    else if(uMsg == WM_TIMER)
    {
        if(wParam == 1)
        {
            static DWORD dwTimeStamp = 0;

            TCHAR szText[100];
            TCHAR *szTimeStr[] = {
                TEXT("��"),
                TEXT("��"),
                TEXT("��"),
                TEXT("��")
            };
            HWND hFileList = GetDlgItem(hwndDlg, IDC_FILELIST);

            for(int nIndex = 0; nIndex < ListView_GetItemCount(GetDlgItem(hwndDlg, IDC_FILELIST)); nIndex += 1)
            {
                ListView_GetItemText(hFileList, nIndex, LVH_RESULT, szText, sizeof(szText) / sizeof(TCHAR));

                if(lstrlen(szText) <= 1)
                {
                    ListView_SetItemText(
                        hFileList,
                        nIndex,
                        LVH_RESULT,
                        szTimeStr[(dwTimeStamp + nIndex) % (sizeof(szTimeStr) / sizeof(szTimeStr[0]))]
                    );
                }
            }

            dwTimeStamp += 1;
        }
    }
    else if(uMsg == WM_NOTIFY)
    {
        LPNMHDR lpNmHdr = (LPNMHDR)lParam;

        if(lpNmHdr != NULL && lpNmHdr->idFrom == IDC_FILELIST)
        {
            if(lpNmHdr->code == NM_CUSTOMDRAW)
            {
                LPNMLVCUSTOMDRAW lpNMCustomDraw = (LPNMLVCUSTOMDRAW)lParam;

                if(CDDS_PREPAINT == lpNMCustomDraw->nmcd.dwDrawStage)
                {
                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                }
                else if(CDDS_ITEMPREPAINT == lpNMCustomDraw->nmcd.dwDrawStage)
                {
                    TCHAR szResult[100] = TEXT("");

                    ListView_GetItemText(
                        hFileList,
                        lpNMCustomDraw->nmcd.dwItemSpec,
                        LVH_RESULT,
                        szResult,
                        sizeof(szResult) / sizeof(TCHAR)
                        );

                    if(lstrcmpi(szResult, TEXT("��ǩ��")) == 0)
                    {
                        lpNMCustomDraw->clrText = RGB(0, 0, 255);
                    }

                    if(lpNMCustomDraw->nmcd.dwItemSpec % 2)
                    {
                        lpNMCustomDraw->clrTextBk = RGB(240, 240, 230);
                    }
                    else
                    {
                        lpNMCustomDraw->clrTextBk = RGB(255, 255, 255);
                    }

                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
                }
                else
                {
                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
                }

                bDlgProcResult = TRUE;
            }
            else if(lpNmHdr->code == LVN_COLUMNCLICK)
            {
                static int nRet = 1;
                LPNMLISTVIEW lpNmListView = (LPNMLISTVIEW)lpNmHdr;

                nRet = -nRet;

                ListCtrlSortStruct lcss;

                lcss.nRet = nRet;
                lcss.hListCtrl = hFileList;
                lcss.nSubItem = lpNmListView->iSubItem;

                LVITEM item = {LVIF_PARAM};

                for(int nIndex = 0; nIndex < ListView_GetItemCount(hFileList); nIndex += 1)
                {
                    item.iItem = nIndex;
                    item.lParam = nIndex;

                    ListView_SetItem(hFileList, &item);
                }

                ListView_SortItems(hFileList, ListCtrlCompareProc, &lcss);
            }
            else if(lpNmHdr->code == NM_DBLCLK)
            {
                LPNMITEMACTIVATE lpNmItemActivate = (LPNMITEMACTIVATE)lParam;
                TCHAR szFilePath[MAX_PATH] = TEXT("");

                ListView_GetItemText(hFileList, lpNmItemActivate->iItem, LVH_PATH, szFilePath, sizeof(szFilePath) / sizeof(TCHAR));

                if(PathFileExists(szFilePath))
                {
                    BrowseForFile(szFilePath);
                }
                else
                {
                    MessageBox(hwndDlg, TEXT("�ļ������ڣ��޷���λ��"), NULL, MB_ICONERROR);
                }
            }
        }
    }
    else if(uMsg == WM_SIZING)
    {
        LPRECT pRect = (LPRECT)lParam;
        const LONG lWidthLimit = 482;
        const LONG lHeightLimit = 306;

        if(pRect->right - pRect->left < lWidthLimit)
        {
            if(wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_LEFT)
            {
                pRect->left = pRect->right - lWidthLimit;
            }
            else
            {
                pRect->right = pRect->left + lWidthLimit;
            }
        }

        if(pRect->bottom - pRect->top < lHeightLimit)
        {
            if(wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOP)
            {
                pRect->top = pRect->bottom - lHeightLimit;
            }
            else
            {
                pRect->bottom = pRect->top + lHeightLimit;
            }
        }
    }
    else if(uMsg == WM_SIZE)
    {
        RECT rectClient;

        GetClientRect(hwndDlg, &rectClient);
        InflateRect(&rectClient, -9, -9);

        MoveWindow(
            hFileList,
            rectClient.left,
            rectClient.top,
            rectClient.right - rectClient.left,
            rectClient.bottom - rectClient.top,
            TRUE
            );
    }
    else if(uMsg == WM_SYSCOMMAND)
    {
        if(wParam == SC_CLOSE)
        {
            KillTimer(hwndDlg, 1);
            EndDialog(hwndDlg, 0);
        }
    }
    else if(uMsg == WM_SIGNRESULT)
    {
        TCHAR szIndex[100];
        TCHAR szIndexInList[100];
        TCHAR szResult[100];
        HWND hFileList = GetDlgItem(hwndDlg, IDC_FILELIST);
        int nCount = ListView_GetItemCount(GetDlgItem(hwndDlg, IDC_FILELIST));

        //�����б�״̬
        wnsprintf(szIndex, sizeof(szIndex) / sizeof(TCHAR), TEXT("%lu"), wParam + 1);

        for(int nIndex = 0; nIndex < nCount; nIndex += 1)
        {
            ListView_GetItemText(hFileList, nIndex, LVH_INDEX, szIndexInList, sizeof(szIndexInList) / sizeof(TCHAR));

            if(lstrcmpi(szIndexInList, szIndex) == 0)
            {
                if((BOOL)lParam)
                {
                    lstrcpyn(szResult, TEXT("��ǩ��"), sizeof(szResult) / sizeof(TCHAR));
                }
                else
                {
                    lstrcpyn(szResult, TEXT("δǩ��"), sizeof(szResult) / sizeof(TCHAR));
                }

                ListView_SetItemText(hFileList, nIndex, LVH_RESULT, szResult);

                break;
            }
        }

        //���±���
        TCHAR szWindowText[100];

        wnsprintf(
            szWindowText,
            sizeof(szWindowText) / sizeof(TCHAR),
            TEXT("У������ǩ�� %lu/%lu(%lu.%02lu%%)"),
            wParam + 1,
            nCount,
            (wParam + 1) * 100 / nCount,
            (wParam + 1) * 100 * 100 / nCount % 100
            );

        SetWindowText(hwndDlg, szWindowText);
    }

    return bDlgProcResult;
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

void WINAPI T(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    DrvAction(hwndStub, TEXT("D:\\Administrator\\Desktop\\Tools\\InstDrv\\MySYS.sys"), DA_INSTALL);
    return;

    map<tstring, HWND> m;

    m[TEXT("aaaaaaaaaa")] = 0;
    m[TEXT("aaaaaaaaa1")] = 0;
    m[TEXT("aaaaaaaaa2")] = 0;
    m[TEXT("aaaaaaaaa3")] = 0;
    m[TEXT("aaaaaa1aaa")] = 0;
    m[TEXT("aaaaaa1aa1")] = 0;
    m[TEXT("aaaaaa1aa2")] = 0;
    m[TEXT("aaaaaa1aa3")] = 0;

//     IStream *pStream = NULL;
// 
//     if (S_OK == CreateStreamOnHGlobal(NULL, TRUE, &pStream))
//     {
//         HGLOBAL hGlobal = NULL;
// 
//         if (S_OK == GetHGlobalFromStream(pStream, &hGlobal))
//         {
//             while (TRUE)
//             {
//                 pStream->Write("AAAAAAAAAAAAAAAAAAAAAA", 10, NULL);
// 
//                 LPVOID lpBuffer = GlobalLock(hGlobal);
// 
//                 OutputDebugString(NULL);
//             }
//         }
// 
//         pStream->Release();
//     }

    OPENFILENAME ofn = {sizeof(ofn)};

    ofn.hInstance = GetModuleHandle(NULL);

    ofn.lStructSize = sizeof(OPENFILENAME); 
    ofn.hwndOwner = NULL; 
    ofn.lpstrFile = NULL; 
    ofn.nMaxFile = 10000; 
    ofn.lpstrFilter = TEXT("All/0*.*/0Text/0*.TXT/0"); 
    ofn.nFilterIndex = 1; 
    ofn.lpstrFileTitle = NULL; 
    ofn.nMaxFileTitle = 0; 
    ofn.lpstrInitialDir = NULL; 
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST; 

    // Display the Open dialog box.  

    GetOpenFileName(&ofn);

    DialogBoxParam(
        g_hinstDll,
        MAKEINTRESOURCE(IDD_MANUALCHECKSIGNATURE_DIALOG),
        NULL,
        CSlxComContextMenu::ManualCheckSignatureDialogProc,
        (LPARAM)&m
        );
}