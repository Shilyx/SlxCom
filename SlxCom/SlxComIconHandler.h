#ifndef _SLXCOMICONHANDLER_H
#define _SLXCOMICONHANDLER_H

#include <Windows.h>
#include <ShlObj.h>
#include <ShlGuid.h>

class CSlxComIconHandler : public IExtractIconW, public IPersistFile
{
public:
    CSlxComIconHandler(void)
    {
        *m_szFilePath = 0;
        m_nRefCount = 0;
    }

    virtual ~CSlxComIconHandler(void)
    {

    }

private:
    WCHAR m_szFilePath[MAX_PATH];
    volatile LONG m_nRefCount;

public:
    // IUnknow Method
    STDMETHOD(QueryInterface)(REFIID, void**);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IExtractIcon methods
    STDMETHOD(GetIconLocation)(THIS_
                               UINT   uFlags,
          __out_ecount(cchMax) LPWSTR pszIconFile,
                               UINT   cchMax,
                         __out int   * piIndex,
                         __out UINT  * pwFlags);

    STDMETHOD(Extract)(THIS_
                       LPCWSTR pszFile,
                       UINT    nIconIndex,
             __out_opt HICON   *phiconLarge,
             __out_opt HICON   *phiconSmall,
                       UINT    nIconSize);

    // IPersistFile Method
    virtual HRESULT STDMETHODCALLTYPE GetClassID( 
        /* [out] */ __RPC__out CLSID *pClassID)
    {
        return E_NOTIMPL;
    }

    // IPersistFile Method
    virtual HRESULT STDMETHODCALLTYPE IsDirty( void)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE Load( 
        /* [in] */ __RPC__in LPCOLESTR pszFileName,
        /* [in] */ DWORD dwMode);

    virtual HRESULT STDMETHODCALLTYPE Save( 
        /* [unique][in] */ __RPC__in_opt LPCOLESTR pszFileName,
        /* [in] */ BOOL fRemember)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE SaveCompleted( 
        /* [unique][in] */ __RPC__in_opt LPCOLESTR pszFileName)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetCurFile( 
        /* [out] */ __RPC__deref_out_opt LPOLESTR *ppszFileName)
    {
        return E_NOTIMPL;
    }
};

#endif /* _SLXCOMICONHANDLER_H */
