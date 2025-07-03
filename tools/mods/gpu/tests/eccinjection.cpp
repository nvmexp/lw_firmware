/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file eccinjection.cpp
//! \brief Checks ECC unit interrupt lines via HW ECC error injections.
//!        This test checks all ECC units with HW ECC error injection support.
//!

#include "class/cl0005.h"          // LW0005_ALLOC_PARAMETERS
#include "class/cl90e6.h"          // GF100_SUBDEVICE_MASTER
#include "ctrl/ctrl208f.h"         // LW208F_CTRL_CMD_*_ECC_*
#include "ctrl/ctrl90e6.h"
#include "core/include/abort.h"
#include "core/include/jscript.h"
#include "core/include/lwrm.h"     // Handle
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tasker.h"   // Mutex, MutexHolder
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rmtest.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/ecccount.h"
#include "gpu/utility/eclwtil.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{
    struct LwRmControlParams
    {
        LwRm::Handle hObject;
        UINT32 cmd;
        void* pParams;
        UINT32 paramsSize;
    };

    static constexpr UINT32 s_IndentDepth = 4;
}

//!
//! \brief Test ECC interrupt lines using HW ECC error injection.
//!
//! Runs background test to have active GR context. GPC ECC units require a
//! valid GR context running in the GPC when they fire an interrupt, otherwise
//! RM will fire a DBG_BREAPOINT. This is an RM sanity check, because, in
//! theory, there should not be an ECC interrupt from a GPC that is not
//! active. HW fake injection breaks this assumption.
//!
class EccErrorInjectionTest : public GpuTest
{
public:

    EccErrorInjectionTest();
    virtual ~EccErrorInjectionTest();
    virtual bool IsSupported() override;
    virtual RC Setup() override;
    virtual RC Run() override;
    virtual RC Cleanup() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(IntrWaitTimeMs, FLOAT64);
    SETGET_PROP(IntrCheckDelayMs, FLOAT64);
    SETGET_PROP(IntrTimeoutMs, FLOAT64);
    SETGET_PROP(ActiveGrContextWaitMs, FLOAT64);
    SETGET_PROP(RetriesOnFailure, UINT32);
    SETGET_PROP(EccTestMask, UINT32);

private:

    //!
    //! \brief Parameters passed to the RM notifier callback.
    //!
    struct EccEventParams
    {
        void* pRwMutex;                   //!< Mutex to lock during r/w

        Ecc::Unit        eclwnit;         //!< [in] Injected ECC unit
        Ecc::ErrorType   errorType;       //!< [in] Injected error type
        GpuSubdevice *   pSubdev;         //!< [in] Injected subdevice

        bool eccEventOclwrred;            //!< [out] True if ECC event oclwrred
    };

    //!
    //! \brief Context for a single interrupt line failure.
    //!
    struct FailRecord
    {
        RC rc;                    //!< RC for failure
        Ecc::Unit eclwnit;        //!< ECC unit
        Ecc::ErrorType errorType; //!< Error type
        string detailedLocDesc;   //!< Detailed location information
        UINT32 attempts;          //!< Number of injection attempts

        FailRecord
        (
            RC rc
            ,const EccEventParams& eventParams
            ,const string& detailedLocDesc
            ,UINT32 attempts
        );

        static string TableHeader(UINT32 indentationLevel);
        static bool TableSort(const FailRecord& a, const FailRecord& b);
        string TableRow(UINT32 indentationLevel) const;
    };


    //! Recommended time to wait for the background test to fully populate the
    //! GPU with an active GR context.
    static constexpr FLOAT32 s_RecommendedGrCtxWaitMs = 1500.0f;

    //! Delay at the start and end of the test so pending interrupts can be serviced
    FLOAT64 m_IntrWaitTimeMs        = 1000.0f;
    //! Minimum time to wait for interrupt to be serviced.
    FLOAT64 m_IntrCheckDelayMs      = 1.0f;
    //! Time to wait for ECC interrupt event to occur.
    FLOAT64 m_IntrTimeoutMs         = m_IntrCheckDelayMs;
    //! Time to wait in ms until the background test GR context is fully active.
    FLOAT64 m_ActiveGrContextWaitMs = s_RecommendedGrCtxWaitMs;
    //! Number of times to retry an injection after a failure.
    UINT32  m_RetriesOnFailure      = 0;
    //! A mask of ECC units to test, default is all available units.
    UINT32  m_EccTestMask           = ~0;
    
    Tasker::Mutex m_EventParamsMutex = Tasker::NoMutex();
    LwRm::Handle  m_hSubdevDiag      = 0;

    EccErrCounter*                  m_pEccErrCounter = nullptr;
    bool                            m_bShouldRestoreNotifiers = false;
    EccEventParams                  m_EccEventParams = {};
    bool                            m_bHookedCorrectable = false;
    bool                            m_bHookedUncorrectable = false;
    vector<FailRecord>              m_Failures;
    vector<FailRecord>              m_MarginalFailures;
    Ecc::UnitList<bool>             m_EnabledEclwnits;

    static void CorrectableEvent(void* pArg);
    static void UncorrectableEvent(void* pArg);

    RC CheckTestArgs();
    RC CheckEnabledEclwnits(Ecc::UnitList<bool>* pEnabledUnits);
    RC CheckIntr();
    RC DoInjection
    (
        const LwRmControlParams& rmCtrlParams,
        const string& injectDesc,
        const string* pDetailedLocStr,
        UINT64* pTotalInjections
    );
    RC InjectErrors(Ecc::Unit eclwnit, UINT64* pTotalInjections);
    RC InjectErrorsIntoGr(Ecc::Unit eclwnit, Ecc::ErrorType errorType, UINT64* pTotalInjections);
    RC InjectErrorsIntoFb(Ecc::Unit eclwnit, Ecc::ErrorType errorType, UINT64* pTotalInjections);
    RC InjectErrorsIntoMmu(Ecc::Unit eclwnit, Ecc::ErrorType errorType, UINT64* pTotalInjections);
    RC InjectErrorsIntoPciBus(Ecc::Unit eclwnit, Ecc::ErrorType errorType, UINT64* pTotalInjections); //$
    RC StartExpectingErrors();
    RC StopExpectingErrors();
    RC ReportFailures(UINT64 totalInjections);
};

//------------------------------------------------------------------------------
EccErrorInjectionTest::FailRecord::FailRecord
(
    RC rc
    ,const EccEventParams& eventParams
    ,const string& detailedLocDesc
    ,UINT32 attempts
)
    : rc(rc)
    , eclwnit(eventParams.eclwnit)
    , errorType(eventParams.errorType)
    , detailedLocDesc(detailedLocDesc)
    , attempts(attempts)
{}

/*static*/
string EccErrorInjectionTest::FailRecord::TableHeader(UINT32 indentationLevel)
{
    using Utility::StrPrintf;
    const UINT32 numSpaces = indentationLevel * s_IndentDepth;
    return StrPrintf("%*sECC Unit        | Location                       | Inject  | Attempts | RC  | Failure\n"
                     "%*s----------------+--------------------------------+---------+----------+-----+---------\n",
                     numSpaces, " ", numSpaces, " ");
}

/*static*/
bool EccErrorInjectionTest::FailRecord::TableSort(const FailRecord& a, const FailRecord& b)
{
    // Sort by:
    //   1) ECC unit
    if (a.eclwnit < b.eclwnit) { return true; }
    if (a.eclwnit > b.eclwnit) { return false; }

    //   2) detailed location
    INT32 cmp = a.detailedLocDesc.compare(b.detailedLocDesc);
    if (cmp != 0)
    {
        return cmp < 0;
    }

    //   3) error type
    if (a.errorType < b.errorType) { return true; }
    if (a.errorType > b.errorType) { return false; }

    return false;
}

string EccErrorInjectionTest::FailRecord::TableRow(UINT32 indentationLevel) const
{
    using Utility::StrPrintf;
    const UINT32 numSpaces = indentationLevel * s_IndentDepth;
    return StrPrintf("%*s%-15s | %-30s | %-7s | %8u | %-.3u | %-s\n", numSpaces,
                     " ", Ecc::ToString(eclwnit), detailedLocDesc.c_str(),
                     Ecc::ToString(errorType), attempts, rc.Get(), rc.Message());
}

//------------------------------------------------------------------------------
EccErrorInjectionTest::EccErrorInjectionTest()
{
    SetName("EccErrorInjectionTest");
}

EccErrorInjectionTest::~EccErrorInjectionTest() {}

//!
//! \brief Check enabled status of all HW injection supporting ECC units.
//!
//! If an ECC unit does not support HW injection, it will be set to false.
//!
//! \param[in,out] Array indexed by Ecc::Unit where the value at index i
//! is the enabled status of the ECC unit in Ecc::Unit corresponding to
//! i.
//!
RC EccErrorInjectionTest::CheckEnabledEclwnits
(
    Ecc::UnitList<bool>* pEnabledUnits
)
{
    MASSERT(pEnabledUnits);
    RC rc;
    LwRm* pLwRm = GetBoundRmClient();
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    pEnabledUnits->fill(false);

    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS eccStatus = {};
    CHECK_RC(pLwRm->ControlBySubdevice(pSubdev,
                                       LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                       &eccStatus, sizeof(eccStatus)));

    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        const Ecc::Unit eclwnit = static_cast<Ecc::Unit>(unit);
        const Ecc::RmGpuEclwnit gpuEclwnit = Ecc::ToRmGpuEclwnit(eclwnit);

        pEnabledUnits->at(unit)
            = (eccStatus.units[gpuEclwnit].supported && 
               eccStatus.units[gpuEclwnit].enabled && 
               Ecc::IsInjectLocSupported(eclwnit) &&
               (m_EccTestMask & (1 << unit)));
    }

    return rc;
}

//!
//! \brief IsSupported() checks if the the master GPU is correctly initialized
//!
//! EccErrorInjectionTest needs: ECC enabled, SHM enabled,
//!                                 and EccErrCounter initialized
//!
//! \return returns whether or not the test can be run in the current environment
//!
bool EccErrorInjectionTest::IsSupported()
{
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    // first check if the chip is supported
    // only pascal+ has the error injection registers
    if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_ECC_PRIV_INJECTION))
    {
        Printf(Tee::PriLow, "ECC Priv Injection only supported Pascal+\n");
        return false;
    }

    if (pSubdev->GetParentDevice()->GetFamily() == GpuDevice::Pascal)
    {
        // This test was not regressed during Pascal or Volta. During Turing it
        // was found that the test would fail if a GR context was not active. As
        // the test was, it is pure luck that there was an active GR context
        // during runs. VkStress has been added as a background test to
        // *hopefully* keep an active GR context. VkStress is not supported on
        // Pascal, so neither is the R400 version of this test.
        Printf(Tee::PriLow, "EccErrorInjectionTest is unsupported for Pascal\n");
        return false;
    }

    // Because there are no PLMs on the Pascal and Volta injection
    // register, the RM injection API is hidden on release builds of
    // MODS due to security concerns.
    if (pSubdev->HasBug(2389946)
        && (Utility::GetSelwrityUnlockLevel() >= Utility::SelwrityUnlockLevel::SUL_LWIDIA_NETWORK))
    {
        const char* s = "EccErrorInjectionTest is not supported for Pascal and "
            "Volta on release builds of MODS";

#ifdef DEBUG
        // Debug build, warn that behaviour is not consistent across builds
        Printf(Tee::PriWarn, "%s\n", s);
#else
        // Non-debug build, unsupported
        Printf(Tee::PriLow, "%s\n", s);
        return false;
#endif
    }

    // Verify that EccErrCounter is ready
    if (!pSubdev->GetEccErrCounter()->IsInitialized())
    {
        Printf(Tee::PriLow, "ECC Error Counter not initialized yet\n");
        return false;
    }

    // Check that ECC is enabled for any injection supporting ECC unit
    Ecc::UnitList<bool> supportedUnits;
    if (OK != CheckEnabledEclwnits(&supportedUnits))
    {
        Printf(Tee::PriLow, "RM ECC Status query failed\n");
        return false;
    }

    // Check if any ECC units are supported
    //
    for (bool supported : supportedUnits)
    {
        if (supported) { return true; }
    }
    Printf(Tee::PriLow, "No ECC units supported\n");
    return false;
}

//!
//! \brief Validate the test arguments.
//!
RC EccErrorInjectionTest::CheckTestArgs()
{
    RC rc;

    if (m_IntrCheckDelayMs < 1.0f)
    {
        Printf(Tee::PriWarn, "IntrCheckDelayMs must allow time for the interrupt to "
               "be serviced. Setting to 1ms.\n");
        m_IntrCheckDelayMs = 1.0f;
    }

    if (m_ActiveGrContextWaitMs < s_RecommendedGrCtxWaitMs)
    {
        Printf(Tee::PriWarn, "Low value for ActiveGrContextMs. RM will assert "
               "without an active GR context during injection.\n"
               "\tLwrrent value:     %fms\n"
               "\tRecommended value: %fms\n",
               m_ActiveGrContextWaitMs,
               s_RecommendedGrCtxWaitMs);
    }

    // Since we must wait at least the time of IntrCheckDelayMs before checking
    // interrupts, IntrTimeoutMs must be equal to (no additional waiting) or
    // greater than IntrCheckDelayMs.
    if (m_IntrCheckDelayMs > m_IntrTimeoutMs)
    {
        Printf(Tee::PriWarn, "IntrTimeoutMs must be at least equal to IntrCheckDelayMs. "
               "Setting to IntrCheckDelayMs (%fms)\n", m_IntrCheckDelayMs);
        m_IntrTimeoutMs = m_IntrCheckDelayMs;
    }
    // We need to to test at least 1 ECC hardware unit.
    if (!m_EccTestMask)
    {
        Printf(Tee::PriWarn, "EccTestMask must be > 0\n");
    }
    return rc;
}

//!
//! \brief Makes all the necessary allocations for EccErrorInjectionTest
//! This registers a callback and registers correctable and uncorrectable ECC
//! error notifiers.
//!
//! \return RC structure to denote the error status of Setup()
//!
RC EccErrorInjectionTest::Setup()
{
    RC rc;

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    GpuTest::Setup();
    CHECK_RC(CheckTestArgs());

    m_hSubdevDiag = pSubdev->GetSubdeviceDiag();
    m_pEccErrCounter = pSubdev->GetEccErrCounter();

    // Get enabled ECC units
    //
    CHECK_RC(CheckEnabledEclwnits(&m_EnabledEclwnits));

    // Get the basic test parameters
    //
    CHECK_RC(InitFromJs());

    // EccErrCounter's notifier callbacks need to be stopped as they
    // would be replaced by this tests's callbacks
    if (m_pEccErrCounter->IsUsingNotifiers())
    {
        CHECK_RC(m_pEccErrCounter->StopNotifiers());
        m_bShouldRestoreNotifiers = true;
    }

    // Setup RM ECC event notifier
    //
    m_EventParamsMutex = Tasker::CreateMutex("EccErrorInjectionTest ECC Event Callback",
                                            Tasker::mtxUnchecked);
    m_EccEventParams.pRwMutex = m_EventParamsMutex.get();
    m_EccEventParams.pSubdev  = GetBoundGpuSubdevice();

    CHECK_RC(pSubdev->HookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::CORR),
                                      CorrectableEvent,
                                      &m_EccEventParams,
                                      LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                                      GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
    m_bHookedCorrectable = true;
    CHECK_RC(pSubdev->HookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::UNCORR),
                                      UncorrectableEvent,
                                      &m_EccEventParams,
                                      LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                                      GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
    m_bHookedUncorrectable = true;

    return rc;
}

//!
//! \brief Runs EccErrorInjectionTest
//!
//! Tests if RM upholds the ISR convention of first setting the notifier and then
//! clearing interrupt bits. Triggers correctable and uncorrectable errors on FB
//! only.
//!
//! \return RC structure to denote the error status of Run()
//!
RC EccErrorInjectionTest::Run()
{
    RC rc;

    // Wait for the background test's GR context to be active and wait to clear
    // any pending interrupts
    Tasker::Sleep(std::max(m_ActiveGrContextWaitMs, m_IntrWaitTimeMs));

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    UINT64 totalInjections = 0;

    BgLogger::PauseBgLogger DisableBgLogger;

    { // Expecting HW intr during scope
        StartExpectingErrors();

        LwRm::DisableRcRecovery disableRcRecovery(pSubdev);

        MASSERT(m_EnabledEclwnits.size() == static_cast<UINT32>(Ecc::Unit::MAX));
        for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
        {
            const Ecc::Unit eclwnit = Ecc::Unit(unit);

            // Handle ECC unit if it is supported
            if (!m_EnabledEclwnits[unit])
            {
                continue;
            }

            MASSERT(Ecc::IsInjectLocSupported(eclwnit));

            rc = InjectErrors(eclwnit, &totalInjections);
            if (rc != RC::OK)
            {
                break; // Do cleanup, return after
            }
        }

        // Wait for any late interrupts to be serviced. This needs to be done in
        // any failure senario after triggering  interrupts.
        Tasker::Sleep(m_IntrWaitTimeMs);
        StopExpectingErrors();

        // It's possible that an intr will take a long time to be serviced and
        // return after we have assumed that an intr isn't coming (IntrTimeoutMs
        // + IntrWaitTimeMs). If this happens, ErrLogger will crash due to an
        // unexpected HW intr.
    }

    if (rc != RC::OK) { return rc; }

    CHECK_RC(ReportFailures(totalInjections));

    m_Failures.clear();
    m_MarginalFailures.clear();

    return rc;
}

//!
//! \brief Prints out failure report and returns appropriate RC based on the
//! test results.
//!
//! \param totalInjections Total number of injections performed during run.
//!
RC EccErrorInjectionTest::ReportFailures(UINT64 totalInjections)
{
    using Utility::StrPrintf;

    RC failRc;
    constexpr UINT32 indentationLevel = 1;
    constexpr UINT32 numSpaces = indentationLevel * s_IndentDepth;

    if (!m_MarginalFailures.empty())
    {
        MASSERT(m_MarginalFailures.size() <= totalInjections);
        std::sort(m_MarginalFailures.begin(), m_MarginalFailures.end(), FailRecord::TableSort);

        string s = StrPrintf("Marginal failures were encountered:\n\n%s",
                             FailRecord::TableHeader(indentationLevel).c_str());

        for (const FailRecord& rec : m_MarginalFailures)
        {
            s += rec.TableRow(indentationLevel);
        }

        Printf(Tee::PriWarn, "%s\n%*sTotal marginal failures: %zu / %llu\n\n", s.c_str(),
               numSpaces, " ", m_MarginalFailures.size(), totalInjections);
    }

    if (!m_Failures.empty())
    {
        MASSERT(m_Failures.size() <= totalInjections);
        failRc = m_Failures[0].rc; // Use first failure as RC

        std::sort(m_Failures.begin(), m_Failures.end(), FailRecord::TableSort);

        string s = StrPrintf("Failures were encountered:\n\n%s",
                             FailRecord::TableHeader(1).c_str());

        for (const FailRecord& rec : m_Failures)
        {
            s += rec.TableRow(1);
        }

        Printf(Tee::PriError, "%s\n%*sTotal failures: %zu / %llu\n\n", s.c_str(),
               numSpaces, " ", m_Failures.size(), totalInjections);
    }

    return failRc;
}

//!
//! \brief Inject supported error types into the given ECC unit location across
//! the GPU.
//!
//! \param eclwnit ECC unit to inject.
//! \param pTotalInjections Counter incremented before each injection attempt.
//!
RC EccErrorInjectionTest::InjectErrors
(
    Ecc::Unit eclwnit
    ,UINT64* pTotalInjections
)
{
    RC rc;

    using InjectFunc = RC (EccErrorInjectionTest::*)(Ecc::Unit, Ecc::ErrorType, UINT64* pTotalInjections);
    LwRm *         pLwRm     = GetBoundRmClient();
    bool bCorrectableSupported   = false;
    bool bUncorrectableSupported = false;

    Ecc::InjectLocInfo injectLocInfo;
    InjectFunc injectFunc;
    switch (eclwnit)
    {
        case Ecc::Unit::PCIE_REORDER:
        case Ecc::Unit::PCIE_P2PREQ:
        {
            LW208F_CTRL_BUS_ECC_INJECTION_SUPPORTED_PARAMS suppParams = { };
            suppParams.errorUnit = Ecc::ToRmBusInjectUnit(eclwnit);
            rc = pLwRm->Control(m_hSubdevDiag,
                                LW208F_CTRL_CMD_BUS_ECC_INJECTION_SUPPORTED,
                                &suppParams,
                                sizeof(suppParams));
            if (rc == RC::OK)
            {
                bCorrectableSupported = suppParams.bCorrectableSupported == LW_TRUE;
                bUncorrectableSupported = suppParams.bUncorrectableSupported == LW_TRUE;
                injectFunc = &EccErrorInjectionTest::InjectErrorsIntoPciBus;
            }
            else if (rc == RC::LWRM_NOT_SUPPORTED)
            {   
                // Supported flags are FALSE (debug purposes)
                MASSERT(!bCorrectableSupported);
                MASSERT(!bUncorrectableSupported);
            }
        }
        break;

        case Ecc::Unit::HUBMMU_L2TLB:
        case Ecc::Unit::HUBMMU_HUBTLB:
        case Ecc::Unit::HUBMMU_FILLUNIT:
        case Ecc::Unit::HSHUB:
        {
            LW208F_CTRL_MMU_ECC_INJECTION_SUPPORTED_PARAMS suppParams = { };
            suppParams.unit = Ecc::ToRmMmuInjectUnit(eclwnit);
            rc = pLwRm->Control(m_hSubdevDiag,
                                LW208F_CTRL_CMD_MMU_ECC_INJECTION_SUPPORTED,
                                &suppParams,
                                sizeof(suppParams));
            if (rc == RC::OK)
            {
                bCorrectableSupported   = suppParams.bCorrectableSupported == LW_TRUE;
                bUncorrectableSupported = suppParams.bUncorrectableSupported == LW_TRUE;
                injectFunc = &EccErrorInjectionTest::InjectErrorsIntoMmu;
            }
            else if (rc == RC::LWRM_NOT_SUPPORTED)
            {   
                // Supported flags are FALSE (debug purposes)
                MASSERT(!bCorrectableSupported);
                MASSERT(!bUncorrectableSupported);
            }
        } break;

        case Ecc::Unit::L2:
        {
            // TU11X is missing manuals that would allow the injection supported
            // control to return that L2 injection is supported
            if (GetBoundGpuSubdevice()->HasBug(2162496))
            {
                bCorrectableSupported   = true;
                bUncorrectableSupported = true;
                injectFunc = &EccErrorInjectionTest::InjectErrorsIntoFb;
            }
            else
            {
                LW208F_CTRL_FB_ECC_INJECTION_SUPPORTED_PARAMS suppParams = { };
                suppParams.location = Ecc::ToRmFbInjectLoc(eclwnit);
                rc = pLwRm->Control(m_hSubdevDiag,
                                    LW208F_CTRL_CMD_FB_ECC_INJECTION_SUPPORTED,
                                    &suppParams,
                                    sizeof(suppParams));
                if (rc == RC::OK)
                {
                    bCorrectableSupported   = suppParams.bCorrectableSupported == LW_TRUE;
                    bUncorrectableSupported = suppParams.bUncorrectableSupported == LW_TRUE;
                    injectFunc = &EccErrorInjectionTest::InjectErrorsIntoFb;
                }
                else if (rc == RC::LWRM_NOT_SUPPORTED)
                {   
                    // Supported flags are FALSE (debug purposes)
                    MASSERT(!bCorrectableSupported);
                    MASSERT(!bUncorrectableSupported);
                }
            }
        } break;

        default: // GR
        {
            LW208F_CTRL_GR_ECC_INJECTION_SUPPORTED_PARAMS suppParams = { };
            suppParams.unit = Ecc::ToRmGrInjectLoc(eclwnit);
            rc = pLwRm->Control(m_hSubdevDiag,
                                LW208F_CTRL_CMD_GR_ECC_INJECTION_SUPPORTED,
                                &suppParams,
                                sizeof(suppParams));

            if (rc == RC::OK)
            {
                bCorrectableSupported   = suppParams.bCorrectableSupported == LW_TRUE;
                bUncorrectableSupported = suppParams.bUncorrectableSupported == LW_TRUE;
                injectFunc = &EccErrorInjectionTest::InjectErrorsIntoGr;
            }
            else if (rc == RC::LWRM_NOT_SUPPORTED)
            {   
                // Supported flags are FALSE (debug purposes)
                MASSERT(!bCorrectableSupported);
                MASSERT(!bUncorrectableSupported);
            }
        } break;
    };

    if (rc == RC::LWRM_INSUFFICIENT_PERMISSIONS)
    {
        Printf(Tee::PriNormal,
               "Insufficient permissions to inject %s ecc errors, skipping\n",
               Ecc::ToString(eclwnit));
         if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
         {
             Printf(Tee::PriNormal,
                    "Provide an appropriate HULK license to enable ECC error injection\n");
         }
        return RC::OK;
    }

    // Injection for an ECC unit is not supported, print an
    // informative message and move on to the next one.
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow, "Injecting %s ecc errors is not supported, skipping\n",
               Ecc::ToString(eclwnit));
        return RC::OK;
    }

    CHECK_RC(rc);

    // Inject correctable errors
    if (bCorrectableSupported)
    {
        {
            // Lock the callback parameter structure for writing
            Tasker::MutexHolder mh(m_EccEventParams.pRwMutex);
            m_EccEventParams.eclwnit = eclwnit;
            m_EccEventParams.errorType = Ecc::ErrorType::CORR;
        }
        CHECK_RC((this->*injectFunc)(eclwnit, Ecc::ErrorType::CORR, pTotalInjections));
    }

    // Inject uncorrectable errors
    if (bUncorrectableSupported)
    {
        {
            // Lock the callback parameter structure for writing
            Tasker::MutexHolder mh(m_EccEventParams.pRwMutex);
            m_EccEventParams.eclwnit = eclwnit;
            m_EccEventParams.errorType = Ecc::ErrorType::UNCORR;
        }
        CHECK_RC((this->*injectFunc)(eclwnit, Ecc::ErrorType::UNCORR, pTotalInjections));
    }

    return rc;
}

//!
//! \brief tells the ecc error counters to start expecting errors
//!
//! \return void
//!
RC EccErrorInjectionTest::StartExpectingErrors()
{
    RC rc;

    MASSERT(m_EnabledEclwnits.size() == static_cast<UINT32>(Ecc::Unit::MAX));
    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        if (m_EnabledEclwnits[unit])
        {
            const Ecc::Unit eclwnit = Ecc::Unit(unit);

            // Expect correctable and uncorrectable errors, regardless of ECC
            // unit injection type behaviour
            CHECK_RC(m_pEccErrCounter->BeginExpectingErrors(eclwnit, Ecc::ErrorType::CORR));
            CHECK_RC(m_pEccErrCounter->BeginExpectingErrors(eclwnit, Ecc::ErrorType::UNCORR));
        }
    }

    return rc;
}

//!
//! \brief tells the ecc error counters to stop expecting errors
//!
//! \return void
//!
RC EccErrorInjectionTest::StopExpectingErrors()
{
    RC rc;

    MASSERT(m_EnabledEclwnits.size() == static_cast<UINT32>(Ecc::Unit::MAX));
    for (UINT32 unit = 0; unit < static_cast<UINT32>(Ecc::Unit::MAX); ++unit)
    {
        if (m_EnabledEclwnits[unit])
        {
            Ecc::Unit eclwnit = Ecc::Unit(unit);

            // Expect correctable and uncorrectable errors, regardless of ECC
            // unit injection type behaviour
            CHECK_RC(m_pEccErrCounter->EndExpectingErrors(eclwnit, Ecc::ErrorType::CORR));
            CHECK_RC(m_pEccErrCounter->EndExpectingErrors(eclwnit, Ecc::ErrorType::UNCORR));
        }
    }

    return rc;
}

//!
//! \brief Check that an ECC interrupt was triggered.
//!
//! Assumed to be called directly after a request for an injection.
//!
RC EccErrorInjectionTest::CheckIntr()
{
    //!
    //! \brief Poll function. Check if ECC callback event oclwred.
    //!
    //! \param pvEccEventParams Pointer to EccEventParams.
    //! \return True if pvEccEventParams indicates an ECC event oclwrred of the correct type, false
    //! o/w.
    //!
    static const auto IsEccEventTriggered = [](void* pvEccEventParams)
    {
        EccEventParams* pEccEventParams = static_cast<EccEventParams*>(pvEccEventParams);
        return pEccEventParams->eccEventOclwrred;
    };

    RC rc;

    // We only need to sleep for an instant to let the interrupts
    // be serviced, but also need to sleep enough so that we're not
    // considered an interrupt storm.
    // NOTE: Power state will affect the responsiveness of the
    // interrupt service handling. Lower power states will need longer
    // timeouts. On Turing, 2ms will work for lower power states.
    Tasker::Sleep(m_IntrCheckDelayMs);

    // Wait for the event to occur
    rc = POLLWRAP_HW(IsEccEventTriggered,
                     static_cast<void*>(&m_EccEventParams),
                     m_IntrTimeoutMs);

    {
        // Lock the NotifierCallbackParams structure
        Tasker::MutexHolder mh(m_EccEventParams.pRwMutex);

        if (rc != RC::TIMEOUT_ERROR) // timeout handled by checking event flag
        {
            CHECK_RC(rc);
        }
        rc.Clear();

        if (!m_EccEventParams.eccEventOclwrred)
        {
            rc = RC::LWRM_IRQ_NOT_FIRING;
        }
    }

    return rc;
}

//!
//! \brief Perform an injection.
//!
//! On failure, performs a test specified number of re-attempts.
//!
//! \param rmCtrlParams RM control call injection parameters.
//! \param injectDesc Details about the injection. Printed on injection failure.
//! \param pDetailedLocStr Description of the location within the ECC unit. Can
//! be nullptr. For example, "GPC x, TPC y".
//! \param pTotalInjections Counter incremented before each injection attempt.
//!
RC EccErrorInjectionTest::DoInjection
(
    const LwRmControlParams& rmCtrlParams
    ,const string& injectDesc
    ,const string* pDetailedLocStr
    ,UINT64* pTotalInjections
)
{
    RC rc;
    LwRm* pLwRm = GetBoundRmClient();

    for (UINT32 loop = 0; loop < GetTestConfiguration()->Loops(); loop++)
    {
        RC injectStatus;
        StickyRC firstFailure;
        const UINT32 totalAttempts = 1 + m_RetriesOnFailure;
        UINT32 attemptsLeft = totalAttempts;
        UINT32 attemptsUsed = 0;
        MASSERT(attemptsLeft > m_RetriesOnFailure);

        do
        {
            --attemptsLeft;
            attemptsUsed = totalAttempts - attemptsLeft;
            injectStatus.Clear();

            (*pTotalInjections)++; // record injection

            {
                // Lock the callback parameter structure for writing
                Tasker::MutexHolder mh(m_EccEventParams.pRwMutex);

                // Initialize event parameters
                m_EccEventParams.eccEventOclwrred     = false;
            }

            // Inject
            CHECK_RC(pLwRm->Control(rmCtrlParams.hObject,
                                    rmCtrlParams.cmd,
                                    rmCtrlParams.pParams,
                                    rmCtrlParams.paramsSize));

            // Check injection result
            injectStatus = CheckIntr();
            switch (injectStatus)
            {
                case RC::LWRM_IRQ_NOT_FIRING:
                    break;

                default: // unexpected failure
                    CHECK_RC(injectStatus);
                    break;
            }
            firstFailure = injectStatus;

            if (injectStatus != RC::OK)
            {
                VerbosePrintf("Injection failed with RC(%u), attempt(%u): %s\n",
                              injectStatus.Get(), attemptsUsed, injectDesc.c_str());
            }

        } while (injectStatus != RC::OK && attemptsLeft > 0);

        if (injectStatus != RC::OK)
        {
            m_Failures.emplace_back(RC(firstFailure.Get()), m_EccEventParams,
                                    (pDetailedLocStr ? *pDetailedLocStr : ""),
                                    attemptsUsed);
        }
        else if (attemptsLeft < (totalAttempts - 1)) // failed at least once
        {
            m_MarginalFailures.emplace_back(RC(firstFailure.Get()), m_EccEventParams,
                                            (pDetailedLocStr ? *pDetailedLocStr : ""),
                                            attemptsUsed);
        }

        injectStatus.Clear();
        firstFailure.Clear();
    }

    MASSERT(m_Failures.size() <= *pTotalInjections);
    MASSERT(m_MarginalFailures.size() <= *pTotalInjections);

    return rc;
}

//!
//! \brief Inject ECC errors into a GR ECC unit.
//!
//! \param eclwnit ECC unit to inject.
//! \param errorType Type of error to inject.
//! \param pTotalInjections Counter incremented before each injection attempt.
//!
RC EccErrorInjectionTest::InjectErrorsIntoGr
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errorType
    ,UINT64* pTotalInjections
)
{
    using Utility::StrPrintf;

    MASSERT(m_EnabledEclwnits[static_cast<size_t>(eclwnit)]);
    MASSERT(Ecc::IsInjectLocSupported(eclwnit));

    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    const char* eclwnitStr = Ecc::ToString(eclwnit);
    const char* errorTypeStr = Ecc::ToString(errorType);
    string injectDesc;
    string detailedLocDesc;

    LW208F_CTRL_GR_ECC_INJECT_ERROR_PARAMS ctrlInjectParams = {};
    ctrlInjectParams.unit = static_cast<UINT08>(Ecc::ToRmGrInjectLoc(eclwnit));
    ctrlInjectParams.errorType = static_cast<UINT08>(Ecc::ToRmGrInjectType(errorType));
    LwRmControlParams rmCtrlParams = {};
    rmCtrlParams.hObject = m_hSubdevDiag;
    rmCtrlParams.cmd = LW208F_CTRL_CMD_GR_ECC_INJECT_ERROR;
    rmCtrlParams.pParams = (void*)&ctrlInjectParams;
    rmCtrlParams.paramsSize = sizeof(ctrlInjectParams);

    for (UINT32 gpc = 0; gpc < pSubdev->GetGpcCount(); gpc++)
    {
        CHECK_RC(Abort::Check());

        UINT32 numSublocations = pSubdev->GetTpcCountOnGpc(gpc);
        if (eclwnit == Ecc::Unit::GPCMMU)
            numSublocations = pSubdev->GetMmuCountPerGpc(gpc);
        for (UINT32 sublocation = 0; sublocation < numSublocations; sublocation++)
        {
            ctrlInjectParams.location = gpc;
            ctrlInjectParams.sublocation = sublocation;
            MASSERT(ctrlInjectParams.unit == static_cast<UINT08>(Ecc::ToRmGrInjectLoc(eclwnit)));
            MASSERT(ctrlInjectParams.errorType == static_cast<UINT08>(Ecc::ToRmGrInjectType(errorType)));

            detailedLocDesc = StrPrintf("GPC %u, %s %u", gpc,
                    (eclwnit == Ecc::Unit::GPCMMU) ? "MMU" : "TPC", sublocation);
            injectDesc = Utility::StrPrintf("%s %s error into %s",
                                            eclwnitStr, errorTypeStr,
                                            detailedLocDesc.c_str());
            VerbosePrintf("Inject: %s\n", injectDesc.c_str());

            CHECK_RC(DoInjection(rmCtrlParams, injectDesc, &detailedLocDesc, pTotalInjections));
        }
    }

    return rc;
}

//!
//! \brief Inject ECC errors into a FB ECC unit.
//!
//! \param eclwnit ECC unit to inject.
//! \param errorType Type of error to inject.
//! \param pTotalInjections Counter incremented before each injection attempt.
//!
RC EccErrorInjectionTest::InjectErrorsIntoFb
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errorType
    ,UINT64* pTotalInjections
)
{
    using Utility::StrPrintf;

    MASSERT(m_EnabledEclwnits[static_cast<size_t>(eclwnit)]);
    MASSERT(eclwnit == Ecc::Unit::L2); // L2 is only supported fb unit

    RC rc;
    FrameBuffer* pFb = GetBoundGpuSubdevice()->GetFB();
    const char* eclwnitStr = Ecc::ToString(eclwnit);
    const char* errorTypeStr = Ecc::ToString(errorType);
    string injectDesc;
    string detailedLocDesc;

    LW208F_CTRL_FB_INJECT_LTC_ECC_ERROR_PARAMS ctrlInjectParams = {};
    ctrlInjectParams.errorType = Ecc::ToRmFbInjectType(errorType);
    ctrlInjectParams.locationMask = LW208F_CTRL_FB_INJECT_LTC_ECC_ERROR_LOC_ANY;
    LwRmControlParams rmCtrlParams = {};
    rmCtrlParams.hObject = m_hSubdevDiag;
    rmCtrlParams.cmd = LW208F_CTRL_CMD_FB_INJECT_LTC_ECC_ERROR;
    rmCtrlParams.pParams = (void*)&ctrlInjectParams;
    rmCtrlParams.paramsSize = sizeof(ctrlInjectParams);

    for (UINT32 ltc = 0; ltc < pFb->GetLtcCount(); ++ltc)
    {
        CHECK_RC(Abort::Check());
        ctrlInjectParams.ltc = ltc;

        const UINT32 numSlices = pFb->GetSlicesPerLtc(ltc);
        MASSERT(numSlices > 0);

        for (UINT08 slice = 0; slice < numSlices; ++slice)
        {
            ctrlInjectParams.slice = slice;

            detailedLocDesc = StrPrintf("LTC %u, slice %u", ltc, slice);
            injectDesc = StrPrintf("%s %s error into %s", eclwnitStr,
                                   errorTypeStr, detailedLocDesc.c_str());
            VerbosePrintf("Inject: %s\n", injectDesc.c_str());

            CHECK_RC(DoInjection(rmCtrlParams, injectDesc, &detailedLocDesc, pTotalInjections));
        }
    }

    return rc;
}

//!
//! \brief Inject ECC errors into a MMU ECC unit.
//!
//! \param eclwnit ECC unit to inject.
//! \param errorType Type of error to inject.
//! \param pTotalInjections Counter incremented before each injection attempt.
//!
RC EccErrorInjectionTest::InjectErrorsIntoMmu
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errorType
    ,UINT64* pTotalInjections
)
{
    using Utility::StrPrintf;

    MASSERT(m_EnabledEclwnits[static_cast<size_t>(eclwnit)]);

    RC rc;
    const char* eclwnitStr = Ecc::ToString(eclwnit);
    const char* errorTypeStr = Ecc::ToString(errorType);
    string injectDesc;
    string detailedLocDesc;

    LW208F_CTRL_MMU_ECC_INJECT_ERROR_PARAMS ctrlInjectParams = {};
    ctrlInjectParams.unit = Ecc::ToRmMmuInjectUnit(eclwnit);
    ctrlInjectParams.errorType = Ecc::ToRmMmuInjectType(errorType);
    LwRmControlParams rmCtrlParams = {};
    rmCtrlParams.hObject = m_hSubdevDiag;
    rmCtrlParams.cmd = LW208F_CTRL_CMD_MMU_ECC_INJECT_ERROR;
    rmCtrlParams.pParams = (void*)&ctrlInjectParams;
    rmCtrlParams.paramsSize = sizeof(ctrlInjectParams);

    const UINT32 numLocations =
        (eclwnit == Ecc::Unit::HSHUB) ? GetBoundGpuSubdevice()->GetMaxHsHubCount() : 1;
    const UINT32 numSublocations =
        (eclwnit == Ecc::Unit::HSHUB) ? LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT : 1;
    for (UINT32 location = 0; location < numLocations; ++location)
    {
        CHECK_RC(Abort::Check());

        for (UINT08 sublocation = 0; sublocation < numSublocations; ++sublocation)
        {
            injectDesc = StrPrintf("%s %s error", eclwnitStr, errorTypeStr);
            if (eclwnit == Ecc::Unit::HSHUB)
            {
                detailedLocDesc = StrPrintf("HSHUB %u, Block %s",
                                            location,
                                            Ecc::HsHubSublocationString(sublocation));
                injectDesc += " into " + detailedLocDesc;
                ctrlInjectParams.location = location;
                ctrlInjectParams.sublocation = sublocation;

            }
            VerbosePrintf("Inject: %s\n", injectDesc.c_str());

            CHECK_RC(DoInjection(rmCtrlParams,
                                 injectDesc,
                                 detailedLocDesc.empty() ? nullptr : &detailedLocDesc,
                                 pTotalInjections));
        }
    }

    return rc;
}

//!
//! \brief Inject ECC errors into a PCI BUS ECC unit.
//!
//! \param eclwnit ECC unit to inject.
//! \param errorType Type of error to inject.
//! \param pTotalInjections Counter incremented before each injection attempt.
//!
RC EccErrorInjectionTest::InjectErrorsIntoPciBus
(
    Ecc::Unit eclwnit
    ,Ecc::ErrorType errorType
    ,UINT64* pTotalInjections
)
{
    using Utility::StrPrintf;

    MASSERT(m_EnabledEclwnits[static_cast<size_t>(eclwnit)]);

    RC rc;
    const char* eclwnitStr = Ecc::ToString(eclwnit);
    const char* errorTypeStr = Ecc::ToString(errorType);
    string injectDesc;

    LW208F_CTRL_BUS_ECC_INJECT_ERROR_PARAMS ctrlInjectParams = {};
    ctrlInjectParams.errorUnit = Ecc::ToRmBusInjectUnit(eclwnit);
    ctrlInjectParams.errorType = Ecc::ToRmBusInjectType(errorType);

    LwRmControlParams rmCtrlParams = {};
    rmCtrlParams.hObject = m_hSubdevDiag;
    rmCtrlParams.cmd = LW208F_CTRL_CMD_BUS_ECC_INJECT_ERROR;
    rmCtrlParams.pParams = (void*)&ctrlInjectParams;
    rmCtrlParams.paramsSize = sizeof(ctrlInjectParams);

    CHECK_RC(Abort::Check());

    injectDesc = StrPrintf("%s %s error", eclwnitStr, errorTypeStr);
    VerbosePrintf("Inject: %s\n", injectDesc.c_str());

    CHECK_RC(DoInjection(rmCtrlParams,
                         injectDesc,
                         nullptr,
                         pTotalInjections));

    return rc;
}
//!
//! \brief Callback for ECC errors
//!
//! Here we check if the appropriate ECC interrupt bit is set.
//!
//! \return nothing
//!
/*static*/
void EccErrorInjectionTest::CorrectableEvent(void *pArg)
{
    MASSERT(pArg);
    EccEventParams* pParams = static_cast<EccEventParams*>(pArg);

    // An uncorrectable error triggered from a potential previous injection, the test has
    // moved on to a correctable type, ignore it
    if (pParams->errorType != Ecc::ErrorType::CORR)
        return;

    LwNotification notifyData = { };
    RC rc = pParams->pSubdev->GetResmanEventData(Ecc::ToRmNotifierEvent(Ecc::ErrorType::CORR),
                                                 &notifyData);

    // An uncorrectable error triggered from a previously tested unit, the test has moved to a
    // different unit, ignore it
    if ((RC::OK == rc) && (notifyData.info16 != Ecc::ToRmGpuEclwnit(pParams->eclwnit)))
        return;

    {
        // Lock the callback parameter structure for writing
        Tasker::MutexHolder mh(pParams->pRwMutex);
        pParams->eccEventOclwrred = true;
    }
}

//!
//! \brief Callback for ECC errors
//!
//! Here we check if the appropriate ECC interrupt bit is set.
//!
//! \return nothing
//!
/*static*/
void EccErrorInjectionTest::UncorrectableEvent(void *pArg)
{
    MASSERT(pArg);
    EccEventParams* pParams = static_cast<EccEventParams*>(pArg);

    // An uncorrectable error triggered from a potential previous injection, the test has
    // moved on to a correctable type, ignore it
    if (pParams->errorType != Ecc::ErrorType::UNCORR)
        return;

    LwNotification notifyData = { };
    RC rc = pParams->pSubdev->GetResmanEventData(Ecc::ToRmNotifierEvent(Ecc::ErrorType::UNCORR),
                                                 &notifyData);

    // An uncorrectable error triggered from a previously tested unit, the test has moved to a
    // different unit, ignore it
    if ((RC::OK == rc) && (notifyData.info16 != Ecc::ToRmGpuEclwnit(pParams->eclwnit)))
        return;

    {
        // Lock the callback parameter structure for writing
        Tasker::MutexHolder mh(pParams->pRwMutex);
        pParams->eccEventOclwrred = true;
    }
}

RC EccErrorInjectionTest::Cleanup()
{
    StickyRC rc;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    if (m_bHookedCorrectable)
    {
        rc = pSubdev->UnhookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::CORR));
        m_bHookedCorrectable = false;
    }
    if (m_bHookedUncorrectable)
    {
        rc = pSubdev->UnhookResmanEvent(Ecc::ToRmNotifierEvent(Ecc::ErrorType::UNCORR));
        m_bHookedUncorrectable = false;
    }

    // Restart EccErrCounter's notifiers if they were previously in use
    if (m_bShouldRestoreNotifiers)
    {
        rc = m_pEccErrCounter->StartNotifiers();
        m_bShouldRestoreNotifiers = false;
    }

    rc = GpuTest::Cleanup();

    return rc;
}

void EccErrorInjectionTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "EccErrorInjectionTest Js Properties:\n");
    Printf(pri, "\t%-32s %f\n",     "IntrWaitTimeMs:",        m_IntrWaitTimeMs);
    Printf(pri, "\t%-32s %f\n",     "IntrCheckDelayMs:",      m_IntrCheckDelayMs);
    Printf(pri, "\t%-32s %f\n",     "IntrTimeoutMs:",         m_IntrTimeoutMs);
    Printf(pri, "\t%-32s %f\n",     "ActiveGrContextWaitMs:", m_ActiveGrContextWaitMs);
    Printf(pri, "\t%-32s %u\n",     "RetriesOnFailure:",      m_RetriesOnFailure);
    Printf(pri, "\t%-32s %#010X\n", "EccTestMask:",           m_EccTestMask);
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EccErrorInjectionTest, GpuTest,
                 "ECC injection test for ECC units with HW ECC injection support");

CLASS_PROP_READWRITE(EccErrorInjectionTest, IntrWaitTimeMs, FLOAT64,
                     "Delay at the start and end of the test so pending interrupts can be serviced (ms)");
CLASS_PROP_READWRITE(EccErrorInjectionTest, IntrCheckDelayMs, FLOAT64,
                     "Time to wait before checking for ECC interrupt completion (ms)");
CLASS_PROP_READWRITE(EccErrorInjectionTest, IntrTimeoutMs, FLOAT64,
                     "ECC interrupt timeout (ms)");
CLASS_PROP_READWRITE(EccErrorInjectionTest, ActiveGrContextWaitMs, FLOAT64,
                     "Wait for background test to be active GR context (ms)");
CLASS_PROP_READWRITE(EccErrorInjectionTest, RetriesOnFailure, UINT32,
                     "Number of times to retry an injection after a failure");
CLASS_PROP_READWRITE(EccErrorInjectionTest, EccTestMask, UINT32,
                     "Debug:Bit mask of ECC units to test (default = all units)");
