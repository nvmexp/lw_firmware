/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _CHECK_CONNECT_
#define _CHECK_CONNECT_

// test the connection

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"
#include "pwr_macro.h"

class CheckConnect : public LWGpuSingleChannelTest
{

public:

  CheckConnect(ArgReader *params);

  virtual ~CheckConnect(void);

  static Test *Factory(ArgDatabase *args);

  //a little extra setup to be done
  virtual int Setup(void);

  //Run() overriden - now we're a real test!
  virtual void Run(void);

  //a little extra cleanup to be done
  virtual void CleanUp(void);

  int check_gp10b_connect();
  int check_host2pmu_reset_connect();
  int check_adc_connect();
  int check_gpcadc_connect();
  int check_xtl2therm_pci_connect();
  int check_dwb_connect();
  int check_sci2therm_xtal_gate_stretch_connect();
  int check_disp_2pmu_intr_connect();
  int check_fuse2pwr_debug_mode_connect();
  int check_pmgr2pwr_ext_alert_connect();
  int check_priv_block_connect();
  int check_vid_pwm();
  int check_misc_connect();
  int check_hshub2x_connect();
//protected:
  //UINT32 vidpwmTest();
private:
  //int errCnt;
  Macro macros;
  UINT32 m_arch;
  IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(check_connect, CheckConnect, "GPU CONNECT TEST");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &check_connect_testentry
#endif

#endif  //_VIDPWM_H_
