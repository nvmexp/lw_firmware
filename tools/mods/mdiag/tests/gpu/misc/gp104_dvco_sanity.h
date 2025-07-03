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
#ifndef _GP104_DVCO_SANITY_
#define _GP104_DVCO_SANITY_

// test the connection of pwr2pmgr_vid_pwm between LW_pwr and pmgr

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class Gp104DvcoSanity : public LWGpuSingleChannelTest
{

    public:

      Gp104DvcoSanity(ResourceController *controller, ArgReader *params);

      virtual ~Gp104DvcoSanity(void);

      static Test *Factory(ArgDatabase *args, ResourceController *controller);

      //a little extra setup to be done
      virtual int Setup(void);

      //Run() overriden - now we're a real test!
      virtual void Run(void);

      //a little extra cleanup to be done
      virtual void CleanUp(void);

      int gp104_dvco_sanity_check();

    protected:
      UINT32  vidpwmTest();
    private:
      int errCnt;
      IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(gp104_dvco_sanity, Gp104DvcoSanity, "GPU GP104 DVCO SANITY Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &gp104_dvco_sanity_testentry
#endif

#endif  // _VIDPWM_H_
