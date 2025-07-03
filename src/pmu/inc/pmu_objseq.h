/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJSEQ_H
#define PMU_OBJSEQ_H

/*!
 * @file pmu_objseq.h
 *
 * PMU Sequencer object and the related functions.
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "unit_api.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu/pmuseqinst.h"
#include "pmu_rtos_queues.h"
#include "pmu_objpmu.h"

#include "config/g_seq_hal.h"

/* ------------------------ Forward Declartion ----------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/*!
 * Sequencer Command type sent to the sequencer task
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;
} DISPATCH_SEQ;

/* ------------------------ External definitions --------------------------- */
/*!
 * SEQ object Definition
 */
typedef struct
{
    LwU8 dummy; // unused -- for compilation purposes only
} OBJSEQ;

extern OBJSEQ Seq;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Private Function Prototypes -------------------- */
LwBool  seqCheckSignal(LwU32 *);

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Public Function Prototypes --------------------- */

#endif // PMU_OBJSEQ_H
