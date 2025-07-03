 /*
 * Copyright (c) 2018-2021, LWPU Corporation.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#include <crypto_system_config.h>

#include <crypto_measure.h>
#include <tegra_cdev.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifndef HAVE_TRAP_INCONSISTENT_MEMORY_MACROS
#define HAVE_TRAP_INCONSISTENT_MEMORY_MACROS 0
#endif

#if TEGRA_MEASURE_PERFORMANCE

#ifndef HAVE_COMPLEX_FMT
#define HAVE_COMPLEX_FMT 0
#endif

uint32_t get_elapsed_usec(uint32_t start_usec)
{
	uint32_t now = GET_USEC_TIMESTAMP;
	uint32_t elapsed_usec = 0U;

	if (now < start_usec) {
		elapsed_usec = (0xFFFFFFFFU) - start_usec + now;
	} else {
		elapsed_usec = now - start_usec;
	}
	return elapsed_usec;
}

void show_elapsed_time(const char *id_str, uint32_t start_usec)
{
	if (NULL == id_str) {
		id_str = "";
	}

	if (0U == start_usec) {
		CCC_ERROR_MESSAGE("PERF ERROR => %s: start time undefined\n", id_str);
	}

	LOG_ALWAYS("%s: elapsed time => %u uSec\n",
		   id_str, get_elapsed_usec(start_usec));
}

static struct {
	engine_id_t mp_eid;
	uint32_t    mp_start_us;
	const char *mp_fun;
	uint32_t    mp_line;
} eperf[] = {
	{ CCC_ENGINE_SE0_AES0, 0U, NULL, 0U },
	{ CCC_ENGINE_SE0_AES1, 0U, NULL, 0U },
	{ CCC_ENGINE_SE0_PKA0, 0U, NULL, 0U },
	{ CCC_ENGINE_SE0_SHA,  0U, NULL, 0U },
	{ CCC_ENGINE_SE1_AES0, 0U, NULL, 0U },
	{ CCC_ENGINE_SE1_AES1, 0U, NULL, 0U },
	{ CCC_ENGINE_SE1_PKA0, 0U, NULL, 0U },
	{ CCC_ENGINE_SE1_SHA,  0U, NULL, 0U },
	{ CCC_ENGINE_PKA,	   0U, NULL, 0U },
	{ CCC_ENGINE_RNG1,	   0U, NULL, 0U },
	{ CCC_ENGINE_SOFT,     0U, NULL, 0U },
	{ CCC_ENGINE_NONE,     0U, NULL, 0U },
};

void measure_perf_locked_operation_mark_start_fl(const engine_t *e, const char *fun, uint32_t line)
{
	if (NULL != e) {
		uint32_t inx = 0U;
		while (eperf[inx].mp_eid != CCC_ENGINE_NONE) {
			if (e->e_id == eperf[inx].mp_eid) {
				eperf[inx].mp_start_us = GET_USEC_TIMESTAMP;
				eperf[inx].mp_fun = fun;
				eperf[inx].mp_line = line;
				break;
			}
			inx++;
		}
	}
}

void measure_perf_locked_operation_show_result(const engine_t *e)
{
	if (NULL != e) {
		uint32_t inx = 0U;
		uint32_t tstamp = 0U;

		while (eperf[inx].mp_eid != CCC_ENGINE_NONE) {
			if (e->e_id == eperf[inx].mp_eid) {
				const char *fun = eperf[inx].mp_fun;
				uint32_t line   = eperf[inx].mp_line;

				if (NULL == fun) {
					fun = "";
				}

				tstamp = eperf[inx].mp_start_us;
				eperf[inx].mp_start_us = 0U;

				if (tstamp == 0U) {
					CCC_ERROR_MESSAGE("Engine %s mutex lock timestamp not set\n",
						      e->e_name);
				} else {
					tstamp = get_elapsed_usec(tstamp);
					CCC_ERROR_MESSAGE("Engine %s locked op time: %u usec [%s:%u]\n",
							  e->e_name, tstamp, fun, line);
				}
				break;
			}
			inx++;
		}
	}
}

#if TEGRA_MEASURE_MCP

#ifndef MCP_SIZE
/* Override table size in makefile if required */
#define MCP_SIZE 512
#endif

typedef struct mcp_s {
	uint32_t tstamp;
	uint32_t line;
	const char *info;
	const char *fun;
} mcp_t;

static mcp_t mcp[MCP_SIZE];
static uint32_t mcp_index;
static uint32_t mcp_epoch;

void measure_mcp_init(void)
{
	mcp_index = 0U;
	mcp_epoch = 0U;
	se_util_mem_set((uint8_t *)&mcp[0], 0U, sizeof_u32(mcp));
}

static bool mcp_show_entry(uint32_t inx)
{
	bool rval = false;

	if (0U != mcp[inx].tstamp) {
		uint32_t stamp = mcp[inx].tstamp;
		uint32_t elapsed_usec = 0U;

		/* In case CCC_ERROR_MESSAGE / LOG_ALWAYS is defined to do nothing.
		 *  If you see this error in your compilation, you need to redefine
		 * CCC_ERROR_MESSAGE macro to actually output something.
		 *
		 * See MB2 (lwtboot) crypto config file for an example on how to
		 * deal with this.
		 */
		if (stamp < mcp_epoch) {
			elapsed_usec = (0xFFFFFFFFU) - mcp_epoch + stamp;
		} else {
			elapsed_usec = stamp - mcp_epoch;
		}

#if HAVE_COMPLEX_FMT
		CCC_ERROR_MESSAGE("[%.3u] %.5u => %s [%s:%u]\n",
			      inx, elapsed_usec, mcp[inx].info,
			      mcp[inx].fun, mcp[inx].line);
#else
		/* In case the log function does not support the
		 * field length modifiers in format strings use this version
		 */
		CCC_ERROR_MESSAGE("[%u] %u => %s [%s:%u]\n",
			      inx, elapsed_usec, mcp[inx].info,
			      mcp[inx].fun, mcp[inx].line);
#endif
	} else {
		rval = true;
	}

	return rval;
}

void measure_mcp_results(bool init)
{
	uint32_t inx = 0U;

	if (0U != mcp_epoch) {

		CCC_ERROR_MESSAGE("mcp entries: %u\n", mcp_index);

		while(inx < mcp_index) {
			if (BOOL_IS_TRUE(mcp_show_entry(inx++))) {
				break;
			}
		}

		if (BOOL_IS_TRUE(init)) {
			measure_mcp_init();
		}
	}
}

void measure_mcp_waypoint(const char *info, uint32_t line, const char *fun)
{
	if (mcp_index < MCP_SIZE) {
		uint32_t tstamp = GET_USEC_TIMESTAMP;

		mcp[mcp_index].tstamp = tstamp;
		mcp[mcp_index].line   = line;
		mcp[mcp_index].fun    = ((NULL != fun) ? fun : "");
		mcp[mcp_index].info   = ((NULL != info) ? info : "");

		if (mcp_epoch == 0U) {
			mcp_epoch = tstamp;
		}

		mcp_index++;

		if (mcp_index == (MCP_SIZE - 1)) {
			CCC_ERROR_MESSAGE("MCP table full\n");
		}
	}
}
#endif /* TEGRA_MEASURE_MCP */

#endif /* TEGRA_MEASURE_PERFORMANCE */

#if TEGRA_MEASURE_MEMORY_USAGE

#ifndef CCC_MEM_LOG
#define CCC_MEM_LOG LOG_INFO
#endif

/* Simple single activation memory measuring and object tracer for the crypto ops
 *
 * This is the max supported objects. Increase if required or colwert to a list if
 * you need something more complex.
 */
#ifndef CRYPTO_MAX_ALLOCATIONS
#define CRYPTO_MAX_ALLOCATIONS 10
#endif

static struct {
	bool     mm_initialized;
	const char *mm_init_purpose;
	const char *mm_init_fun;
	uint32_t mm_init_line;
	uint32_t mm_lwrrent;
	uint32_t mm_max;
	uint32_t mm_used;
	uint32_t mm_all_allocations;
} mes_mem;

struct mem_obj_s {
	const char *mo_fun;
	uint32_t    mo_line;
	uint32_t    mo_size;
	void       *mo_addr;
	bool	    mo_contiguous;
};

static struct mem_obj_s mem_obj[ CRYPTO_MAX_ALLOCATIONS ];

bool measure_mem_get_active_state(const char *fun, uint32_t line)
{
	bool state = false;
	uint32_t inx = 0U;

	if (NULL == fun) {
		fun = "";
	}

	if (!BOOL_IS_TRUE(mes_mem.mm_initialized)) {
		CCC_MEM_LOG("Measure mem: [%s:%u] not active\n", fun, line);
	} else {
		state = true;
		CCC_MEM_LOG("Measure mem: [%s:%u] activated from [%s:%u]\n",
			 fun, line, mes_mem.mm_init_fun, mes_mem.mm_init_line);
		CCC_MEM_LOG("Measure mem: current mem use %u bytes\n", mes_mem.mm_lwrrent);
		CCC_MEM_LOG("Measure mem: max mem use     %u bytes\n", mes_mem.mm_max);
		if (mes_mem.mm_all_allocations > mes_mem.mm_max) {
			CCC_MEM_LOG("Measure mem: all allocations %u bytes (when released memory not accounted)\n", mes_mem.mm_all_allocations);
		}
		if (mes_mem.mm_used > 0U) {
			CCC_MEM_LOG("Measure mem: active objects\n");
			for (inx = 0U; inx < mes_mem.mm_used; inx++) {
				CCC_MEM_LOG("Measure mem: object [%s:%u] => %p (size %u bytes)\n",
					 mem_obj[inx].mo_fun, mem_obj[inx].mo_line,
					 mem_obj[inx].mo_addr, mem_obj[inx].mo_size);
			}
		}
	}

	return state;
}

void measure_mem_stop(const char *fun, uint32_t line)
{
	if (NULL == fun) {
		fun = "";
	}

	if (BOOL_IS_TRUE(mes_mem.mm_initialized)) {
		CCC_MEM_LOG("Measure mem: [%s:%u] stopping trace activated from [%s:%u]\n",
			 fun, line, mes_mem.mm_init_fun, mes_mem.mm_init_line);

		(void)measure_mem_get_active_state(fun, line);

		if (mes_mem.mm_lwrrent != 0U) {
			CCC_MEM_LOG("Measure mem: WARNING: current allocation is not zero (%u bytes allocated)\n",
				 mes_mem.mm_lwrrent);
		}
	}
	mes_mem.mm_initialized = false;
}

bool measure_mem_start(const char *purpose, const char *fun, uint32_t line)
{
	bool started = false;

	if (NULL == fun) {
		fun = "";
	}
	if (NULL == purpose) {
		purpose = "";
	}

	if (BOOL_IS_TRUE(mes_mem.mm_initialized)) {
		CCC_MEM_LOG("Measure mem: [%s:%u] already active. Only single activation measuring supported for now\n",
			 fun, line);
	} else {
		CCC_MEM_LOG("Measure mem: %s [%s:%u] activated\n", purpose, fun, line);

		se_util_mem_set((uint8_t *)&mes_mem, 0U, sizeof_u32(mes_mem));

		mes_mem.mm_init_purpose = purpose;
		mes_mem.mm_init_fun    = fun;
		mes_mem.mm_init_line   = line;
		mes_mem.mm_initialized = true;
		started = true;
	}

	return started;
}

/* TODO: Could trace the objects later... */
void measure_mem_allocate(void *addr, uint64_t size64,
			  const char *fun, uint32_t line, bool contiguous)
{
	uint32_t size = se_util_lsw_u64(size64);
	(void)addr;

	if (NULL == fun) {
		fun = "";
	}

	if (!BOOL_IS_TRUE(mes_mem.mm_initialized)) {
		CCC_MEM_LOG("Measure mem: [%s:%u] not active (allocate %u bytes (%p))\n",
			 fun, line, size, addr);
	} else {
		CCC_MEM_LOG("Measure mem [%s:%u]: allocating %u bytes\n", fun, line, size);

		if (mes_mem.mm_used >= CRYPTO_MAX_ALLOCATIONS) {
			CCC_ERROR_MESSAGE("Crypto mem allocation trace overflow => increase CRYPTO_MAX_ALLOCATIONS (now %u)\n",
				      CRYPTO_MAX_ALLOCATIONS);
			measure_mem_stop(fun, line);
		} else {
			struct mem_obj_s *mo = &mem_obj[mes_mem.mm_used++];
			mo->mo_fun = fun;
			mo->mo_line = line;
			mo->mo_size = size;
			mo->mo_addr = addr;
			mo->mo_contiguous = contiguous;

			/* current and max size track */
			mes_mem.mm_lwrrent += size;
			mes_mem.mm_all_allocations += size;

			if (mes_mem.mm_lwrrent > mes_mem.mm_max) {
				mes_mem.mm_max = mes_mem.mm_lwrrent;
			}
		}
	}
}

void measure_mem_release(void *addr, const char *fun, uint32_t line, bool contiguous)
{
	bool removed = false;
	uint32_t inx = 0U;

	if (NULL == fun) {
		fun = "";
	}

	if (!BOOL_IS_TRUE(mes_mem.mm_initialized)) {
		CCC_MEM_LOG("Measure mem: [%s:%u] not active (release %p object)\n",
			 fun, line, addr);
	} else {
		for (inx = 0U; inx < mes_mem.mm_used; inx++) {
			if (mem_obj[inx].mo_addr == addr) {
				uint32_t size = mem_obj[inx].mo_size;
				CCC_MEM_LOG("Measure mem [%s:%u]: releasing %p (%u bytes)\n", fun, line, addr, size);
				mes_mem.mm_lwrrent -= size;
				removed = true;

				/* Checks that the reserve / release types match */
				if (contiguous != mem_obj[inx].mo_contiguous) {
					if (BOOL_IS_TRUE(mem_obj[inx].mo_contiguous)) {
						CCC_ERROR_MESSAGE("ERROR: Contiguous memory released with non-contiguous release!!!\n");
					} else {
						CCC_ERROR_MESSAGE("ERROR: Non-contiguous memory released with contiguous release!!!\n");
					}
#if HAVE_TRAP_INCONSISTENT_MEMORY_MACROS
					/* Contiguos allocation with non-contiguous release
					 * or vice versa => trap if so configured.
					 */
					DEBUG_ASSERT(0);
#endif
				}
			} else {
				if (BOOL_IS_TRUE(removed)) {
					/* Move one by one (just in case, because overlap
					 * if moving multiple elements) Perf does not matter
					 * in this case anyway...
					 */
					mem_obj[inx-1] = mem_obj[inx];
				}
			}
		}

		if (BOOL_IS_TRUE(removed)) {
			mes_mem.mm_used--;
			mem_obj[mes_mem.mm_used].mo_fun = "released entry";
		} else {
			CCC_MEM_LOG("Measure mem: [%s:%u] WARNING: object %p released, but it was not tracked\n",
				 fun, line, addr);
		}
	}
}
#endif /* TEGRA_MEASURE_MEMORY_USAGE */
