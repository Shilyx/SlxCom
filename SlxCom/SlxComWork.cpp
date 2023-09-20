#include "SlxComWork.h"
#include "SlxComWork_lvm.h"
#include "SlxComWork_sd.h"
#include "SlxComWork_ni.h"
#include "SlxComTools.h"
#include "SlxRunDlgPlus.h"
#include "WindowManager/WindowManager.h"
#include "DesktopIconManager.h"

extern OSVERSIONINFO g_osi; // SlxCom.cpp

BOOL SlxWork(HINSTANCE hinstDll) {
    LvmInit(hinstDll, g_osi.dwMajorVersion <= 5);
    StartNotifyIconManager(hinstDll);
    StartWindowManager(hinstDll);

    if (IsExplorer()) {
        StartDesktopIconManager();
    }

    if (g_osi.dwMajorVersion == 5 && g_osi.dwMinorVersion > 0) {
        StartHookShowDesktop();
    }

    if (g_osi.dwMajorVersion >= 6 && !IsAdminMode()) {
        StartRunDlgPlusMonitor();
    }

    return TRUE;
}