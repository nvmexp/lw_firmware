/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_HS_H
#define SEC2_HS_H


/*!
 * @file sec2_hs.h
 * This file holds the inline definitions of functions used for revocation
 * support for HS overlays
 */

/* ------------------------- System includes -------------------------------- */
#include "dev_fuse.h"
#include "dev_pwr_pri.h"
#include "dev_lwdec_pri.h"
#include "dev_master.h"
#include "dev_sec_csb_addendum.h"
#include "dev_falcon_v4_addendum.h"

/* ------------------------- Application includes --------------------------- */
#include "config/g_profile.h"

#include "rmlsfm.h"
#include "sec2_objsec2.h"
#include "sec2_bar0_hs.h"
#include "sec2_selwrebus_hs.h"
#include "lwosselwreovly.h"

/* ------------------------- Types definitions ------------------------------ */
/* ------------------------- External definitions --------------------------- */
/* ------------------------- Static variables ------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
#define VALIDATE_RETURN_PC_AND_HALT_IF_HS()  do {                       \
        if(_sec2EnsureReturnPCIsNotInHS() != FLCN_OK)                   \
        {                                                               \
            REG_WR32(CSB, LW_CSEC_FALCON_MAILBOX0,                      \
                     FLCN_ERR_HS_CHK_RETURN_PC_AT_HS_ENTRY_IS_OF_HS);   \
            falc_halt();                                                \
        }                                                               \
        }                                                               \
        while (LW_FALSE)

/* ------------------------- Function Prototypes ---------------------------- */
static inline FLCN_STATUS _sec2CheckPmbPLM_GP10X(LwBool bSkipDmemLv2Chk)
    GCC_ATTRIB_ALWAYSINLINE();

static inline void _sec2IsLsOrHsModeSet_GP10X(LwBool *pBLsModeSet, LwBool *pBHsModeSet)
    GCC_ATTRIB_ALWAYSINLINE();

static inline FLCN_STATUS _sec2EnsureReturnPCIsNotInHS(void)
    GCC_ATTRIB_ALWAYSINLINE();

static inline void clearSCPregisters(void)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * @brief clear all the SCP registers
 */
static inline void clearSCPregisters(void)
{
    // Clear SCP registers
    falc_scp_secret(0, SCP_R0);
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_secret(0, SCP_R1);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_secret(0, SCP_R2);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_secret(0, SCP_R3);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_secret(0, SCP_R4);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_secret(0, SCP_R5);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_secret(0, SCP_R6);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_secret(0, SCP_R7);
    falc_scp_xor(SCP_R7, SCP_R7);
}

/*!
 * @brief  Ensure that the PLM of IMEM and DMEM meet the security requirement
 *
 * @param[in]  bSkipDmemLv2Chk   Flag indicates whether we want to skip the lv2
 *                               PLM check for some special condition (e.g.
 *                               LASSAHS mode in PR task)
 *
 * @return FLCN_OK  if the PLM already meets the requirement
 *         FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION  otherwise
 */
static inline FLCN_STATUS
_sec2CheckPmbPLM_GP10X
(
    LwBool bSkipDmemLv2Chk
)
{
    LwU32 dmemPrivLevel;
    LwU32 imemPrivLevel;

    dmemPrivLevel = REG_RD32(CSB, LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK);
    imemPrivLevel = REG_RD32(CSB, LW_CSEC_FALCON_IMEM_PRIV_LEVEL_MASK);

    // TODO: Check for LEVEL1 once acrlib is moved to SEC2

    if (FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _ENABLE, dmemPrivLevel) ||
        (!bSkipDmemLv2Chk && FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2, _ENABLE, dmemPrivLevel)))
    {
        return FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
    }

    if (FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, dmemPrivLevel) ||
        (!bSkipDmemLv2Chk && FLD_TEST_DRF(_CSEC, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, dmemPrivLevel)))
    {
        return FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
    }

    if (FLD_TEST_DRF(_CSEC, _FALCON_IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, imemPrivLevel) ||
        FLD_TEST_DRF(_CSEC, _FALCON_IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, imemPrivLevel))
    {
        return FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
    }

    return FLCN_OK;
}

/*!
 * @brief  Check if SCTL_LSMODE or SCTL_HSMODE is set
 *
 * @param[in]  pBLsModeSet   Pointer saving if SCTL_LSMODE is set
 * @param[in]  pBHsModeSet   Pointer saving if SCTL_HSMODE is set
 */
static inline void
_sec2IsLsOrHsModeSet_GP10X
(
    LwBool *pBLsModeSet,
    LwBool *pBHsModeSet
)
{
    LwU32 data32 = REG_RD32_STALL(CSB, LW_CSEC_FALCON_SCTL);

    if (pBLsModeSet != NULL)
    {
        *pBLsModeSet = FLD_TEST_DRF(_CSEC, _FALCON_SCTL, _LSMODE, _TRUE, data32);
    }

    if (pBHsModeSet != NULL)
    {
        *pBHsModeSet = FLD_TEST_DRF(_CSEC, _FALCON_SCTL, _HSMODE, _TRUE, data32);
    }
}

/*!
 * @brief  Check if return PC is in HS or not.
 *         This function must be called as first statement in HS entry functions.
 *         This is to prevent ROP attacks where LS/NS code calling HSEntry can modify return PC
 *         so that it points to another HS overlay rather than pointing to caller.
 *
 * @return FLCN_OK  if return PC is not in HS
 *         FLCN_ERR_HS_CHK_RETURN_PC_AT_HS_ENTRY_IS_OF_HS if return PC is in HS
 */
static inline FLCN_STATUS
_sec2EnsureReturnPCIsNotInHS(void)
{
    LwU32 *returnPC = __builtin_return_address(0);
    LwU32 imtagVal;

    falc_imtag(&imtagVal, (LwU32)returnPC);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_IMTAG_BLK, _SELWRE, _YES, imtagVal))
    {
        // Return PC is pointing to a secure block. NS/LS must be trying to mount a ROP attack. Take punitive action.
        return FLCN_ERR_HS_CHK_RETURN_PC_AT_HS_ENTRY_IS_OF_HS;
    }

    return FLCN_OK;
}

#endif // SEC2_HS_H
