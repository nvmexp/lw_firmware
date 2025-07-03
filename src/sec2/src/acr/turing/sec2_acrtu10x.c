/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_acrtu10x.c
 * @brief  Check if GC6 needs to be supported and exited by ACR task.
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_os.h"
#include "sec2_hs.h"
#include "lwos_dma_hs.h"
#include "lwosselwreovly.h"
#include "lwuproc.h"
/* ------------------------ Application includes --------------------------- */
#include "sec2_objsec2.h"
#include "acr.h"
#include "lib_lw64.h"
#include "sec2_objacr.h"
#include "config/g_acr_private.h"
#include "config/g_sec2_hal.h"
#include "dev_fb.h"

/* ------------------------ Macros ------------------------------ */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
//
// Setting default alignment to 128KB
// TODO:Replace this with manual define when its available
//
#define LW_ACR_DEFAULT_ALIGNMENT                (0x20000)

extern void _lw64Add32(LwU64 *, LwU32);
extern FLCN_STATUS _acrGetRegionAndBootFalconWithHaltHs(LwU32 falconId, LwU32 falconInstance, LwU32 falconIndexMask, LwU32 flags, LwBool bUseVA,
                                                        PRM_FLCN_U64 pWprBaseVirtual, LwBool bHalt, FLCN_STATUS *pAcrGatherWprRegionDetailsStatus);
extern FLCN_STATUS _acrGatherWprRegionDetails(void);
extern FLCN_STATUS _acrBootstrapFalcon(LwU32 falconId, LwU32 falconInstance, LwU32 flags, LwBool bUseVA, PRM_FLCN_U64 pWprBaseVirtual);
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

void
acrCheckSupportAndExitGc6ByAcrTask_TU10X
(
   FLCN_STATUS *pAcrGatherWprRegionDetailsStatus
)
{
    // Get PGC6 Island Interrupt status
    LwBool bGc6Exit       = acrGetGpuGc6ExitStatus_HAL();
    LwBool bIsBsiRamValid = acrIsBsiRamValid_HAL();
    FLCN_STATUS status;

    //
    // Boot PMU on GC6 exit w/o RM sending command
    // For IFR driven path,
    // follow cold boot style by allowing RM sending command
    //
    if (bGc6Exit && bIsBsiRamValid)
    {
        LwU32 flags = 0;

        // Load , execute and detach VPR HS overlay
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(acr));
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));

        OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(acr), NULL, 0, HASH_SAVING_DISABLE);

        //
        // All the below functions are HS.
        // Not using the status here as this falcon bootstrapping is done w/o any request from RM.
        // This is just to boot PMU to allow GC6 exit work fine.
        //
        status =  _acrGetRegionAndBootFalconWithHaltHs(LSF_FALCON_ID_PMU, LSF_FALCON_INSTANCE_DEFAULT_0,
                                                       LSF_FALCON_INDEX_MASK_DEFAULT_0, flags, LW_FALSE,
                                                       NULL, LW_TRUE, pAcrGatherWprRegionDetailsStatus);

        OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(acr));

        // Trigger next phase for SCI controller
        acrTriggerNextSCIPhase_HAL();
    }
}

/*!
 * @brief acrGetRegionDetails_TU10X
 *          returns ACR region details
 *
 * @param [in]  regionID ACR region ID
 * @param [out] pStart   Start address of ACR region
 * @param [out] pEnd     End address of ACR region
 * @param [out] pRmask   Read permission of ACR region
 * @param [out] pWmask   Write permission of ACR region
 *
 * @return      FLCN_OK : If proper register details are found
 *              FLCN_ERR_ILWALID_ARGUMENT : invalid Region ID
 */
FLCN_STATUS
acrGetRegionDetails_TU10X
(
    LwU32      regionID,
    LwU64      *pStart,
    LwU64      *pEnd,
    LwU16      *pRmask,
    LwU16      *pWmask
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32      numAcrRegions;
    LwU32      regVal;

    numAcrRegions = (LW_PFB_PRI_MMU_WPR_ALLOW_READ_WPR__SIZE_1 - 1);

    // Region Id 0 is unprotected FB, hence will lead to failues.
    if ((regionID == 0) || (regionID > numAcrRegions))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    if (!pStart || !pEnd)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if ((regionID - 1) == ACR_WPR1_REGION_IDX)
    {
        // Read start address for WPR1
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR1_ADDR_LO, &regVal));
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, regVal);
        *pStart = (LwU64)regVal << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT;

        // Read end address for WPR1
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR1_ADDR_HI, &regVal));
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, regVal);
        *pEnd = (LwU64)regVal << LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT;
    }
    else if ((regionID - 1) == ACR_WPR2_REGION_IDX)
    {
        // Read start address for WPR2
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR2_ADDR_LO, &regVal));
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, regVal);
        *pStart = (LwU64)regVal << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;

        // Read end address for WPR2
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR2_ADDR_HI, &regVal));
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, regVal);
        *pEnd = (LwU64)regVal << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;
    }
    else
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check for a valid ACR region
    if (*pStart <= *pEnd)
    {
        // End address always point to start of the last aligned address
        _lw64Add32(pEnd, LW_ACR_DEFAULT_ALIGNMENT);
    }
    else
    {
        // Not a valid ACR region.
        flcnStatus = FLCN_ERROR;
        goto ErrorExit;

    }

    // Read ReadMask
    if (pRmask != NULL)
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_ALLOW_READ, &regVal));
        *pRmask = DRF_IDX_VAL(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR, regionID, regVal);
    }

    // Read WriteMask
    if (pWmask != NULL)
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, &regVal));
        *pWmask = DRF_IDX_VAL(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR, regionID, regVal);
    }
    
ErrorExit:
    if (flcnStatus != FLCN_OK)
    {
        // Ilwalidate start and end addresses
        *pStart = 0;
        *pEnd   = 0;
    }
    return flcnStatus;
}

