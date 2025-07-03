/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef __riscvdemo__STRESS_H
#define __riscvdemo__STRESS_H
#include <lwtypes.h>
#include <stddef.h>

struct lz4CompressionTestConfig
{
    LwS32 src_size;
    LwS32 dest_size;
    void *src; // source data
    void *src2; // destination compressed data.
    void *dest; // destination data
};
int lz4CompressionTest(void *cfg);

void lfsrTest(unsigned size, LwUPtr d, LwBool print);

void initCrcTable(void);
LwU32 crc32(LwUPtr data, int length, LwU32 crc_in);
LwU32 slow_crc32(LwUPtr data, int length, LwU32 crc_in);
void verify(LwUPtr d, int length, LwU32 expectedCRC);
int pipelinedMulTest(void *unused);
int branchPredictorStressTest(LwU64 iterations);
int branchPredictorMultiplyStressTest(LwU64 iterations);
int callwlatePiAndVerify(unsigned int ref);

void fastAlignedMemSet(LwUPtr data, LwU8 valU8, int size);
void fastAlignedMemCopy(LwU64 *dest, LwU64 *src, LwU64 size);
void LZ4_malloc_init(void *heap_ptr, LwU64 size);

#endif
