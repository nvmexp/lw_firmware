/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010, 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "idle_flipped.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

static bool idle_flipped = false;

extern const ParamDecl idle_flipped_params[] =
{
    SIMPLE_PARAM("-idle_flipped", "add idle flipped check"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

IdleFlipped::IdleFlipped(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-idle_flipped"))
      idle_flipped = 1;
}

IdleFlipped::~IdleFlipped(void)
{
}

STD_TEST_FACTORY(idle_flipped, IdleFlipped);

int
IdleFlipped::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("IdleFlipped: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("IdleFlipped: idle_flipped can only be run on GPU's that support a register map!\n");
        return (0);
    }

    getStateReport()->init("idle_flipped");
    getStateReport()->enable();

    return 1;
}

void
IdleFlipped::CleanUp(void)
{
 if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("IdleFlipped::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
IdleFlipped::Run(void)
{
    // InfoPrintf Flag to determine whether the test fail because of elw issue or test issue
    InfoPrintf("FLAG : IdleFlipped starting...\n");

    SetStatus(TEST_INCOMPLETE);
    if(idle_flipped)
    {
      if(idle_flipped_check())
        {
          SetStatus(TEST_FAILED);
          ErrPrintf("IdleFlipped::idle_flipped_check test failed\n");
          return;
        }
    }

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
    InfoPrintf(" idle flipped test END.\n");
    return;
}

int
IdleFlipped::idle_flipped_check(){

    UINT32 wr_data0 = 0x0;
    UINT32 errCnt = 0x0;

    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);
    lwgpu->GetRegFldNum("LW_PPWR_PMU_PG_STAT(4)", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);

    Platform::EscapeWrite("eng_pg_on_n", 0, 1, 1);
    Platform::Delay(1);
    Platform::EscapeWrite("eng_pg_on_n", 0, 1, 0);
    Platform::Delay(5);

    lwgpu->GetRegFldNum("LW_PPWR_PMU_PG_STAT(4)", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);
    if( (wr_data0 & 0x10000000 ) != 0x0 )
    {
       ErrPrintf("111 nonpmu Error!\n");
       errCnt++;
    }

    lwgpu->SetRegFldNum("LW_PSEC_FALCON_DMATRFBASE", "", 0x10000);
    lwgpu->SetRegFldNum("LW_PSEC_FALCON_DMATRFFBOFFS", "", 0x0);
    lwgpu->SetRegFldNum("LW_PSEC_FALCON_DMATRFMOFFS", "", 0x0);
    lwgpu->SetRegFldNum("LW_PSEC_FALCON_DMATRFCMD", "", 0x600);
    Platform::Delay(5);

    lwgpu->GetRegFldNum("LW_PPWR_PMU_PG_STAT(4)", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);
    if( (wr_data0 & 0x10000000 ) != 0x10000000 )
     {
       ErrPrintf("000 nonpmu Error!\n");
       errCnt++;
     }

    Platform::EscapeWrite("eng_pg_on_n", 0, 1, 1);
    Platform::Delay(1);
    Platform::EscapeWrite("eng_pg_on_n", 0, 1, 0);
    Platform::Delay(5);

    lwgpu->GetRegFldNum("LW_PPWR_PMU_PG_STAT(4)", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", &wr_data0);
    if( (wr_data0 & 0x10000000 ) != 0x0 )
     {
       ErrPrintf("001 nonpmu Error!\n");
       errCnt++;
     }

    lwgpu->SetRegFldNum("LW_PFB_PRI_MMU_BIND_IMB", "", 0x100);
    lwgpu->SetRegFldNum("LW_PFB_PRI_MMU_BIND", "", 0x80000015);
    lwgpu->GetRegFldNum("LW_PPWR_PMU_PG_STAT(4)", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);
    if( (wr_data0 & 0x10000000 ) != 0x10000000 )
     {
       ErrPrintf("010 nonpmu Error!\n");
       errCnt++;
     }
    Platform::Delay(5);
    return errCnt;
}

