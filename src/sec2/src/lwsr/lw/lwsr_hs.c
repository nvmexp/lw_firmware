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
 * @file   lwsr_hs.c
 * @brief  HS routines in LWSR task
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_hs.h"
#include "dev_sec_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwsr/lwsr_mthds.h"
#include "scp_internals.h"
#include "sha256.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Index of HW secret that will be used for LWSR-related encryption and
 * decryption.
 */
#define LWSR_HW_SECRET_INDEX 5

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Decrypt the private key by SCP engine.
 *
 * @param[in]  pKeyBuf   Pointer to the key buffer to be decrypted
 *
 * @return FLCN_OK if the key is decrypted and validated successfully.
 */
FLCN_STATUS lwsrCryptEntry
(
    LwU8 *pKeyBuf
)
{
    FLCN_STATUS status;
    LwU32       index = 0;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);
    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        goto lwsrCryptEntry_exit;
    }

    OS_SEC_HS_LIB_VALIDATE(libScpCryptHs);

    // Trap the DMA instructions indefinitely
    falc_scp_trap(TC_INFINITY);

    // Load the HW secret into R1
    falc_scp_secret(LWSR_HW_SECRET_INDEX, SCP_R1);

    //
    // Colwert an AES key to the 10th round AES round key that is the form of
    // the key required for the decrypt instruction to work correctly
    //
    falc_scp_rkey10(SCP_R1, SCP_R2);

    // Use the key in R2 for decryption
    falc_scp_key(SCP_R2);

    for (index = 0; index < (LWSR_PLAIN_PRIV_KEY_SIZE + SHA256_HASH_BLOCK);
         index += 16)
    {
        // Write the encrypted key into R3
        falc_trapped_dmwrite(falc_sethi_i((LwU32)(&(pKeyBuf[index])), SCP_R3));
        falc_dmwait();

        // Decrypt the key into R4
        falc_scp_decrypt(SCP_R3, SCP_R4);

        // Write the decrypted key in R4 back to the key buffer
        falc_trapped_dmread(falc_sethi_i((LwU32)(&(pKeyBuf[index])), SCP_R4));
        falc_dmwait();
    }

    // Disable the trap mode
    falc_scp_trap(0);

lwsrCryptEntry_exit:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return status;
}

