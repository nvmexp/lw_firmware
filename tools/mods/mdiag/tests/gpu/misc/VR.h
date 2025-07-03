/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VR_H_
#define _VR_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"

class VR_Test: public LWGpuSingleChannelTest
{
public:
    VR_Test(ArgReader *params);
    virtual int Setup(void);
    virtual void Run(void);
    virtual void CleanUp(void);
    static Test *Factory(ArgDatabase *args);
private:
    UINT32 m_arch;
    bool high;
    bool low;
    float rate;
    float hightest();
    float lowtest();
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(duty_cycle_vr, VR_Test, "Duty cycle of VR" );
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &duty_cycle_vr_testentry
#endif

#endif
