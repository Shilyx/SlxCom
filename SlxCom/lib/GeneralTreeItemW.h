#ifndef _GENERALTREEITEMW_H
#define _GENERALTREEITEMW_H

#if defined(_MSC_VER) && defined(__cplusplus) && _MSC_VER>1200

#include <Windows.h>
#include <string>
#include <sstream>
#include <map>
#include <memory>
#include <vector>

namespace std {
#if defined(_MSC_VER) && _MSC_VER == 1500
    using namespace tr1;
#endif
}

class GeneralTreeItemW
{
public:
    GeneralTreeItemW(const std::wstring &strKey, const std::wstring &strValue);
    GeneralTreeItemW(const std::wstring &strKey);

public:
    void SetValue(const std::wstring &strValue);
    std::wstring GetValue() const;

    std::shared_ptr<GeneralTreeItemW> AppendItem(const std::wstring &strKey);
    std::shared_ptr<GeneralTreeItemW> AppendItem(std::shared_ptr<GeneralTreeItemW> pItem);

    std::wstring GenerateXml(int nDepth = 1) const;
    std::string GenerateXmlUtf8(int nDepth = 1) const;

public:
    bool SetAttribute(const std::wstring &strName, const std::wstring &strValue);
    std::wstring GetAttribute(const std::wstring &strName) const;
    bool HasAttribute(const std::wstring &strName) const;
    bool RemoveAttribute(const std::wstring &strName);

public:
    static std::wstring Escape(const std::wstring &strText);
    static std::wstring Unescape(const std::wstring &strText);

public:
    BOOL AttachToTreeCtrl(HWND hTreeCtrl) const;

private:
    std::wstring m_strKey;
    std::map<std::wstring, std::wstring> m_mapAttributes;
    std::vector<std::shared_ptr<GeneralTreeItemW> > m_vectorSubItems;
};

BOOL AttachToTreeCtrlW(const std::wstring &strXml, HWND hTreeCtrl);
BOOL AttachToTreeCtrlUtf8(const std::string &strXml, HWND hTreeCtrl);

#endif

#endif /* _GENERALTREEITEMW_H */
