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
#ifndef _GSP2HOST_
#define _GSP2HOST_

// test the connection of gsp2host_status_idle

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class Gsp2Host : public LWGpuSingleChannelTest
{

public:

  Gsp2Host(ArgReader *Params);

  virtual ~Gsp2Host(void);

  static Test *Factory(ArgDatabase *args);

  //a little extra setup to be done
  virtual int Setup(void);

  //Run() overridden - now we're a real test!
  virtual void Run(void);

  //a little extra cleanup to be done
  virtual void CleanUp(void);

  int gsp2host_status_idle_check();

private:
  IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(check_gsp2host_status_idle, Gsp2Host, "GPU GSP2HOST STATUS IDLE Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &check_gsp2host_status_idle_testentry
#endif

#endif
