/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2014,2016,2018-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Dma Copy Test: Tests multple copy engine instances conlwrrently.
// This is done by setting up each copy, making sure there is no
// overlap in source and destination offsets, exelwting them all, and
// waiting for all intances to complete.  The results of all copies
// are then checked.

#ifndef CLAS85BS_INCLUDED
#define CLAS85BS_INCLUDED

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_GOLDEN_H
#include "core/include/golden.h"
#endif
#ifndef INCLUDED_GPUTESTC_H
#include "gputestc.h"
#endif
#ifndef INCLUDED_FPK_COMM_H
#define INCLUDED_FPK_COMM_H
#include "gpu/js/fpk_comm.h"
#endif
#ifndef INCLUDED_DMASEMAPHORE_H
#include "gpu/include/nfysema.h"
#endif
#ifndef RANGE_PICKER
#include "gpu/include/rngpickr.h"
#endif
#ifndef INCLUDED_JSCRIPT_H
#include "core/include/jscript.h"
#endif
#ifndef INCLUDED_GRALLOC_H
#include "gpu/include/gralloc.h"
#endif
#ifndef INCLUDED_PEXTEST_H
#include "gpu/tests/pcie/pextest.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif
#ifndef INCLUDED_STL_STRING
#define INCLUDED_STL_STRING
#include <map>
#endif
#ifndef INCLUDED_STL_SET
#define INCLUDED_STL_SET
#include <set>
#endif

class CpyEngTest : public PexTest
{
public:
    CpyEngTest();
    virtual ~CpyEngTest();

    //! Copy and check, report HW pass/fail.
    virtual RC Setup() override;
    virtual RC RunTest() override;
    virtual RC Cleanup() override;

    virtual bool IsSupported() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;

    bool IsSupportedVf() override { return !(GetBoundGpuSubdevice()->IsSMCEnabled()); }

    // Set FancyPicker default values.
    RC SetDefaultsForPicker(UINT32 idx) override;

    // Get the total number of errors for the previous run (only relevant
    // when we're logging errors)
    UINT32 GetTotalErrorCount() const;

    bool ParseUserDataPattern(const JsArray &ptnArray);
    void AddToUserDataPattern(UINT32 val);
    void PrintUserDataPattern(Tee::Priority pri) const;
    void ClearUserDataPattern() { m_UserDataPattern.clear(); }

    enum WhichMem
        {
            SrcMem,
            DstMem,
            SysMem
        };

    enum SurfaceT
        {
            FB_LINEAR,
            FB_TILED,
            COHERENT,
            NON_COHERENT,
            FB_BLOCKLINEAR,
            NUM_SURFACES
        };

    void SeedRandom(UINT32 s);

    void ForcePitchInEven (bool b) { m_ForcePitchInEven  = b; }
    void ForcePitchOutEven(bool b) { m_ForcePitchOutEven = b; }
private:
    // JavaScript configuration data (user preferences):
    GpuTestConfiguration * m_pTstCfg;
    UINT32            m_SurfSize;
    UINT32            m_MinLineWidthInBytes;

    UINT32            m_MaxXBytes;
    UINT32            m_MaxYLines;

    UINT32            m_PrintPri;
    UINT32            m_DetailedPri;
    bool              m_TestStep;

    UINT32            m_XOptThresh;
    UINT32            m_YOptThresh;

    bool              m_CheckInputForError;
    bool              m_DebuggingOnLA;
    UINT32            m_EngMask;
    bool              m_AllMappings;

    INT32            m_EngineCount; // Number of engines to test

    // Other private data:
    FancyPicker::FpContext * m_pFpCtx;
    FancyPickerArray *       m_pPickers;
    NotifySemaphore          m_Semaphore;
    UINT32                   m_SubdevMask;

    // Channel stuff
    vector<LwRm::Handle> m_hCh;
    vector<Channel *>    m_pCh;

    vector<DmaCopyAlloc> m_DmaCopyAlloc;
    vector<UINT32>       m_EngineList;

    // Keep track of which offsets are free for a given surface
    map<Surface2D *,RangePicker> m_mapDstPicker;
    vector<Surface2D>            m_pSurfaces;

    Surface2D         m_SrcCheckSurf;
    Surface2D         m_DstCheckSurf;

    vector<UINT32>    m_UserDataPattern;

    bool              m_CoherentSupported;
    bool              m_NonCoherentSupported;

    bool              m_ForcePitchInEven;
    bool              m_ForcePitchOutEven;
    static const UINT32 sc_DmaCopy;

    struct SurfDimensions
    {
        Surface2D *surf;
        Surface2D::Layout MemLayout;
        UINT32 offsetOrig;
        UINT32 Pitch;
        UINT32 Width;
        UINT32 Height;
        UINT32 GobHeight;
        UINT32 BlockWidth;
        UINT32 BlockHeight;
        UINT32 BlockDepth;
        UINT32 OriginX;
        UINT32 OriginY;
        UINT32 format;
        bool skipRun;  // indicates whether this copy should be skipped
    };

    struct CopyParams
    {
        UINT32 lengthIn;
        UINT32 lineCount;
    };

    vector<SurfDimensions>       m_srcDims;
    vector<SurfDimensions>       m_dstDims;
    vector<CopyParams>           m_testParams;

    //---------------------------------------------------------------------------
    // Error logging stuff
    //---------------------------------------------------------------------------

    struct ErrorInfo
    {
        UINT08* virtAddr;
        UINT32 expectedVal;
        UINT32 actualVal;
    };

    struct lterr
    {
        bool operator()(const ErrorInfo &e1, const ErrorInfo &e2) const
        {
            if((e1.virtAddr < e2.virtAddr) ||
               (e1.expectedVal < e2.expectedVal) ||
               (e1.actualVal < e2.actualVal))
            {
                return true;
            }

            return false;
        }
    };

    bool m_LogErrors;
    map<ErrorInfo, UINT32, lterr> m_ErrorLog;
    typedef map<ErrorInfo, UINT32, lterr>::iterator ErrIterator;
    UINT32 m_TotalErrorCount;

    UINT32 m_DumpChunks;
    UINT32 m_NumLoopsWithError;

    // Private functions:
    RC InitModeSpecificMembers();

    //! Fill surfaces used by tests
    RC FillSurfaces();
    //! Get Engine Count and set vectors to number of CE instances
    RC InitializeEngines(const vector<UINT32>& engineList);
    RC CleanupEngines();
    RC DoDmaTest(const vector<UINT32>& engineList);
    UINT32 ValidatePick(UINT32 pick);
    void ForcePitchEvenIfApplicable(UINT32 which, UINT32 *pitch, UINT32 minPitch);

    //! Run copy engine test
    RC CopyAndVerifyOneLoop
    (
        UINT32 id,
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 *lengthIn,
        UINT32 *lineCount,
        UINT32 pri
    );

    //! Perform all copy engine memory checks
    RC PerformAllChecks
    (
        const vector<UINT32>& engineList,
        UINT32 pri
    );

    //! Setup Channels for multiple CE instances
    RC SetupChannelsPerInstance(const vector<UINT32>& engineList);

    //! Exlude ranges
    bool ExcludeRanges
    (
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 lineCount,
        UINT32 lengthIn
    );

    // Set free ranges for source and destination surface
    void SetSrcAndDstRanges
    (
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 *srcStart,
        UINT32 *srcEnd,
        UINT32 *dstStart,
        UINT32 *dstEnd
    );

    //! Setup surface dimensions
    bool SetupSurfaceDims
    (
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 *lCount,
        UINT32 *length,
        UINT32 pri
    );
    //! Setup surface sizes
    bool SetSurfSizes
    (
        SurfDimensions *srcDims,
        UINT32 minPitch,
        UINT32 gobWidth,
        UINT32 gobHeight,
        UINT32 *lineCt,
        UINT32 FpkPitch,
        UINT32 FpkWidth,
        UINT32 FpkHeight,
        UINT32 lMaxYLines,
        UINT32 Size,
        UINT32 pri
    );

    void AdjustCopyDims
    (
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 *lCount,
        UINT32 *length,
        UINT32 srcStart,
        UINT32 srcEnd,
        UINT32 dstStart,
        UINT32 dstEnd
    );

    //! Write methods for copy engine
    RC WriteMethods
    (
        UINT32 id,
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 lengthIn,
        UINT32 lineCount,
        UINT32 pri
    );

    RC Write90b5Methods
    (
        UINT32 id,
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 lengthIn,
        UINT32 lineCount,
        UINT32 offsetIn,
        UINT32 offsetInUpper,
        UINT32 offsetOut,
        UINT32 offsetOutUpper,
        UINT32 pri
    );

    //! Set Surface offset or origin for copy regions
    void SetSurfOffset
    (
        SurfDimensions *Dims,
        UINT32 minOffset,
        UINT32 maxOffset,
        UINT32 lineCount,
        UINT32 lengthIn,
        UINT32 FpkOffset,
        UINT32 FpkOrigX,
        UINT32 FpkOrigY,
        UINT32 pri
    );

    //! Prepare for memory check
    RC SetupAndCallCheck
    (
        UINT32 id,
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 lineCount,
        UINT32 lengthIn,
        bool *ErrorInThisLoop,
        Tee::Priority detailed,
        UINT32 pri
    );

    //! Check memory for correct copy
    RC CheckResult
    (
        UINT32 id,
        UINT08 * AddressIn,
        UINT08 * AddressOut,
        SurfDimensions *srcDims,
        SurfDimensions *dstDims,
        UINT32 LengthIn,
        UINT32 LineCount,
        Tee::Priority pri,
        bool *errorThisLoop
    );
    //! Print copied regions; used for debugging only
    RC PrintResults
    (
        UINT08 * AddressIn,
        SurfDimensions *srcDims,
        UINT32 LengthIn,
        UINT32 LineCount,
        UINT32 idNum
    );

    RC AllocMem ();
    void FreeMem ();
    UINT32 GetBlockWidth(UINT32 pickIdx);

public:
    SETGET_PROP(SurfSize, UINT32);
    UINT32 GetErrorCount() const { return m_TotalErrorCount; }
    SETGET_PROP(MinLineWidthInBytes, UINT32);
    SETGET_PROP(MaxXBytes, UINT32);
    SETGET_PROP(MaxYLines, UINT32);
    SETGET_PROP(PrintPri, UINT32);
    SETGET_PROP(DetailedPri, UINT32);
    SETGET_PROP(TestStep, bool);
    SETGET_PROP(XOptThresh, UINT32);
    SETGET_PROP(YOptThresh, UINT32);
    SETGET_PROP(CheckInputForError, bool);
    SETGET_PROP(DebuggingOnLA, bool);
    SETGET_PROP(DumpChunks, UINT32);
    SETGET_PROP(LogErrors, bool);
    SETGET_PROP(EngMask, UINT32);
    SETGET_PROP(AllMappings, bool);
};

#endif
