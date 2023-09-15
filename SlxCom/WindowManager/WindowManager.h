#ifndef _WINDOWMANAGER_H
#define _WINDOWMANAGER_H

#include <Windows.h>

// 由子进程邮寄，wParam为目标窗口，lParam为MAKELONG的屏幕坐标
#define WM_SCREEN_CONTEXT_MENU (WM_USER + 165)

void StartWindowManager(HINSTANCE hinstDll);

#endif /* _WINDOWMANAGER_H */