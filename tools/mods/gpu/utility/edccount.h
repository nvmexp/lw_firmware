/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2013-2016,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_EDCCOUNT_H
#define INCLUDED_EDCCOUNT_H

#ifndef INCLUDED_GPUERRCOUNT_H
#include "gpuerrcount.h"
#endif

#ifndef INCLUDED_GPUSBDEV_H
#include "gpu/include/gpusbdev.h"
#endif

#include <functional>
#include <map>
#include <vector>

//--------------------------------------------------------------------
//! \brief Class to keep track of the EDC error counts on a subdevice.
//!
//! The gddr5 memory has an EDC feature that tests whether the data
//! was sent correctly from the GPU to memory, and automatically
//! corrects errors.  (The difference between ECC and EDC is that ECC
//! corrects corruption that oclwrs in the chip, whereas EDC corrects
//! corruption that oclwrs on the bus.)
//!
//! A count of EDC errors is kept in the GPU.  This class is designed
//! to read the counter, and cause a mods error when an unexpected EDC
//! error happens.  The mechanics of causing a mods error are handled
//! by the ErrCounterMonitor base class.
//!
class EdcErrCounter : public GpuErrCounter
{
public:
    EdcErrCounter(GpuSubdevice *pSubdev) : GpuErrCounter(pSubdev) {}
    RC Initialize();

    // Returns the total number of EDC records (including subpartition records)
    UINT32 GetEdcMaxRecordCount() const;

    // GetEdcErrIndex(p) + *_EDC_ERR_RECORD yields an index into the array
    // filled by ReadErrorCount()
    UINT32 GetEdcErrIndex(UINT32 virtualFbio) const;

    // Order matters here, since only the records from [0, NUM_EDC_ERROR_CHECKED_RECORDS)
    // are used as a fail criteria, the rest are for providing additional failure info
    static constexpr UINT32 NUM_EDC_ERROR_CHECKED_RECORDS = 6;
    enum EDC_ERROR_RECORDS : UINT32
    {
        TOTAL_EDC_ERRORS_RECORD,
        READ_EDC_ERRORS_RECORD,
        WRITE_EDC_ERRORS_RECORD,
        READ_REPLAY_EDC_ERRORS_RECORD,
        WRITE_REPLAY_EDC_ERRORS_RECORD,
        MAX_REPLAY_RETRY_EDC_RECORD,
        // Records below this line are not used as a fail condition
        MAX_REPLAY_CYCLES_SUBP0_EDC_RECORD = NUM_EDC_ERROR_CHECKED_RECORDS,
        MAX_REPLAY_CYCLES_SUBP1_EDC_RECORD,
        MAX_REPLAY_RETRY_SUBP0_EDC_RECORD,
        MAX_REPLAY_RETRY_SUBP1_EDC_RECORD,
        MAX_DELTA_EDC_RECORD,
        CRC_TICK_EDC_RECORD,
        CRC_TICK_CLOCK_EDC_RECORD,
        BYTES_READ_EDC_ERROR_RECORD_BASE,
        BYTES_READ_EDC_ERROR_RECORD_MAX = BYTES_READ_EDC_ERROR_RECORD_BASE + 7,
        BYTES_WRITE_EDC_ERROR_RECORD_BASE,
        BYTES_WRITE_EDC_ERROR_RECORD_MAX = BYTES_WRITE_EDC_ERROR_RECORD_BASE + 7,
        NUM_EDC_ERROR_RECORDS,
    };
    virtual string DumpStats();

protected:
    virtual RC ReadErrorCount(UINT64 *pCount);
    virtual void OnCheckpoint(const CheckpointData *pData) const;
    virtual RC OnError(const ErrorData *pData);
    virtual void OnCountChange(const CountChangeData *pData);
    virtual RC RunBeforeEnterTest();
    virtual void InitFromJs();
private:
    static UINT64 AggregateIdentity(UINT64 a, UINT64 b) { return b; }
    static UINT64 AggregateSum(UINT64 a, UINT64 b) { return (a + b); }
    static UINT64 AggregateMax(UINT64 a, UINT64 b) { return max(a, b); }

    using AggregateFunc = UINT64 (*)(UINT64, UINT64);
    static AggregateFunc GetAggregateFunc(EDC_ERROR_RECORDS);

    virtual void ReportLwrrEPM(PerMinErrorData *pErrData,
                               UINT64 elapsedTimeMS,
                               bool tolExceeded);
    virtual void OverwriteAllowedErrors(UINT64 *pAllowedErrors, size_t bufSize);
    virtual void OverwriteAllowedEPM(UINT64 *pAllowedEPM, size_t bufSize);

    string DumpNewEdcFormat(const UINT64* pCount, const UINT64* pAllowedCount, const bool isNewErrors) const;
    struct EdcCrcData {
        UINT32 expected;
        UINT32 actual;
        /*Struct to record extended data for GK10x*/
        UINT08 EdcErrType;     //This is a mask representing EDC error for FBPartition
        UINT32 EdcTrgByteNum;  //Lowest byte num that had an EDC error
        bool EdcRateExceeded;  //True if Edc Rate is measured to be greater than tolerance
    };
    enum EDC_ERROR_TYPE_MASKS
    {
        NO_ERR = 0,
        RD_ERR_SUBP0 = (1<<0),
        WR_ERR_SUBP0 = (1<<1),
        RD_RPL_SUBP0 = (1<<2),
        WR_RPL_SUBP0 = (1<<3),
        RD_ERR_SUBP1 = (1<<4),
        WR_ERR_SUBP1 = (1<<5),
        RD_RPL_SUBP1 = (1<<6),
        WR_RPL_SUBP1 = (1<<7)
    };
    struct EdcRecordInfo {
        const char* name;
        UINT08 typeMask;
    };
    EdcRecordInfo m_RecordInfo[NUM_EDC_ERROR_RECORDS] = {};
    bool m_UseNewEdcFormat = false;

    // The outer-index = NumFbios (determined at Initialize())
    // The inner-index = Number of logged miscompares. This will be <= number
    // of CrcErrors.
    vector< vector<EdcCrcData> > m_EdcCrcs;

    // The outer-index = NumFbios (determined at Initialize())
    // The inner-index = NUM_EDC_ERROR_RECORDS
    vector< vector<UINT64> > m_EdcErrorRecords;

    // Flag to indicate whether we need to clear the edc max delta
    // register between tests. This would be set by global JS argument:
    // g_ClearEdcMaxDelta, when -clear_edc_max_delta is used in cmd line
    bool m_ClearEdcMaxDelta = false;

    static constexpr UINT32 NUM_EDC_ERROR_PER_MIN_CHECKED_RECORDS = 1;
    EDC_ERROR_RECORDS m_RecordsCheckedPerMin[NUM_EDC_ERROR_PER_MIN_CHECKED_RECORDS] =
    {
        TOTAL_EDC_ERRORS_RECORD
    };

    // When one or any combination of these EDC tolerance args are set
    //  -edc_replay_retry_tol,
    //  -edc_per_min_tol_pertest,
    //  -testarg <TestNum> EdcTotalPerMinTol <val>,
    // ignore all other error count threshholds.
    //
    // This feature was requested by MED
    bool m_EdcOverrideDefaultTol = false;
};

#endif // INCLUDED_EDCCOUNT_H
