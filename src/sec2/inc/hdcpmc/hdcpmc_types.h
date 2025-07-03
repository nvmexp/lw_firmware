/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#ifndef HDCPMC_TYPES_H
#define HDCPMC_TYPES_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "hdcpmc/hdcpmc_constants.h"

/*!
 * @brief Session status.
 */
typedef enum _hdcp_session_status
{
    HDCP_SESSION_FREE,      //<! Session is available.
    HDCP_SESSION_IN_USE,    //<! Session is in use.
    HDCP_SESSION_ACTIVE,    //<! Session is actively transmitting HDCP content.
} HDCP_SESSION_STATUS;

/*!
 * @brief HDCP 2.0 authentication stages.
 */
typedef enum _hdcp_auth_stage
{
    HDCP_AUTH_STAGE_NONE,
    HDCP_AUTH_STAGE_AKE_INIT,
    HDCP_AUTH_STAGE_GET_CERT,
    HDCP_AUTH_STAGE_VERIFY_CERT,
    HDCP_AUTH_STAGE_GENERATE_EKM,
    HDCP_AUTH_STAGE_VERIFY_HPRIME,
    HDCP_AUTH_STAGE_GENERATE_LC_INIT,
    HDCP_AUTH_STAGE_GET_RTT_CHALLENGE,
    HDCP_AUTH_STAGE_VERIFY_LPRIME,
    HDCP_AUTH_STAGE_GENERATE_SKE_INIT,
    HDCP_AUTH_STAGE_ENCRYPT_PAIRING_INFO,
    HDCP_AUTH_STAGE_DECRYPT_PAIRING_INFO,
    HDCP_AUTH_STAGE_VERIFY_VPRIME,
    HDCP_AUTH_STAGE_GET_CERT_RX,
    HDCP_AUTH_STAGE_DECRYPT_KM,
    HDCP_AUTH_STAGE_GET_HPRIME,
    HDCP_AUTH_STAGE_VERIFY_RTT_CHALLENGE,
    HDCP_AUTH_STAGE_GET_LPRIME,
} HDCP_AUTH_STAGE;

/*!
 * @brief V/V' authentication status.
 */
typedef enum _hdcp_vprime_status
{
    HDCP_VPRIME_STATUS_NOT_STARTED,     //<! Revocation check has not started.
    HDCP_VPRIME_STATUS_SUCCESS,         //<! Receiver passed revocation check.
    HDCP_VPRIME_STATUS_FAILURE,         //<! V' does match V.
    HDCP_VPRIME_STATUS_REVOKED,         //<! Receiver found in revocation list.
    HDCP_VPRIME_STATUS_SRM_ILWALID,     //<! SRM provided is not valid.
} HDCP_VPRIME_STATUS;

/*!
 * @brief Contains the encryption state of the session.
 */
typedef struct _hdcp_encryption_state
{
    /*!
     * Input counter.
     */
    LwU64 inputCtr[HDCP_MAX_STREAMS_PER_RECEIVER];

    /*!
     * Snapshot of inputCtr during the last status call.
     */
    LwU64 inputCtrSnap[HDCP_MAX_STREAMS_PER_RECEIVER];

    /*!
     * Stream counter.
     */
    LwU32 streamCtr[HDCP_MAX_STREAMS_PER_RECEIVER];

    /*!
     * Session Key
     */
    LwU64 ks[HDCP_SIZE_LwU64(HDCP_SIZE_KS)];

    /*!
     * Pseudo-random number used during the session key exchange.
     */
    LwU64 riv;

    /*!
     * Number of streams in this receiver.
     */
    LwU32 numStreams;

    /*!
     * TODO-JBH: Document.
     */
    LwU64 iv[HDCP_SIZE_LwU64(HDCP_SIZE_IV)];
} HDCP_ENCRYPTION_STATE;


/*!
 * @brief Contains the current session state.
 */
typedef struct _hdcp_session_state
{
    /*!
     * Counter used in key derivation.
     */
    LwU64 dkeyCtr;

    /*!
     * A 256-bit value used in key derivation.
     */
    LwU64 kd[HDCP_SIZE_LwU64(HDCP_SIZE_KD)];

    /*!
     * A 64-bit pseudo-random value sent by the transmitter. Used during
     * the authentication and key exchange pairing.
     */
    LwU64 rtx;

    /*!
     * A 64-bit pseudo-random value sent by the receiver. Used during
     * the authentication and key exchange pairing.
     */
    LwU64 rrx;

    /*!
     * A 64-bit pseudo-random nonce sent by the transmitter. Used during
     * the locality check.
     */
    LwU64 rn;

    /*!
     * Session's master key.
     */
    LwU64 km[HDCP_SIZE_LwU64(HDCP_SIZE_KM)];
} HDCP_SESSION_STATE;

/*!
 * @brief Contains information about the HDCP transmitter, including
 * transmitter capabilities.
 */
typedef struct _hdcp_transmitter_info
{
    /*!
     * Version as defined by the specification. Must be set to 0x02 for
     * HDPC 2.2.
     */
    LwU8 version;

    /*!
     * Bitmask of transmitter's capabilities.
     */
    LwU16 capabilitiesMask;
} HDCP_TRANSMITTER_INFO;

#define HDCP_TRANSMITTER_VERSION                                           0x02

/*
 * Bit definition of @ref HDCP_TRANSMITTER_INFO::capabilitiesMask.
 *
 * Bit 15:1 - Reserved
 * Bit 0    - Pre-compute support for L and L'
 */
#define HDCP_TRANSMITTER_CAPABILITIES_RESERVED                             15:1
#define HDCP_TRANSMITTER_CAPABILITIES_PRECOMPUTE_SUPPORT                    0:0
#define HDCP_TRANSMITTER_CAPABILITIES_PRECOMPUTE_SUPPORT_YES                  1
#define HDCP_TRANSMITTER_CAPABILITIES_PRECOMPUTE_SUPPORT_NO                   0

/*!
 * @brief Contains information about the HDCP receiver, including
 * receiver capabilities.
 */
typedef struct _hdcp_receiver_info
{
    /*!
     * Version as defined by the specification. Must be set to 0x02 for
     * HDPC 2.2.
     */
    LwU8 version;

    /*!
     * Bitmask of receiver's capabilities.
     */
    LwU16 capabilitiesMask;
} HDCP_RECEIVER_INFO;

#define HDCP_RECEIVER_VERSION                                              0x02

/*
 * Bit definition of @ref HDCP_RECEIVER_INFO::capabilitiesMask.
 *
 * Bit 15:1 - Reserved
 * Bit 0    - Pre-compute support for L and L'
 */
#define HDCP_RECEIVER_CAPABILITIES_RESERVED                                15:1
#define HDCP_RECEIVER_CAPABILITIES_PRECOMPUTE_SUPPORT                       0:0
#define HDCP_RECEIVER_CAPABILITIES_PRECOMPUTE_SUPPORT_YES                     1
#define HDCP_RECEIVER_CAPABILITIES_PRECOMPUTE_SUPPORT_NO                      0

#define HDCP_RECEIVER_REPEATER                                              0:0
#define HDCP_RECEIVER_REPEATER_YES                                            1
#define HDCP_RECEIVER_REPEATER_NO                                             0
#define HDCP_RECEIVER_DP_CAPABLE                                            1:1
#define HDCP_RECEIVER_DP_CAPABLE_YES                                          1
#define HDCP_RECEIVER_DP_CAPABLE_NO                                           0

/*!
 * @brief Session state.
 */
typedef struct _hdcp_session
{
    /*!
     * Session signature.
     */
    LwU64 signature[HDCP_SIZE_LwU64(HDCP_SIZE_SESSION_SIGNATURE)];

    /*!
     * Session ID.
     */
    LwU32 id;

    /*!
     * Receiver's HDCP version.
     */
    LwU32 version;

    /*!
     * Sequence number of M'.
     */
    LwU32 seqNumM;

    /*!
     * Session status.
     */
    HDCP_SESSION_STATUS status;

    /*!
     * Session authentication stage.
     */
    HDCP_AUTH_STAGE stage;

    /*!
     * Session's state.
     */
    HDCP_SESSION_STATE sessionState;

    /*!
     * Encryption state.
     */
    HDCP_ENCRYPTION_STATE encryptionState;

    /*!
     * Specifies if the receiver is a repeater.
     */
    LwBool bRepeater;

    /*!
     * Reciever's ID.
     */
    LwU8 receiverId[HDCP_SIZE_LwU8(HDCP_SIZE_RCVR_ID)];

    /*!
     * Modulus portion of the receiver's public key.
     */
    LwU8 modulus[HDCP_SIZE_LwU8(HDCP_SIZE_RX_MODULUS)];

    /*!
     * Exponent portion of the receiver's public key.
     */
    LwU8 exponent[HDCP_SIZE_LwU8(HDCP_SIZE_RX_EXPONENT)];

    /*!
     * Flags for the session.
     * bit[7:4] - Protocol descriptor.
     * bit[3:0] - Reserved for future use.
     */
    LwU8 rfu1;

    /*!
     * Specifies to use test keys for debug purposes.
     */
    LwBool bIsUseTestKeys;

    /*!
     * Index in test keys ds.
     */
    LwU8 testKeysIndex;

    /*!
     * Specifies if the revocation check is complete.
     */
    LwBool bIsRevocationCheckDone;

    /*!
     * Specifies if L pre-compute support is supported.
     */
    LwBool bIsPreComputeSupported;

    /*!
     * V' authentication status.
     */
    HDCP_VPRIME_STATUS vprimeStatus;

    /*!
     * Number of attempts made to verify V'.
     */
    LwU32 vprimeAttemptCount;

    /*!
     * Value of L used in the locality check.
     */
    LwU64 L[HDCP_SIZE_LwU64(HDCP_SIZE_LPRIME)];

    /*!
     * Transmitter information.
     */
    HDCP_TRANSMITTER_INFO transmitterInfo;

    /*!
     * Receiver information.
     */
    HDCP_RECEIVER_INFO receiverInfo;

    /*!
     * Specifies if the GPU is acting as a transmitter or receiver for the
     * session.
     *
     * TODO-JBH: Make an enumeration.
     */
    LwU8 sessionType;

    /*!
     * Specifies the display type.
     * 0 = generic (WFD), 1 = HDMI, 2 = DP
     *
     * TODO-JBH: Make an enumeration.
     */
    LwU8 displayType;

    /*!
     * Used for tracking a rollover of the sequence number V when
     * authenticating with repeaters.
     */
    LwU8 seqNumRollover;
} HDCP_SESSION;

#define LW_HDCP_SESSION_RFU_PD                                              7:4
#define LW_HDCP_SESSION_RFU_PD_VAL_0                                          0
#define LW_HDCP_SESSION_RFU_PD_VAL_1                                          1

/*!
 * Header for the scratch buffer. The scratch buffer is used reduce the amount
 * of stack space consumed by the HDCP task.
 */
typedef struct _hdcp_sb_header
{
    /*!
     * HDCP app version.
     */
    LwU32 version;

    /*!
     * Number of used sessions.
     */
    LwU32 numSessionsInUse;

    /*!
     * Scratch buffer flags.
     */
    LwU32 flags;

    /*!
     * Used for session ID.
     */
    LwU32 sessionIdSeqCnt;

    /*!
     * Global stream counter.
     */
    LwU32 streamCtr;

    /*!
     * Session status masks.
     */
    LwU8 sessionStatusMask[HDCP_SIZE_SESSION_STATUS_MASK];
} HDCP_SCRATCH_BUFFER_HDR;

/*
 * Bitmask defines for @ref HDCP_SCRATCH_BUFFER_HDR::flags.
 */
#define LW_HDCP_SCRATCH_BUFFER_HEADER_FLAG_INIT                             0:0
#define LW_HDCP_SCRATCH_BUFFER_HEADER_FLAG_INIT_DONE                          1
#define LW_HDCP_SCRATCH_BUFFER_HEADER_FLAG_INIT_NOT_DONE                      0

/*!
 * Active session information.
 */
typedef struct _hdcp_session_active_rec
{
    /*!
     * Session ID.
     */
    LwU32 sessionId;

    /*!
     * Session status.
     */
    HDCP_SESSION_STATUS status;

    /*!
     * Specifies whether this session is a regular or demo session.
     */
    LwBool bIsDemoSession;
} HDCP_SESSION_ACTIVE_REC;

/*!
 * Encryption states of active sessions.
 */
typedef struct _hdcp_session_active_enc
{
    /*!
     * Active session.
     */
    HDCP_SESSION_ACTIVE_REC rec;

    /*!
     * Session's encryption state.
     */
    HDCP_ENCRYPTION_STATE encryptionState;
} HDCP_SESSION_ACTIVE_ENCRYPTION GCC_ATTRIB_ALIGN(16);

/*!
 * State information used to track session & encryption status of the various
 * receivers/repeaters.
 * The structure must be dma transfer size aligned.
 */
typedef struct _hdcp_global_data
{
    /*!
     * Random number - should be first data member.
     * TODO: Do we need this?
     */
    LwU8 randNum[16] GCC_ATTRIB_ALIGN(16);

    /*!
     * Stores the active session in encrypted form.
     */
    HDCP_SESSION_ACTIVE_ENCRYPTION activeSessions[HDCP_POR_NUM_RECEIVERS];

    /*!
     * Session IDs in active sessions. The session ID at a particular index
     * will match the session details stored in @ref activeSessions.
     */
    HDCP_SESSION_ACTIVE_REC activeRecs[HDCP_POR_NUM_RECEIVERS];

    /*!
     * Offset within the FB as to where the scratch buffer is contained.
     */
    LwU64 sbOffset;

    /*!
     * Used to track the current state of the scratch buffer.
     */
    HDCP_SCRATCH_BUFFER_HDR header;

    /*!
     * Bitmask containing a list of the supported versions of HDCP.
     */
    LwU32 supportedHdcpVersionsMask;

    /*!
     * GPU chip ID. Used in upstream.
     */
    LwU32 chipId;
} HDCP_GLOBAL_DATA GCC_ATTRIB_ALIGN(16);

/*!
 * @brief HDCP receiver certificate.
 */
typedef struct _hdcp_certificate
{
    /*!
     * HDCP receiver's ID.
     */
    LwU8 id[HDCP_SIZE_LwU8(HDCP_SIZE_RCVR_ID)];

    /*!
     * The modulus portion of the receiver's public key. When combined with
     * @ref HDCP_CERTIFICATE::exponent, it makes up the 1048 bit public key.
     * The modulus is the first 1024 bits of the public key.
     */
    LwU8 modulus[HDCP_SIZE_LwU8(HDCP_SIZE_RX_MODULUS)];

    /*!
     * The exponent portion of the receiver's public key. When combined with
     * @ref HDCP_CERTIFICATE::modulus, it makes up the 1048 bit public key.
     * The exponent is the last 24 bits of the public key.
     */
    LwU8 exponent[HDCP_SIZE_LwU8(HDCP_SIZE_RX_EXPONENT)];

    /*!
     * The protocol descriptor field. Only the upper 4 bits are used. The lower
     * 4 bits are reserved, and must be 0x0.
     */
    LwU8 flags;

    /*!
     * Reserved for future use. Must be 0x00.
     */
    LwU8 rfu;

    /*!
     * A cryptographic signature callwlated over all the proceeding fields of
     * the certificate. RSASSA-PKCS1-1-v1_5 is the signature scheme used as
     * defined by PKCS #1 V2.1: RSA Cryptography Standard. SHA-256 is the
     * underlying has function.
     */
    LwU8 signature[HDCP_SIZE_LwU8(HDCP_SIZE_RX_SIGNATURE)];
} HDCP_CERTIFICATE;

/*
 * Bit definition of @ref HDCP_CERTIFICATE::flags.
 *
 * Bit 7:4 - Protocol descriptor.
 * Bit 3:0 - Reserved for future use. Must be 0x0.
 */
#define HDCP_CERTIFICATE_FLAGS_PROTOCOL_DESCRIPTOR                          7:4
#define HDCP_CERTIFICATE_FLAGS_PROTOCOL_DESCRIPTOR_VAL0                     0x0
#define HDCP_CERTIFICATE_FLAGS_PROTOCOL_DESCRIPTOR_VAL1                     0x1
#define HDCP_CERTIFICATE_FLAGS_RESERVED                                     3:0
#define HDCP_CERTIFICATE_FLAGS_RESERVED_VAL                                 0x0


/*!
 * @brief HDCP receiver's private key.
 *
 * Used when the GPU is acting as the HDCP receiver.
 */
typedef struct _hdcp_receiver_private_key
{
    LwU8 P[HDCP_SIZE_LwU8(HDCP_SIZE_P)];
    LwU8 Q[HDCP_SIZE_LwU8(HDCP_SIZE_Q)];
    LwU8 DmodP[HDCP_SIZE_LwU8(HDCP_SIZE_D)];
    LwU8 DmodQ[HDCP_SIZE_LwU8(HDCP_SIZE_D)];
    LwU8 ilwQmodP[HDCP_SIZE_LwU8(HDCP_SIZE_Q)];
    LwU8 signature[HDCP_SIZE_LwU8(HDCP_SIZE_RX_PRIV_KEY_SIGNATURE)];
} HDCP_RECEIVER_PRIVATE_KEY;

/*!
 * @brief HDCP receiver's public/private key.
 */
typedef struct _hdcp_receiver_key
{
    /*!
     * Receiver's certificate.
     */
    HDCP_CERTIFICATE certificate;

    /*!
     * Receiver's private key.
     */
    HDCP_RECEIVER_PRIVATE_KEY privateKey;
} HDCP_RECEIVER_KEY;

#endif // HDCPMC_TYPES_H
#endif
