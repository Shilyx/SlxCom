#define INITGUID

#include "WindowAppID.h"
#include <propkey.h>
#include <propsys.h>
#include <propvarutil.h>
#include "../SlxComTools.h"

using namespace std;

DEFINE_PROPERTYKEY(PKEY_AppUserModel_ID_redefine, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 5);

BOOL HasWinAppID(HWND hWnd)
{
    BOOL bRet = FALSE;

    IPropertyStore* ps = NULL;
    if (SUCCEEDED(SHGetPropertyStoreForWindowHelper(hWnd, IID_PPV_ARGS(&ps))))
    {
        PROPVARIANT propvar;

        if (SUCCEEDED(ps->GetValue(PKEY_AppUserModel_ID_redefine, &propvar)))
        {
            if (VT_EMPTY != propvar.vt)
            {
                bRet = TRUE;
            }

            PropVariantClear(&propvar);
        }

        ps->Release();
    }

    return bRet;
}

BOOL SetWinAppID(HWND hWnd)
{
    BOOL bRet = FALSE;
    wstring strAppID = L"SlxCom_" + AnyTypeToString<HWND>(hWnd);

    IPropertyStore* ps = NULL;
    if (SUCCEEDED(SHGetPropertyStoreForWindowHelper(hWnd, IID_PPV_ARGS(&ps))))
    {
        PROPVARIANT propvar;

        if (SUCCEEDED(InitPropVariantFromString(strAppID.c_str(), &propvar)))
        {
            bRet = SUCCEEDED(ps->SetValue(PKEY_AppUserModel_ID_redefine, propvar));
            PropVariantClear(&propvar);
        }

        ps->Release();
    }

    return bRet;
}

BOOL UnsetWinAppID(HWND hWnd)
{
    BOOL bRet = FALSE;

    IPropertyStore* ps = NULL;
    if (SUCCEEDED(SHGetPropertyStoreForWindowHelper(hWnd, IID_PPV_ARGS(&ps))))
    {
        PROPVARIANT propvar;
        propvar.vt = VT_EMPTY;
        bRet = SUCCEEDED(ps->SetValue(PKEY_AppUserModel_ID_redefine, propvar));
        ps->Release();
    }

    return bRet;
}
