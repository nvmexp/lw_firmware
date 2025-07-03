/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   gfe_hs.c
 * @brief  HS routines in GFE task
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_hs.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objgfe.h"
#include "gfe/gfe_mthds.h"
#include "gfe/gfe_key.h"
#include "scp_crypt.h"
#include "scp_rand.h"
#include "mem_hs.h"
#include "sha256_hs.h"
#include "dev_sec_csb.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */

/* Buffer for the salt */
static LwU8 _saltVal[SCP_AES_SIZE_IN_BYTES]
    GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_gfe", "_saltVal");

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Function to do encryption/decryption using the HW secret
 *
 * @param [in]  bEncrypt    To do encryption or decryption
 * @param [in]  pSrc        Pointer to the input buffer
 * @param [in]  size        Size to be encrypted/decrypted
 * @param [out] pDest       Pointer to the output buffer
 * @param [in]  pIvBuf      Buffer holding the IV to be used
 */
FLCN_STATUS
gfeCryptEntry
(
    LwBool bEncrypt,
    LwU8  *pSrc,
    LwU32  size,
    LwU8  *pDest,
    LwU8  *pIvBuf
)
{
    //
    // Check return PC to make sure that it is not in HS
    // This must always be the first statement in HS entry function
    //
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    FLCN_STATUS status = FLCN_OK;
    LwU32 *pSalt = (LwU32 *)_saltVal;
    LwU32 i = 0;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        goto gfeCryptEntry_exit;
    }

#ifndef GFE_GEN_MODE
    //
    // Permit encryption only for GFE_GEN_MODE
    // In general, we should be cautious in letting LS code use both encrypt
    // and decrypt operations since that permits LS code to do just about
    // anything with the key.
    //
    if (bEncrypt)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto gfeCryptEntry_exit;
    }
#endif

    // Check that the inputs look ok
    if ((pSrc == NULL) || (pDest == NULL) || (pIvBuf == NULL) ||
        (size == 0) || ((size % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pSrc % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pDest % SCP_BUF_ALIGNMENT) != 0) ||
        ((((LwU32)pIvBuf) % SCP_BUF_ALIGNMENT) != 0))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto gfeCryptEntry_exit;
    }

    OS_SEC_HS_LIB_VALIDATE(libScpCryptHs, HASH_SAVING_DISABLE);

    //
    // Build a salt which when encrypted using SCP secret key will lead to the
    // derived key used for GFE symmetric key operations
    //
    pSalt[i++] = LW_SCP_SECRET_KDF_SALT_GFE_PART_1;
    pSalt[i++] = LW_SCP_SECRET_KDF_SALT_GFE_PART_2;
    pSalt[i++] = LW_SCP_SECRET_KDF_SALT_GFE_PART_3;
    pSalt[i++] = LW_SCP_SECRET_KDF_SALT_GFE_PART_4;
    //
    // Apply app version to salt to ensure we get a new key each time app
    // version changes. This ensures that in case of app revocation, we
    // do not use the old symmetric GFE derived key
    //
    pSalt[0]  ^= GFE_APP_VERSION;

    //
    // Encrypt the salt with SCP secret key to generate the derived key and
    // encrypt / decrypt the input buffer with that derived key using AES-CBC
    // algo which uses pIvBuf as starting IV
    //
    status = scpCbcCrypt((LwU8 *)pSalt, bEncrypt, LW_FALSE, LW_SCP_SECRET_IDX_GFE_KEK,
                         pSrc, size, pDest, pIvBuf);

gfeCryptEntry_exit:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return status;
}
