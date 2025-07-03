/*
 *  LWIDIA_COPYRIGHT_BEGIN
 *
 *  Copyright 2016, 2020-2021  by LWPU Corporation. All rights reserved. All
 *  information contained herein is proprietary and confidential to LWPU
 *  Corporation. Any use, reproduction, or disclosure without the written
 *  permission of LWPU Corporation is prohibited.
 *
 *  LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/stdtest.h"

#include "check_gsp2host_status_idle.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

static bool check_gsp2host_status_idle = false;

extern const ParamDecl check_gsp2host_status_idle_params[] =
{
  SIMPLE_PARAM("-check_gsp2host_status_idle", "add gsp2host status idle check"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};

Gsp2Host::Gsp2Host(ArgReader *params) :
  LWGpuSingleChannelTest(params)
{
  if (params->ParamPresent("-check_gsp2host_status_idle"))
    check_gsp2host_status_idle = 1;
}

Gsp2Host::~Gsp2Host(void)
{
}

STD_TEST_FACTORY(check_gsp2host_status_idle, Gsp2Host);

int
Gsp2Host::Setup(void)
{
  lwgpu = LWGpuResource::FindFirstResource();

  ch = lwgpu->CreateChannel();
  if(!ch)
  {
    ErrPrintf("Gsp2Host: Setup failed to create channel\n");
    return (0);
  }

  m_regMap = lwgpu->GetRegisterMap();
  if(!m_regMap)
  {
    ErrPrintf("Gsp2Host: check_gsp2host_status_idle can only be run on GPU's that support a register map!\n");
    return (0);
  }

  getStateReport()->init("check_gsp2host_status_idle");
  getStateReport()->enable();

  return 1;
}

void
Gsp2Host::CleanUp(void)
{
  if(ch)
  {
    delete ch;
    ch = 0;
  }

  if(lwgpu)
  {
    DebugPrintf("Gsp2Host::CleanUp: Releasing GPU.\n");
    lwgpu = 0;
  }

  LWGpuSingleChannelTest::CleanUp();
}

void
Gsp2Host::Run(void)
{
  //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
  InfoPrintf("FLAG : Gsp2Host starting ....\n");

  SetStatus(TEST_INCOMPLETE);

  if(check_gsp2host_status_idle)
  {
    if(gsp2host_status_idle_check())
    {
      SetStatus(TEST_FAILED);
      ErrPrintf("Gsp2Host::gsp2host_status_idle test failed\n");
      return;
    }
  }

  SetStatus(TEST_SUCCEEDED);
  getStateReport()->crcCheckPassedGold();

  InfoPrintf("gsp2host status idle test END.\n");
  return;
}

int
Gsp2Host::gsp2host_status_idle_check()
{
  UINT32 data1 = 0x0;
  UINT32 data2 = 0x0;
  UINT32 r_data = 0x0;

  InfoPrintf("data1 is 0x%x\n", data1);
  InfoPrintf("data2 is 0x%x\n", data2);
  InfoPrintf("r_data is 0x%x\n", r_data);

  InfoPrintf("Starting gsp2host status idle check\n");

//start test 0
  Platform::EscapeWrite("gsp2host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("gsp2host_status_intr_nostall", 0, 1, 0);
  Platform::Delay(5);

  Platform::EscapeWrite("gsp2host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("gsp2host_status_intr_nostall", 0, 1, 1);
  Platform::Delay(5);

  Platform::EscapeWrite("gsp2host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("gsp2host_status_intr_nostall", 0, 1, 0);
  Platform::Delay(5);

  Platform::EscapeRead("gsp2host_status_idle", 0, 1, &data1);
  Platform::EscapeRead("gsp2host_status_intr_nostall", 0 ,1, &data2);
  Platform::Delay(5);

  lwgpu->GetRegFldNum("LW_PPWR_PMU_IDLE_STATUS_1", "", &r_data);
  Platform::Delay(5);

  InfoPrintf("data1 is 0x%x\n", data1);
  if((data1 & 0x1) != 0) { ErrPrintf("Error. case 1.\n"); return (1); }
  InfoPrintf("data2 is 0x%x\n", data2);
  if((data2 & 0x1) != 0) { ErrPrintf("Error. case 2.\n"); return (1); }
  InfoPrintf("r_data is 0x%x\n", r_data);
  if((r_data & 0x2) != 0) {ErrPrintf("Error. case r_data 0.\n"); return (1); }
//end test 0

  Platform::Delay(10);

//start test 1
  InfoPrintf("data1 is 0x%x\n", data1);
  InfoPrintf("data2 is 0x%x\n", data2);
  InfoPrintf("r_data is 0x%x\n", r_data);

  Platform::EscapeWrite("gsp2host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("gsp2host_status_intr_nostall", 0, 1, 1);
  Platform::Delay(5);

  Platform::EscapeWrite("gsp2host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("gsp2host_status_intr_nostall", 0, 1, 0);
  Platform::Delay(5);

  Platform::EscapeWrite("gsp2host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("gsp2host_status_intr_nostall", 0, 1, 1);
  Platform::Delay(5);

  Platform::EscapeRead("gsp2host_status_idle", 0, 1, &data1);
  Platform::EscapeRead("gsp2host_status_intr_nostall", 0, 1, &data2);
  Platform::Delay(5);

  lwgpu->GetRegFldNum("LW_PPWR_PMU_IDLE_STATUS_1", "", &r_data);
  Platform::Delay(5);

  InfoPrintf("data1 is 0x%x\n", data1);
  InfoPrintf("data2 is 0x%x\n", data2);
  InfoPrintf("r_dara is 0x%x\n", r_data);
  if((data1 & 0x1) != 1) { ErrPrintf("Error. case 3.\n"); return (1); }
  if((data2 & 0x1) != 1) { ErrPrintf("Error. case 4.\n"); return (1); }
  if((r_data & 0x2) != 0x2) { ErrPrintf("Error. case r_data 1.\n"); return (1); }
//end test 1

  Platform::Delay(10);
  return (0);
}

