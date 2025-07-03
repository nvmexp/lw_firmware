/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

// stabilitytest.c

#include <stddef.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>

#include "utils.h"

//Setup LZ4 with 20KiB DTCM usage.
#define LZ4_MEMORY_USAGE 14
// rest of defines hardcoded in lz4.c
/*
#define LZ4_HEAPMODE 1
#define LZ4_USER_MEMORY_FUNCTIONS
#define LZ4_FORCE_MEMORY_ACCESS 1
#define LZ4_FORCE_SW_BITCOUNT
*/
#include "lz4.h"

#include "stress.h"

static LwU32 crc32_slice4_table[256 * 4] __attribute__((section(".crc_table")));
extern LwU64 g_fbOffset;
#define CRC_POLY 0xEDB88320

__attribute__((noinline)) int callwlatePiAndVerify(unsigned int ref)
{
    float reference = *((float*)&ref); // this is pure evil
    float n = 4.0f;
    float d = -1.0f;
    float d_int = 2.0f;
    float aclwm = 0.0f;
    float val;
    int i = 0;
    while (1) //FDIV FADD (and sometimes FSUB, depends on compiler)
    {
        d += d_int;
        val = n / d;
        aclwm += val;
        if (reference == aclwm)
            return i;
        i++;
        d += d_int;
        val = n / d;
        aclwm -= val;
        if (reference == aclwm)
            return i;
        i++;
    }
    // Will never reach here.
    // If you pass in a bad reference value the loop will never end.
}

__attribute__((noinline)) int lz4CompressionTest(void *param)
{
    struct lz4CompressionTestConfig *cfg = (struct lz4CompressionTestConfig *)param;
    LwU32 crc, crc2;
    char *buf, *out;
    int size = 0, o_size = 0;
    buf = (char *)cfg->src;
    crc = crc32((LwUPtr)buf, (LwU64)cfg->src_size, 0);

    out = (char *)cfg->dest;
//     printf("LZ4_compress(buf=%x out=%x srcSize=%u destSize=%u)\n", buf, out, cfg->src_size, cfg->dest_size);
    o_size = LZ4_compress_default(buf, out, cfg->src_size, cfg->dest_size);
//     printf("LZ4 compress %u -> %u\n", cfg->src_size, o_size);

    fastAlignedMemSet((LwUPtr)buf, 0xa5, cfg->src_size);

    // Note that you cannot decompress into ITCM on GA100 because of the HW bug.
    size = LZ4_decompress_safe(out, cfg->src2, o_size, cfg->src_size);
//     printf("LZ4 decompress %u -> %u (should be %u)\n", o_size, size, cfg->src_size);
    crc2 = crc32((LwUPtr)cfg->src2, size, 0);
//     printf("CRC src: %x decomp: %x\n", crc, crc2);
    if (crc != crc2)
    {
        return 1;
#if 0
        LwU32 *buf32 = (LwU32*)cfg->src2;//cfg->src;
        LwU32 *buff32 =(LwU32*)g_fbOffset + 0x2000;
        LwU64 mismatch_cnt = 0;
        for (int i=0; i<cfg->src_size/4; i++)
        {
            if (buf32[i] != buff32[i])
            {
                // printf("MISMATCH[%04x] %x %x\n", i*4, buf32[i], buff32[i]);
                mismatch_cnt++;
            }
        }
        if (mismatch_cnt)
        {
            printf("LZ4: %llu mismatches found!\n", mismatch_cnt);
            return 1;
        }
#endif
    }
    return 0;
}

__attribute__((noinline)) int pipelinedMulTest(void *unused)
{
    //Pipelined MUL (ie. MUL+MULH)
    LwU64 A = 3;
    LwU64 B = 1;
    LwU64 C = 5;
    LwU64 D = 7;
    LwU64 E = 11;
    LwU64 x0, x1;
    __asm__ __volatile__
    (
        "mv a0, %[A]\n"
        "mv a1, %[B]\n"
        "mv a3, %[C]\n"
        "mv a5, %[D]\n"
        "mv a7, %[E]\n"

        "lui s3, 0x10\n" // 0x10000;

        "rdcycle s4\n"
        // dependency-free chain depends on pipeline length.
        // Dependency chain is 7 instructions, which is larger than LWRISCV pipeline.
        // a0    H   L
        // a1 -> t0, t1
        // a3 -> t2, t3
        // a5 -> t4, t5
        // a7 -> t6, s1
        // t1 -> a2, a1
        // t3 -> a4, a3
        // t5 -> a6, a5
        // s1 -> s2, a7
        // a0 : 3
        // s3 : counter
        "1:\n"
        "mulhu t0, a0, a1\n"
        "mul t1, a0, a1\n"
        "mulhu t2, a0, a3\n"
        "mul t3, a0, a3\n"
        "mulhu t4, a0, a5\n"
        "mul t5, a0, a5\n"
        "mulhu t6, a0, a7\n"
        "mul s1, a0, a7\n"
        "addi s3, s3, -1\n"
        "mulhu a2, a0, t1\n"
        "mul a1, a0, t1\n"
        "mulhu a4, a0, t3\n"
        "mul a3, a0, t3\n"
        "mulhu a6, a0, t5\n"
        "mul a5, a0, t5\n"
        "mulhu s2, a0, s1\n"
        "mul a7, a0, s1\n"
        "bnez s3, 1b\n"         // This loop takes around 26.5 cycles (?? what the heck?) to run.
        "rdcycle s5\n"
        "sub a0, s5, s4\n"
/*
Unconfirmed reference values
x0  = 0000000000000000 ra  = 0000000000000d9c sp  = 0000000000003bd0 gp  = 0000000000000000
tp  = 0000000000000000 t0  = 0000000000000001 t1  = 50e7261f0a2aaaab t2  = 0000000000000000
s0  = 0000000000000003 s1  = 79eea3556fd55559 a0  = 0000000001a00001 a1  = f2b5725d1e800001
a2  = 0000000000000000 a3  = bd8b3bd198800005 a4  = 0000000000000001 a5  = a2f6208bd5800007
a6  = 0000000000000000 a7  = 6dcbea004f80000b s2  = 0000000000000001 s3  = 0000000000000000
s4  = 000000000e52d60a s5  = 000000000ff2d60b s6  = 0000000000000001 s7  = 0000000000000005
s8  = 0000000000000007 s9  = 000000000000000b s10 = 0000000000002000 s11 = ffffffffaca76000
t3  = 9483be9b32d55557 t4  = 0000000000000000 t5  = 36520ad9472aaaad t6  = 0000000000000002
*/
        // "2:\n"
        // "j 2b\n"

        "xor a1, a1, a2\n"
        "xor a3, a3, a4\n"
        "xor a5, a5, a6\n"
        "xor a7, s2, a7\n"
        "xor t1, t0, t1\n"
        "xor t3, t2, t3\n"
        "xor t5, t4, t5\n"
        "xor t6, t6, s1\n"

        "xor a1, a1, a3\n"
        "xor a2, a5, a7\n"
        "xor a3, t1, t3\n"
        "xor a4, t5, t6\n"

        "xor a1, a1, a2\n"
        "xor a2, a3, a4\n"

        "xor a1, a1, a2\n"

        "mv %0, a0\n"
        "mv %1, a1\n"
        : "=r" (x0), "=r" (x1), [A] "+r" (A), [B] "+r" (B), [C] "+r" (C), [D] "+r" (D), [E] "+r" (E)
        :
        : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "s1", "s2", "s3", "s4", "s5", "cc"
    );
    if (x1 != 0x0beff1e0f0c00001ull)
        return -1;
    return 0;
}

// see https://lwbugswb.lwpu.com/LwBugs5/HWBug.aspx?bugid=2789594&cmtNo=7
__attribute__((noinline)) static LwU64 bp_loop2(LwU64 loopSz, LwU64 mask) // aka Branch Predictor Stress test
{
    LwU64 mraow = 0;
    __asm__ (
        "mv a4, %[loopSz]\n"
        "mv a2, %[mask]\n"
        "mv a0, a0\n"
        "mv a0, a0\n"
        "mv a0, a0\n"

        "nop\n" /* nop of doom */

        "rdcycle a1\n"
        "1:\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "beqz a4, 4f\n"
        "addi a4, a4, -1\n"

        "and a3, a4, a2\n"
        "nop\n"
        "nop\n"
        "beqz a3, 1b\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "j 1b\n"
        "4:\n"
        "rdcycle a0\n"

        "sub a0, a0, a1\n"
        "mv %0, a0\n"
        : "=r" (mraow)
        : [loopSz] "r" (loopSz), [mask] "r" (mask)
        : "a2", "a3", "a4", "cc", "ra"
    );
    return mraow;
}

__attribute__((noinline)) static LwU64 bp_mul_stress(LwU64 loopSz, LwU64 mask) // aka Branch Predictor Stress test
{
    LwU64 mraow = 0;
    LwU64 checkval;
    __asm__ (
        "mv a4, %[loopSz]\n"
        "mv a2, %[mask]\n"
        "li t6, 3\n"
        "li a5, 5\n"
        "li a7, 7\n"
        "li t5, 11\n"

        "rdcycle a1\n"
        "1:\n"
        "mulhu t1, t6, a7\n"
        "mul t0, t6, a7\n"
        "mulhu t3, t6, t5\n"
        "mul t2, t6, t5\n"
        // a7 -> t1, t0
        // t5 -> t3, t2
        // t0 -> a6, a7
        // t2 -> t4, t5
        // t5 -> a6, a5
        // s1 -> s2, a7

        "beqz a4, 4f\n"
        "addi a4, a4, -1\n"

        "xor a3, a4, t5\n"
        "xor a3, a3, a7\n"

        "and a3, a3, a2\n"
        "beqz a3, 1b\n"
        "mulhu a6, a5, t0\n"
        "mul a7, a5, t0\n"
        "mulhu t4, a5, t2\n"
        "mul t5, a5, t2\n"
        "j 1b\n"
        "4:\n"
        "xor a6, a6, a7\n"
        "xor t0, t0, t1\n"
        "xor t2, t2, t3\n"
        "xor t4, t4, t5\n"

        "xor a6, a6, t0\n"
        "xor t2, t2, t4\n"

        "xor a2, a6, t2\n"

        "rdcycle a0\n"

        "sub a0, a0, a1\n"
        "mv %0, a0\n"
        "mv %1, a2\n"
        : "=r" (mraow), "=r" (checkval)
        : [loopSz] "r" (loopSz), [mask] "r" (mask)
        : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "s1", "s2", "s3", "s4", "s5", "cc"
    );
    return checkval;
}

__attribute__((noinline)) int branchPredictorStressTest(LwU64 iterations) // expected failure mode is "hang"
{
    LwU64 a = 0; // force compiler to actually do something
    a += bp_loop2(iterations, 0x1); //50%
    a += bp_loop2(iterations, 0x3); //75%
    a += bp_loop2(iterations, 0x7); //87.5%
    a += bp_loop2(iterations, 0xf); //93.75%
    return !a;
}

__attribute__((noinline)) int branchPredictorMultiplyStressTest(LwU64 iterations)
{
    LwU64 a, b, c, d;
    a = bp_mul_stress(iterations, 0x1); //50%
    b = bp_mul_stress(iterations, 0x3); //75%
    c = bp_mul_stress(iterations, 0x7); //87.5%
    d = bp_mul_stress(iterations, 0xf); //93.75%
    if (iterations == 131072)
    {
        if ((a == 0x439ba3d87c800039ull) &&
            (b == 0x59008d8346c0003eull) &&
            (c == 0xbeae578158e0003aull) &&
            (d == 0xbf90e1a41010003lwll))
        return 0;
    }
    printf("bpmul FAILURE DETECTED\n");
    return -1;
    // printf("BPMul %u: %llx %llx %llx %llx\n", iterations, a, b, c, d);
    // return 0;
}
struct LFSR_DATA
{
    LwU32 size;
    LwU32 exponent;
} typedef LFSR_DATA;

static const LFSR_DATA lfsr_data[] =
{
    {0x20, 0x14},
    {0x800, 0x500},
    {0x4000, 0x3802},  // 14 | 13 | 12 | 2
    {0x7800, 0x6000},  // 15 | 14 - truncated LFSR
    {0x8000, 0x6000},  // 15 | 14
    {0x10000, 0xD008}, // 16 | 15 | 13 | 4
    {0x20000, 0x12000},// 17 | 14
    {0, 0}
};
#define LFSR_SPREADING_FACTOR 1

__attribute__((noinline)) void lfsrTest(unsigned size, LwUPtr d, LwBool print)
{
    void *dest = (void*)d;
    const char meow[] = "mraaaow!";
    unsigned match = (unsigned)-1;
    for (unsigned x=0;x<(sizeof(lfsr_data)/sizeof(LFSR_DATA));x++)
    {
        if (lfsr_data[x].size==size)
        {
            match = x;
            break;
        }
    }
    if (match==(unsigned)-1)
    {
        printf("lfsr size exponent not found for size %u\n",size);
        return;
    }
    register LwU64 exponent = lfsr_data[match].exponent;
    register LwU64 lfsr = (lfsr_data[match].size-1) & *(LwU64 *)meow;
    register LwU64 period = size;
    LwU64 pre, post;
    LwU64 preCyc, postCyc;
    pre = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    preCyc = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
    __asm__ __volatile__  (
        "mv a4, %[period]\n"
        "mv a2, %[exponent]\n"
        "mv a5, %[lfsr]\n"
        "mv a6, %[dest]\n"
        "lui s0, 0x110\n"
        "loopX:\n"
        "srli a3, a5, 1\n" //a3 = lfsr>>1
        "andi a5, a5, 1\n" //a5 = lfsr&1
        "neg a5, a5\n" //a5 = -(lfsr&1)
        "and a5, a5, a2\n" //lfsr exponent
        "xor a5, a5, a3\n" //lfsr value
        "slli a3, a5, 0x3\n"//lfsr -> ptr arith
        "add a3, s0, a3\n" //a3 contains address
        "sh a3, 0(a6)\n" // Store LFSR output to memory. original was NOP: "mv a0, a0\n"
        "addi a6, a6, 2\n" // increment memory ptr
        "addi a4, a4, -1\n"
        "bnez a4, loopX\n"
        "mv %0, a5"
        : "=r" (lfsr)
        : [lfsr] "r" (lfsr), [period] "r" (period), [exponent] "r" (exponent), [dest] "r" (dest)
        : "a2", "a3", "a4", "a5", "a6", "s0", "cc"
    );
    postCyc = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
    post = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    if (print)
        printf("%6lld us, %6lld cyc\n", ((post - pre) * 32) / 1000, postCyc-preCyc);
}


__attribute__((noinline)) void initCrcTable()
{
//    crc32_slice4_table = (LwU32 *)(void *) g_fbOffset + 0x20; // overwrite the space after the bootloader
    for (LwU32 i=0; i<256; i++)
    {
        LwU32 crc = i;
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc = (crc >> 1) ^ ((crc & 1) * CRC_POLY);
        crc32_slice4_table[i] = crc;
    }
    for (LwU64 i=0; i<256; i++)
        for (LwU64 s=1; s<4; s++)
            crc32_slice4_table[256*s + i] = (crc32_slice4_table[256*(s-1) + i] >> 8) ^ crc32_slice4_table[crc32_slice4_table[256*(s-1) + i] & 0xff];
}
// slice by 4 algorithm, should be approximately the fastest on riscv
__attribute__((noinline)) LwU32 crc32(LwUPtr data, int length, LwU32 crc_in)
{
    LwU32 *data_asU32 = (LwU32 *)data;
    LwU8 *data_asU8 = (LwU8 *)data;

    LwU32 crc = ~crc_in;
    while (length >= 4)
    {
        LwU32 val = *data_asU32++ ^ crc;
        crc =   crc32_slice4_table[256*0 + ((val>>24) & 0xff)] ^
                crc32_slice4_table[256*1 + ((val>>16) & 0xff)] ^
                crc32_slice4_table[256*2 + ((val>> 8) & 0xff)] ^
                crc32_slice4_table[256*3 + ((val    ) & 0xff)];
        length -= 4;
    }
    data_asU8 = (LwU8 *)data_asU32;
    while (length > 0)
    {
        LwU8 val = *data_asU8++;
        crc = (crc >> 8) ^ crc32_slice4_table[/*256*0 +*/ val ^ (crc & 0xff)];
        length--;
    }
    return ~crc;
}
__attribute__((noinline)) LwU32 slow_crc32(LwUPtr data, int length, LwU32 crc_in)
{
    LwU8 *data_asU8 = (LwU8 *)data;

    LwU32 crc = ~crc_in;
    while (length > 0)
    {
        LwU8 val = *data_asU8++;
        crc = (crc >> 8) ^ crc32_slice4_table[/*256*0 +*/ val ^ (crc & 0xff)];
        length--;
    }
    return ~crc;
}
// We caught hardware bug in this function!
__attribute__((noinline)) void verify(LwUPtr d, int length, LwU32 expectedCRC)
{
    void *data = (void*)d;
    LwU32 crc[2] = {0,};
    crc[0] = crc32((LwUPtr)data, length, 0);
    crc[1] = slow_crc32((LwUPtr)data, length, 0);
    for (LwU64 i=0; i<2; i++)
    {
        if (crc[i] != expectedCRC)
        {
            printf("CRC[%u] Miscompare: %x (Expected %x)\n", i, crc[i], expectedCRC);
            ICD_HALT
        }
    }
}
void fastAlignedMemSet(LwUPtr data, LwU8 valU8, int size)
{
    LwU64 valU64 = ((LwU64)valU8 << 56 |
                    (LwU64)valU8 << 48 |
                    (LwU64)valU8 << 40 |
                    (LwU64)valU8 << 32 |
                    (LwU64)valU8 << 24 |
                    (LwU64)valU8 << 16 |
                    (LwU64)valU8 <<  8 |
                    (LwU64)valU8);
    LwU64 *data_asU64 = (LwU64 *)data;
    if ((size & (8-1)) || ((LwU64)data & (8-1)))
    {
        ICD_HALT
    }
    for (int i=0; i<(size/8); i++)
        data_asU64[i] = valU64;
    return;
}

void fastAlignedMemCopy(LwU64 *dest, LwU64 *src, LwU64 size)
{
    for (LwU64 i=0; i<(size/8); i++)
        dest[i] = src[i];
}

// World's fastest malloc routine.
static size_t malloc_managed_memory_size = 0;
static void *malloc_managed_memory_ptr = (void*)0xa77acca7; //make sure it breaks
static LwU8 malloc_managed_memory_used = 1;
void LZ4_malloc_init(void *heap_ptr, LwU64 size)
{
    malloc_managed_memory_size = size;
    malloc_managed_memory_ptr = heap_ptr;
    malloc_managed_memory_used = 0;
}
void* LZ4_malloc(size_t s)
{
    if ((malloc_managed_memory_used == 0) && (s < malloc_managed_memory_size))
    {
        malloc_managed_memory_used = 1;
        return malloc_managed_memory_ptr;
    }
    else
        return 0;
}
void* LZ4_calloc(size_t n, size_t s)
{
    char *retVal = LZ4_malloc(n * s);
    fastAlignedMemSet((LwUPtr)retVal, 0, (int)(n * s));
    return retVal;
}
void LZ4_free(void* p)
{
    if (malloc_managed_memory_used == 1)
        malloc_managed_memory_used = 0;
    else
        ICD_HALT
}
void LZ4_memmove(void *dst, const void *src, size_t len)
{
    if (!len) return;
    if (dst == src) return;
    LwU8 *dst_U8 = (LwU8 *)dst;
    LwU8 *src_U8 = (LwU8 *)src;
    if ((size_t)dst < (size_t)src)
    {
        for (LwU64 i = 0; i < len; i++)
            dst_U8[i] = src_U8[i];
    }
    else
    {
        for (LwU64 i = len; i > 0; i--)
            dst_U8[i - 1] = src_U8[i - 1];
    }
}
void* LZ4_memset(void* str, int u8, size_t len)
{
    LwU8 *srlw8 = (LwU8 *)str;
    for (size_t i=0; i<len; i++)
        srlw8[i] = (LwU8)(u8 & 0xFF);
    return str;
}
