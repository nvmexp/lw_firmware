/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _AD10X_HOT_RST_REINIT_H_
#define _AD10X_HOT_RST_REINIT_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

#include "mdiag/utils/cregexcl.h"
#include "mdiag/IRegisterMap.h"

#include <map>
using std::map;

class Ad10xHotRstReInit : public LWGpuSingleChannelTest {
public:
    Ad10xHotRstReInit(ArgReader *params);

    virtual ~Ad10xHotRstReInit(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // gives the test valid pointer access to the gpu resources after an init/reset
    virtual int SetupGpuPointers(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    //UINT32 hotRstReInitTest();
    UINT32 testhotrst1();
    UINT32 testhotrst2();
    UINT32 testhotrst3();
    UINT32 testhotrst4();

private:

    bool check_results( UINT32 address, UINT32 expect_data);
    bool GetInitValue        (IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal, UINT32 *initMask);
    void disable_sbr_on_hotrst(void);
    void disable_fundamental_reset_on_hot_rst(void);
    void enable_gpu_hot_rst(void);
    void enable_cfg_hot_rst(void);
    void disable_gpu_hot_rst(void);
    void disable_cfg_hot_rst(void);
    void enable_gr_pfifo_engine(void);
    void corrupt_registers(void);
    bool check_register_reset(void);
    bool check_register_not_reset(void);
    void hot_reset(void);
    UINT32 gpu_initialize(void);

    IRegisterMap*                   m_regMap;
    unique_ptr<IRegisterClass> m_reg;
    UINT32 m_initVal;
    UINT32 m_initMask;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(ad10x_hotrst_reinit, Ad10xHotRstReInit, "Gt21x+ Hot Reset ReInit Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &ad10x_hotrst_reinit_testentry
#endif

#endif  // _AD10X_HOT_RST_REINIT_H_
