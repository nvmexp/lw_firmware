/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2013,2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azastrm.h"
#include "azactrl.h"
#include "azacdc.h"
#include "azareg.h"
#include "azart.h"
#include "core/include/platform.h"
#include <stdio.h>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaStream"

// For simulation there is a corner case due to the small sample size and
// timing which will cause the us to think that the stream is still running
// due to wrap-around. Increase the threshold tolerance to prevent this.a
#ifdef SIM_BUILD
#define END_THRESHOLD 0.8f
#else
#define END_THRESHOLD 0.9f
#endif

#define DEFAULT_GRANULARITY 1
#define MIN_BLOCKS ((UINT32) 2)
#define MAX_BLOCKS ((UINT32) (48000*4))
#define MIN_BDLE_SEGMENT_SIZE 1

AzaStream::AzaStream(AzaliaController* pAza, AzaliaDmaEngine* pEngine,
                     AzaliaDmaEngine::DIR Dir, UINT32 SwStreamNumber)
{

    m_pAza = pAza;
    m_pFormat = NULL;
    m_BufMgr = NULL;
    m_pBdl = NULL;
    m_pNewBdl = nullptr;
    m_Stats = NULL;
    m_pEngine = pEngine;
    m_vpRoutes.clear();
    m_FftRatio = 0.0;
    m_IsReserved = false;
    m_BaseGranularity = DEFAULT_GRANULARITY;
    m_SwStreamNumber = SwStreamNumber;
    m_FundPeriod = 1;
    Reset(true);
    SetDefaultParameters();
    m_Dir = Dir;

}

AzaStream::~AzaStream()
{
    Reset(false); // Don't release routes, they might be
                  // deleted at this point
}

RC AzaStream::SetDefaultParameters()
{
    input.channels = AzaliaFormat::CHANNELS_2;
    input.size = AzaliaFormat::SIZE_16;
    input.rate = AzaliaFormat::RATE_48000;
    input.type = AzaliaFormat::TYPE_PCM;
    input.tp = AzaliaDmaEngine::TP_HIGH;
    input.nBDLEs = 0;
    input.minBlocks = 0;
    input.maxBlocks = 0;
    input.sync = false;
    input.outputAttenuation = 0.0;
    input.maxHarmonics = 0;
    input.clickThreshPercent = 100;
    input.smoothThreshPercent = 100;
    input.randomData = false;
    input.sdoLines = AzaliaDmaEngine::SL_ONE;
    input.codingType = 0;
    input.routingRequirements = StreamProperties::MUST_ROUTE_ALL_CHANNELS;
    input.reservation.reserved = 0;
    input.reservation.codec = 0;
    input.reservation.pin = 0;
    input.streamNumber = RANDOM_STRM_NUMBER;
    input.allowIocs = false;
    input.intEn = true;
    input.fifoEn = true;
    input.descEn = true;
    input.iocEn = true;
    input.formatSubstOk = false;
    input.testDescErrors = false;
    input.testFifoErrors = false;
    input.testPauseResume = false;
    input.truncStream = false;
    input.failIfNoData = true;
    input.testOutStrmBuffMon = false;
    input.filename = "";

    return OK;
}

RC AzaStream::Reset(bool ReleaseRoutes)
{

    if (m_pFormat)
    {
        delete m_pFormat;
        m_pFormat = NULL;
    }

    if(m_pBdl)
    {
        delete m_pBdl;
        m_pBdl = NULL;
    }

    if (m_pNewBdl)
    {
        delete m_pNewBdl;
        m_pNewBdl = nullptr;
    }

    if(m_BufMgr)
    {
        delete m_BufMgr;
        m_BufMgr = NULL;
    }

    if (m_Stats)
    {
        free(m_Stats);
        m_Stats = NULL;
    }

    if (ReleaseRoutes)
    {
        // Release the routes this stream used so that other streams can use them
        for(UINT32 i = 0; i < m_vpRoutes.size(); ++i)
        {
            m_vpRoutes[i]->Release();
        }
    }

    // Routes belong to the codec--don't delete them!
    m_vpRoutes.clear();

    memset(&m_status, 0, sizeof(m_status));

    return OK;
}

bool AzaStream::IsReserved()
{
    return m_IsReserved;
}

void AzaStream::Reserve(bool Reserve)
{
    m_IsReserved = Reserve;
}

RC AzaStream::ResetElapsedTime()
{
    m_status.elapsedTime = 0;
    return OK;
}

RC AzaStream::ProgramStream()
{
    RC rc;
    MASSERT(m_pAza);
    MASSERT(m_pFormat);

    UINT08 streamNumber;
    // Allocate hardware and prepare driver for stream
    CHECK_RC(m_pAza->GenerateStreamNumber(m_Dir, &streamNumber, input.streamNumber));
    CHECK_RC(m_pEngine->SetStreamNumber(streamNumber));

    UINT08 sIndex, eIndex;
    CHECK_RC(m_pEngine->GetStreamNumber(&sIndex));
    CHECK_RC(m_pEngine->GetEngineIndex(&eIndex));
    Printf(Tee::PriDebug, "Programming stream %d, assigned to engine %d and stream index %d",
           m_SwStreamNumber, eIndex, sIndex);

    UINT32 nBDLEs = input.nBDLEs;

    // Create the buffer manager
    m_BufMgr = new BufferMgr(m_pAza);
    if (!m_BufMgr)
    {
        Printf(Tee::PriError, "Can't create BufferMgr object\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Crete the m_stats object
    m_Stats = (AudioUtil::SineStatistics*) malloc(sizeof(AudioUtil::SineStatistics) *
                                                         m_pFormat->GetChannelsAsInt());
    MASSERT(m_Stats);
    memset(m_Stats,
           0,
           sizeof(AudioUtil::SineStatistics) * m_pFormat->GetChannelsAsInt());

    // Create the BDL object
    UINT32 addrBits = m_pAza->GetAddrBits();

    m_pBdl = new AzaBDL(addrBits,
                        m_pAza->GetMemoryType(),
                        input.allowIocs,
                        input.iocSpace,
                        m_pAza);

    CHECK_RC(m_pBdl->Init(MAX_BDLES));

    // For simulation builds allow small number of blocks for
    // even high rate streams
    #ifndef SIM_BUILD
     if ( (m_Dir == AzaliaDmaEngine::DIR_INPUT) &&
         (input.minBlocks < (UINT32)input.rate/4) )
    {
        input.minBlocks = input.rate/4;
        if (input.minBlocks > input.maxBlocks)
            input.maxBlocks = MAX_BLOCKS;
    }
    #endif

    if (!nBDLEs)
    {
        nBDLEs = AzaliaUtilities::GetRandom(MIN_BDLES, MAX_BDLES);
    }
    if (nBDLEs < MIN_BDLES || nBDLEs > MAX_BDLES)
    {
        Printf(Tee::PriError, "Number of BDLEs (%d) must be between %d and %d\n",
               nBDLEs, MIN_BDLES, MAX_BDLES);

        return RC::BAD_PARAMETER;
    }

    UINT32 nBlocks = max(input.minBlocks, max(nBDLEs, MIN_BLOCKS));
     nBlocks = AzaliaUtilities::GetRandom(
            nBlocks,
            (input.maxBlocks ? min(input.maxBlocks, MAX_BLOCKS) : MAX_BLOCKS));

    if (input.minBlocks && (nBlocks < input.minBlocks))
    {
       Printf(Tee::PriError, "nblocks less than input blocks: %d\n", nBlocks);
       return RC::SOFTWARE_ERROR;
    }

    if (input.maxBlocks && (nBlocks > input.maxBlocks))
    {
        Printf(Tee::PriError, "nblocks more than input blocks: %d\n", nBlocks);
        return RC::SOFTWARE_ERROR;
    }

    if ((nBlocks < nBDLEs) ||
        (nBlocks < MIN_BLOCKS) ||
        (nBlocks > MAX_BLOCKS))
    {
        Printf(Tee::PriError, "Invalid number of blocks: %d\n", nBlocks);
        return RC::SOFTWARE_ERROR;
    }

    // For output streams, select basic data type
    if (m_Dir == AzaliaDmaEngine::DIR_OUTPUT && !input.randomData)
    {
        UINT32 sampleRate = m_pFormat->GetRateAsSamples();
        const UINT32 ff_low = 80; // Hz
        const UINT32 ff_high = (sampleRate<16000) ? 120 : 160; // Hz
        const UINT32 ff_inc = 11; // Hz
        UINT32 fund_freq = 0;
        m_FundPeriod = GetGranularity();

        if (m_FundPeriod == 1)
        {
            // This is the first channel pair programmed. Pick a random
            // starting fundamental.
            fund_freq = AzaliaUtilities::GetRandom(ff_low, ff_high);
        }
        else
        {
            // Increment the fundamental frequency for each channel pair
            fund_freq = (sampleRate + (m_FundPeriod * ff_inc)) / m_FundPeriod;
            if (fund_freq > ff_high)
            {
                fund_freq -= (ff_high - ff_low);
            }
        }
        m_FundPeriod = sampleRate / fund_freq;
        SetGranularity(m_FundPeriod);
    }

    UINT32 nNewBlocks = 0;
    CHECK_RC(SatisfyGranularity(nBlocks,&nNewBlocks));
    nBlocks = nNewBlocks;

    MemoryFragment::FRAGLIST AzaFragList;
    CHECK_RC(AllocateBuffer(nBDLEs,
                            nBlocks,
                            addrBits,
                            &AzaFragList));

    CHECK_RC(m_pBdl->FillTableEntries(&AzaFragList));

    m_MaxHarmonics = input.maxHarmonics;

    // Fill buffer with data
    if (m_Dir == AzaliaDmaEngine::DIR_OUTPUT)
    {
        if (input.randomData)
        {
            CHECK_RC(m_pBdl->WriteBufferWithRandomData(m_pFormat->GetSizeInBytes()));
        }
        else
        {
            // Some codecs have a higher full-scale output voltage than others
            // (or than their own full-scale input voltage). For these codecs,
            // we want to decrease (attenuate) the volume of the output
            // streams. If the user explicitly set an attenuation value, use
            // it. Otherwise, use the default value for the codec.
            FLOAT32 attenuation = input.outputAttenuation;
            if (attenuation == 0.0)
            {
                // We assume that all channels of one stream are routed
                // to a single codec.
                AzaliaCodec* pCodec = NULL;
                if (m_vpRoutes.size() > 0) pCodec = m_vpRoutes[0]->GetCodec();
                if (pCodec)
                {
                    const CodecDefaultInformation* pCDInfo;
                    CHECK_RC(pCodec->GetCodecDefaults(&pCDInfo));
                    MASSERT(pCDInfo);
                    attenuation = pCDInfo->attenuation;
                }
                else
                {
                    // For unrouted streams, use 1.0
                    attenuation = 1.0;
                }

            }
            FLOAT32 amp = (FLOAT32) attenuation *
            AzaliaUtilities::GetRandom(500, 707) / 1000;
            UINT32 phase = AzaliaUtilities::GetRandom(0, m_FundPeriod-1);

            CHECK_RC(WriteBufferWithSines(m_FundPeriod,
                                          amp,
                                          phase,
                                          input.maxHarmonics));
        }
    }
    else
    {
        // Clear buffer contents for input streams
        CHECK_RC(m_pBdl->WriteBufferWithZeros());
    }

    CHECK_RC(m_pBdl->CopyTable(m_pAza->GetIsUseMappedPhysAddr(), &m_pNewBdl));
    CHECK_RC(m_pNewBdl->FillTableEntries(&AzaFragList));

    // Program the engine
    CHECK_RC(m_pEngine->ProgramEngine(m_pFormat,
                                      m_pNewBdl->GetBufferLengthInBytes(),
                                      nBDLEs,
                                      m_pNewBdl->GetBaseVirt(),
                                      input.tp, input.sdoLines));

    AzaliaCodec* pCodec = NULL;
    if ( (rc = m_pAza->GetCodec(input.reservation.codec, &pCodec)) != OK )
    {
        Printf(Tee::PriError, "Codec %d does not exist on the system\n",
               input.reservation.codec);

        return rc;
    }

    if (pCodec->IsSupportRoutableInputPath())
    {
        // Program the route
        for (UINT32 i=0; i<m_vpRoutes.size(); i++)
        {
            CHECK_RC(m_vpRoutes[i]->Configure(sIndex, 2*i, m_pFormat));
        }
    }
    else
    {
        // Program the route
        for (UINT32 i=0; i<m_vpRoutes.size(); i++)
        {
            if(m_Dir == AzaliaDmaEngine::DIR_OUTPUT)
                CHECK_RC(m_vpRoutes[i]->Configure(sIndex, 2*i, m_pFormat, input.sdoLines,
                                                 input.codingType));
        }

        if(m_Dir == AzaliaDmaEngine::DIR_INPUT)
        {
            // Send the verb to 7a0 here:
            // This is a hack only for testing loopback on GT21x since there is no
            // actual input route possible.
            UINT32 response = 0;
            UINT08 sNumber;
            CHECK_RC(m_pEngine->GetStreamNumber(&sNumber));
            Printf(Tee::PriDebug, "Send Verb 0x7a0 to set loopback for input "
                                  "stream %d of GPU to codec %d\n",
                   sNumber, input.reservation.codec);

            CHECK_RC(pCodec->SendCommand(1,0x7a0, (sNumber << 4)|0x1, &response));
            Printf(Tee::PriDebug, "Response from Verb 0x7a0 = %d\n", response);
        }
    }

    // Program stream interrupt enables
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_STREAM_DESC_ERR, input.descEn));
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_STREAM_FIFO_ERR, input.fifoEn));
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_STREAM_IOC, input.iocEn));
    CHECK_RC(m_pAza->IntToggle(AzaliaController::INT_STREAM, input.intEn));

    return OK;
}

RC AzaStream::PrepareTest()
{
    // Make sure fifoready bit is deasserted
    if (m_pEngine->IsFifoReady())
    {
        Printf(Tee::PriError, "Stream has a premature fifoready bit.\n");
        return RC::HW_STATUS_ERROR;
    }

    // Initialize the next IOC position expected
    m_status.nextIocPos_UW = m_pBdl->GetNextIOCPos(0);

    // Create a bad BDLE entry for Descriptor Error Test
    if (input.testDescErrors)
    {
        if (m_pAza->IsIch6())
        {
            Printf(Tee::PriLow, "Not testing descriptor errors on Intel ICH6 HDA"
                                "Controller. It doesn't generate them.\n");
            input.testDescErrors = false;
        }
        else
        {
            UINT32 maxBdleIndex = m_pBdl->GetLastBdleIndex();
            if (maxBdleIndex>10)
                maxBdleIndex = 10;
            UINT32 bdleIndex = AzaliaUtilities::GetRandom(1, maxBdleIndex);

            UINT08* base = (UINT08*)m_pBdl->GetBaseVirt();

            const unsigned int bdleSize = 16;

            // Set BDLE address to zero
            MEM_WR32(base+bdleSize*bdleIndex+0x0,0x0);
            Printf(Tee::PriDebug, "Forcing bdle %d of stream %d to zero "
                                  "to force descriptor error.\n",
                   bdleIndex,m_SwStreamNumber);
        }
    }

    return OK;
}

AzaliaDmaEngine* AzaStream::GetEngine()
{
    return m_pEngine;
}

UINT32 AzaStream::GetNLoops()
{
    return m_status.loop;
}

AzaliaFormat* AzaStream::GetFormat()
{
    return m_pFormat;
}

UINT32 AzaStream::GetLastPosition()
{
    return m_status.lastPos;
}

AzaliaDmaEngine::DIR AzaStream::GetDir()
{
    return m_Dir;
}

bool AzaStream::StreamGotTruncated()
{
    if(input.truncStream && !m_status.truncErrMsgPrinted)
    {
        Printf(Tee::PriError, "Stream %d did not get truncated when expected!!!\n",
               m_SwStreamNumber);
        return true;
    }
    return false;
}

bool AzaStream::DescErrorOclwrred()
{
    if(input.testDescErrors && !m_status.descErrOclwred)
    {
        Printf(Tee::PriError, "Stream %d did not have a descriptor error when expected!!!\n",
               m_SwStreamNumber);
        return true;
    }
    return false;
}

bool AzaStream::FifoErrOclwrred()
{
    if(input.testFifoErrors & !m_status.fifoErrTested)
    {
        Printf(Tee::PriError, "Stream %d fifo error did not occur when expected!!!\n",
               m_SwStreamNumber);
        return true;
    }

    return false;
}

RC AzaStream::UpdateAndVerifyPosition(bool* success)
{
    RC rc;
    UINT64 clock;
    UINT32 pos_before = m_pEngine->GetPosition();

    AzaliaController::IntStreamStatus strmsts;
    UINT08 engineIndex;
    CHECK_RC(m_pEngine->GetEngineIndex(&engineIndex));
    CHECK_RC(m_pAza->IntStreamGetStatus(engineIndex, &strmsts));
    CHECK_RC(m_pAza->IntStreamClearStatus(engineIndex));

    UINT32 pos_now = m_pEngine->GetPosition();
    if (AzaliaUtilities::GetRandom(0,1))
        CHECK_RC(m_pAza->RegRd(WALCLK, reinterpret_cast<UINT32*>(&clock)));
    else
        CHECK_RC(m_pAza->RegRd(WALCLKA, reinterpret_cast<UINT32*>(&clock)));

    Printf(Tee::PriDebug, "Before Verify Position: pos_now: %d, pos_before: %d\n",
           pos_now, pos_before);

    CHECK_RC(VerifyPosition(success, pos_now, pos_before, clock, strmsts.bcis));

    // Verify interrupt status
    if (strmsts.dese)
    {
        m_status.descErrOclwred = true;
        if (input.testDescErrors)
        {
            Printf(Tee::PriHigh, "Descriptor Error detected as expected on stream %d!\n",
                   m_SwStreamNumber);

            AzaliaDmaEngine::STATE engState;
            CHECK_RC(m_pEngine->GetEngineState(&engState));
            if (engState != AzaliaDmaEngine::STATE_STOP)
            {
                Printf(Tee::PriError, "Stream %d should have stopped after descriptor error!\n",
                       m_SwStreamNumber);
                *success = false;
            }
        }
        else
        {
            Printf(Tee::PriError, "Descriptor error on stream %d\n", m_SwStreamNumber);
            *success = false;
        }
    }

    if (strmsts.fifoe)
    {
        if (input.testFifoErrors)
        {
            // Print the message only once
            if(!m_status.fifoErrTested)
            {
                Printf(Tee::PriLow, "Stream %d fifo error flag set as expected.\n",
                       m_SwStreamNumber);

                m_status.fifoErrTested = true;
            }
        }
        else
        {
            Printf(Tee::PriError, "FIFO error on stream %d\n", m_SwStreamNumber);
            *success = false;
        }
    }

    return OK;
}

RC AzaStream::VerifyPosition(bool* success, UINT32 pos_now,
                             UINT32 pos_before, UINT64 clock, bool bcisSet)
{
    UINT32 bufSize = m_pBdl->GetBufferLengthInBytes();
    UINT32 bytesPerSample = m_pFormat->GetSizeInBytes();
    UINT32 bytesPerBlock = bytesPerSample * m_pFormat->GetChannelsAsInt();
    UINT32 avgBytesPerSec = bytesPerBlock * m_pFormat->GetRateAsSamples();

    *success = true;
    // Sanity checking
    if (pos_before > bufSize)
    {
        Printf(Tee::PriError, "Position (0x%08x) exceeds buffer size (0x%08x)!\n",
               pos_before, bufSize);

        *success = false;
    }
    if (pos_now > bufSize)
    {
        Printf(Tee::PriError, "Position (0x%08x) exceeds buffer size (0x%08x)!\n",
               pos_now, bufSize);

        *success = false;
    }
    if ((!pos_now || !pos_before) && !input.testDescErrors)
    {
        Printf(Tee::PriError, "Position of 0 detected. This should not happen "
                              "for a running stream!\n");
        *success = false;
    }

    // Update the elapsed time for this stream.
    if (!m_status.elapsedTime)
    {
        // There's likely to be a small offset between starting the clock
        // and starting the stream (at least in the case of non-sync'd streams).
        // This is especially true when we've done more complicated tests like
        // pause-resume testing.
        UINT64 trueElapsedTime = (UINT64) pos_now * 500 * 48000 / avgBytesPerSec;
        m_status.elapsedTime = clock;
        while (m_status.elapsedTime < trueElapsedTime)
        {
            // Account for possible clock wrap.
            m_status.elapsedTime += (((UINT64) 1) << 32);
        }
        m_status.timeOffset = m_status.elapsedTime - trueElapsedTime;
        m_status.loop = 0;
    }
    else
    {
        // Clock measurement is only 32 bits. Subtract lower 32 bits of
        // elapsed time from clock, then add (1<<32) to account for possible
        // clock wrap. Finally, preserve only 32 bits, since that's how big
        // the clock is. Add everything to the existing value of elapsed time.
        m_status.elapsedTime += (clock
                                  - (m_status.elapsedTime & 0xffffffff)
                                  + ((UINT64) 1 << 32))
                                 % ((UINT64) 1<<32);
    }

    // Callwlate the expected position from the time measurement.
    UINT64 expPosUW = (m_status.elapsedTime - m_status.timeOffset) *
        avgBytesPerSec / 48000 / 500;

    // Callwlate the expected number of loops. I don't understand this code
    // at all--have to check with Ed R. about how it works.
    UINT32 estLoopCnt_before;
    if (pos_before < (bufSize / 2))
        estLoopCnt_before = (UINT32) (expPosUW + bufSize / 4) / bufSize;
    else
        estLoopCnt_before = (UINT32) (expPosUW - bufSize / 4) / bufSize;

    UINT32 estLoopCnt_now;
    if (pos_now < (bufSize / 2))
        estLoopCnt_now = (UINT32) (expPosUW + bufSize / 4) / bufSize;
    else
        estLoopCnt_now = (UINT32) (expPosUW - bufSize / 4) / bufSize;

    if (estLoopCnt_now < m_status.loop && !input.truncStream)
    {
        Printf(Tee::PriError, "Estimated loop count (%d) is less than previous (%d)\n",
               estLoopCnt_now, m_status.loop);

        *success = false;
    }
    else
    {
        m_status.loop = estLoopCnt_now;
    }

    // Callwlate the estimate unwrapped position
    UINT64 estPosUW_before = (UINT64) pos_before + (UINT64) estLoopCnt_before * bufSize;
    UINT64 estPosUW_now = (UINT64) pos_now + (UINT64) estLoopCnt_now * bufSize;
    m_status.lastPos = pos_now;

    // Check IOC status
    // TODO: I don't understand this IOC code. Need to check with Ed R. to
    // understand how it works. Ideally, I think that we'd check for an
    // IOC entirely based on position here (and callwlate the next IOC
    // position), then compare our callwlation vs. the actual IOC status
    // elsewhere. Unfortunately, that's not something we can do right
    // now based on how the code works.
    bool hasIocError = false;
    if (bcisSet)
    {
        Printf(Tee::PriDebug, "Stream %d BCIS between positions 0x%08x (0x%08x%08x) "
                              "and 0x%08x (0x%08x%08x)\n",
               m_SwStreamNumber, pos_before, (UINT32) (estPosUW_before>>32),
               (UINT32) (estPosUW_before), pos_now, (UINT32) (estPosUW_now>>32),
               (UINT32) (estPosUW_now));

        // Verify that IOCs are present in buffer
        if (m_pBdl->GetNumIOCsSet() == 0)
        {
            Printf(Tee::PriError, "No IOCs are enabled, BCIS should not have been set\n");
            *success = false;
            hasIocError = true;
        }

        // Make sure IOC isn't premature
        if (m_status.nextIocPos_UW > estPosUW_now + m_pEngine->GetFifoSize())
        {
            Printf(Tee::PriError, "Stream %d IOC triggered too early!\n", m_SwStreamNumber);
            *success = false;
            hasIocError = true;
        }

        // Determine location of next IOC
        UINT64 oldPos_UW;
        if (m_status.nextIocPos_UW > pos_before)
            oldPos_UW = m_status.nextIocPos_UW;
        else
            oldPos_UW = estPosUW_before;

        UINT32 pos_old = (UINT32) (oldPos_UW % bufSize);
        UINT32 nextIoc = m_pBdl->GetNextIOCPos(pos_old);
        UINT64 nextIoc_UW = (oldPos_UW + (UINT64)nextIoc) - (UINT64)pos_old;
        if (nextIoc <= pos_old)
            nextIoc_UW += bufSize;

        m_status.nextIocPos_UW = nextIoc_UW;
        // Check for cases where we might have already cleared the nextIoc
        m_status.nextIocMayHaveBeenClearedAlready = false;
        if (m_status.nextIocPos_UW <= (estPosUW_now + m_pEngine->GetFifoSize()) )
        {
            // The next Ioc is close enough that we can't be sure if it
            // already triggered and was cleared.  Disable the missed IOC
            // checking for this particular IOC.
            m_status.nextIocMayHaveBeenClearedAlready = true;
        }
    }
    else
    {
        // Make sure IOC was not missed
        if ((m_pBdl->GetNumIOCsSet() > 0) &&
            (m_status.nextIocMayHaveBeenClearedAlready == false) &&
            (m_status.nextIocPos_UW < estPosUW_before))
        {
            Printf(Tee::PriError, "Stream %d IOC should have been set!\n", m_SwStreamNumber);
            *success = false;
            hasIocError = true;

            // Callwlate next IOC position
            // Because we've already failed, just use the IOC following
            // the current position
            UINT32 nextIoc = m_pBdl->GetNextIOCPos(pos_now);
            m_status.nextIocPos_UW = estPosUW_now + nextIoc - pos_now;
            if (nextIoc <= pos_now)
                m_status.nextIocPos_UW += bufSize;
        }
    }
    if (hasIocError)
    {
        Printf(Tee::PriLow, "Stream %d IOC Error:\n", m_SwStreamNumber);
        Printf(Tee::PriLow, "    Position A: 0x%08x (0x%08x%08x)\n",
               pos_before,
               (UINT32) (estPosUW_before>>32),
               (UINT32) (estPosUW_before));

        Printf(Tee::PriLow, "    Position B: 0x%08x (0x%08x%08x)\n",
               pos_now,
               (UINT32) (estPosUW_now>>32),
               (UINT32) (estPosUW_now));

        Printf(Tee::PriLow, "    Next IOC: 0x%08x%08x\n",
               (UINT32) (m_status.nextIocPos_UW>>32),
               (UINT32) (m_status.nextIocPos_UW));

        Printf(Tee::PriLow, "    Buf Size: %d\n", bufSize);
    }

    return OK;
}

RC AzaStream::FindRoute(bool* success)
{
    RC rc;
    *success = false;

    m_pFormat = new AzaliaFormat(input.type, input.size,
                                 input.rate, input.channels);
    MASSERT(m_pFormat);

    // Truncate stream tests often require that the truncated stream be a
    // high bandwidth one.  Since not all codecs can route high-rate streams
    // lets skip it.
    if (input.truncStream)
    {
        Printf(Tee::PriDebug, "    This is a truncated stream so we're not routing it. Skipping.\n");
        *success = true;
        return OK;
    }

    AzaliaCodec* pCodec = NULL;
    if ( (rc = m_pAza->GetCodec(input.reservation.codec, &pCodec)) != OK )
    {
        Printf(Tee::PriError, "Codec %d does not exist on the system\n",
               input.reservation.codec);

        return rc;
    }

    // For streams that do not support a routable input path set the
    // routing requirement as must be unrouted.
    if ((m_Dir == AzaliaDmaEngine::DIR_INPUT) && !pCodec->IsSupportRoutableInputPath())
        input.routingRequirements = StreamProperties::MUST_BE_UNROUTED;

    // Make sure that unrouted streams do not have a reservation
    if (input.routingRequirements == StreamProperties::MUST_BE_UNROUTED)
    {
        if (pCodec->IsSupportRoutableInputPath())
        {
            if (input.reservation.reserved)
            {
                Printf(Tee::PriError, "Route with reservation also tagged as unroutable!\n");
                return RC::BAD_PARAMETER;
            }
        }
        *success = true;
        return OK;
    }

    UINT32 cMin = 0;
    UINT32 cMax = MAX_NUM_SDINS;
    if (input.reservation.reserved)
    {
        cMin = input.reservation.codec;
        cMax = cMin + 1;
    }

    for ( ; cMin < cMax; cMin++)
    {
        AzaliaCodec* pCodec = NULL;
        if ((m_pAza->GetCodec(cMin, &pCodec) == OK) &&
            (pCodec != NULL))
        {
            AzaliaRoute* pRoute = NULL;

            Printf(Tee::PriDebug, "Stream: %d, Finding Route for Channels 1-2\n",
                   m_SwStreamNumber);

            CHECK_RC(pCodec->FindRoute(&pRoute, m_Dir,
                                       m_pFormat, false, input.reservation.pin));
            if (pRoute)
            {
                pRoute->PrintInfo(Tee::PriDebug);
                UINT32 chanCnt = 0;

                m_vpRoutes.push_back(pRoute);
                chanCnt = 2;
                Printf(Tee::PriDebug, "Found Route for Channels 1-2\n");

                // If we found an HDMI capable route then we can route upto
                // 8 channels on it.
                if (pRoute->IsHdmiCapable())
                {
                    chanCnt = min(m_pFormat->GetChannelsAsInt(), (UINT32)8);
                    Printf(Tee::PriDebug, "HDMI Route Found. Routing %d channels on it\n",
                           chanCnt);
                }

                switch (input.routingRequirements)
                {
                    case StreamProperties::MUST_ROUTE_ONLY_ONE_CHANNEL_PAIR:
                        // Do nothing--we've found our one route
                        *success = true;
                        break;
                    case StreamProperties::MUST_ROUTE_AT_LEAST_ONE_CHANNEL_PAIR:
                    case StreamProperties::MAY_BE_UNROUTED:
                    case StreamProperties::MUST_ROUTE_ALL_CHANNELS:
                        *success = true;
                        for (; chanCnt < m_pFormat->GetChannelsAsInt(); chanCnt += 2)
                        {
                            Printf(Tee::PriDebug, "Finding Route for Channels %d-%d\n",
                                   chanCnt + 1, chanCnt + 2);

                            CHECK_RC(pCodec->FindRoute(&pRoute, m_Dir, m_pFormat,
                                                       false, 0));
                            if (pRoute)
                            {
                                m_vpRoutes.push_back(pRoute);
                                Printf(Tee::PriDebug, "Found Route for Channels %d-%d\n",
                                       chanCnt + 1, chanCnt + 2);
                            }
                            else if (input.routingRequirements == StreamProperties::MUST_ROUTE_ALL_CHANNELS)
                            {
                                // Couldn't find a route for all channels on
                                // this codec. Must release all of the routes
                                // that we grabbed so far.
                                *success = false;
                                while (m_vpRoutes.size() > 0)
                                {
                                    pRoute = m_vpRoutes.back();
                                    pRoute->Release();
                                    m_vpRoutes.pop_back();
                                }
                                break;
                            }
                            else
                            {
                                // We have routed at least one channel pair,
                                // so we have succeeded as far as MAY_BE_UNROUTED
                                // and MUST_ROUTE_ONE_CHANNEL_PAIR are concerned.
                                // Note that we end up with a partially
                                // routed stream. I'm still on the fence as to
                                // whether this is the desired behavior for
                                // MAY_BE_UNROUTED or whether we should make
                                // that an all-or-nothing condition.
                                Printf(Tee::PriDebug, "All routes not routed based on routing requirements\n");
                                break;
                            }
                        }
                        break;
                    default:
                        Printf(Tee::PriError, "Unknown routing requirement type: %d!\n",
                               input.routingRequirements);
                        return RC::BAD_PARAMETER;
                }
                // If we found a set of valid routes, we want to exit this loop
                if (m_vpRoutes.size() > 0)
                {
                    break;
                }
            }
        }
    }

    if (input.routingRequirements == StreamProperties::MAY_BE_UNROUTED)
    {
        // If it may be unrouted, we always succeed, regardless of
        // whether or not we found a route
        *success = true;
    }
    return OK;
}

RC AzaStream::PrintInfo(Tee::Priority Pri)
{
    RC rc;

    Printf(Pri, "\nAzalia Stream %d\n", m_SwStreamNumber);
    Printf(Pri, "    %s Stream\n", (m_Dir == AzaliaDmaEngine::DIR_INPUT) ?
           "INPUT" : "OUTPUT");
    Printf(Pri, "    Reservation: %d, Codec %d, Pin %d\n",
           input.reservation.reserved, input.reservation.codec,
           input.reservation.pin);
    Printf(Pri, "    Loop %d, Last Position %d\n", m_status.loop,
           m_status.lastPos);
    if (m_pFormat)
    {
        m_pFormat->PrintInfo(Pri);
    }
    if (m_pEngine)
    {
        CHECK_RC(m_pEngine->PrintInfo(Pri==Tee::PriSecret));
    }
    if (m_vpRoutes.size() > 0)
    {
        for (UINT32 i=0; i<m_vpRoutes.size(); i++)
        {
            CHECK_RC(m_vpRoutes[i]->PrintInfo(Tee::PriDebug));
        }
    } else {
        Printf(Pri, "    Stream is not routed\n");
    }
    return OK;
}

RC AzaStream::VerifySettings()
{
    RC rc;

    UINT08 sNumber;
    CHECK_RC(m_pEngine->GetStreamNumber(&sNumber));
    for (UINT32 i=0; i<m_vpRoutes.size(); i++)
    {
        CHECK_RC(m_vpRoutes[i]->VerifyConfiguration(m_Dir, sNumber, 2*i,
                                                   m_pFormat));
    }

    // For GPU Azalia we do not check the input FIFO size since this is an
    // internal thing.
    if (m_pAza->IsGpuAza() && (m_Dir == AzaliaDmaEngine::DIR_INPUT))
        return OK;

    // Check for correct fifo size
    UINT16 expFifoSize = m_pEngine->ExpectedFifoSize(m_Dir, m_pFormat);
    UINT16 realFifoSize = m_pEngine->GetFifoSize();

    if (expFifoSize != realFifoSize)
    {
        Printf(Tee::PriError, "Fifo size (%d) does not equal expected (%d)\n",
               realFifoSize, expFifoSize);

        return RC::HW_STATUS_ERROR;
    }

    return OK;
}

UINT32 AzaStream::GetBufferLengthInBytes()
{
    return m_pBdl->GetBufferLengthInBytes();
}

UINT32 AzaStream::GetEndThresholdInBytes()
{
    return ((UINT32) (GetBufferLengthInBytes() * END_THRESHOLD));
}

UINT32 AzaStream::GetBufferLengthInSamples()
{
    return (UINT32)(GetBufferLengthInBytes() / m_pFormat->GetSizeInBytes());
}

UINT32 AzaStream::GetBufferLengthInBlocks()
{
    UINT32 samples = GetBufferLengthInSamples();
    UINT32 samplesPerBlock = m_pFormat->GetChannelsAsInt();
    MASSERT(!(samples % samplesPerBlock));
    return samples / samplesPerBlock;
}

RC AzaStream::AnalyzeBuffer(UINT32 ClickThreshPercent, UINT32 SmoothThreshPercent)
{
    RC rc;
    MASSERT(m_Stats);
    const UINT32 nSamples = GetBufferLengthInSamples();
    const UINT32 nBlocks = GetBufferLengthInBlocks();

    // Get the contents of the buffer
    vector<FLOAT32> pSamples(nSamples);
    vector<INT08> intSamples08(nSamples*m_pFormat->GetSizeInBytes());
    UINT32 nSamplesRead = 0;

    CHECK_RC(m_pBdl->ReadSampleList(0,
                                    &intSamples08[0],
                                    nSamples,
                                    &nSamplesRead,
                                    m_pFormat->GetSizeInBytes()));

    CHECK_RC(ColwertSamplesToFloats(pSamples,intSamples08,nSamplesRead));

    if (nSamples != nSamplesRead)
    {
        Printf(Tee::PriError, "Buffer read did not complete\n");
        return RC::FAILED_TO_COPY_MEMORY;
    }

    CHECK_RC(AudioUtil::AnalyzeSample(pSamples,
                                      nBlocks,
                                      m_pFormat->GetChannelsAsInt(),
                                      static_cast<AudioUtil::RATE>(m_pFormat->GetRateAsSamples()),
                                      m_FundPeriod,
                                      m_MaxHarmonics,
                                      m_Stats,
                                      ClickThreshPercent,
                                      SmoothThreshPercent,
                                      m_FftRatio));

    return rc;
}

RC AzaStream::CheckLoopback
(
    UINT32 srcChan,
    AzaStream* pDest,
    UINT32 destChan
)
{
    RC rc, toReturn;

    if (input.randomData)
    {
        Printf(Tee::PriDebug, "Directly comparing buffers with random data\n");
        Printf(Tee::PriDebug, "This stream pos is %d, dest stream pos is %d\n",
               GetLastPosition(), pDest->GetLastPosition());

        UINT32 nSamplesToSkip = (GetBufferLengthInBytes()
                               - GetEndThresholdInBytes())
                                /(m_pFormat->GetSizeInBytes());

        nSamplesToSkip = min(nSamplesToSkip,
                             (pDest->GetBufferLengthInSamples()
                            - pDest->GetEndThresholdInBytes())
                            /(pDest->GetFormat()->GetSizeInBytes()));

        CHECK_RC(CompareBufferContents(pDest, 0, nSamplesToSkip));
        return rc;
    }

    Printf(Tee::PriDebug, "Analyzing output buffer\n");
    CHECK_RC(AnalyzeBuffer(input.clickThreshPercent,
                           input.smoothThreshPercent));

    AudioUtil::SineStatistics* pSrcStat = m_Stats;

    bool isMono = false;
    // Check one or two channels (to account for stereo)
    for (UINT32 stereoCnt=0; stereoCnt<2; stereoCnt++)
    {
        UINT32 chan = srcChan + stereoCnt;
        if (chan >= m_pFormat->GetChannelsAsInt())
        {
            if (!stereoCnt)
            {
                Printf(Tee::PriError, "Loopback specified higher channel than exists in stream!\n");
                return RC::BAD_PARAMETER;
            }
            Printf(Tee::PriDebug, "Source stream is mono\n");
            isMono = true;
            break;
        }

        // Sanity check output buffer
        if (!pSrcStat[chan].IsValid)
        {
            if (input.failIfNoData)
            {
                Printf(Tee::PriError, "Output buffer for channel %d is too small to analyze\n",
                       chan);
                toReturn = RC::BAD_PARAMETER;
            }
            else
                Printf(Tee::PriDebug, "Output buffer for channel %d is too small to analyze\n",
                       chan);
            continue;
        }
        else if (pSrcStat[chan].CalcFreq == 0 && !pSrcStat[chan].IsSilent)
        {
            Printf(Tee::PriError, "Output buffer inconsistent. Expected silence, but found sound!\n");
            toReturn = RC::AZA_STREAM_ERROR;
            continue;
        }
        else if (pSrcStat[chan].IsSilent ||
                 pSrcStat[chan].CalcFreq <= (pSrcStat[chan].MeasFreq - pSrcStat[chan].MeasErr) ||
                 pSrcStat[chan].CalcFreq >= (pSrcStat[chan].MeasFreq + pSrcStat[chan].MeasErr) ||
                 !pSrcStat[chan].IsSmooth ||
                 !pSrcStat[chan].HasNoClicks)
        {
            Printf(Tee::PriError, "Output buffer is not clean, consistent sine wave!\n");
            toReturn = RC::AZA_STREAM_ERROR;
            continue;
        }
        Printf(Tee::PriLow, "Stream %d: Output channel %d, frequency %.2f\n",
               m_SwStreamNumber, chan, pSrcStat[chan].CalcFreq);
    }
    if (toReturn != OK)
    {
        char temp[32];
        sprintf(temp,"outstrm%d.pcm", m_SwStreamNumber);
        string filename = temp;
        Printf(Tee::PriLow, "Dumping output stream %d's buffer to file %s for analysis.\n",
               m_SwStreamNumber, filename.c_str());

        CHECK_RC(m_pBdl->ReadBufferToFile(filename.c_str(),
                                             m_pFormat->GetSizeInBytes()));

        m_pBdl->Print();
    }

    CHECK_RC(toReturn);

    Printf(Tee::PriDebug, "Analyzing input buffer\n");
    CHECK_RC(pDest->AnalyzeBuffer(pDest->input.clickThreshPercent,
                                  pDest->input.smoothThreshPercent));
    AudioUtil::SineStatistics* pDestStat = pDest->GetBufferStatistics();

    for (INT32 stereoCnt=0; stereoCnt<(isMono?1:2); stereoCnt++)
    {
        UINT32 dChan = destChan + stereoCnt;
        UINT32 sChan = srcChan + stereoCnt;
        if (dChan >= pDest->GetFormat()->GetChannelsAsInt())
        {
            if (!stereoCnt)
            {
                Printf(Tee::PriError, "Loopback specified higher channel than exists in stream!\n");
                return RC::BAD_PARAMETER;
            }
            Printf(Tee::PriDebug, "Dest stream is mono\n");
            if (!isMono)
            {
                Printf(Tee::PriError, "Source stream is not mono, but dest stream is stereo\n");
                return RC::SOFTWARE_ERROR;
            }
            break;
        }

        if (pSrcStat[sChan].CalcFreq == 0)
        {
            if (!pDestStat[dChan].IsSilent)
            {
                Printf(Tee::PriError, "Expected silence in input channel %d, found noise\n",
                       dChan);

                toReturn = RC::AZA_STREAM_ERROR;
                continue;
            }
            Printf(Tee::PriDebug, "Expected and found silence in input channel %d\n", dChan);
        }
        else if (pDestStat[dChan].IsSilent)
        {
            Printf(Tee::PriError, "Unexpected silence in input channel %d\n", dChan);
            toReturn = RC::AZA_STREAM_ERROR;
            continue;
        }
        else if (pDestStat[dChan].IsValid)
        {
            Printf(Tee::PriLow, "Stream %d: Input channel %d, frequency %.2f "
                                   "+- %.2f\n",
                   pDest->m_SwStreamNumber, dChan, pDestStat[dChan].MeasFreq,
                   pDestStat[dChan].MeasErr);

            if ((pSrcStat[sChan].CalcFreq <
                 (pDestStat[dChan].MeasFreq - pDestStat[dChan].MeasErr))
                || (pSrcStat[sChan].CalcFreq >
                    (pDestStat[dChan].MeasFreq + pDestStat[dChan].MeasErr)))
            {
                Printf(Tee::PriError, "Frequencies do not match!\n");
                toReturn = RC::AZA_STREAM_ERROR;
                continue;
            }
            if (!pDestStat[dChan].IsSmooth)
            {
                Printf(Tee::PriError, "Sine wave on input channel %d is not smooth!\n", dChan);
                toReturn = RC::AZA_STREAM_ERROR;
                continue;
            }
            if (!pDestStat[dChan].HasNoClicks)
            {
                Printf(Tee::PriError, "Click detected on input channel %d!\n", dChan);
                toReturn = RC::AZA_STREAM_ERROR;
                continue;
            }
        }
        else
        {
            Printf(Tee::PriError, "No valid audio data on input channel %d!\n", dChan);
            toReturn = RC::AZA_STREAM_ERROR;
            continue;
        }
    }

    if (toReturn != OK)
    {
        char temp[32];
        sprintf(temp,"instrm%d.pcm", pDest->m_SwStreamNumber);
        string filename = temp;
        Printf(Tee::PriLow, "Dumping input stream %d's buffer to file %s for analysis.\n",
               pDest->m_SwStreamNumber, filename.c_str());

        CHECK_RC(pDest->m_pBdl->ReadBufferToFile(filename.c_str(),
                                          pDest->GetFormat()->GetSizeInBytes()));

        pDest->m_pBdl->Print();
    }

    return toReturn;
}

void AzaStream::SetFftRatio(FLOAT64 value)
{
    m_FftRatio = value;
}

RC AzaStream::SetStreamProp(StreamProperties::strmProp* pProperties)
{
    RC rc;
    input.channels = pProperties->channels;
    input.size = pProperties->size;
    input.rate = pProperties->rate;
    input.type = pProperties->type;
    input.tp = pProperties->tp;
    input.nBDLEs = pProperties->nBDLEs;
    input.minBlocks = pProperties->minBlocks;
    input.maxBlocks = pProperties->maxBlocks;
    input.sync = pProperties->sync;
    input.outputAttenuation = pProperties->outputAttenuation;
    input.maxHarmonics = pProperties->maxHarmonics;
    input.clickThreshPercent = pProperties->clickThreshPercent;
    input.smoothThreshPercent = pProperties->smoothThreshPercent;
    input.randomData = pProperties->randomData;
    input.sdoLines = pProperties->sdoLines;
    input.codingType = pProperties->codingType;
    input.routingRequirements = pProperties->routingRequirements;
    input.reservation.reserved = pProperties->reservation.reserved;
    input.reservation.codec = pProperties->reservation.codec;

    // If default pin is set override it here
    if (pProperties->reservation.pin == 0xffffffff)
    {
        AzaliaCodec* pCodec = NULL;
        if ( (rc = m_pAza->GetCodec(input.reservation.codec, &pCodec)) != OK )
        {
            Printf(Tee::PriError, "Codec %d does not exist on the system\n",
                   input.reservation.codec);
            return rc;
        }

        // Get the default pin info from the codec
        const CodecDefaultInformation* pCDInfo = NULL;
        CHECK_RC(pCodec->GetCodecDefaults(&pCDInfo));

        if (m_Dir == AzaliaDmaEngine::DIR_INPUT)
            input.reservation.pin = pCDInfo->input;
        else if (m_Dir == AzaliaDmaEngine::DIR_OUTPUT)
            input.reservation.pin = pCDInfo->output;
    }
    else
        input.reservation.pin = pProperties->reservation.pin;

    input.streamNumber = pProperties->streamNumber;
    input.allowIocs = pProperties->allowIocs;
    input.intEn = pProperties->intEn;
    input.fifoEn = pProperties->fifoEn;
    input.descEn = pProperties->descEn;
    input.iocEn = pProperties->iocEn;
    input.formatSubstOk = pProperties->formatSubstOk;
    input.testDescErrors = pProperties->testDescErrors;
    input.testFifoErrors = pProperties->testFifoErrors;
    input.testPauseResume = pProperties->testPauseResume;
    input.truncStream = pProperties->truncStream;
    input.failIfNoData = pProperties->failIfNoData;
    input.testOutStrmBuffMon = pProperties->testOutStrmBuffMon;
    input.iocSpace = pProperties->iocSpace;
    input.filename = pProperties->filename;
    return rc;
}

AzaBDL* AzaStream::GetBdl()
{
    return m_pBdl;
}

RC AzaStream::SatisfyGranularity(UINT32 nBlocks, UINT32* nNewBlocks)
{
    RC rc = OK;

    // Satisfy granularity and 16-bit requirements by increasing nBlocks if necessary
    UINT32 granularity = m_BaseGranularity;
    AzaliaFormat::RATEMULT rMult = m_pFormat->GetRateMult();

    if ((rMult == AzaliaFormat::RATE_MULT_X4) && (granularity & 0x3))
    {
        // make granularity a multiple of 4
        granularity <<= 2;
    }
    else if ((rMult == AzaliaFormat::RATE_MULT_X2) && (granularity & 0x1))
    {
        // make granularity a multiple of 2
        granularity <<= 1;
    }
    if ((m_pFormat->GetSize() == AzaliaFormat::SIZE_8)
        && (m_pFormat->GetChannels() & 0x1)
        && (granularity & 0x1))
    {
        granularity <<= 2;
    }

    *nNewBlocks = nBlocks + granularity - (nBlocks % granularity);

    if ((*nNewBlocks != nBlocks) && ((*nNewBlocks - nBlocks) != granularity))
    {
        Printf(Tee::PriLow, "NumBlocks increased from %d to %d to satisfy granularity and "
                            "16-bit requirements\n",
               nBlocks, *nNewBlocks);

        nBlocks = *nNewBlocks;
    }

    return rc;
}

void AzaStream::SetGranularity(UINT32 Gran)
{
    m_BaseGranularity = Gran;
}

UINT32 AzaStream::GetGranularity() const
{
    return m_BaseGranularity;
}

RC AzaStream::CompareBufferContents
(
    AzaStream* pOther,
    UINT32 nSkipAtBegin,
    UINT32 nSkipAtEnd
)
{
    RC rc;
    MASSERT(pOther);

    const UINT32 nChannelsThis =  m_pFormat->GetChannelsAsInt();
    const UINT32 nSamplesThis =   GetBufferLengthInSamples();
    const UINT32 nChannelsOther = pOther->GetFormat()->GetChannelsAsInt();
    const UINT32 nSamplesOther =  pOther->GetBufferLengthInSamples();

    Printf(Tee::PriDebug, "Compare skipping %d samples at beginning and %d at end\n",
           nSkipAtBegin, nSkipAtEnd);

    if (nChannelsThis != nChannelsOther)
    {
        Printf(Tee::PriError, "Buffers do not have same number of channels!\n");
        return RC::DATA_MISMATCH;
    }

    UINT32 nSamplesToCompare = min(nSamplesThis, nSamplesOther);
    if (nSamplesToCompare <= nSkipAtEnd)
    {
        Printf(Tee::PriError, "Not enough samples!\n");
        return RC::DATA_MISMATCH;
    }

    nSamplesToCompare -= nSkipAtEnd;

    UINT32 nSamplesRead = 0;
    vector<FLOAT32> pSamplesThis(nSamplesToCompare);
    vector<INT08> intSamples08(nSamplesToCompare*m_pFormat->GetSizeInBytes());
    vector<INT08> intSamplesOther08(nSamplesToCompare*
                                    pOther->GetFormat()->GetSizeInBytes());

    CHECK_RC(m_pBdl->ReadSampleList(0,
                                    &intSamples08[0],
                                    nSamplesToCompare,
                                    &nSamplesRead,
                                    m_pFormat->GetSizeInBytes()));

    CHECK_RC(ColwertSamplesToFloats(pSamplesThis,intSamples08,nSamplesRead));

    if (nSamplesToCompare != nSamplesRead)
    {
        Printf(Tee::PriError, "Buffer read did not complete!\n");
        CHECK_RC(RC::FAILED_TO_COPY_MEMORY);
    }
    vector<FLOAT32> pSamplesOther(nSamplesToCompare);
    nSamplesRead = 0;

    CHECK_RC(m_pBdl->ReadSampleList(0,
                                   &intSamplesOther08[0],
                                   nSamplesToCompare,
                                   &nSamplesRead,
                                   pOther->GetFormat()->GetSizeInBytes()));

    CHECK_RC(ColwertSamplesToFloats(pSamplesOther,intSamplesOther08,nSamplesRead));

    if (nSamplesToCompare != nSamplesRead)
    {
        Printf(Tee::PriError, "Buffer read did not complete!\n");
        CHECK_RC(RC::FAILED_TO_COPY_MEMORY);
    }

    for (UINT32 i = nSkipAtBegin; i < nSamplesToCompare; i++)
    {
        Printf(Tee::PriDebug, "This[%d] = %f, Other[%d] = %f\n",
               i, pSamplesThis[i], i, pSamplesOther[i]);

        if (pSamplesThis[i] != pSamplesOther[i])
        {
            Printf(Tee::PriError, "Buffers do not match!\n");
            rc = RC::DATA_MISMATCH;
        }
    }
    return rc;
}

AudioUtil::SineStatistics* AzaStream::GetBufferStatistics()
{
    return m_Stats;
}

RC AzaStream::AllocateBuffer
(
    UINT32 nBDLEs,
    UINT32 nBlocks,
    UINT32 AddrBits,
    MemoryFragment::FRAGLIST* FragList

)
{
    RC rc = OK;
    UINT32 samplesPerBlock = m_pFormat->GetChannelsAsInt();
    UINT32 bytesPerSample = m_pFormat->GetSizeInBytes();
    UINT32 bytesPerBlock = samplesPerBlock * bytesPerSample;
    UINT32 totalSize =0;
    AllocParam numFrags;
    AllocParam fragSizes;
    AllocParam offsets;

    CHECK_RC(m_BufMgr->InitBufferAttr(nBDLEs,
                                      m_pFormat->GetSizeInBytes(),
                                      1 << 31,
                                      AddrBits,
                                      m_pAza->GetMemoryType()));

    // We ask the buffer manager to allocate an extra page for each
    // entry. Once its been allocated we later randomize the start
    // address across the page
    totalSize = nBlocks*(samplesPerBlock*m_pFormat->GetSizeInBytes());
    totalSize = totalSize + (nBDLEs * (Platform::GetPageSize()));

    Printf(Tee::PriDebug, "Total Size Allocated = %d", totalSize);

    // This is terrible API interface from Buffer Manager. Atleast there should be a
    // setter function to assign this, or it should be part of InitBufferAttr()
    // This is considering that we allocate the minimum size to all entries except
    // one entry which is the largest.
    UINT32 maxFragSize = totalSize
                            - ((nBDLEs-1) * (bytesPerBlock+Platform::GetPageSize()))
                            + Platform::GetPageSize();

    m_BufMgr->SetMaxFragSize(maxFragSize);

    Printf(Tee::PriDebug, "Maximum fragment size = %d\n", maxFragSize);

    CHECK_RC(m_BufMgr->AllocMemBuffer(totalSize,
                                      true,
                                      BDL_ALIGNMENT));

    // The BDLE list size is decided by the stream, so we keep it fixed.
    // The fragment sizes themselves can vary starting from the smallest size
    // possible - size of a single sample to the max size callwlated earlier
    // Each BDLE entry has to be aligned to 128 bytes and we disallow reshuffling
    // We also allocate an extra page so that we can randomize the address
    // later across a complete memory page. I am taking this from the previous code
    // and the spec also mentions having a buffer across a page
    numFrags.fixNum = nBDLEs;
    numFrags.minNum = nBDLEs;
    numFrags.maxNum = nBDLEs;
    fragSizes.maxNum = maxFragSize ;
    fragSizes.minNum = m_pFormat->GetSizeInBytes() + Platform::GetPageSize();
    offsets.maxNum = BDL_ALIGNMENT;
    offsets.minNum = BDL_ALIGNMENT;
    offsets.stepSize = 0;

    CHECK_RC(m_BufMgr->GenerateFragmentList(FragList,
                                            MODE_ALIGN | MODE_RND_FRAGSIZE,
                                            BDL_ALIGNMENT,
                                            totalSize,
                                            m_pFormat->GetSizeInBytes(),
                                            &numFrags,
                                            &fragSizes,
                                            &offsets,
                                            true,
                                            false));

    for (UINT32 i=0; i < FragList->size(); ++i)
    {
        // Also we eliminate the extra page size itself
        (*FragList)[i].ByteSize -= Platform::GetPageSize();

        // Now we randomize the start addresses across page
        Random rGen;
        rGen.SeedRandom(0x4321);
        UINT32 range = (Platform::GetPageSize()) / BDL_ALIGNMENT;
        UINT32 randomNumber = rGen.GetRandom(0, range-1);
        (*FragList)[i].Address = (void*)(((UINT08*)(*FragList)[i].Address)+
                                         (randomNumber * BDL_ALIGNMENT));

        Printf(Tee::PriDebug, "Entry %03d: a=0x%p (0x%08llx phys), l=0x%08x \n",
               i,
               (*FragList)[i].Address,
               (*FragList)[i].PhysAddr,
               (*FragList)[i].ByteSize);
    }

    // we clear all buffers to 0 before working on them.
    m_pBdl->SetBufferMgr(m_BufMgr);
    vector<UINT08> pattern;
    pattern.push_back(0);
    CHECK_RC(m_pBdl->WriteToBuffer(&pattern));

    return OK;
}

void AzaStream::ColwertFloatsToSamples(vector<INT08>& IntSamples,
                                       vector<FLOAT32>& FloatSamples)
{
    // Colwert the floats to ints of the proper size
    // I don't understand this colwersion, but it pulls directly from
    // Ed Riegelsberger's original algorithm. It claims to colwert the
    // floating-point numbers to two's complement integers.

    UINT32 nBytesPerSample = m_pFormat->GetSizeInBytes();
    UINT32 nSamples = static_cast<UINT32>(FloatSamples.size());
    IntSamples.resize(nSamples * nBytesPerSample);

    INT16* tempIntBuffer16 = reinterpret_cast<INT16*>(&((IntSamples)[0]));
    INT32* tempIntBuffer32 = reinterpret_cast<INT32*>(&((IntSamples)[0]));
    UINT32 i = 0;

    switch (m_pFormat->GetSize())
    {
        case AzaliaFormat::SIZE_8:
            for (i = 0; i < nSamples; ++i)
            {
                IntSamples[i] = static_cast<INT08> (FloatSamples[i] * 0x7f);
            }
            break;
        case AzaliaFormat::SIZE_16:
            for (i = 0; i < nSamples; ++i)
            {
                tempIntBuffer16[i] = static_cast<INT16> (FloatSamples[i] * 0x7fff);
            }
            break;
        case AzaliaFormat::SIZE_20:
            for (i = 0; i < nSamples; ++i)
            {
                tempIntBuffer32[i] = static_cast<INT32> (FloatSamples[i] * 0x7fffffff) & 0xfffff000;
            }
            break;
        case AzaliaFormat::SIZE_24:
            for (i = 0; i < nSamples; ++i)
            {
                tempIntBuffer32[i] = static_cast<INT32> (FloatSamples[i] * 0x7fffffff) & 0xffffff00;
            }
            break;
        case AzaliaFormat::SIZE_32:
            for (i = 0; i < nSamples; ++i)
            {
                tempIntBuffer32[i] = static_cast<INT32> (FloatSamples[i] * 0x7fffffff);
            }
            break;
        default:
            MASSERT(!"Invalid sample size");
            break;
    }
}

RC AzaStream::ColwertSamplesToFloats
(
    vector<FLOAT32>& FloatSamples,
    vector<INT08>& IntSamples08,
    UINT32 nSamplesRead
)
{
    RC rc = OK;
    INT16* intSamples16 = reinterpret_cast<INT16*> (&IntSamples08[0]);
    INT32* intSamples32 = reinterpret_cast<INT32*> (&IntSamples08[0]);

    // Once again, I don't understand this algorithm to colwert integer Samples
    // to floating-point values, but it's from Ed Riegelsberger's original
    // algorithm, and has something to do with 2's complement representation.
    UINT32 i = 0;
    switch (m_pFormat->GetSize())
    {
        case AzaliaFormat::SIZE_8:
            for (i=0; i<nSamplesRead; ++i)
            {
                FloatSamples[i] = static_cast<FLOAT32> (IntSamples08[i] / static_cast<float>(0x80));
            }
            break;
        case AzaliaFormat::SIZE_16:
            for (i=0; i<nSamplesRead; ++i)
            {
                FloatSamples[i] = static_cast<FLOAT32> (intSamples16[i] / static_cast<float>(0x8000));
            }
            break;
        case AzaliaFormat::SIZE_20:
            for (i=0; i<nSamplesRead; ++i)
            {
                INT32 data = intSamples32[i];
                if (data & 0xfff)
                {
                    Printf(Tee::PriWarn, "LSBs of 20-bit sample are non-zero (0x%08x)!\n", data);
                }
                FloatSamples[i] = static_cast<FLOAT32> ( data / static_cast<FLOAT32> (0x80000000));
            }
            break;
        case AzaliaFormat::SIZE_24:
            for (i=0; i<nSamplesRead; ++i)
            {
                INT32 data = intSamples32[i];
                if (data & 0xff)
                {
                    Printf(Tee::PriLow, "LSBs of 24-bit sample are non-zero (0x%08x)!\n", data);
                }
                FloatSamples[i] = static_cast<FLOAT32> ( data / static_cast<FLOAT32> (0x80000000));
            }
            break;
        case AzaliaFormat::SIZE_32:
            for (i=0; i<nSamplesRead; ++i)
            {
                FloatSamples[i] = static_cast<FLOAT32> ( intSamples32[i] / static_cast<FLOAT32> (0x80000000));
            }
            break;
        default:
            Printf(Tee::PriError, "Invalid size\n");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC AzaStream::WriteBufferWithSines
(
    UINT32 fund_period,
    FLOAT32 amplitude,
    UINT32 phase,
    UINT32 MaxHarmonics
)
{
    RC rc = OK;
    vector<INT08> intSamples;
    vector<FLOAT32> floatSamples;

    // Get the contents of the buffer
    CHECK_RC(AudioUtil::FillSampleWithSines(&floatSamples,
                                            &fund_period,
                                            amplitude,
                                            phase,
                                            m_pFormat->GetChannelsAsInt(),
                                            MaxHarmonics));

    CHECK_RC(AudioUtil::ColwertFloatsToIntegerSamples(floatSamples,
                                                      (AudioUtil::SAMPLESIZE)m_pFormat->GetSize(),
                                                      &intSamples));

    rc = m_pBdl->WriteBufferWithPattern(&intSamples[0],
                                        (UINT32)floatSamples.size(),
                                        true,
                                        m_pFormat->GetSizeInBytes());

    if (rc == OK)
    {
        m_FundPeriod = fund_period;
        m_MaxHarmonics = MaxHarmonics;
        m_Amplitude = amplitude;
    }

    return rc;
}

void StreamProperties::strmProp::Reset()
{
    *this = { };
}
