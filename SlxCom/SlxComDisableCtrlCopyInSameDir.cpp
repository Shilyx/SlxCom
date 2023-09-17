#include "SlxComDisableCtrlCopyInSameDir.h"

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define _WIN32_WINNT 0x600

#include <Windows.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <ShlGuid.h>
#include <string>
#include <vector>
#include <set>
#include "SlxComTools.h"

using namespace std;

static BOOL gs_bRunning = FALSE;
static HANDLE gs_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
static volatile HANDLE gs_hWorkThread = NULL;
static volatile LPVOID gs_oldProcCopyItems = NULL;
static volatile LPVOID* gs_pVTable = NULL;

static set<wstring> GetClipboardFiles(HWND hWnd)
{
    set<wstring> setFiles;
    WCHAR szPath[MAX_PATH];

    if (OpenClipboard(hWnd))
    {
        if (IsClipboardFormatAvailable(CF_HDROP))
        {
            HANDLE hData = GetClipboardData(CF_HDROP);

            if (hData != NULL)
            {
                HDROP hDrop = (HDROP)GlobalLock(hData);

                if (hDrop)
                {
                    for (DWORD i = 0; i < 1000; i++)
                    {
                        if (DragQueryFileW(hDrop, i, szPath, RTL_NUMBER_OF(szPath))) 
                        {
                            setFiles.insert(szPath);
                        }
                        else
                        {
                            break;
                        }
                    }

                    GlobalUnlock(hDrop);
                }
            }
        }

        CloseClipboard();
    }

    return setFiles;
}

vector<wstring> GetFilesFromDataObject(IUnknown* pUnknown)
{
    vector<wstring> vectorPaths;
    IPersistIDList* pPersistIDList = NULL;
    IDataObject* pDataObject = NULL;
    IShellItemArray* pShellItemArray = NULL;
    IEnumShellItems* pEnumShellItems = NULL;

    if (SUCCEEDED(pUnknown->QueryInterface(IID_IPersistIDList, (void**)&pPersistIDList)))
    {
        PIDLIST_ABSOLUTE pIdl = NULL;

        if (SUCCEEDED(pPersistIDList->GetIDList(&pIdl)))
        {
            PWSTR pText = NULL;

            if (SUCCEEDED(SHGetNameFromIDListHelper(pIdl, SIGDN_FILESYSPATH, &pText))) 
            {
                vectorPaths.push_back(pText);
                CoTaskMemFree(pText);
            }

            CoTaskMemFree(pIdl);
        }

        pPersistIDList->Release();
    }
    else if (SUCCEEDED(pUnknown->QueryInterface(IID_IDataObject, (void**)&pDataObject)))
    {
        WCHAR szPath[MAX_PATH];
        FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg = { TYMED_HGLOBAL };

        if (SUCCEEDED(pDataObject->GetData(&fmt, &stg)))
        {
            HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);

            if (hDrop)
            {
                for (DWORD i = 0; i < 100; i++)
                {
                    if (DragQueryFileW(hDrop, i, szPath, RTL_NUMBER_OF(szPath)))
                    {
                        vectorPaths.push_back(szPath);
                    }
                    else
                    {
                        break;
                    }
                }

                GlobalUnlock(stg.hGlobal);
            }

            ReleaseStgMedium(&stg);
        }

        pDataObject->Release();
    }
    else if (SUCCEEDED(pUnknown->QueryInterface(IID_IShellItemArray, (void**)&pShellItemArray)))
    {
        pShellItemArray->Release();
    }
    else if (SUCCEEDED(pUnknown->QueryInterface(IID_IEnumShellItems, (void**)&pEnumShellItems)))
    {
        pEnumShellItems->Release();
    }

    return vectorPaths;
}

wstring GetFolderName(IShellItem* psiFolder)
{
    wstring strName;
    LPWSTR lpDisplayName = NULL;

    if (SUCCEEDED(psiFolder->GetDisplayName(SIGDN_FILESYSPATH, &lpDisplayName)) && lpDisplayName != NULL)
    {
        strName = lpDisplayName;
        CoTaskMemFree(lpDisplayName);
    }

    return strName;
}

static HRESULT WINAPI newCopyItems(IFileOperation *pThis, IUnknown *punkItems, IShellItem *psiDestinationFolder)
{
    vector<wstring> vectorSrcs = GetFilesFromDataObject(punkItems);
    wstring strDest = GetFolderName(psiDestinationFolder);

    if (gs_bRunning &&
        !vectorSrcs.empty() &&
        vectorSrcs.begin()->find((strDest + L"\\").c_str()) == 0 && 
        vectorSrcs.begin()->substr(strDest.length() + 1).find(L'\\') == wstring::npos)
    {
        set<wstring> setClipboardFiles = GetClipboardFiles(NULL);
        BOOL bAllSrcInClipboard = TRUE;

        for (vector<wstring>::const_iterator it = vectorSrcs.begin(); it != vectorSrcs.end(); ++it)
        {
            if (setClipboardFiles.count(*it) == 0)
            {
                bAllSrcInClipboard = FALSE;
                break;
            }
        }

        if (!bAllSrcInClipboard)
        {
            return E_FAIL;
        }
    }

    return ((HRESULT(WINAPI*)(IFileOperation*, IUnknown*, IShellItem*))gs_oldProcCopyItems)(
        pThis,
        punkItems,
        psiDestinationFolder);
}

BOOL DisableCtrlCopyInSameDir_GetCurrentAddress(void**& pVTable, IFileOperation*& pFop)
{
    BOOL bRet = FALSE;
    CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_SERVER, IID_IFileOperation, (LPVOID*)&pFop);

    if (pFop == NULL)
    {
        return bRet;
    }

    __try
    {
        pVTable = ((void***)pFop)[0];
        bRet = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {

    }

    if (!bRet)
    {
        pFop->Release();
    }

    return bRet;
}

static void DisableCtrlCopyInSameDir_Restore()
{
    if (gs_pVTable == NULL || gs_oldProcCopyItems == NULL)
    {
        return;
    }

    DWORD dwOldProtect = 0;
    VirtualProtect((LPVOID)(gs_pVTable + 17), sizeof(void*), PAGE_READWRITE, &dwOldProtect);
    __try
    {
        if (gs_pVTable[17] == (void*)newCopyItems) {
            gs_pVTable[17] = gs_oldProcCopyItems;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {

    }
    VirtualProtect((LPVOID)(gs_pVTable + 17), sizeof(void*), dwOldProtect, &dwOldProtect);

    gs_pVTable = NULL;
    gs_oldProcCopyItems = NULL;
}

static DWORD CALLBACK WorkProc(LPVOID lpParam)
{
    CoInitialize(NULL);

    while (WaitForSingleObject(gs_hStopEvent, 3000) == WAIT_TIMEOUT)
    {
        // 是否启用
        if (!gs_bRunning)
        {
            continue;
        }

        // 获取关键地址
        void** pVTable = NULL;
        IFileOperation* pFop = NULL;
        if (!DisableCtrlCopyInSameDir_GetCurrentAddress(pVTable, pFop))
        {
            continue;
        }

        if (gs_pVTable != pVTable)
        {
            DisableCtrlCopyInSameDir_Restore();
            gs_pVTable = pVTable;
        }

        DWORD dwOldProtect = 0;

        VirtualProtect((LPVOID)(gs_pVTable + 17), sizeof(void*), PAGE_READWRITE, &dwOldProtect);
        __try
        {
            if (gs_pVTable[17] != (void*)newCopyItems) {
                gs_oldProcCopyItems = gs_pVTable[17];
                gs_pVTable[17] = (void*)newCopyItems;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER)
        {

        }
        VirtualProtect((LPVOID)(gs_pVTable + 17), sizeof(void*), dwOldProtect, &dwOldProtect);

        // 释放资源
        pFop->Release();
    }

    return 0;
}

bool DisableCtrlCopyInSameDir_IsRunning()
{
    return !!gs_bRunning;
}

void DisableCtrlCopyInSameDir_Op(DccOperation op)
{
    if (op == DCCO_ON)
    {
        gs_bRunning = TRUE;
        if (InterlockedCompareExchangePointer(&gs_hWorkThread, (PVOID)1, (PVOID)0) == 0)
        {
            gs_hWorkThread = CreateThread(NULL, 0, WorkProc, NULL, 0, NULL);
        }
    }
    else if (op == DCCO_OFF)
    {
        gs_bRunning = FALSE;
    }
    else
    {
        // we should never be here
    }
}

void DisableCtrlCopyInSameDir_StopAll()
{
    if (gs_hWorkThread != NULL)
    {
        TerminateThread(gs_hWorkThread, 0);
        CloseHandle(gs_hWorkThread);
        gs_hWorkThread = NULL;
    }

    DisableCtrlCopyInSameDir_Restore();
}
