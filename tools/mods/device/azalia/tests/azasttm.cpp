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

#include "azasttm.h"
#include "device/azalia/azacdc.h"
#include "device/azalia/azactrl.h"
#include "device/azalia/azareg.h"
#include "device/azalia/azafpci.h"
#include "core/include/platform.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/tasker.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaSTTM"

AzaliaStreamTestModule::AzaliaStreamTestModule()
: AzaliaTestModule()
{
    m_FftRatio = 0.0;
    SetDefaults();
}

AzaliaStreamTestModule::~AzaliaStreamTestModule()
{
}

RC AzaliaStreamTestModule::DoPrintInfo(Tee::Priority Pri,
                                       bool PrintInput, bool PrintStatus)
{
    RC rc;
    if (m_IsEnabled && PrintInput)
    {
        Printf(Pri, "Stream Test (%s)\n", (m_IsEnabled ? "enabled" : "disabled"));
        Printf(Pri, "Stream Test Properties\n");
        Printf(Pri, "    JS Variable         Value  Description\n");
        Printf(Pri, "    -----------         -----  -----------\n");
        Printf(Pri, "    Properties that are set by calling Reset\n");
        Printf(Pri, "    -----------------------------------------\n");
        Printf(Pri, "    ST_LoopInput      : %5d  Loop the input stream indefinitely\n", m_LoopInput);
        Printf(Pri, "    ST_LoopOutput     : %5d  Loop the output stream indefinitely\n", m_LoopOutput);
        Printf(Pri, "    ST_DmaPos         : %5d  Report the position using DMA position buffer\n", m_DmaPos);
        Printf(Pri, "    ST_TestFifoErrors : %5d  Run the FIFO Error test\n", m_TestFifoErr);
        Printf(Pri, "    ST_FftRatio       : %5f  Set the FFT threshold ratio for data comparison (overrides regular comparison)\n", m_FftRatio);
        Printf(Pri, "    -----------------------------------------\n");
        Printf(Pri, "    Properties that can be changed on the fly \n");
        Printf(Pri, "    -----------------------------------------\n");
        Printf(Pri, "    ST_SkipLbCheck    : %5d  Skip data comparison when running loopback\n", m_SkipLbCheck);
        Printf(Pri, "    ST_Enable         : %5d  Enable the stream test\n", m_IsEnabled);

        Printf(Pri, "\n");
        Printf(Pri, "-----------------\n");
        Printf(Pri, "Stream Properties\n");
        Printf(Pri, "-----------------\n");
        Printf(Pri, "Number of Test Streams: %d\n", (UINT32)m_vStreamIndices.size());
        for (UINT32 i=0; i<m_vStreamIndices.size(); i++)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
            CHECK_RC(pStream->PrintInfo(Pri));
        }
    }
    if (m_IsEnabled && PrintStatus)
    {
        Printf(Pri, "ST Test: Finished %d, Has Err: %d\n", m_IsFinished, m_IsError);
    }
    return OK;
}

RC AzaliaStreamTestModule::SetDefaults()
{
    RC rc;
    CHECK_RC(SetDefaultsBase());
    memset(&m_Loopbacks, 0, sizeof(Loopback)*MAX_NUM_LOOPBACKS);
    m_NActiveLoopbacks = 0;
    m_LoopInput = false;
    m_LoopOutput = true;
    m_RunUntilStopped = true;
    m_DmaPos = true;
    m_TestFifoErr = false;
    m_FifoErrNotTestedYet = true;
    m_PauseResumeNotTestedYet = true;
    m_StartuSec = 0;
    m_SkipLbCheck = false;
    m_2ChanOutBuffSize = 10;
    m_6ChanOutBuffSize = 10;
    m_8ChanOutBuffSize = 10;
    m_TestProgOutStrmBuff = false;

    return OK;
}

RC AzaliaStreamTestModule::Reset(AzaliaController* pAza)
{
    RC rc;

    CHECK_RC(ResetBase(pAza));
    if (m_IsEnabled)
    {
        CHECK_RC(m_pAza->DmaPosToggle(m_DmaPos));

        bool hasInputs = false, hasOutputs = false;
        UINT32 maxTestStreams = 0;
        CHECK_RC(m_pAza->GetMaxNumStreams(&maxTestStreams));
        for (UINT32 i=0; i<maxTestStreams; i++)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(i, &pStream));
            CHECK_RC(pStream->Reset(true)); // Release routes
            if (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT)
                hasInputs = true;
            else
                hasOutputs = true;
        }
        m_RunUntilStopped = true;
        if ((hasInputs && !m_LoopInput) || (hasOutputs && !m_LoopOutput))
        {
            m_RunUntilStopped = false;
        }

        // Reset the fifo error test status
        m_FifoErrNotTestedYet = true;

        CHECK_RC(RouteStreams());

        if (m_TestProgOutStrmBuff)
        {
            Printf(Tee::PriDebug, "Programming output stream buffer frames\n");
            UINT32 val = 0;
            val = RF_NUM(AZA_OB_BUFSZ_NUM_OF_FRAMES_2CHAN, m_2ChanOutBuffSize) |
                  RF_NUM(AZA_OB_BUFSZ_NUM_OF_FRAMES_6CHAN, m_6ChanOutBuffSize) |
                  RF_NUM(AZA_OB_BUFSZ_NUM_OF_FRAMES_8CHAN, m_8ChanOutBuffSize);

            CHECK_RC( m_pAza->CfgWr32(AZA_OB_BUFSZ_NUM_OF_FRAMES, val) );

            val = 0;
            CHECK_RC( m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR_EN, &val) );
            for (UINT32 i=0; i<m_vStreamIndices.size(); i++)
            {
                AzaStream *pStream;
                CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
                if ((pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT) &&
                    pStream->input.testOutStrmBuffMon)
                {
                    UINT08 engine = 0;
                    UINT32 maxInputStrm = 0;
                    CHECK_RC(pStream->GetEngine()->GetEngineIndex(&engine));
                    CHECK_RC(m_pAza->GetMaxNumInputStreams(&maxInputStrm));

                    val |= RF_SET(AZA_OB_STR_FIFO_MONITOR_EN_STR(engine - maxInputStrm));
                    Printf(Tee::PriDebug, "Enabling Monitoring on Stream: %d\n", i);
                }
            }
            CHECK_RC( m_pAza->CfgWr32(AZA_OB_STR_FIFO_MONITOR_EN, val) );
        }

        // Set RMS for all streams
        if (m_FftRatio > 0.0)
            for (UINT32 i=0; i<m_vStreamIndices.size(); i++)
            {
                AzaStream *pStream;
                CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
                pStream->SetFftRatio(m_FftRatio);
            }

        // Program non-truncated streams
        for (UINT32 i=0; i<m_vStreamIndices.size(); i++)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
            if(!pStream->input.truncStream)
                CHECK_RC(pStream->ProgramStream());
        }

        // Program truncated streams first so that it can get the
        // engine it requires
        for (UINT32 i=0; i<m_vStreamIndices.size(); i++)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
            if(pStream->input.truncStream)
                CHECK_RC(pStream->ProgramStream());
        }

        for (UINT32 i=0; i<m_vStreamIndices.size(); i++)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
#ifndef SIM_BUILD
            // Do this in a separate loop to make sure that two streams
            // didn't stomp on each others' routes. Don't do it on
            // simulation, though, because it calls for sending a bunch
            // of commands to the codec, which takes far too long. It's
            // mostly useful for dealing with various third-party codecs
            // anyways (to make sure that we programmed them correctly).
            CHECK_RC(pStream->VerifySettings());
#endif
            CHECK_RC(pStream->PrepareTest());

        }
    }
    return OK;
}

RC AzaliaStreamTestModule::Start()
{
    RC rc;
    UINT32 i;

    if (!m_IsEnabled)
    {
        m_IsFinished = true;
        return OK;
    }

    if (m_vStreamIndices.size() == 0)
    {
        Printf(Tee::PriWarn, "No streams to start for stream test!\n");
    }

    // Set sync bits for sync'd streams
    UINT32 syncval = 0;
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.sync)
        {
            UINT08 temp = 0;
            CHECK_RC(pStream->GetEngine()->GetEngineIndex(&temp));
            syncval |= 1 << temp;
        }
    }
    CHECK_RC(m_pAza->RegWr(SSYNC, syncval));

    // Turn on DMA for each stream that's enabled
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        CHECK_RC(pStream->GetEngine()->SetEngineState(AzaliaDmaEngine::STATE_RUN));
    }

    // Verify that synchronized streams are not running and non-synchornized
    // ones are. Use register positions only because DMA positions can have a
    // very large delay that makes it look like the stream is not running.
    CHECK_RC(VerifyStreamsRunning(false, true));

    // Make sure fifo ready is asserted for all synchronized streams.
    // Fifo ready behavior is unspecific for unsynchronized streams, so
    // we don't test that case.
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.sync)
        {
            CHECK_RC(POLLWRAP_HW((bool (*)(void*))AzaliaDmaEngine::PollIsFifoReady,
                                  pStream->GetEngine(), Tasker::GetDefaultTimeoutMs()));
        }
    }

    // Start synchronized streams
    CHECK_RC(m_pAza->RegWr(SSYNC, 0));

    // Verify that all streams are now running. Use register positions only
    // because DMA positions can have a very large delay that makes it look
    // like the stream is not running.
    CHECK_RC(VerifyStreamsRunning(true, true));

    m_StartuSec = Platform::GetTimeUS();
    return OK;
}

bool AzaliaStreamTestModule::ShouldBeStopped(UINT32 Sindex, UINT32 Pos)
{
    RC rc;
    AzaStream *pStream;
    CHECK_RC(m_pAza->GetStream(Sindex, &pStream));
    return (((pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT &&
              !m_LoopInput) ||
             (pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT &&
              !m_LoopOutput)) &&
            (pStream->GetNLoops() != 0 ||
             pStream->GetEndThresholdInBytes() < Pos));
}

RC AzaliaStreamTestModule::VerifyStreamsRunning(bool SyncRunning, bool UnSyncRunning)
{
    RC rc;
    UINT32 i;
    UINT32 positions[MAX_NUM_TEST_STREAMS];

    // This is WAR for GT21x RTL only due to a corner case when the LPIB is
    // polled too quickly. Wait for a timescaler to avoid this issue.
    #ifdef SIM_BUILD
    if (m_pAza->IsGpuAza())
        AzaliaUtilities::Pause(1);
    #endif

    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        positions[m_vStreamIndices[i]] = pStream->GetEngine()->GetPosition();
    }

    AzaliaUtilities::Pause(1);
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        UINT32 pos = pStream->GetEngine()->GetPosition();

        if (pStream->input.sync)
        {
            if (SyncRunning && (pos == positions[m_vStreamIndices[i]]))
            {
                if (ShouldBeStopped(m_vStreamIndices[i], pos))
                {
                    Printf(Tee::PriDebug, "Stream %d stopped manually near end of buffer\n",
                           m_vStreamIndices[i]);
                }
                else
                {
                    Printf(Tee::PriError, "Stream %d (%s) is synchronized and "
                                          "should be running (LPIB not progressing)\n",
                           m_vStreamIndices[i],
                           (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT) ? "IB" : "OB");

                    return RC::AZA_STREAM_ERROR;
                }
            }
            else if (!SyncRunning && (pos != positions[m_vStreamIndices[i]]))
            {
                Printf(Tee::PriError, "Stream %d (%s) is synchronized and "
                                      "should NOT be running (LPIB progressing)\n",
                       m_vStreamIndices[i],
                       (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT) ? "IB" : "OB");

                return RC::AZA_STREAM_ERROR;
            }
        }
        else
        {
            if (UnSyncRunning && (pos == positions[m_vStreamIndices[i]]))
            {
                if (ShouldBeStopped(m_vStreamIndices[i], pos))
                {
                    Printf(Tee::PriDebug, "Stream %d stopped manually near end of buffer\n",
                           m_vStreamIndices[i]);
                }
                else
                {
                    // For descriptor error test we expect the stream to be stopped
                    if (!pStream->input.testDescErrors)
                    {

                        Printf(Tee::PriError, "Stream %d (%s) is unsynchronized and "
                                              "should be running (LPIB not progressing)\n",
                               m_vStreamIndices[i],
                               (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT) ? "IB" : "OB");

                        return RC::AZA_STREAM_ERROR;
                    }
                }
            }
            else if (!UnSyncRunning && (pos != positions[m_vStreamIndices[i]]))
            {
                Printf(Tee::PriError, "Stream %d (%s) is unsynchronized and "
                                      "should NOT be running (LPIB progressing)\n",
                       m_vStreamIndices[i],
                       (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT) ? "IB" : "OB");

                return RC::AZA_STREAM_ERROR;
            }
        }
    }
    return OK;
}

RC AzaliaStreamTestModule::Continue()
{
    RC rc;
    UINT32 i;
    bool success;
    bool localFinished = true;
    if (!m_IsEnabled)
    {
        m_IsFinished = localFinished;
        return OK;
    }

    if (m_TestProgOutStrmBuff)
    {
        for (UINT32 i = 0; i < m_vStreamIndices.size(); ++i)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
            if (pStream->input.testOutStrmBuffMon &&
                (pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT))
            {
                UINT08 engine = 0;
                UINT32 maxInputStrm = 0;
                CHECK_RC(pStream->GetEngine()->GetEngineIndex(&engine));
                CHECK_RC(m_pAza->GetMaxNumInputStreams(&maxInputStrm));
                engine = engine - maxInputStrm;

                UINT32 val = 0;
                switch (engine)
                {
                    case 0: CHECK_RC(m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR0, &val));
                            val = RF_VAL(AZA_OB_STR_FIFO_MONITOR0_STR0, val);
                            Printf(Tee::PriDebug, "Engine 0 Stream Monitor: 0x%08x\n", val);
                            break;
                    case 1: CHECK_RC(m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR0, &val));
                            val = RF_VAL(AZA_OB_STR_FIFO_MONITOR0_STR1, val);
                            Printf(Tee::PriDebug, "Engine 1 Stream Monitor: 0x%08x\n", val);
                            break;
                    case 2: CHECK_RC(m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR1, &val));
                            val = RF_VAL(AZA_OB_STR_FIFO_MONITOR1_STR2, val);
                            Printf(Tee::PriDebug, "Engine 2 Stream Monitor: 0x%08x\n", val);
                            break;
                    case 3: CHECK_RC(m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR1, &val));
                            val = RF_VAL(AZA_OB_STR_FIFO_MONITOR1_STR3, val);
                            Printf(Tee::PriDebug, "Engine 3 Stream Monitor: 0x%08x\n", val);
                            break;
                    case 4: CHECK_RC(m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR2, &val));
                            val = RF_VAL(AZA_OB_STR_FIFO_MONITOR2_STR4, val);
                            Printf(Tee::PriDebug, "Engine 4 Stream Monitor: 0x%08x\n", val);
                            break;
                    case 5: CHECK_RC(m_pAza->CfgRd32(AZA_OB_STR_FIFO_MONITOR2, &val));
                            val = RF_VAL(AZA_OB_STR_FIFO_MONITOR2_STR5, val);
                            Printf(Tee::PriDebug, "Engine 5 Stream Monitor: 0x%08x\n", val);
                            break;
                    default:
                            Printf(Tee::PriError, "Invalid Engine Index for Output Stream\n");
                            return RC::SOFTWARE_ERROR;
                }

                UINT32 expectedFifoSize = pStream->GetEngine()->ExpectedFifoSize(pStream->GetDir(), pStream->GetFormat());
                if (expectedFifoSize * 0.15 > val)
                {
                    Printf(Tee::PriError, "Fifo monitor buffer size(%d) not in line with "
                                          "expected fifo size(%d) for engine %d\n",
                           val, expectedFifoSize, engine);

                    return RC::AZATEST_FAIL;
                }
            }
        }
    }

    if(m_TestFifoErr && m_FifoErrNotTestedYet)
    {
        Printf(Tee::PriDebug, "Testing Fifo Errors...\n");

        // Temporarily turn off bus mastering
        Printf(Tee::PriLow, "Disabling Bus Mastering...\n");
        CHECK_RC(m_pAza->CfgWr08(AZA_CFG_1, 0x03));

        AzaliaUtilities::Pause(100); // this value must be large enough to ensure that a fifo error will occur

        // We should see fifo errors on all running streams
        // Set this in each of the streams
        //
        // However on codecs that don't have a routable input path (codecs tested with internal loopback)
        // input streams will not see a Fifo Error. Hence only expect it for the output streams
        //
        // --this should not cause RIRB overruns in most cases, but we will tolerate them
        // Also our position checks will get confused since we paused - so Reset it
        for (i = 0; i < m_vStreamIndices.size(); ++i)
        {
            AzaStream *pStream;
            AzaliaCodec *pCodec;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
            CHECK_RC(m_pAza->GetCodec(pStream->input.reservation.codec, &pCodec));
            if((pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT) ||
               ((pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT) && (pCodec->IsSupportRoutableInputPath())))
            {
                pStream->input.testFifoErrors = true;
                pStream->ResetElapsedTime();
            }
        }

        // Make sure we only do this once per test
        m_FifoErrNotTestedYet = false;
    }

    if (m_PauseResumeNotTestedYet)
    {
        for (i = 0; i < m_vStreamIndices.size(); ++i)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
            if (pStream->input.testPauseResume && (Platform::GetTimeUS() > ((UINT64)(m_StartuSec + 2000000))))
            {
                Printf(Tee::PriLow, "Testing Stream Pause/Resume on stream %d..\n", i);

                AzaliaDmaEngine::STATE state;
                CHECK_RC(pStream->GetEngine()->GetEngineState(&state));
                if (state==AzaliaDmaEngine::STATE_STOP)
                {
                    Printf(Tee::PriError, "Stream %d is already stopped. Cannot pause/resume!!\n",
                           m_vStreamIndices[i]);

                    return RC::AZATEST_FAIL;
                }

                // Pause the stream and verify that it enters stopped state within 40us (two frames)
                CHECK_RC(pStream->GetEngine()->SetEngineState(AzaliaDmaEngine::STATE_STOP));
                UINT64 clockBefore, clockAfter;
                CHECK_RC(m_pAza->RegRd(WALCLK, reinterpret_cast<UINT32*>(&clockBefore)));

                CHECK_RC(pStream->GetEngine()->GetEngineState(&state));
                while(state != AzaliaDmaEngine::STATE_STOP)
                {
                    CHECK_RC(m_pAza->RegRd(WALCLK, reinterpret_cast<UINT32*>(&clockAfter)));
                    CHECK_RC(pStream->GetEngine()->GetEngineState(&state));
                    UINT32 elapsedTime = (clockAfter - (clockBefore % (((UINT64)1)<<32)));
                    if (elapsedTime > (2 * 500 * AzaliaUtilities::GetTimeScaler()))
                    {
                        Printf(Tee::PriError, "Stream %d, did not enter paused state!!\n",
                               m_vStreamIndices[i]);

                        return RC::AZATEST_FAIL;
                    }
                }

                AzaliaUtilities::Pause(500); // 0.5 seconds, large enough to hear pause

                // Make sure fifo error did not occur
                bool fifoErr = false;
                UINT08 sNumber;
                CHECK_RC(pStream->GetEngine()->GetStreamNumber(&sNumber));
                CHECK_RC(m_pAza->IntQuery(AzaliaController::INT_STREAM_FIFO_ERR, sNumber, &fifoErr));

                if (fifoErr)
                {
                    Printf(Tee::PriError, "Fifo error oclwred while stream %d was paused!!\n",
                           m_vStreamIndices[i]);

                    return RC::AZATEST_FAIL;
                }

                // Because we paysed our position will be confused. Reset things
                CHECK_RC(pStream->ResetElapsedTime());

                CHECK_RC(pStream->GetEngine()->SetEngineState(AzaliaDmaEngine::STATE_RUN));
                m_PauseResumeNotTestedYet = false;
            }
        }
    }

    for (i = 0; i < m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        CHECK_RC(pStream->UpdateAndVerifyPosition(&success));
        m_IsError |= !success;
    }

    // If it's not OK to loop the input or output buffers, stop the buffers when
    // they are near the end. Note that this algorithm isn't perfect--a
    // small part of the buffer may be left unfilled, or may be looped--but
    // it should still give the buffer enough valid data to analyze,
    // except in the case of very small buffers.
    for (i = 0; i < m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
#ifdef SIM_BUILD
        // Print a handy status message while running the test on
        // simulation, just so it's clear that the test is still active.
        Printf(Tee::PriDebug, "Stream %d is at position %d\n",
               m_vStreamIndices[i], pStream->GetLastPosition());
#endif
        if ((!m_LoopInput && pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT) ||
            (!m_LoopOutput && pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT))
        {
            AzaliaDmaEngine::STATE state;
            CHECK_RC(pStream->GetEngine()->GetEngineState(&state));
            if (state != AzaliaDmaEngine::STATE_STOP)
            {
                if (ShouldBeStopped(m_vStreamIndices[i], pStream->GetLastPosition()))
                {
                    Printf(Tee::PriDebug, "Stopping stream %d near position %d\n",
                           m_vStreamIndices[i], pStream->GetLastPosition());
                    CHECK_RC(pStream->GetEngine()->SetEngineState(AzaliaDmaEngine::STATE_STOP));
                }
                else
                {
                    localFinished = false;
                }
            }
        }
    }

    if (!m_IsFinished && !m_RunUntilStopped)
    {
        m_IsFinished = localFinished;
    }

    return OK;
}

RC AzaliaStreamTestModule::Stop()
{
    RC rc;
    UINT32 i;

    if (!m_IsEnabled) return OK;

    // Set sync bits for sync'd streams
    UINT32 syncval = 0;

    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.sync)
        {
            UINT08 temp = 0;
            CHECK_RC(pStream->GetEngine()->GetEngineIndex(&temp));
            syncval |= 1 << temp;
        }
    }
    CHECK_RC(m_pAza->RegWr(SSYNC, syncval));

    UINT64 clock;
    CHECK_RC(m_pAza->RegRd(WALCLK, reinterpret_cast<UINT32*>(&clock)));

    // Verify positions and update loop counters for sync'd streams
    // in preparation for loopback check
    bool success = true;
    bool localSuccess = true;
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.sync)
        {
            CHECK_RC(pStream->UpdateAndVerifyPosition(&localSuccess));
            success &= localSuccess;
        }
    }
    if (!success)
    {
        Printf(Tee::PriError, "Error in running stream\n");
        return RC::AZATEST_FAIL;
    }

    // Verify that sync'd streams have stopped and non-sync'd streams are
    // still running. Use register positions only.
    CHECK_RC(VerifyStreamsRunning(false, true));

    // Verify that the sync'd streams (which are stopped) have their
    // fifo ready bit set.
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.sync)
        {
            CHECK_RC(POLLWRAP_HW((bool (*)(void*))AzaliaDmaEngine::PollIsFifoReady,
                                  pStream->GetEngine(), Tasker::GetDefaultTimeoutMs()));
        }
    }

    // Turn off DMA for each stream that's enabled, including sync'd streams
    success = true;
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        CHECK_RC(pStream->GetEngine()->SetEngineState(AzaliaDmaEngine::STATE_STOP));
        if (!pStream->input.sync)
        {
            CHECK_RC(pStream->UpdateAndVerifyPosition(&localSuccess));
            success &= localSuccess;
        }
        // Verify position and update loop counts for non-sync'd streams in
        // preparation for loopback checking.
    }

    // Give streams a chance to stop
    AzaliaUtilities::Pause(1);

    // Verify that all streams are stopped
    CHECK_RC(VerifyStreamsRunning(false, false));

    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        AzaliaController::IntStreamStatus strmsts;
        UINT08 engineIndex;
        CHECK_RC(pStream->GetEngine()->GetEngineIndex(&engineIndex));
        CHECK_RC(m_pAza->IntStreamGetStatus(engineIndex, &strmsts));
        CHECK_RC(m_pAza->IntStreamClearStatus(engineIndex));
        if (strmsts.dese)
        {
            Printf(Tee::PriError, "Descriptor error detected at shutdown for stream %d\n",
                   m_vStreamIndices[i]);
            return RC::AZATEST_FAIL;
        }
        if (strmsts.fifoe)
        {
            Printf(Tee::PriError, "Fifo error detected at shutdown for stream %d\n",
                   m_vStreamIndices[i]);
            return RC::AZATEST_FAIL;
        }
        if (strmsts.bcis)
        {
            Printf(Tee::PriError, "Unexpected IOC at shutdown for stream %d\n",
                   m_vStreamIndices[i]);
            return RC::AZATEST_FAIL;
        }
        if(pStream->FifoErrOclwrred())
        {
            return RC::AZATEST_FAIL;
        }
        if(pStream->StreamGotTruncated())
        {
            return RC::AZATEST_FAIL;
        }
        if(pStream->DescErrorOclwrred())
        {
            return RC::AZATEST_FAIL;
        }
    }

    // Clear the sync register
    CHECK_RC(m_pAza->RegWr(SSYNC, 0));

    // Initiate a flush
    // --Flush is very poorly dolwmented in the HDA spec and there is
    //   no evidence that flushes do anything on ICH6!
    // --Lwpu's implementation does execute a flush.
    // --The code below that surrounds the actual Flush will verify that
    //   any with any observable flush behavior, the samples written are
    //   consistent with changes in link position.
    if (!m_pAza->IsGpuAza())
    {
        const UINT32 numSamplesBefore = 15;
        const UINT32 numSamplesAfter = 2;
        const UINT32 arraySizeInSamples = numSamplesBefore + numSamplesAfter + 1;
        UINT32 position[MAX_NUM_TEST_STREAMS];
        UINT32 dmaPosition[MAX_NUM_TEST_STREAMS];

        // use floats to avoid format colwersions in printfs and checks
        vector<FLOAT32>bufferValues [MAX_NUM_TEST_STREAMS];

        for (UINT32 stream=0; stream < m_vStreamIndices.size(); ++stream)
        {
            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[stream], &pStream));

            // Get Positions
            position[stream] = pStream->GetEngine()->GetPosition();

            if (m_DmaPos)
                dmaPosition[stream] = pStream->GetEngine()->GetPositiolwiaDma();

            // Get last few samples written to buffer
            if (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT)
            {
                UINT32 bufSize = pStream->GetBufferLengthInBytes();
                AzaliaFormat* pFormat = pStream->GetFormat();
                UINT32 bytesPerSample = pFormat->GetSizeInBytes();

                UINT32 startSampleInd = 0;
                if (position[stream] > numSamplesBefore*bytesPerSample)
                    startSampleInd = position[stream]/bytesPerSample - numSamplesBefore;

                UINT32 endSampleInd = position[stream]/bytesPerSample + numSamplesAfter + 1;
                if (endSampleInd > bufSize/bytesPerSample)
                    endSampleInd = bufSize/bytesPerSample;

                UINT32 numSamplesToFetch = endSampleInd - startSampleInd + 1;
                if (numSamplesToFetch > arraySizeInSamples)
                    numSamplesToFetch = arraySizeInSamples;

                UINT32 nSamplesRead = 0;

                vector<INT08> intSamples08(numSamplesToFetch*
                                           pFormat->GetSizeInBytes());

                CHECK_RC(pStream->GetBdl()->ReadSampleList(startSampleInd,
                                                           &intSamples08[0],
                                                           numSamplesToFetch,
                                                           &nSamplesRead,
                                                           pFormat->GetSizeInBytes()));

                bufferValues[stream].resize(nSamplesRead);
                CHECK_RC(pStream->ColwertSamplesToFloats(bufferValues[stream],
                                                         intSamples08,
                                                         nSamplesRead));

            }
        }

        CHECK_RC(FlushFifos());

        for (UINT32 stream=0; stream < m_vStreamIndices.size(); ++stream)
        {
            // Check for position changes
            // For input streams, lpib can change up to 3 bytes after flush
            // on the LWPU controller. This isn't idea but it is tolerable

            AzaStream *pStream;
            CHECK_RC(m_pAza->GetStream(m_vStreamIndices[stream], &pStream));
            UINT32 newPos = pStream->GetEngine()->GetPosition();
            if ((pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT &&
                 (newPos < position[stream] || newPos > position[stream] + 3))
                ||
                (pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT &&
                 (position[stream] != newPos)))
            {
                Printf(Tee::PriError, "Stream %d position(LPIB) changed from 0x%08x to 0x%08x\n",
                       m_vStreamIndices[stream],
                       position[stream],
                       newPos);

                success = false;
            }

            UINT32 newDmaPos = 0;
            if (m_DmaPos && !m_pAza->IsIch6())
            {
                INT32 loopCnt = 0;
                newDmaPos = pStream->GetEngine()->GetPositiolwiaDma();

                while (newDmaPos != newPos)
                {
                    AzaliaUtilities::Pause(1);
                    loopCnt++;

                    if (loopCnt > 10)
                    {
                        Printf(Tee::PriError, "Timeout waiting for DmaPosition to match LPIB.\n");
                        Printf(Tee::PriError, "After flush, Stream %d dma position (0x%08x) does"
                                              "not match LPIB (0x%08x)!\n",
                               m_vStreamIndices[stream],
                               newDmaPos,
                               newPos);

                        success = false;
                        break;
                    }

                    newDmaPos = pStream->GetEngine()->GetPositiolwiaDma();
                }

                if (dmaPosition[stream] != newDmaPos)
                {
                    // Not an error
                    Printf(Tee::PriDebug, "Stream %d dma position changed from 0x%08x to 0x%08x\n",
                           m_vStreamIndices[stream], dmaPosition[stream], newDmaPos);
                }
            }

            // Check for changes in the last few samples written to the buffer
            // -- I haven't observed any for ICH6 so I'm not sure what is
            // correct behavior. This will indicate that flush is doing
            // something, at which time we can add additional checks
            //
            // NOTE: In the case of testing fifo errors, we skip this since
            // disabling bus-mastering can cause the stream position to be
            // slightly different
            if (pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT &&
                pStream->input.testFifoErrors == false)
            {
                UINT32 bufSize = pStream->GetBufferLengthInBytes();
                AzaliaFormat* pFormat = pStream->GetFormat();
                UINT32 bytesPerSample = pFormat->GetSizeInBytes();

                UINT32 startSampleInd = 0;
                if (position[stream] > numSamplesBefore*bytesPerSample)
                {
                    startSampleInd = position[stream]/bytesPerSample - numSamplesBefore;
                }
                UINT32 actualNumSamplesBefore = position[stream]/bytesPerSample - startSampleInd;

                UINT32 endSampleInd = position[stream]/bytesPerSample + numSamplesAfter + 1;
                if (endSampleInd > bufSize/bytesPerSample)
                    endSampleInd = bufSize/bytesPerSample;

                UINT32 numSamplesToFetch = endSampleInd - startSampleInd + 1;
                if (numSamplesToFetch > arraySizeInSamples)
                    numSamplesToFetch = arraySizeInSamples;

                UINT32 nSamples = 0;
                vector<FLOAT32> tempBufferValues(arraySizeInSamples);
                vector<INT08> intSamples08(numSamplesToFetch*
                                           pFormat->GetSizeInBytes());

                CHECK_RC(pStream->GetBdl()->ReadSampleList(startSampleInd,
                                                          &intSamples08[0],
                                                          numSamplesToFetch,
                                                          &nSamples,
                                                          pStream->GetFormat()->GetSizeInBytes()));

                CHECK_RC(pStream->ColwertSamplesToFloats(tempBufferValues,
                                                         intSamples08,
                                                         numSamplesToFetch));

                for (UINT32 cnt=0; cnt < nSamples; ++cnt)
                {
                    INT32 posRelativeCnt = cnt - actualNumSamplesBefore;
                    if (tempBufferValues[cnt] != bufferValues[stream][cnt])
                    {
                        // Make sure we don't overwrite data we've already
                        // written
                        if (posRelativeCnt <= -pStream->GetEngine()->GetFifoSize())
                        {
                            Printf(Tee::PriError, "Stream %d sample %d (relative to stop"
                                                  "postion) changed from %f to %f. This is"
                                                  "not reasonable.\n",
                                   m_vStreamIndices[stream],
                                   posRelativeCnt,
                                   bufferValues[stream][cnt],
                                   tempBufferValues[cnt]);

                            if (m_DmaPos)
                            {
                                Printf(Tee::PriHigh, "DMA position before is 0x%08x, "
                                                     "position after is 0x%08x\n",
                                       dmaPosition[stream],
                                       newDmaPos);
                            }

                            success = false;
                        }
                        else if (posRelativeCnt <= 0)
                        {
                            Printf(Tee::PriLow, "Stream %d sample %d (relative to stop "
                                                "position) changed from %f to %f. This "
                                                "is reasonable.\n",
                                   m_vStreamIndices[stream],
                                   posRelativeCnt,
                                   bufferValues[stream][cnt],
                                   tempBufferValues[cnt]);
                        }

                        // Make sure we don't write past the new position value
                        if (posRelativeCnt > (int)((newPos - position[stream])/bytesPerSample))
                        {
                            Printf(Tee::PriError, "Stream %d sample %d (relative to stop position) "
                                                  "changed from %f to %f\n",
                                   m_vStreamIndices[stream],
                                   posRelativeCnt,
                                   bufferValues[stream][cnt],
                                   tempBufferValues[cnt]);

                            Printf(Tee::PriHigh, "This is beyond the new position!\n");
                            Printf(Tee::PriHigh, "LPIB Position before is 0x%08x, position "
                                                 "after is 0x%08x\n",
                                    position[stream], newPos);

                            if (m_DmaPos)
                            {
                                Printf(Tee::PriHigh, "DMA position before is 0x%08x, position "
                                                     "after is 0x%08x\n",
                                        dmaPosition[stream], newDmaPos);
                            }

                            pStream->GetBdl()->Print();
                            success = false;
                        }
                    }
                }
            }
        }
    }
    else
        CHECK_RC(FlushFifos());

    if (!m_SkipLbCheck)
    {
        // Check loopbacks
        CHECK_RC(CheckLoopbacks());
    }

    // Dump the input buffers to a file
    string filename = "";
    for (UINT32 stream = 0; stream < m_vStreamIndices.size(); ++stream)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[stream], &pStream));

        filename = pStream->input.filename;
        if (!filename.empty())
        {
            CHECK_RC(pStream->GetBdl()->ReadBufferToFile(filename.c_str(),
                                                         pStream->GetFormat()->GetSizeInBytes()));
        }
    }

    if (!success)
        return RC::AZATEST_FAIL;

    return OK;
}

RC AzaliaStreamTestModule::RouteStreams()
{
    RC rc;
    UINT32 i;
    bool success = false;

    // In general, route input streams first because there tend to
    // be fewer input colwerters, so the possible routing schemes for
    // input streams are more restrictive.

    // First route reserved input streams
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.reservation.reserved &&
            pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT)
        {
            if (m_pAza->IsGpuAza())
            {
                Printf(Tee::PriDebug, "GPU input streams don't need to be routed.\n");
                pStream->input.routingRequirements = StreamProperties::MUST_BE_UNROUTED;
            }
            CHECK_RC(pStream->FindRoute(&success));
            if (!success)
            {
                Printf(Tee::PriError, "Cannot find route for stream %d\n",
                       m_vStreamIndices[i]);
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
        }
    }

    // Then route reserved output streams
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (pStream->input.reservation.reserved &&
            pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT)
        {
            if (m_pAza->IsGpuAza())
            {
                Printf(Tee::PriDebug, "All channels of GPU Output stream should be routed.\n");
                pStream->input.routingRequirements = StreamProperties::MUST_ROUTE_ALL_CHANNELS;
            }

            CHECK_RC(pStream->FindRoute(&success));
            if (!success)
            {
                Printf(Tee::PriError, "Cannot find route for stream %d\n",
                       m_vStreamIndices[i]);
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
        }
    }

    // Then route other input streams
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (!pStream->input.reservation.reserved &&
            pStream->GetDir() == AzaliaDmaEngine::DIR_INPUT)
        {
            if (m_pAza->IsGpuAza())
            {
                Printf(Tee::PriDebug, "GPU input streams don't need to be routed.\n");
                pStream->input.routingRequirements = StreamProperties::MUST_BE_UNROUTED;
            }

            CHECK_RC(pStream->FindRoute(&success));
            if (!success)
            {
                Printf(Tee::PriError, "Cannot find route for stream %d\n",
                       m_vStreamIndices[i]);
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
        }
    }

    // Finally route other output streams
    for (i=0; i<m_vStreamIndices.size(); i++)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        if (!pStream->input.reservation.reserved &&
            pStream->GetDir() == AzaliaDmaEngine::DIR_OUTPUT)
        {
            if (m_pAza->IsGpuAza())
            {
                Printf(Tee::PriDebug, "All channels of GPU Output stream should be routed.\n");
                pStream->input.routingRequirements = StreamProperties::MUST_ROUTE_ALL_CHANNELS;
            }
            CHECK_RC(pStream->FindRoute(&success));
            if (!success)
            {
                Printf(Tee::PriError, "Cannot find route for stream %d\n",
                       m_vStreamIndices[i]);
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
        }
    }

    return OK;
}

RC AzaliaStreamTestModule::CheckLoopbacks()
{
    RC myRc, rc;
    for (UINT32 i=0; i<m_NActiveLoopbacks; i++)
    {
        Printf(Tee::PriDebug, "Checking loopback between stream %d, channel %d and stream "
                              "%d, channel %d\n",
               m_Loopbacks[i].srcSIndex, m_Loopbacks[i].srcChan,
               m_Loopbacks[i].destSIndex, m_Loopbacks[i].destChan);

        AzaStream *pSrcStream, *pDstStream;
        CHECK_RC(m_pAza->GetStream(m_Loopbacks[i].srcSIndex, &pSrcStream));
        CHECK_RC(m_pAza->GetStream(m_Loopbacks[i].destSIndex, &pDstStream));
        myRc = pSrcStream->CheckLoopback(m_Loopbacks[i].srcChan,
                                         pDstStream,
                                         m_Loopbacks[i].destChan);
        if (myRc != OK)
        {
            m_IsError = true;
        }
    }
    return myRc;
}

RC AzaliaStreamTestModule::ClearTestStreams()
{
    m_vStreamIndices.clear();
    return OK;
}

RC AzaliaStreamTestModule::AddTestStream(UINT32 index)
{
    m_vStreamIndices.push_back(index);
    return OK;
}

RC AzaliaStreamTestModule::GetStream(UINT32 Index, AzaStream **pStrm)
{
    if (Index >= MAX_NUM_TEST_STREAMS)
    {
        Printf(Tee::PriError, "Invalid stream number requested\n");
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    if (pStrm)
        CHECK_RC(m_pAza->GetStream(Index, pStrm));

    return OK;
}

RC AzaliaStreamTestModule::CreateLoopback(UINT32 SrcStreamIndex, UINT32 SrcChan,
                                          UINT32 DestStreamIndex, UINT32 DestChan)
{
    if (m_NActiveLoopbacks >= MAX_NUM_LOOPBACKS)
    {
        Printf(Tee::PriError, "Maximum number of loopbacks already created\n");
        return RC::SOFTWARE_ERROR;
    }
    m_Loopbacks[m_NActiveLoopbacks].srcSIndex = SrcStreamIndex;
    m_Loopbacks[m_NActiveLoopbacks].srcChan = SrcChan;
    m_Loopbacks[m_NActiveLoopbacks].destSIndex = DestStreamIndex;
    m_Loopbacks[m_NActiveLoopbacks].destChan = DestChan;
    m_NActiveLoopbacks++;
    return OK;
}

RC AzaliaStreamTestModule::FlushFifos()
{
    RC rc;

    UINT32 i;
    AzaliaDmaEngine::STATE engState;
    // Make sure all streams are stopped
    for (i=0; i<m_vStreamIndices.size(); ++i)
    {
        AzaStream *pStream;
        CHECK_RC(m_pAza->GetStream(m_vStreamIndices[i], &pStream));
        CHECK_RC(pStream->GetEngine()->GetEngineState(&engState));
        if (engState == AzaliaDmaEngine::STATE_RUN)
        {
            Printf(Tee::PriError, "Cannot perform flush because stream %d is running!\n", i);
            return RC::AZA_STREAM_ERROR;
        }
    }

    // Make sure Flush Status is already clear
    UINT32 value;
    CHECK_RC(m_pAza->RegRd(GSTS, &value));

    if (FLD_TEST_RF_DEF(AZA_GSTS_FSTS, _FLUSH, value))
    {
        // Clear the flush status bit
        Printf(Tee::PriHigh, "Clearing flush status before initiating flush.\n");
        CHECK_RC(m_pAza->RegWr(GSTS, RF_DEF(AZA_GSTS_FSTS, _FLUSH)));

        // Verify the clear
        CHECK_RC(m_pAza->RegRd(GSTS, &value));
        if (FLD_TEST_RF_DEF(AZA_GSTS_FSTS, _FLUSH, value))
        {
            Printf(Tee::PriError, "Cannot clear flush status before initiating flush!\n");
            return RC::AZA_STREAM_ERROR;
        }
    }

    // Initiate the flush
    UINT32 value32;
    CHECK_RC(m_pAza->RegRd(GCTL, &value32));
    value32 = FLD_SET_RF_DEF(AZA_GCTL_FCNTRL, _INITIATE, value32);
    CHECK_RC(m_pAza->RegWr(GCTL, value32));

    // Wait until the flush is complete
    AzaRegPollArgs pollArgs;
    pollArgs.pAzalia = m_pAza;
    pollArgs.reg = GSTS;
    pollArgs.mask = RF_SHIFTMASK(AZA_GSTS_FSTS);
    pollArgs.value = RF_DEF(AZA_GSTS_FSTS, _FLUSH);
    rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, Tasker::GetDefaultTimeoutMs() * 10);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Timeout waiting for Crush19 Azalia controller "
                              "for completing a flush!");
        return RC::TIMEOUT_ERROR;
    }

    // Clear flush status
    if (m_pAza->IsIch6())
        AzaliaUtilities::Pause(1); // I don't know why, but this is needed for
                                   // the following to work on ICH6

    CHECK_RC(m_pAza->RegWr(GSTS, RF_DEF(AZA_GSTS_FSTS, _FLUSH)));

    // Verify clear of status
    pollArgs.pAzalia = m_pAza;
    pollArgs.reg = GSTS;
    pollArgs.mask = RF_SHIFTMASK(AZA_GSTS_FSTS);
    pollArgs.value = RF_DEF(AZA_GSTS_FSTS, _NOFLUSH);
    rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, Tasker::GetDefaultTimeoutMs());
    if (rc != OK)
    {
        Printf(Tee::PriError, "Timeout waiting for flush status to clear (0x%04x)!\n",
               static_cast<unsigned>(RF_VAL(AZA_GSTS_FSTS, value)));
        return RC::TIMEOUT_ERROR;
    }

    // Make sure flush control bit has cleared
    // --actually the spec isn't clear on whether the flush control bit
    // should clear automatically or not. Both Lwpu's and Intel's designs
    // do though.
    CHECK_RC(m_pAza->RegRd(GCTL, &value32));
    if (FLD_TEST_RF_DEF(AZA_GCTL_FCNTRL, _INITIATE, value32))
    {
        Printf(Tee::PriError, "Flush Control bit didn't clear after flush!\n");
        return RC::AZA_STREAM_ERROR;
    }
    return OK;
}
