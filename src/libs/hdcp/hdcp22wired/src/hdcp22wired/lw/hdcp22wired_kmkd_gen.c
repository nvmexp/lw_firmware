/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_kmkd_gen.c
 * @brief  Hdcp22 wrapper Functions for security
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_aes.h"
#include "csb.h"
#include "common_hslib.h"
#include "seapi.h"

/* ------------------------ Types definitions ------------------------------- */
#define RSA_HDCP_TARGET_MSVLD       1
#define RSA_OAEP_BUFFERSIZE         128
#define RSA_OAEP_HLEN               32
#define RSA_OAEP_CLEN               4
#define RSA_SEED_BUFFER             128

// This buffer size is sufficient for 1024-bit RSAES-OAEP decryption
#define RSA_MAX_SEEDLEN             (RSA_SEED_BUFFER - RSA_OAEP_CLEN)

// AES block size constants
#define RSA_AES_B                   16
#define RSA_AES_DW                  4
#define RSA_AES_RK                  44

//
// rsaOaepLHash is the hash of a "label" string defined in PKCS #1 v2.1 page 18.
// DCP chooses this value to be the SHA-256 of the empty string.
//
LwU8 rsaOaepLHash[RSA_OAEP_HLEN] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
    0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

// Fixed seed value for RSA-OAEP
LwU8 rsaOaepTestSeed[RSA_OAEP_HLEN] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};

/* ------------------------ External definitions ---------------------------- */
#ifdef GSPLITE_RTOS
static void _hdcp22wiredGenerateKmkdEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateKmkd", "start");
#endif

static FLCN_STATUS _hdcp22GetDkey(LwU8 *pRtx, LwU64 *pRrx, LwU8 *pKm, LwU8 *pDk, LwU64  dkeyCtr)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateKmkd", "_hdcp22GetDkey");
static FLCN_STATUS _hdcp22RsaOaep(LwU8 *pEkpub, LwU8* pKm, LwU32* recvModulus, LwU32* dwExponent)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateKmkd", "_hdcp22RsaOaep");
static FLCN_STATUS  _rsaOaepMaskGenFunction(LwU8* pMask, LwU32 nMaskLen, LwU8* pSeed, LwU32 nSeedLen)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateKmkd", "_rsaOaepMaskGenFunction");
static FLCN_STATUS  _rsaOaepEncode(LwU8* pOutput, const LwU8* pInput, const LwU32 dbsize)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateKmkd", "_rsaOaepEncode");

FLCN_STATUS hdcp22wiredKmKdGenHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateKmkd", "hdcp22wiredKmKdGenHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredGenerateKmkdEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*!
 * _hdcp22GetDkey
 *
 * @brief Generates dkey as specified in HDCP spec
 * @param[in]  pRtx         Pointer to rTx.
 * @param[in]  pRrx         Pointer to rRx.
 * @param[in]  pMasterKey   Pointer to master key(Km).
 * @param[out] pDk          Pointer to derived key.
 * @param[in]  dkeyCtr      dkey counter
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
_hdcp22GetDkey
(
    LwU8   *pRtx,
    LwU64  *pRrx,
    LwU8   *pMasterKey,
    LwU8   *pDk,
    LwU64   dkeyCtr
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU8       *pRtxCtr = NULL;
    LwU8       *pKm     = NULL;
    LwU8       *pRn     = NULL;
    LwU64       kmRnXor  = 0x0;
    LwU8        tmpBuf[HDCP22_SIZE_DKEY_WORKINGBUF + HDCP22_DKEY_ALIGNMENT];
    LwU64      *pWorkBuf = (LwU64 *)LW_ALIGN_UP((LwUPtr)tmpBuf, HDCP22_DKEY_ALIGNMENT);     // LwU64 aligned because LwU64 xor operation.
    LwU64       rRn[HDCP22_SIZE_R_N/LW_SIZEOF32(LwU64)];

    if ((dkeyCtr==0) || (dkeyCtr==1))
    {
        // dkey0, dkey1 using rn as 0.
        HDCP22WIRED_SEC_ACTION_MEMSET(rRn, 0, HDCP22_SIZE_R_N);
    }
    else
    {
        // Rn is only needed for dkey2.
        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_RN,
                                                         (LwU8 *)rRn));
    }

    HDCP22WIRED_SEC_ACTION_MEMSET(pDk, 0, HDCP22_SIZE_DKEY);
    HDCP22WIRED_SEC_ACTION_MEMSET(tmpBuf, 0, LW_SIZEOF32(tmpBuf));

    pRtxCtr = (LwU8 *)pWorkBuf; // 128-bits to store AES-CTR-MSG
    pKm     = pRtxCtr + 16;     // 128-bits to store Km
    pRn     = pKm     + 16;     //  64-bits to store Rn

    HDCP22WIRED_SEC_ACTION_MEMCPY(pRtxCtr, (LwU8 *)pRtx, HDCP22_SIZE_R_TX);
    HDCP22WIRED_SEC_ACTION_MEMCPY((LwU8*)(pRtxCtr + HDCP22_SIZE_R_TX), (LwU8 *)&dkeyCtr, HDCP22_SIZE_DKEY_CTR);

    // According to spec, Ctr should be in Big-endian. So swap the endianness
    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS((LwU8*)(pRtxCtr + HDCP22_SIZE_R_TX),
                                           (LwU8*)(pRtxCtr + HDCP22_SIZE_R_TX), HDCP22_SIZE_DKEY_CTR);

    // FLCN code is defaulted to HDCP22
    *((LwU64 *)(pRtxCtr + HDCP22_SIZE_R_TX)) ^= *pRrx;

    // Now XOR Rn with least significant 64-bits of Km.
    HDCP22WIRED_SEC_ACTION_MEMCPY(pKm, pMasterKey, HDCP22_SIZE_KM);
    HDCP22WIRED_SEC_ACTION_MEMCPY(pRn, (LwU8 *)rRn, HDCP22_SIZE_R_N);
    kmRnXor = *((LwU64*)(pKm + 8)) ^ *((LwU64*)pRn);
    HDCP22WIRED_SEC_ACTION_MEMCPY((LwU8*)(pKm+8), (LwU8*)&kmRnXor, 8);

    // Using AES-CTR128 to generate dKey.
#if defined(HDCP22_USE_SCP_GEN_DKEY)
    status = hdcp22GenerateDkeyScpHs(pRtxCtr, pKm, pDk);
#else
    status = hdcp22wiredGenerateDkeySw(pRtxCtr, pKm, pDk);
#endif // HDCP22_USE_SCP_GEN_DKEY


label_return:
    // Scrub tmpBuf before exit.
    HDCP22WIRED_SEC_ACTION_MEMSET(tmpBuf, 0, LW_SIZEOF32(tmpBuf));

    return status;
}

/*!
 * @brief MGF1 is a "Mask Generation Function" based on a hash function
 * see PKCS #1 v2.1 page 48. DCP chooses SHA-256 with 32-bit counter.
 *
 * @param[in]  pMask
 * @param[in]  nMaskLen
 * @param[in]  pSeed
 * @param[in]  nSeedLen
 */
static FLCN_STATUS
_rsaOaepMaskGenFunction
(
    LwU8 *pMask,
    LwU32 nMaskLen,
    LwU8 *pSeed,
    LwU32 nSeedLen
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 i;
    LwU32 nBytesToCopy;
    LwU8 mgfseed[RSA_SEED_BUFFER];
    LwU8 mgfhash[RSA_OAEP_HLEN];

    if (nSeedLen > RSA_MAX_SEEDLEN)
    {
        return FLCN_ERROR;
    }

    HDCP22WIRED_SEC_ACTION_MEMSET(mgfseed, 0, RSA_SEED_BUFFER);
    HDCP22WIRED_SEC_ACTION_MEMSET(mgfhash, 0, RSA_OAEP_HLEN);
    HDCP22WIRED_SEC_ACTION_MEMCPY(mgfseed, pSeed, nSeedLen);

    // Load/Unload overlay will only be called for prevolta (LS version)
    hdcpAttachAndLoadOverlay(HDCP22WIRED_LIB_SHA);

    for (i = 0; nMaskLen > 0; i++)
    {
        // The 32-bit counter must be appended to the seed in big-endian order
        mgfseed[nSeedLen]   = (LwU8)(i >> 24);
        mgfseed[nSeedLen+1] = (LwU8)(i >> 16);
        mgfseed[nSeedLen+2] = (LwU8)(i >> 8 );
        mgfseed[nSeedLen+3] = (LwU8)(i      );

        CHECK_STATUS(HDCP22WIRED_SEC_ACTION_SHA256(mgfhash, mgfseed, nSeedLen + RSA_OAEP_CLEN));

        nBytesToCopy = (nMaskLen >= RSA_OAEP_HLEN) ? RSA_OAEP_HLEN : nMaskLen;
        HDCP22WIRED_SEC_ACTION_MEMCPY(pMask, mgfhash, nBytesToCopy);
        nMaskLen -= nBytesToCopy;
        pMask    += nBytesToCopy;
    }

label_return:
    hdcpDetachOverlay(HDCP22WIRED_LIB_SHA);

    return status;
}

static FLCN_STATUS
_rsaOaepEncode
(
    LwU8        *pOutput,
    const LwU8  *pInput,
    const LwU32 dbsize
)
{
    LwU8        seed[RSA_OAEP_HLEN];
    LwU8        seedMask[RSA_OAEP_HLEN];
    LwU8        db[RSA_OAEP_BUFFERSIZE - RSA_OAEP_HLEN - 1];
    LwU8        dbMask[RSA_OAEP_BUFFERSIZE - RSA_OAEP_HLEN - 1];
    LwU32       i = 0;
    FLCN_STATUS status = FLCN_OK;

    if (dbsize > RSA_OAEP_BUFFERSIZE - 2 * RSA_OAEP_HLEN - 2)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pOutput[0] = 0;

    HDCP22WIRED_SEC_ACTION_MEMSET(db, 0, sizeof(db));
    HDCP22WIRED_SEC_ACTION_MEMCPY(db, (LwU8 *) rsaOaepLHash, RSA_OAEP_HLEN);
    HDCP22WIRED_SEC_ACTION_MEMCPY(db + sizeof(db) - dbsize, (LwU8 *) pInput, dbsize);
    db[sizeof(db) - dbsize - 1] = 0x1;

    HDCP22WIRED_SEC_ACTION_MEMCPY(seed, (LwU8 *)rsaOaepTestSeed, RSA_OAEP_HLEN);
    CHECK_STATUS(_rsaOaepMaskGenFunction(dbMask, sizeof(dbMask), seed, sizeof(seed)));

    for (i = 0; i < sizeof(dbMask); i++)
    {
        pOutput[i + RSA_OAEP_HLEN + 1] = db[i] ^ dbMask[i];
    }

    CHECK_STATUS(_rsaOaepMaskGenFunction(seedMask, sizeof(seedMask), pOutput + RSA_OAEP_HLEN + 1, sizeof(dbMask)));
    for (i = 0; i < sizeof(seedMask); i++)
    {
        pOutput[i + 1] = seedMask[i] ^ seed[i];
    }

label_return:
    return status;
}

/*!
 * @brief This function is to support hdcp22 Rsa operation.
 *
 * @param[in]  pEkpub
 * @param[in]  pKm
 * @param[in]  recvModulus
 * @param[in]  dwExponent
 */
static FLCN_STATUS
_hdcp22RsaOaep
(
    LwU8   *pEkpub,
    LwU8   *pKm,
    LwU32  *pRecvModulus,
    LwU32  *pDwExponent
)
{
    FLCN_STATUS status = FLCN_OK;
    // Align the computation buffers with LwU32 for callwlation.
    LwU32       rsaEncBuffer[RSA_OAEP_BUFFERSIZE/sizeof(LwU32)];
    LwU32       eKpubBuffer[RSA_OAEP_BUFFERSIZE/sizeof(LwU32)];

    HDCP22WIRED_SEC_ACTION_MEMSET(rsaEncBuffer, 0, RSA_OAEP_BUFFERSIZE);
    HDCP22WIRED_SEC_ACTION_MEMSET(eKpubBuffer, 0, RSA_OAEP_BUFFERSIZE);

    CHECK_STATUS(_rsaOaepEncode((LwU8 *)rsaEncBuffer, pKm, RSA_AES_B));

#ifdef HDCP22_USE_HW_RSA
    if (HDCP22WIRED_SEC_ACTION_HW_RSA_MODULAR_EXP(HDCP22_SIZE_RX_MODULUS_8,
                                                  pRecvModulus,
                                                  pDwExponent,
                                                  rsaEncBuffer,
                                                  eKpubBuffer) != FLCN_OK)
    {
        status = FLCN_ERR_HDCP22_RSA_HW;
    }
#else
    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(rsaEncBuffer, rsaEncBuffer,
                                           RSA_OAEP_BUFFERSIZE);

    // Load/Unload overlay will only be called for prevolta (LS version)
    hdcpAttachAndLoadOverlay(HDCP_BIGINT);
    status = hdcp22SwRsaModularExp(HDCP22_SIZE_RX_MODULUS_8,
                                   (LwU8 *)pRecvModulus,
                                   pDwExponent,
                                   rsaEncBuffer,
                                   eKpubBuffer);
    hdcpDetachOverlay(HDCP_BIGINT);
#endif

    if (status != FLCN_OK)
    {
        goto label_return;
    }

    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(pEkpub, eKpubBuffer,
                                           RSA_OAEP_BUFFERSIZE);

label_return:
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to reset authentication session.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredKmKdGenHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_KMKD_GEN_ARG pActionArg  = &pArg->action.secActionKmKdGen;
    SELWRE_ACTION_TYPE          prevState   = SELWRE_ACTION_ILWALID;
    FLCN_STATUS status          = FLCN_OK;
    LwBool      bAesOvlAttached = LW_FALSE;
    LwU32       dwExponent      = 0;
    LwU32       rTx[HDCP22_SIZE_R_TX/LW_SIZEOF32(LwU32)];
    LwU32       km[HDCP22_SIZE_KM/LW_SIZEOF32(LwU32)];
    LwU32       eKm[HDCP22_SIZE_EKM/LW_SIZEOF32(LwU32)];
    LwU8       *pKpubRx = pActionArg->kPubRxEkm.kPubRx;
    LwU8        kd[HDCP22_SIZE_KD + HDCP22_DKEY_ALIGNMENT];
    LwU8       *pKd = (LwU8 *)LW_ALIGN_UP((LwUPtr)kd, HDCP22_DKEY_ALIGNMENT);

    // Check if the previous state.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));

    //
    // TODO: change state check to SELWRE_ACTION_CERTIFICATE_SRM_CHECKED if RM
    // assigned SRM or pre-built SRM suppported.
    //
    if(prevState != SELWRE_ACTION_VERIFY_CERTIFICATE)
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

    // Reset all buffers.
    HDCP22WIRED_SEC_ACTION_MEMSET(km, 0, HDCP22_SIZE_KM);
    HDCP22WIRED_SEC_ACTION_MEMSET(eKm, 0, HDCP22_SIZE_EKM);
    HDCP22WIRED_SEC_ACTION_MEMSET(pKd, 0, HDCP22_SIZE_KD);

    // Retrieve rTx.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_RTX,
                                                     (LwU8 *)rTx));

    // Generate km here and not store in secure memory.
    CHECK_STATUS(hdcp22wiredChooseRandNum(km, HDCP22_RAND_TYPE_KM,
                                          HDCP22_SIZE_KM));

    // TODO: Check pKpubRx's hash to make sure it's not tainted.

    // Generate eKm, Load/Unload overlay will only be called for prevolta (LS version)
    HDCP22WIRED_SEC_ACTION_MEMCPY(&dwExponent, &pKpubRx[HDCP22_SIZE_RX_MODULUS_8],
                 HDCP22_SIZE_RX_EXPONENT_8);
    CHECK_STATUS(_hdcp22RsaOaep((LwU8 *)eKm, (LwU8 *)km, (LwU32 *)pKpubRx, &dwExponent));
    HDCP22WIRED_SEC_ACTION_MEMCPY(pActionArg->kPubRxEkm.eKm, eKm, HDCP22_SIZE_EKM);

    // Load/Unload overlay will only be called for prevolta (LS version)
    hdcpAttachAndLoadOverlay(HDCP22WIRED_AES);
    bAesOvlAttached = LW_TRUE;

    // Generate kd(dkey0||dkey1) and store in secure memory.
    // TODO: retrieve rRx from secure memory which stored in certificate validation.
    // dkey0
    CHECK_STATUS(_hdcp22GetDkey((LwU8 *)rTx, pActionArg->rRx, (LwU8 *)km, pKd, 0));
    // dkey1
    CHECK_STATUS(_hdcp22GetDkey((LwU8 *)rTx, pActionArg->rRx, (LwU8 *)km, pKd + HDCP22_SIZE_DKEY, 1));
    // Save confidential kd(dkey0 || dkey1).
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_KD,
                                                    pKd,
                                                    LW_FALSE));

    // dkey2
    CHECK_STATUS(_hdcp22GetDkey((LwU8 *)rTx, pActionArg->rRx, (LwU8 *)km, pKd, 2));
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_DKEY,
                                                    pKd,
                                                    LW_FALSE));

    // Update Previous state.
    prevState = SELWRE_ACTION_KMKD_GEN;
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_PREV_STATE,
                                                    (LwU8 *)&prevState,
                                                    LW_FALSE));
label_return:

    if (bAesOvlAttached)
    {
        hdcpDetachOverlay(HDCP22WIRED_AES);
        bAesOvlAttached = LW_FALSE;
    }
    return status;
}

