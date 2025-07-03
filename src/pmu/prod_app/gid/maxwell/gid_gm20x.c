/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    gid_gm20x.c
 *          GID HAL Functions for GM20X and later GPUs
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "scp_internals.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "lwoslayer.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*
 * Using macros for both KM and SALT instead of initializing global variables to ensure
 * this gets into code section.
 * TODO: Assign these to global variables and explicitly move into code section to make
 * it easier for future changes.
 */
#define DEVID_KM_DWORD_0 0x8502C30B
#define DEVID_KM_DWORD_1 0xEC5D952E
#define DEVID_KM_DWORD_2 0x6B982A89
#define DEVID_KM_DWORD_3 0x20D30714

#define DEVID_SALT_DWORD_0 0x59AAE274
#define DEVID_SALT_DWORD_1 0x7EE02B46
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_pmuScpCbcCryptWithKey_GM20X
(
    LwU8         *pKey,
    LwBool        bIsEncrypt,
    LwBool        bShortlwt,
    LwU8         *pSrc,
    LwU32         size,
    LwU8         *pDest,
    const LwU8   *pIvBuf
)
// VR-TODO: WAR for ODP
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "s_pmuScpCbcCryptWithKey_GM20X");
#else /* ON_DEMAND_PAGING_BLK */
    GCC_ATTRIB_SECTION("imem_gid", "s_pmuScpCbcCryptWithKey_GM20X");
#endif /* ON_DEMAND_PAGING_BLK */

static void
s_pmuScpCbcCryptLoadTrace0_GM20X
(
    const LwU8 *pIv,
    LwBool      bIsEncrypt
) GCC_ATTRIB_SECTION("imem_gid", "s_pmuScpCbcCryptLoadTrace0_GM20X");

static void
s_pmuScpDoCbcCrypt_GM20X
(
    LwU8        *pSrc,
    LwU32        size,
    LwU8        *pDest,
    const LwU8  *pIvBuf,
    LwBool       bEncrypt,
    LwBool       bShortlwt
) GCC_ATTRIB_SECTION("imem_gid", "s_pmuScpDoCbcCrypt_GM20X");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Initializing keys required for DEVID name decryption.
 * Two elements are generated from this function:
 * --> Encrypted devId
 * --> Derived key using devId and SALT
 * Please refer to https://confluence.lwpu.com/pages/viewpage.action?pageId=115943060
 */
FLCN_STATUS
pmuInitDevidKeys_GM20X(RM_PMU_DEVID_ENC_INFO *pDevidInfoOut)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       devId  = 0;
    LwU8        km[SCP_AES_SIZE_IN_BYTES * 2];
    LwU8        iv[SCP_AES_SIZE_IN_BYTES * 2];
    LwU8        devIdEnc[SCP_AES_SIZE_IN_BYTES * 2];
    LwU8        devIdDer[SCP_AES_SIZE_IN_BYTES * 2];
    LwU32       *pKm = NULL;
    LwU32       *pIv = NULL;
    LwU32       *pDevIdEnc = NULL;
    LwU32       *pDevIdDer = NULL;

    // Required for correctness of ALIGN_UP below
    ct_assert(SCP_BUF_ALIGNMENT <= SCP_AES_SIZE_IN_BYTES);
    ct_assert(RM_PMU_DEVID_INFO_SIZE_BYTES == SCP_AES_SIZE_IN_BYTES);

    pKm       = PMU_ALIGN_UP_PTR(km, SCP_BUF_ALIGNMENT);
    pIv       = PMU_ALIGN_UP_PTR(iv, SCP_BUF_ALIGNMENT);
    pDevIdEnc = PMU_ALIGN_UP_PTR(devIdEnc, SCP_BUF_ALIGNMENT);
    pDevIdDer = PMU_ALIGN_UP_PTR(devIdDer, SCP_BUF_ALIGNMENT);

    // Read DEVID
    devId = Pmu.gpuDevId;

    //
    // Copy KM. Using direct assignment instead of memcpy to avoid including mem_hs overlay
    // NOTE: Needs scrubbing before exiting this function
    //
    pKm[0] = DEVID_KM_DWORD_0;
    pKm[1] = DEVID_KM_DWORD_1;
    pKm[2] = DEVID_KM_DWORD_2;
    pKm[3] = DEVID_KM_DWORD_3;

    // Init IV to 0 (IV is expected to be 0)
    pIv[0] = pIv[1] = pIv[2] = pIv[3] = 0;

    // Using pDevIdEnc as both input and output buffer
    pDevIdEnc[0] = devId;
    pDevIdEnc[1] = 0;
    pDevIdEnc[2] = 0;
    pDevIdEnc[3] = 0;

    // Encrypt devId
    if ((flcnStatus = s_pmuScpCbcCryptWithKey_GM20X((LwU8*)pKm, LW_TRUE, LW_FALSE, (LwU8*)pDevIdEnc,
             SCP_AES_SIZE_IN_BYTES, (LwU8*)pDevIdEnc, (LwU8*)pIv)) != FLCN_OK)
    {
        // Scrub devId if encryption fails
        pDevIdEnc[0] = 0;
        goto pmuInitDevidKeys_GM20X_Exit;
    }

    // Get derived key using devId and salt. Using pDevIdDer for both input and output
    pDevIdDer[0] = DEVID_SALT_DWORD_0;
    pDevIdDer[1] = DEVID_SALT_DWORD_1;
    pDevIdDer[2] = devId;
    pDevIdDer[3] = 0;

    // Get derived key
    if ((flcnStatus = s_pmuScpCbcCryptWithKey_GM20X((LwU8*)pKm, LW_TRUE, LW_FALSE, (LwU8*)pDevIdDer,
             SCP_AES_SIZE_IN_BYTES, (LwU8*)pDevIdDer, (LwU8*)pIv)) != FLCN_OK)
    {
        // Scrub SALT if encryption fails
        pDevIdDer[0] = 0;
        pDevIdDer[1] = 0;
        pDevIdDer[2] = 0;
        goto pmuInitDevidKeys_GM20X_Exit;
    }

    memcpy(pDevidInfoOut->devidEnc, pDevIdEnc, RM_PMU_DEVID_INFO_SIZE_BYTES);
    memcpy(pDevidInfoOut->devidDerivedKey, pDevIdDer, RM_PMU_DEVID_INFO_SIZE_BYTES);

pmuInitDevidKeys_GM20X_Exit:
    // Scrub pKm
    memset(pKm, 0x0, SCP_AES_SIZE_IN_BYTES);

    return flcnStatus;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * _pmuScpCbcCryptLoadTrace0
 *
 * @brief  Loads trace0 with instructions needed for CBC encryption
 *         and decryption. Assumes key to be stored in SCP_R3
 * @param[in] pIv         IV to be used. 16B aligned address.
 * @param[in] bIsEncrypt  Encrypt or Decrypt
 */
static void
s_pmuScpCbcCryptLoadTrace0_GM20X
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
    falc_scp_trap(0);

    if (bIsEncrypt)
    {
        falc_scp_mov(SCP_R3, SCP_R4);
    }
    else
    {
        falc_scp_rkey10(SCP_R3, SCP_R4);
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
    falc_scp_trap(0);
}

/*!
 * _pmuScpDoCbcCrypt
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
static void
s_pmuScpDoCbcCrypt_GM20X
(
    LwU8        *pSrc,
    LwU32        size,
    LwU8        *pDest,
    const LwU8  *pIvBuf,
    LwBool       bEncrypt,
    LwBool       bShortlwt
)
{
    LwU32    dmwaddr         = 0;
    LwU32    dmraddr         = 0;
    LwU32    dewaddr         = 0;
    LwU32    numIterations   = 0;
    LwU32    tsz             = 0;
    LwU32    srcPa;
    LwU32    destPa;

    s_pmuScpCbcCryptLoadTrace0_GM20X(pIvBuf, bEncrypt);
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
static
FLCN_STATUS s_pmuScpCbcCryptWithKey_GM20X
(
    LwU8         *pKey,
    LwBool        bIsEncrypt,
    LwBool        bShortlwt,
    LwU8         *pSrc,
    LwU32         size,
    LwU8         *pDest,
    const LwU8   *pIvBuf
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

    // Write key to SCP_R3 as expected by _scpDoCbcCrypt
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(keyPa, SCP_R3));
    falc_dmwait();


    s_pmuScpDoCbcCrypt_GM20X(pSrc, size, pDest, pIvBuf, bIsEncrypt, bShortlwt);

    // Scrub SCP registers to avoid leaking any secret data
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
#ifndef IS_SSP_ENABLED_WITH_SCP
    falc_scp_xor(SCP_R7, SCP_R7);
#endif // IS_SSP_ENABLED_WITH_SCP
    return status;
}

