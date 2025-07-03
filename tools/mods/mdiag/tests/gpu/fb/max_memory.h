/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _MAX_MEMORY_H
#define _MAX_MEMORY_H

#include "mdiag/utils/types.h"

#define GOB_WIDTH 64
#define MAX_RANDOM_NUMBER_VALUE 255
#define DEFAULT_SURFACE_WIDTH 256
#define DEFAULT_SURFACE_HEIGHT 16
#define SEMAPHORE_RELEASE_VALUE 0x1234
#define SEMAPHORE_SIZE 16

#define MAX_TEST_BUFFERS 2
#define BURST_SIZE 128

#define SUB_CHANNEL 0

class RandomStream;

// very simple fb_flush test - write to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine
#include "mdiag/tests/gpu/lwgpu_single.h"

#include "fb_common.h"

class max_memoryTest : public FBTest
{
public:
    max_memoryTest(ArgReader *params);
    virtual ~max_memoryTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);
    virtual void CleanUp(void);

private:
//     RandomStream *RndStream;
    string m_pteKindName;

    int local_status;
    UINT32 error_count;

    ArgReader *params;

    UINT32 maxErrCount;

    UINT64 m_dramCap;
    UINT32 m_num_fbpas;
    UINT32 m_bom;

    UINT64 m_offset;
    UINT32 m_size;
    UINT32 m_stride;
    UINT32 m_access_mode;
    UINT32 m_page_size;
    UINT32 m_enable_print;
    UINT64 m_phys_addr;
    UINT32 m_phys_addr_percent;
    UINT32 m_phys_size;
    UINT32 m_target;

    DmaBuffer m_dmaBuffer;
    unsigned char *m_refBuff;
    unsigned char *m_backup;
    unsigned char *m_readBuff;

    Surface m_bigBuffer;
    Surface m_bufferLow;        // 4k page located at lower address
    Surface m_bufferHigh;       // 4k page located at higher address

    UINT32 m_blk_phys_rw_test_buff_size;

    void SingleRun();

    // Basic surface test:
    //   * read from bar1 / write to bar0 window
    //   * read from bar0 window / write to bar1
    void BasicBufferTest(DmaBuffer *buff, UINT32 stride);
    void MemBlkRdWrTest(UINT64 addr, UINT64, bool isRandom,
                        unsigned char *refBuff,
                        unsigned char *readBuff,
                        unsigned char *backupBuff);
    void MemBlkRdWrTest2(UINT64 addr, UINT64, bool isRandom);
    void MemRdWrTest32(UINT64 addr, bool isRandom, UINT32 data);

    void BasicBufferTestMode1(DmaBuffer *buff, UINT32 stride);
    void MemBlkRdWrTestMode0();
    void MemBlkRdWrTestMode2(UINT64 addr, UINT32 size);
    void MemBlkRdWrTestMode3(UINT64 addr, UINT32 size);

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(max_memory, max_memoryTest, "Test for fermi FB pa maximum memory size configurations.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &max_memory_testentry
#endif

#endif
