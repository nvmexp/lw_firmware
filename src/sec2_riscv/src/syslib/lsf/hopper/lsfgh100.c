/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lsfgh100.c
 * @brief  Light Secure Falcon (LSF) GH100 Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

#define KERNEL_SOURCE_FILE 1

/* ------------------------ LW Includes ------------------------------------- */
#include <lwtypes.h>
#include <riscvifriscv.h>
#include "rmlsfm.h"

/* ------------------------ Register Includes ------------------------------- */
#include <dev_top.h>
#include <engine.h>
#include <riscv_csr.h>
#include <lwriscv-mpu.h>
#include <dev_sec_csb_addendum.h>

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
#include "config/sec2-config.h"
#include "sec2ifcmn.h"


/*!
 @brief: Program MTHDCTX PRIV level mask
 */
sysKERNEL_CODE void
lsfSetupMthdctx_GH100(void)
{
    LwU32 data32;

    data32 = bar0Read(LW_PSEC_FALCON_MTHDCTX_PRIV_LEVEL_MASK);

    data32 = FLD_SET_DRF(_PSEC, _FALCON_MTHDCTX_PRIV_LEVEL_MASK,
                         _READ_PROTECTION, _ALL_LEVELS_ENABLED, data32);
    bar0Write(LW_PSEC_FALCON_MTHDCTX_PRIV_LEVEL_MASK, data32);

    // Do not RMW since unselwred entity could have setup invalid fields
    data32 = 0;
    data32 = FLD_SET_DRF(_PSEC, _FALCON_ITFEN, _MTHDEN,       _ENABLE, data32);
    data32 = FLD_SET_DRF(_PSEC, _FALCON_ITFEN, _CTXEN,        _ENABLE, data32);
    data32 = FLD_SET_DRF(_PSEC, _FALCON_ITFEN, _PRIV_POSTWR,  _INIT,   data32);

    bar0Write(LW_PSEC_FALCON_ITFEN, data32);

    // Below code is required since in RTOS environment case RISCV core will not always be idle
    data32 = 0;
    data32 = bar0Read(LW_PSEC_FALCON_DEBUG1);
    data32 =  FLD_SET_DRF_NUM(_PSEC, _FALCON_DEBUG1, _CTXSW_MODE1, 0x1, data32);
    bar0Write(LW_PSEC_FALCON_DEBUG1, data32);

    data32 = bar0Read(LW_PSEC_FALCON_DEBUG1);
}

/*!
 * @brief Initialize aperture settings (protected TRANSCFG regs)
 */
sysKERNEL_CODE void
lsfInitApertureSettings_GH100(void)
{
    LwU32 data32 = 0;

    //
    // The following setting updates only take affect if SEC2 is in LSMODE &&
    // UCODE_LEVEL > 0
    //

    // PHYS_VID mem aperture for SPA.
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_VID_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _LOCAL_FB) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // COHERENT_SYSMEM aperture for SPA.
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_SYS_COH_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)        |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _COHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // NONCOHERENT_SYSMEM aperture for SPA.
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_SYS_NCOH_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)           |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _NONCOHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // PHYS_VID mem aperture for GPA
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_GUEST_PHYS_VID_BOUND),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _LOCAL_FB) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _OWN)));

    // COHERENT_SYSMEM aperture for GPA
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_GUEST_PHYS_SYS_COH_BOUND),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)        |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _COHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _OWN)));

    // NONCOHERENT_SYSMEM aperture for GPA
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_GUEST_PHYS_SYS_NCOH_BOUND),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)           |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _NONCOHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _OWN)));

    // Generic VIRTUAL aperture
    csbWrite(LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_VIRT),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL)));

    // Setup REGIONCFG
    data32 = csbRead(LW_CSEC_FBIF_REGIONCFG);
    if (DRF_IDX_VAL( _CSEC, _FBIF_REGIONCFG, _T, RM_SEC2_DMAIDX_UCODE,
                     data32) == LSF_WPR_EXPECTED_REGION_ID)
    {
        //
        // SEC2 IS secure.
        // Clear rest of REGIONCFG to INIT value (0)
        //
        data32 = data32 & FLD_IDX_SET_DRF_NUM( _CSEC, _FBIF_REGIONCFG, _T,
                                               RM_SEC2_DMAIDX_UCODE,
                                               LSF_WPR_EXPECTED_REGION_ID, 0x0);
        csbWrite(LW_CSEC_FBIF_REGIONCFG, data32);
    }
}
