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
 * SpdmResponderTest: CAPABILITIES
 *
 * Tests CAPABILITIES response from GET_CAPABILITIES request.
 */

#include "rmt_spdmresponder.h"

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
                                              SpdmCapabilityMeasFreshCap)

/*!
 * CAPABILITIES positive test case. Ensures successful response
 * of CAPABILITIES from valid GET_CAPABILITIES request.
 */
RC SpdmResponderTest::ExpectedCapabilities()
{
    SPDM_MESSAGE_GET_CAPABILITIES reqGetCapabilities;
    SPDM_MESSAGE_CAPABILITIES     rspCapabilities;
    LwU32                         reqSize = sizeof(reqGetCapabilities);
    LwU32                         rspSize = sizeof(rspCapabilities);
    RC                            rc      = OK;

    memset(&reqGetCapabilities, 0, reqSize);
    memset(&rspCapabilities,    0, rspSize);

    //
    // GET_CAPABILITIES request, provides capabilities supported
    // and requests Responder capabilities.
    //
    reqGetCapabilities.GetCapabilities.SpdmVersionId       = SpdmVersionId_1_1;
    reqGetCapabilities.GetCapabilities.RequestResponseCode = SpdmRequestIdGetCapabilities;
    reqGetCapabilities.GetCapabilities.Reserved1           = 0;
    reqGetCapabilities.GetCapabilities.Reserved2           = 0;
    reqGetCapabilities.GetCapabilities.Reserved3           = 0;
    reqGetCapabilities.GetCapabilities.CTExponent          = SPDM_EXPECTED_CT_EXPONENT;
    reqGetCapabilities.GetCapabilities.Reserved4           = 0;
    reqGetCapabilities.GetCapabilities.Reserved5           = 0;
    reqGetCapabilities.GetCapabilities.Flags               = SPDM_EXPECTED_REQUESTER_CAPABILITIES;

    CHECK_RC(SendSpdmRequest((LwU8 *)&reqGetCapabilities, reqSize,
                             (LwU8 *)&rspCapabilities, &rspSize));

    MASSERT(rspSize                                          == sizeof(SPDM_MESSAGE_CAPABILITIES));
    MASSERT(rspCapabilities.Capabilities.SpdmVersionId       == SpdmVersionId_1_1);
    MASSERT(rspCapabilities.Capabilities.RequestResponseCode == SpdmResponseIdCapabilities);
    MASSERT(rspCapabilities.Capabilities.CTExponent          == SPDM_EXPECTED_CT_EXPONENT);
    MASSERT(rspCapabilities.Capabilities.Flags               == SPDM_EXPECTED_RESPONDER_CAPABILITIES);
    return rc;
}

/*!
 * Run all SPDM Responder CERTIFICATE message tests.
 */
RC SpdmResponderTest::RunCapabilitiesTests()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "--------------------------------------------------\n");
    Printf(Tee::PriHigh, "SpdmResponderTest: Running CAPABILITIES tests.\n");
    Printf(Tee::PriHigh, "--------------------------------------------------\n");

    // Initialize positive test case separately, as test case used in SpdmSetState().
    CHECK_RC(SpdmSetState(SpdmRequestIdGetCapabilities));
    CHECK_RC(ExpectedCapabilities());

    return rc;
}
