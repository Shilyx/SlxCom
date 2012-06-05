#ifndef _SLX_COM_OVERLAY_H
#define _SLX_COM_OVERLAY_H

#include <unknwn.h>
#include <shlobj.h>
#include "SlxStringStatusCache.h"

class CSlxComOverlay : public IShellIconOverlayIdentifier
{
public:
    CSlxComOverlay();

    //IUnknow Method
    STDMETHOD(QueryInterface)(REFIID, void**);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    //IShellIconOverlayIdentifier
    STDMETHOD(IsMemberOf)(LPCWSTR pwszPath, DWORD dwAttrib);
    STDMETHOD(GetOverlayInfo)(LPWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags);
    STDMETHOD(GetPriority)(int *pPriority);

public:
    static BOOL GenericIconFile();
    static TCHAR m_szIconFilePath[MAX_PATH];
    static DWORD m_dwIcoFileSize;

protected:
    volatile DWORD m_dwRefCount;
    static SlxStringStatusCache m_cache;
};

#endif
