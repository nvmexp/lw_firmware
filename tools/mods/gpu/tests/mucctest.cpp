/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/fileholder.h"
#include "core/include/jswrap.h"
#include "core/include/mle_protobuf.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gpumtest.h"
#include "gpu/utility/scopereg.h"
#include "../mucc/mucc.h"

// These bitfields describe the
// LW_PFB_FBPA_TRAINING_DEBUG_DQ0-LW_PFB_FBPA_TRAINING_DEBUG_DQ3
// registers, after writing
// LW_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_1ST_ERR_ADDR.
// These bitfields are described in
// //hw/doc/gpu/ampere/ampere/design/IAS/FB/FBPA_incr_uArch_ga100.docx
//
#define FIRST_ERROR0_RANK_AMPERE 0:0
#define FIRST_ERROR0_RANK_HOPPER 1:0
#define FIRST_ERROR0_RANK        17:16
#define FIRST_ERROR1_BANK        3:0
#define FIRST_ERROR2_COL         11:0
#define FIRST_ERROR2_ROW         26:12
#define FIRST_ERROR3_PC          0:0
// TODO bug 3115114: Update this bitfield once we know where arch will
// put the channel bit
#define FIRST_ERROR3_CHAN        1:1
static const UINT32 NUM_DQ_REGISTERS = 4;

// These bitfields tell how to enable ECC & DM in HBM mode register 4.
// They're dolwmented in chapter 5 of the JEDEC standard, JESD235A,
// \\netapp-hq\archive\datasheets\memory\HBM-GWIO\JEDEC\Component\JESD235A.pdf
// DM & ECC are mutually exclusive, and we want DM disabled for mucc.
//
#define HBM_MRS4_ECC          0:0
#define HBM_MRS4_ECC_DISABLED   0
#define HBM_MRS4_ECC_ENABLED    1
#define HBM_MRS4_DM           1:1
#define HBM_MRS4_DM_DISABLED    1
#define HBM_MRS4_DM_ENABLED     0

//--------------------------------------------------------------------
//! \brief Class for running a Mucc program to test HBM memory.
//!
//! A Mucc (Memory uController Characterization) program runs in a
//! microcontroller that drives an HBM bus.  There is one
//! microcontroller per subpartition.  The syntax is at
//! https://confluence.lwpu.com/display/GM/MuCC+Assembly+Language
//!
//! The mucc program is normally run from a standalone mucc.js
//! program, which does an implicit -skip_rm_state_init.  The plan is
//! to also run it from the gputest infrastructure.
//!
//! This class is lwrrently folwsed on Ampere architecture.
//!
//! TODO: To run from gputest.js, the Run() function needs to
//! temporarily shut down the the GPU in the RM, so that the mucc
//! program doesn't clobber any GPU memory owned by the RM.  Until
//! that is done, this class can only be used via the RunStandalone()
//! function from mucc.js.
//!
class MuccTest : public GpuMemTest
{
public:
    MuccTest(): GpuMemTest() { SetName("Mucc"); }
    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC RunStandalone();

    // Testargs
    //
    SETGET_PROP(Program,        string);
    SETGET_PROP(Define,         string);
    SETGET_PROP(DumpLaneRepair, string);
    SETGET_PROP(FbpaMask,       string);
    SETGET_PROP(NegativeTest,   UINT32);
    SETGET_PROP(OptionalEcc,    bool);
    SETGET_PROP(OptionalDbi,    bool);
    SETGET_PROP(Start,          string);
    SETGET_PROP(TimeoutMs,      FLOAT64);

private:
    // Errors returned by FinishProgram()
    //
    struct LaneError  // Number of errors on the DQ/ECC/DBI lanes
    {
        using Type = Memory::Lane::Type;
        static constexpr Type DQ  = Type::DATA;
        static constexpr Type ECC = Type::DM;
        static constexpr Type DBI = Type::DBI;

        UINT32 hwFbpa;
        UINT32 subp;
        Type   type;
        UINT32 laneNum;
        UINT32 errCount;
        UINT32 GetFbpaLaneNum(const GpuSubdevice* pGpuSubdevice) const;
        string GetLaneName(const GpuSubdevice* pGpuSubdevice) const;
    };
    struct FirstError
    {
        FbDecode rbcAddr;
        UINT32   hwFbpa;
        UINT32   dq[8];
        UINT32   ecc;
        UINT32   dbi;
    };
    struct Errors
    {
        vector<LaneError>  laneErrors;
        vector<FirstError> firstErrors;
    };

    // Private methods
    //
    static vector<UINT32> GetBitNumbers(UINT32 bitMask);
    RC AllocateResources();
    RC LoadThread(const Mucc::Thread& thread, UINT32 fbpaMask, UINT32 subp);
    RC StartProgram(UINT32 fbpaMask, UINT32 subp, UINT32 startAddr);
    RC FinishProgram(UINT32 hwFbpa, UINT32 subp, Errors* pErrors);
    void PrintFakeMatsinfo(const Errors& errors) const;
    void PrintMleMetadata(const Errors& errors) const;
    RC   DumpLaneRepairFile(const Errors& errors) const;

    // Testargs
    //
    string  m_Program;            //! The mucc program to run
    string  m_Define;             //! Comma-separated list of "symbol[=value]"
    string  m_DumpLaneRepair;     //! Dump a lane-repair file, arg is filename
    string  m_FbpaMask;           //! Colon-separated masks of FBPAs to run on
    UINT32  m_NegativeTest = 0;   //! Pass if the test has this many errors
    bool    m_OptionalEcc = false; //! Allow test to run with ECC disabled
    bool    m_OptionalDbi = false; //! Allow test to run with DBI disabled
    string  m_Start    = "start"; //! Start address
    FLOAT64 m_TimeoutMs = Tasker::NO_TIMEOUT;

    // Values set by AllocateResources()
    //
    GpuSubdevice*  m_pGpuSubdevice = nullptr;
    unique_ptr<Mucc::Program> m_pProgram; //! Compiled m_Program
    vector<UINT32> m_UnsweptMaskPerSubp;  //! Non-floorswept fbpas per subp
    vector<UINT32> m_RequestMaskPerSubp;  //! FBPAs they requested in m_FbpaMask

    // Other values
    //
    bool m_Standalone = false;  //! Set by RunStandalone()
};

//--------------------------------------------------------------------
//! \brief Overrides GpuMemTest::IsSupported()
//!
bool MuccTest::IsSupported()
{
    const GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    const FrameBuffer*  pFb           = pGpuSubdevice->GetFB();

    if (!m_Standalone && !GpuMemTest::IsSupported())
    {
        return false;
    }
    if (pGpuSubdevice->IsSOC())
    {
        return false;
    }
    if (!pFb->IsHbm())
    {
        return false;
    }
    switch (pGpuSubdevice->GetParentDevice()->GetFamily())
    {
        case GpuDevice::Maxwell:
        case GpuDevice::Pascal:
        case GpuDevice::Volta:
        case GpuDevice::Turing:
            // This test is only supported on HBM boards with Ampere
            // on up.  Eliminating the handful of pre-Ampere boards
            // that supported HBM.
            return false;
        default:
            break;
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief Overrides GpuMemTest::Setup()
//!
RC MuccTest::Setup()
{
    RC rc;
    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(AllocateResources());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Overrides GpuMemTest::Run()
//!
//! Load and run the Mucc program on the GPU
//!
RC MuccTest::Run()
{
    const FrameBuffer* pFb = m_pGpuSubdevice->GetFB();
    RegHal& regs = m_pGpuSubdevice->Regs();
    const UINT32 numSubpartitions = pFb->GetSubpartitions();
    const bool isHbm = pFb->IsHbm();
    const bool isEccEnabled =
        regs.Test32(MODS_PFB_FBPA_ECC_CONTROL_MASTER_EN_ENABLED);
    const bool isDbiEnabled =
        regs.Test32(MODS_PFB_FBPA_FBIO_CONFIG_DBI_IN_ON_ENABLED) &&
        regs.Test32(MODS_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON_ENABLED);
    RC rc;

    // Print warnings if skipping tests
    //
    const string testarg =
        m_Standalone ? "-muccarg" :
        Utility::StrPrintf("-testarg %d", ErrorMap::Test());
    if (!isEccEnabled && !m_OptionalEcc)
    {
        Printf(Tee::PriError,
            "ECC is disabled.  Use \"%s OptionalEcc true\" to run anyways.\n",
            testarg.c_str());
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
    if (!isDbiEnabled && !m_OptionalDbi)
    {
        Printf(Tee::PriError,
            "DBI is disabled.  Use \"%s OptionalDbi true\" to run anyways.\n",
            testarg.c_str());
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // Set priv level 3, and use RAII to restore the original priv
    // level on exit.
    //
    vector<unique_ptr<ScopedRegister>> privLevels;
    for (UINT32 hwFbpa: GetBitNumbers(pFb->GetFbioMask()))
    {
        privLevels.push_back(make_unique<ScopedRegister>(
            m_pGpuSubdevice, MODS_PFB_FBPA_0_UC_PRIV_LEVEL_MASK, hwFbpa));
        regs.Write32(
            MODS_PFB_FBPA_0_UC_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL3_ENABLE,
            hwFbpa);
        regs.Write32(
            MODS_PFB_FBPA_0_UC_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL3_ENABLE,
            hwFbpa);
    }

    // Set other temporary registers to enable ECC in the
    // microcontroller and HBM chips.
    //
    vector<unique_ptr<ScopedRegister>> tmpRegs;
    const ModsGpuRegValue hbmCharEcc = (isEccEnabled ?
                                        MODS_PFB_FBPA_0_HBM_CHAR_ECC_ENABLED :
                                        MODS_PFB_FBPA_0_HBM_CHAR_ECC_DISABLED);
    const ModsGpuRegValue hbmCharDbi = (isDbiEnabled ?
                                        MODS_PFB_FBPA_0_HBM_CHAR_DBI_ENABLED :
                                        MODS_PFB_FBPA_0_HBM_CHAR_DBI_DISABLED);
    const UINT32 hbmMrs4Ecc = (isEccEnabled ?
                               HBM_MRS4_ECC_ENABLED :
                               HBM_MRS4_ECC_DISABLED);
    for (UINT32 hwFbpa: GetBitNumbers(pFb->GetFbioMask()))
    {
        if (isHbm)
        {
            tmpRegs.push_back(make_unique<ScopedRegister>(
                m_pGpuSubdevice, MODS_PFB_FBPA_0_HBM_CHAR, hwFbpa));
            regs.Write32(hbmCharEcc, hwFbpa);
            regs.Write32(hbmCharDbi, hwFbpa);

            tmpRegs.push_back(make_unique<ScopedRegister>(
                m_pGpuSubdevice, MODS_PFB_FBPA_0_GENERIC_MRS4, hwFbpa));
            UINT32 mrs4 = *tmpRegs.back();
            mrs4 = FLD_SET_REF_NUM(HBM_MRS4_DM, HBM_MRS4_DM_DISABLED, mrs4);
            mrs4 = FLD_SET_REF_NUM(HBM_MRS4_ECC, hbmMrs4Ecc, mrs4);
            regs.Write32(MODS_PFB_FBPA_0_GENERIC_MRS4, hwFbpa, mrs4);

            tmpRegs.push_back(make_unique<ScopedRegister>(
                m_pGpuSubdevice, MODS_PFB_FBPA_0_REFCTRL, hwFbpa));
            regs.Write32(MODS_PFB_FBPA_0_REFCTRL_VALID_0, hwFbpa);

            tmpRegs.push_back(make_unique<ScopedRegister>(
                m_pGpuSubdevice, MODS_PFB_FBPA_0_TRAINING_ARRAY_CTRL, hwFbpa));
            regs.Write32(
                    MODS_PFB_FBPA_0_TRAINING_ARRAY_CTRL_GDDR5_COMMANDS_DISABLED,
                    hwFbpa);
            regs.Write32(MODS_PFB_FBPA_0_TRAINING_ARRAY_CTRL_PRBS_MODE_DISABLE,
                         hwFbpa);
        }
    }

    // This variable holds a per-subpartition bitmask of which FBPAs
    // have programs running on them.  It is initialized when we load
    // the mucc program to the GPU, and used to figure out which
    // FBPAs/subpartitions we should collect data from at the end.
    //
    vector<UINT32> runningFbpasPerSubp(numSubpartitions);

    // This variable holds a per-subpartition map of start addresses
    // to FBPAs that start at that address.  We can use a broadcast
    // register to start the mucc program *if* all FBPAs on a
    // subpartition start from the same start address.  This variable
    // is set when we load the mucc program to the GPU, and used when
    // we start the program.
    //
    vector<unordered_map<UINT32, UINT32>> subpStartToFbpas(numSubpartitions);

    // Loop through the threads, and download the threads to the GPU
    //
    for (const Mucc::Thread& thread: m_pProgram->GetThreads())
    {
        // Get a bitmask of which FBPAs we will load the thread to.
        // This bitmask is per subpartition, so that we can hopefully
        // take advantage of the broadcast registers, which can
        // broadcast to all FBPAs on a subpartition.
        //
        // This bitmask does not take floorsweeping or testargs into
        // account.
        //
        vector<UINT32> fbpasToRunPerSubp(numSubpartitions);
        const Mucc::ThreadMask& threadMask = thread.GetThreadMask();
        for (UINT32 threadBit = 0; threadBit < threadMask.size(); ++threadBit)
        {
            if (threadMask[threadBit])
            {
                const UINT32 hwFbpa = threadBit / numSubpartitions;
                const UINT32 subp   = threadBit % numSubpartitions;
                fbpasToRunPerSubp[subp] |= 1 << hwFbpa;
            }
        }

        // Get the start address of this thread.
        //
        const auto  pStartAddr = thread.FindLabel(m_Start);
        if (pStartAddr == thread.GetLabels().end())
        {
            Printf(Tee::PriError,
                   "Cannot find label \"%s\" in program \"%s\"\n",
                   m_Start.c_str(), m_Program.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Load the thread to the GPU, and update runningFbpasPerSubp
        // & subpStartToFbpas.
        //
        for (UINT32 subp = 0; subp < numSubpartitions; ++subp)
        {
            const UINT32 fbpasToRun = (fbpasToRunPerSubp[subp] &
                                       m_UnsweptMaskPerSubp[subp] &
                                       m_RequestMaskPerSubp[subp]);
            CHECK_RC(LoadThread(thread, fbpasToRun, subp));

            MASSERT((runningFbpasPerSubp[subp] & fbpasToRun) == 0);
            runningFbpasPerSubp[subp] |= fbpasToRun;
            subpStartToFbpas[subp][pStartAddr->second] |= fbpasToRun;
        }
    }

    // Start the program
    //
    for (UINT32 subp = 0; subp < numSubpartitions; ++subp)
    {
        for (const auto& startAndFbpas: subpStartToFbpas[subp])
        {
            const UINT32 startAddr = startAndFbpas.first;
            const UINT32 fbpaMask  = startAndFbpas.second;
            CHECK_RC(StartProgram(fbpaMask, subp, startAddr));
        }
    }

    // Wait for the program to finish, and collect the errors
    //
    Errors errors;
    for (UINT32 hwFbpa: GetBitNumbers(pFb->GetFbioMask()))
    {
        for (UINT32 subp = 0; subp < numSubpartitions; ++subp)
        {
            if (((runningFbpasPerSubp[subp] >> hwFbpa) & 1) != 0)
            {
                CHECK_RC(FinishProgram(hwFbpa, subp, &errors));
            }
        }
    }

    // Print the errors
    //
    if (m_Standalone)
    {
        PrintFakeMatsinfo(errors);
    }
    PrintMleMetadata(errors);
    if (!m_DumpLaneRepair.empty())
    {
        CHECK_RC(DumpLaneRepairFile(errors));
    }

    // Return an error code if memory errors were found
    //
    UINT32 errCount = 0;
    for (const LaneError& laneError: errors.laneErrors)
    {
        errCount += laneError.errCount;
    }
    if (errCount != m_NegativeTest)
    {
        return RC::BAD_MEMORY;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Overrides GpuMemTest::PrintJsProperties()
//!
void MuccTest::PrintJsProperties(Tee::Priority pri)
{
    GpuMemTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tProgram:\t%s\n", m_Program.c_str());
}

//--------------------------------------------------------------------
//! \brief Used to run the test from mucc.js
//!
RC MuccTest::RunStandalone()
{
    Utility::TempValue<bool> tmpStandalone(m_Standalone, true);
    RC rc;

    if (!IsSupported())
    {
        Printf(Tee::PriError, "Mucc is not supported on this GPU\n");
        CHECK_RC(RC::SPECIFIC_TEST_NOT_RUN);
    }
    CHECK_RC(AllocateResources());
    CHECK_RC(Run());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the lane number for the FBPA
//!
//! The laneNum field starts at 0 for each subpartition.  This method
//! returns the lane number as if the subpartitions in the FPBA were
//! combined into one giant bus.
//!
UINT32 MuccTest::LaneError::GetFbpaLaneNum
(
    const GpuSubdevice* pGpuSubdevice
) const
{
    const FrameBuffer* pFb            = pGpuSubdevice->GetFB();
    const RegHal&      regs           = pGpuSubdevice->Regs();
    const UINT32       dqBytesPerSubp = (pFb->GetBeatSize() *
                                         pFb->GetPseudoChannelsPerSubpartition());
    UINT32             bitsPerSubp    = 0;

    switch (type)
    {
        case LaneError::DQ:
        {
            bitsPerSubp = dqBytesPerSubp * 8;
            break;
        }
        case LaneError::ECC:
        {
            const UINT32 bitsPerDqWord =
                regs.LookupMaskSize(MODS_PFB_FBPA_TRAINING_DP_CNTD_EDC);
            bitsPerSubp = bitsPerDqWord * (dqBytesPerSubp / sizeof(UINT32));
            break;
        }
        case LaneError::DBI:
        {
            const UINT32 bitsPerDqWord =
                regs.LookupMaskSize(MODS_PFB_FBPA_TRAINING_DP_CNTD_DBI);
            bitsPerSubp = bitsPerDqWord * (dqBytesPerSubp / sizeof(UINT32));
            break;
        }
        default:
        {
            MASSERT(!"Illegal type");  // Should never get here
            break;
        }
    }
    return laneNum + subp * bitsPerSubp;
}

//--------------------------------------------------------------------
//! \brief Return a human-readable name for the lane
//!
//! The DQ, ECC, and DBI lanes have names such as A20, A_ECC20, and
//! A_DBI20, where "A" is the FBPA letter and "20" is the lane number.
//! The lanes are numbered as if the subpartitions in the FBPA were
//! combined into one giant bus.
//!
string MuccTest::LaneError::GetLaneName
(
    const GpuSubdevice* pGpuSubdevice
) const
{
    const char   busLetter   = 'A' + hwFbpa;
    const UINT32 fbpaLaneNum = GetFbpaLaneNum(pGpuSubdevice);

    switch (type)
    {
        case LaneError::DQ:
            return Utility::StrPrintf("%c%u", busLetter, fbpaLaneNum);
        case LaneError::ECC:
            return Utility::StrPrintf("%c_ECC%u", busLetter, fbpaLaneNum);
        case LaneError::DBI:
            return Utility::StrPrintf("%c_DBI%u", busLetter, fbpaLaneNum);
        default:
            MASSERT(!"Illegal type");  // Should never get here
            return "???";
    }
}

//--------------------------------------------------------------------
//! \brief Given a bitmask, return the indexes of bits set to 1
//!
//! Used for loops like "for (UINT32 fbpa: GetBitNumbers(fbpaMask))",
//! which loops through all the bits in fbpaMask.
//!
/* static */ vector<UINT32> MuccTest::GetBitNumbers(UINT32 bitMask)
{
    vector<UINT32> bits;
    for (INT32 bit = Utility::BitScanForward(bitMask); bit >= 0;
         bit = Utility::BitScanForward(bitMask, bit + 1))
    {
        bits.push_back(bit);
    }
    return bits;
}

//--------------------------------------------------------------------
//! \brief Compile the Mucc program, and analyze testargs & FS masks
//!
//! Called by Setup() and RunStandalone() to set up the test
//!
RC MuccTest::AllocateResources()
{
    m_pGpuSubdevice = GetBoundGpuSubdevice();
    const FrameBuffer* pFb = m_pGpuSubdevice->GetFB();
    RC rc;

    // Sanity checks
    //
    if (m_Program.empty())
    {
        Printf(Tee::PriError, "Must specify MuccTest.Program testarg\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Compile the MUCC program
    //
    LwDiagUtils::Preprocessor preprocessor;
    preprocessor.LoadFile(m_Program);

    vector<string> macros;
    CHECK_RC(Utility::Tokenizer(m_Define, ",", &macros));
    for (const string& macro: macros)
    {
        auto equalPos = macro.find('=');
        if (equalPos == string::npos)
        {
            preprocessor.AddMacro(macro.c_str(), "");
        }
        else
        {
            preprocessor.AddMacro(macro.substr(0, equalPos).c_str(),
                                  macro.substr(equalPos + 1).c_str());
        }
    }

    Mucc::Launcher muccLauncher;
    muccLauncher.SetLitter(static_cast<AmapLitter>(static_cast<UINT32>(
                            pFb->GetAmapLitter())));
    m_pProgram = muccLauncher.AllocProgram();
    CHECK_RC(Utility::InterpretModsUtilErrorCode(
                    m_pProgram->Assemble(&preprocessor)));

    // Get the un-floorswept FBPA mask per subpartition.  (They could
    // be different due to half-FBPA configurations.)
    //
    const UINT32 numSubpartitions = pFb->GetSubpartitions();
    const UINT32 fbpaMask         = pFb->GetFbioMask();
    const UINT32 halfFbpaMask     = pFb->GetHalfFbpaMask();

    m_UnsweptMaskPerSubp.resize(numSubpartitions);
    for (UINT32 subp = 0; subp < numSubpartitions; ++subp)
    {
        m_UnsweptMaskPerSubp[subp] = fbpaMask;
        if (subp == 0)
        {
            m_UnsweptMaskPerSubp[subp] &= ~halfFbpaMask;
        }
    }

    // Take the FbpaMask testarg, which represents the FBPAs that the
    // user requested we run on, and store it in m_RequestMaskPerSubp.
    //
    m_RequestMaskPerSubp.clear();
    m_RequestMaskPerSubp.reserve(numSubpartitions);
    if (m_FbpaMask.empty())
    {
        m_RequestMaskPerSubp.resize(numSubpartitions, ~0);
    }
    else
    {
        vector<string> requestStrings;
        CHECK_RC(Utility::Tokenizer(m_FbpaMask, ":", &requestStrings));
        if (requestStrings.size() != 1 &&
            requestStrings.size() != numSubpartitions)
        {
            Printf(Tee::PriError,
                   "FbpaMask testarg must be a number, or a colon-separated"
                   " list of %d numbers (one per subpartition)\n",
                   numSubpartitions);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        for (const string& requestString: requestStrings)
        {
            char* endp;
            const long num = strtol(requestString.c_str(), &endp, 0);
            if (*endp != '\0')
            {
                Printf(Tee::PriError,
                       "\"%s\" in FbpaMask testarg is not a number\n",
                       requestString.c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            m_RequestMaskPerSubp.push_back(static_cast<UINT32>(num));
        }
        if (requestStrings.size() == 1)
        {
            m_RequestMaskPerSubp.resize(numSubpartitions,
                                        m_RequestMaskPerSubp[0]);
        }
    }

    MASSERT(m_UnsweptMaskPerSubp.size() == numSubpartitions);
    MASSERT(m_RequestMaskPerSubp.size() == numSubpartitions);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Load a Mucc thread into the GPU
//!
//! Load a Mucc thread into a set of uControllers that all have the
//! same subpartition.  A "thread" in this context is a set of
//! instructions & patram that run on a uController; the Mucc source
//! code can specify that different instructions should run on
//! different uControllers.
//!
//! \param thread The thread to load
//! \param fbpaMask Bitmask of FBPAs on which to load the Mucc thread.
//!     This method uses broadcast-write registers if we're loading
//!     all FBPAs.
//! \param subp The subpartition on which to load the Mucc thread.
//!
RC MuccTest::LoadThread
(
    const Mucc::Thread& thread,
    UINT32 fbpaMask,
    UINT32 subp
)
{
    const bool doBroadcast = (fbpaMask == m_UnsweptMaskPerSubp[subp]);
    const FrameBuffer* pFb = m_pGpuSubdevice->GetFB();
    RegHal& regs = m_pGpuSubdevice->Regs();
    const UINT32 dqWordsPerPatramEntry = pFb->GetBurstSize() / sizeof(UINT32);
    const UINT32 eccBitsPerDqWord =
        regs.LookupMaskSize(MODS_PFB_FBPA_TRAINING_DP_CNTD_EDC);
    const UINT32 dbiBitsPerDqWord =
        regs.LookupMaskSize(MODS_PFB_FBPA_TRAINING_DP_CNTD_DBI);

    // Write the pattern ram.  The following pseudocode shows how we
    // write one entry of pattern ram:
    //
    // UINT32 dq[n];  // n=8 for HBM1 & HBM2
    // UINT04 ecc[n]; // There are eccBitsPerDqWord bits in a "UINT04"
    // UINT04 dbi[n]; // There are dbiBitsPerDqWord bits in a "UINT04"
    //
    // for ii = 0 to n-1:
    //     Write(LW_PFB_FBPA_<fbpa>_TRAINING_PATTERN_PTR(<subp>),
    //           _DP = 8 * entry + ii, _ACT_ADR = _ENABLED)
    //     Write(LW_PFB_FBPA_<fbpa>_TRAINING_DP_CNTD(<subp>),
    //           _SEL = _CHAR_ENGINE, _DBI = dbi[ii], _EDC = ecc[ii])
    //     Write(LW_PFB_FBPA_<fbpa>_TRAINING_DP(<subp>), dq[ii])
    //
    const vector<Mucc::Patram>& patrams = thread.GetPatrams();
    for (UINT32 entry = 0; entry < patrams.size(); ++entry)
    {
        const Mucc::Bits& dq  = patrams[entry].GetDq();
        const Mucc::Bits& ecc = patrams[entry].GetEcc();
        const Mucc::Bits& dbi = patrams[entry].GetDbi();
        MASSERT(dq.GetSize()  == dqWordsPerPatramEntry * 32);
        MASSERT(ecc.GetSize() == dqWordsPerPatramEntry * eccBitsPerDqWord);
        MASSERT(dbi.GetSize() == dqWordsPerPatramEntry * dbiBitsPerDqWord);
        for (UINT32 word = 0; word < dqWordsPerPatramEntry; ++word)
        {
            const UINT32 trainingPtr =
                regs.SetField(MODS_PFB_FBPA_TRAINING_PATTERN_PTR_DP,
                              dqWordsPerPatramEntry * entry + word) |
                regs.SetField(MODS_PFB_FBPA_TRAINING_PATTERN_PTR_ACT_ADR_ENABLED);
            const UINT32 trainingDpCntd =
                regs.SetField(MODS_PFB_FBPA_TRAINING_DP_CNTD_SEL_CHAR_ENGINE) |
                regs.SetField(MODS_PFB_FBPA_TRAINING_DP_CNTD_DBI,
                              dbi.GetBits(dbiBitsPerDqWord * (word + 1) - 1,
                                          dbiBitsPerDqWord * word)) |
                regs.SetField(MODS_PFB_FBPA_TRAINING_DP_CNTD_EDC,
                              ecc.GetBits(eccBitsPerDqWord * (word + 1) - 1,
                                          eccBitsPerDqWord * word));
            const UINT32 trainingDp = dq.GetData()[word];

            if (doBroadcast)
            {
                regs.Write32(MODS_PFB_FBPA_TRAINING_PATTERN_PTR, subp,
                             trainingPtr);
                regs.Write32(MODS_PFB_FBPA_TRAINING_DP_CNTD, subp,
                             trainingDpCntd);
                regs.Write32(MODS_PFB_FBPA_TRAINING_DP, subp, trainingDp);
            }
            else
            {
                for (UINT32 hwFbpa: GetBitNumbers(fbpaMask))
                {
                    regs.Write32(MODS_PFB_FBPA_0_TRAINING_PATTERN_PTR,
                                 {hwFbpa, subp}, trainingPtr);
                    regs.Write32(MODS_PFB_FBPA_0_TRAINING_DP_CNTD,
                                 {hwFbpa, subp}, trainingDpCntd);
                    regs.Write32(MODS_PFB_FBPA_0_TRAINING_DP,
                                 {hwFbpa, subp}, trainingDp);
                }
            }
        }
    }

    // Write the instructions.  The following pseudocode shows how we
    // write one instruction:
    //
    // UINT32 code[n];  // n=6 for Ampere uControllers
    // for ii = 0 to n-1:
    //     Write(LW_PFB_FBPA_<fbpa>_UC_DATA(<subp>), code[ii])
    // Write(LW_PFB_FBPA_<fbpa>_UC_CTL(<subp>),
    //       _LOAD_ENABLE = _TRUE, _LOAD_ADDR = address)
    //
    const vector<Mucc::Instruction>& instructions = thread.GetInstructions();
    for (UINT32 addr = 0; addr < instructions.size(); ++addr)
    {
        const vector<UINT32> words = instructions[addr].GetBits().GetData();
        const UINT32 ucCtl =
            regs.SetField(MODS_PFB_FBPA_UC_CTL_LOAD_ENABLE_TRUE) |
            regs.SetField(MODS_PFB_FBPA_UC_CTL_LOAD_ADDR, addr);
        if (doBroadcast)
        {
            for (UINT32 word: words)
            {
                regs.Write32(MODS_PFB_FBPA_UC_DATA, subp, word);
            }
            regs.Write32(MODS_PFB_FBPA_UC_CTL, subp, ucCtl);
        }
        else
        {
            for (UINT32 hwFbpa: GetBitNumbers(fbpaMask))
            {
                for (UINT32 word: words)
                {
                    regs.Write32(MODS_PFB_FBPA_0_UC_DATA, {hwFbpa, subp}, word);
                }
                regs.Write32(MODS_PFB_FBPA_0_UC_CTL, {hwFbpa, subp}, ucCtl);
            }
        }
    }

    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Start the Mucc program
//!
//! Start the Mucc program on a set of uControllers that all have the
//! same subpartition and start address.
//!
//! \param fbpaMask Bitmask of FBPAs on which to start Mucc
//! \param subp The subpartition on which to start Mucc
//! \param startAddr The address at which to start the program
//!
RC MuccTest::StartProgram(UINT32 fbpaMask, UINT32 subp, UINT32 startAddr)
{
    RegHal&    regs        = m_pGpuSubdevice->Regs();
    const bool doBroadcast = (fbpaMask == m_UnsweptMaskPerSubp[subp]);
    const UINT32 ucCtl =
        regs.SetField(MODS_PFB_FBPA_UC_CTL_ENABLE_TRUE) |
        regs.SetField(MODS_PFB_FBPA_UC_CTL_START_ADDR, startAddr);

    if (doBroadcast)
    {
        // Use a broadcast write if all of the FBPAs on this
        // subpartition start at the same startAddr
        //
        Printf(GetVerbosePrintPri(),
               "Starting program on subpartition %u...\n", subp);
        regs.Write32(MODS_PFB_FBPA_UC_CTL, subp, ucCtl);
    }
    else
    {
        // Otherwise, start the program on each FBPA at a time
        //
        for (UINT32 hwFbpa: GetBitNumbers(fbpaMask))
        {
            Printf(GetVerbosePrintPri(),
                   "Starting program on FBPA %u subpartition %u...\n",
                   hwFbpa, subp);
            regs.Write32(MODS_PFB_FBPA_0_UC_CTL, {hwFbpa, subp}, ucCtl);
        }
    }
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Wait for the Mucc program to finish, and collect errors
//!
//! This method waits for the Mucc program to finish on one
//! uController, identified by the hwFbpa & subp args.  Any errors
//! found in the uController are appended to *pErrors.
//!
//! In general, we read the error info by writing to the SELECT field
//! of the LW_PFB_FBPA_<fbpa>_TRAINING_DEBUG_CTRL(<subp>) register to
//! choose the error value(s) we want, and then reading those values
//! from the LW_PFB_FBPA_<fbpa>_TRAINING_DEBUG_DQ<idx>(<subp>)
//! registers, where <idx> ranges from 0 to 3 (NUM_DQ_REGISTERS-1).
//!
RC MuccTest::FinishProgram(UINT32 hwFbpa, UINT32 subp, Errors* pErrors)
{
    MASSERT(pErrors != nullptr);

    const FrameBuffer* pFb  = m_pGpuSubdevice->GetFB();
    RegHal&            regs = m_pGpuSubdevice->Regs();
    const size_t origNumLaneErrors = pErrors->laneErrors.size();
    const GpuDevice::GpuFamily family =
        m_pGpuSubdevice->GetParentDevice()->GetFamily();
    RC rc;

    // Wait for the program to finish
    //
    Printf(GetVerbosePrintPri(), "Waiting for FBPA %u subpartition %u...\n",
           hwFbpa, subp);
    CHECK_RC(Tasker::PollHw(
        m_TimeoutMs,
        [&]{ return regs.Test32(MODS_PFB_FBPA_0_UC_CTL_ENABLE_FALSE,
                                {hwFbpa, subp}); },
        __func__));

    // Collect the number of errors on each DQ, ECC, & DBI lane.
    //
    // The laneErrorConfigs array helps read the errors.  It tells
    // which values to write to the TRAINING_DEBUG_CTRL_SELECT field,
    // what type of lane errors (DQ/ECC/DBI) will be in the
    // TRAINING_DEBUG_DQ* registers after writing SELECT, and which
    // lane number will be in the least-significant byte of
    // TRAINING_DEBUG_DQ0.
    //
    Printf(GetVerbosePrintPri(),
           "Collecting results for FBPA %u subpartition %u...\n",
           hwFbpa, subp);

    struct LaneErrorConfig
    {
        ModsGpuRegValue regValue;
        LaneError::Type type;
        UINT32          laneNum0;
    };
    static const LaneErrorConfig laneErrorConfigs[] =
    {
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE0,
            LaneError::DQ, 0 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE1,
            LaneError::DQ, 16 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE2,
            LaneError::DQ, 32 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE3,
            LaneError::DQ, 48 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE4,
            LaneError::DQ, 64 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE5,
            LaneError::DQ, 80 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE6,
            LaneError::DQ, 96 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE7,
            LaneError::DQ, 112 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE8,
            LaneError::DBI, 0 },
        { MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_ERRCNT_WIDE9,
            LaneError::ECC, 0 },
    };
    for (const auto& laneErrorConfig: laneErrorConfigs)
    {
        regs.Write32(laneErrorConfig.regValue, {hwFbpa, subp});
        for (UINT32 ii = 0; ii < NUM_DQ_REGISTERS; ++ii)
        {
            const UINT32 word = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                            {hwFbpa, ii, subp});
            for (UINT32 jj = 0; jj < sizeof(UINT32); ++jj)
            {
                const UINT32 byte = (word >> (8 * jj)) & 0x00ff;
                if (byte != 0)
                {
                    LaneError laneError = {};
                    laneError.hwFbpa    = hwFbpa;
                    laneError.subp      = subp;
                    laneError.type      = laneErrorConfig.type;
                    laneError.laneNum   = (laneErrorConfig.laneNum0 +
                                           sizeof(UINT32) * ii + jj);
                    laneError.errCount  = byte;
                    pErrors->laneErrors.push_back(laneError);
                }
            }
        }
    }

    // If any lane errors were found, then writing HBM_CHAR_1ST_ERR_*
    // to TRAINING_DEBUG_CTRL_SELECT should give us detailed info
    // about the first error on this FBPA/subp.  Append that info to
    // pErrors->firstErrors.
    //
    if (pErrors->laneErrors.size() != origNumLaneErrors)
    {
        FirstError firstError = {};

        // Rank, bank, column, row, channel, and pseudochannel
        //
        regs.Write32(
            MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_1ST_ERR_ADDR,
            {hwFbpa, subp});
        const UINT32 word0 = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                         {hwFbpa, 0, subp});
        const UINT32 word1 = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                         {hwFbpa, 1, subp});
        const UINT32 word2 = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                         {hwFbpa, 2, subp});
        const UINT32 word3 = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                         {hwFbpa, 3, subp});
        switch (family)
        { 
            case GpuDevice::Ampere:
            case GpuDevice::Ada:
                firstError.rbcAddr.rank = REF_VAL(FIRST_ERROR0_RANK_AMPERE, word0);
                break;
            case GpuDevice::Hopper:
                firstError.rbcAddr.rank = REF_VAL(FIRST_ERROR0_RANK_HOPPER, word0);
                break;
            default:
                firstError.rbcAddr.rank = REF_VAL(FIRST_ERROR0_RANK, word0);
                break;
                
        }
        firstError.rbcAddr.bank          = REF_VAL(FIRST_ERROR1_BANK, word1);
        firstError.rbcAddr.extColumn     = REF_VAL(FIRST_ERROR2_COL,  word2);
        firstError.rbcAddr.row           = REF_VAL(FIRST_ERROR2_ROW,  word2);
        firstError.rbcAddr.pseudoChannel = REF_VAL(FIRST_ERROR3_PC,   word3);
        firstError.rbcAddr.channel       = REF_VAL(FIRST_ERROR3_CHAN, word3);

        firstError.rbcAddr.pseudoChannel += (pFb->GetPseudoChannelsPerChannel()
                                             * firstError.rbcAddr.channel);

        // The erroneous DQ bits for the full burst.
        //
        UINT32* pFirstErrorDq = &firstError.dq[0];
        regs.Write32(
            MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_1ST_ERR_DQ_L,
            {hwFbpa, subp});
        for (UINT32 ii = 0; ii < NUM_DQ_REGISTERS; ++ii)
        {
            *pFirstErrorDq++ = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                           {hwFbpa, ii, subp});
        }
        regs.Write32(
            MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_1ST_ERR_DQ_U,
            {hwFbpa, subp});
        for (UINT32 ii = 0; ii < NUM_DQ_REGISTERS; ++ii)
        {
            *pFirstErrorDq++ = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                           {hwFbpa, ii, subp});
        }
        MASSERT(pFirstErrorDq == &firstError.dq[0] + NUMELEMS(firstError.dq));

        // The erroneous ECC and DBI bits.
        //
        regs.Write32(
            MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_1ST_ERR_ECC,
            {hwFbpa, subp});
        firstError.ecc = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                     {hwFbpa, 0, subp});
        regs.Write32(
            MODS_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_1ST_ERR_DBI,
            {hwFbpa, subp});
        firstError.dbi = regs.Read32(MODS_PFB_FBPA_0_TRAINING_DEBUG_DQx,
                                     {hwFbpa, 0, subp});

        // Add extra info that we can derive about the first error
        //
        firstError.hwFbpa               = hwFbpa;
        firstError.rbcAddr.virtualFbio  = pFb->HwFbioToVirtualFbio(hwFbpa);
        firstError.rbcAddr.subpartition = subp;
        firstError.rbcAddr.burst = (firstError.rbcAddr.extColumn /
                                    pFb->GetPseudoChannelsPerChannel());
        if (pFb->GetRamProtocol() == FrameBuffer::RamHBM3)
        {
            CHECK_RC(pFb->FbioSubpToHbmSiteChannel(hwFbpa, subp,
                                                   firstError.rbcAddr.pseudoChannel,
                                                   &firstError.rbcAddr.hbmSite,
                                                   &firstError.rbcAddr.hbmChannel));
        }
        else
        {
            CHECK_RC(pFb->FbioSubpToHbmSiteChannel(hwFbpa, subp,
                                                   &firstError.rbcAddr.hbmSite,
                                                   &firstError.rbcAddr.hbmChannel));
        }

        // Append the error to pErrors
        //
        pErrors->firstErrors.push_back(firstError);
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Print a summary of Mucc memory errors similar to -matsinfo
//!
//! \param errors The memory errors returned by FinishProgram()
//!
void MuccTest::PrintFakeMatsinfo(const Errors& errors) const
{
    // Abort if there are no errors to print
    //
    if (errors.laneErrors.empty())
    {
        MASSERT(errors.firstErrors.empty());
        return;
    }

    // Print the total number of lane errors in each subpartition
    //
    Printf(Tee::PriNormal, "=== MEMORY ERRORS BY SUBPARTITION ===\n");
    Printf(Tee::PriNormal, "SUBPART ERRORS\n");
    Printf(Tee::PriNormal, "------- ------\n");

    map<string, UINT32> subpToErrCounts;
    for (const LaneError& laneError: errors.laneErrors)
    {
        string subpString = Utility::StrPrintf("FBIO%c%u",
                                               'A' + laneError.hwFbpa,
                                               laneError.subp);
        subpToErrCounts[subpString] += laneError.errCount;
    }

    for (const auto& subpAndErrCount: subpToErrCounts)
    {
        Printf(Tee::PriNormal, "%-7s %6u\n",
               subpAndErrCount.first.c_str(),
               subpAndErrCount.second);
    }
    Printf(Tee::PriNormal, "\n");

    // Print a list of all lanes that had failures, numbering the
    // lanes as if the subpartitions in the FBPA were combined into
    // one giant bus.
    //
    // The DQ, ECC, and DBI lanes have names such as A20, A_ECC20, and
    // A_DBI20, where "A" is the FBPA letter and "20" is the lane
    // number.
    //
    Printf(Tee::PriNormal, "=== MEMORY ERRORS BY BIT ===\n");
    Printf(Tee::PriNormal, "Failing Bits:\n");
    string outBuffer;
    for (const LaneError& laneError: errors.laneErrors)
    {
        outBuffer += " " + laneError.GetLaneName(m_pGpuSubdevice);
        if (outBuffer.size() >= 65 || &laneError == &errors.laneErrors.back())
        {
            Printf(Tee::PriNormal, "   %s\n", outBuffer.c_str());
            outBuffer.clear();
        }
    }
    Printf(Tee::PriNormal, "\n");

    Printf(Tee::PriNormal, "BIT      ERRORS\n");
    Printf(Tee::PriNormal, "---      ------\n");
    for (const LaneError& laneError: errors.laneErrors)
    {
        Printf(Tee::PriNormal, "%-8s %3u%s\n",
               laneError.GetLaneName(m_pGpuSubdevice).c_str(),
               laneError.errCount,
               laneError.errCount == 255 ? "+" : "");
    }
    Printf(Tee::PriNormal, "\n");

    // Print a table showing the RBC of the first error in each
    // subpartition
    //
    MASSERT(!errors.firstErrors.empty());
    Printf(Tee::PriNormal, "=== FIRST MEMORY ERROR PER SUBPARTITION ===\n");
    Printf(Tee::PriNormal, "P : Partition (FBIO)\n");
    Printf(Tee::PriNormal, "S : Subpartition\n");
    Printf(Tee::PriNormal, "H : HBM Site\n");
    Printf(Tee::PriNormal, "C : HBM Channel\n");
    Printf(Tee::PriNormal, "R : Rank (Stack ID)\n");
    Printf(Tee::PriNormal, "B : Bank\n");
    Printf(Tee::PriNormal, "U : PseudoChannel\n");
    Printf(Tee::PriNormal,
           "PSHCRBU ROW  COL BAD DBI  BAD ECC  BAD DQ BITS, MSB TO LSB\n");
    Printf(Tee::PriNormal,
           "------- ---- --- -------- -------- -----------------------------------------------------------------------\n");
    for (const auto& firstError: errors.firstErrors)
    {
        string dqBits = "";
        dqBits.reserve(NUMELEMS(firstError.dq) * 9);
        for (size_t ii = NUMELEMS(firstError.dq); ii > 0; --ii)
        {
            dqBits += Utility::StrPrintf("%08x ", firstError.dq[ii - 1]);
        }
        dqBits.resize(dqBits.size() - 1);
        const FbDecode& rbcAddr = firstError.rbcAddr;
        Printf(Tee::PriNormal, "%c%x%x%c%x%x%x %04x %03x %08x %08x %s\n",
               'A' + firstError.hwFbpa,
               rbcAddr.subpartition,
               rbcAddr.hbmSite,
               'a' + rbcAddr.hbmChannel,
               rbcAddr.rank,
               rbcAddr.bank,
               rbcAddr.pseudoChannel,
               rbcAddr.row,
               rbcAddr.extColumn,
               firstError.dbi,
               firstError.ecc,
               dqBits.c_str());
    }
    Printf(Tee::PriNormal, "\n");
}

//--------------------------------------------------------------------
//! \brief Print MLE metadata
//!
//! \param errors The memory errors returned by FinishProgram()
//!
void MuccTest::PrintMleMetadata(const Errors& errors) const
{
    for (const LaneError& laneError: errors.laneErrors)
    {
        Mle::LaneErrors::Type laneType = Mle::LaneErrors::unknown_ty;
        switch (laneError.type)
        {
            case Memory::Lane::Type::DATA:    laneType = Mle::LaneErrors::data; break;
            case Memory::Lane::Type::DBI:     laneType = Mle::LaneErrors::dbi; break;
            case Memory::Lane::Type::DM:      laneType = Mle::LaneErrors::dm; break;
            case Memory::Lane::Type::ADDR:    laneType = Mle::LaneErrors::addr; break;
            case Memory::Lane::Type::UNKNOWN: laneType = Mle::LaneErrors::unknown_ty; break;
            default:
                MASSERT(!"Unknown RepairType colwersion");
                break;
        }

        Mle::Print(Mle::Entry::lane_errors)
            .name(laneError.GetLaneName(m_pGpuSubdevice))
            .hw_fbpa(laneError.hwFbpa)
            .fbpa_lane(laneError.GetFbpaLaneNum(m_pGpuSubdevice))
            .type(laneType)
            .errors(laneError.errCount);
    }

    for (const FirstError& firstError: errors.firstErrors)
    {
        // The generic MemErrorDecodeMetadata class doesn't have
        // separate fields for the DQ, ECC, & DBI busses, so for Mucc
        // we combine them into a "super bus" and indicate the layout
        // in the "type" string.
        //
        vector<UINT32> bits(firstError.dq,
                            firstError.dq + NUMELEMS(firstError.dq));
        bits.push_back(firstError.ecc);
        bits.push_back(firstError.dbi);
        Mle::Print(Mle::Entry::mem_err)
            .location("FB")
            .type("MUCC_256DQ_32ECC_32DBI")
            .fbio(firstError.hwFbpa)
            .subp(firstError.rbcAddr.subpartition)
            .pseudo_chan(firstError.rbcAddr.pseudoChannel)
            .bank(firstError.rbcAddr.bank)
            .row(firstError.rbcAddr.row)
            .col(firstError.rbcAddr.extColumn)
            .rank(firstError.rbcAddr.rank)
            .hbm_site(firstError.rbcAddr.hbmSite)
            .hbm_chan(firstError.rbcAddr.hbmChannel)
            .bits(bits);
    }
}

//--------------------------------------------------------------------
//! \brief Write the lane-repair JS file
//!
//! If the user specified the DumpLaneRepair testarg, then write a JS
//! containing lane-repair info, using the format specified at
//! https://confluence.lwpu.com/display/GM/Memory+Repair#MemoryRepair-LaneRepairJSFileContents
//!
RC MuccTest::DumpLaneRepairFile(const Errors& errors) const
{
    RC rc;

    // Do nothing if there user did not request a lane-repair file
    //
    if (m_DumpLaneRepair.empty())
    {
        return rc;
    }

    // Open the file
    //
    FileHolder fileHolder;
    CHECK_RC(fileHolder.Open(m_DumpLaneRepair, "w"));
    FILE* pFile = fileHolder.GetFile();

    // Write the file
    //
    for (const LaneError& laneError: errors.laneErrors)
    {
        const UINT32 laneNum  = laneError.GetFbpaLaneNum(m_pGpuSubdevice);
        const string laneName = laneError.GetLaneName(m_pGpuSubdevice);
        string       repairType;

        switch (laneError.type)
        {
            case LaneError::DQ:
                repairType = "Data";
                break;
            case LaneError::ECC:
                repairType = "DM";
                break;
            case LaneError::DBI:
                repairType = "DBI";
                break;
            default:
                MASSERT(!"Illegal type");  // Should never get here
                break;
        }

        fprintf(pFile, "g_FailingLanes[\"%s\"] = {\n",    laneName.c_str());
        fprintf(pFile, "    \"LaneName\": \"%s\",\n",     laneName.c_str());
        fprintf(pFile, "    \"HwFbio\": \"%d\",\n",       laneError.hwFbpa);
        fprintf(pFile, "    \"Subpartition\": \"%d\",\n", laneError.subp);
        fprintf(pFile, "    \"LaneBit\": \"%d\",\n",      laneNum);
        fprintf(pFile, "    \"RepairType\": \"%s\"\n",    repairType.c_str());
        fprintf(pFile, "};\n");
        fprintf(pFile, "\n");
    }
    return rc;
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(MuccTest, GpuMemTest,
    "Mucc (Memory uController Characterization) Test");
JS_REGISTER_METHOD(MuccTest, RunStandalone,
    "Run MuccTest without the GpuTest infrastructure");
CLASS_PROP_READWRITE(MuccTest, Program, string,
    "Program to run (required)");
CLASS_PROP_READWRITE(MuccTest, Define, string,
    "Comma-separated list of macro[=value] preprocessor macros");
CLASS_PROP_READWRITE(MuccTest, DumpLaneRepair, string,
    "Dump a JavaScript lane-repair file.  The argument is the filename.");
CLASS_PROP_READWRITE(MuccTest, FbpaMask, string,
    "FBPA mask to run on, or colon-separated list of masks (one per subp)");
CLASS_PROP_READWRITE(MuccTest, NegativeTest, UINT32,
    "Pass if the test has the indicated number of errors.  Default is 0.");
CLASS_PROP_READWRITE(MuccTest, OptionalEcc, bool,
    "Allow Mucc to run with ECC disabled.  Default is false (mandatory ECC).");
CLASS_PROP_READWRITE(MuccTest, OptionalDbi, bool,
    "Allow Mucc to run with DBI disabled.  Default is false (mandatory DBI).");
CLASS_PROP_READWRITE(MuccTest, Start, string,
    "Start address; specified as a label.  Default is \"start\".");
CLASS_PROP_READWRITE(MuccTest, TimeoutMs, double,
    "Timeout for the mucc program to finish, in ms.  Default is -1 (forever).");
