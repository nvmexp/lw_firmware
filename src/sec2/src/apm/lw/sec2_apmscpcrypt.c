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
 * @file   sec2_apmsccrypt.c
 * @brief  Encryption/decryption utilities using SCP
 *         Copied from SCP libray.  Initial implementation of SEC2 APM 
 *         encrypted DMA does not need to be HS,  make a copy of SCP library
 *         functions for initial implementation.  Will move to SCP library HS 
 *         version once things are working correctly.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_csb.h"


/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Private Functions ------------------------------ */

static inline void _apmScpDoCrypt(LwU8 *pSrc, LwU32 size, LwU8 *pDest,
                                  LwBool bShortlwt, LwU8 *pIvBuf, LwBool bOutputIv)
    GCC_ATTRIB_SECTION("imem_apm", "_apmScpDoCrypt");
static inline void _apmScpCtrCryptLoadTrace0(const LwU8 *pIv)
    GCC_ATTRIB_SECTION("imem_apm", "_apmScpCtrCryptLoadTrace0");
static inline void _apmScpScrubRegisters(const LwU8 *pClearBuf)
    GCC_ATTRIB_SECTION("imem_apm", "_apmScpScrubRegisters");

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
 * @param[in]   pSrc       Buffer containing data to encrypt/decrypt
 * @param[in]   size       Size of data at pSrc, must be multiple of 16B
 * @param[out]  pDest      Output buffer, same size as pSrc
 * @param[in]   bShortlwt  Should the output be shortlwt to memory?
 * @param[out]  pIvBuf     Can optionally output updated IV to this buffer.
 * @param[in]   bOutputIv  Whether or not to output next IV to pIvBuf.
 */
void _apmScpDoCrypt
(
    LwU8   *pSrc,
    LwU32   size,
    LwU8   *pDest,
    LwBool  bShortlwt,
    LwU8   *pIvBuf,
    LwBool  bOutputIv 
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

    if (bOutputIv && pIvBuf != NULL)
    {
        LwU32 ivDestPa = osDmemAddressVa2PaGet(pIvBuf);
        dewaddr = falc_sethi_i(ivDestPa, DMSIZE_16B);

        falc_scp_fetch(SCP_R2);
        falc_scp_trap_suppressed(2);
        falc_trapped_dmread(dewaddr);
        falc_dmwait();

        // Ensure no further instructions are suppressed.
        falc_scp_trap_suppressed(0);
    }
}

/*!
 * _scpCtrCryptLoadTrace0
 *
 * @brief  Loads trace0 with instructions needed for CTR encryption
 *         and decryption. Assumes key to be stored in SCP_R3.
 * 
 * @param[in] pIv IV to be used, size of 16B and aligned to SCP_BUF_ALIGNMENT
 */
void _apmScpCtrCryptLoadTrace0
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
void _apmScpScrubRegisters
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
 * scpCtrCryptWithKey
 *
 * @brief Similar to scpCbcCryptWithKey, but uses AES-CTR mode to perform
 *        encryption/decryption, rather than AES-CBC.
 *
 *        All buffer parameters must be aligned to SCP_BUF_ALIGNMENT.
 *
 * @param[in]       pKey       Key used for encryption/decryption, size of 16B
 * @param[in]       bShortlwt  Enable/disable shortlwt DMA
 * @param[in]       pSrc       Buffer containing data to encrypt or decrypt
 * @param[in]       size       Size of the input buffer, must be multiple of 16B
 * @param[out]      pDest      Output buffer, same size as pSrc
 * @param[in, out]  pIvBuf     Buffer containing the IV to be used, size of 16B.
 *                             Can optionally output updated IV to this buffer.
 * @param[in]       bOutputIv  Whether or not to output next IV to pIvBuf.
 */
FLCN_STATUS apmScpCtrCryptWithKey
(
    LwU8   *pKey,
    LwBool  bShortlwt,
    LwU8   *pSrc,
    LwU32   size,
    LwU8   *pDest,
    LwU8   *pIvBuf,
    LwBool  bOutputIv
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
    _apmScpCtrCryptLoadTrace0(pIvBuf);
    _apmScpDoCrypt(pSrc, size, pDest, bShortlwt, pIvBuf, bOutputIv);

    // Scrub SCP registers to avoid leaking any secret data
    _apmScpScrubRegisters(pIvBuf);

    return FLCN_OK;
}
