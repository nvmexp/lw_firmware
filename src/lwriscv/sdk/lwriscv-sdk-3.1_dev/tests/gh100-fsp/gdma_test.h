#ifndef GDMA_TEST_H
#define GDMA_TEST_H

#include <stddef.h>
#include <liblwriscv/gdma.h>
#include <liblwriscv/fbif.h>
#include <liblwriscv/print.h>

#define BUF_SIZE 0x200
#define ENABLE_GDMA_PRI_XFER 0

void copyToPri(uint32_t* dest);
void copyFromPri(uint32_t* src);
void createRefBuf(GDMA_ADDR_CFG* cfg, volatile uint8_t* src, volatile uint8_t* dest, uint32_t length);
int dbg_memcmp(const void* src1, const void* src2, unsigned size);
void initBuffers(void);
int gdma_test(void);

#endif // end GDMA_TEST_H
