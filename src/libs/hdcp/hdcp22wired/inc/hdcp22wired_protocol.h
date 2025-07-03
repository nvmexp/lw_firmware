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
 * @file   hdcp22wired_protocol.h
 */

#ifndef HDCP22WIRED_PROTOCOL_H
#define HDCP22WIRED_PROTOCOL_H

// Keep the file as simple definition and not include other header file here.

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ---------------------------------------- */
// Sizes of HDCP22 Keys
#define HDCP22_SIZE_CERT_RX                     522
#define HDCP22_SIZE_RECV_ID_8                   5
#define HDCP22_SIZE_RX_MODULUS_8                128
#define HDCP22_SIZE_RX_EXPONENT_8               3
#define HDCP22_SIZE_RX_MODULUS_ROOT_8           3072/8
#define HDCP22_SIZE_KPUB_DCP                    384

// Sizes of HDCP22 Random Variables
#define HDCP22_SIZE_R_TX                        8
#define HDCP22_SIZE_R_RX                        8
#define HDCP22_SIZE_R_N                         8
#define HDCP22_SIZE_M                           16
#define HDCP22_SIZE_KM                          16
#define HDCP22_SIZE_EKM                         128
#define HDCP22_SIZE_EKH_KM                      16
#define HDCP22_SIZE_DKEY                        16
#define HDCP22_SIZE_DKEY_WORKINGBUF             40
#if defined(HDCP22_CHECK_STATE_INTEGRITY) || defined(HDCP22_USE_SCP_GEN_DKEY)
#define HDCP22_INTEGRITY_ALIGNMENT              SCP_BUF_ALIGNMENT
#define HDCP22_DKEY_ALIGNMENT                   SCP_BUF_ALIGNMENT
#else
#define HDCP22_INTEGRITY_ALIGNMENT              sizeof(LwU32)  // v0207 profile compiler doesn't support 16bytes alignment attribute
#define HDCP22_DKEY_ALIGNMENT                   sizeof(LwU64)
#endif

#define HDCP22_SIZE_KD                          32
#define HDCP22_SIZE_H                           32
#define HDCP22_SIZE_L                           32
#define HDCP22_SIZE_DKEY_CTR                    8
#define HDCP22_SIZE_R_IV                        8
#define HDCP22_SIZE_KS                          16
#define HDCP22_SIZE_EKS                         16
#define HDCP22_SIZE_TX_CAPS                     3
#define HDCP22_SIZE_RX_CAPS                     3
#define HDCP22_SIZE_RX_STREAM_TYPE              1
#define HDCP22_SIZE_SESSION_CRYPT               16
#define HDCP22_SIZE_SESSION_CRYPT_U32           (HDCP22_SIZE_SESSION_CRYPT/sizeof(LwU32))
#define HDCP22_SIZE_INTEGRITY_HASH              16
#define HDCP22_SIZE_INTEGRITY_HASH_U32          (HDCP22_SIZE_INTEGRITY_HASH/sizeof(LwU32))
#define HDCP22_SIZE_RN_MAX                      (HDCP22_SIZE_SESSION_CRYPT)         // Max random number length used in hdcp22.
#define HDCP22_SIZE_RN_MAX_U32                  (HDCP22_SIZE_RN_MAX/sizeof(LwU32))
#define HDCP22_SIZE_SRM_MAX                     (2048)                              // TODO: Support the 1st generation SRM max 5Kbytes.
#define HDCP22_SIZE_SRM_MAX_U32                 (HDCP22_SIZE_SRM_MAX/sizeof(LwU32))
#define HDCP22_SRM_EMEM_ALIGNMENT               (0x100)
#define HDCP22_SHA256_HASH_BLOCK                (32)

// HDCP22 Repeater Variable Sizes
#define HDCP22_SIZE_RX_INFO                     2
#define HDCP22_SIZE_SEQ_NUM_V                   3
#define HDCP22_KSV_LIST_NUM_MAX                 32
#define HDCP22_KSV_LIST_SIZE_MAX                (HDCP22_KSV_LIST_NUM_MAX*HDCP22_SIZE_RECV_ID_8)
#define HDCP22_SIZE_V_PRIME                     16
#define HDCP22_SIZE_V                           32
#define HDCP22_SIZE_SEQ_NUM_M                   3
#define HDCP22_SIZE_RPT_K                       2
#define HDCP22_SIZE_STR_ID_TYPE                 2
#define HDCP22_SIZE_RPT_M                       32

// Sizes of HDCP22 I2C messages
#define HDCP22_SIZE_MSG_ID                      1
#define HDCP22_SIZE_MSG_AKE_INIT                HDCP22_SIZE_MSG_ID + HDCP22_SIZE_R_TX + HDCP22_SIZE_TX_CAPS
#define HDCP22_SIZE_MSG_SEND_CERT               534
#define HDCP22_SIZE_MSG_NO_STORED_KM            129
#define HDCP22_SIZE_MSG_STORED_KM               33
#define HDCP22_SIZE_MSG_SEND_RRX                9
#define HDCP22_SIZE_MSG_SEND_HPRIME             33
#define HDCP22_SIZE_MSG_PAIRING_INFO            17
#define HDCP22_SIZE_MSG_LC_INIT                 9
#define HDCP22_SIZE_MSG_SEND_LPRIME             33
#define HDCP22_SIZE_MSG_SEND_EKS_RIV            25
// Sizes of HDCP22 I2C Repeater Messages
#define HDCP22_SIZE_MSG_RPT_RX_IDS              182
#define HDCP22_SIZE_MSG_RPT_RX_IDS_MIN          3
#define HDCP22_SIZE_MSG_RPT_RX_ID_ACK           17
#define HDCP22_SIZE_MSG_RPT_STR_MANAGE          8
#define HDCP22_SIZE_MSG_RPT_STR_READY           33
// Offsets of HDCP22 AUX transactions
#define HDCP22_DPCD_OFFSET_RTX                  0x69000
#define HDCP22_DPCD_OFFSET_TX_CAPS              0x69008
#define HDCP22_DPCD_OFFSET_CERT_RX              0x6900b
#define HDCP22_DPCD_OFFSET_RRX                  0x69215
#define HDCP22_DPCD_OFFSET_RX_CAPS              0x6921d
#define HDCP22_DPCD_OFFSET_RX_EKM               0x69220
#define HDCP22_DPCD_OFFSET_RX_EHKM              0x692a0
#define HDCP22_DPCD_OFFSET_RX_M                 0x692b0
#define HDCP22_DPCD_OFFSET_RX_HPRIME            0x692c0
#define HDCP22_DPCD_OFFSET_RX_EHKM_RD           0x692E0
#define HDCP22_DPCD_OFFSET_RX_RN                0x692F0
#define HDCP22_DPCD_OFFSET_RX_LPRIME            0x692F8
#define HDCP22_DPCD_OFFSET_RX_EKS               0x69318
#define HDCP22_DPCD_OFFSET_RX_RIV               0x69328
// Repeater DPCD Offsets
#define HDCP22_DPCD_OFFSET_RX_RX_INFO           0x69330
#define HDCP22_DPCD_OFFSET_RX_SEQ_NUM_V         0x69332
#define HDCP22_DPCD_OFFSET_RX_VPRIME            0x69335
#define HDCP22_DPCD_OFFSET_RX_ID_LIST           0x69345
#define HDCP22_DPCD_OFFSET_RX_V_LSB             0x693e0
#define HDCP22_DPCD_OFFSET_RX_SEQ_NUM_M         0x693f0
#define HDCP22_DPCD_OFFSET_RX_K                 0x693f3
#define HDCP22_DPCD_OFFSET_RX_STR_ID_TYPE       0x693f5
#define HDCP22_DPCD_OFFSET_RX_M_PRIME           0x69473
#define HDCP22_DPCD_OFFSET_RX_STREAM_TYPE       0x69494
// HDCP22 state machine
#define HDCP22_LINK_PRIMARY                     0
#define HDCP22_LINK_SECONDARY                   1
#define HDCP22_MAX_PAIRED_DEVICES               4
#define HDCP22_WAIT_FOR_RX_CERT_20MS            5000
#define HDCP22_WAIT_FOR_H_PRIME_20MS            50000
#define HDCP22_WAIT_FOR_PAIRED_H_PRIME_20MS     20
#define HDCP22_WAIT_FOR_PAIRING_20MS            20
#define HDCP22_INTERVAL_MS_20                   200
#define HDCP22_INTERVAL_MS_7                    7
#define HDCP22_INTERVAL_MS_200                  200
#define HDCP22_INTERVAL_MS_100                  100
#define HDCP22_MAX_DMA_SIZE                     256
#define HDCP22_RANDOM_NUM_GEN_ATTEMPTS          10
#define HDCP22_CERT_PD_MASK                     0xf0
#define HDCP22_CERT_PD_VAL_0                    0x00
#define HDCP22_CERT_PD_VAL_1                    0x10
#define HDCP22_RND_NUM_WAIT_CNT                 100
#define HDCP22_RX_STATUS_LINK_ACTIVE            0x1
// DP AUX Commands
#define HDCP22_DP_AUXCTL_CMD_AUXWR              8
#define HDCP22_DP_AUXCTL_CMD_AUXRD              9
// I2C API
#define LW_FLCN_I2C_INDEX_MAX                   0x4
#define LW_HDMI_VIDEO_LINK0_ADDRESS             0x74
#define LW_HDMI_HDCP22_VERSION                  (0x50)
#define LW_HDMI_HDCP22_VERSION_SIZE             (0x1)
#define LW_HDMI_HDCP22_VERSION_CAPABLE          2:2
#define LW_HDMI_HDCP22_VERSION_CAPABLE_NO       0x0
#define LW_HDMI_HDCP22_VERSION_CAPABLE_YES      0x1
#define LW_HDMI_SCDC_WRITE_OFFSET               0x60
#define LW_HDMI_SCDC_READ_OFFSET                0x80

#define CHECK_STATUS(x) if ((status = (x)) != FLCN_OK){goto label_return;}

// TX capabilities
#define TX_VERSION                              0x02
#define TX_PRECOMPUTE_SUPPORT_MASK              0x0001
#define TX_PRECOMPUTE_SUPPORT_YES               0x0001
#define TX_PRECOMPUTE_SUPPORT_NO                0x0000

typedef struct
{
   LwU8   version;
   LwU16  txCapsMask;
} HDCP22_TX_CAPS;

// RX Capabilities
#define RCVR_PRECOMPUTE_SUPPORT_MASK            0x0001
#define RCVR_PRECOMPUTE_SUPPORT_YES             0x0001
#define RCVR_PRECOMPUTE_SUPPORT_NO              0x0000

#define LW_RCV_HDCP22_RX_STATUS                             (0x70)
#define LW_RCV_HDCP22_RX_STATUS_SIZE                        (0x02)
#define LW_RCV_HDCP22_RX_STATUS_MESSAGE_SIZE                9:0
#define LW_RCV_HDCP22_RX_STATUS_READY                       10:10
#define LW_RCV_HDCP22_RX_STATUS_READY_YES                   0x1
#define LW_RCV_HDCP22_RX_STATUS_READY_NO                    0x0
#define LW_RCV_HDCP22_RX_STATUS_REAUTH_REQUEST              11:11
#define LW_RCV_HDCP22_RX_STATUS_REAUTH_REQUEST_YES          0x1
#define LW_RCV_HDCP22_RX_STATUS_REAUTH_REQUEST_NO           0x0
#define LW_RCV_HDCP22_RX_CAPS_HDCP_REPEATER                 0:0
#define LW_RCV_HDCP22_RX_CAPS_HDCP_DP_CAPABLE               1:1
#define LW_RCV_HDCP22_RX_CAPS_HDCP_RECEIVER_CAPABILITY_MASK 15:2
#define LW_RCV_HDCP22_RX_CAPS_HDCP_VERSION                  23:16
#define LW_RCV_HDCP22_RX_INFO                               HDCP22_DPCD_OFFSET_RX_RX_INFO
#define LW_RCV_HDCP22_RX_INFO_SIZE                          (0x02)
#define LW_RCV_HDCP22_RX_INFO_HDCP1_X_DEVICE_DOWNSTREAM     0:0
#define LW_RCV_HDCP22_RX_INFO_HDCP2_0_REPEATER_DOWNSTREAM   1:1
#define LW_RCV_HDCP22_RX_INFO_MAX_CASCADE_EXCEEDED          2:2
#define LW_RCV_HDCP22_RX_INFO_MAX_DEVS_EXCEEDED             3:3
#define LW_RCV_HDCP22_RX_INFO_DEVICE_COUNT                  8:4
#define LW_RCV_HDCP22_RX_INFO_DEVS_DEPTH                    11:9

// HDCP2.2 Authentication Time Intervals
// All the TimeIntervals are in Micro-Seconds
#define HDCP22_TIMER_POLL_INTERVAL              10000

#define HDCP22_TIMER_POLL_INTERVAL_HPRIME       10000
#define HDCP22_TIMER_POLL_INTERVAL_PAIRING      10000

// Reduce interval to be less than 100ms that can detect unplug at rxStatus read
#define HDCP22_TIMER_POLL_INTERVAL_RXID_LIST    50000

#define HDCP22_TIMER_POLL_INTERVAL_HPD          10000

#define HDCP22_TIMER_STAGE_DELAY                10000
#define HDCP22_TIMER_RXID_ACK_DELAY             10000
#define HDCP22_TIMER_STR_MANAGE_DELAY           100000
#define HDCP22_TIMEROUT_LPRIME                  20000
#define HDCP22_TIMEROUT_ENC_ENABLE              400000
// 1A-13T1 intermittent failure without retry, and maximum 1023 retry per spec.
#define HDCP22_LC_RETRIES                       4
#define HDCP22_M_RETRIES                        20
#define HDCP22_AKE_INIT_RETRY_MAX               2

// HDCP2.2 Authentication Timeouts
#define HDCP22_TIMEOUT_RXCERT                   100000
#define HDCP22_TIMEOUT_HPRIME_DP                200000
#define HDCP22_TIMEOUT_LPRIME(protocol)         (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 20000 : 16000)
#define HDCP22_TIMEOUT_MPRIME(protocol)         (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 200000 : 100000)
#define HDCP22_TIMEOUT_COUNT_HPRIME             1000000/HDCP22_TIMER_POLL_INTERVAL_HPRIME
#define HDCP22_TIMEOUT_COUNT_PAIRING            200000/HDCP22_TIMER_POLL_INTERVAL_PAIRING

//
// Intervals & reties for Certificate Rx/Lprime/Mprime to apply are different
// to (DP/HDMI). Meanwhile, without state registers to tell if CertRx, Lprime,
// Mprime on DP are ready, they are set with longest allowable timeout and poll
// for once.
//
#define HDCP22_TIMER_POLL_INTERVAL_RXCERT(protocol)     (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 10000 : HDCP22_TIMEOUT_RXCERT)
#define HDCP22_TIMER_POLL_INTERVAL_LPRIME(protocol)     (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 2000 : HDCP22_TIMEOUT_LPRIME(protocol))
#define HDCP22_TIMER_POLL_INTERVAL_MPRIME(protocol)     (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 10000 : HDCP22_TIMEOUT_MPRIME(protocol))
#define HDCP22_TIMEOUT_COUNT_RXCERT(protocol)           HDCP22_TIMEOUT_RXCERT/HDCP22_TIMER_POLL_INTERVAL_RXCERT(protocol)
#define HDCP22_TIMEOUT_COUNT_LPRIME(protocol)           HDCP22_TIMEOUT_LPRIME(protocol)/HDCP22_TIMER_POLL_INTERVAL_LPRIME(protocol)
#define HDCP22_TIMEOUT_COUNT_MPRIME(protocol)           HDCP22_TIMEOUT_MPRIME(protocol)/HDCP22_TIMER_POLL_INTERVAL_MPRIME(protocol)

#define HDCP22_TIMER_INTERVAL_LC_RETRY_WAR(protocol)    (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 600000 : HDCP22_TIMER_STAGE_DELAY)

//
// Verify CertRx takes too much time and possible doesn't detect L-HPD.
// We rely on RM to forward bHpdFromRm cmd to abort authenticaton for
// the situation. To be conservative, HDMI use minimum delay count as before.
//
#define HDCP22_TIMEOUT_MASTER_KEY(protocol)             (((protocol)==RM_FLCN_HDCP22_SOR_HDMI) ? 10000 : 200000)
#define HDCP22_TIMER_POLL_INTERVAL_MASTER_KEY           10000
#define HDCP22_TIMEOUT_COUNT_MASTER_KEY(protocol)       HDCP22_TIMEOUT_MASTER_KEY(protocol)/HDCP22_TIMER_POLL_INTERVAL_MASTER_KEY

//
// From aux trace, expiry time is only 94% of physical timeout value,
// and need to consider that for RxIdList long timeout waiting.
//
#define HDCP22_TIMEOUT_COUNT_RXID_LIST          3300000/HDCP22_TIMER_POLL_INTERVAL_RXID_LIST
#define HDCP22_TIMEOUT_COUNT_HPD                200000/HDCP22_TIMER_POLL_INTERVAL_HPD

#define HDCP22_MUTEX_ACQUIRE_TIMEOUT_NS         1000000  // 1ms

//
// DP Port Errata v3 Timing Requirement
// Timing in Decimal in Nanoseconds
//
#define HDCP22_TIMEOUT_RXCERT_READ_DP         110000000 // 110 ms
#define HDCP22_TIMEOUT_HPRIME_READ_DP         7000000   // 7 ms
#define HDCP22_TIMEOUT_PAIRING_READ_DP        5000000   // 5 ms
#define HDCP22_TIMEOUT_MPRIME_READ_DP         7000000   // 7 ms

/*!
 * Timeout value for PMGR mutex to access DISP scratch space register.
 * For HDCP2.2 type lock request from SEC2. Lwrent value is 250ms.
 */
// TODO: It needs to be refined later
#define HDCP22_TYPELOCK_MUTEX_ACQUIRE_TIMEOUT_NS   250000000

#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_STARTTIMER               0:0
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_STARTTIMER_NO            (0x00000000)
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_STARTTIMER_YES           (0x00000001)
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_STOPTIMER                1:1
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_STOPTIMER_NO             (0x00000000)
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_STOPTIMER_YES            (0x00000001)
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_RESPONSE                 2:2
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_RESPONSE_NO              (0x00000000)
#define LW_HDCP22_HSBUILD_RUNTIME_CTRL_PENDING_RESPONSE_YES             (0x00000001)

#ifdef HDCP22_DEBUG_MODE
#define LW_HDCP22_TEST_RCVR_COUNT               2
#endif

#define HDCP22_U32_ALIGN_INC_SIZE               (sizeof(LwU32) - 1)
#define HDCP22_AUX_PORT(pSession)               ((pSession)->linkIndex? (pSession)->secDDCPort : (pSession)->priDDCPort)

#define HDCP22_HWDRM_WAR_POLLING_INTERVAL_US    (100)
#define HDCP22_HWDRM_WAR_MAX_POLL_US            (16000)
#define HDCP22_HWDRM_WAR_MAX_POLL_ATTEMPTS      (HDCP22_HWDRM_WAR_MAX_POLL_US/HDCP22_HWDRM_WAR_POLLING_INTERVAL_US)

#define SELWREMEMORY_KSLT_ILWALID_OFFSET        (0xFFFFFFFF)
#define SELWREMEMORY_SECRET_MAX_SIZE            (HDCP22_SIZE_KD)

typedef struct
{
    LwU8   version;
    LwU16  rcvrCapsMask;
} HDCP22_RX_CAPS;

typedef enum
{
    HDCP22_MST_ENC_ILWALID=0,
    HDCP22_MST_ENC_NO_CHANGE,
    HDCP22_MST_ENC_NO_ACTIVE_TIMESLOTS,
    HDCP22_MST_ENC_DISABLE,
    HDCP22_MST_ENC_ENABLE,
    HDCP22_MST_ENC_ENABLE_MISMATCH,
} HDCP22_MST_ENC_REQ;

typedef enum
{
    HDCP22_SEC2LOCK_SUCCESS_TYPE1_DISABLED=0,
    HDCP22_SEC2LOCK_SUCCESS_HDCP22_OFF,
    HDCP22_SEC2LOCK_SUCCESS,
    HDCP22_SEC2LOCK_FAILURE,
} HDCP22_SEC2LOCK_STATUS;

typedef enum
{
    LW_HDCP22_RX_STATUS_ID_RECV_CERT=0,
    LW_HDCP22_RX_STATUS_ID_HPRIME=1,
    LW_HDCP22_RX_STATUS_ID_PAIRING_INFO=2,
    LW_HDCP22_RX_STATUS_ID_LPRIME=3,
    LW_HDCP22_RX_STATUS_ID_MPRIME=4,
    LW_HDCP22_RX_STATUS_ID_SIZE         = 5,
} LW_HDCP22_RX_STATUS_ID;

// MSG IDs for I2C HDCP22 Messagesr
typedef enum
{
    HDCP22_MSG_ID_NULL              = 1,
    HDCP22_MSG_ID_AKE_INIT,
    HDCP22_MSG_ID_AKE_SEND_CERT,
    HDCP22_MSG_ID_AKE_NO_STORED_KM,
    HDCP22_MSG_ID_AKE_STORED_KM,
    HDCP22_MSG_ID_AKE_SEND_RRX,
    HDCP22_MSG_ID_AKE_SEND_HPRIME,
    HDCP22_MSG_ID_AKE_SEND_PAIRING,
    HDCP22_MSG_ID_LC_INIT,
    HDCP22_MSG_ID_LC_SEND_LPRIME,
    HDCP22_MSG_ID_SKE_SEND_EKS_RIV,
    HDCP22_MSG_ID_RPT_RXIDS,
    HDCP22_MSG_ID_RPT_RXID_ACK      = 15,
    HDCP22_MSG_ID_RPT_STREAM_MANAGE = 16,
    HDCP22_MSG_ID_RPT_STREAM_READY  = 17,
} HDCP22_MSG_ID;

// HDCP22 Random number Type
typedef enum
{
    HDCP22_RAND_TYPE_RTX = 0,
    HDCP22_RAND_TYPE_RRX    ,
    HDCP22_RAND_TYPE_KM     ,
    HDCP22_RAND_TYPE_RN     ,
    HDCP22_RAND_TYPE_KS     ,
    HDCP22_RAND_TYPE_RIV    ,
    HDCP22_RAND_TYPE_SESSION,
} HDCP22_RAND_TYPE;

// HDCP22 Random number Type
typedef enum
{
    HDCP22_SECRET_TYPE_ILWALID = 0,
    HDCP22_SECRET_TYPE_KM         ,
    HDCP22_SECRET_TYPE_KD         ,
} HDCP22_SECRET_TYPE;

// HDCP Version
typedef enum
{
    HDCP22_VERSION_20 = 0,
    HDCP22_VERSION_21    ,
    HDCP22_VERSION_22    ,
} HDCP22_VERSION;

// HDCP22 Variables used in SKE stage
typedef struct
{
    LwU32   Rn[HDCP22_SIZE_R_N/sizeof(LwU32)];
} HDCP22_SESSION_VARS_SKE;

// HDCP22 Variables used in AKE stage
typedef struct
{
    LwU32   rTx[HDCP22_SIZE_R_TX/sizeof(LwU32)];
    LwU64   rRx[HDCP22_SIZE_R_RX/sizeof(LwU64)];   // LwU64 aligned for Eks computation.

    LwU8    receiverId[HDCP22_SIZE_RECV_ID_8];
} HDCP22_SESSION_VARS_AKE;

// HDCP22 Variables used in Repeater stage
typedef struct
{
    LwU16           rxInfo;
    LwU8            Vprime[HDCP22_SIZE_V_PRIME];
    LwU8            Vlsb[HDCP22_SIZE_V_PRIME];
    LwU8            numDevices;
    LwU16           numStreams;
    HDCP22_STREAM   streamIdType[HDCP22_NUM_STREAMS_MAX];   // LS storage to keep type information sent to sink.
    LwU32           dpTypeMask[HDCP22_NUM_DP_TYPE_MASK];
    LwU32           seqNumV;
    LwU32           seqNumM;
    LwBool          bEnforceType0Hdcp1xDS;
} HDCP22_SESSION_VARS_RPTR;

// HDCP22 state variables that integrity is required.
typedef struct
{
    LwU16           hdcp22LcRetryCount;
    LwBool          bRepeater;
    LwU8            hdcp22authRetryCount;

    LwBool          bIsStreamTypeLocked;
    LwBool          bSec2ReAuthTypeEnforceReq;
    LwBool          bTypeChangeRequired;
} HDCP22_SESSION_VARS_IR;

// HDCP22 Authentication Stages
typedef enum
{
    HDCP22_AUTH_STAGE_NULL = 0,
    HDCP22_AUTH_STAGE_WAIT_FOR_RX_CERT,
    HDCP22_AUTH_STAGE_WAIT_TO_MASTER_KEY_EXCHANGE,
    HDCP22_AUTH_STAGE_WAIT_FOR_H_PRIME,
    HDCP22_AUTH_STAGE_WAIT_FOR_PAIRING,
    HDCP22_AUTH_STAGE_WAIT_FOR_LC_INIT,
    HDCP22_AUTH_STAGE_WAIT_FOR_LPRIME,
    HDCP22_AUTH_STAGE_WAIT_FOR_SKE,
    HDCP22_AUTH_STAGE_WAIT_TO_ENABLE_ENC,
    HDCP22_AUTH_STAGE_WAIT_TO_START_RPTR,
    HDCP22_AUTH_STAGE_WAIT_TO_READ_RXID,
    HDCP22_AUTH_STAGE_WAIT_TO_RXID_ACK,
    HDCP22_AUTH_STAGE_WAIT_FOR_STREAM_MANAGE,
    HDCP22_AUTH_STAGE_WAIT_FOR_MPRIME,
    HDCP22_AUTH_STAGE_WAIT_FOR_REAUTH,
} HDCP22_AUTH_STAGE;

// HDCP22 Rx Certificate Structure
typedef struct
{
    LwU8     id[HDCP22_SIZE_RECV_ID_8];
    LwU8     modulus[HDCP22_SIZE_RX_MODULUS_8];
    LwU8     exponent[HDCP22_SIZE_RX_EXPONENT_8];
    LwU8     rfu1;
    LwU8     rfu2;
    LwU8     signature[HDCP22_SIZE_RX_MODULUS_ROOT_8];
} HDCP22_CERTIFICATE;

typedef struct
{
    LwU8     rfu1;
    LwU8     rfu2;
    LwU8     signature[HDCP22_SIZE_RX_MODULUS_ROOT_8];
} HDCP22_DCP_SIGNATURE;

// HDCP22 Pairing Info storage
typedef struct
{
    LwU8     rxId[HDCP22_SIZE_RECV_ID_8];
    LwU8     Km[HDCP22_SIZE_KM];
} HDCP22_PAIRING_INFO;

// HDCP22 Session Structure
typedef struct
{
    // Note: Put the field var in front of struct because needs LwU32 alignment for HW integrity computation..
    HDCP22_SESSION_VARS_IR      sesVariablesIr GCC_ATTRIB_ALIGN(sizeof(LwU32));    // State variables that integrity required.
    HDCP22_SESSION_VARS_AKE     sesVariablesAke;                                        // Authentication and key exchange stage variables.
    HDCP22_SESSION_VARS_SKE     sesVariablesSke;                                        // Session key exchange stage variables.
    HDCP22_SESSION_VARS_RPTR    sesVariablesRptr;                                       // Repeater authentication stage variables.

    // Hash value for state variables integrity check. TODO: move to protected scratch reg if later HW supports.
    LwU32                       integrityHashVal[HDCP22_SIZE_INTEGRITY_HASH_U32];

    LwU8                        txCaps[HDCP22_SIZE_TX_CAPS];
    LwU8                        rxCaps[HDCP22_SIZE_TX_CAPS];
    LwU16                       hdcp22MRetryCount;

    RM_FLCN_MEM_DESC            srmDma;

    LwU16                       hdcp22TimerCount;
    LwU32                       seqNumV;

    // Enums
    HDCP22_VERSION              hdcpVersion;
    HDCP22_AUTH_STAGE           stage;
    RM_FLCN_HDCP22_STATUS       msgStatus;
    RM_FLCN_HDCP22_SOR_PROTOCOL sorProtocol;

    LwBool                      bReadKsvList;
    LwBool                      bReadyNotified;
    LwBool                      bReAuthNotified;
    LwU8                        priDDCPort;
    LwU8                        secDDCPort;
    LwU8                        cmdSeqNumId;
    LwU8                        cmdQueueId;
    LwU8                        sorNum;
    LwU8                        linkIndex;
    LwU8                        maxNoOfSors;
    LwU8                        maxNoOfHeads;

    LwBool                      bIsStateMachineActive;
    LwBool                      bIsAux;
    LwBool                      bHandleHpdFromRM;
    LwBool                      bApplyHwDrmType1LockedWar;
} HDCP22_SESSION;

// HDCP 2.2 Active Session Structure
typedef struct
{
    LwU8                priDDCPort;
    LwU8                secDDCPort;
    LwU8                sorNum;
    LwBool              bActive;
    LwU32               seqNumV;
    LwU16               numStreams;
    HDCP22_STREAM       streamIdType[HDCP22_NUM_STREAMS_MAX];
    RM_FLCN_MEM_DESC    srmDma;
    RM_FLCN_HDCP22_SOR_PROTOCOL sorProtocol;
    LwBool              bIsAux;
    LwBool              bIsRepeater;
    LwBool              bTypeChangeRequired;
    LwBool              bEnforceType0Hdcp1xDS;
} HDCP22_ACTIVE_SESSION;

#define HDCP22_INTEGRITY_CHECK_SIZE_MAX     (LW_ALIGN_UP(LW_MAX(sizeof(HDCP22_ACTIVE_SESSION), sizeof(HDCP22_SESSION_VARS_IR)), SCP_BUF_ALIGNMENT))
#define HDCP22_ACTIVE_SESSIONS_MAX          4
/*!
 * @brief These are the subtypes defined for HDCP22 to differentiate
 * between each intr.
 */
#define HDCP22_SOR_INTR                     0x00
#define HDCP22_TIMER0_INTR                  0x01
#define HDCP22_SEC2_INTR                    0x02

// Hdcp22 timer is msec order and using max of 2msec, 1/64 as tolerance now.
#define HDCP22_TIMER_TOLERANCE(intervalUs)  LW_MAX(((intervalUs)>>6), 2000)

typedef enum
{
    HDCP22_TIMER_LEGAL_EVT = 0,
    HDCP22_TIMER_ILLEGAL_EVT,
} HDCP22_TIMER_EVT_TYPE;

typedef union
{
    /*!
     * Indicates the SOR for which HDCP22 needs to be disabled.
     */
    LwU8                    sorNum;

    /*!
     * Indicates HDCP22 timer timeout event type that is legal or not.
     */
    HDCP22_TIMER_EVT_TYPE   timerEvtType;
} HDCP22_EVT_INFO;

/*!
 * @brief HDCP22 event stucture used for SOR and timer interrupts.
 */
typedef struct
{
    /*!
     * This field is intended to store a unique-id that represents the
     * type of work-item that is being dispatched.
     */
    LwU8    eventType;

    /*!
     * This field is used to differentiate SOR and timer0 intrs.
     */
    LwU8    subType;

    /*!
     * This field is used to tell information of event.
     */
    HDCP22_EVT_INFO eventInfo;
} HDCP22_EVT;

typedef enum
{
    ENC_DISABLE                 = 0,
    ENC_ENABLE                  = 1,
    HDMI_NONREPEATER_TYPECHANGE = 2,
} HDCP22_ENC_CONTROL;

#endif // HDCP22WIRED_PROTOCOL_H
