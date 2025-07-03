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
#ifndef _HOST_ALL_SIZE_H
#define _HOST_ALL_SIZE_H

#include "mdiag/utils/types.h"

#define GOB_WIDTH 64
#define MAX_RANDOM_NUMBER_VALUE 255
#define DEFAULT_SURFACE_WIDTH 256
#define DEFAULT_SURFACE_HEIGHT 16
#define SEMAPHORE_RELEASE_VALUE 0x1234
#define SEMAPHORE_SIZE 16

#define SUB_CHANNEL 0

class RandomStream;

#include "fb_common.h"

class host_all_size : public FBTest
{
public:
    host_all_size(ArgReader *params);

    virtual ~host_all_size(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    int FBFlush();

    void EnableBackdoor();
    void DisableBackdoor();

    // Basic test routine
    void TestMemoryRW(int lwrLoop);

protected:
    string m_pteKindName;

    int local_status;

    ArgReader *params;

    UINT64 m_offset;
    UINT32 m_size;
    UINT64 m_phys_addr;

    //! Pointer to the gpu sub device we're manipulating
    DmaBuffer m_testBuffer;

    UINT32 *m_testData;
    UINT32 *m_testRdData;
    unsigned char *m_refData;

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(host_all_size, host_all_size, "FB NISO HUB: host all size requests.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &host_all_size_testentry
#endif

#endif
