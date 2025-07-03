/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _VR_INPUT_SIDE_SENSING_H_
#define _VR_INPUT_SIDE_SENSING_H_

#include "mdiag/tests/gpu/lwgpu_single.h"

class VRInputSideSensingConnectionTest: public LWGpuSingleChannelTest
{
public:
    VRInputSideSensingConnectionTest(ArgReader *params);
    virtual int Setup(void);
    virtual void Run(void);
    virtual void CleanUp(void);
    static Test *Factory(ArgDatabase *args);

private:
    UINT32 m_arch;
    int errCnt;
    int test_mux_sel_connection;
    int test_adc_therm_connection;
    int test_adc_input_connection;

    int TestMuxSelConnection();
    int TestAdcThermConnection();
    int TestAdcInputConnection();
};

#ifdef MAKE_TEST_LIST
// This macro is defined in ../../test.h
CREATE_TEST_LIST_ENTRY(vr_input_side_sensing, VRInputSideSensingConnectionTest, "Connection test for Voltage Regulator input side sensing feature.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &vr_input_side_sensing_testentry
#endif

#endif //_VR_INPUT_SIDE_SENSING_H_

