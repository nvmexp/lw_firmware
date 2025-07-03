/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  uprocsec2rtos.h
 * @brief class for UprocSec2Rtos
 */
#ifndef _UPROCSEC2RTOS_H_
#define _UPROCSEC2RTOS_H_

#include "uprocrtos.h"
#include "core/include/lwrm.h"

class UprocSec2Rtos : public UprocRtos
{
public:
    UprocSec2Rtos(GpuSubdevice *pParent);
    virtual ~UprocSec2Rtos();
    virtual RC Initialize();
    virtual void Shutdown();
    virtual RC FreeResources();
    virtual RC Command(RM_UPROC_TEST_CMD    *inCmd,
                       RM_UPROC_TEST_MSG    *inMsg,
                       RM_FLCN_QUEUE_HDR    *inQueueHdr,
                       LwU32                *pSeqNum);
    virtual RC IsCommandComplete(LwU32 seqNum, bool *pbComplete);
    virtual RC WaitMessage(LwU32 seqNum);
    virtual RC WaitUprocReady();
    virtual bool CheckTestSupported(LwU8 testId);
    virtual RC SetupFbSurface(LwU32 size);
    virtual RC SetupUprocChannel(GpuTestConfiguration *testConfig);
    virtual RC SendUprocTraffic(LwU32 wakeupMethod, FLOAT64 timeOutMs);
    virtual LwU32 GetMscgWakeupReasonMask(LwU32 wakeupMethod);

private:
    bool bHandleIsOursToFree; //Will be deprecated when we remove Sec2RtosSub
    struct PollMsgArgs
    {
        UprocSec2Rtos   *pSec2Rtos;
        LwU32           seqNum;
        RC              pollRc;
    };

    static bool PollMsgReceived(void *pVoidPollArgs);

    struct PollSec2Args
    {
        UprocSec2Rtos   *pSec2Rtos;
        RC              pollRc;
    };
    static bool  PollSec2Ready(void *pVoidPollArgs);

    struct PollSemaphoreArgs
    {
        LwU32           value;
        LwU32           *pAddr;
    };
    static bool  PollUprocSemaphoreVal(void *pVoidPollArgs);

    //for FreeResources
    bool m_bSetupUprocChannel;
    bool m_bSetupFbSurface;
    GpuTestConfiguration *m_pTestConfig;

    // SEC2 channel data
    Channel         *m_pCh;
    LwU64           m_gpuAddr;
    LwU32           *m_cpuAddr;

    LwRm::Handle    m_hCh;
    LwRm::Handle    m_hObj;
    LwRm::Handle    m_hSemMem;
    LwRm::Handle    m_hVA;

    // SEC2 fb buffer
    Surface2D       m_fbmemSurface;
};

#endif
