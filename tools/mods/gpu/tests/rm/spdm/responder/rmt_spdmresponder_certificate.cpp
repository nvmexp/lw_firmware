/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*!
 * SpdmResponderTest: CERTIFICATE
 *
 * Tests CERTIFICATE response from GET_CERTIFICATE request.
 */

#include "rmt_spdmresponder.h"

// TODO:  Class implementing these tests should define these values.
#define SPDM_EXPECTED_SLOT_NUMBER 0
#define SPDM_EXPECTED_CERT_CHAIN_VALUE_AT_OFFSET(x) ((LwU8)(x % 256))
#define SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES      (0x400)

/*!
 * CERTIFICATE positive test case. Ensures successful CERTIFICATE response
 * from valid GET_CERTIFICATE request, with expected certificates returned.
 */
RC SpdmResponderTest::ExpectedCertificate()
{
    SPDM_MESSAGE_GET_CERTIFICATE reqGetCertificate;
    LwU32                        reqSize         = sizeof(reqGetCertificate);

    // Use buffer for message, as we need space for certificate chain response.
    LwU8                         rspBuffer[sizeof(SPDM_MESSAGE_CERTIFICATE) +
                                           SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES];
    PSPDM_MESSAGE_CERTIFICATE    pRspCertificate = (PSPDM_MESSAGE_CERTIFICATE)rspBuffer;
    LwU32                        rspSize         = sizeof(rspBuffer);
    RC                           rc              = OK;

    memset(&reqGetCertificate, 0, reqSize);
    memset(pRspCertificate,    0, rspSize);

    // GET_CERTIFICATES request, requests entire certificate.
    reqGetCertificate.GetCertificate.SpdmVersionId       = SpdmVersionId_1_1;
    reqGetCertificate.GetCertificate.RequestResponseCode = SpdmRequestIdGetCertificate;
    reqGetCertificate.GetCertificate.SlotNumber          = SPDM_EXPECTED_SLOT_NUMBER;
    reqGetCertificate.GetCertificate.Reserved1           = 0;
    reqGetCertificate.GetCertificate.Offset              = 0;
    reqGetCertificate.GetCertificate.Length              = 0xFFFF;

    CHECK_RC(SendSpdmRequest((LwU8 *)&reqGetCertificate, reqSize, (LwU8 *)pRspCertificate, &rspSize));

    MASSERT(rspSize                                          == (sizeof(SPDM_MESSAGE_CERTIFICATE) +
                                                                 SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES));
    MASSERT(pRspCertificate->Certificate.SpdmVersionId       == SpdmVersionId_1_1);
    MASSERT(pRspCertificate->Certificate.RequestResponseCode == SpdmResponseIdCertificate);
    MASSERT(pRspCertificate->Certificate.SlotNumber          == SPDM_EXPECTED_SLOT_NUMBER);
    MASSERT(pRspCertificate->Certificate.PortionLength       == SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES);
    MASSERT(pRspCertificate->Certificate.RemainderLength     == 0);

    for (int i = 0; i < SPDM_EXPECTED_CERT_CHAIN_SIZE_IN_BYTES; i++)
    {
        MASSERT(pRspCertificate->Certificate.CertChain[i] == SPDM_EXPECTED_CERT_CHAIN_VALUE_AT_OFFSET(i));
    }

    return rc;
}

/*!
 * Run all SPDM Responder CERTIFICATE message tests.
 */
RC SpdmResponderTest::RunCertificateTests()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "--------------------------------------------------\n");
    Printf(Tee::PriHigh, "SpdmResponderTest: Running CERTIFICATE tests.\n");
    Printf(Tee::PriHigh, "--------------------------------------------------\n");

    // Initialize positive test case separately, as test case used in SpdmSetState().
    CHECK_RC(SpdmSetState(SpdmRequestIdGetCertificate));
    CHECK_RC(ExpectedCertificate());

    return rc;
}
