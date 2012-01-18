#ifndef _SLX_COM_OVERLAY_H
#define _SLX_COM_OVERLAY_H

#include <unknwn.h>
#include <shlobj.h>

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

protected:
    volatile DWORD m_dwRefCount;
};

#endif
