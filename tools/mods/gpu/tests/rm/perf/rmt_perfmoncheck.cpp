/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2013,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_perfmoncheck.cpp
//! \brief perfmoncheck functionality test
//!
//! This file is used to verify if Perf engine able to raise clock to P0
//! when 3D application is running. Now we only support Win32 test
//! environment.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/rmclkutil.h"
#include "core/include/version.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "lwos.h"
#include "g98/dev_disp.h"
#include "core/include/platform.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/display.h"
#include "core/include/utility.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/jscript.h"
#include "core/include/cnslmgr.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0073/ctrl0073system.h"
//!-----Header for LWAPI interface
#include <lwapi.h>
#include <string> // Only use <> for built-in C++ stuff
#include <stdlib.h>
#include "core/include/xp.h"
#include <math.h>
#include "core/include/memcheck.h"

#define OGLAPP_PATH         "balls.exe"

typedef struct
{
    LwU32   flags;
    LwU32   numPstates;
    LwU32   numClocks;
    LwU32   numVoltages;
    LwU32   maxPerfLevel, minPerfLevel;

    struct
    {
            LwU32   pstate;
            LwU32   flags;
            LW2080_CTRL_PERF_CLK_DOM_INFO clocks[LWAPI_MAX_GPU_PERF_CLOCKS];
            LW2080_CTRL_PERF_VOLT_DOM_INFO voltages[LWAPI_MAX_GPU_PERF_VOLTAGES];
            LwU32   bitIndex;
    } pstates[LWAPI_MAX_GPU_PERF_PSTATES];
} LW_GPU_PSTATES_INFO;

typedef struct
{
    LwU32   domains;
    LwU32   flags;
    LwU32   numClkInfos;
    LW2080_CTRL_CLK_EXTENDED_INFO clkInfos[LW2080_CTRL_CLK_MAX_EXTENDED_INFOS];
} LW_GPU_CLOCKS_INFO;

typedef struct
{
    LW_GPU_CLOCKS_INFO   clkInfo_Lwrrent, clkInfo_Saved;
    LW_GPU_PSTATES_INFO  pStatesInfo;
    LwRm::Handle hSubDev;
}SUBDEVICE_INFO;

//PerfMonCheckTest(PMCT) information
typedef struct
{
    LwRm::Handle hDev;
    LwU32 subDevCount;
    SUBDEVICE_INFO subDevInfo[LW2080_MAX_SUBDEVICES];
}PMCT_INFO;

#if LWOS_IS_WINDOWS
#include <windows.h>
// Declare Callback Enum Functions.
extern BOOL CALLBACK TerminateAppEnum (HWND hwnd, LPARAM lParam);
#endif

#define LWCTRL_DEVICE_HANDLE                (0xbb008000)
#define LWCTRL_SUBDEVICE_HANDLE             (0xbb208000)
static const double CLK_TOLERANCE = 0.05;

class PerfMonCheckTest : public RmTest
{
public:
    PerfMonCheckTest();
    virtual ~PerfMonCheckTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle m_hDevice;
    LwU32 m_devIds;
    LwU32 m_lwrrentDev, m_lwrrentSubDev;
    RmtClockUtil m_ClkUtil;          // Instantiate Clk Util
    LwRm::Handle m_hClient;
    PMCT_INFO m_Info[LW0080_MAX_DEVICES];

    RC GetAllClocksInfo(LW_GPU_CLOCKS_INFO *pClksInfo);
    RC GetPstatesInfo();
    RC SetForcePstate(LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS *pParms);
    RC CompareClocksInfo(LW_GPU_CLOCKS_INFO *pClocksSaved, LW_GPU_CLOCKS_INFO *pClocksLwrrent);
    RC SliStatusCheck();

#if LWOS_IS_WINDOWS
    PROCESS_INFORMATION m_procInfoOglApp;
    STARTUPINFO m_stInfoOglApp;
    BOOL LaunchAppProcess(string procCmdLine,STARTUPINFO& startInfo,
                          PROCESS_INFORMATION& procInfo);
    void ExitAppProcess(PROCESS_INFORMATION& procInfo);
#endif

};

//! \brief Constructor for PerfMonCheckTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
PerfMonCheckTest::PerfMonCheckTest()
{
    SetName("PerfMonCheckTest");

    m_devIds = 0;
    m_hClient = 0;
    m_lwrrentDev = 0;
    m_lwrrentSubDev = 0;
    m_hDevice = LWCTRL_DEVICE_HANDLE;
    memset(&m_Info, 0, sizeof(m_Info));
#if LWOS_IS_WINDOWS
    memset(&m_procInfoOglApp, 0, sizeof(m_procInfoOglApp));
    memset(&m_stInfoOglApp, 0, sizeof(m_stInfoOglApp));
#endif
}

//! \brief Destructor for PerfMonCheckTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PerfMonCheckTest::~PerfMonCheckTest()
{

}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports PerfMonCheckTest.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string PerfMonCheckTest::IsTestSupported()
{
    string info = g_BuildOS;
    LwRmPtr pLwRm;

    if (info == "win32")
    {
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            // debug
            Printf(Tee::PriHigh,"PerfMon is supported only on HW, %d\n",
                   __LINE__);
            return "Supported only on HW";
        }

        return RUN_RMTEST_TRUE;
    }

    Printf(Tee::PriHigh,
          "PerfMonCheckTest is only supported on Windows, OS = %s \n", info.c_str());

    return "Supported only on win32";
}

//! \brief Setup
//!
//! Just JS settings
//
//! \return RC -> OK if everything is allocated,
//------------------------------------------------------------------------------
RC PerfMonCheckTest::Setup()
{
    LwU32 status, i, j;
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW2080_ALLOC_PARAMETERS lw2080Params = {0};
    LW0000_CTRL_GPU_GET_DEVICE_IDS_PARAMS deviceIdsParams = {0};
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};

    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    status = (LwU32)LwRmAllocRoot((LwU32*)&m_hClient);
    rc = RmApiStatusToModsRC(status);
    CHECK_RC(rc);

    rc = SliStatusCheck();

    CHECK_RC(rc);

    // get valid device instances mask
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_DEVICE_IDS,
                         &deviceIdsParams, sizeof (deviceIdsParams));

    if (status != LW_OK)
    {
            Printf(Tee::PriHigh,
                   "CtrlDispTest: GET_DEVICE_IDS failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
    }

    m_devIds = deviceIdsParams.deviceIds;

    Printf(Tee::PriHigh,
           "Total deviceIds: 0x%x\n", m_devIds);

    // for each device...
    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((deviceIdsParams.deviceIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        // allocate device
        lw0080Params.deviceId = i;
        lw0080Params.hClientShare = 0;
        m_Info[i].hDev = m_hDevice + i;
        status = LwRmAlloc(m_hClient, m_hClient, m_Info[i].hDev,
                           LW01_DEVICE_0,
                           &lw0080Params);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "CtrlDispTest: LwRmAlloc(LW01_DEVICE): id=%d failed!\n", i);
            return RmApiStatusToModsRC(status);
        }

        // get number of subdevices
        status = LwRmControl(m_hClient, m_Info[i].hDev,
                             LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                             &numSubDevicesParams,
                             sizeof (numSubDevicesParams));

        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "CtrlDispTest: subdevice cnt cmd failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        m_Info[i].subDevCount = numSubDevicesParams.numSubDevices;

        Printf(Tee::PriHigh, "Dev = %d, subDevCount = %d \n", i, m_Info[i].subDevCount);

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
            // allocate subdevice
            lw2080Params.subDeviceId = j;
            m_Info[i].subDevInfo[j].hSubDev = LWCTRL_SUBDEVICE_HANDLE + j;

            status = LwRmAlloc(m_hClient, m_Info[i].hDev, m_Info[i].subDevInfo[j].hSubDev,
                               LW20_SUBDEVICE_0,
                               &lw2080Params);

            if (status != LW_OK)
            {
                Printf(Tee::PriHigh,
                       "LwRmAlloc(SubDevice) failed: status 0x%x, gpu = %d, %d\n",
                        status, i, __LINE__);
                rc = RmApiStatusToModsRC(status);
                CHECK_RC(rc);
            }

            m_lwrrentSubDev = j;

            // Get p-state info for each subDevice
            CHECK_RC(GetPstatesInfo());
            CHECK_RC(m_ClkUtil.ModsEnDisAble(m_hClient,m_Info[i].subDevInfo[j].hSubDev,true));
        }
    }

    return rc;
}

//! \brief Runs each PerfMonCheck test
//!
//! Central point where all tests are run.
//!
//! \return Always OK since we have only one test and it's a visual test.
//------------------------------------------------------------------------------
RC PerfMonCheckTest::Run()
{
    RC rc;
    LwU32  i, j, maxLevel, minLevel;
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS setPerf = {0};
    string cmdLine = "";

    // Flush all the queued keyboard input.
    Utility::FlushInputBuffer();

    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
             m_lwrrentSubDev = j;
             maxLevel = m_Info[i].subDevInfo[j].pStatesInfo.maxPerfLevel;
             setPerf.forcePstate = m_Info[i].subDevInfo[j].pStatesInfo.pstates[maxLevel].pstate;
             SetForcePstate(&setPerf);
             // wait clock stable, delay 50000 microSocend
             Utility::Delay(50000);

             // get GPU clocks
             GetAllClocksInfo(&m_Info[i].subDevInfo[j].clkInfo_Saved);
        }
    }

    // clear suspend p-state
    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
           m_lwrrentSubDev = j;
           //Set to highest perf level (usually is P0)
           memset(&setPerf, 0,
                  sizeof(LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS));
           SetForcePstate(&setPerf);
        }
    }

    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
            m_lwrrentSubDev = j;
            minLevel = m_Info[i].subDevInfo[j].pStatesInfo.minPerfLevel;
            setPerf.forcePstate = m_Info[i].subDevInfo[j].pStatesInfo.pstates[minLevel].pstate;
            SetForcePstate(&setPerf);

            // wait clock stable, delay 50000 microSocend
            Utility::Delay(50000);
        }
    }

    // clear suspend p-state
    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
           m_lwrrentSubDev = j;
           //Set to highest perf level (usually is P0)
           memset(&setPerf, 0,
                  sizeof(LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS));
           SetForcePstate(&setPerf);
        }
    }

#if LWOS_IS_WINDOWS

    m_stInfoOglApp.cb = sizeof(m_stInfoOglApp);
    m_stInfoOglApp.dwFlags = STARTF_USESTDHANDLES;

    cmdLine = OGLAPP_PATH;

    // run 3D application
    if(!LaunchAppProcess(cmdLine,m_stInfoOglApp,m_procInfoOglApp))
    {
        Printf(Tee::PriHigh, "ERROR Cannot Launch OGL Application\n");
        return RC::SOFTWARE_ERROR;
    }

    // wait clock stable, delay 2 seconds
    Utility::Delay(2000000);

#endif

    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
             m_lwrrentSubDev = j;
             // get GPU clocks again
             GetAllClocksInfo(&m_Info[i].subDevInfo[j].clkInfo_Lwrrent);
        }
    }

#if LWOS_IS_WINDOWS

    // exit 3D application
    ExitAppProcess(m_procInfoOglApp);

#endif

    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount;j ++)
        {
            m_lwrrentSubDev = j;
            // compare clock
            CHECK_RC(CompareClocksInfo(&m_Info[i].subDevInfo[j].clkInfo_Saved,
                                       &m_Info[i].subDevInfo[j].clkInfo_Lwrrent));
        }
    }

    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC PerfMonCheckTest::Cleanup()
{
    LwU32 i, j;
    RC rc;

    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((m_devIds & (1 << i)) == 0)
        {
            continue;
        }

        m_lwrrentDev = i;

        for (j = 0; j < m_Info[i].subDevCount; j++)
        {
            CHECK_RC(m_ClkUtil.ModsEnDisAble(m_hClient, m_Info[i].subDevInfo[j].hSubDev, false));
            // free handles
            LwRmFree(m_hClient, m_Info[i].hDev,  m_Info[i].subDevInfo[j].hSubDev);
        }
        LwRmFree(m_hClient, m_hClient, m_Info[i].hDev);
   }

   LwRmFree(m_hClient, m_hClient, m_hClient);

   return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
RC PerfMonCheckTest::GetPstatesInfo()
{
    RC rc;
    LwU32 i, j, status, maxPstate, minPstate;
    LW_GPU_PSTATES_INFO *pStatesInfo = NULL;

    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS pstatesInfo = {0};
    LW2080_CTRL_PERF_GET_PSTATE_INFO_PARAMS  pstateInfo = {0};
    LwRm::Handle m_hSubDev;

    m_hSubDev =  m_Info[m_lwrrentDev].subDevInfo[m_lwrrentSubDev].hSubDev;
    pStatesInfo = &m_Info[m_lwrrentDev].subDevInfo[m_lwrrentSubDev].pStatesInfo;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                         &pstatesInfo,
                         sizeof(pstatesInfo));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO failed  0x%x, %d\n",
               status, __LINE__);
        rc = RmApiStatusToModsRC(status);
        return rc;
    }

    pStatesInfo->flags = pstatesInfo.flags;

    //
    // Get supported clock domains
    //
    pStatesInfo->numClocks = 0;

    for (j = 0; j < LWAPI_MAX_GPU_PERF_CLOCKS; j++)
    {
        if ((1 << j) & pstatesInfo.perfClkDomains)
        {
            for ( i = 0; i < LWAPI_MAX_GPU_PERF_PSTATES; i++ )
            {
                pStatesInfo->pstates[i].clocks[pStatesInfo->numClocks].domain = (1 << j);
            }
            pStatesInfo->numClocks++;
        }
    }

    Printf(Tee::PriHigh, "numClocks = %x, \n", pStatesInfo->numClocks);

    pStatesInfo->numVoltages = 0;

    for (j = 0; j < LWAPI_MAX_GPU_PERF_VOLTAGES; j++)
    {
        if ((1 << j) & pstatesInfo.perfVoltageDomains)
        {
            for (i = 0; i < LWAPI_MAX_GPU_PERF_PSTATES; i++)
            {
                pStatesInfo->pstates[i].voltages[pStatesInfo->numVoltages].domain = (1 << j);
            }
            pStatesInfo->numVoltages++;
        }
    }

    Printf(Tee::PriHigh, "numVoltages = %x, \n", pStatesInfo->numVoltages);

    pStatesInfo->numPstates = 0;
    for(i = 0; i < LWAPI_MAX_GPU_PERF_PSTATES; i++)
    {
        if ((1 << i) & pstatesInfo.pstates)
        {
            pStatesInfo->pstates[pStatesInfo->numPstates].pstate = (1 << i);
            pStatesInfo->pstates[pStatesInfo->numPstates].bitIndex = i;
            pStatesInfo->numPstates++;
        }
    }

    Printf(Tee::PriHigh, "numPstates = %x, \n", pStatesInfo->numPstates);

    //
    // Call LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO
    //
    for(i = 0; i < pStatesInfo->numPstates; i++)
    {
        pstateInfo.pstate                  = pStatesInfo->pstates[i].pstate;
        pstateInfo.perfClkDomInfoListSize  = pStatesInfo->numClocks;
        pstateInfo.perfClkDomInfoList      = (LwP64)&pStatesInfo->pstates[i].clocks;
        pstateInfo.perfVoltDomInfoListSize = pStatesInfo->numVoltages;
        pstateInfo.perfVoltDomInfoList     = (LwP64)&pStatesInfo->pstates[i].voltages;

        status = LwRmControl(m_hClient, m_hSubDev,
                             LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO,
                             &pstateInfo,
                             sizeof (pstateInfo));

        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO failed, pstate = %d,  0x%x, %d\n",
                   i, status, __LINE__);
            rc = RmApiStatusToModsRC(status);
            return rc;
        }

        pStatesInfo->pstates[i].flags = pstateInfo.flags;
    }

    maxPstate = 0xFFFFFFFF;

    for (i = 0; i < pStatesInfo->numPstates; i++)
    {
      if (pStatesInfo->pstates[i].pstate < maxPstate)
      {
          maxPstate = pStatesInfo->pstates[i].pstate;
          pStatesInfo->maxPerfLevel = i;
      }
    }

    minPstate = 0;

    for (i = 0; i < pStatesInfo->numPstates; i++)
    {
      if (pStatesInfo->pstates[i].pstate > minPstate)
      {
          minPstate = pStatesInfo->pstates[i].pstate;
          pStatesInfo->minPerfLevel = i;
      }
    }

    Printf(Tee::PriHigh,
           "MAX perf Level = %x, maxPstate = %x.  MIN perf Level = %x, minPstate = %x\n",
            pStatesInfo->maxPerfLevel, pStatesInfo->pstates[pStatesInfo->maxPerfLevel].pstate,
            pStatesInfo->minPerfLevel, pStatesInfo->pstates[pStatesInfo->minPerfLevel].pstate);

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief SetForcePstate()
//!
//! Suspend/resume pstate
//! perfctrl.c lw2080CtrlCmdPerfSetForcePstateEx()
//------------------------------------------------------------------------------

RC PerfMonCheckTest::SetForcePstate(LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS *pParms)
{
    LwU32 status;
    RC rc;
    LwRm::Handle m_hSubDev;

    m_hSubDev =  m_Info[m_lwrrentDev].subDevInfo[m_lwrrentSubDev].hSubDev;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE_EX,
                         pParms,
                         sizeof(LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE_EX failed, pstate = %d,  0x%x, %d\n",
               pParms->forcePstate, status, __LINE__);
        rc = RmApiStatusToModsRC(status);
        return rc;
    }

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief GetAllClocksInfo()
//!
//! get lwurrent all clocks info from RM.
//!
//------------------------------------------------------------------------------

RC PerfMonCheckTest::GetAllClocksInfo(LW_GPU_CLOCKS_INFO *pClksInfo)
{
    RC rc;
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS rmValidDomains = {0};
    LW2080_CTRL_CLK_GET_EXTENDED_INFO_PARAMS rmExtendedClkInfo = {0};
    LwU32 i, bit, status;

    LwRm::Handle m_hSubDev;

    m_hSubDev =  m_Info[m_lwrrentDev].subDevInfo[m_lwrrentSubDev].hSubDev;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_CLK_GET_DOMAINS,
                         &rmValidDomains,
                         sizeof (rmValidDomains));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LW2080_CTRL_CMD_CLK_GET_DOMAINS failed, lwrrentDev = %d, lwrrentSubDev = %d, status = 0x%x, %d\n",
               m_lwrrentDev, m_lwrrentSubDev, status, __LINE__);
        rc = RmApiStatusToModsRC(status);
        return rc;
    }

    pClksInfo->domains = rmValidDomains.clkDomains;

    pClksInfo->numClkInfos = 0;

    for(bit=0x1; bit!=0; bit<<=1)
    {
        if (bit & rmValidDomains.clkDomains)
        {
          rmExtendedClkInfo.clkInfos[pClksInfo->numClkInfos].clkInfo.clkDomain = bit;
          pClksInfo->clkInfos[pClksInfo->numClkInfos++].clkInfo.clkDomain = bit;
        }
    }

   // set clock info number
    rmExtendedClkInfo.numClkInfos = pClksInfo->numClkInfos;

    status = LwRmControl(m_hClient, m_hSubDev,
                         LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
                         &rmExtendedClkInfo,
                         sizeof (rmExtendedClkInfo));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO failed, lwrrentDev = %d, lwrrentSubDev = %d, status = 0x%x, %d\n",
               m_lwrrentDev, m_lwrrentSubDev, status, __LINE__);
        rc = RmApiStatusToModsRC(status);
        return rc;
    }

    for (i = 0; i < rmExtendedClkInfo.numClkInfos; i++)
    {
        pClksInfo->clkInfos[i] = rmExtendedClkInfo.clkInfos[i];
    }

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief CompareClocksInfo(LW_GPU_CLOCKS_INFO *pClksSource, LW_GPU_CLOCKS_INFO *pClksTarget)
//!
//! Compare clocks.
//!
//------------------------------------------------------------------------------

RC PerfMonCheckTest::CompareClocksInfo(LW_GPU_CLOCKS_INFO *pClocksSaved, LW_GPU_CLOCKS_INFO *pClocksLwrrent)
{
    LwU32 i, numClkInfos;
    bool status = TRUE;
    double tolerance = 0.0;

    if (pClocksSaved->numClkInfos != pClocksLwrrent->numClkInfos)
    {
        Printf(Tee::PriHigh,
               "CompareClocksInfo(), pClocksSaved->numClkInfos != pClocksLwrrent->numClkInfos, %d\n",
               __LINE__);
        return RC::SOFTWARE_ERROR;
    }

    numClkInfos = pClocksSaved->numClkInfos;

    for (i = 0; i < numClkInfos; i++ )
    {
      if( pClocksSaved->clkInfos[i].effectiveFreq != pClocksLwrrent->clkInfos[i].effectiveFreq)
      {
          // Check if difference can be accepted
          tolerance = (pClocksSaved->clkInfos[i].effectiveFreq - pClocksLwrrent->clkInfos[i].effectiveFreq);
          tolerance /= pClocksSaved->clkInfos[i].effectiveFreq;

          // get abslolute difference value
          tolerance = fabs(tolerance);

          if (tolerance > CLK_TOLERANCE)
          {
              Printf(Tee::PriHigh,
                     "CompareClocksInfo() failed, clkIndex=%d, %d\n",
                     i, __LINE__);

              Printf(Tee::PriHigh,
                     "pClocksSaved = %x, pClocksLwrrent = %x\n",
                     pClocksSaved->clkInfos[i].effectiveFreq, pClocksLwrrent->clkInfos[i].effectiveFreq);

              status = FALSE;
          }
      }
      else
      {
          Printf(Tee::PriHigh,
                 "Dev = %d, SubDev = %d: Test Pass -- clock(%d), Freq = %x\n",
                 m_lwrrentDev, m_lwrrentSubDev, i, pClocksSaved->clkInfos[i].effectiveFreq);
      }
    }

    return (status ? OK : RC::SOFTWARE_ERROR);
}

#if LWOS_IS_WINDOWS
//! \brief LaunchAppProcess
//! This Function creates the windows process
//------------------------------------------------------------------------------
BOOL PerfMonCheckTest::LaunchAppProcess(string procCmdLine,
                                 STARTUPINFO& startInfo,
                                 PROCESS_INFORMATION& procInfo)
{
    LwU32 dwExitCode = 0;

    if (CreateProcess(NULL,
        (LPSTR)procCmdLine.c_str(),
        NULL, NULL, FALSE, 0, NULL, NULL, &startInfo, &procInfo))
    {
        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
        return TRUE;
    }

    dwExitCode = WaitForSingleObject(procInfo.hProcess, (20 * 1000));

    return FALSE;
}

//! \brief ExitAppProcess
//! This Function Exits the Windows Process
//------------------------------------------------------------------------------
void PerfMonCheckTest::ExitAppProcess(PROCESS_INFORMATION& procInfo)
{
    DWORD   dwRet ;
    LwU32  dwTimeout = 10000;

    HANDLE  hProc  = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE,
                                 procInfo.dwProcessId);

    if(hProc == NULL)
    {
        return;
    }

    // TerminateAppEnum() posts WM_CLOSE to all windows whose PID
    // matches your process's.
    EnumWindows ((WNDENUMPROC)TerminateAppEnum, (LPARAM) procInfo.dwProcessId) ;

    // Wait on the handle. If it signals, great. If it times out,
    // then you kill it.

    if (WaitForSingleObject(hProc, dwTimeout)!=WAIT_OBJECT_0)
        dwRet= TerminateProcess(hProc,0);

    CloseHandle(hProc) ;

    return;
}

//! \brief ExitAppProcess
//! This Function Terminates the Windows APP
//------------------------------------------------------------------------------
BOOL CALLBACK TerminateAppEnum( HWND hwnd, LPARAM lParam )
{
    DWORD dwID ;

    GetWindowThreadProcessId(hwnd, &dwID) ;

    if(dwID == (DWORD)lParam)
    {
        PostMessage(hwnd, WM_CLOSE, 0, 0) ;
    }
    return TRUE ;
}
#endif

//! \brief SLI capable and enable check
//!
//! Just JS settings
//
//! \return RC -> OK if everything is allocated,
//------------------------------------------------------------------------------
RC PerfMonCheckTest::SliStatusCheck()
{
    LwRmPtr pLwRm;
    LW0000_CTRL_GPU_GET_VALID_SLI_CONFIGS_PARAMS validSliTopos = {0};
    RC rc = OK;
    LwU32 status;

    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_VALID_SLI_CONFIGS,
                         &validSliTopos, sizeof(validSliTopos));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LW0000_CTRL_CMD_GPU_GET_VALID_SLI_CONFIGS, %d\n",
                __LINE__);
        rc = RmApiStatusToModsRC(status);
        return rc;
    }

    Printf(Tee::PriHigh,
           "validSliTopos.sliConfigCount = %d, validSliTopos.sliStatus = %d\n",
            validSliTopos.sliConfigCount, validSliTopos.sliStatus);

    if (validSliTopos.sliConfigCount != 0) //there are SLI capable GPUs in the system
    {
        LW0000_CTRL_GPU_GET_ID_INFO_V2_PARAMS gpuInfoParams = { };

        gpuInfoParams.gpuId = GetBoundGpuSubdevice()->GetGpuId();

        status = LwRmControl(m_hClient, m_hClient, LW0000_CTRL_CMD_GPU_GET_ID_INFO_V2, &gpuInfoParams, sizeof(gpuInfoParams));

        if ((status != LW_OK) ||
            !FLD_TEST_DRF(0000, _CTRL_GPU_ID_INFO, _LINKED_INTO_SLI_DEVICE, _TRUE, gpuInfoParams.gpuFlags))
        {
           Printf(Tee::PriHigh, "Please enable SLI to test \n" );
           rc = RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PerfMonCheckTest, RmTest,
    "PerfMonCheck 1.0 test.\
     This test will set system to P0 and save clocks value.\
     Then we set system to P12.\
     Then we run 3D application(balls.exe) and then save clocks values again.\
     We compare these 2 clocks values to see if perf engine can raise clock \
     to correct value or not. \n");
