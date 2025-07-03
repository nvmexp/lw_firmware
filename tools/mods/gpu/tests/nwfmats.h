/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file nwfmats.h
 * @brief Declare NewWfMatsTest in a header file so that the MCP SST can use it.
 */

#ifndef INCLUDED_NWFMATS_H
#define INCLUDED_NWFMATS_H

#include "gpumtest.h"
#include "core/utility/ptrnclss.h"
#include "gpu/utility/mapfb.h"
#include "gpu/include/nfysema.h"
#include "core/include/tasker.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/notifier.h"
#include "class/cla06fsubch.h"
#include "gpu/include/dmawrap.h"

class ChannelSubroutine;

//! WF MATS Memory Test.  MATS is short for Modified Algorithmic Test Sequence
//!
//! WF means "Wicked Fast"
//!
//! Based on an algorithm described in "March tests for word-oriented
//! memories" by A.J. van de Goor, Delft University, the Netherlands
//!
//! WF MATS is similar to MATS.  It divides the memory space into "tiles".
//! (If you watch the test run, you'll know what I mean.)  The tiles are
//! divided into two "sets".
//!
//! Each of the tiles in the first "set" is filled with a different heterogeneous
//! pattern.  The pushbuffer is then repeatedly blits these tiles
//! amongst one another.  The algorithm that generates the "from" and "to" tile
//! numbers is designed so that at the end of the loop, every tile will
//! be in its original place.
//!
//! The second set of tiles are tested using the plain-old mats algorithm.
//! This test is more stressful than it used to be because of the background
//! blits are still going on while the mats algorithm is running.
//!
//! When that is done, the pushbuffer blits are stopped.  Since each tile in the
//! set is now in its original location, you can check them against "known-good"
//! values.  This check is done and any errors are logged.

class NewWfMatsTest : public GpuMemTest
{
public:

    NewWfMatsTest();
    virtual ~NewWfMatsTest();

    //! This test operates on all GPUs of an SLI device in parallel.
    virtual bool GetTestsAllSubdevices()
    {
        return true;
    }

    virtual bool IsSupported();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    enum Method
    {
        Use2D,
        UseCE,
        UseVIC
    };

private:
    enum
    {
        s_TwoD = LWA06F_SUBCHANNEL_2D, // = 3
        s_CopyEng = LWA06F_SUBCHANNEL_COPY_ENGINE, // = 4
    };

    PatternClass    m_PatternClass;
    UINT32          m_PatternCount;
    PatternClass::PatternSets m_PatternSet;
    UINT32          m_CpuPattern;
    UINT32          m_CpuPatternLength;

    MapFb m_MapFb;

    Random * m_pRandom;
    GpuTestConfiguration *m_pTestConfig;

    UINT32 m_LwrrentReadbackSubdev;

    FLOAT64 m_TimeoutMs;

    struct Box
    {
        Box() : FbOffset(0), CtxDmaOffset(0), CtxDmaHandle(0), StartingPartChan(0) {}
        ~Box() {}
        UINT64       FbOffset;
        UINT64       CtxDmaOffset;
        UINT32       CtxDmaHandle;
        UINT32       StartingPartChan;
    };
    vector<Box> m_BoxTable;

    // Box height can be set through JavaScript.
    // Width is fixed for the decode algorithm to work correctly.
    static const UINT32 c_BoxWidthPixels = 1024;
    static const UINT32 c_BoxWidthBytes  = 4 * c_BoxWidthPixels;

    // Parameters below are setting up the TwoD class to treat a box
    // as a surface "TwoDTransferWidth" wide. The effect is the same as if
    // a pitch linear transfer was done with c_BoxWidthPixels.
    // Value TwoDTransferWidth = 16 (single gob) seems to be less
    // stressful then 2 gobs. Going higher doesn't look to make a difference
    // up to c_BoxWidthPixels, where block linear TwoD transfers are colliding
    // with pitch linear transfers from the plain old mats.
    static const UINT32 c_TwoDTransferWidth = 32; // 2 GOBs width for Y32
    static const UINT32 c_TwoDTransferHeightMult =
            c_BoxWidthPixels/c_TwoDTransferWidth;

    UINT32  m_BoxHeight;
    UINT32  m_BoxPixels;
    UINT32  m_BoxBytes;
    UINT32  m_DecodeSpan;
    FLOAT64 m_BoxesPerMegabyte;

    // This is normally 1 (copy a box with one blit), but is
    // more for the "narrow" modes.
    UINT32  m_BlitsPerBoxCopy;

    //! Boxes are separated into 1 or more groups where all boxes w/in the
    //! group have the same partition/channel decode at all offsets.
    //! Only boxes w/in the same group are blitted onto each other, so
    //! we can get accurate relative-address decodes for aclwmulated errors.
    //! We have one "blit loop" per partition/channel-decode group.
    UINT32 m_NumBlitLoops;

    //! This controls how much FB memory is tested by the CPU with pci r/w.
    //! The rest of FB memory is tested by the blit loop.
    //! This parameter (along with m_InnerLoops and m_OuterLoops) controls the
    //! duration of the test.
    UINT32 m_CpuTestBytes;
    UINT32 m_OuterLoops;
    UINT32 m_InnerLoops;

    UINT32 m_NumBoxes;      //!< Total number of boxes.
    UINT32 m_BlitNumBoxes;  //!< num in one of the blit loop(s), not counting spare
    UINT32 m_CpuNumBoxes;   //!< num to be tested with CPU r/w.

    //! Indices of boxes that will be used by the CPU read/write loop.
    //! Indices are in a shuffled order.
    vector<UINT32> m_CpuBoxIndices;

    //! One or more (m_NumBlitLoops) arrays of indices of boxes for blit loops.
    //! Indices are in a shuffled order, and if there are two loops, each only
    //! contains boxes that start with the same point in the FB-channel swizzle.
    vector< vector<UINT32> > m_BlitBoxIndices;

    //! This is the index of the box(es) used as temps by the blit-loop(s).
    //! Read and written each loop like all the other boxes, but
    //! only holds a temporary copy of the first/last box.
    //!
    //! There's no need to initialize these boxes, and we shouldn't count their
    //! errors, since they hold an exact copy (minus one read) of the data in
    //! the last box of a blit-loop.
    vector<UINT32> m_SpareBoxIndices;

    //! Since HandleError is called from PatternClass with only the offset
    //! within a box, we need to add the reported error address for errors
    //! found within the m_BlitBoxindices[1] boxes so that
    //! channel/subpartition/lane (same thing) comes out right in reports.
    UINT64 m_ExtraOffsetForHandleError;
    UINT32 m_LwrrentBlitLoop;

    //! This is the map used to store per patition/channel errors
    vector<UINT32> m_ErrCountByPartChan;
    vector<UINT32> m_ErrMaskByPartChan;

    //! This is a two dimensional array used to hold full partition/channel
    //! decode for a box belong to Blit loop. Full partition/channel decode
    //! would be same for all boxes in  same blit loop
    vector< vector<UINT08> >m_PartChanDecodesByBlitLoop;

    // Housekeeping data for KeepGpuBusy:
    UINT64 m_TimeBlitLoopStarted;
    UINT32 m_NumKeepBusyCalls;

    //! Track per-subdevice status of the blit loop:
    struct BlitLoopStatus
    {
        Surface2D SemaphoreBuf; //!< Sysmem semphore surface.
        UINT32  NumCompleted;   //!< Last semaphore value written by GPU.
        UINT32  NumSent;        //!< Num gpfifo buffers queued up so far
        UINT32  LwrPatShift;    //!< Shift in pattern (0 to m_PatternCount-1)
        UINT32  LwrXor;         //!< Ilwersion in pattern (0 or 1)
        UINT64  TimeDone;       //!< Timestamp at which blitloop finished.
        double  TargetAheadOnEntry;
        double  TargetPending;  //!< Control-loop variable.
    };
    //! Blit-loop status: Per-subdevice and per blit-engine.
    vector< vector<BlitLoopStatus> > m_BlitLoopStatus;

    Surface2D m_CompareBuf;
    bool m_CtxDmaOffsetFitIn32Bits;

    // Channel parameters.
    vector<Channel *>           m_pCh;
    vector<LwRm::Handle>        m_hCh;
    vector<ChannelSubroutine *> m_pChSubShift;
    ChannelSubroutine * m_pChSubXor;
    Channel *    m_pAuxCh;
    LwRm::Handle m_hAuxCh;
    VICDmaWrapper               m_VICDma;

    // HW class allocators
    TwodAlloc    m_TwoD;
    vector<DmaCopyAlloc> m_CopyEng;
    DmaCopyAlloc m_AuxCopyEng;

    // notifiers
    vector<Notifier> m_Notifier;
    NotifySemaphore m_AuxSemaphore;
    UINT32 m_AuxPayload; // Used to generate non-repeating sema payloads

    UINT32   m_ExitEarly;
    bool     m_UseXor;
    UINT32   m_NumCopyEngines;
    UINT32   m_Method;
    vector<UINT32> m_EnabledCopyEngine;
    UINT32   m_NumBlitEngines;
    UINT32   m_AuxMethod;
    bool     m_UseNarrowBlits;
    bool     m_LegacyUseNarrow;
    bool     m_DoPcieTraffic;
    bool     m_UseBlockLinear;
    bool     m_VerifyChannelSwizzle;
    bool     m_SaveReadBackData;
    bool     m_UseVariableBlits;
    UINT32   m_VariableBlitSeed;
    UINT32   m_MaxBlitLoopErrors;
    bool     m_DoDetach = true; // TODO Remove this when there are no issues

    struct SavedReadBackData
    {
        PHYSADDR        FbOffset;
        UINT32          PteKind;
        UINT32          PageSizeKB;
        vector<UINT32>  Data;

        SavedReadBackData()
        {
            FbOffset = 0;
            PteKind = 0;
            PageSizeKB = 0;
        };
    };
    vector<SavedReadBackData> m_ReadBackData;

    // Patterns for bitlane test

    //! Contains fixed user dword patterns.
    //! The outer array contains arrays for particular DRAM channel
    //! (partition*num_channels+channel is the index).
    //! The inner array contains a dword pattern. A particular bit in each
    //! dword corresponds to a bit expected on a particular bit lane (wire
    //! on the board). The pattern has a lenght which is a mulitple of
    //! burst length times the number of reversed bursts. The pattern
    //! already takes burst reversal into account. See the description of
    //! DecodeUserBitLanePatternForAll to find out what burst reversal is.
    vector<vector<UINT32> > m_BitLanePatterns;
    //! Contains box data for each blit-loop.
    //! The index to the outer array is blit-loop number.
    //! The inner array contains data for one box for the blit-loop it corresponds to.
    //! The purpose of this member is to keep box data for comparison in CheckBlitBoxes.
    vector<vector<UINT32> > m_BitLaneBoxDataByLoop;

    bool m_RetryHasErrors;
    bool m_LogError;

    // Not all tests will want to report the bandwidth, for example MscgMatsTest
    bool m_ReportBandwidth;

    struct VICCopy
    {
        VICCopy(UINT32 srcHandle, UINT32 srcOffset, UINT32 dstHandle, UINT32 dstOffset)
            : srcHandle(srcHandle), srcOffset(srcOffset)
            , dstHandle(dstHandle), dstOffset(dstOffset)
        {
        }

        UINT32 srcHandle;
        UINT32 srcOffset;
        UINT32 dstHandle;
        UINT32 dstOffset;
    };

    vector<VICCopy> m_VICCopies;

private:
    RC InitSystemStressTest();

    // Do the CPU reads and writes portion of the test
    RC DoPlainOldMats(UINT32 iteration);

    RC CheckBlitBoxes(UINT32 subdev);

    // Allocate the graphic surfaces instantiate the graphics objects....etc
    RC AllocFb();
    RC AllocSysmem();
    RC AllocAdditionalCopyEngineResources();
    RC CalcNumCopyEngines();

    RC SetupGraphics();
    RC SetupGraphicsBlit();
    RC SetupGraphicsTwoD();
    RC SetupCopyEngine(UINT32 idx);
    RC SetupVIC();

    RC CalcBoxHeight();
    RC BuildBoxTable();
    UINT32 CalcPartChanHash(UINT64);
    string GetUserFriendlyPartChanHash(UINT32 hash);

    RC DoWfMats(UINT32 iteration);

    // Routines for filling ChannelSubroutines.
    void FillChanSubCopyEngine();
    void FillChanSubTwoD();
    void PrepareVICBoxes();

    typedef void (NewWfMatsTest::*tBoxCopyFnp)(ChannelSubroutine * pChSub,
                                               UINT32 SourceBox,
                                               UINT32 DestBox);
    void FillChanSubCommon(tBoxCopyFnp BoxCopyFnp);

    void BoxCopyEngine(ChannelSubroutine * pChSub, UINT32 SourceBox, UINT32 DestBox);
    void BoxTwoD(ChannelSubroutine * pChSub, UINT32 SourceBox, UINT32 DestBox);
    void BoxVIC(ChannelSubroutine * pChSub, UINT32 SourceBox, UINT32 DestBox);
    void SetRopXorTwoD(ChannelSubroutine * pChSub, bool UseXor);
    void SetCtxDmaTwoD(ChannelSubroutine * pChSub);

    // fill or check a box with a specific color
    void FillBox(UINT32 BoxNum, UINT32 Color, INT32 Direction);
    RC FillBlitLoopBoxes();

    RC CheckFillBox(UINT32 BoxNum, UINT32 Check, UINT32 Fill, INT32 Direction);

    RC TransferBoxToSystemMemory(UINT32 BoxNum, UINT32 BufNum, UINT32 subdev);
    RC TransferBoxToSystemMemoryCE(UINT32 BoxNum, UINT32 BufNum, UINT32 subdev);
    RC TransferBoxToSystemMemoryVIC(UINT32 BoxNum, UINT32 BufNum);
    RC TransferSystemMemoryToBox(UINT32 BoxNum);
    RC TransferSystemMemoryToBoxCE(UINT32 BoxNum);
    RC TransferSystemMemoryToBoxVIC(UINT32 BoxNum);
    RC TransferSystemMemoryToBoxWait();
    RC TransferSystemMemoryToBoxCEWait();
    RC TransferSystemMemoryToBoxVICWait();
    RC WaitForTransfer();
    UINT32 IncPayload() { return ++m_AuxPayload; }

    RC KeepGpuBusy();
    RC SendSubroutines(UINT32 * numToSend);
    RC SendFinalXorsIfNeeded();
    RC SendSubroutine(UINT32 subdevMask, ChannelSubroutine * pChSub, UINT32 idx);
    RC SendVICCopies();
    void InitBlitLoopStatus();
    void ReportBandwidth(Tee::Priority pri);

    RC GetRandomBlitWidths(vector<UINT32>* BlitWidths, UINT32 TotalWidth);
    UINT32 GuessBlitLoopSeconds();
    bool IsMemorySame(const UINT32 *buf1, const UINT32 *buf2);

protected:
    // Overridden by MscgMatsTest to sleep between sending subroutines
    virtual RC SubroutineSleep();

    RC RunLiveFbioTuning();
    RC WaitForBlitLoopDone(bool doPcieTraffic, bool doneOldMats);

    // Set whether or not bandwidth should be reported to PVS
    void SetReportBandwidth(bool enable) { m_ReportBandwidth = enable; }

public:
    RC RunLiveFbioTuningWrapper();

    SETGET_PROP(PatternSet,        PatternClass::PatternSets);
    SETGET_PROP(CpuPattern,        UINT32);
    SETGET_PROP(CpuTestBytes,      UINT32);
    SETGET_PROP(OuterLoops,        UINT32);
    SETGET_PROP(InnerLoops,        UINT32);
    SETGET_PROP(BoxHeight,         UINT32);
    SETGET_PROP(ExitEarly,         UINT32);
    SETGET_PROP(UseXor,            bool);
    SETGET_PROP_LWSTOM(UseCopyEngine, bool);
    SETGET_PROP(NumCopyEngines,    UINT32);
    SETGET_PROP(Method,            UINT32);
    SETGET_PROP(AuxMethod,         UINT32);
    SETGET_PROP(UseNarrowBlits,    bool);
    SETGET_PROP(LegacyUseNarrow,   bool);
    SETGET_PROP(DoPcieTraffic,     bool);
    SETGET_PROP(UseBlockLinear,    bool);
    SETGET_PROP(VerifyChannelSwizzle, bool);
    SETGET_PROP(SaveReadBackData,  bool);
    SETGET_PROP(UseVariableBlits,  bool);
    SETGET_PROP(VariableBlitSeed,  UINT32);
    SETGET_PROP(MaxBlitLoopErrors, UINT32);
    SETGET_PROP(DoDetach,          bool); // TODO Remove this when there are no issues

    RC SetPatternSet( jsval * );
    RC SelectPatterns( vector<UINT32> * pVec );
    RC GetSavedReadBackData( UINT32 pattern,  jsval * pjsv );
    RC GetBestRotations( UINT32 maxPerChanPatLen, JsArray * pjsa );
    RC SetBitLanePatternForAll(const UINT08* pattern, UINT32 size);
    RC SetBitLanePattern(char partitionLetter, UINT32 bit,
            const UINT08* pattern, UINT32 size);

    static RC ApplyBurstReversal(vector<UINT08>* pPattern, FrameBuffer* pFB);
    static RC DecodeUserBitLanePatternForAll(GpuSubdevice* pGpuSubdevice,
            const UINT08* pattern, UINT32 size, vector<UINT08>* pOutPattern);
    static RC DecodeUserBitLanePattern(GpuSubdevice* pGpuSubdevice,
            const UINT08* pattern, UINT32 size, UINT32 expectedSize,
            vector<UINT08>* pOutPattern);

    //! PatternClass error-reporting callback (member function).
    RC HandleError
    (
        UINT32 offset,
        UINT32 expected,
        UINT32 actual,
        UINT32 patternOffset,
        const string &patternName
    );

    //! PatternClass error-reporting callback (nonmember function).
    static RC HandleError
    (
        void * pvThis,
        UINT32 offset,
        UINT32 expected,
        UINT32 actual,
        UINT32 patternOffset,
        const string &patternName
    );
};

extern SObject NewWfMatsTest_Object;

#endif // INCLUDED_NWFMATS_H
