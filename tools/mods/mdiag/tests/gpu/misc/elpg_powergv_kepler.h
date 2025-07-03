/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2017, 2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _ELPG_POWERGV_KEPLER_H_
#define _ELPG_POWERGV_KEPLER_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/utils/randstrm.h"

#define LW_PBUS_DEBUG_1 0x00001084

class Elpg_Powergv_Kepler : public LWGpuSingleChannelTest {
public:

    Elpg_Powergv_Kepler(ArgReader *params);

    virtual ~Elpg_Powergv_Kepler(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
  UINT32 elpg_powergv_register_Test();
  UINT32 elpg_powergv_railgating_Test();
  UINT32 elpg_powergv_gx_maxwellTest();
  UINT32 elpg_powergv_gx_keplerTest();
  UINT32 elpg_powergv_vd_keplerTest();
  UINT32 elpg_powergv_ms_keplerTest();
  UINT32 elpg_powergv_fspg_keplerTest();
  UINT32 elpg_pgob_Test();
  UINT32 bbox_verify_Test();
  void VGA_INDEXED_WRITE(UINT32 addr, UINT32 data);
  int ChkRegVal2(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
  int ChkRegVal3(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
  int WriteReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
  int ReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
  UINT32 Validate_PG_Reg(UINT32 data);
  UINT32 Validate_slave_PG_Reg(UINT32 data);
  UINT32 Validate_PG1_Reg(UINT32 data);
  const char* ELPG_enabled(UINT32 data);
  void PrivInit(void);
  UINT32 check_clamp_sleep_gr(bool fuse_blown);
  UINT32 check_clamp_sleep_vd(bool fuse_blown);
  void WriteBBOXInputs(const char Input[]);
  void DelayUsingDummyReads();

  int fs_control;
  int fs_control_gpc;
  int fs_control_gpc_tpc;
  int fs_control_gpc_zlwll;
  int fspg_option;
  int register_check;
  int centralized_elpg;
  int fspg_enable_sm0_only;
  int fspg_enable_sm1_only;
  int register_test;
  int randomize_blg_regs;
  int seed_1;
  int pgob;
  int railgating;
  int gxpg;
  int vdpg;
  int mspg;
  int skip_MMUBind;
  int bbox_verify;

  int WriteGateCtrl(int index, UINT32 newFieldValue);
  int WrRegArray(int index, const char *regName, UINT32 data);
  UINT32 RdRegArray(int index, const char *regName);

  RandomStream*                   m_rndStream;

  int programFuse(UINT32 addr, UINT32 data, UINT32 mask);
  int senseFuse(void);
  void Enable_fuse_program(void);
  void updateFuseopt(void);

  void Wait(int count);

  void PollMMUFifoEmpty(GpuSubdevice *pSubdev);
  void MMUBindGR(int unbind, GpuSubdevice *pSubdev);
  void MMUIlwalidate(GpuSubdevice *pSubdev);

  void writeFusedata(UINT32 addr, UINT32 mask, UINT32 val,UINT32 &fusedata0,UINT32 &fusedata1,UINT32 &fusedata2,UINT32 &fusedata3,UINT32 &fusedata4,UINT32 &fusedata5,UINT32 &fusedata6,UINT32 &fusedata7,UINT32 &fusedata8,UINT32 &fusedata9,UINT32 &fusedata10,UINT32 &fusedata11,UINT32 &fusedata12,UINT32 &fusedata13,UINT32 &fusedata14,UINT32 &fusedata15,UINT32 &fusedata16,UINT32 &fusedata17,UINT32 &fusedata18,UINT32 &fusedata19,UINT32 &fusedata20,UINT32 &fusedata21,UINT32 &fusedata22,UINT32 &fusedata23,UINT32 &fusedata24,UINT32 &fusedata25,UINT32 &fusedata26,UINT32 &fusedata27);

private:
  IRegisterMap* m_regMap;
  ArgReader *params;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(elpg_powergv_kepler, Elpg_Powergv_Kepler, "GPU Elpg_Powergv_Kepler Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &elpg_powergv_kepler_testentry
#endif

#endif  // _ELPG_POWERGV_KEPLER_H_
