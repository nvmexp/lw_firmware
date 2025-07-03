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
 * @file: booter_sub_wpr_tu10x.c
 */
//
// Includes
//
#include "booter.h"


// TODO-derekw
//
// Use GSP sub-WPR 0 for GSP-RM (WprMeta + GSP-RM only, no FRTS)
// Use SEC sub-WPR 6 for Booter Load (WprMeta + GSP-RM only, no FRTS -- can be shrunk to WprMeta only before handing off Booter Unload)
// Use LWDEC sub-WPR 3 for Booter Reload (only WprMeta)
//

static BOOTER_STATUS s_booterGetSubWprAddrVals(const LwBool, const LwU64, const LwU64, LwU32 *, LwU32 *) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS
s_booterGetSubWprAddrVals(const LwBool bValid, const LwU64 startAddr, const LwU64 endAddr, LwU32 *pAddrLoVal, LwU32 *pAddrHiVal)
{
    if (bValid)
    {
        // Sub-WPRs are aligned to 4KiB
        if (!(LW_IS_ALIGNED(startAddr, (1u << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT)) &&
              LW_IS_ALIGNED(endAddr, (1u << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT))))
        {
            return BOOTER_ERROR_UNKNOWN;
        }

        *pAddrLoVal = (startAddr >> LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);
        *pAddrHiVal = ((endAddr - 1) >> LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT);
    }
    else
    {
        *pAddrLoVal = ILWALID_WPR_ADDR_LO;
        *pAddrHiVal = ILWALID_WPR_ADDR_HI;
    }

    return BOOTER_OK;
}

BOOTER_STATUS
booterProgramSubWprGsp_TU10X(LwU32 subWprId, LwBool bValid, LwU64 startAddr, LwU64 endAddr, LwU32 readMask, LwU32 writeMask)
{
    BOOTER_STATUS status;
    LwU32 addrLoVal;
    LwU32 addrHiVal;
    LwU32 reg;

    // GSP only has 4 sub-WPRs
    if (subWprId >= LW_PFB_PRI_MMU_FALCON_GSP_CFGA__SIZE_1)
    {
        return BOOTER_ERROR_UNKNOWN;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(s_booterGetSubWprAddrVals(bValid, startAddr, endAddr, &addrLoVal, &addrHiVal));

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_GSP_CFGA, _ALLOW_READ, readMask, reg);
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_GSP_CFGA, _ADDR_LO, addrLoVal, reg);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_GSP_CFGA(subWprId), reg);

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_GSP_CFGB, _ALLOW_WRITE, writeMask, reg);
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_GSP_CFGB, _ADDR_HI, addrHiVal, reg);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_GSP_CFGB(subWprId), reg);

    return status;
}

BOOTER_STATUS
booterProgramSubWprSec_TU10X(LwU32 subWprId, LwBool bValid, LwU64 startAddr, LwU64 endAddr, LwU32 readMask, LwU32 writeMask)
{
    BOOTER_STATUS status;
    LwU32 addrLoVal;
    LwU32 addrHiVal;
    LwU32 reg;

    // SEC only has 8 sub-WPRs
    if (subWprId >= LW_PFB_PRI_MMU_FALCON_SEC_CFGA__SIZE_1)
    {
        return BOOTER_ERROR_UNKNOWN;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(s_booterGetSubWprAddrVals(bValid, startAddr, endAddr, &addrLoVal, &addrHiVal));

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ALLOW_READ, readMask, reg);
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ADDR_LO, addrLoVal, reg);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_SEC_CFGA(subWprId), reg);

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_SEC_CFGB, _ALLOW_WRITE, writeMask, reg);
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_SEC_CFGB, _ADDR_HI, addrHiVal, reg);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_SEC_CFGB(subWprId), reg);

    return status;
}

BOOTER_STATUS
booterProgramSubWprLwdec_TU10X(LwU32 subWprId, LwBool bValid, LwU64 startAddr, LwU64 endAddr, LwU32 readMask, LwU32 writeMask)
{
    BOOTER_STATUS status;
    LwU32 addrLoVal;
    LwU32 addrHiVal;
    LwU32 reg;

    // LWDEC only has 4 sub-WPRs
    if (subWprId >= LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA__SIZE_1)
    {
        return BOOTER_ERROR_UNKNOWN;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(s_booterGetSubWprAddrVals(bValid, startAddr, endAddr, &addrLoVal, &addrHiVal));

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC0_CFGA, _ALLOW_READ, readMask, reg);
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC0_CFGA, _ADDR_LO, addrLoVal, reg);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(subWprId), reg);

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC0_CFGB, _ALLOW_WRITE, writeMask, reg);
    reg = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC0_CFGB, _ADDR_HI, addrHiVal, reg);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGB(subWprId), reg);

    return status;
}