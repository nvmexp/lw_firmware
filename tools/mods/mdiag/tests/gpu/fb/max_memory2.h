/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2011,2013, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _MAX_MEMORY2_H
#define _MAX_MEMORY2_H

#include "mdiag/utils/types.h"

#define GOB_WIDTH 64
#define MAX_RANDOM_NUMBER_VALUE 255
#define DEFAULT_SURFACE_WIDTH 256
#define DEFAULT_SURFACE_HEIGHT 16
#define SEMAPHORE_RELEASE_VALUE 0x1234
#define SEMAPHORE_SIZE 16

#define MAX_TEST_BUFFERS 2
#define BURST_SIZE 128

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine
#include "mdiag/tests/gpu/lwgpu_single.h"

#include "fb_common.h"
#include "gpu/include/gralloc.h"

class Surface2D;

class max_memory2Test : public FBTest
{
public:
    max_memory2Test(ArgReader *params);
    virtual ~max_memory2Test(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);
    virtual void CleanUp(void);

protected:
    UINT32 subChannel;

    int TestVidMem();
    int TestSysMem();
    int TestP2pMem();

private:
//     RandomStream *RndStream;
    string m_pteKindName;

    int local_status;

    ArgReader *params;

    UINT64 m_dramCap;
    UINT32 m_num_fbpas;

    UINT32 m_size;
    UINT64 m_phys_addr;
    UINT32 m_target;
    bool m_bLoopback;
    bool m_bDoCpuWrite;
    bool m_bCachable;

    DmaBuffer m_dmaBuffer;

    DmaCopyAlloc m_DmaCopyAlloc;

    int CreateLoopbackSurface(Surface2D& buf, UINT64 size);
    int DoCopyEngineTest(UINT64 offset1);
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(max_memory2, max_memory2Test, "Test for fermi FB pa maximum memory size configurations.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &max_memory2_testentry
#endif

#endif
