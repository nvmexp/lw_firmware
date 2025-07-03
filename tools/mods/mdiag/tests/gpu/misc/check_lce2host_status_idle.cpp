/*
 *  LWIDIA_COPYRIGHT_BEGIN
 *
 *  Copyright 2016, 2020-2021 by LWPU Corporation. All rights reserved. All
 *  information contained herein is proprietary and confidential to LWPU
 *  Corporation. Any use, reproduction, or disclosure without the written
 *  permission of LWPU Corporation is prohibited.
 *
 *  LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/stdtest.h"

#include "check_lce2host_status_idle.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

static bool check_lce2host_status_idle = false;

extern const ParamDecl check_lce2host_status_idle_params[] =
{
  SIMPLE_PARAM("-check_lce2host_status_idle", "add lce2host status idle check"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};

//  constructor
Lce2Host::Lce2Host(ArgReader *params) :
  LWGpuSingleChannelTest(params)
{
  if (params->ParamPresent("-check_lce2host_status_idle"))
    check_lce2host_status_idle = 1;
}

//  destructor
Lce2Host::~Lce2Host(void)
{
}

//  Factory
STD_TEST_FACTORY(check_lce2host_status_idle, Lce2Host);

//  setup method
int Lce2Host::Setup(void)
{
  lwgpu = LWGpuResource::FindFirstResource();

  ch = lwgpu->CreateChannel();
  if(!ch)
  {
    ErrPrintf("Lce2Host: Setup failed to create channel\n");
    return (0);
  }

  m_regMap = lwgpu->GetRegisterMap();
  if(!m_regMap)
  {
    ErrPrintf("Lce2Host: lce2host can only be run on GPU's that support a register map!\n");
    return (0);
  }

  getStateReport()->init("check_lce2host_status_idle");
  getStateReport()->enable();

  return 1;
}

void
Lce2Host::CleanUp(void)
{
  if(ch)
  {
    delete ch;
    ch = 0;
  }

  if(lwgpu)
  {
    DebugPrintf("Lce2Host::CleanUp: Releasing GPU.\n");
    lwgpu = 0;
  }

  LWGpuSingleChannelTest::CleanUp();
}

void
Lce2Host::Run(void)
{
  //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
  InfoPrintf("FLAG : Lce2Host starting ....\n");

  SetStatus(TEST_INCOMPLETE);
  if(check_lce2host_status_idle)
  {
    if(lce2host_status_idle_check())
    {
      SetStatus(TEST_FAILED);
      ErrPrintf("Lce2Host::lce2host_status_idle test failed\n");
      return;
    }
    else
    {
      SetStatus(TEST_SUCCEEDED);
      getStateReport()->crcCheckPassedGold();
      return;
    }
  }
  InfoPrintf("lce2host status idle test END.\n");
}

int
Lce2Host::lce2host_status_idle_check()
{
  UINT32 errCnt = 0X0;
  UINT32 data6 = 0x0;
  UINT32 data7 = 0x0;
  UINT32 data8 = 0x0;

  InfoPrintf("data6 is 0x%x\n", data6);
  InfoPrintf("data7 is 0x%x\n", data7);
  InfoPrintf("data8 is 0x%x\n", data8);

  UINT32 r_data = 0x0;
  InfoPrintf("r_data is 0x%x\n", r_data);

  InfoPrintf("Starting lce2host status idle check\n");

//start test 0
  Platform::EscapeWrite("lce62host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("lce72host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("lce82host_status_idle", 0, 1, 0);
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("lce72host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("lce82host_status_idle", 0, 1, 1);
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("lce72host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("lce82host_status_idle", 0, 1, 0);
  Platform::Delay(5);

  Platform::EscapeRead("lce62host_status_idle", 0, 1, &data6);
  Platform::EscapeRead("lce72host_status_idle", 0, 1, &data7);
  Platform::EscapeRead("lce82host_status_idle", 0, 1, &data8);
  Platform::Delay(5);

  lwgpu->GetRegFldNum("LW_PPWR_PMU_IDLE_STATUS_2", "", &r_data);
  Platform::Delay(5);

  InfoPrintf("data6 is 0x%x\n", data6);
  InfoPrintf("data7 is 0x%x\n", data7);
  InfoPrintf("data8 is 0x%x\n", data8);

  InfoPrintf("r_data is 0x%x\n", r_data);

  if((data6 & 0x1) != 0) { ErrPrintf("Error. case 6.\n"); errCnt++; }
  if((data7 & 0x1) != 0) { ErrPrintf("Error. case 7.\n"); errCnt++; }
  if((data8 & 0x1) != 0) { ErrPrintf("Error. case 8.\n"); errCnt++; }

  if((r_data & 0x1) != 0) {ErrPrintf("Error. case r_data6.\n"); errCnt++; }
  if((r_data & 0x2) != 0) {ErrPrintf("Error. case r_data7.\n"); errCnt++; }
  if((r_data & 0x4) != 0) {ErrPrintf("Error. case r_data8.\n"); errCnt++; }

  Platform::Delay(10);

  Platform::EscapeWrite("lce62host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("lce72host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("lce82host_status_idle", 0, 1, 1);
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("lce72host_status_idle", 0, 1, 0);
  Platform::EscapeWrite("lce82host_status_idle", 0, 1, 0);
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("lce72host_status_idle", 0, 1, 1);
  Platform::EscapeWrite("lce82host_status_idle", 0, 1, 1);
  Platform::Delay(5);

  Platform::EscapeRead("lce62host_status_idle", 0, 1, &data6);
  Platform::EscapeRead("lce72host_status_idle", 0, 1, &data7);
  Platform::EscapeRead("lce82host_status_idle", 0, 1, &data8);
  Platform::Delay(5);

  lwgpu->GetRegFldNum("LW_PPWR_PMU_IDLE_STATUS_2", "", &r_data);
  Platform::Delay(5);

  InfoPrintf("data6 is 0x%x\n", data6);
  InfoPrintf("data7 is 0x%x\n", data7);
  InfoPrintf("data8 is 0x%x\n", data8);

  InfoPrintf("r_data is 0x%x\n", r_data);

  if((data6 & 0x1) != 1) { ErrPrintf("Error. case 6.\n"); errCnt++; }
  if((data7 & 0x1) != 1) { ErrPrintf("Error. case 7.\n"); errCnt++; }
  if((data8 & 0x1) != 1) { ErrPrintf("Error. case 8.\n"); errCnt++; }

  if((r_data & 0x1) != 0x1) {ErrPrintf("Error. case r_data6.\n"); errCnt++; }
  if((r_data & 0x2) != 0x2) {ErrPrintf("Error. case r_data7.\n"); errCnt++; }
  if((r_data & 0x4) != 0x4) {ErrPrintf("Error. case r_data8.\n"); errCnt++; }

  InfoPrintf("End test lce2host_status_idle\n");
  Platform::Delay(10);

///////////////////////////////////////////////////////////////////////////////////////
//=================================test intr_nostall=================================//
///////////////////////////////////////////////////////////////////////////////////////

  InfoPrintf("Start test lce2host_status_intr_nostall\n");
  //ce2host nostall interrupt is not expected
  r_data = 0x0;
  InfoPrintf("r_data is 0x%x.\n", r_data);
  lwgpu->GetRegFldNum("LW_PMC_INTR_EN(1)", "_VALUE", &r_data);
  InfoPrintf("r_data is 0x%x.\n", r_data);
  if((r_data & 0x4009) == 0x4009 )
    {
      InfoPrintf("CE6, CE7, CE8 interrupt is on.\n");
    }
  else
    {
      InfoPrintf("CE6, CE7, CE8 interrupt is off.\n");
    }
  //clear ce6 ce7 ce8 interrupt
  lwgpu->SetRegFldNum("LW_PMC_INTR_EN_CLEAR(1)", "_VALUE", 0x00004009);
  Platform::Delay(10);

  r_data = 0x0;
  InfoPrintf("r_data is 0x%x.\n", r_data);
  lwgpu->GetRegFldNum("LW_PMC_INTR_EN(1)", "_VALUE", &r_data);
  InfoPrintf("r_data is 0x%x.\n", r_data);
  if((r_data & 0x4009) == 0x4009 )
    {
      InfoPrintf("CE6, CE7, CE8 interrupt is on.\n");
    }
  else
    {
      InfoPrintf("CE6, CE7, CE8 interrupt is off.\n");
    }

  Platform::EscapeWrite("lce62host_status_intr_nostall", 0, 1, 0);
  Platform::EscapeWrite("lce72host_status_intr_nostall", 0, 1, 0);
  Platform::EscapeWrite("lce82host_status_intr_nostall", 0, 1, 0);
  InfoPrintf("test write 0 \n");
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_intr_nostall", 0, 1, 1);
  Platform::EscapeWrite("lce72host_status_intr_nostall", 0, 1, 1);
  Platform::EscapeWrite("lce82host_status_intr_nostall", 0, 1, 1);
  InfoPrintf("test write 1 \n");
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_intr_nostall", 0, 1, 0);
  Platform::EscapeWrite("lce72host_status_intr_nostall", 0, 1, 0);
  Platform::EscapeWrite("lce82host_status_intr_nostall", 0, 1, 0);
  InfoPrintf("test write 0 \n");
  Platform::Delay(5);

  Platform::EscapeRead("lce62host_status_intr_nostall", 0, 1, &data6);
  Platform::EscapeRead("lce72host_status_intr_nostall", 0, 1, &data7);
  Platform::EscapeRead("lce82host_status_intr_nostall", 0, 1, &data8);
  InfoPrintf("test read 0 \n");
  Platform::Delay(5);

  lwgpu->GetRegFldNum("LW_PPWR_PMU_IDLE_STATUS_2", "", &r_data);
  Platform::Delay(5);

  InfoPrintf("data6 is 0x%x\n", data6);
  InfoPrintf("data7 is 0x%x\n", data7);
  InfoPrintf("data8 is 0x%x\n", data8);

  InfoPrintf("r_data is 0x%x\n", r_data);

  if((data6 & 0x1) != 0) { ErrPrintf("Error. case 6.\n"); errCnt++; }
  if((data7 & 0x1) != 0) { ErrPrintf("Error. case 7.\n"); errCnt++; }
  if((data8 & 0x1) != 0) { ErrPrintf("Error. case 8.\n"); errCnt++; }

  if((r_data & 0x08) != 0x08) {ErrPrintf("Error. case r_data6.\n"); errCnt++; }
  if((r_data & 0x10) != 0x10) {ErrPrintf("Error. case r_data7.\n"); errCnt++; }
  if((r_data & 0x20) != 0x20) {ErrPrintf("Error. case r_data8.\n"); errCnt++; }

  Platform::Delay(10);

  Platform::EscapeWrite("lce62host_status_intr_nostall", 0, 1, 1);
  Platform::EscapeWrite("lce72host_status_intr_nostall", 0, 1, 1);
  Platform::EscapeWrite("lce82host_status_intr_nostall", 0, 1, 1);
  InfoPrintf("test write 1 \n");
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_intr_nostall", 0, 1, 0);
  Platform::EscapeWrite("lce72host_status_intr_nostall", 0, 1, 0);
  Platform::EscapeWrite("lce82host_status_intr_nostall", 0, 1, 0);
  InfoPrintf("test write 0 \n");
  Platform::Delay(5);

  Platform::EscapeWrite("lce62host_status_intr_nostall", 0, 1, 1);
  Platform::EscapeWrite("lce72host_status_intr_nostall", 0, 1, 1);
  Platform::EscapeWrite("lce82host_status_intr_nostall", 0, 1, 1);
  InfoPrintf("test write 1 \n");
  Platform::Delay(5);

  Platform::EscapeRead("lce62host_status_intr_nostall", 0, 1, &data6);
  Platform::EscapeRead("lce72host_status_intr_nostall", 0, 1, &data7);
  Platform::EscapeRead("lce82host_status_intr_nostall", 0, 1, &data8);
  InfoPrintf("test read 1 \n");
  Platform::Delay(5);

  lwgpu->GetRegFldNum("LW_PPWR_PMU_IDLE_STATUS_2", "", &r_data);
  Platform::Delay(5);

  InfoPrintf("data6 is 0x%x\n", data6);
  InfoPrintf("data7 is 0x%x\n", data7);
  InfoPrintf("data8 is 0x%x\n", data8);

  InfoPrintf("r_data is 0x%x.\n", r_data);

  if((data6 & 0x1) != 1) { ErrPrintf("Error. case 6.\n"); errCnt++; }
  if((data7 & 0x1) != 1) { ErrPrintf("Error. case 7.\n"); errCnt++; }
  if((data8 & 0x1) != 1) { ErrPrintf("Error. case 8.\n"); errCnt++; }

  if((r_data & 0x08) != 0x0) {ErrPrintf("Error. case r_data6.\n"); errCnt++; }
  if((r_data & 0x10) != 0x0) {ErrPrintf("Error. case r_data7.\n"); errCnt++; }
  if((r_data & 0x20) != 0x0) {ErrPrintf("Error. case r_data8.\n"); errCnt++; }

  Platform::Delay(10);
  return errCnt;
}

