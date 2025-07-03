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
 * @file  uprocrtos.h
 * @brief Base class for generic UprocRtos
 *        Derived classes: UprocSec2Rtos, UprocGspRtos
 */

#ifndef _UPROCRTOS_H_
#define _UPROCRTOS_H_

#ifndef LWTYPES_INCLUDED
#include "lwtypes.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_SURF2D_H
#include "gpu/utility/surf2d.h"
#endif

#include "uproctest.h"
#include "uprociftest.h"
#include "flcnifcmn.h"

#define CMD_TIMEOUT_MS      1000    //!< Maximum time to wait for a command
#define CMD_EMU_TIMEOUT_MS  1200000 //!< Maximum time to wait for a command when running in emulation
#define BOOT_TIMEOUT_MS     10000   //!< Maximum time to wait for bootstrap
#define BOOT_EMU_TIMEOUT_MS 1200000 //!< Maximum time to wait for bootstrap when running in emulation

class GpuSubdevice;

class UprocRtos
{
public:
    UprocRtos(GpuSubdevice *pParent);
    virtual ~UprocRtos()
    {
    }
    virtual RC Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual RC FreeResources() = 0;

    virtual RC Command(RM_UPROC_TEST_CMD    *inCmd,
                       RM_UPROC_TEST_MSG    *inMsg,
                       RM_FLCN_QUEUE_HDR    *inQueueHdr,
                       LwU32 *pSeqNum) = 0;

    virtual RC WaitUprocReady() = 0;
    virtual bool CheckTestSupported(LwU8 testId) = 0;
    virtual RC SetupFbSurface(LwU32 size) = 0;
    virtual RC SetupUprocChannel(GpuTestConfiguration* testConfig) = 0;
    virtual RC SendUprocTraffic(LwU32 wakeupMethod, FLOAT64 timeOutMs) = 0;
    virtual LwU32 GetMscgWakeupReasonMask(LwU32 wakeupMethod) = 0;

    RC UprocRtosRunTests(LwU32 test, ...);
    void SetRmTestHandle(RmTest *pRmTestHandle);
    void Reset();

    enum UPROC_ID
    {
        UPROC_ID_ILWALID = 0,            //!< Zero should not map to a uproc.
        UPROC_ID_SEC2,
        UPROC_ID_GSP,
        UPROC_ID_END                     //!< END is MAX+1.
    };

    LwU32           m_UprocId;           //!< Uproc Id for this uproc
    RmTest          *m_pRt;              //!< RmTest pointer for reghal access
    UprocTest       *m_UprocTest;        //!< Test framework object (should we make this protected?)

protected:
    bool            m_Initialized;      //!< true if the Uproc object has been initialized
    LwRm::Handle    m_Handle;           //!< Handle to the Uproc object
    GpuSubdevice    *m_Subdevice;          //!< GpuSubdevice that owns this Uproc instance

    typedef enum
    {
        COMMAND_HDR,
        MESSAGE_HDR,
        EVENT_HDR
    } UprocHeaderType;

    virtual RC IsCommandComplete(LwU32 seqNum, bool *pbComplete) = 0;
    virtual RC WaitMessage(LwU32 seqNum) = 0;

    RC CreateSurface(Surface2D *pSurface, LwU32 Size);
    void DumpHeader(void *pHdr, UprocHeaderType hdrType);

};

#endif
