/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mle_protobuf.h"
#include "gpu/repair/row_remapper/row_remapper.h"

#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "ctrl/ctrl208f/ctrl208ffb.h"
#include "gpu/include/gpusbdev.h"
#include <tuple>

string ToString(RowRemapper::Source source)
{
    switch (source)
    {
        case RowRemapper::Source::FACTORY: return "factory";
        case RowRemapper::Source::FIELD:   return "field";
        case RowRemapper::Source::MODS:    return "MODS";
        default:                           return "unknown";
    }
}

RC RowRemapper::ToRmSource
(
    RowRemapper::Source source,
    Memory::ErrorType error,
    UINT08* pOut
)
{
    MASSERT(pOut);

    switch (source)
    {
        case Source::MODS:
            *pOut = LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_MODS_MEM_ERROR;
            break;

        case Source::FACTORY:
            switch (error)
            {
                case MemErrT::SBE:
                    *pOut = LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_SBE_FACTORY;
                    break;

                case MemErrT::DBE:
                    *pOut = LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_DBE_FACTORY;
                    break;

                case MemErrT::MEM_ERROR:
                    // If the error type is generic, use the generic source.
                    *pOut = LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_MODS_MEM_ERROR;
                    break;

                default:
                    Printf(Tee::PriError,
                           "Unsupported error type for source 'FACTORY': %s\n",
                           ToString(error).c_str());
                    MASSERT(!"Unsupported error type for source 'FACTORY'");
                    return RC::BAD_PARAMETER;
            }
            break;

        case Source::FIELD:
            switch (error)
            {
                case MemErrT::SBE:
                    *pOut = LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_SBE_FIELD;
                    break;

                case MemErrT::DBE:
                // If the error type is generic, use the field DBE entry to
                // avoid confusing lwpu-smi with non-field data. This is the
                // same strategy used by page blacklisting.
                case MemErrT::MEM_ERROR:
                    *pOut = LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_DBE_FIELD;
                    break;

                default:
                    Printf(Tee::PriError,
                           "Unsupported error type for source 'FIELD': %s\n",
                           ToString(error).c_str());
                    MASSERT(!"Unsupported error type for source 'FIELD'");
                    return RC::BAD_PARAMETER;
            }
            break;

        default:
            Printf(Tee::PriError, "Unsupported source type: %s\n",
                   ToString(source).c_str());
            MASSERT(!"Unsupported source type");
            return RC::BAD_PARAMETER;
    }

    return RC::OK;
}

RC RowRemapper::FromRmSource
(
    UINT08 rmVal,
    RowRemapper::Source* pSource,
    Memory::ErrorType* pError
)
{
    MASSERT(pSource);
    MASSERT(pError);

    switch (rmVal)
    {
        case LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_SBE_FACTORY:
            *pSource = Source::FACTORY;
            *pError  = MemErrT::SBE;
            break;

        case LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_DBE_FACTORY:
            *pSource = Source::FACTORY;
            *pError  = MemErrT::DBE;
            break;

        case LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_SBE_FIELD:
            *pSource = Source::FIELD;
            *pError  = MemErrT::SBE;
            break;

        case LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_DBE_FIELD:
            *pSource = Source::FIELD;
            *pError  = MemErrT::DBE;
            break;

        case LW2080_CTRL_FB_REMAPPED_ROW_SOURCE_MODS_MEM_ERROR:
            *pSource = Source::MODS;
            *pError  = MemErrT::NONE;
            break;

        default:
            Printf(Tee::PriError,
                   "Unknown RM remapped row source value: %#X\n", rmVal);
            return RC::BAD_PARAMETER;
    }

    return RC::OK;
}

RowRemapper::RowRemapper(const GpuSubdevice* pSubdev, LwRm* pLwRm)
    : m_pSubdev(pSubdev), m_pLwRm(pLwRm)
{
    MASSERT(pSubdev);
    // NOTE: pLwRm can be null if RM is not loaded.
}

RC RowRemapper::Initialize()
{
    JavaScriptPtr pJs;
    RC rc = pJs->GetProperty(pJs->GetGlobalObject(), "g_RowRemapAsField",
                             &m_RowRemapAsField);
    if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        // Keep m_RowRemapAsField to its default value if undefined in JS
        Printf(Tee::PriLow, "Global variable g_RowRemapAsField not defined!\n"
               "There is a hard-coded assumption that this variable must exist,"
               "which will be addressed by JIRA-1544.\nUntil then, ignore this"
               "if you do not expect row remapping to be done."
        );
        rc.Clear();
    }

    m_Initialized = true;
    return rc;
}

bool RowRemapper::IsEnabled() const
{
    MASSERT(m_Initialized);
    const RegHal& regs = m_pSubdev->Regs();
    return regs.Test32(MODS_FUSE_FEATURE_READOUT_2_ROW_REMAPPER_ENABLED);
}

RC RowRemapper::GetNumInfoRomRemappedRows(UINT32* pNumRows) const
{
    MASSERT(m_Initialized);
    MASSERT(pNumRows);
    RC rc;

    LW2080_CTRL_FB_GET_REMAPPED_ROWS_PARAMS params = {};
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                                         LW2080_CTRL_CMD_FB_GET_REMAPPED_ROWS,
                                         &params,
                                         sizeof(params)));

    *pNumRows = params.entryCount;
    return rc;
}

RC RowRemapper::PrintInfoRomRemappedRows(const char * pPrefix) const
{
    RC rc;

    if (!m_pLwRm)
    {
        Printf(Tee::PriError, "RM not set\n");
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_FB_GET_REMAPPED_ROWS_PARAMS params = {};
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                                         LW2080_CTRL_CMD_FB_GET_REMAPPED_ROWS,
                                         &params,
                                         sizeof(params)));

    string prefix;
    if (pPrefix)
        prefix = Utility::StrPrintf("%s", pPrefix);

    string s = prefix + "Remapped Rows (sourced from InfoROM):\n";
    if (params.entryCount == 0)
    {
        s += prefix + "    [none]\n";
    }
    else
    {
        const RegHal& regs = m_pSubdev->Regs();

        s += prefix + "    Timestamp                 | Source   | Cause | HW FBPA | Subp | Rank | Bank | Row    | Remap Entry\n";
        s += prefix + "    --------------------------+----------+-------+---------+------+------+------+--------+------------\n";

        for (UINT32 i = 0; i < params.entryCount; ++i)
        {
            const LW2080_CTRL_FB_REMAP_ENTRY& entry = params.entries[i];

            Source source = Source::UNKNOWN;
            MemErrT cause = MemErrT::NONE;
            CHECK_RC(FromRmSource(entry.source, &source, &cause));

            const UINT32 rank = regs.GetField(entry.remapRegVal, MODS_PFB_FBPA_0_ROW_REMAP_WR_EXT_BANK);
            const UINT32 bank = regs.GetField(entry.remapRegVal, MODS_PFB_FBPA_0_ROW_REMAP_WR_BANK);
            const UINT32 row = regs.GetField(entry.remapRegVal, MODS_PFB_FBPA_0_ROW_REMAP_WR_ROW);
            const UINT32 tableEntry = regs.GetField(entry.remapRegVal, MODS_PFB_FBPA_0_ROW_REMAP_WR_ENTRY);

            s += prefix + Utility::StrPrintf("    %-25s | %8s | %5s | %7u | %4u | %4u | %4u | %6u | %11u\n",
                                    Utility::ColwertEpochToGMT(entry.timestamp).c_str(),
                                    ToString(source).c_str(),
                                    ToString(cause).c_str(), entry.fbpa,
                                    entry.sublocation, rank, bank, row, tableEntry);

            Mle::RowRemapInfo::Source sourceEnum = Mle::RowRemapInfo::unknown;
            switch (source)
            {
                case RowRemapper::Source::UNKNOWN: sourceEnum = Mle::RowRemapInfo::unknown; break;
                case RowRemapper::Source::NONE:    sourceEnum = Mle::RowRemapInfo::none; break;
                case RowRemapper::Source::FACTORY: sourceEnum = Mle::RowRemapInfo::factory; break;
                case RowRemapper::Source::FIELD:   sourceEnum = Mle::RowRemapInfo::field; break;
                case RowRemapper::Source::MODS:    sourceEnum = Mle::RowRemapInfo::mods; break;
                default:
                    MASSERT("Unknown row remap source colwersion");
                    break;
            }

            Mle::RowRemapInfo::Cause causeEnum = Mle::RowRemapInfo::cause_none;
            switch (cause)
            {
                case Memory::ErrorType::MEM_ERROR: causeEnum = Mle::RowRemapInfo::mem_error; break;
                case Memory::ErrorType::SBE:       causeEnum = Mle::RowRemapInfo::sbe; break;
                case Memory::ErrorType::DBE:       causeEnum = Mle::RowRemapInfo::dbe; break;
                case Memory::ErrorType::NONE:      causeEnum = Mle::RowRemapInfo::cause_none; break;
                default:
                    MASSERT("Unknown memory error type colwersion");
                    break;
            }

            Mle::Print(Mle::Entry::row_remap_info)
                .timestamp(entry.timestamp)
                .source(sourceEnum)
                .cause(causeEnum)
                .fbpa(entry.fbpa)
                .subpa(entry.sublocation)
                .rank(rank)
                .bank(bank)
                .row(row)
                .entry(tableEntry);
        }

        s += prefix + Utility::StrPrintf("    Total remapped rows: %u\n", params.entryCount);
        const bool bRemapFailure =  FLD_TEST_DRF(2080_CTRL_FB,
                                                 _GET_REMAPPED_ROWS_FLAGS,
                                                 _FAILURE,
                                                 _TRUE,
                                                 params.flags);
        s += prefix + Utility::StrPrintf("Remap Failure Detected: %s\n",
                                         bRemapFailure ? "True" : "False");
    }

    Printf(Tee::PriNormal, "%s\n", s.c_str());
    return rc;
}

RC RowRemapper::CheckMaxRemappedRows(const MaxRemapsPolicy& policy) const
{
    using namespace Memory;
    RC rc;

    if (!m_pLwRm)
    {
        Printf(Tee::PriError, "RM not set\n");
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_FB_GET_REMAPPED_ROWS_PARAMS params = {};
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                                         LW2080_CTRL_CMD_FB_GET_REMAPPED_ROWS,
                                         &params,
                                         sizeof(params)));

    if (policy.totalRows != MaxRemapsPolicy::UNSET && params.entryCount > policy.totalRows)
    {
        Printf(Tee::PriError, "GPU has %u remapped rows; maximum allowed is %u.\n",
               params.entryCount, policy.totalRows);
        return RC::ROW_REMAP_FAILURE;
    }

    if (policy.numRowsPerBank != MaxRemapsPolicy::UNSET)
    {
        using BankLoc = std::tuple<HwFbpa, FbpaSubp, Rank, Bank>;
        map<BankLoc, UINT32> remapCounts;
        const RegHal& regs = m_pSubdev->Regs();

        for (UINT32 i = 0; i < params.entryCount; ++i)
        {
            const LW2080_CTRL_FB_REMAP_ENTRY& entry = params.entries[i];
            const Rank rank = Rank(regs.GetField(entry.remapRegVal,
                                                 MODS_PFB_FBPA_0_ROW_REMAP_WR_EXT_BANK));
            const Bank bank = Bank(regs.GetField(entry.remapRegVal,
                                                 MODS_PFB_FBPA_0_ROW_REMAP_WR_BANK));
            const BankLoc loc = std::make_tuple(HwFbpa(entry.fbpa),
                                                FbpaSubp(entry.sublocation),
                                                rank, bank);

            if ((++remapCounts[loc]) > policy.numRowsPerBank)
            {
                Printf(Tee::PriError, "GPU has %u rows remapped in HW FBPA %u, "
                       "sublocation %u, rank %u, bank %u; maximum allowed is %u.\n",
                       params.entryCount, entry.fbpa, entry.sublocation, rank.Number(),
                       bank.Number(), policy.numRowsPerBank);
                return RC::ROW_REMAP_FAILURE;
            }
        }
    }

    return rc;
}

RC RowRemapper::CheckRemapError() const
{
    RC rc;

    if (!m_pLwRm)
    {
        Printf(Tee::PriError, "RM not set\n");
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_FB_GET_REMAPPED_ROWS_PARAMS params = {};
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                                         LW2080_CTRL_CMD_FB_GET_REMAPPED_ROWS,
                                         &params,
                                         sizeof(params)));

    if (FLD_TEST_DRF(2080_CTRL_FB, _GET_REMAPPED_ROWS_FLAGS, _FAILURE, _TRUE, params.flags))
    {
        Printf(Tee::PriError, "GPU has detected a failure during row remapping.\n");
        return RC::ROW_REMAP_FAILURE;
    }

    return rc;
}

RC RowRemapper::Remap(const vector<Request>& requests) const
{
    MASSERT(m_Initialized);
    RC rc;

    if (!m_pLwRm)
    {
        Printf(Tee::PriError, "RM not set\n");
        return RC::SOFTWARE_ERROR;
    }

    constexpr UINT32 maxRowsPerCall = LW208F_CTRL_FB_REMAPPED_ROWS_MAX_ROWS;
    const size_t numRequests = requests.size();
    const UINT32 numRemapCalls
        = static_cast<UINT32>(std::ceil(numRequests
                                        / static_cast<double>(maxRowsPerCall)));

    if (numRemapCalls > 1)
    {
        Printf(Tee::PriWarn, "Asking to remap more rows than possible in one "
               "call: max %u, requested %zu\n"
               "    Performing %u row remap calls.\n",
               maxRowsPerCall, numRequests, numRemapCalls);
    }

    const Source remapSource = (m_RowRemapAsField ? Source::FIELD : Source::FACTORY);
    LW208F_CTRL_FB_REMAP_ROW_PARAMS params = {};

    StickyRC firstRc;
    UINT32 requestIdx = 0;
    for (UINT32 callNum = 0;
         callNum < numRemapCalls && requestIdx < numRequests;
         ++callNum)
    {
        params = {};

        // Populate row remap request
        //
        UINT32 entryIdx = 0;
        for (entryIdx = 0;
             entryIdx < maxRowsPerCall && requestIdx < numRequests;
             ++entryIdx, ++requestIdx)
        {
            const Request& request = requests[requestIdx];
            LW208F_CTRL_FB_REMAPPING_ADDRESS_INFO* pEntry = &params.addressList[entryIdx];

            pEntry->physicalAddress = request.physicalAddr;
            CHECK_RC(RowRemapper::ToRmSource(remapSource,
                                             request.cause,
                                             &pEntry->source));
        }
        const UINT32 validEntries = entryIdx;
        params.validEntries = validEntries;

        // Perform request
        //
        CHECK_RC(m_pLwRm->Control(m_pSubdev->GetSubdeviceDiag(),
                                  LW208F_CTRL_CMD_FB_REMAP_ROW,
                                  &params,
                                  sizeof(params)));

        MASSERT(params.validEntries == validEntries); // Valid entry count should not change
        MASSERT(params.numEntriesAdded <= validEntries);
        const bool errorOclwred = params.numEntriesAdded < validEntries;
        Printf((errorOclwred ? Tee::PriError : Tee::PriLow),
               "Row remap (call %u): requested %u, %u completed successfully\n",
               callNum, validEntries, params.numEntriesAdded);

        // Check result of each requested row remap
        //
        StickyRC firstRemapCallRc;
        bool errorFound = false;
        for (UINT32 entryIdx = 0; entryIdx < params.validEntries; ++entryIdx)
        {
            const LW208F_CTRL_FB_REMAPPING_ADDRESS_INFO& entry = params.addressList[entryIdx];

            RC remapRc;
            string msg;
            switch (entry.status)
            {
                case LW208F_CTRL_FB_REMAP_ROW_STATUS_OK:
                case LW208F_CTRL_FB_REMAP_ROW_STATUS_REMAPPING_PENDING:
                    // Pending implies it will be remapped at next boot.
                    break;

                case LW208F_CTRL_FB_REMAP_ROW_STATUS_TABLE_FULL:
                    msg = "Row remapping table is full. Unable to complete row remapping";
                    remapRc = RC::ROW_REMAP_FAILURE;
                    break;

                case LW208F_CTRL_FB_REMAP_ROW_STATUS_ALREADY_REMAPPED:
                    msg = "Row remapping error. Trying to remap an already remapped row";
                    remapRc = RC::ROW_REMAP_FAILURE;
                    break;

                case LW208F_CTRL_FB_REMAP_ROW_STATUS_INTERNAL_ERROR:
                    msg = "Row remapping triggered an RM internal error";
                    remapRc = RC::ROW_REMAP_FAILURE;
                    break;

                default:
                    msg = Utility::StrPrintf("Failed to row remap, unknown reason: %u",
                                             static_cast<UINT32>(entry.status));
                    remapRc = RC::ROW_REMAP_FAILURE;
                    break;
            }

            if (!msg.empty())
            {
                Printf(Tee::PriError, "Row remap (addr 0x%08llX): failure - %s\n",
                       entry.physicalAddress, msg.c_str());
                errorFound = true;
            }
            else
            {
                Printf(Tee::PriNormal, "Row remap (addr 0x%08llX): ok\n", entry.physicalAddress);
            }

            firstRemapCallRc = remapRc;
        }

        // Ensure if RM reported an error, we actually found it.
        if (errorOclwred && !errorFound)
        {
            Printf(Tee::PriError, "Row remap (call %u): RM reported an error, "
                   "but all entries succeeded!\n", callNum);
            return RC::SOFTWARE_ERROR;
        }

        if (firstRemapCallRc != RC::OK)
        {
            firstRc = firstRemapCallRc;
            firstRemapCallRc.Clear();

            const UINT32 lowerEntryNum = maxRowsPerCall * callNum;
            Printf(Tee::PriError, "Failed to row remap entries [%u, %u) of "
                   "requested addresses.\n",
                   lowerEntryNum, lowerEntryNum + params.validEntries);

            // Finish if an error oclwrred
            return firstRc;
        }
    }

    return firstRc;
}

RC RowRemapper::ClearRemappedRows(const vector<ClearSelection>& clearSelections) const
{
    MASSERT(m_Initialized);
    RC rc;

    if (!m_pLwRm)
    {
        Printf(Tee::PriError, "RM not set\n");
        return RC::SOFTWARE_ERROR;
    }

    LW208F_CTRL_FB_CLEAR_REMAPPED_ROWS_PARAMS params = {};
    for (const ClearSelection& sel : clearSelections)
    {
        UINT08 rmVal = 0;
        CHECK_RC(ToRmSource(sel.source, sel.cause, &rmVal));
        params.sourceMask |= (1U << rmVal);
    }

    return m_pLwRm->Control(m_pSubdev->GetSubdeviceDiag(),
                                       LW208F_CTRL_CMD_FB_CLEAR_REMAPPED_ROWS,
                                       &params,
                                       sizeof(params));
}

UINT64 RowRemapper::GetRowRemapReservedRamAmount() const
{
    FrameBuffer* pFb = m_pSubdev->GetFB();

    const UINT64 numRows = 1ULL << pFb->GetRowBits();
    (void)numRows;
    const UINT32 numRowRemapReservedRows = m_pSubdev->NumRowRemapTableEntries();
    MASSERT(numRows >= numRowRemapReservedRows);
    const UINT64 totalRowSize = (static_cast<UINT64>(pFb->GetRowSize()) *
                                 pFb->GetPseudoChannelsPerChannel());

    return totalRowSize * numRowRemapReservedRows * pFb->GetBankCount()
        * pFb->GetRankCount() * pFb->GetChannelsPerFbio() * pFb->GetFbioCount();
}

//-----------------------------------------------------------------------------
JS_CLASS(RowRemapperConst);
static SObject RowRemapperConst_Object
(
    "RowRemapperConst",
    RowRemapperConstClass,
    0,
    0,
    "RowRemapperConst JS Object"
);
PROP_CONST(RowRemapperConst, REMAP_POLICY_UNSET, RowRemapper::MaxRemapsPolicy::UNSET);
