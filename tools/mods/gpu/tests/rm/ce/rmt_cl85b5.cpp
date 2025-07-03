/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2009,2019-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class85b5 test.
//

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl2080.h" // LW2080_ENGINE_TYPE*
#include "class/cl85b5.h" // GT212_DMA_COPY
#include "class/cl85b5sw.h"
#include "ctrl/ctrl2080.h"
#include "core/utility/errloggr.h"

#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"

#define MAX_COPY_ENGINES 2
#define CE_INSTANCES 5

class Class85b5Test: public RmTest
{
public:
    Class85b5Test();
    virtual ~Class85b5Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
     FLOAT64 m_TimeoutMs = Tasker::NO_TIMEOUT;
};

//------------------------------------------------------------------------------
Class85b5Test::Class85b5Test()
{
    SetName("Class85b5Test");
}

//------------------------------------------------------------------------------
Class85b5Test::~Class85b5Test()
{
}

//------------------------------------------------------------------------
string Class85b5Test::IsTestSupported()
{

    if( !IsClassSupported(GT212_DMA_COPY) )
       return "Class GT212_DMA_COPY not supported on this plateform";
    return RUN_RMTEST_TRUE;
}

//------------------------------------------------------------------------------
RC Class85b5Test::Setup()
{
    RC           rc;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    m_TestConfig.SetAllowMultipleChannels(true);

    return rc;
}

//------------------------------------------------------------------------
RC Class85b5Test::Run()
{
    UINT32 semVal;
    RC     rc;
    LwRmPtr      pLwRm;
    const UINT32 memSize = 0x1000;
    LW85B5_ALLOCATION_PARAMETERS parms = {0};
    UINT32       i = 0;
    bool         enginePresent[MAX_COPY_ENGINES] = {false, false};
    RC           errorRc;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = {0};
    UINT64       gpuAddr[CE_INSTANCES];
    UINT32 *     cpuAddr[CE_INSTANCES];
    LwRm::Handle hObj[CE_INSTANCES];
    LwRm::Handle hVA[CE_INSTANCES];
    LwRm::Handle hSemMem[CE_INSTANCES];
    Channel      *pCh[CE_INSTANCES];
    LwRm::Handle hCh[CE_INSTANCES];

    parms.version = 0;

    for ( int x = 0; x < CE_INSTANCES; x++ )
    {
        gpuAddr[x] = 0;
        cpuAddr[x] = 0;
        hObj[x] = 0;
        hVA[x] = 0;
        hSemMem[x] = 0;
        pCh[x] = 0;
        hCh[x] = 0;
    }

    CHECK_RC(StartRCTest());

    // Get a list of supported engines

    CHECK_RC_CLEANUP( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)) );

    for ( i = 0; i < engineParams.engineCount; i++ )
    {
          switch (engineParams.engineList[i])
          {
              case LW2080_ENGINE_TYPE_COPY0:
                  enginePresent[0] = true;
                  break;
              case LW2080_ENGINE_TYPE_COPY1:
                  enginePresent[1] = true;
                  break;
          }
    }

    for ( int i = 0; i < MAX_COPY_ENGINES; i++ )
    {
        if (!enginePresent[i])
            continue;

        CHECK_RC_CLEANUP(
            m_TestConfig.AllocateChannel(&pCh[0],
                                         &hCh[0],
                                         LW2080_ENGINE_TYPE_COPY(i)));

        parms.engineInstance = i;

        CHECK_RC_CLEANUP(pLwRm->Alloc(hCh[0], &hObj[0], GT212_DMA_COPY, &parms));

        //CHECK_RC(pLwRm->Alloc(hCh[0], &m_hObj, LW50_TESLA, NULL));
        //m_Notifier.Allocate(pCh[0], 1, &m_TestConfig);

        // Send in invalid method, assuming the last dword in the
        // subchannel is invalid.

        pCh[0]->SetObject(0, hObj[0]);
        pCh[0]->Write(0, LW85B5_PM_TRIGGER_END+sizeof(UINT32), 0);

        /*pCh[0]->SetObject(0, m_hObj);
        CHECK_RC(m_Notifier.Instantiate(0, LW5097_SET_CONTEXT_DMA_NOTIFY));
        m_Notifier.Clear(LW5097_NOTIFIERS_NOTIFY);
        pCh[0]->Write(0, LW5097_NOTIFY, LW5097_NOTIFY_TYPE_WRITE_ONLY);
        pCh[0]->Write(0, LW5097_NO_OPERATION, 0);
        pCh[0]->Flush();
        CHECK_RC(m_Notifier.Wait(LW5097_NOTIFIERS_NOTIFY,m_TimeoutMs));*/

        pCh[0]->Flush();
        pCh[0]->WaitIdle(m_TimeoutMs);

        errorRc = pCh[0]->CheckError();

        if ( errorRc != RC::RM_RCH_CE0_ERROR + i )
        {
            Printf(Tee::PriHigh,
                   "RobustChannelTest: error notifier incorrect. error = '%s'\n", errorRc.Message());
            rc = RC::UNEXPECTED_ROBUST_CHANNEL_ERR;

            //
            // If unexpected RC error then return that error to MODS
            // go to cleanup to free the allocated resources
            //
            goto Cleanup;
        }

        // after we get the Robust Channel error send the command on
        // the same channel, this is expected to fail with same
        // Robust channel error as we got previously because the channel
        // is isolated but not freed yet
        //
        CHECK_RC_CLEANUP(pCh[0]->Write(0, LW85B5_NOP, 0));

        errorRc = pCh[0]->Flush();
        if (errorRc != RC::RM_RCH_CE0_ERROR + i )
        {
            Printf(Tee::PriHigh,
                   "RobustChannelTest: error notifier incorrect. error = '%s'\n", errorRc.Message());
            rc = RC::UNEXPECTED_ROBUST_CHANNEL_ERR;

            //
            // If unexpected RC error then return that error to MODS
            // go to cleanup to free the allocated resources
            //
            goto Cleanup;
        }

        pLwRm->Free(hObj[0]);
        hObj[0] = 0;

        m_TestConfig.FreeChannel(pCh[0]);
        pCh[0] = 0;

        // Allocate channels
        for ( int x = 0; x < CE_INSTANCES; x++ )
        {
            CHECK_RC_CLEANUP(
                m_TestConfig.AllocateChannel(&pCh[x],
                                             &hCh[x],
                                             LW2080_ENGINE_TYPE_COPY(i)));
            CHECK_RC_CLEANUP(pLwRm->Alloc(hCh[x], &hObj[x], GT212_DMA_COPY, &parms));
        }

        // Send method down each channel
        for ( int x = 0; x < CE_INSTANCES; x++ )
        {
            CHECK_RC_CLEANUP(pLwRm->AllocSystemMemory(&hSemMem[x],
                     DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
                     DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                     DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
                     memSize, GetBoundGpuDevice()));

            LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
            CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                          &hVA[x], LW01_MEMORY_VIRTUAL, &vmparams));

            CHECK_RC_CLEANUP(pLwRm->MapMemoryDma(hVA[x], hSemMem[x], 0, memSize,
                                         LW04_MAP_MEMORY_FLAGS_NONE, &gpuAddr[x], GetBoundGpuDevice()));
            CHECK_RC_CLEANUP(pLwRm->MapMemory(hSemMem[x], 0, memSize, (void **)&cpuAddr[x], 0, GetBoundGpuSubdevice()));

            MEM_WR32(cpuAddr[x], 0x87654321);

            pCh[x]->SetObject(0, hObj[x]);

            pCh[x]->Write(0, LW85B5_SET_CTX_DMA(0), hVA[x]);
            pCh[x]->Write(0, LW85B5_SET_SEMAPHORE_A,
                            DRF_NUM(85B5, _SET_SEMAPHORE_A, _UPPER, (UINT32)(gpuAddr[x] >> 32LL)) |
                            DRF_NUM(85B5, _SET_SEMAPHORE_A, _CTX_DMA, 0));
            pCh[x]->Write(0, LW85B5_SET_SEMAPHORE_B, (UINT32)(gpuAddr[x] & 0xffffffffLL));
            pCh[x]->Write(0, LW85B5_SET_SEMAPHORE_PAYLOAD, 0x12345678);
            pCh[x]->Write(0, LW85B5_LAUNCH_DMA,
                            DRF_DEF(85B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_FOUR_WORD_SEMAPHORE) |
                            DRF_DEF(85B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                            DRF_DEF(85B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                            DRF_DEF(85B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NONE));

            pCh[x]->Flush();
        }

        // Verify CE engine wrote the correct value
        for ( int x = 0; x < CE_INSTANCES; x++ )
        {
            CHECK_RC_CLEANUP(pCh[x]->WaitIdle(m_TimeoutMs));

            semVal = MEM_RD32(cpuAddr[x]);

            if (semVal != 0x12345678)
            {
                Printf(Tee::PriHigh,
                      "Class85b5Test: expected semaphore to be 0x%x, was 0x%x\n",
                      0x12345678, semVal);
                rc = RC::DATA_MISMATCH;
                goto Cleanup;
            }
        }

        for ( int x = 0; x < CE_INSTANCES; x++ )
        {
            if ( gpuAddr[x] != 0 )
            {
                pLwRm->UnmapMemoryDma(hVA[x], hSemMem[x],
                                  LW04_MAP_MEMORY_FLAGS_NONE, gpuAddr[x], GetBoundGpuDevice());
                gpuAddr[x] = 0;
            }

            if ( cpuAddr[x] )
            {
                pLwRm->UnmapMemory(hSemMem[x], cpuAddr[x], 0, GetBoundGpuSubdevice());
                cpuAddr[x] = 0;
            }

            if ( hVA[x] )
            {
                pLwRm->Free(hVA[x]);
                hVA[x] = 0;
            }

            if ( hSemMem[x] )
            {
                pLwRm->Free(hSemMem[x]);
                hSemMem[x] = 0;
            }

            if ( hObj[x] )
            {
                pLwRm->Free(hObj[x]);
                hObj[x] = 0;
            }

            if ( pCh[x] )
            {
                m_TestConfig.FreeChannel(pCh[x]);
                pCh[x] = 0;
            }

            Printf(Tee::PriNormal, "CE%d (inst: %d) semaphore test passed\n", i, x);
        }

    }

Cleanup:
    for ( int x = 0; x < CE_INSTANCES; x++ )
    {
        if ( gpuAddr[x] != 0 )
        {
            pLwRm->UnmapMemoryDma(hVA[x], hSemMem[x],
                              LW04_MAP_MEMORY_FLAGS_NONE, gpuAddr[x], GetBoundGpuDevice());
            gpuAddr[x] = 0;
        }

        if ( cpuAddr[x] )
        {
            pLwRm->UnmapMemory(hSemMem[x], cpuAddr[x], 0, GetBoundGpuSubdevice());
            cpuAddr[x] = 0;
        }

        if ( hVA[x] )
        {
            pLwRm->Free(hVA[x]);
            hVA[x] = 0;
        }

        if ( hSemMem[x] )
        {
            pLwRm->Free(hSemMem[x]);
            hSemMem[x] = 0;
        }

        if ( hObj[x] )
        {
            pLwRm->Free(hObj[x]);
            hObj[x] = 0;
        }

        if ( pCh[x] )
        {
            m_TestConfig.FreeChannel(pCh[x]);
            pCh[x] = 0;
        }
    }

    EndRCTest();

    return rc;
}

//------------------------------------------------------------------------------
RC Class85b5Test::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class85b5Test object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class85b5Test, RmTest,
                 "Class85b5 RM test.");
