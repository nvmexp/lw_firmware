/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _CHECK_CLAMP_H_
#define _CHECK_CLAMP_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"

#define LW_PBUS_DEBUG_1 0x00001084

class Check_clamp: public LWGpuSingleChannelTest {
public:
    Check_clamp(ArgReader *params);

    virtual ~Check_clamp(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 m_arch;
    int Read_and_check_clamp_enable(UINT32 length, UINT32 clamp_0_bit, UINT32 clamp_1_bit, int i);
    int Read_and_check_clamp_disable(UINT32 length, UINT32 clamp_0_bit, UINT32 clamp_1_bit, int i);
    int programFuse(UINT32 addr, UINT32 data, UINT32 mask);
    int senseFuse(void);
    void Enable_fuse_program(void);
    void updateFuseopt(void);
    int main_test(void);

    void writeFusedata(UINT32 addr, UINT32 mask, UINT32 val,UINT32 &fusedata0,UINT32 &fusedata1,UINT32 &fusedata2,UINT32 &fusedata3,UINT32 &fusedata4,UINT32 &fusedata5,UINT32 &fusedata6,UINT32 &fusedata7,UINT32 &fusedata8,UINT32 &fusedata9,UINT32 &fusedata10,UINT32 &fusedata11,UINT32 &fusedata12,UINT32 &fusedata13,UINT32 &fusedata14,UINT32 &fusedata15,UINT32 &fusedata16,UINT32 &fusedata17,UINT32 &fusedata18,UINT32 &fusedata19,UINT32 &fusedata20,UINT32 &fusedata21,UINT32 &fusedata22,UINT32 &fusedata23,UINT32 &fusedata24,UINT32 &fusedata25,UINT32 &fusedata26,UINT32 &fusedata27);

private:
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(check_clamp, Check_clamp, "GPU Thermal Check_clamp Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &check_clamp_testentry
#endif

#endif  // _I2CSLAVE_H_
