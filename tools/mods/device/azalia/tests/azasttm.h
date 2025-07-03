/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZASTTM_H
#define INCLUDED_AZASTTM_H

#include "azatmod.h"
#include <vector>

#define SETGET_TEST_PROP(propName, resulttype) \
    resulttype Get##propName(){ return m_##propName; } \
    RC Set##propName(resulttype val){ m_##propName = val; return OK; }

class AzaStream;

class AzaliaStreamTestModule : public AzaliaTestModule
{
public:
    AzaliaStreamTestModule();
    virtual ~AzaliaStreamTestModule();

    virtual RC SetDefaults();
    virtual RC Reset(AzaliaController* pAza);
    virtual RC Start();
    virtual RC Continue();
    virtual RC Stop();

    SETGET_TEST_PROP(LoopInput, bool);
    SETGET_TEST_PROP(LoopOutput, bool);
    SETGET_TEST_PROP(DmaPos, bool);
    SETGET_TEST_PROP(TestFifoErr, bool);
    SETGET_TEST_PROP(SkipLbCheck, bool);
    SETGET_TEST_PROP(FftRatio, double);
    SETGET_TEST_PROP(2ChanOutBuffSize, bool);
    SETGET_TEST_PROP(6ChanOutBuffSize, bool);
    SETGET_TEST_PROP(8ChanOutBuffSize, bool);
    SETGET_TEST_PROP(TestProgOutStrmBuff, bool);

    virtual RC GetStream(UINT32 index, AzaStream **pStrm);
    RC CreateLoopback(UINT32 srcStreamIndex, UINT32 srcChan,
                      UINT32 destStreamIndex, UINT32 destChan);

    RC FlushFifos();
    RC AddTestStream(UINT32 index);
    RC ClearTestStreams();

private:
    virtual RC DoPrintInfo(Tee::Priority Pri, bool PrintInput, bool PrintStatus);
    RC VerifyStreamsRunning(bool SyncRunning, bool UnSyncRunning);
    RC RouteStreams();
    RC CheckLoopbacks();
    bool ShouldBeStopped(UINT32 Sindex, UINT32 Pos);

    typedef struct
    {
        UINT32 srcSIndex;
        UINT32 srcChan;
        UINT32 destSIndex;
        UINT32 destChan;
    } Loopback;

#define MAX_NUM_TEST_STREAMS 10
#define MAX_NUM_LOOPBACKS 4
    Loopback m_Loopbacks[MAX_NUM_LOOPBACKS];
    UINT32 m_NActiveLoopbacks;
    vector<UINT32> m_vStreamIndices;
    bool m_LoopInput;
    bool m_LoopOutput;
    bool m_RunUntilStopped;
    bool m_DmaPos;
    bool m_TestFifoErr;
    bool m_SkipLbCheck;
    bool m_FifoErrNotTestedYet;
    bool m_PauseResumeNotTestedYet;
    bool m_2ChanOutBuffSize;
    bool m_6ChanOutBuffSize;
    bool m_8ChanOutBuffSize;
    bool m_TestProgOutStrmBuff;

    UINT64 m_StartuSec;
    double m_FftRatio;
};

#endif // INCLUDED_AZASTTM_H

