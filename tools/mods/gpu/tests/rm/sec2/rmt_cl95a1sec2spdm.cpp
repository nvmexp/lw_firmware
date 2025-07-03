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
// Class95a1Sec2Spdm test.
//

#include "rmt_cl95a1sec2.h"
#include "gpu/tests/rm/spdm/responder/rmt_spdmresponder.h"
#include "gpu/utility/ga10xfalcon.h"
#include <stdlib.h>

/*!
 * Class95a1Sec2SpdmTest
 *
 * This derives from the common SpdmResponderTest class in order
 * to verify the SEC2 SPDM Responder implementation.
 */
class Class95a1Sec2SpdmTest: public SpdmResponderTest
{
public:
    Class95a1Sec2SpdmTest();
    virtual ~Class95a1Sec2SpdmTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    virtual RC ReadSec2Emem(LwU32, LwU32, LwU8 *);
    virtual RC WriteSec2Emem(LwU32, LwU32, LwU8 *);
    virtual RC Sec2SpdmInitialize();
    virtual RC SendSec2Cmd(RM_FLCN_CMD_SEC2 *, RM_FLCN_MSG_SEC2 *);
    virtual RC SendSpdmRequest(LwU8 *, LwU32, LwU8 *, LwU32 *);

private:
    FLOAT64                 m_TimeoutMs;

    GpuSubdevice           *m_pSubdev;
    Sec2Rtos               *m_pSec2Rtos;
    unique_ptr<FalconImpl>  m_pSec2Falcon;

    LwU32                   m_reqPayloadEmemAddr;
    LwU32                   m_reqPayloadSize;
};

//------------------------------------------------------------------------
Class95a1Sec2SpdmTest::Class95a1Sec2SpdmTest() :
    m_pSubdev(nullptr),
    m_pSec2Falcon(nullptr)
{
    SetName("Class95a1Sec2SpdmTest");
}

//------------------------------------------------------------------------
Class95a1Sec2SpdmTest::~Class95a1Sec2SpdmTest()
{
}

//------------------------------------------------------------------------
string Class95a1Sec2SpdmTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::Setup()
{
    RC           rc = OK;
    LwRmPtr      pLwRm;

    CHECK_RC(InitFromJs());
    m_TimeoutMs = m_TestConfig.TimeoutMs();

    // Initialize GPU objects.
    m_pSubdev = GetBoundGpuSubdevice();
    m_pSubdev->GetSec2Rtos(&m_pSec2Rtos);

    // Initialize SEC2 Falcon object, used to read and write from EMEM.
    m_pSec2Falcon = (m_pSubdev->GetDeviceId() >= Device::GA102) ?
        make_unique<GA10xFalcon>(m_pSubdev, GA10xFalcon::FalconType::SEC) :
        make_unique<GM20xFalcon>(m_pSubdev, GM20xFalcon::FalconType::SEC);
    CHECK_RC(m_pSec2Falcon->Reset());

    // Get payload buffer information and initialize SPDM task in SEC2.
    m_reqPayloadEmemAddr = 0;
    m_reqPayloadSize     = 0;
    CHECK_RC(Sec2SpdmInitialize());

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::ReadSec2Emem(LwU32 offset, LwU32 numBytes, LwU8 *pOut)
{
    LwU32          numDwords  = 0;
    UINT32         tempDword  = 0;
    LwU32          dwordShift = 0;
    LwU32          dwordIndex = 0;
    LwU32          i          = 0;
    RC             rc         = OK;
    vector<UINT32> ememOutput;

    if (offset == 0 || numBytes == 0 || pOut == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }

    //
    // Callwlate number of dwords needed to read all data.
    // OK if we read more than needed, as SPDM payload should never hit EMEM boundary.
    //
    numDwords = (numBytes + (sizeof(LwU32) - 1)) / sizeof(LwU32);

    CHECK_RC(m_pSec2Falcon->ReadEMem(offset, numDwords, &ememOutput));

    if (ememOutput.size() == 0 || ememOutput.size() != numDwords)
    {
        return RC::SOFTWARE_ERROR;
    }

    // Store all requested bytes in output buffer.
    for (i = 0; i < numBytes; i++)
    {
        if ((i % sizeof(UINT32)) == 0)
        {
            // Read next dword.
            tempDword  = ememOutput[dwordIndex];
            dwordShift = 0;
            dwordIndex++;
        }

        pOut[i]     = (tempDword >> dwordShift) & 0xFF;
        dwordShift += 8;
    }

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::WriteSec2Emem(LwU32 offset, LwU32 numBytes, LwU8 *pIn)
{
    UINT32         tempDword  = 0;
    LwU32          dwordShift = 0;
    LwU32          i          = 0;
    RC             rc         = OK;
    vector<UINT32> ememInput;

    if (offset == 0 || numBytes == 0 || pIn == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }

    //
    // Write memory to EMEM in dword increments. As input is number of bytes,
    // we may write more than needed, but OK as SPDM payload should never hit
    // EMEM boundary, and SEC2 will only use number of bytes specified.
    //
    for (i = 0; i < numBytes; i++)
    {
        if (((i % sizeof(UINT32)) == 0) && i != 0)
        {
            // New dword added, push to buffer and reset.
            ememInput.push_back(tempDword);
            tempDword  = 0;
            dwordShift = 0;
        }

        tempDword  |= ((pIn[i] & 0xFF) << dwordShift);
        dwordShift += 8;
    }

    // Push back last dword.
    ememInput.push_back(tempDword);

    CHECK_RC(m_pSec2Falcon->WriteEMem(ememInput, offset));

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::Sec2SpdmInitialize()
{
    RC               rc = OK;
    RM_FLCN_CMD_SEC2 sec2Cmd;
    RM_FLCN_MSG_SEC2 sec2Msg;
    LwU32            seqNum;

    memset(&sec2Cmd, 0, sizeof(sec2Cmd));
    memset(&sec2Msg, 0, sizeof(sec2Msg));

    sec2Cmd.hdr.unitId       = RM_SEC2_UNIT_SPDM;
    sec2Cmd.hdr.size         = RM_SEC2_CMD_SIZE(SPDM, INIT);
    sec2Cmd.cmd.spdm.cmdType = RM_SEC2_CMD_TYPE(SPDM, INIT);

    CHECK_RC(m_pSec2Rtos->Command(&sec2Cmd, &sec2Msg, &seqNum));

    if (sec2Msg.msg.spdm.rspMsg.spdmStatus != 0)
    {
        Printf(Tee::PriHigh, "SPDM Task initialization failed!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_reqPayloadEmemAddr = sec2Msg.msg.spdm.initMsg.reqPayloadEmemAddr;
    m_reqPayloadSize     = sec2Msg.msg.spdm.initMsg.reqPayloadSize;

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::SendSec2Cmd(RM_FLCN_CMD_SEC2 *pReqCmd, RM_FLCN_MSG_SEC2 *pRspMsg)
{
    RC     rc = OK;
    UINT32 seqNum;

    if (m_pSec2Rtos == nullptr)
    {
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: Test called but not initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: Submitting SPDM CMD to SEC2.\n");

    // Submit the command. We will block until message response is received.
    CHECK_RC(m_pSec2Rtos->Command(pReqCmd, pRspMsg, &seqNum));

    // Success, print out details.
    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: CMD submission success, got seqNum = %u\n",
           seqNum);

    Printf(Tee::PriDebug,  "Class95a1Sec2SpdmTest: Received MSG response from SEC2\n");
    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: Printing MSG header:\n");
    Printf(Tee::PriDebug, "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
           pRspMsg->hdr.unitId, pRspMsg->hdr.size, pRspMsg->hdr.ctrlFlags, pRspMsg->hdr.seqNumId);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::SendSpdmRequest(LwU8 *pPayloadIn, LwU32 payloadInSize, LwU8 *pPayloadOut, LwU32 *pPayloadOutSize)
{
    RC               rc    = OK;
    RM_FLCN_CMD_SEC2 sec2Cmd;
    RM_FLCN_MSG_SEC2 sec2Msg;

    if (pPayloadIn == NULL  || payloadInSize == 0 || pPayloadOut == NULL ||
        pPayloadOutSize == NULL || *pPayloadOutSize == 0)
    {
        return RC::SOFTWARE_ERROR;
    }
    else if (m_pSec2Rtos == nullptr || m_reqPayloadEmemAddr == 0 || m_reqPayloadSize == 0)
    {
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: Test called but not initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    // Prepare SEC2 Request command.
    sec2Cmd.hdr.unitId                         = RM_SEC2_UNIT_SPDM;
    sec2Cmd.hdr.size                           = RM_SEC2_CMD_SIZE(SPDM, REQUEST);
    sec2Cmd.cmd.spdm.cmdType                   = RM_SEC2_SPDM_CMD_ID_REQUEST;
    sec2Cmd.cmd.spdm.reqCmd.reqPayloadSize     = payloadInSize;
    sec2Cmd.cmd.spdm.reqCmd.reqPayloadEmemAddr = m_reqPayloadEmemAddr;

    if (m_reqPayloadSize < payloadInSize)
    {
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: Request payload is too large for EMEM buffer!\n");
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: Request payload: %u bytes, Buffer: %u bytes\n",
               payloadInSize, m_reqPayloadSize);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: Writing %u bytes to EMEM address 0x%u.\n",
           payloadInSize, m_reqPayloadEmemAddr);
    CHECK_RC(WriteSec2Emem(m_reqPayloadEmemAddr, payloadInSize, pPayloadIn));

    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: Printing request:\n");
    DumpSpdmMessage(pPayloadIn, payloadInSize);

    CHECK_RC(SendSec2Cmd(&sec2Cmd, &sec2Msg));

    if (sec2Msg.msg.spdm.rspMsg.spdmStatus != FLCN_OK)
    {
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: SEC2 returned error: %02X!\n",
               sec2Msg.msg.spdm.rspMsg.spdmStatus);
        return RC::SOFTWARE_ERROR;
    }

    LwU32 recvPayloadSize = sec2Msg.msg.spdm.rspMsg.rspPayloadSize;
    LwU32 recvPayloadAddr = sec2Msg.msg.spdm.rspMsg.rspPayloadEmemAddr;
    if (recvPayloadSize > *pPayloadOutSize)
    {
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: Response payload is too large for buffer!\n");
        Printf(Tee::PriHigh, "Class95a1Sec2SpdmTest: Response payload: %u bytes, Buffer: %u bytes\n",
               recvPayloadSize, *pPayloadOutSize);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: Reading %u bytes from EMEM address 0x%u.\n",
           recvPayloadSize, recvPayloadAddr);

    CHECK_RC(ReadSec2Emem(recvPayloadAddr, recvPayloadSize, pPayloadOut));
    *pPayloadOutSize = recvPayloadSize;

    Printf(Tee::PriDebug, "Class95a1Sec2SpdmTest: Printing response:\n");
    DumpSpdmMessage(pPayloadOut, recvPayloadSize);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::Run()
{
    RC rc = OK;

    CHECK_RC(RunAllTests());

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2SpdmTest::Cleanup()
{
    RC rc = OK;
    LwRmPtr pLwRm;

    m_pSec2Rtos = NULL;

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2SpdmTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2SpdmTest, RmTest, "Class95a1Sec2SpdmTest RM test.");
