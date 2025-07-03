/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_QUEUES_H
#define FBFLCN_QUEUES_H

/* ------------------------- Includes --------------------------------------- */
#include <lwtypes.h>
#include "lwuproc.h"
#include "rmfbflcncmdif.h"
#include "fbflcn_defines.h"
#include "fbflcn_table.h"

#include "dev_fbfalcon_csb.h"

#include "config/fbfalcon-config.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */


// cmd queue definition

// total number of command queues used
// internal and external.  The implicit assumption is that the interal queues come
// first in the setup and that the index of the queue info matches the queue data array
#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
#define FBFLCN_CMD_QUEUE_NUM                5
#else
#define FBFLCN_CMD_QUEUE_NUM                4
#endif


// number of command queues inside fbflcn
// used in ucode.
#define FBFLCN_CMD_QUEUE_INTERNAL_NUM_GV100 3
#define FBFLCN_CMD_QUEUE_INTERNAL_NUM_TU10X 4

#define FBFLCN_CMD_QUEUE_ADVERTISED_NUM     3

// queue indexes covered by FBFLCN_CMD_QUEUE_NUMs
// internal

#define FBFLCN_CMD_QUEUE_RM2FBFLCN            0
#define FBFLCN_CMD_QUEUE_PMU2FBFLCN           1
#define FBFLCN_CMD_QUEUE_FBFLCN2RM            2
// external (external definitions have to be after internal ones which need to form
#define FBFLCN_CMD_QUEUE_FBFLCN2PMU           3
#define FBFLCN_CMD_QUEUE_PMU2FBFLCN_SECONDARY 4

// assignmnt of queue indexes to specific purpose
#define FBFLCN_COM_RM_MSG_QUEUE             4

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * Universal queue resource management structure.
 */
typedef struct
{
    LwU32   headPriv;           // priv address of the head pointer
    LwU32   tailPriv;           // priv address of the tail pointer
    LwU8    response;           // queueId for responce (ack)
    struct
    {
        LwU8 enabled: 1;        // enable/disable the queue
        LwU8 internal: 1;       // internal=1: queue is inside the fbfalcon
                                // internal=0: queue is oustide (in another falcon)
        LwU8 swgen_irq: 1;      // indicates if an interrupt has to be generated
                                // by the fbfalcon sw and refers to the msg interrupt
                                // back to host.
    } flags;
} FBFLCN_QUEUE_RESOURCE,
*PFBFLCN_QUEUE_RESOURCE;

/* ------------------------- Prototypes ------------------------------------ */
void initQueueResources(void)
    GCC_ATTRIB_SECTION("init", "initQueueResources");

void fbflcnQInterruptHandler(void)
    GCC_ATTRIB_SECTION("resident", "fbflcnQInterruptHandler");

void fbflcnQInitDirect(void)
    GCC_ATTRIB_SECTION("resident", "fbflcnQInit");

void processCmdQueueDirect(LwU32 queueId)
    GCC_ATTRIB_SECTION("resident", "processCmdQueueDirect");

#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
void processCmdQueue2DirectForHalt(LwU32 queueId)
    GCC_ATTRIB_SECTION("resident", "processCmdQueue2DirectForHalt");
#endif

#endif //FBFLCN_QUEUES_H
