/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_boot_gsprm_ga10x.c
 */

//
// Includes
//
#include "booter.h"

#include "dev_gsp.h"

/*
 * @brief 
 */
BOOTER_STATUS
booterPollForScrubbing_GA10X(void)
{
    BOOTER_STATUS       status = BOOTER_OK;
    LwU32               data   = 0;
    BOOTER_TIMESTAMP    startTimeNs;
    LwS32               timeoutLeftNs;

    // Poll for SRESET to complete
    booterGetLwrrentTimeNs_HAL(pBooter, &startTimeNs);
    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_HWCFG2);
    while (FLD_TEST_DRF(_PFALCON_FALCON, _HWCFG2, _MEM_SCRUBBING, _PENDING, data))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckTimeout_HAL(pBooter, (LwU32)BOOTER_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs));
        data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_HWCFG2);
    }

    return status;
}

BOOTER_STATUS
booterSetupTargetRegisters_GA10X(void)
{
    // Raise BCR PLM (all other PLMs are set by RISC-V manifest)
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_PRIV_LEVEL_MASK, BOOTER_PLMASK_READ_L0_WRITE_L3);

    return BOOTER_OK;
}

void
booterStartGspRm_GA10X(void)
{
    LwU32 bcr;

    bcr = DRF_DEF(_PRISCV_RISCV, _BCR_CTRL, _CORE_SELECT, _RISCV)   |
          DRF_DEF(_PRISCV_RISCV, _BCR_CTRL, _VALID,       _TRUE)    |
          DRF_DEF(_PRISCV_RISCV, _BCR_CTRL, _BRFETCH,     _TRUE);
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_CTRL, bcr);

    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_CPUCTL, DRF_DEF(_PGSP, _RISCV_CPUCTL, _STARTCPU, _TRUE));

    // Wait for lockdown to be released
    {
        BOOTER_TIMESTAMP startTimeNs;
        LwS32 timeoutLeftNs;
        LwU32 hwcfg2;

        booterGetLwrrentTimeNs_HAL(pBooter, &startTimeNs);
        hwcfg2 = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_HWCFG2);
        while (!FLD_TEST_DRF(_PFALCON, _FALCON_HWCFG2, _RISCV_BR_PRIV_LOCKDOWN, _UNLOCK, hwcfg2))
        {
            if (booterCheckTimeout_HAL(pBooter, (LwU32)BOOTER_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs) != BOOTER_OK)
            {
                break;
            }
            hwcfg2 = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_HWCFG2);
        }
    }
}
