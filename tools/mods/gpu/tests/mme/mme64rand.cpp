/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mme64rand.cpp
//! \brief MME64 (Method Macro Expander 64) random program test
//!

#include "align.h"
#include "class/clc597.h"           // Turing 3d engine (contains MME64)
#include "class/clc697.h"           // AMPERE_A
#include "core/include/abort.h"
#include "core/include/massert.h"
#include "core/include/platform.h"  // MEM_RD32
#include "core/include/types.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/utility/gpuutils.h"
#include "mme64igen.h"
#include "mme64manager.h"
#include "mme64sim.h"

#include <vector>

using namespace Mme64;

//!
//! \brief Adds the time to execute the given scope to the given
//! aclwmulation variable.
//!
//! \param Aclwmulator Variable to add the exelwtion time in ms.
//! \param Scope Scope that will be exelwted and timed.
//!
#define TIME_SCOPE_MS(Aclwmulator, Scope)                               \
    do                                                                  \
    {                                                                   \
        const UINT64 startTimeMs = Xp::GetWallTimeMS();                 \
        Scope;                                                          \
        const UINT64 deltaTimeMs = Xp::GetWallTimeMS() - startTimeMs;   \
        (Aclwmulator) += deltaTimeMs;                                   \
    } while (0)

namespace
{
    //!
    //! \brief Return pointer to string literal corresponding to the given
    //! boolean.
    //!
    constexpr const char* StrBool(bool b)
    {
        return (b ? "true" : "false");
    }

    //!
    //! \brief Colwert the result of given fraction as a percentage.
    //!
    //! \param numerator Fraction numerator.
    //! \param denominator Fraction denominator.
    //!
    //! \return Fraction as a percentage in the range [0, 100].
    //!
    constexpr double ToPercent(UINT64 numerator, UINT64 denominator)
    {
        return (numerator / static_cast<double>(denominator)) * 100.0f;
    }
};

////////////////////////////////////////////////////////////////////////////////
//!
//! \brief MME64 Random Test
//!
//! The Method Macro Expander 64 (MME64) is capable of receiving arbitrary
//! programs using a simple instruction set (see gpu/tests/mme/mme64macro.h for
//! details). The macros are both programmed and run using push buffer
//! commands. The output of the MME64 is method/data pairs which are then passed
//! on to the rest of host processing. The MME64 has a shadow RAM which can
//! track, filter, and replay previous macro output. It also has a data RAM,
//! which is a new addition from the original MME.
//!
//! This test creates random macro programs and runs them through an MME64 to
//! determine what the output should be. The macros are then run on hardware.
//! Instead of passing on the output of the MME64 to host, the macro output is
//! used as input data to an Inline MemToMem transfer. The outputs are then
//! checked against the expected output from the simulator.
//!
//! The random macro generator the successor of the original MME test,
//! MMERandom.
//!
class MME64Random : public GpuTest
{
public:
    MME64Random();

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

    static void NoEmissionSimulatorCallback
    (
        void* callbackData
        ,unsigned int method
        ,unsigned int data
    );
    static void SimulatorCallback
    (
        void *callbackData
        ,unsigned int method
        ,unsigned int data
    );

    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(MacroSize,              UINT32);
    SETGET_PROP(SimOnly,                bool);
    SETGET_PROP(SimTrace,               bool);
    SETGET_PROP(DumpMacros,             bool);
    SETGET_PROP(DumpSimEmissions,       bool);
    SETGET_PROP(DumpMmeEmissions,       bool);
    SETGET_PROP(NumMacrosPerLoop,       UINT32);
    SETGET_PROP(GenMacrosEachLoop,      bool);
    SETGET_PROP(GenOpsArithmetic,       bool);
    SETGET_PROP(GenOpsLogic,            bool);
    SETGET_PROP(GenOpsComparison,       bool);
    SETGET_PROP(GenOpsDataRam,          bool);
    SETGET_PROP(GenNoLoad,              bool);
    SETGET_PROP(GenNoSendingMethods,    bool);
    SETGET_PROP(GenUncondPredMode,      bool);
    SETGET_PROP(GenMaxNumRegisters,     UINT32);
    SETGET_PROP(GenMaxDataRamEntry,     UINT32);
    SETGET_PROP(PrintTiming,            bool);
    SETGET_PROP(PrintDebug,             bool);

private:
    //!
    //! \brief MME Simulator callback data.
    //!
    struct SimCallbackData
    {
        bool traceEnabled; //!< If true, print simulator callback data
        vector<Emission>* pEmissions; //!< Simulator emissions
    };

    //!
    //! \brief State of a macro during processing.
    //!
    struct MacroTask
    {
        Macro macro;
        vector<Emission> simEmissions;
        //! True if this task has been run on the simulator and the MME.
        bool completed;

        MacroTask();

        void ResetInfo();
    };

    struct TimeInterval
    {
        UINT64 start;
        UINT64 end;
    };

    //!
    //! \brief Breakdown of the time spent doing different stages of the test
    //! over some interval.
    //!
    struct TimingMetrics
    {
        enum : UINT64 { ILWALID_TIME = _UINT64_MAX };

        //! Start and end epoch (ms)
        TimeInterval interval = { ILWALID_TIME, ILWALID_TIME };

        // Net durations over loop interval
        UINT64 generation    = 0; //!< Time in interval generating macros
        UINT64 loadMme       = 0; //!< Time in interval loading the MME
        UINT64 runOnSim      = 0; //!< Time in interval running on the simulator
        UINT64 runOnMme      = 0; //!< Time in interval running on the MME
        UINT64 comparison    = 0; //!< Time in interval comparing the simulator and MME results
        UINT64 dataRamUpdate = 0; //!< Time in interval updating simulator Data Ram

        void   Reset();
        UINT64 TotalRecordedTime() const;
        bool   IsValid() const;
        string Report() const;
    };

    GpuTestConfiguration*   m_pGpuTestConfig = nullptr;
    GpuDevice*              m_pGpudev        = nullptr;
    GpuSubdevice*           m_pSubdev        = nullptr;
    FancyPicker::FpContext* m_pFpCtx         = nullptr;

    unique_ptr<ThreeDAlloc>    m_pThreeDAlloc; //!< 3D channel class allocater

    unique_ptr<MacroGenerator> m_pMmeIGen;     //!< MME64 Macro generator
    unique_ptr<Simulator>      m_pSimulator;   //!< MME64 Simulator
    unique_ptr<Manager>        m_pMmeMgr;      //!< MME64 hardware manager

    /* MME Simulator */
    Simulator::Memory           m_SimShadowRam; //!< Simulator Shadow RAM
    Simulator::Memory           m_SimDataRam;   //!< Simulator Data RAM
    //! Simulator GPU memory accessed by the DMA unit
    Simulator::Memory           m_SimMemory;
    //! Callback data for the simulator callback
    SimCallbackData             m_SimCbData      = {};
    //! Simulator run configuration for background setup macros
    Simulator::RunConfiguration m_SimSetupConfig = {};
    //! Simulator run configuration for macros run during the test
    Simulator::RunConfiguration m_SimTestConfig  = {};

    UINT32 m_LwrLoop = 0; //!< Current test loop

    /* Test arguments */
    // TODO(stewarts): Possible test arguments:
    // - Compare at the end of the loop instead of after each macro.
    //   - Requires keeping I2M surface in MacroTasks.

    //! Testarg: total size in groups of generated macros, including the
    //! required preamble.
    UINT32 m_MacroSize           = 256;
    bool   m_SimOnly             = false;
    bool   m_SimTrace            = false;
    bool   m_DumpMacros          = false;
    bool   m_DumpSimEmissions    = false;
    bool   m_DumpMmeEmissions    = false;
    UINT32 m_NumMacrosPerLoop    = 12;
    bool   m_GenMacrosEachLoop   = true;
    bool   m_GenOpsArithmetic    = true;
    bool   m_GenOpsLogic         = true;
    bool   m_GenOpsComparison    = true;
    bool   m_GenOpsDataRam       = true;
    bool   m_GenNoLoad           = false;
    bool   m_GenNoSendingMethods = false;
    bool   m_GenUncondPredMode   = false;
    UINT32 m_GenMaxNumRegisters  = MacroGenerator::Config::MAX_NUM_REGISTERS_ALL;
    UINT32 m_GenMaxDataRamEntry  = MacroGenerator::Config::MAX_DATA_RAM_ENTRY_ALL;
    bool   m_PrintTiming         = false;
    bool   m_PrintDebug          = false;

    static constexpr UINT32 GetNumGroupsInMacroPreamble();

    RC InnerRun();
    RC ResetState();

    RC SetMacroGenConfig();
    RC GenMacro(Macro* pMacro) const;
    RC ClearMacro(Macro* pMacro);
    RC GenMacroTask(MacroTask* pTask) const;
    RC ClearMacroTask(MacroTask* pTask);
    RC GenMacroTasks(vector<MacroTask>* pTasks);
    RC LoadMacroTasks(vector<MacroTask>* pTasks, UINT32 nextTaskIndex);

    RC FillDataRamsViaDma(bool randValues);
    RC FillDataRamsViaMacros(bool randValues);

    RC RunOnSim(MacroTask* pTask);
    RC RunOnMme(MacroTask* pTask, VirtualAddr* pMacroOutputAddr);

    RC CompareEmissions
    (
        const MacroTask& task
        ,VirtualAddr mmeMacroOutputAddr
    ) const;

    RC DumpSimEmissions
    (
        Tee::Priority printLevel
        ,const vector<Emission>& emissions
    ) const;
    RC DumpMmeI2mEmissions
    (
        Tee::Priority printLevel
        ,VirtualAddr mmeMacroOutputAddr
        ,UINT32 numEmissions
    ) const;

    RC UpdateSimDataRamFromMme
    (
        const Surface2D& mmeDataRamSurf
    );

    Tee::Priority GetDebugPrintPri() const;
};

//------------------------------------------------------------------------------
MME64Random::MacroTask::MacroTask()
    : completed(false)
{}

//!
//! \brief Reset the macro state information. The macro is not altered.
//!
void MME64Random::MacroTask::ResetInfo()
{
    simEmissions.clear();
    completed = false;
}

//------------------------------------------------------------------------------
//!
//! \brief Reset all metrics to 0 and ilwalidate the interval.
//!
void MME64Random::TimingMetrics::Reset()
{
    interval      = { ILWALID_TIME, ILWALID_TIME };
    generation    = 0;
    loadMme       = 0;
    runOnSim      = 0;
    runOnMme      = 0;
    comparison    = 0;
    dataRamUpdate = 0;
}

//!
//! \brief Get the total recorded time across all metrics.
//!
UINT64 MME64Random::TimingMetrics::TotalRecordedTime() const
{
    return generation + loadMme + runOnSim + runOnMme + comparison + dataRamUpdate;
}

//!
//! \brief Check if the recorded timing metrics are valid.
//!
//! To be valid, the interval must be set and the recorded times must fit within
//! within the interval (inclusive).
//!
//! \return True if valid, false o/w.
//!
bool MME64Random::TimingMetrics::IsValid() const
{
    const bool loopIntervalValid = (interval.start != ILWALID_TIME
                                    && interval.end != ILWALID_TIME
                                    && (interval.start <= interval.end));
    const bool breakdownWithinInterval
        = (TotalRecordedTime() <= (interval.end - interval.start));

    return loopIntervalValid && breakdownWithinInterval;
}

//!
//! \brief Create report of the metrics over the interval.
//!
//! \return The report.
//!
string MME64Random::TimingMetrics::Report() const
{
    const UINT64 totalTime = interval.end - interval.start;
    const UINT64 totalUnaccountedTime = totalTime - TotalRecordedTime();

    return Utility::StrPrintf("%llums over interval [%llu, %llu]\n"
                              "\tmacro generation    : %6.2f%% (%3llums)\n"
                              "\tloading MME         : %6.2f%% (%3llums)\n"
                              "\trun on simulator    : %6.2f%% (%3llums)\n"
                              "\trun on MME          : %6.2f%% (%3llums)\n"
                              "\tcomparison          : %6.2f%% (%3llums)\n"
                              "\tsim data ram update : %6.2f%% (%3llums)\n"
                              "\tmisc                : %6.2f%% (%3llums)",
                              totalTime, interval.start, interval.end,
                              ToPercent(generation, totalTime), generation,
                              ToPercent(loadMme, totalTime), loadMme,
                              ToPercent(runOnSim, totalTime), runOnSim,
                              ToPercent(runOnMme, totalTime), runOnMme,
                              ToPercent(comparison, totalTime), comparison,
                              ToPercent(dataRamUpdate, totalTime), dataRamUpdate,
                              ToPercent(totalUnaccountedTime, totalTime), totalUnaccountedTime);
}

//------------------------------------------------------------------------------
MME64Random::MME64Random()
    : m_pGpuTestConfig(GetTestConfiguration())
    , m_pFpCtx(GetFpContext())
    , m_pThreeDAlloc(make_unique<ThreeDAlloc>())
{
    SetName("MME64Random");

    // Code relies on the flag for using all data ram entries to be 0.
    MASSERT(0 == MacroGenerator::Config::MAX_DATA_RAM_ENTRY_ALL);
}

bool MME64Random::IsSupported()
{
    // If we are only running the simulator, we are only using the CPU
    if (m_SimOnly)
    {
        return true;
    }

    GpuDevice* gpudev = GetBoundGpuDevice();
    GpuSubdevice* subdev = GetBoundGpuSubdevice();

    m_pThreeDAlloc->SetOldestSupportedClass(TURING_A);
    // We are dropping support for this test moving forward.
    // We had not caught anything in production from this test for a VERY long time.
    m_pThreeDAlloc->SetNewestSupportedClass(AMPERE_A);

    return subdev->HasFeature(Device::GPUSUB_HAS_MME64)
        && m_pThreeDAlloc->IsSupported(gpudev);
}

RC MME64Random::Setup()
{
    RC rc;

    // TODO(stewarts): SimOnly is lwrrently not supported.
    if (m_SimOnly)
    {
        Printf(Tee::PriError, "SimOnly is lwrrently unsupported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(AllocDisplayAndSurface(false/*blockLinear*/));

    m_pFpCtx->Rand.SeedRandom(GetTestConfiguration()->Seed());

    m_pGpudev = GetBoundGpuDevice();
    m_pSubdev = GetBoundGpuSubdevice();

    m_pMmeIGen = make_unique<MacroGenerator>(m_pFpCtx);
    if (!m_pMmeIGen)
    {
        Printf(Tee::PriError, "Failed to contruct MME64 instruction generator\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    m_pSimulator = make_unique<Simulator>();
    if (!m_pSimulator)
    {
        Printf(Tee::PriError, "Failed to construct MME64 simulator\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Setup the simulator
    //
    m_SimCbData.traceEnabled = m_SimTrace;

    // Background setup run configuration
    m_SimSetupConfig.traceEnabled = m_SimTrace;
    m_SimSetupConfig.disableMemoryUnit = true; // Runs MME in I2M mode
    m_SimSetupConfig.callback = MME64Random::NoEmissionSimulatorCallback;
    m_SimSetupConfig.pCallbackData = nullptr;
    m_SimSetupConfig.pShadowRamState = &m_SimShadowRam;
    m_SimSetupConfig.pDataRamState = &m_SimDataRam;
    m_SimSetupConfig.pMemoryState = &m_SimMemory;

    // Test run configuration
    m_SimTestConfig.traceEnabled = m_SimTrace;
    m_SimTestConfig.disableMemoryUnit = true; // Runs MME in I2M mode
    m_SimTestConfig.callback = MME64Random::SimulatorCallback;
    m_SimTestConfig.pCallbackData = reinterpret_cast<void*>(&m_SimCbData);
    m_SimTestConfig.pShadowRamState = &m_SimShadowRam;
    m_SimTestConfig.pDataRamState = &m_SimDataRam;
    m_SimTestConfig.pMemoryState = &m_SimMemory;

    // Setup the hardware
    //
    // NOTE: Skip hardware setup if only running the simulator
    if (m_SimOnly) { return rc; }

    m_pMmeMgr = make_unique<Manager>
        (
            this
            ,m_pGpuTestConfig
            ,std::move(m_pThreeDAlloc)
        );
    if (!m_pMmeMgr)
    {
        Printf(Tee::PriError, "Failed to construct MME64 manager\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    m_pThreeDAlloc = nullptr;

    CHECK_RC(m_pMmeMgr->Setup());

    // Initialize the sim and MME data ram
    //
    if (m_GenOpsDataRam)
    {
        CHECK_RC(FillDataRamsViaDma(true/*randValues*/));
    }

    // Check test arguments
    //
    // TODO(stewarts): Arg checks and the gen config setting should be moved
    // before the HW checks. Required for properly supporting SimOnly.
    const UINT32 numGroupsInMacroPreamble = GetNumGroupsInMacroPreamble();
    const UINT32 maxNumGroupsPerMacro
        = (m_pMmeMgr->GetInstrRamSize() / Macro::Groups::value_type::GetElementCount())
        // Each macro must start by setting the register file to a known state
        - GetNumGroupsInMacroPreamble();
    if (m_MacroSize > maxNumGroupsPerMacro)
    {
        Printf(Tee::PriWarn,
               "MacroSize is larger than the MME instruction RAM supports. "
               "Setting it to the maximum: %u groups (+ %u groups for preamble)\n",
               maxNumGroupsPerMacro, numGroupsInMacroPreamble);

        m_MacroSize = maxNumGroupsPerMacro;
    }
    else if (m_MacroSize < numGroupsInMacroPreamble)
    {
        Printf(Tee::PriWarn,
               "MacroSize is smaller than the required macro preamble to "
               "set the MME to a known state. Setting it to the miniumum "
               " value: %u\n", numGroupsInMacroPreamble);

        m_MacroSize = numGroupsInMacroPreamble;
    }

    VerbosePrintf("Macro size set to %u groups: GeneratedGroups(%u) + "
                  "MacroPreamble(%u)\n",
                  m_MacroSize, m_MacroSize - numGroupsInMacroPreamble,
                  numGroupsInMacroPreamble);

    const UINT32 totalNumRegs = s_HighestRegEncoding - s_LowestRegEncoding;
    if (m_GenMaxNumRegisters > totalNumRegs)
    {
        Printf(Tee::PriWarn, "GenMaxNumRegisters larger than the total number "
               "of registers. Setting it to the maximum: %u registers\n",
               totalNumRegs);
        m_GenMaxNumRegisters = MacroGenerator::Config::MAX_NUM_REGISTERS_ALL;
    }

    if (m_GenMaxDataRamEntry > 0 && !m_GenOpsDataRam)
    {
        Printf(Tee::PriWarn, "GenMaxDataRamEntry set but Data RAM are not "
               "being generated (GenOpsDataRam=false). Value will be "
               "ignored.\n");
    }
    else
    {
        const UINT32 usableDataRamSize = m_pMmeMgr->GetUsableDataRamSize();
        if (m_GenMaxDataRamEntry > usableDataRamSize)
        {
            Printf(Tee::PriWarn, "GenMaxDataRamEntry is larger than the usable "
                   "Data RAM size. Setting it to the usable Data RAM size: %u\n",
                   usableDataRamSize);
        }
    }

    // Set the macro gen config
    CHECK_RC(SetMacroGenConfig());

    if (m_PrintDebug && !m_PrintTiming)
    {
        Printf(Tee::PriNormal, "Debug prints enabled. Enabled timing prints.\n");
        m_PrintTiming = true;
    }

    return rc;
}

//!
//! \brief Load as many macro tasks as possible starting the given task index.
//!
//! Attempts to pre-emptively load as many task as possible to keep the
//! instruction RAM full of ready to run macros.
//!
//! Newly loaded tasks are considered incomplete.
//!
//! NOTE: Will not do anything if the given task index is already loaded.
//!
//! Assumptions:
//! - NOTE: Assuming that all macros are the same size. When a macro is freed,
//!   it is guaranteed another macro can be loaded. Can safely assume memory
//!   fragmentation is not an issue.
//! - Tasks are run sequentially in increasing index order, with index wrap.
//!   Ex/ If there are 3 tasks and lwr task is 2, the last three runs, LRU to
//!   MRU are 2, 0, 1.
//! - If a previous task is not completed, it shouldn't be evicted to make room
//! for other tasks.
//!
//! \param pTasks Macro task list.
//! \param nextTaskIndex Index of the task to load in the given macro task list.
//! \return RC::CANNOT_ALLOCATE_MEMORY if there is no room to load the task at
//! the given index.
//!
RC MME64Random::LoadMacroTasks
(
    vector<MacroTask>* pTasks
    ,UINT32 nextTaskIndex
)
{
    // Using Most Recently Used (MRU) eviction policy and load until RAM is
    // full. Will only evict completed macros.
    //
    // Won't evict a task that was loaded on a previous iteration. Newly loaded
    // tasks have their run information reset, including the 'completed' field
    // being set to false. Tasks are only evicted if they are completed.
    //
    // Stops pre-emptively loading when the next task to load wraps back around
    // to the given nextTaskIndex.

    RC rc;
    MASSERT(pTasks);
    MASSERT(nextTaskIndex < pTasks->size());

    // NOP if the given task is loaded
    if (pTasks->at(nextTaskIndex).macro.IsLoaded())
    {
        return rc;
    }

    auto decrementWrap = [](UINT32 i, UINT32 wrapVal)
        {
            return (i + (wrapVal - 1)) % wrapVal;
        };

    const UINT32 numTasks = static_cast<UINT32>(pTasks->size());
    MASSERT(numTasks == pTasks->size()); // Truncation check
    UINT32 mruTaskIndex = decrementWrap(nextTaskIndex, numTasks);
    UINT32 loadTaskIndex = nextTaskIndex;
    bool loading = true;

    while (loading)
    {
        Printf(GetDebugPrintPri(), "Attempting to load macro %u\n", loadTaskIndex);

        MASSERT(loadTaskIndex < pTasks->size());
        MASSERT(mruTaskIndex < pTasks->size());
        MacroTask& loadTask = pTasks->at(loadTaskIndex);
        MacroTask& mruTask = pTasks->at(mruTaskIndex);

        // Assuming macros are all the same size
        MASSERT(loadTask.macro.groups.size() == m_MacroSize);
        MASSERT(mruTask.macro.groups.size() == m_MacroSize);

        // Attempt to load the macro
        RC loadRc = m_pMmeMgr->LoadMacro(&loadTask.macro);
        if (loadRc != OK)
        {
            // Stop if unexpected RC is encountered
            if (loadRc != RC::CANNOT_ALLOCATE_MEMORY)
            {
                return loadRc;
            }

            // Instruction RAM is full, try to evict
            if (mruTask.completed && mruTask.macro.IsLoaded())
            {
                // Can evict the completed MRU task
                Printf(GetDebugPrintPri(), "Evicting macro %u from MME\n", mruTaskIndex);

                CHECK_RC(m_pMmeMgr->FreeMacro(&mruTask.macro));
                MASSERT(!mruTask.macro.IsLoaded());
            }
            else
            {
                // The last MRU task was not completed, so we can't free it to
                // load the next task
                loading = false; // Done loading tasks

                if (loadTaskIndex == nextTaskIndex)
                {
                    // Was not able to allocate any new tasks, send error
                    Printf(GetDebugPrintPri(), "Unable to allocate macro %u\n",
                           loadTaskIndex);

                    return RC::CANNOT_ALLOCATE_MEMORY;
                }
            }
        }
        else
        {
            // Task was loaded
            Printf(GetDebugPrintPri(), "Loaded macro %u\n", loadTaskIndex);

            // Reset the task run information
            loadTask.ResetInfo();

            // Attmpt to load the next task
            loadTaskIndex = (loadTaskIndex + 1) % numTasks;
            // Find next MRU task
            mruTaskIndex = decrementWrap(mruTaskIndex, numTasks);

            if (loadTaskIndex == nextTaskIndex)
            {
                // Wrapped back around to the first task we loaded, everything
                // has been loaded!
                loading = false; // Done loading tasks

                Printf(GetDebugPrintPri(), "All macros loaded\n");
            }
        }
    }

    return rc;
}

RC MME64Random::GenMacroTasks(vector<MacroTask>* pTasks)
{
    RC rc;
    MASSERT(pTasks);

    if (pTasks->empty())
    {
        // First time generation
        pTasks->reserve(m_NumMacrosPerLoop);

        for (UINT32 n = 0; n < m_NumMacrosPerLoop; ++n)
        {
            pTasks->emplace_back();
            CHECK_RC(GenMacroTask(&pTasks->back()));
        }
    }
    else if (m_GenMacrosEachLoop)
    {
        Printf(GetDebugPrintPri(), "Regenerating macros\n");

        // Generate new set of macros
        for (MacroTask& task : *pTasks)
        {
            CHECK_RC(ClearMacroTask(&task));
            CHECK_RC(GenMacroTask(&task));
        }
    }

    return rc;
}

//!
//! \brief Main run loop.
//!
//! Progress is defined by the number of macros we have exelwted over the total
//! number of macros we will execute.
//!
RC MME64Random::InnerRun()
{
    RC rc;
    vector<MacroTask> macroTasks;
    Surface2D mmeDataRamSurf; // Buffer for Data RAM examination

    const UINT32 numLoops = m_pGpuTestConfig->Loops();
    CHECK_RC(PrintProgressInit(numLoops * m_NumMacrosPerLoop));

    TimingMetrics loopTimingMs = {};

    // Run
    //
    for (m_LwrLoop = 0; m_LwrLoop < numLoops; ++m_LwrLoop)
    {
        CHECK_RC(Abort::Check()); // Check if user aborted the run

        loopTimingMs.Reset();
        loopTimingMs.interval.start = Xp::GetWallTimeMS();
        Printf(GetDebugPrintPri(), "Starting loop %u\n", m_LwrLoop);

        // Generate macros
        //
        TIME_SCOPE_MS(loopTimingMs.generation, {
                CHECK_RC(GenMacroTasks(&macroTasks));
            });

        // Run each macro
        //
        for (UINT32 i = 0; i < macroTasks.size(); ++i)
        {
            Printf(GetDebugPrintPri(), "Running macro %u\n", i);

            MacroTask& task = macroTasks.at(i);

            // Check if user aborted the run
            CHECK_RC(Abort::Check());

            // Load macro task (NOP if already loaded)
            //
            // TODO(stewarts): can be smarter about the loading. If we know we
            // don't need to load macro 0 when wrapping if we are regenerating
            // macros. It is just going to need to be evicted.
            TIME_SCOPE_MS(loopTimingMs.loadMme, {
                    CHECK_RC(LoadMacroTasks(&macroTasks, i));
                });
            // TODO(stewarts): Flush to start macros loading before being run?

            // Dump current macro
            if (m_DumpMacros)
            {
                task.macro.Print(Tee::PriNormal);
            }

            // Run on the simulator
            TIME_SCOPE_MS(loopTimingMs.runOnSim, {
                    CHECK_RC(RunOnSim(&task));
                });

            // Check if user aborted the run
            CHECK_RC(Abort::Check());

            if (m_SimOnly) { continue; } // Skip the hw run and result check

            // Run on the MME
            VirtualAddr macroOutputAddr;
            TIME_SCOPE_MS(loopTimingMs.runOnMme, {
                    CHECK_RC(RunOnMme(&task, &macroOutputAddr));
                });

            // Macro task has finished running
            task.completed = true;

            // Check if user aborted the run
            CHECK_RC(Abort::Check());

            // Compare the results
            TIME_SCOPE_MS(loopTimingMs.comparison, {
                    rc = CompareEmissions(task, macroOutputAddr);
                });
            if (rc != OK)
            {
                Printf(Tee::PriError, "Comparison failed: emissions\n");
                GetJsonExit()->AddFailLoop(m_LwrLoop);
                CHECK_RC(rc);
            }

            // Update the simulator's data ram with the MME's data ram state
            if (m_GenOpsDataRam)
            {
                TIME_SCOPE_MS(loopTimingMs.dataRamUpdate, {
                        CHECK_RC(m_pMmeMgr->CopyDataRam(&mmeDataRamSurf));
                        CHECK_RC(UpdateSimDataRamFromMme(mmeDataRamSurf));
                    });
            }

            CHECK_RC(PrintProgressUpdate(m_LwrLoop * m_NumMacrosPerLoop + i));
        }

        // Since it is the end of the loop, all macro tasks can be considered
        // completed.
        // NOTE: This is necessary so there is no leaked memory in the
        // instruction RAM caused by a loaded macro that can't be evicted
        // because it isn't "complete."
        for (MacroTask& task : macroTasks)
        {
            task.completed = true;
        }

        CHECK_RC(EndLoop(m_LwrLoop));
        loopTimingMs.interval.end = Xp::GetWallTimeMS();
        MASSERT(loopTimingMs.IsValid());

        // Print loop timing stats
        if (m_PrintTiming)
        {
            Printf(Tee::PriNormal, "[timing] loop %u: %s\n\n",
                   m_LwrLoop, loopTimingMs.Report().c_str());
        }
    }

    CHECK_RC(PrintProgressUpdate(numLoops * m_NumMacrosPerLoop));

    return rc;
}

//!
//! \brief Run MME64 test.
//!
RC MME64Random::Run()
{
    // Main run
    //
    StickyRC firstRc = InnerRun();

    // Reset test state
    // NOTE: It's possible Run could be called multiple times in a row. Reset to
    // a usable state state so run can be called again.
    //
    RC resetRc = ResetState();
    if (resetRc != OK)
    {
        Printf(Tee::PriWarn, "Failed to reset run state\n");
    }
    firstRc = resetRc;

    return firstRc;
}

//!
//! \brief Run the given macro task on the MME simulator.
//!
//! \param pTask Macro task to be run.
//!
RC MME64Random::RunOnSim(MacroTask* pTask)
{
    RC rc;
    MASSERT(pTask);
    MASSERT(!m_SimCbData.pEmissions); // Avoid dangling pointers
    // Callback prints must reflect testarg selection
    MASSERT(m_SimCbData.traceEnabled == m_SimTrace);

    // Store the simulator emissions in the macro task
    m_SimCbData.pEmissions = &pTask->simEmissions;

    rc = m_pSimulator->Run(&pTask->macro, m_SimTestConfig);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Simulator failed to run\n");
        GetJsonExit()->AddFailLoop(m_LwrLoop);
        CHECK_RC(rc);
    }
    // Should have valid cycle count after being run on simulator
    MASSERT(pTask->macro.cycleCount != Macro::CYCLE_COUNT_UNKNOWN);

    // Dump simulator emissions
    if (m_DumpSimEmissions)
    {
        CHECK_RC(DumpSimEmissions(Tee::PriNormal, pTask->simEmissions));
    }

    // Ensure the emissions counts are correct
    if (pTask->macro.numEmissions != m_SimCbData.pEmissions->size())
    {
        Printf(Tee::PriError,
               "Number of emissions do not agree\n"
               "\tGen(%u), Sim(%u)\n",
               pTask->macro.numEmissions,
               static_cast<UINT32>(m_SimCbData.pEmissions->size()));

        return RC::SOFTWARE_ERROR;
    }

    m_SimCbData.pEmissions = nullptr;

    return rc;
}

//!
//! \brief Run the given macro task on the MME.
//!
//! \param pTask Macro task to run.
//! \param pMacroOutputAddr GPU virtual address to the start of the MME's I2M
//! emissions in the GPU framebuffer.
//!
RC MME64Random::RunOnMme(MacroTask* pTask, VirtualAddr* pMacroOutputAddr)
{
    RC rc;
    MASSERT(pTask);
    MASSERT(pTask->macro.IsLoaded()); // Macro should be loaded
    MASSERT(pMacroOutputAddr);

    rc = m_pMmeMgr->RunMacro(pTask->macro, pMacroOutputAddr);
    if (rc != OK)
    {
        Printf(Tee::PriError, "MME failed to run\n");
        GetJsonExit()->AddFailLoop(m_LwrLoop);
        CHECK_RC(rc);
    }

    // Dump MME I2M Emissions
    if (m_DumpMmeEmissions)
    {
        CHECK_RC(DumpMmeI2mEmissions(Tee::PriNormal, *pMacroOutputAddr,
                                     pTask->macro.numEmissions));
    }

    return rc;
}

//!
//! \brief Return the number of groups in each macro's required preamble.
//!
/* static */ constexpr
UINT32 MME64Random::GetNumGroupsInMacroPreamble()
{
    return MacroGenerator::GetNumGroupsInMmeConfigSetup(s_NumGroupAlus)
        + MacroGenerator::GetNumGroupsInRegfileSetter(s_NumGroupAlus);
}

//!
//! \brief Reset state to Run's expected start state.
//!
RC MME64Random::ResetState()
{
    MASSERT(m_pMmeMgr);
    return m_pMmeMgr->FreeAll();
}

//!
//! \brief Set the macro generation configuration
//!
RC MME64Random::SetMacroGenConfig()
{
    RC rc;

    unique_ptr<MacroGenerator::Config> pGenConfig
        = make_unique<MacroGenerator::Config>();

    pGenConfig->flags = OpSelectionFlags(0);

    if (m_GenOpsArithmetic)
    {
        pGenConfig->flags
            = OpSelectionFlags(pGenConfig->flags | OPS_ARITHMETIC);
    }

    if (m_GenOpsLogic)
    {
        pGenConfig->flags
            = OpSelectionFlags(pGenConfig->flags | OPS_LOGIC);
    }

    if (m_GenOpsComparison)
    {
        pGenConfig->flags
            = OpSelectionFlags(pGenConfig->flags | OPS_COMP);
    }

    if (m_GenOpsDataRam)
    {
        pGenConfig->flags
            = OpSelectionFlags(pGenConfig->flags | OPS_DATA_RAM);
    }

    if (pGenConfig->flags == OpSelectionFlags(0))
    {
        Printf(Tee::PriError, "No operation selections\n");
        return RC::ILWALID_INPUT;
    }

    pGenConfig->percentFlowControlOps = 0.0f;
    pGenConfig->noLoad = m_GenNoLoad;
    pGenConfig->noSendingMethods = m_GenNoSendingMethods;
    pGenConfig->unconditionalPredMode = m_GenUncondPredMode;
    pGenConfig->maxNumRegisters = m_GenMaxNumRegisters;
    pGenConfig->maxDataRamEntry = m_GenMaxDataRamEntry;

    CHECK_RC(m_pMmeIGen->SetGenConfig(std::move(pGenConfig)));

    // Test the config can produce valid groups
    Macro testMacro;
    rc = m_pMmeIGen->GenMacro(1/*numGroups*/, 1/*usableDataRamSize*/,
                              &testMacro);
    if (rc == RC::ILWALID_INPUT)
    {
        Printf(Tee::PriError, "Provided generation configuration cannot "
               "produce a valid Group\n");
    }

    return rc;
}

//!
//! \brief Generate a macro
//!
RC MME64Random::GenMacro(Macro* pMacro) const
{
    RC rc;
    MASSERT(pMacro);
    MASSERT(pMacro->mmeTableEntry == Macro::NOT_LOADED);
    pMacro->Reset();

    const UINT32 numGroupsInPreamble = MME64Random::GetNumGroupsInMacroPreamble();

    // Internal consistency check, make sure each section of the macro
    // preamble matches the expected values.
    UINT32 lastMacroSize = static_cast<UINT32>(pMacro->groups.size());
    MASSERT(lastMacroSize == pMacro->groups.size()); // Truncation check
    (void)lastMacroSize;

    // Generate the macro preamble
    //
    // Set MME state to ignore emissions
    CHECK_RC(m_pMmeIGen->GenMmeConfigSetup(pMacro));
    MASSERT(pMacro->groups.size()
            == MacroGenerator::GetNumGroupsInMmeConfigSetup(s_NumGroupAlus));
    lastMacroSize = static_cast<UINT32>(pMacro->groups.size());
    MASSERT(lastMacroSize == pMacro->groups.size()); // Truncation check

    // Set the register file to a known state
    //
    // NOTE: We can't assume that the regfile is in some known state when a new
    // macro is being run. This includes the method register.
    //
    // Ref: http://p4viewer/get/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_FD.docx //$
    //   Section 5.1.3 MME CPU - "The Regfile does not zero all regs on macro
    //   start. [...] The register file is context switched."
    CHECK_RC(m_pMmeIGen->GenRegfileSetter(true/*randRegfile*/, pMacro));
    MASSERT(pMacro->groups.size() - lastMacroSize
            == MacroGenerator::GetNumGroupsInRegfileSetter(s_NumGroupAlus));
    lastMacroSize = static_cast<UINT32>(pMacro->groups.size());
    MASSERT(lastMacroSize == pMacro->groups.size()); // Truncation check
    MASSERT(pMacro->groups.size() == numGroupsInPreamble);

    // Randomly generate the remaining portion of the macro.
    //
    const UINT32 numGenedGroups = m_MacroSize - numGroupsInPreamble;
    CHECK_RC(m_pMmeIGen->GenMacro(numGenedGroups,
                                  m_pMmeMgr->GetUsableDataRamSize(), pMacro));
    MASSERT(pMacro->numEmissions != Macro::NUM_EMISSIONS_UNKNOWN);
    MASSERT(pMacro->groups.size() == m_MacroSize);

    return rc;
}

//!
//! \brief Clear given macro. Frees macro if it is loaded.
//!
//! \param pMacro Macro to clear.
//!
RC MME64Random::ClearMacro(Macro* pMacro)
{
    RC rc;
    MASSERT(pMacro);

    // Free loaded macros
    if (pMacro->mmeTableEntry != Macro::NOT_LOADED)
    {
        CHECK_RC(m_pMmeMgr->FreeMacro(pMacro));
        MASSERT(pMacro->mmeTableEntry == Macro::NOT_LOADED);
    }

    pMacro->Reset();

    return rc;
}

//!
//! \brief Generate a new macro task.
//!
//! \param[out] pTask Macro task to populate.
//!
RC MME64Random::GenMacroTask(MacroTask* pTask) const
{
    RC rc;
    MASSERT(pTask);
    MASSERT(!pTask->macro.IsLoaded());

    // Generate a new macro
    CHECK_RC(GenMacro(&pTask->macro));

    return rc;
}

//!
//! \brief Clear the given macro task.
//!
//! \param[out] pTask Macro task to clear.
//!
RC MME64Random::ClearMacroTask(MacroTask* pTask)
{
    RC rc;
    MASSERT(pTask);

    // Clear the existing macro
    CHECK_RC(ClearMacro(&pTask->macro));

    // Reset the task information
    pTask->ResetInfo();

    return rc;
}

//!
//! \brief Fill the simulator and the MME Data RAM with the same state using
//! DMA.
//!
//! \param randValue If true, randomize the values in the Data RAM. Otherwise
//! zero the Data RAM.
//!
RC MME64Random::FillDataRamsViaDma(bool randValues)
{
    RC rc;
    const UINT32 usableDataRamSize = m_pMmeMgr->GetUsableDataRamSize();
    vector<UINT32> memory;
    memory.reserve(usableDataRamSize);

    // Fill simulator data ram
    //
    m_SimDataRam.Clear();
    m_SimDataRam.Reserve(usableDataRamSize);
    for (UINT32 addr = 0; addr < usableDataRamSize; ++addr)
    {
        UINT32 val = 0;
        if (randValues)
        {
            val = m_pFpCtx->Rand.GetRandom();
        }

        // Set the simulator
        m_SimDataRam.Set(addr, val);

        // Record the value
        memory.push_back(val);
    }

    // Fill the MME data ram
    //
    CHECK_RC(m_pMmeMgr->FillDataRam(memory));

    return rc;
}

//!
//! \brief Fill the MME Data RAM using MME macros.
//!
//! NOTE: Clears all the macros from the Data RAM due to the large macros
//! required to initialize the full data ram.
//!
//! \param randValue If true, randomize the values in the Data RAM. Otherwise
//! zero the Data RAM.
//!
RC MME64Random::FillDataRamsViaMacros(bool randValues)
{
    RC rc;
    Macro dataRamSetter;

    const UINT32 dataRamSize = m_pMmeMgr->GetUsableDataRamSize();
    const UINT32 instrMemSize = m_pMmeMgr->GetInstrRamSize();

    // Find a macro size that fits in the data ram.
    // @Performance Could be smarter about what size is selected.
    UINT32 numEntriesPerMacro = dataRamSize; // Num data ram entries set per macro
    while (instrMemSize
           < (MacroGenerator::GetNumGroupsInDataRamSetter(s_NumGroupAlus,
                                                          numEntriesPerMacro)
              * Group<s_NumGroupAlus>::GetElementCount()))
    {
        numEntriesPerMacro /= 2;
        MASSERT(numEntriesPerMacro > 0);
    }

    // Set the Data RAM
    //
    // Clear the macros from the mme
    CHECK_RC(m_pMmeMgr->FreeAll());

    UINT32 nextEntry = 0; // Next data ram entry to set
    while (nextEntry < dataRamSize)
    {
        // Number of data ram entries to set in this macro. Clamp to the end of
        // the data ram.
        UINT32 numToSet = std::min(numEntriesPerMacro, dataRamSize - nextEntry);

        Printf(Tee::PriNormal, "Setting MME Data RAM entries %u to %u\n",
               nextEntry, nextEntry + numToSet - 1);

        dataRamSetter.Reset();
        CHECK_RC(m_pMmeIGen->GenDataRamSetter(nextEntry, numToSet,
                                              randValues, &dataRamSetter));

        if (m_DumpMacros)
        {
            Printf(Tee::PriNormal,
                   "Dumping MME Data RAM initialization macro...\n");
            dataRamSetter.Print(Tee::PriNormal);
        }

        // Set the HW MME data ram via macro
        CHECK_RC(m_pMmeMgr->LoadMacro(&dataRamSetter));
        CHECK_RC(m_pMmeMgr->RunMacro(dataRamSetter, nullptr));

        // Clear the macros from the mme
        CHECK_RC(m_pMmeMgr->WaitIdle()); // Wait for MME to complete
        CHECK_RC(m_pMmeMgr->FreeAll());

        nextEntry += numToSet;
    }

    // Update the simulator data ram input from the MME
    //
    m_SimDataRam = Simulator::Memory(0, dataRamSize);
    Surface2D mmeDataRamSurf;
    CHECK_RC(m_pMmeMgr->CopyDataRam(&mmeDataRamSurf));
    CHECK_RC(UpdateSimDataRamFromMme(mmeDataRamSurf));

    return rc;
}

//!
//! \brief Compare the emissions from the simulator and the MME.
//!
//! \param macro Macro that was run.
//! \param simCbData Callback data from the MME simulator.
//! \param mmeMacroOutputAddr GPU DMA address of the MME's I2M output buffer.
//!
RC MME64Random::CompareEmissions
(
    const MacroTask& task
    ,VirtualAddr mmeMacroOutputAddr
) const
{
    RC rc;
    const Macro& macro = task.macro;
    const vector<Emission>& simEmissions = task.simEmissions;
    UINT32* pMacroOutput = reinterpret_cast<UINT32*>(mmeMacroOutputAddr);
    I2mEmission i2mEmission;
    bool hasMiscompare = false;

    // Simulator and the generator should agree on the number of emissions
    MASSERT(simEmissions.size() == macro.numEmissions);
    // Should have valid cycle count from being run
    MASSERT(macro.cycleCount != Macro::CYCLE_COUNT_UNKNOWN);

    // Compare each emission
    //
    // NOTE: The MME I2M emissions do not contain the method increment part
    // of the methd emission like the simulator does. Method increment is
    // ignored during this comparison; it is only checked to confirm it is a
    // valid value.
    //
    UINT32 lwrEmission = 0;      // Current emission number of total
    UINT32 lwrGroupEmission = 0; // Current emission number for current group
    UINT32 lastPc = 0;
    bool abortComparison = false;
    while (!abortComparison && lwrEmission < macro.numEmissions)
    {
        // Get MME's emission
        UINT32 outputLo = MEM_RD32(pMacroOutput);
        ++pMacroOutput;
        UINT32 outputHi = MEM_RD32(pMacroOutput);
        ++pMacroOutput;
        i2mEmission = I2mEmission(outputLo, outputHi);

        const UINT32 lwrPc = i2mEmission.GetPC();
        MASSERT(lwrPc != _UINT32_MAX); // Check for default value collision

        if (lwrPc != lastPc)
        {
            // Found a new group, reset the group emission counter.
            lwrGroupEmission = 0;
        }

        if (i2mEmission != simEmissions[lwrEmission])
        {
            using namespace Utility;

            hasMiscompare = true;
            string msg;

            // Check if current emission is from the same group as the last
            // emission.
            if (lwrPc != lastPc)
            {
                // This is the first emission from a group. Reset the group
                // emission counter.
                lwrGroupEmission = 0;
            }

            msg = StrPrintf("Miscompare detected: Emission %u @ PC 0x%x "
                            "(Group %u's emission %u)\n", lwrEmission,
                            lwrPc, lwrPc, lwrGroupEmission);
            if (i2mEmission.IsValid())
            {
                msg += StrPrintf("\t(method, data): Sim(0x%03x, 0x%08x), "
                                 "MME(0x%03x, 0x%08x)\n",
                                 simEmissions[lwrEmission].GetMethod(),
                                 simEmissions[lwrEmission].GetData(),
                                 i2mEmission.GetMethod(), i2mEmission.GetData());
                Printf(Tee::PriError, "%s", msg.c_str());
            }
            else
            {
                const UINT32 upper = i2mEmission.GetUpper();
                const UINT32 lower = i2mEmission.GetLower();
                msg += StrPrintf("\tMME inline-to-memory produced invalid emission: "
                                 "0x%08x %08x\n @ PC %u", upper, lower, lwrPc);
                Printf(Tee::PriError, "%s\n", msg.c_str());

                // If the I2M emission is all zero, it is very possible that the
                // MME did not produce enough emissions and one or more of the
                // LoadInlineData methods used to pad the I2M transaction with
                // zero has been encountered. This implies that the MME did not
                // produce enough emissions.
                //
                // NOTE: Can't count on triggering this when the MME has less
                // emissions than expected for two reasons:
                // - Can't guarantee an I2M transaction will end with zero
                //   padding. Some I2M transactions may not need padding.
                //   Unclear what happens when I2M output buffer is read that
                //   has not yet been written by LoadInlineData.
                // - MME may only produce part of a I2M emission (one
                //   LoadInlineData for an emission instead of two). In this
                //   case only half of a DWORD will be all zero from
                //   LoadInlineData padding.
                //
                // TODO(stewarts): This means that the I2M transaction is still
                // open!!! It will need to be closed before the next method is
                // sent to the GPU. If another method has been written to the
                // push buffer but not flushed, it will need to be cleared to
                // avoid a HWW.
                if (upper == 0 && lower == 0)
                {
                    Printf(Tee::PriError, "Possibly less emissions than expected:\n"
                           "\tExpected emissions: %u, Current invalid emission: %u\n",
                           macro.numEmissions, lwrEmission);
                }

                // If the MME's emission is invalid, something went
                // seriously wrong. Don't bother checking the rest of the
                // emissions.
                Printf(Tee::PriError, "Aborting comparison\n");
                abortComparison = true;
                break;
            }
        }

        // Record the PC
        lastPc = lwrPc;

        // Next emission
        ++lwrEmission;
        ++lwrGroupEmission;
    }

    // NOTE: Lwrrently not thoroughly checking if the number of emissions from
    // MME exceed the number of expected emissions. In theory this should never
    // need to happen. Setting up I2M requires specifying the expected number of
    // LoadInlineData methods, which includes the expected emissions and the
    // padding to complete the transaction. Excess emissions would add
    // additional LoadInlineData method that should cause a "Too Many
    // LoadInlineData" HWW.

    if (!abortComparison && (lwrEmission != simEmissions.size()))
    {
        const UINT32 numSimEmissions = static_cast<UINT32>(simEmissions.size());
        MASSERT(numSimEmissions == simEmissions.size()); // Truncation check

        Printf(Tee::PriError, "Total emissions counted per Group does not "
               "match the number of simulator emissions\n"
               "\tGen(%u), Sim(%u)\n", lwrEmission, numSimEmissions);
        return RC::SOFTWARE_ERROR;
    }

    // NOTE: The MME I2M emissions provide the PC, and the simulator provides
    // the cycle count. Because each one does not provide the other value, they
    // can't be validated. Additionally, the simualtor provides the final cycle
    // count. The MME I2M emissions only provide values at the time of the
    // emission. The last emissions may not necessarily be emitted by the last
    // group in the macro.

    if (hasMiscompare)
    {
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    return rc;
}

RC MME64Random::DumpSimEmissions
(
    Tee::Priority printLevel
    ,const vector<Emission>& emissions
) const
{
    RC rc;
    string s;

    for (const auto& emission : emissions)
    {
        s += emission.ToString();
        s += '\n';
    }

    Printf(printLevel, "Dumping simulator emissions...\n%s", s.c_str());

    return rc;
}

RC MME64Random::DumpMmeI2mEmissions
(
    Tee::Priority printLevel
    ,VirtualAddr mmeMacroOutputAddr
    ,UINT32 numEmissions
) const
{
    RC rc;
    UINT32* pMacroOutput = reinterpret_cast<UINT32*>(mmeMacroOutputAddr);
    I2mEmission i2mEmission;
    string s;

    for (UINT32 i = 0; i < numEmissions; ++i)
    {
        UINT32 outputLo = MEM_RD32(pMacroOutput);
        ++pMacroOutput;
        UINT32 outputHi = MEM_RD32(pMacroOutput);
        ++pMacroOutput;

        i2mEmission = I2mEmission(outputLo, outputHi);

        s += i2mEmission.ToString();
        s += '\n';
    }

    Printf(printLevel, "Dumping MME emissions...\n%s", s.c_str());

    return rc;
}

//!
//! \brief Update the simulator's Data RAM state input with the given MME's data
//! RAM state.
//!
//! \param mmeDataRamSurf Surface containing the state of the MME's Data RAM.
//! Displayable surface must be allocated contiguously.
//!
RC MME64Random::UpdateSimDataRamFromMme
(
    const Surface2D& mmeDataRamSurf
)
{
    RC rc;
    MASSERT(mmeDataRamSurf.IsAllocated() && mmeDataRamSurf.IsGpuMapped());
    const UINT32 dataRamSize = m_pMmeMgr->GetUsableDataRamSize();
    MASSERT(m_SimDataRam.GetNumEntries() == dataRamSize);
    MASSERT(mmeDataRamSurf.GetSize() / 4 == dataRamSize); // bytes / 4 == DWORD

    // Compare both data rams
    //
    UINT32* pMmeDataRamAddr
        = reinterpret_cast<UINT32*>(mmeDataRamSurf.GetAddress());

    for (UINT32 addr = 0; addr < dataRamSize; ++addr)
    {
        // Expectation is that the simulator data ram was set in accending order
        // starting with address zero. This makes the data RAM address
        // equivalent to the data ram entry number.
        const auto simEntry = m_SimDataRam.GetEntry(addr);
        UINT32 simAddr = simEntry.first;
        UINT32 simValue = simEntry.second;
        UINT32 mmeValue = MEM_RD32(pMmeDataRamAddr);
        ++pMmeDataRamAddr;

        // Simulator data ram consistency check
        if (simAddr != addr)
        {
            Printf(Tee::PriError, "Simulator's Data RAM does not directly "
                   "correspond to the MME Data RAM.\n"
                   "\tData RAM address: %u, Sim address: %u\n", addr, simAddr);
            MASSERT(!"See previous error");
            return RC::SOFTWARE_ERROR;
        }

        // Update any data ram miscompares with the MME's value
        if (simValue != mmeValue)
        {
            m_SimDataRam.SetEntry(addr, addr, mmeValue);
        }
    }

    return rc;
}

RC MME64Random::Cleanup()
{
    StickyRC firstRc;

    if (!m_SimOnly)
    {
        // Cleanup MME64 manager
        firstRc = m_pMmeMgr->Cleanup();
    }

    // Owned objects
    //
    m_pMmeMgr.reset();
    m_pSimulator.reset();
    m_pMmeIGen.reset();

    // Cleanup allocated surface (AllocDisplayAndSurface)
    firstRc = GpuTest::Cleanup();

    return firstRc;
}

//!
//! \brief Simualtor callback for when no emissions are expected.
//!
//! This callback is only called when the simulator releases an emission.
//! Therefore, this function should never be called.
//!
//! \param callbackData The callback data in the simulator run configuration.
//! \param method The 12-bit method address part of the emission. Does not
//! include method increment.
//! \param data Data part of the emission.
//!
void MME64Random::NoEmissionSimulatorCallback
(
    void* callbackData
    ,unsigned int method
    ,unsigned int data
)
{
    MASSERT(!"Unexpected emission from simulator exelwtion");
}

//!
//! \brief Simulator callback triggered for every emission.
//!
//! NOTE: The 'method address' and referring to a 'class method', or just
//! 'method', refer to the same thing. It is the method register that has a
//! 'method increment' and a 'method address' part. The emissions do not include
//! the method increment, only the method address, or just method.
//!
//! \param callbackData The callback data in the simulator run configuration.
//! \param method The 12-bit method address part of the emission. Does not
//! include method increment.
//! \param data Data part of the emission.
//!
void MME64Random::SimulatorCallback
(
    void* callbackData
    ,unsigned int method
    ,unsigned int data
)
{
    MASSERT(!(method & ~0x0fff)); // Method is 12-bits
    SimCallbackData* pSimCbData
        = reinterpret_cast<SimCallbackData*>(callbackData);

    if (pSimCbData->traceEnabled)
    {
        Printf(Tee::PriNormal,
               "[mme64sim] Callback: Method(0x%03x), Data(0x%08x)\n",
               method, data);
    }

    // Record method-data pair
    pSimCbData->pEmissions->emplace_back(method, data);
}

//!
//! \brief Get the priority of debug prints.
//!
Tee::Priority MME64Random::GetDebugPrintPri() const
{
    return (m_PrintDebug ? Tee::PriNormal : Tee::PriSecret);
}

void MME64Random::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "MME64Random Js Properties:\n");
    Printf(pri, "    MacroSize:               %u\n", m_MacroSize);
    Printf(pri, "    SimOnly:                 %s\n", StrBool(m_SimOnly));
    Printf(pri, "    SimTrace:                %s\n", StrBool(m_SimTrace));
    Printf(pri, "    DumpMacros:              %s\n", StrBool(m_DumpMacros));
    Printf(pri, "    DumpSimEmissions:        %s\n", StrBool(m_DumpSimEmissions));
    Printf(pri, "    DumpMmeEmissions:        %s\n", StrBool(m_DumpMmeEmissions));
    Printf(pri, "    NumMacrosPerLoop:        %u\n", m_NumMacrosPerLoop);
    Printf(pri, "    GenMacrosEachLoop:       %s\n", StrBool(m_GenMacrosEachLoop));
    Printf(pri, "    GenOpsArithmetic:        %s\n", StrBool(m_GenOpsArithmetic));
    Printf(pri, "    GenOpsLogic:             %s\n", StrBool(m_GenOpsLogic));
    Printf(pri, "    GenOpsComparison:        %s\n", StrBool(m_GenOpsComparison));
    Printf(pri, "    GenOpsDataRam:           %s\n", StrBool(m_GenOpsDataRam));
    Printf(pri, "    GenNoLoad:               %s\n", StrBool(m_GenNoLoad));
    Printf(pri, "    GenSendingMethods:       %s\n", StrBool(m_GenNoSendingMethods));
    Printf(pri, "    GenUncondPredMode:       %s\n", StrBool(m_GenUncondPredMode));
    Printf(pri, "    GenMaxNumRegisters:      %u\n", m_GenMaxNumRegisters);
    Printf(pri, "    GenMaxDataRamEntry:      %u\n", m_GenMaxDataRamEntry);
    Printf(pri, "    PrintTiming:             %s\n", StrBool(m_PrintTiming));
    Printf(pri, "    PrintDebug:              %s\n", StrBool(m_PrintDebug));
}

JS_CLASS_INHERIT(MME64Random, GpuTest, "MME64 Random");

CLASS_PROP_READWRITE(MME64Random, MacroSize, UINT32,
                     "Number of MME64 Groups per Macro");
CLASS_PROP_READWRITE(MME64Random, SimOnly, bool,
                     "Only run the software simulator");
CLASS_PROP_READWRITE(MME64Random, SimTrace, bool,
                     "Show the simulator exelwtion trace");
CLASS_PROP_READWRITE(MME64Random, DumpMacros, bool,
                     "Dump each macro's details");
CLASS_PROP_READWRITE(MME64Random, DumpSimEmissions, bool,
                     "Dump simulator emissions for each macro");
CLASS_PROP_READWRITE(MME64Random, DumpMmeEmissions, bool,
                     "Dump MME emissions for each macro");
CLASS_PROP_READWRITE(MME64Random, NumMacrosPerLoop, UINT32,
                     "Number of macros run per loop");
CLASS_PROP_READWRITE(MME64Random, GenMacrosEachLoop, bool,
                     "If true, generate new macros each loop");
CLASS_PROP_READWRITE(MME64Random, GenOpsArithmetic, bool,
                     "Generate Groups with arithmetic operations");
CLASS_PROP_READWRITE(MME64Random, GenOpsLogic, bool,
                     "Generate Groups with logic operations");
CLASS_PROP_READWRITE(MME64Random, GenOpsComparison, bool,
                     "Generate Groups with comparison operations");
CLASS_PROP_READWRITE(MME64Random, GenOpsDataRam, bool,
                     "Generate Groups with Data RAM operations");
CLASS_PROP_READWRITE(MME64Random, GenNoLoad, bool,
                     "Generate Groups without LOADn");
CLASS_PROP_READWRITE(MME64Random, GenNoSendingMethods, bool,
                     "Generate Groups that don't send methods down the pipeline");
CLASS_PROP_READWRITE(MME64Random, GenUncondPredMode, bool,
                     "Generate Groups with unconditional predicate mode");
CLASS_PROP_READWRITE(MME64Random, GenMaxNumRegisters, UINT32,
                     "Limit register use to the first n registers");
CLASS_PROP_READWRITE(MME64Random, GenMaxDataRamEntry, UINT32,
                     "Limit generated Data RAM accesses to the first n entries");
CLASS_PROP_READWRITE(MME64Random, PrintTiming, bool,
                     "Print timing breakdown of each loop");
CLASS_PROP_READWRITE(MME64Random, PrintDebug, bool,
                     "Print debug information");

#undef TIME_SCOPE_MS
