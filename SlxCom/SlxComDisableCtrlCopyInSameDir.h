#ifndef _SLXCOMDISABLECTRLCOPYINSAMEDIR_H
#define _SLXCOMDISABLECTRLCOPYINSAMEDIR_H

typedef enum {
    DCCO_ON,
    DCCO_OFF,
} DccOperation;

// ��ȡ���״̬
bool DisableCtrlCopyInSameDir_IsRunning();

// �ñ�ǣ�������״ο������������߳�
void DisableCtrlCopyInSameDir_Op(DccOperation op);

// ֹͣ�߳�
void DisableCtrlCopyInSameDir_StopAll();

#endif
