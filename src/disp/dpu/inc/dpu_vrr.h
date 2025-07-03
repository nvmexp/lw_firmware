/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_VRR_H
#define DPU_VRR_H


/*!
 * @file    dpu_vrr.h
 * @copydoc task4_vrr.c
 */

/* ------------------------- System Includes ------------------------------- */
#include "unit_dispatch.h"
#include "lwostimer.h"

/* ------------------------- Application Includes -------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */

/*!
 * @brief OS_TIMER_ENTRYs for VRR Task
 */
enum
{
    VRR_OS_TIMER_ENTRY = 0x0,

    // Max entries for VRR OS_TIMER
    VRR_OS_TIMER_ENTRY_NUM_ENTRIES,
};

/*!
 * @brief Context information for VRR.
 */
typedef struct
{
    LwBool      bEnableVrrForceFrameRelease;    // Set when DPU is scheduling force frame callback
    LwU8        head;                           // index of head we are going to schedule force frame callback for
    LwU8        vrrHeadGroupMask;               // Headmask for all heads in VRR headgroup of above head
    LwU32       relCnt;                         // count of releases exelwted in DPU
    LwU32       forceReleaseThresholdUs;        // max wait time (us) for releasing force frame
    OS_TIMER    osTimer;                        // OS Timer for VRR force frame release
} VRR_CONTEXT;

/*!
 * @brief Context information for the VRR task.
 */
typedef union
{
    LwU8          eventType;
    DISP2UNIT_CMD command;
} DISPATCH_VRR;

/* ------------------------ Global Variables ------------------------------- */
extern VRR_CONTEXT VrrContext;

void vrrInit(void)
    GCC_ATTRIB_SECTION("imem_init", "vrrInit");

#endif // DPU_VRR_H
