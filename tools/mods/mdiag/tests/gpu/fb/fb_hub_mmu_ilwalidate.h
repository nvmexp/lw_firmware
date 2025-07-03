/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _HUB_MMU_ILWALIDATE_H
#define _HUB_MMU_ILWALIDATE_H

#include "mdiag/utils/types.h"

#define SUB_CHANNEL 0

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine

// Test using CE (gt212)

#include "mdiag/tests/gpu/lwgpu_single.h"

class fb_hub_mmu_ilwalidateTest : public LWGpuSingleChannelTest
{
public:
    fb_hub_mmu_ilwalidateTest(ArgReader *params);

    virtual ~fb_hub_mmu_ilwalidateTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    void WriteSrc(void);
    int  ReadDst(void);

protected:
    int subChannel;
    int local_status;

    ArgReader *params;

    DmaBuffer surfA;
    DmaBuffer surfB;
    bool no_ilwalidate;
    bool ilwal_coh;

    void EnableBackdoor();
    void DisableBackdoor();

    UINT64 m_gpu_addrA_old;
    UINT64 m_gpu_addrB_old;
    UINT64 m_gpu_addrA_new;
    UINT64 m_gpu_addrB_new;

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(fb_hub_mmu_ilwalidate, fb_hub_mmu_ilwalidateTest, "Verifies fb_hub_mmu_ilwalidate_only flag");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &fb_hub_mmu_ilwalidate_testentry
#endif

#endif
