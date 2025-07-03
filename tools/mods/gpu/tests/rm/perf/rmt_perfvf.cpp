/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file PerfVF.cpp
//! \brief RM test for verifying perf VF APIs..
//!
//! This test queries the VF table for each clk domain supported.
//! It then cycles through each frequency point, caps to it.
//! It then clears the cap to restore the frequency.
//!
//! TODO:
//! 1. Add voltage change requests
//! 2. Use clock counters to verify if the frequency is actually set
//!    for silicon runs.
//! 3. Use RM-API to query the acceptable frequency tolerance instead of
//!    using the hardcoded value.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/lwrm.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include <cmath>
#include "class/cl0000.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "core/include/memcheck.h"

#define LWCTRL_DEVICE_HANDLE                (0xbb008001)
#define LWCTRL_SUBDEVICE_HANDLE             (0xbb208001)

// Tolerance 1/2% as per RM usage of tolerance
#define TOLERANCE_CLK_DIFFERENCE(freq)      (freq/200)

#define STATUS_CHECK_RC(status, errorString)        \
    if (status != LW_OK)              \
    {                                               \
        Printf(Tee::PriHigh, errorString, status);  \
        rc = RmApiStatusToModsRC(status);           \
        CHECK_RC(rc);                               \
    }

static UINT32 hDevice = LWCTRL_DEVICE_HANDLE;
static UINT32 hSubDevice = LWCTRL_SUBDEVICE_HANDLE;

class PerfVF : public RmTest
{
private:

public:

    LwRm::Handle m_hClient;
    UINT32 hDev, hSubDev;

    PerfVF();
    virtual ~PerfVF();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//! \brief PerfVF constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
PerfVF::PerfVF()
{
    SetName("PerfVF");
}

//! \brief PerfVF destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
PerfVF::~PerfVF()
{
}

//! \brief IsTestSupported Function.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
string PerfVF::IsTestSupported()
{
    // This test is Supported only for CHEETAH BIG GPUS
    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        return "Supported only on CheetAh big gpu chips (T124+).\n";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! Does the basic allocation of device and subdevice that are
//! needed by all perf control APIs.
//!
//! \return OK if the allocations succeed, specific RC if failed
//! \sa Run
//-----------------------------------------------------------------------------
RC PerfVF::Setup()
{
    LwU32 status;
    LwRm::Handle m_hDevice;
    LwRm::Handle m_hSubDev;
    RC rc = OK;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW2080_ALLOC_PARAMETERS lw2080Params = {0};

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_hDevice = hDevice;
    m_hSubDev = hSubDevice;
    m_hClient = 0;

    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    STATUS_CHECK_RC((unsigned int) status, "ERROR:LwRmAllocRoot failed: status 0x%x\n");

    // get attached gpus ids
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                         &gpuAttachedIdsParams, sizeof (gpuAttachedIdsParams));
    STATUS_CHECK_RC((unsigned int) status, "ERROR:GET_ATTACHED_IDS failed: status 0x%x\n");

    // for each attached gpu...
    gpuIdInfoParams.szName = (LwP64)NULL;

    // get gpu instance info
    gpuIdInfoParams.gpuId = gpuAttachedIdsParams.gpuIds[0];
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                         &gpuIdInfoParams, sizeof (gpuIdInfoParams));
    STATUS_CHECK_RC((unsigned int) status, "ERROR:GET_ID_INFO failed: status 0x%x\n");

    //
    // allocate the gpu's device.
    //
    lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
    lw0080Params.hClientShare = 0;
    hDev = m_hDevice + gpuIdInfoParams.deviceInstance;
    status = LwRmAlloc(m_hClient, m_hClient, hDev,
                       LW01_DEVICE_0,
                       &lw0080Params);
    STATUS_CHECK_RC((unsigned int) status, "ERROR:LwRmAlloc failed: status 0x%x\n");

    // allocate subdevice
    hSubDev = m_hSubDev + gpuIdInfoParams.subDeviceInstance;
    lw2080Params.subDeviceId = gpuIdInfoParams.subDeviceInstance;
    status = LwRmAlloc(m_hClient, hDev, hSubDev,
                       LW20_SUBDEVICE_0,
                       &lw2080Params);
    STATUS_CHECK_RC((unsigned int) status, "ERROR:LwRmAlloc(SubDevice) failed: status 0x%x\n");

    return rc;
}

//! \brief Run Function.
//!
//! Does the actual verification of perf VF APIs.
//! It queries the VF table. It then loops through
//! each frequency point, caps to it and queries the capped frequency.
//! It then clears the cap to restore the frequency.
//! TODO: Change voltage.
//!
//! \return OK if the tests passed, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC PerfVF::Run()
{
    LwU32 status, i, j;
    LwU32 lwrrFreqKHz;
    LwU32 lwrrVoltuV;
    LW2080_CTRL_PERF_VF_POINT_OPS_PARAMS perfSetVfPointParams = {0};
    LW2080_CTRL_PERF_VF_POINT_OPS_DOMAINS *pVfDomainsParams = NULL;
    LW2080_CTRL_PERF_VF_TABLES_GET_INFO_PARAMS perfVfTablesParams = {0};
    LW2080_CTRL_PERF_VF_TABLES_ENTRIES_INFO_PARAMS perfVfTablesEntriesParams = {0};
    LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO *pVfIndexesInfo;
    LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO *pVfEntriesInfo;
    RC rc = OK;
    RC firstRc = OK;
    LwU32 pstateNum;

    //
    // We first get the range for VF index and entries tables both of which
    // are needed for the control call to query VF tables.
    //
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_VF_TABLES_GET_INFO,
                         &perfVfTablesParams,
                         sizeof (perfVfTablesParams));
    STATUS_CHECK_RC((unsigned int) status, "ERROR:VF_TABLES_GET_INFO command failed: 0x%x\n");
    // We can only support specific number of domains for now.
    if (perfVfTablesParams.numIndexes > LW2080_CTRL_PERF_VF_POINT_OPS_DOMAINS_MAX)
    {
        Printf(Tee::PriHigh,
        "ERROR:VFIndexTableInfo has more than %d entries:\n",
        LW2080_CTRL_PERF_VF_POINT_OPS_DOMAINS_MAX);
        return RC::SOFTWARE_ERROR;
    }

    //
    // Initialize the parameters for VF tables GET control call.
    // Index Table provides the index into the entries table per pstate.
    // Entries Table provides the individual VF entry.
    //
    perfVfTablesEntriesParams.vfIndexesInfoListSize = perfVfTablesParams.numIndexes;
    perfVfTablesEntriesParams.vfEntriesInfoListSize = perfVfTablesParams.numEntries;
    pVfIndexesInfo = new LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO[perfVfTablesParams.numIndexes *
                       sizeof(LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO)];
    if (NULL == pVfIndexesInfo)
    {
        Printf(Tee::PriHigh,
        "ERROR:Allocation of Index Table Info failed:\n");
        return RC::SOFTWARE_ERROR;
    }
    memset((void*)pVfIndexesInfo, 0, perfVfTablesParams.numIndexes *
           sizeof(LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO));

    for (i = 0; i < perfVfTablesParams.numIndexes; i++)
    {
        pVfIndexesInfo[i].index = i;
    }

    pVfEntriesInfo = new LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO[perfVfTablesParams.numEntries *
                         sizeof(LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO)];
    if (NULL == pVfEntriesInfo)
    {
        Printf(Tee::PriHigh,
        "ERROR:Allocation of Entries Table Info failed:\n");
        return RC::SOFTWARE_ERROR;
    }
    memset((void*)pVfEntriesInfo, 0, perfVfTablesParams.numEntries *
           sizeof(LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO));

    for (i = 0; i < perfVfTablesParams.numEntries; i++)
    {
        pVfEntriesInfo[i].index = i;
    }

    perfVfTablesEntriesParams.vfIndexesInfoList = (LwP64)pVfIndexesInfo;
    perfVfTablesEntriesParams.vfEntriesInfoList = (LwP64)pVfEntriesInfo;

    // Query the VF tables.
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_VF_TABLES_ENTRIES_GET_INFO,
                         &perfVfTablesEntriesParams,
                         sizeof (perfVfTablesEntriesParams));
    STATUS_CHECK_RC((unsigned int) status, "ERROR:VF_TABLES_ENTRIES_GET_INFO command failed: 0x%x\n");
    //
    // We assume that we have a single pstate. So if the number of indices
    // returned are more than 1, we expect them to correspond to different
    // clk domains. At present we only support gpc2clk, so the value
    // should really be 1.
    //
    pstateNum = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
    for (i = 0; i < perfVfTablesParams.numIndexes; i++)
    {
        if (pstateNum == LW2080_CTRL_PERF_PSTATES_UNDEFINED)
            pstateNum = pVfIndexesInfo[i].pstate;

        if (pVfIndexesInfo[i].pstate != pstateNum)
        {
            Printf(Tee::PriHigh,
            "ERROR:Found more than 1 pstate:\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    // Didn't find any clk domain/pstate
    if (0 == perfVfTablesParams.numIndexes)
    {
        Printf(Tee::PriHigh,
        "ERROR:Found zero pstate/domains:\n");
        return RC::SOFTWARE_ERROR;
    }

    //
    // Iterate over the pstate VF index table. T124 has only 1.
    // Each index entry, provides the first and last index within
    // the VF entry table that are valid and will be used for this
    // test. For each VF point the test
    // 1. Sets the frequency and caps to it using the perf limit FORCED
    // 2. Queries the current capped frequency which should be same as
    //    the value set in #1 since we don't expect any other perf limits
    //    higher than FORCED to be in effect.
    // 3. Clears the CAP after iterating over all frequency points
    // Prior to setting the first frequency in VF point, it queries
    // the frequency for the domain and towards the end of test compares
    // the value with initial value for mismatch.
    // TODO:The test works on a single domain at a time and doesn't attempt
    // setting multiple domains in one call. This is probably something
    // that can be improved whenever CheetAh supports multiple clk domains.
    //
    perfSetVfPointParams.numOfEntries = 1;
    pVfDomainsParams = &perfSetVfPointParams.vfDomains[0];
    for (i = 0; i < perfVfTablesParams.numIndexes; i++)
    {
        // Initialize fields for querying VF info.
        pVfDomainsParams->clkDomain = pVfIndexesInfo[i].domain;
        pVfDomainsParams->reason = LW2080_CTRL_PERF_VF_POINT_OPS_REASON_FORCED;
        pVfDomainsParams->operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_GET;
        pVfDomainsParams->freqKHz = 0x0;
        pVfDomainsParams->voltuV = 0x0;
        status = LwRmControl(m_hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                             &perfSetVfPointParams,
                             sizeof (perfSetVfPointParams));
        STATUS_CHECK_RC((unsigned int) status, "ERROR:Unable to get VF point:status 0x%x\n");
        //
        // Store initial values to check at the end of the test to
        // make sure we have not left the system in a bad state.
        //
        lwrrFreqKHz = pVfDomainsParams->freqKHz;
        lwrrVoltuV = pVfDomainsParams->voltuV;

        for (j = pVfIndexesInfo[i].entryIndexFirst; j < pVfIndexesInfo[i].entryIndexLast; j++)
        {
            pVfDomainsParams->operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_SET;
            pVfDomainsParams->freqKHz = pVfEntriesInfo[j].maxFreqKHz;
            pVfDomainsParams->voltuV = 0;
            status = LwRmControl(m_hClient, hSubDev,
                                 LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                                 &perfSetVfPointParams,
                                 sizeof (perfSetVfPointParams));
            if (status != LW_OK)
            {
                Printf(Tee::PriHigh,
                "ERROR:Unable to cap to VF point frequency 0x%x:status 0x%x\n",
                (unsigned int) pVfEntriesInfo->maxFreqKHz, (unsigned int) status);
                firstRc = RmApiStatusToModsRC(status);
                continue;
            }
            pVfDomainsParams->operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_GET;
            pVfDomainsParams->freqKHz = 0x0;
            status = LwRmControl(m_hClient, hSubDev,
                                 LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                                 &perfSetVfPointParams,
                                 sizeof (perfSetVfPointParams));
            STATUS_CHECK_RC((unsigned int) status, "ERROR:Unable to get VF point:status 0x%x\n");
            //
            // Check for difference in frequencies which are outside the
            // tolerance limits.
            //
            double diff = abs((double)pVfEntriesInfo[j].maxFreqKHz -
                              (double)pVfDomainsParams->freqKHz);
            if (diff >
                TOLERANCE_CLK_DIFFERENCE(pVfEntriesInfo[j].maxFreqKHz))
            {
                Printf(Tee::PriHigh,
                "Mismatch: Set frequency: 0x%x KHz, read back 0x%x KHz.\n",
                pVfEntriesInfo[j].maxFreqKHz, pVfDomainsParams->freqKHz);
                firstRc = RC::SOFTWARE_ERROR;
            }
        }
        //
        // Now that we are done with our test on one domain, clear the capped
        // frequency limit. This should result in the frequency to be whatever
        // it was when the test was started.
        //
        pVfDomainsParams->reason = LW2080_CTRL_PERF_VF_POINT_OPS_REASON_FORCED;
        pVfDomainsParams->operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_CLEAR;
        status = LwRmControl(m_hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                             &perfSetVfPointParams,
                             sizeof (perfSetVfPointParams));
        STATUS_CHECK_RC((unsigned int) status, "ERROR:Unable to clear frequency cap:status 0x%x\n");

        pVfDomainsParams->operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_GET;
        pVfDomainsParams->freqKHz = 0x0;
        status = LwRmControl(m_hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                             &perfSetVfPointParams,
                             sizeof (perfSetVfPointParams));
        STATUS_CHECK_RC((unsigned int) status, "ERROR:Unable to get VF point:status 0x%x\n");
        //
        // Any difference in frequencies between start of the test and now
        // should be flagged as an error.
        //
        if (lwrrFreqKHz != pVfDomainsParams->freqKHz)
        {
            Printf(Tee::PriHigh,
            "Frequency at the end of test 0x%x KHz doesn't match beginning value 0x%x kHz.\n",
            pVfDomainsParams->freqKHz, lwrrFreqKHz);
            return RC::SOFTWARE_ERROR;
        }
        if (lwrrVoltuV != pVfDomainsParams->voltuV)
        {
            Printf(Tee::PriHigh,
            "Volt at the end of test 0x%x uV doesn't match beginning value 0x%x uV.\n",
            pVfDomainsParams->freqKHz, lwrrVoltuV);
            return RC::SOFTWARE_ERROR;
        }
    }
    delete []pVfIndexesInfo;
    delete []pVfEntriesInfo;
    return firstRc;
}

//! \brief Cleanup Function.
//!
//! Cleans up the allocated resources during setup
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC PerfVF::Cleanup()
{
    // free handles
    LwRmFree(m_hClient, hDev, hSubDev);
    LwRmFree(m_hClient, m_hClient, hDev);
    LwRmFree(m_hClient, m_hClient, m_hClient);

    return OK;
}

//------------------------------------------------------------------------------
// PerfVF JavaScript linkage.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT
        (
        PerfVF,
        RmTest,
        "Simple test for verifying SW paths for Perf VF APIs"
        );

