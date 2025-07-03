/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010      by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _GP104_IDLE_FLIPPED_
#define _GP104_IDLE_FLIPPED_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class Gp104IdleFlipped : public LWGpuSingleChannelTest
{

  public:

      Gp104IdleFlipped(ResourceController *controller, ArgReader *params);

      virtual ~Gp104IdleFlipped(void);

      static Test *Factory(ArgDatabase *args, ResourceController *controller);

      // a little extra setup to be done
      virtual int Setup(void);

      // Run() overridden - now we're a real test!
      virtual void Run(void);

      // a little extra cleanup to be done
      virtual void CleanUp(void);

      int gp104_idle_flipped_check();
  protected:
      UINT32 vidpwmTest();
  private:
      int errCnt;
      IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(gp104_idle_flipped, Gp104IdleFlipped, "GPU GP104 IDLE FLIPPED Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &gp104_idle_flipped_testentry
#endif

#endif
