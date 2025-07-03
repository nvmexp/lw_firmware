/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2012-2013, 2019-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _FB_COMMON_H_
#define _FB_COMMON_H_

#include "mdiag/tests/loopingtest.h"
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"

#include "fermi/gf100/dev_bus.h"

#define fb_test_params GpuChannelHelper::Params

class ArgReader;
class LWGpuResource;
class LWGpuChannel;
class LWGpuSubChannel;
class GpuChannelHelper;

class FBHelper;

// subclasses from LoopingTest so '-forever' support is there (i.e. you can
//   call KeepLooping()), but you still have to add FOREVER_PARAM to your
//   ParamDecl list in your test

// A common test class provide many useful methods
class FBTest : public LoopingTest {
public:
    FBTest(ArgReader *params);
    virtual ~FBTest();

    virtual void DebugPrintArgs();

    virtual int Setup();

    // Methods from class Test
    virtual void CleanUp();

    virtual void Fail(Test::TestStatus err) {
        SetStatus(err);
        status = 0;
    }

    static ParamDecl fbtest_common_params[];

protected:
    int status;

    ArgReader *m_pParams;
    LWGpuResource *lwgpu;
    GpuSubdevice  *gpuSubDev;
    LWGpuChannel *ch;
    GpuChannelHelper *chHelper;
    RandomStream *RndStream;

    FBHelper *fbHelper;

    // Common test arguments
    UINT32 seed0,seed1,seed2;
    UINT32 loopCount;
    UINT32 rndTestCount;
    bool   useSysMem;

};

// a helper class for FB tests
class FBHelper {
public:
    FBHelper(FBTest *fbtest, LWGpuResource *lwgpu, GpuChannelHelper *chHelper);
    ~FBHelper();

    enum ENUM_PAGE_SIZE { PAGE_4K, PAGE_64K, PAGE_128K };

    int GetStatus() { return m_status; }
    void SetBackdoor(bool en);
    void Flush();

    unsigned ReadRegister(string regName);

    UINT32 PhysRd32(UINT64 addr);
    void PhysWr32(UINT64 addr, UINT32 data);

    void PhysRdBlk(UINT64 addr, UINT32 *data, UINT32 size);
    void PhysWrBlk(UINT64 addr, const UINT32 *data, UINT32 size);

    UINT64 SetBar0Window(UINT64 addr, UINT32 target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM);

    // Physical read/write to BAR0 WINDOW
    UINT32 PhysRd32_Bar0Window(UINT64 addr, UINT32 target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM);
    void PhysWr32_Bar0Window(UINT64 addr, UINT32 data,
                             UINT32 target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM);

    void PhysRdBlk_Bar0Window(UINT64 addr, unsigned char *data, UINT32 size,
                              UINT32 target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM);
    void PhysWrBlk_Bar0Window(UINT64 addr, const unsigned char *data, UINT32 size,
                              UINT32 target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM);

    UINT64 FBSize();
    void SetupSurface(Surface *buffer, UINT32 width, UINT32 height);
    void SetupSurface(Surface *buffer, UINT32 width, UINT32 height, UINT32 align, UINT32 page_size);
    void FreeSurface(Surface *buffer);

    // _PAGE_LAYOUT PageSize();
    UINT32 NumFBPs();
    UINT32 NumFBPAs();
    UINT32 NumL2Slices();
    //UINT32 NumRows();
    //UINT32 NumIntBanks();
    //UINT32 NumExtBanks();
    //UINT32 NumCols();

private:
    FBTest *m_fbtest;

    LWGpuResource *m_lwgpu;
    GpuSubdevice  *m_gpuSubDev;

    int m_status;
    bool m_gen5Capable;
};

class Mem {
public:
    Mem(void);
    virtual ~Mem(void);
    uintptr_t GetVBase() { return m_vbase; }
    UINT64 GetBase() { return m_base; }
    UINT32 GetSize() { return m_size; }
    UINT32 GetValid() { return m_valid;}
    void SetBase(UINT32 m) { m_base = m;}
    void SetVBase(uintptr_t v) { m_vbase = v;}
    void SetSize(UINT32 s) { m_size = s;}
    void SetValid(UINT32 i) { m_valid = i;}
    void Dump(void);
    int  Map(UINT64 base,UINT32 size);
    void UnMap(void);
private:
    uintptr_t m_vbase;
    UINT64 m_base;
    UINT32 m_size;
    UINT32 m_valid;
};

#endif
