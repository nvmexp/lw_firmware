/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _MEMSYS_H_
#define _MEMSYS_H_

// Include files for MDiag defines.
#include "mdiag/tests/gpu/lwgpu_single.h"

// Include for handling Register read writes.
#include "regmap_helper.h"

class MemSys_Island : public LWGpuSingleChannelTest {
public:
    // Constructor-Deconstructor
    MemSys_Island(ArgReader *params);
    virtual ~MemSys_Island(void);

    static Test *Factory ( ArgDatabase *args); 

    // Test functions
    virtual int Setup(void);
    virtual void Run(void);
    virtual void CleanUp(void);

protected:
    // Internal function declarations: Test Functions
    int MSI_Speedy_Test();
    int MSI_GPU_Init();
    int MSI_Entry();
    int MSI_Exit();

    // Internal function declarations: Helper Functions
    UINT32 getRunlistInfo(string field);
    bool isGPCIdle();
    int mmuBind(bool bind);
    int togglePrivBlocker(bool enable);

    // Local Variable declarations.
    bool idle_wake_up;
    bool pri_ring_old_init;
    bool speedy_test;

    string engineIdx;
    string engID;
    string idleMaskIdx;

    UINT32 engPriBase;
    UINT32 idle_mask[3];
    UINT32 idle_thres;
    UINT32 interPollWait;
    UINT32 maxPoll;
    UINT32 resetID;

private:
    IRegisterMap* m_regMap;
    regmap_helper* hReg;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(memsys_island, MemSys_Island, "GPU MEMSYS Island Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &memsys_island_testentry
#endif

#endif
