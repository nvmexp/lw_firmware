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

#ifndef _PARTITION_WHITE_BOX_H_
#define _PARTITION_WHITE_BOX_H_

#include "mdiag/utils/types.h"

#include "fb_common.h"

// this test will become obsolete.
class partition_white_boxTest : public FBTest {
public:
    partition_white_boxTest(ArgReader *params);
    virtual ~partition_white_boxTest();

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);
    virtual void CleanUp(void);

private:
    int local_status;
    UINT64 m_dramCap;
    UINT32 m_num_partitions;
    UINT32 m_num_subpartitions;
    UINT32 m_num_slices;
    UINT32 m_num_rows;
    UINT32 m_num_banks;

    UINT32 m_size;

    DmaBuffer m_nullBuffer;     // place holder
    DmaBuffer m_dmaBuffer;

    unsigned char *m_refData;
    unsigned char *m_bufData;
    UINT64 m_dmaBufBegin;
    UINT64 m_dmaBufEnd;

    UINT32 m_run_test;

    bool m_frontdoor_check;
    bool m_frontdoor_init;
    string m_pteKindName;

    int CompareBuffer(unsigned char *src, unsigned char *dst, UINT32 size);

    typedef bool (*Cond_Func_t)(UINT32 part_v, UINT32 subpart_v, UINT32 row_v, UINT32 bank_v,
                                UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank);

    int FindPhysAddrByRowBank(Cond_Func_t condition, UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank,
                              UINT64 start, UINT64 range, UINT64 *addr);
    // given a row-bank return a physical address to that row bank
    int RowBank2PhysAddr(UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank,
                         UINT64 vidMemOffset, UINT64 range, UINT64 *addr);
    int FindPhysAddrByFT(bool rd, UINT32 ftSet, UINT64 start, UINT64 range, UINT64 *addr);

    void InitBuffers();

    void Test_ReadWriteBuffer(bool rd, UINT64 offset, UINT64 size, UINT32 tag = 0);

    // Create streaming r/w requests to a given row-bank(says all requests go in one dram page)
    void Test_ReqsToSameRowBank();
    void Test_ReqsToDiffRowBank2();

    // Create streaming r/w requests to different row-bank(says in the same dram page)
    void Test_ReqsToDiffRowBank();

    void Test_ReqsToSameFT();

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(partition_white_box, partition_white_boxTest,
                       "Testing interface between L2 and FB: stresses read and write varying sizes, sequences.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &partition_white_box_testentry
#endif

#endif
