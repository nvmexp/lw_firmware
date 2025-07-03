/*
 * Copyright (c) 2016-2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* Crypto system config file for shared LWPKA */

#ifndef CRYPTO_SYSTEM_CONFIG_H
#define CRYPTO_SYSTEM_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include "error_code.h"

#define HAVE_NO_STATIC_DATA_INITIALIZER 1

#define HAVE_EXTERNAL_SHA  1
#define HAVE_EXTERNAL_DRNG 1

#ifdef UPROC_ARCH_FALCON
// Define required stuff from stdint locally
typedef signed char             int8_t;
typedef signed short int        int16_t;
typedef signed int              int32_t;
typedef signed long long        int64_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long long int  uint64_t;
typedef int                     intptr_t;
typedef unsigned int            uintptr_t;
typedef long long int           intmax_t;
typedef unsigned long long int  uintmax_t;
typedef uint32_t status_t;

#ifdef CCC_ENGINE_PMU
#error "This config does not support Falcon PMU!\n"
#elif CCC_ENGINE_LWDEC
#error "This config does not support Falcon LWDEC!\n"
#elif CCC_ENGINE_SEC
#include "dev_sec_csb.h"
#define LW_PRGNLCL_FALCON_DOC_CTRL  LW_CSEC_FALCON_DOC_CTRL
#define LW_PRGNLCL_FALCON_DOC_D0    LW_CSEC_FALCON_DOC_D0
#define LW_PRGNLCL_FALCON_DOC_D1    LW_CSEC_FALCON_DOC_D1
#define LW_PRGNLCL_FALCON_DIC_CTRL  LW_CSEC_FALCON_DIC_CTRL
#define LW_PRGNLCL_FALCON_DIC_D0    LW_CSEC_FALCON_DIC_D0
#elif CCC_ENGINE_GSP
#error "This config does not support Falcon GSP!\n"
#elif CCC_ENGINE_DPU
#error "This config does not support Falcon DPU!\n"
#elif CCC_ENGINE_FSP
#error "This config does not support Falcon FSP!\n"
#endif

static inline uint32_t localReadInline(uint32_t addr)
{
    unsigned int result;
    asm volatile ( "ldxb %0 %1 %2;" : "=r"(result) : "r"(addr), "n"(0) );
    return result;
}

static inline void localWriteInline(uint32_t addr, uint32_t val)
{
    asm volatile ( "stxb %0 %1 %2;" :: "r"(addr), "n"(0), "r"(val) );
}

static inline void dioSeWriteWrapper(const volatile void* addr, uint32_t val)
{
    while(localReadInline(LW_PRGNLCL_FALCON_DOC_CTRL) != 0x1c0004);
    localWriteInline(LW_PRGNLCL_FALCON_DOC_D1, val);
    localWriteInline(LW_PRGNLCL_FALCON_DOC_D0,(uint32_t)(uintptr_t)addr);
    while(!(localReadInline(LW_PRGNLCL_FALCON_DOC_CTRL)&0x80000));
}

static inline uint32_t dioSeReadWrapper(const volatile void* addr)
{
    while(localReadInline(LW_PRGNLCL_FALCON_DOC_CTRL) != 0x1c0004);
    localWriteInline(LW_PRGNLCL_FALCON_DOC_D0, (0x10000|((uint32_t)(uintptr_t)addr)));
    while(!(localReadInline(LW_PRGNLCL_FALCON_DOC_CTRL)&0x100000));
    while ((localReadInline(LW_PRGNLCL_FALCON_DIC_CTRL)&0xFF) == 0x0);
    localWriteInline(LW_PRGNLCL_FALCON_DIC_CTRL, 0x100000);
    return localReadInline(LW_PRGNLCL_FALCON_DIC_D0);
}

#else   // UPROC_ARCH_RISCV
#include <stdint.h>
#include <lwriscv/gcc_attrs.h>
#include <liblwriscv/io_dio.h>

typedef uint32_t status_t;

static inline void dioSeWriteWrapper(const volatile void* addr, uint32_t val)
{
    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    // TODO: Check return value for error
    dioReadWrite(dioPort, DIO_OPERATION_WR, (uint32_t)(uintptr_t)addr, &val);
}

static inline uint32_t dioSeReadWrapper(const volatile void* addr)
{
    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    uint32_t val = 0xbadfbadf;
    // TODO: Check return value for error
    dioReadWrite(dioPort, DIO_OPERATION_RD, (uint32_t)(uintptr_t)addr, &val);
    return val;
}
#endif

#define CCC_BASE_WHEN_DEVICE_NOT_PRESENT (uint8_t*)0x1

#define KERNEL_TEST_MODE 0
#define SE_RD_DEBUG 0
//#define PSC_TRACEF LTRACEF

/* This config file is for T23X with LWPKA, do not use this
 * for PKA1 units.
 */
#define CCC_SOC_WITH_LWPKA 1
//#define HAVE_PKA1_RSA          1


#define HAVE_LESS_FUNCTION_POINTERS 1

/* Context memory configuration options. */
#define HAVE_CONTEXT_MEMORY 1 /* Enable context memory support */
#define HAVE_USE_STATIC_BUFFER_IN_CMEM 1
#define HAVE_IMPLICIT_CONTEXT_MEMORY_USE 1 /* Implicitly pass internal static page as CMEM
                                            * in all CCC api calls.
                                            *
                                            * Only possible to set this in sequential
                                            * subsystems, like PSC.
                                            *
                                            * Also usable in well controlled test setups in more
                                            * complex systems that might normally not be
                                            * sequential, if the tests are run sequentially.
                                            */
#define FORCE_CONTEXT_MEMORY 1 /* If set, not possible to use memory pool: CMEM is mandatory.
                                * Memory pool use triggers an error for algorithm memory.
                                *
                                * If not set, use memory pool if context does not set CMEM
                                * (i.e. selected by caller at runtime).
                                */

/* Tracing options for CMEM implementation:
 *  CCC_TRACE_CMEM_OPS : trace all CCC CMEM object manipulations if set.
 *  CCC_TRACE_CMEM_USAGE : trace all CCC CMEM memory usage if set.
 */
#define CCC_TRACE_CMEM_OPS 0
#define CCC_TRACE_CMEM_USAGE 0
/* context memory options */

/* PSC SE does not support SE mutexes.
 */
#define HAVE_SE_MUTEX 0

/* PSC LWPKA mutex is supported: define 1 if used, 0 otherwise
 */
#define HAVE_LWPKA_MUTEX 1

#define CCC_SOC_FAMILY_TYPE CCC_SOC_FAMILY_T23X

#define HAVE_LWPKA11_SUPPORT 1
#define HAVE_SE_ASYNC 0

/* Define 0 if SCC mechanisms are not usable in the subsystem.
 *  All SCC protection systems turned on by default now.
 */
#define HAVE_SE_SCC        0 /* Enables SE AES SCC features (default:1) */

/* For LWPKA KEY LOADING
 *
 * EC key loading feature more or less unusable for everything else than ECDH =>
 * not recommended to use PKA key loading for any ECC operations. For ECC the
 * HW can use PKA keyslot only for scalar value in point multiplications.
 *
 * All other EC field values are unused.
 */
#define HAVE_LWPKA_LOAD   0
#define SHA_DST_MEMORY    0    /* VDK CMOD does not support HASH_REGs with SHAKEs
                                * (Must be set 1 if using SHAKEs)
                                *
                                * Other digests and HMAC-SHA2 HW seem OK now.
                                */

#define HAVE_USER_MODE    0 /* Support secure AARCH-32 user mode (Trusty TA's) */

#define HAVE_SE_AES       0    /* Support AES with the SE AES engine(s) */
#define HAVE_AES_XTS      0    /* Support XTS-AES mode */
#define HAVE_AES_GCM      0    /* Support AES-GCM mode */
#define HAVE_SE_SHA       0    /* Support SHA digests with the SE SHA engine */
#define HAVE_CMAC_AES     0    /* Support CMAC-AES macs with the SE AES engine */
#define HAVE_HW_HMAC_SHA  0    /* Support HW HMAC-SHA-* macs with the SE HASH engine*/
#define HAVE_SE_RANDOM    0    /* Support SE DRNG random numbers with AES0 engine seeded from RO's */
#define HAVE_ALLOW_CACHED_RANDOM 0 /* Use cache of fresh random numbers */
#define HAVE_LWRNG_DRNG   1    /* Support DRNG random numbers with LWRNG engine */
#define HAVE_SE_RSA       0    /* Support SE unit RSA engine */
#define HAVE_SE_KAC_KDF   0    /* Support SE key access control key derivations KDF/KDD */
#define HAVE_SE_KAC_KDF_NIST_ENCODER 0 /* Enable NIST 800-108 encoder if HAVE_SE_KAC_KDF enabled */
#define HAVE_SE_KAC_KEYOP 0    /* Support SE key access control operation, e.g., KW/KUW */
#define HAVE_FIXED_KW_AAD 0    /* HW allow AAD data to be optional */
#define HAVE_LWPKA_RSA     1    /* Add LWPKA RSA (LW unit with up to 4096 bit RSA keys) */
#define HAVE_LWPKA_ECC     1    /* Add LWPKA ECC (LW unit with for ECDH, ECDSA, EC point operations) */

#define HAVE_LWPKA_ECDH    0    /* Add LWPKA ECDH operation (requires HAVE_LWPKA_ECC) */
#define HAVE_LWPKA_ECDSA   1    /* Add LWPKA ECDSA (requires HAVE_LWPKA_ECC, HAVE_LWPKA_MODULAR) */
#define HAVE_SHAMIR_TRICK 0     /* Enable Shamir's trick for PKA1/LWPKA */
#define HAVE_LWPKA_MODULAR 1    /* Add LWPKA modular library (e.g. mandatory for ECDSA, requires HAVE_LWPKA_ECC) */

#define HAVE_GEN_MULTIPLY 0     /* Use generic multiply instead of MOD_MULT
                                 *
                                 * MOD_MULT is probably faster in LWPKA so define
                                 * this 0 (for now at least).
                                 */


#define HAVE_RSA_CIPHER   0    /* Add RSA encrypt/decrypt (plain && padding crypto) */

/*
 * Diffie-Hellman-Merke key agreement (modular exponentiation,
 *    requires HAVE_RSA_CIPHER)
 */
#define HAVE_PLAIN_DH    0

#if HAVE_RSA_CIPHER
#define HAVE_RSA_OAEP     0
#define HAVE_OAEP_LABEL   0    /* Optionally support passing a LABEL to OAEP (len max 32 bytes now) */
#endif

#define HAVE_RSA_PKCS_VERIFY      0 /* RSA-PKCS#1v1.5 signature support */

#define HAVE_RSA_PKCS_SIGN      0 /* RSASSA PKCS#1v1.5 signing */
#define HAVE_RSA_PSS_SIGN      0 /* do not test rsa signing yet in PSC SE */

#if HAVE_LWPKA_ECC

#define HAVE_ECDSA_SIGNING   1 /* ECDSA signature generation */

#define HAVE_LWPKA_ED25519   0 /* LWPKA support for ED25519 signatures */
#define HAVE_ED25519_SIGN    0 /* ED25519 signing support */

#define HAVE_P521            0 /* LWPKA P521 lwrve support */

#define HAVE_NIST_LWRVES     1 /* Support NIST EC lwrves */
#define HAVE_NIST_LWRVE_ALL  0 /* Support all NIST EC lwrves (or select individual lwrves) */

/* Select individual NIST lwrves below. */
#define HAVE_NIST_P256        1
#define HAVE_NIST_P384        1

#define HAVE_NIST_P192        0 /* LWPKA does not support this lwrve. Need to test this case
                                 * is handled properly.
                                 * All tests using this lwrve MUST FAIL!
                                 */
#define HAVE_NIST_P521        HAVE_P521

#define HAVE_BRAINPOOL_LWRVES         0 /* Support brainpool EC lwrves */
#define HAVE_BRAINPOOL_LWRVES_ALL     0 /* Support brainpool EC lwrves (or select individual lwrves) */

#if HAVE_BRAINPOOL_LWRVES
/* Select the Brainpool lwrves to support */
#define HAVE_BP_256           0 /* Used in PSC BP for tests only! */
#define HAVE_BP_512           0 /* Used in PSC BP for tests only! */

#define HAVE_BRAINPOOL_TWISTED_LWRVES 0        /* Support brainpool EC twisted lwrves */
#define HAVE_BRAINPOOL_TWISTED_LWRVES_ALL 0    /* Support brainpool EC twisted lwrves (or select individual lwrves) */
#define HAVE_BPT_512    0     /* Support brainpoolP512r1 twisted lwrve */
#endif /* HAVE_BRAINPOOL_LWRVES */

#if HAVE_LWPKA_ED25519
#define HAVE_LWRVE_ED25519 0
#else
#define HAVE_LWRVE_ED25519 0
#define HAVE_ED25519_SIGN  0
#endif

#if HAVE_LWPKA_X25519
#define HAVE_LWRVE_C25519 0
#else
#define HAVE_LWRVE_C25519 0
#endif

#endif /* HAVE_LWPKA_ECC */

/* Optional support for MD5 digest and HMAC-MD5 (set in makefile) */
#ifndef HAVE_MD5
#define HAVE_MD5    0    /* Set 0 to drop MD5/HMAC-MD5 support */
#endif

/* Only VDK CMOD requires this. It can be removed on HW */
#define SE_NULL_SHA_DIGEST_FIXED_RESULTS 0 /* Get correct results from sha-*(<NULL INPUT>) */

/* Define non-zero to see each full operation memory allocation
 * (from beginning of INIT to end of RESET)
 */
#define TEGRA_MEASURE_MEMORY_USAGE 0    /* Measure runtime memory usage */

#define HAVE_SE1    0   /* secondary SE1 unit in T21X variants */

/* The tags for encrypted keys are truncated to 64 bits to save fuse space.*/
#define AES_GCM_MIN_TAG_LENGTH 8U

/****************************/

#ifndef KERNEL_TEST_MODE
#define KERNEL_TEST_MODE 0
#endif

#ifndef SE_RD_DEBUG
#define SE_RD_DEBUG    0    /* Compile with R&D code (allow override by makefile for now) */
#endif

#if KERNEL_TEST_MODE
#if SE_RD_DEBUG == 0
#error "SE_RD_DEBUG must be nonzero to enable test code in PSC"
#endif
#endif


#define DEBUG_VARIABLE(x)  (void)(x)

/* Log levels */
#define CCC_INFO_LEVEL 4
#define CCC_ERROR_LEVEL 3
#define CCC_CRITICAL_LEVEL 2
#define CCC_ALWAYS_LEVEL 0

/* Output log entries at this level and more serious
 */
#ifndef CCC_LOG_THRESHOLD
#define CCC_LOG_THRESHOLD CCC_ERROR_LEVEL
#endif

void printk(const char *pcFmt, ...);
/* PSC FW specific macro to support logging */
/*#define CCC_LOG(level, msg, ...) \
    do { if ((level) <= CCC_LOG_THRESHOLD) { printk(msg, ## __VA_ARGS__); }} while (false)*/
//#define CCC_LOG(level, msg, ...) printf(msg, ## __VA_ARGS__)
#define CCC_LOG(level, msg, ...)

#define ERROR_MESSAGE LOG_ERROR
#define LOG_INFO(msg,...)   CCC_LOG(CCC_INFO_LEVEL, msg, ## __VA_ARGS__)
#define LOG_ERROR(msg,...)  CCC_LOG(CCC_ERROR_LEVEL, msg, ## __VA_ARGS__)
#define LOG_ALWAYS(msg,...) CCC_LOG(CCC_ALWAYS_LEVEL, msg, ## __VA_ARGS__)

#define ERROR_MESSAGE LOG_ERROR

/* userspace address is visible to kernel directly. */
#define GET_DOMAIN_ADDR(domain, addr) (addr)

/* MPU for address translation
 * it is implemented with MPU walk, can be optimized further.
*/
#define DOMAIN_VADDR_TO_PADDR(domain, addr) (PHYS_ADDR_T)(addr)

/* Get CACHE_LINE from <arch/defines.h> or define is for your CPU
 * for LWRISCV, I-cache is 2-way set associative with 64 bytes per cache line.
 * BUT it is disabled for PSC(cache size is 0).
 */
#define CACHE_LINE 64U /* misra compatibility */

/* For aligning data objects WRITTEN to by the SE DMA engine; in
 * practice this is relevant only for AES related OUTPUT BUFFERS
 * (Nothing else is written with the DMA).
 *
 * E.g. SE DRNG output is also affected, but that is also generated by
 * SE AES engine and written by DMA. But e.g. CMAC is not affected
 * because the CMAC output is not written by DMA, even though it is
 * callwlated with AES engine.
 *
 * If the buffers are not aligned you can get them aligned by the
 * compiler using e.g. like this:
 *
 * DMA_ALIGN_DATA uint8_t my_output_buffer[ DMA_ALIGN_SIZE(MY_BUFFER_SIZE) ];
 *
 * If the AES buffers are not cache line aligned there is VERY BIG
 * RISK that the data will get corrupted after the DMA has written
 * the correct result to phys memory.
 *
 * E.g. cache flushes would not prevent this in general case due to
 * cache prefetches. So, to prevent corruptions please always align
 * DMA output buffers (both by address and size)!
 */
#ifndef DMA_ALIGN_DATA
#define DMA_ALIGN_DATA GCC_ATTR_ALIGNED(CACHE_LINE)
#endif

/*
 * Required for aligning SE engine AES output buffers
 */
#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define DMA_ALIGN_SIZE(size) ROUNDUP(size, CACHE_LINE)

/* Colwert the value to ordinal type: it may be a virtual address or size */
#define IS_DMA_ALIGNED(value) ((((uintptr_t)(value)) & (CACHE_LINE - 1U)) == 0U)
#if 0
#define TEGRA_READ32(a) ({ uint32_t val = readl(a); printk("READ32: reg:0x%08x, val:0x%08x\n", (a), val); val;})
#define TEGRA_WRITE32(a, v) ({printk("WRITE32: reg:0x%08x, val:0x%08x\n", (a), (v)); writel(v, a); })
#else
#define TEGRA_READ32(a)     ({ dioSeReadWrapper(a); })
#define TEGRA_WRITE32(a, v) ({ dioSeWriteWrapper(a, v); })
#endif
/* System physical and virtual address types */
/* PSC VA is 64bit, PA is in 1G range and external mmio is 32bit */
#define PHYS_ADDR_T uint64_t
#define VIRT_ADDR_T uint64_t

/*  Max host address length for phys addresses */
#define PHYS_ADDR_LENGTH 8

/* Word align objects in memory.
 *
 * Used e.g. for aligning EC lwrve parameter uint8_t arrays
 * which are accessed via (const uint32_t *)pointer.
 *
 * If CPU supports misaligned pointer accesses,
 * this does not need to be defined at all.
 */
#define ALIGN32 GCC_ATTR_ALIGNED(sizeof(uint32_t))

/* Use a compiler fence before cache ops
 *
 * With GCC the following could be used as compiler fence =>
 * #define CF do { __asm__ volatile("" ::: "memory"); } while(false)
 */
/* fence o,w is sufficient to ensure that all writes to the device have
 * completed */
#define CF                                          \
    do {                                            \
        __asm__ volatile("" ::: "memory");          \
    } while (false)
/*
 * RISC-V doesn't have an instruction to flush parts of the icache.
 * asm volatile ("fence.i" ::: "memory"); to flush all.
 * disabled for PSC firmware(no cache) now
 * */
#define SE_CACHE_FLUSH(vaddr, size) \
    do {                            \
        CF;                         \
    } while (false)

#define SE_CACHE_FLUSH_ILWALIDATE(vaddr, size) \
    do {                                       \
        CF;                                    \
    } while (false)

#define SE_CACHE_ILWALIDATE(vaddr, size) \
    do {                                 \
        CF;                              \
    } while (false)

/* with current PSC FW common crypto porting, common crypto code runs in kernel mode,
 * which means in RISCV-V S-mode(or M-mode) with interrupt disabled. user task
 * must use syscall to call into common crypto library.
 * NO mutex is required in kernel mode. However, in user space, a global crypto mutex lock
 * shared by all user tasks is added. User task is required to grab the mutex before use common crypto syscall.
 * it simplify access control and reduce memory footprint(only one context).
 */
#define SW_MUTEX_TAKE
#define SW_MUTEX_RELEASE

#if SE_RD_DEBUG

/* we don't turn on builtin */
#ifndef unlikely
#define unlikely(x) (x)
#endif

/* assert.h does not define proper DEBUG_ASSERT()
 * unless LK_DEBUGLEVEL > 1 (it's not) => redefine it here
 */
#undef DEBUG_ASSERT
#define DEBUG_ASSERT(x)                                                        \
    do {                                                                       \
        if (BOOL_IS_TRUE(unlikely(!BOOL_IS_TRUE(x)))) {                        \
            printk("DEBUG ASSERT FAILED at (%s:%d): %s\n", __FILE__, __LINE__, \
                  #x);                                                         \
            for (;;) {                                                         \
            }                                                                  \
        }                                                                      \
    } while (false)

/* Rejects only NULL paddr for now in PSC (XXX TODO: improve this)
 *
 * XXXX Should trap everything that cannot be accessed by the SE ENGINE DMA
 * XXXX  if data is in some internal memory => that may not be valid (I do not know
 * XXXX  the address ranges to trap with PSC). If this traps, data needs to be placed
 * XXXX  somewhere else before calling the trapped function.
 */
#define VALID_PADDR_SE_RANGE(paddr, size) ((paddr) != (PHYS_ADDR_T)(uintptr_t)NULL)

#define SE_PHYS_RANGE_CHECK(paddr, size)                                   \
    do {                                                                   \
        if (!VALID_PADDR_SE_RANGE((paddr), (size))) {                      \
            ret = SE_ERROR(ERR_OUT_OF_RANGE);                              \
            LTRACEF(                                                       \
                "Phys addr 0x%llx..0x%llx (size %u) not SE HW accessible " \
                "(%d)\n",                                                  \
                (paddr), (paddr) + (size), (size), ret);                   \
            goto fail;                                                     \
        }                                                                  \
    } while (false)

inline static status_t log_get_SE_ERROR(status_t ecode, const char *ename,
                                          const char *func, uint32_t line)
{
    LOG_INFO("SE_ERROR[%s:%u] (%s [%d])\n", func, line, ename, ecode);
    return ecode;
}

#define SE_ERROR(x) log_get_SE_ERROR(x, #x, __func__, __LINE__)

/* Debug feature: Trace all register writes and reads if nonzero; shows
 * engine, register offset and name, values written and read
 *
 * If engine is SE0_AES1 the names may be for SE0_AES0 (they are
 *  offset to SE0_AES1 if AES1 is selected, but the debug strings
 *  do not change...)
 */
#ifndef TRACE_REG_OPS
#define TRACE_REG_OPS 0
#endif

/* If nonzero: trace code exelwtion, output other trace data as well */
#ifndef MODULE_TRACE
#define MODULE_TRACE  0
#endif

/* assuming all DMA accesses are OK for now */
#define DEBUG_ASSERT_PHYS_DMA_OK(addr,size)

#if MODULE_TRACE

#undef LTRACEF
#define LTRACEF(str, x...) do { printk("%s:%d: " str, __PRETTY_FUNCTION__, __LINE__, ## x); } while (false)

/* To get location info in error messages when debugging with trace:
 * use ltrace for this as well...
 */
#undef  ERROR_MESSAGE
#define ERROR_MESSAGE LTRACEF

#define LOG_HEXBUF(name,base,len) do { LTRACEF(""); dump_data_object(name, (const uint8_t *)base, len, len); } while(false)
#endif

/*
#define DUMP_DATA_PARAMS(name,flags,dp)                    \
  { struct se_data_params *__di = dp;                    \
    if (__di) {                                \
      LTRACEF("%s [" #dp "(%p)] => src=%p input_size=%u: dst=%p output_size=%u\n", \
          name,__di,__di->src,__di->input_size,__di->dst,__di->output_size); \
      if (((flags) & 0x1U) && __di->src) { LOG_HEXBUF(" " #dp "->src:", __di->src, __di->input_size); } \
      if (((flags) & 0x2U) && __di->dst) { LOG_HEXBUF(" " #dp "->dst:", __di->dst, __di->output_size); } \
    }                                    \
  }
*/

#else /* SE_RD_DEBUG */

#define SE_PHYS_RANGE_CHECK(paddr, size) ({ (void) paddr; (void) size; })
#define SE_ERROR(x) x
#define TRACE_REG_OPS 0
#define MODULE_TRACE  0

#undef LTRACEF

#endif    /* SE_RD_DEBUG */

/* LW-MPU page size. new lwMPU has 1K */
#define PAGE_SIZE 4096U
#define PAGE_MASK (PAGE_SIZE - 1U)

#ifndef LOG_HEXBUF
#define LOG_HEXBUF(name,base,len)
#endif

#ifndef DEBUG_ASSERT_PHYS_DMA_OK
/* This means all addresses are valid for SE DMA */
#define DEBUG_ASSERT_PHYS_DMA_OK(addr,size)
#endif

#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT(x)
#endif

#ifndef DUMP_DATA_PARAMS
#define DUMP_DATA_PARAMS(name, flags, dp)
#endif

#ifndef LTRACEF
#define LTRACEF(...)
#endif

#define TEGRA_PERF_NONE        0x0000U
#define TEGRA_PERF_CONTEXT    0x0001U
#define TEGRA_PERF_ENGINE    0x0002U

/* Perf monitor the crypto ops from context setup to dofinal() completed */
/* #define TEGRA_MEASURE_PERFORMANCE TEGRA_PERF_CONTEXT */

/* Add platform specific code for microsecond time functions.
 * These do not exist in the Trusty OS...
 *
 * Added uSec functions, but do not use the udelay() to
 *  slow down the device status register polling (also added
 *  usec performance timing to the crypto context level operations
 *  and these numbers are affected by delay loops => disabled udelay())
 */
#define HAVE_USEC_COUNTER 1

#if HAVE_USEC_COUNTER

#ifndef TEGRA_MEASURE_PERFORMANCE
#define TEGRA_MEASURE_PERFORMANCE TEGRA_PERF_NONE
#endif

#if 0    // Do not use for now => better crypto perf.
void     udelay(uint32_t n);
#define TEGRA_UDELAY(usec) udelay(usec)
#endif

#define GET_USEC_TIMESTAMP psc_get_usec_counter32
#endif /* HAVE_USEC_COUNTER */

#ifndef TEGRA_UDELAY

#define TEGRA_UDELAY(usec)

/* If there is no decent delay function, use larger
 * poll loop max value for LWPKA operation wait complete.
 */
#define TEGRA_DELAY_FUNCTION_UNDEFINED 1

/* with zero defined delay value => TEGRA_UDELAY is not called
 * in the corresponding polling loop.
 */
#define TEGRA_UDELAY_VALUE_SE0_OP_INITIAL   0 /* initial delay before polling the status */
#define TEGRA_UDELAY_VALUE_SE0_OP        0 /* SE engine operation complete poll interval */
#define TEGRA_UDELAY_VALUE_SE0_MUTEX        0 /* SE mutex lock poll interval */

#define TEGRA_UDELAY_VALUE_LWPKA_OP_INITIAL  0  /* initial delay before polling the status */
#define TEGRA_UDELAY_VALUE_LWPKA_OP           0    /* LWPKA operation complete poll interval */
#define TEGRA_UDELAY_VALUE_LWPKA_MUTEX        0    /* LWPKA mutex lock poll interval */

#else

/* with zero defined delay value => TEGRA_UDELAY is not called in the
 * corresponding polling loop. Use minimal values for Trusty with
 * udelay() to avoid crypto performance drops.
 *
 * I guess there is not much pwr consumption diff in polling a SE
 * status register and polling a usec timer register => polling SE
 * status register is better for performance.
 */
#define TEGRA_UDELAY_VALUE_SE0_OP_INITIAL   1 /* initial delay before polling the status */
#define TEGRA_UDELAY_VALUE_SE0_OP        1 /* SE engine operation complete poll interval */
#define TEGRA_UDELAY_VALUE_SE0_MUTEX        1 /* SE mutex lock poll interval */

#define TEGRA_UDELAY_VALUE_LWPKA_OP_INITIAL  1  /* initial delay before polling the status */
#define TEGRA_UDELAY_VALUE_LWPKA_OP           1    /* LWPKA operation complete poll interval */
#define TEGRA_UDELAY_VALUE_LWPKA_MUTEX        1    /* LWPKA mutex lock poll interval */

#define TEGRA_DELAY_FUNCTION_UNDEFINED        0

#endif /* TEGRA_UDELAY */

#if TEGRA_MEASURE_MEMORY_USAGE
#define MEASURE_MEMORY_START(purpose)                    \
  do { bool _mes_start = measure_mem_start(purpose, __func__, __LINE__); \
    if (!BOOL_IS_TRUE(_mes_start)) {                    \
      ERROR_MESSAGE("Mem measure start failed: %s\n", __func__); goto fail; \
    } } while(false)
#define MEASURE_MEMORY_STOP   measure_mem_stop(__func__, __LINE__)

#else

#define MEASURE_MEMORY_START(purpose)
#define MEASURE_MEMORY_STOP
#endif

/* SYSTEM_MAP_DEVICE is a function name => override it in this define
 * or define a function called SYSTEM_MAP_DEVICE (what ever you prefer).
 *
 * The function is called at most MAX_CRYPTO_DEVICES times with
 * device_id values set from 0..(MAX_CRYPTO_DEVICES-1U) matching the
 * numeral values of se_cdev_id_t enums. If a device if not configured,
 * it is not called to be initialized.
 *
 * Lwrrently the values are like this, but please check se_cdev_id_t
 * for the actual mapping, each with it's own base address:
 *
 *  SE_CDEV_SE0  = 0,
 *  SE_CDEV_PKA1 = 1, => LWPKA uses this device as well
 *  SE_CDEV_RNG1 = 2,
 *  SE_CDEV_SE1  = 3,
 *
 * Must return non-zero on error.
 *
 * If the function returns 0 and the *base_p == NULL then the device does not
 * exist in that system.
 */
#define SYSTEM_MAP_DEVICE map_crypto_device
#include <crypto_api.h>
/*
 * copied from address_map.h:
 * https://lwtegra/home/tegra_manuals/html/manuals/t234/address_map_new.txt
 */
#define LW_ADDRESS_MAP_PSC_SE_BASE      CCC_BASE_WHEN_DEVICE_NOT_PRESENT
#define LW_ADDRESS_MAP_SE_LWPKA_BASE    0x0
#define LW_ADDRESS_MAP_SE_LWRNG_BASE    0xd800
/**
 *  @brief eturn register VA base address for valid ids.
 *
 *  @param[in] device_id  SE engine enum id with se_cdev_id_t type.
 *  @param[out] base      LW-MPU mapped base VA address.
 */
static inline status_t map_crypto_device(uint32_t device_id, void **base)
{
    uint32_t ret = ERR_NOT_FOUND;

    switch (device_id) {
        case SE_CDEV_SE0:
            *base = (void *)LW_ADDRESS_MAP_PSC_SE_BASE;
            ret = NO_ERROR;
            break;

        case SE_CDEV_PKA1:
            *base = (void *)LW_ADDRESS_MAP_SE_LWPKA_BASE;
            ret = NO_ERROR;
            break;

        case SE_CDEV_RNG1:
            *base = (void *)(LW_ADDRESS_MAP_SE_LWRNG_BASE);
            ret = NO_ERROR;
            break;

        default:
            *base = NULL;
            break;
    }
    return ret;
}
/* This initializes the SE/HW driver in tegra_cdev.c, called once
 * when initializing the system.
 */
status_t tegra_init_crypto_devices(void);
#define init_crypto_devices() tegra_init_crypto_devices()

/* Map SE0, SE1, LWPKA and RNG1 and set the base address to the arg
 * in Trusty. Returning NULL base address is treated like engine does not exist,
 * disabling that engine.
 *
 * Return non-zero value to indicate an error (or just panic the system).
 */
/* psc_status_t SYSTEM_MAP_DEVICE(uint32_t device_id, void **base); */

#ifdef CCC_SOC_WITH_LWPKA
#include <lwpka_defs.h>
#endif

/* For R&D debug data dumps and other utility functions */
#include <crypto_util.h>

#endif /* INCLUDED_CRYPTO_SYSTEM_CONFIG_H */
