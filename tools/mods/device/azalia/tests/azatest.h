/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZATEST_H
#define INCLUDED_AZATEST_H

#include "device/azalia/azafmt.h"
#include "device/azalia/azautil.h"
#include "device/azalia/azadmae.h"
#include "azarbtm.h"
#include "azasttm.h"
#include "cheetah/include/bsctest.h"
#include "core/include/rc.h"
#include <queue>
#include <vector>
#include "strmprop.h"
#include "core/include/setget.h"

class AzaliaTest : public BscTest
{
public:

    RC CheckLoopback(UINT32 srcChan, AzaStream* pSrcStream,
                     UINT32 destChan, AzaStream* pDestStream);

    AzaliaTest();
    virtual ~AzaliaTest();

    virtual RC PrintProperties(Tee::Priority Pri = Tee::PriNormal);

    RC SetDefaultParameters();
    RC Reset();
    RC DmaTest(Tee::Priority pri = Tee::PriNormal);
    RC VerifyDmaTest(Tee::Priority pri = Tee::PriNormal);

    // Core test properties
    SETGET_TEST_PROP(Loops, UINT32);
    SETGET_PROP_VAR(Duration, UINT32, m_DmaTestInfo.duration);
    SETGET_PROP_FUNC(TimeScaler, UINT32, AzaliaUtilities::SetTimeScaler, AzaliaUtilities::GetTimeScaler);
    SETGET_PROP_FUNC(MaxSingleWaitTime, UINT32, AzaliaUtilities::SetMaxSingleWaitTime, AzaliaUtilities::GetMaxSingleWaitTime);
    SETGET_TEST_PROP(SimOnSilicon, bool);
    SETGET_TEST_PROP(AzaDevIndex, UINT32);

    // Interrupt Properties
    SETGET_TEST_PROP(Pci_IntEn, bool);
    SETGET_TEST_PROP(Global_IntEn, bool);
    SETGET_TEST_PROP(Cme_IntEn, bool);
    SETGET_TEST_PROP(Cntrl_IntEn, bool);
    SETGET_TEST_PROP(Seed, UINT32);

    // Coherence Properties
    SETGET_TEST_PROP(BdlCoh, bool);
    SETGET_TEST_PROP(CorbCoh, bool);
    SETGET_TEST_PROP(RirbCoh, bool);
    SETGET_TEST_PROP(DmaPosCoh, bool);
    SETGET_TEST_PROP(InputStreamCoh, bool);
    SETGET_TEST_PROP(OutputStreamCoh, bool);

    // Ring buffer test properites
    SETGET_PROP_FUNC(RB_Enable, bool, m_RBTest->SetEnabled, m_RBTest->IsEnabled);
    SETGET_PROP_FUNC(RB_NCommands, UINT32, m_RBTest->SetNumCommands, m_RBTest->GetNumCommands);
    SETGET_PROP_FUNC(RB_CorbStop, bool, m_RBTest->SetIsCorbStop, m_RBTest->GetIsCorbStop);
    SETGET_PROP_FUNC(RB_RirbStop, bool, m_RBTest->SetIsRirbStop, m_RBTest->GetIsRirbStop)
    SETGET_PROP_FUNC(RB_AllowPtrResets, bool, m_RBTest->SetIsAllowPtrResets, m_RBTest->GetIsAllowPtrResets);
    SETGET_PROP_FUNC(RB_AllowNullCommands, bool, m_RBTest->SetIsAllowNullCommands, m_RBTest->GetIsAllowNullCommands);
    SETGET_PROP_FUNC(RB_CorbSize, UINT32, m_RBTest->SetCorbSize, m_RBTest->GetCorbSize);
    SETGET_PROP_FUNC(RB_RirbSize, UINT32, m_RBTest->SetRirbSize, m_RBTest->GetCorbSize);
    SETGET_PROP_FUNC(RB_RIntCnt, UINT32, m_RBTest->SetRIntCnt, m_RBTest->GetRIntCnt);
    SETGET_PROP_FUNC(RB_ROIntEn, bool, m_RBTest->SetIsRirbOIntEn, m_RBTest->GetIsRirbOIntEn);
    SETGET_PROP_FUNC(RB_RRIntEn, bool, m_RBTest->SetIsRirbRIntEn, m_RBTest->GetIsRirbRIntEn);
    SETGET_PROP_FUNC(RB_OverrunTest, bool, m_RBTest->SetIsEnableOverrunTest, m_RBTest->GetIsEnableOverrunTest);
    SETGET_PROP_FUNC(RB_EnableUnsolResponses, bool, m_RBTest->SetIsUnsolRespEnable, m_RBTest->GetIsUnsolRespEnable);
    SETGET_PROP_FUNC(RB_ExpectUnsolResponses, bool, m_RBTest->SetIsExpUnsolResp, m_RBTest->GetIsExpUnsolResp);
    SETGET_PROP_FUNC(RB_EnableCodelwnsolResponses, bool, m_RBTest->SetIsUnsolRespEnableCdc, m_RBTest->GetIsUnsolRespEnableCdc);

    // Stream test properties
    SETGET_PROP_FUNC(ST_Enable, bool, m_STest->SetEnabled, m_STest->IsEnabled);
    SETGET_PROP_FUNC(ST_LoopInput, bool, m_STest->SetLoopInput, m_STest->GetLoopInput);
    SETGET_PROP_FUNC(ST_LoopOutput, bool, m_STest->SetLoopOutput, m_STest->GetLoopOutput);
    SETGET_PROP_FUNC(ST_DmaPos, bool, m_STest->SetDmaPos, m_STest->GetDmaPos);
    SETGET_PROP_FUNC(ST_TestFifoErrors, bool, m_STest->SetTestFifoErr, m_STest->GetTestFifoErr);
    SETGET_PROP_FUNC(ST_SkipLbCheck, bool, m_STest->SetSkipLbCheck, m_STest->GetSkipLbCheck);
    SETGET_PROP_FUNC(ST_FftRatio, FLOAT64, m_STest->SetFftRatio, m_STest->GetFftRatio);
    SETGET_PROP_FUNC(ST_2ChanOutBuffSize, UINT08, m_STest->Set2ChanOutBuffSize, m_STest->Get2ChanOutBuffSize);
    SETGET_PROP_FUNC(ST_6ChanOutBuffSize, UINT08, m_STest->Set6ChanOutBuffSize, m_STest->Get6ChanOutBuffSize);
    SETGET_PROP_FUNC(ST_8ChanOutBuffSize, UINT08, m_STest->Set8ChanOutBuffSize, m_STest->Get8ChanOutBuffSize);
    SETGET_PROP_FUNC(ST_TestProgOutStrmBuff, UINT08, m_STest->SetTestProgOutStrmBuff, m_STest->GetTestProgOutStrmBuff);

    RC SetStreamInput(UINT32 stream, UINT32 type, string value, UINT08 dev = 0);
    RC SetStreamInput(UINT32 stream, UINT32 type, UINT32 value, UINT08 dev = 0);
    RC SetReservation(UINT32 stream, UINT32 codec, UINT32 pin, UINT08 dev = 0);
    RC CreateLoopback(UINT32 srcStreamIndex, UINT32 srcChan, UINT32 destStreamIndex, UINT32 destChan);
    RC CreateDefaultLoopback(UINT32 codec, UINT32 srcStreamIndex, UINT32 srcChan, UINT32 destStreamIndex, UINT32 destChan);
    RC SetStreamDefaults(StreamProperties::strmProp* stream);

    enum
    {
        ALSA_DEV = 0xa15a
    };
    RC DualLoopback(UINT32 srcDev, UINT32 srcCodec, UINT32 srcPin, UINT32 srcStreamIndex,
                    UINT32 destDev, UINT32 destCodec, UINT32 destPin, UINT32 destStreamIndex, Tee::Priority pri);

private:
    //-----------------------------------------//
    //           Private data types            //
    //-----------------------------------------//

    typedef struct
    {
        UINT64 startTime;
        UINT32 duration;
        bool error;
    } DmaTestInfo;

    //-----------------------------------------//
    //      Private Methods and Members        //
    //-----------------------------------------//

    // Dma test functions
    RC DmaTestReset();
    RC DmaTestStart();
    RC DmaTestPoll(bool* isFinished);
    RC DmaTestStop();
    RC DmaTestCheck();
    RC DmaTestPrintInfo(Tee::Priority pri, bool printInput, bool printStatus);

    RC Initialize_Private() override;
    RC Uninitialize_Private() override;
    RC InitProperties_Private() override;
    RC PrintInfo_Private() override;
    RC PrintPropertiesInit_Private(Tee::Priority Pri) override {return OK;}
    RC PrintPropertiesNoInit_Private(Tee::Priority Pri) override {return OK;}

    UINT32 m_AzaDevIndex;
    AzaliaController* m_pAza;
    UINT32 m_Loops;
    DmaTestInfo m_DmaTestInfo;
    UINT32 m_Seed;

    AzaliaRingBufferTestModule* m_RBTest;
    AzaliaStreamTestModule* m_STest;
    bool m_Pci_IntEn;       // Pci Interrupt
    bool m_Global_IntEn;    // Global Interrupt
    bool m_Cntrl_IntEn;     // Controller Interrupt
    bool m_Cme_IntEn;       // CORB Memory Error Interrupt
    bool m_SimOnSilicon;

    bool m_BdlCoh;
    bool m_CorbCoh;
    bool m_RirbCoh;
    bool m_InputStreamCoh;
    bool m_OutputStreamCoh;
    bool m_DmaPosCoh;

    StreamProperties::strmProp m_streams[10];
};

#endif // INCLUDED_AZATEST_H
