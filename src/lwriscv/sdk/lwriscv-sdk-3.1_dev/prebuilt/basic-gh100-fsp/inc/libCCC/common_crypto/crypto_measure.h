/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
#ifndef INCLUDED_CRYPTO_MEASURE_H
#define INCLUDED_CRYPTO_MEASURE_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#ifndef TEGRA_MEASURE_PERFORMANCE
#define TEGRA_MEASURE_PERFORMANCE 0
#endif

#ifndef TEGRA_PERF_CONTEXT
#define TEGRA_PERF_CONTEXT	0x0001U
#endif
#ifndef TEGRA_PERF_ENGINE
#define TEGRA_PERF_ENGINE	0x0002U
#endif
#ifndef TEGRA_PERF_MCP
#define TEGRA_PERF_MCP		0x0010U
#endif

#if (TEGRA_PERF_CONTEXT == 0) || (TEGRA_PERF_ENGINE == 0) || (TEGRA_PERF_MCP == 0)
#error "TEGRA_PERF_xxx value incorrectly defined to zero"
#endif

#if TEGRA_MEASURE_PERFORMANCE == TEGRA_PERF_MCP

#if MODULE_TRACE
#error "MCP timing traces can not be enabled when logging is enabled"
#endif

/* feature: CCC waypoint time measurement */
#define TEGRA_MEASURE_MCP 1
#define MCP(info) measure_mcp_waypoint(info, __LINE__, __func__)

#if !defined(LOCAL_TRACE) || !LOCAL_TRACE
/* Colwert debug log trace output macro into waypoint timestamp
 * collecting checkpoints.
 *
 * Since most functions start and end with this macro this effectively
 * collects all function exelwtion times in microsecond granularity.
 */
#ifdef LTRACEF
#undef LTRACEF
#endif
#define LTRACEF(str, x...) MCP(#str)
#endif

#else

#define TEGRA_MEASURE_MCP 0
#ifndef MCP
#define MCP(info)
#endif

#endif /*  TEGRA_MEASURE_PERFORMANCE == TEGRA_PERF_MCP */

#define MEASURE_ENGINE_PERF(pv) \
	((TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_ENGINE) && (pv))

#if TEGRA_MEASURE_PERFORMANCE

#ifndef GET_USEC_TIMESTAMP
#error "Need microsecond timestamps for performance measuring"
#endif

uint32_t get_elapsed_usec(uint32_t start_usec);
void show_elapsed_time(const char *id_str, uint32_t start_usec);

struct engine_s;

void measure_perf_locked_operation_mark_start_fl(const struct engine_s *e, const char *fun, uint32_t line);

/* Backwards compatibility macro naming
 */
#define measure_perf_locked_operation_mark_start(eng) \
	measure_perf_locked_operation_mark_start_fl(eng, __func__, __LINE__)

void measure_perf_locked_operation_show_result(const struct engine_s *e);

#if TEGRA_MEASURE_MCP
void measure_mcp_init(void);
void measure_mcp_results(bool init);
void measure_mcp_waypoint(const char *info, uint32_t line, const char *fun);
#endif

#endif /* TEGRA_MEASURE_PERFORMANCE */

#if TEGRA_MEASURE_MEMORY_USAGE
bool measure_mem_get_active_state(const char *fun, uint32_t line);
void measure_mem_stop(const char *fun, uint32_t line);
bool measure_mem_start(const char *purpose, const char *fun, uint32_t line);
void measure_mem_allocate(void *addr, uint64_t size,
			  const char *fun, uint32_t line, bool contiguous);
void measure_mem_release(void *addr, const char *fun, uint32_t line, bool contiguous);
#endif /* TEGRA_MEASURE_MEMORY_USAGE */

#endif /* INCLUDED_CRYPTO_MEASURE_H */
