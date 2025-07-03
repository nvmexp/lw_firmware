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
 * SpdmResponderTest: KEY_EXCHANGE_RSP & FINISH_RSP
 *
 * Tests session establishment via handshake completed in KEY_EXCHANGE,
 * KEY_EXCHANGE_RSP, FINISH, and FINISH_RSP messages.
 */

#include "rmt_spdmresponder.h"

/*!
 * Session initialization positive test case.
 * Ensures successful secure session initialization from valid
 * KEY_EXCHANGE, KEY_EXCHANGE_RSP, FINISH, and FINISH_RSP sequence.
 */
RC SpdmResponderTest::ExpectedKeyExchangeFinish()
{
    RC rc = OK;

    // TODO: Not implemented.

    return rc;
}

/*!
 * Run all SPDM Responder KEY_EXCHANGE_RSP and FINISH_RSP message tests.
 */
RC SpdmResponderTest::RunKeyExchangeFinishTests()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "--------------------------------------------------\n");
    Printf(Tee::PriHigh, "SpdmResponderTest: Running KEY_EXCHANGE & FINISH tests.\n");
    Printf(Tee::PriHigh, "--------------------------------------------------\n");

    // Initialize positive test case separately, as test case used in SpdmSetState().
    CHECK_RC(SpdmSetState(SpdmRequestIdKeyExchange));
    CHECK_RC(ExpectedKeyExchangeFinish());

    return rc;
}
