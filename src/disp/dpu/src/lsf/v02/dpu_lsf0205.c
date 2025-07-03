/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_lsf0205.c
 * @brief  Light Secure Falcon (LSF) v02.05 Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "rmlsfm.h"
#include "dpu_task.h"
#include "dispflcn_regmacros.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu_objlsf.h"
#include "dpu_objdpu.h"
#include "dpu/dpuifcmn.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */

/*!
 * Address defined in the linker script to mark the memory shared between DPU
 * and RM.  Specifically, this marks the starting offset for the chunk of DMEM
 * that is writeable by the RM.
 */
extern LwU32       _rm_share_start[];

/*!
 * Address defined in the linker script to mark the end of the DPU DMEM.
 * This offset combined with @ref _rm_share_start define the size of the
 * RM-writeable portion of the DMEM.
 */
extern LwU32       _end_physical_dmem[];

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */


/*!
 * @brief Initializes Light Secure Falcon settings.
 *
 * @returns     FLCN_STATUS   FLCN_OK always.
 */
FLCN_STATUS
lsfInit_v02_05(void)
{
    // Ensure ICD debugability
    lsfEnableIcdDebugging_HAL(&LsfHal);

    // Before we change anything, ensure secure.
    if (!lsfVerifyFalconSelwreRunMode_HAL(&LsfHal))
    {
         // 
         // Ignore other LSF setup if we are not expected to run at secure mode
         // Example: FPGA which is running in NS mode, thus no ACR support
         // 
         return FLCN_OK;
    }

    // Lock down the reset register
    lsfRaiseResetPrivLevelMask_HAL(&LsfHal);

    // Setup DMEM carveout regions
    lsfInitDmemCarveoutRegions_HAL(&LsfHal);

    // Setup aperture settings (protected TRANSCONFIG regs)
    lsfInitApertureSettings_HAL(&LsfHal);

    return FLCN_OK;
}

//
// GSP-Lite doesn't have register to configure DMEM carveout regisons
// and uses EMEM to support open carveout region instead. In order to
// reuse the functions in this file, we have to exclude this function
// from compiling.
//
#if (DPU_PROFILE_v0205) || (DPU_PROFILE_v0207)
/*!
 * @brief Initializes DMEM carveout regions.
 */
void
lsfInitDmemCarveoutRegions_v02_05(void)
{
    //
    // We need to open up 3 specific sections of DMEM to RM access:
    // 1. Command queues
    // 2. Message queues
    // 3. DMEM heap
    //
    // Lwrrently, the 3 sections are grouped together like so:
    //  _rm_share_start
    //      _RM_DPU_CmdQueue
    //      _RM_DPU_MsgQueue
    //      RM DMEM heap
    //  _end_physical_dmem
    //
    // We will use a single range control (RANGE0) to expose all 3.
    //

    //
    // Our carveout range controls have DMEM BLOCK (256 byte) resolution, so
    // to correctly expose _only_ these sections, _rm_share_start and
    // _end_physical_dmem both need to be block aligned.  HALT if it is not.
    //
    if (!VAL_IS_ALIGNED((LwU32)_rm_share_start, (LwU32)256))
    {
        DPU_HALT();
    }
    if (!VAL_IS_ALIGNED((LwU32)_end_physical_dmem, (LwU32)256))
    {
        DPU_HALT();
    }

    // Everything appears to be aligned, proceed to open up the range.
    DFLCN_REG_WR32(DMEM_PRIV_RANGE0,
             (DFLCN_DRF_NUM(DMEM_PRIV_RANGE0, _START_BLOCK,
                      (LwU16)(((LwU32)_rm_share_start)    >> 8)) |
              DFLCN_DRF_NUM(DMEM_PRIV_RANGE0, _END_BLOCK,
                      (LwU16)(((LwU32)_end_physical_dmem) >> 8))));

    //
    // (Bug 1884327) Enable level 3 protection on DMEM to avoid sensitive data
    // leaking. Note that DMEM_PRIV_RANGE0/1 registers are also protected by
    // DMEM PLM, so we can't raise DMEM PLM before DMEM carveout is configured.
    //
    dpuRaiseDmemPlmToLevel3_HAL(&Dpu.hal);
}
#endif

/*!
 * @brief Verifies this Falcon is running in secure mode.
 */
LwBool
lsfVerifyFalconSelwreRunMode_v02_05(void)
{
    LwU32 data32;

    // Verify that we are in LSMODE
    data32 = DFLCN_REG_RD32(SCTL);
    if (DFLCN_FLD_TEST_DRF(SCTL, _LSMODE, _TRUE, data32))
    {
        // Verify that UCODE_LEVEL > the init value
        if (DFLCN_DRF_VAL(SCTL, _UCODE_LEVEL,  data32) > DFLCN_DRF(SCTL_UCODE_LEVEL_INIT))
        {
            //
            // Verify that the FBIF REGIONCTL for TRANSCFG(0) (ucode) ==
            // LSF_WPR_EXPECTED_REGION_ID
            //
            data32 = DFBIF_REG_RD32(REGIONCFG);
            if (DFBIF_DRF_IDX_VAL(REGIONCFG, _T, RM_DPU_DMAIDX_UCODE,
                            data32) == LSF_WPR_EXPECTED_REGION_ID)
            {
                //
                // Falcon (DPU) IS secure.
                // Clear rest of REGIONCFG to INIT value (0)
                //
                data32 = data32 &
                         DFBIF_FLD_IDX_SET_DRF_NUM(REGIONCFG, _T,
                                             RM_DPU_DMAIDX_UCODE,
                                             LSF_WPR_EXPECTED_REGION_ID, 0x0);
                DFBIF_REG_WR32(REGIONCFG, data32);

                return LW_TRUE;
            }
        }
    }

    // Falcon (DPU) IS NOT secure.

    // Write a falcon mode token to signal the inselwre condition.
    DFLCN_REG_WR32(MAILBOX0, LSF_FALCON_MODE_TOKEN_FLCN_INSELWRE);

    // If we should be in secure mode, halt.
    if (DpuInitArgs.selwreMode)
    {
        DPU_HALT();
    }

    return LW_FALSE;
}

/*!
 * @brief Setup aperture settings (protected TRANSCONFIG regs)
 */
void
lsfInitApertureSettings_v02_05(void)
{
    //
    // The following setting updates only take affect if the
    // the DPU is LSMODE && UCODE_LEVEL > 0
    //

    // UCODE aperture
    DFBIF_REG_WR32(TRANSCFG(RM_DPU_DMAIDX_UCODE),
             (DFBIF_DRF_DEF(TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DFBIF_DRF_DEF(TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic PHYS_VID aperture
    DFBIF_REG_WR32(TRANSCFG(RM_DPU_DMAIDX_PHYS_VID),
             (DFBIF_DRF_DEF(TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DFBIF_DRF_DEF(TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic COHERENT_SYSMEM aperture
    DFBIF_REG_WR32(TRANSCFG(RM_DPU_DMAIDX_PHYS_SYS_COH),
             (DFBIF_DRF_DEF(TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DFBIF_DRF_DEF(TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    DFBIF_REG_WR32(TRANSCFG(RM_DPU_DMAIDX_PHYS_SYS_NCOH),
             (DFBIF_DRF_DEF(TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DFBIF_DRF_DEF(TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));
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
lsfIsDmaIdxPermitted_v02_05
(
    LwU8 dmaIdx
)
{
    LwU32 val = DFBIF_REG_RD32(REGIONCFG);

    return (DFBIF_DRF_IDX_VAL(REGIONCFG, _T, dmaIdx, val) ==
            LSF_UNPROTECTED_REGION_ID);
}

/*!
 * @brief Enable ICD Debugging.
 */
void
lsfEnableIcdDebugging_v02_05(void)
{
    DFLCN_REG_WR32(DBGCTL, 0x3FF);
}
