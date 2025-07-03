/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_helper_functions.c
 */

//
// Includes
//
#include "acr.h"
#include "acr_objacr.h"

RM_FLCN_ACR_DESC    g_desc          ATTR_OVLY(".data")         ATTR_ALIGNED(ACR_DESC_ALIGN);

/*
 * This function will return true if falcon with passed falconId is supported.
 */
LwBool
acrIsFalconSupported(LwU32 falconId)
{
//TODO add support for checking if falcon is present from hardware.
    switch(falconId)
    {
        case LSF_FALCON_ID_PMU_RISCV:
#ifdef SEC2_PRESENT
        case LSF_FALCON_ID_SEC2:
        case LSF_FALCON_ID_SEC2_RISCV:
#endif
        case LSF_FALCON_ID_FECS:
        case LSF_FALCON_ID_GPCCS:
#ifdef LWDEC_PRESENT
        case LSF_FALCON_ID_LWDEC:
#endif
#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC0:
        case LSF_FALCON_ID_LWENC1:
        case LSF_FALCON_ID_LWENC2:
#endif
        case LSF_FALCON_ID_GSP_RISCV:
#ifdef FBFALCON_PRESENT
        case LSF_FALCON_ID_FBFALCON:
#endif

#ifdef LWDEC1_PRESENT
        case LSF_FALCON_ID_LWDEC1:
#endif

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
#endif

#ifdef OFA_PRESENT
        case LSF_FALCON_ID_OFA:
#endif
#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
#endif
            return LW_TRUE;
        default:
            return LW_FALSE;
    }
}

/*!
 * acrMemcmp
 *
 * @brief    Mimics standard memcmp for unaligned memory
 *
 * @param[in] pSrc1   Source1
 * @param[in] pSrc2   Source2
 * @param[in] size    Number of bytes to be compared
 *
 * @return ZERO if equal,
 *         0xff if argument passed is NULL or size argument is passed as zero
 *         non-zero value if values mismatch
 */
LwU8 acrMemcmp(const void *pS1, const void *pS2, LwU32 size)
{
    const LwU8 *pSrc1        = (const LwU8 *)pS1;
    const LwU8 *pSrc2        = (const LwU8 *)pS2;
    LwU8       retVal        = 0;

    if ((pSrc1 == NULL) || (pSrc2 == NULL) || (0 == size))
    {
        //
        // Return a non-zero value
        // WARNING: Note that this does not match with typical memcmp expectations
        //
        return 0xff;
    }

    do
    {
        //
        // If any mismatch is hit, the retVal will contain a non-zero value
        // at the end of the loop. Regardless of where we find the mismatch,
        // we must still continue comparing the entire buffer and return failure at
        // the end, otherwise in cases where signature is being comared,
        // an attacker can measure time taken to return failure and hence
        // identify which byte was mismatching, then try again with a new
        // value for that byte, and eventually find the right value of
        // that byte and so on continue to hunt down the complete value of signature.
        //
        // More details are here:
        //     https://security.stackexchange.com/a/74552
        //     http://beta.ivc.no/wiki/index.php/Xbox_360_Timing_Attack
        //     https://confluence.lwpu.com/display/GFS/Security+Metrics See Defensive strategies
        //
        retVal |= (LwU8)(*(pSrc1++) ^ *(pSrc2++));
        size--;
    } while(size);

    return retVal;
}

