/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_MSCGWITHFRL_H
#define DPU_MSCGWITHFRL_H

#include "dpu/dpuifmscgwithfrl.h"
#include "unit_dispatch.h"
#include "lwostimer.h"

/*!
 * @brief OS_TIMER_ENTRYs for MSCG with FRL Task
 */
enum
{
    MSCG_WITH_FRL_OS_TIMER_ENTRY = 0x0,

    // Max entries for MSCG_WITH_FRL OS_TIMER
    MSCG_WITH_FRL_OS_TIMER_ENTRY_NUM_ENTRIES,
};

/*!
 * @brief Structure used to pass MSCGWITHFRL related commands
 */
typedef union
{
    LwU8          eventType;
    DISP2UNIT_CMD command;
} DISPATCH_MSCGWITHFRL;

/*!
 * @brief Context information for the MSCGWITHFRL task.
 */
typedef struct
{
    /*!
     * Head for which feature is enabled
     */
    LwU32           head;
    /*!
     * OS Timer for MSCG with FRL
     */
    OS_TIMER        osTimer;
} MSCGWITHFRL_CONTEXT;

extern MSCGWITHFRL_CONTEXT  mscgWithFrlContext;

void mscgWithFrlInit(void)
    GCC_ATTRIB_SECTION("imem_init", "mscgWithFrlInit");

#endif // DPU_MSCGWITHFRL_H
