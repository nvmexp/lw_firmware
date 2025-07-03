/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   mmerand.cpp
 * @brief  Implemetation of a Method Macro Expander random
 *         program test.
 *
 */

#include "gputest.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"
#include "lwos.h"
#include "core/include/utility.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "cheetah/include/teggpufirewallown.h"
#include "core/include/jsonlog.h"

#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06fsubch.h"
#include "class/cla097.h" // KEPLER_A
#include "class/clc397.h" // VOLTA_A
#include "gpu/utility/mmeigen.h"
#include "gpu/utility/mmesim.h"

#include <vector>
#include <set>

//! The minimum macro size must contain enough space to initialize all
//! the MME registers plus one instruction to setup the carry bit,
//! plus 2 instructions (an instruction with the EndingNext
//! flag set, and the actual last instruction)
#define MIN_MACRO_SIZE         (m_pMmeIGen->GetRegCount() + 1 + 2)

//! The minimum block size to create a branch is based on the worst case of
//! a backward branch which requires:
//!    Src0 Load
//!    <block body>
//!    Src0 Decrement
//!    Branch Instruction
//!    NOP Instruction (for the delay slot)
#define MIN_BRA_BLOCK_SIZE                                   4

//! Eight output bytes are generated per MME release
#define MME_BYTES_PER_RELEASE                                8

//! Number of NOPs to insert in the channel after perfoming a R-M-W via the
//! Falcon engine
#define FALCON_PRI_RMW_NOP_COUNT                            10

//! Amount of padding to send at a time
#define PADDING_BLOCK_SIZE                                  2048

//! Instruction offsets from the start of a fixed program to its release
//! instruction
#define SHADOW_INIT_RELEASE_OFFSET                          2
#define SHADOW_VERIF_RELEASE_OFFSET                         4

//! MME Random Test.  MME is short for Method Macro Expander
//!
//! The Method Macro Expander is capable of receiving arbitrary programs
//! using a very simple instruction set (see gpu/utility/mmeigen.h for
//! details).  The macros are both programmed and run using push buffer
//! commands.  The output of the MME is method/data pairs which are then
//! passed on to the rest of host processing.  The MME has a shadow RAM
//! which can track, filter, and replay previous macro output.
//!
//! This test creates random macro programs and runs them through a
//! MME and shadow RAM simulator to determine what the output should
//! be.  The macros are then run on hardware, but instead of passing on
//! the output of the MME to host, the macro output is used as input
//! data to an Inline transfer and the output results are
//! checked against the expected output from the simulator.
//
//! For the purposes of this test the MME shadow RAM is placed into
//! passthrough mode.  The current shadow RAM model and MME test would
//! have to be adapted to support filtered shadow RAM mode.
//!
//! The random macro generator was adapted from an RTL unit testbench
//! at //hw/fermi1_gf100/stand_sim/fe/t_mme_common.cpp and
//! //hw/fermi1_gf100/stand_sim/fe/t_mme_random.cpp
//!
class MMERandomTest: public GpuTest
{
public:
    MMERandomTest();

    bool IsSupported();
    RC   Setup();
    RC   Run();
    RC   Cleanup();

    RC SetDefaultsForPicker(UINT32 Idx);

    SETGET_PROP(DumpMacroOnMiscompare, bool);
    SETGET_PROP(DumpMacrosPreRun, bool);
    SETGET_PROP(DumpMacrosPostRun, bool);
    SETGET_PROP(TraceStartLoop, UINT32);
    SETGET_PROP(TraceEndLoop, UINT32);
    SETGET_PROP(OnlyExecTracedLoops, bool);
    SETGET_PROP(VerifyShadowRam, bool);
    SETGET_PROP(ShowSurfaceProgress, bool);
    SETGET_PROP(PrintSummary, bool);

private:
    //! This enum describes the subchannels used by the MMERandomTest
    enum
    {
        s_3D = LWA06F_SUBCHANNEL_3D
    };

    //! This enum describes the types of branches for the macro
    enum
    {
        NoBranch,
        ForwardBranch,
        BackwardBranch
    };

    //! This structure describes a MME Macro
    struct MmeMacro
    {
        UINT32 StartAddr;   //!< Start address of macro in instruction RAM
        UINT32 Size;        //!< Size of the macro in instructions
        UINT32 Index;       //!< Macro Index in start address RAM
        UINT32 Loads;       //!< Number of loads (32-bit inputs) that the macro
                            //!< requires
        UINT32 Releases;    //!< Number of releases (64-bit outputs) that the
                            //!< macro generates
        vector<UINT32> Instructions;  //!< Actual macro instructions
    };

    //! This structure describes a MME Macro that may be run during a frame
    //! of exelwtion of the MMERandomTest (one macro is exelwted per loop)
    struct MacroToRun
    {
        bool           bSkip;          //!< True to skip exelwtion of the macro
                                       //!< because it is invalid
        bool           bSkippedForTracing; //!< True if this macro is skipped
                                           //!< because it is untraced (not
                                           //!< because it is invalid)
        MmeMacro      *pMacro;         //!< Pointer to the macro to run
        vector<UINT32> InputData;      //!< Input data for the macro
        vector<UINT32> OutputData;     //!< Output data for the macro
    };

    // Pointers to generic test data
    GpuTestConfiguration   *m_pTestConfig = nullptr;
    FancyPicker::FpContext *m_pFpCtx      = nullptr;
    FancyPickerArray       *m_pPickers    = nullptr;

    // Channel parameters.
    Channel *               m_pCh = nullptr;
    LwRm::Handle            m_hCh = 0;

    // HW Class allocators
    ThreeDAlloc             m_ThreeD;
    UINT32                  m_HwClass = 0;

    //! Output surface where the MME will place all of its generated methods
    //! via an inline MemToMem transfer
    Surface2D              *m_pSurface = nullptr;

    //!< Notifier for MemToMem completion
    Notifier                m_Notifier;

    //! Instruction generated for the MME instructions
    MmeInstGenerator       *m_pMmeIGen = nullptr;

    //! Simulator for running MME programs
    MMESimulator           *m_pMmeSim  = nullptr;

    //! List of macro indecies.  Used to keep track of which macro indecies
    //! have already been generated for a given frame.  Ensures that macros
    //! with duplicate indecies are not generated.
    vector<UINT32>          m_MacroIndecies;

    //! List of macros generated for the current frame.  Enough macros are
    //! generated to fill up instruction memory, not all macros will
    //! necessarily be run.
    vector<MmeMacro>        m_Macros;

    //! Shadow ram initialization and verification macros
    MmeMacro m_ShadowInitMacro;
    MmeMacro m_ShadowVerifyMacro;

    //! Total size (instructions) of the shadow RAM macros being run in the
    //! frame
    UINT32 m_ShadowRamMacroSize  = 0;

    //! Number of the shadow RAM macros being run in the frame
    UINT32 m_ShadowRamMacroCount = 0;

    //! List of macros to actually be run on the current frame.  One macro
    //! corresponds to a "loop" each entry may either be run or skipped
    //! depending upon whether its output will fit onto the output surface.
    vector<MacroToRun>      m_MacrosToRun;

    //! Current start address in Macro Instruction RAM where the next
    //! random macro will reside
    UINT32                  m_MacroStartAddr = 0;

    //! Number of loops (macros) to run per "frame"
    UINT32                  m_LoopsPerFrame  = 200;

    //! Number of retries that will occur when picking macros before deciding
    //! that the current loop will be skipped
    UINT32                  m_PickRetries    = 5;

    //! Size of the instruction RAM, may vary depending upon GPU
    UINT32                  m_MmeInstructionRamSize = 2048;

    //! State of shadow RAM
    MMEShadowSimulator * m_pShadowState = nullptr;

    //! Background color used for fill
    UINT32 m_BgColor     = 0;

    //! Amount to shift methods by to colwert from raw method to value defined
    //! in header files
    UINT32 m_MethodShift = 0;

    //! Dummy semaphore to use for flushing
    Surface2D m_FlushSemaphore;

    //! Object which lets us do the testing on CheetAh
    TegraGpuRegFirewallOwner m_GpuRegFirewall;

    // Variables set from JS
    bool   m_DumpMacroOnMiscompare = false;
    bool   m_DumpMacrosPreRun      = false;
    bool   m_DumpMacrosPostRun     = false;
    UINT32 m_TraceStartLoop        = ~0U;
    UINT32 m_TraceEndLoop          = ~0U;
    bool   m_OnlyExecTracedLoops   = false;
    bool   m_VerifyShadowRam       = false;
    bool   m_ShowSurfaceProgress   = false;
    bool   m_PrintSummary          = false;

    RC   RandomizeOncePerRestart(const UINT32 Seed);
    void RandomizeShadowState();
    RC   SetupShadowRamMacros(UINT32 *pOutputBytes);

    void   CreateMacro(MmeMacro * const pMacro);
    UINT32 GetMacroSize();
    void   AddInstruction(MmeMacro * const pMacro,
                          const bool bEndingNext,
                          const UINT32 blockLoad,
                          const bool bReleaseOnly,
                          const set<UINT32> &ValidOutputRegs);
    bool   IsOutputDataValid(const vector<UINT32> &OutputData);
    RC DownloadMacro(const MmeMacro * const pMacro,
                     bool bBlockLoad);
    RC SimulateMacro(MacroToRun * const pMacro,
                     bool               bUpdateOutput,
                     bool *             pbOutputValid);

    RC Send(UINT32 * const pBytesThisSend,
            UINT32 * const pMacrosThisSend);

    RC ValidateOutput(UINT32 * const pFailingIndex);

    RC SetMmeToMemToMem(bool bEnable);
    RC StartMemToMemInline(UINT32 Releases,
                           UINT32 *pInlinePaddingCount);
    RC SendInlinePadding(UINT32 Count);

    void DumpMacros(const UINT32 StartIndex,
                    const UINT32 EndIndex,
                    const bool bDumpOutput);
    void PrintMacro(const MmeMacro * const pMacro,
                    const vector<UINT32> &InputData,
                    const vector<UINT32> &ExpectedOutputData,
                    const vector<UINT32> &OutputData);
    void PrintFormattedData(const string Header,
                            const vector<UINT32> &Data);
};

//----------------------------------------------------------------------------
//! \brief Constructor
//!
MMERandomTest::MMERandomTest()
{
    SetName("MMERandomTest");
    m_pTestConfig = GetTestConfiguration();

    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_MMERANDOM_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

//----------------------------------------------------------------------------
//! \brief Determine if the MMERandomTest is supported
//!
//! \return true if MMERandomTest is supported, false if not
bool MMERandomTest::IsSupported()
{
    m_ThreeD.SetOldestSupportedClass(KEPLER_A);
    m_ThreeD.SetNewestSupportedClass(VOLTA_A);
    return (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_MME) &&
            m_ThreeD.IsSupported(GetBoundGpuDevice()));
}

//----------------------------------------------------------------------------
//! \brief Setup the MMERandomTest
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC MMERandomTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // On CheetAh the GPU driver in the kernel only allows FECS to access
    // certain whitelisted registers. We need to turn off this firewall
    // to be able to run this test.
    if (Platform::UsesLwgpuDriver())
    {
        CHECK_RC(m_GpuRegFirewall.DisableFirewall());
    }

    CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                      &m_hCh,
                                                      &m_ThreeD));
    CHECK_RC(GpuTest::AllocDisplayAndSurface(false/*blockLinear*/));

    m_LoopsPerFrame   = m_pTestConfig->RestartSkipCount();

    // Either tracing is disabled (both are ~0), tracing is enabled for a single
    // loop (both are the same non ~0 value), or only one of start or end loop
    // has been set (this is the only interesting case and is handled below)
    if (m_TraceStartLoop != m_TraceEndLoop)
    {
        // End loop was set, but start loop was not, set start loop to the first
        // loop in the frame where end loop resides
        if (m_TraceStartLoop == (UINT32)(~0))
        {
            m_TraceStartLoop = m_TraceEndLoop -
                               (m_TraceEndLoop % m_LoopsPerFrame);
        }
        else if (m_TraceEndLoop == (UINT32)(~0))
        {
            // Start loop was set, but end loop was not, set end loop to the
            // last loop in the frame where start loop resides
            UINT32 loopsRemaining = m_LoopsPerFrame -
                                    (m_TraceStartLoop % m_LoopsPerFrame) - 1;
            m_TraceEndLoop = m_TraceStartLoop + loopsRemaining;
        }

        // Ensure that start loop and end loop are in the same frame
        if ((m_TraceStartLoop / m_LoopsPerFrame) !=
            (m_TraceEndLoop / m_LoopsPerFrame))
        {
            Printf(Tee::PriHigh,
                   "%s : Starting and ending trace loops must be in the "
                   "same frame\n",
                   GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    m_MmeInstructionRamSize =
        GetBoundGpuSubdevice()->GetMmeInstructionRamSize();
    m_pMmeIGen =  new MmeInstGenerator(m_pFpCtx);
    if (!m_pMmeIGen)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    m_pMmeSim = new MMESimulator(m_pMmeIGen,
                                 GetBoundGpuSubdevice()->GetMmeVersion());
    if (!m_pMmeSim)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    m_pShadowState = new MMEShadowSimulator(GetBoundGpuSubdevice()->GetMmeShadowMethods());
    if (!m_pShadowState)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // MacroSize depends upon m_MmeInstructionRamSize being setup correctly.
    // Reset the picker now that m_MmeInstructionRamSize has been set.
    if (!(*m_pPickers)[FPK_MMERANDOM_MACRO_SIZE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_MMERANDOM_MACRO_SIZE));

    // Branch Src0 depends upon m_pMmeIGen being created.  Now that it is,
    // setup the defaults if necessary
    if (!(*m_pPickers)[FPK_MMERANDOM_MACRO_BRA_SRC0].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_MMERANDOM_MACRO_BRA_SRC0));

    m_HwClass = m_ThreeD.GetClass();
    m_pCh->SetObject(s_3D, m_ThreeD.GetHandle());

    // Amount to shift methods by to get the raw values used by FE
    m_MethodShift = 2;
    m_pMmeSim->SetMethodShift(m_MethodShift);

    // Allocate surface for the MemToMem target.
    m_pSurface = GetDisplayMgrTestContext()->GetSurface2D();
    m_pSurface->Map(GetBoundGpuSubdevice()->GetSubdeviceInst());

    // Allocate the notifier for the MemToMem.
    CHECK_RC(m_Notifier.Allocate(m_pCh, 2,
                                   m_pTestConfig->DmaProtocol(),
                                   Memory::Fb, GetBoundGpuDevice()));

    // Allocate dummy semaphore used for flushing
    m_FlushSemaphore.SetWidth(4);
    m_FlushSemaphore.SetHeight(1);
    m_FlushSemaphore.SetColorFormat(ColorUtils::VOID32);
    m_FlushSemaphore.SetLocation(Memory::NonCoherent);
    CHECK_RC(m_FlushSemaphore.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_FlushSemaphore.BindGpuChannel(m_hCh));

    // Ensure that shadow RAM is in passthrough mode
    m_pCh->Write(s_3D,
                 LWA097_SET_MME_SHADOW_RAM_CONTROL,
                 LWA097_SET_MME_SHADOW_RAM_CONTROL_MODE_METHOD_TRACK);

    // Shadow RAM initialization macro (used to initialize shadow RAM with
    // random data.  The first input is the number of shadow RAM methods to
    // be initialized and then each method to be initialized requires a
    // (Method, Data) pair as an input to the macro
    //
    // This macro will always be at address 0 of the instruction RAM and
    // will always use macro index 0
    //
    // Note that the instruction that releases the method from the MME is at
    // offset 2 from the start of the macro.  This is captured by the
    // SHADOW_INIT_RELEASE_OFFSET define.  If this macro is changed then that
    // define needs to be updated if the offset changes
    m_ShadowInitMacro.Index = 0;
    m_ShadowInitMacro.StartAddr = 0;
    m_ShadowInitMacro.Instructions.clear();
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->ADDI(1, 0, 0,
                                             MmeInstGenerator::MUX_LNN, false));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->ADDI(2, 0, 0,
                                             MmeInstGenerator::MUX_LNN, false));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->ADDI(2, 2, 0,
                                             MmeInstGenerator::MUX_RLR, false));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->ADDI(1, 1, (UINT32)-1,
                                             MmeInstGenerator::MUX_RNN, false));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->BRA(1, (UINT32)-3,
                                                             true,
                                                             false,
                                                             false));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->NOP(false));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->NOP(true));
    m_ShadowInitMacro.Instructions.push_back(m_pMmeIGen->NOP(false));
    m_ShadowInitMacro.Size = (UINT32)m_ShadowInitMacro.Instructions.size();

    // Shadow RAM verification macro (used to verify the HW shadow RAM state is
    // exactly as expected.  The first input is the number of shadow RAM methods
    // to verify and then each method to be verified requires the method number
    // as an input to the macro.  This macro may or may not be used depending
    // upon the setting of m_VerifyShadowRam
    //
    // This macro will always immediately follow the shadow init macro in the
    // instruction RAM and will always use macro index 1
    //
    // Note that the instruction that releases the method from the MME is at
    // offset 4 from the start of the macro.  This is captured by the
    // SHADOW_VERIF_RELEASE_OFFSET define.  If this macro is changed then that
    // define needs to be updated if the offset changes
    m_ShadowVerifyMacro.Index = 1;
    m_ShadowVerifyMacro.StartAddr = m_ShadowInitMacro.Size;
    m_ShadowVerifyMacro.Instructions.clear();
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->ADDI(1, 0, 0,
                                             MmeInstGenerator::MUX_LNN, false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->ADDI(2, 0, 0,
                                             MmeInstGenerator::MUX_LNN, false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->ADDI(2, 2, 0,
                                             MmeInstGenerator::MUX_RNR, false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->STATEI(3, 2, 0,
                                                                  false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->ADDI(3, 3, 0,
                                             MmeInstGenerator::MUX_RRN, false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->ADDI(1, 1,
                                                                (UINT32)-1,
                                             MmeInstGenerator::MUX_RNN, false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->BRA(1, (UINT32)-5,
                                                               true,
                                                               false,
                                                               false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->NOP(false));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->NOP(true));
    m_ShadowVerifyMacro.Instructions.push_back(m_pMmeIGen->NOP(false));
    m_ShadowVerifyMacro.Size = (UINT32)m_ShadowVerifyMacro.Instructions.size();

    // Setup the size and number of shadow RAM macros used as appropriate
    m_ShadowRamMacroSize = m_ShadowInitMacro.Size;
    m_ShadowRamMacroCount = 1;
    if (m_VerifyShadowRam)
    {
        m_ShadowRamMacroCount = 2;
        m_ShadowRamMacroSize += m_ShadowVerifyMacro.Size;
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the MMERandomTest
//!
//! \return OK if the test was successful, not OK otherwise
//!
RC MMERandomTest::Run()
{
    RC rc;
    UINT32 startFrame;
    UINT32 endFrame;
    UINT32 endLoop;
    UINT32 bytesThisFrame;
    UINT32 failingIndex;
    UINT32 totalMacrosRun = 0;
    UINT32 totalBytesGenerated = 0;
    UINT32 macrosThisFrame;
    UINT32 startLoop = m_pTestConfig->StartLoop();
    UINT32 loops = m_pTestConfig->Loops();
    Tee::Priority summaryPri = m_PrintSummary ? Tee::PriHigh : Tee::PriLow;

    // If tracing, override the startLoop/loops to isolate the
    // loop being traced
    if (m_TraceStartLoop != (UINT32)(~0))
    {
        startLoop = (m_TraceStartLoop / m_LoopsPerFrame) * m_LoopsPerFrame;
        loops = m_LoopsPerFrame;
    }

    // Determine the the frame where the first output would be generated (based
    // on the starting loop) and the frame which will contain the final output
    // (based on the total loops in the test)
    startFrame = startLoop / m_LoopsPerFrame;
    endFrame   = (startLoop + loops +
                  m_LoopsPerFrame - 1) / m_LoopsPerFrame;

    // For each frame that will generate output
    for (m_pFpCtx->RestartNum = startFrame;
         m_pFpCtx->RestartNum < endFrame;
         m_pFpCtx->RestartNum++)
    {
        // Setup the loop number and determine the last loop number for the
        // frame
        m_pFpCtx->LoopNum = m_pFpCtx->RestartNum * m_LoopsPerFrame;
        if (m_pFpCtx->LoopNum < startLoop)
            m_pFpCtx->LoopNum = startLoop;

        endLoop = (m_pFpCtx->RestartNum + 1) * m_LoopsPerFrame;
        if (endLoop > startLoop + loops)
            endLoop  = startLoop + loops;

        // Clear the output surface, pick the macros, pick which macros
        // will be run in this frame and their input data
        CHECK_RC(RandomizeOncePerRestart(m_pTestConfig->Seed() +
                                         m_pFpCtx->LoopNum));

        // Initialized data used to send macros
        bytesThisFrame = 0;
        macrosThisFrame = 0;

        // Send all the macros for the frame
        CHECK_RC(Send(&bytesThisFrame, &macrosThisFrame));

        if (OK != (rc = ValidateOutput(&failingIndex)))
        {
            const UINT32 failLoop = m_pFpCtx->LoopNum + failingIndex;

            GetJsonExit()->AddFailLoop(failLoop);

            Printf(Tee::PriHigh,
                   "Validation error %d in loop %d\n", INT32(rc),
                   failLoop);
            if (m_DumpMacroOnMiscompare)
            {
                DumpMacros(failingIndex, failingIndex + 1, true);
            }
            return rc;
        }

        Printf(summaryPri, "\n");
        Printf(summaryPri, "    Frame %d of %d (Loops %d through %d)\n",
               m_pFpCtx->RestartNum + 1, endFrame,
               m_pFpCtx->LoopNum, endLoop - 1);
        Printf(summaryPri, "      Macros Run      : %d\n", macrosThisFrame);
        Printf(summaryPri, "      Bytes Generated : %d\n", bytesThisFrame);
        Printf(summaryPri, "\n");

        totalMacrosRun += macrosThisFrame;
        totalBytesGenerated += bytesThisFrame;
    }

    if (totalMacrosRun == 0)
    {
        Printf(Tee::PriHigh, "Test is not able to generate any macros to run\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(summaryPri, "\n");
    Printf(summaryPri, "%s run statistics\n", GetName().c_str());
    Printf(summaryPri, "  Macros Run      : %d\n", totalMacrosRun);
    Printf(summaryPri, "  Bytes Generated : %d\n", totalBytesGenerated);
    Printf(summaryPri, "\n");

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Clean up the MMERandomTest
//!
//! \return OK if cleanup was successful, not OK otherwise
//!
RC MMERandomTest::Cleanup()
{
    StickyRC rc;

    m_FlushSemaphore.Free();

    if (m_pSurface)
        m_pSurface->Unmap();
    m_Notifier.Free();

    m_ThreeD.Free();

    if (m_pShadowState)
    {
        delete m_pShadowState;
        m_pShadowState = NULL;
    }

    if (m_pMmeSim)
    {
        delete m_pMmeSim;
        m_pMmeSim = NULL;
    }

    if (m_pMmeIGen)
    {
        delete m_pMmeIGen;
        m_pMmeIGen = NULL;
    }

    if (m_pCh)
    {
        rc = m_pTestConfig->FreeChannel(m_pCh);
        m_hCh = 0;
        m_pCh = NULL;
    }

    m_GpuRegFirewall.RestoreFirewall();

    rc = GpuTest::Cleanup();

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Setup the default values for a particular fancy picker.  Note that
//!        the default values setup here are designed to make MMERandomTest
//!        mimic the behavior of the hardware unit test at:
//!            //hw/fermi1_gf100/stand_sim/fe/t_mme_common.cpp
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC MMERandomTest::SetDefaultsForPicker(UINT32 Idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[Idx];

    switch (Idx)
    {
        case FPK_MMERANDOM_MACRO_SIZE:
            Fp.ConfigRandom();
            Fp.AddRandRange(64, MIN_MACRO_SIZE, 63);
            Fp.AddRandRange(60, 32, 61);
            Fp.AddRandItem(1, (m_MmeInstructionRamSize / 2) - 1);
            Fp.AddRandItem(1, m_MmeInstructionRamSize / 2);
            Fp.AddRandItem(1, m_MmeInstructionRamSize - 2);
            Fp.AddRandItem(1, m_MmeInstructionRamSize - 1);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_OVERSIZE:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_OVERSIZE_Fill);
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_OVERSIZE_Truncate);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BLOCK_TYPE:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BLOCK_TYPE_NoBranch);
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BLOCK_TYPE_ForwardBranch);
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BLOCK_TYPE_BackwardBranch);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BRA_SRC0:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, 0);
            if (m_pMmeIGen)
            {
                Fp.AddRandRange(1, 0, m_pMmeIGen->GetMaxImmedValue());
            }
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BRA_NZ:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BRA_NZ_True);
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BRA_NZ_False);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BRA_NDS:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BRA_NDS_True);
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BRA_NDS_False);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BRA_SRC0_TO_BRA:
            Fp.ConfigRandom();
            Fp.AddRandRange(1, 0, 5);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_SPACING:
            Fp.ConfigRandom();
            Fp.AddRandRange(1, 0, 16);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_RELEASE_ONLY:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_RELEASE_ONLY_Disable);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BLOCK_LOAD:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BLOCK_LOAD_Enable);
            Fp.CompileRandom();
            break;
        case FPK_MMERANDOM_MACRO_BLOCK_CALL:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_MMERANDOM_MACRO_BLOCK_CALL_Enable);
            Fp.CompileRandom();
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Randomize data in the test that is only randomized on a restart
//!        (once per frame)
//!
//! \param Seed : Seed to use for reseeding the random generator
//!
//! \return OK if successful, not OK otherwise
//!
RC MMERandomTest::RandomizeOncePerRestart(const UINT32 Seed)
{
    RC         rc;
    MmeMacro   macro;
    MacroToRun lwrMacro;
    UINT32     macroIndex;
    UINT32     outputBytes = 0;
    UINT32     lwrOutputBytes;
    UINT32     retries;
    UINT32     traceStartIdx = m_LoopsPerFrame;
    UINT32     traceEndIdx = m_LoopsPerFrame;
    INT32      rollbackToken = MMEShadowSimulator::ROLLBACK_NONE;
    bool       bOutputValid = false;

    // Reset the random-number generator.
    m_pFpCtx->Rand.SeedRandom(Seed);
    m_pPickers->Restart();

    // Fill the output surface with a random solid color
    m_BgColor = m_pFpCtx->Rand.GetRandom();

    // Reinitialize the macro indecies that have been used since new macros
    // be created every frame (do not use any of the indecies that would be
    // used by the shadow RAM macros)
    m_MacroIndecies.clear();
    for (UINT32 i = m_ShadowRamMacroCount;
          i < GetBoundGpuSubdevice()->GetMmeStartAddrRamSize(); i++)
    {
        m_MacroIndecies.push_back(i);
    }

    // Initialize the surface
    CHECK_RC(m_pSurface->Fill(m_BgColor));

    //------------------------------------------------------
    // Create the macros for the frame
    m_MacroStartAddr = m_ShadowRamMacroSize +
                       (*m_pPickers)[FPK_MMERANDOM_MACRO_SPACING].Pick();
    m_Macros.clear();
    do
    {
        CreateMacro(&macro);
        if ((macro.Size) &&
            (macro.Releases < (m_pSurface->GetSize() / MME_BYTES_PER_RELEASE)))
        {
            // Callwlate the start address for the next macro
            m_MacroStartAddr += macro.Size;
            m_MacroStartAddr +=
                (*m_pPickers)[FPK_MMERANDOM_MACRO_SPACING].Pick();
            if (m_MacroStartAddr > m_MmeInstructionRamSize)
            {
                m_MacroStartAddr = m_MmeInstructionRamSize;
            }
            // Add the macro to the list of created macros for this frame
           m_Macros.push_back(macro);
        }
    } while (macro.Size);

    // Setup which macro index we will be tracing based on the loop
    if (m_TraceStartLoop != (UINT32)(~0))
    {
        traceStartIdx = m_TraceStartLoop - m_pFpCtx->LoopNum;
        traceEndIdx   = m_TraceEndLoop - m_pFpCtx->LoopNum;
    }

    //-------------------------------------------------------------------------
    // Randomize the shadow RAM state and setup the shadow RAM macros.
    // This must be done prior to running and macro simulations
    m_MacrosToRun.clear();
    RandomizeShadowState();

    // Only save a rollback point here  if tracing is active
    if (traceStartIdx < m_LoopsPerFrame)
        rollbackToken = m_pShadowState->SetRollbackPoint();

    CHECK_RC(SetupShadowRamMacros(&outputBytes));

    //-------------------------------------------------------------------------
    // Keep a list of valid macros that may be chosen.  A macro is only removed
    // from the list of valid macros if no input data can be found which would
    // generate valid output
    vector<UINT32> validMacros;
    for (UINT32 i = 0; i < (UINT32)m_Macros.size(); i++)
    {
        validMacros.push_back(i);
    }

    //-------------------------------------------------------------------------
    // Pick which macros which will be run during this frame and their data.
    // This process is governed by the following (everything here needs to
    // be very carefully taken into account if this macro selection process
    // is changed in any way)
    //
    //   - The current state of shadow RAM is a potential input to any macro
    //     since one of the instructions that a macro can execute will pull
    //     a value from shadow RAM and use it to generate an output
    //   - The output of any macro may change the state of shadow RAM
    //   - Shadow RAM is used to shadow the current state of methods within
    //     a class (i.e. the current value for (Class X, Method Y) is Z)
    //   - There are some values that should never be generated by any macro
    //     since the methods are processed even when the MME is setup to
    //     generate inline MemToMem data
    //
    // Each loop represents a single macro, however whether the macro assigned
    // to a loop is actually run depends on several factors:
    //
    //     1.  Will the macro output fit in the available remaining data on
    //         the surface
    //     2.  If tracing is active, is the macro one of the traced macros
    //
    // Note that there is always at least one shadow RAM macro in the frame (to
    // initialize shadow RAM) and possibly more (to verify shadow RAM)
    //
    // After picking a macro, simulate the macros to determine whether it will
    // generate illegal data
    //
    // When a macro generates illegal data, flag that macro as skipped so it
    // will not be exelwted.  In addition, if a macro generates illegal data
    // then (from empirical testing) it is highly likely that running that same
    // macro in a different loop will also fail so mark all future loops which
    // also use that macro as skipped
    //
    // The first m_ShadowRamMacroCount macros have already been chosen and have
    // had their intput and expected output data generated
    for (UINT32 idx = m_ShadowRamMacroCount; idx < m_LoopsPerFrame; idx++)
    {
        if (validMacros.empty())
        {
            Printf(Tee::PriLow,
                    "Warning: No valid macros generated, frame will be skipped\n");
            break;
        }

        retries = 0;

        do
        {
            macroIndex =
                m_pFpCtx->Rand.GetRandom(0, (UINT32)validMacros.size() - 1);
            lwrMacro.pMacro = &m_Macros[validMacros[macroIndex]];
            // Set skip based on whether the output of the macro will fit into
            // the output surface
            lwrOutputBytes = lwrMacro.pMacro->Releases * MME_BYTES_PER_RELEASE;
            lwrMacro.bSkip = ((outputBytes + lwrOutputBytes) >
                              m_pSurface->GetSize());
            lwrMacro.bSkippedForTracing = false;
        } while (lwrMacro.bSkip && (++retries < m_PickRetries));
        m_MacrosToRun.push_back(lwrMacro);

        // If macro tracing is activated and the macro is not in the range of
        // traced macros, then flag it as "skipped for tracing".  Skipped for
        // tracing macros will not be run, but still need to have input data
        // generated (to avoid perturbing the random sequence) and also still
        // need to be simulated (so that the shadow state can be captured
        // immediately prior to the traced macro)
        if ((traceStartIdx < m_LoopsPerFrame) && m_OnlyExecTracedLoops &&
            ((idx < traceStartIdx) || (idx > traceEndIdx)))
        {
            lwrMacro.bSkippedForTracing = true;
        }

        // If the current macro will be run, create the input data
        if (!m_MacrosToRun[idx].bSkip)
        {
            retries = 0;
            do
            {
                m_MacrosToRun[idx].InputData.clear();
                for (UINT32 InputData = 0;
                      InputData < m_MacrosToRun[idx].pMacro->Loads;
                      InputData++)
                {
                    m_MacrosToRun[idx].InputData.push_back(m_pFpCtx->Rand.GetRandom());
                }

                // Simulate the program, if the output of the simulation
                // was valid, then the shadow state will have been updated
                // based on the assumption that the macro will really be run
                CHECK_RC(SimulateMacro(&m_MacrosToRun[idx],
                                       true,
                                       &bOutputValid));
            } while (!bOutputValid && (++retries < m_PickRetries));

            // If there was no set of input data to the macro that will
            // generate valid data
            if (retries == m_PickRetries)
            {
                m_MacrosToRun[idx].bSkip = true;
                m_MacrosToRun[idx].InputData.clear();
                m_MacrosToRun[idx].OutputData.clear();
                validMacros.erase(validMacros.begin() + macroIndex);
            }
            else if (m_MacrosToRun[idx].bSkippedForTracing)
            {
                m_MacrosToRun[idx].OutputData.clear();
            }
        }

        if (!m_MacrosToRun[idx].bSkip && !m_MacrosToRun[idx].bSkippedForTracing)
            outputBytes += lwrOutputBytes;
    }

    //-------------------------------------------------------------------------
    // Now handle any debugging parameters that may have been specified.
    //
    //     1.  Whether a certain range of loops should have trace output for
    //         their simulation
    //     2.  Whether only the specified range of traced loops should be
    //         exelwted
    //
    if (traceStartIdx < m_LoopsPerFrame)
    {
        // Since we are about to start simulating from the beginning of the
        // frame again, rollback the shadow state to the state it was at the
        // beginning of the frame
        CHECK_RC(m_pShadowState->Rollback(rollbackToken));

        for (UINT32 idx = 0; idx < m_LoopsPerFrame; idx++)
        {
            // Capture the rollback point prior to running the first traced
            // macro since that is what we will need to initialize the
            // shadow RAM to
            if ((idx == traceStartIdx) && m_OnlyExecTracedLoops)
            {
                rollbackToken = m_pShadowState->SetRollbackPoint();
            }

            // If the current macro will be run, simulate it, note that we
            // simulate even the "skippedForTrace" macros so that any shadow
            // RAM updates that they would generate still occur and we can start
            // the traced macro block with a shadow state as if all the previous
            // macros had been run
            if (!m_MacrosToRun[idx].bSkip)
            {
                Tee::Priority pri = ((idx >= traceStartIdx) &&
                                     (idx <= traceEndIdx)) ?
                                         Tee::PriNormal : Tee::PriSecret;

                MMESimulator::ChangeTracePriority setPri(m_pMmeSim, pri);

                Printf(pri,
                      "**************** Simulating Macro %d ****************\n",
                      idx);
                // If the program will actually be run, then resimulate the
                // program so that the shadow state will actually be updated
                //
                // Note that the shadow RAM macros should not have their output
                // updated by the simulator (since it provides better validation
                // to allow the SetupShadowRamMacros() function to setup the
                // output for those macros)
                CHECK_RC(SimulateMacro(&m_MacrosToRun[idx],
                                idx >= m_ShadowRamMacroCount,
                                &bOutputValid));
                MASSERT(bOutputValid);
            }
        }

        // When only exelwting traced loops, rollback the shadow RAM state to
        // which is the shadow state immediately before the first traced loop
        // and update the shadow RAM macros
        if (m_OnlyExecTracedLoops)
        {
            CHECK_RC(m_pShadowState->Rollback(rollbackToken));
            CHECK_RC(SetupShadowRamMacros(&outputBytes));
        }
    }

    CHECK_RC(DownloadMacro(&m_ShadowInitMacro, true));
    if (m_VerifyShadowRam)
    {
        CHECK_RC(DownloadMacro(&m_ShadowVerifyMacro, true));
    }

    // Download all the macros to the MME
    for (UINT32 idx = 0; idx < (UINT32)m_Macros.size(); idx++)
    {
        const bool bBlockLoad =
            ((*m_pPickers)[FPK_MMERANDOM_MACRO_BLOCK_LOAD].Pick() ==
             FPK_MMERANDOM_MACRO_BLOCK_LOAD_Enable);
        CHECK_RC(DownloadMacro(&m_Macros[idx], bBlockLoad));
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Initialize Shadow RAM to random values
//!
void MMERandomTest::RandomizeShadowState()
{
    const GpuSubdevice::MmeMethods mmeMethods =
                               GetBoundGpuSubdevice()->GetMmeShadowMethods();
    UINT32 value;

    GpuSubdevice::MmeMethods::const_iterator pMmeMethod = mmeMethods.begin();
    for ( ; pMmeMethod != mmeMethods.end(); pMmeMethod++)
    {
        value = m_pFpCtx->Rand.GetRandom();
        m_pShadowState->SetValue(pMmeMethod->first, value);
    }
}

//----------------------------------------------------------------------------
//! \brief Set the contents of the hardware shadow RAM to the software state
//!
RC MMERandomTest::SetupShadowRamMacros(UINT32 *pOutputBytes)
{
    RC rc;
    const GpuSubdevice::MmeMethods mmeMethods =
                               GetBoundGpuSubdevice()->GetMmeShadowMethods();
    set<UINT32> sharedMethodsSent;
    MacroToRun initMacro;
    MacroToRun verifyMacro;

    *pOutputBytes = 0;

    // Initialize the shadow RAM initialization macro
    initMacro.pMacro = &m_ShadowInitMacro;
    initMacro.bSkip = false;
    initMacro.bSkippedForTracing = false;
    initMacro.InputData.clear();
    initMacro.OutputData.clear();
    initMacro.InputData.push_back(0);  // Unused initial value
    initMacro.InputData.push_back(0);  // Reserved for number of releases
    initMacro.pMacro->Loads = 2;
    initMacro.pMacro->Releases = 0;

    // Initialize the shadow RAM verification macro
    if (m_VerifyShadowRam)
    {
        verifyMacro.pMacro = &m_ShadowVerifyMacro;
        verifyMacro.bSkip = false;
        verifyMacro.bSkippedForTracing = false;
        verifyMacro.InputData.clear();
        verifyMacro.OutputData.clear();
        verifyMacro.InputData.push_back(0);  // Unused initial value
        verifyMacro.InputData.push_back(0);  // Reserved for number of releases
        verifyMacro.pMacro->Loads = 2;
        verifyMacro.pMacro->Releases = 0;
    }

    GpuSubdevice::MmeMethods::const_iterator pMmeMethod = mmeMethods.begin();
    for ( ; pMmeMethod != mmeMethods.end(); pMmeMethod++)
    {
        // If the shadow method is one that the macro could conceivably use
        // then send the method down so that it gets initialized in shadow
        // RAM correctly on the channel context
        if ((!pMmeMethod->second &&
             (pMmeMethod->first.ShadowClass == m_HwClass)) ||
            (pMmeMethod->second &&
             (sharedMethodsSent.count(pMmeMethod->first.ShadowMethod) == 0)))
        {
            // Callwlate the values for both the input and output data for the
            // shadow RAM macros
            UINT32 inputMethod = pMmeMethod->first.ShadowMethod;
            UINT32 inputData = m_pShadowState->GetValue(pMmeMethod->first);
            UINT32 releasePC = initMacro.pMacro->StartAddr +
                               SHADOW_INIT_RELEASE_OFFSET;
            UINT32 outputMethodValue =
                m_pMmeSim->GetReleasedMethodValue(s_3D,
                                                  inputMethod,
                                                  releasePC);

            if (pMmeMethod->second)
            {
                sharedMethodsSent.insert(inputMethod);
            }
            initMacro.InputData.push_back(inputMethod);
            initMacro.InputData.push_back(inputData);
            initMacro.OutputData.push_back(inputData);
            initMacro.OutputData.push_back(outputMethodValue);
            initMacro.pMacro->Loads += 2;
            initMacro.pMacro->Releases++;

            if (m_VerifyShadowRam)
            {
                releasePC = verifyMacro.pMacro->StartAddr +
                            SHADOW_VERIF_RELEASE_OFFSET;
                outputMethodValue =
                    m_pMmeSim->GetReleasedMethodValue(s_3D,
                                                      inputMethod,
                                                      releasePC);

                verifyMacro.InputData.push_back(inputMethod);
                verifyMacro.OutputData.push_back(inputData);
                verifyMacro.OutputData.push_back(outputMethodValue);
                verifyMacro.pMacro->Loads++;
                verifyMacro.pMacro->Releases++;
            }
        }
    }
    initMacro.InputData[1] = initMacro.pMacro->Releases;
    *pOutputBytes = initMacro.pMacro->Releases * MME_BYTES_PER_RELEASE;
    m_MacrosToRun.push_back(initMacro);

    if (m_VerifyShadowRam)
    {
        verifyMacro.InputData[1] = verifyMacro.pMacro->Releases;
        *pOutputBytes += verifyMacro.pMacro->Releases * MME_BYTES_PER_RELEASE;
        m_MacrosToRun.push_back(verifyMacro);
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Create a random macro.  Based on code from
//!        //hw/fermi1_gf100/stand_sim/fe/t_mme_common.cpp
//!
//! \param pMacro : Pointer to returned macro
//!
void MMERandomTest::CreateMacro(MmeMacro * const pMacro)
{
    // If the macro size is zero, then space could not be allocated for a new
    // macro in the instruction RAM
    pMacro->Size = GetMacroSize();
    if (pMacro->Size == 0)
    {
        return;
    }

    pMacro->StartAddr = m_MacroStartAddr;
    pMacro->Instructions.clear();

    UINT32 macroIndex = m_pFpCtx->Rand.GetRandom(0,
                                           (UINT32)m_MacroIndecies.size() - 1);
    pMacro->Index = m_MacroIndecies[macroIndex];
    m_MacroIndecies.erase(m_MacroIndecies.begin() + macroIndex);

    // Initialized to 1, since first data to mme_call_macro is data arg to
    // CALL_MACRO.
    pMacro->Loads = 1;
    pMacro->Releases = 0;

    UINT32 instructionsLeft = pMacro->Size;
    UINT32 blockSize, blockType, blockBodySize, blockLoad, blockLoadArg;
    UINT32 branchSrc0Reg, branchSrc0Value = 0;
    bool   branchNzFlag = false, branchNdsFlag = false;
    bool   bReleaseOnly, bEndingNext;
    UINT32 regCount = m_pMmeIGen->GetRegCount();

    set<UINT32> validOutputRegs;
    set<UINT32> allRegs;

    UINT32 src0LoadToBranch = 0;
    UINT32 i;

    // Departure from RTL test bench code.  In order to be able to isolate a
    // loopall registers must be randomized prior to each macro
    MASSERT(regCount > 2);
    for (i = 0; i < regCount; i++, instructionsLeft--)
    {
        // Load the registers from the input data so instead of choosing
        // random values since the registers contribute to the output
        pMacro->Instructions.push_back(m_pMmeIGen->ADDI(
                                              i,
                                              0,
                                              0,
                                              MmeInstGenerator::MUX_LNN,
                                              0));
        pMacro->Loads++;
    }

    // Insert an ADD instruction to load up the Method register to
    // ensure consistency by using MUX_RNR.  Also, ADD R0 to R0 and
    // store the result in R0 so that the carry bit gets initialized
    // and the current state of the carry does not affect the actual
    // ADD (since R0 always returns 0)
    pMacro->Instructions.push_back(m_pMmeIGen->ADD(
                                          0,
                                          0,
                                          0,
                                          MmeInstGenerator::MUX_RNR,
                                          0));
    instructionsLeft--;

    // Create a set containing all the existing registers, used to restrict
    // which registers may be accessed in certain cases
    for (i = 0; i < regCount; i++)
    {
        allRegs.insert(i);
    }

    // Set whether this macro should only generate instructions which release
    // data (i.e. every instruction will cause a method/data pair to be
    // created).  When true, this causes only OP_BIN type instructions to be
    // picked
    bReleaseOnly =
        ((*m_pPickers)[FPK_MMERANDOM_MACRO_RELEASE_ONLY].Pick() != 0);
    while (instructionsLeft)
    {
        // Reset which output registers are valid
        validOutputRegs = allRegs;

        // Generate a random block size.
        blockSize = (instructionsLeft <= MIN_BRA_BLOCK_SIZE) ?
                            instructionsLeft :
                            m_pFpCtx->Rand.GetRandom(0, instructionsLeft);

        // Generate random block type. There are 3 types of blocks :
        // block_type = 0 : Sequence of random instructions without branches.
        //
        // block_type = 1 : Sequence of random instructions (block body),
        //    wrapped by a forward branch at the start of the block, which
        //    branches (when branch is taken) to 2 intructions from the end of
        //    the block. Branch may randomly be taken or not taken.  Whether
        //    the branch is taken or not is determined at CREATION time not
        //    run time
        //
        // block_type = 3 : Sequence of random instructions, wrapped by a
        //    backward branch at the end of the block, which branches back to
        //    the second instruction from the start of the block (the first
        //    instruction loads a loop count (2) into a random register). It
        //    loops through the block body twice, so that the branch is taken
        //    after the first iteration, and not taken after the second
        //    iteration. The loop count register (used as Src0 of the branch
        //    instruction) is not touched in the body of the backward branch
        //    block.
        if (blockSize <= MIN_BRA_BLOCK_SIZE)
        {
            blockType = NoBranch;
        }
        else
        {
            blockType = (*m_pPickers)[FPK_MMERANDOM_MACRO_BLOCK_TYPE].Pick();
        }

        if (blockType != NoBranch)
        {
            // The Src0 register for branch instructions.
            branchSrc0Reg = m_pFpCtx->Rand.GetRandom(0, regCount - 1);

            // Random value used to load the Src0 register of branch
            // instructions. Value can be forced to zero through a fancy
            // picker.  The source0 value is set explicitly when doing
            // backward branches.
            if (blockType == BackwardBranch)
            {
                branchSrc0Value = 0;
            }
            else
            {
                branchSrc0Value =
                    (*m_pPickers)[FPK_MMERANDOM_MACRO_BRA_SRC0].Pick();
            }

            // Flags for branch instructions.
            branchNzFlag = ((*m_pPickers)[FPK_MMERANDOM_MACRO_BRA_NZ].Pick() ==
                            FPK_MMERANDOM_MACRO_BRA_NZ_True);
            branchNdsFlag =
                ((*m_pPickers)[FPK_MMERANDOM_MACRO_BRA_NDS].Pick() ==
                 FPK_MMERANDOM_MACRO_BRA_NDS_True);

            // Number of instructions from the time Src0 is loaded, to
            // the branch.
            src0LoadToBranch =
                (*m_pPickers)[FPK_MMERANDOM_MACRO_BRA_SRC0_TO_BRA].Pick();
            if (blockSize <= (MIN_BRA_BLOCK_SIZE + src0LoadToBranch))
            {
                src0LoadToBranch = blockSize - MIN_BRA_BLOCK_SIZE - 1;
            }
        }

        if (blockType == ForwardBranch)
        {
            // For forward-branch blocks, the first instruction loads the Src0
            // register for the branch, and the second register is the branch.
            // Remaining instructions form the block body.
            blockBodySize = blockSize - 2 - src0LoadToBranch;

            // For forward-branch blocks, block_load indicates whether the
            // block body is skipped (i.e., branched over) or not. If skipped,
            // data load instructions in "most" (exceptions described later) of
            // the block body will not need load data to be provided through
            // callData methods (since they are skipped).
            if ((((branchSrc0Reg == 0) || (branchSrc0Value == 0)) &&
                 (branchNzFlag == 0)) ||
                ((branchSrc0Value != 0) && (branchSrc0Reg != 0) &&
                 (branchNzFlag == 1)))
            {
                blockLoad = 0;
            }
            else
            {
                blockLoad = 1;
            }

            // Load the picked brance src0 value
            pMacro->Instructions.push_back(m_pMmeIGen->ADDI(
                                                  branchSrc0Reg,
                                                  0,
                                                  branchSrc0Value,
                                                  MmeInstGenerator::MUX_RNN,
                                                  0));

            // Between Src0 load and the branch, ensure that the Src0 reg is
            // not touched
            validOutputRegs.erase(branchSrc0Reg);

            //Fill in instructions between Src0 load and branch.
            for (i = 0; i < src0LoadToBranch; i++)
            {
                //dont touch branchSrc0Reg. bEndingNext = 0, blockLoad = 1.
                AddInstruction(pMacro, false, 1, bReleaseOnly,
                               validOutputRegs);
            }

            // For the remaining instructions, all registers are fair game
            validOutputRegs = allRegs;

            // Branch instruction. Branches to penultimate instruction of
            // the block.  The second parameter indicates how many instructions
            // are skipped (including the branch instruction).  In this case,
            // the number of skipped instructionsis the total block size less
            //     1.  The ADDI instruction that loads Src0 for the branch
            //     2.  The last 2 instructions in the block (always exelwted)
            //     3.  The number of instructions between the Src0 and the
            //         Branch
            pMacro->Instructions.push_back(m_pMmeIGen->BRA(branchSrc0Reg,
                                                           blockSize - 3 - src0LoadToBranch,
                                                           branchNzFlag,
                                                           branchNdsFlag,
                                                           false));
        }
        else if (blockType == BackwardBranch)
        {
            // Src0 = r0 not allowed for backward branches
            if (branchSrc0Reg == 0)
                branchSrc0Reg = 1;

            // For backward-branch blocks, the first instruction loads Src0,
            // followed by the block body, and the last 3 instructions consist
            // of the Src0-decrement, the branch instruction, and a NOP in the
            // potential delay-slot.
            blockBodySize = blockSize - MIN_BRA_BLOCK_SIZE - src0LoadToBranch;

            // blockLoad = 2, since block body is looped through twice, so each
            // data load instruction in the block body will result in 2
            // callData methods being needed.
            blockLoad = 2;

            // Make sure Src0 register used by the branch instruction is not
            // touched in the body of backward-branch blocks.
            validOutputRegs.erase(branchSrc0Reg);

            // Load Src0 register such that the block body will be interated
            // through twice, whether the backward-branch has the NZ_FLAG set
            // or not.
            if (branchNzFlag)
            {
                branchSrc0Value = 2;
            }
            else
            {
                branchSrc0Value = 1;
            }

            // Instruction to load Src0.
            pMacro->Instructions.push_back(m_pMmeIGen->ADDI(branchSrc0Reg,
                                                  0,
                                                  branchSrc0Value,
                                                  MmeInstGenerator::MUX_RNN,
                                                  0));
        }
        else
        {
            // body = whole block in no-branch blocks.
            blockBodySize = blockSize;
            // Each load instruction will need one callData macro, since each
            // is exelwted exactly once.
            blockLoad = 1;
        }

        // Generate block body.
        for (i=0; i < blockBodySize; i++)
        {
            bEndingNext = (pMacro->Instructions.size() == (pMacro->Size - 2));

            // Exceptions to block_load being 0 when a block body is branched
            // over :
            //     - When branch has delay-slot, the first instruction of the
            //       block body is the delay slot, and will be exelwted.
            //     - The last 2 instructions of the block body are always
            //       exelwted.
            //
            // Adjust block_load to 1 in either of those cases.
            if ((blockType == ForwardBranch) &&
                (((branchNdsFlag == 0) && (i ==0 )) ||
                 (i >= blockBodySize - 2)))
            {
                blockLoadArg = 1;
            }
            else
            {
                blockLoadArg = blockLoad;
            }

            //dont touch branchSrc0Reg. bEndingNext = 0, blockLoad = 1.
            AddInstruction(pMacro, bEndingNext, blockLoadArg,
                           bReleaseOnly, validOutputRegs);

            UINT32 lastInstr;

            lastInstr = pMacro->Instructions[pMacro->Instructions.size() - 1];
            MmeInstGenerator::OpCode lastOp = m_pMmeIGen->GetOpCode(lastInstr);

            // Randomizing INSERTSB0/INSERTDB0 instruction's Src0 register in
            // range 0-31, for better shift_amount testpoint coverage, by
            // inserting an ADDI instruction before INSERT?B0.  Dont do this if
            // INSERT?B0 was last instruction in block body (then there is no
            // room to insert the ADDI).  Dont do this for FORWARD_BRANCH
            // blocks, where the branch is taken (block_load = 0), as moving
            // the INSERT?B0 out of the delay slot at the beginning of the
            // block or into the last 2 instructions of the block messes up
            // the number of loads needed.
            if (((lastOp == MmeInstGenerator::OP_INSERTSB0) ||
                 (lastOp == MmeInstGenerator::OP_INSERTDB0)) &&
                (i < blockBodySize - 1) && (blockLoad != 0))
            {
                UINT32 insertSrc0 = m_pMmeIGen->GetSrc0Reg(lastInstr);
                const UINT32 src0Val = m_pFpCtx->Rand.GetRandom(0,
                                             m_pMmeIGen->GetMaxSrcBitValue());

                if (validOutputRegs.count(insertSrc0) == 0)
                {
                    insertSrc0 = 0;
                }

                // Set the new src0 value for the insert instruction
                pMacro->Instructions[pMacro->Instructions.size() - 1] =
                    m_pMmeIGen->ADDI(insertSrc0,
                                     0,
                                     src0Val,
                                     MmeInstGenerator::MUX_RNN,
                                     bEndingNext);

                // Set EndingNext bit if moved insert instruction is EN, else
                // clear EN bit.
                if (pMacro->Instructions.size() == (pMacro->Size - 2))
                {
                    lastInstr = m_pMmeIGen->SetEndingNext(lastInstr);
                }
                else
                {
                    lastInstr = m_pMmeIGen->ClearEndingNext(lastInstr);
                }
                pMacro->Instructions.push_back(lastInstr);
                i++;
            }
        }

        if (blockType == BackwardBranch)
        {
            // Decrement bra-instruction's Src0 register
            pMacro->Instructions.push_back(m_pMmeIGen->ADDI(branchSrc0Reg,
                                                    branchSrc0Reg,
                                                    (UINT32)-1,
                                                    MmeInstGenerator::MUX_RNN,
                                                    0));

            //Fill in instructions between Src0 load and branch.
            for (i = 0; i < src0LoadToBranch; i++)
            {
                //dont touch branchSrc0Reg. bEndingNext=0, blockLoad=2.
                AddInstruction(pMacro, false, 2,
                               bReleaseOnly, validOutputRegs);
            }

            // Branch instruction.  Branch (blockSize - 3) backwards since
            // we want to end up after the first Src0 load and the branch
            // and subsequent NOP are not part of the count
            bEndingNext = (pMacro->Instructions.size() == (pMacro->Size - 2));
            pMacro->Instructions.push_back(m_pMmeIGen->BRA(branchSrc0Reg,
                                                (UINT32)(3 - (INT32)blockSize),
                                                           branchNzFlag,
                                                           branchNdsFlag,
                                                           bEndingNext));

            // NOP following branch. This will be EndingNext if the next block
            // is the last block in the macro, and has exactly one instruction.
            bEndingNext = (pMacro->Instructions.size() == (pMacro->Size - 2));
            pMacro->Instructions.push_back(m_pMmeIGen->NOP(bEndingNext));
        }

        instructionsLeft = instructionsLeft - blockSize;
    }
}

//----------------------------------------------------------------------------
//! \brief Get the size to use for the macro
//!
//! \return Zero if a macro cound not be allocated (i.e. Instruction RAM is
//!         full), not zero if space for a macro was allocated
//!
UINT32 MMERandomTest::GetMacroSize()
{
    UINT32 macroSize;

    // Not possible to fit a macro if less than the minimum number of
    // instructions are left.
    if ((m_MmeInstructionRamSize - m_MacroStartAddr) < MIN_MACRO_SIZE)
    {
        return 0;
    }

    macroSize = (*m_pPickers)[FPK_MMERANDOM_MACRO_SIZE].Pick();

    // Truncate to end of instruction ram if macro overruns end of instrucion
    // ram.  Depending upon fancy picker setup, either fill up macro to fit
    // instruction ram or leave some space at the end.
    if ((macroSize + m_MacroStartAddr) > m_MmeInstructionRamSize)
    {
        const UINT32 oversizeMode =
            (*m_pPickers)[FPK_MMERANDOM_MACRO_OVERSIZE].Pick();
        if ((MIN_MACRO_SIZE == (m_MmeInstructionRamSize - m_MacroStartAddr)) ||
            (oversizeMode == FPK_MMERANDOM_MACRO_OVERSIZE_Fill))
        {
            macroSize = m_MmeInstructionRamSize - m_MacroStartAddr;
        }
        else
        {
            macroSize = m_pFpCtx->Rand.GetRandom(MIN_MACRO_SIZE,
                               m_MmeInstructionRamSize - m_MacroStartAddr - 1);
        }
    }

    return (macroSize < MIN_MACRO_SIZE) ? MIN_MACRO_SIZE : macroSize;
}

//----------------------------------------------------------------------------
//! \brief Add a random instruction to the macro
//!
//! \param pMacro          : Pointer to the macro to add an instruction to
//! \param bEndingNext     : true if this is the penultimate instruction in the
//!                          macro
//! \param BlockLoad       : Number of times the instruction will be exelwted
//! \param bReleaseOnly    : true if only release instructions should be
//!                          generated
//! \param ValidOutputRegs : Set of output registers which are valid for the
//!                          new instruction
//!
void MMERandomTest::AddInstruction(MmeMacro * const   pMacro,
                                   const bool         bEndingNext,
                                   const UINT32       BlockLoad,
                                   const bool         bReleaseOnly,
                                   const set<UINT32> &ValidOutputRegs)
{
    UINT32 Instr = m_pMmeIGen->GenRandInstruction(bReleaseOnly,
                                                  false,
                                                  ValidOutputRegs,
                                                  bEndingNext);

    if (m_pMmeIGen->IsLoadInstruction(Instr))
    {
        pMacro->Loads += BlockLoad;
    }

    if (m_pMmeIGen->IsReleaseInstruction(Instr))
    {
        pMacro->Releases += BlockLoad;
    }

    pMacro->Instructions.push_back(Instr);
}

//----------------------------------------------------------------------------
//! \brief Check whether the output data is valid. Even when the MME output is
//!        routed to MemToMem certain methods may still be processed by the
//!        MME Shadow RAM block (i.e methods that affect Shadow RAM operation).
//!        If these methods were to be generated with particular data, the test
//!        could become corrupt.
//!
//! \param OutputData : Output data to validate
//!
//! \return true if the output data is not valid, false otherwise
//!
bool MMERandomTest::IsOutputDataValid(const vector<UINT32> &OutputData)
{
    UINT32 method;

    // Method addresses are stored at odd indexes
    for (UINT32 i = 1; i < OutputData.size(); i += 2)
    {
        method = m_pMmeIGen->GetMethod(OutputData[i]) << m_MethodShift;
        if (method == LWA097_SET_MME_SHADOW_RAM_CONTROL)
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------
//! \brief Download an MME macro into start addr ram and instruction ram
//!
//! \param pMacro          : Pointer to the macro to download
//! \param bBlockLoad      : True if the macro should be loaded as a block
//!
//! \return OK if successful, not OK otherwise
//!
RC MMERandomTest::DownloadMacro(const MmeMacro * const pMacro,
                                bool  bBlockLoad)
{
    RC rc;

    // Init MME Start address Ram
    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_LOAD_MME_START_ADDRESS_RAM_POINTER,
                          pMacro->Index));
    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_LOAD_MME_START_ADDRESS_RAM,
                          pMacro->StartAddr));

    if (!bBlockLoad)
    {
        // Load up the macro using single method writes
        CHECK_RC(m_pCh->Write(s_3D,
                              LWA097_LOAD_MME_INSTRUCTION_RAM_POINTER,
                              pMacro->StartAddr));
        for (UINT32 i = 0; i < pMacro->Instructions.size(); i++)
        {
            CHECK_RC(m_pCh->Write(s_3D,
                                  LWA097_LOAD_MME_INSTRUCTION_RAM,
                                  pMacro->Instructions[i]));
        }
    }
    else
    {
        vector<UINT32> sendData(pMacro->Instructions);

        // Load the macro using a single IncOnce method
        sendData.insert(sendData.begin(), pMacro->StartAddr);
        CHECK_RC(m_pCh->WriteIncOnce(s_3D,
                                     LWA097_LOAD_MME_INSTRUCTION_RAM_POINTER,
                                     (UINT32)sendData.size(),
                                     &sendData[0]));
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the specified macro through the simulator
//!
//! If this routine determines that the output is valid, the shadow state will
//! be left in the state it would be as if the macro was run.  If the output is
//! determined to be invalid, then the shadow state will be reverted to the
//! state it was when entering this function
//!
//! \param pMacro            : Macro to simulate
//! \param bUpdateOutput     : True to update the output data of the macro
//! \param pbOutputValid     : Returned value indicating if the output is valid
//!
//! \return OK if successful, not OK otherwise
//!
RC MMERandomTest::SimulateMacro(MacroToRun * const pMacro,
                                bool               bUpdateOutput,
                                bool *             pbOutputValid)
{
    Tasker::DetachThread threadDetach;
    RC rc;
    INT32 rollbackToken = MMEShadowSimulator::ROLLBACK_NONE;
    vector<UINT32> outputData;
    vector<UINT32> * pOutputData = &outputData;

    if (bUpdateOutput)
        pOutputData = &pMacro->OutputData;

    *pbOutputValid = false;
    rollbackToken = m_pShadowState->SetRollbackPoint();

    pOutputData->clear();

    rc = m_pMmeSim->RunProgram(s_3D,
                               m_HwClass,
                               pMacro->pMacro->Instructions,
                               pMacro->InputData,
                               m_pShadowState,
                               pMacro->pMacro->StartAddr,
                               pOutputData);

    if ((rc != OK) && !m_pMmeSim->GetError().empty())
    {
        Printf(Tee::PriHigh, "Error Simulating Macro : %s\n",
               m_pMmeSim->GetError().c_str());
    }
    if (!m_pMmeSim->GetWarningCategory().empty() ||
        !m_pMmeSim->GetWarningMessage().empty())
    {
        Printf(Tee::PriHigh, "Warning Simulating Macro : %s - %s\n",
               m_pMmeSim->GetWarningCategory().c_str(),
               m_pMmeSim->GetWarningMessage().c_str());
    }

    *pbOutputValid = IsOutputDataValid(*pOutputData);

    if (!(*pbOutputValid))
    {
        CHECK_RC(m_pShadowState->Rollback(rollbackToken));
    }
    else
    {
        CHECK_RC(m_pShadowState->ForgetRollbackPoint(rollbackToken));
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Setup a inline transfer and execute a block of macros to
//!        fill the surface
//!
//! \param pBytesThisSend  : Number of bytes that this send will generate
//! \param pMacrosThisSend : Number of macros that this send will execute
//!
//! \return OK if successful, not OK otherwise
//!
RC MMERandomTest::Send(UINT32 * const pBytesThisSend,
                       UINT32 * const pMacrosThisSend)
{
    RC     rc;
    UINT32 idx;
    UINT32 inlinePaddingCount = 0;

    *pBytesThisSend = 0;
    *pMacrosThisSend = 0;

    // Callwlate the number of bytes that will be generated by this call
    for (idx = 0; idx < (UINT32)m_MacrosToRun.size(); idx++)
    {
        if (!m_MacrosToRun[idx].bSkip && !m_MacrosToRun[idx].bSkippedForTracing)
        {
            *pBytesThisSend += (m_MacrosToRun[idx].pMacro->Releases *
                               MME_BYTES_PER_RELEASE);
            (*pMacrosThisSend)++;
        }
    }

    if (m_DumpMacrosPreRun)
    {
        DumpMacros(0, UNSIGNED_CAST(UINT32, m_MacrosToRun.size()), false);
    }

    // If the macro calls will generate bytes, then setup the MemToMem object
    // to capture the output.  Instead of returning here and skipping the
    // macros (because of no output), continue and actually run the macros
    // anyway to ensure that there are no side effects.
    if (*pBytesThisSend != 0)
    {
        // If methods will be released by the MME, ensure that they go to the
        // inline MemToMem transfer rather than to the front end.
        CHECK_RC(SetMmeToMemToMem(true));

        CHECK_RC(StartMemToMemInline(*pBytesThisSend / MME_BYTES_PER_RELEASE,
                                     &inlinePaddingCount));
    }

    // Run the macros
    for (idx = 0; idx < (UINT32)m_MacrosToRun.size(); idx++)
    {
        if (m_MacrosToRun[idx].bSkip || m_MacrosToRun[idx].bSkippedForTracing)
            continue;

        const UINT32 blockCall =
            (*m_pPickers)[FPK_MMERANDOM_MACRO_BLOCK_CALL].Pick();

        if (blockCall == FPK_MMERANDOM_MACRO_BLOCK_CALL_Enable)
        {
            CHECK_RC(m_pCh->WriteIncOnce(s_3D,
                   LWA097_CALL_MME_MACRO(m_MacrosToRun[idx].pMacro->Index),
                   (UINT32)m_MacrosToRun[idx].InputData.size(),
                   &m_MacrosToRun[idx].InputData[0]));
        }
        else
        {
            CHECK_RC(m_pCh->Write(s_3D,
                   LWA097_CALL_MME_MACRO(m_MacrosToRun[idx].pMacro->Index),
                   m_MacrosToRun[idx].InputData[0]));
            for (UINT32 i = 1; i < m_MacrosToRun[idx].InputData.size(); i++)
            {
                CHECK_RC(m_pCh->Write(s_3D,
                    LWA097_CALL_MME_DATA(m_MacrosToRun[idx].pMacro->Index),
                    m_MacrosToRun[idx].InputData[i]));
            }
        }
    }

    // Disable MME to MemToMem at the end of the transfer
    CHECK_RC(SetMmeToMemToMem(false));

    CHECK_RC(SendInlinePadding(inlinePaddingCount));

    // Clear GPU cache on SOC
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        // Wait for previous engine method to finish (release host semaphore with WFI)
        CHECK_RC(m_pCh->SetSemaphoreOffset(m_FlushSemaphore.GetCtxDmaOffsetGpu()));
        CHECK_RC(m_pCh->SemaphoreRelease(0));

        // Flush GPU cache
        CHECK_RC(m_pCh->WriteL2FlushDirty());

        // Wait for L2 flush to complete
        CHECK_RC(m_pTestConfig->IdleChannel(m_pCh));
    }

    CHECK_RC(m_pCh->Flush());

    if (*pBytesThisSend != 0)
    {
        CHECK_RC(m_Notifier.Wait(1, m_pTestConfig->TimeoutMs()));
    }

    if (m_DumpMacrosPostRun)
    {
        DumpMacros(0, UNSIGNED_CAST(UINT32, m_MacrosToRun.size()), true);
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Validate that the output from the macros is valid
//!
//! \param pFailingIndex   : Pointer to returned first failing macro on an
//!                          error
//!
//! \return OK if successful and the output matches the desired output,
//!         not OK otherwise
//!
RC MMERandomTest::ValidateOutput(UINT32 * const pFailingIndex)
{
    UINT32 *pOutputData;
    UINT32  idx;
    UINT32  lwrOffset = 0;
    Tee::Priority showProgressPri = m_ShowSurfaceProgress ? Tee::PriHigh :
                                                            Tee::PriNone;
    Tasker::DetachThread threadDetach;

    Printf(showProgressPri, "Verifying Macro Output.");
    pOutputData = (UINT32 *)((UINT08 *)m_pSurface->GetAddress());
    for (idx = 0; idx < (UINT32)m_MacrosToRun.size(); idx++)
    {
        if (m_MacrosToRun[idx].bSkip || m_MacrosToRun[idx].bSkippedForTracing)
            continue;

        UINT32  outIdx;
        UINT32  actualOutput;

        for (outIdx = 0;
             outIdx < m_MacrosToRun[idx].OutputData.size();
             outIdx++, pOutputData++, lwrOffset += 4)
        {
            actualOutput = MEM_RD32(pOutputData);
            if (m_MacrosToRun[idx].OutputData[outIdx] != actualOutput)
            {
                *pFailingIndex = idx;
                Printf(showProgressPri, "ERROR!!\n");
                Printf(Tee::PriHigh,
                       "Miscompare detected running macro %d at "
                       "output value %d\n", idx, outIdx);
                Printf(Tee::PriHigh,
                       "Expected Value 0x%08x, Actual Value 0x%08x\n",
                       m_MacrosToRun[idx].OutputData[outIdx],
                       actualOutput);
                return RC::GOLDEN_VALUE_MISCOMPARE;
            }
            Tasker::PollMemSleep();
        }
        Printf(showProgressPri, ".");
    }

    Printf(showProgressPri, ".done\n");

    Printf(showProgressPri, "Verifying No Macro Overrun.");
    UINT32 startOffset = lwrOffset;
    UINT32 pitch = m_pSurface->GetPitch();

    // Validate that we did not overrun the expected output.  To ensure decent
    // test time only validate that we didnt overrun up to the end of the
    // current line plus the next line (it is highly unlikely that a macro will
    // generate 1+ lines of the background color)
    UINT32 endOffset = lwrOffset + (pitch - (lwrOffset % pitch)) + pitch;
    while ((lwrOffset < endOffset) && (lwrOffset < m_pSurface->GetSize()))
    {
        if (MEM_RD32(pOutputData) != m_BgColor)
        {
            *pFailingIndex = UNSIGNED_CAST(UINT32, (m_MacrosToRun.size() - 1));
            Printf(showProgressPri, "ERROR!!\n");
            Printf(Tee::PriHigh,
                   "Macro %d generated more output than expected\n",
                   *pFailingIndex);
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
        lwrOffset += 4;
        pOutputData++;
        Tasker::PollMemSleep();
        if (!((lwrOffset - startOffset) % 1024))
            Printf(showProgressPri, ".");
    }
    Printf(showProgressPri, ".done\n");

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Enable/Disable MME to MemToMem inline transfer
//!
//! \param bEnable  : true to enable MME to MemToMem inline transfer, false to
//!                   disable
//!
//! \return OK if successful, not OK otherwise
//!
RC MMERandomTest::SetMmeToMemToMem(bool bEnable)
{
    UINT32 offset, mask, data;
    RC     rc;

    GetBoundGpuSubdevice()->GetMmeToMem2MemRegInfo(bEnable, &offset,
                                                   &mask,   &data);

    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_SET_MME_SHADOW_SCRATCH(0),
                          3,
                          0,
                          data,
                          mask));
    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_SET_FALCON04,
                          offset));

    // TODO : Remove this once ucode is updated to not require NOPS when
    // a R-M-W is done via the falcon engine
    for (UINT32 i = 0; i < FALCON_PRI_RMW_NOP_COUNT; i++)
    {
        CHECK_RC(m_pCh->Write(s_3D, LWA097_NO_OPERATION, 0));
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Setup for mem to mem inline transfer
//!
//! \param Releases  : Number of releases for the transfer
//! \param pInlinePaddingCount : Returned number of inline data releases that
//!                              will be necessary to complete the transfer
//!
//! \return OK if successful, not OK otherwise
RC MMERandomTest::StartMemToMemInline(UINT32 Releases,
                                      UINT32 *pInlinePaddingCount)
{
    RC rc;

    UINT64 offset = m_pSurface->GetCtxDmaOffsetGpu();
    UINT32 pitch  = m_pSurface->GetPitch();

    MASSERT(pInlinePaddingCount);

    CHECK_RC(m_pCh->Write(s_3D, LWA097_OFFSET_OUT, UINT32(offset)));
    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_OFFSET_OUT_UPPER,
                          UINT32(offset >> 32)));
    //CHECK_RC(m_pCh->Write(s_3D, LWA097_PITCH_IN,       pitch));
    CHECK_RC(m_pCh->Write(s_3D, LWA097_PITCH_OUT,      pitch));
    CHECK_RC(m_pCh->Write(s_3D, LWA097_LINE_LENGTH_IN, pitch));
    CHECK_RC(m_pCh->Write(s_3D, LWA097_LINE_COUNT,
                          ((Releases * MME_BYTES_PER_RELEASE) / pitch) + 1));
    m_Notifier.Clear(1);

    *pInlinePaddingCount =
        (pitch - ((Releases * MME_BYTES_PER_RELEASE) % pitch)) /
         sizeof(UINT32);

    UINT64 NotifierOffset = m_Notifier.GetCtxDmaOffsetGpu(
        GetBoundGpuSubdevice()->GetSubdeviceInst()) + 1*16 + 12;

    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_SET_I2M_SEMAPHORE_A,
                          UINT32(NotifierOffset>>32)));
    CHECK_RC(m_pCh->Write(s_3D,
                          LWA097_SET_I2M_SEMAPHORE_B,
                          UINT32(NotifierOffset)));
    CHECK_RC(m_pCh->Write(s_3D, LWA097_SET_I2M_SEMAPHORE_C, 0));

    CHECK_RC(m_pCh->Write(s_3D, LWA097_LAUNCH_DMA,
         DRF_DEF(A097, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
         DRF_DEF(A097, _LAUNCH_DMA, _COMPLETION_TYPE, _RELEASE_SEMAPHORE) |
         DRF_DEF(A097, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
         DRF_DEF(A097, _LAUNCH_DMA, _SEMAPHORE_STRUCT_SIZE, _ONE_WORD)));
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Send the padding data to complete the inline transfer
//!
//! \param Count  : Amount of inline data to sent
//!
//! \return OK if successful, not OK otherwise
RC MMERandomTest::SendInlinePadding(UINT32 Count)
{
    if (!Count)
        return OK;

    UINT32 LoadInlineDataMethod = LWA097_LOAD_INLINE_DATA;

    vector<UINT32> PaddingData(PADDING_BLOCK_SIZE, m_BgColor);
    RC rc;
    UINT32 paddingToSend;
    while (Count)
    {
        paddingToSend = Count > PADDING_BLOCK_SIZE ? PADDING_BLOCK_SIZE : Count;
        CHECK_RC(m_pCh->WriteNonInc(s_3D,
                                    LoadInlineDataMethod,
                                    paddingToSend,
                                    &PaddingData[0]));
        Count -= paddingToSend;
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Dump the specified macros to the output
//!
//! \param StartIndex  : Index of first macro to dump
//! \param EndIndex    : Index of first non-dumped macro
//! \param bDumpOutput : true to dump the macro output
//!
void MMERandomTest::DumpMacros(const UINT32 StartIndex,
                               const UINT32 EndIndex,
                               const bool   bDumpOutput)
{
    UINT32 *pMacroOutput = (UINT32 *)m_pSurface->GetAddress();
    vector<UINT32> outputData;
    bool    bAnyMacrosDumped = false;

    for (UINT32 macroIdx = StartIndex;
          (macroIdx < EndIndex) && !bAnyMacrosDumped; macroIdx++)
    {
        bAnyMacrosDumped = !m_MacrosToRun[macroIdx].bSkip &&
                           !m_MacrosToRun[macroIdx].bSkippedForTracing;
    }
    if (!bAnyMacrosDumped)
        return;

    Printf(Tee::PriHigh,
           "******************** BEGIN MACRO DUMP ********************\n");

    for (UINT32 macroIdx = 0; macroIdx < EndIndex; macroIdx++)
    {
        if (m_MacrosToRun[macroIdx].bSkip ||
            m_MacrosToRun[macroIdx].bSkippedForTracing)
        {
            continue;
        }

        if (macroIdx < StartIndex)
        {
            pMacroOutput += (m_MacrosToRun[macroIdx].pMacro->Releases * 2);
            continue;
        }

        if (macroIdx >= StartIndex)
        {
            outputData.clear();
            if (bDumpOutput)
            {
                UINT32 outputIdx;
                UINT32 value;
                for (outputIdx = 0;
                     outputIdx < m_MacrosToRun[macroIdx].pMacro->Releases;
                     outputIdx++)
                {
                    value = MEM_RD32(pMacroOutput + (2 * outputIdx));
                    outputData.push_back(value);
                    value = MEM_RD32(pMacroOutput + (2 * outputIdx) + 1);
                    outputData.push_back(value);
                }
            }
            PrintMacro(m_MacrosToRun[macroIdx].pMacro,
                       m_MacrosToRun[macroIdx].InputData,
                       m_MacrosToRun[macroIdx].OutputData,
                       outputData);
        }

        pMacroOutput += (m_MacrosToRun[macroIdx].pMacro->Releases * 2);
    }

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh,
           "********************* END MACRO DUMP *********************\n");
}

//----------------------------------------------------------------------------
//! \brief Print a macro to the output
//!
//! \param pMacro             : Pointer to the input macro to print
//! \param InputData          : Input data of the macro
//! \param ExpectedOutputData : Expected output data of the macro
//! \param OutputData         : Output data of the macro
//!
void MMERandomTest::PrintMacro(const MmeMacro * const pMacro,
                               const vector<UINT32>  &InputData,
                               const vector<UINT32>  &ExpectedOutputData,
                               const vector<UINT32>  &OutputData)
{
    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "--------------- Macro Index %3d ---------------\n",
           pMacro->Index);
    Printf(Tee::PriHigh, "  Start Address : 0x%03x\n", pMacro->StartAddr);
    Printf(Tee::PriHigh, "  Size          : %d\n", pMacro->Size);
    Printf(Tee::PriHigh, "  Inputs        : %d (4 bytes per)\n",
           pMacro->Loads);
    Printf(Tee::PriHigh, "  Outputs       : %d (%d bytes per)\n",
           pMacro->Releases,
           MME_BYTES_PER_RELEASE);
    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "  Instructions  :\n");

    // Disassemble the macro instructions into a readable format
    for (UINT32 idx = 0; idx < pMacro->Size; idx++)
    {
        Printf(Tee::PriHigh, "    [%d] : %s\n",
               pMacro->StartAddr + idx,
               m_pMmeIGen->Disassemble(pMacro->Instructions[idx]).c_str());
    }

    // Print the macro data
    PrintFormattedData("Input Data", InputData);
    PrintFormattedData("Expected Output Data", ExpectedOutputData);
    PrintFormattedData("Output Data", OutputData);

    Printf(Tee::PriHigh, "-----------------------------------------------\n");
}

//----------------------------------------------------------------------------
//! \brief Print macro data in a formatted fashion
//!
//! \param Header     : Header for the data
//! \param Data       : Data to print
//!
void MMERandomTest::PrintFormattedData(const string          Header,
                                       const vector<UINT32> &Data)
{
    if (Data.size())
    {
        UINT32 idx;

        Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "  %s  :\n", Header.c_str());
        for (idx = 0; idx < Data.size(); idx++)
        {
            Printf(Tee::PriHigh, "    0x%08x", Data[idx]);
            if (((idx + 1) % 4) == 0)
            {
                Printf(Tee::PriHigh, "\n");
            }
        }
        Printf(Tee::PriHigh, "\n");
        if (Data.size() % 4)
        {
            Printf(Tee::PriHigh, "\n");
        }
    }
}

//------------------------------------------------------------------------------
// MMERandomTest.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(MMERandomTest, GpuTest, "MME Random Test");

CLASS_PROP_READWRITE(MMERandomTest, DumpMacroOnMiscompare, bool,
        "Dump the last batch of macros run on a miscompare");
CLASS_PROP_READWRITE(MMERandomTest, DumpMacrosPreRun, bool,
        "Dump the batch of macros about to be run");
CLASS_PROP_READWRITE(MMERandomTest, DumpMacrosPostRun, bool,
        "Dump the batch of macros after running");
CLASS_PROP_READWRITE(MMERandomTest, TraceStartLoop, UINT32,
        "Enable trace starting at the specified loop");
CLASS_PROP_READWRITE(MMERandomTest, TraceEndLoop, UINT32,
        "Enable trace ending at the specified loop");
CLASS_PROP_READWRITE(MMERandomTest, OnlyExecTracedLoops, bool,
        "When tracing, only execute the traced loops instead "
        "of all macros in the frame");
CLASS_PROP_READWRITE(MMERandomTest, VerifyShadowRam, bool,
        "Verify the hardware shadow ram agains the expected value");
CLASS_PROP_READWRITE(MMERandomTest, ShowSurfaceProgress, bool,
        "Print progress bars for surface access");
CLASS_PROP_READWRITE(MMERandomTest, PrintSummary, bool,
        "Print frame and test summaries");
