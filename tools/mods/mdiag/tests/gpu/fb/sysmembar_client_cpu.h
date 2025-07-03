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

#ifndef _SYSMEMBAR_CLIENT_CPU_H
#define _SYSMEMBAR_CLIENT_CPU_H

#include "mdiag/utils/types.h"

#define SEMAPHORE_RELEASE_VALUE 0x1234
#define MY_SEMAPHORE_SIZE 32

#define DEFAULT_WIDTH           512             // Eqaul to pitch
#define DEFAULT_HEIGHT          512

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine

// Test using CE (gt212)

#include "mdiag/tests/gpu/lwgpu_single.h"
#include "gpu/include/gralloc.h"

class sysmembar_client_cpuTest : public LWGpuSingleChannelTest
{
public:
    sysmembar_client_cpuTest(ArgReader *params);

    virtual ~sysmembar_client_cpuTest(void);

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
    UINT32 subChannel;
    int local_status;

    ArgReader *params;

    DmaBuffer srcBuffer;
    DmaBuffer dstBuffer;
    DmaBuffer semaphoreBuffer;
    DmaBuffer semaphoreBuffer_src;

    UINT32 mem2memHandle;        // DMA copy class

    DmaCopyAlloc m_DmaCopyAlloc;

    unsigned char *surfaceBuffer;
    UINT32 noflush;

    UINT32 override_force_flush; UINT32 force_flush;
    UINT32 override_skip_xv0; UINT32 skip_xv0;
    UINT32 override_skip_xv1; UINT32 skip_xv1;
    UINT32 override_skip_peer;UINT32 skip_peer;

    void EnableBackdoor();
    void DisableBackdoor();

    int setup_copy1();
    int setup_copy2();

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(sysmembar_client_cpu, sysmembar_client_cpuTest, "Verifies gart by accessing AGP memory through the aperture from CPU and GPU + direct access to sysmem");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &sysmembar_client_cpu_testentry
#endif

#endif
