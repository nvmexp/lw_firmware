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
 * @file: booter_sanity_checks_tu10x.c
 */
//
// Includes
//
#include "booter.h"

// hack: prevent uproc/libs/lwos/dev/inc/falcon.h from trying to include falcon_spr.h
#ifdef UPROC_FALCON
#undef UPROC_FALCON
#endif
#include "falcon.h"

/*
 * This funtion will ilwalidate the bubbles (blocks not of ACR HS, caused if ACR blocks are not loaded contiguously in IMEM)
 */
void
booterValidateBlocks_TU10X(void)
{
    LwU32 start        = (BOOTER_PC_TRIM((LwU32) _booter_resident_code_start));
    LwU32 end          = (BOOTER_PC_TRIM((LwU32) _booter_resident_code_end));

    LwU32 tmp          = 0;
    LwU32 imemBlks     = 0;
    LwU32 blockInfo    = 0;
    LwU32 lwrrAddr     = 0;

    //
    // Loop through all IMEM blocks and ilwalidate those that doesnt fall within
    // acr_resident_code_start and acr_resident_code_end
    //

    // Read the total number of IMEM blocks
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    tmp = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CSEC_FALCON, _HWCFG, _IMEM_SIZE, tmp);
#elif defined(BOOTER_RELOAD)
    tmp = BOOTER_REG_RD32(CSB, LW_CLWDEC_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CLWDEC_FALCON, _HWCFG, _IMEM_SIZE, tmp);
#else
    ct_assert(0);
#endif
    for (tmp = 0; tmp < imemBlks; tmp++)
    {
        falc_imblk(&blockInfo, tmp);

        if (!(IMBLK_IS_ILWALID(blockInfo)))
        {
            lwrrAddr = IMBLK_TAG_ADDR(blockInfo);

            // Ilwalidate the block if it is not a part of ACR binary
            if (lwrrAddr < start || lwrrAddr >= end)
            {
                falc_imilw(tmp);
            }
        }
    }
}

/*
 * Verify if binary is running on expected falcon/engine. Volta onwards we have ENGID support
 */
BOOTER_STATUS
booterCheckIfEngineIsSupported_TU10X(void)
{
    //
    // Check if binary is allowed on engine/falcon, lwrrently ENGID supported only on SEC2 and GSPLite
    //
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    LwU32 engId = DRF_VAL(_CSEC, _FALCON_ENGID, _FAMILYID, BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_ENGID));
    if (engId != LW_CSEC_FALCON_ENGID_FAMILYID_SEC)
    {
        return BOOTER_ERROR_BINARY_NOT_RUNNING_ON_EXPECTED_FALCON;
    }
#elif defined(BOOTER_RELOAD)
    LwU32 engId = DRF_VAL(_CLWDEC, _FALCON_ENGID, _FAMILYID, BOOTER_REG_RD32(CSB, LW_CLWDEC_FALCON_ENGID));
    if (engId != LW_CLWDEC_FALCON_ENGID_FAMILYID_LWDEC)
    {
        return BOOTER_ERROR_BINARY_NOT_RUNNING_ON_EXPECTED_FALCON;
    }
#else
    ct_assert(0);
#endif

    return BOOTER_OK;
}

/*!
 * @brief Function that checks if priv sec is enabled on prod board
 */
BOOTER_STATUS
booterCheckIfPrivSecEnabledOnProd_TU10X(void)
{

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    if ( FLD_TEST_DRF( _CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, BOOTER_REG_RD32( CSB, LW_CSEC_SCP_CTL_STAT ) ) )
#elif defined(BOOTER_RELOAD)
    if ( FLD_TEST_DRF( _CLWDEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, BOOTER_REG_RD32( CSB, LW_CLWDEC_SCP_CTL_STAT ) ) )
#else
    ct_assert(0);
#endif
    {
        if ( FLD_TEST_DRF( _FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, BOOTER_REG_RD32( BAR0, LW_FUSE_OPT_PRIV_SEC_EN ) ) )
        {
            // Error: Priv sec is not enabled
            return BOOTER_ERROR_PRIV_SEC_NOT_ENABLED;
        }
    }
    return BOOTER_OK;
}

/*!
 * @brief Check the HW fpf version
 */
BOOTER_STATUS
booterCheckFuseRevocationAgainstHWFpfVersion_TU10X(LwU32 ucodeBuildVersion)
{
    BOOTER_STATUS status              = BOOTER_OK;
    LwU32      ucodeFpfFuseVersion = 0;

    // Read the version number from FPF FUSE
    status = booterGetUcodeFpfFuseVersion_HAL(pBooter, &ucodeFpfFuseVersion);
    if (status != BOOTER_OK)
    {
        return BOOTER_ERROR_ILWALID_UCODE_FUSE;
    }

    if (ucodeBuildVersion < ucodeFpfFuseVersion)
    {
        // Halt if this ucode is not supposed to run in this chip
        return BOOTER_ERROR_UCODE_REVOKED;
    }

    return BOOTER_OK;
}
