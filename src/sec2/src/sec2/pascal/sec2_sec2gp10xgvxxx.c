/*
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_sec2gp10xonly.c
 * @brief  SEC2 HAL Functions for GP10X only functions
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_objsec2.h"


#include "dev_fifo.h"
#include "mscg_wl_bl_init.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Reads a Clock-gated domain register to wake-up MSCG from sleep.
 *
 * Read a clock gated domain register, when the system is in MSCG,
 * so that the system comes out of MSCG.
 *
 * @param[out] *pDenylistedRegVal the value of the register read, which is
 *                                 supposed to trigger MSCG exit.
 *
 * @returns    FLCN_OK on successful register read
 *             and whatever *_REG_RD32_ERRCHK returns if we can't read register,
 *             usually FLCN_ERR_BAR0_PRIV_READ_ERROR/FLCN_ERR_CSB_PRIV_READ_ERROR
 */
FLCN_STATUS
sec2MscgDenylistedRegRead_GP10X
(
    LwU32 *pDenylistedRegVal
)
{
    if (pDenylistedRegVal == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    return BAR0_REG_RD32_ERRCHK(SEC_BL_RANGE_START_4, pDenylistedRegVal);
}

