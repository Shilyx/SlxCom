#include "SlxComContextMenu.h"
#include <shlwapi.h>

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

#define ID_REGISTER     1
#define ID_UNREGISTER   2
#define ID_3            3
#define ID_4            4
#define ID_5            5
#define ID_6            6
#define ID_7            7
#define ID_8            8

//IContextMenu
STDMETHODIMP CSlxComContextMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    if(m_uFileCount == 0)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }

    UINT uFileIndex;
    BOOL bExistDll = FALSE;

    for(uFileIndex = 0; uFileIndex < m_uFileCount; uFileIndex += 1)
    {
        if(m_pFiles[uFileIndex].bIsDll)
        {
            bExistDll = TRUE;
            break;
        }
    }

    UINT uMenuIndex = 0;

    if(bExistDll)
    {
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, TEXT(""));

        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_REGISTER, TEXT("注册"));
        InsertMenu(hmenu, indexMenu + uMenuIndex++, MF_BYPOSITION | MF_STRING, idCmdFirst + ID_UNREGISTER, TEXT("反注册"));
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, uMenuIndex + 1);
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
                            lstrcat(szText, TEXT("[成功]"));
                        }
                        else
                        {
                            lstrcat(szText, TEXT("[失败]"));
                        }

                        lstrcat(szText, m_pFiles[uIndex].szPath);
                        lstrcat(szText, TEXT("\r\n"));
                    }

                    MessageBox(pici->hwnd, szText, TEXT("RegSvr32 执行结果"), MB_ICONINFORMATION | MB_TOPMOST);

                    delete []szText;
                }

                delete []pProcessHandle;
            }
        }

        break;
    }

    default:
        return E_INVALIDARG;
    }

    return S_OK;
}