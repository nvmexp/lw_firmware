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
#ifndef _SEMAPHORE_BASHING_H
#define _SEMAPHORE_BASHING_H

//#include "mdiag/utils/buf.h"
#include "mdiag/utils/types.h"

#define GOB_WIDTH 64
#define MAX_RANDOM_NUMBER_VALUE 255
#define SEM_BASH_DEFAULT_SURFACE_WIDTH 64
#define SEM_BASH_DEFAULT_SURFACE_HEIGHT 8
#define SEMAPHORE_SIZE 16

class RandomStream;

// very simple semaphore_bashing test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine

#include "mdiag/tests/gpu/lwgpu_single.h"

class semaphore_bashingTest : public LWGpuSingleChannelTest
{
public:
    semaphore_bashingTest(ArgReader *params);

    virtual ~semaphore_bashingTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    void WriteSrc(uintptr_t srcAddr);
    int  ReadDst(uintptr_t dstAddr);
    void EnableBackdoor(void);
    void DisableBackdoor(void);

protected:
    enum SemaphoreLocation { semVidMem = 0, semSysMem, semNumLocations };
    enum SemaphorePayloadSize     { sem32bit = 0, sem64bit, semNumSizes };
    enum SemaphoreTimestampEn     { semTimestampDis = 0, semTimestampEn, semNumTimestampModes };
    enum SemaphoreAcqRel   { semAcqCpuRelCpu = 0, semAcqCpuRelGpu, semAcqGpuRelCpu, semAcqGpuRelGpu, semNumAcqRel };
    enum TransferSrcDst    { trVidVid = 0, trVidSys, trSysVid, trSysSys, trNumCombos };
    enum SemaphoreAcqType  { semAcqTypeEq = 0, semAcqTypeStrictGeq, semAcqTypeCircGeq, semAcqTypeAnd, semAcqTypeNor, semNumAcqTypes };
    UINT32 dstHeight, dstPitch;
    UINT32 surfaceWidth;
    UINT32 surfaceHeight;
    UINT32 surfaceSize;
    bool   surfaceCheck;
    UINT32 subChannel;
    RandomStream *RndStream;

    int local_status;

    ArgReader *params;

    UINT32 seed0,seed1,seed2;
    UINT32 loopCount;
    bool   useSysMem;

    DmaBuffer dmaBuffer[2];
    DmaBuffer sysmemSemaphoreBuffer;
    DmaBuffer vidmemSemaphoreBuffer;

    UINT32 copyHandle;
    UINT32 copyClassToUse;

    unsigned char *surfaceBuffer;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(semaphore_bashing, semaphore_bashingTest, "Verifies gart by accessing AGP memory through the aperture from CPU and GPU + direct access to sysmem");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &semaphore_bashing_testentry
#endif

#endif
