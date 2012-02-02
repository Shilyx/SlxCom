#include "SlxComContextMenu.h"

CSlxComContextMenu::CSlxComContextMenu()
{
    m_dwRefCount = 0;

    m_pFiles = new FileInfo[1];
    m_uFileCount = 1;
    lstrcpy(m_pFiles[0].szPath, TEXT("1:\\"));
}

STDMETHODIMP CSlxComContextMenu::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IShellExtInit)
    {
        *ppv = static_cast<IShellExtInit *>(this);
    }
    else if(riid == IID_IContextMenu)
    {
        *ppv = static_cast<IContextMenu *>(this);
    }
    else if(riid == IID_IUnknown)
    {
        *ppv = static_cast<IShellExtInit *>(this);
    }
    else
    {
        *ppv = 0;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown *>(*ppv)->AddRef();

    return S_OK;
}

STDMETHODIMP_(ULONG) CSlxComContextMenu::AddRef()
{
    return InterlockedIncrement((volatile LONG *)&m_dwRefCount);
}

STDMETHODIMP_(ULONG) CSlxComContextMenu::Release()
{
    DWORD dwRefCount = InterlockedDecrement((volatile LONG *)&m_dwRefCount);

    if(dwRefCount == 0)
    {
        delete this;
    }

    return dwRefCount;
}

//IShellExtInit
STDMETHODIMP CSlxComContextMenu::Initialize(LPCITEMIDLIST pidlFolder, IDataObject *pdtobj, HKEY hkeyProgID)
{
    if(pdtobj == NULL)
    {
        if(pidlFolder == NULL)
        {
            return E_INVALIDARG;
        }
        else
        {
            if(SHGetPathFromIDList(pidlFolder, m_pFiles[0].szPath))
            {
                m_uFileCount = 1;

                return S_OK;
            }
            else
            {
                return E_INVALIDARG;
            }
        }
    }
    else
    {
        FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stg = {TYMED_HGLOBAL};
        HDROP hDrop;
        HRESULT hResult = E_INVALIDARG;

        if(SUCCEEDED(pdtobj->GetData(&fmt, &stg)))
        {
            hDrop = (HDROP)GlobalLock(stg.hGlobal);

            if(hDrop != NULL)
            {
                UINT uIndex;
                UINT uNumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

                if(uNumFiles > 0)
                {
                    delete []m_pFiles;

                    m_pFiles = new FileInfo[uNumFiles];
                    m_uFileCount = uNumFiles;

                    for(uIndex = 0; uIndex < m_uFileCount; uIndex += 1)
                    {
                        DragQueryFile(hDrop, uIndex, m_pFiles[uIndex].szPath, MAX_PATH);
                    }

                    hResult = S_OK;
                }
            }

            ReleaseStgMedium(&stg);
        }

        return hResult;
    }
}

//IContextMenu
STDMETHODIMP CSlxComContextMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
}

STDMETHODIMP CSlxComContextMenu::GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
    return S_OK;
}

STDMETHODIMP CSlxComContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    return S_OK;
}