/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   memerror.cpp
 * @brief  Implement class MemError.
 * The purpose of this class is to keep track of and interpret memory errors.
 */

#include "core/include/memerror.h"

#ifndef MATS_STANDALONE
#include "inc/bytestream.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/memtypes.h"
#include "core/include/mle_protobuf.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#endif

#include <algorithm>
#include <set>

using namespace MemErrorHelper;
using MemErrT = Memory::ErrorType;

/* static */ set<MemError*> MemError::s_Registry;
/* static */ void *MemError::s_pRegistryMutex = nullptr;

namespace
{
    // ReadCounter(Counter<N> counter, i, j, k...) returns the value
    // in counter[i][j][k]..., except that it returns 0 for any
    // element that hasn't been allocated yet.
    //
    UINT64 ReadCounter(const Counter<0> &counter)
    {
        return counter.value;
    }
    template<typename Index, typename... Indices> UINT64 ReadCounter
    (
        const Counter<sizeof...(Indices)+1> &counter,
        Index                                index,
        Indices...                           remainingIndices
    )
    {
        const size_t idx = static_cast<size_t>(index);
        return (idx < counter.value.size() ?
                ReadCounter(counter.value[idx], remainingIndices...) : 0);
    }

    // Increment(Counter<N> pCounter, count, i, j, k...) works like
    // (*pCounter)[i][j][k]... += count, except that it lazily resizes
    // the counter vector if the indicated element doesn't exist.
    //
    void IncrementCounter(Counter<0> *pCounter, UINT64 count)
    {
        pCounter->value += count;
    }
    template<typename Index, typename... Indices> void IncrementCounter
    (
        Counter<sizeof...(Indices)+1> *pCounter,
        UINT64                         count,
        Index                          index,
        Indices...                     remainingIndices
    )
    {
        const size_t idx = static_cast<size_t>(index);
        if (idx >= pCounter->value.size())
            pCounter->value.resize(idx + 1);
        IncrementCounter(&pCounter->value[idx], count, remainingIndices...);
    }

    // Helper class for printing a table along the lines of:
    //
    // HEADER1 HEADER2 HEADER3
    // ------- ------- -------
    // data1   data2   data3
    // data4   data5   data6
    //
    // This class processes the table in four "phases", which are
    // advanced by NextPhase():
    // * SETUP:   Call AddColumn("HEADER") to define the columns
    // * PREVIEW: Call AddCell("data") to preview the data, to figure
    //       out how wide to make the columns.  At the end of the
    //       PREVIEW phase, GetColWidth() contains the widest data in
    //       each column (not counting headers).
    // * PRINTING: Call AddCell("data") to print the data.  Normally
    //       we pass the same data in both preview & printing phases.
    // * DONE:  Done printing
    //
    class TablePrinter
    {
    public:
        TablePrinter(Tee::Priority pri, const string &title)
            : m_Pri(pri), m_Title(title) {}
        void     AddColumn(const string &header, const string &desc = "");
        void     EraseColumn(size_t index);
        void     AddCell(const string &cell);
        bool     NextPhase();
        size_t   GetNumRows()  const { return m_NumRows; }
        size_t   GetNumCols()  const { return m_Columns.size(); }
        unsigned GetColWidth(size_t idx) const
            { return static_cast<unsigned>(m_Columns[idx].width); }
        bool     InPrintingPhase() const;

    private:
        enum class Phase
        {
            SETUP,
            PREVIEW,
            PRINTING,
            DONE
        };
        struct Column
        {
            vector<string>    headerLines;
            string            legend;
            string            format;
            bool              packLeft  = false;
            bool              packRight = false;
            bool              alignLeft = false;
            string::size_type width     = 0;
        };
        const Tee::Priority m_Pri;
        string              m_Title;
        vector<Column>      m_Columns;
        Phase               m_Phase      = Phase::SETUP;
        size_t              m_PendingCol = 0;
        string              m_PendingRow;
        size_t              m_NumRows    = 0;
    };

    //! \brief Add one column in the SETUP phase.
    //!
    //! \param header The string printed at the top of the column.
    //!     May include newlines ('\n') for multi-line headers.  To
    //!     "pack" columns together without whitespace between, the
    //!     header should start with "<" to pack on the left or ">" to
    //!     pack on the right; both columns must consent to be packed.
    //!     To left-align the column, the header should start with
    //!     "-".  Left-aligned packed-left colums should start with
    //!     "<-".
    //! \param desc Optional description used to print a legend for the
    //!     table.  If non-empty, add "header : desc" to the legend.
    //
    void TablePrinter::AddColumn(const string &header, const string &desc)
    {
        MASSERT(m_Phase == Phase::SETUP);
        MASSERT(header.find_first_not_of("-<>") != string::npos);
        Column newColumn;

        string::size_type hdrPos   = 0;
        string::size_type hdrEnd   = header.size();
        string::size_type hdrWidth = 0;
        if (header[hdrPos] == '<')
        {
            newColumn.packLeft = true;
            ++hdrPos;
        }
        if (header[hdrEnd - 1] == '>')
        {
            newColumn.packRight = true;
            --hdrEnd;
        }
        if (header[hdrPos] == '-')
        {
            newColumn.alignLeft = true;
            ++hdrPos;
        }
        while (hdrPos < hdrEnd)
        {
            string::size_type lineEnd = header.find('\n', hdrPos);
            if (lineEnd == string::npos)
               lineEnd = hdrEnd;
            newColumn.headerLines.push_back(header.substr(hdrPos,
                                                          lineEnd - hdrPos));
            hdrWidth = max(hdrWidth, lineEnd - hdrPos);
            hdrPos = lineEnd + 1;
        }

        if (!desc.empty())
        {
            for (const string &line: newColumn.headerLines)
                newColumn.legend += line + " ";
            newColumn.legend += ": " + desc;
        }

        newColumn.headerLines.push_back(string(hdrWidth, '-'));
        m_Columns.push_back(newColumn);
    }

    // Delete one column.  This method cannot be called in the
    // PRINTING phase once we have already started printing data.
    //
    void TablePrinter::EraseColumn(size_t index)
    {
        MASSERT(index < m_Columns.size());
        MASSERT(m_PendingCol == 0);
        MASSERT(m_NumRows == 0);
        m_Columns.erase(m_Columns.begin() + index);
    }

    // Pass the string that will be printed in one cell of the data.
    // For an MxN table, AddCell() must be called M*N times, in
    // left-to-right top-to-bottom order.
    //
    void TablePrinter::AddCell(const string &cell)
    {
        MASSERT(!m_Columns.empty());
        MASSERT(m_Phase == Phase::PREVIEW || m_Phase == Phase::PRINTING);
        Column *pColumn = &m_Columns[m_PendingCol];

        // The PREVIEW phase: Use the cell data to decide how
        // wide the column should be.
        //
        if (m_Phase == Phase::PREVIEW)
        {
            pColumn->width = max(pColumn->width, cell.size());
            m_PendingCol = (m_PendingCol + 1) % m_Columns.size();
            return;
        }

        // Everything after this point handles the PRINTING phase.

        // Before adding the first cell to the first row, callwlate
        // the width of each column (including headers) and print the
        // title, legends, and headers
        //
        if (m_PendingCol == 0)
        {
            if (m_NumRows == 0)
            {
                size_t numHeaderLines = 0;
                bool   isFirstCol     = true;
                bool   canPack        = true;
                if (!m_Title.empty())
                    Printf(m_Pri, "%s\n", m_Title.c_str());
                for (Column &column: m_Columns)
                {
                    if (!column.legend.empty())
                        Printf(m_Pri, "%s\n", column.legend.c_str());
                    numHeaderLines = max(numHeaderLines,
                                         column.headerLines.size());
                    column.width = max(column.width,
                                       column.headerLines.back().size());
                    column.format = Utility::StrPrintf(
                                "%s%%%s%us",
                                isFirstCol || (canPack && column.packLeft)
                                ? "" : " ",
                                column.alignLeft ? "-" : "",
                                static_cast<unsigned>(column.width));
                    isFirstCol = false;
                    canPack    = column.packRight;
                }
                for (size_t ii = 0; ii < numHeaderLines; ++ii)
                {
                    string headerLine;
                    for (const auto &column: m_Columns)
                    {
                        const size_t blankLines = (numHeaderLines -
                                                   column.headerLines.size());
                        headerLine += Utility::StrPrintf(
                                column.format.c_str(),
                                ii >= blankLines
                                ? column.headerLines[ii - blankLines].c_str()
                                : "");
                    }
                    Printf(m_Pri, "%s\n", headerLine.c_str());
                }
            }
        }

        // Append the cell to the pending row.
        //
        m_PendingRow += Utility::StrPrintf(pColumn->format.c_str(),
                                           cell.c_str());
        m_PendingCol = (m_PendingCol + 1) % m_Columns.size();

        // Print the row when it's  done
        //
        if (m_PendingCol == 0)
        {
            Printf(m_Pri, "%s\n", m_PendingRow.c_str());
            m_PendingRow.clear();
            ++m_NumRows;
        }
    }

    // Advance to the next phase.  Returns false when DONE.
    //
    bool TablePrinter::NextPhase()
    {
        MASSERT(m_PendingCol == 0);
        if (m_Phase == Phase::SETUP)
            m_Phase = Phase::PREVIEW;
        else if (m_Phase == Phase::PREVIEW)
            m_Phase = Phase::PRINTING;
        else // PRINTING or DONE
            m_Phase = Phase::DONE;
        return (m_Phase != Phase::DONE);
    }

    bool TablePrinter::InPrintingPhase() const
    {
        return m_Phase == Phase::PRINTING;
    }

    // Colwenience function for printing FBIO as a letter or SOC channel
    string FbioString(FrameBuffer *pFb, UINT32 virtualFbio)
    {
        return pFb->IsSOC()
            ? Utility::StrPrintf("%1u", pFb->VirtualFbioToHwFbio(virtualFbio))
            : Utility::StrPrintf("%c", pFb->VirtualFbioToLetter(virtualFbio));
    }
}

#ifndef MATS_STANDALONE
//------------------------------------------------------------------------------
// JS Interface
//------------------------------------------------------------------------------
JS_CLASS(MemError);

static SObject MemError_Object
(
    "MemError",
    MemErrorClass,
    0,
    0,
    "Memory error database class."
);

PROP_DESC(MemError, AutoStatus, true,
          "Automatically call Status() after Run()");
PROP_DESC(MemError, MaxErrors, 1000,
          "Maximum Number of Errors to Log");
PROP_DESC(MemError, MaxPrintedErrors, 50,
          "Maximum Number of Errors to Print");
PROP_DESC(MemError, IgnoreReadErrors, false,
          "Ignore Read Errors");
PROP_DESC(MemError, IgnoreWriteErrors, false,
          "Ignore Write Errors");
PROP_DESC(MemError, IgnoreUnknownErrors, false,
          "Ignore Unknown Errors");
PROP_DESC(MemError, BlacklistPagesOnError, false,
          "Blacklist Pages On Error");
PROP_DESC(MemError, DumpPagesOnError, false,
          "Dump Pages On Error");
PROP_DESC(MemError, MinLaneErrThresh, 256,
          "Minuimum errors on a pin to designate it a lane error");
PROP_DESC(MemError, RowRemapOnError, false,
          "Remap rows on error");

#endif // MATS_STANDALONE

//------------------------------------------------------------------------------
// MemError Constructor
//------------------------------------------------------------------------------
MemError::MemError() :
      m_CriticalMemFailure(false)
    , m_TestInitialized(false)
    , m_HasHelpMessageBeenPrinted(false)
    , m_FbioCount(0)
    , m_NumSubpartitions(0)
    , m_BeatsPerBurst(0)
    , m_BitsPerSubpartition(0)
    , m_BitsPerFbio(0)
    , m_RankCount(0)
    , m_UnknownRank(0)
    , m_MaxErrors(0)
    , m_MaxPrintedErrors(0)
    , m_AutoStatus(true)
    , m_IgnoreReadErrors(false)
    , m_IgnoreWriteErrors(false)
    , m_IgnoreUnknownErrors(false)
    , m_BlacklistPagesOnError(false)
    , m_DumpPagesOnError(false)
    , m_MinLaneErrThresh(0)
    , m_pFrameBuffer(nullptr)
    , m_TestName("")
    , m_pMutex(Tasker::AllocMutex("MemError", Tasker::mtxLast))
{
    DeleteErrorList();

    Tasker::MutexHolder lockRegistry(s_pRegistryMutex);
    s_Registry.insert(this);
}

//------------------------------------------------------------------------------
// MemError Destructor
//------------------------------------------------------------------------------
MemError::~MemError()
{
    Tasker::MutexHolder lockRegistry(s_pRegistryMutex);
    s_Registry.erase(this);

    DeleteErrorList();
    m_ErrorsPerPattern.clear();
    m_pFrameBuffer = nullptr;
    Tasker::FreeMutex(m_pMutex);
}

//------------------------------------------------------------------------------
/* static */ RC MemError::Initialize()
{
    s_pRegistryMutex = Tasker::AllocMutex("MemError::s_pRegistryMutex",
                                          Tasker::mtxNLast);
    return OK;
}

//------------------------------------------------------------------------------
/* static */ void MemError::Shutdown()
{
    if (s_pRegistryMutex)
    {
        Tasker::FreeMutex(s_pRegistryMutex);
        s_pRegistryMutex = nullptr;
    }
}

//--------------------------------------------------------------------
//! \brief Add a column to the table printed by PrintErrorLog()
//!
//! \param header The string printed at the top of the column.  May
//!     include newlines ('\n') for multi-line headers.  To "pack"
//!     columns together without whitespace between, the header should
//!     start with "<" to pack on the left or ">" to pack on the
//!     right; both columns must consent to be packed.
//! \param desc Optional description used to print a legend for the
//!     table.  If non-empty, add "header : desc" to the legend.
//! \param getCellFunc Function that generates the string to put in
//!     each cell of the column.  Should return an empty string if the
//!     column does not apply; PrintErrorLog() automatically omits
//!     any column in which all cells are empty strings.
//!
void MemError::ExtendErrorLog
(
    const string &header,
    const string &desc,
    GetCellFunc getCellFunc
)
{
    Tasker::MutexHolder lock(m_pMutex);
    MASSERT(!header.empty());
    m_ErrorLogFields.emplace_back(header, desc, getCellFunc);
}

//------------------------------------------------------------------------------
// Delete the linked list of errors.
//------------------------------------------------------------------------------
void MemError::DeleteErrorList()
{
    Tasker::MutexHolder lock(m_pMutex);
    m_ErrorLog.clear();
    m_BitErrors.value.clear();
    m_WordErrors.value.clear();

    m_ErrorCount          = 0;
    m_ReadErrorCount      = 0;
    m_WriteErrorCount     = 0;
    m_UnknownErrorCount   = 0;
    m_CriticalMemFailure  = false;
}

//------------------------------------------------------------------------------
// Simple one-line accessor functions
//------------------------------------------------------------------------------
UINT64 MemError::GetErrorCount()        const { return m_ErrorCount; }
UINT64 MemError::GetWriteErrorCount()   const { return m_WriteErrorCount; }
UINT64 MemError::GetReadErrorCount()    const { return m_ReadErrorCount; }
UINT64 MemError::GetUnknownErrorCount() const { return m_UnknownErrorCount; }
bool   MemError::GetAutoStatus()        const { return m_AutoStatus; }
void   MemError::SetMaxErrors(UINT32 x)       { m_MaxErrors = x; return; }
UINT32 MemError::GetMaxErrors()         const { return m_MaxErrors; }
void   MemError::SetCriticalFbRange(const FbRanges& x)
                                              { m_CriticalFbRanges = x; }

RC MemError::ErrorInfoPerBit
(
    UINT32          virtualFbio,
    UINT32          rankMask,
    UINT32          beatMask,
    bool            includeReadErrors,
    bool            includeWriteErrors,
    bool            includeUnknownErrors,
    bool            includeR0X1,
    bool            includeR1X0,
    vector<UINT64> *pErrorsPerBit,
    UINT64         *pErrorCount
)
{
    Tasker::MutexHolder lock(m_pMutex);
    MASSERT(pErrorsPerBit);
    MASSERT(pErrorCount);
    pErrorsPerBit->clear();
    *pErrorCount = 0;

    // Validate the parameters
    //
    if (!m_pFrameBuffer)
    {
        Printf(Tee::PriError, "Test has not been setup yet.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (virtualFbio >= m_FbioCount)
    {
        Printf(Tee::PriError,
               "FBIO %d is out of range (max %d).\n",
               virtualFbio, m_FbioCount - 1);
        return RC::BAD_PARAMETER;
    }

    const UINT32 allRanks = (1 << m_RankCount) - 1;
    rankMask &= allRanks;
    if (rankMask == allRanks)
    {
        // If reading all ranks (usual case), include unknown rank
        rankMask |= (1 << m_UnknownRank);
    }
    if (rankMask == 0)
    {
        Printf(Tee::PriError, "At least one valid rank must be set\n");
        return RC::BAD_PARAMETER;
    }

    if (0 == beatMask)
    {
        Printf(Tee::PriError, "BeatMask cannot be zero\n");
        return RC::BAD_PARAMETER;
    }

    if (!includeReadErrors && !includeWriteErrors && !includeUnknownErrors)
    {
        Printf(Tee::PriError,
               "At least one of IncludeReadErrors, IncludeWriteErrors and\n"
               "IncludeUnknownErrors must be set\n");
        return RC::BAD_PARAMETER;
    }

    if (!includeR0X1 && !includeR1X0)
    {
        Printf(Tee::PriError,
               "At least one of IncludeR0X1 and IncludeR1X0 must be set\n");
        return RC::BAD_PARAMETER;
    }

    // If counting both READ0_EXPECT1 and READ1_EXPECT0, then also
    // count errors in which we don't know the toggle direction.
    const bool includeRXunknown = includeR0X1 && includeR1X0;

    // Callwlate the values returned from this method
    //
    vector<UINT64> errorsPerBit(m_BitsPerFbio, 0);
    UINT64 errorCount = 0;

    for (UINT32 ioType = 0; ioType < m_BitErrors.value.size(); ++ioType)
    {
        IoType myIoType = static_cast<IoType>(ioType);
        if ((myIoType == IoType::READ    && !includeReadErrors   ) ||
            (myIoType == IoType::WRITE   && !includeWriteErrors  ) ||
            (myIoType == IoType::UNKNOWN && !includeUnknownErrors))
        {
            continue;
        }
        const auto &flipDirErrors = m_BitErrors.value[ioType];
        for (UINT32 flipDir = 0;
             flipDir < flipDirErrors.value.size(); ++flipDir)
        {
            FlipDir myFlipDir = static_cast<FlipDir>(flipDir);
            if ((myFlipDir == FlipDir::READ0_EXPECT1 && !includeR0X1) ||
                (myFlipDir == FlipDir::READ1_EXPECT0 && !includeR1X0) ||
                (myFlipDir == FlipDir::UNKNOWN       && !includeRXunknown))
            {
                continue;
            }
            const auto &fbioErrors = flipDirErrors.value[flipDir];
            if (virtualFbio >= fbioErrors.value.size())
                continue;
            const auto &rankErrors = fbioErrors.value[virtualFbio];
            for (UINT32 rank = 0; rank < rankErrors.value.size(); ++rank)
            {
                if ((rankMask & (1 << rank)) == 0)
                    continue;
                const auto &beatErrors = rankErrors.value[rank];
                for (UINT32 beat = 0; beat < beatErrors.value.size(); ++beat)
                {
                    if ((beatMask & (1 << beat)) == 0)
                        continue;
                    const auto &bitErrors = beatErrors.value[beat];
                    for (UINT32 bit = 0; bit < bitErrors.value.size(); ++bit)
                    {
                        errorsPerBit[bit] += bitErrors.value[bit].value;
                    }
                }
            }
        }
    }

    for (UINT32 ioType = 0; ioType < m_WordErrors.value.size(); ++ioType)
    {
        IoType myIoType = static_cast<IoType>(ioType);
        if ((myIoType == IoType::READ    && !includeReadErrors   ) ||
            (myIoType == IoType::WRITE   && !includeWriteErrors  ) ||
            (myIoType == IoType::UNKNOWN && !includeUnknownErrors))
        {
            continue;
        }
        const auto &fbioErrors = m_WordErrors.value[ioType];
        if (fbioErrors.value.size() >= virtualFbio)
            continue;
        const auto &subpErrors = fbioErrors.value[virtualFbio];
        for (const auto &rankErrors: subpErrors.value)
        {
            for (UINT32 rank = 0; rank < rankErrors.value.size(); ++rank)
            {
                if ((rankMask & (1 << rank)) == 0)
                    continue;
                errorCount += rankErrors.value[rank].value;
            }
        }
    }

    // Return the callwlated values to the caller
    //
    *pErrorsPerBit = move(errorsPerBit);
    *pErrorCount   = errorCount;
    return OK;
}

//------------------------------------------------------------------------------
// Initialize the object.
//------------------------------------------------------------------------------
RC MemError::SetupTest(FrameBuffer *pFB, UINT32 specialFlags)
{
    Tasker::MutexHolder lock(m_pMutex);
    MASSERT(!m_TestInitialized);
    if (m_TestInitialized)
        Printf(Tee::PriLow, "Warning: multiple calls to MemError::SetupTest "
               "without MemError::Cleanup().  Please adhere to ModsTest "
               "SETUP/RUN/CLEANUP structure, calling MemError::Cleanup in "
               "CLEANUP.\n");
    if (pFB->IsFbBroken())
    {
        Printf(Tee::PriLow,
                "Memerror reporting is not used when mods is running with -fb_broken option");
        return OK;
    }
    m_pFrameBuffer = pFB;
    UINT64 FbSize = m_pFrameBuffer->GetGraphicsRamAmount();

    m_FbioCount           = m_pFrameBuffer->GetFbioCount();
    m_NumSubpartitions    = m_pFrameBuffer->GetSubpartitions();
    m_BeatsPerBurst       = FbSize ? (m_pFrameBuffer->GetBurstSize() /
                             m_pFrameBuffer->GetBeatSize()) : 0;
    m_BitsPerSubpartition = FbSize ? (m_pFrameBuffer->GetPseudoChannelsPerSubpartition()
                             * m_pFrameBuffer->GetBeatSize() * 8) : 0;
    m_BitsPerFbio         = m_NumSubpartitions * m_BitsPerSubpartition;
    m_RankCount           = (m_pFrameBuffer->IsSOC()) ? 1 : m_pFrameBuffer->GetRankCount();
    m_UnknownRank         = (m_RankCount > 1 ? m_RankCount : 0);

    DeleteErrorList();

    // Define the columns printed by PrintErrorLog().
    // GET_CELL() is a colwenience macro for generating the lambda
    // functions that return the string in each cell
    //
#define GET_CELL(enableCondition, printfArgs)                   \
    [&](const ErrorRecord &err, const FbDecode &dec)->string    \
    {                                                           \
        if (enableCondition)                                    \
            return Utility::StrPrintf printfArgs;               \
        else                                                    \
            return "";                                          \
    }

    m_ErrorLogFields.clear();
    ExtendErrorLog("ADDRESS",
            "Failing memory address, or buffer offset if starting with 'X+'",
            GET_CELL(true,
            (err.type == ErrType::OFFSET ? "X+%08llx" : "%010llx", err.addr)));
    ExtendErrorLog("EXPECTED", "",
            [&](const ErrorRecord &err, const FbDecode &dec)->string
            {
                if (err.type == ErrType::NORMAL_ECC)
                    return (err.actual ? "<SBE>" : "<DBE>");
                else
                    return Utility::StrPrintf("%0*llx", err.width >> 2,
                                              err.expected);
            });
    ExtendErrorLog("ACTUAL", "", GET_CELL(
            err.type != ErrType::NORMAL_ECC || err.actual != err.expected,
            ("%0*llx", err.width >> 2, err.actual)));
    ExtendErrorLog("REREAD1", "", GET_CELL(err.type == ErrType::NORMAL,
            ("%0*llx", err.width >> 2, err.reRead1)));
    ExtendErrorLog("REREAD2", "", GET_CELL(err.type == ErrType::NORMAL,
            ("%0*llx", err.width >> 2, err.reRead2)));
    ExtendErrorLog("FAILBITS", "", GET_CELL(err.type == ErrType::NORMAL,
            ("%0*llx", err.width >> 2, err.actual ^ err.expected)));

    ExtendErrorLog("<T>", "Type of memory error: W = write, R = read",
            GET_CELL(err.type == ErrType::NORMAL,
            (err.reRead1 == err.actual &&
             err.reRead2 == err.actual ? "W" : "R")));
    ExtendErrorLog("<P>",
            pFB->IsSOC() ? "Partition (SOC Channel)" : "Partition (FBIO)",
            GET_CELL(true, ("%s", FbioString(m_pFrameBuffer,
                                             dec.virtualFbio).c_str())));
    ExtendErrorLog("<S>", "Subpartition",
            GET_CELL(m_pFrameBuffer->GetSubpartitions() > 1,
            ("%1x", dec.subpartition)));
    if (m_pFrameBuffer->IsHbm())
    {
        ExtendErrorLog("<H>", "HBM Site", GET_CELL(true,
            ("%1x", dec.hbmSite)));
        ExtendErrorLog("<C>", "HBM Channel", GET_CELL(true,
            ("%c", 'a' + dec.hbmChannel)));
    }
    ExtendErrorLog("<R>", pFB->IsHbm() ? "Rank (Stack ID)" : "Rank", GET_CELL(
            err.type != ErrType::OFFSET && m_RankCount > 1,
            ("%1x", dec.rank)));
    ExtendErrorLog("<B>", "Bank", GET_CELL(err.type != ErrType::OFFSET,
            ("%1x", dec.bank)));
    ExtendErrorLog("<E>", "Beat", GET_CELL(true,
            ("%1x", dec.beat)));
    ExtendErrorLog("<U>", "PseudoChannel", GET_CELL(
            err.type != ErrType::OFFSET &&
            m_pFrameBuffer->GetPseudoChannelsPerSubpartition() > 1,
            ("%1x", dec.pseudoChannel)));
    ExtendErrorLog("ROW", "", GET_CELL(err.type != ErrType::OFFSET,
            ("%04x", static_cast<UINT32>(dec.row))));
    ExtendErrorLog("COL", "", GET_CELL(err.type != ErrType::OFFSET,
            ("%03x", static_cast<UINT32>(dec.extColumn))));
    ExtendErrorLog("BIT(s)", "",
            [&](const ErrorRecord &err, const FbDecode &dec)->string
            {
                const char fbioLetter =
                    m_pFrameBuffer->VirtualFbioToLetter(dec.virtualFbio);
                const UINT64 errorMask = err.actual ^ err.expected;
                set<string> badBitNames;
                for (INT32 badBit = Utility::BitScanForward64(errorMask);
                     badBit >= 0;
                     badBit = Utility::BitScanForward64(errorMask, badBit + 1))
                {
                    const UINT32 fbioBitOffset = GetFbioBitOffset(dec, err,
                                                                  badBit);
                    badBitNames.insert(Utility::StrPrintf("%c%03u", fbioLetter,
                                                          fbioBitOffset));
                }
                string retval;
                for (const string& badBitName: badBitNames)
                {
                    retval += (retval.empty() ? "" : ",") + badBitName;
                }
                return retval;
            });
    ExtendErrorLog("TIMESTAMP", "", GET_CELL(err.timeStamp != NO_TIME_STAMP,
            ("%016llx", err.timeStamp)));
    ExtendErrorLog("PATTERN", "", GET_CELL(!err.patternName.empty(),
            ("%s", err.patternName.c_str())));
    ExtendErrorLog("OFFSET", "", GET_CELL(!err.patternName.empty(),
            ("0x%x", err.patternOffset)));

#ifndef MATS_STANDALONE
    JavaScriptPtr pJs;
    RC rc = OK;

    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_AutoStatus,
                              &m_AutoStatus));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_MaxErrors,
                              &m_MaxErrors));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_MaxPrintedErrors,
                              &m_MaxPrintedErrors));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_IgnoreReadErrors,
                              &m_IgnoreReadErrors));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_IgnoreWriteErrors,
                              &m_IgnoreWriteErrors));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_IgnoreUnknownErrors,
                              &m_IgnoreUnknownErrors));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_BlacklistPagesOnError,
                              &m_BlacklistPagesOnError));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_DumpPagesOnError,
                              &m_DumpPagesOnError));
    CHECK_RC(pJs->GetProperty(MemError_Object, MemError_MinLaneErrThresh,
                              &m_MinLaneErrThresh));
#endif
    m_TestInitialized = true;
    return OK;
}

//------------------------------------------------------------------------------
// Cleanup the object.
//------------------------------------------------------------------------------
RC MemError::Cleanup()
{
    Tasker::MutexHolder lock(m_pMutex);
    m_TestInitialized = false;

    return OK;
}

//------------------------------------------------------------------------------
// Output the list of errors found
//------------------------------------------------------------------------------
RC MemError::PrintErrorLog(UINT32 printCount)
{
    Tasker::MutexHolder lock(m_pMutex);
    const UINT32 maxErrors = min(printCount, m_MaxErrors);
    const size_t numRows = min<size_t>(maxErrors, m_ErrorLog.size());
    RC rc;

    TablePrinter tablePrinter(Tee::PriNormal,
                              "\n=== MEMORY ERRORS BY ADDRESS ===");
    vector<GetCellFunc> getCellFuncs;
    for (const auto &field: m_ErrorLogFields)
    {
        tablePrinter.AddColumn(field.header, field.desc);
        getCellFuncs.push_back(field.getCellFunc);
    }

    while (tablePrinter.NextPhase())
    {
        for (size_t row = 0; row < numRows; ++row)
        {
            const auto &pErrorRecord = m_ErrorLog[row];
            FbDecode decode = {};
            CHECK_RC(DecodeErrorAddress(&decode, *pErrorRecord));
            for (const auto &func: getCellFuncs)
            {
                tablePrinter.AddCell(func(*pErrorRecord, decode));
            }

            // Print MLE for each memory error
            if (tablePrinter.InPrintingPhase())
            {
                PrintMemErrorToMle(*pErrorRecord, decode);
            }
        }

        // Remove blank columns after preview phase
        for (size_t col = 0; col < tablePrinter.GetNumCols(); ++col)
        {
            if (tablePrinter.GetColWidth(col) == 0)
            {
                tablePrinter.EraseColumn(col);
                getCellFuncs.erase(getCellFuncs.begin() + col);
                --col;
            }
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
// Output the number of errors found for each pattern
//------------------------------------------------------------------------------
RC MemError::PrintErrorsPerPattern(UINT32 printCount)
{
    Tasker::MutexHolder lock(m_pMutex);
    UINT32 linesPrinted = 0;

    Printf(Tee::PriNormal, "Errors found for each pattern:\n");
    for (const auto &iter: m_ErrorsPerPattern)
    {
        if (++linesPrinted > printCount)
        {
            break;
        }
        Printf(Tee::PriNormal, "%s    #: %d\n",
               iter.first.c_str(), iter.second);
    }
    Printf(Tee::PriNormal, "End of errors found for each pattern.\n");

    return OK;
}

//------------------------------------------------------------------------------
// print the status from the last run
//------------------------------------------------------------------------------
RC MemError::PrintStatus()
{
    Tasker::MutexHolder lock(m_pMutex);
    if (m_pFrameBuffer == nullptr)
    {
        Printf(Tee::PriNormal, "Test has not been setup yet.\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriNormal, "%s%sMemory Errors on %s\n",
           m_TestName.c_str(),
           m_TestName.empty() ? "" : " ",
           m_pFrameBuffer->GetGpuName(true).c_str());

#if defined(TEGRA_MODS)
    if (m_ReadErrorCount || m_WriteErrorCount || m_UnknownErrorCount)
    {
        // In order to remove this warning, we need a way to:
        // 1) Allocate contiguous chunks via rmapi_tegra
        // 2) Query physical address of each allocation from rmapi_tegra
        // Right now these things are not supported by kernel drivers.
        Printf(Tee::PriNormal,
               "WARNING: Reported addresses/locations are lwrrently\n"
              "          bogus on Android and cannot be relied on\n"
               "\n");
    }
#endif

    Printf(Tee::PriNormal, "Read    Error Count: %llu\n", m_ReadErrorCount);
    Printf(Tee::PriNormal, "Write   Error Count: %llu\n", m_WriteErrorCount);
    Printf(Tee::PriNormal, "Unknown Error Count: %llu\n", m_UnknownErrorCount);

    ErrorSubpartSummary();
    ErrorBitsSummary();

    return OK;
}

//------------------------------------------------------------------------------
// Look through a list of lane errors and update those that are detected as DBI
//------------------------------------------------------------------------------
/* static */ void MemError::DetectDBIErrors(vector<LaneError> *errors)
{
    MASSERT(errors);

    // Look for DBI Lanes by counting number of lane errors within a byte
    map<LaneError, UINT32> errCounter;
    for (auto laneIt = errors->begin(); laneIt != errors->end(); ++laneIt)
    {
        if (laneIt->laneType == LaneError::RepairType::DATA)
        {
            // Mask out the lower 3 lane bits since we're looking for adjacent lanes within a byte
            LaneError lwrLane = *laneIt;
            lwrLane.laneBit &= ~0x7U;

            auto foundLane = errCounter.find(lwrLane);
            if (foundLane != errCounter.end())
            {
                foundLane->second++;
            }
            else
            {
                errCounter[lwrLane] = 1;
            }
        }
    }

    // Find all lanes that are DBI and remove them
    for (auto laneIt = errors->begin(); laneIt != errors->end(); )
    {
        if (laneIt->laneType == LaneError::RepairType::DATA)
        {
            // Mask out the lower 3 lane bits since we're looking for adjacent lanes within a byte
            LaneError lwrLane = *laneIt;
            lwrLane.laneBit &= ~0x7U;

            auto foundLane = errCounter.find(lwrLane);
            if (foundLane != errCounter.end() && foundLane->second == 8)
            {
                laneIt = errors->erase(laneIt);
                continue;
            }
        }
        ++laneIt;
    }

    // Add back all DBI lanes with adjusted laneType
    for (auto laneIt = errCounter.begin(); laneIt != errCounter.end(); ++laneIt)
    {
        if (laneIt->second == 8)
        {
            LaneError lwrLane = laneIt->first;
            lwrLane.laneType = LaneError::RepairType::DBI;
            errors->push_back(lwrLane);
        }
    }
}

//------------------------------------------------------------------------------
// Get all detected lane errors
//------------------------------------------------------------------------------
vector<LaneError> MemError::GetLaneErrors() const
{
    Tasker::MutexHolder lock(m_pMutex);
    vector<LaneError> errors;
    for (UINT32 rank = 0; rank < m_RankCount; ++rank)
    {
        vector< pair<UINT32, UINT32> > badLanes = GetBadBitList(rank, m_MinLaneErrThresh);
        for (const auto& badLane : badLanes)
        {
            const UINT32 hwFbio = m_pFrameBuffer->VirtualFbioToHwFbio(badLane.first);
            const UINT32 hwFbpa = m_pFrameBuffer->HwFbioToHwFbpa(hwFbio);
            const UINT32 lane = badLane.second;

            // MODS Memory tests can only detect Data lane errors. Outside this for-loop, we'll
            // analyze those errors to see if any of them are actually DBI errors
            errors.emplace_back(Memory::HwFbpa(hwFbpa), lane, LaneError::RepairType::DATA);
        }
    }

    // Remove any duplicates, specifically if we're on an 8H HBM part,
    // where we could have duplicate lanes because the same broken
    // lanes were detected in each rank
    set<LaneError> uniqueErrs(errors.begin(), errors.end());
    errors.assign(uniqueErrs.begin(), uniqueErrs.end());

    // Check to see if any of the lane errors reported above are
    // actually DBI errors, and update the error list to reflect those
    // that are found
    DetectDBIErrors(&errors);

    return errors;
}

//------------------------------------------------------------------------------
// Use LogMysteryError when you don't know even the approximate
// location of the error, just that one oclwrred.
//------------------------------------------------------------------------------
RC MemError::LogMysteryError()
{
    return MemError::LogMysteryError(1);
}

RC MemError::LogMysteryError(UINT64 newErrors)
{
    Tasker::MutexHolder lock(m_pMutex);
    if (!m_IgnoreUnknownErrors)
    {
        m_ErrorCount += newErrors;
        m_UnknownErrorCount += newErrors;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Helper function for recording failing bits--avoids duplicated code
//------------------------------------------------------------------------------
RC MemError::LogHelper(ErrorRecordPtr pErr, ErrType errType, IoType ioType)
{
    Tasker::MutexHolder lock(m_pMutex);
    MASSERT(m_TestInitialized);
    MASSERT(pErr);
    RC rc;

    if (!m_TestInitialized || !pErr)
        return RC::SOFTWARE_ERROR;

    pErr->type = errType;
    pErr->originTestId = ErrorMap::Test();

    // Increment the error counters
    //
    m_ErrorCount++;
    switch (ioType)
    {
        case IoType::READ:
            m_ReadErrorCount++;
            break;
        case IoType::WRITE:
            m_WriteErrorCount++;
            break;
        case IoType::UNKNOWN:
            m_UnknownErrorCount++;
            break;
        default:
            MASSERT(!"Unknown IoType");
            break;
    }

    const UINT64 errorMask = pErr->actual ^ pErr->expected;
    FbDecode decode = {};
    CHECK_RC(DecodeErrorAddress(&decode, *pErr));

    UINT32 rank = (pErr->type == ErrType::OFFSET ? m_UnknownRank : decode.rank);
    IncrementCounter(&m_WordErrors, 1, ioType, decode.virtualFbio,
                     decode.subpartition, rank);
    for (INT32 badBit = Utility::BitScanForward64(errorMask); badBit >= 0;
         badBit = Utility::BitScanForward64(errorMask, badBit + 1))
    {
        CHECK_RC(DecodeErrorAddress(&decode, *pErr, 1ULL << badBit));
        const UINT32 fbioBitOffset = GetFbioBitOffset(decode, 1ULL << badBit,
                                                      badBit);
        const FlipDir flipDir = (
                (pErr->type == ErrType::NORMAL_ECC) ? FlipDir::UNKNOWN :
                ((pErr->actual >> badBit) & 1)      ? FlipDir::READ1_EXPECT0 :
                                                      FlipDir::READ0_EXPECT1);
        IncrementCounter(&m_BitErrors, 1, ioType, flipDir, decode.virtualFbio,
                         rank, decode.beat, fbioBitOffset);
    }

    if (!pErr->patternName.empty())
    {
        ++m_ErrorsPerPattern[pErr->patternName];
    }

    // Assorted actions if we have the actual physAddr
    //
    if (pErr->type == ErrType::NORMAL ||
        pErr->type == ErrType::NORMAL_UNK ||
        pErr->type == ErrType::NORMAL_ECC)
    {
        if (pErr->type != ErrType::NORMAL_ECC)
        {
            using BlacklistSource = Memory::BlacklistSource;

#ifndef MATS_STANDALONE
            // Blacklist the failing page
            //
            if (m_BlacklistPagesOnError)
            {
                CHECK_RC(m_pFrameBuffer->BlacklistPage(
                                pErr->addr,
                                pErr->pteKind,
                                pErr->pageSizeKB,
                                BlacklistSource::MemError,
                                0));
            }

            // Row remap failing row
            if (m_RowRemapOnError)
            {
                CHECK_RC(m_pFrameBuffer->RemapRow(RowRemapper::Request(pErr->addr,
                                                                       MemErrT::MEM_ERROR)));
            }
#endif

            // Dump the failing page
            //
            if (m_DumpPagesOnError)
            {
                CHECK_RC(m_pFrameBuffer->DumpFailingPage(
                                pErr->addr,
                                pErr->pteKind,
                                pErr->pageSizeKB,
                                BlacklistSource::MemError,
                                0,
                                pErr->originTestId));
            }
        }

        // Detect failures in the critical ranges
        //
        for (const FbRange &range: m_CriticalFbRanges)
        {
            if (pErr->addr >= range.start &&
                pErr->addr < range.start + range.size)
            {
                m_CriticalMemFailure = true;
            }
        }
    }

    // Append this error to the error log
    //
    if (m_ErrorLog.size() < m_MaxErrors)
    {
        m_ErrorLog.push_back(move(pErr));
    }

    return OK;
}

//------------------------------------------------------------------------------
// Use LogOffsetError when you don't know the exact address of the failure
// (e.g., when using a blit).  The value of sampleAddr must be an address
// that has the correct FBIO, subpartition, pseudoChannel, and beatOffset,
// but the other RBC components (rank, bank, row, column, etc) may be wrong.
//------------------------------------------------------------------------------
RC MemError::LogOffsetError
(
    UINT32 width,
    UINT64 sampleAddr,
    UINT64 actual,
    UINT64 expected,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    const string &patternName,
    UINT32 patternOffset
)
{
    Tasker::MutexHolder lock(m_pMutex);
    ErrorRecordPtr pErr(new ErrorRecord);
    RC rc;

    pErr->width          = width;
    pErr->addr           = sampleAddr;
    pErr->actual         = actual;
    pErr->expected       = expected;
    pErr->pteKind        = pteKind;
    pErr->pageSizeKB     = pageSizeKB;
    pErr->patternName    = patternName;
    pErr->patternOffset  = patternOffset;

    CHECK_RC(LogOffsetError(move(pErr)));
    return OK;
}

RC MemError::LogOffsetError(ErrorRecordPtr pErr)
{
    Tasker::MutexHolder lock(m_pMutex);
    RC rc;
    if (!m_IgnoreUnknownErrors)
        CHECK_RC(LogHelper(move(pErr), ErrType::OFFSET, IoType::UNKNOWN));
    return OK;
}

//------------------------------------------------------------------------------
// use LogUnknownMemoryError when you know the exact address of the
// failure, but not whether it was a read vs. write error (GLStress).
//------------------------------------------------------------------------------
RC MemError::LogUnknownMemoryError
(
    UINT32 width,
    UINT64 physAddr,
    UINT64 actual,
    UINT64 expected,
    UINT32 pteKind,
    UINT32 pageSizeKB
)
{
    Tasker::MutexHolder lock(m_pMutex);
    ErrorRecordPtr pErr(new ErrorRecord);
    RC rc;

    pErr->width       = width;
    pErr->addr        = physAddr;
    pErr->actual      = actual;
    pErr->expected    = expected;
    pErr->pteKind     = pteKind;
    pErr->pageSizeKB  = pageSizeKB;

    CHECK_RC(LogUnknownMemoryError(move(pErr)));
    return OK;
}

RC MemError::LogUnknownMemoryError(ErrorRecordPtr pErr)
{
    Tasker::MutexHolder lock(m_pMutex);
    RC rc;
    if (!m_IgnoreUnknownErrors)
        CHECK_RC(LogHelper(move(pErr), ErrType::NORMAL_UNK, IoType::UNKNOWN));
    return OK;
}

//------------------------------------------------------------------------------
// Use LogMemoryError when you know the exact address of the failure (like when
// doing a PCI read/write) and can repeat the operation multiple times.
// (This will be used to determine if it is a "read" or "write" error.
//------------------------------------------------------------------------------
RC MemError::LogMemoryError
(
    UINT32 width,
    UINT64 physAddr,
    UINT64 actual,
    UINT64 expected,
    UINT64 reRead1,
    UINT64 reRead2,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    UINT64 timeStamp,
    const string &patternName,
    UINT32 patternOffset
)
{
    Tasker::MutexHolder lock(m_pMutex);
    ErrorRecordPtr pErr(new ErrorRecord);
    RC rc;

    pErr->width         = width;
    pErr->addr          = physAddr;
    pErr->actual        = actual;
    pErr->expected      = expected;
    pErr->reRead1       = reRead1;
    pErr->reRead2       = reRead2;
    pErr->pteKind       = pteKind;
    pErr->pageSizeKB    = pageSizeKB;
    pErr->timeStamp     = timeStamp;
    pErr->patternName   = patternName;
    pErr->patternOffset = patternOffset;

    CHECK_RC(LogMemoryError(move(pErr)));
    return OK;
}

RC MemError::LogMemoryError(ErrorRecordPtr pErr)
{
    Tasker::MutexHolder lock(m_pMutex);
    RC rc;
    const IoType ioType =
        (pErr->reRead1 == pErr->actual && pErr->reRead2 == pErr->actual) ?
        IoType::WRITE : IoType::READ;

    if (!(ioType == IoType::WRITE && m_IgnoreWriteErrors) &&
        !(ioType == IoType::READ && m_IgnoreReadErrors))
    {
        CHECK_RC(LogHelper(move(pErr), ErrType::NORMAL, ioType));
    }
    return OK;
}

//------------------------------------------------------------------------------
// Use LogFailingBits when you know the FBIO, subpartition and
// bit of the failure but do not know the exact address or offset.
//------------------------------------------------------------------------------
RC MemError::LogFailingBits
(
    UINT32 virtualFbio,
    UINT32 subpartition,
    UINT32 pseudoChannel,
    UINT32 rank,
    UINT32 beatMask,
    UINT32 beatOffset,
    UINT32 bitOffset,
    UINT64 wordCount,
    UINT64 bitCount,
    IoType ioType
)
{
    Tasker::MutexHolder lock(m_pMutex);
    // If we don't know which beat failed, assume the worst.

    // If we don't know which beat(s) failed, assume the worst.
    if (0 == beatMask)
        beatMask = 0xffffffffU;

    m_ErrorCount += wordCount;

    IncrementCounter(&m_WordErrors, wordCount, ioType,
                     virtualFbio, subpartition, rank);

    switch (ioType)
    {
        case IoType::READ:
            m_ReadErrorCount += wordCount;
            break;
        case IoType::WRITE:
            m_WriteErrorCount += wordCount;
            break;
        case IoType::UNKNOWN:
            m_UnknownErrorCount += wordCount;
            break;
        default:
            MASSERT(!"Unknown IoType");
    }

    const UINT32 fbioBitOffset = m_pFrameBuffer->GetFbioBitOffset(subpartition,
                                                                  pseudoChannel,
                                                                  beatOffset,
                                                                  bitOffset);
    for (UINT32 beat = 0; beat < m_BeatsPerBurst; ++beat)
    {
        if (0 == (beatMask & (1 << beat)))
            continue;
        IncrementCounter(&m_BitErrors, bitCount, ioType, FlipDir::UNKNOWN,
                         virtualFbio, rank, beat, fbioBitOffset);
    }
    return OK;
}

//------------------------------------------------------------------------------
// Use LogEccError when an asynchronous ECC error oclwrs.
//------------------------------------------------------------------------------
/* static */ RC MemError::LogEccError
(
    const FrameBuffer *pFb,
    UINT64 physAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    bool   isSbe,
    UINT32 sbeBitPosition
)
{
    Tasker::MutexHolder lockRegistry(s_pRegistryMutex);
    RC rc;

    for (MemError *pMemError: s_Registry)
    {
        if (pMemError->GetFB() == pFb && pMemError->m_TestInitialized)
        {
            ErrorRecordPtr pErr(new ErrorRecord);
            pErr->width      = 4;
            pErr->addr       = physAddr;
            pErr->pteKind    = pteKind;
            pErr->pageSizeKB = pageSizeKB;
            if (isSbe)
            {
                // For pascal+ ECC errors, expected is always 0, while
                // actual is the bit error for SBEs and 0 for DBEs.
                //
                // For pre-pascal, in which we don't know the bad bit
                // for SBEs, we set expected=actual=1.  This maintains
                // the colwentions that expected^actual is the bit
                // errors (if known), and that actual=nonzero
                // indicates an SBE.
                //
                if (sbeBitPosition == _UINT32_MAX)
                {
                    pErr->expected = 1;
                    pErr->actual   = 1;
                }
                else
                {
                    MASSERT(sbeBitPosition < 32);
                    pErr->actual = 1ULL << sbeBitPosition;
                }
            }
            CHECK_RC(pMemError->LogHelper(move(pErr), ErrType::NORMAL_ECC,
                                          IoType::UNKNOWN));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// Print a summary of the errors in each subpartition
//------------------------------------------------------------------------------
void MemError::ErrorSubpartSummary()
{
    bool hasFilledUnknowns = false;
    if (m_RankCount > 1)
    {
        for (UINT32 virtualFbio = 0; virtualFbio < m_FbioCount; ++virtualFbio)
        {
            for (UINT32 subpart = 0; subpart < m_NumSubpartitions; subpart++)
            {
                if (ReadCounter(m_WordErrors, IoType::READ,
                                virtualFbio, subpart, m_UnknownRank) |
                    ReadCounter(m_WordErrors, IoType::WRITE,
                                virtualFbio, subpart, m_UnknownRank) |
                    ReadCounter(m_WordErrors, IoType::UNKNOWN,
                                virtualFbio, subpart, m_UnknownRank))
                {
                    hasFilledUnknowns = true;
                    break;
                }
            }
            if (hasFilledUnknowns)
            {
                break;
            }
        }
    }

    TablePrinter tablePrinter(Tee::PriNormal,
                              "\n=== MEMORY ERRORS BY SUBPARTITION ===");
    tablePrinter.AddColumn("-SUBPART");
    if (m_RankCount > 1)
        tablePrinter.AddColumn("RANK");
    tablePrinter.AddColumn("READ ERRORS");
    tablePrinter.AddColumn("WRITE ERRORS");
    tablePrinter.AddColumn("UNKNOWN ERRS");

    const UINT32 rankLimit = (m_RankCount > 1 ? m_UnknownRank + 1
                                              : m_RankCount);
    while (tablePrinter.NextPhase())
    {
        for (UINT32 virtualFbio = 0; virtualFbio < m_FbioCount; ++virtualFbio)
        {
            for (UINT32 subpart = 0; subpart < m_NumSubpartitions; subpart++)
            {
                for (UINT32 rank = 0; rank < rankLimit; ++rank)
                {
                    if (m_RankCount > 1 &&
                        rank == m_UnknownRank &&
                        !hasFilledUnknowns)
                    {
                        continue;
                    }
                    if (m_pFrameBuffer->IsSOC())
                    {
                        tablePrinter.AddCell(Utility::StrPrintf(
                            "CHAN%d%u",
                            m_pFrameBuffer->VirtualFbioToHwFbio(virtualFbio),
                            subpart));
                    }
                    else
                    {
                        tablePrinter.AddCell(Utility::StrPrintf(
                            "FBIO%c%u",
                            m_pFrameBuffer->VirtualFbioToLetter(virtualFbio),
                            subpart));
                    }
                    if (m_RankCount > 1)
                    {
                        tablePrinter.AddCell(
                                rank == m_UnknownRank
                                ? "?" : Utility::StrPrintf("%u", rank));
                    }
                    tablePrinter.AddCell(Utility::StrPrintf("%llu",
                           ReadCounter(m_WordErrors, IoType::READ,
                                       virtualFbio, subpart, rank)));
                    tablePrinter.AddCell(Utility::StrPrintf("%llu",
                           ReadCounter(m_WordErrors, IoType::WRITE,
                                       virtualFbio, subpart, rank)));
                    tablePrinter.AddCell(Utility::StrPrintf("%llu",
                           ReadCounter(m_WordErrors, IoType::UNKNOWN,
                                       virtualFbio, subpart, rank)));

                    // No MLE in standalone MATS
#ifndef MATS_STANDALONE
                    vector<string> typeStrs = { "MISC-RD", "MISC-WR", "MISC" };
                    if (tablePrinter.InPrintingPhase())
                    {
                        for (UINT32 ioType = 0;
                             ioType < static_cast<UINT32>(IoType::NUM_VALUES);
                             ioType++)
                        {
                            auto count = ReadCounter(m_WordErrors, ioType,
                                virtualFbio, subpart, rank);
                            if (count == 0)
                            {
                                continue;
                            }

                            auto hwFbio = m_pFrameBuffer->VirtualFbioToHwFbio(virtualFbio);
                            Mle::Print(Mle::Entry::mem_err_ctr)
                                .location("FB")
                                .type(typeStrs[ioType])
                                .error_src(Mle::MemErrCtr::mats_info)
                                .part_gpc(hwFbio)
                                .subp_tpc(subpart)
                                .count(static_cast<UINT32>(count))
                                .rc(static_cast<UINT32>(RC::BAD_MEMORY));
                        }
                    }
#endif
                } // for rank
            } // for subpart
        } // for virtualFbio
    } // while NextPhase()
}

//------------------------------------------------------------------------------
// Get list of the failing bits
//------------------------------------------------------------------------------
vector<pair<UINT32, UINT32>> MemError::GetBadBitList
(
    UINT32 rank,
    UINT64 minErrThresh
) const
{
    vector< pair<UINT32, UINT32> > failingBits;
    for (UINT32 virtualFbio = 0; virtualFbio < m_FbioCount; ++virtualFbio)
    {
        for (UINT32 bit = 0; bit < m_BitsPerFbio; ++bit)
        {
            UINT64 count = 0;

            for (UINT32 beat = 0; beat < m_BeatsPerBurst; ++beat)
            {
                for (UINT32 ioType = 0;
                     ioType < static_cast<UINT32>(IoType::NUM_VALUES);
                     ++ioType)
                {
                    for (UINT32 flipDir = 0;
                         flipDir < static_cast<UINT32>(FlipDir::NUM_VALUES);
                         ++flipDir)
                    {
                        count += ReadCounter(m_BitErrors, ioType, flipDir,
                                             virtualFbio, rank, beat, bit);
                    } // for flipDir
                } // for ioType
            } // for beat

            if (count > minErrThresh)
            {
                failingBits.push_back(make_pair(virtualFbio, bit));
            } // if
        } // for bit
    } // for virtualFbio

    return failingBits;
}

//------------------------------------------------------------------------------
// Print out a sub-list of the failing bits
//------------------------------------------------------------------------------
void MemError::PrintBadBitList(UINT32 rank)
{
    vector< pair<UINT32, UINT32> > failingBits = GetBadBitList(rank);

    vector<string> badBitStrings;

    for (UINT32 i = 0; i < failingBits.size(); i++)
    {
        Tee::ScreenPrintState sps = Tee::SPS_NORMAL;
        if (m_pFrameBuffer->IsSOC())
        {
            string badBit = Utility::StrPrintf("%3u", failingBits[i].second);
            Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), sps,
                   "%s", badBit.c_str());
            badBitStrings.push_back(badBit);
        }
        else
        {
            string badBit = Utility::StrPrintf("%c%03u",
                m_pFrameBuffer->VirtualFbioToLetter(failingBits[i].first),
                failingBits[i].second);

            Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), sps, "%s", badBit.c_str());
            badBitStrings.push_back(badBit);

        }
        Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(),
               Tee::SPS_NORMAL, " ");

        if ((i % 16 == 15) || (i == failingBits.size() - 1))
        {
            Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(),
                   Tee::SPS_NORMAL, "\n");
        }
    }

    if (failingBits.empty())
    {
        Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL,
               "None\n");
    }

    Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, "\n");

    // No MLE for stand-alone MATS
#ifndef MATS_STANDALONE
    if (!badBitStrings.empty())
    {
        ByteStream bs;
        auto entry = Mle::Entry::mem_bad_bits(&bs);
        entry.rank(rank);
        for (const auto& str : badBitStrings)
        {
            entry.bits(Mle::ProtobufString(str));
        }
        entry.Finish();
        Tee::PrintMle(&bs);
    }
#endif
}

//------------------------------------------------------------------------------
// Print a binary-encoded Error Record object to the MLE file
//------------------------------------------------------------------------------
void MemError::PrintMemErrorToMle(const ErrorRecord& errorRecord, const FbDecode& decode)
{
    // No MLE for stand-alone MATS
#ifndef MATS_STANDALONE

    bool isHbm = m_pFrameBuffer->IsHbm();

    // We have an address decode, so we can assume FB
    string loc = "FB";

    string type;

    // All non-ECC errors are considered simple miscompares
    // Single bit errors will mark which bit failed, double-
    // bit errors don't record anything because they can't
    if (errorRecord.type != ErrType::NORMAL_ECC)
    {
        type = "MISCOMPARE";
    }
    else if (errorRecord.actual != 0)
    {
        type = "SBE";
    }
    else
    {
        type = "DBE";
    }
    UINT64 addr = errorRecord.addr;
    UINT08 fbio = static_cast<UINT08>(m_pFrameBuffer->VirtualFbioToHwFbio(decode.virtualFbio));
    UINT08 subp = static_cast<UINT08>(decode.subpartition);
    UINT08 pseudoChan = static_cast<UINT08>(decode.pseudoChannel);
    UINT08 bank = static_cast<UINT08>(decode.bank);
    UINT32 row = decode.row;
    UINT32 col = decode.extColumn;
    UINT08 beat = static_cast<UINT08>(decode.beat);
    UINT08 beatOffset = static_cast<UINT08>(decode.beatOffset);
    UINT08 rank = static_cast<UINT08>(decode.rank);

    vector<UINT32> bits;
    UINT64 errorMask = errorRecord.actual ^ errorRecord.expected;
    for (INT32 badBit = Utility::BitScanForward64(errorMask);
         badBit >= 0;
         badBit = Utility::BitScanForward64(errorMask, badBit + 1))
    {
        bits.push_back(GetFbioBitOffset(decode, errorRecord, badBit));
    }

    if (isHbm)
    {
        UINT08 hbmSite = static_cast<UINT08>(decode.hbmSite);
        UINT08 hbmChan = static_cast<UINT08>(decode.hbmChannel);

        Mle::Print(Mle::Entry::mem_err)
            .location(loc)
            .type(type)
            .addr(addr)
            .fbio(fbio)
            .subp(subp)
            .pseudo_chan(pseudoChan)
            .bank(bank)
            .row(row)
            .col(col)
            .beat(beat)
            .beat_offs(beatOffset)
            .rank(rank)
            .hbm_site(hbmSite)
            .hbm_chan(hbmChan)
            .bits(bits)
            .rc(static_cast<UINT32>(RC::BAD_MEMORY));
    }
    else
    {
        Mle::Print(Mle::Entry::mem_err)
            .location(loc)
            .type(type)
            .addr(addr)
            .fbio(fbio)
            .subp(subp)
            .pseudo_chan(pseudoChan)
            .bank(bank)
            .row(row)
            .col(col)
            .beat(beat)
            .beat_offs(beatOffset)
            .rank(rank)
            .bits(bits)
            .rc(static_cast<UINT32>(RC::BAD_MEMORY));
    }
#endif
}

//------------------------------------------------------------------------------
// Print out a list of the failing bits
//------------------------------------------------------------------------------
void MemError::ErrorBitsSummary()
{
    Printf(Tee::PriNormal, "\n");
    if (m_RankCount > 1)
    {
        for (UINT32 rank = 0; rank < m_RankCount; ++rank)
        {
            Printf(Tee::PriNormal, "Rank %d Failing Bits:\n", rank);
            PrintBadBitList(rank);
        }
        Printf(Tee::PriNormal, "Unknown Rank Failing Bits: \n");
        PrintBadBitList(m_UnknownRank);
    }
    else
    {
        Printf(Tee::PriNormal, "Failing Bits: \n");
        PrintBadBitList(0);
    }
}

//------------------------------------------------------------------------------
// Thin wrapper around FrameBuffer::DecodeAddress() that passes the
// memory address of the first failing bit in the error.
//------------------------------------------------------------------------------
RC MemError::DecodeErrorAddress
(
    FbDecode*          pDecode,
    const ErrorRecord& error
) const
{
    return DecodeErrorAddress(pDecode, error, error.actual ^ error.expected);
}

RC MemError::DecodeErrorAddress
(
    FbDecode*          pDecode,
    const ErrorRecord& error,
    UINT64             errorMask
) const
{
    const INT32  firstBitError  = Utility::BitScanForward64(errorMask);
    const UINT32 firstByteError = firstBitError >= 0 ? firstBitError / 8 : 0;
    return m_pFrameBuffer->DecodeAddress(pDecode, error.addr + firstByteError,
                                         error.pteKind, error.pageSizeKB);
}

//------------------------------------------------------------------------------
// Thin wrapper around FrameBuffer::GetFbioBitOffset() that modifies
// the bitOffset arg to compensate for the memory address callwlated
// by DecodeErrorAddress().
//------------------------------------------------------------------------------
UINT32 MemError::GetFbioBitOffset
(
    const FbDecode&    decode,
    const ErrorRecord& error,
    UINT32             bitOffset
) const
{
    return GetFbioBitOffset(decode, error.actual ^ error.expected, bitOffset);
}

UINT32 MemError::GetFbioBitOffset
(
    const FbDecode& decode,
    UINT64          errorMask,
    UINT32          bitOffset
) const
{
    const INT32  firstBitError  = Utility::BitScanForward64(errorMask);
    const UINT32 firstByteError = firstBitError >= 0 ? firstBitError / 8 : 0;
    return m_pFrameBuffer->GetFbioBitOffset(decode.subpartition,
                                            decode.pseudoChannel,
                                            decode.beatOffset,
                                            bitOffset - 8 * firstByteError);
}

//------------------------------------------------------------------------------
// Print out a list of the failing bits
//------------------------------------------------------------------------------
RC MemError::ErrorBitsDetailed()
{
    Tasker::MutexHolder lock(m_pMutex);
    TablePrinter tablePrinter(Tee::PriNormal,
                              "\n=== MEMORY ERRORS BY BIT ===");
    tablePrinter.AddColumn("<P>",
                           m_pFrameBuffer->IsSOC()
                           ? "Partition (SOC Channel)" : "Partition (FBIO)");
    tablePrinter.AddColumn("BIT");
    if (m_RankCount > 1)
    {
        tablePrinter.AddColumn("RANK");
    }
    tablePrinter.AddColumn("READ ERRORS");
    tablePrinter.AddColumn("WRITE ERRORS");
    tablePrinter.AddColumn("UNKNOWN ERRS");
    tablePrinter.AddColumn("READ 0\nEXP. 1");
    tablePrinter.AddColumn("READ 1\nEXP. 0");
    tablePrinter.AddColumn("READ ?\nEXP. ?");

    const UINT32 rankLimit = (m_RankCount > 1 ? m_UnknownRank + 1
                                              : m_RankCount);
    while (tablePrinter.NextPhase())
    {
        for (UINT32 virtualFbio = 0; virtualFbio < m_FbioCount; ++virtualFbio)
        {
            for (UINT32 subpart = 0; subpart < m_NumSubpartitions; ++subpart)
            {
                for (UINT32 bit = 0; bit < m_BitsPerSubpartition; bit++)
                {
                    for (UINT32 rank = 0; rank < rankLimit; ++rank)
                    {
                        const UINT32 fbioBit =
                            subpart * m_BitsPerSubpartition + bit;
                        UINT64 readCount      = 0;
                        UINT64 writeCount     = 0;
                        UINT64 unknownCount   = 0;
                        UINT64 r0x1Count      = 0;
                        UINT64 r1x0Count      = 0;
                        UINT64 rxUnknownCount = 0;
                        for (UINT32 beat = 0; beat < m_BeatsPerBurst; ++beat)
                        {
                            const UINT64 readR0x1 =
                                ReadCounter(m_BitErrors, IoType::READ,
                                            FlipDir::READ0_EXPECT1,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 readR1x0 =
                                ReadCounter(m_BitErrors, IoType::READ,
                                            FlipDir::READ1_EXPECT0,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 readRxUnknown =
                                ReadCounter(m_BitErrors, IoType::READ,
                                            FlipDir::UNKNOWN,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 writeR0x1 =
                                ReadCounter(m_BitErrors, IoType::WRITE,
                                            FlipDir::READ0_EXPECT1,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 writeR1x0 =
                                ReadCounter(m_BitErrors, IoType::WRITE,
                                            FlipDir::READ1_EXPECT0,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 writeRxUnknown =
                                ReadCounter(m_BitErrors, IoType::WRITE,
                                            FlipDir::UNKNOWN,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 unknownR0x1 =
                                ReadCounter(m_BitErrors, IoType::UNKNOWN,
                                            FlipDir::READ0_EXPECT1,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 unknownR1x0 =
                                ReadCounter(m_BitErrors, IoType::UNKNOWN,
                                            FlipDir::READ1_EXPECT0,
                                            virtualFbio, rank, beat, fbioBit);
                            const UINT64 unknownRxUnknown =
                                ReadCounter(m_BitErrors, IoType::UNKNOWN,
                                            FlipDir::UNKNOWN,
                                            virtualFbio, rank, beat, fbioBit);
                            readCount      += (readR0x1 + readR1x0 +
                                               readRxUnknown);
                            writeCount     += (writeR0x1 + writeR1x0 +
                                               writeRxUnknown);
                            unknownCount   += (unknownR0x1 + unknownR1x0 +
                                               unknownRxUnknown);
                            r0x1Count      += (readR0x1 + writeR0x1 +
                                               unknownR0x1);
                            r1x0Count      += (readR1x0 + writeR1x0 +
                                               unknownR1x0);
                            rxUnknownCount += (readRxUnknown + writeRxUnknown +
                                               unknownRxUnknown);
                        }
                        if (r0x1Count == 0 && r1x0Count == 0 &&
                            rxUnknownCount == 0)
                        {
                            continue;
                        }
                        tablePrinter.AddCell(FbioString(m_pFrameBuffer,
                                                        virtualFbio));
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%03u", fbioBit));
                        if (m_RankCount > 1)
                        {
                            tablePrinter.AddCell(
                                    rank == m_UnknownRank
                                    ? "?" : Utility::StrPrintf("%u", rank));

                        }
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%llu", readCount));
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%llu", writeCount));
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%llu", unknownCount));
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%llu", r0x1Count));
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%llu", r1x0Count));
                        tablePrinter.AddCell(Utility::StrPrintf(
                                        "%llu", rxUnknownCount));
                    } // for rank
                } // for bit
            } // for subpart
        } // for virtualFbio
    } // while tablePrinter.NextPhase()

    return OK;
}

RC MemError::ResolveMemErrorResult()
{
    Tasker::MutexHolder lock(m_pMutex);
    if (GetErrorCount() == 0)
    {
        return OK;
    }

    if (GetAutoStatus())
    {
        PrintStatus();
        PrintErrorLog(GetMaxPrintedErrors());
    }
    else if (!m_HasHelpMessageBeenPrinted)
    {
        Printf(Tee::PriNormal, "Errors found.\n"
               "This message will only appear once.\n");
        m_HasHelpMessageBeenPrinted = true;
    }

#ifndef MATS_STANDALONE
    if (GetCriticalMemFailure())
        return RC::BAD_MEMORY_CRITICAL;
#endif
    return RC::BAD_MEMORY;
}

#ifndef MATS_STANDALONE
void MemError::AddErrCountsToJsonItem(JsonItem * pJsonExit)
{
    Tasker::MutexHolder lock(m_pMutex);
    const UINT32 fbiosPerFbp  = m_pFrameBuffer->GetFbiosPerFbp();
    const UINT32 maxFbpCount  = m_pFrameBuffer->GetMaxFbpCount();
    const UINT32 maxFbioCount = m_pFrameBuffer->GetMaxFbioCount();
    MASSERT(maxFbioCount == maxFbpCount * fbiosPerFbp);

    // Count errors indexed in two ways:
    // (1) By hwFbio and channel (subpartition)
    // (2) By hwFbp
    //
    vector<UINT64> errsByFbioChan(maxFbioCount * m_NumSubpartitions, 0);
    vector<UINT64> errsByFbp(maxFbpCount, 0);

    for (UINT32 virtualFbio = 0; virtualFbio < m_FbioCount; ++virtualFbio)
    {
        const UINT32 hwFbio = m_pFrameBuffer->VirtualFbioToHwFbio(virtualFbio);
        const UINT32 hwFbpa = m_pFrameBuffer->HwFbioToHwFbpa(hwFbio);
        const UINT32 hwFbp  = hwFbpa / fbiosPerFbp;

        for (UINT32 ch = 0; ch < m_NumSubpartitions; ++ch)
        {
            UINT64 errs = 0;
            for (UINT32 rank = 0; rank < m_RankCount; ++rank)
            {
                for (UINT32 ioType = 0;
                     ioType < static_cast<UINT32>(IoType::NUM_VALUES);
                     ++ioType)
                {
                    errs += ReadCounter(m_WordErrors, ioType,
                                        virtualFbio, ch, rank);
                }
            }
            errsByFbioChan[hwFbio * m_NumSubpartitions + ch] = errs;
            errsByFbp[hwFbp] += errs;
        }
    }

    // Build JsArrays with the same data we callwlated above
    //
    JavaScriptPtr pJs;
    JsArray jsaByFbioChan;  // Error counts by hwFbio & channel
    JsArray jsaByFbp;       // Error counts by hwFbp
    jsval jsv;

    for (const auto err : errsByFbioChan)
    {
        pJs->ToJsval(err, &jsv);
        jsaByFbioChan.push_back(jsv);
    }

    for (const auto err : errsByFbp)
    {
        pJs->ToJsval(err, &jsv);
        jsaByFbp.push_back(jsv);
    }

    pJsonExit->SetField("memerrors_by_fbio_channel", &jsaByFbioChan);
    pJsonExit->SetField("memerrors_by_fbp", &jsaByFbp);
    pJsonExit->SetField("memerrors_total", m_ErrorCount);
}
#endif

//! Get the FrameBuffer object for the MemError test
FrameBuffer *MemError::GetFB()
{
    return m_pFrameBuffer;
}
