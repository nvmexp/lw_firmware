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
 * @file: booter_boot_gsprm_tu10x_ga100.c
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
booterPollForScrubbing_TU10X(void)
{
    BOOTER_STATUS       status = BOOTER_OK;
    LwU32               data   = 0;
    BOOTER_TIMESTAMP    startTimeNs;
    LwS32               timeoutLeftNs;

    // Poll for SRESET to complete
    booterGetLwrrentTimeNs_HAL(pBooter, &startTimeNs);
    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_DMACTL);
    while ((FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _DMEM_SCRUBBING, _PENDING, data)) || 
           (FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _IMEM_SCRUBBING, _PENDING, data)))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckTimeout_HAL(pBooter, (LwU32)BOOTER_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs));
        data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_DMACTL);
    }

    return status;
}

/*
 * @brief 
 */
BOOTER_STATUS
booterSetupTargetRegisters_TU10X(void)
{
    BOOTER_STATUS status = BOOTER_OK;
    LwU32 data;

    // TODO suppal/derekw: revisit this
    // Setting REGIONCFG causes SACC in GSP-RM / LIBOS during boot
#if 0
    //
    // For few LS falcons ucodes, we use bootloader to load actual ucode,
    // so we need to restrict them to be loaded from WPR only
    //

    LwU32 mask           = 0;
    LwU32 i              = 0;

    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FBIF_REGIONCFG);
    for (i = 0; i < LW_PFALCON_FBIF_TRANSCFG__SIZE_1; i++)
    {
        mask = ~(0xF << (i*4));
        data = (data & mask) | ((2 & 0xF) << (i * 4));
    }
    BOOTER_REG_WR32(BAR0, LW_PGSP_FBIF_REGIONCFG, data);
#endif

    // Step 1: Set SCTL PLM to only HS accessible
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_SCTL_PRIV_LEVEL_MASK, BOOTER_PLMASK_READ_L0_WRITE_L3);

    //
    // Step-2: Set initial SCTLs
    //
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _FALSE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _FALSE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_SCTL, data);
    
    // Step-3: Set CPUCTL_ALIAS_EN to FALSE
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, 0);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_CPUCTL, data);

    //
    // Step-4: Setup PLMs
    //
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IMEM_PRIV_LEVEL_MASK,    BOOTER_PLMASK_READ_L0_WRITE_L3);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_DMEM_PRIV_LEVEL_MASK,    BOOTER_PLMASK_READ_L0_WRITE_L0);  // TODO suppal/derekw: TASK_RM fails to come up if this is WRITE_L3
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_CPUCTL_PRIV_LEVEL_MASK,  BOOTER_PLMASK_READ_L0_WRITE_L3);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_EXE_PRIV_LEVEL_MASK,     BOOTER_PLMASK_READ_L0_WRITE_L0);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IRQTMR_PRIV_LEVEL_MASK,  BOOTER_PLMASK_READ_L0_WRITE_L0);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_MTHDCTX_PRIV_LEVEL_MASK, BOOTER_PLMASK_READ_L0_WRITE_L0);

    // Setup more target falcon PLMs
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK,   BOOTER_PLMASK_READ_L0_WRITE_L3);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK,  BOOTER_PLMASK_READ_L0_WRITE_L3);

    //
    // Step-5: Ensure HALTED
    //
    // (skipped as Booter already resets GSP before setting up registers)
    //
 
    //
    // Step-6: Set final SCTLs
    //

    // Note: GSP-RM is L0 (so do not program _LSMODE or _LSMODE_LEVEL here)
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_SCTL, data);

    // Check if AUTH_EN is still TRUE
    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_SCTL);
    if (!FLD_TEST_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data))
    {
        return BOOTER_ERROR_LS_BOOT_FAIL_AUTH;
    }

    // Step-7: Set CPUCTL_ALIAS_EN to TRUE (RISC-V as GSP-RM is RISC-V)
    data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL, _ALIAS_EN, _TRUE, 0);
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_CPUCTL, data);

    return status;
}

/*!
 * @brief 
 */
void
booterStartGspRm_TU10X(void)
{
    // Enable FBIF, allow physical addresses
    BOOTER_REG_WR32(BAR0, LW_PGSP_FBIF_CTL, DRF_DEF(_PGSP, _FBIF_CTL, _ENABLE, _TRUE) |
                                            DRF_DEF(_PGSP, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW));

    // TODO(suppal): Setup NACK as ACK that should help if we try invalid read
    BOOTER_REG_WR32(BAR0, LW_PGSP_FBIF_CTL2, DRF_DEF(_PGSP, _FBIF_CTL2, _NACK_MODE, _NACK_AS_ACK));

#ifdef LW_PRISCV_RISCV_CPUCTL_ALIAS_EN // Pre-GA10X only
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_CPUCTL_ALIAS, DRF_DEF(_PGSP, _RISCV_CPUCTL_ALIAS, _STARTCPU, _TRUE));
#else
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_CPUCTL, DRF_DEF(_PGSP, _RISCV_CPUCTL, _STARTCPU, _TRUE));
#endif
}
