#include "SlxComWork.h"
#include "SlxComWork_lvm.h"
#include "SlxComWork_sd.h"

BOOL SlxWork(HINSTANCE hinstDll)
{
    OSVERSIONINFO osi = {sizeof(osi)};

    GetVersionEx(&osi);

    LvmInit(hinstDll, osi.dwMajorVersion <= 5);

    if(osi.dwMajorVersion == 5 && osi.dwMinorVersion > 0)
    {
        StartHookShowDesktop();
    }

    return TRUE;
}