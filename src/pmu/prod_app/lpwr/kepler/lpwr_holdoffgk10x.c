/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_holdoffgkxxx.c
 * @brief  LPWR holdoff framework
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief Timeout for engaging holdoff
 */
#define LPWR_HOLDOFF_TIMEOUT_NS                         (50000)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void        s_lpwrHoldoffMaskSetHw_GMXXX(LwU32 engHoldoffMask);
static FLCN_STATUS s_lpwrHoldoffMaskPoll_GMXXX(LwU32 engHoldoffMask);
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief API to set method holdoff for engines
 *
 * @param[in]   clientId            Client ID - LPWR_HOLDOFF_CLIENT_ID_xyz
 * @param[in]   clientHoldoffMask   Holdoff mask requested by client
 *
 * @return  FLCN_ERROR  On failure
 *          FLCN_OK     On success
 */
FLCN_STATUS
lpwrHoldoffMaskSet_GMXXX
(
    LwU32 clientId,
    LwU32 clientHoldoffMask
)
{
    LwU32 i;
    LwU32 finalHoldoffMask = 0;

    //
    // Update the client cache and generate final holdoff mask by checking
    // all clients
    //
    Lpwr.clientHoldoffMask[clientId] = clientHoldoffMask;

    for (i = 0; i < LPWR_HOLDOFF_CLIENT_ID__COUNT; i++)
    {
        finalHoldoffMask |= Lpwr.clientHoldoffMask[i];
    }

    // Set holdoff mask in HW
    s_lpwrHoldoffMaskSetHw_GMXXX(finalHoldoffMask);

    //
    // Client requested to set non-zero holdoff mask:
    //
    // - Poll for engagement of client holdoff mask
    // - Ensure engines are idle after updating holdoff
    //
    if (clientHoldoffMask != 0)
    {
        if (FLCN_OK != s_lpwrHoldoffMaskPoll_GMXXX(clientHoldoffMask))
        {
            return FLCN_ERROR;
        }

        FOR_EACH_INDEX_IN_MASK(32, i, clientHoldoffMask)
        {
            if (!fifoIsEngineIdle_HAL(&Fifo, i))
            {
                return FLCN_ERROR;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper to set method holdoff in HW
 *
 * @param[in]   engHoldoffMask   requested holdoff mask
 */
static void
s_lpwrHoldoffMaskSetHw_GMXXX
(
    LwU32 engHoldoffMask
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // WAR1748086: Priv access to update holdoff
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_HOLDOFF_WAR1748086_PRIV_ACCESS))
    {
        REG_WR32_STALL(FECS, LW_THERM_ENG_HOLDOFF, engHoldoffMask);
    }
    else // CSB access to update holdoff
    {
        REG_WR32_STALL(CSB, LW_CPWR_THERM_ENG_HOLDOFF, engHoldoffMask);
    }
#endif
}

/*!
 * @brief Hepler to poll for holdoff engagement
 *
 * @param[in]   engHoldoffMask   requested holdoff mask for polling
 *
 * @return  FLCN_ERROR  On failure
 *          FLCN_OK     On success
 */
static FLCN_STATUS
s_lpwrHoldoffMaskPoll_GMXXX
(
    LwU32 engHoldoffMask
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // WAR1748086: Priv access to poll holdoff engagement
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_HOLDOFF_WAR1748086_PRIV_ACCESS))
    {
        if (!PMU_REG32_POLL_NS(USE_FECS(LW_THERM_ENG_HOLDOFF_ENTER),
                               engHoldoffMask,
                               engHoldoffMask,
                               LPWR_HOLDOFF_TIMEOUT_NS,
                               PMU_REG_POLL_MODE_BAR0_EQ))
        {
            return FLCN_ERROR;
        }
    }
    else // CSB access to poll holdoff engagement
    {
        if (!PMU_REG32_POLL_NS(LW_CPWR_THERM_ENG_HOLDOFF_ENTER,
                               engHoldoffMask,
                               engHoldoffMask,
                               LPWR_HOLDOFF_TIMEOUT_NS,
                               PMU_REG_POLL_MODE_CSB_EQ))
        {
            return FLCN_ERROR;
        }
    }
#endif

    return FLCN_OK;
}
