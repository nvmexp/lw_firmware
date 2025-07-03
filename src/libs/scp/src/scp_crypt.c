/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   scp_crypt.c
 * @brief  Encryption/decryption utilities using SCP
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "scp_internals.h"
#include "scp_crypt.h"
#include "scp_secret_usage.h"
#include "common_hslib.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define ADD_CASE_TO_SELECT_SCP_KEY_IDX(i) \
        case i: \
        falc_scp_secret(i, SCP_R2); \
        break
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void _scpCryptEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "start");

static inline FLCN_STATUS _scpGenDerivedKeyByEncryptingSalt(LwU8 *pSalt,
                                                            LwU8 keyIndex)
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "_scpGenDerivedKeyByEncryptingSalt");
static inline void _scpDoCrypt(LwU8 *pSrc, LwU32 size, LwU8 *pDest,
                               LwBool bShortlwt)
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "_scpDoCrypt");
static inline void _scpCbcCryptLoadTrace0(const LwU8 *pIv, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "_scpCbcCryptLoadTrace0");
static inline void _scpCtrCryptLoadTrace0(const LwU8 *pIv)
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "_scpCtrCryptLoadTrace0");
static inline void _scpScrubRegisters(const LwU8 *pClearBuf)
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "_scpScrubRegisters");

/*!
 * _scpDoCrypt
 *
 * @brief  Encrypts/Decrypts memory of 'size' bytes in DMEM at pSrc
 *         and stores the encrypted output to memory/DMEM at pDest.
 * 
 *         Caller needs to load trace0 sequencer with desired AES algorithm
 *         using _scpCtrCryptLoadTrace0() or _scpCbcCryptLoadTrace0().
 *
 *         All buffer parameters must be aligned to SCP_BUF_ALIGNMENT.
 * 
 * @param[in]  pSrc      Buffer containing data to encrypt/decrypt
 * @param[in]  size      Size of data at pSrc, must be multiple of 16B
 * @param[out] pDest     Output buffer, same size as pSrc
 * @param[in]  bShortlwt Should the output be shortlwt to memory?
 */
void _scpDoCrypt
(
    LwU8   *pSrc,
    LwU32   size,
    LwU8   *pDest,
    LwBool  bShortlwt
)
{
    LwU32    dmwaddr         = 0;
    LwU32    dmraddr         = 0;
    LwU32    dewaddr         = 0;
    LwU32    numIterations   = 0;
    LwU32    tsz             = 0;
    LwU32    srcPa;
    LwU32    destPa;

    while (size > 0)
    {
        srcPa  = osDmemAddressVa2PaGet(pSrc);
        destPa = bShortlwt ? (LwU32)pDest : osDmemAddressVa2PaGet(pDest);

        //
        // Use the SCP sequencer to do the encryption or decryption.
        // Find the number of iterations we need: each iteration consumes 16B,
        // and the number of iterations needs to be a power of two, up to 16,
        // so we can consume up to 256B per sequencer iteration.
        // Do this until the entire input is consumed.
        //

        numIterations = size / 16;
        if (numIterations > 16)
        {
            numIterations = 16;
        }

        // Check if numIterations is power of 2. Else assume 16B
        if (numIterations & (numIterations - 1))
        {
            numIterations = 1;
        }

        // How many bytes are consumed
        tsz = numIterations * 16;

        // Check that the source and dest are also aligned to tsz
        if (((LwU32)pSrc % tsz) || ((LwU32)pDest % tsz))
        {
            // Not aligned. Switch to 16B
            tsz = 16;
        }

        //
        // TODO: Remove the switch statement if possible. falc_scp_loop_trace0
        // requires an immediate operand
        //
        switch (tsz)
        {
            case 32:
            {
                dmwaddr = falc_sethi_i(srcPa,  DMSIZE_32B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_32B);
                dmraddr = falc_sethi_i(0,      DMSIZE_32B);

                // Program the sequencer for 2 iterations
                falc_scp_loop_trace0(2);
                break;
            }
            case 64:
            {
                dmwaddr = falc_sethi_i(srcPa,  DMSIZE_64B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_64B);
                dmraddr = falc_sethi_i(0,      DMSIZE_64B);

                // Program the sequencer for 4 iterations
                falc_scp_loop_trace0(4);
                break;
            }
            case 128:
            {
                dmwaddr = falc_sethi_i(srcPa,  DMSIZE_128B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_128B);
                dmraddr = falc_sethi_i(0,      DMSIZE_128B);

                // Program the sequencer for 8 iterations
                falc_scp_loop_trace0(8);
                break;
            }
            case 256:
            {
                dmwaddr = falc_sethi_i(srcPa,  DMSIZE_256B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_256B);
                dmraddr = falc_sethi_i(0,      DMSIZE_256B);

                // Program the sequencer for 16 iterations
                falc_scp_loop_trace0(16);
                break;
            }
            default:
            {
                // Go with lowest possible.. Need a better soln
                // TODO: change asm constraints
                dmwaddr = falc_sethi_i(srcPa,  DMSIZE_16B);
                dewaddr = falc_sethi_i(destPa, DMSIZE_16B);
                dmraddr = falc_sethi_i(0,      DMSIZE_16B);

                // Program the sequencer for 1 iteration
                falc_scp_loop_trace0(1);
                tsz = 16;
                break;
            }
        }
        falc_scp_trap_suppressed(1);
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

        size  -= tsz;
        pSrc  += tsz;
        pDest += tsz;
    }
}

/*!
 * _scpCbcCryptLoadTrace0
 *
 * @brief  Loads trace0 with instructions needed for CBC encryption
 *         and decryption. Assumes key to be stored in SCP_R3.
 *
 * @param[in] pIv         IV to be used, size of 16B and aligned to
 *                        SCP_BUF_ALIGNMENT
 * @param[in] bIsEncrypt  Encrypt or Decrypt
 */
void _scpCbcCryptLoadTrace0
(
    const LwU8 *pIv,
    LwBool      bIsEncrypt
)
{
    LwU32 ivPa;

    ivPa = osDmemAddressVa2PaGet((LwU8 *)pIv);

    // IV is at SCP_R2
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(ivPa, SCP_R2));
    falc_dmwait();

    if (bIsEncrypt)
    {
        falc_scp_mov(SCP_R3, SCP_R4);
    }
    else
    {
        falc_scp_rkey10(SCP_R3, SCP_R4);
    }
    falc_scp_key(SCP_R4);

    falc_scp_trap(0);
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
    falc_scp_trap(0);
}

/*!
 * _scpCtrCryptLoadTrace0
 *
 * @brief  Loads trace0 with instructions needed for CTR encryption
 *         and decryption. Assumes key to be stored in SCP_R3.
 * 
 * @param[in] pIv IV to be used, size of 16B and aligned to SCP_BUF_ALIGNMENT
 */
void _scpCtrCryptLoadTrace0
(
    const LwU8 *pIv
)
{
    LwU32 ivPa;

    ivPa = osDmemAddressVa2PaGet((LwU8 *)pIv);

    // IV is at SCP_R2
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(ivPa, SCP_R2));
    falc_dmwait();

    //
    // Move key into SCP_R4 as expected.
    // Process is same for encryption & decryption,
    // so do not set key as "decryption" key.
    //
    falc_scp_mov(SCP_R3, SCP_R4);
    falc_scp_key(SCP_R4);

    falc_scp_trap(0);
    falc_scp_load_trace0(5);

    falc_scp_push(SCP_R3);
    falc_scp_encrypt(SCP_R2, SCP_R5);
    falc_scp_xor(SCP_R3, SCP_R5);
    falc_scp_add(1, SCP_R2);
    falc_scp_fetch(SCP_R5);

    falc_scp_trap(0);
}

/*!
 * _scpGenDerivedKeyByEncryptingSalt
 *
 * @brief Generate an AES key using the salt. The key is generated by
 *        encrypting the salt with the HW secret. The key is stored
 *        in SCP_R3 at the end. Given the side effect of leaving the
 *        key in SCP_R3, please use caution when calling this function.
 *
 * @param[in]  pSalt     Salt value to generate an AES key,
 *                       size of 16B and aligned to SCP_BUF_ALIGNMENT
 * @param[in]  keyIndex  Key index to use to generate the AES key
 */
FLCN_STATUS _scpGenDerivedKeyByEncryptingSalt
(
    LwU8 *pSalt,
    LwU8  keyIndex
)
{
    LwU32       saltPa;
    FLCN_STATUS status = FLCN_OK;

    saltPa = osDmemAddressVa2PaGet(pSalt);

    // Encrypt the salt with HW key
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(saltPa, SCP_R1));
    falc_dmwait();


    //
    // falc_scp_secret needs the key index to be an immediate value.
    // SCP secret is loaded into R2
    //
    switch (keyIndex)
    {
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto _scpGenDerivedKeyByEncryptingSalt_exit;
            break;

        // GFE Key Index
        ADD_CASE_TO_SELECT_SCP_KEY_IDX(LW_SCP_SECRET_IDX_GFE_KEK);

        // GSP(Lite) HDCP22 uses index 10.
        ADD_CASE_TO_SELECT_SCP_KEY_IDX(LW_SCP_SECRET_IDX_10);
    }

    falc_scp_key(SCP_R2);
    falc_scp_encrypt(SCP_R1, SCP_R3);

_scpGenDerivedKeyByEncryptingSalt_exit:
    falc_scp_trap(0);

    return status;
}

/*!
 * _scpScrubRegisters
 * 
 * @brief Clears all SCP registers to ensure no secret data leaks.
 * 
 *        In order to ensure registers can be cleared, requires buffer
 *        to load into SCP_R0. This gives it the fetchable attribute,
 *        which will then be loaded into other registers. Without
 *        fetchable attribute, xor operations will fail silently.
 * 
 * @param[in] pClearBuf  Pointer to buffer of size 16B and aligned to
 *                       SCP_BUF_ALIGNMENT                     
 */
void _scpScrubRegisters
(
    const LwU8 *pClearBuf
)
{
    LwU32 clearBufPa;

    clearBufPa = osDmemAddressVa2PaGet((LwU8 *)pClearBuf);

    // Load SCP_R0 with data, thereby giving it fetchable ACL
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(clearBufPa, SCP_R0));
    falc_dmwait();
    falc_scp_trap(0);

    // Move SCP_R0 into all registers, copying its ACLs
    falc_scp_mov(SCP_R0, SCP_R1);
    falc_scp_mov(SCP_R0, SCP_R2);
    falc_scp_mov(SCP_R0, SCP_R3);
    falc_scp_mov(SCP_R0, SCP_R4);
    falc_scp_mov(SCP_R0, SCP_R5);
    falc_scp_mov(SCP_R0, SCP_R6);
    falc_scp_mov(SCP_R0, SCP_R7);

    // Scrub all registers
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_xor(SCP_R7, SCP_R7);
} 

/*!
 * scpCbcCrypt
 *
 * @brief Do AES-CBC encryption or decryption using the SCP.
 *        It generates an AES key by encrypting the salt with the HW secret,
 *        and places it in register SCP_R3.
 *        Then, it runs the AES-CBC encryption/decryption routine with this
 *        generated AES key and the IV passed in.
 *        Note that this function scrubs the SCP registers so the caller cannot
 *        depend on any SCP state.
 * 
 *        All buffer parameters must be aligned to SCP_BUF_ALIGNMENT.
 *
 * @param[in]  pSalt        Salt value to generate an AES key, size of 16B
 * @param[in]  bIsEncrypt   Whether to do encryption or decryption
 * @param[in]  bShortlwt    Enable/disable shortlwt DMA
 * @param[in]  keyIndex     The SCP key index to use
 * @param[in]  pSrc         Buffer containing data to encrypt or decrypt
 * @param[in]  size         Size of the input buffer, must be multiple of 16B
 * @param[out] pDest        Output buffer, size of pSrc
 * @param[in]  pIvBuf       Buffer containing the IV to be used, size of 16B
 */
FLCN_STATUS scpCbcCrypt
(
    LwU8       *pSalt,
    LwBool      bIsEncrypt,
    LwBool      bShortlwt,
    LwU8        keyIndex,
    LwU8       *pSrc,
    LwU32       size,
    LwU8       *pDest,
    const LwU8 *pIvBuf
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check the alignment of the inputs
    if ((((LwU32)pSalt % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pSrc % SCP_BUF_ALIGNMENT) != 0) ||
        (size == 0) || ((size % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pDest % SCP_BUF_ALIGNMENT) != 0) ||
        ((((LwU32)pIvBuf) % SCP_BUF_ALIGNMENT) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Write key to SCP_R3 as expected by _scpCbcCryptLoadTrace0
    if ((status = _scpGenDerivedKeyByEncryptingSalt(pSalt, keyIndex)) != FLCN_OK)
    {
        goto scpCbcCrypt_exit;
    }

    _scpCbcCryptLoadTrace0(pIvBuf, bIsEncrypt);
    _scpDoCrypt(pSrc, size, pDest, bShortlwt);

scpCbcCrypt_exit:
    // Scrub SCP registers to avoid leaking any secret data
    _scpScrubRegisters(pIvBuf);

    return status;
}

/*!
 * _scpCryptEntry
 *
 * @brief Entry function of SCP crypt library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void _scpCryptEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/*!
 * scpCbcCryptWithKey
 *
 * @brief This function is similar to scpCbcCrypt except for one difference where
 *        the caller has to provide the key needed for encryption (scpCbcCrypt derives the key).
 *        
 *        All buffer parameters must be aligned to SCP_BUF_ALIGNMENT.
 *
 * @param[in]  pKey         Key used for encryption/decryption, size of 16B
 * @param[in]  bIsEncrypt   Whether to do encryption or decryption
 * @param[in]  bShortlwt    Enable/disable shortlwt DMA
 * @param[in]  pSrc         Buffer containing data to be encrypted or decrypted
 * @param[in]  size         Size of the input buffer, must be multiple of 16B
 * @param[out] pDest        Output buffer, same size as pSrc
 * @param[in]  pIvBuf       Buffer containing the IV to be used, size of 16B
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
    LwU32       keyPa;

    // Check the alignment of the inputs
    if ((((LwU32)pKey % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pSrc % SCP_BUF_ALIGNMENT) != 0) ||
        (size == 0) || ((size % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pDest % SCP_BUF_ALIGNMENT) != 0) ||
        ((((LwU32)pIvBuf) % SCP_BUF_ALIGNMENT) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    keyPa = osDmemAddressVa2PaGet(pKey);

    // Write key to SCP_R3 as expected by _scpCbcCryptLoadTrace0
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(keyPa, SCP_R3));
    falc_dmwait();
    falc_scp_trap(0);

    _scpCbcCryptLoadTrace0(pIvBuf, bIsEncrypt);
    _scpDoCrypt(pSrc, size, pDest, bShortlwt);

    // Scrub SCP registers to avoid leaking any secret data
    _scpScrubRegisters(pIvBuf);

    return status;
}

/*!
 * scpCtrCrypt
 *
 * @brief Similar to scpCbcCrypt, but performs AES-CTR encryption/decryption,
 *        rather than AES-CBC. Note that AES-CTR works the same for encryption
 *        and decryption, so it does not need to be specified.
 *
 *        All buffer parameters must be aligned to SCP_BUF_ALIGNMENT.
 *
 * @param[in]  pSalt        Salt value to generate an AES key, size of 16B
 * @param[in]  bShortlwt    Enable/disable shortlwt DMA
 * @param[in]  keyIndex     The SCP key index to use
 * @param[in]  pSrc         Buffer containing data to encrypt or decrypt
 * @param[in]  size         Size of the input buffer, must be multiple of 16B
 * @param[out] pDest        Output buffer, same size as pSrc
 * @param[in]  pIvBuf       Buffer containing the IV to be used, size of 16B
 */
FLCN_STATUS scpCtrCrypt
(
    LwU8       *pSalt,
    LwBool      bShortlwt,
    LwU8        keyIndex,
    LwU8       *pSrc,
    LwU32       size,
    LwU8       *pDest,
    const LwU8 *pIvBuf
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check the alignment of the inputs
    if ((((LwU32)pSalt % SCP_BUF_ALIGNMENT) != 0)        ||
        (((LwU32)pSrc % SCP_BUF_ALIGNMENT) != 0)         ||
        (size == 0) || ((size % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pDest % SCP_BUF_ALIGNMENT) != 0)        ||
        (((LwU32)pIvBuf % SCP_BUF_ALIGNMENT) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Load key into SCP_R3 as expected by _scpCtrCryptLoadTrace0
    if ((status = _scpGenDerivedKeyByEncryptingSalt(pSalt, keyIndex)) != FLCN_OK)
    {
        goto scpCtrCrypt_exit;
    }

    // Load instruction sequence and IV, then perform encryption/decryption
    _scpCtrCryptLoadTrace0(pIvBuf);
    _scpDoCrypt(pSrc, size, pDest, bShortlwt);

scpCtrCrypt_exit:
    // Scrub SCP registers to avoid leaking any secret data
    _scpScrubRegisters(pIvBuf);

    return status;
}

/*!
 * scpCtrCryptWithKey
 *
 * @brief Similar to scpCbcCryptWithKey, but uses AES-CTR mode to perform
 *        encryption/decryption, rather than AES-CBC.
 *
 *        All buffer parameters must be aligned to SCP_BUF_ALIGNMENT.
 *
 * @param[in]  pKey         Key used for encryption/decryption, size of 16B
 * @param[in]  bShortlwt    Enable/disable shortlwt DMA
 * @param[in]  pSrc         Buffer containing data to encrypt or decrypt
 * @param[in]  size         Size of the input buffer, must be multiple of 16B
 * @param[out] pDest        Output buffer, same size as pSrc
 * @param[in]  pIvBuf       Buffer containing the IV to be used, size of 16B
 */
FLCN_STATUS scpCtrCryptWithKey
(
    LwU8       *pKey,
    LwBool      bShortlwt,
    LwU8       *pSrc,
    LwU32       size,
    LwU8       *pDest,
    const LwU8 *pIvBuf
)
{
    LwU32 keyPa = 0;

    // Check the alignment of the inputs
    if ((((LwU32)pKey % SCP_BUF_ALIGNMENT) != 0)         ||
        (((LwU32)pSrc % SCP_BUF_ALIGNMENT) != 0)         ||
        (size == 0) || ((size % SCP_BUF_ALIGNMENT) != 0) ||
        (((LwU32)pDest % SCP_BUF_ALIGNMENT) != 0)        ||
        (((LwU32)pIvBuf % SCP_BUF_ALIGNMENT) != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    keyPa = osDmemAddressVa2PaGet(pKey);

    // Write key to SCP_R3 as expected by _scpCtrCryptLoadTrace0
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(keyPa, SCP_R3));
    falc_dmwait();
    falc_scp_trap(0);

    // Load instruction sequence and IV, then perform encryption/decryption
    _scpCtrCryptLoadTrace0(pIvBuf);
    _scpDoCrypt(pSrc, size, pDest, bShortlwt);

    // Scrub SCP registers to avoid leaking any secret data
    _scpScrubRegisters(pIvBuf);

    return FLCN_OK;
}
