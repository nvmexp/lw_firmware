/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "inc/bytestream.h"
#include "core/include/jscript.h"
#include "core/include/memerror.h"
#include "core/include/memtypes.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/hshubecc.h"
#include "gpu/include/pcibusecc.h"
#include "gpu/utility/ecccount.h"
#include "gpu/utility/tpceccerror.h"

/*
 * Want to only include active ECC units in ECC error counter. This is not
 * possible with the current design because:
 * - 1D array stores all the counts (requirement from ErrCount).
 * - Checking of what units are enabled is done post-GPU init. Meaning of
 *   indices into the counts array would be dependent on ECC unit enablement.
 * - Allowed ECC errors are set in JS in a similar 1D array.
 * - ecc_corr/uncorr_tol set the values in the allowed array during their arg
 *   handlers (pre-GPU init).
 * - InitEccJSDataStructure is called in HandleGpuPreInitArgs that copies that
 *   registers the value of the allowed ECC errors.
 * - The values are picked up by EccErrCounter during ErrCounter::DoStackFrame.
 * Therefore knowing where the allowed values should be set must be known before
 * GPU init => impossible.
 *
 * This implementation allocates the first N locations for the total counts for
 * each type of error that can occur. This region will contain an entry for all
 * possible error types, including those that are possible on unupported or
 * disabled ECC units.
 *
 * \see EccErrCounter::s_NumEccErrTypes
 * \see PackArray
 * \see UnpackArray
 */

//! EccErrCounter JS class
JS_CLASS(EccErrCounter);
static SObject EccErrCounter_Object
(
    "EccErrCounter",
    EccErrCounterClass,
    0,
    0,
    "EccErrCounter JS Object"
);
PROP_DESC(EccErrCounter, BlacklistPagesOnError, false,
          "Blacklist FB pages on ECC error");
PROP_DESC(EccErrCounter, DumpPagesOnError, false,
          "Dump failing FB pages on ECC error");
PROP_DESC(EccErrCounter, LogAsMemError, true,
          "If the failing bit is known, log as a MemError for all running tests");
PROP_DESC(EccErrCounter, RowRemapOnError, false,
          "Remap rows on error");

UINT32 EccErrCounter::UnitErrCountIndex
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errType
)
{
    return (static_cast<UINT32>(eclwnit) * static_cast<UINT32>(Ecc::ErrorType::MAX))
        + static_cast<UINT32>(errType);
}

//--------------------------------------------------------------------
//!
//! \brief Colwerts from an index in the total counts portion of the count array
//! to an ECC unit and error type.
//!
RC EccErrCounter::UnitErrFromErrCountIndex
(
    UINT32 countIndex
    ,Ecc::Unit* pEclwnit
    ,Ecc::ErrorType* pErrType
)
{
    MASSERT(countIndex < EccErrCounter::EccErrCounter::s_NumEccErrTypes);

    *pEclwnit = static_cast<Ecc::Unit>(countIndex / static_cast<UINT32>(Ecc::ErrorType::MAX));
    *pErrType = static_cast<Ecc::ErrorType>(countIndex % static_cast<UINT32>(Ecc::ErrorType::MAX));
    MASSERT(*pEclwnit != Ecc::Unit::MAX);
    MASSERT(*pErrType != Ecc::ErrorType::MAX);

    return RC::OK;
}

//--------------------------------------------------------------------
namespace
{
    static const std::map<EccErrCounter::Index, std::pair<Ecc::Unit, Ecc::ErrorType>> s_FromEccErrCounterIndexMap =
    {
        { EccErrCounter::FB_CORR,       std::make_pair(Ecc::Unit::DRAM,   Ecc::ErrorType::CORR)   },
        { EccErrCounter::FB_UNCORR,     std::make_pair(Ecc::Unit::DRAM,   Ecc::ErrorType::UNCORR) },
        { EccErrCounter::L2_CORR,       std::make_pair(Ecc::Unit::L2,     Ecc::ErrorType::CORR)   },
        { EccErrCounter::L2_UNCORR,     std::make_pair(Ecc::Unit::L2,     Ecc::ErrorType::UNCORR) },
        { EccErrCounter::L1_CORR,       std::make_pair(Ecc::Unit::L1,     Ecc::ErrorType::CORR)   },
        { EccErrCounter::L1_UNCORR,     std::make_pair(Ecc::Unit::L1,     Ecc::ErrorType::UNCORR) },
        { EccErrCounter::SM_CORR,       std::make_pair(Ecc::Unit::LRF,    Ecc::ErrorType::CORR)   },
        { EccErrCounter::SM_UNCORR,     std::make_pair(Ecc::Unit::LRF,    Ecc::ErrorType::UNCORR) },
        { EccErrCounter::SHM_CORR,      std::make_pair(Ecc::Unit::SHM,    Ecc::ErrorType::CORR)   },
        { EccErrCounter::SHM_UNCORR,    std::make_pair(Ecc::Unit::SHM,    Ecc::ErrorType::UNCORR) },
        { EccErrCounter::TEX_CORR,      std::make_pair(Ecc::Unit::TEX,    Ecc::ErrorType::CORR)   },
        { EccErrCounter::TEX_UNCORR,    std::make_pair(Ecc::Unit::TEX,    Ecc::ErrorType::UNCORR) },
        { EccErrCounter::L1DATA_CORR,   std::make_pair(Ecc::Unit::L1DATA, Ecc::ErrorType::CORR)   },
        { EccErrCounter::L1DATA_UNCORR, std::make_pair(Ecc::Unit::L1DATA, Ecc::ErrorType::UNCORR) },
        { EccErrCounter::L1TAG_CORR,    std::make_pair(Ecc::Unit::L1TAG,  Ecc::ErrorType::CORR)   },
        { EccErrCounter::L1TAG_UNCORR,  std::make_pair(Ecc::Unit::L1TAG,  Ecc::ErrorType::UNCORR) },
        { EccErrCounter::CBU_CORR,      std::make_pair(Ecc::Unit::CBU,    Ecc::ErrorType::CORR)   },
        { EccErrCounter::CBU_UNCORR,    std::make_pair(Ecc::Unit::CBU,    Ecc::ErrorType::UNCORR) },
    };
};

//!
//! \brief Colwert EccErrCounter::Index to ECC unit and error type.
//!
RC EccErrCounter::UnitErrFromEccErrCounterIndex
(
    EccErrCounter::Index idx
    ,Ecc::Unit* pEclwnit
    ,Ecc::ErrorType* pErrType
)
{
    MASSERT(pEclwnit);
    MASSERT(pErrType);

    auto eccErrSearch = s_FromEccErrCounterIndexMap.find(idx);
    if (eccErrSearch == s_FromEccErrCounterIndexMap.end())
    {
        Printf(Tee::PriError, "Unknown EccErrCounter::Index. If this is a new "
               "ECC unit, please index by (Eclwnit, ErrType)\n");
        MASSERT(!"Unknown EccErrCounter::Index. If this is a new ECC unit, "
                "please index by (Eclwnit, ErrType)");
        return RC::SOFTWARE_ERROR;
    }

    *pEclwnit = eccErrSearch->second.first;
    *pErrType = eccErrSearch->second.second;

    return RC::OK;
}

//--------------------------------------------------------------------
//!
//! EccErrCounter constructor
//!
EccErrCounter::EccErrCounter(GpuSubdevice *pGpuSubdevice) :
    GpuErrCounter(pGpuSubdevice),
    m_BlacklistPagesOnError(false),
    m_DumpPagesOnError(false),
    m_LogAsMemError(true),
    m_RowRemapOnError(false),
    m_UpdateMode(CounterUpdateMode::DirectRMCalls),
    m_TotalFbAddressCount(0)
{
    m_EnabledEclwnits.fill(false);
}

// Used to tell ErrorLogger to ignore ECC interrupts.  We're handling
// them in this class.
//
static bool ErrorLogFilter(const char *errMsg)
{
    const char * patterns[] =
    {
        "LW_PFB_FBPA_*_ECC_STATUS(?)_SEC_INTR_PENDING\n"
        ,"LW_PFB_FBPA_*_ECC_STATUS(?)_DED_INTR_PENDING\n"
        ,"LW_PFB_FBPA_*_ECC_STATUS(*)_POISON_INTR_PENDING at address: 0x*\n"
        ,"LW_PLTCG_LTC*_LTS?_INTR_ECC_SEC_ERROR_PENDING\n"
        ,"LW_PLTCG_LTC*_LTS?_INTR_ECC_DED_ERROR_PENDING\n"
        ,"LW_PLTCG_LTC*_LTS*_L2_CACHE_ECC_STATUS_UNCORRECTED_ERR_CLIENT_POISON_RETURNED_PENDING\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_HWW_ESR_ECC_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_L1C_HWW_ESR:0x80000010 "
         "LW_PGRAPH_PRI_GPC?_TPC?_L1C_HWW_ESR_ADDR:0x0 "
         "LW_PGRAPH_PRI_GPC?_TPC?_L1C_HWW_ESR_REQ:0x0\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_WARP_ESR_ERROR_NONE\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_WARP_ESR :0x0 "
         "LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_WARP_ESR_REPORT_MASK :0x??eff2\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_GLOBAL_ESR :0x0 "
         "LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_GLOBAL_ESR_REPORT_MASK :0x*f\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS_CORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS_UNCORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_L1_DATA_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_L1_DATA_ECC_STATUS_CORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_L1_DATA_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_L1_TAG_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_L1_TAG_ECC_STATUS_CORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_L1_TAG_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_CBU_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_CBU_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS_SINGLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_HWW_PRI_GPC?_TPC?_SM_LRF_ECC_STATUS_CORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_SM_SHM_ECC_STATUS_SINGLE_ERR_CORRECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_SM_HWW_PRI_GPC?_TPC*_SM_SHM_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_SM_SHM_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_HWW_ESR:0x80000??? "
         "LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_HWW_ESR_REQ:0x0\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_HWW_ESR_ECC_SEC*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_HWW_ESR_ECC_DED*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_ECC_CNT_TOTAL_SEC?\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_ECC_CNT_TOTAL_DED?\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_ECC_CNT_TOTAL_DED? *\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_ECC_CNT_TOTAL_SEC? *\n"
        ,"Handle TPC Tex HWW for TRM=? on GPC? TPC*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_SM_HWW_WARP_ESR\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_HWW_WARP_ESR\n"
        ,"LW_PFB_NISO_FB_INTR ? pending\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_HWW_ESR_PARITY_CORR_?"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_TEX_M_HWW_ESR_PARITY_UNCORR_?"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_SM_ICACHE_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC*_SM_ICACHE_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_MMU_GPCMMU_GLOBAL_ESR_ECC_UNCORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_MMU_L1TLB_ECC_STATUS\n"
        ,"LW_PGRAPH_PRI_GPC?_MMU_L1TLB_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_MMU_L1TLB_ECC_STATUS_UNCORRECTED_ERR\n"
        ,"LW_PGRAPH_PRI_GPC?_GCC_HWW_ESR_ECC_UNCORRECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_GCC_L15_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_GCC_L15_ECC_STATUS_UNCORRECTED_ERR\n"
        ,"LW_PFB_PRI_MMU_HUBMMU_GLOBAL_ESR_ECC_UNCORRECTED_ERR\n"
        ,"LW_PFB_PRI_MMU_L2TLB_ECC_STATUS :0x*\n"
        ,"LW_PFB_PRI_MMU_HUBTLB_ECC_STATUS :0x*\n"
        ,"LW_PFB_PRI_MMU_FILLUNIT_ECC_STATUS :0x*\n"
        ,"LW_PGRAPH_PRI_GPC?_GPCCS_HWW_ESR_ECC_CORRECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_GPCCS_HWW_ESR_ECC_UNCORRECTED\n"
        ,"LW_PGRAPH_PRI_FECS_HOST_INT_STATUS_ECC_CORRECTED\n"
        ,"LW_PGRAPH_PRI_FECS_HOST_INT_STATUS_ECC_UNCORRECTED\n"
        ,"LW_XAL_EP_REORDER_ECC_STATUS :0x*\n"
        ,"LW_XAL_EP_REORDER_ECC_ADDRESS :0x*\n"
        ,"LW_XAL_EP_REORDER_ECC_STATUS_CORRECTED_ERR\n"
        ,"LW_XAL_EP_REORDER_ECC_STATUS_UNCORRECTED_ERR\n"
        ,"LW_XAL_EP_P2PREQ_ECC_STATUS :0x*\n"
        ,"LW_XAL_EP_P2PREQ_ECC_ADDRESS :0x*\n"
        ,"LW_XAL_EP_P2PREQ_ECC_STATUS_CORRECTED_ERR\n"
        ,"LW_XAL_EP_P2PREQ_ECC_STATUS_UNCORRECTED_ERR\n"
        ,"INTR_0_ECC_CORRECTABLE pending.\n"
        ,"INTR_0_ECC_UNCORRECTABLE pending.\n"
        ,"FBHUB POISON interrupt detected.\n"
        ,"LW_PGRAPH_PRI_GPC?_TPC?_SM_RAMS_ECC_STATUS_DOUBLE_ERR_DETECTED\n"
        ,"LW_PGRAPH_PRI_GPC?_MMU?_GPCMMU_GLOBAL_ESR_ECC_UNCORRECTED_ERR\n"
        ,"LW_PFB_HSMMU?_PRI_MMU_HSHUBMMU_GLOBAL_ESR_ECC_UNCORRECTED_ERR\n"
        ,"LW_PFB_HSMMU?_PRI_MMU_HSCE_L1TLB_ECC_STATUS:0x*\n"
        ,"LW_PFB_HSMMU?_PRI_MMU_LINK_L1TLB_ECC_STATUS:0x*\n"
        ,"LW_PGRAPH_PRI_GPC*_TPC*_SM*_HWW_GLOBAL_ESR_POISON_DATA\n"
        ,"INTR_0_EGRESS_POISON pending.\n"
    };

    for (size_t i = 0; i < NUMELEMS(patterns); i++)
    {
        if (Utility::MatchWildCard(errMsg, patterns[i]))
        {
            if (Utility::MatchWildCard(errMsg, "*POISON*"))
            {
                Printf(Tee::PriError, "Device interrupt: %s\n", errMsg);
            }
            return false;
        }
    }
    return true; // log this error, it's not one of the filtered items.
}

//--------------------------------------------------------------------
//!
//! \brief Initialize the counter, and start checking for errors.
//!
RC EccErrCounter::Initialize()
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    JavaScriptPtr pJs;
    RC rc;

    // Determine which ECC sources to track
    //
    bool eccSupported = 0;
    UINT32 eccEnabledMask = 0;
    CHECK_RC(pGpuSubdevice->GetEccEnabled(&eccSupported, &eccEnabledMask));

    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);

        m_EnabledEclwnits[unit] = eccSupported && Ecc::IsUnitEnabled(eclwnit, eccEnabledMask);
    }

    // Allocate space for the ecc-error arrays
    //
    UINT32 arraySize = GetPackedArraySize();
    m_TotalErrors.resize(arraySize, 0);
    m_ExpectedErrors.resize(arraySize, 0);
    m_IsExpectingErrors.resize(arraySize, 0);

    // Set fields provided through the command line
    if (m_EnabledEclwnits[static_cast<UINT32>(Ecc::Unit::DRAM)])
    {
        pJs->GetProperty(EccErrCounter_Object, EccErrCounter_BlacklistPagesOnError,
                         &m_BlacklistPagesOnError);
        pJs->GetProperty(EccErrCounter_Object, EccErrCounter_DumpPagesOnError,
                         &m_DumpPagesOnError);
        pJs->GetProperty(EccErrCounter_Object, EccErrCounter_LogAsMemError,
                         &m_LogAsMemError);
        pJs->GetProperty(EccErrCounter_Object, EccErrCounter_RowRemapOnError,
                         &m_RowRemapOnError);
    }

    // Wait for synchronous scrub to finish
    // Always do the sync scrub because of problems with async scrub
    //if (pGpuSubdevice->HasBug(622646))
    {
        CHECK_RC(WaitForScrub(10 * Tasker::GetDefaultTimeoutMs()));
    }

    // Disable dynamic page-retirement, so that the RM doesn't
    // blacklist pages in which we test ECC.  (This can be overridden
    // via -enable_ecc_inforom_reporting, but don't run test 155!)
    //
    bool enableEccInforomReporting = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_EnableEccInforomReporting",
                     &enableEccInforomReporting);
    if (!enableEccInforomReporting)
    {
        CHECK_RC(pGpuSubdevice->DisableEccInfoRomReporting());
    }

    // Setup ECC event notifiers
    if (!Platform::IsTegra() && Platform::GetSimulationMode() != Platform::SimulationMode::Amodel)
    {
        StartNotifiers();
    }

    // Initialize the error counter
    //
    CHECK_RC(ErrCounter::Initialize("EccErrCount",
                                    EccErrCounter::s_NumEccErrTypes,
                                    arraySize - EccErrCounter::s_NumEccErrTypes,
                                    nullptr, MODS_TEST, ECC_PRI));

    // If the EccErrCounter is disabled in JS, then ignore ECC
    // interrupts.
    //
    if (!IsDisabled())
    {
        ErrorLogger::InstallErrorLogFilter(ErrorLogFilter);
    }
    return rc;
}

EccErrCounter::~EccErrCounter()
{
    StopNotifiers();
}

RC EccErrCounter::StartNotifiers()
{
    RC rc;
    Tasker::MutexHolder lock(GetMutex());
    if (!IsUsingNotifiers())
    {
        GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
        CHECK_RC(pGpuSubdevice->HookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::CORR),
                                        HandleCorrectableNotifierEvent,
                                        this,
                                        LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                                        GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
        CHECK_RC(pGpuSubdevice->HookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::UNCORR),
                                        HandleUncorrectableNotifierEvent,
                                        this,
                                        LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                                        GpuSubdevice::NOTIFIER_MEMORY_ENABLED));

        // Initialize all counts
        m_NotifierEccCounts.Clear();
        CHECK_RC(GetAllErrorCountsFromRM(pGpuSubdevice, m_NotifierEccCounts));

        m_UpdateMode = CounterUpdateMode::Notifiers;
    }
    return rc;
}

RC EccErrCounter::StopNotifiers()
{
    RC rc;
    Tasker::MutexHolder lock(GetMutex());
    if (IsUsingNotifiers())
    {
        GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
        CHECK_RC(pGpuSubdevice->UnhookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::CORR)));
        CHECK_RC(pGpuSubdevice->UnhookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::UNCORR)));
        m_UpdateMode = CounterUpdateMode::DirectRMCalls;
    }
    return rc;
}

RC EccErrCounter::GetErrorCountFromRM(
    GpuSubdevice* pGpuSubdevice,
    Ecc::Unit eclwnit,
    SubdevDetailedEccCounts& subdevDetailedEccCounts)
{
    RC rc;
    MASSERT(pGpuSubdevice);
    SubdeviceFb* pSubdeviceFb = pGpuSubdevice->GetSubdeviceFb();
    MASSERT(pSubdeviceFb);
    SubdeviceGraphics* pSubdeviceGraphics =
        pGpuSubdevice->GetSubdeviceGraphics();
    MASSERT(pSubdeviceGraphics);

    switch (eclwnit)
    {
        case Ecc::Unit::DRAM:
            CHECK_RC(pSubdeviceFb->GetDramDetailedEccCounts(
                     &subdevDetailedEccCounts.fbCounts));
            break;

        case Ecc::Unit::L2:
            CHECK_RC(pSubdeviceFb->GetLtcDetailedEccCounts(
                     &subdevDetailedEccCounts.l2Counts));
            break;

        case Ecc::Unit::L1:
            CHECK_RC(pSubdeviceGraphics->GetL1cDetailedEccCounts(
                     &subdevDetailedEccCounts.l1Counts));
            break;

        case Ecc::Unit::L1DATA:
            CHECK_RC(pSubdeviceGraphics->GetL1DataDetailedEccCounts(
                     &subdevDetailedEccCounts.l1DataCounts));
            break;

        case Ecc::Unit::L1TAG:
            CHECK_RC(pSubdeviceGraphics->GetL1TagDetailedEccCounts(
                     &subdevDetailedEccCounts.l1TagCounts));
            break;

        case Ecc::Unit::CBU:
            CHECK_RC(pSubdeviceGraphics->GetCbuDetailedEccCounts(
                     &subdevDetailedEccCounts.cbuCounts));
            break;

        case Ecc::Unit::LRF:
            CHECK_RC(pSubdeviceGraphics->GetRfDetailedEccCounts(
                     &subdevDetailedEccCounts.smCounts));
            break;

        case Ecc::Unit::SHM:
            CHECK_RC(pSubdeviceGraphics->GetSHMDetailedEccCounts(
                     &subdevDetailedEccCounts.shmCounts));
            break;

        case Ecc::Unit::TEX:
        {
            SubdeviceGraphics::GetTexDetailedEccCountsParams subdevGrTexCounts;
            CHECK_RC(pSubdeviceGraphics->GetTexDetailedEccCounts(&subdevGrTexCounts));

            // Colwert the 3-dim record (GPC, TPC, TEX) to 2-dim (GPC, TPC)
            const UINT32 numGpcs = pGpuSubdevice->GetGpcCount();
            const UINT32 numTexs = pGpuSubdevice->GetTexCountPerTpc();

            subdevDetailedEccCounts.texCounts.eccCounts.resize(numGpcs);
            MASSERT(subdevGrTexCounts.eccCounts.size() == numGpcs);
            for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
            {
                const UINT32 numTpcs = pGpuSubdevice->GetTpcCountOnGpc(gpc);
                subdevDetailedEccCounts.texCounts.eccCounts[gpc].resize(numTpcs);
                MASSERT(subdevGrTexCounts.eccCounts[gpc].size() == numTpcs);
                for (UINT32 tpc = 0; tpc < numTpcs; ++tpc)
                {
                    Ecc::ErrCounts eccCounts = {};

                    MASSERT(subdevGrTexCounts.eccCounts[gpc][tpc].size() == numTexs);
                    for (UINT32 tex = 0; tex < numTexs; ++tex)
                    {
                        eccCounts += subdevGrTexCounts.eccCounts[gpc][tpc][tex];
                    }

                    subdevDetailedEccCounts.texCounts.eccCounts[gpc][tpc] = eccCounts;
                }
            }
            break;
        }

        case Ecc::Unit::SM_ICACHE:
            CHECK_RC(pSubdeviceGraphics->GetSmIcacheDetailedEccCounts(
                     &subdevDetailedEccCounts.smIcacheCounts));
            break;

        case Ecc::Unit::GCC_L15:
            CHECK_RC(pSubdeviceGraphics->GetGccL15DetailedEccCounts(
                     &subdevDetailedEccCounts.gccL15Counts));
            break;

        case Ecc::Unit::GPCMMU:
            CHECK_RC(pSubdeviceGraphics->GetGpcMmuDetailedEccCounts(
                     &subdevDetailedEccCounts.gpcMmuCounts));
            break;

        case Ecc::Unit::HUBMMU_L2TLB:
            CHECK_RC(pSubdeviceGraphics->GetHubMmuL2TlbDetailedEccCounts(
                     &subdevDetailedEccCounts.hubMmuL2TlbCounts));
            break;

        case Ecc::Unit::HUBMMU_HUBTLB:
            CHECK_RC(pSubdeviceGraphics->GetHubMmuHubTlbDetailedEccCounts(
                     &subdevDetailedEccCounts.hubMmuHubTlbCounts));
            break;

        case Ecc::Unit::HUBMMU_FILLUNIT:
            CHECK_RC(pSubdeviceGraphics->GetHubMmuFillUnitDetailedEccCounts(
                     &subdevDetailedEccCounts.hubMmuFillUnitCounts));
            break;

        case Ecc::Unit::GPCCS:
            CHECK_RC(pSubdeviceGraphics->GetGpccsDetailedEccCounts(
                     &subdevDetailedEccCounts.gpccsCounts));
            break;

        case Ecc::Unit::FECS:
            CHECK_RC(pSubdeviceGraphics->GetFecsDetailedEccCounts(
                     &subdevDetailedEccCounts.fecsCounts));
            break;

        case Ecc::Unit::SM_RAMS:
            CHECK_RC(pSubdeviceGraphics->GetSmRamsDetailedEccCounts(
                     &subdevDetailedEccCounts.smRamsCounts));
            break;

        case Ecc::Unit::HSHUB:
            CHECK_RC(pGpuSubdevice->GetHsHubEcc()->GetHsHubDetailedEccCounts(
                     &subdevDetailedEccCounts.hshubCounts));
            break;

        case Ecc::Unit::PMU:
            CHECK_RC(pGpuSubdevice->GetFalconEcc()->GetPmuDetailedEccCounts(
                     &subdevDetailedEccCounts.pmuCounts));
            break;

        case Ecc::Unit::PCIE_REORDER:
            CHECK_RC(pGpuSubdevice->GetPciBusEcc()->GetDetailedReorderEccCounts(
                     &subdevDetailedEccCounts.pciBusReorderCounts));
            break;

        case Ecc::Unit::PCIE_P2PREQ:
            CHECK_RC(pGpuSubdevice->GetPciBusEcc()->GetDetailedP2PEccCounts(
                     &subdevDetailedEccCounts.pciBusP2PCounts));
            break;

        default:
            Printf(Tee::PriError, "Enabled ECC unit (%s) is not supported\n",
                   Ecc::ToString(eclwnit));
            MASSERT(!"Enabled ECC unit is not supported");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief Get Ecc errcounts directly by querying RM.
//!
RC EccErrCounter::GetAllErrorCountsFromRM(
    GpuSubdevice* pGpuSubdevice,
    SubdevDetailedEccCounts& subdevDetailedEccCounts)
{
    RC rc;
    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        if (m_EnabledEclwnits[unit])
        {
            CHECK_RC(GetErrorCountFromRM(pGpuSubdevice, static_cast<Ecc::Unit>(unit),
                                         subdevDetailedEccCounts));
        }
    }
    return rc;
}

/* static */
void EccErrCounter::HandleNotifierEvent(void *pArg, Ecc::ErrorType type)
{
    MASSERT(pArg);
    EccErrCounter* pCounter = static_cast<EccErrCounter*>(pArg);

    // Lock the counter's mutex for the duration of this function
    Tasker::MutexHolder lock(pCounter->GetMutex());

    LwNotification notifyData = {};
    GpuSubdevice *pGpuSubdevice = pCounter->GetGpuSubdevice();
    RC rc = pGpuSubdevice->GetResmanEventData(Ecc::ToRmNotifierEvent(type), &notifyData);
    if (rc != RC::OK)
    {
        return;
    }

    Ecc::Unit unit = Ecc::ToEclwnitFromRmGpuEclwnit(
                        static_cast<Ecc::RmGpuEclwnit>(notifyData.info16));

    Printf(Tee::PriDebug, "Got notifier event: type: %s, unit: %s\n",
                Ecc::ToString(type), Ecc::ToString(unit));

    pCounter->GetErrorCountFromRM(pGpuSubdevice, unit, pCounter->m_NotifierEccCounts);
}

//--------------------------------------------------------------------
//!
//! \brief Update the ECC error counts.
//!
RC EccErrCounter::Update()
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();

    // Lock the mutex for the duration of this function
    //
    Tasker::MutexHolder lock(GetMutex());

    // When using notifiers, m_NotifierEccCounts will hold the most recent error counts
    SubdevDetailedEccCounts eccCounts = {};
    const bool usingNotifiers = (m_UpdateMode == CounterUpdateMode::Notifiers);
    if (!usingNotifiers)
    {
        CHECK_RC(GetAllErrorCountsFromRM(pGpuSubdevice, eccCounts));
    }

    // Update m_TotalErrors[] and m_ExpectedErrors[] from the queried
    // values.
    //
    vector<UINT64> errorArray(m_TotalErrors.size());
    CHECK_RC(PackArray(usingNotifiers ? m_NotifierEccCounts : eccCounts, &errorArray[0]));

    const UINT32 fbCorrIndex
        = EccErrCounter::UnitErrCountIndex(Ecc::Unit::DRAM, Ecc::ErrorType::CORR);
    const UINT32 fbUncorrIndex
        = EccErrCounter::UnitErrCountIndex(Ecc::Unit::DRAM, Ecc::ErrorType::UNCORR);
    const bool foundNewFbSbes = (errorArray[fbCorrIndex] != m_TotalErrors[fbCorrIndex]);
    const bool foundNewFbDbes = (errorArray[fbUncorrIndex] != m_TotalErrors[fbUncorrIndex]);
    for (size_t i = 0; i < errorArray.size(); ++i)
    {
        MASSERT(m_TotalErrors[i] <= errorArray[i]);
        MASSERT(m_ExpectedErrors[i] <= m_TotalErrors[i]);
        if (m_IsExpectingErrors[i])
            m_ExpectedErrors[i] += (errorArray[i] - m_TotalErrors[i]);
        m_TotalErrors[i] = errorArray[i];
    }

    // Get the addresses of new FB ECC errors
    //
    if (foundNewFbSbes || foundNewFbDbes)
    {
        SubdeviceFb *pSubdeviceFb = pGpuSubdevice->GetSubdeviceFb();
        SubdeviceFb::GetLatestEccAddressesParams fbAddrParams;
        if (pSubdeviceFb->GetLatestEccAddresses(&fbAddrParams) == OK)
        {
            UINT32 numNewAddrs = min(
                    fbAddrParams.totalAddressCount - m_TotalFbAddressCount,
                    static_cast<UINT32>(fbAddrParams.addresses.size()));
            for (size_t i = fbAddrParams.addresses.size() - numNewAddrs;
                    i < fbAddrParams.addresses.size(); ++i)
            {
                const FbAddressType &addr = fbAddrParams.addresses[i];
                if ((addr.isSbe && !m_IsExpectingErrors[fbCorrIndex]) ||
                    (!addr.isSbe && !m_IsExpectingErrors[fbUncorrIndex]))
                {
                    m_FbAddresses.push_back(addr);
                }
            }
            m_TotalFbAddressCount = fbAddrParams.totalAddressCount;
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//!
//! \see EccErrCounter::GetTotalErrors
//!
UINT64 EccErrCounter::GetTotalErrors(Index idx) const
{
    Ecc::Unit eclwnit;
    Ecc::ErrorType errType;
    if (EccErrCounter::UnitErrFromEccErrCounterIndex(idx, &eclwnit, &errType) != OK)
    {
        Printf(Tee::PriError, "Unknown EccErrCounter::Index\n");
        MASSERT(!"Unknown EccErrCounter::Index");
        return 0;
    }

    return GetTotalErrors(eclwnit, errType);
}

//--------------------------------------------------------------------
//!
//! \brief Get the total error count for a given ECC unit and error type.
//!
UINT64 EccErrCounter::GetTotalErrors
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errType
) const
{
    return m_TotalErrors[UnitErrCountIndex(eclwnit, errType)];
}

//--------------------------------------------------------------------
//!
//! \brief Get the ECC errors that weren't deliberately injected by ECC tests.
//!
RC EccErrCounter::GetUnexpectedErrors(vector<UINT64>* pCounts)
{
    Tasker::MutexHolder lock(GetMutex());
    vector<UINT64> errCounts(m_TotalErrors.size());
    RC rc;

    pCounts->clear();
    pCounts->reserve(EccErrCounter::s_NumEccErrTypes);

    CHECK_RC(EccErrCounter::ReadErrorCount(&errCounts[0]));
    pCounts->insert(pCounts->begin(), errCounts.begin(),
                    errCounts.begin() + EccErrCounter::s_NumEccErrTypes);

    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief Begin expecting errors on the indicated counter
//!
//! This method should be called at the start of any method that
//! deliberately injects ECC errors, to let this object know that the
//! errors should be ignored until the corresponding
//! EndExpectingErrors() is called.
//!
RC EccErrCounter::BeginExpectingErrors(Ecc::Unit eclwnit, Ecc::ErrorType errType)
{
    return SetExpectingErrors(eclwnit, errType, true/*isExpectingErrs*/);
}

RC EccErrCounter::BeginExpectingErrors(Index idx)
{
    RC rc;
    Ecc::Unit eclwnit;
    Ecc::ErrorType errType;
    CHECK_RC(UnitErrFromEccErrCounterIndex(idx, &eclwnit, &errType));
    CHECK_RC(BeginExpectingErrors(eclwnit, errType));
    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief End expecting errors on the indicated counter
//!
//! This method should be called at the end of any method that
//! deliberately injects ECC errors.  See BeginExpectedErrors() for
//! more details.
//!
RC EccErrCounter::EndExpectingErrors(Ecc::Unit eclwnit, Ecc::ErrorType errType)
{
    return SetExpectingErrors(eclwnit, errType, false/*isExpectingErrs*/);
}

RC EccErrCounter::EndExpectingErrors(Index idx)
{
    RC rc;
    Ecc::Unit eclwnit;
    Ecc::ErrorType errType;
    CHECK_RC(UnitErrFromEccErrCounterIndex(idx, &eclwnit, &errType));
    CHECK_RC(EndExpectingErrors(eclwnit, errType));
    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief End expecting errors on all counters
//!
//! This method is equivalent to calling EndExpectingErrors() on all
//! indices that have had BeginExpectingErrors called on it.  Mostly
//! used as an argument to Utility::CleanupOnReturn() so that callers
//! can use RAII to do cleanup if the caller aborts.
//!
RC EccErrCounter::EndExpectingAllErrors()
{
    Tasker::MutexHolder lock(GetMutex());
    RC rc;

    CHECK_RC(Update());
    fill(m_IsExpectingErrors.begin(), m_IsExpectingErrors.end(), 0);
    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief Wait for the RM to finish scrubbing.
//!
//! The RM "scrubs" the ECC during the first few seconds after
//! initialization, by writing every ECC-checked memory sector so that
//! the ECC checkbits all contain valid values.  If mods is going to
//! do anything with ECC, the easiest way to deal with the scrub is
//! simply to wait until it's done.
//!
RC EccErrCounter::WaitForScrub(FLOAT64 timeoutMs)
{
    GpuSubdevice* const pSubdev = GetGpuSubdevice();
    StickyRC rc;

    rc = Tasker::PollHw(timeoutMs, [&]()->bool
    {
        LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS eccStatus;

        memset(&eccStatus, 0, sizeof(eccStatus));
        if (pSubdev->IsSMCEnabled())
        {
            pSubdev->SetGrRouteInfo(&(eccStatus.grRouteInfo), pSubdev->GetSmcEngineIdx());
        }
        RC myrc = LwRmPtr()->ControlBySubdevice(
                                    pSubdev,
                                    LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                    &eccStatus, sizeof(eccStatus));
        if (myrc != OK)
        {
            if (myrc == RC::LWRM_NOT_SUPPORTED)
            {
                myrc.Clear();
            }
            else
            {
                rc = myrc;
            }
            return true;
        }

        for (UINT32 i = 0; i < LW2080_CTRL_GPU_ECC_UNIT_COUNT; ++i)
        {
            if (eccStatus.units[i].enabled && !eccStatus.units[i].scrubComplete)
            {
                return false;
            }
        }
        return true;
    }, __FUNCTION__);

    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief Override the base-class ReadErrorCount method.
//!
/* virtual */ RC EccErrCounter::ReadErrorCount(UINT64 *pCount)
{
    RC rc;
    CHECK_RC(Update());
    for (size_t i = 0; i < m_TotalErrors.size(); ++i)
    {
        pCount[i] = m_TotalErrors[i] - m_ExpectedErrors[i];
    }
    return rc;
}

//--------------------------------------------------------------------
/*virtual*/ string EccErrCounter::DumpStats()
{
    string errStr;
    vector<UINT64> lwrrentCount = GetLwrrentCount();
    vector<UINT64> allowedCount = GetAllowedErrors();

    errStr = Utility::StrPrintf( "\nECC errors stats\n"
            "                  Errors          Allowed\n");

    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);

        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        for (UINT32 err = 0;
             err < static_cast<UINT32>(Ecc::ErrorType::MAX);
             ++err)
        {
            const UINT32 index = EccErrCounter::UnitErrCountIndex(eclwnit,
                                                                static_cast<Ecc::ErrorType>(err));

            errStr += Utility::StrPrintf("%s %s \t%12llu %16llu\n",
                                         Ecc::ToString(eclwnit),
                                         Ecc::ToString(static_cast<Ecc::ErrorType>(err)),
                                         lwrrentCount[index], allowedCount[index]);
        }
    }

    return errStr;
}

//--------------------------------------------------------------------
void EccErrCounter::ReportError
(
    Ecc::Unit      eclwnit,
    Ecc::ErrorType errType,
    UINT08         partGpc,
    UINT08         subpTpc,
    const Ecc::ErrCounts* pErrCounts
) const
{
    UINT64 count = ExtractErrCount(pErrCounts, errType);

    // If new error, report it
    if (count > 0)
    {
        string loc;
        GetLocation(eclwnit, partGpc, subpTpc, &loc, nullptr);
        Printf(Tee::PriNormal, "  * %llu in %s\n", count, loc.c_str());
    }
}

//--------------------------------------------------------------------
//!
//! \brief Override the base-class OnError() method.
//!
/* virtual */ RC EccErrCounter::OnError(const ErrorData *pData)
{
    RC rc;

    if (!pData->bCheckExitOnly)
    {
        SubdevDetailedEccCounts subdevDetailedEccCounts;
        CHECK_RC(UnpackArray(&subdevDetailedEccCounts, pData->pNewErrors));

        if (DevMgrMgr::d_GraphDevMgr->NumGpus() > 1)
        {
            Printf(Tee::PriNormal, "ECC errors exceeded the threshold on %s:\n",
                   GetGpuSubdevice()->GetName().c_str());
        }
        else
        {
            Printf(Tee::PriNormal, "ECC errors exceeded the threshold:\n");
        }

        for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
        {
            // Skip disabled units
            if (!m_EnabledEclwnits[unit]) { continue; }

            const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);
            Ecc::UnitType unitType = Ecc::GetUnitType(eclwnit);
            const vector<vector<Ecc::ErrCounts>> * pDetailedEccCounts = nullptr;
            if ((unitType != Ecc::UnitType::FALCON) &&
                (unitType != Ecc::UnitType::PCIE))
            {
                const Ecc::DetailedErrCounts * pErrCounts =
                    GetEclwnitCounts(&subdevDetailedEccCounts, eclwnit);
                MASSERT(pErrCounts);
                pDetailedEccCounts = &pErrCounts->eccCounts;
            }

            for (UINT32 err = 0; err < static_cast<UINT32>(Ecc::ErrorType::MAX); ++err)
            {
                const Ecc::ErrorType errType = static_cast<Ecc::ErrorType>(err);
                const UINT32 index = EccErrCounter::UnitErrCountIndex(eclwnit, errType);

                // Skip if no new errors
                if (!pData->pNewErrors[index]) { continue; }

                Printf(Tee::PriNormal, "- %s %s: errs = %llu, thresh = %llu\n",
                       Ecc::ToString(eclwnit), Ecc::ToString(errType),
                       pData->pNewErrors[index], pData->pAllowedErrors[index]);

                // No unit specific location data so dont need to report where errors oclwrred
                if ((unitType == Ecc::UnitType::FALCON) || (unitType == Ecc::UnitType::PCIE))
                    continue;

                const UINT32 dim1Size = static_cast<UINT32>(pDetailedEccCounts->size());
                MASSERT(dim1Size == pDetailedEccCounts->size()); // truncation check
                for (UINT32 i = 0; i < dim1Size; ++i)
                {
                    const UINT32 dim2Size = static_cast<UINT32>((*pDetailedEccCounts)[i].size());
                    MASSERT(dim2Size == (*pDetailedEccCounts)[i].size()); // truncation check
                    for (UINT32 j = 0; j < dim2Size; ++j)
                    {
                        const auto& eccCount = (*pDetailedEccCounts)[i][j];
                        ReportError(eclwnit, errType, i, j, &eccCount);
                    }
                }
            }
        }
    }

    // Get the high level GPU location information
    //
    vector<std::pair<Ecc::Unit, Ecc::ErrorType>> eclwnitErrs;
    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);

        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        for (UINT32 err = 0;
             err < static_cast<UINT32>(Ecc::ErrorType::MAX);
             ++err)
        {
            const Ecc::ErrorType errType = static_cast<Ecc::ErrorType>(err);
            const UINT32 index = EccErrCounter::UnitErrCountIndex(eclwnit, errType);

            if (pData->pNewErrors[index])
            {
                eclwnitErrs.emplace_back(eclwnit, errType);
            }
        }
    }

    if (!eclwnitErrs.empty())
    {
        // List all the errors and return one of them
        string s = "The following ECC errors were encountered:\n";
        const auto* pWorstErr = &eclwnitErrs[0];

        for (const auto& eclwnitErr : eclwnitErrs)
        {
            RC errRc = Ecc::GetRC(eclwnitErr.first, eclwnitErr.second);
            s += Utility::StrPrintf("\tRC %u: %s\n", errRc.Get(), errRc.Message());

            // Return uncorrectable over correctable errors
            //
            // NOTE: Can define any heuristic for error precedence here
            if ((pWorstErr->second != Ecc::ErrorType::UNCORR)
                && (eclwnitErr.second == Ecc::ErrorType::UNCORR))
            {
                pWorstErr = &eclwnitErr;
            }
        }

        Printf(Tee::PriLow, "%s", s.c_str());

        return Ecc::GetRC(pWorstErr->first, pWorstErr->second);
    }

    return rc;
}

//--------------------------------------------------------------------
void EccErrCounter::ReportNewError
(
    Tee::Priority  pri,
    Ecc::Unit      eclwnit,
    Ecc::ErrorType errType,
    UINT08         partGpc,
    UINT08         sublocTpc,
    const Ecc::ErrCounts* pErrCounts
) const
{
    UINT64 count = ExtractErrCount(pErrCounts, errType);

    // If new error, report it
    if (count > 0)
    {
        const string eclwnitStr = Ecc::ToString(eclwnit);
        const string errTypeStr = Ecc::ToString(errType);
        string loc;
        Ecc::UnitType unitType;
        GetLocation(eclwnit, partGpc, sublocTpc, &loc, &unitType);

        string errStr = Utility::StrPrintf("New ECC error(s): %llu %s %s ERROR(s)",
                                           count, eclwnitStr.c_str(), errTypeStr.c_str());
        if ((unitType != Ecc::UnitType::FALCON) &&
            (unitType != Ecc::UnitType::PCIE))
        {
            errStr += Utility::StrPrintf(" in %s", loc.c_str());
        }
        Printf(pri, "%s\n", errStr.c_str());
        PrintNewErrCountsToMle(eclwnitStr.c_str(),
                               errTypeStr.c_str(),
                               partGpc, sublocTpc, count, unitType, eclwnit, errType);
    }
}

//--------------------------------------------------------------------
//!
//! \brief Override the base-class OnCountChange() method.
//!
/* virtual */ void EccErrCounter::OnCountChange(const CountChangeData *pData)
{
    FrameBuffer *pFb = GetGpuSubdevice()->GetFB();
    Tee::Priority pri = pData->Verbose ? Tee::PriNormal : Tee::PriLow;

    // Find delta in counts
    //
    vector<UINT64> delta(m_TotalErrors.size());
    for (UINT32 i = 0; i < delta.size(); ++i)
        delta[i] = pData->pNewCount[i] - pData->pOldCount[i];

    // Unpack delta
    //
    SubdevDetailedEccCounts subdevDetailedEccCounts;
    if (UnpackArray(&subdevDetailedEccCounts, delta.data()) != OK)
    {
        Printf(Tee::PriWarn, "Unable to successfully unpack ECC counts\n");
        MASSERT(!"Unable to successfully unpack ECC counts");
        return;
    }

    // Report count change
    //
    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);

        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        Ecc::UnitType unitType = Ecc::GetUnitType(eclwnit);
        const Ecc::DetailedErrCounts * pErrCounts = nullptr;
        const vector<vector<Ecc::ErrCounts>> * pDetailedEccCounts = nullptr;
        if ((unitType != Ecc::UnitType::FALCON) &&
            (unitType != Ecc::UnitType::PCIE))
        {
            pErrCounts = GetEclwnitCounts(&subdevDetailedEccCounts, eclwnit);
            MASSERT(pErrCounts);
            pDetailedEccCounts = &pErrCounts->eccCounts;
        }

        if (unitType == Ecc::UnitType::GPC)
            TpcEccError::LogErrors(GetGpuSubdevice(), *pErrCounts);

        for (UINT32 err = 0; err < static_cast<UINT32>(Ecc::ErrorType::MAX); ++err)
        {
            Ecc::ErrorType errType = static_cast<Ecc::ErrorType>(err);
            if ((unitType == Ecc::UnitType::FALCON) ||
                (unitType == Ecc::UnitType::PCIE))
            {
                MASSERT(pDetailedEccCounts == nullptr);
                const auto *pEccCount = GetEccSingleUnitCounts(&subdevDetailedEccCounts,
                                                               eclwnit);
                ReportNewError(pri, eclwnit, errType, pEccCount);
                continue;
            }

            const UINT32 dim1Size = static_cast<UINT32>(pDetailedEccCounts->size());
            MASSERT(dim1Size == pDetailedEccCounts->size()); // truncation check
            for (UINT32 i = 0; i < dim1Size; ++i)
            {
                const UINT32 dim2Size = static_cast<UINT32>((*pDetailedEccCounts)[i].size());
                MASSERT(dim2Size == (*pDetailedEccCounts)[i].size()); // truncation check
                for (UINT32 j = 0; j < dim2Size; ++j)
                {
                    const auto& eccCount = (*pDetailedEccCounts)[i][j];
                    ReportNewError(pri, eclwnit, errType, i, j, &eccCount);
                }
            }

            // Print DRAM RBC decoding if this is the last of the FB reporting
            if (eclwnit == Ecc::Unit::DRAM
                && (err + 1 == static_cast<UINT32>(Ecc::ErrorType::MAX)))
            {
                // Print the full RBC decoding of any new FB ECC errors for which
                // we have the physAddress.
                //
                for (vector<FbAddressType>::iterator pAddr = m_FbAddresses.begin();
                     pAddr != m_FbAddresses.end(); ++pAddr)
                {
                    FbDecode fbDecode;
                    pFb->DecodeAddress(&fbDecode, pAddr->physAddress,
                                       FrameBuffer::GENERIC_DECODE_PTE_KIND,
                                       FrameBuffer::GENERIC_DECODE_PAGE_SIZE);
                    string message = pAddr->isSbe ? "New FB SBE" : "New FB DBE";
                    message += Utility::StrPrintf(" at PhysAddr=0x%010llx ",
                                                  pAddr->physAddress);
                    message += pFb->GetDecodeString(fbDecode).c_str();
                    if (pAddr->isSbe && pAddr->sbeBitPosition != _UINT32_MAX)
                    {
                        message += Utility::StrPrintf(
                            " Bit=%c%d",
                            pFb->VirtualFbioToLetter(fbDecode.virtualFbio),
                            pFb->GetFbioBitOffset(fbDecode.subpartition,
                                                  fbDecode.pseudoChannel,
                                                  fbDecode.beatOffset,
                                                  pAddr->sbeBitPosition));
                    }
                    message +=
                        Utility::StrPrintf
                        (
                            " Source=%s",
                            Ecc::ToString(pAddr->eccSource)
                        );
                    Printf(pri, "%s\n", message.c_str());

                    if (m_LogAsMemError)
                    {
                        MemError::LogEccError(pFb, pAddr->physAddress,
                                              FrameBuffer::GENERIC_DECODE_PTE_KIND,
                                              FrameBuffer::GENERIC_DECODE_PAGE_SIZE,
                                              pAddr->isSbe, pAddr->sbeBitPosition);
                    }
                }
            }
        }
    }

    using BlacklistSource = Memory::BlacklistSource;
    if (m_BlacklistPagesOnError)
    {
        // Blacklist any FB addresses that got an ECC error
        for (vector<FbAddressType>::iterator pAddr = m_FbAddresses.begin();
                pAddr != m_FbAddresses.end(); ++pAddr)
        {
            Printf(Tee::PriDebug,
                    "Blacklisting addr 0x%llx due to ECC error\n",
                    pAddr->physAddress);
            pFb->BlacklistPage(pAddr->physAddress,
                               FrameBuffer::GENERIC_DECODE_PTE_KIND,
                               FrameBuffer::GENERIC_DECODE_PAGE_SIZE,
                               (pAddr->isSbe ? BlacklistSource::Sbe : BlacklistSource::Dbe),
                               pAddr->rbcAddress);
        }
    }

    if (m_RowRemapOnError)
    {
        using MemErrT = Memory::ErrorType;

        // Row remap any FB address that got an ECC error
        vector<RowRemapper::Request> requests;
        requests.reserve(m_FbAddresses.size());
        for (const FbAddressType& addr : m_FbAddresses)
        {
            Printf(Tee::PriDebug, "Row remap: preparing addr 0x%llx for remap "
                   "due to ECC error\n", addr.physAddress);
            requests.emplace_back(addr.physAddress,
                                  (addr.isSbe ? MemErrT::SBE : MemErrT::DBE));
        }

        pFb->RemapRow(requests);
    }

    if (m_DumpPagesOnError)
    {
        for (vector<FbAddressType>::iterator pAddr = m_FbAddresses.begin();
                pAddr != m_FbAddresses.end(); ++pAddr)
        {
            Printf(Tee::PriDebug,
                    "Dumping page for addr 0x%llx due to ECC error\n",
                    pAddr->physAddress);
            pFb->DumpFailingPage(pAddr->physAddress,
                                 FrameBuffer::GENERIC_DECODE_PTE_KIND,
                                 FrameBuffer::GENERIC_DECODE_PAGE_SIZE,
                                 (pAddr->isSbe ? BlacklistSource::Sbe : BlacklistSource::Dbe),
                                 pAddr->rbcAddress,
                                 ErrorMap::Test());
        }
    }

    // Clear the queue of FB addresses that got an error
    //
    m_FbAddresses.clear();
}

//--------------------------------------------------------------------
//!
//! \brief Extract the ECC error counts for a particular ECC unit.
//!
Ecc::DetailedErrCounts * EccErrCounter::GetEclwnitCounts
(
    SubdevDetailedEccCounts* pSubdevDetailedEccCounts
    ,Ecc::Unit eclwnit
) const
{
    Ecc::DetailedErrCounts * pErrCounts = nullptr;

    switch (eclwnit)
    {
        case Ecc::Unit::DRAM:
            pErrCounts = &pSubdevDetailedEccCounts->fbCounts;
            break;
        case Ecc::Unit::L2:
            pErrCounts = &pSubdevDetailedEccCounts->l2Counts;
            break;
        case Ecc::Unit::L1:
            pErrCounts = &pSubdevDetailedEccCounts->l1Counts;
            break;
        case Ecc::Unit::L1DATA:
            pErrCounts = &pSubdevDetailedEccCounts->l1DataCounts;
            break;
        case Ecc::Unit::L1TAG:
            pErrCounts = &pSubdevDetailedEccCounts->l1TagCounts;
            break;
        case Ecc::Unit::CBU:
            pErrCounts = &pSubdevDetailedEccCounts->cbuCounts;
            break;
        case Ecc::Unit::LRF:
            pErrCounts = &pSubdevDetailedEccCounts->smCounts;
            break;
        case Ecc::Unit::SHM:
            pErrCounts = &pSubdevDetailedEccCounts->shmCounts;
            break;
        case Ecc::Unit::TEX:
            pErrCounts = &pSubdevDetailedEccCounts->texCounts;
            break;
        case Ecc::Unit::SM_ICACHE:
            pErrCounts = &pSubdevDetailedEccCounts->smIcacheCounts;
            break;
        case Ecc::Unit::GCC_L15:
            pErrCounts = &pSubdevDetailedEccCounts->gccL15Counts;
            break;
        case Ecc::Unit::GPCMMU:
            pErrCounts = &pSubdevDetailedEccCounts->gpcMmuCounts;
            break;
        case Ecc::Unit::HUBMMU_L2TLB:
            pErrCounts = &pSubdevDetailedEccCounts->hubMmuL2TlbCounts;
            break;
        case Ecc::Unit::HUBMMU_HUBTLB:
            pErrCounts = &pSubdevDetailedEccCounts->hubMmuHubTlbCounts;
            break;
        case Ecc::Unit::HUBMMU_FILLUNIT:
            pErrCounts = &pSubdevDetailedEccCounts->hubMmuFillUnitCounts;
            break;
        case Ecc::Unit::GPCCS:
            pErrCounts = &pSubdevDetailedEccCounts->gpccsCounts;
            break;
        case Ecc::Unit::FECS:
            pErrCounts = &pSubdevDetailedEccCounts->fecsCounts;
            break;
        case Ecc::Unit::SM_RAMS:
            pErrCounts = &pSubdevDetailedEccCounts->smRamsCounts;
            break;
        case Ecc::Unit::HSHUB:
            pErrCounts = &pSubdevDetailedEccCounts->hshubCounts;
            break;
        case Ecc::Unit::PMU:
        case Ecc::Unit::PCIE_REORDER:
        case Ecc::Unit::PCIE_P2PREQ:
            MASSERT(!"GetEclwnitCountsStruct not suported for this ECC unit");
            break;
        default:
            Printf(Tee::PriError, "Enabled ECC unit (%s) is not supported\n",
                   Ecc::ToString(eclwnit));
            MASSERT(!"Enabled ECC unit is not supported");
    }

    return pErrCounts;
}

//--------------------------------------------------------------------
Ecc::ErrCounts * EccErrCounter::GetEccSingleUnitCounts
(
    SubdevDetailedEccCounts* pSubdevDetailedEccCounts
    ,Ecc::Unit eclwnit
) const
{
    Ecc::ErrCounts * pErrCounts = nullptr;

    switch (eclwnit)
    {
        case Ecc::Unit::DRAM:
        case Ecc::Unit::L2:
        case Ecc::Unit::L1:
        case Ecc::Unit::L1DATA:
        case Ecc::Unit::L1TAG:
        case Ecc::Unit::CBU:
        case Ecc::Unit::LRF:
        case Ecc::Unit::SHM:
        case Ecc::Unit::TEX:
        case Ecc::Unit::SM_ICACHE:
        case Ecc::Unit::GCC_L15:
        case Ecc::Unit::GPCMMU:
        case Ecc::Unit::HUBMMU_L2TLB:
        case Ecc::Unit::HUBMMU_HUBTLB:
        case Ecc::Unit::HUBMMU_FILLUNIT:
        case Ecc::Unit::GPCCS:
        case Ecc::Unit::FECS:
        case Ecc::Unit::SM_RAMS:
        case Ecc::Unit::HSHUB:
            MASSERT(!"GetEclwnitCounts must be used to retrieve errors for this ECC unit");
            break;

        case Ecc::Unit::PMU:
            pErrCounts = &pSubdevDetailedEccCounts->pmuCounts;
            break;

        case Ecc::Unit::PCIE_REORDER:
            pErrCounts = &pSubdevDetailedEccCounts->pciBusReorderCounts;
            break;

        case Ecc::Unit::PCIE_P2PREQ:
            pErrCounts = &pSubdevDetailedEccCounts->pciBusP2PCounts;
            break;

        default:
            Printf(Tee::PriError, "Enabled ECC unit (%s) is not supported\n",
                   Ecc::ToString(eclwnit));
            MASSERT(!"Enabled ECC unit is not supported");
    }

    return pErrCounts;
}

//--------------------------------------------------------------------
//!
//! \brief Extract the count associated with the given error type.
//!
UINT64 EccErrCounter::ExtractErrCount
(
    const Ecc::ErrCounts* pErrCounts
    ,Ecc::ErrorType errType
) const
{
    switch (errType)
    {
        case Ecc::ErrorType::CORR:
            return pErrCounts->corr;

        case Ecc::ErrorType::UNCORR:
            return pErrCounts->uncorr;

        default:
            Printf(Tee::PriWarn, "Unsupported ECC error type (%s)\n",
                   Ecc::ToString(errType));
            MASSERT(!"Unsupported ECC error type");
            return 0;
    }
}

//--------------------------------------------------------------------
//!
//! \see BeginExpectingErrors
//! \see EndExpectingErrors
//!
RC EccErrCounter::SetExpectingErrors
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errType
    ,bool isExpectingErrs
)
{
    RC rc;
    Tasker::MutexHolder lock(GetMutex());
    const UINT64 flagVal = (isExpectingErrs ? 1 : 0);
    const UINT32 idx = EccErrCounter::UnitErrCountIndex(eclwnit, errType);
    (void)idx;

    // Skip disabled units
    if (!m_EnabledEclwnits[static_cast<UINT32>(eclwnit)]) { return rc; }

    MASSERT(!!m_IsExpectingErrors[idx] != isExpectingErrs);
    CHECK_RC(Update());

    SubdevDetailedEccCounts subdevDetailedEccCounts;
    CHECK_RC(UnpackArray(&subdevDetailedEccCounts, &m_IsExpectingErrors[0]));

    if ((Ecc::GetUnitType(eclwnit) == Ecc::UnitType::FALCON) ||
        (Ecc::GetUnitType(eclwnit) == Ecc::UnitType::PCIE))
    {
        auto *pEccCount = GetEccSingleUnitCounts(&subdevDetailedEccCounts,
                                                       eclwnit);
        switch (errType)
        {
            case Ecc::ErrorType::CORR:
                pEccCount->corr = flagVal;
                break;

            case Ecc::ErrorType::UNCORR:
                pEccCount->uncorr = flagVal;
                break;

            default:
                Printf(Tee::PriError, "Unsupported ECC error type (%s)\n",
                       Ecc::ToString(errType));
                MASSERT(!"Unsupported ECC error type");
                return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        Ecc::DetailedErrCounts * pErrCounts = GetEclwnitCounts(&subdevDetailedEccCounts, eclwnit);
        MASSERT(pErrCounts);
        auto pDetailedEccCounts = &pErrCounts->eccCounts;

        const UINT32 dim1Size = static_cast<UINT32>(pDetailedEccCounts->size());
        MASSERT(dim1Size == pDetailedEccCounts->size()); // truncation check
        for (UINT32 i = 0; i < dim1Size; ++i)
        {
            const UINT32 dim2Size = static_cast<UINT32>((*pDetailedEccCounts)[i].size());
            MASSERT(dim2Size == (*pDetailedEccCounts)[i].size()); // truncation check
            for (UINT32 j = 0; j < dim2Size; ++j)
            {
                switch (errType)
                {
                    case Ecc::ErrorType::CORR:
                        (*pDetailedEccCounts)[i][j].corr = flagVal;
                        break;

                    case Ecc::ErrorType::UNCORR:
                        (*pDetailedEccCounts)[i][j].uncorr = flagVal;
                        break;

                    default:
                        Printf(Tee::PriError, "Unsupported ECC error type (%s)\n",
                               Ecc::ToString(errType));
                        MASSERT(!"Unsupported ECC error type");
                        return RC::SOFTWARE_ERROR;
                }
            }
        }
    }

    CHECK_RC(PackArray(subdevDetailedEccCounts, &m_IsExpectingErrors[0]));
    MASSERT(!!m_IsExpectingErrors[idx] == isExpectingErrs);

    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief Pack the given ECC unit locationcounts into the given array starting at the
//! given index.
//!
void EccErrCounter::PackEclwnitLocation
(
    Ecc::Unit eclwnit
    ,const Ecc::ErrCounts& counts
    ,UINT64* pArray
    ,UINT32* pIndex
) const
{
    pArray[(*pIndex)++] = counts.corr;
    pArray[UnitErrCountIndex(eclwnit, Ecc::ErrorType::CORR)] += counts.corr;
    pArray[(*pIndex)++] = counts.uncorr;
    pArray[UnitErrCountIndex(eclwnit, Ecc::ErrorType::UNCORR)] += counts.uncorr;
}

//--------------------------------------------------------------------
bool EccErrCounter::ReportCheckpointError
(
    Ecc::Unit      eclwnit,
    Ecc::ErrorType errType,
    UINT08         partGpc,
    UINT08         subpTpc,
    const Ecc::ErrCounts* pErrCounts
) const
{
    Tee::Priority pri = Tee::PriNormal;
    UINT64 count = ExtractErrCount(pErrCounts, errType);

    // If new error, report it
    if (count > 0)
    {
        const string eclwnitStr = Ecc::ToString(eclwnit);
        const string errTypeStr = Ecc::ToString(errType);
        string loc;
        Ecc::UnitType unitType;
        GetLocation(eclwnit, partGpc, subpTpc, &loc, &unitType);

        string errStr = Utility::StrPrintf("- %llu %s %s ERROR(s)",
                                           count, eclwnitStr.c_str(), errTypeStr.c_str());
        if ((unitType != Ecc::UnitType::FALCON) &&
            (unitType != Ecc::UnitType::PCIE))
        {
            errStr += Utility::StrPrintf(" in %s", loc.c_str());
        }

        Printf(pri, "%s\n", errStr.c_str());
        return true;
    }
    return false;
}

//--------------------------------------------------------------------
//!
//! \brief Override the base-class OnCheckpoint() method.
//!
/* virtual */ void EccErrCounter::OnCheckpoint
(
    const CheckpointData *pData
) const
{
    bool foundErrors = false;
    Tee::Priority pri = Tee::PriNormal;

    Printf(pri, "Cumulative ECC errors:\n");

    SubdevDetailedEccCounts subdevDetailedEccCounts;
    if (UnpackArray(&subdevDetailedEccCounts, pData->pCount) != OK)
    {
        Printf(Tee::PriWarn, "Unable to successfully unpack ECC counts\n");
        MASSERT(!"Unable to successfully unpack ECC counts");
        return;
    }

    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);
        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        Ecc::UnitType unitType = Ecc::GetUnitType(eclwnit);
        const vector<vector<Ecc::ErrCounts>> * pDetailedEccCounts = nullptr;
        if ((unitType != Ecc::UnitType::FALCON) &&
            (unitType != Ecc::UnitType::PCIE))
        {
            const Ecc::DetailedErrCounts * pErrCounts =
                GetEclwnitCounts(&subdevDetailedEccCounts, eclwnit);
            MASSERT(pErrCounts);
            pDetailedEccCounts = &pErrCounts->eccCounts;
        }

        for (UINT32 err = 0;
             err < static_cast<UINT32>(Ecc::ErrorType::MAX);
             ++err)
        {
            Ecc::ErrorType errType = static_cast<Ecc::ErrorType>(err);

            if ((unitType == Ecc::UnitType::FALCON) ||
                (unitType == Ecc::UnitType::PCIE))
            {
                MASSERT(pDetailedEccCounts == nullptr);
                const auto *pEccCount = GetEccSingleUnitCounts(&subdevDetailedEccCounts,
                                                               eclwnit);
                if (ReportCheckpointError(eclwnit, errType, pEccCount))
                {
                    foundErrors = true;
                }
                continue;
            }

            const UINT32 dim1Size = static_cast<UINT32>(pDetailedEccCounts->size());
            MASSERT(dim1Size == pDetailedEccCounts->size()); // truncation check
            for (UINT32 i = 0; i < dim1Size; ++i)
            {
                const UINT32 dim2Size = static_cast<UINT32>((*pDetailedEccCounts)[i].size());
                MASSERT(dim2Size == (*pDetailedEccCounts)[i].size()); // truncation check
                for (UINT32 j = 0; j < dim2Size; ++j)
                {
                    const auto& eccCount = (*pDetailedEccCounts)[i][j];
                    if (ReportCheckpointError(eclwnit, errType, i, j, &eccCount))
                    {
                        foundErrors = true;
                    }
                }
            }
        }
    }

    if (!foundErrors)
    {
        Printf(Tee::PriNormal, "- <none>\n");
    }
}

//--------------------------------------------------------------------
//!
//! \brief Copy the ECC error counts from data structs to an array
//!
//! Take the ECC error counts in the fbErrors and grErrors structs,
//! and copy them into a 1-dimensinal array suitable for
//! ReadErrorCount().
//!
//! The first EccErrCounter::s_NumEccErrTypes elements of the array are the
//! "visible" errors that have thresholds applied by the base class.
//!
//! \see UnpackArray()
//! \see GetPackedArraySize()
//!
RC EccErrCounter::PackArray
(
    const SubdevDetailedEccCounts& subdevDetailedEccCounts
    ,UINT64 *pArray
) const
{
    MASSERT(pArray);
    RC rc;

    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const FrameBuffer *pFb = pGpuSubdevice->GetFB();
    const UINT32 numFbios           = pFb->GetFbioCount();
    const UINT32 numChannelsPerFbio = pFb->GetChannelsPerFbio();
#ifdef DEBUG
    const UINT32 numLtcs          = pFb->GetLtcCount();
    const UINT32 maxSlices        = pFb->GetMaxSlicesPerLtc();
    const UINT32 numGpcs          = pGpuSubdevice->GetGpcCount();
#endif

    // TODO : Hopper will be adding floorsweeping of HSHUB sot that will need
    // to be accounted for here (use a different call than "Max")
    const UINT32 numHsHubs = pGpuSubdevice->GetMaxHsHubCount();
    UINT32 index = EccErrCounter::s_NumEccErrTypes;

    for (UINT32 i = 0; i < EccErrCounter::s_NumEccErrTypes; ++i)
    {
        pArray[i] = 0;
    }

    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);
        const vector<vector<Ecc::ErrCounts>>* pGpcErrCounts = nullptr;

        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        switch (eclwnit)
        {
            case Ecc::Unit::DRAM:
            {
                const auto& fbCounts = subdevDetailedEccCounts.fbCounts;
                MASSERT(fbCounts.eccCounts.size() == numFbios);

                for (UINT32 virtualFbio = 0; virtualFbio < numFbios; ++virtualFbio)
                {
                    MASSERT(fbCounts.eccCounts[virtualFbio].size()
                            == numChannelsPerFbio);
                    for (UINT32 sublocation = 0; sublocation < numChannelsPerFbio; ++sublocation)
                    {
                        const auto& eccCount = fbCounts.eccCounts[virtualFbio][sublocation];
                        PackEclwnitLocation(eclwnit, eccCount, pArray, &index);
                    }
                }
            } break;

            case Ecc::Unit::L2:
            {
                const auto& l2Counts = subdevDetailedEccCounts.l2Counts;
                MASSERT(l2Counts.eccCounts.size() == numLtcs);

                for (UINT32 virtualLtc = 0; virtualLtc < l2Counts.eccCounts.size(); ++virtualLtc)
                {
                    MASSERT(l2Counts.eccCounts[virtualLtc].size() == maxSlices);
                    for (UINT32 slice = 0; slice < l2Counts.eccCounts[virtualLtc].size(); ++slice)
                    {
                        const auto& eccCount = l2Counts.eccCounts[virtualLtc][slice];
                        PackEclwnitLocation(eclwnit, eccCount, pArray, &index);
                    }
                }
            } break;

            case Ecc::Unit::L1:
                pGpcErrCounts = &subdevDetailedEccCounts.l1Counts.eccCounts;
                break;

            case Ecc::Unit::L1DATA:
                pGpcErrCounts = &subdevDetailedEccCounts.l1DataCounts.eccCounts;
                break;

            case Ecc::Unit::L1TAG:
                pGpcErrCounts = &subdevDetailedEccCounts.l1TagCounts.eccCounts;
                break;

            case Ecc::Unit::CBU:
                pGpcErrCounts = &subdevDetailedEccCounts.cbuCounts.eccCounts;
                break;

            case Ecc::Unit::LRF:
                pGpcErrCounts = &subdevDetailedEccCounts.smCounts.eccCounts;
                break;

            case Ecc::Unit::SHM:
                pGpcErrCounts = &subdevDetailedEccCounts.shmCounts.eccCounts;
                break;

            case Ecc::Unit::TEX:
                pGpcErrCounts = &subdevDetailedEccCounts.texCounts.eccCounts;
                break;

            case Ecc::Unit::SM_ICACHE:
                pGpcErrCounts = &subdevDetailedEccCounts.smIcacheCounts.eccCounts;
                break;

            case Ecc::Unit::GCC_L15:
                pGpcErrCounts = &subdevDetailedEccCounts.gccL15Counts.eccCounts;
                break;

            case Ecc::Unit::GPCMMU:
                pGpcErrCounts = &subdevDetailedEccCounts.gpcMmuCounts.eccCounts;
                break;

            case Ecc::Unit::HUBMMU_L2TLB:
                pGpcErrCounts = &subdevDetailedEccCounts.hubMmuL2TlbCounts.eccCounts;
                break;

            case Ecc::Unit::HUBMMU_HUBTLB:
                pGpcErrCounts = &subdevDetailedEccCounts.hubMmuHubTlbCounts.eccCounts;
                break;

            case Ecc::Unit::HUBMMU_FILLUNIT:
                pGpcErrCounts = &subdevDetailedEccCounts.hubMmuFillUnitCounts.eccCounts;
                break;

            case Ecc::Unit::GPCCS:
                pGpcErrCounts = &subdevDetailedEccCounts.gpccsCounts.eccCounts;
                break;

            case Ecc::Unit::FECS:
                pGpcErrCounts = &subdevDetailedEccCounts.fecsCounts.eccCounts;
                break;

            case Ecc::Unit::SM_RAMS:
                pGpcErrCounts = &subdevDetailedEccCounts.smRamsCounts.eccCounts;
                break;

            case Ecc::Unit::HSHUB:
            {
                const auto& hshubCounts = subdevDetailedEccCounts.hshubCounts;
                MASSERT(hshubCounts.eccCounts.size() == numHsHubs);

                for (UINT32 hshub = 0; hshub < numHsHubs; ++hshub)
                {
                    MASSERT(hshubCounts.eccCounts[hshub].size() == LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT);
                    for (UINT32 sublocation = 0; sublocation < LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT; ++sublocation)
                    {
                        const auto& eccCount = hshubCounts.eccCounts[hshub][sublocation];
                        PackEclwnitLocation(eclwnit, eccCount, pArray, &index);
                    }
                }
            } break;

            case Ecc::Unit::PMU:
                {
                    auto& pmuCounts = subdevDetailedEccCounts.pmuCounts;
                    PackEclwnitLocation(eclwnit, pmuCounts, pArray, &index);
                }
                break;

            case Ecc::Unit::PCIE_REORDER:
                {
                    auto& pciBusReorderCounts = subdevDetailedEccCounts.pciBusReorderCounts;
                    PackEclwnitLocation(eclwnit, pciBusReorderCounts, pArray, &index);
                }
                break;

            // The cases in this switch statement exhaust all possible values of this
            // enum, so having a default to catch new/missed enum values would trigger
            // Coverity due to unreachable code.  Handle the last case with a default
            // to avoid this and double-check if we've hit the correct value.
            default:
                if (eclwnit == Ecc::Unit::PCIE_P2PREQ)
                {
                    auto& pciBusP2PCounts = subdevDetailedEccCounts.pciBusP2PCounts;
                    PackEclwnitLocation(eclwnit, pciBusP2PCounts, pArray, &index);
                }
                else
                {
                    Printf(Tee::PriError, "Enabled ECC unit (%s) is not supported\n",
                           Ecc::ToString(eclwnit));
                    MASSERT(eclwnit == Ecc::Unit::PCIE_P2PREQ);
                    return RC::SOFTWARE_ERROR;
                }
                break;
        }

        // GPC based ECC unit
        if (pGpcErrCounts)
        {
            MASSERT(pGpcErrCounts->size() == numGpcs);
            for (UINT32 gpc = 0; gpc < pGpcErrCounts->size(); ++gpc)
            {
#ifdef DEBUG
                UINT32 numSublocations = pGpuSubdevice->GetTpcCountOnGpc(gpc);
                if (eclwnit == Ecc::Unit::GPCMMU)
                {
                    numSublocations = pGpuSubdevice->GetMmuCountPerGpc(gpc);
                }
                MASSERT((*pGpcErrCounts)[gpc].size() == numSublocations);
#endif

                for (UINT32 sublocation = 0; sublocation < (*pGpcErrCounts)[gpc].size(); ++sublocation)
                {
                    const auto& eccCount = (*pGpcErrCounts)[gpc][sublocation];
                    PackEclwnitLocation(eclwnit, eccCount, pArray, &index);
                }
            }
        }
    }

    MASSERT(index == GetPackedArraySize());

    return rc;
}

//--------------------------------------------------------------------
//!
//! \brief Copy the ECC error counts from an array to data structs
//!
//! Take an array generated by PackArray(), and colwert back into data
//! structures.
//!
//! \see PackArray()
//! \see GetPackedArraySize()
//!
RC EccErrCounter::UnpackArray
(
    SubdevDetailedEccCounts* pSubdevDetailedEccCounts
    ,const UINT64 *pArray
) const
{
    MASSERT(pSubdevDetailedEccCounts);
    MASSERT(pArray);
    RC rc;

    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const FrameBuffer *pFb = pGpuSubdevice->GetFB();
    const UINT32 numFbios           = pFb->GetFbioCount();
    const UINT32 numChannelsPerFbio = pFb->GetChannelsPerFbio();
    const UINT32 numLtcs            = pFb->GetLtcCount();
    const UINT32 maxSlices          = pFb->GetMaxSlicesPerLtc();
    const UINT32 numGpcs            = pGpuSubdevice->GetGpcCount();

    // TODO : Hopper will be adding floorsweeping of HSHUB sot that will need
    // to be accounted for here (use a different call than "Max")
    const UINT32 numHsHubs = pGpuSubdevice->GetMaxHsHubCount();
    UINT32 index = EccErrCounter::s_NumEccErrTypes;

    pSubdevDetailedEccCounts->Clear();

    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);
        vector<vector<Ecc::ErrCounts>>* pGpcErrCounts = nullptr;

        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        switch (eclwnit)
        {
            case Ecc::Unit::DRAM:
            {
                auto& fbCounts = pSubdevDetailedEccCounts->fbCounts;

                fbCounts.eccCounts.resize(numFbios);
                for (UINT32 virtualFbio = 0; virtualFbio < numFbios; ++virtualFbio)
                {
                    fbCounts.eccCounts[virtualFbio].resize(numChannelsPerFbio);
                    for (UINT32 sublocation = 0; sublocation < numChannelsPerFbio; ++sublocation)
                    {
                        fbCounts.eccCounts[virtualFbio][sublocation].corr = pArray[index++];
                        fbCounts.eccCounts[virtualFbio][sublocation].uncorr = pArray[index++];
                    }
                }
            } break;

            case Ecc::Unit::L2:
            {
                auto& l2Counts = pSubdevDetailedEccCounts->l2Counts;

                l2Counts.eccCounts.resize(numLtcs);
                for (UINT32 virtualLtc = 0; virtualLtc < numLtcs; ++virtualLtc)
                {
                    l2Counts.eccCounts[virtualLtc].resize(maxSlices);
                    for (UINT32 slice = 0; slice < maxSlices; ++slice)
                    {
                        l2Counts.eccCounts[virtualLtc][slice].corr = pArray[index++];
                        l2Counts.eccCounts[virtualLtc][slice].uncorr = pArray[index++];
                    }
                }
            } break;

            case Ecc::Unit::L1:
                pGpcErrCounts = &pSubdevDetailedEccCounts->l1Counts.eccCounts;
                break;

            case Ecc::Unit::L1DATA:
                pGpcErrCounts = &pSubdevDetailedEccCounts->l1DataCounts.eccCounts;
                break;

            case Ecc::Unit::L1TAG:
                pGpcErrCounts = &pSubdevDetailedEccCounts->l1TagCounts.eccCounts;
                break;

            case Ecc::Unit::CBU:
                pGpcErrCounts = &pSubdevDetailedEccCounts->cbuCounts.eccCounts;
                break;

            case Ecc::Unit::LRF:
                pGpcErrCounts = &pSubdevDetailedEccCounts->smCounts.eccCounts;
                break;

            case Ecc::Unit::SHM:
                pGpcErrCounts = &pSubdevDetailedEccCounts->shmCounts.eccCounts;
                break;

            case Ecc::Unit::TEX:
                pGpcErrCounts = &pSubdevDetailedEccCounts->texCounts.eccCounts;
                break;

            case Ecc::Unit::SM_ICACHE:
                pGpcErrCounts = &pSubdevDetailedEccCounts->smIcacheCounts.eccCounts;
                break;

            case Ecc::Unit::GCC_L15:
                pGpcErrCounts = &pSubdevDetailedEccCounts->gccL15Counts.eccCounts;
                break;

            case Ecc::Unit::GPCMMU:
                pGpcErrCounts = &pSubdevDetailedEccCounts->gpcMmuCounts.eccCounts;
                break;

            case Ecc::Unit::HUBMMU_L2TLB:
                pGpcErrCounts = &pSubdevDetailedEccCounts->hubMmuL2TlbCounts.eccCounts;
                break;

            case Ecc::Unit::HUBMMU_HUBTLB:
                pGpcErrCounts = &pSubdevDetailedEccCounts->hubMmuHubTlbCounts.eccCounts;
                break;

            case Ecc::Unit::HUBMMU_FILLUNIT:
                pGpcErrCounts = &pSubdevDetailedEccCounts->hubMmuFillUnitCounts.eccCounts;
                break;

            case Ecc::Unit::GPCCS:
                pGpcErrCounts = &pSubdevDetailedEccCounts->gpccsCounts.eccCounts;
                break;

            case Ecc::Unit::FECS:
                pGpcErrCounts = &pSubdevDetailedEccCounts->fecsCounts.eccCounts;
                break;

            case Ecc::Unit::SM_RAMS:
                pGpcErrCounts = &pSubdevDetailedEccCounts->smRamsCounts.eccCounts;
                break;

            case Ecc::Unit::PMU:
                {
                    auto& pmuCounts = pSubdevDetailedEccCounts->pmuCounts;
                    pmuCounts.corr   = pArray[index++];
                    pmuCounts.uncorr = pArray[index++];
                }
                break;

            case Ecc::Unit::HSHUB:
            {
                auto& hshubCounts = pSubdevDetailedEccCounts->hshubCounts;

                hshubCounts.eccCounts.resize(numHsHubs);
                for (UINT32 hshub = 0; hshub < numHsHubs; ++hshub)
                {
                    hshubCounts.eccCounts[hshub].resize(LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT);
                    for (UINT32 sublocation = 0; sublocation < LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT; ++sublocation)
                    {
                        hshubCounts.eccCounts[hshub][sublocation].corr = pArray[index++];
                        hshubCounts.eccCounts[hshub][sublocation].uncorr = pArray[index++];
                    }
                }
            } break;

            case Ecc::Unit::PCIE_REORDER:
            {
                auto& pciBusReorderCounts = pSubdevDetailedEccCounts->pciBusReorderCounts;
                pciBusReorderCounts.corr   = pArray[index++];
                pciBusReorderCounts.uncorr = pArray[index++];
            }
            break;

            case Ecc::Unit::PCIE_P2PREQ:
            {
                auto& pciBusP2PCounts = pSubdevDetailedEccCounts->pciBusP2PCounts;
                pciBusP2PCounts.corr   = pArray[index++];
                pciBusP2PCounts.uncorr = pArray[index++];
            }
            break;

            default:
                Printf(Tee::PriError, "Enabled ECC unit (%s) is not supported\n",
                       Ecc::ToString(eclwnit));
                MASSERT(!"Enabled ECC unit is not supported");
                return RC::SOFTWARE_ERROR;
        }

        // GPC based ECC unit
        if (pGpcErrCounts)
        {
            pGpcErrCounts->resize(numGpcs);
            for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
            {
                UINT32 numSublocations = pGpuSubdevice->GetTpcCountOnGpc(gpc);
                if (eclwnit == Ecc::Unit::GPCMMU)
                {
                    numSublocations = pGpuSubdevice->GetMmuCountPerGpc(gpc);
                }
                (*pGpcErrCounts)[gpc].resize(numSublocations);
                for (UINT32 sublocation = 0; sublocation < numSublocations; ++sublocation)
                {
                    (*pGpcErrCounts)[gpc][sublocation].corr = pArray[index++];
                    (*pGpcErrCounts)[gpc][sublocation].uncorr = pArray[index++];
                }
            }
        }
    }

    MASSERT(index == GetPackedArraySize());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the array-size used by PackArray() and UnpackArray()
//!
//! \sa PackArray()
//! \sa UnpackArray()
//!
UINT32 EccErrCounter::GetPackedArraySize() const
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const FrameBuffer *pFb = pGpuSubdevice->GetFB();
    const UINT32 numFbios           = pFb->GetFbioCount();
    const UINT32 numChannelsPerFbio = pFb->GetChannelsPerFbio();
    const UINT32 numLtcs            = pFb->GetLtcCount();
    const UINT32 maxSlices          = pFb->GetMaxSlicesPerLtc();
    const UINT32 numGpcs            = pGpuSubdevice->GetGpcCount();
    // TODO : Hopper will be adding floorsweeping of HSHUB so that will need
    // to be accounted for here (use a different call than "Max")
    const UINT32 numHsHubs = pGpuSubdevice->GetMaxHsHubCount();
    UINT32 arraySize = 0;

    // Total counts position on the array: one entry for each ECC error type
    //
    arraySize += EccErrCounter::s_NumEccErrTypes;

    UINT32 totalTpcsOnGpu = 0;
    for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
    {
        totalTpcsOnGpu += pGpuSubdevice->GetTpcCountOnGpc(gpc);
    }

    // Location specific counts portion of the array
    //
    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);

        // Skip disabled units
        if (!m_EnabledEclwnits[unit]) { continue; }

        switch (eclwnit)
        {
            case Ecc::Unit::DRAM:
                arraySize += 2 * numFbios * numChannelsPerFbio;
                break;

            case Ecc::Unit::L2:
                arraySize += 2 * numLtcs * maxSlices;
                break;

            case Ecc::Unit::GPCMMU:
                for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
                {
                    arraySize += (2 * pGpuSubdevice->GetMmuCountPerGpc(gpc));
                }
                break;

            case Ecc::Unit::L1:
            case Ecc::Unit::L1DATA:
            case Ecc::Unit::L1TAG:
            case Ecc::Unit::CBU:
            case Ecc::Unit::LRF:
            case Ecc::Unit::SHM:
            case Ecc::Unit::TEX:
            case Ecc::Unit::SM_ICACHE:
            case Ecc::Unit::GCC_L15:
            case Ecc::Unit::HUBMMU_L2TLB:
            case Ecc::Unit::HUBMMU_HUBTLB:
            case Ecc::Unit::HUBMMU_FILLUNIT:
            case Ecc::Unit::GPCCS:
            case Ecc::Unit::FECS:
            case Ecc::Unit::SM_RAMS:
                arraySize += 2 * totalTpcsOnGpu;
                break;

            case Ecc::Unit::HSHUB:
                arraySize += 2 * numHsHubs * LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT;
                break;

            case Ecc::Unit::PMU:
            case Ecc::Unit::PCIE_REORDER:
            case Ecc::Unit::PCIE_P2PREQ:
                arraySize += 2;
                break;

            default:
                Printf(Tee::PriError, "Enabled ECC unit (%s) is not supported\n",
                       Ecc::ToString(eclwnit));
                MASSERT(!"Enabled ECC unit is not supported");
                return 0;
        }
    }

    return arraySize;
}

//--------------------------------------------------------------------
//!
//! \brief Return location information for the given ECC unit.
//!
//! \param eclwnit ECC unit.
//! \param i First index into the ECC unit (for example, GPC for LRF).
//! \param j Second index into the ECC unit (for example, TPC for LRF).
//! \param[out] pLocStr Location as a string.
//! \param[out] pUnitType location type of the error (whether it oclwrs in FB,
//!             GPC or no specific location)
//!
void EccErrCounter::GetLocation
(
    Ecc::Unit eclwnit
    ,UINT32 i
    ,UINT32 j
    ,string* pLocStr
    ,Ecc::UnitType* pUnitType
) const
{
    MASSERT(pLocStr);
    const Ecc::UnitType unitType      = Ecc::GetUnitType(eclwnit);
    const GpuSubdevice* pGpuSubdevice = GetGpuSubdevice();
    const bool          multiGpu      = DevMgrMgr::d_GraphDevMgr->NumGpus() > 1;
    const FrameBuffer*  pFb           = pGpuSubdevice->GetFB();

    switch (unitType)
    {
        case Ecc::UnitType::FB:
            *pLocStr = Utility::StrPrintf(
                    "partition %c, subpartition %d",
                    pFb->VirtualFbioToLetter(i),
                    j);

            // Starting GH100, ECC FB errors for HBM devices are based on per-channel.
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (pGpuSubdevice->DeviceId() >= Gpu::GH100 && pFb->IsHbm())
            {
                UINT32 hbmSite = 0;
                UINT32 hbmChannel = 0;
                if (pFb->IsHbm3())
                {
                    // Colwert 'j' (sublocation) to subpartition/pseudoChannel
                    // Bit 1 is subpartition
                    // Bit 0 is channel within subp. Colwert to pseudochannel
                    // Either (ch * 2) or (ch * 2 + 1) is fine
                    const UINT32 subp = (j & BIT(1)) >> 1;
                    const UINT32 pc = (j & BIT(0)) * 2;

                    pFb->FbioSubpToHbmSiteChannel(pFb->VirtualFbioToHwFbio(i),
                                                  subp,
                                                  pc,
                                                  &hbmSite,
                                                  &hbmChannel);
                }
                // HBM2e, HBM1
                else
                {
                    const UINT32 subp = (j & BIT(0));
                    pFb->FbioSubpToHbmSiteChannel(pFb->VirtualFbioToHwFbio(i),
                                                  subp,
                                                  &hbmSite,
                                                  &hbmChannel);
                }

                *pLocStr += Utility::StrPrintf(", channel %c", 'a' + hbmChannel);
            }
#endif
            if (multiGpu)
            {
                *pLocStr += ", " + pGpuSubdevice->GetName();
            }
            break;
        case Ecc::UnitType::L2:
            {
                *pLocStr = Utility::StrPrintf("ltc %u, slice %u",
                                              pFb->VirtualLtcToHwLtc(i),
                                              pFb->VirtualL2SliceToHwL2Slice(i, j));

                if (multiGpu)
                {
                    *pLocStr += ", " + pGpuSubdevice->GetName();
                }
            }
            break;

        case Ecc::UnitType::FALCON:
        case Ecc::UnitType::PCIE:
            *pLocStr = multiGpu ?  pGpuSubdevice->GetName() : "";
            break;

        case Ecc::UnitType::HSHUB:
            *pLocStr = Utility::StrPrintf("HSHUB %u %s", i, Ecc::HsHubSublocationString(j));
            if (multiGpu)
            {
                *pLocStr += ", " + pGpuSubdevice->GetName();
            }
            break;

        default:
            if (eclwnit == Ecc::Unit::GPCMMU)
            {
                UINT32 hwGpc;
                RC rc;
                rc = GetGpuSubdevice()->VirtualGpcToHwGpc(i, &hwGpc);
                MASSERT(rc == OK);
                if (rc != OK)
                    *pLocStr = "<unknown>";
                else
                {
                    *pLocStr = Utility::StrPrintf("GPC %c, MMU %d", hwGpc, j);
                    if (multiGpu)
                    {
                        *pLocStr += ", " + pGpuSubdevice->GetName();
                    }
                }
            }
            else
            {
                UINT32 hwGpc;
                UINT32 hwTpc;
                if (pGpuSubdevice->VirtualGpcToHwGpc(i, &hwGpc) == RC::OK &&
                    pGpuSubdevice->VirtualTpcToHwTpc(hwGpc, j,
                                                     &hwTpc) == RC::OK)
                {
                    *pLocStr = Utility::StrPrintf("GPC %d, TPC %d",
                                                  hwGpc, hwTpc);
                    if (multiGpu)
                    {
                        *pLocStr += ", " + pGpuSubdevice->GetName();
                    }
                }
                else
                {
                    MASSERT(!"Failed to colwert GPC or TPC");
                    *pLocStr = "<unknown>";
                }
            }
            break;
    }

    if (pUnitType) { *pUnitType = unitType; }
}

//--------------------------------------------------------------------
void EccErrCounter::PrintNewErrCountsToMle
(
    const char* location,
    const char* type,
    UINT08 virtPartGpc,
    UINT08 virtSublocTpc,
    UINT64 count,
    Ecc::UnitType unitType,
    Ecc::Unit eclwnit,
    Ecc::ErrorType errType
) const
{
    UINT08 hwPartGpc = 0;
    UINT08 hwSublocTpc = 0;

    const GpuSubdevice* pSubdev = GetGpuSubdevice();
    FrameBuffer *pFb = pSubdev->GetFB();

    switch (unitType)
    {
        case Ecc::UnitType::FB:
            {
                hwPartGpc = static_cast<UINT08>(pFb->VirtualFbioToHwFbio(virtPartGpc));
                hwSublocTpc = virtSublocTpc;
            }
            break;
        case Ecc::UnitType::L2:
            {
                hwPartGpc = static_cast<UINT08>(pFb->VirtualLtcToHwLtc(virtPartGpc));
                hwSublocTpc = virtSublocTpc;
            }
            break;
        case Ecc::UnitType::GPC:
            {
                UINT32 temp;
                pSubdev->VirtualGpcToHwGpc(virtPartGpc, &temp);
                hwPartGpc = static_cast<UINT08>(temp);
                if (eclwnit == Ecc::Unit::GPCMMU)
                {
                    hwSublocTpc = virtSublocTpc;
                }
                else
                {
                    pSubdev->VirtualTpcToHwTpc(hwPartGpc, virtSublocTpc, &temp);
                    hwSublocTpc = static_cast<UINT08>(temp);
                }
            }
            break;
        case Ecc::UnitType::FALCON:
        case Ecc::UnitType::HSHUB:
        case Ecc::UnitType::PCIE:
            break;
        default:
            MASSERT(!"Unknown ECC unit type");
            break;
    }

    RC errRc = Ecc::GetRC(eclwnit, errType);

    Mle::Print(Mle::Entry::mem_err_ctr)
        .location(location)
        .type(type)
        .error_src(Mle::MemErrCtr::ecc)
        .part_gpc(hwPartGpc)
        .subp_tpc(hwSublocTpc)
        .count(static_cast<UINT32>(count))
        .rc(static_cast<UINT32>(errRc));
}

//--------------------------------------------------------------------
void EccErrCounter::SubdevDetailedEccCounts::Clear()
{
    fbCounts.eccCounts.clear();
    l2Counts.eccCounts.clear();
    l1Counts.eccCounts.clear();
    l1DataCounts.eccCounts.clear();
    l1TagCounts.eccCounts.clear();
    cbuCounts.eccCounts.clear();
    smCounts.eccCounts.clear();
    shmCounts.eccCounts.clear();
    texCounts.eccCounts.clear();
    smIcacheCounts.eccCounts.clear();
    gccL15Counts.eccCounts.clear();
    gpcMmuCounts.eccCounts.clear();
    hubMmuL2TlbCounts.eccCounts.clear();
    hubMmuHubTlbCounts.eccCounts.clear();
    hubMmuFillUnitCounts.eccCounts.clear();
    gpccsCounts.eccCounts.clear();
    fecsCounts.eccCounts.clear();
    smRamsCounts.eccCounts.clear();
    hshubCounts.eccCounts.clear();
    pmuCounts = { };
    pciBusP2PCounts = { };
    pciBusReorderCounts = { };
}

//------------------------------------------------------------------------------
JS_SMETHOD_LWSTOM
(
    EccErrCounter,
    UnitErrCountIndex,
    2,
    "Colwerts Ecc::Unit and Ecc::ErrorType to an index into the counts array"
)
{
    STEST_HEADER(2, 2,
                 "EccErrCounter.UnitErrCountIndex(EccConst.UNIT_DRAM, "
                 "EccConst.ERR_CORR)\n");

    STEST_ARG(0, UINT32, jsEclwnit);
    STEST_ARG(1, UINT32, jsErrType);

    const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(jsEclwnit);
    const Ecc::ErrorType errType = static_cast<Ecc::ErrorType>(jsErrType);

    jsval jsVal;
    if (pJavaScript->ToJsval(EccErrCounter::UnitErrCountIndex(eclwnit, errType),
                             &jsVal) != OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    *pReturlwalue = jsVal;
    return JS_TRUE;
}
