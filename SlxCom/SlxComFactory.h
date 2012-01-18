#ifndef _SLX_COM_FACTORY_H
#define _SLX_COM_FACTORY_H

#include <unknwn.h>
#include <shlobj.h>

class CSlxComFactory : public IClassFactory
{
public:
    CSlxComFactory();

    //IUnknow Method
    STDMETHOD(QueryInterface)(REFIID, void**);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    //IClassFactory Method
    STDMETHOD(CreateInstance)(IUnknown *, REFIID, void **);
    STDMETHOD(LockServer)(BOOL);

protected:
    volatile DWORD m_dwRefCount;
};

#endif
