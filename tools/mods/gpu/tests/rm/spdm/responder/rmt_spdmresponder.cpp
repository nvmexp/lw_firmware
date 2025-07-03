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

//
// Implementations of common SpdmResponderTest functionality.
//

#include "rmt_spdmresponder.h"

/*
 * Print SPDM payload message.
 */
RC SpdmResponderTest::DumpSpdmMessage(LwU8 *message, LwU32 size)
{
    if (message == nullptr || size == 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    for (LwU32 i = 0; i < size; i++)
    {
        // Print max 16 bytes in one row.
        if ((i != 0) && ((i % 16) == 0))
        {
            Printf(Tee::PriDebug, "\n");
        }

        Printf(Tee::PriDebug, "0x%02X ", *(message + i));
    }

    Printf(Tee::PriDebug, "\n");

    return OK;
}

/*
 * Put Responder in state to expect "nextRequest" next. Do so by running
 * positive unit tests for all messages prior to it in SPDM sequence.
 */
RC SpdmResponderTest::SpdmSetState(SpdmRequestId nextRequest)
{
    RC rc = OK;

    // No need to reset state, as sending GET_VERSION should reset Responder.
    if (nextRequest == SpdmRequestIdGetVersion)
    {
        return OK;
    }
    CHECK_RC(ExpectedVersion());

    if (nextRequest == SpdmRequestIdGetCapabilities)
    {
        return OK;
    }
    CHECK_RC(ExpectedCapabilities());

    if (nextRequest == SpdmRequestIdNegotiateAlgorithms)
    {
        return OK;
    }
    CHECK_RC(ExpectedAlgorithms());

    if (nextRequest == SpdmRequestIdGetDigests)
    {
        return OK;
    }
    CHECK_RC(ExpectedDigests());

    if (nextRequest == SpdmRequestIdGetCertificate)
    {
        return OK;
    }
    CHECK_RC(ExpectedCertificate());

    if (nextRequest == SpdmRequestIdKeyExchange)
    {
        return OK;
    }
    CHECK_RC(ExpectedKeyExchangeFinish());

    return rc;
}

/*
 * Run all SPDM Responder tests.
 */
RC SpdmResponderTest::RunAllTests()
{
    RC rc = OK;

    //
    // TODO: Supported tests should be chosen by
    // capabilities set by derived class.
    //

    CHECK_RC(RulwersionTests());
    CHECK_RC(RunCapabilitiesTests());
    CHECK_RC(RunAlgorithmsTests());
    CHECK_RC(RunDigestsTests());
    CHECK_RC(RunCertificateTests());
    CHECK_RC(RunKeyExchangeFinishTests());

    return rc;
}
