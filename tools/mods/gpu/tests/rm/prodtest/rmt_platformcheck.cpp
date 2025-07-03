/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2015-2016 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include <sys/stat.h>
#include <string> // Only use <> for built-in C++ stuff
#include "core/include/jscript.h"
#include "gpu/utility/rmclkutil.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "lwmisc.h"
#include "core/include/memcheck.h"
#include "gpu/perf/perfutil.h"

#define LWCTRL_DEVICE_HANDLE                (0xbb008001)
#define LWCTRL_SUBDEVICE_HANDLE             (0xbb208001)
#define GOLD_FILE_LENGTH 30
#define MAX_PREFIX_LENGTH 80
#define MAX_VALUE_LENGTH 80
#define MAX_NAME_LENGTH 80
#define MAX_TMP_LENGTH 80
#define MAX_TYPE_LENGTH 5
/*
// NOTE: This test always generates a gold file based on the platform devid
// and subdevice id like so :
// ./platformcheck_gold_values_Device_0xXXXXXXXX_SubDevice_0xXXXXXXXX.txt.new
// in order to use this gold remove the .new from the filename and check in
// Test will now use the .txt file to compare instead of generating a new gold
*/
class PlatformCheck : public RmTest
{
public:
    LwRm::Handle m_hClient;
    LwRm::Handle m_hSubDev;
    PlatformCheck();
    virtual ~PlatformCheck();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific variables
    LwRmPtr pLwRm;
    UINT32 m_rmStatus;
    GpuSubdevice  *m_pSubdevice;
    Perf          *m_pPerf;
    RmtClockUtil m_ClkUtil;
    enum value_type {VALUE_TYPE_INT, VALUE_TYPE_STRING, VALUE_TYPE_ERR};
    //@}

    //@{
    //! Test specific functions
    void ResetState();

    void TestProductNameUnicode();
    void TestShortName();
    RC TestClocks();
    void TestPCIEinfo();
    RC TestPCIECounters();
    RC TestCoreVoltage(UINT32 PStateNum);
    RC TestPState20Data();
    void TestGPIOs();

    UINT32 getNumVPStates();
    UINT32 getNumPStates();
    UINT32 getPCIEInfoParam(UINT32 i,  const char** name);

    void setVPState(UINT32 pstate);
    void setPState(UINT32 pstate);

    bool isValidPState(UINT32 pstate);

    // Golden value functions
    RC GoldenFileInit();
    void checkInt(string name, UINT32 value);
    void checkChar(string name, string value);
    void putErr(string name, UINT32 value);
    void GoldenFileInsert(string name, void* value, value_type type);
    void GoldenAddPrefix(string value);
    void GoldenRemovePrefix(string value);
    void setError(string value);
    void GoldenFileCompare(string name, void* value, value_type type);
    void PrintInserted(Tee::Priority pri, string name, void* value, value_type type);

    //GoldValues internal varibles
    char m_tmpChar[MAX_TMP_LENGTH];
    FILE *m_fpGoldFile;
    UINT32 m_fGoldID;
    string m_goldPrefix;
    bool isError;
    bool isGoldCapturing;
    bool isGoldInit;
    //@}
};

//! \brief Simple Constructor.
//!
//! \sa Reserts state and sets test name.
//------------------------------------------------------------------------------
PlatformCheck::PlatformCheck() :
    m_pSubdevice(NULL),
    m_pPerf(NULL)
{
    SetName("PlatformCheck");
    ResetState();
}

//! \brief simple destructor.
//!
//! \sa does nothing
//------------------------------------------------------------------------------
PlatformCheck::~PlatformCheck()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! returns RUN_RMTEST_TRUE.
//------------------------------------------------------------------------------
string PlatformCheck::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test
//!
//! init all vars
//! init gold value file or prepare to write gold value file
//!
//! gets hClient and hDevice for future rmcalls
//!
//! \return RC -> OK if the test can be Run, test-specific RC if something
//!         failed while selwring resources
//------------------------------------------------------------------------------
RC PlatformCheck::Setup()
{
    RC rc = OK;

    m_rmStatus = 0; //global m_rmStatus var
    m_pSubdevice = NULL;
    m_pPerf = NULL;
    memset(m_tmpChar, 0, sizeof(m_tmpChar));

    m_pSubdevice = GetBoundGpuSubdevice();
    m_pPerf = m_pSubdevice->GetPerf();
    m_hSubDev = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    m_hClient = pLwRm->GetClientHandle();

    // return error if any of the above did not get allocated
    if(!m_pSubdevice || !m_pPerf || !m_hSubDev || !m_hClient)
        return RC::LWRM_INSUFFICIENT_RESOURCES;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    CHECK_RC(GoldenFileInit());

    //
    // Enable the Clk changes, MODS disabled it, enable for this test
    // Last Argument reprersenting enabled should be true in this case
    //
    CHECK_RC(m_ClkUtil.ModsEnDisAble(m_hClient, m_hSubDev, true));

    // get and dump active level ensure
    // to everything is working before we continue
    Printf(Tee::PriHigh, "Dumping current perf level...\n");
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};
    getLwrrLevelInfoParams.lwrrLevel = 0xffffffff;
    m_rmStatus = LwRmControl(m_hClient, m_hSubDev,
                             LW2080_CTRL_CMD_PERF_GET_LWRRENT_LEVEL,
                             &getLwrrLevelInfoParams,
                             sizeof (getLwrrLevelInfoParams));

    if (m_rmStatus != LW_OK)
    {
        Printf(Tee::PriHigh,
               "CMD_PERF_GET_LWRRENT_LEVEL failed: 0x%x , %d\n",
               m_rmStatus, __LINE__);

        // Mask error in case feature isn't supported.
        if (m_rmStatus == LW_ERR_NOT_SUPPORTED)
        {
            m_rmStatus = LW_OK;
        }

        return RmApiStatusToModsRC(m_rmStatus);
    }

    Printf(Tee::PriDebug, "  current level: %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel));

    return OK;
}

//! \brief Run the test!
//!
//!  This is the test plan
//!  1. Marketing name
//!  2. loop over the different pstates and check clocks
//!  3. dump PCIe settings like PCie gen speed, ASPM etc
//!  4. voltage settings
//!  5. GPIO settings
//!
//!  This test will either write values to the gold file
//!  or compare values gotten with gold file.
//!  if there are any mismatches isError will be set to true
//!
//! \return OK if all went well
//!         LWRM_ERROR if Gold mismatch
//------------------------------------------------------------------------------
RC PlatformCheck::Run()
{
    RC rc = OK;
    UINT32 numPstates = 0;
    UINT32 pState = 0;

    // 1. Marketing Name
    TestProductNameUnicode();
    TestShortName();

    // First get number of VPstates to loop over:
    numPstates = getNumPStates();

    if (numPstates < 1)
    {
        Printf(Tee::PriHigh, "ERROR could not get pState\n");
    }
    else
    {
        Printf(Tee::PriDebug, "numVPStates = %d", numPstates );
    }

    // loop over Pstates and dump clocks.
    for (pState = 0; pState < numPstates; pState++)
    {
        if(!isValidPState(pState))
            continue;

        sprintf(m_tmpChar, "PSTATE_%d", pState);
        GoldenAddPrefix(m_tmpChar);

        setPState(pState);

        // 2. check clocks
        TestClocks();

        // 3. check PCIe settings like PCie gen speed, ASPM etc
        TestPCIEinfo();
        TestPCIECounters();

        // 4.1 check voltage per pstate
        TestCoreVoltage(pState);

        // 5. check GPIO's
        TestGPIOs();

        sprintf(m_tmpChar, "PSTATE_%d", pState);
        GoldenRemovePrefix(m_tmpChar);
    }

    // 4.2 more voltage settings
    TestPState20Data();

    if (isError == true)
        rc = RC::LWRM_ERROR;

    return rc;
}

//! \brief Free any resources that the test selwred
//!
//! Closes GoldFile if it exist
//!
//! \return OK if all went well
//------------------------------------------------------------------------------
RC PlatformCheck::Cleanup()
{
    if(m_fpGoldFile)
        fclose(m_fpGoldFile);

    ResetState();
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief Reset member variables to default values
//------------------------------------------------------------------------------
void PlatformCheck::ResetState()
{
    m_fGoldID = 0;
    m_goldPrefix = "";
    isError = false;
    isGoldCapturing = false;
    isGoldInit = false;
    memset(m_tmpChar, 0, sizeof(m_tmpChar));
}

//! \brief Test Product Name
//------------------------------------------------------------------------------
void PlatformCheck::TestProductNameUnicode()
{
    GoldenAddPrefix(__FUNCTION__);
    LW2080_CTRL_GPU_GET_NAME_STRING_PARAMS params = {0};

    params.gpuNameStringFlags = LW2080_CTRL_GPU_GET_NAME_STRING_FLAGS_TYPE_UNICODE;

    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_GPU_GET_NAME_STRING,
                             &params,
                             sizeof(params));

    if (m_rmStatus == LW_OK)
    {
        LwU16 *pUnicode = params.gpuNameString.unicode;
        while (*pUnicode)
        {
            checkInt("gpuNameString.unicode", (UINT32) *pUnicode);
            pUnicode++;
        }
    }
    else
    {
        putErr("LW2080_CTRL_CMD_GPU_GET_NAME_STRING", m_rmStatus);
    }

    GoldenRemovePrefix(__FUNCTION__);
}

//! \brief Test Product Short Name
//------------------------------------------------------------------------------
void PlatformCheck::TestShortName()
{
    GoldenAddPrefix(__FUNCTION__);
    LW2080_CTRL_GPU_GET_SHORT_NAME_STRING_PARAMS params;

    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_GPU_GET_SHORT_NAME_STRING,
                             &params,
                             sizeof(params));

    if (m_rmStatus == LW_OK)
    {
        checkChar("gpuNameStringAscii", (char*) params.gpuShortNameString);
    }
    else
    {
        putErr("LW2080_CTRL_CMD_GPU_GET_SHORT_NAME_STRING", m_rmStatus);
    }

    GoldenRemovePrefix(__FUNCTION__);
}

//! \brief Checks if input Pstate is valid.
//! \return true if valid, false if not
//------------------------------------------------------------------------------
bool PlatformCheck::isValidPState(UINT32 pstate)
{
    bool isValidPState = false;
#ifdef LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS
    isValidPState = m_pPerf->HasVirtualPState((Perf::GpcPerfMode)pstate);
#else
    m_pPerf->DoesPStateExist(pstate, &isValidPState);
#endif
    return isValidPState;
}

//! \brief gets number of pstates
//! \return number of pStates, 0 means there are no pstates
//------------------------------------------------------------------------------
UINT32 PlatformCheck::getNumPStates()
{
#ifdef LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS
    return getNumVPStates();
#else
    UINT32 numPstates;
    m_pPerf->GetNumPStates(&numPstates);
    return numPstates;
#endif
}
//! \brief gets number of vpstates
//! \return number of vpStates, 0 means there are no vpstates
//------------------------------------------------------------------------------
UINT32 PlatformCheck::getNumVPStates()
{
    GoldenAddPrefix(__FUNCTION__);
    UINT32 numVPStates;
    LW2080_CTRL_PERF_GET_FSTATE_PARAMS fStateParams = {0};

    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_PERF_GET_FSTATE,
                             &fStateParams,
                             sizeof(fStateParams));

    if(m_rmStatus == LW_OK)
    {
        numVPStates = (fStateParams.max + 1);
        checkInt("numVPstates", numVPStates);
    }
    else
    {
        numVPStates = 0;
        putErr("LW2080_CTRL_CMD_PERF_GET_FSTATE", m_rmStatus);
    }

    GoldenRemovePrefix(__FUNCTION__);
    return numVPStates;
}

//! \brief sets pstate
//------------------------------------------------------------------------------
void PlatformCheck::setPState(UINT32 pstate)
{
#ifdef LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS
    setVPState(pstate);
#else
    m_pPerf->ForcePState(pstate, LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR);
#endif
}

//! \brief sets vpstate
//------------------------------------------------------------------------------
void PlatformCheck::setVPState(UINT32 vpstate)
{
    GoldenAddPrefix(__FUNCTION__);

    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS perfLimitsParams = {0};
    LW2080_CTRL_PERF_LIMIT_SET_STATUS limitSet = {0};

    perfLimitsParams.numLimits = 1;
    limitSet.limitId = LW2080_CTRL_PERF_LIMIT_FORCED;
    limitSet.input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VPSTATE;
    limitSet.input.data.vpstate.vpstate = vpstate;
    perfLimitsParams.pLimits = LW_PTR_TO_LwP64(&limitSet);

    m_rmStatus = LwRmControl(m_hClient, m_hSubDev,
                             LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
                             &perfLimitsParams,
                             sizeof(perfLimitsParams));

    if(m_rmStatus == LW_OK)
    {
        checkInt("forceVPstate", vpstate);
    }
    else
    {
        putErr("LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS", m_rmStatus);
    }

    GoldenRemovePrefix(__FUNCTION__);
}

//! \brief Test Clock values
//------------------------------------------------------------------------------
RC PlatformCheck::TestClocks()
{
    GoldenAddPrefix(__FUNCTION__);
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS rmValidDomains = {0};
    LW2080_CTRL_CLK_GET_EXTENDED_INFO_PARAMS rmExtendedClkInfo = {0};
    UINT32 bit, i;

    // Get all exposable clock domains.
    rmValidDomains.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_ALL;

    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_CLK_GET_DOMAINS,
                             &rmValidDomains,
                             sizeof(rmValidDomains));

    if (m_rmStatus != LW_OK)
    {
        putErr("LW2080_CTRL_CMD_CLK_GET_DOMAINS", m_rmStatus);
        GoldenRemovePrefix(__FUNCTION__);
        return RmApiStatusToModsRC(m_rmStatus);
    }

    rmExtendedClkInfo.numClkInfos = 0;
    for(bit = 0x1; bit != 0; bit <<= 1)
    {
        if (bit & rmValidDomains.clkDomains)
        {
            rmExtendedClkInfo.clkInfos[rmExtendedClkInfo.numClkInfos++].clkInfo.clkDomain = bit;
        }
    }

    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
                             &rmExtendedClkInfo,
                             sizeof (rmExtendedClkInfo));

    if (m_rmStatus != LW_OK)
    {
        putErr("LW2080_CTRL_CMD_CLK_GET_DOMAINS", m_rmStatus);
        GoldenRemovePrefix(__FUNCTION__);
        return RmApiStatusToModsRC(m_rmStatus);
    }

    for (i = 0; i < rmExtendedClkInfo.numClkInfos; i++)
    {
        sprintf(m_tmpChar, "%d_ClkDomainName", i);
        checkChar(m_tmpChar, m_ClkUtil.GetClkDomainName(rmExtendedClkInfo.clkInfos[i].clkInfo.clkDomain));
        sprintf(m_tmpChar, "%d_ClkSourceName", i);
        checkChar(m_tmpChar, m_ClkUtil.GetClkSourceName(rmExtendedClkInfo.clkInfos[i].clkInfo.clkSource));
        sprintf(m_tmpChar, "%d_actualFreq", i);
        checkInt(m_tmpChar, rmExtendedClkInfo.clkInfos[i].clkInfo.actualFreq);
        sprintf(m_tmpChar, "%d_targetFreq", i);
        checkInt(m_tmpChar, rmExtendedClkInfo.clkInfos[i].clkInfo.targetFreq);
        sprintf(m_tmpChar, "%d_effectiveFreq", i);
        checkInt(m_tmpChar, rmExtendedClkInfo.clkInfos[i].effectiveFreq);
    }

    GoldenRemovePrefix(__FUNCTION__);
    return RmApiStatusToModsRC(m_rmStatus);
}

//! \brief Helper for PCIE info. containes list of params to test
//------------------------------------------------------------------------------
// TOSTRING sets name to string value.
// This function is required for TestPCIEinfo()
#define TOSTRING(s) *name=#s; return s;
// must match number of cases in getPCIEInfoParam
#define NUM_PCIE_INFO_PARAMS 9
UINT32 PlatformCheck::getPCIEInfoParam(UINT32 i,  const char** name)
{
    switch (i)
    {
        // case 0 returns max value
        // This is the list of indexs for LW2080_CTRL_CMD_BUS_GET_INFO
        case 0: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CAPS);
        case 1: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CTRL_STATUS);
        case 2: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_PCIE_GEN_INFO);
        case 5: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_FLAGS);
        case 6: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_PCIE_GEN2_INFO);
        case 7: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_PCIE_ASLM_STATUS);
        case 8: TOSTRING(LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_CYA_ASPM);
        default: return NUM_PCIE_INFO_PARAMS; // must update if adding new params
    }
}

//! \brief test PCIE params found in getPCIEInfoParam
//------------------------------------------------------------------------------
void PlatformCheck::TestPCIEinfo()
{
    GoldenAddPrefix(__FUNCTION__);

    UINT32 i;
    const char* name;
    LW2080_CTRL_BUS_INFO bus_info[NUM_PCIE_INFO_PARAMS] = {{0}};
    LW2080_CTRL_BUS_GET_INFO_PARAMS bus_info_params = {0};

    for(i = 0; i<NUM_PCIE_INFO_PARAMS; i++)
    {
        bus_info[i].index = getPCIEInfoParam(i, &name);
    }

    bus_info_params.busInfoListSize = NUM_PCIE_INFO_PARAMS;
    bus_info_params.busInfoList = LW_PTR_TO_LwP64(bus_info);
    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_BUS_GET_INFO,
                             &bus_info_params,
                             sizeof(bus_info_params));

    if(m_rmStatus != LW_OK)
    {
        putErr("LW2080_CTRL_CMD_BUS_GET_INFO", m_rmStatus);
    }
    else
    {
        for(i = 0; i<NUM_PCIE_INFO_PARAMS; i++)
        {
            getPCIEInfoParam(i, &name);
            checkInt(name, bus_info[i].data);
        }
    }

    GoldenRemovePrefix(__FUNCTION__);
}

//! \brief checks PCIE counters. filtered values should always be == 0
//!         and notfiltered values should always be > 0
//------------------------------------------------------------------------------
RC PlatformCheck::TestPCIECounters()
{
    GoldenAddPrefix(__FUNCTION__);
    m_rmStatus = LW_OK;
    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS params = {0};
    UINT32 i;

    // notFiltered items will always print count
    UINT32 notFiltered =
        LW2080_CTRL_BUS_PEX_COUNTER_TYPE                         |
        LW2080_CTRL_BUS_PEX_COUNTER_RECEIVER_ERRORS              |
        LW2080_CTRL_BUS_PEX_COUNTER_REPLAY_COUNT                 |
        LW2080_CTRL_BUS_PEX_COUNTER_REPLAY_ROLLOVER_COUNT        |
        LW2080_CTRL_BUS_PEX_COUNTER_BAD_DLLP_COUNT               |
        LW2080_CTRL_BUS_PEX_COUNTER_BAD_TLP_COUNT                |
        LW2080_CTRL_BUS_PEX_COUNTER_8B10B_ERRORS_COUNT           |
        LW2080_CTRL_BUS_PEX_COUNTER_SYNC_HEADER_ERRORS_COUNT     |
        LW2080_CTRL_BUS_PEX_COUNTER_LCRC_ERRORS_COUNT            |
        LW2080_CTRL_BUS_PEX_COUNTER_FAILED_L0S_EXITS_COUNT       |
        LW2080_CTRL_BUS_PEX_COUNTER_NAKS_SENT_COUNT              |
        LW2080_CTRL_BUS_PEX_COUNTER_NAKS_RCVD_COUNT              |
        LW2080_CTRL_BUS_PEX_COUNTER_LANE_ERRORS                  |
        LW2080_CTRL_BUS_PEX_COUNTER_DEEP_L1_ENTRY_COUNT          |
        LW2080_CTRL_BUS_PEX_COUNTER_ASLM_COUNT;
    // filtered items will only print if coutner == 0
    UINT32 filtered =
        LW2080_CTRL_BUS_PEX_COUNTER_L1_TO_RECOVERY_COUNT         |
        LW2080_CTRL_BUS_PEX_COUNTER_L0_TO_RECOVERY_COUNT         |
        LW2080_CTRL_BUS_PEX_COUNTER_RECOVERY_COUNT               |
        LW2080_CTRL_BUS_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT     |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT               |
        LW2080_CTRL_BUS_PEX_COUNTER_L1P_ENTRY_COUNT              |
        LW2080_CTRL_BUS_PEX_COUNTER_TOTAL_CORR_ERROR_COUNT       |
        LW2080_CTRL_BUS_PEX_COUNTER_CORR_ERROR_COUNT             |
        LW2080_CTRL_BUS_PEX_COUNTER_NON_FATAL_ERROR_COUNT        |
        LW2080_CTRL_BUS_PEX_COUNTER_FATAL_ERROR_COUNT            |
        LW2080_CTRL_BUS_PEX_COUNTER_UNSUPP_REQ_COUNT;

    params.pexCounterMask = 0xFFFFFFFF; // get all counts

    m_rmStatus = LwRmControl(m_hClient,
                             m_hSubDev,
                             LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                             &params,
                             sizeof(params));

    if (m_rmStatus != LW_OK)
    {
        putErr("LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS", m_rmStatus);
        GoldenRemovePrefix(__FUNCTION__);
        return RmApiStatusToModsRC(m_rmStatus);
    }

    checkInt("pexCorrectableErrors", params.pexTotalCorrectableErrors);
    checkInt("pexCorrectableErrors", params.pexCorrectableErrors);
    checkInt("pexTotalNonFatalErrors", params.pexTotalNonFatalErrors);
    checkInt("pexTotalFatalErrors", params.pexTotalFatalErrors);
    checkInt("pexTotalUnsupportedReqs", params.pexTotalUnsupportedReqs);

    for (i = 0; i < LW2080_CTRL_PEX_MAX_COUNTER_TYPES; i++)
    {
        sprintf(m_tmpChar, "pexCounter_%d", i);
        if ( ((1 << i) & filtered) && (params.pexCounters[i] == 0) )
        {
            checkInt(m_tmpChar, params.pexCounters[i]);
        }
        else if ((1 << i) & notFiltered )
        {
            checkInt(m_tmpChar, params.pexCounters[i]);
        }
    }

    GoldenRemovePrefix(__FUNCTION__);
    return RmApiStatusToModsRC(m_rmStatus);
}

//! \brief checks core Voltage
//------------------------------------------------------------------------------
RC PlatformCheck::TestCoreVoltage(UINT32 PStateNum)
{
    GoldenAddPrefix(__FUNCTION__);
    Gpu::VoltageDomain Domain;
    RC rc;
    UINT32 coreVoltage = 0, i;
    UINT32 NumVolts =  m_pPerf->GetPStateVoltageDomainMask();
    rc = m_pPerf->GetCoreVoltageMv(&coreVoltage);

    if(rc != OK)
    {
        putErr("GetCoreVoltageMv", (UINT32)rc);
        GoldenRemovePrefix(__FUNCTION__);
        return rc;
    }

    checkInt("sampledCoreVoltage", coreVoltage);

    for(i = 1; NumVolts; i = i << 1)
    {
        if (!(NumVolts & i))
        {
            continue;
        }

        NumVolts &= ~i;
        Domain = PerfUtil::VoltCtrl2080BitToGpuVoltDomain(i);
        rc = m_pPerf->GetVoltageAtPState(PStateNum, Domain, &coreVoltage);

        if(rc != OK)
        {
            putErr("GetVoltageAtPState", (UINT32)rc);
            GoldenRemovePrefix(__FUNCTION__);
            return rc;

        }
        checkInt(PerfUtil::VoltDomainToStr(Domain), coreVoltage);
    }

    GoldenRemovePrefix(__FUNCTION__);
    return rc;
}

//! \brief checks pstate 2.0 data
//------------------------------------------------------------------------------
RC PlatformCheck::TestPState20Data()
{
    GoldenAddPrefix(__FUNCTION__);
    RC rc;
    const UINT32 NumClks = Utility::CountBits(m_pPerf->GetPStateClkDomainMask());
    const UINT32 NumVolts =  Utility::CountBits(m_pPerf->GetPStateVoltageDomainMask());
    UINT32 NumPStates = 0;
    m_pPerf->GetNumPStates(&NumPStates);

    checkInt("PStateClkDomainMask", NumClks);
    checkInt("PStateVoltageDomainMask", NumVolts);
    checkInt("NumPStates", NumPStates);

    Perf::PStates2Data p2d;
    rc = m_pPerf->GetPStates2Data(&p2d);
    if(rc != OK)
    {
        putErr("GetPState20Data", (UINT32)rc);
        GoldenRemovePrefix(__FUNCTION__);
        return rc;
    }

    const vector<vector<LW2080_CTRL_PERF_PSTATE20_CLK_DOM_INFO> > & Clks = p2d.Clocks;
    const vector<vector<LW2080_CTRL_PERF_PSTATE20_VOLT_DOM_INFO> > & Volts = p2d.Volts;
    const LW2080_CTRL_PERF_GET_PSTATES20_DATA_PARAMS & Params = p2d.Params;

    //Dump the tables
    checkInt("PState20DataFlags", Params.flags);
    checkInt("PState20DataNumPstates", Params.numPstates);
    checkInt("PState20DataNumVoltages", Params.numVoltages);

    UINT32 clkIdx = 0;
    UINT32 vltIdx = 0;
    UINT32 psIdx  = 0;

    for (psIdx = 0; psIdx < NumPStates; psIdx++)
    {
        sprintf(m_tmpChar, "PSTATE_INDEX_%d", psIdx);
        GoldenAddPrefix(m_tmpChar);

        checkInt("PstateID", Params.pstate[psIdx].pstateID);
        checkInt("PStateInfoFlag", Params.pstate[psIdx].flags);

        GoldenAddPrefix("ClkDomain");
        for (clkIdx = 0; clkIdx < NumClks; clkIdx++)
        {
            checkInt("Domain", Clks[psIdx][clkIdx].domain);
            checkInt("flags",  Clks[psIdx][clkIdx].flags);
            checkInt("freqDeltakHzVal", Clks[psIdx][clkIdx].freqDeltakHz.value);
            checkInt("freqDeltakHzMin", Clks[psIdx][clkIdx].freqDeltakHz.valueRange.min);
            checkInt("freqDeltakHzMax", Clks[psIdx][clkIdx].freqDeltakHz.valueRange.max);
            checkInt("type", Clks[psIdx][clkIdx].type);

            switch(Clks[psIdx][clkIdx].type)
            {
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_FIXED:
                    checkChar("CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE", "Fixed");
                    checkInt("freqkHz", Clks[psIdx][clkIdx].data.fixed.freqkHz);
                    break;
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_PSTATE:
                    checkChar("CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE", "Pstate");
                    checkInt("freqkHz", Clks[psIdx][clkIdx].data.pstate.freqkHz);
                    break;
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_DECOUPLED:
                    checkChar("CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE", "decoupled");
                    checkInt("minFreqkHz", Clks[psIdx][clkIdx].data.decoupled.minFreqkHz);
                    checkInt("maxFreqkHz", Clks[psIdx][clkIdx].data.decoupled.maxFreqkHz);
                    checkInt("voltageDomain", Clks[psIdx][clkIdx].data.decoupled.voltageDomain);
                    checkInt("minFreqVoltageuV", Clks[psIdx][clkIdx].data.decoupled.minFreqVoltageuV);
                    checkInt("maxFreqVoltageuV", Clks[psIdx][clkIdx].data.decoupled.maxFreqVoltageuV);
                    break;
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_RATIO:
                    checkChar("CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE", "ratio");
                    checkInt("minFreqkHz", Clks[psIdx][clkIdx].data.ratio.minFreqkHz);
                    checkInt("maxFreqkHz", Clks[psIdx][clkIdx].data.ratio.maxFreqkHz);
                    checkInt("voltageDomain", Clks[psIdx][clkIdx].data.ratio.voltageDomain);
                    checkInt("minFreqVoltageuV", Clks[psIdx][clkIdx].data.ratio.minFreqVoltageuV);
                    checkInt("maxFreqVoltageuV", Clks[psIdx][clkIdx].data.ratio.maxFreqVoltageuV);
                    break;
                default:
                    checkChar("CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE", "unknown");
                    break;
            }
        }
        GoldenRemovePrefix("ClkDomain");

        GoldenAddPrefix("VoltageDomains");
        for (vltIdx = 0; vltIdx < NumVolts; vltIdx++)
        {
            checkInt("Domain", Volts[psIdx][vltIdx].domain);
            checkInt("flags", Volts[psIdx][vltIdx].flags);
            checkInt("voltageDeltauVval", Volts[psIdx][vltIdx].voltageDeltauV.value);
            checkInt("voltageDeltauVmin", Volts[psIdx][vltIdx].voltageDeltauV.valueRange.min);
            checkInt("voltageDeltauVmax",  Volts[psIdx][vltIdx].voltageDeltauV.valueRange.max);
            checkInt("lwrrTragetVoltageuV", Volts[psIdx][vltIdx].lwrrTargetVoltageuV);
            checkInt("type", Volts[psIdx][vltIdx].type);

            switch(Volts[psIdx][vltIdx].type)
            {
                case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL:
            checkChar("PERF_VOLT_DOM_INFO_TYPE", "logical");
            checkInt("logicalVoltageuV", Volts[psIdx][vltIdx].data.logical.logicalVoltageuV);
                    break;
                case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT:
            checkChar("PERF_VOLT_DOM_INFO_TYPE", "vdt");
            checkInt("vdtIndex", Volts[psIdx][vltIdx].data.vdt.vdtIndex);
                    break;
                default:
            checkChar("PERF_VOLT_DOM_INFO_TYPE", "unknown");
                    break;
            }
        }
        GoldenRemovePrefix("VoltageDomains");

        sprintf(m_tmpChar, "PSTATE_INDEX_%d", psIdx);
        GoldenRemovePrefix(m_tmpChar);
    }

    GoldenRemovePrefix(__FUNCTION__);
    return rc;
}
//! \brief checks GPIO pins
//------------------------------------------------------------------------------
// API to get # of pins is verif feature
#define NUM_GPIO_PINS 32
void PlatformCheck::TestGPIOs()
{
    GoldenAddPrefix(__FUNCTION__);
    UINT32 i = 0;

    for(i = 0; i<NUM_GPIO_PINS; i++)
    {
        sprintf(m_tmpChar, "GPIO_PIN_%d", i);
        GoldenAddPrefix(m_tmpChar);

        if(m_pSubdevice->GpioIsOutput(i))
        {
            checkInt("output", m_pSubdevice->GpioGetOutput(i));
        }
        else
        {
            checkInt("input", m_pSubdevice->GpioGetInput(i));
        }
        GoldenRemovePrefix(m_tmpChar);
    }

    GoldenRemovePrefix(__FUNCTION__);
}

//! \brief prints string which was put into gold file
//------------------------------------------------------------------------------
void PlatformCheck::PrintInserted(Tee::Priority pri, string name, void* value, value_type type)
{
    Printf(pri, "[%08d] %s: ", m_fGoldID, m_goldPrefix.c_str());
    switch(type)
    {
        case VALUE_TYPE_INT:
            Printf(pri, "INT %s %x\n", name.c_str(), *(UINT32*)value);
        break;
        case VALUE_TYPE_STRING:
            Printf(pri, "STR %s %s\n", name.c_str(), (char*)value);
        break;
        case VALUE_TYPE_ERR:
            Printf(pri, "ERR %s %x\n", name.c_str(), *(UINT32*)value);
        break;
        default:
        break;
    }
}

//------------------------------------------------------------------------------
// Gold value function
//------------------------------------------------------------------------------

//! \brief Initializes all golden file state
//! \return error if something went wrong
//------------------------------------------------------------------------------
RC PlatformCheck::GoldenFileInit()
{
    //GoldValues internal varibles
    string m_goldFilePath = "";
    char const* elwResult = getelw("PLATFORMCHECK_GOLD_PATH");
    m_fpGoldFile = NULL;
    m_fGoldID = 0;
    m_goldPrefix = "";
    isError = false;
    isGoldCapturing = false;
    isGoldInit = false;
    //string platformId = "";
    struct stat buffer;

    LwU32 PCIDeviceID = 0;
    LwU32 PCISubDeviceID = 0;
    m_rmStatus = LwRmConfigGet( m_hClient, m_hSubDev, LW_CFG_PCI_ID, &PCIDeviceID);
    if (m_rmStatus != LW_OK)
    {
        Printf(Tee::PriHigh, "%s: getting DeviceID failed!!|\
                              LW_CFG_PCI_ID error 0x%08X",
                              __FUNCTION__, m_rmStatus);
        return RC::LWRM_ERROR;
    }
    m_rmStatus = LwRmConfigGet( m_hClient, m_hSubDev, LW_CFG_PCI_SUB_ID, &PCISubDeviceID);
    if (m_rmStatus != LW_OK)
    {
        Printf(Tee::PriHigh, "%s: getting SubDevice ID failed!!|\
                              LW_CFG_PCI_SUB_ID error 0x%08X",
                              __FUNCTION__, m_rmStatus);
        return RC::LWRM_ERROR;
    }

    sprintf(m_tmpChar, "Device_0x%08X_SubDevice_0x%08X",
                        (unsigned int)PCIDeviceID,
                        (unsigned int)PCISubDeviceID);

    // If the gold path environment variable was valid
    if (elwResult == NULL)
    {
        m_goldFilePath.assign(".");
    }
    else
    {
        m_goldFilePath.assign(elwResult);
    }

#ifdef LW_WINDOWS
    m_goldFilePath.append("\\platformcheck_gold_values_");
#else
    m_goldFilePath.append("/platformcheck_gold_values_");
#endif

    m_goldFilePath.append(m_tmpChar);
    m_goldFilePath.append(".txt");
    // Check if file exist
    if(stat(m_goldFilePath.c_str(), &buffer) == 0)
    {
        Printf(Tee::PriHigh, "detected gold file using gold at %s\n", m_goldFilePath.c_str());
        isGoldCapturing = false;
        m_fpGoldFile = fopen(m_goldFilePath.c_str(), "r");
    }
    else
    {
        m_goldFilePath.append(".new");
        Printf(Tee::PriHigh, "no gold found, capturing at %s\n", m_goldFilePath.c_str());
        isGoldCapturing = true;
        m_fpGoldFile = fopen(m_goldFilePath.c_str(), "w");
    }

    if(m_fpGoldFile == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s\n", m_goldFilePath.c_str());
        isGoldCapturing = false;
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    // get header info
    // print header
    if(isGoldCapturing)
    {
        fprintf(m_fpGoldFile, "Gold build date info: Device_0x%08X_SubDevice_0x%08X\n",
                        (unsigned int)PCIDeviceID,
                        (unsigned int)PCISubDeviceID);
    }
    else
    {
        if (1 != fscanf(m_fpGoldFile, "Gold build date info: %s", m_tmpChar))
        {
            Printf(Tee::PriHigh, "Unable to read build date from %s\n", m_goldFilePath.c_str());
            return RC::FILE_READ_ERROR;
        }
        Printf(Tee::PriHigh, "using Gold for %s\n", m_tmpChar);
    }

    isGoldInit = true;
    return OK;
}

//! \brief adds or checks an integer to goldFile
//------------------------------------------------------------------------------
void PlatformCheck::checkInt(string name, UINT32 value)
{
    if(!isGoldInit)
    {
        Printf(Tee::PriHigh, "Call goldenFileInit before running this!!\n");
        exit(1);
    }
    // Check if we are comparing golds or creating gold
    if(isGoldCapturing)
    {
       GoldenFileInsert(name, &value, VALUE_TYPE_INT);
    }
    else // or we are comparing golds
    {
       GoldenFileCompare(name, &value, VALUE_TYPE_INT);
    }
}

//! \brief adds or checks a string to goldFile
//------------------------------------------------------------------------------
void PlatformCheck::checkChar(string name, string value)
{
    if(!isGoldInit)
    {
        Printf(Tee::PriHigh, "Call goldenFileInit before running this!!\n");
        exit(1);
    }

    // Check if we are comparing golds or creating gold
    strcpy(m_tmpChar,  value.c_str());
    if(isGoldCapturing)
    {
       GoldenFileInsert(name, m_tmpChar, VALUE_TYPE_STRING);
    }
    else // or we are comparing golds
    {
       GoldenFileCompare(name, m_tmpChar, VALUE_TYPE_STRING);
    }
}

//! \brief adds or checks an error to goldFile
//------------------------------------------------------------------------------
void PlatformCheck::putErr(string name, UINT32 value)
{
    if(!isGoldInit)
    {
        Printf(Tee::PriHigh, "Call goldenFileInit before running this!!\n");
        exit(1);
    }

    // Check if we are comparing golds or creating gold
    if(isGoldCapturing)
    {
       GoldenFileInsert(name, &value, VALUE_TYPE_ERR);
    }
    else // or we are comparing golds
    {
       GoldenFileCompare(name, &value, VALUE_TYPE_ERR);
    }
}

//! \brief does actual inserting into goldFile
//------------------------------------------------------------------------------
void PlatformCheck::GoldenFileInsert(string name, void* value, value_type type)
{
    fprintf(m_fpGoldFile, "%d %s ", m_fGoldID, m_goldPrefix.c_str());
    switch(type)
    {
        case VALUE_TYPE_INT:
            fprintf(m_fpGoldFile, "INT %s %d\n", name.c_str(), *(UINT32*)value);
        break;
        case VALUE_TYPE_STRING:
            fprintf(m_fpGoldFile, "STR %s %s\n", name.c_str(), (char*)value);
        break;
        case VALUE_TYPE_ERR:
            fprintf(m_fpGoldFile, "ERR %s %d\n", name.c_str(), *(UINT32*)value);
        break;
        default:
        break;
    }

    PrintInserted(Tee::PriHigh, name, value, type);
    m_fGoldID++;
}
//! \brief does actual comparing of goldFile and current value
//------------------------------------------------------------------------------
void PlatformCheck::GoldenFileCompare(string name, void* value, value_type type)
{
    char fPrefix[MAX_PREFIX_LENGTH] = {0};
    char fType[MAX_TYPE_LENGTH] = {0};
    char fStringValue[MAX_VALUE_LENGTH] = {0};
    char fName[MAX_NAME_LENGTH] = {0};

    UINT32 fIntValue = 0;
    UINT32 fGoldID = 0;

    // first check and ensure we are comparing same entry
    if (1 != fscanf(m_fpGoldFile, "%d ", &fGoldID) || fGoldID != m_fGoldID)
    {
        // file corrupted or modified. unrecovarable
        setError("FATAL: ID. gold file might be corrupted or modified");
        Printf(Tee::PriHigh, "GoldID from file: %d GoldID from runtime %d\n", fIntValue, m_fGoldID);
    }

    // Check if we are comparing same test
    if (1 != fscanf(m_fpGoldFile, "%s ", fPrefix) || m_goldPrefix.compare(fPrefix) != 0)
    {
        setError("FATAL: prefix. test being compared are different");
        Printf(Tee::PriHigh, "file prefix: %s runtime prefix: %s\n", fPrefix, m_goldPrefix.c_str());
    }

    if (1 != fscanf(m_fpGoldFile, "%s ", fType))
    {
        setError("FATAL: unable to read type");
        Printf(Tee::PriHigh, "unable to read type\n");
    }

    // Check if we are comparing same fields
    if (1 != fscanf(m_fpGoldFile, "%s ", fName) || name.compare(fName) != 0)
    {
        setError("FATAL: valueName. field being compared is different");
        Printf(Tee::PriHigh, "file valueName: %s runtime valueName: %s\n", fName, name.c_str());
    }

    Printf(Tee::PriDebug, "decoded: %d %s %s %s ", fGoldID, fPrefix, fType, fName);
    int numRead = 0;
    switch(type)
    {
        case VALUE_TYPE_INT:
            numRead = fscanf(m_fpGoldFile, "%d\n", &fIntValue);
            Printf(Tee::PriDebug, "%x\n", fIntValue);
            if(numRead != 1 || fIntValue != *(UINT32*)value)
            {
                setError("FATAL: IntValue. value being compared is different");
                Printf(Tee::PriHigh, "file IntValue: %d runtime IntValue: %d\n", fIntValue, *(UINT32*)value);
            }
        break;
        case VALUE_TYPE_STRING:
            numRead = fscanf(m_fpGoldFile, "%s\n", fStringValue);
            Printf(Tee::PriDebug, "%s\n", fStringValue );
            if(numRead != 1 || strcmp(fStringValue, (char*)value) != 0)
            {
                setError("FATAL: StringValue. field being compared is different");
                Printf(Tee::PriHigh, "file StringValue: %s runtime StringValue: %s\n", fStringValue, (char*)value);
            }
        break;
        case VALUE_TYPE_ERR:
            numRead = fscanf(m_fpGoldFile, "%d\n", &fIntValue);
            Printf(Tee::PriDebug, "%x\n", fIntValue);
            if(numRead != 1 || fIntValue != *(UINT32*)value)
            {
                setError("FATAL: IntValue. value being compared is different");
                Printf(Tee::PriHigh, "file IntValue: %d runtime IntValue: %d\n", fIntValue, *(UINT32*)value);
            }
        break;
        default:
        break;
    }

    // print line which caused mismatch on failure
    if(isError)
    {
        Printf(Tee::PriHigh, "mismatch line: %d %s %s %s ", fGoldID, fPrefix, fType, fName);
    }

    m_fGoldID++;
}

//! \brief appends string to prefix
//------------------------------------------------------------------------------
void PlatformCheck::GoldenAddPrefix(string value)
{
    value.append("_");
    m_goldPrefix.append(value);
}

//! \brief removes string from prefix
//------------------------------------------------------------------------------
void PlatformCheck::GoldenRemovePrefix(string value)
{
    value.append("_");
    m_goldPrefix.erase(m_goldPrefix.find(value), value.length());
}

//! \brief sets error flag to true. will cause test to fail if this is true
//------------------------------------------------------------------------------
void PlatformCheck::setError(string err)
{
    isError = true;
    Printf(Tee::PriHigh, "ERROR MISMATCH %s\n", err.c_str());
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PlatformCheck, RmTest, "Platform Check");
