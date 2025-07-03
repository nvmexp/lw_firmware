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
 * @file    hdcpmc_scp.h
 * @brief   Interface implementationto the SCP.
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "sec2_hs.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_hs.h"
#include "scp_rand.h"
#include "hdcpmc/hdcpmc_scp.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define HDCP_CBC_CRYPT_LOAD_TRACE0_BUFFER_SIZE                                4

/* ------------------------- Prototypes ------------------------------------- */
static void _libHdcpCryptHsEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "start");
static void _hdcpCbcCryptLoadTrace0(LwU32 dmemOffset, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "_hdcpCbcCryptLoadTrace0");
static void _hdcpImemToDmemCopyLoadTrace0(void)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "_hdcpImemToDmemCopyLoadTrace0");
static void _hdcpEncryptBlockLoadTrace0(void)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "_hdcpEncryptBlockLoadTrace0");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * _libHdcpCryptHsEntry
 *
 * @brief Entry function of HDCP crypt library. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_libHdcpCryptHsEntry(void)
{
    return;
}

/*!
 * @brief Loads trace0 with instructions needed for CBC encryption and
 *        decryption.
 *
 * Assumes the key is stored in SCP_R4.
 *
 * @param [in]  dmemOffset  A 16-byte aligned DMEM offset used for IV purpose.
 * @param [in]  bIsEncrypt  Is the key needed for encryption (LW_TRUE) or
 *                          decryption (LW_FALSE).
 */
static void
_hdcpCbcCryptLoadTrace0
(
    LwU32  dmemOffset,
    LwBool bIsEncrypt
)
{
    LwU32 *pTemp = (LwU32*)dmemOffset;
    LwU32  temp[HDCP_CBC_CRYPT_LOAD_TRACE0_BUFFER_SIZE];
    LwU32  i;

    // Cannot do a memcpy(), as we are in HS mode. Copy individual elements.
    for (i = 0; i < HDCP_CBC_CRYPT_LOAD_TRACE0_BUFFER_SIZE; i++)
    {
        temp[i] = pTemp[i];
    }

    // Set the IV for CBC
    *pTemp       = 0xDEADBEEF;
    *(pTemp + 1) = 0xBEEFDEAD;
    *(pTemp + 2) = 0xABCD1234;
    *(pTemp + 3) = 0x5678DCBA;

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pTemp, SCP_R2));
    falc_dmwait();
    falc_scp_trap(0);

    // Cannot do a memcpy(), as we are in HS mode. Copy individual elements.
    for (i = 0; i < HDCP_CBC_CRYPT_LOAD_TRACE0_BUFFER_SIZE; i++)
    {
        pTemp[i] = temp[i];
    }

    falc_scp_key(SCP_R4);
    falc_scp_load_trace0(5);
    falc_scp_push(SCP_R3);

    if (bIsEncrypt)
    {
        falc_scp_xor(SCP_R2, SCP_R3);
        falc_scp_encrypt(SCP_R3, SCP_R5);
        falc_scp_mov(SCP_R5, SCP_R2);
    }
    else
    {
        falc_scp_decrypt(SCP_R3, SCP_R5);
        falc_scp_xor(SCP_R2, SCP_R5);
        falc_scp_mov(SCP_R3, SCP_R2);
    }

    falc_scp_fetch(SCP_R5);
}

/*!
 * @brief Copies IMEM to DMEM.
 */
static void
_hdcpImemToDmemCopyLoadTrace0()
{
    falc_scp_load_trace0(2);
    falc_scp_push(SCP_R3);
    falc_scp_fetch(SCP_R3);
}

/*!
 * @brief Sequencer needed for encrypting the buffer in HDCP 2.0 standards.
 *
 * Assumes the key is in R4 and the counter in R1.
 */
static void
_hdcpEncryptBlockLoadTrace0()
{
    falc_scp_load_trace0(6);
    falc_scp_key(SCP_R4);
    falc_scp_push(SCP_R3);
    falc_scp_encrypt(SCP_R1, SCP_R5);
    falc_scp_xor(SCP_R5, SCP_R3);
    falc_scp_fetch(SCP_R3);
    falc_scp_add(1, SCP_R1);
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Sets the keys for encryption of content. The keys are specific to the
 *        session.
 *
 * @param [in]  pAesData  Pointer to the buffer containing ((riv^sctr)||ictr).
 * @param [in]  pKs       Pointer to the buffer containing the session key.
 * @param [in]  pLc       Pointer to the buffer containing lc128.
 */
void
hdcpSetEncryptionKeys
(
    LwU8 *pAesData,
    LwU8 *pKs,
    LwU8 *pLc
)
{
    LwU32 pAesDataPa = osDmemAddressVa2PaGet((LwU8*)pAesData);
    LwU32 pKsPa      = osDmemAddressVa2PaGet((LwU8*)pKs);
    LwU32 pLcPa      = osDmemAddressVa2PaGet((LwU8*)pLc);

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pAesDataPa, SCP_R1));
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pLcPa, SCP_R3));
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pKsPa, SCP_R4));
    falc_dmwait();
    falc_scp_xor(SCP_R3, SCP_R4);
    falc_scp_trap(0);
}

/*!
 * @brief Set the IV in R6 and the shared key in R7 for re-encryption of
 *        content.
 *
 * @param [in]  pIv   Pointer to the buffer containing IV.
 * @param [in]  pKey  Pointer to the buffer containing the shared key.
 */
void
hdcpSetReEncryptionKeys
(
    LwU8 *pIV,
    LwU8 *pKey
)
{
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pIV, SCP_R6));
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pKey, SCP_R7));
    falc_dmwait();
    falc_scp_trap(0);
}

/*!
 * @brief Set the keys for encryption of content using AES ECB mode.
 *
 * @param [in]  pKey  Key used to encrypt/decrypt content.
 */
void
hdcpSetAesKey
(
    LwU8 *pKey
)
{
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pKey, SCP_R4));
    falc_dmwait();
    falc_scp_trap(0);
}

/*!
 * @brief Set the AES-128 key needed for EPair encryption.
 *
 * Generates a key using the secret key index 14 and a random number generated
 * during init. The generated key is stored in SCP_R4.
 *
 * @param [in]  pEpairKey   Pointer to the buffer containing the EPair key.
 * @param [in]  bIsEncrypt  Is the key needed for encryption (LW_TRUE) or
 *                          decryption (LW_FALSE).
 */
void
hdcpSetEpairEncryptionKey
(
    LwU8   *pEpairKey,
    LwBool  bIsEncrypt
)
{
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pEpairKey, SCP_R1));
    falc_dmwait();
    falc_scp_secret(HDCP_SCP_KEY_INDEX_EPAIR, SCP_R2);
    falc_scp_key(SCP_R2);
    falc_scp_encrypt(SCP_R1, SCP_R3);

    if (bIsEncrypt)
    {
        falc_scp_mov(SCP_R3, SCP_R4);
    }
    else
    {
        falc_scp_rkey10(SCP_R3, SCP_R4);
    }

    // TODO: Check if clearing registers is needed
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);

    falc_scp_trap(0);
}

/*!
 * @brief Encrypts/decrypts memory of size 'size' in DMEM at offset 'dmemOffset'
 *        and stores the encrypted output to FB/DMEM at offset 'destOffset'.
 *
 * Caller needs to set CTX/DMB/key in SCP_R4.
 *
 * This function also uses bit-wise operators and shifts to do multiplies,
 * divides, and modulos to prevent the chance of using LS assembly libraries.
 *
 * @param [in]  dmemOffset  Starting DMEM offset of the memory block. Must be
 *                          on a 16 byte boundary.
 * @param [in]  sz          Size of the memory block in DMEM. Must be a
 *                          multiple of 16 bytes.
 * @param [in]  destOffset  Starting offset either in FB or DMEM to store the
 *                          result. Must be on a 16 byte boundary.
 * @param [in]  bIsEncrypt  Is the operation encryption (LW_TRUE) or decryption
 *                          (LW_FALSE)?
 * @param [in]  bShortlwt   Should the output be shortlwt to FB?
 * @param [in]  mode        The load trace mode to be used.
 *
 * @return FLCN_OK if the data was successfully encrypted/decrypted;
 *         FLCN_ERR_ILWALID_ARGUMENT if the offsets/size was not properly
 *         aligned or an unsupported mode is supplied.
 */
FLCN_STATUS
hdcpEcbCrypt
(
    LwU32   dmemOffset,
    LwU32   size,
    LwU32   destOffset,
    LwBool  bIsEncrypt,
    LwBool  bShortlwt,
    LwU32   mode
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       dmwaddr  = 0;
    LwU32       dmraddr  = 0;
    LwU32       dewaddr  = 0;
    LwU32       loops    = 0;
    LwU32       tempSize = 0;
    LwU32       dmemPa   = osDmemAddressVa2PaGet((LwU8*)dmemOffset);
    LwU32       destPa   = osDmemAddressVa2PaGet((LwU8*)destOffset);
    LwU32       dmemPage = dmemOffset >> 8;
    LwU32       destPage = destOffset >> 8;

    // Perform alignment check on parameters.
    if ((0 == size)                ||
        (0 != (size & 0x0F))       ||
        (0 != (dmemOffset & 0x0F)) ||
        (0 != (destOffset & 0x0F)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto hdcpEcbCrypt_end;
    }

    switch (mode)
    {
        case HDCP_CBC_CRYPT_MODE_CRYPT:
        {
            _hdcpCbcCryptLoadTrace0(dmemPa, bIsEncrypt);
            break;
        }
        case HDCP_ECB_CRYPT_MODE_HDCP_ENCRYPT:
        {
            _hdcpEncryptBlockLoadTrace0();
            break;
        }
        case HDCP_ECB_CRYPT_MODE_IMEM_TRANSFER:
        {
            _hdcpImemToDmemCopyLoadTrace0();
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto hdcpEcbCrypt_end;
            break;
        }
    }

    while (size > 0)
    {
        loops = (size >> 4);
        if (loops > 16)
        {
            loops = 16;
        }

        // Check if loops is a power of 2; else, assume 16B.
        if ((loops & (loops - 1)) != 0)
        {
            loops = 1;
        }

        tempSize = loops << 4;

        //
        // Check for size alignment. In this case, tempSize is guaranteed to
        // be a power of 2 (loops is a power of 2, and we are multiplying by
        // a power of 2). Therefore, we can use a bit-wise AND operation to
        // check for alignment (avoid using "%" operator, as it _may_ trigger
        // a halt going to LS assembly.
        //
        if ((dmemPa & (tempSize - 1)) ||
            (destPa & (tempSize - 1)))
        {
            tempSize = 16;
        }

        switch (tempSize)
        {
            case 32:
            {
                dmwaddr = falc_sethi_i(dmemPa, DMSIZE_32B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_32B);
                dmraddr = falc_sethi_i(0,      DMSIZE_32B);
                falc_scp_loop_trace0(2);
                break;
            }
            case 64:
            {
                dmwaddr = falc_sethi_i(dmemPa, DMSIZE_64B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_64B);
                dmraddr = falc_sethi_i(0,      DMSIZE_64B);
                falc_scp_loop_trace0(4);
                break;
            }
            case 128:
            {
                dmwaddr = falc_sethi_i(dmemPa, DMSIZE_128B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_128B);
                dmraddr = falc_sethi_i(0,      DMSIZE_128B);
                falc_scp_loop_trace0(8);
                break;
            }
            case 256:
            {
                dmwaddr = falc_sethi_i(dmemPa, DMSIZE_256B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_256B);
                dmraddr = falc_sethi_i(0,      DMSIZE_256B);
                falc_scp_loop_trace0(16);
                break;
            }
            default:
            {
                //
                // Go with lowest possible. Need a better solution.
                // TODO: change asm constraints.
                //
                dmwaddr = falc_sethi_i(dmemPa, DMSIZE_16B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_16B);
                dmraddr = falc_sethi_i(0,      DMSIZE_16B);
                falc_scp_loop_trace0(1);
                tempSize = 16;
                break;
            }
        }

        if (HDCP_ECB_CRYPT_MODE_IMEM_TRANSFER == mode)
        {
            falc_scp_trap_imem_suppressed(1);
        }
        else
        {
            falc_scp_trap_suppressed(1);
        }

        falc_trapped_dmwrite(dmwaddr);
        if (bShortlwt)
        {
            falc_scp_trap_shortlwt(1);
            falc_trapped_dmread(dmraddr);

            falc_dmwrite(destPa, dmraddr);
            falc_dmwait();
        }
        else
        {
            falc_scp_trap_suppressed(2);
            falc_trapped_dmread(dewaddr);
            falc_dmwait();
        }

        falc_scp_trap_imem_suppressed(0);
        falc_scp_trap_suppressed(0);

        size       -= tempSize;
        dmemOffset += tempSize;
        destOffset += tempSize;

        // If cross page boundary, update physical address again.
        if (dmemPage != (dmemOffset >> 8))
        {
            dmemPa   = osDmemAddressVa2PaGet((LwU8*)dmemOffset);
            dmemPage = dmemOffset >> 8;
        }
        else
        {
            dmemPa += tempSize;
        }

        if (destPage != (destOffset >> 8))
        {
            destPa   = osDmemAddressVa2PaGet((LwU8*)destOffset);
            destPage = destOffset >> 8;
        }
        else
        {
            destPa += tempSize;
        }
    }

hdcpEcbCrypt_end:
    return status;
}

/*!
 * @brief Obtains a randomly generated number from the SCP.
 *
 * @param [out]  pDest  16-byte aligned buffer to store the random number.
 * @param [in]   size   Size of the buffer. Must be a multiple of 16 bytes.
 *
 * @return FLCN_OK
 *             The random number was generated.
 *         FLCN_ERR_ILWALID_ARGUMENT
 *             The buffer is not 16-byte aligned and/or the size is not a
 *             multiple of 16.
 *         FLCN_ERR_ILLEGAL_OPERATION
 *             This HS function is not allowed to run.
 */
FLCN_STATUS
hdcpRandomNumber
(
    LwU8  *pDest,
    LwS32  size
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       bytesToCopy;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);
    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        goto hdcpRandomNumber_end;
    }

    //
    // Perform alignment check on parameters. Use bit-wise AND operation instead
    // of modulus to guarantee we do not call some LS assembly function.
    //
    if ((0 != ((LwU32)pDest & 0x0F)) ||
        (0 != (size & 0x0F)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto hdcpRandomNumber_end;
    }

    OS_SEC_HS_LIB_VALIDATE(libScpRandHs);
    scpStartRand();

    while (size > 0)
    {
        scpGetRand128(pDest);
        bytesToCopy = (size >= 16) ? 16 : size; // maximum 16 bytes at a time.
        size  -= bytesToCopy;
        pDest += bytesToCopy;
    }

    scpStopRand();

hdcpRandomNumber_end:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();
    return status;
}

/*!
* @brief Generates dkey as per the HDCP 2.0 specification.
*
* All parameter buffers must be 16 byte aligned.
*
* @param [in]  pRtxCtr   Pointer to the buffer containing the value
*                        Rtx concatenated with Ctr (Rtx || Ctr).
* @param [in]  pKmRnXor  Pointer to the buffer containing the value
*                        (Rn ^ Km[63:0]).
* @param [out] pDkey     Buffer to store the resulting dkey.
*
* @return FLCN_OK if a valid dkey was generated; FLCN_ERR_ILWALID_ARGUMENT if
*         any of the parameters were NULL or not 16-byte aligned.
*/
FLCN_STATUS
hdcpGenerateDkey
(
    LwU8 *pRtxCtr,
    LwU8 *pKmRnXor,
    LwU8 *pDkey
)
{
    LwU32 rtxCtrPa = osDmemAddressVa2PaGet(pRtxCtr);
    LwU32 kmRnxPa  = osDmemAddressVa2PaGet(pKmRnXor);
    LwU32 dkeyPa   = osDmemAddressVa2PaGet(pDkey);

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);

    // Check for NULL pointers.
    if ((NULL == pRtxCtr) ||
        (NULL == pKmRnXor) ||
        (NULL == pDkey))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check for buffer alignment.
    if ((((LwU32)pRtxCtr % 16) != 0) ||
        (((LwU32)pKmRnXor % 16) != 0) ||
        (((LwU32)pDkey % 16) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)rtxCtrPa, SCP_R1));
    falc_trapped_dmwrite(falc_sethi_i((LwU32)kmRnxPa, SCP_R2));
    falc_dmwait();
    falc_scp_key(SCP_R2);
    falc_scp_encrypt(SCP_R1, SCP_R4);
    falc_trapped_dmread(falc_sethi_i((LwU32)dkeyPa, SCP_R4));
    falc_dmwait();
    falc_scp_trap(0);

    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return FLCN_OK;
}

/*!
* @brief XORs two 128-bit unsigned integers.
*
* All buffers provided must be 16 byte aligned.
*
* @param [in]  pIn1    Pointer to 128-bit value to be XOR'd.
* @param [in]  pIn2    Pointer to 128-bit value to be XOR'd.
* @param [out] pOut    Pointer to buffer that stores the result.
*
* @return FLCN_OK if the values were XOR'd; FLCN_ERR_ILWALID_ARGUMENT if any
*         of the buffers are not 16 byte aligned.
*/
FLCN_STATUS
hdcpXor128Bits
(
    LwU8 *pIn1,
    LwU8 *pIn2,
    LwU8 *pOut
)
{
    LwU32 pIn1Pa = osDmemAddressVa2PaGet(pIn1);
    LwU32 pIn2Pa = osDmemAddressVa2PaGet(pIn2);
    LwU32 pOutPa = osDmemAddressVa2PaGet(pOut);

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);

    if ((((LwU32)pIn1 % 16) != 0) ||
        (((LwU32)pIn2 % 16) != 0) ||
        (((LwU32)pOut % 16) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pIn1Pa, SCP_R1));
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pIn2Pa, SCP_R2));
    falc_dmwait();
    falc_scp_xor(SCP_R1, SCP_R2);
    falc_trapped_dmread(falc_sethi_i((LwU32)pOutPa, SCP_R2));
    falc_dmwait();
    falc_scp_trap(0);

    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return FLCN_OK;
}
#endif
