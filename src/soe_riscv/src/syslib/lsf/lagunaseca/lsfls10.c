/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lsfLS10.c
 * @brief  Light Secure Falcon (LSF) LS10 Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

#define KERNEL_SOURCE_FILE 1

/* ------------------------ LW Includes ------------------------------------- */
#include <lwtypes.h>
#include <riscvifriscv.h>
#include "rmlsfm.h"

/* ------------------------ Register Includes ------------------------------- */
#include <engine.h>
#include <riscv_csr.h>
#include <lwriscv-mpu.h>

/* ------------------------ SafeRTOS Includes ------------------------------- */
#include <lwrtos.h>
#include <sections.h>
#include <lwoslayer.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <drivers/drivers.h>
#include <syslib/idle.h>
#include <lw_riscv_address_map.h>

/* ------------------------ Module Includes --------------------------------- */
#include "config/g_profile.h"
#include "config/g_sections_riscv.h"
#include "config/soe-config.h"
#include "soeifcmn.h"


/*!
 @brief: Program MTHDCTX PRIV level mask
 */
sysKERNEL_CODE void
lsfSetupMthdctx_LS10(void)
{
    LwU32 data32;

    data32 = csbRead(LW_CSOE_FALCON_MTHDCTX_PRIV_LEVEL_MASK);

    data32 = FLD_SET_DRF(_CSOE, _FALCON_MTHDCTX_PRIV_LEVEL_MASK,
                         _READ_PROTECTION, _ALL_LEVELS_ENABLED, data32);
    csbWrite(LW_CSOE_FALCON_MTHDCTX_PRIV_LEVEL_MASK, data32);

    // Do not RMW since unselwred entity could have setup invalid fields
    data32 = 0;
    data32 = FLD_SET_DRF(_CSOE, _FALCON_ITFEN, _MTHDEN,       _ENABLE, data32);
    data32 = FLD_SET_DRF(_CSOE, _FALCON_ITFEN, _CTXEN,        _ENABLE, data32);
    data32 = FLD_SET_DRF(_CSOE, _FALCON_ITFEN, _PRIV_POSTWR,  _INIT,   data32);

    csbWrite(LW_CSOE_FALCON_ITFEN, data32);

    // Below code is required since in RTOS environment case RISCV core will not always be idle
    data32 = 0;
    data32 = csbRead(LW_CSOE_FALCON_DEBUG1);
    data32 =  FLD_SET_DRF_NUM(_CSOE, _FALCON_DEBUG1, _CTXSW_MODE1, 0x1, data32);
    csbWrite(LW_CSOE_FALCON_DEBUG1, data32);

    data32 = csbRead(LW_CSOE_FALCON_DEBUG1);
}


/*!
 * @brief Initialize aperture settings (protected TRANSCFG regs)
 */
sysKERNEL_CODE void
lsfInitApertureSettings_LS10(void)
{
    //
    // The following setting updates only take affect if SOE is in LSMODE &&
    // UCODE_LEVEL > 0
    //

    // COHERENT_SYSMEM aperture for SPA.
    csbWrite(LW_CSOE_FBIF_TRANSCFG(SOE_DMAIDX_PHYS_SYS_COH_FN0),
             (DRF_DEF(_CSOE, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)        |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _TARGET,         _COHERENT_SYSMEM) |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // NONCOHERENT_SYSMEM aperture for SPA.
    csbWrite(LW_CSOE_FBIF_TRANSCFG(SOE_DMAIDX_PHYS_SYS_NCOH_FN0),
             (DRF_DEF(_CSOE, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)           |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _TARGET,         _NONCOHERENT_SYSMEM) |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));
}

