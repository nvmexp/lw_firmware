/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * The code in this file refers to the "HDCP DisplayPort spec." The actual
 * document is entitled "High-bandwidth Digital Content Protection System v1.3,
 * Amendment for DisplayPort" Revision 1.1 with a publication date of 15
 * January, 2010 by Digital Content Protection LLC.
 */

/*!
 * @file    lib_hdcpauth.c
 * @brief   Supplies HDCP authentication \& verification functions.
 */

// TODO: To remove HDCPAUTH if we combine HDCP1x and HDCP22 tasks.

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include <string.h>
#include "lwos_dma.h"
#ifdef HDCPAUTH_HS_BUILD
#include "lwos_dma_hs.h"
#include "mem_hs.h"
#endif
#include "lwoslayer.h"

/* ------------------------- Application Includes --------------------------- */
#ifdef HDCPAUTH_HS_BUILD
#include "bigint_hs.h"
#else
#include "bigint.h"
#endif
#include "lib_sha1.h"
#include "lib_hdcpauth.h"
#include "lib_intfchdcp_cmn.h"
#include "flcnifhdcp.h"

#ifdef HDCPAUTH_HS_BUILD
#define hdcpDmaRead(a,b,c,d)           dmaRead_hs(a,b,c,d)
#define hdcpDmaWrite(a,b,c,d)          dmaWrite_hs(a,b,c,d)

#define hdcpMemcpy(x,y,z)              memcpy_hs(x,y,z)
#define hdcpMemcmp(x,y,z)              memcmp_hs(x,y,z)
#define hdcpMemset(x,y,z)              memset_hs(x,y,z)

#define hdcpBigIntIlwerseMod(a,b,c)    bigIntIlwerseMod_hs(a,b,c)
#define hdcpBigIntMultiplyMod(a,b,c,d) bigIntMultiplyMod_hs(a,b,c,d)
#define hdcpBigIntPowerMod(a,b,c,d,e)  bigIntPowerMod_hs(a,b,c,d,e)
#define hdcpBigIntMod(a,b,c,d)         bigIntMod_hs(a,b,c,d)
#define hdcpBigIntCompare(a,b,c)       bigIntCompare_hs(a,b,c)
#else
#define hdcpDmaRead(a,b,c,d)           dmaRead(a,b,c,d)
#define hdcpDmaWrite(a,b,c,d)          dmaWrite(a,b,c,d)

#define hdcpMemcpy(x,y,z)              memcpy(x,y,z)
#define hdcpMemcmp(x,y,z)              memcmp(x,y,z)
#define hdcpMemset(x,y,z)              memset(x,y,z)

#define hdcpBigIntIlwerseMod(a,b,c)    bigIntIlwerseMod(a,b,c)
#define hdcpBigIntMultiplyMod(a,b,c,d) bigIntMultiplyMod(a,b,c,d)
#define hdcpBigIntPowerMod(a,b,c,d,e)  bigIntPowerMod(a,b,c,d,e)
#define hdcpBigIntMod(a,b,c,d)         bigIntMod(a,b,c,d)
#define hdcpBigIntCompare(a,b,c)       bigIntCompare(a,b,c)
#endif

extern BigIntModulus HDCP_Divisor;
extern BigIntModulus HDCP_Modulus;
extern LwU8 g_HDCP_SRM_Generator[SRM_GENERATOR_SIZE];
extern LwU8 g_HDCP_SRM_PublicKey[SRM_MODULUS_SIZE];
extern LwU8 g_HDCP_SRM_TestPublicKey[SRM_MODULUS_SIZE];

/* ------------------------- Prototypes ------------------------------------- */
static LwU32 _hdcpSrmCopyCallback(LwU8 *pBuff, LwU32 offset, LwU32 size, void *pInfo)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_libHdcpauth", "_hdcpSrmCopyCallback");
static void _hdcpToBigInt(LwU8 *pBuf)
    GCC_ATTRIB_SECTION("imem_libHdcpauth", "hdcpToBigInt");
#ifdef HDCPAUTH_HS_BUILD
static void _libHdcpauthHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libHdcpauthHs", "start")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------ */
/*!
 * _libHdcpauthHsEntry
 *
 * @brief Entry function of Hdcpauth HS library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_libHdcpauthHsEntry(void)
{
    return;
}
#endif

/*!
 * @brief Reads data from DMA.
 *
 * This function abstracts dmaRd. It does not require the calling function
 * to have aligned the destination data or set the size to a multiple of
 * DMA_MIN_READ_ALIGNMENT.
 *
 * @param[out]  pDst
 *      Buffer to hold the data read from DMA.
 *
 * @param[in]   pSrc
 *      Pointer to the DMA space to read from.
 *
 * @param[in]   size
 *      The number of bytes to read.
 *
 * @return The number of bytes from from DMA.
 */
#if !UPROC_RISCV
static LwU8 HdcpDmaReadBuffer[HDCP_SHA1_BUFFER_SIZE]
    GCC_ATTRIB_ALIGN(HDCP_DMA_READ_ALIGNMENT);
#endif

LwU32
hdcpAuthDmaRead
(
    LwU8                *pDst,
    PRM_FLCN_MEM_DESC   pSrc,
    LwU32               srcOffset,
    LwU32               size
)
{
#if UPROC_RISCV
    //
    // GSP all global datas located at FB, and no need to use DMA transfer.
    // TODO (Bug 200417270)
    // - syslib needs to provide API that can get VA of data at FB.
    // - Revisit that put all hdcp22 global datas on DMEM instead FB considering security.
    //
    LwU64 srcVAddr = engineFBGPA_FULL_VA_BASE +
        (((LwU64)pSrc->address.hi << 32) | pSrc->address.lo) +
        srcOffset;

    hdcpMemcpy((void *)pDst, (void *)srcVAddr, size);
    return size;
#else
    LwU32   offset    = 0;
    LwU32   bytesRead = 0;
    LwU32   copySize;
    LwU32   dmaSize;
    LwU16   dmaOffset;
    LwU8    unalignedBytes;

    RM_FLCN_MEM_DESC desc;
    desc = *pSrc;

    srcOffset += (LwU8)desc.address.lo;
    desc.address.lo &= 0xFFFFFF00;

    unalignedBytes = srcOffset & (HDCP_DMA_READ_ALIGNMENT - 1);

    while (size > 0)
    {
        dmaOffset = ALIGN_DOWN(srcOffset + offset, HDCP_DMA_READ_ALIGNMENT);

        dmaSize = LW_MIN(ALIGN_UP(size + unalignedBytes,
                                  HDCP_DMA_READ_ALIGNMENT),
                         HDCP_SHA1_BUFFER_SIZE);
        copySize = LW_MIN(size, HDCP_SHA1_BUFFER_SIZE - unalignedBytes);

        if (FLCN_OK != hdcpDmaRead(HdcpDmaReadBuffer, &desc, dmaOffset, dmaSize))
        {
            break;
        }
        hdcpMemcpy((void *)(pDst + offset),
                   (void *)(HdcpDmaReadBuffer + unalignedBytes), copySize);

        size -= copySize;
        offset += copySize;
        bytesRead += copySize;
        unalignedBytes = 0;
    }

    return bytesRead;
#endif
}

/*!
 * @brief   Colwerts a byte array into a big integer.
 *
 * @param[in,out]   pBuf
 *      Pointer to a 20 byte array with the number represented in big endian
 *      format. The function will colwert this to a format usable by the
 *      big integer library.
 */
static void
_hdcpToBigInt
(
    LwU8 *pBuf
)
{
    LwU8    tmp;
    LwU8    i;
    LwU8    j;

    // byte swap individual words
    for (i = 0, j = (LW_HDCPSRM_SIGNATURE_SIZE - 1);
         i < (LW_HDCPSRM_SIGNATURE_SIZE / 2);
         i++, j--)
    {
        tmp     = pBuf[i];
        pBuf[i] = pBuf[j];
        pBuf[j] = tmp;
    }
}

/*!
 * @brief Copy callback for SRM signature verification.
 *
 * Since the SRM is stored in system memory, DMA transfers are required to
 * pass the data into the SHA-1 function.
 *
 * @param[out]  pBuff
 *      The buffer local to the SHA-1 function.
 *
 * @param[in]   offset
 *      The offset within the SRM to begin copying from.
 *
 * @param[in]   size
 *      The number of bytes to copy.
 *
 * @param[in]   pInfo
 *      Pointer to the SRM in system memory.
 *
 * @return The number of bytes copied.
 */
static LwU32
_hdcpSrmCopyCallback
(
    LwU8   *pBuff,
    LwU32   offset,
    LwU32   size,
    void   *pInfo
)
{
    return hdcpAuthDmaRead(pBuff, (PRM_FLCN_MEM_DESC)pInfo, offset, size);
}

/*!
 * @brief Verifies the SRM signature.
 *
 * @param[in]  pSRM
 *          Pointer to an SRM.
 *
 * @param[in]  dwSRMDataLength
 *          The size, in bytes, of the SRM.
 *
 * @param[in]  CtlOptions
 *               Control options of selection of debug/production settings.
 *
 * @returns FLCN_STATUS: FLCN_OK indicates that SRM has been verified.
 */

FLCN_STATUS
hdcpValidateSrm
(
    PRM_FLCN_MEM_DESC  pSRM,
    LwU32              dwSRMDataLength,
    LwU32              CtlOptions
)
{
    LwU32  dwHeader;
    LwU32  dwVRLLength;
    LwU8   srmTemp[32] = {0};
    LwU32  hdcpSrmSig_R[LW_HDCPSRM_SIGNATURE_SIZE/sizeof(LwU32)] = { 0 };
    LwU32  hdcpSrmSig_S[LW_HDCPSRM_SIGNATURE_SIZE/sizeof(LwU32)] = { 0 };
    LwU32  hdcpSrmSha1Hash[LW_HDCPSRM_SIGNATURE_SIZE/sizeof(LwU32)] = { 0 };
    LwU32  u1[SRM_MODULUS_SIZE/sizeof(LwU32)] = { 0 };
    LwU32  u2[SRM_MODULUS_SIZE/sizeof(LwU32)] = { 0 };
    LwU32  w[SRM_MODULUS_SIZE/sizeof(LwU32)];
    FLCN_STATUS ret = FLCN_ERROR;

    // Verify instance and length
    if (!pSRM || dwSRMDataLength < LW_HDCPSRM_MIN_LENGTH)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (hdcpAuthDmaRead(srmTemp, pSRM, 0, LW_HDCPSRM_SIZE) != LW_HDCPSRM_SIZE)
    {
        return FLCN_ERR_DMA_GENERIC;
    }

    // Verify ID
    dwHeader = (srmTemp[LW_HDCPSRM_SRM_ID] << 8) ^
                srmTemp[LW_HDCPSRM_RESERVED1];
    if (dwHeader != 0x8000)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    dwVRLLength = (srmTemp[LW_HDCPSRM_VRL_LENGTH_HI] << 16) ^
        (srmTemp[LW_HDCPSRM_VRL_LENGTH_MED] << 8) ^
        srmTemp[LW_HDCPSRM_VRL_LENGTH_LO];

    if (dwSRMDataLength != dwVRLLength + LW_HDCPSRM_VRL_LENGTH)
    {
        //
        // SRM length should equal to VRL length + 5 (header + version +
        // reserved)
        //
        return FLCN_ERROR;
    }

    // Callwlate the SHA-1
    hdcpAttachAndLoadOverlay(HDCP_SHA1);
    libSha1((LwU8 *)hdcpSrmSha1Hash, (void *) pSRM, dwSRMDataLength -
            (LW_HDCPSRM_SIGNATURE_SIZE << 1), (FlcnSha1CopyFunc (*))(_hdcpSrmCopyCallback));
    hdcpDetachOverlay(HDCP_SHA1);

    _hdcpToBigInt((LwU8 *)hdcpSrmSha1Hash);

    // Get the R portion of the signature
    if ((hdcpAuthDmaRead((LwU8 *)hdcpSrmSig_R, pSRM,
                         dwSRMDataLength - LW_HDCPSRM_SIGNATURE_SIZE * 2,
                         LW_HDCPSRM_SIGNATURE_SIZE)) != LW_HDCPSRM_SIGNATURE_SIZE)
    {
        return FLCN_ERR_DMA_GENERIC;
    }

    _hdcpToBigInt((LwU8 *)hdcpSrmSig_R);

    // Get the S portion of the signature
    if ((hdcpAuthDmaRead((LwU8 *)hdcpSrmSig_S, pSRM,
                         dwSRMDataLength - LW_HDCPSRM_SIGNATURE_SIZE,
                         LW_HDCPSRM_SIGNATURE_SIZE)) != LW_HDCPSRM_SIGNATURE_SIZE)
    {
        return FLCN_ERR_DMA_GENERIC;
    }

    _hdcpToBigInt((LwU8 *)hdcpSrmSig_S);

    //
    // Verify the DSA signature.
    //
    // The following values need to be made available to the signature
    // verifier.
    //
    // p = prime modulus: Provided by HDCP spec (HDCP_Modulus)
    // q = prime divisor: Provided by HDCP spec (HDCP_Divisor)
    // g = generator: Provided by HDCP spec (g_HDCP_SRM_Generator)
    // y = public key: Provided by sender (g_HDCP_SRM_PublicKey)
    // r = SRM signature, "r" value: Found in SRM (hdcpSrmSig_R)
    // s = SRM signature, "s" value: Found in SRM (hdcpSrmSig_S)
    // h = SHA-1(message): Callwlated from SRM (hdcpSrmSha1Hash)
    //
    // Steps needed to verify the DSA signature:
    //
    //  w = pow(s, -1) % q
    // u1 = (sha1(m) * w) % q
    // u2 = (r * w) % q
    //  v = ((pow(g, u1) * pow(y, u2)) % p) % q
    //
    // if (v == r) then the signature is verified, and the verifier can have
    // high confidence that the received message was sent by the party holding
    // the public key
    //

    hdcpAttachAndLoadOverlay(HDCP_BIGINT);

    // w = pow(s, -1) % q
    hdcpBigIntIlwerseMod(w, hdcpSrmSig_S, &HDCP_Divisor);

    // u1 = (sha1(m) * w) % q
    hdcpBigIntMultiplyMod(u1, hdcpSrmSha1Hash, w, &HDCP_Divisor);

    // u2 = (r * w) % q
    hdcpBigIntMultiplyMod(u2, hdcpSrmSig_R, w, &HDCP_Divisor);

    //
    // Since (pow(g, u1) * pow(y, u2)) may too large for the buffers we use,
    // we need to rely on a property of mod to keep the values in more
    // manageable chunks.
    //
    // (a * b) % c = ((a % c) * (b % c)) % c
    //

    //
    // Don't need the value stored in w anymore; reuse the variable
    // w = pow(g, u1) % p
    //
    hdcpBigIntPowerMod(w, (LwU32 *)g_HDCP_SRM_Generator, u1,
        &HDCP_Modulus, SRM_MODULUS_SIZE / sizeof(LwU32));

    //
    // Don't need the value stored in u1 anymore; reuse the variable
    // u1 = pow(y, u2) % p
    //
   if (FLD_TEST_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, CtlOptions))
    {
        hdcpBigIntPowerMod(u1, (LwU32 *)g_HDCP_SRM_PublicKey, u2, &HDCP_Modulus,
            SRM_MODULUS_SIZE / sizeof(LwU32));
    }
    else
    {
        hdcpBigIntPowerMod(u1, (LwU32 *)g_HDCP_SRM_TestPublicKey, u2, &HDCP_Modulus,
            SRM_MODULUS_SIZE / sizeof(LwU32));
    }

    //
    // Don't need the value store in u2 anymore; reuse the variable
    // u2 = (w * u1) % p
    //
    hdcpBigIntMultiplyMod(u2, w, u1, &HDCP_Modulus);

    // At this point, we have v = u2 % q; reuse w as v
    hdcpBigIntMod(w, u2, &HDCP_Divisor, SRM_MODULUS_SIZE / sizeof(LwU32));

    // If (w == r), the signature is verified
    ret = (hdcpBigIntCompare(w, hdcpSrmSig_R,
               SRM_DIVISOR_SIZE / sizeof(LwU32)) == 0) ? FLCN_OK : FLCN_ERROR;

    hdcpDetachOverlay(HDCP_BIGINT);

    return ret;
}
