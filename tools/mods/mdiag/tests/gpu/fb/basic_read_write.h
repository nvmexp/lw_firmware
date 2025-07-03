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
#ifndef _BASIC_READ_WRITE_H
#define _BASIC_READ_WRITE_H

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

class basic_read_writeTest : public FBTest
{
public:
    basic_read_writeTest(ArgReader *params);

    virtual ~basic_read_writeTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    virtual void CleanUp(void);

    int FBFlush();

    void EnableBackdoor();
    void DisableBackdoor();

    void ReadSurface();
    void WriteSurface();

    // Basic test routine
    void TestMemoryRW();

protected:
    RandomStream *RndStream;
    bool m_frontdoor_check;
    string m_pteKindName;

    int local_status;

    ArgReader *params;

    UINT64 m_offset;
    UINT32 m_size;
    UINT64 m_phys_addr;

    //! Pointer to the gpu sub device we're manipulating
    DmaBuffer m_testBuffer;

    unsigned char *m_testData;
    unsigned char *m_refData;

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(basic_read_write, basic_read_writeTest, "FB PA: Basic reads and writes.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &basic_read_write_testentry
#endif

#endif
