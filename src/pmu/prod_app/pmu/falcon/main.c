/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file main.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Implementation --------------------------------- */
int pmuMain(int argc, char **ppArgv)
    GCC_ATTRIB_SECTION("imem_resident", "pmuMain");

/*!
 * Main entry-point for the RTOS application.
 *
 * @param[in]  argc    Number of command-line arguments (always 1)
 * @param[in]  ppArgv  Pointer to the command-line argument structure (@ref
 *                     RM_PMU_CMD_LINE_ARGS).
 *
 * @return zero upon success; non-zero negative number upon failure
 */
GCC_ATTRIB_NO_STACK_PROTECT()
int
pmuMain
(
    int    argc,
    char **ppArgv
)
{
    LwU32 ind;
    LwU32 blk;
    LwU32 hwcfg;
    LwU32 imemBlks;

    // Read the total number of IMEM blocks
    hwcfg = REG_RD32(CSB, LW_CMSDEC_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CMSDEC_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);

    // Ilwalidate bootloader
    if (PMUCFG_ENGINE_ENABLED(LSF))
    {
        // LSMODE bootloader may not be at the default block
        falc_imtag(&blk, PMU_LOADER_OFFSET);
        if (!IMTAG_IS_MISS(blk))
        {
            blk = IMTAG_IDX_GET(blk);

            // Bootloader might take more than a block, so ilwalidate those as well.
            for (ind = blk; ind < imemBlks; ind++)
            {
                falc_imilw(ind);
            }
        }
    }
    else
    {
        blk = IMEM_ADDR_TO_IDX(PMU_LOADER_OFFSET);

        // Bootloader might take more than a block, so ilwalidate those as well.
        for (ind = blk; ind < imemBlks; ind++)
        {
            falc_imilw(ind);
        }
    }

#ifdef ON_DEMAND_PAGING_BLK
    {
        //
        // Used for an on-demand paging, this routine is called as early as
        // possible to allow for paging of the init code. Regular overlay builds
        // will have the early exception handler installed during @ref lwosInit().
        //
        extern void EV_preinit_routine(void);
        falc_wspr(EV, EV_preinit_routine);
    }
#else // ON_DEMAND_PAGING_BLK
    {
        LwU8 initOvlIndex = OVL_INDEX_IMEM(init);

        OSTASK_LOAD_IMEM_OVERLAYS_LS(&initOvlIndex, 1);
    }
#endif // ON_DEMAND_PAGING_BLK

    return initPmuApp((RM_PMU_CMD_LINE_ARGS*)((void *)ppArgv));
}

/*!
 * @brief      Initialize the IDLE Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
idlePreInitTask_IMPL(void)
{
    // NOP, task is created in xPortInitialize().
    return FLCN_OK;
}
