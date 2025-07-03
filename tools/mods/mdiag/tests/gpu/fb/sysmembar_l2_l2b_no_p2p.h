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

#ifndef _SYSMEMBAR_L2_L2B_NO_P2P_H
#define _SYSMEMBAR_L2_L2B_NO_P2P_H

#include "mdiag/utils/types.h"

#define MAGIC_VALUE 0x12345678

#define DEFAULT_WIDTH           512             // Eqaul to pitch
#define DEFAULT_HEIGHT          512

#define SUB_CHANNEL 0

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine

// Test using CE (gt212)

#include "mdiag/tests/gpu/lwgpu_single.h"
#include "gpu/include/gralloc.h"

class sysmembar_l2_l2b_no_p2pTest : public LWGpuSingleChannelTest
{
public:
    sysmembar_l2_l2b_no_p2pTest(ArgReader *params);

    virtual ~sysmembar_l2_l2b_no_p2pTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    void WriteSrc(void);
    int  ReadDst(void);

protected:
    UINT32 pitch, height;
    UINT32 surfaceSize;
    int subChannel;
    int local_status;

    ArgReader *params;

    DmaBuffer srcBuffer;
    DmaBuffer dstBuffer;
    DmaBuffer dstBuffer2;

    UINT32 mem2memHandle;        // DMA copy class

    DmaCopyAlloc m_DmaCopyAlloc;

    unsigned char *surfaceBuffer;
    UINT32 noflush;

    void EnableBackdoor();
    void DisableBackdoor();

    int setup_copy1();
    int setup_copy2();

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(sysmembar_l2_l2b_no_p2p, sysmembar_l2_l2b_no_p2pTest, "Verifies gart by accessing AGP memory through the aperture from CPU and GPU + direct access to sysmem");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &sysmembar_l2_l2b_no_p2p_testentry
#endif

#endif
