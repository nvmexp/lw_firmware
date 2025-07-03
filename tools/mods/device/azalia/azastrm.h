
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZASTRM_H
#define INCLUDED_AZASTRM_H

#include "azadmae.h"
#include "azafmt.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "azabdl.h"
#include "device/include/bufmgr.h"
#include "azautil.h"
#include "cheetah/utility/audio/audioutil.h"

#include <vector>

#include "device/azalia/tests/strmprop.h"

class AzaliaController;
class AzaliaDmaEngine;
class AzaliaRoute;

class AzaStream
{
public:
    enum InputArgType
    {
        //unsigned int
        IARG_CHANNELS=0,
        IARG_SIZE=1,
        IARG_RATE=3,
        IARG_TYPE=4,
        IARG_DIR=5,
        IARG_TP=6,
        IARG_MIN_NUM_BLOCKS=7,
        IARG_MAX_NUM_BLOCKS=8,
        IARG_NUM_BDLES=9,
        IARG_STRM_INDEX=10,
        IARG_STRM_NUMBER=11,
        IARG_MAX_HARMONICS=12,
        IARG_CLICK_THRESH=13,
        IARG_SMOOTH_THRESH=14,
        IARG_ROUTING_REQUIREMENTS=15,
        IARG_STRIPE_LINES=16,
        IARG_CODING_TYPE=17,
        IARG_CODEC=18,
        //string
        IARG_FILENAME=109,
        //bool
        IARG_SYNC=210,
        IARG_ALLOW_IOCS=211,
        IARG_INT_EN=212,
        IARG_FIFO_EN=213,
        IARG_DESC_EN=214,
        IARG_IOC_EN=215,
        IARG_FSUB_OK=216,
        IARG_TRUNC_STRM=217,
        IARG_TEST_DESC_ERRORS=218,
        IARG_TEST_PAUSE=219,
        IARG_TOLERATE_DRIFT=220,
        IARG_FAIL_IF_NODATA=221,
        IARG_RANDOM_DATA=222,
        IARG_TEST_OUT_STREAM_BUFF_MON=223,
        IARG_IOC_SPACE=224,
    };

    StreamProperties::strmProp input;

    AzaStream(AzaliaController* pAza, AzaliaDmaEngine* pEngine,
              AzaliaDmaEngine::DIR Dir, UINT32 SwStreamNumber = 0);
    ~AzaStream();

    RC SetDefaultParameters();
    RC Reset(bool ReleaseRoutes);
    RC ResetElapsedTime();

    RC ProgramStream();
    RC PrepareTest();

    UINT32 GetNLoops();
    UINT32 GetLastPosition();
    AzaliaDmaEngine::DIR GetDir();

    bool FifoErrOclwrred();
    bool StreamGotTruncated();
    bool DescErrorOclwrred();
    bool IsReserved();
    void Reserve(bool Reserve);

    void SetFftRatio(FLOAT64 value); //set ratio for signal energy pass/fail
    FLOAT64 GetFftRatio(){return m_FftRatio;}; //get ratio for signal energy pass/fail

    RC UpdateAndVerifyPosition(bool* pIsSuccess);
    RC VerifyPosition(bool* pIsSuccess, UINT32 Pos_now, UINT32 Pos_before,
                      UINT64 Clock, bool IsBcisSet);

    AzaliaDmaEngine* GetEngine();
    AzaliaFormat* GetFormat();

    RC FindRoute(bool* success);
    RC PrintInfo(Tee::Priority Pri);

    RC VerifySettings();

    RC CheckLoopback(UINT32 SrcChan, AzaStream* pDest, UINT32 DestChan);
    RC SetStreamProp(StreamProperties::strmProp* pProperties);
    UINT32 GetBufferLengthInBytes();
    UINT32 GetEndThresholdInBytes();
    UINT32 GetBufferLengthInSamples();
    UINT32 GetBufferLengthInBlocks();
    UINT32 GetGranularity() const;
    void SetGranularity(UINT32 Gran);

    RC AnalyzeBuffer(UINT32 ClickThreshPercent, UINT32 SmoothThreshPercent);
    RC WriteBufferWithSines (UINT32 fund_period, FLOAT32 amplitude, UINT32 phase, UINT32 MaxHarmonics);
    RC SatisfyGranularity(UINT32 nBlocks, UINT32* nNewBlocks);
    RC CompareBufferContents(AzaStream* pOther, UINT32 nSkipAtBegin, UINT32 nSkipAtEnd);

    AzaBDL* GetBdl();
    AudioUtil::SineStatistics* GetBufferStatistics();

    RC ColwertSamplesToFloats(vector<FLOAT32>& FloatSamples,
                              vector<INT08>& IntSamples,
                              UINT32 nSamplesRead);
    void ColwertFloatsToSamples(vector<INT08>& IntSamples,
                                vector<FLOAT32>& FloatSamples);

protected:

    AzaliaFormat* m_pFormat;
    AudioUtil::SineStatistics* m_Stats;
    AzaBDL* m_pBdl;
    AzaBDL* m_pNewBdl;

private:

    RC AllocateBuffer(UINT32 nBDLEs,
                      UINT32 nBlocks,
                      UINT32 AddrBits,
                      MemoryFragment::FRAGLIST* FragList);

    AzaliaController* m_pAza;
    AzaliaDmaEngine* m_pEngine;
    BufferMgr* m_BufMgr;

    vector<AzaliaRoute*> m_vpRoutes;
    // This is the index into the stream controller array, and is purely for
    // colwenience/debugging purposes. It has nothing to do with how the
    // hardware is programmed. It is NOT the same as the StreamIndex (which
    // identifies the stream on the azalia link).
    UINT32 m_SwStreamNumber;

    FLOAT64 m_FftRatio; // Flag indicating if we are using the RMS value for pass/fail
    bool m_IsReserved;
    AzaliaDmaEngine::DIR m_Dir;
    UINT32 m_FundPeriod;
    UINT32 m_MaxHarmonics;
    FLOAT32 m_Amplitude;
    UINT32 m_BaseGranularity;

    struct
    {
        UINT64 timeOffset;
        UINT64 elapsedTime;
        UINT32 loop;
        UINT32 lastPos;
        UINT64 nextIocPos_UW;
        bool descErrOclwred;
        bool nextIocMayHaveBeenClearedAlready;
        bool descErrMsgPrinted;
        bool fifoErrTested;
        bool truncErrMsgPrinted;
    } m_status;
};

#endif // INCLUDED_AZASTRM_H

