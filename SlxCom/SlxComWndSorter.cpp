#include "SlxCom.h"
#include "SlxComWndSorter.h"

#include <Windows.h>
#include <string>
#include <set>
#include <memory>
#include <unordered_map>

using namespace std;

static BOOL gs_bRunning = FALSE;
static volatile HANDLE gs_hWorkThread = NULL;

struct WndInfo {
    HWND hWnd;
    HWND hAddress;
    DWORD dwTime;
    wstring strAddress;

    DWORD GetAliveSeconds() const {
        return (GetTickCount() - dwTime) / 1000;
    }
};
typedef std::unordered_map<HWND, std::shared_ptr<WndInfo> > Map;
typedef std::list<std::shared_ptr<WndInfo> > List;

static BOOL CALLBACK EnumAllSubWndsProc(HWND hWnd, LPARAM lParam) {
    WCHAR szClassName[1024] = L"";

    GetClassNameW(hWnd, szClassName, RTL_NUMBER_OF(szClassName));

    if (lstrcmpiW(szClassName, L"Breadcrumb Parent") == 0) {
        *(HWND*)lParam = hWnd;
        return FALSE;
    }

    return TRUE;
}

static BOOL CALLBACK EnumAllWndsProc(HWND hWnd, LPARAM lParam) {
    HWND hWorkW = FindWindowExW(hWnd, NULL, L"WorkerW", NULL);

    if (hWorkW) {
        HWND hBreadcrumb_Parent = NULL;

        EnumChildWindows(hWorkW, EnumAllSubWndsProc, (LPARAM)&hBreadcrumb_Parent);

        if (IsWindow(hBreadcrumb_Parent)) {
            HWND hAddress = FindWindowEx(hBreadcrumb_Parent, NULL, L"ToolbarWindow32", NULL);

            if (IsWindow(hAddress)) {
                shared_ptr<WndInfo> pWndInfo(new WndInfo());
                WCHAR szText[1024] = L"";;

                pWndInfo->hWnd = hWnd;
                pWndInfo->hAddress = hAddress;
                GetWindowTextW(pWndInfo->hAddress, szText, RTL_NUMBER_OF(szText));
                pWndInfo->strAddress = szText;
                pWndInfo->dwTime = GetTickCount();

                ((List*)lParam)->push_back(pWndInfo);
            }
        }
    }

    return TRUE;
}

static List _GetAllWnds() {
    List listWnds;
    EnumWindows(EnumAllWndsProc, (LPARAM)&listWnds);
    return listWnds;
}

static DWORD CALLBACK _WorkProc(LPVOID lpParam) {
    unordered_map<HWND, DWORD> mapWndTimes;

    while (true) {
        Sleep(1000);
        List listWnds = _GetAllWnds();

        // 更新窗口持续时间
        for (List::iterator it = listWnds.begin(); it != listWnds.end(); ++it) {
            shared_ptr<WndInfo> pWndInfo = *it;
            DWORD dwTime = mapWndTimes[pWndInfo->hWnd];

            if (dwTime > 0) {
                pWndInfo->dwTime = dwTime;
            } else {
                mapWndTimes[pWndInfo->hWnd] = pWndInfo->dwTime;
            }
        }

        // 
        {
            unordered_map<HWND, DWORD>::iterator it = mapWndTimes.begin();

            while (it != mapWndTimes.end()) {
                if (IsWindow(it->first)) {
                    ++it;
                } else {
                    it = mapWndTimes.erase(it);
                }
            }
        }

        // 关闭重复的超时窗口
        set<wstring> setWndAddresses;

        for (List::const_iterator it = listWnds.begin(); it != listWnds.end(); ++it) {
            shared_ptr<WndInfo> pWndInfo = *it;

            if (pWndInfo->GetAliveSeconds() < 30) {
                continue;
            }

            if (setWndAddresses.count(pWndInfo->strAddress) > 0) {
                if (pWndInfo->GetAliveSeconds() > 10) {
                    PostMessage(pWndInfo->hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                }
            } else {
                setWndAddresses.insert(pWndInfo->strAddress);
            }
        }
    }

    return 0;
}

bool WndSorter_IsRunning() {
    return !!gs_bRunning;
}

void WndSorter_Op(WndSorterOperation op) {
    if (op == WNDSORTER_ON) {
        gs_bRunning = TRUE;
        if (InterlockedCompareExchangePointer(&gs_hWorkThread, (PVOID)1, (PVOID)0) == 0) {
            gs_hWorkThread = CreateThread(NULL, 0, _WorkProc, NULL, 0, NULL);
        }
    } else if (op == WNDSORTER_OFF) {
        gs_bRunning = FALSE;
    } else {
        // we should never be here
    }
}
