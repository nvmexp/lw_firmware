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
//! \file rmt_ccspdm.h
//! \brief Sanity test for Confidential Compute.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "gpu/tests/rm/spdm/rmt_spdmdefines.h"
#include "rmlsfm.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"
#include "rmgspcmdif.h"

#define  TEST_GUEST_ID         (0xFFFFFF00)
#define  TEST_REQ_SESSION_ID   (0xFFFE)
#define  TEST_SEQUENCE_NUM     (0xFFFFFF00)
#define  SPDM_SURFACE_SIZE     (0x800)

class CCSpdmTest : public RmTest
{
public:
    UINT32  guestId;
    UINT32  endpointId;
    UINT32  sessionId;

    CCSpdmTest();
    virtual ~CCSpdmTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC SpdmInit();
    virtual RC SpdmDeinit();
    virtual RC SpdmMessageProcess(UINT08 *pMsgBufferReq, UINT32 reqMsgSize,
                                  UINT08 *pMsgBufferRsp, UINT32 rspBufferSize);

    virtual RC SpdmGetVersionTest();
    virtual RC SpdmGetCapabilitiesTest();
    virtual RC SpdmNegotiateAlgorithmsTest();
    virtual RC SpdmGetDigestsTest();
    virtual RC SpdmGetCertificateTest();
    virtual RC SpdmKeyExchangeTest();
    virtual RC SpdmKeyUpdateTest();
    virtual RC SpdmKeyUpdateSubTest(LwU8 keyOp, LwU8 tag);
    virtual RC SpdmFinishTest();
    virtual RC SpdmGetMeasurementsTest();
    virtual RC SpdmEndSessionTest();
    virtual RC SpdmVendorDefinedRequestTest(LwBool);

private:
    unique_ptr<UprocGspRtos>    m_pGspRtos;
};

