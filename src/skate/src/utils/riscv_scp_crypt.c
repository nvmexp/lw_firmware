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
/* ------------------------- Application Includes --------------------------- */
#include "dev_bus.h"

#include "riscv_scp_internals.h"
#include "scp_crypt.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */

void s_riscvScpDoCbcCrypt(LwU8 *pSrc, LwU32 size, LwU8 *pDest,
    const LwU8 *pIvBuf, LwBool bEncrypt);
#define SCP_RAND_DATA_SIZE_IN_BYTES 16
#define BLCK_SZ    SCP_RAND_DATA_SIZE_IN_BYTES

void kmemLoadKeyToScp(LwU32 kSltId, LwU32 scpRegIndex)
{
    LwU32 kmemCtlVal      = 0;
    LwU32 data32          = 0;

    /* Write SECRET63_CTL = 1 to make KMEM as the new source for secret63 */
    priWrite(LW_PPWR_PMU_SCP_SECRET63_CTL, DRF_DEF(_PPWR, _PMU_SCP_SECRET63_CTL, _SEL, _KMEM));

    /* Prepare the command to load secret63 from given keyslot of KMEM */
    kmemCtlVal  = DRF_NUM(_PPWR, _FALCON_KMEM_CTL, _IDX,  kSltId);
    kmemCtlVal |= DRF_DEF(_PPWR, _FALCON_KMEM_CTL, _CMD, _LOADTO63);

    /* Issue the command */
    priWrite(LW_PPWR_FALCON_KMEM_CTL, kmemCtlVal);

    /* Keep reading until CMD field is reset to zero to ensure previous register write was successful */
    data32 = priRead(LW_PPWR_FALCON_KMEM_CTL);
    while (DRF_VAL(_PPWR, _FALCON_KMEM_CTL, _CMD, data32) != 0)
    {
        data32 = priRead(LW_PPWR_FALCON_KMEM_CTL);
    }

    /* Ensure that secret63 is loaded with the keyslot data */
    if(FLD_TEST_DRF(_PPWR, _FALCON_KMEM_CTL, _RESULT, _LOADTO63_DONE, data32) == LW_FALSE) {
        priWrite(0x14d0, 0xdeaddead);
        while(1);
    }

    /* Move the KMEM data in secret63 to the destination scp register */
    riscv_scp_secret(63, scpRegIndex);

}

void scpCbcCryptWithKmem
(
    LwU8        *pSrc,
    LwU32        size,
    LwU8        *pDest,
    const LwU8  *pIvBuf,
    LwBool       bEncrypt,
    LwU32        keySlotId
)
{
    kmemLoadKeyToScp(keySlotId, SCP_R3);
    s_riscvScpDoCbcCrypt(pSrc, size, pDest, pIvBuf, bEncrypt);
}
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
 */
void s_riscvScpDoCbcCrypt
(
    LwU8        *pSrc,
    LwU32        size,
    LwU8        *pDest,
    const LwU8  *pIvBuf,
    LwBool       bEncrypt
)
{
    LwU64   srcPa = ((LwU64)pSrc);
    LwU64   destPa = ((LwU64) pDest);

    //LwU64   srcPa = ((LwU64)&pSrc) - (LwU64)(0x18000);
    //LwU64   destPa = ((LwU64) &pDest)- (LwU64)(0x18000);

    LwU64 ivPa = (LwU64)pIvBuf;
 priWrite(LW_PPWR_FALCON_MAILBOX1, 0x11223344);

    // IV is at SCP_R2
    riscv_dmwrite(ivPa, SCP_R2);
    riscv_scpWait();

 priWrite(LW_PPWR_FALCON_MAILBOX1, 0x11223341);
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

 priWrite(LW_PPWR_FALCON_MAILBOX1, 0xbeef001);
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
        riscv_dmread( destPa, SCP_R5);
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
 * @param[in]  pSrc         Buffer containing data to be encrypted or decrypted
 * @param[in]  size         Size of the input buffer
 * @param[out] pDest        Output buffer
 * @param[in]  pIvBuf       Buffer containing the IV to be used
 */
/*
FLCN_STATUS scpCbcCryptWithKey
(
    LwU8       *pKey,
    LwBool      bIsEncrypt,
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
*/
