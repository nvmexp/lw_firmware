/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "ist_utils.h"          // For SETGET_PROP2 macro
#include "ist_mal_handler.h"    // Our replacement for MAL's CORE

#include <set>
#include <vector>
#include <string>
#include <memory>               // For shared_ptr
#include <sys/file.h>
#include <yaml-cpp/yaml.h>

class GpuDevice;
class ZeroPowerIdle;
class IstSmbus;
namespace image                 // For MAL
{
    class ImageMgr;
}
namespace MATHS_procblock_ns   // For MAL
{
    class MATHS_ProcBlock_impl;
}

namespace Ist
{
//--------------------------------------------------------------------
//! \brief In-Silicon-Test using MATHS-Link interface
//! \doc https://confluence.lwpu.com/x/goqxCQ
//--------------------------------------------------------------------
class IstFlow
{
    using ResultManager = MATHS_procblock_ns::MATHS_ProcBlock_impl;

    struct ShutdownPollingArgs
    {
        GpuDevice* pGpu = nullptr;
        UINT32 address = 0;
        UINT64 notValue = 0;
    };

public:
    struct FuseInfo
    {
        string name;
        UINT32 mask;
        UINT32 size;
    };

    using Fuses = vector<FuseInfo>;
    using FsTags = map<string, vector<string>>;


    using Path = std::string;
    using TestNumber = UINT32;    // aka test sequence (test inside an image)
    struct TestArgs
    {
        TestArgs(const Path& path)
            : path(path) {}

        const Path path;
        std::vector<TestNumber> testNumbers;
        uint16_t numLoops = 1;
    };

    // Specifies whether the image is dumped before or after the test runs
    enum class Dump
    {
        PRE_TEST,
        POST_TEST,
    };

    struct PllProgArgs
    {
        std::string pllName;
        UINT32 pllFrequencyMhz = 0;
    };

    struct VoltageProgInfo
    {
        std::string domain;
        UINT32 valueMv = 0;
    };

    struct JtagProgArgs
    {
        std::string jtagType;
        std::string jtagName;
        UINT08 width = 0;
        UINT32 value = 0;
    };

    struct VoltageSchmoo
    {
        std::string domain;
        UINT32 startValue = 0;
        UINT32 endValue = 0;
        UINT32 step = 0;
    };

    struct FrequencySchmoo
    {
        std::string pllName;
        UINT32 startValue = 0;
        UINT32 endValue = 0;
        UINT32 step = 0;
    };

    struct CvbInfoArgs
    {
        std::string testPattern;
        FLOAT32 voltageV = 0;
    };

    IstFlow(const std::vector<TestArgs>& testArgs);
    ~IstFlow();

    /**
     * \brief Check if IST HW exists and corresponding fuse is not blown
     */
    RC CheckHwCompatibility();

    /**
     * \brief Call InitGpu()
     */
    RC Initialize();

    /**
     * \brief Initialize the MATHS-ACCESS library
     */
    RC Setup();

    /**
     * \brief Call Runloop and Process the test result, return the result RC
     */
    RC Run();

    /**
     * \brief Deinitialize MATHS-ACCESS library and deallocate any memory
     *        allocated in/since Setup()
     */
    RC Cleanup();

    /**
     * \brief Polling function.
     * \param pArgs Expects a PollingArgs*
     * \return true if the value read is different to the given value
     */
    static bool PollNotValue(void* pArgs);

    /**
     * \brief Print the test records, error code and message
     */
    RC ProcessTestRecords();

private:

    /**
     * \brief Call RunTest multiple times, depending on the Loops property
     *        and the number of test images to run
     */
    RC RunLoopAmpere
    (
        const uint16_t numLoops,
        const string& path,
        bool* pTestWasRun,
        const std::vector<TestNumber>& testNums
    );

    RC RunLoopHopper
    (
        const uint16_t numLoops,
        const string& path,
        bool* pTestWasRun,
        const std::vector<TestNumber>& testNums
    );

    /**
     *  /brief Function pointer to run the main loop.
     *         A function pointer is used because the main
     *         running loop is implemented differenty
     *         per GPU family.
     */
    RC (IstFlow::* RunLoop)
    (
        const uint16_t numLoops,
        const std::string &path,
        bool* pTestWasRun,
        const std::vector<TestNumber> &testNumbers
    );

   /**
     *  \brief In preparation to running a test sequence. Reset device in IST mode.
     *  Clear test previous test result. 
     */
    RC PreRunTest(bool testWasRun);

    /**
     * \brief Run a single test sequence once. Fail if not in IST mode.
     */
    RC RunTest
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        const TestNumber& testNumber,
        boost::posix_time::ptime &exeTime
    );

    /**
     * \brief Execute the JTAG common image.
     */
    RC RunJTAGCommonProgram();


    /**
     *  \brief Execute JTAG shutdown
     */ 
    RC RunJTAGShutdownProgram();


    /**
     * Class for LockFile to control multi-instance of IST running (selene scenario)
     */
    class IstLockFile
    {
        public:
            IstLockFile() = default;
            ~IstLockFile()
            {
                CloseLockFile();
            };

            IstLockFile(IstLockFile&) = delete;
            IstLockFile& operator=(const IstLockFile&) = delete;

            RC CreateLockFile(const char *pLockFilename)
            {
                MASSERT(m_LockFileFd == -1);
                m_LockFileFd = open(pLockFilename, O_RDWR | O_CREAT, 0666);
                if (m_LockFileFd == -1)
                {
                    Printf(Tee::PriError, "IST lock file open failed\n");
                    return RC::CANNOT_OPEN_FILE;
                }
                m_pLockFilename = pLockFilename;

                Printf(Tee::PriDebug, "IST lock file %s created\n", m_pLockFilename);
                return RC::OK;
            };

            void CloseLockFile()
            {
                if (m_LockFileFd != -1) 
                {
                    ReleaseLockFile();
                    close(m_LockFileFd);
                    m_LockFileFd = -1;
                }

                return;
            };

            RC AcquireLockFile()
            {
                MASSERT(m_LockFileFd != -1);
                MASSERT(!m_LockFileAquired);
                if (flock(m_LockFileFd, LOCK_EX) != 0)
                {
                    Printf(Tee::PriError, "IST failed to apply lock %s\n", m_pLockFilename);
                    CloseLockFile();
                    return RC::FILE_ACCES;
                }

                m_LockFileAquired = true;
                Printf(Tee::PriDebug, "IST lock file Acquired %s\n", m_pLockFilename);

                return RC::OK;
            };

            void ReleaseLockFile()
            {
                if (m_LockFileAquired)
                {
                    flock(m_LockFileFd, LOCK_UN);
                    m_LockFileAquired = false;
                    Printf(Tee::PriDebug, "IST lock file Released %s\n", m_pLockFilename);
                }

                return;
            };
        private:
            int m_LockFileFd = -1;
            bool m_LockFileAquired = false;
            const char *m_pLockFilename = nullptr;
    };

    /**
     * \brief Wait for HW to start running test
     */
    RC PollOnLinkCheck
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        const TestNumber& testNumber,
        boost::posix_time::ptime &exeTime
    );
    
    /**
     * \brief Wait for HW to finish running test
     */
    RC PollOnShutdownRegister(const TestNumber& testNumber, FLOAT64 timeoutMs);

    /**
     * \brief Call Init::InitGpu
     */
    RC InitGpu(bool bIstMode);

    /**
     * \brief Release all pointers to pcie devices
     */
    static RC ReleaseGpu();

    /**
     * \brief Set the correct register and reset the GPU
     */
    RC ResetIntoIstMode();

    /**
     * \brief Use ZPI to reboot the GPU and re-enumerate
     */
    RC RebootIntoNormalMode();

    /**
     * \brief Use ZPI to reboot the GPU
     */
    RC ZpiPowerCycle();

    /**
     * \brief Load a test image into memory
     *        Allocates memory if it does not have enough or of the right format
     * \note  Templated to avoid a dependency with MAL in this file
     */
    template<typename T>
    RC LoadTestToSysMem
    (
        const std::string& path,
        T* pMemory
    );

    /**
     * \brief Return the first error code from the Test results if any
     */
    RC GetFirstError();

    /**
     * \brief Retrieve, interpret and output the test results
     */
    RC ProcessTestResults
    (
        shared_ptr<TestProgram_ifc> &testProgram,
        const TestNumber& testNumber,
        boost::posix_time::ptime &exeTime,
        bool isCommonOrShutdown = false
    );

    /**
     * \brief Check the Controller Status bits.
     *        The status bits are indication of irregularities in the test.
     *        This function get the number of failing bits and prints the
     *        controller status into a file if the relevant flag is set.
     */
    RC CheckControllerStatus
    (
        const shared_ptr<TestProgram_ifc> &testProgram, 
        const TestNumber &testNumber,
        UINT32 voltageOffset,
        UINT32 iterations,
        bool isCommonOrShutdown = false
    );

    /**
     * \brief Dump the test result to file.
     *        This function prints the Test Result
     *        into a file if the relevant flag is set.
     */
    RC DumpResultToFile
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        const TestNumber &testNumber,
        UINT32 voltageOffset,
        UINT32 iterations,
        bool isCommonOrShutdown = false
    );

    /**
     * \brief Clear the chunks in memory where the results were stored
     *        This is needed before a new test is run
     */
    RC ClearTestResults();

    /**
     * \brief Reset the test program result and constroller status bit
     *        using the exelwtion block
     */
    RC ResetTestProgram(const shared_ptr<TestProgram_ifc> &testProgram);

    /**
     * \brief Dump the image into a file. This function is called twice
     *        once after the test is loaded and a second time after the
     *        test runs and the result is written into the result area(s)
     */
    template<typename T>
    RC DumpImageToFile(const T &allocatedChunks, Dump order);
    
    /**
     * \brief Gather GA100 fuse information needed after result processing
     * \note This must be called before the GPU enters IST mode because
     *       fuses are no longer available after that
     **/
    RC GetGA100FuseInfo
    (   
        const GpuDevice& gpu, 
        Ist::IstFlow::FsTags* pFsTagMap,
        Ist::IstFlow::Fuses* pFuses
    );

    /**
     * \brief Gather GA10X fuse information needed after result processing
     * \note This must be called before the GPU enters IST mode because
     *       fuses are no longer available after that
     **/
    RC GetGA10xFuseInfo
    (   
        const GpuDevice& gpu, 
        Ist::IstFlow::FsTags* pFsTagMap,
        Ist::IstFlow::Fuses* pFuses
    );

    /**
     * \brief Gather GH100 fuse information needed after result processing
     * \note This must be called before the GPU enters IST mode because
     *       fuses are no longer available after that
     **/
    RC GetGH100FuseInfo
    (   
        const GpuDevice& gpu, 
        Ist::IstFlow::FsTags* pFsTagMap,
        Ist::IstFlow::Fuses* pFuses
    );

    /**
     * \brief Query the errors from the MATHS Access Library to determine which ones have FS
     * tags associated to them, returning the number of error with no FS tag association.
     * When the proper flag is set, this function prints of the per sequence per FS tag
     * error count. 
     **/
    size_t QueryMalErrorCount
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        const TestNumber &testNumber,
        size_t totalErrorCount,
        const boost::posix_time::ptime &exeTime
    );

     /* \brief Decrypt the encrypted Image1 Yaml file. The file constains
     * configuration setup.
     */
    RC DecryptImage1Yaml();

    /**
     * \brief Filter out the Mbist test Result for specific test program
     **/
    RC FilterMbistResult
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        const string& path
    );

    /**
     * \brief Program a specific pll to a frequency for specific test program
     */
    RC ProgramPllJtags
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        bool isCvbMode = false,
        bool isShmooOp = false
    );

    /**
     * \brief Program a specific/individual Jtag of a test program to a value.
     */
    RC ProgramJtags
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        bool isCvbMode = false
    );

    /**
     * \brief Voltage programming through Jtags
     */
    RC ProgramVoltageJtags
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        bool isShmooOp = false
    );

    /**
     * \brief Program all tags, whether Jtags or PllTags
     *  This function ultimately calls the two previous ones
     *  above.
     */
    RC ProgramAllTags(const shared_ptr<TestProgram_ifc> &testProgram);

    /**
     * \brief Translate the user provided LWPEX script into
     *        MAL compactible transactions.
     */
    RC SetMALProgramFlowToLWPEX();

    /**
     * \brief Extract the CVB info from cvb_equations.yml and callwlate
     * the frequency based on the equation.
     */
    RC CvbGetFreqMhz
    (
        const UINT32 ateSpeedoMhz,
        const UINT32 ateHvtSpeedoMhz,
        INT32 gpuTemp,
        UINT32* targetFreqMhz
    );

    /**
     * \brief Extract the CVB info from cvb_equations.yml and callwlate
     * the Lwvdd Voltage based on the equation .
     */
    RC CvbGetLwvddVoltMv
    (
        const UINT32 ateSpeedoMhz,
        const FLOAT32 voltStep,
        FLOAT32* targetVoltMv
    );

    /**
     * \brief Extract the basic Voltage setting from cvb_equations.yml
     * including MSVDD voltage (mV), High & Low limit of voltage (mV)
     */
    RC CvbGetVoltSettingMv
    (
        FLOAT32* msvddVoltMv,
        FLOAT32* voltLimitHighMv,
        FLOAT32* voltLimitLowMv
    );
    
    /**
     *  \brief Compute the voltage (mV) at which to set GPU.
     *  \note The VFE equations are provided by SSG. The equations differ with the ISTYPE. 
     *        As of now, the equation are provided for at-speed IST, the voltage is function 
     *        of the speedo (in MHz)For stuck-at and predictive the voltage is to 600mV and
     *        575mV respectively.
     *
     **/
    RC GetVfeVoltage
    (
        const string &istType,
        const UINT32 &ateSpeedoMhz,
        const UINT32 &frequency,
        const UINT32 &voltStep,
        UINT32* vfeVoltage
    );

    /**
     * \brief If applicable get the device Speedo. Speedo information can only be obtained
     * on specfic devices. (e.g non production).
     */
    RC GetAteSpeedo();

    /**
     * \brief Reads a user provided file and extract the proper frequency
     *  to program the pll/clocks (done through Jtags) and also extract
     *  volatge info to be written into registers (done by a user provided script)
     */
    RC ExtractCVBInfoFromFile();

    /**
     * \brief Print the table of elapsed time of several process in IST test.
     * The printing is only enabled when -print_time is used in the cmd line
     */
    RC PrintTimeStampTable();

    RC SaveTestProgramResult
    (
        const shared_ptr<TestProgram_ifc> &testProgram,
        const boost::posix_time::ptime &exeTime,
        UINT32 iteration
    );

    /**
     *  \brief Get the set of test program that can be exelwted
     */
    RC GetExelwtableTestProg();

    /**
     *  \brief Online burst image creation
     */
     RC CreateBurstImage();

    /**
     * \brief Testresult encapsulates the record for a test run
     */
    struct TestRecord
    {   
        RC rc;                      // Test result
        UINT32 iterations;          // Iteration for a test
        FLOAT32 targetVoltageMv;    // GPU target voltage(mV)
        UINT32  targetFrequencyMhz;    
        INT32 gpuTemp;              // GPU temperature read from SMBus  

        TestRecord
        (   
            RC rc, 
            UINT32 iterations,
            FLOAT32 targetVoltageMv,
            UINT32 targetFrequencyMhz,
            INT32 gpuTemp
        ) : rc(rc), iterations(iterations), targetVoltageMv(targetVoltageMv),
                targetFrequencyMhz(targetFrequencyMhz), gpuTemp(gpuTemp)
        {}

        string TestRecordLine() const
        {   
            return Utility::StrPrintf("%13.1f |  %9u |  %17d | %7u | %-s  \n", 
                            targetVoltageMv, iterations, gpuTemp, rc.Get(), rc.Message());
        }
    };

    // State machine to enforce correct sequence
    enum class State
    {
        UNDEFINED,      // Unknown
        NOT_SUPPORTED,  // We have determined that the feature is not supported
        NORMAL_MODE,    // GPU is in normal mode (not IST mode)
        IST_MODE,       // GPU is in IST mode
        IST_PAUSE_MODE, // GPU is in IST mode and pause state, for GH100
        ERROR,
    };

    std::map<string, string> m_UserScriptArgsMap = 
    {
        {"$dut_bdf", ""},
        {"$img_addr_lsb", ""},
        {"$img_addr_msb", ""},
        {"$hexlwvdd", ""},
        {"$hexmsvdd", ""}
    };

    State m_State = State::UNDEFINED;   // State of the state machine
    GpuDevice* m_pGpu = nullptr;
    std::shared_ptr<image::ImageMgr> m_TestImageManager;
    MATHS_CORE_ns::MalHandler m_CoreMalHandler; // Our core interface to the MAL

    std::vector<TestArgs> m_TestArgs;       // What the user wants to run
    FLOAT64 m_TestTimeoutMs = 10 * 1000;    // 10s default
    std::string m_PreIstScript;             // Path to script to run to initialize IST - if any
    std::string m_UserScript;
    std::string m_UserScriptArgs;
    bool m_RebootWithZpi = true;            // Try to use ZPI to reboot the chip
    std::string m_ImageDumpFile;            // Filename to dump memory content
    std::string m_CtrlStsDumpFile;          // Filename to dump controller status bits
    UINT32 m_CtrlStsError = 0;              // Number of controller status bits error
    std::string m_TestRstDumpFile;          // Filename to dump the test result

    std::string m_IstType;                  // Name of IST type image to be run
    UINT32 m_FrequencyMhz;                  // GPC clk frequency for IST type

    std::unique_ptr<ZeroPowerIdle> m_ZpiImpl;
    std::unique_ptr<IstSmbus> m_SmbusImpl;
    bool m_SmbusInitialized = false;
    FsTags m_FsTagMap;
    Fuses m_Fuses;
    std::string m_DevUnderTestBdf;          // BDF of the device under Test

    // Voltage (in mV) related variables
    FLOAT32 m_VoltageOverrideMv = 0;
    std::vector<FLOAT32> m_VoltageOverrideMvAry; // Multiple Voltage Override values array
    bool m_PreVoltageOverride = false;      // Predictive VoltageOverride disbaled
    UINT32 m_Repeat = 1;                    // Default to 1 run per test
    FLOAT32 m_VoltageOffsetMv = 0;
    bool m_PreVoltageOffset = false;
    std::map<std::string, std::vector<TestRecord>> m_PerTestProgramResults;
    FLOAT32 m_TargetVoltage = 0;            // GPU target voltage, read from register
    INT32 m_GpuTemp = INT_MIN;              // GPU temperature read from SMBus/BMC. Default invalid

    bool m_FbBroken = false;
    bool m_OffsetShmooDown = false;
    char* m_TestImage = nullptr;
    std::string m_ZpiScript;                // Path to external ZPI script to reset GPU
    bool m_SeleneSystem = false;
    IstLockFile m_SysMemLock;
    IstLockFile m_ZpiLock;
    bool m_DeepL1State = false;
    std::string m_FsArgs;                   // The floorsweeping arg
    bool m_PvsIgnoreError233 = false;       // Ignore ERROR 233, BAD_LWIDIA_CHIP
    bool m_CheckDefectiveFuse = false;
    bool m_PrintPerSeqFsTagErrCnt = false;
    bool m_IgnoreMbist = false;
    bool m_FilterMbist = false;
    FLOAT32 m_StepIncrementOffsetMv = 25;   // Default voltage increment 25mv
    bool m_IsPG199Board = false;            // PG199 board

    bool m_IsGa10x = false;                 // Whether the test is run on a GA10x
    YAML::Node m_Img1RootNode;
    std::string m_Img1Path;                 // Path to IMG1
    UINT32 m_NumRetryReadTemp = 30;         // Default Num of Retry allowed is 30
    std::vector<PllProgArgs> m_PllProgInfos;  
    std::vector<VoltageProgInfo> m_VoltageProgInfos;  
    std::vector<JtagProgArgs> m_JtagProgInfos;
    std::vector<JtagProgArgs> m_CvbMnpTagProgInfos;
    CvbInfoArgs m_CvbInfo;
    const std::string m_CbvEquationYmlPath = "cvb_equations.yml";
    UINT32 m_CvbTargetFreqMhz = UINT_MAX;
    std::string m_ChipID;                   // GPU name used by MAL
    UINT32 m_AteSpeedoMhz = UINT_MAX;
    UINT32 m_AteHvtSpeedoMhz = UINT_MAX;
    bool m_IsATETypeImage = false;          // No ATE type image run by default
    bool m_PrintTimes = false;
    std::map<string, UINT64> m_TimeStampMap = 
    {
        {"AllocateMemory", 0},
        {"ImageLoad", 0},
        {"ImageTestRun", 0},
        {"PreIstScript", 0}
    };
    std::set<UINT32> m_MbistSeqIndexes;      // Index of MBIST Test Sequences in image
    bool m_MALProgramFlowToLWPEX = false;
    std::string m_VoltageDomain;             // default domain
    UINT32 m_VoltageTargetMv;
    std::vector<VoltageSchmoo> m_VoltageSchmooInfos;
    std::vector<FrequencySchmoo> m_FrequencySchmooInfos;
    std::string m_PllTargetName;
    UINT32 m_PllTargetFrequencyMhz;
    UINT32 m_TestSeqRCSTimeoutMs;
    UINT32 m_InitTestSeqRCSTimeoutMs;
    std::vector<std::shared_ptr<TestProgram_ifc>> m_ExecTestProgs;
    bool m_SaveTestProgramResult = false;
    std::string m_CtrlStsRawFileName;
    std::string m_CtrlStsFileName;
    std::string m_TestRstFileName;
    bool m_SkipJTAGCommonInit = false;
    bool m_SkipJTAGShutdown = false;
    bool m_JtagCommonInitRan = false;
    UINT32 m_ExtraMemoryBlocks = 0;
    std::map<std::string, std::vector<std::string>> m_BurstConfig;
    std::vector<std::string> m_TestProgramsNames;
    bool m_IOMMUEnabled = false;

    // ZPI delay related
    UINT32 m_DelayAfterPwrDisableMs;
    UINT32 m_DelayBeforeOOBEntryMs;
    UINT32 m_DelayAfterOOBExitMs;
    UINT32 m_LinkPollTimeoutMs;
    UINT32 m_DelayBeforeRescanMs;
    UINT32 m_RescanRetryDelayMs;
    UINT32 m_DelayAfterPexRstAssertMs;
    UINT32 m_DelayBeforePexDeassertMs;
    UINT32 m_DelayAfterExitZpiMs;
    UINT32 m_DelayAfterPollingMs;
    UINT32 m_DelayAfterBmcCmdMs;

    // Getters / Setters
public:
    SETGET_PROP2(TestTimeoutMs);
    SETGET_PROP2(PreIstScript);
    SETGET_PROP2(UserScript);
    SETGET_PROP2(UserScriptArgs);
    SETGET_PROP2(RebootWithZpi);
    SETGET_PROP2(ImageDumpFile);
    SETGET_PROP2(CtrlStsDumpFile);
    SETGET_PROP2(TestRstDumpFile);
    SETGET_PROP2(IstType);
    SETGET_PROP2(FrequencyMhz);
    SETGET_PROP2(Repeat);
    SETGET_PROP2(VoltageOverrideMvAry);
    SETGET_PROP2(VoltageOffsetMv);
    SETGET_PROP2(FbBroken);
    SETGET_PROP2(ZpiScript);
    SETGET_PROP2(OffsetShmooDown);
    SETGET_PROP2(SeleneSystem);
    SETGET_PROP2(DeepL1State);
    SETGET_PROP2(FsArgs);
    SETGET_PROP2(CheckDefectiveFuse);
    SETGET_PROP2(PrintPerSeqFsTagErrCnt);
    SETGET_PROP2(IgnoreMbist);
    SETGET_PROP2(FilterMbist);
    SETGET_PROP2(NumRetryReadTemp);
    SETGET_PROP2(JtagProgInfos);
    SETGET_PROP2(VoltageProgInfos);
    SETGET_PROP2(CvbInfo);
    SETGET_PROP2(PrintTimes);
    SETGET_PROP2(VoltageSchmooInfos);
    SETGET_PROP2(FrequencySchmooInfos);
    SETGET_PROP2(TestSeqRCSTimeoutMs);
    SETGET_PROP2(InitTestSeqRCSTimeoutMs);
    SETGET_PROP2(SaveTestProgramResult);
    SETGET_PROP2(SkipJTAGCommonInit);
    SETGET_PROP2(SkipJTAGShutdown);
    SETGET_PROP2(ExtraMemoryBlocks);
    SETGET_PROP2(IOMMUEnabled);

    // ZPI delay related
    SETGET_PROP2(DelayAfterPwrDisableMs);
    SETGET_PROP2(DelayBeforeOOBEntryMs);
    SETGET_PROP2(DelayAfterOOBExitMs);
    SETGET_PROP2(LinkPollTimeoutMs);
    SETGET_PROP2(DelayBeforeRescanMs);
    SETGET_PROP2(RescanRetryDelayMs);
    SETGET_PROP2(DelayAfterPexRstAssertMs);
    SETGET_PROP2(DelayBeforePexDeassertMs);
    SETGET_PROP2(DelayAfterExitZpiMs);
};

} // namespace Ist
