/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DISPFLCN_SELWREACTION_H
#define DISPFLCN_SELWREACTION_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
// Only include necessary definition header files.
#include "lwuproc.h"
#ifdef GSPLITE_RTOS
#include "scp_common.h"
#ifdef HDCP22_USE_HW_SHA
#include "lib_intfcshahw.h"
#else
#include "sha256_hs.h"
#endif // HDCP22_USE_HW_SHA
#else
#include "sha256.h"
#endif
#include "flcnifhdcp22wired.h"
// TODO: HDCP22_STREAM needs to be duplicated/moved to library header file
#include "hdcp22wired_protocol.h"

/* ------------------------ External Defines -------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#if defined(HDCP22_KMEM_ENABLED)
#if defined(HDCP22_USE_SCP_ENCRYPT_SECRET) || defined(HDCP22_CHECK_STATE_INTEGRITY)
#error "With KMEM supported, no need enabling scp encrypt and hash integrity check"
#endif
#endif // HDCP22_KMEM_ENABLED

#if defined(GSPLITE_RTOS) && !defined(UPROC_RISCV)
// Volta+ Falcon
#define HDCP22WIRED_SELWRE_ACTION(X, pArg)                  libIntfcHdcp22wiredSelwreAction(pArg)

#define HDCP22WIRED_SEC_ACTION_MEMCPY                       memcpy_hs
#define HDCP22WIRED_SEC_ACTION_MEMSET                       memset_hs
#define HDCP22WIRED_SEC_ACTION_MEMCMP                       memcmp_hs

#define HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS              swapEndiannessHs
#define HDCP22WIRED_SEC_ACTION_BIGINT_MODULUS_INIT          bigIntModulusInit_hs
#define HDCP22WIRED_SEC_ACTION_BIGINT_COMPARE               bigIntCompare_hs
#define HDCP22WIRED_SEC_ACTION_BIGINT_MOD                   bigIntMod_hs
#define HDCP22WIRED_SEC_ACTION_BIGINT_POWER_MOD             bigIntPowerMod_hs
#define HDCP22WIRED_SEC_ACTION_BIGINT_ILWERSE_MOD           bigIntIlwerseMod_hs
#define HDCP22WIRED_SEC_ACTION_BIGINT_MULTIPLY_MOD          bigIntMultiplyMod_hs

#ifdef HDCP22_USE_HW_SHA
#define HDCP22WIRED_SEC_ACTION_SHA256(digest, msg, len)     shahwSha256_hs(msg, len, digest)
#define HDCP22WIRED_SEC_ACTION_HMAC_SHA256(digest, msg, msgSize, key, keySize)  \
                                                            shahwHmacSha256_hs(msg, msgSize, key, keySize, digest)
#define HDCP22WIRED_SEC_ACTION_SHA1(digest, msg, len)       shahwSha1_hs(msg, len, digest)
#else
// Existing SW sha lib doesn't have return status.
static LW_FORCEINLINE FLCN_STATUS
_hdcp22wired_sha256_hs(LwU8 *pDigest, const LwU8 *pMsg, LwU32 len)
{ 
    sha256_hs(pDigest, pMsg, len);
    return FLCN_OK; 
}

static LW_FORCEINLINE FLCN_STATUS
_hdcp22wired_hmacSha256_hs(LwU8 *pDigest, const LwU8 *pMsg, const LwU32 msgSize,
                       const LwU8 *pKey, const LwU32 keySize)
{ 
    hmacSha256_hs(pDigest, pMsg, msgSize, pKey, keySize);
    return FLCN_OK; 
}

#define HDCP22WIRED_SEC_ACTION_SHA256(digest, msg, len)     _hdcp22wired_sha256_hs(digest, msg, len)
#define HDCP22WIRED_SEC_ACTION_HMAC_SHA256(digest, msg, msgSize, key, keySize)  \
                                                            _hdcp22wired_hmacSha256_hs(digest, msg, msgSize, key, keySize)
#define HDCP22WIRED_SEC_ACTION_SHA1_INITIALIZE              sha1Initialize_hs
#define HDCP22WIRED_SEC_ACTION_SHA1_UPDATE                  sha1Update_hs
#define HDCP22WIRED_SEC_ACTION_SHA1_FINAL                   sha1Final_hs
#endif // HDCP22_USE_HW_SHA

#ifdef LIB_CCC_PRESENT
// TODO: Call LibCCC wrapper once implemented in LibCCC. Tracking Bug: 200753570
#define HDCP22WIRED_SEC_ACTION_HW_RSA_MODULAR_EXP           hdcp22LibCccHwRsaModularExpHs
#else
#define HDCP22WIRED_SEC_ACTION_HW_RSA_MODULAR_EXP(keySize, pModulus, pExponent, pSignature, pOutput)    \
                                                            seRsaModularExpHs(keySize * 8, pModulus, pExponent, pSignature, pOutput)
#endif // LIB_CCC_PRESENT

#define HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS               hdcp22wiredGetMaxNoOfSorsHeadsHs_HAL
#define HDCP22WIRED_ENCRYPTION_ACTIVE                       hdcp22wiredEncryptionActiveHs_HAL
#define HDCP22WIRED_IS_TYPE1LOCK_ACTIVE                     hdcp22wiredIsType1LockActiveHs_HAL
#define HDCP22WIRED_SPIN_WAIT_US(uSec)                      osPTimerCondSpinWaitNs_hs(NULL, NULL, uSec)
#elif defined(UPROC_RISCV)
// RISC-V
extern void usleep(LwU64 uSec);;
extern FLCN_STATUS hdcp22wiredMpSwitchSelwreAction(void);

#define SELWRE_ACTION_STATUS_ALIGNMENT          16  // SCP_BUF_ALIGNMENT

typedef enum
{
    SELWRE_ACTION_STATE_ILWALID             = 0x0000,
    SELWRE_ACTION_STATE_PROCESSING          = 0x1234,
    SELWRE_ACTION_STATE_COMPLETED           = 0x8765,
} SELWRE_ACTION_STATE;

typedef union
{
    struct
    {
        LwU32                               actionState;
        FLCN_STATUS                         actionResult;
    } state;

    LwU8                                    alignedBuf[SELWRE_ACTION_STATUS_ALIGNMENT];
} SELWRE_ACTION_STATUS;

#define HDCP22WIRED_SPIN_WAIT_US(uSec)                      usleep(uSec)
#define HDCP22WIRED_SELWRE_ACTION(X, pArg)                  hdcp22wiredMpSwitchSelwreAction()

#define HDCP22WIRED_SEC_ACTION_MEMCPY                       memcpy
#define HDCP22WIRED_SEC_ACTION_MEMSET                       memset
#define HDCP22WIRED_SEC_ACTION_MEMCMP                       memcmp

#define HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS              swapEndiannessHs
#define HDCP22WIRED_SEC_ACTION_BIGINT_MODULUS_INIT          bigIntModulusInit
#define HDCP22WIRED_SEC_ACTION_BIGINT_COMPARE               bigIntCompare
#define HDCP22WIRED_SEC_ACTION_BIGINT_MOD                   bigIntMod
#define HDCP22WIRED_SEC_ACTION_BIGINT_POWER_MOD             bigIntPowerMod
#define HDCP22WIRED_SEC_ACTION_BIGINT_ILWERSE_MOD           bigIntIlwerseMod
#define HDCP22WIRED_SEC_ACTION_BIGINT_MULTIPLY_MOD          bigIntMultiplyMod

// Existing SW sha lib doesn't have return status.
static LW_FORCEINLINE FLCN_STATUS
_hdcp22wired_sha256(LwU8 *pDigest, const LwU8 *pMsg, LwU32 len)
{ 
    sha256(pDigest, pMsg, len);
    return FLCN_OK; 
}

static LW_FORCEINLINE FLCN_STATUS
_hdcp22wired_hmacSha256(LwU8 *pDigest, const LwU8 *pMsg, const LwU32 msgSize,
                       const LwU8 *pKey, const LwU32 keySize)
{ 
    hmacSha256(pDigest, pMsg, msgSize, pKey, keySize);
    return FLCN_OK; 
}

#define HDCP22WIRED_SEC_ACTION_SHA256(digest, msg, len)     _hdcp22wired_sha256(digest, msg, len)
#define HDCP22WIRED_SEC_ACTION_HMAC_SHA256(digest, msg, msgSize, key, keySize)  \
                                                            _hdcp22wired_hmacSha256(digest, msg, msgSize, key, keySize)
#define HDCP22WIRED_SEC_ACTION_HW_RSA_MODULAR_EXP           "Error HW RSA not supported"
#define HDCP22WIRED_SEC_ACTION_SHA1_INITIALIZE              sha1Initialize
#define HDCP22WIRED_SEC_ACTION_SHA1_UPDATE                  sha1Update
#define HDCP22WIRED_SEC_ACTION_SHA1_FINAL                   sha1Final
#define HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS               hdcp22wiredGetMaxNoOfSorsHeadsHs_HAL
#define HDCP22WIRED_ENCRYPTION_ACTIVE                       hdcp22wiredEncryptionActiveHs_HAL
#define HDCP22WIRED_IS_TYPE1LOCK_ACTIVE                     hdcp22wiredIsType1LockActiveHs_HAL
#else
// PASCAL
#define HDCP22WIRED_SELWRE_ACTION_START_SESSION(pArg)       hdcp22wiredStartSessionHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_VERIFY_CERTIFICATE(pArg)  hdcp22wiredVerifyRxCertificateHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_KMKD_GEN(pArg)            hdcp22wiredKmKdGenHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_HPRIME_VALIDATION(pArg)   hdcp22wiredHprimeValidationHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_LPRIME_VALIDATION(pArg)   hdcp22wiredLprimeValidationHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_EKS_GEN(pArg)             hdcp22wiredEksGenHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_CONTROL_ENCRYPTION(pArg)  hdcp22wiredControlEncryptionHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_VPRIME_VALIDATION(pArg)   hdcp22wiredVprimeValidationHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_MPRIME_VALIDATION(pArg)   hdcp22wiredMprimeValidationHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_WRITE_DP_ECF(pArg)        hdcp22wiredWriteDpEcfHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_END_SESSION(pArg)         hdcp22wiredEndSessionHandler(pArg)
#define HDCP22WIRED_SELWRE_ACTION_SRM_REVOCATION(pArg)      hdcp22wiredSrmRevocationHandler(pArg)

#define HDCP22WIRED_SELWRE_ACTION(X, pArg)                  HDCP22WIRED_SELWRE_ACTION_##X(pArg)

#define HDCP22WIRED_SEC_ACTION_MEMCPY                       memcpy
#define HDCP22WIRED_SEC_ACTION_MEMSET                       memset
#define HDCP22WIRED_SEC_ACTION_MEMCMP                       memcmp

#define HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS              swapEndianness
#define HDCP22WIRED_SEC_ACTION_BIGINT_MODULUS_INIT          bigIntModulusInit
#define HDCP22WIRED_SEC_ACTION_BIGINT_COMPARE               bigIntCompare
#define HDCP22WIRED_SEC_ACTION_BIGINT_MOD                   bigIntMod
#define HDCP22WIRED_SEC_ACTION_BIGINT_POWER_MOD             bigIntPowerMod
#define HDCP22WIRED_SEC_ACTION_BIGINT_ILWERSE_MOD           bigIntIlwerseMod
#define HDCP22WIRED_SEC_ACTION_BIGINT_MULTIPLY_MOD          bigIntMultiplyMod

// Existing SW sha lib doesn't have return status.
static LW_FORCEINLINE FLCN_STATUS
_hdcp22wired_sha256(LwU8 *pDigest, const LwU8 *pMsg, LwU32 len)
{ 
    sha256(pDigest, pMsg, len);
    return FLCN_OK; 
}

static LW_FORCEINLINE FLCN_STATUS
_hdcp22wired_hmacSha256(LwU8 *pDigest, const LwU8 *pMsg, const LwU32 msgSize,
                       const LwU8 *pKey, const LwU32 keySize)
{ 
    hmacSha256(pDigest, pMsg, msgSize, pKey, keySize);
    return FLCN_OK; 
}

#define HDCP22WIRED_SEC_ACTION_SHA256(digest, msg, len)     _hdcp22wired_sha256(digest, msg, len)
#define HDCP22WIRED_SEC_ACTION_HMAC_SHA256(digest, msg, msgSize, key, keySize)  \
                                                            _hdcp22wired_hmacSha256(digest, msg, msgSize, key, keySize)
#define HDCP22WIRED_SEC_ACTION_SHA1_INITIALIZE              sha1Initialize
#define HDCP22WIRED_SEC_ACTION_SHA1_UPDATE                  sha1Update
#define HDCP22WIRED_SEC_ACTION_SHA1_FINAL                   sha1Final
#define HDCP22WIRED_SEC_ACTION_HW_RSA_MODULAR_EXP           "Error preVolta doesn't support HW RSA"
#define HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS               hdcp22wiredGetMaxNoOfSorsHeads_HAL
#define HDCP22WIRED_ENCRYPTION_ACTIVE                       hdcp22wiredEncryptionActive_HAL
#define HDCP22WIRED_IS_TYPE1LOCK_ACTIVE                     hdcp22wiredIsType1LockActive_HAL
#define HDCP22WIRED_SPIN_WAIT_US                            OS_PTIMER_SPIN_WAIT_US
#endif

/*!
 * @brief secure action type enum.
 */
typedef enum
{
    // TODO, update after provided random enum values.
    SELWRE_ACTION_REG_ACCESS                = 0xf132,
    SELWRE_ACTION_SCP_CALLWLATE_HASH        = 0xabcd,
    SELWRE_ACTION_SCP_GEN_DKEY              = 0x1234,

    // New added LS/HS secure actions.
    SELWRE_ACTION_START_SESSION             = 0xd132,
    SELWRE_ACTION_VERIFY_CERTIFICATE        = 0xd932,
    SELWRE_ACTION_KMKD_GEN                  = 0xd133,
    SELWRE_ACTION_HPRIME_VALIDATION         = 0xd134,
    SELWRE_ACTION_LPRIME_VALIDATION         = 0xd137,
    SELWRE_ACTION_EKS_GEN                   = 0xd135,
    SELWRE_ACTION_CONTROL_ENCRYPTION        = 0xd136,
    SELWRE_ACTION_VPRIME_VALIDATION         = 0xd138,
    SELWRE_ACTION_MPRIME_VALIDATION         = 0xd139,
    SELWRE_ACTION_WRITE_DP_ECF              = 0xd240,
    SELWRE_ACTION_END_SESSION               = 0xd241,
    SELWRE_ACTION_REPEATER_START_SESSION    = 0xd857,   // special startSession to retrieve kd only.
    SELWRE_ACTION_SRM_REVOCATION            = 0xd732,
    SELWRE_ACTION_ILWALID                   = 0x00,
} SELWRE_ACTION_TYPE, *PSELWRE_ACTION_TYPE;

typedef enum
{
    HDCP22_SECRET_SOR_NUM               = 0,
    HDCP22_SECRET_LINK_IDX,
    HDCP22_SECRET_IS_MST,
    HDCP22_SECRET_IS_REPEATER,
    HDCP22_SECRET_NUM_STREAMS,
    HDCP22_SECRET_STREAM_ID_TYPE,
    HDCP22_SECRET_DP_TYPE_MASK,
    HDCP22_SECRET_RX_ID,
    HDCP22_SECRET_RTX,
    HDCP22_SECRET_RN,
    HDCP22_SECRET_DKEY,
    HDCP22_SECRET_KD,
    HDCP22_SECRET_RSESSION,
    HDCP22_SECRET_MAX_NO_SORS,
    HDCP22_SECRET_MAX_NO_HEADS,
    HDCP22_SECRET_PREV_STATE,
} HDCP22_SECRET;

/*!
 * @brief Secret Assets that needs  Integrity.
 */
typedef struct
{
    /*!
     * This field indicates Sor number of the current session
     */
    LwU8                sorNum;

    /*!
     * This field indicates Link Index of the current session
     */
    LwU8                linkIndex;

    /*!
     * This field indicates if it is MST supported monitor or not.
     */
    LwBool              bIsMst;

    /*!
     * This field indicates number of streams
     */
    LwU8                numStreams;

    /*!
     * This field indicates stream id type requested
     */
    HDCP22_STREAM       streamIdType[HDCP22_NUM_STREAMS_MAX];

    /*!
     * This field indicates dpTypeMask
     */
    LwU32               dpTypeMask[HDCP22_NUM_DP_TYPE_MASK];


    /*!
     * This field indicates rxId
     */
    LwU8                rxId[HDCP22_SIZE_RECV_ID_8];

    /*!
     * This field indicates Rtx
     */
    LwU32               Rtx[HDCP22_SIZE_R_TX/sizeof(LwU32)];

    /*!
     * This field indicates Rn
     */
    LwU32               Rn[HDCP22_SIZE_R_N/sizeof(LwU32)];

#ifndef HDCP22_KMEM_ENABLED
    /*!
     * This field indicates random number for confidential encryption operation.
     */
    LwU32               rnSesCrypt[HDCP22_SIZE_SESSION_CRYPT_U32];
#endif

    /*!
     * This field indicates if it is repeater or not.
     */
    LwBool              bIsRepeater;

    /*!
     * This field indicates max number of SORs.
     */
    LwU8                maxNoOfSors;

    /*!
     * This field indicates max number of HEADs.
     */
    LwU8                maxNoOfHeads;

    /*!
     * This field indicates previous state.
     */
    SELWRE_ACTION_TYPE  previousState;

    /*!
     * This field is internal padding for integrity check and needs to be 16bytes aligned.
     */
    LwU8                scpAlignPadding[12];

} SECRET_ASSETS_INTEGRITY, *PSECRET_ASSETS_INTEGRITY;

/*!
 * @brief Secret Assets that needs confidentiality
 */
typedef struct
{
    /*!
     * This field indicates encrypted Kd.
     */
    LwU32               encKd[HDCP22_SIZE_KD/sizeof(LwU32)];

    /*!
     * This field indicates encrypted Dkey2.
     */
    LwU32               encDkey[HDCP22_SIZE_DKEY/sizeof(LwU32)];

} SECRET_ASSETS_CONFIDENTIAL, *PSECRET_ASSETS_CONFIDENTIAL;

/*!
 * @brief HDCP 2.2 Active Session Structure Secure fields of SOR
 */
typedef struct
{
#ifndef HDCP22_KMEM_ENABLED
    /*!
     * This field indicates random number for confidential encryption operation.
     */
    LwU32               rnSesCrypt[HDCP22_SIZE_SESSION_CRYPT_U32];
#endif

    /*!
     * This field indicates encrypted Kd.
     */
    LwU32               encKd[HDCP22_SIZE_KD/sizeof(LwU32)];

} SECRET_ASSETS_ACTIVE_SESSION, *PSECRET_ASSETS_ACTIVE_SESSION;

/*!
 * @brief Secure memory structure containing secret assest
 */
typedef struct
{
#if defined(HDCP22_CHECK_STATE_INTEGRITY)
    /*!
     * This field indicates hash value for integrity check and must be the 1st field.
     */
    LwU32                           IntegrityHash[HDCP22_SIZE_INTEGRITY_HASH_U32];
#endif

    /*!
     * This field indicates Secret assets which require integrity.
     * Must be the 1st field of the structure because scp hash alignment.
     */
    SECRET_ASSETS_INTEGRITY         secAssetsIntegrity;

     /*!
     * This field indicates Secret assets which require integrity
     */
    SECRET_ASSETS_CONFIDENTIAL      secAssetsConfidential;

    /*!
    * This field indicates Secret assets for active sessions with SOR
    */
    SECRET_ASSETS_ACTIVE_SESSION    secAssetsActiveSession[HDCP22_ACTIVE_SESSIONS_MAX];

} HDCP22_SELWRE_MEMORY, *PHDCP22_SELWRE_MEMORY;

/*!
 * @brief Start session input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input Sor number of the current session
     */
    LwU8                sorNum;

    /*!
     * This field indicates input Link Index of the current session
     */
    LwU8                linkIndex;

    /*!
     * This field indicates input stream id type requested
     */
    HDCP22_STREAM       streamIdType[HDCP22_NUM_STREAMS_MAX];

    /*!
     * This field indicates input number of streams
     */
    LwU8                numStreams;

    /*!
     * This field indicates input dpTypeMask
     */
    LwU32               dpTypeMask[HDCP22_NUM_DP_TYPE_MASK];

    /*!
     * This field indicates input flag if HWDRMTYP1LOCKED WAR case.
     */
    LwBool              bApplyHwDrmType1LockedWar;

    /*!
     * This field indicates output rTx for AKE_INIT.
     */
    LwU32               rTx[HDCP22_SIZE_R_TX/sizeof(LwU32)];

    /*!
     * This field indicates output rn for LC_INIT.
     */
    LwU32               rRn[HDCP22_SIZE_R_N/sizeof(LwU32)];

} SELWRE_ACTION_START_SESSION_ARG, *PSELWRE_ACTION_START_SESSION_ARG;

/*!
 * @brief Verify Certificate input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input if receiver certificate.
     */
    HDCP22_CERTIFICATE  rxCert;
} SELWRE_ACTION_VERIFY_CERTIFICATE_ARG, *PSELWRE_ACTION_VERIFY_CERTIFICATE_ARG;

/*!
 * @brief Km, Kd, dkey2 generation input parameters.
 */
typedef struct
{
    union
    {
        /*!
         * This field indicates input RSA public key from receiver's certificate.
         */
        LwU8                kPubRx[HDCP22_SIZE_RX_MODULUS_8 + HDCP22_SIZE_RX_EXPONENT_8];

        /*!
         * This field indicates output encrypted master key.
         */
        LwU8                eKm[HDCP22_SIZE_EKM];
    } kPubRxEkm;

    /*!
     * This field indicates input rRx from receiver at AKE stge.
     */
    LwU64               rRx[HDCP22_SIZE_R_RX/sizeof(LwU64)];   // LwU64 aligned for dKey, Eks computation.

} SELWRE_ACTION_KMKD_GEN_ARG, *PSELWRE_ACTION_KMKD_GEN_ARG;

/*!
 * @brief Hprime validation input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input rxCaps from receiver.
     */
    LwU8                rxCaps[HDCP22_SIZE_TX_CAPS];

    /*!
     * This field indicates input AKE stage hprime from receiver.
     */
    LwU8                hPrime[HDCP22_SIZE_H];

    /*!
     * This field indicates input AKE stage repeater flage from receiver. TODO: bRepeater flag should be saved at certificateValidation action. Move that when it's implemented.
     */
    LwBool              bRepeater;
} SELWRE_ACTION_HPRIME_VALIDATION_ARG, *PSELWRE_ACTION_HPRIME_VALIDATION_ARG;

/*!
 * @brief Lprime validation input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input rRx from receiver at AKE stge.
     */
    LwU64               rRx[HDCP22_SIZE_R_RX/sizeof(LwU64)];   // LwU64 aligned for computation.

    /*!
     * This field indicates input LC stage lprime from receiver.
     */
    LwU8                lPrime[HDCP22_SIZE_L];
} SELWRE_ACTION_LPRIME_VALIDATION_ARG, *PSELWRE_ACTION_LPRIME_VALIDATION_ARG;

/*!
 * @brief eKs genertion input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input rRx from receiver at AKE stge.
     */
    LwU64               rRx[HDCP22_SIZE_R_RX/sizeof(LwU64)];   // LwU64 aligned for Eks computation.

    /*!
     * This field indicates output SKE stage encrypted session key (eKs) to receiver.
     */
    LwU8                eKs[HDCP22_SIZE_EKS];

    /*!
     * This field indicates output SKE stage Riv to receiver.
     */
    LwU8                rRiv[HDCP22_SIZE_R_IV];
} SELWRE_ACTION_EKS_GEN_ARG, *PSELWRE_ACTION_EKS_GEN_ARG;

/*!
 * @brief HW encryption enable/disable input parameters.
 */
typedef struct
{
    /*!

     * This field indicates input to tell enable/disable HW encryption or update HDMI non-repeater streamType reg.
     */
    HDCP22_ENC_CONTROL  controlAction;

    /*!
     * This field indicates input SOR index number.
     */
    LwU8                sorNum;

    /*!
     * This field indicates input link index number.
     */
    LwU8                linkIndex;

    /*!
     * This field indicates input flag to see if applying HwDRM type1Locked WAR sequence.
     */
    LwBool              bApplyHwDrmType1LockedWar;

    /*!
     * This field indicates input flag to see wait crypt inactive at disable.
     */
    LwBool              bCheckCryptStatus;

    /*!
     * This field indicates input flag to see if type0 or type1 for HDMI non-repeater case.
     */
    LwU8                hdmiNonRepeaterStreamType;

    /*!
     * This field indicates input flag to see if DP connection.
     */
    LwBool              bIsAux;
} SELWRE_ACTION_CONTROL_ENCRYPTION_ARG, *PSELWRE_ACTION_CONTROL_ENCRYPTION_ARG;

/*!
 * @brief Vprime validation input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input repeater RxIdList number of devices.
     */
    LwU8                numDevices;

    /*!
     * This field indicates input of v(Vprime) computation. V(V') = hmacsha256(ReceiverIdList || RxInfo || SeqV, Kd).
     */
    LwU8                vInput[HDCP22_KSV_LIST_SIZE_MAX + HDCP22_SIZE_RX_INFO + HDCP22_SIZE_SEQ_NUM_V];

    /*!
     * This field indicates input repeater stage Vprime MSB from receiver.
     */
    LwU8                vPrime[HDCP22_SIZE_V_PRIME];

    /*!
     * This field indicates output Vprime LSB.
     */
    LwU8                vLsb[HDCP22_SIZE_V_PRIME];

    /*!
     * This field indicates indicate input/output flag if force type0 with HDCP1X DS.
     */
    LwBool              bEnforceType0Hdcp1xDS;
} SELWRE_ACTION_VPRIME_VALIDATION_ARG, *PSELWRE_ACTION_VPRIME_VALIDATION_ARG;

/*!
 * @brief Mprime validation input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input number of streams from receiver.
     */
    LwU16               numStreams;

    /*!
     * This field indicates input stream management sequence number M.
     */
    LwU32               seqNumM;

    /*!
     * This field indicates input repeater stage Mprime from receiver.
     */
    LwU8                mPrime[HDCP22_SIZE_RPT_M];
} SELWRE_ACTION_MPRIME_VALIDATION_ARG, *PSELWRE_ACTION_MPRIME_VALIDATION_ARG;

/*!
 * @brief Write DP Ecf input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input SOR number.
     */
    LwU8                sorNum;

    /*!
     * This field indicates input ecfTimeslot reg value to program.
     */
    LwU32               ecfTimeslot[2];

    /*!
     * This field indicates input flag if clear ECF along with time slots.
     */
    LwBool              bForceClearEcf;

    /*!
     * This field indicates input flag if add back time slot after clearing ECF.
     */
    LwBool              bAddStreamBack;

    /*!
     * This field indicates input flag if request comes from internal.
     */
    LwBool              bInternalReq;
} SELWRE_ACTION_WRITE_DP_ECF_ARG, *PSELWRE_ACTION_WRITE_DP_ECF_ARG;

/*!
 * @brief SRM revocation input parameters.
 */
typedef struct
{
    /*!
     * This field indicates input SRM.
     */
    LwU32               srm[HDCP22_SIZE_SRM_MAX_U32];

    /*!
    * This field indicates input SRM size.
    */
    LwU32               totalSrmSize;

    /*!
     * This field indicates input KSVList number.
     */
    LwU32               noOfKsvs;

    /*!
     * This field indicates input KSVList
     */
    LwU8                ksvList[HDCP22_KSV_LIST_SIZE_MAX];

    /*!
     * This field indicates input flag if prod/debug setting.
     */
    LwU32               ctrlOptions;
} SELWRE_ACTION_SRM_REVOCATION_ARG, *PSELWRE_ACTION_SRM_REVOCATION_ARG;

#ifdef GSPLITE_RTOS

#define MAX_HDCP22_INTEGRITY_CHECK_INPUT_SIZE   (LW_MAX(sizeof(HDCP22_ACTIVE_SESSION), sizeof(HDCP22_SESSION_VARS_IR)))
#define MAX_HDCP22_INTEGRITY_CHECK_SIZE         (LW_ALIGN_UP(MAX_HDCP22_INTEGRITY_CHECK_INPUT_SIZE, SCP_BUF_ALIGNMENT))
#define MAX_HDCP22_INTEGRITY_CHECK_SIZE_U32     (MAX_HDCP22_INTEGRITY_CHECK_SIZE/sizeof(LwU32))

/*!
 * @brief secure bus access handling argument.
 */
typedef struct
{
    /*!
     * This field is used to tell is secure bus read request or write request
     */
    LwBool      bIsReadReq;

    /*!
     * This field is address of register to be read or written.
     */
    LwU32       addr;

    /*!
     * This field is value to be written (not applicable for read request).
     */
    LwU32       val;

    /*!
     * This field is to be filled with read value (not applicable for write request).
     */
    LwU32       retVal;
} SELWRE_ACTION_REG_ACCESS_ARG, *PSELWRE_ACTION_REG_ACCESS_ARG;

#define MAX_SECRET_SCP_CRYPT_SIZE               (32)
#define MAX_SECRET_SCP_CRYPT_SIZE_U32           (MAX_SECRET_SCP_CRYPT_SIZE/sizeof(LwU32))

/*!
 * @brief Using scp to encrypt/decrypt confidential secret.
 */
typedef struct
{
    /*!
     * This field is random number used to secret cryption.
     */
    LwU32               cryptRn[HDCP22_SIZE_SESSION_CRYPT_U32];

    /*!
     * This field is pointer to secret variable to read/write.
     */
    LwU32               cryptSecret[MAX_SECRET_SCP_CRYPT_SIZE_U32];

    /*!
     * This field is secure encrypted buffer to encrypt or desintiation to decrypt.
     */
    LwU32               cryptDest[MAX_SECRET_SCP_CRYPT_SIZE_U32];

    /*!
     * This field is secret size in bytes.
     */
    LwU32               secretSizeinBytes;

    /*!
     * This field is to tell encrypt or decrypt operation.
     */
    LwBool              bIsEncrypt;
} SELWRE_ACTION_SCP_CRYPT_SECRET_ARG, *PSELWRE_ACTION_SCP_CRYPT_SECRET_ARG;

/*!
 * @brief Using scp to callwlate hash value for integrity check.
 */
typedef struct
{
    /*!
     * This field is scp aligned output hash buffer.
     */
    LwU32               outHash[HDCP22_SIZE_INTEGRITY_HASH_U32];

    /*!
     * This field is pointer to input data.
     */
    LwU32               inputData[MAX_HDCP22_INTEGRITY_CHECK_SIZE_U32];

    /*!
     * This field is input size in bytes.
     */
    LwU32               inputSizeinBytes;

} SELWRE_ACTION_SCP_CALLWLATE_HASH_ARG, *PSELWRE_ACTION_SCP_CALLWLATE_HASH_ARG;

/*!
 * @brief Using scp to generate dkey.
 */
typedef struct
{
    /*!
     * This field is used to tell the input message to be encrypted.
     */
    LwU8                message[HDCP22_SIZE_DKEY];

    /*!
     * This field is used to tell the Key with which message is to be encrypted.
     */
    LwU8                key[HDCP22_SIZE_DKEY];

    /*!
     * This field is used to tell the encrypted message.
     */
    LwU8                encryptMessage[HDCP22_SIZE_DKEY];
} SELWRE_ACTION_SCP_GEN_DKEY_ARG, *PSELWRE_ACTION_SCP_GEN_DKEY_ARG;
#endif

/*!
 * @brief secure access argument structure.
 */
typedef struct
{
#ifdef UPROC_RISCV
    /*!
     * This field is used to tell RISC-V HS partition status to secure action request.
     */
    SELWRE_ACTION_STATUS                        actionStatus;
#endif

    /*!
     * This union's offset should be SCP aligned for SCP HW operation..
     */
    union
    {
#ifdef GSPLITE_RTOS
        /*!
         * This field is used to tell argument of secure bus register access.
         */
        SELWRE_ACTION_REG_ACCESS_ARG            regAccessArg;

        /*!
         * This field is used to tell argument of action to using scp encrypt/decrypt confidential secret.
         */
        SELWRE_ACTION_SCP_CRYPT_SECRET_ARG      scpCryptSecretArg;

        /*!
         * This field is used to tell argument of action to using scp callwlate hash value of input data.
         */
        SELWRE_ACTION_SCP_CALLWLATE_HASH_ARG    scpCallwlateHashArg;

        /*!
         * This field is used to tell argument of action to using scp geneating dkey.
         */
        SELWRE_ACTION_SCP_GEN_DKEY_ARG          scpGenDkeyArg;
#endif

        /*!
         * This field is used to tell argument of action to start session.
         */
        SELWRE_ACTION_START_SESSION_ARG         secActionStartSession;

        /*!
         * This field is used to tell argument of action to verify certificate.
         */
        SELWRE_ACTION_VERIFY_CERTIFICATE_ARG    secActiolwerifyCertificate;

        /*!
         * This field is used to tell argument of action to generte Km, Kd.
         */
        SELWRE_ACTION_KMKD_GEN_ARG              secActionKmKdGen;

        /*!
         * This field is used to tell argument of action to validate hPrime.
         */
        SELWRE_ACTION_HPRIME_VALIDATION_ARG     secActionHprimeValidation;

        /*!
         * This field is used to tell argument of action to validate lprime.
         */
        SELWRE_ACTION_LPRIME_VALIDATION_ARG     secActionLprimeValidation;

        /*!
         * This field is used to tell argument of action to generate eKs.
         */
        SELWRE_ACTION_EKS_GEN_ARG               secActionEksGen;

        /*!
         * This field is used to tell argument of action to enable/disable HW encryption.
         */
        SELWRE_ACTION_CONTROL_ENCRYPTION_ARG    secActionControlEncryption;

        /*!
         * This field is used to tell argument of action to valiate vprime.
         */
        SELWRE_ACTION_VPRIME_VALIDATION_ARG     secActiolwprimeValidation;

        /*!
         * This field is used to tell argument of action to validate mprime.
         */
        SELWRE_ACTION_MPRIME_VALIDATION_ARG     secActionMprimeValidation;

        /*!
         * This field is used to tell argument of action to write DP ecf.
         */
        SELWRE_ACTION_WRITE_DP_ECF_ARG          secActionWriteDpEcf;

        /*!
         * This field is used to tell argument of action to execute SRM revocation.
         */
        SELWRE_ACTION_SRM_REVOCATION_ARG        secActionSrmRevocation;
    } action;

    /*!
     * This field is used to tell argument of secure action type.
     */
    SELWRE_ACTION_TYPE                          actionType;
} SELWRE_ACTION_ARG, *PSELWRE_ACTION_ARG;

typedef struct
{
#ifdef GSPLITE_RTOS
    /*!
     * This field is used to tell argument of secure bus register access.
     */
    SELWRE_ACTION_REG_ACCESS_ARG                regAccessArg;
#endif

    /*!
     * This field is used to tell argument of secure action type.
     */
    SELWRE_ACTION_TYPE                          actionType;
} SELWRE_ACTION_HDCP_ARG, *PSELWRE_ACTION_HDCP_ARG;

#define HDCP1X_SELWRE_ACTION_ARG_BUF_SIZE  (sizeof(SELWRE_ACTION_HDCP_ARG))
#define HDCP22_SELWRE_ACTION_ARG_BUF_SIZE  (sizeof(SELWRE_ACTION_ARG))

/* ------------------------- Prototypes ------------------------------------- */
extern HDCP22_SELWRE_MEMORY hdcp22SelwreMemory;

/* ------------------------ Function Prototypes ---------------------------- */
extern FLCN_STATUS libIntfcHdcp22wiredSelwreAction(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22ConfidentialSecretDoCryptHs(LwU32 *pRn, LwU32 *pSecret, LwU32 *pDest, LwU32 size, LwBool bIsEncrypt);
extern FLCN_STATUS hdcp22CallwateHashHs(LwU32 *pHash, LwU32 *pData, LwU32 size);
extern FLCN_STATUS hdcp22GenerateDkeyScpHs(LwU8 *pMessage, LwU8 *pKey, LwU8 *pEncryptMessage);
extern FLCN_STATUS hdcp22wiredStartSessionHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredVerifyRxCertificateHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredKmKdGenHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredHprimeValidationHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredLprimeValidationHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredEksGenHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredControlEncryptionHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredVprimeValidationHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredMprimeValidationHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredWriteDpEcfHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredEndSessionHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredSelwreRegAccessHandler(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22wiredSrmRevocationHandler(SELWRE_ACTION_ARG *pArg);
#endif // DISPFLCN_SELWREACTION_H
