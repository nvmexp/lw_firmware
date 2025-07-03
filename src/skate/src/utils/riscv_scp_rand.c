/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
  * @file   riscv_scp_crypt.c
  * @brief  Encryption/decryption utilities using SCP
  *
  */

  /* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "riscv_scp_internals.h"
#include "scp_crypt.h"
#include "riscv_scp_internals.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */

static inline void s_riscvScpDoCbcCrypt(LwU8 *pSrc, LwU32 size, LwU8 *pDest,
    const LwU8 *pIvBuf, LwBool bEncrypt,
    LwBool bShortlwt);

#define SCP_RAND_DATA_SIZE_IN_BYTES 16
#define BLCK_SZ    SCP_RAND_DATA_SIZE_IN_BYTES

/*!
 * s_riscvScpDoCbcCrypt
 *
 * @brief  Encrypts/Decrypts memory using AES-CBC of size 'size' in DMEM at
 *         pSrc and stores the encrypted output to memory/DMEM at pDest.
 *         Caller needs to set the AES key in SCP_R3
 *
 * @param[in]  pSrc       Buffer containing data to be encrypted or decrypted
 * @param[in]  size       Size of memory block in DMEM. Multiple of 16B
 * @param[out] pDest      Output buffer
 * @param[in]  pIvBuf     Buffer holding IV to be used. Should be 16B aligned
 * @param[in]  bIsEncrypt Is encryption or decryption?
 * @param [in] bShortlwt  Should the output be shortlwt to memory?
 */
static inline void s_riscvScpDoCbcCrypt
(
    LwU8        *pSrc,
    LwU32        size,
    LwU8        *pDest,
    const LwU8  *pIvBuf,
    LwBool       bEncrypt,
    LwBool       bShortlwt
)
{
    LwU64   srcPa = 0;
    LwU64   destPa = 0;

    // decryptedc data is in R5
    // Encrypted Data in R3
    if (FLCN_OK != sysVirtToPhys((LwU64*)pSrc, &srcPa))
    {
        return;
    }

    if (FLCN_OK != sysVirtToPhys((LwU64*)pDest, &destPa))
    {
        return;
    }

    LwU64 ivPa = 0;
    sysVirtToPhys((void*)pIvBuf, &ivPa);

    // IV is at SCP_R2
    riscv_dmwrite(ivPa, SCP_R2);
    riscv_scpWait();

    if (bEncrypt)
    {
        riscv_scp_mov(SCP_R3, SCP_R4);
    }
    else
    {
        riscv_scp_rkey10(SCP_R3, SCP_R4);
    }

    riscv_scp_key(SCP_R4);

    // TODO: Employ loop trace to optimize large size encrypts/decrypts

    // Do Everything here itself(in blocks of 16B)
    while (size)
    {
        riscv_dmwrite(srcPa, SCP_R3);
        riscv_scpWait();

        if (bEncrypt)
        {
            riscv_scp_xor(SCP_R2, SCP_R3);
            riscv_scp_encrypt(SCP_R3, SCP_R5);
            riscv_scp_mov(SCP_R5, SCP_R2);
        }
        else
        {
            riscv_scp_decrypt(SCP_R3, SCP_R5);
            riscv_scp_xor(SCP_R2, SCP_R5);
            riscv_scp_mov(SCP_R3, SCP_R2);
        }

        // Now transfer the decrypted data from SCP_R5
        riscv_dmread(destPa, SCP_R5);
        riscv_scpWait();

        size -= 16;
        srcPa += 16;
        destPa += 16;
    }
}

/*!
 * scpCbcCryptWithKey
 *
 * @brief This function is similar to scpCbcCrypt except for one difference where
 *        the caller has to provide the key needed for encryption (scpCbcCrypt derives the key)
 *
 * @param[in]  pKey         Key used for encryption/decryption. Should be aligned.
 * @param[in]  bIsEncrypt   Whether to do encryption or decryption
 * @param[in]  bShortlwt    Enable/disable shortlwt DMA
 * @param[in]  pSrc         Buffer containing data to be encrypted or decrypted
 * @param[in]  size         Size of the input buffer
 * @param[out] pDest        Output buffer
 * @param[in]  pIvBuf       Buffer containing the IV to be used
 */
FLCN_STATUS scpCbcCryptWithKey
(
    LwU8       *pKey,
    LwBool      bIsEncrypt,
    LwBool      bShortlwt,
    LwU8       *pSrc,
    LwU32       size,
    LwU8       *pDest,
    const LwU8 *pIvBuf
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU64       keyPa = 0;
    // Check the alignment of the inputs
    if ((((LwU64)pKey % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU64)pSrc % SCP_BUF_ALIGNMENT) != 0) ||
        (size == 0) || ((size % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU64)pDest % SCP_BUF_ALIGNMENT) != 0) ||
        ((((LwU64)pIvBuf) % SCP_BUF_ALIGNMENT) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Take care of any possible context-switches while using SCP registers
    lwrtosENTER_CRITICAL();

    sysVirtToPhys((void*)pKey, &keyPa);
    // Write key to SCP_R3 as expected by _scpDoCbcCrypt
    riscv_dmwrite(keyPa, SCP_R3);
    riscv_scpWait();

    s_riscvScpDoCbcCrypt(pSrc, size, pDest, pIvBuf, bIsEncrypt, bShortlwt);

    // Scrub SCP registers to avoid leaking any secret data
    riscv_scp_xor(SCP_R0, SCP_R0);
    riscv_scp_xor(SCP_R1, SCP_R1);
    riscv_scp_xor(SCP_R2, SCP_R2);
    riscv_scp_xor(SCP_R3, SCP_R3);
    riscv_scp_xor(SCP_R4, SCP_R4);
    riscv_scp_xor(SCP_R5, SCP_R5);
    riscv_scp_xor(SCP_R6, SCP_R6);
#ifndef IS_SSP_ENABLED_WITH_SCP
    riscv_scp_xor(SCP_R7, SCP_R7);
#endif // IS_SSP_ENABLED_WITH_SCP

    lwrtosEXIT_CRITICAL();
    return status;
}
