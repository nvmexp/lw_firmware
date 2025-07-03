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
#ifndef _NO_SWIZZLE_H
#define _NO_SWIZZLE_H

//#include "mdiag/utils/buf.h"
#include "mdiag/utils/types.h"

#define SUB_CHANNEL 0

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine

#include "fb_common.h"

#include "mdiag/tests/gpu/lwgpu_single.h"

class no_swizzleTest : public FBTest {
public:
    no_swizzleTest(ArgReader *params);
    virtual ~no_swizzleTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

protected:
    int local_status;

    ArgReader *m_params;
    DmaBuffer m_dmaBuffer;

    UINT32 seed0,seed1,seed2;
    UINT32 loopCount;
    bool   useSysMem;
    string m_pteKindName;

    //! Pointer to the gpu sub device we're manipulating
    GpuSubdevice *pGpuSub;

    DmaBuffer srcBuffer;
    DmaBuffer dstBuffer;
    DmaBuffer semaphoreBuffer;

    Surface bar1Buffer; //!< One buffer for bar 0, and 1

    unsigned char *surfaceBuffer;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(no_swizzle, no_swizzleTest, "Test by Fung");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &no_swizzle_testentry
#endif

#endif
