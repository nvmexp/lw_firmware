/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJPCM_H
#define PMU_OBJPCM_H

/*!
 * @file pmu_objpcm.h
 */

/* ------------------------ System includes --------------------------------- */
#include "flcntypes.h"
#include "unit_api.h"
#include "main.h"

/* ------------------------ Application includes ---------------------------- */
#include "pmu_oslayer.h"
#include "lwostimer.h"

/* ------------------------ Types definitions ------------------------------- */
typedef enum
{
    PMU_OS_TIMER_ENTRY_SW_CNTR = 0,
    PCM_OS_TIMER_ENTRY_NUM_ENTRIES
} PCM_OS_TIMER_ENTRIES;

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
extern OS_TIMER PcmOsTimer;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Work-Item IMPL Prototypes ----------------------- */

#endif // PMU_OBJPCM_H

