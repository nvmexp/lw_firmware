/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objpg.c
 * @brief  PMU PG framework
 *
 * Manages PG framework. Initially PG frame was introduced for ELPG. We
 * extended it most of non-powergating LowPower features.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "lwostimer.h"
#include "main.h"
#include "main_init_api.h"

#include "dbgprintf.h"

#include "config/g_pg_private.h"
#if(PG_MOCK_FUNCTIONS_GENERATED)
#include "config/g_pg_mock.c"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJPG Pg;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct the PG object.
 *
 * @return  FLCN_OK     On success
 * @return  Others      Error return value from failing child function
 */
FLCN_STATUS
constructPg(void)
{
    LwU32 hwEngIdx;
    FLCN_STATUS status = FLCN_OK;

    memset(&Pg, 0, sizeof(Pg));

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
    {
        status = lpwrCallbackConstruct();
        if (status != FLCN_OK)
        {
            goto constructPg_exit;
        }
    }

    // Ilwalidate Volt Rails. POST_INIT command will init this structure
    Pg.voltRail[PG_VOLT_RAIL_IDX_LOGIC].voltRailIdx = PG_VOLT_RAIL_IDX_ILWALID;
    Pg.voltRail[PG_VOLT_RAIL_IDX_SRAM].voltRailIdx  = PG_VOLT_RAIL_IDX_ILWALID;

    //
    // Initialize PG_ENG IDX Map. All PG_ENG/LPWR_ENG HW IDX pointing to
    // _MAX SW ID.
    //
    for (hwEngIdx = 0; hwEngIdx < PG_ENG_IDX__COUNT; hwEngIdx++)
    {
        Pg.hwEngIdxMap[hwEngIdx] = RM_PMU_LPWR_CTRL_ID_MAX;
    }

constructPg_exit:
    return status;
}

/*!
 * @brief Initialize PG
 *
 * This function does the initialization of PG before scheduler schedules
 * LPWR task for the exelwtion.
 */
void
pgPreInit(void)
{
#if PMUCFG_FEATURE_ENABLED(PMU_GC6_PLUS)
    static LwU8 pgDmaReadBuffer[PG_BUFFER_SIZE_BYTES]
                GCC_ATTRIB_SECTION("dmem_lpwr", "pgDmaReadBuffer")
                GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

    //
    // Wire PG generic buffer. We use this buffer to transfer DI-SSC/GC6
    // related data from FB/SYS MEM to DMEM.
    //
    Pg.dmaReadBuffer = pgDmaReadBuffer;
#endif

    // Initialize PgLog
    pgLogPreInit();

    // Check if pg island is enabled
    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_PLUS) &&
        pgIslandIsEnabled_HAL(&Pg))
    {
        pgIslandPreInit_HAL();
    }

    // Initialize ARCH sub-feature support mask
    Pg.archSfSupportMask = RM_PMU_ARCH_FEATURE_MASK_ALL;

    if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG))
    {
        Pg.archSfSupportMask =
            LPWR_SF_MASK_CLEAR(ARCH, LPWR_ENG, Pg.archSfSupportMask);
    }
}
