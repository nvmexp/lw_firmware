/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Header for MemError class
//
//------------------------------------------------------------------------------
// HOW TO USE THE MEMERROR OBJECT
//------------------------------------------------------------------------------
// 1.  Add one MemError to your class for each GPU being tested
// 2.  Before initializing your JS properties, call SetupTest(FB) where FB is
//       the FrameBuffer* under test.  Ideally this should be done in the
//       Setup() portion of a ModsTest.
// 3.  At the end of the test, call ResolveMemErrorResult().  On GPUs, this
//       should be done after you restore the VGA display
// 4.  After finishing with the MemError class (mats/wfmats/etc.) instantiation,
//       call Cleanup() on the MemError object.  Ideally this should be done
//       in the Cleanup() portion of a ModsTest.

#pragma once

#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#include <functional>
#ifdef _WIN32
#   pragma warning(pop)
#endif

#include "framebuf.h"
#ifndef MATS_STANDALONE
#   include "core/include/errormap.h"
#endif
#include "core/include/memlane.h"
#include <deque>
#include <map>
#include <set>
#include <vector>
#include <tuple>

class JsonItem;

using LaneError = Memory::FbpaLane; // WAR(MODSAMPERE-753):

namespace MemErrorHelper
{
    // Counter<N> is an N-dimensional vector of UINT64, wrapped in a
    // struct to avoid "decorated name length exceeded" from MSVC.
    // For example, Counter<3> is vector<vector<vector<UINT64>>>.
    // Used in MemError to store error counts.  We lazily allocate
    // elements as needed; unallocated elements are assumed to be 0.
    template<int N> struct Counter    { vector<Counter<N-1>> value; };
    template<>      struct Counter<0> { UINT64               value; };
}

class MemError
{
public:
    static const UINT64 NO_TIME_STAMP = _UINT64_MAX;

    enum class IoType
    {
        READ,
        WRITE,
        UNKNOWN,
        NUM_VALUES  // Upper bound for loops; must be last
    };

    enum class ErrType // ErrorRecord type
    {
        OFFSET,    // LogOffsetError() record: we don't know the error addr
        NORMAL,    // LogMemoryError() record: we know the error addr
        NORMAL_UNK,// LogUnknownMemoryError() record: don't know rank or IoType
        NORMAL_ECC // LogEccError() record: know the addr but not the contents
    };

    struct ErrorRecord
    {
        virtual ~ErrorRecord() {}
        ErrType type             = ErrType::OFFSET;
        UINT32  width            = 0;
        UINT64  addr             = 0;
        UINT64  actual           = 0;
        UINT64  expected         = 0;
        UINT64  reRead1          = 0;
        UINT64  reRead2          = 0;
        UINT32  pteKind          = 0;
        UINT32  pageSizeKB       = 0;
        UINT64  timeStamp        = NO_TIME_STAMP;
        string  patternName      = "";
        UINT32  patternOffset    = 0;
        INT32   originTestId     = s_UnknownTestId; //!< Test running when the error oclwred
    };

    typedef unique_ptr<ErrorRecord> ErrorRecordPtr;

    // Use LogMysteryError when you don't know even the approximate
    // location of the error, just that one oclwrred.
    RC LogMysteryError();
    RC LogMysteryError(UINT64 newErrors);

    // use LogOffsetError when you don't know the exact address of the
    // failure (e.g. NewWfMats blits)
    RC LogOffsetError(UINT32 width,
                      UINT64 sampleAddr,
                      UINT64 actual,
                      UINT64 expected,
                      UINT32 pteKind,
                      UINT32 pageSizeKB,
                      const string &patternName,
                      UINT32 patternOffset);
    RC LogOffsetError(ErrorRecordPtr pErr);

    // use LogUnknownMemoryError when you know the exact address of the
    // failure, but not whether it was a read vs. write error (GLStress).
    RC LogUnknownMemoryError(UINT32 width,
                             UINT64 physAddr,
                             UINT64 actual,
                             UINT64 expected,
                             UINT32 pteKind,
                             UINT32 pageSizeKB);
    RC LogUnknownMemoryError(ErrorRecordPtr pErr);

    // use LogMemoryError when you know the exact address of the
    // failure (like when doing a PCI read/write) and can repeat the
    // operation multiple times (which will be used to determine if it
    // is a "read" or "write" error).
    RC LogMemoryError(UINT32 width,
                      UINT64 physAddr,
                      UINT64 actual,
                      UINT64 expected,
                      UINT64 reRead1,
                      UINT64 reRead2,
                      UINT32 pteKind,
                      UINT32 pageSizeKB,
                      UINT64 timeStamp,
                      const string &patternName,
                      UINT32 patternOffset);
    RC LogMemoryError(ErrorRecordPtr pErr);

    // Use LogFailingBits when you have enough data to know which bit
    // on the bus failed, but do not know the exact address or offset
    RC LogFailingBits
    (
        UINT32 virtualFbio,   //!< returned by DecodeAddress
        UINT32 subpartition,  //!< returned by DecodeAddress
        UINT32 pseudoChannel, //!< returned by DecodeAddress
        UINT32 rank,          //!< returned by DecodeAddress
        UINT32 beatMask,      //!< Mask of failing beats in burst, or 0 for all
        UINT32 beatOffset,    //!< Byte offset of failing word in beat
        UINT32 bitOffset,     //!< Offset of bit within failing byte or word
        UINT64 wordCount,     //!< number of failing words
        UINT64 bitCount,      //!< number of individual bit failures
        IoType ioType         //!< READ or WRITE
    );

    // Use LogEccError when an asynchronous ECC error oclwrs.  It's
    // hard to tell which test induced the error, so it's broadcast to
    // all memory tests (if any) on the failing Framebuffer.
    static RC LogEccError
    (
        const FrameBuffer *pFb,
        UINT64 physAddr,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        bool   isSbe,          //!< true = SBE, false = DBE
        UINT32 sbeBitPosition  //!< Bit position of error, _UINT32_MAX = unknown
    );

    // Call this at the end of the test
    RC   ResolveMemErrorResult();
    void AddErrCountsToJsonItem(JsonItem * pJsonExit);

    RC   PrintErrorLog(UINT32 printCount);
    RC   PrintErrorsPerPattern(UINT32 count);
    RC   PrintStatus();
    RC   ErrorBitsDetailed();

    MemError();
    ~MemError();
    static RC Initialize();
    static void Shutdown();

    using GetCellFunc = function<string(const ErrorRecord&, const FbDecode&)>;
    void ExtendErrorLog(const string &header, const string &desc,
                        GetCellFunc getCellFunc);

    UINT64 GetErrorCount()        const;
    UINT64 GetWriteErrorCount()   const;
    UINT64 GetReadErrorCount()    const;
    UINT64 GetUnknownErrorCount() const;

    void   SetMaxErrors(UINT32 x);
    UINT32 GetMaxErrors() const;

    void   SetMaxPrintedErrors(UINT32 x) { m_MaxPrintedErrors = x; }
    UINT32 GetMaxPrintedErrors() const { return m_MaxPrintedErrors; }

    vector<LaneError> GetLaneErrors() const;
    static void DetectDBIErrors(vector<LaneError> *errors);

    bool   GetAutoStatus() const;

    // Get per-bit error info.
    RC ErrorInfoPerBit(UINT32          virtualFbio,
                       UINT32          rankMask,
                       UINT32          beatMask,
                       bool            includeReadErrors,
                       bool            includeWriteErrors,
                       bool            includeUnknownErrors,
                       bool            includeR0X1, // read 0, expected 1
                       bool            includeR1X0, // read 1, expected 0
                       vector<UINT64> *pErrorsPerBit,
                       UINT64         *pErrorCount);

    RC SetupTest(FrameBuffer *pFb, UINT32 specialFlags = 0);
    RC Cleanup();
    bool IsSetup() const { return m_pFrameBuffer != nullptr; }

    void   SetCriticalFbRange(const FbRanges& x);
    bool   GetCriticalMemFailure() const { return m_CriticalMemFailure; }

    FrameBuffer *GetFB();

    void DeleteErrorList();
    void SuppressHelpMessage()
    {
        m_HasHelpMessageBeenPrinted = true;
    }

    void SetTestName(const string &testName) { m_TestName = testName; }

private:
    enum class FlipDir
    {
        READ0_EXPECT1,
        READ1_EXPECT0,
        UNKNOWN,
        NUM_VALUES  // Upper bound for loops; must be last
    };

    void ErrorSubpartSummary();
    void ErrorBitsSummary();
    RC DecodeErrorAddress(FbDecode* pDecode, const ErrorRecord& error) const;
    RC DecodeErrorAddress(FbDecode* pDecode, const ErrorRecord& error,
                          UINT64 errorMask) const;
    UINT32 GetFbioBitOffset(const FbDecode& pDecode, const ErrorRecord& error,
                            UINT32 bitOffset) const;
    UINT32 GetFbioBitOffset(const FbDecode& pDecode, UINT64 errorMask,
                            UINT32 bitOffset) const;

    // Returns a pair of <virtualFbio, bit> data
    vector< pair<UINT32, UINT32> > GetBadBitList(UINT32 rank, UINT64 minErrThresh = 0) const;

    void PrintBadBitList(UINT32 rank);

    void PrintMemErrorToMle(const ErrorRecord& error, const FbDecode& decode);

    RC LogHelper(ErrorRecordPtr pErr, ErrType errType, IoType ioType);

    //! Defines columns printed by PrintErrorLog()
    struct ErrorLogField
    {
        ErrorLogField(const string &h, const string &d, const GetCellFunc &g)
            : header(h), desc(d), getCellFunc(g) {}
        string      header;       //!< Header at the top of the column
        string      desc;         //!< Description of column
        GetCellFunc getCellFunc;  //!< Function returning string in cells
    };
    vector<ErrorLogField> m_ErrorLogFields;

    // Indexed by [ioType][flipDir][virtualFbio][rank][beat][bit]
    MemErrorHelper::Counter<6> m_BitErrors;
    // Indexed by [ioType][virtualFbio][subpartition][rank]
    MemErrorHelper::Counter<4> m_WordErrors;

    UINT64 m_ErrorCount;
    UINT64 m_ReadErrorCount;
    UINT64 m_WriteErrorCount;
    UINT64 m_UnknownErrorCount;    // Don't know if it was read or write
    bool   m_CriticalMemFailure;

    deque<ErrorRecordPtr> m_ErrorLog;
    map<string, int> m_ErrorsPerPattern;

    bool   m_TestInitialized;
    bool   m_HasHelpMessageBeenPrinted;

    UINT32 m_FbioCount;
    UINT32 m_NumSubpartitions;
    UINT32 m_BeatsPerBurst;
    UINT32 m_BitsPerSubpartition;
    UINT32 m_BitsPerFbio;
    UINT32 m_RankCount;

    // m_UnknownRank is used to index into m_BitErrors & m_WordErrors
    // for LogOffsetError().  It's maxRank+1 on multi-rank systems, to
    // record the unknown-rank errors separately, and 0 on single-rank
    // systems since the "unknown" rank can be trivially deduced.
    UINT32 m_UnknownRank;

    UINT32 m_MaxErrors;
    UINT32 m_MaxPrintedErrors;
    bool   m_AutoStatus;
    bool   m_IgnoreReadErrors;
    bool   m_IgnoreWriteErrors;
    bool   m_IgnoreUnknownErrors;
    bool   m_BlacklistPagesOnError;
    bool   m_DumpPagesOnError;
    UINT64 m_MinLaneErrThresh;
    bool   m_RowRemapOnError = false;

    FrameBuffer *m_pFrameBuffer;

    // inhibit the default copy constructor
    MemError(const MemError &);
    MemError & operator= (const MemError &);

    string m_TestName;
    FbRanges m_CriticalFbRanges;

    static set<MemError*> s_Registry;
    static void          *s_pRegistryMutex;
    void                 *m_pMutex;
};
