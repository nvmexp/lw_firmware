/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_i2cmtxga100.c
 * @brief  Contains all I2C Mutex routines specific to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_obji2c.h"
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Accessor of per-port I2C mutex state register.
 *
 * @return  Current state of per-port I2C mutex mask.
 */
LwU32
i2cMutexMaskRd32_GA100(void)
{
    return REG_RD32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_I2C_MUTEX));
}

/*!
 * @brief   Mutator of per-port I2C mutex state register.
 *
 * @param[in]    New state of per-port I2C mutex mask.
 */
void
i2cMutexMaskWr32_GA100(LwU32 mutexMask)
{
    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_I2C_MUTEX), mutexMask);
}
