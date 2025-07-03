/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  uprocgsprtos.h
 * @brief class for UprocGspRtos
 */
#ifndef _UPROCGSPRTOS_H_
#define _UPROCGSPRTOS_H_

#include "uprocrtos.h"
#include "rmgspcmdif.h"
#include "core/include/lwrm.h"

// At the time the tests were written, GSP ACR did not work, and it still does not
// work as of submission. When GSP ACR is finally enabled, this should be set to 1
#define GSPUPROCRTOS_USE_ACR 0

class UprocGspRtos : public UprocRtos
{
public:
    UprocGspRtos(GpuSubdevice *pParent);
    virtual ~UprocGspRtos();
    virtual RC Initialize();
    virtual void Shutdown();
    virtual RC FreeResources();
    virtual RC Command(RM_UPROC_TEST_CMD *inCmd,
                       RM_UPROC_TEST_MSG *inMsg,
                       RM_FLCN_QUEUE_HDR *inQueueHdr,
                       LwU32             *pSeqNum);
    virtual RC Command(RM_FLCN_CMD_GSP   *inCmd,
                       RM_FLCN_MSG_GSP   *inMsg,
                       LwU32             *pSeqNum);
    virtual RC CommandCC(RM_FLCN_CMD_GSP   *inCmd,
                         RM_FLCN_MSG_GSP   *inMsg,
                         LwU32             *pSeqNum,
                         LwRm::Handle       hMemory,
                         LwU32              reqMsgSize,
                         LwU32              rspBufferSize);

    virtual RC IsCommandComplete(LwU32 seqNum, bool *pbComplete);
    virtual RC WaitMessage(LwU32 seqNum);
    virtual RC WaitUprocReady();
    virtual bool CheckTestSupported(LwU8 testId);
    virtual RC SetupFbSurface(LwU32 size);
    virtual RC SetupUprocChannel(GpuTestConfiguration* testConfig);
    virtual RC SendUprocTraffic(LwU32 wakeupMethod, FLOAT64 timeOutMs);
    virtual LwU32 GetMscgWakeupReasonMask(LwU32 wakeupMethod);
    virtual RC AllocateMessageBuffer(Surface2D *pSurface, LwU32 Size);

private:

    struct PollMsgArgs
    {
        UprocGspRtos  *pGspRtos;
        LwU32         seqNum;
        RC            pollRc;
    };

    static bool PollMsgReceived(void *pVoidPollArgs);

    struct PollGspArgs
    {
        UprocGspRtos  *pGspRtos;
        RC            pollRc;
    };
    static bool  PollGspReady(void *pVoidPollArgs);

    //for FreeResources
    bool m_bSetupFbSurface;

    // GSP fb buffer
    Surface2D       m_fbmemSurface;
};

#endif

