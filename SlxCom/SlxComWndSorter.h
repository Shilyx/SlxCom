#ifndef _SLXCOMWNDSORTER_H
#define _SLXCOMWNDSORTER_H

typedef enum {
    WNDSORTER_ON,
    WNDSORTER_OFF,
} WndSorterOperation;

// 获取标记状态
bool WndSorter_IsRunning();

// 置标记，如果是首次开启则开启工作线程
void WndSorter_Op(WndSorterOperation op);

// 停止线程
void WndSorter_StopAll();

#endif
