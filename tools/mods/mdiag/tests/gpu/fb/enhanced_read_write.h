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

#ifndef _ENHANCED_READ_WRITE_H_
#define _ENHANCED_READ_WRITE_H_

#include "mdiag/utils/types.h"

#include "fb_common.h"

class enhanced_read_writeTest : public FBTest {
public:
    enhanced_read_writeTest(ArgReader *params);
    virtual ~enhanced_read_writeTest();

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);
    virtual void CleanUp(void);

private:
    int local_status;

    UINT64 m_dramCap;
    UINT32 m_num_fbpas;

    DmaBuffer m_nullBuffer;     // Place holder?
    DmaBuffer m_testBuffer;
    UINT64 m_offset;
    UINT32 m_size;
    string m_pteKindName;

    bool m_frontdoor_check, m_frontdoor_init;

    unsigned char *m_refData;
    unsigned char *m_readData;

    void InitBuffer();
    void SelfCheck();

// 4)   Read various sizes (32-128 bytes)
// 5)   Writes of various sizes (1-128 bytes)
    void StressReadWriteTest(UINT64 addr, UINT32 size);

// 12)  Make tests to see that there are no read write conflicts
// I'd like to implement it in test partition_white_box
// It's hard to do CPU r/w simultaneously in a mdiag test.

// 13)  Cleans and dirties to the same line(partition, slice, cache line)
    void DirtyCleanTest();

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(enhanced_read_write, enhanced_read_writeTest,
                       "Testing interface between L2 and FB: stresses read and write varying sizes, sequences.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &enhanced_read_write_testentry
#endif

#endif  // _ENHANCED_READ_WRITE_H_
