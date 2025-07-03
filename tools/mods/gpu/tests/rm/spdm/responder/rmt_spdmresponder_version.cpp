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
 * SpdmResponderTest: VERSION
 *
 * Tests VERSION response from GET_VERSION request.
 */

#include "rmt_spdmresponder.h"

/*!
 * VERSION positive test case. Ensures successful response
 * of VERSION from valid GET_VERSION request.
 */
RC SpdmResponderTest::ExpectedVersion()
{
    SPDM_MESSAGE_GET_VERSION reqGetVersion;
    SPDM_MESSAGE_VERSION     rspVersion;
    LwU32                    reqSize = sizeof(reqGetVersion);
    LwU32                    rspSize = sizeof(rspVersion);
    RC                       rc      = OK;

    memset(&reqGetVersion, 0, reqSize);
    memset(&rspVersion,    0, rspSize);

    // GET_VERSION request, requests list of supported versions from Responder.
    reqGetVersion.GetVersion.SpdmVersionId       = SpdmVersionId_1_0;
    reqGetVersion.GetVersion.RequestResponseCode = SpdmRequestIdGetVersion;
    reqGetVersion.GetVersion.Reserved1           = 0;
    reqGetVersion.GetVersion.Reserved2           = 0;

    CHECK_RC(SendSpdmRequest((LwU8 *)&reqGetVersion, reqSize, (LwU8 *)&rspVersion, &rspSize));

    MASSERT(rspSize                                == sizeof(SPDM_MESSAGE_VERSION));
    MASSERT(rspVersion.Version.SpdmVersionId       == SpdmVersionId_1_0);
    MASSERT(rspVersion.Version.RequestResponseCode == SpdmResponseIdVersion);

    // TODO: Do we want to validate anything else?

    return rc;
}

/*!
 * Run all SPDM Responder VERSION message tests.
 */
RC SpdmResponderTest::RulwersionTests()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "--------------------------------------------------\n");
    Printf(Tee::PriHigh, "SpdmResponderTest: Running VERSION tests.\n");
    Printf(Tee::PriHigh, "--------------------------------------------------\n");

    // Initialize positive test case separately, as test case used in SpdmSetState().
    CHECK_RC(SpdmSetState(SpdmRequestIdGetVersion));
    CHECK_RC(ExpectedVersion());

    return rc;
}
