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

#ifndef INCLUDED_AZARBTM_H
#define INCLUDED_AZARBTM_H

#include "azatmod.h"
#include <queue>
#include <vector>

#define SETGET_TEST_PROP(propName, resulttype) \
    resulttype Get##propName(){ return m_##propName; } \
    RC Set##propName(resulttype val){ m_##propName = val; return OK; }

class AzaliaRingBufferTestModule : public AzaliaTestModule
{
public:

    AzaliaRingBufferTestModule();
    virtual ~AzaliaRingBufferTestModule();

    virtual RC SetDefaults();
    virtual RC Reset(AzaliaController* pAza);
    virtual RC Start();
    virtual RC Continue();
    virtual RC Stop();

    SETGET_TEST_PROP(NumCommands, UINT32);
    SETGET_TEST_PROP(CorbSize, UINT32);
    SETGET_TEST_PROP(RirbSize, UINT32);
    SETGET_TEST_PROP(RIntCnt, UINT32);
    SETGET_TEST_PROP(IsCorbStop, bool);
    SETGET_TEST_PROP(IsRirbStop, bool);
    SETGET_TEST_PROP(IsAllowPtrResets, bool);
    SETGET_TEST_PROP(IsAllowNullCommands, bool);
    SETGET_TEST_PROP(IsUnsolRespEnable, bool);
    SETGET_TEST_PROP(IsUnsolRespEnableCdc, bool);
    SETGET_TEST_PROP(IsExpUnsolResp, bool);
    SETGET_TEST_PROP(IsRirbRIntEn, bool);
    SETGET_TEST_PROP(IsRirbOIntEn, bool);
    SETGET_TEST_PROP(IsEnableOverrunTest, bool);

private:

    virtual RC DoPrintInfo(Tee::Priority Pri, bool PrintInput, bool PrintStatus);

    typedef struct
    {
        UINT08 codecAddr;
        UINT32 nodeID;
        UINT32 verb;
        UINT32 payload;
        UINT32 response;
    } RegTestInfo;

    typedef struct
    {
        bool stopped;
        UINT64 resumeTime;
        UINT32 commandsUntilPause;
    } RingBufferInfo;

    enum TestStatus
    {
        DO_NOT_TEST = 0,
        DO_TEST = 1,
        IN_PROGRESS = 2,
        COMPLETED_PASS = 9,
        COMPLETED_FAIL = 10
    };

    struct
    {
        RingBufferInfo corbInfo;
        RingBufferInfo rirbInfo;
        UINT32 chunkSize;
        UINT32 requiredFreeSpace;
        UINT32 commandsStarted;
        UINT32 commandsFinished;
        queue<UINT32>* pushedVerbs;
        queue<UINT64>* expectedResponses;
        TestStatus rirbOverrunStatus;
        bool busMasterDisabled;
        UINT32 numUnsolRespReceived;
        UINT32 xveFifoWrLimit;
        UINT32 physGpuBar0;
    } m_Status;

    vector<RegTestInfo> m_RegReadOnlyTable;
    vector<RegTestInfo> m_RegReadWriteTable;

    RC PrepareRegisterTables();
    RC ToggleCorbRunning(bool IsRun);
    RC ToggleRirbRunning(bool IsRun);
    RC GenerateBatchOfCommands(UINT32* pVerbs, UINT64* pResponses, UINT32 nCommands);
    RC PushCommands();
    RC ProcessResponses();
    RC SendCommands();

    UINT32 m_NumCommands;
    UINT32 m_CorbSize;
    UINT32 m_RirbSize;
    UINT32 m_RIntCnt;
    bool m_IsCorbStop;
    bool m_IsRirbStop;
    bool m_IsAllowPtrResets;
    bool m_IsAllowNullCommands;
    bool m_IsUnsolRespEnable;
    bool m_IsUnsolRespEnableCdc;
    bool m_IsExpUnsolResp;
    bool m_IsRirbRIntEn;
    bool m_IsRirbOIntEn;
    bool m_IsEnableOverrunTest;

};

#endif // INCLUDED_AZARBTM_H
