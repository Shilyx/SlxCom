#ifndef _WINDOWMANAGER_H
#define _WINDOWMANAGER_H

#include <Windows.h>

// ���ӽ����ʼģ�wParamΪĿ�괰�ڣ�lParamΪMAKELONG����Ļ����
#define WM_SCREEN_CONTEXT_MENU (WM_USER + 165)

void StartWindowManager(HINSTANCE hinstDll);

#endif /* _WINDOWMANAGER_H */