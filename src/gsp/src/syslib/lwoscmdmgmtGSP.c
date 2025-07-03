/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwoscmdmgmtGSP.c
 * @brief   Wrappers for LwOs for userspace (legacy) tasks.
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwmisc.h>
#include <lwriscv/print.h>
#include "syslib/syslib.h"

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <sections.h>
#include <lwrtos.h>

/* ------------------------ Module Includes -------------------------------- */
#include "tasks.h"

//
// This file implements features required by (future) tasks to communicate with
// Resman message queue. For ordinary falcon, every task just writes to RM queue,
// but that may not be possible in protected rtos.
//
// Therefore we have this wrapper that passess the message to proper task that
// would dispatch it on our behalf.
//

sysSYSLIB_CODE static LwBool
msgQueueSendInternal(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody, LwBool bBlocking)
{

    dbgPrintf(LEVEL_ERROR, "%s: Implement message posting.\n", __FUNCTION__);
    //
    // MK TODO: we need something like
    // lwrtosQueueSendBlocking(rmMsgRequestQueue, &msg, 4 + msg.hdr.size);
    // BUT
    // The way rm messages are handled is they have separate header and payload pointer.
    // For Falcons, that data is just copied to RM queue here and that's it.
    // Because our tasks have no access to RM queues, we can't do it, so what
    // we'd need do is to pass whole data via lwrtos queues.
    //
    // This requires rework of communication structures, and for now no task
    // requires that. Therefore we keep that stubbed for now.
    //

    return LW_FALSE;

}

sysSYSLIB_CODE LwBool
osCmdmgmtRmQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody)
{
    return msgQueueSendInternal(pHdr, pBody, LW_TRUE);
}

sysSYSLIB_CODE LwBool
osCmdmgmtRmQueuePostNonBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody)
{
    return msgQueueSendInternal(pHdr, pBody, LW_FALSE);
}

