#include "SlxComWork.h"
#include "SlxComWork_lvm.h"
#include "SlxComWork_sd.h"
#include "SlxComWork_ni.h"

BOOL SlxWork(HINSTANCE hinstDll)
{
    OSVERSIONINFO osi = {sizeof(osi)};

    GetVersionEx(&osi);

    LvmInit(hinstDll, osi.dwMajorVersion <= 5);
    StartNotifyIconManager(hinstDll);

    if(osi.dwMajorVersion == 5 && osi.dwMinorVersion > 0)
    {
        StartHookShowDesktop();
    }

    return TRUE;
}