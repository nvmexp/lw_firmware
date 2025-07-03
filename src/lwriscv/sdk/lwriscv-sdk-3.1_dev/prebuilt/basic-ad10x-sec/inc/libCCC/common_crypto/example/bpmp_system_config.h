/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

/* Crypto system config file for target: BPMP running the LK kernel */

#ifndef __CRYPTO_SYSTEM_CONFIG_H__
#define __CRYPTO_SYSTEM_CONFIG_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <string.h>
#include <err.h>
#include <malloc.h>
#include <lib/heap.h>
#include <core.h>		// for udelay()

// Add some missing error codes :-)
#define ERR_BAD_STATE		-30
#define ERR_OUT_OF_RANGE	-31
#define ERR_NOT_IMPLEMENTED	-32
#define ERR_FAULT		-33
#define ERR_BAD_LEN		-34
#define ERR_SIGNATURE_INVALID	-35

#include <debug.h>
#include <trace.h>
#include <assert.h>

#include <kernel/mutex.h>
#include <arch/defines.h>	// for CACHE_LINE
#include <mpu.h>		// phys addr mapping via AST

/* Define 0 if SCC mechanisms are not usable in the subsystem.
 *  All SCC protection systems turned on by default now.
 */
#define HAVE_SE_SCC		1 // Enables SE AES SCC features (default:1)
#define HAVE_PKA1_SCC_DPA	1 // PKA1 SCC features (default:1)
#define HAVE_PKA1_BLINDING	1 // PKA1 priv key operation blinding (default:1)
#define HAVE_JUMP_PROBABILITY	1 // PKA1 jump probability (default:1)

// For evaluating PKA1 KEY LOADING
// PKA1 RSA key loading seems to work
// ECC key loading code NOT TESTED/DOES NOT WORK for now
//
#define HAVE_PKA1_LOAD	1

#define HAVE_DEPRECATED 0	  // If non-zero => compiles in deprecated functions for
				  // backwards compatibility.
				  // Currently: AES && RSA keyslot clear functions
				  // These have new versions with device_id instead of engine_hint
				  // parameters. Define as zero unless you need the old versions...

#define HAVE_DYNAMIC_KEY_OBJECT 1 // In-kernel crypto context (crypto_context_t) can optionally
				  // be split into two objects:
				  //  the context and a separate key object;
				  // This keeps crypto_context_t much smaller since the major
				  // portion of it was the key object (more than a kilobyte)
				  //
				  // This benefits algorithms that do not use keys, specifically
				  //  DIGESTS (which are used e.g. in EDDSA verification)
				  //
				  // Reduces the crypto_context_t object size to =>
				  //  7*sizeof(uint32_t)+2*sizeof(void *) == 36 bytes;

#define HAVE_NC_REGIONS	0	// Handle non-contiguous phys memory regions for SE engine DMA
				// (XXX Not complete: AES cipher data is COPIED around =>FIXME)

#define HAVE_USER_MODE	0	// Support secure AARCH-32 user mode (Trusty TA's)

#define HAVE_SE_AES	1	// Support AES with the SE AES engine(s)
#define HAVE_SE_SHA	1	// Support SHA digests with the SE SHA engine
#define HAVE_CMAC_AES	1	// Support CMAC-AES macs with the SE AES engine (requires HAVE_SE_AES)
#define HAVE_HMAC_SHA	1	// Support HMAC-SHA-* macs with the SE SHA engine (requires HAVE_SE_SHA)
#define HAVE_SE_RANDOM	1	// Support SE DRNG random numbers (requires HAVE_SE_AES)
#define HAVE_SE_RSA	1	// Support SE unit RSA engine
#define HAVE_PKA1_RSA	1	// Add PKA1 RSA (Elliptic unit with up to 4096 bit RSA keys)
#define HAVE_PKA1_ECC	1	// Add PKA1 ECC (Elliptic unit with for ECDH, ECDSA, EC point operations)

#define HAVE_PKA1_ECDH	1	// Add PKA1 ECDH operation (requires HAVE_PKA1_ECC)
#define HAVE_PKA1_ECDSA	1	// Add PKA1 ECDSA (requires HAVE_PKA1_ECC, HAVE_PKA1_MODULAR)
#define HAVE_PKA1_MODULAR 1	// Add PKA1 modular library (e.g. mandatory for ECDSA, requires HAVE_PKA1_ECC)

#define HAVE_RSA_CIPHER	1	// Add RSA encrypt/decrypt (plain && padding crypto)
#define HAVE_PLAIN_DH	1	// Diffie-Hellman-Merke key agreement (modular exponentiation,
				//   requires HAVE_RSA_CIPHER)

#define HAVE_RSAPSS_SIGNING	      0 // XXX Not implemented yet
#define HAVE_ECDSA_SIGNING	      0 // XXX Not implemented yet

#define HAVE_RSA_PKCS_VERIFY	      1	// RSA-PKCS#1v1.5 signature support
#define HAVE_RSA_PKCS_SIGN	      0 // XXX Not implemented yet

#if HAVE_PKA1_ECC

#ifndef HAVE_P521
#define HAVE_P521 		      0 // XXX NIST-P521 curve not fully implemented yet...
#endif

#define HAVE_NIST_CURVES	      1 // Support NIST EC curves
#define HAVE_NIST_CURVE_ALL	      1 // Support all NIST EC curves (or select individual curves)

#define HAVE_BRAINPOOL_CURVES	      1	// Support brainpool EC curves
#define HAVE_BRAINPOOL_CURVES_ALL     1	// Support brainpool EC curves (or select individual curves)

#if HAVE_BRAINPOOL_CURVES
#define HAVE_BRAINPOOL_TWISTED_CURVES 1		// Support brainpool EC twisted curves
#define HAVE_BRAINPOOL_TWISTED_CURVES_ALL 1	// Support brainpool EC twisted curves (or select individual curves)
#endif

#ifndef HAVE_ELLIPTIC_20
#define HAVE_ELLIPTIC_20 0	// Code running in T194 with Elliptic-2.0 FW (with new curves and algos)
#endif

#if HAVE_ELLIPTIC_20

#ifndef HAVE_PKA1_ED25519
#define HAVE_PKA1_ED25519 0	// Add PKA1 ED25519 (requires HAVE_PKA1_ECC, HAVE_PKA1_MODULAR)
#endif

#ifndef HAVE_PKA1_X25519
#define HAVE_PKA1_X25519 0	// Add PKA1 X25519 (requires HAVE_PKA1_ECC)
#endif

// X25519 and ED25519 requires new PKA1 FW => HAVE_ELLIPTIC_20 must be nonzero
#if HAVE_PKA1_ED25519
#define HAVE_CURVE_ED25519 1
#else
#define HAVE_CURVE_ED25519 0
#endif

#if HAVE_PKA1_X25519
#define HAVE_CURVE_C25519 1
#else
#define HAVE_CURVE_C25519 0
#endif

#else /* HAVE_ELLIPTIC_20 */

/* This is unconditional because no support can exist */
#define HAVE_CURVE_ED25519 0
#define HAVE_CURVE_C25519  0

#endif /* HAVE_ELLIPTIC_20 */

#endif /* HAVE_PKA1_ECC */

#ifndef HAVE_DEPRECATED
#define HAVE_DEPRECATED 0
#endif

/* Optional support for MD5 digest and HMAC-MD5 (set in makefile) */
#ifndef HAVE_MD5
#define HAVE_MD5	0	// Set 0 to drop MD5/HMAC-MD5 support
#endif

#define SE_NULL_SHA_DIGEST_FIXED_RESULTS 1	// Get correct results from sha-*(<NULL INPUT>)

/* Define non-zero to see each full operation memory allocation
 * (from beginning of INIT to end of RESET)
 */
#define TEGRA_MEASURE_MEMORY_USAGE 0	// Measure runtime memory usage

// Do not turn on the following yet (not implemented)

/* TODO => I have no way to test this (this is incomplete!!!) */
#define HAVE_SE1	0	// (untested) => secondary SE1 unit

#define HAVE_PKA1_TRNG	1	// Add PKA1 TNG (entropy seed generator)

/* TODO => I have not implemented these yet */
#define HAVE_PKA1_RNG	0	// (Not implemented yet) => Add PKA1 DRNG unit

/****************************/

#define __TEGRA__	1	// (used in md5.c only; XXX should use HOST_LITTLE_ENDIAN or some such instead)

#ifndef KERNEL_TEST_MODE
#define KERNEL_TEST_MODE 0
#endif

#ifndef SE_RD_DEBUG
#define SE_RD_DEBUG	0	// Compile with R&D code (allow override by makefile for now)
#endif

#if KERNEL_TEST_MODE
#if SE_RD_DEBUG == 0
#error "SE_RD_DEBUG must be nonzero to enable test code in BPMP"
#endif
#endif

#define misra_dprintf(level, x...) do { if ((level) <= LK_DEBUGLEVEL) { (void)_dprintf(x); } } while (false)

/* BPMP specific macro to support logging */
#define BPMP_LOG(level, msg,...) misra_dprintf(level, msg, ## __VA_ARGS__)

/* SE driver log macros for BPMP */
// #define LOG_NOISE(msg,...)  BPMP_LOG(SPEW,    msg, ## __VA_ARGS__)
#define LOG_INFO(msg,...)   BPMP_LOG(INFO,    msg, ## __VA_ARGS__)
#define LOG_ERROR(msg,...)  BPMP_LOG(CRITICAL,msg, ## __VA_ARGS__)
#define LOG_ALWAYS(msg,...) BPMP_LOG(CRITICAL,msg, ## __VA_ARGS__)

#define ERROR_MESSAGE LOG_ERROR

/* No userspace contexts supported by this driver */
#define GET_DOMAIN_ADDR(domain, addr) ((void *)addr)

/* BPMP uses only phys addresses (AST mapped phys addresses!!!) */
#define DOMAIN_VADDR_TO_PADDR(domain, addr) to_phys_addr((void *)addr)

/* Get CACHE_LINE from <arch/defines.h> or define is for your CPU
 */
#ifndef CACHE_LINE
#define CACHE_LINE 32U	/* ARM-R5 has 32 bytes per cache line */
#endif

#if CACHE_LINE == 64
#undef CACHE_LINE
#define CACHE_LINE 64U	/* misra compatibility */
#endif

#if CACHE_LINE == 32
#undef CACHE_LINE
#define CACHE_LINE 32U	/* misra compatibility */
#endif

/* For aligning data objects WRITTEN to by the SE DMA engine; in
 * practice this is relevant only for AES related OUTPUT BUFFERS
 * (Nothing else is written with the DMA).
 *
 * E.g. SE DRNG output is also affected, but that is also generated by
 * SE AES engine and written by DMA. But e.g. CMAC is not affected
 * because the CMAC output is not written by DMA, even though it is
 * calculated with AES engine.
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
#define DMA_ALIGN_DATA __ALIGNED(CACHE_LINE)
#endif

/*
 * Required for aligning SE engine AES output buffers
 */
#define DMA_ALIGN_SIZE(size) ROUNDUP(size, CACHE_LINE)

/* Convert the value to ordinal type: it may be a virtual address or size */
#define IS_DMA_ALIGNED(value) ((((uintptr_t)(value)) & (CACHE_LINE - 1U)) == 0U)

#define TEGRA_READ32(a)     readl(a)
#define TEGRA_WRITE32(a, v) writel(v, a)

/* System physical and virtual address types */
//
// bpmp paddr_t seems to be 32 bit long, but there is some kind of AST
// which maps the virtual addresses to 64 bit phys addrs => need to use 64 bit values anyway!
//
#define PHYS_ADDR_T uint64_t
#define VIRT_ADDR_T vaddr_t

// Max host address length for phys addresses
#define PHYS_ADDR_LENGTH 8

/* Word align objects in memory.
 *
 * Used e.g. for aligning EC curve parameter uint8_t arrays
 * which are accessed via (const uint32_t *)pointer.
 *
 * If CPU supports misaligned pointer accesses,
 * this does not need to be defined at all.
 */
#define ALIGN32 __ALIGNED(sizeof(uint32_t))

/* Use a compiler fence before cache ops
 *
 * With GCC the following could be used as compiler fence =>
 * #define CF do { __asm__ volatile("" ::: "memory"); } while(false)
 */
#define SE_CACHE_FLUSH(vaddr, size) do { CF; arch_clean_cache_range(vaddr,size); } while(false)
#define SE_CACHE_FLUSH_INVALIDATE(vaddr, size) do { CF; arch_clean_invalidate_cache_range(vaddr,size); } while(false)
#define SE_CACHE_INVALIDATE(vaddr, size) do { CF; arch_invalidate_cache_range(vaddr,size); } while(false)

/* BPMP does not need SW mutexes, only takes the HW mutex */
#define SW_MUTEX_TAKE
#define SW_MUTEX_RELEASE

struct engine_s;
void bpmp_engine_reserve(const struct engine_s *engine);
void bpmp_engine_release(const struct engine_s *engine);

#define SYSTEM_ENGINE_RESERVE(engine) bpmp_engine_reserve(engine)
#define SYSTEM_ENGINE_RELEASE(engine) bpmp_engine_release(engine)

#if SE_RD_DEBUG

/* assert.h does not define proper DEBUG_ASSERT()
 * unless LK_DEBUGLEVEL > 1 (it's not) => redefine it here
 */
#undef DEBUG_ASSERT
#define DEBUG_ASSERT(x) \
  do { if (BOOL_IS_TRUE(unlikely(!BOOL_IS_TRUE(x)))) {			\
      panic("DEBUG ASSERT FAILED at (%s:%d): %s\n", __FILE__, __LINE__, #x); } } while (false)

/* Rejects only NULL paddr for now in BPMP (XXX TODO: improve this)
 *
 * XXXX Should trap everything that cannot be accessed by the SE ENGINE DMA
 * XXXX  if data is in some internal memory => that may not be valid (I do not know
 * XXXX  the address ranges to trap with BPMP). If this traps, data needs to be placed
 * XXXX  somewhere else before calling the trapped function.
 */
#define VALID_PADDR_SE_RANGE(paddr, size) ((paddr) != (PHYS_ADDR_T)(uintptr_t)NULL)

#define SE_PHYS_RANGE_CHECK(paddr, size)				\
  do {									\
    if (!VALID_PADDR_SE_RANGE((paddr),(size))) {			\
      ret = SE_ERROR(ERR_OUT_OF_RANGE);					\
      LTRACEF("Phys addr 0x%llx..0x%llx (size %u) not SE HW accessible (%d)\n", \
	      (paddr), (paddr)+(size), (size), ret);			\
      goto fail;							\
    }									\
  } while(false)

static __inline status_t
log_get_SE_ERROR(status_t ecode, const char *ename, const char *func, uint32_t line)
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

/* If nonzero: trace code execution, output other trace data as well */
#ifndef MODULE_TRACE
#define MODULE_TRACE  0
#endif

/* BPMP uses AST mapped phys addresses, assuming all DMA accesses are OK for now */
#define DEBUG_ASSERT_PHYS_DMA_OK(addr,size)

#if MODULE_TRACE

/* To get location info in error messages when debugging with trace:
 * use ltrace for this as well...
 */
#undef  ERROR_MESSAGE
#define ERROR_MESSAGE LTRACEF

#define LOG_HEXBUF(name,base,len) do { LTRACEF(""); dump_data_object(name, (const uint8_t *)base, len, len); } while(false)
#endif

#define DUMP_DATA_PARAMS(name,flags,dp)					\
  { struct se_data_params *__di = dp;					\
    if (__di) {								\
      LTRACEF("%s [" #dp "(%p)] => src=%p input_size=%u: dst=%p output_size=%u\n", \
	      name,__di,__di->src,__di->input_size,__di->dst,__di->output_size); \
      if (((flags) & 0x1U) && __di->src) { LOG_HEXBUF(" " #dp "->src:", __di->src, __di->input_size); } \
      if (((flags) & 0x2U) && __di->dst) { LOG_HEXBUF(" " #dp "->dst:", __di->dst, __di->output_size); } \
    }									\
  }

#else /* SE_RD_DEBUG */

#define SE_PHYS_RANGE_CHECK(paddr, size)
#define SE_ERROR(x) x
#define TRACE_REG_OPS 0
#define MODULE_TRACE  0

#undef LTRACEF

#endif	/* SE_RD_DEBUG */

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

#define TEGRA_PERF_NONE		0x0000U
#define TEGRA_PERF_CONTEXT	0x0001U
#define TEGRA_PERF_ENGINE	0x0002U

/* Perf monitor the crypto ops from context setup to dofinal() completed */
//#define TEGRA_MEASURE_PERFORMANCE TEGRA_PERF_CONTEXT

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

#if 0	// Do not use for now => better crypto perf.
void     udelay(uint32_t n);
#define TEGRA_UDELAY(usec) udelay(usec)
#endif

uint32_t usec_counter(void);
#define GET_USEC_TIMESTAMP usec_counter()
#endif /* HAVE_USEC_COUNTER */

#ifndef TEGRA_UDELAY

#define TEGRA_UDELAY(usec)

/* If there is no decent delay function, use larger
 * poll loop max value for PKA1 operation wait complete.
 */
#define TEGRA_DELAY_FUNCTION_UNDEFINED 1

/* with zero defined delay value => TEGRA_UDELAY is not called
 * in the corresponding polling loop.
 */
#define TEGRA_UDELAY_VALUE_SE0_OP_INITIAL   0 // initial delay before polling the status
#define TEGRA_UDELAY_VALUE_SE0_OP	    0	// SE engine operation complete poll interval
#define TEGRA_UDELAY_VALUE_SE0_MUTEX	    0	// SE mutex lock poll interval

#define TEGRA_UDELAY_VALUE_PKA1_OP_INITIAL  0  // initial delay before polling the status
#define TEGRA_UDELAY_VALUE_PKA1_OP   	    0	// PKA1 operation complete poll interval
#define TEGRA_UDELAY_VALUE_PKA1_MUTEX	    0	// PKA1 mutex lock poll interval

#else

/* with zero defined delay value => TEGRA_UDELAY is not called in the
 * corresponding polling loop. Use minimal values for Trusty with
 * udelay() to avoid crypto performance drops.
 *
 * I guess there is not much pwr consumption diff in polling a SE
 * status register and polling a usec timer register => polling SE
 * status register is better for performance.
 */
#define TEGRA_UDELAY_VALUE_SE0_OP_INITIAL   1 // initial delay before polling the status
#define TEGRA_UDELAY_VALUE_SE0_OP	    1	// SE engine operation complete poll interval
#define TEGRA_UDELAY_VALUE_SE0_MUTEX	    1	// SE mutex lock poll interval

#define TEGRA_UDELAY_VALUE_PKA1_OP_INITIAL  1  // initial delay before polling the status
#define TEGRA_UDELAY_VALUE_PKA1_OP   	    1	// PKA1 operation complete poll interval
#define TEGRA_UDELAY_VALUE_PKA1_MUTEX	    1	// PKA1 mutex lock poll interval

#define TEGRA_DELAY_FUNCTION_UNDEFINED	    0

#endif /* TEGRA_UDELAY */

/* This needs to return aligned memory chunks which can be accessed
 * with SE DMA.
 *
 * Further, allocates physically contiguous memory pages (in case size > 4096)
 *
 * The SE driver SE units using DMA no longer support scatter lists in HW.
 * The code supports reading non-contiguous data to the engines for most algorithms
 * if HAVE_NC_REGIONS is defined. Otherwise the data needs to be
 * physically contiquous from [ <base> .. <base> + <size - 1> ].
 *
 * Note:the SE engine is now configured to write out output of all
 * possible results to HW registers instead of memory. This has the
 * benefit that the caches do not need to be cleared and data does not
 * need to be cache line aligned for accessing the output data written
 * by SE HW.
 *
 * The AES block cipher output is unfortunately still an exception
 * (AES output => to memory). [CMAC-AES is written to registers, as
 * well as SHA and RSA, so CMAC is not an issue].
 *
 * This means that AES output buffers need special care
 * and attention.
 *
 * NOTE: SE DRNG is OK (all cases handled), so AES cipher output is now the
 * only remaining issue.
 *
 * Since SE HW no longer has linked list supports since T186 => need to
 * implement something like that in SW for AES input and output handling.
 * TODO => FIXME!!!!
 */
void *bpmp_get_contiguous_zeroed_memory(uint32_t align, size_t size, const char *fun, uint32_t line);
void bpmp_release_contiguous_memory(void *addr, const char *fun, uint32_t line);

#define GET_CONTIGUOUS_ZEROED_MEMORY(align, size) \
  bpmp_get_contiguous_zeroed_memory(align, size, __func__, __LINE__)

#define RELEASE_CONTIGUOUS_MEMORY(addr) \
  bpmp_release_contiguous_memory(addr, __func__, __LINE__)

/* Allocating zeroed memory which does not need not be contiguous or aligned
 * (i.e. it is not accessed by DMA engine with phys addresses)
 */
void *bpmp_get_zeroed_memory(size_t size, const char *fun, uint32_t line);
void bpmp_release_memory(void *addr, const char *fun, uint32_t line);

#define GET_ZEROED_MEMORY(size)			\
  bpmp_get_zeroed_memory(size, __func__, __LINE__)

#define RELEASE_MEMORY(addr) \
  bpmp_release_memory(addr, __func__, __LINE__)

#if TEGRA_MEASURE_MEMORY_USAGE
#define MEASURE_MEMORY_START(purpose)					\
  do { bool _mes_start = measure_mem_start(purpose, __func__, __LINE__); \
    if (!BOOL_IS_TRUE(_mes_start)) {					\
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
 * Currently the values are like this, but please check se_cdev_id_t
 * for the actual mapping, each with it's own base address:
 *
 *  SE_CDEV_SE0  = 0,
 *  SE_CDEV_PKA1 = 1,
 *  SE_CDEV_RNG1 = 2,
 *  SE_CDEV_SE1  = 3,
 *
 * Must return non-zero on error.
 *
 * If the function returns 0 and the *base_p == NULL then the device does not
 * exist in that system.
 */
#define SYSTEM_MAP_DEVICE bpmp_map_crypto_device

/* Map SE0, SE1, PKA1 and RNG1 and set the base address to the arg 
 * in Trusty. Returning NULL base address is treated like engine does not exist,
 * disabling that engine.
 *
 * Return non-zero value to indicate an error (or just panic the system).
 */
status_t SYSTEM_MAP_DEVICE(uint32_t device_id, void **base_p);

/* This initializes the SE/HW driver in tegra_cdev.c, called once
 * when initializing the system.
 */
status_t tegra_init_crypto_devices(void);

/* For R&D debug data dumps and other utility functions */
#include <crypto_util.h>

#endif /* __CRYPTO_SYSTEM_CONFIG_H__ */
