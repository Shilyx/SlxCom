#ifndef _SLXCOMDISABLECTRLCOPYINSAMEDIR_H
#define _SLXCOMDISABLECTRLCOPYINSAMEDIR_H

typedef enum 
{
    DCCO_ON,
    DCCO_OFF,
} DccOperation;

bool DisableCtrlCopyInSameDir_IsRunning();

void DisableCtrlCopyInSameDir_Op(DccOperation op);

// »Ö¸´hook£¨²»µ÷ÓÃ£©
void DisableCtrlCopyInSameDir_End();

#endif
