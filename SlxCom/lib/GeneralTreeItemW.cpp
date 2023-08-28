#include "GeneralTreeItemW.h"
#include <Windows.h>
#include <CommCtrl.h>

#if defined(_MSC_VER) && defined(__cplusplus) && _MSC_VER>1200

using namespace std;

namespace Internal
{
#include "Internal/rapidxml.hpp"

    using namespace rapidxml;

    BOOL AttachToTreeCtrlItemW(const xml_node<wchar_t> *pElement, HWND hTreeCtrl, HTREEITEM hTreeParentItem)
    {
        TVINSERTSTRUCT tvis;

        ZeroMemory(&tvis, sizeof(tvis));
        tvis.hParent = hTreeParentItem;
        tvis.hInsertAfter = TVI_LAST;

        HTREEITEM hTreeItem = (HTREEITEM)SendMessageW(hTreeCtrl, TVM_INSERTITEM, 0, (LPARAM)&tvis);

        if (hTreeItem == NULL)
        {
            return FALSE;
        }

        TVITEMEXW tvi = {TVIF_TEXT};
        xml_attribute<wchar_t> *pType = pElement->first_attribute(L"key");
        xml_attribute<wchar_t> *pValue = pElement->first_attribute(L"value");
        wstring strText;

        if (pType != NULL)
        {
            strText += pType->value();
        }

        if (pValue != NULL)
        {
            strText += L": ";
            strText += pValue->value();
        }

        tvi.hItem = hTreeItem;
        tvi.pszText = const_cast<wchar_t *>(strText.c_str());
        SendMessageW(hTreeCtrl, TVM_SETITEM, 0, (LPARAM)&tvi);

        const xml_node<wchar_t> *pChild = pElement->first_node();

        while (pChild != NULL)
        {
            if (!AttachToTreeCtrlItemW(pChild, hTreeCtrl, hTreeItem))
            {
                return FALSE;
            }

            pChild = pChild->next_sibling();
        }

//         PostMessageW(hTreeCtrl, TVM_EXPAND, TVE_EXPAND, (LPARAM)hTreeItem);

        return TRUE;
    }

    BOOL AttachToTreeCtrlW(const std::wstring &strXml, HWND hTreeCtrl)
    {
        xml_document<wchar_t> doc;
        vector<wchar_t> vectorBuffer(strXml.begin(), strXml.end());

        vectorBuffer.push_back(L'\0');

        try
        {
            doc.parse<0>(&*vectorBuffer.begin());
        }
        catch (parse_error &)
        {
            return FALSE;
        }

        xml_node<wchar_t> *pRoot = doc.first_node();

        if (pRoot == NULL)
        {
            return FALSE;
        }

        return AttachToTreeCtrlItemW(pRoot, hTreeCtrl, NULL);
    }
}

std::wstring GeneralTreeItemW::Unescape(const wstring &strText)
{
    // 暂不支持&#32;等类型的转义
    const wchar_t *lpOlds[] = {L"&lt;", L"&gt;", L"&apos;", L"&quot;", L"&amp;"};
    const wchar_t *lpNews[] = {L"<", L">", L"\'", L"\"", L"&"};
    wstring strNewText = strText;

    for (int i = 0; i < min(RTL_NUMBER_OF(lpOlds), RTL_NUMBER_OF(lpNews)); ++i)
    {
        while (true)
        {
            wstring::size_type nPos = strNewText.find(lpOlds[i]);

            if (nPos == wstring::npos)
            {
                break;
            }
            else
            {
                strNewText.replace(nPos, lstrlenW(lpOlds[i]), lpNews[i]);
            }
        }
    }

    return strNewText;
}

std::wstring GeneralTreeItemW::Escape(const wstring &strText)
{
    wstringstream ss;

    for (wstring::const_iterator it = strText.begin(); it != strText.end(); ++it)
    {
        wchar_t ch = *it;

        switch (ch)
        {
        case L'&':
            ss<<L"&amp;";
            break;

        case L'<':
            ss<<L"&lt;";
            break;

        case L'>':
            ss<<L"&gt;";
            break;

        case L'\'':
            ss<<L"&apos;";
            break;

        case L'\"':
            ss<<L"&quot;";
            break;

        default:
            ss<<ch;
            break;
        }
    }

    return ss.str();
}

std::wstring GeneralTreeItemW::GetAttribute(const wstring &strName) const
{
    if (strName == L"key")
    {
        return m_strKey;
    }
    else
    {
        map<wstring, wstring>::const_iterator it = m_mapAttributes.find(strName);

        if (it != m_mapAttributes.end())
        {
            return Unescape(it->second);
        }
        else
        {
            return L"";
        }
    }
}

bool GeneralTreeItemW::RemoveAttribute(const wstring &strName)
{
    return m_mapAttributes.erase(strName) > 0;
}

bool GeneralTreeItemW::HasAttribute(const wstring &strName) const
{
    return strName == L"key" || m_mapAttributes.count(strName) > 0;
}

bool GeneralTreeItemW::SetAttribute(const wstring &strName, const wstring &strValue)
{
    if (strName != L"depth" && strName != L"key")
    {
        m_mapAttributes[strName] = strValue;
        return true;
    }

    return false;
}

std::string GeneralTreeItemW::GenerateXmlUtf8(int nDepth /*= 1*/) const
{
    wstring strXml = GenerateXml(nDepth);
    string strXmlUtf8;

    int in_len = (int)strXml.length();
    int count = WideCharToMultiByte(CP_UTF8, 0, strXml.c_str(), in_len, NULL, 0, NULL, NULL);

    if (count > 0)
    {
        char *buffer = (char *)malloc(count * sizeof(char));

        if (buffer != 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, strXml.c_str(), in_len, buffer, count, NULL, NULL);
            strXmlUtf8.assign(buffer, count);

            free((void *)buffer);
        }
    }

    return strXmlUtf8;
}

std::wstring GeneralTreeItemW::GenerateXml(int nDepth /*= 1*/) const
{
    wstringstream ss;

    for (int i = 1; i < nDepth; ++i)
    {
        ss<<L"  ";
    }

    ss<<L"<item depth=\""<<nDepth<<L"\" key=\""<<Escape(m_strKey)<<L"\"";

    for (map<wstring, wstring>::const_iterator it = m_mapAttributes.begin(); it != m_mapAttributes.end(); ++it)
    {
        ss<<L" "<<it->first<<L"=\""<<Escape(it->second)<<L"\"";
    }

    if (m_vectorSubItems.empty())
    {
        ss<<L" />"<<endl;
    }
    else
    {
        ss<<L">"<<endl;

        for (vector<tr1::shared_ptr<GeneralTreeItemW> >::const_iterator it = m_vectorSubItems.begin(); it != m_vectorSubItems.end(); ++it)
        {
            ss<<(*it)->GenerateXml(nDepth + 1);
        }

        for (int i = 1; i < nDepth; ++i)
        {
            ss<<L"  ";
        }

        ss<<L"</item>"<<endl;
    }

    return ss.str();
}

tr1::shared_ptr<GeneralTreeItemW> GeneralTreeItemW::AppendItem(tr1::shared_ptr<GeneralTreeItemW> pItem)
{
    m_vectorSubItems.push_back(pItem);
    return pItem;
}

tr1::shared_ptr<GeneralTreeItemW> GeneralTreeItemW::AppendItem(const wstring &strKey)
{
    return AppendItem(tr1::shared_ptr<GeneralTreeItemW>(new GeneralTreeItemW(strKey)));
}

std::wstring GeneralTreeItemW::GetValue() const
{
    return GetAttribute(L"value");
}

void GeneralTreeItemW::SetValue(const wstring &strValue)
{
    SetAttribute(L"value", strValue);
}

GeneralTreeItemW::GeneralTreeItemW(const wstring &strKey)
    : m_strKey(strKey)
{
}

GeneralTreeItemW::GeneralTreeItemW(const wstring &strKey, const wstring &strValue)
    : m_strKey(strKey)
{
    SetValue(strValue);
}

BOOL GeneralTreeItemW::AttachToTreeCtrl(HWND hTreeCtrl) const
{
    return AttachToTreeCtrlW(GenerateXml(), hTreeCtrl);
}

BOOL AttachToTreeCtrlW(const std::wstring &strXml, HWND hTreeCtrl)
{
    return Internal::AttachToTreeCtrlW(strXml, hTreeCtrl);
}

BOOL AttachToTreeCtrlUtf8(const std::string &strXml, HWND hTreeCtrl)
{
    BOOL bSucceed = FALSE;

    int in_len = (int)strXml.length();
    int count = MultiByteToWideChar(CP_UTF8, 0, strXml.c_str(), in_len, NULL, 0);

    if (count > 0)
    {
        wchar_t *buffer = (wchar_t *)malloc(count * sizeof(wchar_t));

        if (buffer != 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, strXml.c_str(), in_len, buffer, count);
            bSucceed = AttachToTreeCtrlW(wstring(buffer, count), hTreeCtrl);

            free((void *)buffer);
        }
    }

    return bSucceed;
}

#endif