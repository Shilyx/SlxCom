#include "SlxComWork.h"
#include "SlxComWork_lvm.h"
#include "SlxComWork_sd.h"
#include "SlxComWork_ni.h"
#include "SlxComTools.h"
#include "SlxRunDlgPlus.h"
#include "WindowManager/WindowManager.h"
#include "DesktopIconManager.h"

BOOL SlxWork(HINSTANCE hinstDll) {
    OSVERSIONINFO osi = {sizeof(osi)};

    GetVersionEx(&osi);

    LvmInit(hinstDll, osi.dwMajorVersion <= 5);
    StartNotifyIconManager(hinstDll);
    StartWindowManager(hinstDll);

    if (IsExplorer()) {
        StartDesktopIconManager();
    }

    if (osi.dwMajorVersion == 5 && osi.dwMinorVersion > 0) {
        StartHookShowDesktop();
    }

    if (osi.dwMajorVersion >= 6 && !IsAdminMode()) {
        StartRunDlgPlusMonitor();
    }

    return TRUE;
}