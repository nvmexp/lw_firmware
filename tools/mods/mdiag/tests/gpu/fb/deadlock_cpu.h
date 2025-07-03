/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _DEADLOCK_CPU_H_
#define _DEADLOCK_CPU_H_

#include "mdiag/utils/types.h"

#define GOB_WIDTH 64
#define MAX_RANDOM_NUMBER_VALUE 255
#define DEFAULT_SURFACE_WIDTH 256
#define DEFAULT_SURFACE_HEIGHT 16
#define SEMAPHORE_RELEASE_VALUE 0x1234
#define SEMAPHORE_SIZE 16

#define SUB_CHANNEL 0

#include "fb_common.h"

class LWGpuChannel;

class deadlock_cpuTest : public FBTest
{
public:
    deadlock_cpuTest(ArgReader *params);

    virtual ~deadlock_cpuTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    int FBFlush();
    int IlwalidTLB(bool inband);

    void EnableBackdoor();
    void DisableBackdoor();

    // Basic test routine
    void TestMemoryRW();

protected:
    UINT32 m_writeNum;
    bool m_inband;
    UINT32 m_size;
    int local_status;
    ArgReader *params;

    //! Pointer to the gpu sub device we're manipulating
    DmaBuffer m_testBuffer;

    LWGpuChannel* m_pGpuChannel;
    UINT32 m_hClassFermi;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(deadlock_cpu, deadlock_cpuTest, "FB PA: test for a simple deadlock case of TLB.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &deadlock_cpu_testentry
#endif

#endif
