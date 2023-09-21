#ifndef _CQuickPopupMenu_H
#define _CQuickPopupMenu_H

#include <Windows.h>

class CQuickPopupMenu {
public:
    CQuickPopupMenu();
    ~CQuickPopupMenu();

public:
    int Popup(HWND hWnd, int x, int y) const;

public:
    CQuickPopupMenu& AddItemIf(LPCWSTR lpText, int nID, BOOL bChecked, BOOL bDisabled, BOOL bGray, BOOL bExecute);
    CQuickPopupMenu& AddItem(LPCWSTR lpText, int nID);
    CQuickPopupMenu& AddSubMenu(LPCWSTR lpText);
    CQuickPopupMenu& ExitSubMenu();
    CQuickPopupMenu& AddSeparator();

private:
    HMENU m_hMenu;
    void* m_stackCurrent;
};

#endif // _CQuickPopupMenu_H
