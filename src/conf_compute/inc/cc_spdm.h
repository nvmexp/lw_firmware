/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef _CC_SPDM_H_
#define _CC_SPDM_H_
#include "cc_dma.h"

#ifdef USE_LIBSPDM
#include "spdm_responder_lib_internal.h"
#include "lw_gsp_spdm_lib_config.h"
#include "lw_utils.h"

/* ------------------------- Macros and Defines ----------------------------- */
//
// CTExponent is byte value used to determine when requester can retry message
// due to timeout. This timeout is callwlated as (2^CTExponent) microseconds.
// We expect RM to always block for response - therefore, simply use max value.
//
#define CC_SPDM_CT_EXPONENT           (0xFF)

// APM-TODO CONFCOMP-397: Ensure all required capabilities are added.
#define CC_SPDM_CAPABILITY_FLAGS      (SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP       | \
                                       SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCRYPT_CAP    | \
                                       SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MAC_CAP        | \
                                       SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_EX_CAP     | \
                                       SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_SIG   | \
                                       SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_UPD_CAP    | \
                                       SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_FRESH_CAP)

#define CC_SPDM_BASE_ASYM_ALGO        (SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384)
#define CC_SPDM_BASE_HASH_ALGO        (SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384)
#define CC_SPDM_DHE_GROUP             (SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1)
#define CC_SPDM_REQ_ASYM_ALGO         (SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384)
#define CC_SPDM_AEAD_SUITE            (SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM)
#define CC_SPDM_MEASUREMENT_HASH_ALGO (SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384)
#define CC_SPDM_MEASUREMENT_SPEC      (SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF)
#define CC_SPDM_KEY_SCHEDULE          (SPDM_ALGORITHMS_KEY_SCHEDULE_HMAC_HASH)
#define CC_SPDM_CERT_SLOT_COUNT       (1)
#define CC_SPDM_CERT_SLOT_ID          (0)
#endif

// TODO: 0x700 could be too less, but GSP-RISCV doesn't have more DMEM space.
#define CC_SPDM_MESSAGE_BUFFER_SIZE_IN_BYTE   (0x700)
#define CC_SPDM_MESSAGE_BUFFER_ALIGNMENT      (256)

//
// [31:24] FalconId : GSP-RISCV is 0xC
// [23:16] Falcon Instance : Lwrrently, only 1 GSP exist.
// In POR, the GSP MAX SPDM endpoint count is 8, so we define 8 endpoint Id.
//
#define CC_GSP_SPDM_ENDPOINT_ID_0    0x0C000000
#define CC_GSP_SPDM_ENDPOINT_ID_1    0x0C000001
#define CC_GSP_SPDM_ENDPOINT_ID_2    0x0C000002
#define CC_GSP_SPDM_ENDPOINT_ID_3    0x0C000003
#define CC_GSP_SPDM_ENDPOINT_ID_4    0x0C000004
#define CC_GSP_SPDM_ENDPOINT_ID_5    0x0C000005
#define CC_GSP_SPDM_ENDPOINT_ID_6    0x0C000006
#define CC_GSP_SPDM_ENDPOINT_ID_7    0x0C000007

typedef enum
{
    PART_STATE_UNKNOWN = 0,
    PART_STATE_READY,           // The SPDM partition state is ready(after init command), GSP-SPDM can handle SPDM message.
    PART_STATE_FREE,            // The SPDM partition state is free(after deinit command), can receive init command only.
    PART_STATE_CTRL_PROCESSING, // The SPDM partition is processing SPDM message and not finish yet.
} LW_SPDM_PART_STATE;

typedef struct _CC_SPDM_PART_CTX
{
    LW_SPDM_PART_STATE state;
    CC_DMA_PROP        dmaProp;
    LwU32              guestId;
    LwU32              endpointId;
    LwU32              seqNum;
    LwU32              rmBufferSizeInByte;
} CC_SPDM_PART_CTX, *PCC_SPDM_PART_CTX;

extern FLCN_STATUS
spdmWriteDescHdr
(
    PLW_SPDM_DESC_HEADER pDescHdrRsp
);

extern FLCN_STATUS
spdmReadDescHdr
(
    PLW_SPDM_DESC_HEADER pDescHdrReq
);

extern FLCN_STATUS
spdmMsgProcess
(
    PRM_GSP_SPDM_CC_CTRL_CTX   pCtrlCtx,
    void                       *pContext
);

#ifdef USE_LIBSPDM

extern FLCN_STATUS
spdmLibContextInit
(
   void   *pContext
);

#endif

#endif // CC_SPDM_H

