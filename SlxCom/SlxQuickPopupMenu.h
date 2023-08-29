#ifndef _CSLXQUICKPOPUPMENU_H
#define _CSLXQUICKPOPUPMENU_H

#include <Windows.h>

class CSlxQuickPopupMenu
{
public:
    CSlxQuickPopupMenu();
    ~CSlxQuickPopupMenu();

public:
    int Popup(HWND hWnd, int x, int y) const;

public:
    CSlxQuickPopupMenu& AddItemIf(LPCWSTR lpText, int nID, BOOL bChecked, BOOL bDisabled, BOOL bGray, BOOL bExecute);
    CSlxQuickPopupMenu& AddItem(LPCWSTR lpText, int nID);
    CSlxQuickPopupMenu& AddSubMenu(LPCWSTR lpText);
    CSlxQuickPopupMenu& ExitSubMenu();
    CSlxQuickPopupMenu& AddSeparator();

private:
    HMENU m_hMenu;
    void* m_stackCurrent;
};

#endif // _CSLXQUICKPOPUPMENU_H
