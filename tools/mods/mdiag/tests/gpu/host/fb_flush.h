/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008, 2019, 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _FB_FLUSH_H
#define _FB_FLUSH_H

//#include "mdiag/utils/buf.h"
#include "mdiag/utils/types.h"

#define GOB_WIDTH 64
#define MAX_RANDOM_NUMBER_VALUE 255
#define DEFAULT_SURFACE_WIDTH 256
#define DEFAULT_SURFACE_HEIGHT 16
#define SEMAPHORE_RELEASE_VALUE 0x1234
#define SEMAPHORE_SIZE 16

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine

#include "mdiag/tests/gpu/lwgpu_single.h"

class fb_flushTest : public LWGpuSingleChannelTest
{
public:
    fb_flushTest(ArgReader *params);

    virtual ~fb_flushTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    void WriteSrc(void);
    int  ReadDst(void);

protected:
    UINT32 dstHeight, dstPitch;
    UINT32 surfaceWidth;
    UINT32 surfaceHeight;
    UINT32 surfaceSize;
    UINT32 subChannel;
    RandomStream *RndStream;

    int local_status;

    ArgReader *params;

    UINT32 seed0,seed1,seed2;
    UINT32 loopCount;
    bool   useSysMem;

    DmaBuffer srcBuffer;
    DmaBuffer dstBuffer;
    DmaBuffer semaphoreBuffer;

    UINT32 copyHandle;
    UINT32 copyClassToUse;

    unsigned char *surfaceBuffer;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(fb_flush, fb_flushTest, "Verifies gart by accessing AGP memory through the aperture from CPU and GPU + direct access to sysmem");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &fb_flush_testentry
#endif

#endif
