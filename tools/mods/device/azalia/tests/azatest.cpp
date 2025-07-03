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

#include "azatest.h"
#include "azarbtm.h"
#include "azasttm.h"
#include "alsasttm.h"

#include "device/azalia/azacdc.h"
#include "device/azalia/azactrl.h"
#include "device/azalia/azactrlmgr.h"
#include "device/azalia/azareg.h"
#include "core/include/platform.h"
#include "core/include/xp.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaTest"

#define DMA_TEST_TIMEOUT (60 * 1000000) // 60 seconds

AzaliaTest::AzaliaTest()
{
    m_AzaDevIndex = 0xff;
    m_pAza = NULL;
    m_RBTest = NULL;
    m_STest = NULL;
    m_RBTest = new AzaliaRingBufferTestModule();
    m_STest = new AzaliaStreamTestModule();
    MASSERT(m_RBTest && m_STest);
    SetDefaultParameters();
}

AzaliaTest::~AzaliaTest()
{
    Uninitialize();
    delete m_RBTest;
    delete m_STest;
}

RC AzaliaTest::Initialize_Private()
{
    RC rc;
    LOG_ENT();
    if (m_pTestObj == NULL)
    {
        Printf(Tee::PriError, "Call SetJSObject() first\n");
        return RC::ILWLIAD_TEST_JS_OBJECT;
    }

    CHECK_RC( InitProperties() );

    // Setup the random number generator
    AzaliaUtilities::SeedRandom(m_Seed);

    m_AzaDevIndex = (m_AzaDevIndex != 0xff) ? m_AzaDevIndex : 0;
    CHECK_RC(AzaliaControllerMgr::Mgr()->Get(m_AzaDevIndex, reinterpret_cast<Controller**>(&m_pAza)));

    if (!m_pAza)
    {
        Printf(Tee::PriError, "No azalia device index %d!\n", m_AzaDevIndex);
        return RC::DEVICE_NOT_FOUND;
    }

    CHECK_RC(Reset());

    AzaliaCodec* pCodec;
    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        if ((m_pAza->GetCodec(i, &pCodec) == OK) &&
            (pCodec != NULL))
        {
            CHECK_RC(pCodec->PowerUpAllWidgets());
        }
    }

    m_IsInit=true;
    LOG_EXT();
    return OK;
}

RC AzaliaTest::InitProperties_Private()
{
    return OK;
}

RC AzaliaTest::Uninitialize_Private()
{
    RC rc = OK;
    CHECK_RC(SetDefaultParameters());

    m_IsInit = false;

    return rc;
}

RC AzaliaTest::Reset()
{
    RC rc;
    CHECK_RC(DmaTestReset());
    return OK;
}

RC AzaliaTest::SetStreamDefaults(StreamProperties::strmProp* pStream)
{

    MASSERT(pStream != NULL);
    pStream->channels = AzaliaFormat::CHANNELS_2;
    pStream->size = AzaliaFormat::SIZE_16;
    pStream->rate = AzaliaFormat::RATE_48000;
    pStream->type = AzaliaFormat::TYPE_PCM;
    pStream->tp = AzaliaDmaEngine::TP_HIGH;
    pStream->nBDLEs = 0;
    pStream->minBlocks = 0;
    pStream->maxBlocks = 0;
    pStream->sync = false;
    pStream->outputAttenuation = 0.0;
    pStream->maxHarmonics = 0;
    pStream->clickThreshPercent = 100;
    pStream->smoothThreshPercent = 100;
    pStream->randomData = false;
    pStream->sdoLines = AzaliaDmaEngine::SL_ONE;
    pStream->codingType = 0;
    pStream->routingRequirements = StreamProperties::MUST_ROUTE_ALL_CHANNELS;
    pStream->reservation.reserved = 0;
    pStream->reservation.codec = 0;
    pStream->reservation.pin = 0;
    pStream->streamNumber = RANDOM_STRM_NUMBER;
    pStream->allowIocs = false;
    pStream->intEn = true;
    pStream->fifoEn = true;
    pStream->descEn = false;
    pStream->iocEn = false;
    pStream->formatSubstOk = false;
    pStream->testDescErrors = false;
    pStream->testFifoErrors = false;
    pStream->testPauseResume = false;
    pStream->truncStream = false;
    pStream->failIfNoData = true;
    pStream->testOutStrmBuffMon = false;
    pStream->filename = "";

    return OK;
}

RC AzaliaTest::SetDefaultParameters()
{
    RC rc;
    m_Loops = 1;
    m_DmaTestInfo.duration = 2;
    m_Global_IntEn = false;
    m_Pci_IntEn = false;
    m_Cntrl_IntEn = false;
    m_Cme_IntEn = false;

    if (Xp::GetOperatingSystem() == Xp::OS_LINUX)
    {
        m_CorbCoh = true;
        m_RirbCoh = true;
        m_BdlCoh = true;
        m_InputStreamCoh = true;
        m_OutputStreamCoh = true;
        m_DmaPosCoh = true;
    }

    m_Seed = 0x4321;
    m_SimOnSilicon = false;
    for (UINT32 i = 0; i < 10; ++i)
        CHECK_RC(SetStreamDefaults(&m_streams[i]));

    CHECK_RC(m_RBTest->SetDefaults());
    CHECK_RC(m_STest->SetDefaults());
    return OK;
}

RC AzaliaTest::PrintProperties(Tee::Priority Pri)
{
    RC rc;
    CHECK_RC(m_STest->PrintInfo(Pri, true, false));
    CHECK_RC(m_RBTest->PrintInfo(Pri, true, false));
    return OK;
}

RC AzaliaTest::PrintInfo_Private()
{
    return OK;
}

RC AzaliaTest::VerifyDmaTest(Tee::Priority pri)
{
    RC rc = OK;
    RC myRc = OK;
    LOG_ENT();

    bool isFinished = false;
    UINT64 sleep = 0;
    while (!isFinished)
    {
#ifndef SIM_BUILD
        if (sleep*1000 > DMA_TEST_TIMEOUT)
        {
            Printf(Tee::PriError, "AzaliaTest::DmaTest timeout error!\n");
            rc = RC::TIMEOUT_ERROR;
            break;
        }
        if (m_DmaTestInfo.duration &&
            (sleep*1000 > ((UINT64)m_DmaTestInfo.duration * 1000000)))
        {
            Printf(Tee::PriDebug, "Duration passed, shutting down\n");
            break;
        }
#else
        if (!m_SimOnSilicon)
        {
            if (m_DmaTestInfo.duration)
            {
                Printf(Tee::PriError, "Tests running on simulation cannot have a duration set\n");
                return RC::BAD_PARAMETER;
            }
        }
#endif

        AzaliaUtilities::Pause(1);
        sleep += AzaliaUtilities::GetTimeScaler();
        CHECK_RC(DmaTestPoll(&isFinished));
    }
    CHECK_RC(DmaTestStop());
    myRc = DmaTestCheck();

    CHECK_RC(DmaTestPrintInfo(pri, false, true));

#ifndef SIM_BUILD
    LOG_EXT();
#else
    // This is a workaround for GT21x so that we give it enough time to
    // clear all writes from the controller to the xve.
    CHECK_RC(m_pAza->DmaPosToggle(false));
    AzaliaUtilities::Pause(1);
    CHECK_RC(m_pAza->Reset(true));
    if (pri >= Tee::PriNormal)
    {
        if(OK == myRc)
        {
            Printf(Tee::PriHigh, "$$$ PASSED $$$");
        }
        else
        {
            Printf(Tee::PriHigh, "*** FAILED ***");
        }
        Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);
    }
#endif

    return myRc;
}

RC AzaliaTest::DmaTest(Tee::Priority pri)
{
    RC myRC = OK;
    RC rc = OK;
    LOG_ENT();

    if(!m_IsInit)
    {
        Printf(Tee::PriError, "No azalia device index %d!\n", m_AzaDevIndex);
        return RC::DEVICE_NOT_FOUND;
    }

#ifdef SIM_BUILD
    if (!m_SimOnSilicon)
    {
        CHECK_RC(DmaTestPrintInfo(pri, true, false));
        CHECK_RC(DmaTestStart());
        Printf(Tee::PriNormal, "%s", "Dma Test Has been Started");
        LOG_EXT();
    }

    // In SIM_BUILD over silicon, run test for 'm_Loops' loops
    else
#endif
    {
        for (UINT32 loops = 0; loops < m_Loops; loops++)
        {
            if (loops != 0)
            {
                CHECK_RC(DmaTestReset());
            }
            CHECK_RC(DmaTestPrintInfo(pri, true, false));

            CHECK_RC(DmaTestStart());

            myRC = VerifyDmaTest(pri);

            if (myRC != OK) break;
        }

        if (pri >= Tee::PriNormal)
        {
            if(OK == myRC)
            {
                Printf(Tee::PriHigh, "$$$ PASSED $$$");
            }
            else
            {
                Printf(Tee::PriHigh, "*** FAILED ***");
            }
            Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);
        }

        LOG_EXT();
    }

    return myRC;
}

RC AzaliaTest::DmaTestPrintInfo(Tee::Priority pri, bool printInput, bool printStatus)
{
    RC rc;
    if (printInput)
    {
        Printf(pri, "\nDma Test: %d loops, duration %d sec\n", m_Loops, m_DmaTestInfo.duration);
    }
    if(m_pAza->GetIsUseMappedPhysAddr())
    {
        //physical to virtual not support for pcie mapped address
        return OK;
    }
    CHECK_RC(m_RBTest->PrintInfo(pri, printInput, printStatus));
    CHECK_RC(m_STest->PrintInfo(pri, printInput, printStatus));
    return OK;
}

RC AzaliaTest::DmaTestCheck()
{
    // If the test was not duration-based and one or more
    // modules are not finished, it's an error
    if (!m_DmaTestInfo.duration)
    {
        if (!m_RBTest->IsFinished())
        {
            Printf(Tee::PriError, "RB test did not finish!\n");
            return RC::SOFTWARE_ERROR;
        }
        if (!m_STest->IsFinished())
        {
            Printf(Tee::PriError, "Stream test did not finish!\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    // If any module has an error, the test fails
    if (m_RBTest->IsError())
    {
        Printf(Tee::PriError, "RB test had an error\n");
        return RC::AZA_RING_BUFFER_ERROR;
    }
    if (m_STest->IsError())
    {
        Printf(Tee::PriError, "Stream test had an error\n");
        return RC::AZA_STREAM_ERROR;
    }
    if (m_DmaTestInfo.error)
    {
        Printf(Tee::PriError, "DMA test had an error\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC AzaliaTest::DmaTestPoll(bool* isFinished)
{
    RC rc;
    *isFinished = false;
    bool localFinished = true;

    CHECK_RC(m_RBTest->Continue());
    localFinished &= m_RBTest->IsFinished();

    CHECK_RC(m_STest->Continue());
    localFinished &= m_STest->IsFinished();

    *isFinished = localFinished;
    return OK;
}

RC AzaliaTest::DmaTestReset()
{
    RC rc = OK;

    // Reset DMA test info
    m_DmaTestInfo.startTime = 0;
    m_DmaTestInfo.error = false;

    // Reset ring buffer test info
    if (m_RBTest)
    {
        CHECK_RC(m_RBTest->Reset(m_pAza));
    }
    if (m_STest)
    {
        CHECK_RC(m_STest->ClearTestStreams());

        UINT32 maxNumStreams = 0;
        CHECK_RC(m_pAza->GetMaxNumStreams(&maxNumStreams));

        for (UINT32 i=0; i<maxNumStreams; ++i)
        {
            AzaStream* pStream = NULL;
            CHECK_RC(m_pAza->GetStream(i, &pStream));
            CHECK_RC(pStream->SetStreamProp(&m_streams[i]));
            if(pStream->IsReserved())
            {
                CHECK_RC(m_STest->AddTestStream(i));
            }
        }

        CHECK_RC(m_STest->Reset(m_pAza));
    }
    return OK;
}

/*
RC AzaliaTest::SetupBufBuild()
{
    m_BufBuild.p_NumMin = 2;    // Max number of buffers - Number of frags
    m_BufBuild.p_NumMax = 2;    // Min number of buffers - number of frags
    m_BufBuild.p_SizeMin = 2;   // Min size for each buffer
    m_BufBuild.p_SizeMax = 2;   // Max size for each buffer
    m_BufBuild.p_OffMin = 2;    // Min offset for each buffer
    m_BufBuild.p_OffMax = 2;    // Max offset for each buffer
    m_BufBuild.p_DataWidth = 2;
    m_BufBuild.p_IsShuffle = false;
    m_BufBuild.p_Align = 128;   // Alignment
}
*/
RC AzaliaTest::DmaTestStart()
{
    RC rc;
    Printf(Tee::PriLow, "Starting Azalia DMA test\n");
    m_DmaTestInfo.startTime = Platform::GetTimeUS();

    // Handle global interrupt status here based on input parameters
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_GLOBAL, m_Global_IntEn));
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_CONTROLLER, m_Cntrl_IntEn));
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_CORB_MEM_ERR, m_Cme_IntEn));

    // Assign coherence to all memory traffic types
    CHECK_RC(m_pAza->ToggleBdlCoherence(m_BdlCoh));
    CHECK_RC(m_pAza->StreamToggleInputCoherence(m_InputStreamCoh));
    CHECK_RC(m_pAza->StreamToggleOutputCoherence(m_OutputStreamCoh));
    CHECK_RC(m_pAza->CorbToggleCoherence(m_CorbCoh));
    CHECK_RC(m_pAza->RirbToggleCoherence(m_RirbCoh));
    CHECK_RC(m_pAza->DmaPosToggleCoherence(m_DmaPosCoh));

    CHECK_RC(m_RBTest->Start());
    CHECK_RC(m_STest->Start());
    return OK;
}

RC AzaliaTest::DmaTestStop()
{
    RC rc;
    Printf(Tee::PriLow, "Stopping Azalia DMA test\n");
    CHECK_RC(m_RBTest->Stop());
    CHECK_RC(m_STest->Stop());
    return OK;
}

RC AzaliaTest::SetStreamInput(UINT32 stream, UINT32 type, string value, UINT08 dev)
{
    m_streams[stream].filename = value;
    return OK;
}

RC AzaliaTest::SetStreamInput(UINT32 stream, UINT32 type, UINT32 value, UINT08 dev)
{
    switch ((AzaStream::InputArgType) type)
    {
        case AzaStream::IARG_CHANNELS:
            m_streams[stream].channels = AzaliaFormat::ToChannels(value);
            break;
        case AzaStream::IARG_SIZE:
            m_streams[stream].size = AzaliaFormat::ToSize(value);
            break;
        case AzaStream::IARG_RATE:
            m_streams[stream].rate = AzaliaFormat::ToRate(value);
            break;
        case AzaStream::IARG_TYPE:
            m_streams[stream].type = AzaliaFormat::ToType(value);
            break;
        case AzaStream::IARG_TP:
            m_streams[stream].tp = (AzaliaDmaEngine::TRAFFIC_PRIORITY)value;
            break;
        case AzaStream::IARG_NUM_BDLES:
            m_streams[stream].nBDLEs = value;
            break;
        case AzaStream::IARG_MIN_NUM_BLOCKS:
            m_streams[stream].minBlocks = value;
            break;
        case AzaStream::IARG_MAX_NUM_BLOCKS:
            m_streams[stream].maxBlocks = value;
            break;
        case AzaStream::IARG_MAX_HARMONICS:
            m_streams[stream].maxHarmonics = value;
            break;
        case AzaStream::IARG_RANDOM_DATA:
            m_streams[stream].randomData = value;
            break;
        case AzaStream::IARG_CLICK_THRESH:
            m_streams[stream].clickThreshPercent = value;
            break;
        case AzaStream::IARG_SMOOTH_THRESH:
            m_streams[stream].smoothThreshPercent = value;
            break;
        case AzaStream::IARG_ROUTING_REQUIREMENTS:
            m_streams[stream].routingRequirements = (StreamProperties::RoutingRequirementsType) value;
            break;
        case AzaStream::IARG_STRIPE_LINES:
            m_streams[stream].sdoLines = (AzaliaDmaEngine::STRIPE_LINES)(value >> 1);
            break;
        case AzaStream::IARG_CODING_TYPE:
            m_streams[stream].codingType = value;
            break;
        case AzaStream::IARG_STRM_NUMBER:
            m_streams[stream].streamNumber = value;
            break;
        case AzaStream::IARG_IOC_SPACE:
            m_streams[stream].iocSpace = value;
            break;
        case AzaStream::IARG_CODEC:
            m_streams[stream].reservation.codec = value;
            break;

        // Booleans
        case AzaStream::IARG_ALLOW_IOCS:
            m_streams[stream].allowIocs = (bool)value;
            break;
        case AzaStream::IARG_INT_EN:
            m_streams[stream].intEn = (bool)value;
            break;
        case AzaStream::IARG_FIFO_EN:
            m_streams[stream].fifoEn = (bool)value;
            break;
        case AzaStream::IARG_DESC_EN:
            m_streams[stream].descEn = (bool)value;
            break;
        case AzaStream::IARG_IOC_EN:
            m_streams[stream].iocEn = (bool)value;
            break;
        case AzaStream::IARG_FSUB_OK:
            m_streams[stream].formatSubstOk = (bool)value;
            break;
        case AzaStream::IARG_TEST_DESC_ERRORS:
            m_streams[stream].testDescErrors = (bool)value;
            break;
        case AzaStream::IARG_TEST_PAUSE:
            m_streams[stream].testPauseResume = (bool)value;
            break;
        case AzaStream::IARG_SYNC:
            m_streams[stream].sync = (bool)value;
            break;
        case AzaStream::IARG_TRUNC_STRM:
            m_streams[stream].truncStream = (bool)value;
            break;
        case AzaStream::IARG_FAIL_IF_NODATA:
            m_streams[stream].failIfNoData = (bool)value;
            break;
        case AzaStream::IARG_TEST_OUT_STREAM_BUFF_MON:
            m_streams[stream].testOutStrmBuffMon = (bool)value;
            break;

        default:
            Printf(Tee::PriError, "Unknown stream input arg type\n");
            return RC::BAD_PARAMETER;
    }
    return OK;
}

RC AzaliaTest::CheckLoopback(UINT32 srcChan, AzaStream* pSrcStream,
                             UINT32 destChan, AzaStream* pDestStream)
{
    RC rc = OK;

    if (pSrcStream->input.randomData)
    {
        Printf(Tee::PriDebug, "Directly comparing buffers with random data\n");
        Printf(Tee::PriDebug, "This stream pos is %d, dest stream pos is %d\n",
               pSrcStream->GetLastPosition(), pDestStream->GetLastPosition());

        UINT32 nSamplesToSkip =
            (pSrcStream->GetBufferLengthInBytes() -
             pSrcStream->GetEndThresholdInBytes()) /
            (pSrcStream->GetFormat()->GetSizeInBytes());

        nSamplesToSkip = min(
            nSamplesToSkip,
            (pDestStream->GetBufferLengthInSamples() -
             pDestStream->GetEndThresholdInBytes()) /
            (pDestStream->GetFormat()->GetSizeInBytes()));

        CHECK_RC(pSrcStream->CompareBufferContents(pDestStream,
                                                   0,
                                                   nSamplesToSkip));
        return rc;
    }

    return rc;
}

RC AzaliaTest::SetReservation(UINT32 stream, UINT32 codec, UINT32 pin, UINT08 dev)
{
    m_streams[stream].reservation.reserved = true;
    m_streams[stream].reservation.codec = codec;
    m_streams[stream].reservation.pin = pin;
    return OK;
}

RC AzaliaTest::CreateLoopback(UINT32 srcStreamIndex, UINT32 srcChan,
                              UINT32 destStreamIndex, UINT32 destChan)
{
    RC rc;
    CHECK_RC(m_STest->CreateLoopback(srcStreamIndex, srcChan,
                                     destStreamIndex, destChan));
    return OK;
}

RC AzaliaTest::CreateDefaultLoopback(UINT32 codec,
                                     UINT32 srcStreamIndex, UINT32 srcChan,
                                     UINT32 destStreamIndex, UINT32 destChan)
{
    RC rc;
    m_streams[srcStreamIndex].reservation.codec = codec;
    m_streams[srcStreamIndex].reservation.reserved = true;
    m_streams[srcStreamIndex].reservation.pin = 0xffffffff;

    m_streams[destStreamIndex].reservation.codec = codec;
    m_streams[destStreamIndex].reservation.reserved = true;
    m_streams[destStreamIndex].reservation.pin = 0xffffffff;

    CHECK_RC(CreateLoopback(srcStreamIndex,srcChan,destStreamIndex,destChan));

    return OK;
}

RC AzaliaTest::DualLoopback(UINT32 srcDev, UINT32 srcCodec,
                            UINT32 srcPin, UINT32 srcStreamIndex,
                            UINT32 destDev, UINT32 destCodec,
                            UINT32 destPin, UINT32 destStreamIndex, Tee::Priority pri)
{
    RC rc;
    AzaliaController *pSrcAza;
    AzaliaController *pDestAza;
    AzaliaStreamTestModule *pSrcTestMod = NULL;
    AzaliaStreamTestModule *pDestTestMod = NULL;
    AzaStream *pSrcStrm = NULL;
    AzaStream *pDestStrm = NULL;
    bool srcFinished = false;
    bool destFinished = false;
    UINT64 sleep = 0;

    UINT32 numCtrl;
    CHECK_RC(AzaliaControllerMgr::Mgr()->GetNumControllers(&numCtrl));

    bool AlsaIsDest = (destDev == ALSA_DEV);

    if (srcDev >= numCtrl)
    {
        Printf(Tee::PriError, "Source device index overflow. Number of devices is %d.\n",
               numCtrl);
        return RC::BAD_PARAMETER;
    }
    if (!AlsaIsDest && (destDev >= numCtrl))
    {
        Printf(Tee::PriError, "Destination device index overflow. Number of devices is %d.\n",
               numCtrl);
        return RC::BAD_PARAMETER;
    }

    //------------------------------------
    Printf(pri, "\tInitialize Azalia Device %d as source\n", srcDev);
    CHECK_RC(AzaliaControllerMgr::Mgr()->Get(srcDev, reinterpret_cast<Controller**>(&pSrcAza)));
    pSrcTestMod = new AzaliaStreamTestModule();

    AzaliaCodec* pCodec;
    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        if ((pSrcAza->GetCodec(i, &pCodec) == OK) &&
            (pCodec != NULL))
        {
            CHECK_RC_CLEANUP(pCodec->PowerUpAllWidgets());
        }
    }

    pSrcAza->GetCodec(srcCodec, &pCodec);
    CHECK_RC_CLEANUP(pCodec->SetDIPTransmitControl(srcPin, true));
    CHECK_RC_CLEANUP(pSrcTestMod->SetDefaults());
    CHECK_RC_CLEANUP(SetReservation(srcStreamIndex, srcCodec, srcPin));
    CHECK_RC_CLEANUP(pSrcAza->GetStream(srcStreamIndex, &pSrcStrm));
    CHECK_RC_CLEANUP(pSrcStrm->SetStreamProp(&m_streams[srcStreamIndex]));
    CHECK_RC_CLEANUP(pSrcTestMod->AddTestStream(srcStreamIndex));
    CHECK_RC_CLEANUP(pSrcTestMod->SetEnabled(true));
    CHECK_RC_CLEANUP(pSrcTestMod->SetFftRatio(m_STest->GetFftRatio()));
    CHECK_RC_CLEANUP(pSrcTestMod->Reset(pSrcAza));
    //------------------------------------

    //------------------------------------
    if (AlsaIsDest)
    {
        Printf(pri, "\tInitialize ALSA as destination\n");
        pDestTestMod = new AlsaStreamTestModule();
        CHECK_RC_CLEANUP(pDestTestMod->GetStream(destStreamIndex, &pDestStrm));
        CHECK_RC_CLEANUP(pDestStrm->SetStreamProp(&m_streams[destStreamIndex]));
    }
    else
    {
        Printf(pri, "\tInitialize Azalia Device %d as destination\n", destDev);
        CHECK_RC_CLEANUP(AzaliaControllerMgr::Mgr()->Get(destDev, reinterpret_cast<Controller**>(&pDestAza)));
        pDestTestMod = new AzaliaStreamTestModule();

        for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
        {
            if ((pDestAza->GetCodec(i, &pCodec) == OK) &&
                (pCodec != NULL))
            {
                CHECK_RC_CLEANUP(pCodec->PowerUpAllWidgets());
            }
        }

        CHECK_RC_CLEANUP(pDestTestMod->SetDefaults());
        CHECK_RC_CLEANUP(SetReservation(destStreamIndex, destCodec, destPin));
        CHECK_RC_CLEANUP(pDestAza->GetStream(destStreamIndex, &pDestStrm));
        CHECK_RC_CLEANUP(pDestStrm->SetStreamProp(&m_streams[destStreamIndex]));
        CHECK_RC_CLEANUP(pDestTestMod->AddTestStream(destStreamIndex));
        CHECK_RC_CLEANUP(pDestTestMod->SetEnabled(true));
        CHECK_RC_CLEANUP(pDestTestMod->SetFftRatio(m_STest->GetFftRatio()));
        CHECK_RC_CLEANUP(pDestTestMod->Reset(pDestAza));
        //------------------------------------
    }

    Printf(pri, "Source Stream\n");
    CHECK_RC_CLEANUP(pSrcTestMod->PrintInfo(pri, true, false));

    Printf(pri, "Destination Stream\n");
    CHECK_RC_CLEANUP(pDestTestMod->PrintInfo(pri, true, false));

    // Setup Source Device Parameters
    CHECK_RC_CLEANUP(pSrcAza->IntToggle(AzaliaController::INT_GLOBAL, m_Global_IntEn));
    CHECK_RC_CLEANUP(pSrcAza->IntToggle(AzaliaController::INT_CONTROLLER, m_Cntrl_IntEn));
    CHECK_RC_CLEANUP(pSrcAza->IntToggle(AzaliaController::INT_CORB_MEM_ERR, m_Cme_IntEn));
    CHECK_RC_CLEANUP(pSrcAza->ToggleBdlCoherence(m_BdlCoh));
    CHECK_RC_CLEANUP(pSrcAza->StreamToggleInputCoherence(m_InputStreamCoh));
    CHECK_RC_CLEANUP(pSrcAza->StreamToggleOutputCoherence(m_OutputStreamCoh));
    CHECK_RC_CLEANUP(pSrcAza->CorbToggleCoherence(m_CorbCoh));
    CHECK_RC_CLEANUP(pSrcAza->RirbToggleCoherence(m_RirbCoh));
    CHECK_RC_CLEANUP(pSrcAza->DmaPosToggleCoherence(m_DmaPosCoh));

    if (!AlsaIsDest)
    {
        // Setup Destination Device Parameters
        CHECK_RC_CLEANUP(pDestAza->IntToggle(AzaliaController::INT_GLOBAL, m_Global_IntEn));
        CHECK_RC_CLEANUP(pDestAza->IntToggle(AzaliaController::INT_CONTROLLER, m_Cntrl_IntEn));
        CHECK_RC_CLEANUP(pDestAza->IntToggle(AzaliaController::INT_CORB_MEM_ERR, m_Cme_IntEn));
        CHECK_RC_CLEANUP(pDestAza->ToggleBdlCoherence(m_BdlCoh));
        CHECK_RC_CLEANUP(pDestAza->StreamToggleInputCoherence(m_InputStreamCoh));
        CHECK_RC_CLEANUP(pDestAza->StreamToggleOutputCoherence(m_OutputStreamCoh));
        CHECK_RC_CLEANUP(pDestAza->CorbToggleCoherence(m_CorbCoh));
        CHECK_RC_CLEANUP(pDestAza->RirbToggleCoherence(m_RirbCoh));
        CHECK_RC_CLEANUP(pDestAza->DmaPosToggleCoherence(m_DmaPosCoh));
    }

    // Start the streams and the test
    CHECK_RC_CLEANUP(pDestTestMod->Start());
    Printf(Tee::PriDebug, "Destination DMA Started\n");

    CHECK_RC_CLEANUP(pSrcTestMod->Start());
    Printf(Tee::PriDebug, "Source DMA Started\n");

    while (!srcFinished || !destFinished)
    {
        if (sleep*1000 > DMA_TEST_TIMEOUT)
        {
            Printf(Tee::PriError, "AzaliaTest::DmaTest timeout error!\n");
            rc = RC::TIMEOUT_ERROR;
            break;
        }
        if (m_DmaTestInfo.duration &&
            (sleep*1000 > ((UINT64)m_DmaTestInfo.duration * 1000000)))
        {
            Printf(Tee::PriDebug, "Duration passed, shutting down\n");
            break;
        }

        AzaliaUtilities::Pause(1);
        sleep += AzaliaUtilities::GetTimeScaler();
        CHECK_RC_CLEANUP(pSrcTestMod->Continue());
        srcFinished = pSrcTestMod->IsFinished();

        CHECK_RC_CLEANUP(pDestTestMod->Continue());
        destFinished = pDestTestMod->IsFinished();
    }

    CHECK_RC_CLEANUP(pSrcTestMod->Stop());
    CHECK_RC_CLEANUP(pDestTestMod->Stop());

    Printf(pri, "Source Stream Status\n");
    CHECK_RC_CLEANUP(pSrcTestMod->PrintInfo(pri, false, true));

    Printf(pri, "Destination Stream Status\n");
    CHECK_RC_CLEANUP(pDestTestMod->PrintInfo(pri, false, true));

    // Check the Streams for errors
    if (!m_DmaTestInfo.duration)
    {
        if (!pSrcTestMod->IsFinished())
        {
            Printf(Tee::PriError, "Source Stream did not finish!\n");
            return RC::AZA_STREAM_ERROR;
        }

        if (!pDestTestMod->IsFinished())
        {
            Printf(Tee::PriError, "Destination Stream did not finish!\n");
            return RC::AZA_STREAM_ERROR;
        }
    }
    if (pSrcTestMod->IsError())
    {
        Printf(Tee::PriError, "Source Stream had an error\n");
        return RC::AZA_STREAM_ERROR;
    }
    if (pDestTestMod->IsError())
    {
        Printf(Tee::PriError, "Destination Stream had an error\n");
        return RC::AZA_STREAM_ERROR;
    }

    // Check the two stream buffers
    Printf(pri, "Checking loopback between device %d and %d\n",
           srcDev, destDev);
    CHECK_RC_CLEANUP(pSrcStrm->CheckLoopback(0, pDestStrm, 0));

Cleanup:

    pSrcAza->GetCodec(srcCodec, &pCodec);
    pCodec->SetDIPTransmitControl(srcPin, false);

    // Reset the codec by sending reset verb to node 1 twice
    pCodec->SubmitVerb(1, VERB_RESET, 0);
    pCodec->SubmitVerb(1, VERB_RESET, 0);

    if (pSrcTestMod)
        delete pSrcTestMod;

    if (pDestTestMod)
        delete pDestTestMod;

    if (pri >= Tee::PriNormal)
    {
        if(OK == rc)
        {
            Printf(Tee::PriHigh, "$$$ PASSED $$$");
        }
        else
        {
            Printf(Tee::PriHigh, "*** FAILED ***");
        }
        Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);
    }

    return rc;
}

//------------------------------------------------------------------------------
//     JavaScript Interface
//------------------------------------------------------------------------------

EXPOSE_MODS_TESTS_BSC(AzaliaTest, "Azalia test class");

// Stream Input Test properties
PROP_CONST(AzaliaTest, IARG_CHANNELS, AzaStream::IARG_CHANNELS);
PROP_CONST(AzaliaTest, IARG_SIZE, AzaStream::IARG_SIZE);
PROP_CONST(AzaliaTest, IARG_RATE, AzaStream::IARG_RATE);
PROP_CONST(AzaliaTest, IARG_TYPE, AzaStream::IARG_TYPE);
PROP_CONST(AzaliaTest, IARG_DIR_INPUT, AzaliaDmaEngine::DIR_INPUT);
PROP_CONST(AzaliaTest, IARG_DIR_OUTPUT, AzaliaDmaEngine::DIR_OUTPUT);
PROP_CONST(AzaliaTest, IARG_TP, AzaStream::IARG_TP);
PROP_CONST(AzaliaTest, IARG_MIN_NUM_BLOCKS, AzaStream::IARG_MIN_NUM_BLOCKS);
PROP_CONST(AzaliaTest, IARG_MAX_NUM_BLOCKS, AzaStream::IARG_MAX_NUM_BLOCKS);
PROP_CONST(AzaliaTest, IARG_NUM_BDLES, AzaStream::IARG_NUM_BDLES);
PROP_CONST(AzaliaTest, IARG_STRM_NUMBER, AzaStream::IARG_STRM_NUMBER);
PROP_CONST(AzaliaTest, IARG_MAX_HARMONICS, AzaStream::IARG_MAX_HARMONICS);
PROP_CONST(AzaliaTest, IARG_CLICK_THRESH, AzaStream::IARG_CLICK_THRESH);
PROP_CONST(AzaliaTest, IARG_SMOOTH_THRESH, AzaStream::IARG_SMOOTH_THRESH);
PROP_CONST(AzaliaTest, IARG_ROUTING_REQUIREMENTS, AzaStream::IARG_ROUTING_REQUIREMENTS);
PROP_CONST(AzaliaTest, IARG_STRIPE_LINES, AzaStream::IARG_STRIPE_LINES);
PROP_CONST(AzaliaTest, IARG_CODING_TYPE, AzaStream::IARG_CODING_TYPE);
PROP_CONST(AzaliaTest, IARG_FILENAME, AzaStream::IARG_FILENAME);
PROP_CONST(AzaliaTest, IARG_SYNC, AzaStream::IARG_SYNC);
PROP_CONST(AzaliaTest, IARG_ALLOW_IOCS, AzaStream::IARG_ALLOW_IOCS);
PROP_CONST(AzaliaTest, IARG_INT_EN, AzaStream::IARG_INT_EN);
PROP_CONST(AzaliaTest, IARG_FIFO_EN, AzaStream::IARG_FIFO_EN);
PROP_CONST(AzaliaTest, IARG_DESC_EN, AzaStream::IARG_DESC_EN);
PROP_CONST(AzaliaTest, IARG_IOC_EN, AzaStream::IARG_IOC_EN);
PROP_CONST(AzaliaTest, IARG_FSUB_OK, AzaStream::IARG_FSUB_OK);
PROP_CONST(AzaliaTest, IARG_TRUNC_STRM, AzaStream::IARG_TRUNC_STRM);
PROP_CONST(AzaliaTest, IARG_TEST_DESC_ERRORS, AzaStream::IARG_TEST_DESC_ERRORS);
PROP_CONST(AzaliaTest, IARG_TEST_PAUSE, AzaStream::IARG_TEST_PAUSE);
PROP_CONST(AzaliaTest, IARG_FAIL_IF_NODATA, AzaStream::IARG_FAIL_IF_NODATA);
PROP_CONST(AzaliaTest, IARG_RANDOM_DATA, AzaStream::IARG_RANDOM_DATA);
PROP_CONST(AzaliaTest, IARG_TEST_OUT_STREAM_BUFF_MON, AzaStream::IARG_TEST_OUT_STREAM_BUFF_MON);
PROP_CONST(AzaliaTest, IARG_IOC_SPACE, AzaStream::IARG_IOC_SPACE);
PROP_CONST(AzaliaTest, IARG_CODEC, AzaStream::IARG_CODEC);

PROP_CONST(AzaliaTest, ALSA_DEV, AzaliaTest::ALSA_DEV);

// Core test properties
CLASS_PROP_READWRITE(AzaliaTest, Duration, UINT32, "Duration to run the test for before timing out");
CLASS_PROP_READWRITE(AzaliaTest, Seed, UINT32, "Random generator seed");
CLASS_PROP_READWRITE(AzaliaTest, Pci_IntEn, bool, "Enable/Disable PCI Interrupts");
CLASS_PROP_READWRITE(AzaliaTest, Global_IntEn, bool, "Enable/Disable Global Interrupts");
CLASS_PROP_READWRITE(AzaliaTest, Cntrl_IntEn, bool, "Enable/Disable Controller Interrupts");
CLASS_PROP_READWRITE(AzaliaTest, BdlCoh, bool, "Enable/Disable BDLE Coherence");
CLASS_PROP_READWRITE(AzaliaTest, DmaPosCoh, bool, "Enable/Disable Dma Position Coherence");
CLASS_PROP_READWRITE(AzaliaTest, RirbCoh, bool, "Enable/Disable RIRB Coherence");
CLASS_PROP_READWRITE(AzaliaTest, CorbCoh, bool, "Enable/Disable CORB Coherence");
CLASS_PROP_READWRITE(AzaliaTest, InputStreamCoh, bool, "Enable/Disable Input Stream Coherence");
CLASS_PROP_READWRITE(AzaliaTest, OutputStreamCoh, bool, "Enable/Disable Output Stream Coherence");
CLASS_PROP_READWRITE(AzaliaTest, SimOnSilicon, bool, "Indicates running simulation build on silicon (for gputest.js)");
CLASS_PROP_READWRITE(AzaliaTest, AzaDevIndex, UINT32, "Index of the azalia device on which this test will run.");

// Ring buffer test properties
CLASS_PROP_READWRITE(AzaliaTest, RB_Enable, bool, "Enable Ring Buffer Testing");
CLASS_PROP_READWRITE(AzaliaTest, RB_NCommands, UINT32, "Number of commands to send for Ring Buffer Tests");
CLASS_PROP_READWRITE(AzaliaTest, RB_CorbStop, bool, "Pause the CORB after sending commands");
CLASS_PROP_READWRITE(AzaliaTest, RB_RirbStop, bool, "Pause the RIRB after sending commands");
CLASS_PROP_READWRITE(AzaliaTest, RB_AllowPtrResets, bool, "Allow the CORB/RIRB pointers to be reset when stopping them");
CLASS_PROP_READWRITE(AzaliaTest, RB_AllowNullCommands, bool, "Allow Null Commands in the CORB");
CLASS_PROP_READWRITE(AzaliaTest, RB_CorbSize, UINT32, "Set the CORB Size to use");
CLASS_PROP_READWRITE(AzaliaTest, RB_RirbSize, UINT32, "Set the RIRB Size to use");
CLASS_PROP_READWRITE(AzaliaTest, RB_RIntCnt, UINT32, "Set the initial response interrupt count");
CLASS_PROP_READWRITE(AzaliaTest, RB_RRIntEn, bool, "Enable RIRB Response Interrupt");
CLASS_PROP_READWRITE(AzaliaTest, RB_ROIntEn, bool, "Enable RIRB Overrun Interrupt");
CLASS_PROP_READWRITE(AzaliaTest, RB_OverrunTest, bool, "Enable the RIRB Overrun test");
CLASS_PROP_READWRITE(AzaliaTest, RB_EnableUnsolResponses, bool, "Enable Unsolicited Responses");
CLASS_PROP_READWRITE(AzaliaTest, RB_ExpectUnsolResponses, bool, "Should the test expect Unsolicited Responses");
CLASS_PROP_READWRITE(AzaliaTest, RB_EnableCodelwnsolResponses, bool, "Enable Unsolicitied Responses from the codec");

// Stream test properties
CLASS_PROP_READWRITE(AzaliaTest, ST_Enable, bool, "Enable the Stream Tests");
CLASS_PROP_READWRITE(AzaliaTest, ST_LoopInput, bool, "Loop the input stream");
CLASS_PROP_READWRITE(AzaliaTest, ST_LoopOutput, bool, "Loop the output stream");
CLASS_PROP_READWRITE(AzaliaTest, ST_DmaPos, bool, "Enable DMA Position reporting");
CLASS_PROP_READWRITE(AzaliaTest, ST_TestFifoErrors, bool, "Enable Testing Fifo Errors");
CLASS_PROP_READWRITE(AzaliaTest, ST_SkipLbCheck, bool, "Skip comparing the data after a loopback test");
CLASS_PROP_READWRITE(AzaliaTest, ST_FftRatio, FLOAT64, "Set the FFT ratio to override regular data comparison");
CLASS_PROP_READWRITE(AzaliaTest, ST_TestProgOutStrmBuff, bool, "Test the programmable output stream buffering feature");

// Methods
JS_STEST_BIND_NO_ARGS(AzaliaTest, Reset, "Reset test state so it is ready to run again");
JS_STEST_TEMPLATE(AzaliaTest, VerifyDmaTest, 1, "Verify the DMA Test Result");
JS_STEST_TEMPLATE(AzaliaTest, DmaTest, 1, "Play and Record to test DMA");
JS_STEST_TEMPLATE(AzaliaTest, SetStreamInput, 3, "Set a parameter for a test stream");
JS_STEST_TEMPLATE(AzaliaTest, SetReservation, 3, "Reserve a pin for a specific stream");
JS_STEST_TEMPLATE(AzaliaTest, CreateLoopback, 4, "Specify source and destination stream and channel for a loopback");
JS_STEST_TEMPLATE(AzaliaTest, CreateDefaultLoopback, 5, "Create a loopback between the streams using the default pins on the specified codec");
JS_STEST_TEMPLATE(AzaliaTest, DualLoopback, 9, "Specify loopback using 2 Azalia devices.");
// Can't find property named Device in cpp, to be remove
//JSCLASS_PROPERTY(AzaliaTest, Device, 0xff, "Azalia Device to be tested");

// Method implementations
C_(AzaliaTest_DmaTest)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if (NumArguments > 1 )
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.DmaTest(Priority for test output)");
        return JS_FALSE;
    }

    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);

    // Get arguments
    UINT32 pri = Tee::PriNormal;
    if (NumArguments == 1)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &pri))
        {
            JS_ReportError(pContext, "Invalid value for priority");
            return JS_FALSE;
        }
    }

    RETURN_RC(pAzaTest->DmaTest((Tee::Priority)pri));
}

C_(AzaliaTest_VerifyDmaTest)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if (NumArguments > 1 )
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.VerifyDmaTest(Priority for test output)");
        return JS_FALSE;
    }

    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);

    // Get arguments
    UINT32 pri = Tee::PriNormal;
    if (NumArguments == 1)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &pri))
        {
            JS_ReportError(pContext, "Invalid value for priority");
            return JS_FALSE;
        }
    }

    RETURN_RC(pAzaTest->VerifyDmaTest((Tee::Priority)pri));
}

C_(AzaliaTest_SetStreamInput)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if (NumArguments != 3)
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.SetStreamInputArg(stream_index,StreamInputArgType,value)");
        return JS_FALSE;
    }
    // Get arguments
    UINT32 stream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &stream))
    {
        JS_ReportError(pContext, "Invalid value for stream index");
        return JS_FALSE;
    }
    UINT32 argType = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &argType))
    {
        JS_ReportError(pContext, "Invalid value for arg type");
        return JS_FALSE;
    }
    UINT32 argVal = 0;
    string strVal = "";
    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);
    if (argType == AzaStream::IARG_FILENAME)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &strVal))
        {
            JS_ReportError(pContext, "Invalid value for arg value");
            return JS_FALSE;
        }
        RETURN_RC(pAzaTest->SetStreamInput(stream, argType, strVal));
    }
    else
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &argVal))
        {
            JS_ReportError(pContext, "Invalid value for arg value");
            return JS_FALSE;
        }
        RETURN_RC(pAzaTest->SetStreamInput(stream, argType, argVal));
    }
}

C_(AzaliaTest_SetReservation)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if (NumArguments != 3)
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.SetReservation(stream,codec,pin)");
        return JS_FALSE;
    }
    // Get arguments
    UINT32 stream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &stream))
    {
        JS_ReportError(pContext, "Invalid value for stream index");
        return JS_FALSE;
    }
    UINT32 codec = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &codec))
    {
        JS_ReportError(pContext, "Invalid value for codec");
        return JS_FALSE;
    }
    UINT32 pin = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &pin))
    {
        JS_ReportError(pContext, "Invalid value for pin");
        return JS_FALSE;
    }
    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);
    RETURN_RC(pAzaTest->SetReservation(stream, codec, pin));
}

C_(AzaliaTest_CreateLoopback)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if (NumArguments != 4)
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.CreateLoopback(srcStrm,srcChan,destStrm,destChan)");
        return JS_FALSE;
    }
    // Get arguments
    UINT32 srcStream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &srcStream))
    {
        JS_ReportError(pContext, "Invalid value for source stream index");
        return JS_FALSE;
    }
    UINT32 srcChan = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &srcChan))
    {
        JS_ReportError(pContext, "Invalid value for source channel");
        return JS_FALSE;
    }
    UINT32 destStream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &destStream))
    {
        JS_ReportError(pContext, "Invalid value for dest stream index");
        return JS_FALSE;
    }
    UINT32 destChan = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[3], &destChan))
    {
        JS_ReportError(pContext, "Invalid value for dest channel");
        return JS_FALSE;
    }
    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);
    RETURN_RC(pAzaTest->CreateLoopback(srcStream, srcChan, destStream, destChan));
}

C_(AzaliaTest_CreateDefaultLoopback)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if (NumArguments != 5)
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.CreateDefaultLoopback(codec,srcStrm,srcChan,destStrm,destChan)");
        return JS_FALSE;
    }
    // Get arguments
    UINT32 codec = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &codec))
    {
        JS_ReportError(pContext, "Invalid value for codec");
        return JS_FALSE;
    }
    UINT32 srcStream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &srcStream))
    {
        JS_ReportError(pContext, "Invalid value for source stream index");
        return JS_FALSE;
    }
    UINT32 srcChan = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &srcChan))
    {
        JS_ReportError(pContext, "Invalid value for source channel");
        return JS_FALSE;
    }
    UINT32 destStream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[3], &destStream))
    {
        JS_ReportError(pContext, "Invalid value for dest stream index");
        return JS_FALSE;
    }
    UINT32 destChan = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[4], &destChan))
    {
        JS_ReportError(pContext, "Invalid value for dest channel");
        return JS_FALSE;
    }
    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);
    RETURN_RC(pAzaTest->CreateDefaultLoopback(codec, srcStream, srcChan,
                                              destStream, destChan));
}

C_(AzaliaTest_DualLoopback)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a method with 3 arg.
    if ((NumArguments < 8) || (NumArguments > 9))
    {
        JS_ReportError(pContext,"Usage: AzaliaTest.DualLoopback("
                                "srcDev,srcCodec,srcPin,srcStrm,"
                                "destDev,destCodec,destPin,destStrm, <Print priority>)");
        return JS_FALSE;
    }
    // Get arguments
    UINT32 srcDev = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &srcDev))
    {
        JS_ReportError(pContext, "Invalid value for source device");
        return JS_FALSE;
    }
    UINT32 srcCodec = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &srcCodec))
    {
        JS_ReportError(pContext, "Invalid value for source codec");
        return JS_FALSE;
    }
    UINT32 srcPin = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &srcPin))
    {
        JS_ReportError(pContext, "Invalid value for source codec");
        return JS_FALSE;
    }
    UINT32 srcStream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[3], &srcStream))
    {
        JS_ReportError(pContext, "Invalid value for source stream index");
        return JS_FALSE;
    }
    UINT32 destDev = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[4], &destDev))
    {
        JS_ReportError(pContext, "Invalid value for dest device");
        return JS_FALSE;
    }
    UINT32 destCodec = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[5], &destCodec))
    {
        JS_ReportError(pContext, "Invalid value for dest codec");
        return JS_FALSE;
    }
    UINT32 destPin = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[6], &destPin))
    {
        JS_ReportError(pContext, "Invalid value for source codec");
        return JS_FALSE;
    }
    UINT32 destStream = 0;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[7], &destStream))
    {
        JS_ReportError(pContext, "Invalid value for dest stream index");
        return JS_FALSE;
    }

    UINT32 pri = Tee::PriNormal;
    if (NumArguments == 9)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[8], &pri))
        {
            JS_ReportError(pContext, "Invalid value for priority");
            return JS_FALSE;
        }
    }

    AzaliaTest* pAzaTest = (AzaliaTest*) JS_GetPrivate(pContext, pObject);
    RETURN_RC(pAzaTest->DualLoopback(srcDev, srcCodec, srcPin, srcStream,
                                     destDev, destCodec, destPin, destStream, (Tee::Priority)pri));
}

