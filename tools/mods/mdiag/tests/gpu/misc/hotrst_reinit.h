/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _HOT_RST_REINIT_H_
#define _HOT_RST_REINIT_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

#include "mdiag/utils/cregexcl.h"
#include "mdiag/IRegisterMap.h"

#include <map>
using std::map;

class HotRstReInit : public LWGpuSingleChannelTest {
public:
    HotRstReInit(ArgReader *params);

    virtual ~HotRstReInit(void);

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
    UINT32 testswrst();

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
    void CorruptRegisters(UINT32 pciDomainNum, UINT32 pciBusNum, UINT32 pciDevNum, UINT32 pciFuncNum);
    bool CheckRegisterReset(UINT32 pciDomainNum, UINT32 pciBusNum, UINT32 pciDevNum, UINT32 pciFuncNum);
    bool CheckRegisterNotReset(UINT32 pciDomainNum, UINT32 pciBusNum, UINT32 pciDevNum, UINT32 pciFuncNum);
    void hot_reset(void);
    UINT32 gpu_initialize(void);

    IRegisterMap*                   m_regMap;
    unique_ptr<IRegisterClass> m_reg;
    UINT32 m_initVal;
    UINT32 m_initMask;

//    bool testhotrst1_flag ;
//    bool testhotrst2_flag ;
//    bool testhotrst3_flag ;
//    bool testhotrst4_flag ;

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(hotrst_reinit, HotRstReInit, "Gt21x+ Hot Reset ReInit Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &hotrst_reinit_testentry
#endif

#endif  // _HOT_RST_REINIT_H_
