/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lsftu10x.c
 * @brief  Light Secure Falcon (LSF) TU10x HAL Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "main.h"
/* ------------------------ Application includes --------------------------- */
#include "sec2_objlsf.h"
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Initialize aperture settings (protected TRANSCFG regs)
 */
void
lsfInitApertureSettings_TU10X(void)
{
    //
    // The following setting updates only take affect if SEC2 is in LSMODE &&
    // UCODE_LEVEL > 0
    //

    // PHYS_VID mem aperture for SPA.
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_VID_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _LOCAL_FB) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // COHERENT_SYSMEM aperture for SPA.
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_SYS_COH_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)        |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _COHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // NONCOHERENT_SYSMEM aperture for SPA.
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_SYS_NCOH_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)           |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _NONCOHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // PHYS_VID mem aperture for GPA
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_GUEST_PHYS_VID_BOUND),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _LOCAL_FB) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _OWN)));

    // COHERENT_SYSMEM aperture for GPA
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_GUEST_PHYS_SYS_COH_BOUND),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)        |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _COHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _OWN)));

    // NONCOHERENT_SYSMEM aperture for GPA
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_GUEST_PHYS_SYS_NCOH_BOUND),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)           |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,         _NONCOHERENT_SYSMEM) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _OWN)));

    // Generic VIRTUAL aperture
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_VIRT),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL)));
}
