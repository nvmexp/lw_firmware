/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012, 2016-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file golden.h
//! \brief Declares the Goldelwalues class.

#ifndef INCLUDED_GOLDEN_H
#define INCLUDED_GOLDEN_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif

#ifndef INCLUDED_SCRIPT_H
#include "script.h"
#endif

#ifndef INCLUDED_GOLDDB_H
#include "golddb.h"
#endif

#ifndef INCLUDED_TEE_H
#include "tee.h"
#endif

#ifndef INCLUDED_COLOR_H
#include "color.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#include <memory>

class Surface2D;
class GoldenSurfaces; // Defined later in this file.
class GpuDevice;
class CrcGoldenInterface;
class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

//------------------------------------------------------------------------------
//! \class Goldelwalues
//!
//! \brief  Check that screen buffers match correct values.
//!
//! On a known good system, exercise the graphics engine then grab a
//! "golden value" from the screen buffer (i.e. a checksum, CRC, etc).
//!
//! Later you can run the same operations under other conditions (different
//! card, faster memory clock, etc) to see if you get the same values.
//!
//! We handle creating, storing, retreiving, and comparing golden values.
//!
//! We support many coding schemes, including two different memory CRC methods,
//! memory checksums, the DAC hardware CRC during scanout, etc.
//! These codes can be combined, and interleaved.
//!
//! The golden values are stored in a database.  See golddb.h, class GoldenDb.
//!
//! Also:
//!
//! The Goldelwalues class has become a place to store common code that should
//! be exelwted on an error, or when storing new CRCs, etc.  Most of this stuff
//! should probably be moved out of the core Goldelwalues class someday.
//! Refer to bug 216793 for rewrite dislwssions.
//!
class Goldelwalues
{
public:
    Goldelwalues();
    ~Goldelwalues();

    //! Gets properties from the Golden object, then gets
    //! overrides from the testname.Golden object.
    RC InitFromJs
    (
        SObject & tstObj        //!< Your JS test object
    );
    RC InitFromJs
    (
        JSObject * tstObj       //!< Your JS test object
    );

    //! Print out information about JS accessable properties
    void PrintJsProperties(Tee::Priority pri);

    //! \brief Give Goldelwalues its GoldenSurfaces container object.
    RC SetSurfaces
    (
        GoldenSurfaces * pgs  //!< Pointer to the test's GoldenSurfaces object.
    );

    //! \brief Increment loop counter, check or generate CRCs as needed.
    RC Run();

    //! \brief Check overall error-rate failures, set summary JS properties.
    RC ErrorRateTest
    (
        SObject & tstSObj
    );

    //! \brief Check overall error-rate failures, set summary JS properties.
    RC ErrorRateTest
    (
        JSObject * tstSObj
    );

    bool IsErrorRateTestCallMissing();

    //! \brief Clear state associated with GoldenSurfaces.
    void ClearSurfaces();

    //! \brief Set the current-loop counter and db record index to same value.
    void SetLoop
    (
        UINT32 Loop
    );
    //! \brief Override the current-loop and db-record index counters.
    void SetLoopAndDbIndex
    (
        UINT32 Loop,
        UINT32 DbIndex
    );

    //! \brief Set a callback to print test state on error.
    void SetPrintCallback( void (*pFunc)(void *), void * pData );

    //! \brief Set a callback for generating the "Extra" code value.
    //!
    //! Used by GLRandom to get zpass pixel counts & Sbo checksums, for example.
    //!
    void AddExtraCodeCallback(const char * name, UINT32 (*pFunc)(void *),
                              void * pData);

    typedef void (*ErrCallbackFuncPtr)(void* pData, UINT32 loop, RC rc,
                                       UINT32 surf);

    //! \brief Set a callback for recording error loop & surf info.
    void AddErrorCallback( ErrCallbackFuncPtr, void * );

    //! \brief Set a callback to wait for GPU idle.
    //!
    //! The callback is run once just before grabbing a CRC to make sure that
    //! nothing is still drawing.  By default Goldelwalues *assumes* idle.
    //
    // @@@ We ought to move this functionality into GoldenSurfaces.
    void SetIdleCallback
    (
        RC (*pIdleCallback)(void *pCallbackObj, void * pCallbackData),
        void *pCallbackObj,
        void *pCallbackData
    );

    enum Action
    {
        Check       //!< normal mode -- check for errors.
        ,Store       //!< store these values as the new golden values
        ,Skip        //!< don't do anything.
    };
    enum Code
    {
        CheckSums  = 1 << 0 //!< do a set of component-wise checksums
        ,Crc        = 1 << 1 //!< do a cyclic redundancy check on color and zeta buffers
        ,DacCrc     = 1 << 2 //!< do a DAC CRC
        ,TmdsCrc    = 1 << 3 //!< do a TMDS/LVDS CRC
        // Unsused bit 4
        ,OldCrc     = 1 << 5 //!< use the CRC algorithm that matches the emulation golden values
        ,Extra      = 1 << 6 //!< extra code (test-specific)
        // Unused bit 7
        ,ExtCrc     = 1 << 8 //!< external device CRC
        ,END_CODE   = 1 << 9 // no such code, used in for loops inside golden.cpp
        ,SurfaceCodes = CheckSums|Crc|OldCrc
        ,DisplayCodes = DacCrc|TmdsCrc
        ,OtherCodes = Extra|ExtCrc
    };

    // optimization hint when accessing FB memory
    enum BufferFetchHint
    {
        opCpu       = 0 //!< use BAR0 mapped memory access
        ,opCpuDma   = 1 //!< use DMA to xfer from FB to CPU memory
    };

    // Method to use to callwlate the checksum/CRCs
    enum CalcAlgorithm
    {
        CpuCalcAlgorithm             = 0 //!< Find CRC of entire surface on CPU
        ,FragmentedCpuCalcAlgorithm  = 1 //!< Split the surface into smaller chunks, and let each CPU thread compute goldens in parallel
        ,FragmentedGpuCalcAlgorithm  = 2 //!< Mimic "FragmentedCpuCalcAlgorithm" on GPU
    };

    static void  PrintCodesValue(UINT32 codes, Tee::Priority);
    enum When
    {
        Never      =  0  //!< do (dumpTGA/pause/whatever) never
        ,OnStore    =  1  //!< do (...) on Store
        ,OnCheck    =  2  //!< do (...) on Check
        ,OnSkip     =  4  //!< do (...) on Skip
        ,OnError    =  8  //!< do (...) when Check fails due to too many bad pixels
        ,OnBadPixel = 16  //!< do (...) when Check passes, but there was a bad pixel
        ,Always     = 32  //!< do (...) on every loop
    };
    static void  PrintWhelwalue(UINT32 wval, Tee::Priority pri);
    enum TestMode
    {
        Oqa              //!< Outgoing Quality Assurance (after system assembly), most lenient
        ,Mfg              //!< Manufacturing (after board assembly)
        ,Engr             //!< Engineering (during board and GPU qualifications), most strict
    };
    enum { MaxSurfaces = 16 };
    enum { MaxTpcs     = 10 };  // Tesla only
    enum { NumElem     = ColorUtils::elNUM_USED_ELEMENT_TYPES };

    Action GetAction() const;
    UINT32 GetDumpPng() const;
    UINT32 GetSkipCount() const;
    UINT32 GetCodes() const;
    string GetName(void) const;
    bool   GetStopOnError() const;
    UINT32 GetSurfChkOnGpuBinSize() const;
    CalcAlgorithm GetCalcAlgorithmHint() const;

    RC OverrideSuffix(string NewSuffix);

    void SetYIlwerted
    (
        bool Y0IsBottom // If true, for error reporting, treat lowest row as Y=0
    );

    void BindTestDevice(TestDevicePtr pTestDevice){ m_pTestDevice = pTestDevice; }
    GpuDevice *GetGpuDevice() const;
    TestDevicePtr GetTestDevice() const { return m_pTestDevice; }

    //! Set the mask of which subdevices to test (by default, loops over all).
    void SetSubdeviceMask(UINT32 mask);
    void SetAction(Action action);
    void SetSkipCount(UINT32 skipCount);

    void SetCheckCRCsUnique(bool enable);
    void SetSurfChkOnGpuBinSize(UINT32 SurfChkOnGpuBinSize);

    bool GetRetryWithReducedOptimization(const RC &rc) const;

    RC IsGoldenPresent
    (
        UINT32 LoopOrDbIndex,
        bool   bDbIndex,
        bool  *pbGoldenPresent
    );

    int GetNumSurfaceChecks() const;
    //! For tests that create copies of `Goldelwalues`. This function updates
    //! `m_NumSurfaceChecks`, so later `GpuTest::Cleanup` can correctly report
    //! `NumCrcs` metric.
    void Consolidate(const Goldelwalues &anotherGv)
    {
        m_NumSurfaceChecks += anotherGv.GetNumSurfaceChecks();
    }
    void SetDeferredRcWasRead(bool bWasRead)
    {   
        m_DeferredRcWasRead = bWasRead;
    }
private:

    enum  { m_MaxCodeBins = 512 };

    //! ToDo: Simplify action/when controls!
    //! ToDo: We need something flexible & orthogonal ilwolving function ptrs & callbacks...

    // Controls (set from JavaScript, per-test)
    string       m_TstName;             //!< for debug and error messages and database record headers
    string       m_PlatformName;        //!< for database record headers
    string       m_NameSuffix;          //!< for database record headers that need to be unique (e.g. for DACs)
    Action       m_Action;              //!< check/store/skip
    UINT32       m_SkipCount;           //!< loops to skip between checks or stores
    UINT32       m_Codes;               //!< csum/crc
    UINT32       m_ForceErrorMask;      //!< xor'd with the first 4 bytes of buffer
    UINT32       m_ForceErrorLoop;      //!< what loop to force the error on
    BufferFetchHint m_BufferFetchHint;  //!< Optimization hint for GoldenSurface access to FB memory.
    CalcAlgorithm m_CalcAlgoHint;       //!< Algorithm to use for callwlating Goldens
    UINT32       m_DumpTga;             //!< when (if ever) to dump a .TGA file
    UINT32       m_DumpPng;             //!< when (if ever) to dump a .PNG file
    vector< vector<UINT08> > m_SurfaceDataToDump; //!< Copy of surface data used for dumping to TGA/PNG file
    UINT32       m_Print;               //!< Call the print callback when a miscompare oclwrs.
    UINT32       m_Interact;            //!< when (if ever) to start an interactive prompt.
    bool         m_UseCrcFile;          //!< if true, get CRCs from file.  if false, use JavaScript.
    string       m_CrcFileName;         //!< name of old-style (non-JavaScript) crc file to use
    string       m_TgaName;             //!< Base name for dumped TGA files.
    UINT32       m_NumCodeBins;         //!< Pixel -> codebin interleave factor.
    //!<   Try to set this to a number 5x to 10x your MaxBadBinsOneCheck limit,
    //!<   and pick a number that doesn't divide evenly into your screen width.
    UINT32       m_AllowedBadBinsOneCheck;     //!< Allowed bad bins in any one check.
    UINT32       m_AllowedBinDiff[NumElem];    //!< Allowed checksum difference, one bin, one check, for r,g,b,a,d,s,o
    UINT32       m_AllowedErrorBinsOneCheck;   //!< Allowed bins with too-big checksum differences, one check
    UINT32       m_AllowedExtraDiffOneCheck;   //!< Allowed difference in Extra code values, one check
    bool         m_StopOnError;         //!< Stop the test when a Allowed*OneCheck limit is exceeded (else continue to end of test before returning error)
    bool         m_TgaAlphaToRgb;       //!< if true, copy alpha (stencil) channel to r,g,b in dumping .TGAs.
    bool         m_PrintCsv;            //!< Print comma-delimited data for reading into Excel.
    bool         m_CheckDmaOnFail;      //!< Do a word-by-word compare on fail, see if the GPU DMA failed (very unlikely).
    bool         m_RetryDmaOnFail;      //!< Retry miscompares without dmacopy to hide DMA errors.
    bool         m_SendTrigger;         //!< Send a trigger on given loop or error so HW engineers can debug problems using a logic analyzer.
    UINT32       m_TriggerLoop;         //!< Send a trigger on the specified loop.
    UINT32       m_TriggerSubdevice;    //!< Send a trigger on the specified subdevice.
    UINT32       m_CrcDelay;            //!< Arbitrary delay in callwlating CRC for end user MODS.
    bool         m_CheckCRCsUnique;     //!< Checks that subsequent OnStore loops generate different values
    UINT32       m_SurfChkOnGpuBinSize; //!< Bin size for the compute shader surface check

    GoldenSurfaces * m_pSurfaces;

    // Runtime data
    UINT32           m_Loop;            //!< current loop (for triggering actions)
    UINT32           m_DbIndex;         //!< current loop (as database key)
    UINT32           m_RecOffsets[10];  //!< offsets of parts of the GoldenDb data
    GoldenDb::RecHandle m_DbHandle;     //!< Golden database handle
    UINT32 *         m_CodeBuf;         //!< buffer for CRC values
    void           (*m_pPrintFunc)(void *);   // Callback to test for printing state.
    void *           m_PrintFuncData;   // Data for the callback
    UINT32           m_NumChecks;       // Number of checks since last Reset()
    RC               m_DeferredRc;      // Failure code from Run(), deferred until ErrorRatTest() because m_StopOnError is false.
    bool             m_DeferredRcWasRead;
    RC             (*m_pIdleFunc)(void *, void *); // Callback to make sure nothing is still drawing when we're trying to grab a CRC
    void *           m_pIdleCallbackData0;
    void *           m_pIdleCallbackData1;
    bool             m_YIlwerted;    //!< When reporting error locations, Y0 is bottom.
    UINT32           m_SubdevMask;
    int              m_NumSurfaceChecks; //!< For MODS test performance regression checks.

    // Callback functions for reporting errors.
    vector<ErrCallbackFuncPtr>   m_ErrCallbackFuncs;
    vector<void*>                m_ErrCallbackData;

    TestDevicePtr m_pTestDevice;
    struct ExtraCallbacks
    {
        string   name;   // Per-test Extra code name, i.e. "zpass pixel count".
        UINT32   (*pFunc)(void*); // Per-test Extra code function.
        void *   data;   // Per-test data for m_pExtraCodeFunc.
    };

    vector<ExtraCallbacks> m_Extra;
    struct ExtraValues
    {
        INT32 expected = 0; // expected value from the ExtraCallback function
        INT32 actual = 0;   // actual value from teh ExtraCallback function
        INT32 diff = 0;     // difference between expected vs actual could be callback specific.
        ExtraValues() = default;
        ExtraValues(INT32 e, INT32 a, INT32 d):
            expected(e), actual(a), diff(d)
        {
        }
    };
    struct MiscompareDetails
    {
        vector<bool> badBins;
        vector<bool> errBins;
        UINT32       sumDiffs[NumElem];
        vector<ExtraValues>extraDiff;
        UINT32       badTpcMask;

        MiscompareDetails (int numBins = 0, int numExtra = 0);
        void Accumulate (const MiscompareDetails & that);
        int numBad() const;
        int numErr() const;
        int numExtraDiff() const;
    };
    map<UINT32, MiscompareDetails> m_AclwmErrsBySurf;

    struct ErrHandlerArgs
    {
        MiscompareDetails * pMD;
        UINT32              surf;
        Goldelwalues::Code  code;
        const UINT32 *      pNew;
        const UINT32 *      pOld;
        UINT32              bin;
        UINT32              elem;
    };

    typedef bool (Goldelwalues::*tErrHandlerFnp)(ErrHandlerArgs &Arg);
    typedef bool (Goldelwalues::*tFormatSupportedFnp)(ColorUtils::Format fmt);
    typedef int  (Goldelwalues::*tNumBinsFnp)(int surfNum);
    typedef int  (Goldelwalues::*tNumElemsFnp)(ColorUtils::Format fmt);

    struct CodeDesc
    {
        const Goldelwalues::Code  code;         // enumerated value for this algorithm
        const char * szName;        // ASCII name for this algorighm
        const int    rqDisp;        // if true a display device is required.
        const bool   useRot;        // if true use the rotation algorithm to
                                    // check for miscompares.
        tFormatSupportedFnp pFormatSupported;
        tErrHandlerFnp pErrHndlr;
        tNumBinsFnp pNumBins;
        tNumElemsFnp pNumElems;
    };

    static const CodeDesc s_Codex[];
    // Private functions

    bool CksumErrHandler(ErrHandlerArgs &Arg);
    bool CrcErrHandler(ErrHandlerArgs &Arg);
    bool DacCrcErrHandler(ErrHandlerArgs &Arg);
    bool TmdsCrcErrHandler(ErrHandlerArgs &Arg);
    bool GenErrHandler(ErrHandlerArgs &Arg);
    bool ZpassErrHandler(ErrHandlerArgs &Arg);
    bool ExtCrcErrHandler(ErrHandlerArgs &Arg);

    bool NoFFmt( ColorUtils::Format fmt);
    bool AllFmts( ColorUtils::Format fmt);
    int OneBin(int surfIdx);
    int MultiBins(int surfIdx);
    int ExtraBins(int surfIdx);
    int ExtCrcBins(int surfIdx);
    int PixelElems(ColorUtils::Format fmt);
    int DacCrcElems(ColorUtils::Format fmt);
    int TmdsCrcElems(ColorUtils::Format fmt);
    int ZpassElems(ColorUtils::Format fmt);
    int ExtCrcElems(ColorUtils::Format fmt);
    RC CheckVal
    (
      int            bufIndex,   // color or Z
      const UINT32 * newVals,    // array of new CRC & checksums
      const UINT32 * oldVals,    // array of expected CRC & checksums
      MiscompareDetails * pmd    // new MiscompareDetails struct to fill in
    );

    RC   StoreVal(UINT32*);       // callwlate and store as new golden
    void Reset();                 // set all vars to defaults
    RC           GetCrcsFromFile(const vector<string> & GoldenNames); // parse .crc file, store CRCs into database.
    UINT32       CalcRecordOffsetsAndSize();
    UINT32       GetRecordOffset( Code code, int bufIndex);

    //! \brief Used by Run to callwlate all CRCs/checksums/etc for one surface.
    //!
    //! All results for all the enabled codes for this surface are stored in
    //! m_CodeBuf for comparing or storing later.
    //! Assumes that the surface has been properly cached (dma'd back) already.
    RC CheckSurface
    (
        int surfIdx,
        UINT32 subdevIdx,
        vector<UINT08> *surfDumpBuffer
    );

    //! \brief Used by Run to print detailed error data to the logfile.
    //!
    //! Spreadsheets can import Comma-Separated-Variable encoded data easily.
    //! Useful for analysing error patterns for HW failure analysis and for
    //! choosing "error tolerance" fudge factors.
    void PrintCsv (const MiscompareDetails & md);

    RC DumpImage
    (
        int      bufIndex,
        UINT32   whenMask,
        bool     png,
        UINT32   subdevNum
    );

    RC InnerRun();
    void InnerRunFailure(UINT32*);
    void InnerRunCleanup(const UINT32*);

    //! \brief Construct a GoldenDb key based on Codes, Name, TestName, etc.
    //! Called from SetSurfaces and from OverrideDisplaySuffix.
    RC OpenDb();

    // Class statics
    static UINT32 DummyExtraCodeFunc(void*);
};

RC GLFinishGoldenIdleCallback(void *nullPtr, void *nothing);

//------------------------------------------------------------------------------
// GoldenSurfaces
//
//! \brief A helper class for Goldelwalues, hides platform/surface details.
//!
//! GoldenSurfaces represents (1 or more) 2-dimensional arrays of pixels.
//! It hides messy and platform-specific details from Goldelwalues.
//!
//! We will only give Goldelwalues a direct pointer to the HW surface if:
//!  - Hint == opCpu, and
//!  - the surface is CPU-addressible, and
//!  - the surface is PitchLinear
//!
//! If the HW surface is not directly accessible by the CPU (i.e. WMP platform),
//! we will read it back to a host memory copy.
//!
//! Goldelwalues only understands pitch surfaces, so if the actual HW surface
//! is in some other format (i.e. BlockLinear), we will make a host-memory
//! copy of the surface in PitchLinear format.
//!
//! If Goldelwalues calls Cache() with Hint >= opCpuDma, we will use
//! whatever DMA HW is available to copy the surface to fast host memory.
//!
//! The CRC-testing order of Goldelwalues is controlled by GoldenSurfaces.
//! By convention, a 3d graphics test will have 2 surfaces (depth and color),
//! with depth as surface 0 so that it gets checked first.
class GoldenSurfaces
{
public:

    GoldenSurfaces(){};
    virtual ~GoldenSurfaces() {};

    //! \brief Get the number of separate surfaces to be CRC-checked.
    //!
    //! Return the number of surfaces represented by this object.
    //! Most 2d graphics tests will have 1 (the color buffer), but
    //! 3d tests will usually have 2 (color and depth buffers).
    //! Any number up to Goldelwalues::MaxSurfaces is allowed.
    virtual int NumSurfaces() const = 0;

    //! \brief Get a short name for the given surface.
    //!
    //! I.e. "C" or "Z"
    //! The name returned should contain no whitespace and be fairly short.
    //! It might be used by Goldelwalues for a database key, or just for
    //! error/status messages.
    virtual const string & Name (int surfNum) const = 0;

    //! \brief Verify the DMA of the most recently Cached surface.
    //!
    //! Do a byte-by-byte compare of the cached copy of the surface to the
    //! original and report any errors that were due to the HW-accellerated
    //! readback to the user.
    //! Return OK if surfaces match, RC::MEM_TO_MEM_RESULT_NOT_MATCH otherwise.
    virtual RC CheckAndReportDmaErrors
    (
        UINT32 subdevNum        //!< the subdevice to use
    ) = 0;

    //! \brief Return the address of copied or original data for this surface.
    //!
    //! This function may return a NULL pointer if the internal Caching fails.
    //  The pointer will be usable by the CPU to access data. It may be mapped
    //  to the actual HW surface, or it may be a copy of the surface sitting
    //  in the CPU address space.
    virtual void * GetCachedAddress(
        int surfNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 subdevNum,                               //!< the subdevice to use
        vector<UINT08> *surfDumpBuffer
    ) = 0;
    virtual void Ilwalidate() = 0;

    //! \brief Get the pitch of the surface.
    //!
    //! This is the pitch of the cached/colwerted copy, in the case that this
    //! differs from the pitch of the original HW surface, or it may be the
    //! actual HW surface pitch, if Address is returning the literal HW surface.
    //!
    //! Pitch means offset in bytes from pixel (x, y) to (x, y+1).
    //! Note that Width * BPP will be <= Pitch.  (Pitch may be padded).
    //!
    //! Note that Pitch may be negative!
    //! This oclwrs in Y-ilwerted (open-GL) surfaces where the earliest memory
    //! address is pixel (x_min, y_max).
    virtual INT32 Pitch(int surfNum) const = 0;

    //! \brief Get width of the surface in pixels (i.e. number of columns).
    virtual UINT32 Width(int surfNum) const = 0;

    //! \brief Get the height of the surface in pixels (i.e. number of rows).
    virtual UINT32 Height(int surfNum) const = 0;

    //! \brief Get the pixel format of the surface, an enum value.
    //!
    //! See the functions in ColorUtils for callwlating bytes-per-pixel from
    //! Format.
    virtual ColorUtils::Format Format(int surfNum) const = 0;

    //! \brief Get the display mask for this "surface" for DAC CRCs.
    //!
    //! In theory a multi-display DAC CRC test could treat the different
    //! displays as different surfaces, so that Goldelwalues can store
    //! separate crc data for each, per loop.
    virtual UINT32 Display(int surfNum) const = 0;

    //! \brief Get the external device of the surface, for external device CRCs.
    //!
    virtual CrcGoldenInterface *CrcGoldenInt(int surfNum) const { return nullptr; }

    //! \brief Return the Buffer Id if available, zero otherwise
    virtual UINT32 BufferId(int surfNum) const;

    //! \brief Fetch and Callwlate a CRC/CheckSum Buffer
    virtual RC FetchAndCallwlate
    (
        int surfNum,
        UINT32 subdevNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 numCodeBins,
        Goldelwalues::Code calcMethod,
        UINT32 * pRtnCodeValues,
        vector<UINT08> *surfDumpBuffer
    );

    //! \brief Attempt to reduce GPU/DMA Optimization in order to re-check
    //         GL miscompares
    //
    // If the surface type being called on supports optimizations (ex.
    // GpuGoldenSurfaces can use DMA transfers, GLGoldenSurfaces can use GPU for
    // callwlations), then this function will attempt to enable/disable the
    // optimization on subsequent callwlations. This function will return true
    // if it is able to successfully reduce optimization.
    virtual bool ReduceOptimization(bool reduce) { return false; }

    //! only GLGoldenSurfaces supports so far
    virtual RC SetSurfaceCheckMethod(int surfNum, Goldelwalues::Code calcMethod)
    { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetModifiedSurfProp(int surfNum, UINT32 *Width, UINT32 *Height,
               UINT32 *Pitch){ return RC::UNSUPPORTED_FUNCTION; }

    //! Some implementation can require particular alignment of input surfaces.
    //! Lwrrently this is GpuGoldenSurfaces that uses VIC engine (CheetAh).
    virtual RC GetPitchAlignRequirement(UINT32 *pitch) = 0;
};

//--------------------------------------------------------------------------------------------------
// Generic class to self-check internal test data for consistency.
// This class will generate a crc value for data item. A data item can be a series of data atoms to
// self-check. The crc value will be stored in the golden database
class GoldenSelfCheck
{
public:
    enum eLogMode {
        Skip,       //skip golden checking
        Store,      //store crc values to the goldenDB
        Check       //check crc values against the goldenDB
    };
    enum eLogState {
        lsUnknown,
        lsBegin,
        lsLog,
        lsFinish
    };
    GoldenSelfCheck();
    ~GoldenSelfCheck(){};

    void Init(const char * szTestName, Goldelwalues *pGVals);

    // Create a new log with crcs to support numItems.
    void LogBegin(
        size_t numItems // number of crc entries for this log.
        );

    // Update the crc item with the new data and return the updated crc value
    UINT32 LogData(
        int item,
        const void * data,
        size_t dataSize
        );

    // Compare/store crc values from/to the golden DB.
    RC LogFinish(const char * label, UINT32 dbIndex);
    void SetLogMode(UINT32 logMode);

private:
    eLogMode        m_LogMode;
    eLogState       m_LogState;
    vector<UINT32>  m_LogItemCrcs;
    string          m_TestName;
    Goldelwalues *  m_pGValues;
};
#endif // !INCLUDED_GOLDEN_H

