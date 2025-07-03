/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <liblwriscv/io.h>
#include <liblwriscv/dma.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwpu-sdk/lwmisc.h>
#include "gsp/gspifspdm.h"
#include "misc.h"
#include "flcnretval.h"
#include "cc_spdm.h"
#include "misc.h"
#include "partitions.h"

#ifdef USE_LIBSPDM
#include "spdm_responder_lib_internal.h"
#include "spdm_lib_config.h"
#include "lw_crypt.h"
#endif

extern CC_SPDM_PART_CTX     g_ccSpdmPartCtx;
extern LW_SPDM_DESC_HEADER  g_spdmDescHdr;
extern LwU8                 g_spdmMsg[];

FLCN_STATUS
spdmWriteDescHdr
(
    PLW_SPDM_DESC_HEADER pDescHdrRsp
)
{
    FLCN_STATUS    status;
    PCC_DMA_PROP   pDmaProp = &g_ccSpdmPartCtx.dmaProp;

    // Just copy g_spdmDescHdrRsp to external memory (Lwrrently is SYS memory)
    status = confComputeIssueDma((void *)pDescHdrRsp,
                                 LW_FALSE,
                                 0,
                                 sizeof(LW_SPDM_DESC_HEADER),
                                 CC_DMA_TO_SYS,
                                 CC_DMA_SYNC_ALL,
                                 pDmaProp);

    return status;
}

FLCN_STATUS
spdmReadDescHdr
(
    PLW_SPDM_DESC_HEADER pDescHdrReq
)
{
    FLCN_STATUS    status;
    PCC_DMA_PROP   pDmaProp = &g_ccSpdmPartCtx.dmaProp;

    // Just copy g_spdmDescHdrRsp to external memory (Lwrrently is SYS memory)
    status = confComputeIssueDma((void *)pDescHdrReq,
                                 LW_FALSE,
                                 0,
                                 sizeof(LW_SPDM_DESC_HEADER),
                                 CC_DMA_FROM_SYS,
                                 CC_DMA_SYNC_ALL,
                                 pDmaProp);

    return status;
}

static LwBool
spdmDescHdrSanityCheck
(
    PLW_SPDM_DESC_HEADER pDescHdr
)
{
    if (pDescHdr->version != LW_SPDM_DESC_HEADER_VERSION_LWRRENT)
    {
        return LW_FALSE;
    }

    if (pDescHdr->guestId != g_ccSpdmPartCtx.guestId ||
        pDescHdr->endpointId != g_ccSpdmPartCtx.endpointId)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

#ifdef USE_LIBSPDM
//
// TODO: We lwrrently do not support AK or KeyEx certificates in GSP-SPDM.
// In the meantime, we will hardcode a template certificate and key for signing.
//
typedef struct
{
    // Length of the certificate.
    LwU16 length;

    // Reserved.
    LwU16 reserved;

    // Hash of the certificate.
    LwU8  root_hash[SHA384_DIGEST_SIZE];

    // The actual certificate blob.
    unsigned char certChain[1024];
} SPDM_CERT_HEADER;

static SPDM_CERT_HEADER g_certHdr =
{
    // Certificate size is lwrrently 560 bytes. This must be kept up-to-date with certificate.
    560,

    // Reserved.
    0,

    // Libspdm does not check for hash validity lwrrently, so we can leave blank.
    { 0 },

    // Hardcoded ECC-384 Certificate.
    {
        0x30, 0x82, 0x02, 0x2c, 0x30, 0x82, 0x01, 0xb2, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x14, 0x6c,
        0xc2, 0x67, 0x1b, 0x33, 0x8a, 0x99, 0xad, 0x4d, 0x0a, 0xc1, 0xf3, 0x78, 0xd4, 0x5a, 0xf1, 0x1d,
        0xf2, 0xb6, 0x15, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03, 0x30,
        0x4d, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x0b,
        0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x02, 0x43, 0x41, 0x31, 0x14, 0x30, 0x12, 0x06,
        0x03, 0x55, 0x04, 0x07, 0x0c, 0x0b, 0x53, 0x61, 0x6e, 0x74, 0x61, 0x20, 0x43, 0x6c, 0x61, 0x72,
        0x61, 0x31, 0x1b, 0x30, 0x19, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x12, 0x4e, 0x56, 0x49, 0x44,
        0x49, 0x41, 0x20, 0x43, 0x6f, 0x72, 0x70, 0x6f, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x30, 0x1e,
        0x17, 0x0d, 0x32, 0x32, 0x30, 0x32, 0x30, 0x31, 0x32, 0x30, 0x32, 0x30, 0x33, 0x35, 0x5a, 0x17,
        0x0d, 0x32, 0x32, 0x30, 0x33, 0x30, 0x33, 0x32, 0x30, 0x32, 0x30, 0x33, 0x35, 0x5a, 0x30, 0x4d,
        0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x0b, 0x30,
        0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x02, 0x43, 0x41, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03,
        0x55, 0x04, 0x07, 0x0c, 0x0b, 0x53, 0x61, 0x6e, 0x74, 0x61, 0x20, 0x43, 0x6c, 0x61, 0x72, 0x61,
        0x31, 0x1b, 0x30, 0x19, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x12, 0x4e, 0x56, 0x49, 0x44, 0x49,
        0x41, 0x20, 0x43, 0x6f, 0x72, 0x70, 0x6f, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x30, 0x76, 0x30,
        0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00,
        0x22, 0x03, 0x62, 0x00, 0x04, 0xa0, 0x46, 0x3e, 0xa8, 0x8b, 0xed, 0xe5, 0xe8, 0x86, 0xca, 0xae,
        0xbb, 0x66, 0x6e, 0x3a, 0x2f, 0x93, 0x7a, 0xb4, 0x75, 0x4a, 0x36, 0x0b, 0x23, 0x40, 0xeb, 0x07,
        0x89, 0xc5, 0x3e, 0x91, 0xef, 0x98, 0xa1, 0x48, 0xd9, 0x77, 0x9f, 0xdd, 0x4c, 0x56, 0x9e, 0x22,
        0x9d, 0x56, 0xd0, 0x34, 0x43, 0x0a, 0x85, 0x48, 0x01, 0x06, 0x34, 0x95, 0x43, 0x28, 0xe1, 0xfd,
        0xd2, 0x30, 0xa2, 0xf7, 0x10, 0xd0, 0xfe, 0xca, 0x1e, 0xee, 0x71, 0xc6, 0x4f, 0x8f, 0x63, 0xb6,
        0x87, 0xf1, 0x7a, 0xc2, 0x4e, 0x7e, 0x63, 0x3c, 0xbc, 0x81, 0xa1, 0x9c, 0xd3, 0x6a, 0x3b, 0x59,
        0x69, 0xf9, 0x60, 0x8a, 0x07, 0xa3, 0x53, 0x30, 0x51, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e,
        0x04, 0x16, 0x04, 0x14, 0x11, 0xde, 0x13, 0xa6, 0xa5, 0xf3, 0xf9, 0xa7, 0x29, 0xad, 0xc4, 0xd1,
        0xda, 0x5f, 0x23, 0x0e, 0x7d, 0x65, 0x41, 0x77, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04,
        0x18, 0x30, 0x16, 0x80, 0x14, 0x11, 0xde, 0x13, 0xa6, 0xa5, 0xf3, 0xf9, 0xa7, 0x29, 0xad, 0xc4,
        0xd1, 0xda, 0x5f, 0x23, 0x0e, 0x7d, 0x65, 0x41, 0x77, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13,
        0x01, 0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
        0x48, 0xce, 0x3d, 0x04, 0x03, 0x03, 0x03, 0x68, 0x00, 0x30, 0x65, 0x02, 0x31, 0x00, 0xec, 0xcd,
        0xb5, 0x69, 0x1f, 0xc4, 0x0b, 0x4a, 0x16, 0x0e, 0xc8, 0x89, 0x32, 0xb5, 0x5b, 0x91, 0x38, 0x45,
        0x75, 0xf1, 0x5e, 0x8d, 0x58, 0xbc, 0x3b, 0x0d, 0xce, 0x97, 0xd7, 0x5a, 0x0e, 0x9d, 0x7b, 0x38,
        0x17, 0xf0, 0x95, 0xc0, 0xc8, 0x5f, 0xc1, 0x8f, 0x24, 0x75, 0xdc, 0x29, 0x9d, 0x17, 0x02, 0x30,
        0x1f, 0x4b, 0xc3, 0x75, 0x52, 0xa6, 0x51, 0x61, 0x50, 0x3b, 0x76, 0xdd, 0x33, 0xe0, 0xd3, 0x5b,
        0x53, 0x1e, 0x7d, 0x89, 0x1a, 0xd1, 0x10, 0xee, 0xa4, 0x60, 0x80, 0x43, 0x7c, 0x02, 0x0e, 0xae,
        0x8d, 0x79, 0x97, 0xe3, 0x1b, 0x57, 0xc2, 0x53, 0xfa, 0x6f, 0x38, 0x08, 0x24, 0x38, 0x8e, 0x26,
    },
};

#pragma GCC diagnostic push
FLCN_STATUS
spdmLibContextInit
(
   void   *pContext
)
{
    return_status          spdmStatus    = RETURN_SUCCESS;
    FLCN_STATUS            flcnStatus    = FLCN_OK;
    LwU8                   ctExponent    = 0;
    LwU32                  capFlags      = 0;
    uint32                 baseAsymAlgo  = 0;
    uint32                 baseHashAlgo  = 0;
    uint16                 aeadSuite     = 0;
    uint16                 dheGroup      = 0;
    uint16                 keySched      = 0;
    uint16                 reqAsymAlg    = 0;
    uint8                  measSpec      = 0;
    uint32                 measHashAlgo  = 0;
    uint8                  slotCount     = 0;
    spdm_data_parameter_t  parameter;

    //
    // TODO: We don't have support for valid AK or KeyEx certificates.
    // In the meantime, use the same hardcoded certificate and key for both.
    //
    uint8  *pCertChain    = (uint8 *)&g_certHdr;
    uint32  certChainSize = (uint32)sizeof(g_certHdr) - 1024 + g_certHdr.length;

    // Initialize all SPDM Requester attributes to CC values.
    ctExponent   = CC_SPDM_CT_EXPONENT;
    capFlags     = CC_SPDM_CAPABILITY_FLAGS;
    baseAsymAlgo = CC_SPDM_BASE_ASYM_ALGO;
    baseHashAlgo = CC_SPDM_BASE_HASH_ALGO;
    aeadSuite    = CC_SPDM_AEAD_SUITE;
    dheGroup     = CC_SPDM_DHE_GROUP;
    keySched     = CC_SPDM_KEY_SCHEDULE;
    reqAsymAlg   = CC_SPDM_BASE_ASYM_ALGO;
    measSpec     = CC_SPDM_MEASUREMENT_SPEC;
    measHashAlgo = CC_SPDM_MEASUREMENT_HASH_ALGO;

    // Initialize libspdm context struct
    spdm_init_context(pContext);

    // Set SPDM Requester attributes.
    // TODO : portMemSet(&parameter, 0, sizeof(parameter));
    uint8  *pU8 = (uint8 *)&parameter;
    uint32  i;

    for (i = 0; i < sizeof(parameter); i++)
    {
        pU8[i] = 0;
    }

    parameter.location = SPDM_DATA_LOCATION_LOCAL;

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_CAPABILITY_CT_EXPONENT,
                                    &parameter, &ctExponent, sizeof(ctExponent)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_CAPABILITY_FLAGS,
                                    &parameter, &capFlags, sizeof(capFlags)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_BASE_ASYM_ALGO,
                                    &parameter, &baseAsymAlgo,
                                    sizeof(baseAsymAlgo)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_BASE_HASH_ALGO,
                                    &parameter, &baseHashAlgo,
                                    sizeof(baseHashAlgo)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_AEAD_CIPHER_SUITE,
                                    &parameter, &aeadSuite, sizeof(aeadSuite)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_DHE_NAME_GROUP,
                                    &parameter, &dheGroup, sizeof(dheGroup)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_KEY_SCHEDULE,
                                    &parameter, &keySched, sizeof(keySched)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_REQ_BASE_ASYM_ALG,
                                    &parameter, &reqAsymAlg,
                                    sizeof(reqAsymAlg)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_MEASUREMENT_SPEC,
                                    &parameter, &measSpec, sizeof(measSpec)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_MEASUREMENT_HASH_ALGO,
                                    &parameter, &measHashAlgo,
                                    sizeof(measHashAlgo)));

    // Store certificate slot count before storing certificate.
    slotCount = CC_SPDM_CERT_SLOT_COUNT;
    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_LOCAL_SLOT_COUNT,
                                    &parameter, &slotCount, sizeof(slotCount)));

    // Specify certificate location, passing slot number as well.
    parameter.additional_data[0] = CC_SPDM_CERT_SLOT_ID;
    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_LOCAL_PUBLIC_CERT_CHAIN,
                                    &parameter, pCertChain, certChainSize));
    // Ensure we clear after usage.
    parameter.additional_data[0] = 0;

ErrorExit:

    flcnStatus = (spdmStatus == RETURN_SUCCESS ? FLCN_OK : FLCN_ERROR);

    return flcnStatus;
}
#pragma GCC diagnostic pop
#endif

FLCN_STATUS
spdmMsgProcess
(
    PRM_GSP_SPDM_CC_CTRL_CTX    pCtrlCtx,
    void                       *pContext
)
{
    PCC_DMA_PROP         pDmaProp     = &g_ccSpdmPartCtx.dmaProp;
    LwU8                *pMsgBufReq   = g_spdmMsg;
    PLW_SPDM_DESC_HEADER pDescHdr     = &g_spdmDescHdr;
    LwU32                msgAlignSizeByte;
    FLCN_STATUS          flcnStatus   = FLCN_OK;

    (void) pContext;

#ifdef USE_LIBSPDM
    //
    // TODO: Unify libspdm variables type and align with lwtypes.
    // Lwrrently, spdmlib doesn't move all variables to lwtypes.
    // Pass LwU32 pointer to spdmlib in GSP-RISCV will cause trp assertion in some APIs.
    //
    uintn                responseSize = CC_SPDM_MESSAGE_BUFFER_SIZE_IN_BYTE;
    LwU32                responseSizeRet;
    LwU8                *pMsgBufRsp   = g_spdmMsg;  // Request and response message share same buffer.
    return_status        spdmStatus   = RETURN_SUCCESS;
    uint32              *pSessionId   = NULL;
    spdm_context_t      *pSpdmContext = (spdm_context_t  *)pContext;
#endif
    //
    // 1. Read LW_SPDM_DESC_HEADER from RM buffer by DMA.
    // 2. Save seqNum to g_ccSpdmPartCtx.seqNum.
    // 3. Read SPDM request message to DMEM by DMA and pass to SPDM library
    //

    // Read SPDM descriptor header from external memory(Lwrrently from SYS memory)
    flcnStatus = spdmReadDescHdr(pDescHdr);

    if (flcnStatus != FLCN_OK)
    {
        return flcnStatus;
    }

    if (!spdmDescHdrSanityCheck(pDescHdr))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read message from external memory(Lwrrently from SYS memory)
    msgAlignSizeByte = LW32_ALIGN_UP(pDescHdr->msgSizeByte, LW_SPDM_DESC_HEADER_ALIGNMENT);

    flcnStatus = confComputeIssueDma((void *)pMsgBufReq,
                                     LW_FALSE,
                                     pDescHdr->msgOffset,
                                     msgAlignSizeByte,
                                     CC_DMA_FROM_SYS,
                                     CC_DMA_SYNC_ALL,
                                     pDmaProp);
    if (flcnStatus != FLCN_OK)
    {
        pPrintf("GSP-SPDM DMA read message failed(0x%x) !! \n ", flcnStatus);

        pDescHdr->status = flcnStatus;

        spdmWriteDescHdr(pDescHdr);

        return flcnStatus;
    }

    // Save sequence number of message
    g_ccSpdmPartCtx.seqNum = pCtrlCtx->ctrlParam;

#ifdef USE_LIBSPDM
#pragma GCC diagnostic push

    spdmStatus = spdm_process_message(pSpdmContext, &pSessionId,
                                      pMsgBufReq, (uintn) pDescHdr->msgSizeByte,
                                      pMsgBufRsp, &responseSize);

#pragma GCC diagnostic pop
    responseSizeRet = (LwU32) responseSize;

    pPrintf("SPDM-REQ msg size = 0x%x, SPDM-RSP msg size = 0x%x \n", pDescHdr->msgSizeByte, responseSizeRet);

    if (spdmStatus != RETURN_SUCCESS)
    {
        pPrintf("CC spdm_process_message() failed, spdmStatus = 0x%llx \n", spdmStatus);
        flcnStatus = FLCN_ERROR;

        pDescHdr->status = flcnStatus;

        spdmWriteDescHdr(pDescHdr);

        return flcnStatus;
    }

    pDescHdr->status = FLCN_OK;
    pDescHdr->msgOffset = LW_SPDM_DESC_MSG_OFFSET_IN_BYTES;
    pDescHdr->msgSizeByte = responseSizeRet;

    // Write message to external memory(Lwrrently, to SYS memory)
    msgAlignSizeByte = LW32_ALIGN_UP(responseSizeRet, LW_SPDM_DESC_HEADER_ALIGNMENT);

    // Write SPDM response message to external memory(Lwrrently from SYS memory)
    flcnStatus = confComputeIssueDma((void *)pMsgBufRsp,
                                     LW_FALSE,
                                     pDescHdr->msgOffset,
                                     msgAlignSizeByte,
                                     CC_DMA_TO_SYS,
                                     CC_DMA_SYNC_ALL,
                                     pDmaProp);
    if (flcnStatus != FLCN_OK)
    {
        pPrintf("GSP-SPDM DMA write message failed(0x%x) !! \n ", flcnStatus);
    }

    pDescHdr->status = flcnStatus;

    spdmWriteDescHdr(pDescHdr);

    return flcnStatus;

#else

   pPrintf("USE_LIBSPDM is NOT defined, this will cause message process error !! \n");
   return FLCN_ERROR;
#endif
}

