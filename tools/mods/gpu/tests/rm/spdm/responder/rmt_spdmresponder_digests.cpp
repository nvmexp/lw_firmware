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
 * SpdmResponderTest: DIGESTS
 *
 * Tests DIGESTS response from GET_DIGESTS request.
 */

#include "rmt_spdmresponder.h"

//
// TODO: Class implementing tests should define these values.
// Lwrrently, the expectedDigest is the SHA-256 digest of the
// "certificate" blob, value defined in rmt_spdmresponder_certificate.cpp.
//
#define SPDM_EXPECTED_SLOT_MASK                 (0x01)
static LwU8 g_expectedDigest[32] = {
    0x78, 0x5B, 0x07, 0x51, 0xFC, 0x2C, 0x53, 0xDC,
    0x14, 0xA4, 0xCE, 0x3D, 0x80, 0x0E, 0x69, 0xEF,
    0x9C, 0xE1, 0x00, 0x9E, 0xB3, 0x27, 0xCC, 0xF4,
    0x58, 0xAF, 0xE0, 0x9C, 0x24, 0x2C, 0x26, 0xC9,
};

/*!
 * DIGESTS positive test case. Ensures successful DIGESTS response from
 * valid GET_DIGESTS request, with expected certificate digests returned.
 */
RC SpdmResponderTest::ExpectedDigests()
{
    SPDM_MESSAGE_GET_DIGESTS reqGetDigests;
    LwU32                    reqSize     = sizeof(reqGetDigests);

    // Use buffer for message, as we need space for digest.
    LwU8                     rspBuffer[sizeof(SPDM_MESSAGE_DIGESTS) +
                                       sizeof(g_expectedDigest)];
    PSPDM_MESSAGE_DIGESTS    pRspDigests = (PSPDM_MESSAGE_DIGESTS)rspBuffer;
    LwU32                    rspSize     = sizeof(rspBuffer);
    RC                       rc          = OK;

    memset(&reqGetDigests, 0, reqSize);
    memset(pRspDigests,    0, rspSize);

    // GET_DIGESTS request, get all cert chain digests from Responder.
    reqGetDigests.GetDigests.SpdmVersionId       = SpdmVersionId_1_1;
    reqGetDigests.GetDigests.RequestResponseCode = SpdmRequestIdGetDigests;
    reqGetDigests.GetDigests.Reserved1           = 0;
    reqGetDigests.GetDigests.Reserved2           = 0;

    CHECK_RC(SendSpdmRequest((LwU8 *)&reqGetDigests, reqSize, (LwU8 *)pRspDigests, &rspSize));

    MASSERT(rspSize                                  == (sizeof(SPDM_MESSAGE_DIGESTS) +
                                                         sizeof(g_expectedDigest)));
    MASSERT(pRspDigests->Digests.SpdmVersionId       == SpdmVersionId_1_1);
    MASSERT(pRspDigests->Digests.RequestResponseCode == SpdmResponseIdDigests);
    MASSERT(pRspDigests->Digests.SlotMask            == SPDM_EXPECTED_SLOT_MASK);

    for (LwU32 i = 0; i < sizeof(g_expectedDigest); i++)
    {
        MASSERT(pRspDigests->Digests.Digests[i] == g_expectedDigest[i]);
    }

    return rc;
}

/*!
 * Run all SPDM Responder DIGESTS message tests.
 */
RC SpdmResponderTest::RunDigestsTests()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "--------------------------------------------------\n");
    Printf(Tee::PriHigh, "SpdmResponderTest: Running DIGESTS tests.\n");
    Printf(Tee::PriHigh, "--------------------------------------------------\n");

    // Initialize positive test case separately, as test case used in SpdmSetState().
    CHECK_RC(SpdmSetState(SpdmRequestIdGetDigests));
    CHECK_RC(ExpectedDigests());

    return rc;
}
