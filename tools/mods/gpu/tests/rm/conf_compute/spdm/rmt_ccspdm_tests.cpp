/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
//!
//! \file rmt_ccspdm_tests.cpp
//! \brief Sanity test for Confidential Compute.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "gpu/tests/rm/spdm/rmt_spdmdefines.h"
#include "gpu/tests/rm/spdm/gsp/rmt_gsp_cc_spdmdefines.h"
#include "gpu/tests/rm/conf_compute/rmt_ccspdm.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "rmlsfm.h"
#include "spdm/rmspdmtransport.h"
#include "spdm/rmspdmselwredif.h"
#include "spdm/rmspdmvendordef.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"

#include "rmgspcmdif.h"

#define  CHECK_AND_RETURN_IF_ERROR(cond, errorCode, label) \
         if (!(cond))                                      \
         {                                                 \
            MASSERT(0);                                    \
            rc = errorCode;                                \
            goto label;                                    \
         }

/*!
 * VERSION positive test case. Ensures successful response
 * of VERSION from valid GET_VERSION request.
 */
RC CCSpdmTest::SpdmGetVersionTest()
{
    #define  GET_VERSION_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_GET_VERSION) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  GET_VERSION_EXPECTED_RESPONSE_MSG_SIZE sizeof(SPDM_MESSAGE_VERSION)
    #define  GET_VERSION_RESPONSE_MSG_BUFFER_SIZE   GET_VERSION_EXPECTED_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                      reqBuffer[GET_VERSION_REQUEST_MSG_BUFFER_SIZE];
    LwU32                     reqBufferSize = GET_VERSION_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER   pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_MESSAGE_GET_VERSION pReqGetVersion;
    LwU32                     expectedRspSize = GET_VERSION_EXPECTED_RESPONSE_MSG_SIZE;
    LwU32                     rspBufferSize   = GET_VERSION_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                      rspBuffer[GET_VERSION_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_VERSION     pRspVersion;
    LwU32                     rspMessageSize;
    RC                        rc      = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = GET_VERSION_REQUEST_MSG_BUFFER_SIZE;
    pLwSpdmMsgHeader->flags     = 0;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_NORMAL;

    // GET_VERSION request, requests list of supported versions from Responder.
    pReqGetVersion = (PSPDM_MESSAGE_GET_VERSION) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqGetVersion->GetVersion.SpdmVersionId       = SpdmVersionId_1_0;
    pReqGetVersion->GetVersion.RequestResponseCode = SpdmRequestIdGetVersion;
    pReqGetVersion->GetVersion.Reserved1           = 0;
    pReqGetVersion->GetVersion.Reserved2           = 0;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize = pLwSpdmMsgHeader->size;
    pRspVersion    = (PSPDM_MESSAGE_VERSION)(rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                           == expectedRspSize,       RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspVersion->Version.SpdmVersionId       == SpdmVersionId_1_0,     RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspVersion->Version.RequestResponseCode == SpdmResponseIdVersion, RC::LWRM_ERROR, Exit);

    // TODO: Do we want to validate anything else?
Exit:
    return rc;
}

#define SPDM_EXPECTED_REQUESTER_CAPABILITIES (SpdmCapabilityCertCap     | \
                                              SpdmCapabilityEncryptCap  | \
                                              SpdmCapabilityMacCap      | \
                                              SpdmCapabilityKeyExCap    | \
                                              SpdmCapabilityKeyUpdCap)

// TODO: Class implementing tests should define these expected capabilities
#define SPDM_EXPECTED_CT_EXPONENT            (0xFF)

#define SPDM_EXPECTED_RESPONDER_CAPABILITIES (SpdmCapabilityCertCap     | \
                                              SpdmCapabilityEncryptCap  | \
                                              SpdmCapabilityMacCap      | \
                                              SpdmCapabilityKeyExCap    | \
                                              SpdmCapabilityMeasSigCap  | \
                                              SpdmCapabilityKeyUpdCap   | \
                                              SpdmCapabilityMeasFreshCap)
/*!
 * CAPABILITIES positive test case. Ensures successful response
 * of CAPABILITIES from valid GET_CAPABILITIES request.
 */
RC CCSpdmTest::SpdmGetCapabilitiesTest()
{
    #define  GET_CAPABILITIES_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_GET_CAPABILITIES) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  GET_CAPABILITIES_EXPECTED_RESPONSE_MSG_SIZE sizeof(SPDM_MESSAGE_CAPABILITIES)
    #define  GET_CAPABILITIES_RESPONSE_MSG_BUFFER_SIZE   GET_CAPABILITIES_EXPECTED_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                            reqBuffer[GET_CAPABILITIES_REQUEST_MSG_BUFFER_SIZE];
    LwU32                           reqBufferSize = GET_CAPABILITIES_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER         pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_MESSAGE_GET_CAPABILITIES  pReqGetCapabilities;
    LwU32                           expectedRspSize = GET_CAPABILITIES_EXPECTED_RESPONSE_MSG_SIZE;
    LwU32                           rspBufferSize   = GET_CAPABILITIES_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                            rspBuffer[GET_CAPABILITIES_RESPONSE_MSG_BUFFER_SIZE];
    LwU32                           rspMessageSize;
    PSPDM_MESSAGE_CAPABILITIES      pRspCapabilities;
    RC                              rc      = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = GET_CAPABILITIES_REQUEST_MSG_BUFFER_SIZE;
    pLwSpdmMsgHeader->flags     = 0;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_NORMAL;

    //
    // GET_CAPABILITIES request, provides capabilities supported
    // and requests Responder capabilities.
    //
    pReqGetCapabilities = (PSPDM_MESSAGE_GET_CAPABILITIES)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqGetCapabilities->GetCapabilities.SpdmVersionId       = SpdmVersionId_1_1;
    pReqGetCapabilities->GetCapabilities.RequestResponseCode = SpdmRequestIdGetCapabilities;
    pReqGetCapabilities->GetCapabilities.Reserved1           = 0;
    pReqGetCapabilities->GetCapabilities.Reserved2           = 0;
    pReqGetCapabilities->GetCapabilities.Reserved3           = 0;
    pReqGetCapabilities->GetCapabilities.CTExponent          = SPDM_EXPECTED_CT_EXPONENT;
    pReqGetCapabilities->GetCapabilities.Reserved4           = 0;
    pReqGetCapabilities->GetCapabilities.Reserved5           = 0;
    pReqGetCapabilities->GetCapabilities.Flags               = SPDM_EXPECTED_REQUESTER_CAPABILITIES;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize    = pLwSpdmMsgHeader->size;
    pRspCapabilities  = (PSPDM_MESSAGE_CAPABILITIES) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                     == expectedRspSize,                      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCapabilities->Capabilities.SpdmVersionId       == SpdmVersionId_1_1,                    RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCapabilities->Capabilities.RequestResponseCode == SpdmResponseIdCapabilities,           RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCapabilities->Capabilities.CTExponent          == SPDM_EXPECTED_CT_EXPONENT,            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCapabilities->Capabilities.Flags               == SPDM_EXPECTED_RESPONDER_CAPABILITIES, RC::LWRM_ERROR, Exit);

Exit:
    return rc;
}

#define SPDM_ALL_ASYM_ALGO                (SpdmBaseAsymAlgoTpmAlgRsaSsa2048       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaPss2048       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaSsa3072       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaPss3072       | \
                                           SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP256 | \
                                           SpdmBaseAsymAlgoTpmAlgRsaSsa4096       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaPss4096       | \
                                           SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP384 | \
                                           SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP521)

#define SPDM_ALL_HASH_ALGO                (SpdmBaseHashAlgoTpmAlgSha256   | \
                                           SpdmBaseHashAlgoTpmAlgSha384   | \
                                           SpdmBaseHashAlgoTpmAlgSha512   | \
                                           SpdmBaseHashAlgoTpmAlgSha3_256 | \
                                           SpdmBaseHashAlgoTpmAlgSha3_384 | \
                                           SpdmBaseHashAlgoTpmAlgSha3_512)

#define SPDM_ALL_DHE_ALGO                 (SpdmDheAlgoTypeFfdhe2048 | \
                                           SpdmDheAlgoTypeFfdhe3072 | \
                                           SpdmDheAlgoTypeFfdhe4096 | \
                                           SpdmDheAlgoTypeSecp256r1 | \
                                           SpdmDheAlgoTypeSecp384r1 | \
                                           SpdmDheAlgoTypeSecp521r1)

#define SPDM_ALL_AEAD_ALGO                (SpdmAeadAlgoAes128Gcm           | \
                                           SpdmAeadAlgoAes256Gcm           | \
                                           SpdmAeadAlgoChaCha20Poly1305    | \
                                           SpdmAeadAlgoAes128CtrHmacSha256)

// TODO: Class implementing tests should define these values.
#define SPDM_EXPECTED_NUM_ALG_TABLES      (0x04)
#define SPDM_EXPECTED_ALG_TABLE_ALG_COUNT (0x20)

/*!
 * ALGORITHMS positive test case. Ensures successful ALGORITHMS response from
 * valid NEGOTIATE_ALGORITHMS request, with expected supported algorithms selected.
 */
RC CCSpdmTest::SpdmNegotiateAlgorithmsTest()
{
    #define  NEGOTIATE_ALGORITHM_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_NEGOTIATE_ALGORITHMS) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  NEGOTIATE_ALGORITHM_EXPECTED_RESPONSE_MSG_SIZE sizeof(SPDM_MESSAGE_ALGORITHMS)
    #define  NEGOTIATE_ALGORITHM_RESPONSE_MSG_BUFFER_SIZE   NEGOTIATE_ALGORITHM_EXPECTED_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU32                               reqBufferSize = NEGOTIATE_ALGORITHM_REQUEST_MSG_BUFFER_SIZE;
    LwU8                                reqBuffer[NEGOTIATE_ALGORITHM_REQUEST_MSG_BUFFER_SIZE];
    PLW_SPDM_MESSAGE_HEADER             pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_MESSAGE_NEGOTIATE_ALGORITHMS  pReqNegotiateAlgorithms;
    LwU32                               expectedRspSize = NEGOTIATE_ALGORITHM_EXPECTED_RESPONSE_MSG_SIZE;
    LwU32                               rspBufferSize   = NEGOTIATE_ALGORITHM_RESPONSE_MSG_BUFFER_SIZE; // Need more 4 bytes to record return message size
    LwU8                                rspBuffer[NEGOTIATE_ALGORITHM_RESPONSE_MSG_BUFFER_SIZE];
    LwU32                               rspMessageSize;
    PSPDM_MESSAGE_ALGORITHMS            pRspAlgorithms;
    RC                                  rc = OK;

    memset(&reqBuffer, 0, reqBufferSize);
    memset(rspBuffer,  0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = NEGOTIATE_ALGORITHM_REQUEST_MSG_BUFFER_SIZE;
    pLwSpdmMsgHeader->flags     = 0;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_NORMAL;

    //
    // NEGOTIATE_ALGORITHMS request, send supported algorithms, Responder will choose for session.
    // Advertise support for all algorithms to ensure Responder selects expected ones.
    //
    pReqNegotiateAlgorithms = (PSPDM_MESSAGE_NEGOTIATE_ALGORITHMS)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqNegotiateAlgorithms->NegotiateAlgorithms.SpdmVersionId             = SpdmVersionId_1_1;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.RequestResponseCode       = SpdmRequestIdNegotiateAlgorithms;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.NumAlgTables              = SPDM_EXPECTED_NUM_ALG_TABLES;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.MessageLength             = sizeof(SPDM_MESSAGE_NEGOTIATE_ALGORITHMS);
    pReqNegotiateAlgorithms->NegotiateAlgorithms.MeasurementSpecification  = SpdmMeasurementSpecificationDmtf;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.BaseAsymAlgo              = SPDM_ALL_ASYM_ALGO;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.BaseHashAlgo              = SPDM_ALL_HASH_ALGO;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.ExtAsymCount              = 0;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.ExtHashCount              = 0;

    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[0].AlgType      = SpdmAlgTableTypeDhe;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[0].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[0].AlgSupported = SPDM_ALL_DHE_ALGO;

    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[1].AlgType      = SpdmAlgTableTypeAeadCipherSuite;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[1].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[1].AlgSupported = SPDM_ALL_AEAD_ALGO;

    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[2].AlgType      = SpdmAlgTableTypeReqBaseAsymAlg;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[2].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[2].AlgSupported = SPDM_ALL_ASYM_ALGO;

    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[3].AlgType      = SpdmAlgTableTypeKeySchedule;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[3].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    pReqNegotiateAlgorithms->NegotiateAlgorithms.AlgTables[3].AlgSupported = SpdmKeyScheduleAlgoSpdm;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize = pLwSpdmMsgHeader->size;
    pRspAlgorithms = (PSPDM_MESSAGE_ALGORITHMS)(rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                         == expectedRspSize,                        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.SpdmVersionId               == SpdmVersionId_1_1,                      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.RequestResponseCode         == SpdmResponseIdAlgorithms,               RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.NumAlgTables                == SPDM_EXPECTED_NUM_ALG_TABLES,           RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.MessageLength               == sizeof(SPDM_MESSAGE_ALGORITHMS),        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.MeasurementSpecificationSel == SpdmMeasurementSpecificationDmtf,       RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.MeasurementHashAlgo         == SpdmMeasurementHashAlgoTpmAlgSha384,    RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.BaseAsymSel                 == SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP384, RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.BaseHashSel                 == SpdmBaseHashAlgoTpmAlgSha384,           RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.ExtAsymSelCount             == 0,                                      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.ExtHashSelCount             == 0,                                      RC::LWRM_ERROR, Exit);

    // TODO: Class implementing tests should define the expected algorithms.
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[0].AlgType        == SpdmAlgTableTypeDhe,               RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[0].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT, RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[0].AlgSupported   == SpdmDheAlgoTypeSecp384r1,          RC::LWRM_ERROR, Exit);

    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[1].AlgType        == SpdmAlgTableTypeAeadCipherSuite,   RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[1].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT, RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[1].AlgSupported   == SpdmAeadAlgoAes256Gcm,             RC::LWRM_ERROR, Exit);

    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[2].AlgType        == SpdmAlgTableTypeReqBaseAsymAlg,         RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[2].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT,      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[2].AlgSupported   == SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP384, RC::LWRM_ERROR, Exit);

    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[3].AlgType        == SpdmAlgTableTypeKeySchedule,       RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[3].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT, RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspAlgorithms->Algorithms.AlgTables[3].AlgSupported   == SpdmKeyScheduleAlgoSpdm,           RC::LWRM_ERROR, Exit);

Exit:
    return rc;
}

//
// TODO: Class implementing tests should define these values.
// Lwrrently, we don't implement SHA-384 for digest of the
// "certificate" blob, so just set expected digest to zero.
// Need to check rmt_spdmresponder_certificate.cpp for the detail.
//
#define SPDM_EXPECTED_SLOT_MASK                 (0x01)
static LwU8 g_expectedDigest[48] = {0x00};

/*!
 * DIGESTS positive test case. Ensures successful DIGESTS response from
 * valid GET_DIGESTS request, with expected certificate digests returned.
 */
RC CCSpdmTest::SpdmGetDigestsTest()
{
    #define  GET_DIGESTS_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_GET_DIGESTS) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  GET_DIGESTS_EXPECTED_RESPONSE_MSG_SIZE sizeof(SPDM_MESSAGE_DIGESTS) + sizeof(g_expectedDigest)
    #define  GET_DIGESTS_RESPONSE_MSG_BUFFER_SIZE   GET_DIGESTS_EXPECTED_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                      reqBuffer[GET_DIGESTS_REQUEST_MSG_BUFFER_SIZE];
    LwU32                     reqBufferSize    = GET_DIGESTS_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER   pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_MESSAGE_GET_DIGESTS pReqGetDigests;
    LwU32                     expectedRspSize = GET_DIGESTS_EXPECTED_RESPONSE_MSG_SIZE;
    LwU32                     rspBufferSize   = GET_DIGESTS_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                      rspBuffer[GET_DIGESTS_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_DIGESTS     pRspDigests     = (PSPDM_MESSAGE_DIGESTS)rspBuffer;
    RC                        rc              = OK;
    LwU32                     rspMessageSize;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = GET_DIGESTS_REQUEST_MSG_BUFFER_SIZE;
    pLwSpdmMsgHeader->flags     = 0;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_NORMAL;

    pReqGetDigests = (PSPDM_MESSAGE_GET_DIGESTS)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    // GET_DIGESTS request, get all cert chain digests from Responder.
    pReqGetDigests->GetDigests.SpdmVersionId       = SpdmVersionId_1_1;
    pReqGetDigests->GetDigests.RequestResponseCode = SpdmRequestIdGetDigests;
    pReqGetDigests->GetDigests.Reserved1           = 0;
    pReqGetDigests->GetDigests.Reserved2           = 0;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize = pLwSpdmMsgHeader->size;
    pRspDigests    = (PSPDM_MESSAGE_DIGESTS) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                           == expectedRspSize,         RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspDigests->Digests.SpdmVersionId       == SpdmVersionId_1_1,       RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspDigests->Digests.RequestResponseCode == SpdmResponseIdDigests,   RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspDigests->Digests.SlotMask            == SPDM_EXPECTED_SLOT_MASK, RC::LWRM_ERROR, Exit);

/*  TODO: Pending by GSP-SPDM SHA library ready
    for (LwU32 i = 0; i < sizeof(g_expectedDigest); i++)
    {
        CHECK_AND_RETURN_IF_ERROR(pRspDigests->Digests.Digests[i] == g_expectedDigest[i], RC::LWRM_ERROR, Exit);
    }
*/

Exit:
    return rc;
}

// TODO:  Class implementing these tests should define these values.
#define SPDM_EXPECTED_SLOT_NUMBER 0
#define SPDM_EXPECTED_CERT_CHAIN_VALUE_AT_OFFSET(x) ((LwU8)(x % 256))
#define SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES      (0x400)
RC CCSpdmTest::SpdmGetCertificateTest()
{
    #define  GET_CERTIFICATE_REQUEST_MSG_BUFFER_SIZE     sizeof(SPDM_MESSAGE_GET_CERTIFICATE) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  GET_CERTIFICATE_EXPECTED_RESPONSE_MSG_SIZE sizeof(SPDM_MESSAGE_CERTIFICATE) + SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES
    #define  GET_CERTIFICATE_RESPONSE_MSG_BUFFER_SIZE   GET_CERTIFICATE_EXPECTED_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                          reqBuffer[GET_CERTIFICATE_REQUEST_MSG_BUFFER_SIZE];
    LwU32                         reqBufferSize    = GET_CERTIFICATE_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER       pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_MESSAGE_GET_CERTIFICATE pReqGetCertificate;
    LwU32                         expectedRspSize = GET_CERTIFICATE_EXPECTED_RESPONSE_MSG_SIZE;
    LwU32                         rspBufferSize   = GET_CERTIFICATE_RESPONSE_MSG_BUFFER_SIZE;
    // Use buffer for message, as we need space for certificate chain response.
    LwU8                          rspBuffer[GET_CERTIFICATE_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_CERTIFICATE     pRspCertificate;
    LwU32                         rspMessageSize;
    RC                            rc              = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = GET_CERTIFICATE_REQUEST_MSG_BUFFER_SIZE;
    pLwSpdmMsgHeader->flags     = 0;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_NORMAL;

    // GET_CERTIFICATES request, requests entire certificate.
    pReqGetCertificate = (PSPDM_MESSAGE_GET_CERTIFICATE) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqGetCertificate->GetCertificate.SpdmVersionId       = SpdmVersionId_1_1;
    pReqGetCertificate->GetCertificate.RequestResponseCode = SpdmRequestIdGetCertificate;
    pReqGetCertificate->GetCertificate.SlotNumber          = SPDM_EXPECTED_SLOT_NUMBER;
    pReqGetCertificate->GetCertificate.Reserved1           = 0;
    pReqGetCertificate->GetCertificate.Offset              = 0;
    pReqGetCertificate->GetCertificate.Length              = 0xFFFF;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize   = pLwSpdmMsgHeader->size;
    pRspCertificate  = (PSPDM_MESSAGE_CERTIFICATE) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                   == expectedRspSize,                        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCertificate->Certificate.SpdmVersionId       == SpdmVersionId_1_1,                      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCertificate->Certificate.RequestResponseCode == SpdmResponseIdCertificate,              RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCertificate->Certificate.SlotNumber          == SPDM_EXPECTED_SLOT_NUMBER,              RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCertificate->Certificate.PortionLength       == SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES, RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspCertificate->Certificate.RemainderLength     == 0,                                      RC::LWRM_ERROR, Exit);

    for (int i = 0; i < SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES; i++)
    {
        CHECK_AND_RETURN_IF_ERROR(pRspCertificate->Certificate.CertChain[i] == SPDM_EXPECTED_CERT_CHAIN_VALUE_AT_OFFSET(i), RC::LWRM_ERROR, Exit);
    }

Exit:
    return rc;
}
/*!
   vendor defined command test, can be issued in the rin non-selwred session or selwred session.
*/
RC CCSpdmTest::SpdmVendorDefinedRequestTest
(
   LwBool bSelwredSession
)
{
    #define  VENDOR_DEFINED_REQUEST_MSG_BUFFER_NONSELWRED_SIZE       LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + sizeof(SPDM_MESSAGE_VENDOR_DEFINED_REQUEST) + \
                                                                     SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE + SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE + \
                                                                     SPDM_VENDOR_DEFINED_REQ_LEN_FIELD_SIZE_IN_BYTE

    #define  VENDOR_DEFINED_REQUEST_MSG_BUFFER_SELWRED_SIZE          VENDOR_DEFINED_REQUEST_MSG_BUFFER_NONSELWRED_SIZE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE

    #define  VENDOR_DEFINED_EXPECTED_RESPONSE_MSG_SIZE               sizeof(SPDM_MESSAGE_VENDOR_DEFINED_RESPONSE) + SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE + \
                                                                     SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE + SPDM_VENDOR_DEFINED_RSP_LEN_FIELD_SIZE_IN_BYTE

    #define  VENDOR_DEFINED_RESPONSE_MSG_BUFFER_SIZE                 LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE + VENDOR_DEFINED_EXPECTED_RESPONSE_MSG_SIZE

    // Since selwred message size is always larger than non-selwred message size, so we select selwred mesage size as buffer size.
    LwU8                                   reqBuffer[VENDOR_DEFINED_REQUEST_MSG_BUFFER_SELWRED_SIZE];
    LwU32                                  reqBufferSize    = VENDOR_DEFINED_REQUEST_MSG_BUFFER_SELWRED_SIZE;
    PLW_SPDM_MESSAGE_HEADER                pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_SELWRED_MESSAGE_HEADER           pSpdmSelwredMsgHeader;
    PSPDM_MESSAGE_VENDOR_DEFINED_REQUEST   pReqVendorDefined;
    LwU32                                  expectedRspSize = VENDOR_DEFINED_EXPECTED_RESPONSE_MSG_SIZE;
    LwU32                                  rspBufferSize   = VENDOR_DEFINED_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                                   rspBuffer[VENDOR_DEFINED_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_VENDOR_DEFINED_RESPONSE  pRspVendorDefined;
    LwU32                                  rspMessageSize;
    LwU16                                  standardId;
    LwU8                                   *pBuf8;
    LwU16                                  *pBuf16;
    LwU8                                   vendorIdLen = 0;
    LwU16                                  rspPayloadLen;
    RC                                     rc = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;

    if (bSelwredSession)
    {
        pLwSpdmMsgHeader->size = VENDOR_DEFINED_REQUEST_MSG_BUFFER_SELWRED_SIZE;
    }
    else
    {
        pLwSpdmMsgHeader->size = VENDOR_DEFINED_REQUEST_MSG_BUFFER_NONSELWRED_SIZE;
    }

    pLwSpdmMsgHeader->flags     = 0;

    if (bSelwredSession)
    {
        pLwSpdmMsgHeader->msgType = LW_SPDM_MESSAGE_TYPE_SELWRED;
        pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
        pSpdmSelwredMsgHeader->sessionId = sessionId;
        pReqVendorDefined = (PSPDM_MESSAGE_VENDOR_DEFINED_REQUEST) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
        reqBufferSize = VENDOR_DEFINED_REQUEST_MSG_BUFFER_SELWRED_SIZE;
    }
    else
    {
        pLwSpdmMsgHeader->msgType = LW_SPDM_MESSAGE_TYPE_NORMAL;
        pReqVendorDefined = (PSPDM_MESSAGE_VENDOR_DEFINED_REQUEST) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
        reqBufferSize = VENDOR_DEFINED_REQUEST_MSG_BUFFER_NONSELWRED_SIZE;
    }
    pReqVendorDefined->spdmVersionId              = SpdmVersionId_1_1;
    pReqVendorDefined->requestResponseCode        = SpdmRequestIdVendorDefinedRequest;
    pReqVendorDefined->reserved1                  = 0;
    pReqVendorDefined->reserved2                  = 0;

    pBuf16 = (LwU16 *)((LwU8 *)pReqVendorDefined + sizeof(SPDM_MESSAGE_VENDOR_DEFINED_REQUEST));
    *pBuf16 = SPDM_REGISTRY_ID_PCISIG;

    // Set vendor id length to zero
    pBuf8  = (LwU8 *)((LwU8 *)pBuf16 + SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE);
    *pBuf8 = vendorIdLen;
    //
    // TODO: eddichang Need to finalize vendor reuqets command in https://confluence.lwpu.com/display/SC/Vendor+defined+message+use-cases
    // just select anyone command to test
    //
    // Set request payload size to zero
    pBuf8 += SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE;
    pBuf16 = (LwU16 *)pBuf8;
    *pBuf16 = 0;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    if (bSelwredSession)
    {
        if (pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_SELWRED)
        {
            CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
        }

        pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(pLwSpdmMsgHeader + 1);

        if (pSpdmSelwredMsgHeader->sessionId != sessionId)
        {
            CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
        }

        rspMessageSize     = pLwSpdmMsgHeader->size - SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
        pRspVendorDefined  = (PSPDM_MESSAGE_VENDOR_DEFINED_RESPONSE) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
    }
    else
    {
        if (pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
        {
           CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
        }

        rspMessageSize     = pLwSpdmMsgHeader->size;
        pRspVendorDefined  = (PSPDM_MESSAGE_VENDOR_DEFINED_RESPONSE) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    }
  

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                         == expectedRspSize,                        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspVendorDefined->spdmVersionId       == SpdmVersionId_1_1,                      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspVendorDefined->requestResponseCode == SpdmResponseIdVendorDefinedResponse,    RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspVendorDefined->reserved1           == 0,                                      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspVendorDefined->reserved2           == 0,                                      RC::LWRM_ERROR, Exit);

    pBuf16 = (LwU16 *)((LwU8 *)pRspVendorDefined + sizeof(SPDM_MESSAGE_VENDOR_DEFINED_REQUEST));
    standardId = *pBuf16;
    CHECK_AND_RETURN_IF_ERROR(standardId == SPDM_REGISTRY_ID_PCISIG, RC::LWRM_ERROR, Exit);

    pBuf8 = (LwU8 *)((LwU8 *)pBuf16 + SPDM_VENDOR_DEFINED_STANDARD_ID_SIZE_IN_BYTE);
    vendorIdLen = *pBuf8;
    CHECK_AND_RETURN_IF_ERROR(vendorIdLen == 0, RC::LWRM_ERROR, Exit);

    pBuf16 = (LwU16 *)(pBuf8 + SPDM_VENDOR_DEFINED_LEN_FIELD_SIZE_IN_BYTE);
    rspPayloadLen = *pBuf16;
    CHECK_AND_RETURN_IF_ERROR(rspPayloadLen == 0, RC::LWRM_ERROR, Exit);

Exit:
    return rc;
}

/*!
 * Session initialization positive test case.
 * Ensures successful secure session initialization from valid
 * KEY_EXCHANGE, KEY_EXCHANGE_RSP, FINISH, and FINISH_RSP sequence.
 */
RC CCSpdmTest::SpdmKeyExchangeTest()
{
    #define  KEY_EXCHANGE_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_KEY_EXCHANGE) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  KEY_EXCHANGE_RESPONSE_MSG_SIZE          sizeof(SPDM_MESSAGE_KEY_EXCHANGE_RESPONSE)
    #define  KEY_EXCHANGE_RESPONSE_MSG_BUFFER_SIZE   KEY_EXCHANGE_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                                                   reqBuffer[KEY_EXCHANGE_REQUEST_MSG_BUFFER_SIZE];
    LwU32                                                  reqBufferSize = KEY_EXCHANGE_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER                                pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_MESSAGE_KEY_EXCHANGE                             pReqKeyExchange;
    LwU32                                                  expectedRspSize = KEY_EXCHANGE_RESPONSE_MSG_SIZE;
    LwU32                                                  rspBufferSize   = KEY_EXCHANGE_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                                                   rspBuffer[KEY_EXCHANGE_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_KEY_EXCHANGE_RESPONSE                    pRspKeyExchange;
    PSPDM_SELWRED_MESSAGE_GENERAL_OPAQUE_DATA_TABLE_HEADER pGerneralOpaqueTableHdr;
    PSPDM_OPAQUE_ELEMENT_TABLE_HEADER                      pOpaqueElementTableHdr;
    PSPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_SUPPORTED_VERSION pSmOpaqueElementSupportedVer;
    PSPDM_VERSION_NUMBER                                   pSpdmVersionNum;
    LwU32                         rspMessageSize;
    LwU32                         i;
    LwU8                          *pByte;
    LwU16                         reqSessionId, rspSessionId;
    RC                            rc = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    reqSessionId = TEST_REQ_SESSION_ID;

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = KEY_EXCHANGE_REQUEST_MSG_BUFFER_SIZE;
    pLwSpdmMsgHeader->flags     = 0;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_NORMAL;


    pReqKeyExchange = (PSPDM_MESSAGE_KEY_EXCHANGE)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqKeyExchange->KeyExchange.SpdmVersionId       = SpdmVersionId_1_1;
    pReqKeyExchange->KeyExchange.RequestResponseCode = SpdmRequestIdKeyExchange;
    pReqKeyExchange->KeyExchange.HashType            = SpdmChallengeRequestHashAlgoNoMeasurementSummaryHash;
    pReqKeyExchange->KeyExchange.SlotNumber          = SPDM_EXPECTED_SLOT_NUMBER;
    pReqKeyExchange->KeyExchange.ReqSessionID        = reqSessionId;

    for ( i =0 ; i < SPDM_KEY_EXCHANGE_RANDOM_DATA_SIZE_IN_BYTE; i++)
    {
        pReqKeyExchange->KeyExchange.RandomData[i] = (LwU8)SPDM_KEY_EXCHANGE_RANDOM_DATA(i);
    }

    pReqKeyExchange->KeyExchange.OpaqueDataLength = SPDM_REQUEST_KEY_EXCHANGE_OPAQUE_DATA_SIZE_ALIGNED_IN_BYTE;

    pGerneralOpaqueTableHdr                = (PSPDM_SELWRED_MESSAGE_GENERAL_OPAQUE_DATA_TABLE_HEADER)pReqKeyExchange->KeyExchange.OpaqueData;
    pGerneralOpaqueTableHdr->SpecId        = SELWRED_MESSAGE_OPAQUE_DATA_SPEC_ID;
    pGerneralOpaqueTableHdr->OpaqueVersion = SELWRED_MESSAGE_OPAQUE_VERSION;
    pGerneralOpaqueTableHdr->TotalElements = 1;

    pByte = (LwU8 *)pGerneralOpaqueTableHdr;
    pOpaqueElementTableHdr                       = (PSPDM_OPAQUE_ELEMENT_TABLE_HEADER)(pByte + sizeof(SPDM_SELWRED_MESSAGE_GENERAL_OPAQUE_DATA_TABLE_HEADER));
    pOpaqueElementTableHdr->Id                   = SPDM_REGISTRY_ID_DMTF;
    pOpaqueElementTableHdr->VendorLen            = 0;
    pOpaqueElementTableHdr->OpaqueElementDataLen =  sizeof(SPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_SUPPORTED_VERSION) +
                                                    sizeof(SPDM_VERSION_NUMBER);

    pByte = (LwU8 *)pOpaqueElementTableHdr;
    pSmOpaqueElementSupportedVer = (PSPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_SUPPORTED_VERSION)(pByte + sizeof(SPDM_OPAQUE_ELEMENT_TABLE_HEADER));
    pSmOpaqueElementSupportedVer->SmDataVersion = SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION;
    pSmOpaqueElementSupportedVer->SmDataId      = SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_SUPPORTED_VERSION;
    pSmOpaqueElementSupportedVer->VersionCount  = 1;

    pByte = (LwU8 *)pSmOpaqueElementSupportedVer;
    pSpdmVersionNum = (PSPDM_VERSION_NUMBER)(pByte + sizeof(SPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_SUPPORTED_VERSION));
    pSpdmVersionNum->MajorVersion = 1;
    pSpdmVersionNum->MinorVersion = 1;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_NORMAL)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize   = pLwSpdmMsgHeader->size;
    pRspKeyExchange  = (PSPDM_MESSAGE_KEY_EXCHANGE_RESPONSE) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                   == expectedRspSize,              RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyExchange->KeyExchange.SpdmVersionId       == SpdmVersionId_1_1,            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyExchange->KeyExchange.RequestResponseCode == SpdmResponseIdKeyExchangeRsp, RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyExchange->KeyExchange.HeartbeatPeriod     == 0,                            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyExchange->KeyExchange.RspSessionID        != SPDM_SESSION_ID_ILWALID,      RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyExchange->KeyExchange.MutAuthRequested    == 0,                            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyExchange->KeyExchange.SlotIDParam         == SPDM_EXPECTED_SLOT_NUMBER,    RC::LWRM_ERROR, Exit);

    rspSessionId = pRspKeyExchange->KeyExchange.RspSessionID;
    sessionId = reqSessionId << 16 | rspSessionId;
Exit:
    return rc;
}

/*!
 * The end command for key_exchange positive test case.
 * Ensures successful secure session initialization from valid
 * KEY_EXCHANGE, KEY_EXCHANGE_RSP, FINISH, and FINISH_RSP sequence.
 */
RC CCSpdmTest::SpdmFinishTest()
{
    #define  FINISH_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_FINISH) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  FINISH_RESPONSE_MSG_SIZE          sizeof(SPDM_MESSAGE_FINISH_RESPONSE)
    #define  FINISH_RESPONSE_MSG_BUFFER_SIZE   FINISH_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                          reqBuffer[FINISH_REQUEST_MSG_BUFFER_SIZE];
    LwU32                         reqBufferSize    = FINISH_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER       pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_SELWRED_MESSAGE_HEADER  pSpdmSelwredMsgHeader;
    PSPDM_MESSAGE_FINISH          pReqFinish;
    LwU32                         expectedRspSize = FINISH_RESPONSE_MSG_SIZE;
    LwU32                         rspBufferSize   = FINISH_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                          rspBuffer[FINISH_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_FINISH_RESPONSE pRspFinish;
    LwU32                         rspMessageSize;
    LwU32                         i;
    RC                            rc = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = FINISH_REQUEST_MSG_BUFFER_SIZE;
    // TODO: eddichang, even selwred message, we still pass plian text to libspdm because RmTest lacks of cryption function support.
    pLwSpdmMsgHeader->flags     = LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_FALSE;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_SELWRED;

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    pSpdmSelwredMsgHeader->sessionId = sessionId;

    pReqFinish = (PSPDM_MESSAGE_FINISH) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqFinish->Finish.SpdmVersionId       = SpdmVersionId_1_1;
    pReqFinish->Finish.RequestResponseCode = SpdmRequestIdFinish;
    pReqFinish->Finish.SignatureIncluded   = 0;
    pReqFinish->Finish.SlotNumber          = SPDM_EXPECTED_SLOT_NUMBER;

    for (i = 0 ; i < SPDM_FINISH_REQUEST_VERIFY_DATA_SIZE_IN_BYTE; i++)
    {
        pReqFinish->Finish.RequesterVerifyData[i] = 0x5A;
    }

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_SELWRED)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(pLwSpdmMsgHeader + 1);

    if (pSpdmSelwredMsgHeader->sessionId != sessionId)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize = pLwSpdmMsgHeader->size - SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
    pRspFinish    = (PSPDM_MESSAGE_FINISH_RESPONSE) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                         == expectedRspSize,          RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspFinish->Finish.SpdmVersionId       == SpdmVersionId_1_1,        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspFinish->Finish.RequestResponseCode == SpdmResponseIdFinishRsp,  RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspFinish->Finish.Reserved1           == 0,                        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspFinish->Finish.Reserved2           == 0,                        RC::LWRM_ERROR, Exit);
Exit:
    return rc;
}

/*!
 * The test for get_measurements command.
 * Ensures successful secure session initialization from valid
 * KEY_EXCHANGE, KEY_EXCHANGE_RSP, FINISH, and FINISH_RSP sequence.
 * This test is only availabe in selwred session.
 */
RC CCSpdmTest::SpdmGetMeasurementsTest()
{
    #define  GET_MEASUREMENTS_REQUEST_MSG_BUFFER_SIZE    sizeof(SPDM_MESSAGE_GET_MEASUREMENTS) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  MEASUREMENTS_RESPONSE_MSG_SIZE              sizeof(SPDM_MESSAGE_MEASUREMENTS_RESPONSE)
    #define  MEASUREMENTS_RESPONSE_MSG_BUFFER_SIZE       MEASUREMENTS_RESPONSE_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                                   reqBuffer[GET_MEASUREMENTS_REQUEST_MSG_BUFFER_SIZE];
    LwU32                                  reqBufferSize    = GET_MEASUREMENTS_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER                pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_SELWRED_MESSAGE_HEADER           pSpdmSelwredMsgHeader;
    PSPDM_MESSAGE_GET_MEASUREMENTS         pReqGetMeasurements;
    LwU32                                  expectedRspSize = MEASUREMENTS_RESPONSE_MSG_SIZE;
    LwU32                                  rspBufferSize   = MEASUREMENTS_RESPONSE_MSG_BUFFER_SIZE;
    LwU8                                   rspBuffer[MEASUREMENTS_RESPONSE_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_MEASUREMENTS_RESPONSE    pRspMeasurements;
    LwU32                                  rspMessageSize;
    LwU32                                  i;
    RC                                     rc = OK;
    LwU8                                   *pByte = NULL;
    LwU32                                  measurmentsRecordLength = 0;
    PSPDM_MEASUREMENT_BLOCK_DMTF           pSpdmMeasurementBlkDmtf = NULL;
    PSPDM_MEASUREMENT_BLOCK_COMMON_HEADER  pSpdmMeasurementBlkCommonHdr = NULL;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = FINISH_REQUEST_MSG_BUFFER_SIZE;
    // TODO: eddichang, even selwred message, we still pass plian text to libspdm because RmTest lacks of cryption function support.
    pLwSpdmMsgHeader->flags     = LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_FALSE;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_SELWRED;

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    pSpdmSelwredMsgHeader->sessionId = sessionId;

    pReqGetMeasurements = (PSPDM_MESSAGE_GET_MEASUREMENTS) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqGetMeasurements->Measurements.SpdmVersionId        = SpdmVersionId_1_1;
    pReqGetMeasurements->Measurements.RequestResponseCode  = SpdmRequestIdGetMeasurements;
    pReqGetMeasurements->Measurements.Attributes           = SPDM_GET_MEASUREMENTS_ATTR_GENERATE_SIG_YES;
    pReqGetMeasurements->Measurements.MeasurementOperation = SPDM_GET_MEASUREMENTS_OP_REQUEST_ALL_BLOCKS;
    pReqGetMeasurements->Measurements.SlotNumber           = SPDM_EXPECTED_SLOT_NUMBER;

    for (i = 0 ; i < SPDM_MEASUREMENT_NONCE_SIZE_IN_BYTE; i++)
    {
        pReqGetMeasurements->Measurements.Nonce[i] = 0x5A; // Just fill random data.
    }

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_SELWRED)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(pLwSpdmMsgHeader + 1);

    if (pSpdmSelwredMsgHeader->sessionId != sessionId)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize    = pLwSpdmMsgHeader->size - SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
    pRspMeasurements  = (PSPDM_MESSAGE_MEASUREMENTS_RESPONSE) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                         == expectedRspSize,                               RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspMeasurements->Measurements.SpdmVersionId           == SpdmVersionId_1_1,                             RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspMeasurements->Measurements.RequestResponseCode     == SpdmResponseIdMeasurements,                    RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspMeasurements->Measurements.Param1                  == 0,                                             RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspMeasurements->Measurements.NumberOfBlocks          == EXPECTED_SPDM_MEASUREMENT_BLOCK_NUM,           RC::LWRM_ERROR, Exit);

    {
        // Because measurement record length just use 3 bytes, we need to move to LwU32 and then compare.
        memcpy((void *)&measurmentsRecordLength,
               (void *)pRspMeasurements->Measurements.MeasurementRecordLength,
               MEASUREMENTS_RECORD_LENGTH_SIZE_IN_BYTE);
    }

    CHECK_AND_RETURN_IF_ERROR(measurmentsRecordLength == EXPECTED_SPDM_MEASUREMENT_RECORD_SIZE_IN_BYTE, RC::LWRM_ERROR, Exit);

    pSpdmMeasurementBlkDmtf = (PSPDM_MEASUREMENT_BLOCK_DMTF) pRspMeasurements->Measurements.MeasurementRecord;

    // Check index of each common header, index should be stored as incremental value and starting from 1.
    for (i = 0; i < pRspMeasurements->Measurements.NumberOfBlocks; i++)
    {
        pSpdmMeasurementBlkCommonHdr = &pSpdmMeasurementBlkDmtf->measurementBlockCommonHeader;

        CHECK_AND_RETURN_IF_ERROR(pSpdmMeasurementBlkCommonHdr->index == (i+1), RC::LWRM_ERROR, Exit);

        pByte = (LwU8 *)pSpdmMeasurementBlkCommonHdr;

        // Move to next measurement block DMTF
        pByte += sizeof(SPDM_MEASUREMENT_BLOCK_DMTF) + SPDM_MEASUREMENT_HASH_SIZE_IN_BYTE;

        pSpdmMeasurementBlkDmtf = (PSPDM_MEASUREMENT_BLOCK_DMTF)pByte;
    }
Exit:
    return rc;
}

/*!
 * The test for end_session command.
 * Ensures successful secure session initialization from valid
 * KEY_EXCHANGE, KEY_EXCHANGE_RSP, FINISH, and FINISH_RSP sequence.
 * This test is used  to end selwred session.
 */
RC CCSpdmTest::SpdmEndSessionTest()
{
    #define  END_SESSION_REQUEST_MSG_BUFFER_SIZE      sizeof(SPDM_MESSAGE_END_SESSION) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  END_SESSION_ACK_MSG_SIZE                 sizeof(SPDM_MESSAGE_END_SESSION_ACK)
    #define  END_SESSION_ACK_MSG_BUFFER_SIZE          END_SESSION_ACK_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                                   reqBuffer[END_SESSION_REQUEST_MSG_BUFFER_SIZE];
    LwU32                                  reqBufferSize    = END_SESSION_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER                pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_SELWRED_MESSAGE_HEADER           pSpdmSelwredMsgHeader;
    PSPDM_MESSAGE_END_SESSION              pReqEndSession;
    LwU32                                  expectedRspSize = END_SESSION_ACK_MSG_SIZE;
    LwU32                                  rspBufferSize   = END_SESSION_ACK_MSG_BUFFER_SIZE;
    LwU8                                   rspBuffer[END_SESSION_ACK_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_END_SESSION_ACK          pRspEndSessionAck;
    LwU32                                  rspMessageSize;
    RC                                     rc = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = END_SESSION_REQUEST_MSG_BUFFER_SIZE;
    // TODO: eddichang, even selwred message, we still pass plian text to libspdm because RmTest lacks of cryption function support.
    pLwSpdmMsgHeader->flags     = LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_FALSE;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_SELWRED;

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    pSpdmSelwredMsgHeader->sessionId = sessionId;

    pReqEndSession = (PSPDM_MESSAGE_END_SESSION) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqEndSession->EndSession.SpdmVersionId        = SpdmVersionId_1_1;
    pReqEndSession->EndSession.RequestResponseCode  = SpdmRequestIdEndSession;
    pReqEndSession->EndSession.Attributes           = SPDM_END_SESSION_REQUEST_ATTR_NEGOTIATED_STATE_CACHE_CLEAR;
    pReqEndSession->EndSession.Reserved1            = 0;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed. \n", __FUNCTION__);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_SELWRED)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(pLwSpdmMsgHeader + 1);

    if (pSpdmSelwredMsgHeader->sessionId != sessionId)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize    = pLwSpdmMsgHeader->size - SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
    pRspEndSessionAck = (PSPDM_MESSAGE_END_SESSION_ACK) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                         == expectedRspSize,              RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspEndSessionAck->EndSessionAck.SpdmVersionId         == SpdmVersionId_1_1,            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspEndSessionAck->EndSessionAck.RequestResponseCode   == SpdmResponseIdEndSessionAck,  RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspEndSessionAck->EndSessionAck.Reserved1             == 0,                            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspEndSessionAck->EndSessionAck.Reserved2             == 0,                            RC::LWRM_ERROR, Exit);

Exit:
    return rc;
}

/*!
 * The assistant function for KEY_UPDATE.
 * Send KEY_UPDATE command and then velidate response message.
 */
RC CCSpdmTest::SpdmKeyUpdateSubTest
(
    LwU8 keyOp,
    LwU8 tag
)
{
    #define  KEY_UPDATE_REQUEST_MSG_BUFFER_SIZE       sizeof(SPDM_MESSAGE_KEY_UPDATE) + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE
    #define  KEY_UPDATE_ACK_MSG_SIZE                  sizeof(SPDM_MESSAGE_KEY_UPDATE_ACK)
    #define  KEY_UPDATE_ACK_MSG_BUFFER_SIZE           KEY_UPDATE_ACK_MSG_SIZE + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE

    LwU8                                   reqBuffer[END_SESSION_REQUEST_MSG_BUFFER_SIZE];
    LwU32                                  reqBufferSize    = END_SESSION_REQUEST_MSG_BUFFER_SIZE;
    PLW_SPDM_MESSAGE_HEADER                pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)reqBuffer;
    PSPDM_SELWRED_MESSAGE_HEADER           pSpdmSelwredMsgHeader;
    PSPDM_MESSAGE_KEY_UPDATE               pReqKeyUpdate;
    LwU32                                  expectedRspSize = KEY_UPDATE_ACK_MSG_SIZE;
    LwU32                                  rspBufferSize   = KEY_UPDATE_ACK_MSG_BUFFER_SIZE;
    LwU8                                   rspBuffer[KEY_UPDATE_ACK_MSG_BUFFER_SIZE];
    PSPDM_MESSAGE_KEY_UPDATE_ACK           pRspKeyUpdateAck;
    LwU32                                  rspMessageSize;
    RC                                     rc = OK;

    memset(reqBuffer, 0, reqBufferSize);
    memset(rspBuffer, 0, rspBufferSize);

    pLwSpdmMsgHeader->version   = LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT;
    pLwSpdmMsgHeader->size      = END_SESSION_REQUEST_MSG_BUFFER_SIZE;
    // TODO: eddichang, even selwred message, we still pass plian text to libspdm because RmTest lacks of cryption function support.
    pLwSpdmMsgHeader->flags     = LW_SPDM_MESSAGE_HEADER_FLAGS_ENCRYPTED_FALSE;
    pLwSpdmMsgHeader->msgType   = LW_SPDM_MESSAGE_TYPE_SELWRED;

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE);

    pSpdmSelwredMsgHeader->sessionId = sessionId;

    pReqKeyUpdate = (PSPDM_MESSAGE_KEY_UPDATE) (reqBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);
    pReqKeyUpdate->KeyUpdate.SpdmVersionId        = SpdmVersionId_1_1;
    pReqKeyUpdate->KeyUpdate.RequestResponseCode  = SpdmRequestIdKeyUpdate;
    pReqKeyUpdate->KeyUpdate.KeyOperation         = keyOp;
    pReqKeyUpdate->KeyUpdate.Tag                  = tag;

    rc = SpdmMessageProcess(reqBuffer, reqBufferSize, rspBuffer, rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "--------------------------------------------------\n");
        Printf(Tee::PriHigh, "SPDM, %s failed keyOp = 0x%x, tag = 0x%x. \n", __FUNCTION__, keyOp, tag);
        Printf(Tee::PriHigh, "--------------------------------------------------\n");

        return rc;
    }

    pLwSpdmMsgHeader = (PLW_SPDM_MESSAGE_HEADER)rspBuffer;

    if (pLwSpdmMsgHeader->version != LW_SPDM_MESSAGE_HEADER_VERSION_LWRRENT ||
        pLwSpdmMsgHeader->msgType != LW_SPDM_MESSAGE_TYPE_SELWRED)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    pSpdmSelwredMsgHeader = (PSPDM_SELWRED_MESSAGE_HEADER)(pLwSpdmMsgHeader + 1);

    if (pSpdmSelwredMsgHeader->sessionId != sessionId)
    {
        CHECK_AND_RETURN_IF_ERROR(LW_FALSE,  RC::LWRM_ERROR, Exit);
    }

    rspMessageSize   = pLwSpdmMsgHeader->size - SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE;
    pRspKeyUpdateAck = (PSPDM_MESSAGE_KEY_UPDATE_ACK) (rspBuffer + LW_SPDM_MESSAGE_HEADER_SIZE_IN_BYTE + SPDM_SELWRED_MESSAGE_HEADER_SIZE_IN_BYTE);

    CHECK_AND_RETURN_IF_ERROR(rspMessageSize                                       == expectedRspSize,              RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyUpdateAck->KeyUpdateAck.SpdmVersionId         == SpdmVersionId_1_1,            RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyUpdateAck->KeyUpdateAck.RequestResponseCode   == SpdmResponseIdKeyUpdateAck,   RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyUpdateAck->KeyUpdateAck.KeyOperation          == keyOp,                        RC::LWRM_ERROR, Exit);
    CHECK_AND_RETURN_IF_ERROR(pRspKeyUpdateAck->KeyUpdateAck.Tag                   == tag,                          RC::LWRM_ERROR, Exit);

Exit:
    return rc;
}

/*!
 * The function for KEY_UPDATE command test.
 * Send UPDTAE_ALL_KEY and VERIFY_NEW_KEY commands.
 */
RC CCSpdmTest::SpdmKeyUpdateTest()
{
    RC                                     rc = OK;
    LwU8                                   keyOp;
    LwU8                                   tag;

    // Phase-1: Update all key
    keyOp = SPDM_KEY_UPDATE_KEY_OP_UPDATE_ALL_KEYS;
    tag = 1;
    CHECK_RC(SpdmKeyUpdateSubTest(keyOp, tag));


    // Phase-2: Verify new key
    keyOp = SPDM_KEY_UPDATE_KEY_OP_VERIFY_NEW_KEY;
    tag = 2;
    CHECK_RC(SpdmKeyUpdateSubTest(keyOp, tag));

    return rc;
}

