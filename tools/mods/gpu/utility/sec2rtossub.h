/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MAXWELL_SEC2_H
#define INCLUDED_MAXWELL_SEC2_H

#ifndef LWTYPES_INCLUDED
#include "lwtypes.h"
#endif

#ifndef _RMSEC2CMDIF_H
#include "rmsec2cmdif.h"
#endif

#ifndef _RMFLCNCMDIF_H
#include "rmflcncmdif.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_SURF2D_H
#include "surf2d.h"
#endif

class GpuSubdevice;
class ModsEvent;
class Surface2D;

//-----------------------------------------------------------------------------
//! \brief Provides an interface for communicating with the MAXWELL_SEC2
//!
//! Each GpuSubdevice that supports the MAXWELL_SEC2 should contain a valid
//! pointer to this class.
//!
class Sec2Rtos
{
public:
    Sec2Rtos(GpuSubdevice *pParent);
    virtual ~Sec2Rtos();
    RC       Initialize();
    void     Shutdown();

    RC       GetUcodeState(UINT32 *pUcodeState);
    RC       Command(RM_FLCN_CMD_SEC2 *pCmd,
                     RM_FLCN_MSG_SEC2 *pMsg,
                     UINT32           *pSeqNum);
    RC       CreateSurface(Surface2D *pSurface, UINT32 Size);
    RC       IsCommandComplete(UINT32 seqNum, bool *pbComplete);
    RC       WaitSEC2Ready();
    bool     CheckTestSupported(UINT08);
    bool     GetAllocatedRmHandle(LwRm::Handle *pHandle);

private:
    bool          m_Initialized;     //!< true if the SEC2 has been initialized
    LwRm::Handle  m_Handle;          //!< Handle to the SEC2 object
    GpuSubdevice *m_Parent;          //!< GpuSubdevice that owns this SEC2

    UINT32        GetWidth(UINT32 *pPitch);
    RC            WaitMessage(UINT32 seqNum);

    struct PollMsgArgs
    {
        Sec2Rtos     *pThis;
        UINT32        seqNum;
        RC            pollRc;
    };
    static bool  PollMsgReceived(void *pVoidPollArgs);

    struct PollSec2Args
    {
        Sec2Rtos     *pThis;
        RC            pollRc;
    };
    static bool  PollSec2Ready(void *pVoidPollArgs);

    typedef enum
    {
        COMMAND_HDR,
        MESSAGE_HDR,
        EVENT_HDR
    } Sec2HeaderType;
    void         DumpHeader(void *pHdr, Sec2HeaderType hdrType);
};

#endif
