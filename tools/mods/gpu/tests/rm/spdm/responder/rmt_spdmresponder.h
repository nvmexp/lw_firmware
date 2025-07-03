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

#ifndef RMT_SPDMRESPONDER_H
#define RMT_SPDMRESPONDER_H

#include "lwos.h"
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"
#include "gpu/tests/rm/spdm/rmt_spdmdefines.h"

//
// Class interface definition for SPDM Responder unit tests.
// Meant to be used as a base class for actual test implementation.
//
class SpdmResponderTest: public RmTest
{
public:
    /*
     * @brief  This function must be provided by a class implementing
     *         SpdmResponderTest.
     * 
     *         Transport layer to send SPDM requests to Responder, and receive
     *         the SPDM responses.
     *
     * @param[in]      pPayloadIn       Buffer containing SPDM request payload.
     * @param[in]      payloadInSize    Size of pPayloadIn buffer, in bytes.
     * @param[out]     pPayloadOut      Buffer where SPDM response will be placed.
     * @param[in, out] pPayloadOutSize  On input, max size of pPayloadOut in bytes.
     *                                  On return, size of the received payload.
     *
     * @return OK, if the SPDM request was sent and response received successfully.
     */
    virtual RC SendSpdmRequest(LwU8 *pPayloadIn, LwU32 payloadInSize,
                               LwU8 *pPayloadOut, LwU32 *pPayloadOutSize) = 0;

    virtual RC DumpSpdmMessage(LwU8 *message, LwU32 size);
    virtual RC SpdmSetState(SpdmRequestId);

    virtual RC RulwersionTests();
    virtual RC RunCapabilitiesTests();
    virtual RC RunAlgorithmsTests();
    virtual RC RunDigestsTests();
    virtual RC RunCertificateTests();
    virtual RC RunKeyExchangeFinishTests();

    virtual RC RunAllTests();

private:

    // All GET_VERSION/VERSION tests
    RC ExpectedVersion();

    // All GET_CAPABILITIES/CAPABILITIES tests
    RC ExpectedCapabilities();

    // All NEGOTIATE_ALGORITHMS/ALGORITHMS tests
    RC ExpectedAlgorithms();

    // All GET_DIGESTS/DIGESTS tests
    RC ExpectedDigests();

    // All GET_CERTIFICATE/CERTIFICATE tests
    RC ExpectedCertificate();

    // All KEY_EXCHANGE/KEY_EXCHANGE_RSP & FINISH/FINISH_RSP tests
    RC ExpectedKeyExchangeFinish();
};

#endif // RMT_SPDMRESPONDER_H
