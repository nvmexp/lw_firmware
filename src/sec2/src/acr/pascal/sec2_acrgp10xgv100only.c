/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_acrgp10xgv100only.c
 * @brief  Get WPR and VPR region details by reading MMU registers.
 *         This file will compile only till VOLTA.
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_objsec2.h"
#include "acr.h"
#include "sec2_objacr.h"
#include "sec2_bar0_hs.h"
#include "lib_lw64.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "config/g_acr_private.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Setting default alignment to 128KB
 * TODO:Replace this with manual define when its available
 */
#define LW_ACR_DEFAULT_ALIGNMENT 0x20000

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern void _lw64Add32(LwU64 *, LwU32); 
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
static FLCN_STATUS _acrReadMmuWprInfoRegister_GP10X(LwU32 index, LwU32*)
    GCC_ATTRIB_SECTION("imem_acr", "_acrReadMmuWprInfoRegister_GP10X");

/*!
 * @brief acrGetRegionDetails_GP10X
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
acrGetRegionDetails_GP10X
(
    LwU32      regionID,
    LwU64      *pStart,
    LwU64      *pEnd,
    LwU16      *pRmask,
    LwU16      *pWmask
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32      index       = 0;
    LwU32      numAcrRegions;
    LwU32      regVal;
    LWU64_TYPE uStart;
    LWU64_TYPE uEnd;

    numAcrRegions = (LW_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_READ_WPR__SIZE_1 - 1);

    // Region Id 0 is unprotected FB, hence will lead to failues.
    if ((regionID == 0) || (regionID > numAcrRegions))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (pStart != NULL)
    {
        // Read start address
        index = LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR_ADDR_LO(regionID);
        CHECK_FLCN_STATUS(_acrReadMmuWprInfoRegister_GP10X(index, &regVal));

        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, regVal);

        LWU64_HI(uStart) = regVal >> (32 - LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT);
        LWU64_LO(uStart) = regVal << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

        *pStart = uStart.val;
    }

    if (pEnd != NULL)
    {
        // Read end address
        index = LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR_ADDR_HI(regionID);
        CHECK_FLCN_STATUS(_acrReadMmuWprInfoRegister_GP10X(index, &regVal));

        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, regVal);

        LWU64_HI(uEnd) = regVal >> (32 - LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT);
        LWU64_LO(uEnd) = regVal << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
        //
        // End address as read from the register always points to start of
        // the last aligned address. Add the alignment to make it point to
        // the first byte after the end.
        //
        *pEnd = uEnd.val;

        _lw64Add32(pEnd, LW_ACR_DEFAULT_ALIGNMENT);
    }

    // Read ReadMask
    if (pRmask != NULL)
    {
        index = LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_READ;
        CHECK_FLCN_STATUS(_acrReadMmuWprInfoRegister_GP10X(index, &regVal));

        *pRmask = DRF_IDX_VAL(_PFB, _PRI_MMU_WPR_INFO, _ALLOW_READ_WPR, regionID, regVal);
    }

    if (pWmask != NULL)
    {
        // Read WriteMask
        index = FLD_SET_DRF(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _ALLOW_WRITE, 0);
        CHECK_FLCN_STATUS(_acrReadMmuWprInfoRegister_GP10X(index, &regVal));

        *pWmask = DRF_IDX_VAL(_PFB, _PRI_MMU_WPR_INFO, _ALLOW_WRITE_WPR, regionID, regVal);
    }

ErrorExit:
    return flcnStatus;
}

static FLCN_STATUS
_acrReadMmuWprInfoRegister_GP10X
(
    LwU32 index,
    LwU32 *val
)
{
    FLCN_STATUS flcnStatus;
    LwU32 localIndex;

    do
    {
        localIndex = DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _INDEX, index);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_INFO, localIndex));

        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_INFO, val));

    //
    // If the index field has changed, some other thread wrote into
    // the register, and the register value is not what we expect.
    // Try again if the index is changed.
    //
    } while (DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _INDEX, (LwU32) *val) != index);

ErrorExit:
    return flcnStatus;
}

