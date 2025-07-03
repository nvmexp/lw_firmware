/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_lsfgm20x.c
 * @brief  Light Secure Falcon (LSF) GM20x HAL Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"
#include "rmlsfm.h"
#include "main.h"
#include "dev_sec_csb.h"
#include "dev_sec_csb_addendum.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_objlsf.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Verifies this Falcon is running in secure mode.
 */
LwBool
lsfVerifyFalconSelwreRunMode_GM20X(void)
{
    LwU32 data32;

    // Verify that we are in LSMODE
    data32 = REG_RD32(CSB, LW_CSEC_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSEC, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        // Verify that UCODE_LEVEL > the init value
        if (DRF_VAL(_CSEC, _FALCON_SCTL, _UCODE_LEVEL,  data32) >
            LW_CSEC_FALCON_SCTL_UCODE_LEVEL_INIT)
        {
            //
            // Verify that the FBIF REGIONCTL for TRANSCFG(ucode) ==
            // LSF_WPR_EXPECTED_REGION_ID
            //
            data32 = REG_RD32(CSB, LW_CSEC_FBIF_REGIONCFG);
            if (DRF_IDX_VAL(_CSEC, _FBIF_REGIONCFG, _T, RM_SEC2_DMAIDX_UCODE,
                            data32) == LSF_WPR_EXPECTED_REGION_ID)
            {
                //
                // Falcon (SEC2) IS secure.
                // Clear rest of REGIONCFG to INIT value (0)
                //
                data32 = data32 &
                    FLD_IDX_SET_DRF_NUM(_CSEC, _FBIF_REGIONCFG, _T,
                                        RM_SEC2_DMAIDX_UCODE,
                                        LSF_WPR_EXPECTED_REGION_ID, 0x0);
                REG_WR32(CSB, LW_CSEC_FBIF_REGIONCFG, data32);

                return LW_TRUE;
            }
        }
    }

    // Falcon (SEC2) IS NOT secure.

    // Write a falcon mode token to signal the inselwre condition.
    REG_WR32(CSB, LW_CSEC_FALCON_MAILBOX0,
             LSF_FALCON_MODE_TOKEN_FLCN_INSELWRE);

    return LW_FALSE;
}

/*!
 * @brief Initialize aperture settings (protected TRANSCFG regs)
 */
void
lsfInitApertureSettings_GM20X(void)
{
    //
    // The following setting updates only take affect if SEC2 is in LSMODE &&
    // UCODE_LEVEL > 0
    //

    // Generic PHYS_VID mem aperture
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_VID_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic COHERENT_SYSMEM aperture
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_SYS_COH_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_PHYS_SYS_NCOH_FN0),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CSEC, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));

    // Generic VIRTUAL aperture
    REG_WR32(CSB, LW_CSEC_FBIF_TRANSCFG(RM_SEC2_DMAIDX_VIRT),
             (DRF_DEF(_CSEC, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL)));
}

/*!
 * Checks whether an access using the specified dmaIdx is permitted.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     LwBool    LW_TRUE  if the access is permitted
 *                       LW_FALSE if the access is not permitted
 */
LwBool
lsfIsDmaIdxPermitted_GM20X
(
    LwU8 dmaIdx
)
{
    LwU32 val = REG_RD32(CSB, LW_CSEC_FBIF_REGIONCFG);

    return (DRF_IDX_VAL(_CSEC, _FBIF_REGIONCFG, _T, dmaIdx, val) ==
            LSF_UNPROTECTED_REGION_ID);
}
