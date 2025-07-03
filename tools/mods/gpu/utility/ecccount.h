/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpuerrcount.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/eclwtil.h"
#include "subdevfb.h"
#include "subdevgr.h"
#include "gpu/include/falconecc.h"

//--------------------------------------------------------------------
//! \brief Class to keep track of the ECC error counts on a subdevice.
//!
//! Each GPU keeps count of the number of ECC errors that have
//! oclwrred in the various memory modules.  This class is designed to
//! read those counters, and cause a mods error when an unexpected ECC
//! error happens.  (The mechanics of causing a mods error are handled
//! by the ErrCounterMonitor base class.)
//!
//! Since some tests are designed to deliberately inject ECC errors,
//! this class also provides a means for ECC-injecting tests to adjust
//! the "expected error count".  The error counts reported to the base
//! class are the "unexpected error count" (i.e. total_error_count
//! minus expected_error_count).
//!
class EccErrCounter : public GpuErrCounter
{
public:
    enum [[deprecated("Index by (Ecc::Unit, Ecc::ErrorType)")]]
    Index
    {
        FB_CORR,                //!< Correctable ECC errors in Framebuffer memory
        FB_UNCORR,              //!< Uncorrectable ECC errors in Framebuffer memory
        L2_CORR,                //!< Correctable ECC errors in L2 cache
        L2_UNCORR,              //!< Uncorrectable ECC errors in L2 cache
        L1_CORR,                //!< Correctable ECC errors in L1 cache
        L1_UNCORR,              //!< Uncorrectable ECC errors in L1 cache
        SM_CORR,                //!< Correctable ECC errors in SM register file
        SM_UNCORR,              //!< Uncorrectable ECC errors in SM register file
        SHM_CORR,               //!< Correctable ECC errors in SHM
        SHM_UNCORR,             //!< Uncorrectable ECC errors in SHM
        TEX_CORR,               //!< Correctable Ecc errors in Tex
        TEX_UNCORR,             //!< Uncorrectable ECC errors in Tex
        L1DATA_CORR,            //!< Correctable ECC errors in L1 data
        L1DATA_UNCORR,          //!< Uncorrectable ECC errors in L1 data
        L1TAG_CORR,             //!< Correctable ECC errors in L1 tag
        L1TAG_UNCORR,           //!< Uncorrectable ECC errors in L1 tag
        CBU_CORR,               //!< Correctable CBU errors
        CBU_UNCORR,             //!< Uncorrectable CBU errors
        NUM_ECC_ERRS,
        ILWALID_INDEX
    };
    static RC UnitErrFromEccErrCounterIndex
    (
        EccErrCounter::Index idx
        ,Ecc::Unit* pEclwnit
        ,Ecc::ErrorType* pErrType
    );

    //! The total number of unique error types.
    static constexpr UINT32 s_NumEccErrTypes = static_cast<UINT32>(Ecc::Unit::MAX)
        * static_cast<UINT32>(Ecc::ErrorType::MAX);

    static UINT32 UnitErrCountIndex(Ecc::Unit eclwnit, Ecc::ErrorType errType);
    static RC UnitErrFromErrCountIndex
    (
        UINT32 countIndex
        ,Ecc::Unit* pEclwnit
        ,Ecc::ErrorType* pErrType
    );

    EccErrCounter(GpuSubdevice *pGpuSubdevice);
    virtual ~EccErrCounter();
    RC Initialize();
    RC Update();

    static UINT32 GetNumEccErrTypes();
    UINT64 GetTotalErrors(Ecc::Unit eclwnit, Ecc::ErrorType errType) const;
    RC GetUnexpectedErrors(vector<UINT64>* pCounts);
    RC BeginExpectingErrors(Ecc::Unit eclwnit, Ecc::ErrorType errType);
    RC EndExpectingErrors(Ecc::Unit eclwnit, Ecc::ErrorType errType);
    RC EndExpectingAllErrors();
    RC WaitForScrub(FLOAT64 TimeoutMs);
    virtual string DumpStats();
    RC StartNotifiers();
    RC StopNotifiers();
    bool IsUsingNotifiers() { return m_UpdateMode == CounterUpdateMode::Notifiers; }

    [[deprecated("Index by (Ecc::Unit, Ecc::ErrorType)")]]
    UINT64 GetTotalErrors(Index idx) const;
    [[deprecated("Index by (Ecc::Unit, Ecc::ErrorType)")]]
    RC BeginExpectingErrors(Index idx);
    [[deprecated("Index by (Ecc::Unit, Ecc::ErrorType)")]]
    RC EndExpectingErrors(Index idx);

protected:
    virtual RC ReadErrorCount(UINT64 *pCount);
    virtual void OnCheckpoint(const CheckpointData *pData) const;
    virtual RC OnError(const ErrorData *pData);
    virtual void OnCountChange(const CountChangeData *pData);

private:
    Ecc::UnitList<bool> m_EnabledEclwnits; //! Enabled ECC units indexed by Ecc::Unit

    vector<UINT64> m_TotalErrors;       //!< Total ECC errors
    vector<UINT64> m_ExpectedErrors;    //!< ECC errors we expected to get
                                        //!< e.g. from ECC tests. Ignore.
    vector<UINT64> m_IsExpectingErrors; //!< If nonzero, we expect errors from
                                        //!< that ECC source right now
    bool m_BlacklistPagesOnError;       //!< Used to blacklist FB pages
    bool m_DumpPagesOnError;            //!< Used to dump failing FB pages
    bool m_LogAsMemError;               //!< If the failing bit is known, log as a MemError
    bool m_RowRemapOnError;             //!< If true, row remap failing rows.

    enum class CounterUpdateMode
    {
        DirectRMCalls,  // Retrieve all error counts by making RM calls for every ECC unit.
        Notifiers       // Retrieve error counts asynchronously using notifier callbacks.
    };

    CounterUpdateMode m_UpdateMode;

    // Queue of FB addresses that got an ECC error.  The queue is
    // processed & cleared on each OnCountChange().
    //
    typedef SubdeviceFb::GetLatestEccAddressesParams::Address FbAddressType;
    vector<FbAddressType> m_FbAddresses;
    UINT32 m_TotalFbAddressCount;

    struct SubdevDetailedEccCounts
    {
        Ecc::DetailedErrCounts fbCounts;
        Ecc::DetailedErrCounts l2Counts;
        Ecc::DetailedErrCounts l1Counts;
        Ecc::DetailedErrCounts l1DataCounts;
        Ecc::DetailedErrCounts l1TagCounts;
        Ecc::DetailedErrCounts cbuCounts;
        Ecc::DetailedErrCounts smCounts;
        Ecc::DetailedErrCounts shmCounts;
        Ecc::DetailedErrCounts texCounts;
        Ecc::DetailedErrCounts smIcacheCounts;
        Ecc::DetailedErrCounts gccL15Counts;
        Ecc::DetailedErrCounts gpcMmuCounts;
        Ecc::DetailedErrCounts hubMmuL2TlbCounts;
        Ecc::DetailedErrCounts hubMmuHubTlbCounts;
        Ecc::DetailedErrCounts hubMmuFillUnitCounts;
        Ecc::DetailedErrCounts gpccsCounts;
        Ecc::DetailedErrCounts fecsCounts;
        Ecc::DetailedErrCounts smRamsCounts;
        Ecc::DetailedErrCounts hshubCounts;
        Ecc::ErrCounts         pmuCounts;
        Ecc::ErrCounts         pciBusReorderCounts;
        Ecc::ErrCounts         pciBusP2PCounts;

        void Clear();
    };

    static void HandleNotifierEvent(void* pArg, Ecc::ErrorType type);
    static void HandleCorrectableNotifierEvent(void* pArg)
    {
        HandleNotifierEvent(pArg, Ecc::ErrorType::CORR);
    }
    static void HandleUncorrectableNotifierEvent(void* pArg)
    {
        HandleNotifierEvent(pArg, Ecc::ErrorType::UNCORR);
    }

    SubdevDetailedEccCounts m_NotifierEccCounts;

    RC GetAllErrorCountsFromRM
    (
        GpuSubdevice* pGpuSubdevice,
        SubdevDetailedEccCounts& subdevDetailedEccCounts
    );
    RC GetErrorCountFromRM
    (
        GpuSubdevice* pGpuSubdevice,
        Ecc::Unit eclwnit,
        SubdevDetailedEccCounts& subdevDetailedEccCounts
    );
    Ecc::DetailedErrCounts * GetEclwnitCounts
    (
        SubdevDetailedEccCounts* pSubdevDetailedEccCounts
        ,Ecc::Unit eclwnit
    ) const;
    Ecc::ErrCounts * GetEccSingleUnitCounts
    (
        SubdevDetailedEccCounts* pSubdevDetailedEccCounts
        ,Ecc::Unit eclwnit
    ) const;

    UINT64 ExtractErrCount
    (
        const Ecc::ErrCounts* pErrCounts
        ,Ecc::ErrorType errType
    ) const;

    RC SetExpectingErrors
    (
        Ecc::Unit eclwnit
        ,Ecc::ErrorType errType
        ,bool isExpectingErrs
    );

    void PackEclwnitLocation
    (
        Ecc::Unit eclwnit
        ,const Ecc::ErrCounts& counts
        ,UINT64* pArray
        ,UINT32* pIndex
    ) const;
    RC PackArray
    (
        const SubdevDetailedEccCounts& subdevDetailedEccCounts
        ,UINT64 *pArray
    ) const;
    RC UnpackArray
    (
        SubdevDetailedEccCounts* pSubdevDetailedEccCounts
        ,const UINT64 *pArray
    ) const;
    UINT32 GetPackedArraySize() const;

    void GetLocation
    (
        Ecc::Unit eclwnit
        ,UINT32 i
        ,UINT32 j
        ,string* pLocStr
        ,Ecc::UnitType* pUnitType
    ) const;
    void PrintNewErrCountsToMle
    (
        const char* location,
        const char* type,
        UINT08 partGpc,
        UINT08 subpTpc,
        UINT64 count,
        Ecc::UnitType loc,
        Ecc::Unit eclwnit,
        Ecc::ErrorType errType
    ) const;
    void ReportError
    (
        Ecc::Unit      eclwnit,
        Ecc::ErrorType errType,
        UINT08         partGpc,
        UINT08         subpTpc,
        const Ecc::ErrCounts* pErrCounts
    ) const;

    void ReportNewError
    (
        Tee::Priority pri,
        Ecc::Unit eclwnit,
        Ecc::ErrorType errType,
        const Ecc::ErrCounts* pErrCounts
    ) const {  return ReportNewError(pri, eclwnit, errType, 0, 0, pErrCounts); }
    void ReportNewError
    (
        Tee::Priority  pri,
        Ecc::Unit      eclwnit,
        Ecc::ErrorType errType,
        UINT08         partGpc,
        UINT08         subpTpc,
        const Ecc::ErrCounts* pErrCounts
    ) const;
    bool ReportCheckpointError
    (
        Ecc::Unit      eclwnit,
        Ecc::ErrorType errType,
        const Ecc::ErrCounts* pErrCounts
    ) const { return ReportCheckpointError(eclwnit, errType, 0, 0, pErrCounts); }
    bool ReportCheckpointError
    (
        Ecc::Unit      eclwnit,
        Ecc::ErrorType errType,
        UINT08         partGpc,
        UINT08         subpTpc,
        const Ecc::ErrCounts* pErrCounts
    ) const;
};
