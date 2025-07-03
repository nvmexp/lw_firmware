/*
 * Copyright 1993-2012 LWPU Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to LWPU intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to LWPU and is being provided under the terms and
 * conditions of a form of LWPU software license agreement by and
 * between LWPU and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of LWPU is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, LWPU MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * LWPU DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL LWPU BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#if !defined(__LWDA_PROFILER_API_H__)
#define __LWDA_PROFILER_API_H__

#include "driver_types.h"

#if defined(__LWDA_API_VERSION_INTERNAL) || defined(__DOXYGEN_ONLY__) || defined(LWDA_ENABLE_DEPRECATED)
#define __LWDA_DEPRECATED
#elif defined(_MSC_VER)
#define __LWDA_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__)
#define __LWDA_DEPRECATED __attribute__((deprecated))
#else
#define __LWDA_DEPRECATED
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * \ingroup LWDART
 * \defgroup LWDART_PROFILER_DEPRECATED Profiler Control [DEPRECATED]
 *
 * ___MANBRIEF___ profiler control functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the profiler control functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Initialize the LWCA profiler.
 *
 * \deprecated
 *	
 * Using this API user can initialize the LWCA profiler by specifying
 * the configuration file, output file and output file format. This
 * API is generally used to profile different set of counters by
 * looping the kernel launch. The \p configFile parameter can be used
 * to select profiling options including profiler counters. Refer to
 * the "Compute Command Line Profiler User Guide" for supported
 * profiler options and counters.
 *
 * Limitation: The LWCA profiler cannot be initialized with this API
 * if another profiling tool is already active, as indicated by the
 * ::lwdaErrorProfilerDisabled return code.
 *
 * Typical usage of the profiling APIs is as follows: 
 *
 * for each set of counters/options\n
 * {\n
 *      lwdaProfilerInitialize(); //Initialize profiling,set the counters/options in 
 * the config file \n
 *      ...\n
 *      lwdaProfilerStart(); \n
 *      // code to be profiled \n
 *      lwdaProfilerStop();\n
 *      ...\n
 *      lwdaProfilerStart(); \n
 *      // code to be profiled \n
 *      lwdaProfilerStop();\n
 *      ...\n
 * }\n
 *
 *
 * \param configFile - Name of the config file that lists the counters/options
 * for profiling.
 * \param outputFile - Name of the outputFile where the profiling results will
 * be stored.
 * \param outputMode - outputMode, can be ::lwdaKeyValuePair OR ::lwdaCSV.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorProfilerDisabled
 * \notefnerr
 *
 * \sa
 * ::lwdaProfilerStart,
 * ::lwdaProfilerStop,
 * ::lwProfilerInitialize
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaProfilerInitialize(const char *configFile, 
                                                             const char *outputFile, 
                                                             lwdaOutputMode_t outputMode);

/** @} */ /* END LWDART_PROFILER_DEPRECATED */

/**
 * \ingroup LWDART
 * \defgroup LWDART_PROFILER Profiler Control
 *
 * ___MANBRIEF___ profiler control functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the profiler control functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Enable profiling.
 *
 * Enables profile collection by the active profiling tool for the
 * current context. If profiling is already enabled, then
 * lwdaProfilerStart() has no effect.
 *
 * lwdaProfilerStart and lwdaProfilerStop APIs are used to
 * programmatically control the profiling granularity by allowing
 * profiling to be done only on selective pieces of code.
 * 
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 *
 * \sa
 * ::lwdaProfilerInitialize,
 * ::lwdaProfilerStop,
 * ::lwProfilerStart
 */
extern __host__ lwdaError_t LWDARTAPI lwdaProfilerStart(void);

/**
 * \brief Disable profiling.
 *
 * Disables profile collection by the active profiling tool for the
 * current context. If profiling is already disabled, then
 * lwdaProfilerStop() has no effect.
 *
 * lwdaProfilerStart and lwdaProfilerStop APIs are used to
 * programmatically control the profiling granularity by allowing
 * profiling to be done only on selective pieces of code.
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 *
 * \sa
 * ::lwdaProfilerInitialize,
 * ::lwdaProfilerStart,
 * ::lwProfilerStop
 */
extern __host__ lwdaError_t LWDARTAPI lwdaProfilerStop(void);

/** @} */ /* END LWDART_PROFILER */

#undef __LWDA_DEPRECATED

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* !__LWDA_PROFILER_API_H__ */

