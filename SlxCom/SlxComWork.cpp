#include "SlxComWork.h"
#include "SlxComWork_lvm.h"
#include "SlxComWork_sd.h"

BOOL SlxWork(HINSTANCE hinstDll)
{
    OSVERSIONINFO osi = {sizeof(osi)};

    GetVersionEx(&osi);

    if(osi.dwMajorVersion <= 5)
    {
        LvmInit(hinstDll);
    }

    if(osi.dwMajorVersion == 5 && osi.dwMinorVersion > 0)
    {
        StartHookShowDesktop();
    }

    return TRUE;
}