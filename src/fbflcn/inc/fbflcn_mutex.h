/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_MUTEX_H
#define FBFLCN_MUTEX_H

#include "fbflcn_defines.h"


#define MUTEX_REQ_TIMEOUT_NS 50000
//
// Mutex definition
//
// This can be switched to use either the PMGR mutex register or the fbfalcon mutex register
//

#define IEEE1500_MUTEX_INDEX            17
#define IEEE1500_MUTEX_REG              LW_PMGR_MUTEX_REG
#define IEEE1500_MUTEX_REG_SIZE         LW_PMGR_MUTEX_REG__SIZE_1
#define IEEE1500_MUTEX_REG_INITIAL_LOCK LW_PMGR_MUTEX_REG_VALUE_INITIAL_LOCK
#define IEEE1500_MUTEX_ID_RELEASE       LW_PMGR_MUTEX_ID_RELEASE
#define IEEE1500_MUTEX_ID_ACQUIRE       LW_PMGR_MUTEX_ID_ACQUIRE
#define IEEE1500_MUTEX_ID_ACQUIRE_VALUE_INIT       LW_PMGR_MUTEX_ID_ACQUIRE_VALUE_INIT
#define IEEE1500_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL  LW_PMGR_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL

typedef struct MUTEX_HANDLER
{
    LwU32 registerMutex;
    LwU32 registerIdRelease;
    LwU32 registerIdAcquire;
    LwU32 valueInitialLock;
    LwU32 valueAcquireInit;
    LwU32 valueAcquireNotAvailable;
} MUTEX_HANDLER;

/*
#define MUTEX_INDEX            0
#define MUTEX_REG              LW_CFBFALCON_MUTEX
#define MUTEX_REG_SIZE         LW_CFBFALCON_MUTEX__SIZE_1
#define MUTEX_REG_INITIAL_LOCK LW_CFBFALCON_MUTEX_VALUE_INITIAL_LOCK
#define MUTEX_ID_RELEASE       LW_CFBFALCON_MUTEX_ID_RELEASE
#define MUTEX_ID_ACQUIRE       LW_CFBFALCON_MUTEX_ID
#define MUTEX_ID_ACQUIRE_VALUE_INIT       LW_CFBFALCON_MUTEX_ID_VALUE_INIT
#define MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL  LW_CFBFALCON_MUTEX_ID_VALUE_NOT_AVAIL
*/

LW_STATUS mutexAcquireByIndex_GV100
(
        MUTEX_HANDLER* pMutex,
        LwU32  timeoutns,
        LwU8  *pToken
)
GCC_ATTRIB_SECTION("resident", "mutexAcquireByIndex_GV100");

LW_STATUS
mutexReleaseByIndex_GV100
(
        MUTEX_HANDLER* pMutex,
        LwU8  token
)
GCC_ATTRIB_SECTION("resident", "mutexReleaseByIndex_GV100");

#endif // FBFLCN_MUTEX_H
