#ifndef _SLXCOMDISABLECTRLCOPYINSAMEDIR_H
#define _SLXCOMDISABLECTRLCOPYINSAMEDIR_H

typedef enum {
    DCCO_ON,
    DCCO_OFF,
} DccOperation;

// 获取标记状态
bool DisableCtrlCopyInSameDir_IsRunning();

// 置标记，如果是首次开启则开启工作线程
void DisableCtrlCopyInSameDir_Op(DccOperation op);

// 停止线程
void DisableCtrlCopyInSameDir_StopAll();

#endif
