/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2016,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "edccount.h"
#include "subdevfb.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

//--------------------------------------------------------------------
//! \brief Initialize the counter, and start checking for errors.
//!
RC EdcErrCounter::Initialize()
{
    const UINT32 numFbios = GetGpuSubdevice()->GetFB()->GetFbioCount();

    // Try to use the direct EDC counts since the other interface provides less detailed info
    if (GetGpuSubdevice()->Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_CNT))
    {
        m_UseNewEdcFormat = true;
    }
    else
    {
        m_EdcErrorRecords.resize(numFbios);
        m_EdcCrcs.resize(numFbios);
        for (UINT32 virtualFbio = 0; virtualFbio < numFbios; ++virtualFbio)
        {
            m_EdcErrorRecords[virtualFbio].clear();
            m_EdcErrorRecords[virtualFbio].resize(NUM_EDC_ERROR_RECORDS,0);
        }
    }

    for (UINT32 record = 0; record < NUM_EDC_ERROR_RECORDS; record++)
    {
        switch(record)
        {
        case TOTAL_EDC_ERRORS_RECORD:
            m_RecordInfo[record].name = "total";
            m_RecordInfo[record].typeMask = 0xff;
            break;
        case READ_EDC_ERRORS_RECORD:
            m_RecordInfo[record].name = "read";
            m_RecordInfo[record].typeMask = (RD_ERR_SUBP0|RD_ERR_SUBP1);
            break;
        case WRITE_EDC_ERRORS_RECORD:
            m_RecordInfo[record].name = "write";
            m_RecordInfo[record].typeMask = (WR_ERR_SUBP0|WR_ERR_SUBP1);
            break;
        case READ_REPLAY_EDC_ERRORS_RECORD:
            m_RecordInfo[record].name = "read replay";
            m_RecordInfo[record].typeMask = (RD_RPL_SUBP0|RD_RPL_SUBP1);
            break;
        case WRITE_REPLAY_EDC_ERRORS_RECORD:
            m_RecordInfo[record].name = "write replay";
            m_RecordInfo[record].typeMask = (WR_RPL_SUBP0|WR_RPL_SUBP1);
            break;
        case MAX_REPLAY_RETRY_EDC_RECORD:
            m_RecordInfo[record].name = "max replay retries";
            break;
        case MAX_REPLAY_CYCLES_SUBP0_EDC_RECORD:
            m_RecordInfo[record].name = "SUBP0 Max Replay Cycles";
            break;
        case MAX_REPLAY_CYCLES_SUBP1_EDC_RECORD:
            m_RecordInfo[record].name = "SUBP1 Max Replay Cycles";
            break;
        case MAX_REPLAY_RETRY_SUBP0_EDC_RECORD:
            m_RecordInfo[record].name = "SUBP0 Max Replay Retries";
            break;
        case MAX_REPLAY_RETRY_SUBP1_EDC_RECORD:
            m_RecordInfo[record].name = "SUBP1 Max Replay Retries";
            break;
        case MAX_DELTA_EDC_RECORD:
            m_RecordInfo[record].name = "Max Delta";
            break;
        case CRC_TICK_EDC_RECORD:
            m_RecordInfo[record].name = "CRC Tick";
            break;
        case CRC_TICK_CLOCK_EDC_RECORD:
            m_RecordInfo[record].name = "CRC Clock";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 0:
            m_RecordInfo[record].name = "Byte 0 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 1:
            m_RecordInfo[record].name = "Byte 1 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 2:
            m_RecordInfo[record].name = "Byte 2 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 3:
            m_RecordInfo[record].name = "Byte 3 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 4:
            m_RecordInfo[record].name = "Byte 4 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 5:
            m_RecordInfo[record].name = "Byte 5 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 6:
            m_RecordInfo[record].name = "Byte 6 (Rd)";
            break;
        case BYTES_READ_EDC_ERROR_RECORD_BASE + 7:
            m_RecordInfo[record].name = "Byte 7 (Rd)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 0:
            m_RecordInfo[record].name = "Byte 0 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 1:
            m_RecordInfo[record].name = "Byte 1 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 2:
            m_RecordInfo[record].name = "Byte 2 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 3:
            m_RecordInfo[record].name = "Byte 3 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 4:
            m_RecordInfo[record].name = "Byte 4 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 5:
            m_RecordInfo[record].name = "Byte 5 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 6:
            m_RecordInfo[record].name = "Byte 6 (Wr)";
            break;
        case BYTES_WRITE_EDC_ERROR_RECORD_BASE + 7:
            m_RecordInfo[record].name = "Byte 7 (Wr)";
            break;
        default:
            MASSERT(!"Should not reach here");
            m_RecordInfo[record].name = "";
            m_RecordInfo[record].typeMask = 0xff;
        }
    }
    GetGpuSubdevice()->ResetEdcErrors();

    vector<UINT32> epmCountIndices;
    epmCountIndices.assign
    (
        m_RecordsCheckedPerMin,
        m_RecordsCheckedPerMin + NUM_EDC_ERROR_PER_MIN_CHECKED_RECORDS
    );

    // We only use the first NUM_EDC_ERROR_CHECKED_RECORDS records as a fail criteria
    // The remaining records and all records from the subpartitions are used
    // For additional debug info
    return ErrCounter::Initialize("EdcErrCount",
                                  NUM_EDC_ERROR_CHECKED_RECORDS,
                                  GetEdcMaxRecordCount() - NUM_EDC_ERROR_CHECKED_RECORDS,
                                  &epmCountIndices,
                                  MODS_TEST, EDC_PRI);
}

UINT32 EdcErrCounter::GetEdcMaxRecordCount() const
{
    const UINT32 numFbios = GetGpuSubdevice()->GetFB()->GetFbioCount();
    return (1 + numFbios) * NUM_EDC_ERROR_RECORDS;
}

UINT32 EdcErrCounter::GetEdcErrIndex(UINT32 virtualFbio) const
{
    const UINT32 errIndex = NUM_EDC_ERROR_RECORDS * (virtualFbio + 1);
    MASSERT(virtualFbio < GetGpuSubdevice()->GetFB()->GetFbioCount());
    MASSERT(errIndex < GetEdcMaxRecordCount());
    return errIndex;
}

//--------------------------------------------------------------------
//! \brief Override the base-class ReadErrorCount method.
//!
//! This method stores an error vector with
//! (NUM_EDC_ERROR_RECORDS*(numFbios+1)) elements, in which the first
//! NUM_EDC_ERROR_RECORDS elements are the totals for each of the defined
//! records, eg. overall total number of EDC errors, total number of EDC
//! write errors. The second NUM_EDC_ERROR_RECORDS elements are the error
//! records for the first partition, the third NUM_EDC_ERROR_RECORDS elements
//! are the error records for the second partition, and so on.
//! Only the first NUM_EDC_ERROR_RECORDS elements are used for error-checking;
//! the remaining elements are "hidden" fields that are used to print the
//! per-partition and per-record info when the corresponding record element
//! increments past the threshold.
//!

/* virtual */ RC EdcErrCounter::ReadErrorCount(UINT64 *pCount)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    SubdeviceFb *pSubdeviceFb = pGpuSubdevice->GetSubdeviceFb();
    const UINT32 numFbios = pGpuSubdevice->GetFB()->GetFbioCount();

    // Try to use the direct EDC counts since the other interface provides less detailed info
    if (m_UseNewEdcFormat)
    {
        bool   directSupported[NUM_EDC_ERROR_RECORDS] = {};
        UINT64 directCounts[NUM_EDC_ERROR_RECORDS] = {};
        for (UINT32 virtFbio = 0; virtFbio < numFbios; virtFbio++)
        {
            CHECK_RC(pGpuSubdevice->GetEdcDirectCounts(virtFbio, directSupported, directCounts));
            for (UINT32 record = 0; record < NUM_EDC_ERROR_RECORDS; record++)
            {
                if (directSupported[record])
                {
                    // Update total and per-subp counters. Use the m_AggregateFunc
                    // map to determine how to aggregate total counters.
                    EdcErrCounter::AggregateFunc aggrFunc =
                        GetAggregateFunc(static_cast<EdcErrCounter::EDC_ERROR_RECORDS>(record));

                    if (!aggrFunc)
                    {
                        Printf(Tee::PriError, "Function pointer should not be null!\n");
                        return RC::SOFTWARE_ERROR;
                    }

                    pCount[record] = aggrFunc(pCount[record], directCounts[record]);
                    pCount[GetEdcErrIndex(virtFbio) + record] = directCounts[record];
                }
            }
        }
    }
    // Otherwise use RM control calls and debug information to get the counts
    // The TOTAL counts should be correct, but the debug register only records whether an event
    // of a specific type oclwred, not the number of events.
    else
    {
        SubdeviceFb::GetEdcCountsParams params = {};
        if (pSubdeviceFb && (OK == pSubdeviceFb->GetEdcCounts(&params)))
        {
            pCount[TOTAL_EDC_ERRORS_RECORD] = 0;
            MASSERT(params.edcCounts.size() == numFbios);
            for (UINT32 virtFbio = 0; virtFbio < numFbios; virtFbio++)
            {
                pCount[TOTAL_EDC_ERRORS_RECORD] +=
                    params.edcCounts[virtFbio];
                pCount[GetEdcErrIndex(virtFbio) + TOTAL_EDC_ERRORS_RECORD] =
                    params.edcCounts[virtFbio];
            }
        }
        for (UINT32 virtFbio = 0; virtFbio < numFbios; virtFbio++)
        {
            if (pCount[GetEdcErrIndex(virtFbio) + TOTAL_EDC_ERRORS_RECORD] >
                m_EdcErrorRecords[virtFbio][TOTAL_EDC_ERRORS_RECORD])
            {
                m_EdcErrorRecords[virtFbio][TOTAL_EDC_ERRORS_RECORD] =
                    pCount[GetEdcErrIndex(virtFbio) + TOTAL_EDC_ERRORS_RECORD];

                UINT32 errtype = 0;
                struct GpuSubdevice::ExtEdcInfo extedcinfo;
                if (OK == pGpuSubdevice->GetEdcDebugInfo(virtFbio, &extedcinfo))
                {
                    errtype = extedcinfo.EdcErrType;
                }

                if (errtype)
                {
                    if (errtype & m_RecordInfo[READ_EDC_ERRORS_RECORD].typeMask)
                        m_EdcErrorRecords[virtFbio][READ_EDC_ERRORS_RECORD]++;

                    if (errtype & m_RecordInfo[WRITE_EDC_ERRORS_RECORD].typeMask)
                        m_EdcErrorRecords[virtFbio][WRITE_EDC_ERRORS_RECORD]++;

                    if (errtype & m_RecordInfo[READ_REPLAY_EDC_ERRORS_RECORD].typeMask)
                        m_EdcErrorRecords[virtFbio][READ_REPLAY_EDC_ERRORS_RECORD]++;

                    if (errtype & m_RecordInfo[WRITE_REPLAY_EDC_ERRORS_RECORD].typeMask)
                        m_EdcErrorRecords[virtFbio][WRITE_REPLAY_EDC_ERRORS_RECORD]++;
                }
            }
            for (UINT32 record = 0; record < NUM_EDC_ERROR_RECORDS; record++)
            {
                // The total edc errors record has already been set above
                // for each partition
                if (record == TOTAL_EDC_ERRORS_RECORD)
                {
                    continue;
                }
                pCount[record] +=
                    m_EdcErrorRecords[virtFbio][record];
                pCount[GetEdcErrIndex(virtFbio) + record] =
                    m_EdcErrorRecords[virtFbio][record];
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Override the base-class OnCheckpoint() method.
//!
/* virtual */ void EdcErrCounter::OnCheckpoint
(
    const CheckpointData *pData
) const
{
    Printf(Tee::PriNormal, "\nEdcErrCounter Checkpoint:\n");
    if (m_UseNewEdcFormat)
    {
        Printf(Tee::PriNormal, "%s",
               DumpNewEdcFormat(pData->pCount, pData->pAllowedErrors, false).c_str());
    }
    else
    {
        Printf(Tee::PriNormal, "EDC total error count: %llu\n",
               pData->pCount[TOTAL_EDC_ERRORS_RECORD]);
        Printf(Tee::PriNormal, "EDC read error count: %llu\n",
               pData->pCount[READ_EDC_ERRORS_RECORD]);
        Printf(Tee::PriNormal, "EDC write error count: %llu\n",
               pData->pCount[WRITE_EDC_ERRORS_RECORD]);
        Printf(Tee::PriNormal, "EDC read replay error count: %llu\n",
               pData->pCount[READ_REPLAY_EDC_ERRORS_RECORD]);
        Printf(Tee::PriNormal, "EDC write replay error count: %llu\n",
               pData->pCount[WRITE_REPLAY_EDC_ERRORS_RECORD]);
        Printf(Tee::PriNormal, "EDC max replay retries: %llu\n",
               pData->pCount[MAX_REPLAY_RETRY_EDC_RECORD]);
    }
}

string EdcErrCounter::DumpNewEdcFormat(const UINT64* pCount,
                                       const UINT64* pAllowedCount,
                                       const bool    isNewErrors) const
{
    MASSERT(pCount);
    const char* typeStr = isNewErrors ? "(New)" : "(Total)";

    // First print the total error counts that we can actually fail on
    // Print the EDC CRC data if available
    string errStr = "";
    for (UINT32 record = 0; record < NUM_EDC_ERROR_CHECKED_RECORDS; record++)
    {
        const string allowedStr =
            pAllowedCount ?
            Utility::StrPrintf(" (%llu Allowed)", pAllowedCount[record]) :
            "";
        errStr += Utility::StrPrintf("EDC %s errors: %llu %s%s\n",
            m_RecordInfo[record].name,
            pCount[record],
            typeStr,
            allowedStr.c_str());
    }
    errStr += Utility::StrPrintf("\n");
    // Then print per-subp error counts and debug info
    FrameBuffer *pFb = GetGpuSubdevice()->GetFB();
    for (UINT32 virtFbio = 0; virtFbio < pFb->GetFbioCount(); virtFbio++)
    {
        // Don't print counts from partitions that don't have errors
        if (pCount[GetEdcErrIndex(virtFbio) + TOTAL_EDC_ERRORS_RECORD] == 0)
        {
            continue;
        }
        errStr += Utility::StrPrintf("Partition %c:\n", pFb->VirtualFbioToLetter(virtFbio));
        for (UINT32 record = 0; record < NUM_EDC_ERROR_RECORDS; record++)
        {
            // Only print the bytes with their respective counts
            if ((record >= BYTES_READ_EDC_ERROR_RECORD_BASE &&
                 record <= BYTES_READ_EDC_ERROR_RECORD_MAX) ||
                (record >= BYTES_WRITE_EDC_ERROR_RECORD_BASE &&
                 record <= BYTES_WRITE_EDC_ERROR_RECORD_MAX))
            {
                continue;
            }
            // Handled with CRC_TICK
            else if (record == CRC_TICK_CLOCK_EDC_RECORD)
            {
                continue;
            }
            // CRC_TICK print is handled a bit differently
            else if (record == CRC_TICK_EDC_RECORD)
            {
                // Print non-count record
                errStr += Utility::StrPrintf("    %s: %llu (%s)\n",
                    m_RecordInfo[record].name,
                    pCount[GetEdcErrIndex(virtFbio) + record],
                    pCount[GetEdcErrIndex(virtFbio) + CRC_TICK_CLOCK_EDC_RECORD] ? "REFCLK" : "LTCCLK");
            }
            // Handle normal error counts
            else
            {
                // Print record
                errStr += Utility::StrPrintf("    %s: %llu %s\n",
                    m_RecordInfo[record].name,
                    pCount[GetEdcErrIndex(virtFbio) + record],
                    typeStr);

                // If there are failing reads or writes, print the per-byte info
                if (record == READ_EDC_ERRORS_RECORD)
                {
                    for (UINT32 byteRecord = BYTES_READ_EDC_ERROR_RECORD_BASE;
                         byteRecord <= BYTES_READ_EDC_ERROR_RECORD_MAX;
                         byteRecord++)
                    {
                        // Only print counts from failing bytes
                        if (pCount[GetEdcErrIndex(virtFbio) + byteRecord] > 0)
                        {
                            errStr += Utility::StrPrintf("        %s: %llu %s\n",
                                m_RecordInfo[byteRecord].name,
                                pCount[GetEdcErrIndex(virtFbio) + byteRecord],
                                typeStr);
                        }
                    }
                }
                else if (record == WRITE_EDC_ERRORS_RECORD)
                {
                    for (UINT32 byteRecord = BYTES_WRITE_EDC_ERROR_RECORD_BASE;
                         byteRecord <= BYTES_WRITE_EDC_ERROR_RECORD_MAX;
                         byteRecord++)
                    {
                        // Only print counts from failing bytes
                        if (pCount[GetEdcErrIndex(virtFbio) + byteRecord] > 0)
                        {
                            errStr += Utility::StrPrintf("        %s: %llu %s\n",
                                m_RecordInfo[byteRecord].name,
                                pCount[GetEdcErrIndex(virtFbio) + byteRecord],
                                typeStr);
                        }
                    }
                }
            }
        }
    }
    return errStr;
}

/*virtual*/ string EdcErrCounter::DumpStats()
{
    const vector<UINT64>& lwrrentCount = GetLwrrentCount();
    const vector<UINT64>& allowedCount = GetAllowedErrors();
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();

    FrameBuffer *pFb = pGpuSubdevice->GetFB();
    string errStr = "\nEDC Error Stats:\n";
    if (m_UseNewEdcFormat)
    {
        errStr += DumpNewEdcFormat(lwrrentCount.data(), allowedCount.data(), false);
    }
    else
    {
        const UINT32 numFbios = pGpuSubdevice->GetFB()->GetFbioCount();
        for (UINT32 record = 0; record < NUM_EDC_ERROR_RECORDS; record++)
        {
            errStr += Utility::StrPrintf("EDC %s errors (Allowed): %llu (%llu)\n",
                    m_RecordInfo[record].name,
                    lwrrentCount[record],
                    allowedCount[record]);
            for (UINT32 virtFbio = 0; virtFbio < numFbios; ++virtFbio)
            {
                errStr += Utility::StrPrintf("  Partition %c: %llu (%llu)\n",
                        pFb->VirtualFbioToLetter(virtFbio),
                        lwrrentCount[GetEdcErrIndex(virtFbio) + record],
                        allowedCount[GetEdcErrIndex(virtFbio) + record]);
                if (m_EdcCrcs[virtFbio].empty())
                {
                    continue;
                }
                for (UINT32 ii = 0; ii < m_EdcCrcs[virtFbio].size(); ii++)
                {
                    if(!(m_EdcCrcs[virtFbio][ii].EdcErrType & m_RecordInfo[record].typeMask))
                    {
                        continue;
                    }
                    errStr += Utility::StrPrintf("  Detailed Crc Error Data:\n");
                    errStr += Utility::StrPrintf("      Expected:%08x vs Actual:%08x\n",
                            m_EdcCrcs[virtFbio][ii].expected,
                            m_EdcCrcs[virtFbio][ii].actual);
                    errStr += Utility::StrPrintf("      Edc error type: %08x\n",
                            static_cast<UINT32>(m_EdcCrcs[virtFbio][ii].EdcErrType));
                    errStr += Utility::StrPrintf("      Edc error rate exceeded: %s\n",
                            (true == m_EdcCrcs[virtFbio][ii].EdcRateExceeded) ?
                            "true":"false");
                }
            }
        }
    }
    return errStr;
}

//--------------------------------------------------------------------
//! \brief Override the base-class OnError() method.
//!
/* virtual */ RC EdcErrCounter::OnError(const ErrorData *pData)
{
    if (!pData->bCheckExitOnly)
    {
        GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
        FrameBuffer *pFb = pGpuSubdevice->GetFB();
        const UINT32 numFbios = pGpuSubdevice->GetFB()->GetFbioCount();

        // Per-partition breakdown
        // Only print errors that we actually fail on
        //
        if (m_UseNewEdcFormat)
        {
            Printf(Tee::PriNormal, "\nEDC Errors Exceeded Per-Test Threshold:\n%s",
                   DumpNewEdcFormat(pData->pNewErrors, pData->pAllowedErrors, true).c_str());
        }
        else
        {
            for (UINT32 record = 0; record < NUM_EDC_ERROR_CHECKED_RECORDS; record++)
            {
                if (pData->pNewErrors[record] <= pData->pAllowedErrors[record])
                {
                    continue;
                }

                Printf(Tee::PriNormal,
                    "EDC %s errors exceeded threshold: errs = %llu, thresh = %llu\n",
                    m_RecordInfo[record].name,
                    pData->pNewErrors[record], pData->pAllowedErrors[record]);
                Printf(Tee::PriNormal, "Cumulative CRC Miscompare Data:\n");

                for (UINT32 virtFbio = 0; virtFbio < numFbios; virtFbio++)
                {
                    UINT32 errIndex = GetEdcErrIndex(virtFbio) + record;
                    UINT64 errorsInPartition = pData->pNewErrors[errIndex];
                    if (!errorsInPartition)
                    {
                        continue;
                    }

                    // Print per-partition record
                    Printf(Tee::PriNormal, "    Partition %c: %llu\n",
                           pFb->VirtualFbioToLetter(virtFbio),
                           errorsInPartition);

                    // Print the CRC error data if available
                    if (m_EdcCrcs[virtFbio].size() > 0)
                    {
                        Printf(Tee::PriNormal,"    Detailed Crc Error Data:\n");
                    }
                    for (UINT32 ii = 0; ii < m_EdcCrcs[virtFbio].size(); ii++)
                    {
                        if (!(m_EdcCrcs[virtFbio][ii].EdcErrType & m_RecordInfo[record].typeMask))
                        {
                            continue;
                        }
                        Printf(Tee::PriNormal, "        Expected:%08x vs Actual:%08x\n",
                            m_EdcCrcs[virtFbio][ii].expected,
                            m_EdcCrcs[virtFbio][ii].actual);
                        Printf(Tee::PriNormal, "        Edc error type: %08x\n",
                        static_cast<UINT32>(m_EdcCrcs[virtFbio][ii].EdcErrType));
                        Printf(Tee::PriNormal, "        Edc error rate exceeded: %s\n",
                        m_EdcCrcs[virtFbio][ii].EdcRateExceeded ? "true" : "false");
                    }
                }
            }
        }
    }

    return RC::EDC_ERROR;
}

//--------------------------------------------------------------------
//! \brief Override the base-class OnCountChange() method.
//!
/* virtual */ void EdcErrCounter::OnCountChange(const CountChangeData *pData)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    FrameBuffer *pFb = pGpuSubdevice->GetFB();
    Tee::Priority pri = pData->Verbose ? Tee::PriNormal : Tee::PriLow;

    // New errors have been detected, get the crc miscompare data.
    if (m_UseNewEdcFormat)
    {
        const UINT32 numRecords = GetEdcMaxRecordCount();
        std::vector<UINT64> newErrors(numRecords);
        for (UINT32 i = 0; i < numRecords; i++)
        {
            newErrors[i] = pData->pNewCount[i] - pData->pOldCount[i];
        }
        Printf(Tee::PriNormal, "\nNew EDC errors have oclwred:\n%s",
               DumpNewEdcFormat(newErrors.data(), nullptr, true).c_str());
    }
    else
    {
        // Only print new EDC errors on records we actually can fail on
        for (UINT32 ii = 0; ii < NUM_EDC_ERROR_CHECKED_RECORDS; ii++)
        {
            UINT64 newCount = pData->pNewCount[ii];
            UINT64 oldCount = pData->pOldCount[ii];
            if (newCount > oldCount)
            {
                Printf(pri, "New %s EDC errors have oclwred: %llu (%llu new)\n",
                       m_RecordInfo[ii].name, newCount, newCount - oldCount);
            }
        }
        if (pData->pNewCount[TOTAL_EDC_ERRORS_RECORD] > pData->pOldCount[TOTAL_EDC_ERRORS_RECORD])
        {
            SubdeviceFb *pSubdeviceFb = pGpuSubdevice->GetSubdeviceFb();
            SubdeviceFb::GetEdcCrcDataParams crcParams;
            if (pSubdeviceFb && (OK == pSubdeviceFb->GetEdcCrcData(&crcParams)))
            {
                const UINT32 numFbios = pGpuSubdevice->GetFB()->GetFbioCount();
                MASSERT(crcParams.crcData.size() == numFbios);
                for (UINT32 virtualFbio = 0; virtualFbio < numFbios; ++virtualFbio)
                {
                    UINT32 errIndex = GetEdcErrIndex(virtualFbio);
                    if (pData->pNewCount[errIndex + TOTAL_EDC_ERRORS_RECORD] -
                        pData->pOldCount[errIndex + TOTAL_EDC_ERRORS_RECORD] > 0)
                    {
                        EdcCrcData data = { crcParams.crcData[virtualFbio].expected,
                                            crcParams.crcData[virtualFbio].actual,
                                            0,0};
                        /*Read Additional EDC Data for GK10x*/
                        /*Update EdcErrData*/
                        UINT32 errtype = 0;
                        struct GpuSubdevice::ExtEdcInfo extedcinfo;
                        if (OK == pGpuSubdevice->GetEdcDebugInfo(virtualFbio, &extedcinfo))
                        {
                            errtype = extedcinfo.EdcErrType;
                            data.EdcErrType = errtype;
                            data.EdcRateExceeded = extedcinfo.EdcRateExceeded;
                            data.EdcTrgByteNum = extedcinfo.EdcTrgByteNum;
                        }

                        if (errtype)
                        {
                            Printf(pri, "In partition %c\n",
                                    pFb->VirtualFbioToLetter(virtualFbio));
                            if (errtype & (RD_ERR_SUBP1|RD_ERR_SUBP0))
                            {
                                Printf(pri, "\tEdc Read error in Subpart:\t");
                                if (errtype & RD_ERR_SUBP0)
                                    Printf(pri, "0  ");
                                if (errtype & RD_ERR_SUBP1)
                                    Printf(pri, "1");
                                Printf(pri, "\n");
                            }
                            if (errtype & (WR_ERR_SUBP1|WR_ERR_SUBP0))
                            {
                                Printf(pri, "\tEdc Write error in Subpart:\t");
                                if (errtype & WR_ERR_SUBP0)
                                    Printf(pri, "0  ");
                                if (errtype & WR_ERR_SUBP1)
                                    Printf(pri, "1");
                                Printf(pri, "\n");
                            }
                            if (errtype & (RD_RPL_SUBP1|RD_RPL_SUBP0))
                            {
                                Printf(pri, "\tEdc Read Replay error in Subpart:\t");
                                if (errtype & RD_RPL_SUBP0)
                                    Printf(pri, "0  ");
                                if (errtype & RD_RPL_SUBP1)
                                    Printf(pri, "1");
                                Printf(pri, "\n");
                            }
                            if (errtype & (WR_RPL_SUBP1|WR_RPL_SUBP0))
                            {
                                Printf(pri, "\tEdc Write Replay error in Subpart:\t");
                                if (errtype & WR_RPL_SUBP0)
                                    Printf(pri, "0  ");
                                if (errtype & WR_RPL_SUBP1)
                                    Printf(pri, "1");
                                Printf(pri, "\n");
                            }
                        }

                        if (true == data.EdcRateExceeded)
                            Printf(pri, "Partition: %c, Exceeded error rate\n",
                                    pFb->VirtualFbioToLetter(virtualFbio));
                        m_EdcCrcs[virtualFbio].push_back(data);
                        Printf(pri, "Partition: %c, Error Trigger Byte Number %u\n",
                                pFb->VirtualFbioToLetter(virtualFbio),
                                data.EdcTrgByteNum);
                        Printf(pri,"EDC miscompare: partition %c: "
                            "Expected:%08x vs Actual:%08x\n",
                               pFb->VirtualFbioToLetter(virtualFbio),
                               crcParams.crcData[virtualFbio].expected,
                               crcParams.crcData[virtualFbio].actual);
                        pGpuSubdevice->ClearEdcDebug(virtualFbio);
                    }
                }
                Printf(pri, "\n");
            }
        }
    }
}

//--------------------------------------------------------------------
//! \brief Do some stuff before entering each MODS test
//!
RC EdcErrCounter::RunBeforeEnterTest()
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numFbios = pGpuSubdevice->GetFB()->GetFbioCount();
    // If "-clear_edc_max_delta" is used in cmd line, clear the Max Delta register
    if (m_ClearEdcMaxDelta)
    {
        for (UINT32 virtFbio = 0; virtFbio < numFbios; virtFbio++)
        {
            CHECK_RC(pGpuSubdevice->ClearEdcMaxDelta(virtFbio));
        }
    }
    return ErrCounter::RunBeforeEnterTest();
}

//--------------------------------------------------------------------
//! \brief Pass some EDC related variables from global JS to this class
//!
void EdcErrCounter::InitFromJs()
{
    // Override the flag which indicates whether we need to
    // clear the edc max delta register
    JSObject *pGlobalObject = JavaScriptPtr()->GetGlobalObject();
    OverrideFromJs(pGlobalObject, "g_ClearEdcMaxDelta", &m_ClearEdcMaxDelta);
    OverrideFromJs(pGlobalObject, "g_EdcOverrideDefaultTol", &m_EdcOverrideDefaultTol);

    // Call the InitFromJs function of Base class to pass other JS variables
    ErrCounter::InitFromJs();
}

//--------------------------------------------------------------------
//! In ReadErrorCount, we want to aggregate error records differently
//! based on the record type.
//! e.g.
//!     For READ_EDC_ERRORS_RECORD, we want to take the sum of read
//!     edc errors from each FBIO.
//!
//!     For MAX_REPLAY_RETRY_EDC_RECORD, we want to take the max of
//!     total replay retries from each FBIO.
//!
/* static */ EdcErrCounter::AggregateFunc EdcErrCounter::GetAggregateFunc
(
    EDC_ERROR_RECORDS record
)
{
    if (record >= BYTES_READ_EDC_ERROR_RECORD_BASE &&
        record <= BYTES_READ_EDC_ERROR_RECORD_MAX)
    {
        return AggregateSum;
    }
    if (record >= BYTES_WRITE_EDC_ERROR_RECORD_BASE &&
        record <= BYTES_WRITE_EDC_ERROR_RECORD_MAX)
    {
        return AggregateSum;
    }
    switch (record)
    {
        case TOTAL_EDC_ERRORS_RECORD:
        case READ_EDC_ERRORS_RECORD:
        case WRITE_EDC_ERRORS_RECORD:
        case READ_REPLAY_EDC_ERRORS_RECORD:
        case WRITE_REPLAY_EDC_ERRORS_RECORD:
            return AggregateSum;
        case MAX_REPLAY_RETRY_EDC_RECORD:
        case MAX_REPLAY_CYCLES_SUBP0_EDC_RECORD:
        case MAX_REPLAY_CYCLES_SUBP1_EDC_RECORD:
        case MAX_REPLAY_RETRY_SUBP0_EDC_RECORD:
        case MAX_REPLAY_RETRY_SUBP1_EDC_RECORD:
        case MAX_DELTA_EDC_RECORD:
            return AggregateMax;
        case CRC_TICK_EDC_RECORD:
        case CRC_TICK_CLOCK_EDC_RECORD:
            return AggregateIdentity;
        default:
            MASSERT(!"Invalid EDC Error Record!\n");
            return nullptr;
    }
}

/* virtual */ void EdcErrCounter::ReportLwrrEPM
(
    PerMinErrorData *pErrData,
    UINT64 elapsedTimeMS,
    bool tolExceeded
)
{
    Printf(Tee::PriNormal, "\nEDC Errors Per Minute:\n");
    Printf(Tee::PriNormal, "    Elapsed Time: %llums\n", elapsedTimeMS);
    for (UINT32 i = 0; i < NUM_EDC_ERROR_PER_MIN_CHECKED_RECORDS; ++i)
    {
        UINT32 recordIdx = m_RecordsCheckedPerMin[i];
        Printf
        (
            Tee::PriNormal,
            "    Current %s errors per minute: %llu (Allowed errors per minute: %llu)\n",
            m_RecordInfo[recordIdx].name,
            pErrData->pLwrrEPM[i],
            pErrData->pAllowedEPM[i]
        );
    }
    Printf(Tee::PriNormal, "\n");
}

/* virtual */ void EdcErrCounter::OverwriteAllowedErrors
(
    UINT64 *pAllowedErrors,
    size_t bufSize
)
{
    // Ensure MAX_REPLAY_RETRY_EDC_RECORD is a valid index into the
    // allowed errors buffer
    MASSERT(bufSize > MAX_REPLAY_RETRY_EDC_RECORD);

    // If either -edc_replay_retry_tol or -edc_per_min_tol_pertest was used, set
    // all other tolerance values to UINT64_MAX to effectively ignore those
    // error counts.
    if (m_EdcOverrideDefaultTol)
    {
        UINT64 tmpVal = pAllowedErrors[MAX_REPLAY_RETRY_EDC_RECORD];
        fill_n(pAllowedErrors, bufSize, UINT64_MAX);
        pAllowedErrors[MAX_REPLAY_RETRY_EDC_RECORD] = tmpVal;
    }
    // Otherwise, ignore max replay retry tol
    else
    {
        pAllowedErrors[MAX_REPLAY_RETRY_EDC_RECORD] = UINT64_MAX;
    }
}

/* virtual */ void EdcErrCounter::OverwriteAllowedEPM
(
    UINT64 *pAllowedEPM,
    size_t bufSize
)
{
    // If neither -edc_replay_retry_tol nor -edc_per_min_tol_pertest was used,
    // ignore EPM
    if (!m_EdcOverrideDefaultTol)
    {
        fill_n(pAllowedEPM, bufSize, UINT64_MAX);
    }
}
