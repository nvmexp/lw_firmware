/*
 * Copyright 1993-2018 LWPU Corporation.  All rights reserved.
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



#if !defined(__LWDA_RUNTIME_API_H__)
#define __LWDA_RUNTIME_API_H__

#if !defined(__LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__)
#define __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#define __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDA_RUNTIME_API_H__
#endif

/**
 * \latexonly
 * \page sync_async API synchronization behavior
 *
 * \section memcpy_sync_async_behavior Memcpy
 * The API provides memcpy/memset functions in both synchronous and asynchronous forms,
 * the latter having an \e "Async" suffix. This is a misnomer as each function
 * may exhibit synchronous or asynchronous behavior depending on the arguments
 * passed to the function. In the reference documentation, each memcpy function is
 * categorized as \e synchronous or \e asynchronous, corresponding to the definitions
 * below.
 * 
 * \subsection MemcpySynchronousBehavior Synchronous
 * 
 * <ol>
 * <li> For transfers from pageable host memory to device memory, a stream sync is performed
 * before the copy is initiated. The function will return once the pageable
 * buffer has been copied to the staging memory for DMA transfer to device memory,
 * but the DMA to final destination may not have completed.
 * 
 * <li> For transfers from pinned host memory to device memory, the function is synchronous
 * with respect to the host.
 *
 * <li> For transfers from device to either pageable or pinned host memory, the function returns
 * only once the copy has completed.
 * 
 * <li> For transfers from device memory to device memory, no host-side synchronization is
 * performed.
 *
 * <li> For transfers from any host memory to any host memory, the function is fully
 * synchronous with respect to the host.
 * </ol>
 * 
 * \subsection MemcpyAsynchronousBehavior Asynchronous
 *
 * <ol>
 * <li> For transfers from device memory to pageable host memory, the function
 * will return only once the copy has completed.
 *
 * <li> For transfers from any host memory to any host memory, the function is fully
 * synchronous with respect to the host.
 * 
 * <li> For all other transfers, the function is fully asynchronous. If pageable
 * memory must first be staged to pinned memory, this will be handled
 * asynchronously with a worker thread.
 * </ol>
 *
 * \section memset_sync_async_behavior Memset
 * The lwdaMemset functions are asynchronous with respect to the host
 * except when the target memory is pinned host memory. The \e Async
 * versions are always asynchronous with respect to the host.
 *
 * \section kernel_launch_details Kernel Launches
 * Kernel launches are asynchronous with respect to the host. Details of
 * conlwrrent kernel exelwtion and data transfers can be found in the LWCA
 * Programmers Guide.
 *
 * \endlatexonly
 */

/**
 * There are two levels for the runtime API.
 *
 * The C API (<i>lwda_runtime_api.h</i>) is
 * a C-style interface that does not require compiling with \p lwcc.
 *
 * The \ref LWDART_HIGHLEVEL "C++ API" (<i>lwda_runtime.h</i>) is a
 * C++-style interface built on top of the C API. It wraps some of the
 * C API routines, using overloading, references and default arguments.
 * These wrappers can be used from C++ code and can be compiled with any C++
 * compiler. The C++ API also has some LWCA-specific wrappers that wrap
 * C API routines that deal with symbols, textures, and device functions.
 * These wrappers require the use of \p lwcc because they depend on code being
 * generated by the compiler. For example, the exelwtion configuration syntax
 * to ilwoke kernels is only available in source code compiled with \p lwcc.
 */

/** LWCA Runtime API Version */
#define LWDART_VERSION  11070

#if defined(__LWDA_API_VER_MAJOR__) && defined(__LWDA_API_VER_MINOR__)
# define __LWDART_API_VERSION ((__LWDA_API_VER_MAJOR__ * 1000) + (__LWDA_API_VER_MINOR__ * 10))
#else
# define __LWDART_API_VERSION LWDART_VERSION
#endif

#ifndef __DOXYGEN_ONLY__
#include "crt/host_defines.h"
#endif
#include "builtin_types.h"

#include "lwda_device_runtime_api.h"

#if defined(LWDA_API_PER_THREAD_DEFAULT_STREAM) || defined(__LWDA_API_VERSION_INTERNAL)
    #define __LWDART_API_PER_THREAD_DEFAULT_STREAM
    #define __LWDART_API_PTDS(api) api ## _ptds
    #define __LWDART_API_PTSZ(api) api ## _ptsz
#else
    #define __LWDART_API_PTDS(api) api
    #define __LWDART_API_PTSZ(api) api
#endif

#define lwdaSignalExternalSemaphoresAsync  __LWDART_API_PTSZ(lwdaSignalExternalSemaphoresAsync_v2)
#define lwdaWaitExternalSemaphoresAsync    __LWDART_API_PTSZ(lwdaWaitExternalSemaphoresAsync_v2)

#if defined(__LWDART_API_PER_THREAD_DEFAULT_STREAM)
    #define lwdaMemcpy                     __LWDART_API_PTDS(lwdaMemcpy)
    #define lwdaMemcpyToSymbol             __LWDART_API_PTDS(lwdaMemcpyToSymbol)
    #define lwdaMemcpyFromSymbol           __LWDART_API_PTDS(lwdaMemcpyFromSymbol)
    #define lwdaMemcpy2D                   __LWDART_API_PTDS(lwdaMemcpy2D)
    #define lwdaMemcpyToArray              __LWDART_API_PTDS(lwdaMemcpyToArray)
    #define lwdaMemcpy2DToArray            __LWDART_API_PTDS(lwdaMemcpy2DToArray)
    #define lwdaMemcpyFromArray            __LWDART_API_PTDS(lwdaMemcpyFromArray)
    #define lwdaMemcpy2DFromArray          __LWDART_API_PTDS(lwdaMemcpy2DFromArray)
    #define lwdaMemcpyArrayToArray         __LWDART_API_PTDS(lwdaMemcpyArrayToArray)
    #define lwdaMemcpy2DArrayToArray       __LWDART_API_PTDS(lwdaMemcpy2DArrayToArray)
    #define lwdaMemcpy3D                   __LWDART_API_PTDS(lwdaMemcpy3D)
    #define lwdaMemcpy3DPeer               __LWDART_API_PTDS(lwdaMemcpy3DPeer)
    #define lwdaMemset                     __LWDART_API_PTDS(lwdaMemset)
    #define lwdaMemset2D                   __LWDART_API_PTDS(lwdaMemset2D)
    #define lwdaMemset3D                   __LWDART_API_PTDS(lwdaMemset3D)
    #define lwdaGraphUpload                __LWDART_API_PTSZ(lwdaGraphUpload)
    #define lwdaGraphLaunch                __LWDART_API_PTSZ(lwdaGraphLaunch)
    #define lwdaStreamBeginCapture         __LWDART_API_PTSZ(lwdaStreamBeginCapture)
    #define lwdaStreamEndCapture           __LWDART_API_PTSZ(lwdaStreamEndCapture)
    #define lwdaStreamGetCaptureInfo       __LWDART_API_PTSZ(lwdaStreamGetCaptureInfo)
    #define lwdaStreamGetCaptureInfo_v2    __LWDART_API_PTSZ(lwdaStreamGetCaptureInfo_v2)
    #define lwdaStreamIsCapturing          __LWDART_API_PTSZ(lwdaStreamIsCapturing)
    #define lwdaMemcpyAsync                __LWDART_API_PTSZ(lwdaMemcpyAsync)
    #define lwdaMemcpyToSymbolAsync        __LWDART_API_PTSZ(lwdaMemcpyToSymbolAsync)
    #define lwdaMemcpyFromSymbolAsync      __LWDART_API_PTSZ(lwdaMemcpyFromSymbolAsync)
    #define lwdaMemcpy2DAsync              __LWDART_API_PTSZ(lwdaMemcpy2DAsync)
    #define lwdaMemcpyToArrayAsync         __LWDART_API_PTSZ(lwdaMemcpyToArrayAsync)
    #define lwdaMemcpy2DToArrayAsync       __LWDART_API_PTSZ(lwdaMemcpy2DToArrayAsync)
    #define lwdaMemcpyFromArrayAsync       __LWDART_API_PTSZ(lwdaMemcpyFromArrayAsync)
    #define lwdaMemcpy2DFromArrayAsync     __LWDART_API_PTSZ(lwdaMemcpy2DFromArrayAsync)
    #define lwdaMemcpy3DAsync              __LWDART_API_PTSZ(lwdaMemcpy3DAsync)
    #define lwdaMemcpy3DPeerAsync          __LWDART_API_PTSZ(lwdaMemcpy3DPeerAsync)
    #define lwdaMemsetAsync                __LWDART_API_PTSZ(lwdaMemsetAsync)
    #define lwdaMemset2DAsync              __LWDART_API_PTSZ(lwdaMemset2DAsync)
    #define lwdaMemset3DAsync              __LWDART_API_PTSZ(lwdaMemset3DAsync)
    #define lwdaStreamQuery                __LWDART_API_PTSZ(lwdaStreamQuery)
    #define lwdaStreamGetFlags             __LWDART_API_PTSZ(lwdaStreamGetFlags)
    #define lwdaStreamGetPriority          __LWDART_API_PTSZ(lwdaStreamGetPriority)
    #define lwdaEventRecord                __LWDART_API_PTSZ(lwdaEventRecord)
    #define lwdaEventRecordWithFlags       __LWDART_API_PTSZ(lwdaEventRecordWithFlags)
    #define lwdaStreamWaitEvent            __LWDART_API_PTSZ(lwdaStreamWaitEvent)
    #define lwdaStreamAddCallback          __LWDART_API_PTSZ(lwdaStreamAddCallback)
    #define lwdaStreamAttachMemAsync       __LWDART_API_PTSZ(lwdaStreamAttachMemAsync)
    #define lwdaStreamSynchronize          __LWDART_API_PTSZ(lwdaStreamSynchronize)
    #define lwdaLaunchKernel               __LWDART_API_PTSZ(lwdaLaunchKernel)

    #define lwdaLaunchKernelEx             __LWDART_API_PTSZ(lwdaLaunchKernelEx)

    #define lwdaLaunchHostFunc             __LWDART_API_PTSZ(lwdaLaunchHostFunc)
    #define lwdaMemPrefetchAsync           __LWDART_API_PTSZ(lwdaMemPrefetchAsync)
    #define lwdaLaunchCooperativeKernel    __LWDART_API_PTSZ(lwdaLaunchCooperativeKernel)
    #define lwdaStreamCopyAttributes       __LWDART_API_PTSZ(lwdaStreamCopyAttributes)
    #define lwdaStreamGetAttribute         __LWDART_API_PTSZ(lwdaStreamGetAttribute)
    #define lwdaStreamSetAttribute         __LWDART_API_PTSZ(lwdaStreamSetAttribute)
    #define lwdaMallocAsync                __LWDART_API_PTSZ(lwdaMallocAsync)
    #define lwdaFreeAsync                  __LWDART_API_PTSZ(lwdaFreeAsync)
    #define lwdaMallocFromPoolAsync        __LWDART_API_PTSZ(lwdaMallocFromPoolAsync)
    #define lwdaGetDriverEntryPoint        __LWDART_API_PTSZ(lwdaGetDriverEntryPoint)
#endif

/** \cond impl_private */
#if !defined(__dv)

#if defined(__cplusplus)

#define __dv(v) \
        = v

#else /* __cplusplus */

#define __dv(v)

#endif /* __cplusplus */

#endif /* !__dv */
/** \endcond impl_private */

#if (!defined(__LWDA_ARCH__) || (__LWDA_ARCH__ >= 350))   /** Visible to SM>=3.5 and "__host__ __device__" only **/

#define LWDART_DEVICE __device__ 

#else

#define LWDART_DEVICE

#endif /** LWDART_DEVICE */

#if !defined(__LWDACC_RTC__)
#define EXCLUDE_FROM_RTC

/** \cond impl_private */
#if defined(__DOXYGEN_ONLY__) || defined(LWDA_ENABLE_DEPRECATED)
#define __LWDA_DEPRECATED
#elif defined(_MSC_VER)
#define __LWDA_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__)
#define __LWDA_DEPRECATED __attribute__((deprecated))
#else
#define __LWDA_DEPRECATED
#endif
/** \endcond impl_private */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * \defgroup LWDART_DEVICE Device Management
 *
 * ___MANBRIEF___ device management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the device management functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Destroy all allocations and reset all state on the current device
 * in the current process.
 *
 * Explicitly destroys and cleans up all resources associated with the current
 * device in the current process. It is the caller's responsibility to ensure
 * that the resources are not accessed or passed in subsequent API calls and
 * doing so will result in undefined behavior. These resources include LWCA types
 * such as ::lwdaStream_t, ::lwdaEvent_t, ::lwdaArray_t, ::lwdaMipmappedArray_t,
 * ::lwdaTextureObject_t, ::lwdaSurfaceObject_t, ::textureReference, ::surfaceReference,
 * ::lwdaExternalMemory_t, ::lwdaExternalSemaphore_t and ::lwdaGraphicsResource_t.
 * Any subsequent API call to this device will reinitialize the device.
 *
 * Note that this function will reset the device immediately.  It is the caller's
 * responsibility to ensure that the device is not being accessed by any 
 * other host threads from the process when this function is called.
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceSynchronize
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceReset(void);

/**
 * \brief Wait for compute device to finish
 *
 * Blocks until the device has completed all preceding requested tasks.
 * ::lwdaDeviceSynchronize() returns an error if one of the preceding tasks
 * has failed. If the ::lwdaDeviceScheduleBlockingSync flag was set for 
 * this device, the host thread will block until the device has finished 
 * its work.
 *
 * \return
 * ::lwdaSuccess
 * \note_device_sync_deprecated
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceReset,
 * ::lwCtxSynchronize
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceSynchronize(void);

/**
 * \brief Set resource limits
 *
 * Setting \p limit to \p value is a request by the application to update
 * the current limit maintained by the device.  The driver is free to
 * modify the requested value to meet h/w requirements (this could be
 * clamping to minimum or maximum values, rounding up to nearest element
 * size, etc).  The application can use ::lwdaDeviceGetLimit() to find out
 * exactly what the limit has been set to.
 *
 * Setting each ::lwdaLimit has its own specific restrictions, so each is
 * dislwssed here.
 *
 * - ::lwdaLimitStackSize controls the stack size in bytes of each GPU thread.
 *
 * - ::lwdaLimitPrintfFifoSize controls the size in bytes of the shared FIFO
 *   used by the ::printf() device system call. Setting
 *   ::lwdaLimitPrintfFifoSize must not be performed after launching any kernel
 *   that uses the ::printf() device system call - in such case
 *   ::lwdaErrorIlwalidValue will be returned.
 *
 * - ::lwdaLimitMallocHeapSize controls the size in bytes of the heap used by
 *   the ::malloc() and ::free() device system calls. Setting
 *   ::lwdaLimitMallocHeapSize must not be performed after launching any kernel
 *   that uses the ::malloc() or ::free() device system calls - in such case
 *   ::lwdaErrorIlwalidValue will be returned.
 *
 * - ::lwdaLimitDevRuntimeSyncDepth controls the maximum nesting depth of a
 *   grid at which a thread can safely call ::lwdaDeviceSynchronize(). Setting
 *   this limit must be performed before any launch of a kernel that uses the
 *   device runtime and calls ::lwdaDeviceSynchronize() above the default sync
 *   depth, two levels of grids. Calls to ::lwdaDeviceSynchronize() will fail
 *   with error code ::lwdaErrorSyncDepthExceeded if the limitation is
 *   violated. This limit can be set smaller than the default or up the maximum
 *   launch depth of 24. When setting this limit, keep in mind that additional
 *   levels of sync depth require the runtime to reserve large amounts of
 *   device memory which can no longer be used for user allocations. If these
 *   reservations of device memory fail, ::lwdaDeviceSetLimit will return
 *   ::lwdaErrorMemoryAllocation, and the limit can be reset to a lower value.
 *   This limit is only applicable to devices of compute capability 3.5 and
 *   higher. Attempting to set this limit on devices of compute capability less
 *   than 3.5 will result in the error ::lwdaErrorUnsupportedLimit being
 *   returned.
 *
 * - ::lwdaLimitDevRuntimePendingLaunchCount controls the maximum number of
 *   outstanding device runtime launches that can be made from the current
 *   device. A grid is outstanding from the point of launch up until the grid
 *   is known to have been completed. Device runtime launches which violate 
 *   this limitation fail and return ::lwdaErrorLaunchPendingCountExceeded when
 *   ::lwdaGetLastError() is called after launch. If more pending launches than
 *   the default (2048 launches) are needed for a module using the device
 *   runtime, this limit can be increased. Keep in mind that being able to
 *   sustain additional pending launches will require the runtime to reserve
 *   larger amounts of device memory upfront which can no longer be used for
 *   allocations. If these reservations fail, ::lwdaDeviceSetLimit will return
 *   ::lwdaErrorMemoryAllocation, and the limit can be reset to a lower value.
 *   This limit is only applicable to devices of compute capability 3.5 and
 *   higher. Attempting to set this limit on devices of compute capability less
 *   than 3.5 will result in the error ::lwdaErrorUnsupportedLimit being
 *   returned. 
 *
 * - ::lwdaLimitMaxL2FetchGranularity controls the L2 cache fetch granularity.
 *   Values can range from 0B to 128B. This is purely a performance hint and
 *   it can be ignored or clamped depending on the platform.
 *
 * - ::lwdaLimitPersistingL2CacheSize controls size in bytes available
 *   for persisting L2 cache. This is purely a performance hint and it
 *   can be ignored or clamped depending on the platform.
 *
 * \param limit - Limit to set
 * \param value - Size of limit
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnsupportedLimit,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceGetLimit,
 * ::lwCtxSetLimit
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceSetLimit(enum lwdaLimit limit, size_t value);

/**
 * \brief Returns resource limits
 *
 * Returns in \p *pValue the current size of \p limit.  The supported
 * ::lwdaLimit values are:
 * - ::lwdaLimitStackSize: stack size in bytes of each GPU thread;
 * - ::lwdaLimitPrintfFifoSize: size in bytes of the shared FIFO used by the
 *   ::printf() device system call.
 * - ::lwdaLimitMallocHeapSize: size in bytes of the heap used by the
 *   ::malloc() and ::free() device system calls;
 * - ::lwdaLimitDevRuntimeSyncDepth: maximum grid depth at which a
 *   thread can isssue the device runtime call ::lwdaDeviceSynchronize()
 *   to wait on child grid launches to complete.
 * - ::lwdaLimitDevRuntimePendingLaunchCount: maximum number of outstanding
 *   device runtime launches.
 * - ::lwdaLimitMaxL2FetchGranularity: L2 cache fetch granularity.
 * - ::lwdaLimitPersistingL2CacheSize: Persisting L2 cache size in bytes
 *
 * \param limit  - Limit to query
 * \param pValue - Returned size of the limit
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnsupportedLimit,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceSetLimit,
 * ::lwCtxGetLimit
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetLimit(size_t *pValue, enum lwdaLimit limit);

/**
 * \brief Returns the maximum number of elements allocatable in a 1D linear texture for a given element size.
 *
 * Returns in \p maxWidthInElements the maximum number of elements allocatable in a 1D linear texture
 * for given format descriptor \p fmtDesc.
 *
 * \param maxWidthInElements    - Returns maximum number of texture elements allocatable for given \p fmtDesc.
 * \param fmtDesc               - Texture format description.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnsupportedLimit,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwDeviceGetMaxTexture1DLinear,
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetTexture1DLinearMaxWidth(size_t *maxWidthInElements, const struct lwdaChannelFormatDesc *fmtDesc, int device);
#endif

/**
 * \brief Returns the preferred cache configuration for the current device.
 *
 * On devices where the L1 cache and shared memory use the same hardware
 * resources, this returns through \p pCacheConfig the preferred cache
 * configuration for the current device. This is only a preference. The
 * runtime will use the requested configuration if possible, but it is free to
 * choose a different configuration if required to execute functions.
 *
 * This will return a \p pCacheConfig of ::lwdaFuncCachePreferNone on devices
 * where the size of the L1 cache and shared memory are fixed.
 *
 * The supported cache configurations are:
 * - ::lwdaFuncCachePreferNone: no preference for shared memory or L1 (default)
 * - ::lwdaFuncCachePreferShared: prefer larger shared memory and smaller L1 cache
 * - ::lwdaFuncCachePreferL1: prefer larger L1 cache and smaller shared memory
 * - ::lwdaFuncCachePreferEqual: prefer equal size L1 cache and shared memory
 *
 * \param pCacheConfig - Returned cache configuration
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceSetCacheConfig,
 * \ref ::lwdaFuncSetCacheConfig(const void*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C API)",
 * \ref ::lwdaFuncSetCacheConfig(T*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C++ API)",
 * ::lwCtxGetCacheConfig
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetCacheConfig(enum lwdaFuncCache *pCacheConfig);

/**
 * \brief Returns numerical values that correspond to the least and
 * greatest stream priorities.
 *
 * Returns in \p *leastPriority and \p *greatestPriority the numerical values that correspond
 * to the least and greatest stream priorities respectively. Stream priorities
 * follow a convention where lower numbers imply greater priorities. The range of
 * meaningful stream priorities is given by [\p *greatestPriority, \p *leastPriority].
 * If the user attempts to create a stream with a priority value that is
 * outside the the meaningful range as specified by this API, the priority is
 * automatically clamped down or up to either \p *leastPriority or \p *greatestPriority
 * respectively. See ::lwdaStreamCreateWithPriority for details on creating a
 * priority stream.
 * A NULL may be passed in for \p *leastPriority or \p *greatestPriority if the value
 * is not desired.
 *
 * This function will return '0' in both \p *leastPriority and \p *greatestPriority if
 * the current context's device does not support stream priorities
 * (see ::lwdaDeviceGetAttribute).
 *
 * \param leastPriority    - Pointer to an int in which the numerical value for least
 *                           stream priority is returned
 * \param greatestPriority - Pointer to an int in which the numerical value for greatest
 *                           stream priority is returned
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreateWithPriority,
 * ::lwdaStreamGetPriority,
 * ::lwCtxGetStreamPriorityRange
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetStreamPriorityRange(int *leastPriority, int *greatestPriority);

/**
 * \brief Sets the preferred cache configuration for the current device.
 *
 * On devices where the L1 cache and shared memory use the same hardware
 * resources, this sets through \p cacheConfig the preferred cache
 * configuration for the current device. This is only a preference. The
 * runtime will use the requested configuration if possible, but it is free to
 * choose a different configuration if required to execute the function. Any
 * function preference set via
 * \ref ::lwdaFuncSetCacheConfig(const void*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C API)"
 * or
 * \ref ::lwdaFuncSetCacheConfig(T*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C++ API)"
 * will be preferred over this device-wide setting. Setting the device-wide
 * cache configuration to ::lwdaFuncCachePreferNone will cause subsequent
 * kernel launches to prefer to not change the cache configuration unless
 * required to launch the kernel.
 *
 * This setting does nothing on devices where the size of the L1 cache and
 * shared memory are fixed.
 *
 * Launching a kernel with a different preference than the most recent
 * preference setting may insert a device-side synchronization point.
 *
 * The supported cache configurations are:
 * - ::lwdaFuncCachePreferNone: no preference for shared memory or L1 (default)
 * - ::lwdaFuncCachePreferShared: prefer larger shared memory and smaller L1 cache
 * - ::lwdaFuncCachePreferL1: prefer larger L1 cache and smaller shared memory
 * - ::lwdaFuncCachePreferEqual: prefer equal size L1 cache and shared memory
 *
 * \param cacheConfig - Requested cache configuration
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceGetCacheConfig,
 * \ref ::lwdaFuncSetCacheConfig(const void*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C API)",
 * \ref ::lwdaFuncSetCacheConfig(T*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C++ API)",
 * ::lwCtxSetCacheConfig
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceSetCacheConfig(enum lwdaFuncCache cacheConfig);

/**
 * \brief Returns the shared memory configuration for the current device.
 *
 * This function will return in \p pConfig the current size of shared memory banks
 * on the current device. On devices with configurable shared memory banks, 
 * ::lwdaDeviceSetSharedMemConfig can be used to change this setting, so that all 
 * subsequent kernel launches will by default use the new bank size. When 
 * ::lwdaDeviceGetSharedMemConfig is called on devices without configurable shared 
 * memory, it will return the fixed bank size of the hardware.
 *
 * The returned bank configurations can be either:
 * - ::lwdaSharedMemBankSizeFourByte - shared memory bank width is four bytes.
 * - ::lwdaSharedMemBankSizeEightByte - shared memory bank width is eight bytes.
 *
 * \param pConfig - Returned cache configuration
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceSetCacheConfig,
 * ::lwdaDeviceGetCacheConfig,
 * ::lwdaDeviceSetSharedMemConfig,
 * ::lwdaFuncSetCacheConfig,
 * ::lwCtxGetSharedMemConfig
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetSharedMemConfig(enum lwdaSharedMemConfig *pConfig);

/**
 * \brief Sets the shared memory configuration for the current device.
 *
 * On devices with configurable shared memory banks, this function will set
 * the shared memory bank size which is used for all subsequent kernel launches.
 * Any per-function setting of shared memory set via ::lwdaFuncSetSharedMemConfig
 * will override the device wide setting.
 *
 * Changing the shared memory configuration between launches may introduce
 * a device side synchronization point.
 *
 * Changing the shared memory bank size will not increase shared memory usage
 * or affect oclwpancy of kernels, but may have major effects on performance. 
 * Larger bank sizes will allow for greater potential bandwidth to shared memory,
 * but will change what kinds of accesses to shared memory will result in bank 
 * conflicts.
 *
 * This function will do nothing on devices with fixed shared memory bank size.
 *
 * The supported bank configurations are:
 * - ::lwdaSharedMemBankSizeDefault: set bank width the device default (lwrrently,
 *   four bytes)
 * - ::lwdaSharedMemBankSizeFourByte: set shared memory bank width to be four bytes
 *   natively.
 * - ::lwdaSharedMemBankSizeEightByte: set shared memory bank width to be eight 
 *   bytes natively.
 *
 * \param config - Requested cache configuration
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceSetCacheConfig,
 * ::lwdaDeviceGetCacheConfig,
 * ::lwdaDeviceGetSharedMemConfig,
 * ::lwdaFuncSetCacheConfig,
 * ::lwCtxSetSharedMemConfig
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceSetSharedMemConfig(enum lwdaSharedMemConfig config);

/**
 * \brief Returns a handle to a compute device
 *
 * Returns in \p *device a device ordinal given a PCI bus ID string.
 *
 * \param device   - Returned device ordinal
 *
 * \param pciBusId - String in one of the following forms: 
 * [domain]:[bus]:[device].[function]
 * [domain]:[bus]:[device]
 * [bus]:[device].[function]
 * where \p domain, \p bus, \p device, and \p function are all hexadecimal values
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceGetPCIBusId,
 * ::lwDeviceGetByPCIBusId
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGetByPCIBusId(int *device, const char *pciBusId);

/**
 * \brief Returns a PCI Bus Id string for the device
 *
 * Returns an ASCII string identifying the device \p dev in the NULL-terminated
 * string pointed to by \p pciBusId. \p len specifies the maximum length of the
 * string that may be returned.
 *
 * \param pciBusId - Returned identifier string for the device in the following format
 * [domain]:[bus]:[device].[function]
 * where \p domain, \p bus, \p device, and \p function are all hexadecimal values.
 * pciBusId should be large enough to store 13 characters including the NULL-terminator.
 *
 * \param len      - Maximum length of string to store in \p name
 *
 * \param device   - Device to get identifier string for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceGetByPCIBusId,
 * ::lwDeviceGetPCIBusId
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGetPCIBusId(char *pciBusId, int len, int device);

/**
 * \brief Gets an interprocess handle for a previously allocated event
 *
 * Takes as input a previously allocated event. This event must have been 
 * created with the ::lwdaEventInterprocess and ::lwdaEventDisableTiming
 * flags set. This opaque handle may be copied into other processes and
 * opened with ::lwdaIpcOpenEventHandle to allow efficient hardware
 * synchronization between GPU work in different processes.
 *
 * After the event has been been opened in the importing process, 
 * ::lwdaEventRecord, ::lwdaEventSynchronize, ::lwdaStreamWaitEvent and 
 * ::lwdaEventQuery may be used in either process. Performing operations 
 * on the imported event after the exported event has been freed 
 * with ::lwdaEventDestroy will result in undefined behavior.
 *
 * IPC functionality is restricted to devices with support for unified 
 * addressing on Linux operating systems. IPC functionality is not supported
 * on CheetAh platforms.
 *
 * \param handle - Pointer to a user allocated lwdaIpcEventHandle
 *                    in which to return the opaque event handle
 * \param event   - Event allocated with ::lwdaEventInterprocess and 
 *                    ::lwdaEventDisableTiming flags.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorMemoryAllocation,
 * ::lwdaErrorMapBufferObjectFailed,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaEventCreate,
 * ::lwdaEventDestroy,
 * ::lwdaEventSynchronize,
 * ::lwdaEventQuery,
 * ::lwdaStreamWaitEvent,
 * ::lwdaIpcOpenEventHandle,
 * ::lwdaIpcGetMemHandle,
 * ::lwdaIpcOpenMemHandle,
 * ::lwdaIpcCloseMemHandle,
 * ::lwIpcGetEventHandle
 */
extern __host__ lwdaError_t LWDARTAPI lwdaIpcGetEventHandle(lwdaIpcEventHandle_t *handle, lwdaEvent_t event);

/**
 * \brief Opens an interprocess event handle for use in the current process
 *
 * Opens an interprocess event handle exported from another process with 
 * ::lwdaIpcGetEventHandle. This function returns a ::lwdaEvent_t that behaves like 
 * a locally created event with the ::lwdaEventDisableTiming flag specified. 
 * This event must be freed with ::lwdaEventDestroy.
 *
 * Performing operations on the imported event after the exported event has 
 * been freed with ::lwdaEventDestroy will result in undefined behavior.
 *
 * IPC functionality is restricted to devices with support for unified 
 * addressing on Linux operating systems. IPC functionality is not supported
 * on CheetAh platforms.
 *
 * \param event - Returns the imported event
 * \param handle  - Interprocess handle to open
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorMapBufferObjectFailed,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorDeviceUninitialized
 * \note_init_rt
 * \note_callback
 *
  * \sa
 * ::lwdaEventCreate,
 * ::lwdaEventDestroy,
 * ::lwdaEventSynchronize,
 * ::lwdaEventQuery,
 * ::lwdaStreamWaitEvent,
 * ::lwdaIpcGetEventHandle,
 * ::lwdaIpcGetMemHandle,
 * ::lwdaIpcOpenMemHandle,
 * ::lwdaIpcCloseMemHandle,
 * ::lwIpcOpenEventHandle
 */
extern __host__ lwdaError_t LWDARTAPI lwdaIpcOpenEventHandle(lwdaEvent_t *event, lwdaIpcEventHandle_t handle);


/**
 * \brief Gets an interprocess memory handle for an existing device memory
 *          allocation
 *
 * Takes a pointer to the base of an existing device memory allocation created 
 * with ::lwdaMalloc and exports it for use in another process. This is a 
 * lightweight operation and may be called multiple times on an allocation
 * without adverse effects. 
 *
 * If a region of memory is freed with ::lwdaFree and a subsequent call
 * to ::lwdaMalloc returns memory with the same device address,
 * ::lwdaIpcGetMemHandle will return a unique handle for the
 * new memory. 
 *
 * IPC functionality is restricted to devices with support for unified 
 * addressing on Linux operating systems. IPC functionality is not supported
 * on CheetAh platforms.
 *
 * \param handle - Pointer to user allocated ::lwdaIpcMemHandle to return
 *                    the handle in.
 * \param devPtr - Base pointer to previously allocated device memory 
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorMemoryAllocation,
 * ::lwdaErrorMapBufferObjectFailed,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMalloc,
 * ::lwdaFree,
 * ::lwdaIpcGetEventHandle,
 * ::lwdaIpcOpenEventHandle,
 * ::lwdaIpcOpenMemHandle,
 * ::lwdaIpcCloseMemHandle,
 * ::lwIpcGetMemHandle
 */
extern __host__ lwdaError_t LWDARTAPI lwdaIpcGetMemHandle(lwdaIpcMemHandle_t *handle, void *devPtr);

/**
 * \brief Opens an interprocess memory handle exported from another process
 *          and returns a device pointer usable in the local process.
 *
 * Maps memory exported from another process with ::lwdaIpcGetMemHandle into
 * the current device address space. For contexts on different devices 
 * ::lwdaIpcOpenMemHandle can attempt to enable peer access between the
 * devices as if the user called ::lwdaDeviceEnablePeerAccess. This behavior is 
 * controlled by the ::lwdaIpcMemLazyEnablePeerAccess flag. 
 * ::lwdaDeviceCanAccessPeer can determine if a mapping is possible.
 *
 * ::lwdaIpcOpenMemHandle can open handles to devices that may not be visible
 * in the process calling the API.
 *
 * Contexts that may open ::lwdaIpcMemHandles are restricted in the following way.
 * ::lwdaIpcMemHandles from each device in a given process may only be opened 
 * by one context per device per other process.
 *
 * If the memory handle has already been opened by the current context, the
 * reference count on the handle is incremented by 1 and the existing device pointer
 * is returned.
 *
 * Memory returned from ::lwdaIpcOpenMemHandle must be freed with
 * ::lwdaIpcCloseMemHandle.
 *
 * Calling ::lwdaFree on an exported memory region before calling
 * ::lwdaIpcCloseMemHandle in the importing context will result in undefined
 * behavior.
 * 
 * IPC functionality is restricted to devices with support for unified 
 * addressing on Linux operating systems. IPC functionality is not supported
 * on CheetAh platforms.
 *
 * \param devPtr - Returned device pointer
 * \param handle - ::lwdaIpcMemHandle to open
 * \param flags  - Flags for this operation. Must be specified as ::lwdaIpcMemLazyEnablePeerAccess
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorMapBufferObjectFailed,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorDeviceUninitialized,
 * ::lwdaErrorTooManyPeers,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \note No guarantees are made about the address returned in \p *devPtr.  
 * In particular, multiple processes may not receive the same address for the same \p handle.
 *
 * \sa
 * ::lwdaMalloc,
 * ::lwdaFree,
 * ::lwdaIpcGetEventHandle,
 * ::lwdaIpcOpenEventHandle,
 * ::lwdaIpcGetMemHandle,
 * ::lwdaIpcCloseMemHandle,
 * ::lwdaDeviceEnablePeerAccess,
 * ::lwdaDeviceCanAccessPeer,
 * ::lwIpcOpenMemHandle
 */
extern __host__ lwdaError_t LWDARTAPI lwdaIpcOpenMemHandle(void **devPtr, lwdaIpcMemHandle_t handle, unsigned int flags);

/**
 * \brief Attempts to close memory mapped with lwdaIpcOpenMemHandle
 * 
 * Decrements the reference count of the memory returnd by ::lwdaIpcOpenMemHandle by 1.
 * When the reference count reaches 0, this API unmaps the memory. The original allocation
 * in the exporting process as well as imported mappings in other processes
 * will be unaffected.
 *
 * Any resources used to enable peer access will be freed if this is the
 * last mapping using them.
 *
 * IPC functionality is restricted to devices with support for unified 
 * addressing on Linux operating systems. IPC functionality is not supported
 * on CheetAh platforms.
 *
 * \param devPtr - Device pointer returned by ::lwdaIpcOpenMemHandle
 * 
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorMapBufferObjectFailed,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMalloc,
 * ::lwdaFree,
 * ::lwdaIpcGetEventHandle,
 * ::lwdaIpcOpenEventHandle,
 * ::lwdaIpcGetMemHandle,
 * ::lwdaIpcOpenMemHandle,
 * ::lwIpcCloseMemHandle
 */
extern __host__ lwdaError_t LWDARTAPI lwdaIpcCloseMemHandle(void *devPtr);

/**
 * \brief Blocks until remote writes are visible to the specified scope
 *
 * Blocks until remote writes to the target context via mappings created
 * through GPUDirect RDMA APIs, like lwidia_p2p_get_pages (see
 * https://docs.lwpu.com/lwca/gpudirect-rdma for more information), are
 * visible to the specified scope.
 *
 * If the scope equals or lies within the scope indicated by
 * ::lwdaDevAttrGPUDirectRDMAWritesOrdering, the call will be a no-op and
 * can be safely omitted for performance. This can be determined by
 * comparing the numerical values between the two enums, with smaller
 * scopes having smaller values.
 *
 * Users may query support for this API via ::lwdaDevAttrGPUDirectRDMAFlushWritesOptions.
 *
 * \param target - The target of the operation, see lwdaFlushGPUDirectRDMAWritesTarget
 * \param scope  - The scope of the operation, see lwdaFlushGPUDirectRDMAWritesScope
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotSupported,
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwFlushGPUDirectRDMAWrites
 */
#if __LWDART_API_VERSION >= 11030
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceFlushGPUDirectRDMAWrites(enum lwdaFlushGPUDirectRDMAWritesTarget target, enum lwdaFlushGPUDirectRDMAWritesScope scope);
#endif

/** @} */ /* END LWDART_DEVICE */













































































































































































































































/**
 * \defgroup LWDART_ERROR Error Handling
 *
 * ___MANBRIEF___ error handling functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the error handling functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Returns the last error from a runtime call
 *
 * Returns the last error that has been produced by any of the runtime calls
 * in the same host thread and resets it to ::lwdaSuccess.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorMissingConfiguration,
 * ::lwdaErrorMemoryAllocation,
 * ::lwdaErrorInitializationError,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorLaunchTimeout,
 * ::lwdaErrorLaunchOutOfResources,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidConfiguration,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorUnmapBufferObjectFailed,
 * ::lwdaErrorIlwalidDevicePointer,
 * ::lwdaErrorIlwalidTexture,
 * ::lwdaErrorIlwalidTextureBinding,
 * ::lwdaErrorIlwalidChannelDescriptor,
 * ::lwdaErrorIlwalidMemcpyDirection,
 * ::lwdaErrorIlwalidFilterSetting,
 * ::lwdaErrorIlwalidNormSetting,
 * ::lwdaErrorUnknown,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorInsufficientDriver,
 * ::lwdaErrorNoDevice,
 * ::lwdaErrorSetOnActiveProcess,
 * ::lwdaErrorStartupFailure,
 * ::lwdaErrorIlwalidPtx,
 * ::lwdaErrorUnsupportedPtxVersion,
 * ::lwdaErrorNoKernelImageForDevice,
 * ::lwdaErrorJitCompilerNotFound,
 * ::lwdaErrorJitCompilationDisabled
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaPeekAtLastError, ::lwdaGetErrorName, ::lwdaGetErrorString, ::lwdaError
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetLastError(void);

/**
 * \brief Returns the last error from a runtime call
 *
 * Returns the last error that has been produced by any of the runtime calls
 * in the same host thread. Note that this call does not reset the error to
 * ::lwdaSuccess like ::lwdaGetLastError().
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorMissingConfiguration,
 * ::lwdaErrorMemoryAllocation,
 * ::lwdaErrorInitializationError,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorLaunchTimeout,
 * ::lwdaErrorLaunchOutOfResources,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidConfiguration,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorUnmapBufferObjectFailed,
 * ::lwdaErrorIlwalidDevicePointer,
 * ::lwdaErrorIlwalidTexture,
 * ::lwdaErrorIlwalidTextureBinding,
 * ::lwdaErrorIlwalidChannelDescriptor,
 * ::lwdaErrorIlwalidMemcpyDirection,
 * ::lwdaErrorIlwalidFilterSetting,
 * ::lwdaErrorIlwalidNormSetting,
 * ::lwdaErrorUnknown,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorInsufficientDriver,
 * ::lwdaErrorNoDevice,
 * ::lwdaErrorSetOnActiveProcess,
 * ::lwdaErrorStartupFailure,
 * ::lwdaErrorIlwalidPtx,
 * ::lwdaErrorUnsupportedPtxVersion,
 * ::lwdaErrorNoKernelImageForDevice,
 * ::lwdaErrorJitCompilerNotFound,
 * ::lwdaErrorJitCompilationDisabled
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetLastError, ::lwdaGetErrorName, ::lwdaGetErrorString, ::lwdaError
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaPeekAtLastError(void);

/**
 * \brief Returns the string representation of an error code enum name
 *
 * Returns a string containing the name of an error code in the enum.  If the error
 * code is not recognized, "unrecognized error code" is returned.
 *
 * \param error - Error code to colwert to string
 *
 * \return
 * \p char* pointer to a NULL-terminated string
 *
 * \sa ::lwdaGetErrorString, ::lwdaGetLastError, ::lwdaPeekAtLastError, ::lwdaError,
 * ::lwGetErrorName
 */
extern __host__ __lwdart_builtin__ const char* LWDARTAPI lwdaGetErrorName(lwdaError_t error);

/**
 * \brief Returns the description string for an error code
 *
 * Returns the description string for an error code.  If the error
 * code is not recognized, "unrecognized error code" is returned.
 *
 * \param error - Error code to colwert to string
 *
 * \return
 * \p char* pointer to a NULL-terminated string
 *
 * \sa ::lwdaGetErrorName, ::lwdaGetLastError, ::lwdaPeekAtLastError, ::lwdaError,
 * ::lwGetErrorString
 */
extern __host__ __lwdart_builtin__ const char* LWDARTAPI lwdaGetErrorString(lwdaError_t error);
/** @} */ /* END LWDART_ERROR */

/**
 * \addtogroup LWDART_DEVICE 
 *
 * @{
 */

/**
 * \brief Returns the number of compute-capable devices
 *
 * Returns in \p *count the number of devices with compute capability greater
 * or equal to 2.0 that are available for exelwtion.
 *
 * \param count - Returns the number of devices with compute capability
 * greater or equal to 2.0
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDevice, ::lwdaSetDevice, ::lwdaGetDeviceProperties,
 * ::lwdaChooseDevice,
 * ::lwDeviceGetCount
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetDeviceCount(int *count);

/**
 * \brief Returns information about the compute-device
 *
 * Returns in \p *prop the properties of device \p dev. The ::lwdaDeviceProp
 * structure is defined as:
 * \code
    struct lwdaDeviceProp {
        char name[256];
        lwdaUUID_t uuid;
        size_t totalGlobalMem;
        size_t sharedMemPerBlock;
        int regsPerBlock;
        int warpSize;
        size_t memPitch;
        int maxThreadsPerBlock;
        int maxThreadsDim[3];
        int maxGridSize[3];
        int clockRate;
        size_t totalConstMem;
        int major;
        int minor;
        size_t textureAlignment;
        size_t texturePitchAlignment;
        int deviceOverlap;
        int multiProcessorCount;
        int kernelExecTimeoutEnabled;
        int integrated;
        int canMapHostMemory;
        int computeMode;
        int maxTexture1D;
        int maxTexture1DMipmap;
        int maxTexture1DLinear;
        int maxTexture2D[2];
        int maxTexture2DMipmap[2];
        int maxTexture2DLinear[3];
        int maxTexture2DGather[2];
        int maxTexture3D[3];
        int maxTexture3DAlt[3];
        int maxTextureLwbemap;
        int maxTexture1DLayered[2];
        int maxTexture2DLayered[3];
        int maxTextureLwbemapLayered[2];
        int maxSurface1D;
        int maxSurface2D[2];
        int maxSurface3D[3];
        int maxSurface1DLayered[2];
        int maxSurface2DLayered[3];
        int maxSurfaceLwbemap;
        int maxSurfaceLwbemapLayered[2];
        size_t surfaceAlignment;
        int conlwrrentKernels;
        int ECCEnabled;
        int pciBusID;
        int pciDeviceID;
        int pciDomainID;
        int tccDriver;
        int asyncEngineCount;
        int unifiedAddressing;
        int memoryClockRate;
        int memoryBusWidth;
        int l2CacheSize;
        int persistingL2CacheMaxSize;
        int maxThreadsPerMultiProcessor;
        int streamPrioritiesSupported;
        int globalL1CacheSupported;
        int localL1CacheSupported;
        size_t sharedMemPerMultiprocessor;
        int regsPerMultiprocessor;
        int managedMemory;
        int isMultiGpuBoard;
        int multiGpuBoardGroupID;
        int singleToDoublePrecisionPerfRatio;
        int pageableMemoryAccess;
        int conlwrrentManagedAccess;
        int computePreemptionSupported;
        int canUseHostPointerForRegisteredMem;
        int cooperativeLaunch;
        int cooperativeMultiDeviceLaunch;
        int pageableMemoryAccessUsesHostPageTables;
        int directManagedMemAccessFromHost;
        int accessPolicyMaxWindowSize;
    }
 \endcode
 * where:
 * - \ref ::lwdaDeviceProp::name "name[256]" is an ASCII string identifying
 *   the device;
 * - \ref ::lwdaDeviceProp::uuid "uuid" is a 16-byte unique identifier.
 * - \ref ::lwdaDeviceProp::totalGlobalMem "totalGlobalMem" is the total
 *   amount of global memory available on the device in bytes;
 * - \ref ::lwdaDeviceProp::sharedMemPerBlock "sharedMemPerBlock" is the
 *   maximum amount of shared memory available to a thread block in bytes;
 * - \ref ::lwdaDeviceProp::regsPerBlock "regsPerBlock" is the maximum number
 *   of 32-bit registers available to a thread block;
 * - \ref ::lwdaDeviceProp::warpSize "warpSize" is the warp size in threads;
 * - \ref ::lwdaDeviceProp::memPitch "memPitch" is the maximum pitch in
 *   bytes allowed by the memory copy functions that involve memory regions
 *   allocated through ::lwdaMallocPitch();
 * - \ref ::lwdaDeviceProp::maxThreadsPerBlock "maxThreadsPerBlock" is the
 *   maximum number of threads per block;
 * - \ref ::lwdaDeviceProp::maxThreadsDim "maxThreadsDim[3]" contains the
 *   maximum size of each dimension of a block;
 * - \ref ::lwdaDeviceProp::maxGridSize "maxGridSize[3]" contains the
 *   maximum size of each dimension of a grid;
 * - \ref ::lwdaDeviceProp::clockRate "clockRate" is the clock frequency in
 *   kilohertz;
 * - \ref ::lwdaDeviceProp::totalConstMem "totalConstMem" is the total amount
 *   of constant memory available on the device in bytes;
 * - \ref ::lwdaDeviceProp::major "major",
 *   \ref ::lwdaDeviceProp::minor "minor" are the major and minor revision
 *   numbers defining the device's compute capability;
 * - \ref ::lwdaDeviceProp::textureAlignment "textureAlignment" is the
 *   alignment requirement; texture base addresses that are aligned to
 *   \ref ::lwdaDeviceProp::textureAlignment "textureAlignment" bytes do not
 *   need an offset applied to texture fetches;
 * - \ref ::lwdaDeviceProp::texturePitchAlignment "texturePitchAlignment" is the
 *   pitch alignment requirement for 2D texture references that are bound to 
 *   pitched memory;
 * - \ref ::lwdaDeviceProp::deviceOverlap "deviceOverlap" is 1 if the device
 *   can conlwrrently copy memory between host and device while exelwting a
 *   kernel, or 0 if not.  Deprecated, use instead asyncEngineCount.
 * - \ref ::lwdaDeviceProp::multiProcessorCount "multiProcessorCount" is the
 *   number of multiprocessors on the device;
 * - \ref ::lwdaDeviceProp::kernelExecTimeoutEnabled "kernelExecTimeoutEnabled"
 *   is 1 if there is a run time limit for kernels exelwted on the device, or
 *   0 if not.
 * - \ref ::lwdaDeviceProp::integrated "integrated" is 1 if the device is an
 *   integrated (motherboard) GPU and 0 if it is a discrete (card) component.
 * - \ref ::lwdaDeviceProp::canMapHostMemory "canMapHostMemory" is 1 if the
 *   device can map host memory into the LWCA address space for use with
 *   ::lwdaHostAlloc()/::lwdaHostGetDevicePointer(), or 0 if not;
 * - \ref ::lwdaDeviceProp::computeMode "computeMode" is the compute mode
 *   that the device is lwrrently in. Available modes are as follows:
 *   - lwdaComputeModeDefault: Default mode - Device is not restricted and
 *     multiple threads can use ::lwdaSetDevice() with this device.
 *   - lwdaComputeModeProhibited: Compute-prohibited mode - No threads can use
 *     ::lwdaSetDevice() with this device.
 *   - lwdaComputeModeExclusiveProcess: Compute-exclusive-process mode - Many 
 *     threads in one process will be able to use ::lwdaSetDevice() with this device.
 *   <br> When an oclwpied exclusive mode device is chosen with ::lwdaSetDevice,
 *   all subsequent non-device management runtime functions will return
 *   ::lwdaErrorDevicesUnavailable.
 * - \ref ::lwdaDeviceProp::maxTexture1D "maxTexture1D" is the maximum 1D
 *   texture size.
 * - \ref ::lwdaDeviceProp::maxTexture1DMipmap "maxTexture1DMipmap" is the maximum
 *   1D mipmapped texture texture size.
 * - \ref ::lwdaDeviceProp::maxTexture1DLinear "maxTexture1DLinear" is the maximum
 *   1D texture size for textures bound to linear memory.
 * - \ref ::lwdaDeviceProp::maxTexture2D "maxTexture2D[2]" contains the maximum
 *   2D texture dimensions.
 * - \ref ::lwdaDeviceProp::maxTexture2DMipmap "maxTexture2DMipmap[2]" contains the
 *   maximum 2D mipmapped texture dimensions.
 * - \ref ::lwdaDeviceProp::maxTexture2DLinear "maxTexture2DLinear[3]" contains the 
 *   maximum 2D texture dimensions for 2D textures bound to pitch linear memory.
 * - \ref ::lwdaDeviceProp::maxTexture2DGather "maxTexture2DGather[2]" contains the 
 *   maximum 2D texture dimensions if texture gather operations have to be performed.
 * - \ref ::lwdaDeviceProp::maxTexture3D "maxTexture3D[3]" contains the maximum
 *   3D texture dimensions.
 * - \ref ::lwdaDeviceProp::maxTexture3DAlt "maxTexture3DAlt[3]"
 *   contains the maximum alternate 3D texture dimensions.
 * - \ref ::lwdaDeviceProp::maxTextureLwbemap "maxTextureLwbemap" is the 
 *   maximum lwbemap texture width or height.
 * - \ref ::lwdaDeviceProp::maxTexture1DLayered "maxTexture1DLayered[2]" contains
 *   the maximum 1D layered texture dimensions.
 * - \ref ::lwdaDeviceProp::maxTexture2DLayered "maxTexture2DLayered[3]" contains
 *   the maximum 2D layered texture dimensions.
 * - \ref ::lwdaDeviceProp::maxTextureLwbemapLayered "maxTextureLwbemapLayered[2]"
 *   contains the maximum lwbemap layered texture dimensions.
 * - \ref ::lwdaDeviceProp::maxSurface1D "maxSurface1D" is the maximum 1D
 *   surface size.
 * - \ref ::lwdaDeviceProp::maxSurface2D "maxSurface2D[2]" contains the maximum
 *   2D surface dimensions.
 * - \ref ::lwdaDeviceProp::maxSurface3D "maxSurface3D[3]" contains the maximum
 *   3D surface dimensions.
 * - \ref ::lwdaDeviceProp::maxSurface1DLayered "maxSurface1DLayered[2]" contains
 *   the maximum 1D layered surface dimensions.
 * - \ref ::lwdaDeviceProp::maxSurface2DLayered "maxSurface2DLayered[3]" contains
 *   the maximum 2D layered surface dimensions.
 * - \ref ::lwdaDeviceProp::maxSurfaceLwbemap "maxSurfaceLwbemap" is the maximum 
 *   lwbemap surface width or height.
 * - \ref ::lwdaDeviceProp::maxSurfaceLwbemapLayered "maxSurfaceLwbemapLayered[2]"
 *   contains the maximum lwbemap layered surface dimensions.
 * - \ref ::lwdaDeviceProp::surfaceAlignment "surfaceAlignment" specifies the
 *   alignment requirements for surfaces.
 * - \ref ::lwdaDeviceProp::conlwrrentKernels "conlwrrentKernels" is 1 if the
 *   device supports exelwting multiple kernels within the same context
 *   simultaneously, or 0 if not. It is not guaranteed that multiple kernels
 *   will be resident on the device conlwrrently so this feature should not be
 *   relied upon for correctness;
 * - \ref ::lwdaDeviceProp::ECCEnabled "ECCEnabled" is 1 if the device has ECC
 *   support turned on, or 0 if not.
 * - \ref ::lwdaDeviceProp::pciBusID "pciBusID" is the PCI bus identifier of
 *   the device.
 * - \ref ::lwdaDeviceProp::pciDeviceID "pciDeviceID" is the PCI device
 *   (sometimes called slot) identifier of the device.
 * - \ref ::lwdaDeviceProp::pciDomainID "pciDomainID" is the PCI domain identifier
 *   of the device.
 * - \ref ::lwdaDeviceProp::tccDriver "tccDriver" is 1 if the device is using a
 *   TCC driver or 0 if not.
 * - \ref ::lwdaDeviceProp::asyncEngineCount "asyncEngineCount" is 1 when the
 *   device can conlwrrently copy memory between host and device while exelwting
 *   a kernel. It is 2 when the device can conlwrrently copy memory between host
 *   and device in both directions and execute a kernel at the same time. It is
 *   0 if neither of these is supported.
 * - \ref ::lwdaDeviceProp::unifiedAddressing "unifiedAddressing" is 1 if the device 
 *   shares a unified address space with the host and 0 otherwise.
 * - \ref ::lwdaDeviceProp::memoryClockRate "memoryClockRate" is the peak memory 
 *   clock frequency in kilohertz.
 * - \ref ::lwdaDeviceProp::memoryBusWidth "memoryBusWidth" is the memory bus width  
 *   in bits.
 * - \ref ::lwdaDeviceProp::l2CacheSize "l2CacheSize" is L2 cache size in bytes. 
 * - \ref ::lwdaDeviceProp::persistingL2CacheMaxSize "persistingL2CacheMaxSize" is L2 cache's maximum persisting lines size in bytes.
 * - \ref ::lwdaDeviceProp::maxThreadsPerMultiProcessor "maxThreadsPerMultiProcessor"  
 *   is the number of maximum resident threads per multiprocessor.
 * - \ref ::lwdaDeviceProp::streamPrioritiesSupported "streamPrioritiesSupported"
 *   is 1 if the device supports stream priorities, or 0 if it is not supported.
 * - \ref ::lwdaDeviceProp::globalL1CacheSupported "globalL1CacheSupported"
 *   is 1 if the device supports caching of globals in L1 cache, or 0 if it is not supported.
 * - \ref ::lwdaDeviceProp::localL1CacheSupported "localL1CacheSupported"
 *   is 1 if the device supports caching of locals in L1 cache, or 0 if it is not supported.
 * - \ref ::lwdaDeviceProp::sharedMemPerMultiprocessor "sharedMemPerMultiprocessor" is the
 *   maximum amount of shared memory available to a multiprocessor in bytes; this amount is
 *   shared by all thread blocks simultaneously resident on a multiprocessor;
 * - \ref ::lwdaDeviceProp::regsPerMultiprocessor "regsPerMultiprocessor" is the maximum number
 *   of 32-bit registers available to a multiprocessor; this number is shared
 *   by all thread blocks simultaneously resident on a multiprocessor;
 * - \ref ::lwdaDeviceProp::managedMemory "managedMemory"
 *   is 1 if the device supports allocating managed memory on this system, or 0 if it is not supported.
 * - \ref ::lwdaDeviceProp::isMultiGpuBoard "isMultiGpuBoard"
 *   is 1 if the device is on a multi-GPU board (e.g. Gemini cards), and 0 if not;
 * - \ref ::lwdaDeviceProp::multiGpuBoardGroupID "multiGpuBoardGroupID" is a unique identifier
 *   for a group of devices associated with the same board.
 *   Devices on the same multi-GPU board will share the same identifier;
 * - \ref ::lwdaDeviceProp::singleToDoublePrecisionPerfRatio "singleToDoublePrecisionPerfRatio"  
 *   is the ratio of single precision performance (in floating-point operations per second)
 *   to double precision performance.
 * - \ref ::lwdaDeviceProp::pageableMemoryAccess "pageableMemoryAccess" is 1 if the device supports
 *   coherently accessing pageable memory without calling lwdaHostRegister on it, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::conlwrrentManagedAccess "conlwrrentManagedAccess" is 1 if the device can
 *   coherently access managed memory conlwrrently with the CPU, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::computePreemptionSupported "computePreemptionSupported" is 1 if the device
 *   supports Compute Preemption, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::canUseHostPointerForRegisteredMem "canUseHostPointerForRegisteredMem" is 1 if
 *   the device can access host registered memory at the same virtual address as the CPU, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::cooperativeLaunch "cooperativeLaunch" is 1 if the device supports launching
 *   cooperative kernels via ::lwdaLaunchCooperativeKernel, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::cooperativeMultiDeviceLaunch "cooperativeMultiDeviceLaunch" is 1 if the device
 *   supports launching cooperative kernels via ::lwdaLaunchCooperativeKernelMultiDevice, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::pageableMemoryAccessUsesHostPageTables "pageableMemoryAccessUsesHostPageTables" is 1 if the device accesses
 *   pageable memory via the host's page tables, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::directManagedMemAccessFromHost "directManagedMemAccessFromHost" is 1 if the host can directly access managed
 *   memory on the device without migration, and 0 otherwise.
 * - \ref ::lwdaDeviceProp::maxBlocksPerMultiProcessor "maxBlocksPerMultiProcessor" is the maximum number of thread blocks
 *   that can reside on a multiprocessor.
 * - \ref ::lwdaDeviceProp::accessPolicyMaxWindowSize "accessPolicyMaxWindowSize" is
 *   the maximum value of ::lwdaAccessPolicyWindow::num_bytes.
 *
 * \param prop   - Properties for the specified device
 * \param device - Device number to get properties for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaGetDevice, ::lwdaSetDevice, ::lwdaChooseDevice,
 * ::lwdaDeviceGetAttribute,
 * ::lwDeviceGetAttribute,
 * ::lwDeviceGetName
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetDeviceProperties(struct lwdaDeviceProp *prop, int device);

/**
 * \brief Returns information about the device
 *
 * Returns in \p *value the integer value of the attribute \p attr on device
 * \p device. The supported attributes are:
 * - ::lwdaDevAttrMaxThreadsPerBlock: Maximum number of threads per block
 * - ::lwdaDevAttrMaxBlockDimX: Maximum x-dimension of a block
 * - ::lwdaDevAttrMaxBlockDimY: Maximum y-dimension of a block
 * - ::lwdaDevAttrMaxBlockDimZ: Maximum z-dimension of a block
 * - ::lwdaDevAttrMaxGridDimX: Maximum x-dimension of a grid
 * - ::lwdaDevAttrMaxGridDimY: Maximum y-dimension of a grid
 * - ::lwdaDevAttrMaxGridDimZ: Maximum z-dimension of a grid
 * - ::lwdaDevAttrMaxSharedMemoryPerBlock: Maximum amount of shared memory
 *   available to a thread block in bytes
 * - ::lwdaDevAttrTotalConstantMemory: Memory available on device for
 *   __constant__ variables in a LWCA C kernel in bytes
 * - ::lwdaDevAttrWarpSize: Warp size in threads
 * - ::lwdaDevAttrMaxPitch: Maximum pitch in bytes allowed by the memory copy
 *   functions that involve memory regions allocated through ::lwdaMallocPitch()
 * - ::lwdaDevAttrMaxTexture1DWidth: Maximum 1D texture width
 * - ::lwdaDevAttrMaxTexture1DLinearWidth: Maximum width for a 1D texture bound
 *   to linear memory
 * - ::lwdaDevAttrMaxTexture1DMipmappedWidth: Maximum mipmapped 1D texture width
 * - ::lwdaDevAttrMaxTexture2DWidth: Maximum 2D texture width
 * - ::lwdaDevAttrMaxTexture2DHeight: Maximum 2D texture height
 * - ::lwdaDevAttrMaxTexture2DLinearWidth: Maximum width for a 2D texture
 *   bound to linear memory
 * - ::lwdaDevAttrMaxTexture2DLinearHeight: Maximum height for a 2D texture
 *   bound to linear memory
 * - ::lwdaDevAttrMaxTexture2DLinearPitch: Maximum pitch in bytes for a 2D
 *   texture bound to linear memory
 * - ::lwdaDevAttrMaxTexture2DMipmappedWidth: Maximum mipmapped 2D texture
 *   width
 * - ::lwdaDevAttrMaxTexture2DMipmappedHeight: Maximum mipmapped 2D texture
 *   height
 * - ::lwdaDevAttrMaxTexture3DWidth: Maximum 3D texture width
 * - ::lwdaDevAttrMaxTexture3DHeight: Maximum 3D texture height
 * - ::lwdaDevAttrMaxTexture3DDepth: Maximum 3D texture depth
 * - ::lwdaDevAttrMaxTexture3DWidthAlt: Alternate maximum 3D texture width,
 *   0 if no alternate maximum 3D texture size is supported
 * - ::lwdaDevAttrMaxTexture3DHeightAlt: Alternate maximum 3D texture height,
 *   0 if no alternate maximum 3D texture size is supported
 * - ::lwdaDevAttrMaxTexture3DDepthAlt: Alternate maximum 3D texture depth,
 *   0 if no alternate maximum 3D texture size is supported
 * - ::lwdaDevAttrMaxTextureLwbemapWidth: Maximum lwbemap texture width or
 *   height
 * - ::lwdaDevAttrMaxTexture1DLayeredWidth: Maximum 1D layered texture width
 * - ::lwdaDevAttrMaxTexture1DLayeredLayers: Maximum layers in a 1D layered
 *   texture
 * - ::lwdaDevAttrMaxTexture2DLayeredWidth: Maximum 2D layered texture width
 * - ::lwdaDevAttrMaxTexture2DLayeredHeight: Maximum 2D layered texture height
 * - ::lwdaDevAttrMaxTexture2DLayeredLayers: Maximum layers in a 2D layered
 *   texture
 * - ::lwdaDevAttrMaxTextureLwbemapLayeredWidth: Maximum lwbemap layered
 *   texture width or height
 * - ::lwdaDevAttrMaxTextureLwbemapLayeredLayers: Maximum layers in a lwbemap
 *   layered texture
 * - ::lwdaDevAttrMaxSurface1DWidth: Maximum 1D surface width
 * - ::lwdaDevAttrMaxSurface2DWidth: Maximum 2D surface width
 * - ::lwdaDevAttrMaxSurface2DHeight: Maximum 2D surface height
 * - ::lwdaDevAttrMaxSurface3DWidth: Maximum 3D surface width
 * - ::lwdaDevAttrMaxSurface3DHeight: Maximum 3D surface height
 * - ::lwdaDevAttrMaxSurface3DDepth: Maximum 3D surface depth
 * - ::lwdaDevAttrMaxSurface1DLayeredWidth: Maximum 1D layered surface width
 * - ::lwdaDevAttrMaxSurface1DLayeredLayers: Maximum layers in a 1D layered
 *   surface
 * - ::lwdaDevAttrMaxSurface2DLayeredWidth: Maximum 2D layered surface width
 * - ::lwdaDevAttrMaxSurface2DLayeredHeight: Maximum 2D layered surface height
 * - ::lwdaDevAttrMaxSurface2DLayeredLayers: Maximum layers in a 2D layered
 *   surface
 * - ::lwdaDevAttrMaxSurfaceLwbemapWidth: Maximum lwbemap surface width
 * - ::lwdaDevAttrMaxSurfaceLwbemapLayeredWidth: Maximum lwbemap layered
 *   surface width
 * - ::lwdaDevAttrMaxSurfaceLwbemapLayeredLayers: Maximum layers in a lwbemap
 *   layered surface
 * - ::lwdaDevAttrMaxRegistersPerBlock: Maximum number of 32-bit registers 
 *   available to a thread block
 * - ::lwdaDevAttrClockRate: Peak clock frequency in kilohertz
 * - ::lwdaDevAttrTextureAlignment: Alignment requirement; texture base
 *   addresses aligned to ::textureAlign bytes do not need an offset applied
 *   to texture fetches
 * - ::lwdaDevAttrTexturePitchAlignment: Pitch alignment requirement for 2D
 *   texture references bound to pitched memory
 * - ::lwdaDevAttrGpuOverlap: 1 if the device can conlwrrently copy memory
 *   between host and device while exelwting a kernel, or 0 if not
 * - ::lwdaDevAttrMultiProcessorCount: Number of multiprocessors on the device
 * - ::lwdaDevAttrKernelExecTimeout: 1 if there is a run time limit for kernels
 *   exelwted on the device, or 0 if not
 * - ::lwdaDevAttrIntegrated: 1 if the device is integrated with the memory
 *   subsystem, or 0 if not
 * - ::lwdaDevAttrCanMapHostMemory: 1 if the device can map host memory into
 *   the LWCA address space, or 0 if not
 * - ::lwdaDevAttrComputeMode: Compute mode is the compute mode that the device
 *   is lwrrently in. Available modes are as follows:
 *   - ::lwdaComputeModeDefault: Default mode - Device is not restricted and
 *     multiple threads can use ::lwdaSetDevice() with this device.
 *   - ::lwdaComputeModeProhibited: Compute-prohibited mode - No threads can use
 *     ::lwdaSetDevice() with this device.
 *   - ::lwdaComputeModeExclusiveProcess: Compute-exclusive-process mode - Many 
 *     threads in one process will be able to use ::lwdaSetDevice() with this
 *     device.
 * - ::lwdaDevAttrConlwrrentKernels: 1 if the device supports exelwting
 *   multiple kernels within the same context simultaneously, or 0 if
 *   not. It is not guaranteed that multiple kernels will be resident on the
 *   device conlwrrently so this feature should not be relied upon for
 *   correctness.
 * - ::lwdaDevAttrEccEnabled: 1 if error correction is enabled on the device,
 *   0 if error correction is disabled or not supported by the device
 * - ::lwdaDevAttrPciBusId: PCI bus identifier of the device
 * - ::lwdaDevAttrPciDeviceId: PCI device (also known as slot) identifier of
 *   the device
 * - ::lwdaDevAttrTccDriver: 1 if the device is using a TCC driver. TCC is only
 *   available on Tesla hardware running Windows Vista or later.
 * - ::lwdaDevAttrMemoryClockRate: Peak memory clock frequency in kilohertz
 * - ::lwdaDevAttrGlobalMemoryBusWidth: Global memory bus width in bits
 * - ::lwdaDevAttrL2CacheSize: Size of L2 cache in bytes. 0 if the device
 *   doesn't have L2 cache.
 * - ::lwdaDevAttrMaxThreadsPerMultiProcessor: Maximum resident threads per 
 *   multiprocessor
 * - ::lwdaDevAttrUnifiedAddressing: 1 if the device shares a unified address
 *   space with the host, or 0 if not
 * - ::lwdaDevAttrComputeCapabilityMajor: Major compute capability version
 *   number
 * - ::lwdaDevAttrComputeCapabilityMinor: Minor compute capability version
 *   number
 * - ::lwdaDevAttrStreamPrioritiesSupported: 1 if the device supports stream
 *   priorities, or 0 if not
 * - ::lwdaDevAttrGlobalL1CacheSupported: 1 if device supports caching globals 
 *    in L1 cache, 0 if not
 * - ::lwdaDevAttrLocalL1CacheSupported: 1 if device supports caching locals 
 *    in L1 cache, 0 if not
 * - ::lwdaDevAttrMaxSharedMemoryPerMultiprocessor: Maximum amount of shared memory
 *   available to a multiprocessor in bytes; this amount is shared by all 
 *   thread blocks simultaneously resident on a multiprocessor
 * - ::lwdaDevAttrMaxRegistersPerMultiprocessor: Maximum number of 32-bit registers 
 *   available to a multiprocessor; this number is shared by all thread blocks
 *   simultaneously resident on a multiprocessor
 * - ::lwdaDevAttrManagedMemory: 1 if device supports allocating
 *   managed memory, 0 if not
 * - ::lwdaDevAttrIsMultiGpuBoard: 1 if device is on a multi-GPU board, 0 if not
 * - ::lwdaDevAttrMultiGpuBoardGroupID: Unique identifier for a group of devices on the
 *   same multi-GPU board
 * - ::lwdaDevAttrHostNativeAtomicSupported: 1 if the link between the device and the
 *   host supports native atomic operations
 * - ::lwdaDevAttrSingleToDoublePrecisionPerfRatio: Ratio of single precision performance
 *   (in floating-point operations per second) to double precision performance
 * - ::lwdaDevAttrPageableMemoryAccess: 1 if the device supports coherently accessing
 *   pageable memory without calling lwdaHostRegister on it, and 0 otherwise
 * - ::lwdaDevAttrConlwrrentManagedAccess: 1 if the device can coherently access managed
 *   memory conlwrrently with the CPU, and 0 otherwise
 * - ::lwdaDevAttrComputePreemptionSupported: 1 if the device supports
 *   Compute Preemption, 0 if not
 * - ::lwdaDevAttrCanUseHostPointerForRegisteredMem: 1 if the device can access host
 *   registered memory at the same virtual address as the CPU, and 0 otherwise
 * - ::lwdaDevAttrCooperativeLaunch: 1 if the device supports launching cooperative kernels
 *   via ::lwdaLaunchCooperativeKernel, and 0 otherwise
 * - ::lwdaDevAttrCooperativeMultiDeviceLaunch: 1 if the device supports launching cooperative
 *   kernels via ::lwdaLaunchCooperativeKernelMultiDevice, and 0 otherwise
 * - ::lwdaDevAttrCanFlushRemoteWrites: 1 if the device supports flushing of outstanding 
 *   remote writes, and 0 otherwise
 * - ::lwdaDevAttrHostRegisterSupported: 1 if the device supports host memory registration
 *   via ::lwdaHostRegister, and 0 otherwise
 * - ::lwdaDevAttrPageableMemoryAccessUsesHostPageTables: 1 if the device accesses pageable memory via the
 *   host's page tables, and 0 otherwise
 * - ::lwdaDevAttrDirectManagedMemAccessFromHost: 1 if the host can directly access managed memory on the device
 *   without migration, and 0 otherwise
 * - ::lwdaDevAttrMaxSharedMemoryPerBlockOptin: Maximum per block shared memory size on the device. This value can
 *   be opted into when using ::lwdaFuncSetAttribute
 * - ::lwdaDevAttrMaxBlocksPerMultiprocessor: Maximum number of thread blocks that can reside on a multiprocessor
 * - ::lwdaDevAttrMaxPersistingL2CacheSize: Maximum L2 persisting lines capacity setting in bytes
 * - ::lwdaDevAttrMaxAccessPolicyWindowSize: Maximum value of lwdaAccessPolicyWindow::num_bytes
 * - ::lwdaDevAttrReservedSharedMemoryPerBlock: Shared memory reserved by LWCA driver per block in bytes
 * - ::lwdaDevAttrSparseLwdaArraySupported: 1 if the device supports sparse LWCA arrays and sparse LWCA mipmapped arrays.
 * - ::lwdaDevAttrHostRegisterReadOnlySupported: Device supports using the ::lwdaHostRegister flag lwdaHostRegisterReadOnly
 *   to register memory that must be mapped as read-only to the GPU
 * - ::lwdaDevAttrMemoryPoolsSupported: 1 if the device supports using the lwdaMallocAsync and lwdaMemPool family of APIs, and 0 otherwise
 * - ::lwdaDevAttrGPUDirectRDMASupported: 1 if the device supports GPUDirect RDMA APIs, and 0 otherwise
 * - ::lwdaDevAttrGPUDirectRDMAFlushWritesOptions: bitmask to be interpreted according to the ::lwdaFlushGPUDirectRDMAWritesOptions enum 
 * - ::lwdaDevAttrGPUDirectRDMAWritesOrdering: see the ::lwdaGPUDirectRDMAWritesOrdering enum for numerical values
 * - ::lwdaDevAttrMemoryPoolSupportedHandleTypes: Bitmask of handle types supported with mempool based IPC

 * - ::lwdaDevAttrDeferredMappingLwdaArraySupported : 1 if the device supports deferred mapping LWCA arrays and LWCA mipmapped arrays.

 *
 * \param value  - Returned device attribute value
 * \param attr   - Device attribute to query
 * \param device - Device number to query 
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaGetDevice, ::lwdaSetDevice, ::lwdaChooseDevice,
 * ::lwdaGetDeviceProperties,
 * ::lwDeviceGetAttribute
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetAttribute(int *value, enum lwdaDeviceAttr attr, int device);

/**
 * \brief Returns the default mempool of a device
 *
 * The default mempool of a device contains device memory from that device.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue
 * ::lwdaErrorNotSupported
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwDeviceGetDefaultMemPool, ::lwdaMallocAsync, ::lwdaMemPoolTrimTo, ::lwdaMemPoolGetAttribute, ::lwdaDeviceSetMemPool, ::lwdaMemPoolSetAttribute, ::lwdaMemPoolSetAccess
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGetDefaultMemPool(lwdaMemPool_t *memPool, int device);


/**
 * \brief Sets the current memory pool of a device
 *
 * The memory pool must be local to the specified device.
 * Unless a mempool is specified in the ::lwdaMallocAsync call,
 * ::lwdaMallocAsync allocates from the current mempool of the provided stream's device.
 * By default, a device's current memory pool is its default memory pool.
 *
 * \note Use ::lwdaMallocFromPoolAsync to specify asynchronous allocations from a device different
 * than the one the stream runs on.
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * ::lwdaErrorIlwalidDevice
 * ::lwdaErrorNotSupported
 * \notefnerr
 * \note_callback
 *
 * \sa ::lwDeviceSetDefaultMemPool, ::lwdaDeviceGetMemPool, ::lwdaDeviceGetDefaultMemPool, ::lwdaMemPoolCreate, ::lwdaMemPoolDestroy, ::lwdaMallocFromPoolAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceSetMemPool(int device, lwdaMemPool_t memPool);

/**
 * \brief Gets the current mempool for a device
 *
 * Returns the last pool provided to ::lwdaDeviceSetMemPool for this device
 * or the device's default memory pool if ::lwdaDeviceSetMemPool has never been called.
 * By default the current mempool is the default mempool for a device,
 * otherwise the returned pool must have been set with ::lwDeviceSetMemPool or ::lwdaDeviceSetMemPool.
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * ::lwdaErrorNotSupported
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwDeviceGetMemPool, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceSetMemPool
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGetMemPool(lwdaMemPool_t *memPool, int device);

/**
 * \brief Return LwSciSync attributes that this device can support.
 *
 * Returns in \p lwSciSyncAttrList, the properties of LwSciSync that
 * this LWCA device, \p dev can support. The returned \p lwSciSyncAttrList
 * can be used to create an LwSciSync that matches this device's capabilities.
 * 
 * If LwSciSyncAttrKey_RequiredPerm field in \p lwSciSyncAttrList is
 * already set this API will return ::lwdaErrorIlwalidValue.
 * 
 * The applications should set \p lwSciSyncAttrList to a valid 
 * LwSciSyncAttrList failing which this API will return
 * ::lwdaErrorIlwalidHandle.
 * 
 * The \p flags controls how applications intends to use
 * the LwSciSync created from the \p lwSciSyncAttrList. The valid flags are:
 * - ::lwdaLwSciSyncAttrSignal, specifies that the applications intends to 
 * signal an LwSciSync on this LWCA device.
 * - ::lwdaLwSciSyncAttrWait, specifies that the applications intends to 
 * wait on an LwSciSync on this LWCA device.
 *
 * At least one of these flags must be set, failing which the API
 * returns ::lwdaErrorIlwalidValue. Both the flags are orthogonal
 * to one another: a developer may set both these flags that allows to
 * set both wait and signal specific attributes in the same \p lwSciSyncAttrList.
 *
 * \param lwSciSyncAttrList     - Return LwSciSync attributes supported.
 * \param device                - Valid Lwca Device to get LwSciSync attributes for.
 * \param flags                 - flags describing LwSciSync usage.
 *
 * \return
 *
 * ::lwdaSuccess,
 * ::lwdaErrorDeviceUninitialized,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidHandle,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorMemoryAllocation
 *
 * \sa
 * ::lwdaImportExternalSemaphore,
 * ::lwdaDestroyExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGetLwSciSyncAttributes(void *lwSciSyncAttrList, int device, int flags);

/**
 * \brief Queries attributes of the link between two devices.
 *
 * Returns in \p *value the value of the requested attribute \p attrib of the
 * link between \p srcDevice and \p dstDevice. The supported attributes are:
 * - ::lwdaDevP2PAttrPerformanceRank: A relative value indicating the
 *   performance of the link between two devices. Lower value means better
 *   performance (0 being the value used for most performant link).
 * - ::lwdaDevP2PAttrAccessSupported: 1 if peer access is enabled.
 * - ::lwdaDevP2PAttrNativeAtomicSupported: 1 if native atomic operations over
 *   the link are supported.
 * - ::lwdaDevP2PAttrLwdaArrayAccessSupported: 1 if accessing LWCA arrays over
 *   the link is supported.
 *
 * Returns ::lwdaErrorIlwalidDevice if \p srcDevice or \p dstDevice are not valid
 * or if they represent the same device.
 *
 * Returns ::lwdaErrorIlwalidValue if \p attrib is not valid or if \p value is
 * a null pointer.
 *
 * \param value         - Returned value of the requested attribute
 * \param attrib        - The requested attribute of the link between \p srcDevice and \p dstDevice.
 * \param srcDevice     - The source device of the target link.
 * \param dstDevice     - The destination device of the target link.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaCtxEnablePeerAccess,
 * ::lwdaCtxDisablePeerAccess,
 * ::lwdaCtxCanAccessPeer,
 * ::lwDeviceGetP2PAttribute
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetP2PAttribute(int *value, enum lwdaDeviceP2PAttr attr, int srcDevice, int dstDevice);

/**
 * \brief Select compute-device which best matches criteria
 *
 * Returns in \p *device the device which has properties that best match
 * \p *prop.
 *
 * \param device - Device with best match
 * \param prop   - Desired device properties
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaGetDevice, ::lwdaSetDevice,
 * ::lwdaGetDeviceProperties
 */
extern __host__ lwdaError_t LWDARTAPI lwdaChooseDevice(int *device, const struct lwdaDeviceProp *prop);

/**
 * \brief Set device to be used for GPU exelwtions
 *
 * Sets \p device as the current device for the calling host thread.
 * Valid device id's are 0 to (::lwdaGetDeviceCount() - 1).
 *
 * Any device memory subsequently allocated from this host thread
 * using ::lwdaMalloc(), ::lwdaMallocPitch() or ::lwdaMallocArray()
 * will be physically resident on \p device.  Any host memory allocated
 * from this host thread using ::lwdaMallocHost() or ::lwdaHostAlloc() 
 * or ::lwdaHostRegister() will have its lifetime associated  with
 * \p device.  Any streams or events created from this host thread will 
 * be associated with \p device.  Any kernels launched from this host
 * thread using the <<<>>> operator or ::lwdaLaunchKernel() will be exelwted
 * on \p device.
 *
 * This call may be made from any host thread, to any device, and at 
 * any time.  This function will do no synchronization with the previous 
 * or new device, and should be considered a very low overhead call.
 * If the current context bound to the calling thread is not the primary context,
 * this call will bind the primary context to the calling thread and all the
 * subsequent memory allocations, stream and event creations, and kernel launches
 * will be associated with the primary context. This function will not initialize 
 * the context until a runtime API requiring the context (such as ::lwdaMalloc()) 
 * is used. This function will not return an error if the device is in 
 * ::lwdaComputeModeExclusiveProcess and is oclwpied by another process or
 * if the device is in ::lwdaComputeModeProhibited.
 *
 * \param device - Device on which the active host thread should execute the
 * device code.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaGetDevice, ::lwdaGetDeviceProperties,
 * ::lwdaChooseDevice,
 * ::lwCtxSetLwrrent
 */
extern __host__ lwdaError_t LWDARTAPI lwdaSetDevice(int device);

/**
 * \brief Returns which device is lwrrently being used
 *
 * Returns in \p *device the current device for the calling host thread.
 *
 * \param device - Returns the device on which the active host thread
 * exelwtes the device code.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaSetDevice, ::lwdaGetDeviceProperties,
 * ::lwdaChooseDevice,
 * ::lwCtxGetLwrrent
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetDevice(int *device);

/**
 * \brief Set a list of devices that can be used for LWCA
 *
 * Sets a list of devices for LWCA exelwtion in priority order using
 * \p device_arr. The parameter \p len specifies the number of elements in the
 * list.  LWCA will try devices from the list sequentially until it finds one
 * that works.  If this function is not called, or if it is called with a \p len
 * of 0, then LWCA will go back to its default behavior of trying devices
 * sequentially from a default list containing all of the available LWCA
 * devices in the system. If a specified device ID in the list does not exist,
 * this function will return ::lwdaErrorIlwalidDevice. If \p len is not 0 and
 * \p device_arr is NULL or if \p len exceeds the number of devices in
 * the system, then ::lwdaErrorIlwalidValue is returned.
 *
 * \param device_arr - List of devices to try
 * \param len        - Number of devices in specified list
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaSetDevice, ::lwdaGetDeviceProperties,
 * ::lwdaSetDeviceFlags,
 * ::lwdaChooseDevice
 */
extern __host__ lwdaError_t LWDARTAPI lwdaSetValidDevices(int *device_arr, int len);

/**
 * \brief Sets flags to be used for device exelwtions
 * 
 * Records \p flags as the flags for the current device. If the current device
 * has been set and that device has already been initialized, the previous flags
 * are overwritten. If the current device has not been initialized, it is
 * initialized with the provided flags. If no device has been made current to
 * the calling thread, a default device is selected and initialized with the
 * provided flags.
 * 
 * The two LSBs of the \p flags parameter can be used to control how the CPU
 * thread interacts with the OS scheduler when waiting for results from the
 * device.
 *
 * - ::lwdaDeviceScheduleAuto: The default value if the \p flags parameter is
 * zero, uses a heuristic based on the number of active LWCA contexts in the
 * process \p C and the number of logical processors in the system \p P. If
 * \p C \> \p P, then LWCA will yield to other OS threads when waiting for the
 * device, otherwise LWCA will not yield while waiting for results and
 * actively spin on the processor. Additionally, on CheetAh devices,
 * ::lwdaDeviceScheduleAuto uses a heuristic based on the power profile of
 * the platform and may choose ::lwdaDeviceScheduleBlockingSync for low-powered
 * devices.
 * - ::lwdaDeviceScheduleSpin: Instruct LWCA to actively spin when waiting for
 * results from the device. This can decrease latency when waiting for the
 * device, but may lower the performance of CPU threads if they are performing
 * work in parallel with the LWCA thread.
 * - ::lwdaDeviceScheduleYield: Instruct LWCA to yield its thread when waiting
 * for results from the device. This can increase latency when waiting for the
 * device, but can increase the performance of CPU threads performing work in
 * parallel with the device.
 * - ::lwdaDeviceScheduleBlockingSync: Instruct LWCA to block the CPU thread 
 * on a synchronization primitive when waiting for the device to finish work.
 * - ::lwdaDeviceBlockingSync: Instruct LWCA to block the CPU thread on a 
 * synchronization primitive when waiting for the device to finish work. <br>
 * \ref deprecated "Deprecated:" This flag was deprecated as of LWCA 4.0 and
 * replaced with ::lwdaDeviceScheduleBlockingSync.
 * - ::lwdaDeviceMapHost: This flag enables allocating pinned
 * host memory that is accessible to the device. It is implicit for the
 * runtime but may be absent if a context is created using the driver API.
 * If this flag is not set, ::lwdaHostGetDevicePointer() will always return
 * a failure code.
 * - ::lwdaDeviceLmemResizeToMax: Instruct LWCA to not reduce local memory
 * after resizing local memory for a kernel. This can prevent thrashing by
 * local memory allocations when launching many kernels with high local
 * memory usage at the cost of potentially increased memory usage. <br>
 * \ref deprecated "Deprecated:" This flag is deprecated and the behavior enabled          
 * by this flag is now the default and cannot be disabled.
 *
 * \param flags - Parameters for device operation
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceFlags, ::lwdaGetDeviceCount, ::lwdaGetDevice, ::lwdaGetDeviceProperties,
 * ::lwdaSetDevice, ::lwdaSetValidDevices,
 * ::lwdaChooseDevice,
 * ::lwDevicePrimaryCtxSetFlags
 */
extern __host__ lwdaError_t LWDARTAPI lwdaSetDeviceFlags( unsigned int flags );

/**
 * \brief Gets the flags for the current device
 *
 * 
 * Returns in \p flags the flags for the current device. If there is a current
 * device for the calling thread, the flags for the device are returned. If
 * there is no current device, the flags for the first device are returned,
 * which may be the default flags.  Compare to the behavior of
 * ::lwdaSetDeviceFlags.
 *
 * Typically, the flags returned should match the behavior that will be seen
 * if the calling thread uses a device after this call, without any change to
 * the flags or current device inbetween by this or another thread.  Note that
 * if the device is not initialized, it is possible for another thread to
 * change the flags for the current device before it is initialized.
 * Additionally, when using exclusive mode, if this thread has not requested a
 * specific device, it may use a device other than the first device, contrary
 * to the assumption made by this function.
 *
 * If a context has been created via the driver API and is current to the
 * calling thread, the flags for that context are always returned.
 *
 * Flags returned by this function may specifically include ::lwdaDeviceMapHost
 * even though it is not accepted by ::lwdaSetDeviceFlags because it is
 * implicit in runtime API flags.  The reason for this is that the current
 * context may have been created via the driver API in which case the flag is
 * not implicit and may be unset.
 *
 * \param flags - Pointer to store the device flags
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDevice, ::lwdaGetDeviceProperties,
 * ::lwdaSetDevice, ::lwdaSetDeviceFlags,
 * ::lwCtxGetFlags,
 * ::lwDevicePrimaryCtxGetState
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetDeviceFlags( unsigned int *flags );
/** @} */ /* END LWDART_DEVICE */

/**
 * \defgroup LWDART_STREAM Stream Management
 *
 * ___MANBRIEF___ stream management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the stream management functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Create an asynchronous stream
 *
 * Creates a new asynchronous stream.
 *
 * \param pStream - Pointer to new stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreateWithPriority,
 * ::lwdaStreamCreateWithFlags,
 * ::lwdaStreamGetPriority,
 * ::lwdaStreamGetFlags,
 * ::lwdaStreamQuery,
 * ::lwdaStreamSynchronize,
 * ::lwdaStreamWaitEvent,
 * ::lwdaStreamAddCallback,
 * ::lwdaStreamDestroy,
 * ::lwStreamCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamCreate(lwdaStream_t *pStream);

/**
 * \brief Create an asynchronous stream
 *
 * Creates a new asynchronous stream.  The \p flags argument determines the 
 * behaviors of the stream.  Valid values for \p flags are
 * - ::lwdaStreamDefault: Default stream creation flag.
 * - ::lwdaStreamNonBlocking: Specifies that work running in the created 
 *   stream may run conlwrrently with work in stream 0 (the NULL stream), and that
 *   the created stream should perform no implicit synchronization with stream 0.
 *
 * \param pStream - Pointer to new stream identifier
 * \param flags   - Parameters for stream creation
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate,
 * ::lwdaStreamCreateWithPriority,
 * ::lwdaStreamGetFlags,
 * ::lwdaStreamQuery,
 * ::lwdaStreamSynchronize,
 * ::lwdaStreamWaitEvent,
 * ::lwdaStreamAddCallback,
 * ::lwdaStreamDestroy,
 * ::lwStreamCreate
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamCreateWithFlags(lwdaStream_t *pStream, unsigned int flags);

/**
 * \brief Create an asynchronous stream with the specified priority
 *
 * Creates a stream with the specified priority and returns a handle in \p pStream.
 * This API alters the scheduler priority of work in the stream. Work in a higher
 * priority stream may preempt work already exelwting in a low priority stream.
 *
 * \p priority follows a convention where lower numbers represent higher priorities.
 * '0' represents default priority. The range of meaningful numerical priorities can
 * be queried using ::lwdaDeviceGetStreamPriorityRange. If the specified priority is
 * outside the numerical range returned by ::lwdaDeviceGetStreamPriorityRange,
 * it will automatically be clamped to the lowest or the highest number in the range.
 *
 * \param pStream  - Pointer to new stream identifier
 * \param flags    - Flags for stream creation. See ::lwdaStreamCreateWithFlags for a list of valid flags that can be passed
 * \param priority - Priority of the stream. Lower numbers represent higher priorities.
 *                   See ::lwdaDeviceGetStreamPriorityRange for more information about
 *                   the meaningful stream priorities that can be passed.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \note Stream priorities are supported only on GPUs
 * with compute capability 3.5 or higher.
 *
 * \note In the current implementation, only compute kernels launched in
 * priority streams are affected by the stream's priority. Stream priorities have
 * no effect on host-to-device and device-to-host memory operations.
 *
 * \sa ::lwdaStreamCreate,
 * ::lwdaStreamCreateWithFlags,
 * ::lwdaDeviceGetStreamPriorityRange,
 * ::lwdaStreamGetPriority,
 * ::lwdaStreamQuery,
 * ::lwdaStreamWaitEvent,
 * ::lwdaStreamAddCallback,
 * ::lwdaStreamSynchronize,
 * ::lwdaStreamDestroy,
 * ::lwStreamCreateWithPriority
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamCreateWithPriority(lwdaStream_t *pStream, unsigned int flags, int priority);

/**
 * \brief Query the priority of a stream
 *
 * Query the priority of a stream. The priority is returned in in \p priority.
 * Note that if the stream was created with a priority outside the meaningful
 * numerical range returned by ::lwdaDeviceGetStreamPriorityRange,
 * this function returns the clamped priority.
 * See ::lwdaStreamCreateWithPriority for details about priority clamping.
 *
 * \param hStream    - Handle to the stream to be queried
 * \param priority   - Pointer to a signed integer in which the stream's priority is returned
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreateWithPriority,
 * ::lwdaDeviceGetStreamPriorityRange,
 * ::lwdaStreamGetFlags,
 * ::lwStreamGetPriority
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamGetPriority(lwdaStream_t hStream, int *priority);

/**
 * \brief Query the flags of a stream
 *
 * Query the flags of a stream. The flags are returned in \p flags.
 * See ::lwdaStreamCreateWithFlags for a list of valid flags.
 *
 * \param hStream - Handle to the stream to be queried
 * \param flags   - Pointer to an unsigned integer in which the stream's flags are returned
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreateWithPriority,
 * ::lwdaStreamCreateWithFlags,
 * ::lwdaStreamGetPriority,
 * ::lwStreamGetFlags
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamGetFlags(lwdaStream_t hStream, unsigned int *flags);

/**
 * \brief Resets all persisting lines in cache to normal status.
 *
 * Resets all persisting lines in cache to normal status.
 * Takes effect on function return.
 *
 * \return
 * ::lwdaSuccess,
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ lwdaError_t LWDARTAPI lwdaCtxResetPersistingL2Cache(void);

/**
 * \brief Copies attributes from source stream to destination stream.
 *
 * Copies attributes from source stream \p src to destination stream \p dst.
 * Both streams must have the same context.
 *
 * \param[out] dst Destination stream
 * \param[in] src Source stream
 * For attributes see ::lwdaStreamAttrID
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotSupported
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamCopyAttributes(lwdaStream_t dst, lwdaStream_t src);

 /**
 * \brief Queries stream attribute.
 *
 * Queries attribute \p attr from \p hStream and stores it in corresponding
 * member of \p value_out.
 *
 * \param[in] hStream
 * \param[in] attr
 * \param[out] value_out
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamGetAttribute(
        lwdaStream_t hStream, lwdaStreamAttrID attr,
        lwdaStreamAttrValue *value_out);

 /**
 * \brief Sets stream attribute.
 *
 * Sets attribute \p attr on \p hStream from corresponding attribute of
 * \p value. The updated attribute will be applied to subsequent work
 * submitted to the stream. It will not affect previously submitted work.
 *
 * \param[out] hStream
 * \param[in] attr
 * \param[in] value
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamSetAttribute(
        lwdaStream_t hStream, lwdaStreamAttrID attr,
        const lwdaStreamAttrValue *value);

 /**
 * \brief Destroys and cleans up an asynchronous stream
 *
 * Destroys and cleans up the asynchronous stream specified by \p stream.
 *
 * In case the device is still doing work in the stream \p stream
 * when ::lwdaStreamDestroy() is called, the function will return immediately 
 * and the resources associated with \p stream will be released automatically 
 * once the device has completed all work in \p stream.
 *
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa ::lwdaStreamCreate,
 * ::lwdaStreamCreateWithFlags,
 * ::lwdaStreamQuery,
 * ::lwdaStreamWaitEvent,
 * ::lwdaStreamSynchronize,
 * ::lwdaStreamAddCallback,
 * ::lwStreamDestroy
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamDestroy(lwdaStream_t stream);

/**
 * \brief Make a compute stream wait on an event
 *
 * Makes all future work submitted to \p stream wait for all work captured in
 * \p event.  See ::lwdaEventRecord() for details on what is captured by an event.
 * The synchronization will be performed efficiently on the device when applicable.
 * \p event may be from a different device than \p stream.
 *
 * flags include:
 * - ::lwdaEventWaitDefault: Default event creation flag.
 * - ::lwdaEventWaitExternal: Event is captured in the graph as an external
 *   event node when performing stream capture.
 *
 * \param stream - Stream to wait
 * \param event  - Event to wait on
 * \param flags  - Parameters for the operation(See above)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate, ::lwdaStreamCreateWithFlags, ::lwdaStreamQuery, ::lwdaStreamSynchronize, ::lwdaStreamAddCallback, ::lwdaStreamDestroy,
 * ::lwStreamWaitEvent
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamWaitEvent(lwdaStream_t stream, lwdaEvent_t event, unsigned int flags __dv(0));

/**
 * Type of stream callback functions.
 * \param stream The stream as passed to ::lwdaStreamAddCallback, may be NULL.
 * \param status ::lwdaSuccess or any persistent error on the stream.
 * \param userData User parameter provided at registration.
 */
typedef void (LWDART_CB *lwdaStreamCallback_t)(lwdaStream_t stream, lwdaError_t status, void *userData);

/**
 * \brief Add a callback to a compute stream
 *
 * \note This function is slated for eventual deprecation and removal. If
 * you do not require the callback to execute in case of a device error,
 * consider using ::lwdaLaunchHostFunc. Additionally, this function is not
 * supported with ::lwdaStreamBeginCapture and ::lwdaStreamEndCapture, unlike
 * ::lwdaLaunchHostFunc.
 *
 * Adds a callback to be called on the host after all lwrrently enqueued
 * items in the stream have completed.  For each 
 * lwdaStreamAddCallback call, a callback will be exelwted exactly once.
 * The callback will block later work in the stream until it is finished.
 *
 * The callback may be passed ::lwdaSuccess or an error code.  In the event
 * of a device error, all subsequently exelwted callbacks will receive an
 * appropriate ::lwdaError_t.
 *
 * Callbacks must not make any LWCA API calls.  Attempting to use LWCA APIs
 * may result in ::lwdaErrorNotPermitted.  Callbacks must not perform any
 * synchronization that may depend on outstanding device work or other callbacks
 * that are not mandated to run earlier.  Callbacks without a mandated order
 * (in independent streams) execute in undefined order and may be serialized.
 *
 * For the purposes of Unified Memory, callback exelwtion makes a number of
 * guarantees:
 * <ul>
 *   <li>The callback stream is considered idle for the duration of the
 *   callback.  Thus, for example, a callback may always use memory attached
 *   to the callback stream.</li>
 *   <li>The start of exelwtion of a callback has the same effect as
 *   synchronizing an event recorded in the same stream immediately prior to
 *   the callback.  It thus synchronizes streams which have been "joined"
 *   prior to the callback.</li>
 *   <li>Adding device work to any stream does not have the effect of making
 *   the stream active until all preceding callbacks have exelwted.  Thus, for
 *   example, a callback might use global attached memory even if work has
 *   been added to another stream, if it has been properly ordered with an
 *   event.</li>
 *   <li>Completion of a callback does not cause a stream to become
 *   active except as described above.  The callback stream will remain idle
 *   if no device work follows the callback, and will remain idle across
 *   conselwtive callbacks without device work in between.  Thus, for example,
 *   stream synchronization can be done by signaling from a callback at the
 *   end of the stream.</li>
 * </ul>
 *
 * \param stream   - Stream to add callback to
 * \param callback - The function to call once preceding stream operations are complete
 * \param userData - User specified data to be passed to the callback function
 * \param flags    - Reserved for future use, must be 0
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate, ::lwdaStreamCreateWithFlags, ::lwdaStreamQuery, ::lwdaStreamSynchronize, ::lwdaStreamWaitEvent, ::lwdaStreamDestroy, ::lwdaMallocManaged, ::lwdaStreamAttachMemAsync,
 * ::lwdaLaunchHostFunc, ::lwStreamAddCallback
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamAddCallback(lwdaStream_t stream,
        lwdaStreamCallback_t callback, void *userData, unsigned int flags);

/**
 * \brief Waits for stream tasks to complete
 *
 * Blocks until \p stream has completed all operations. If the
 * ::lwdaDeviceScheduleBlockingSync flag was set for this device, 
 * the host thread will block until the stream is finished with 
 * all of its tasks.
 *
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate, ::lwdaStreamCreateWithFlags, ::lwdaStreamQuery, ::lwdaStreamWaitEvent, ::lwdaStreamAddCallback, ::lwdaStreamDestroy,
 * ::lwStreamSynchronize
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamSynchronize(lwdaStream_t stream);

/**
 * \brief Queries an asynchronous stream for completion status
 *
 * Returns ::lwdaSuccess if all operations in \p stream have
 * completed, or ::lwdaErrorNotReady if not.
 *
 * For the purposes of Unified Memory, a return value of ::lwdaSuccess
 * is equivalent to having called ::lwdaStreamSynchronize().
 *
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotReady,
 * ::lwdaErrorIlwalidResourceHandle
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate, ::lwdaStreamCreateWithFlags, ::lwdaStreamWaitEvent, ::lwdaStreamSynchronize, ::lwdaStreamAddCallback, ::lwdaStreamDestroy,
 * ::lwStreamQuery
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamQuery(lwdaStream_t stream);

/**
 * \brief Attach memory to a stream asynchronously
 *
 * Enqueues an operation in \p stream to specify stream association of
 * \p length bytes of memory starting from \p devPtr. This function is a
 * stream-ordered operation, meaning that it is dependent on, and will
 * only take effect when, previous work in stream has completed. Any
 * previous association is automatically replaced.
 *
 * \p devPtr must point to an one of the following types of memories:
 * - managed memory declared using the __managed__ keyword or allocated with
 *   ::lwdaMallocManaged.
 * - a valid host-accessible region of system-allocated pageable memory. This
 *   type of memory may only be specified if the device associated with the
 *   stream reports a non-zero value for the device attribute
 *   ::lwdaDevAttrPageableMemoryAccess.
 *
 * For managed allocations, \p length must be either zero or the entire
 * allocation's size. Both indicate that the entire allocation's stream
 * association is being changed. Lwrrently, it is not possible to change stream
 * association for a portion of a managed allocation.
 *
 * For pageable allocations, \p length must be non-zero.
 *
 * The stream association is specified using \p flags which must be
 * one of ::lwdaMemAttachGlobal, ::lwdaMemAttachHost or ::lwdaMemAttachSingle.
 * The default value for \p flags is ::lwdaMemAttachSingle
 * If the ::lwdaMemAttachGlobal flag is specified, the memory can be accessed
 * by any stream on any device.
 * If the ::lwdaMemAttachHost flag is specified, the program makes a guarantee
 * that it won't access the memory on the device from any stream on a device that
 * has a zero value for the device attribute ::lwdaDevAttrConlwrrentManagedAccess.
 * If the ::lwdaMemAttachSingle flag is specified and \p stream is associated with
 * a device that has a zero value for the device attribute ::lwdaDevAttrConlwrrentManagedAccess,
 * the program makes a guarantee that it will only access the memory on the device
 * from \p stream. It is illegal to attach singly to the NULL stream, because the
 * NULL stream is a virtual global stream and not a specific stream. An error will
 * be returned in this case.
 *
 * When memory is associated with a single stream, the Unified Memory system will
 * allow CPU access to this memory region so long as all operations in \p stream
 * have completed, regardless of whether other streams are active. In effect,
 * this constrains exclusive ownership of the managed memory region by
 * an active GPU to per-stream activity instead of whole-GPU activity.
 *
 * Accessing memory on the device from streams that are not associated with
 * it will produce undefined results. No error checking is performed by the
 * Unified Memory system to ensure that kernels launched into other streams
 * do not access this region. 
 *
 * It is a program's responsibility to order calls to ::lwdaStreamAttachMemAsync
 * via events, synchronization or other means to ensure legal access to memory
 * at all times. Data visibility and coherency will be changed appropriately
 * for all kernels which follow a stream-association change.
 *
 * If \p stream is destroyed while data is associated with it, the association is
 * removed and the association reverts to the default visibility of the allocation
 * as specified at ::lwdaMallocManaged. For __managed__ variables, the default
 * association is always ::lwdaMemAttachGlobal. Note that destroying a stream is an
 * asynchronous operation, and as a result, the change to default association won't
 * happen until all work in the stream has completed.
 *
 * \param stream  - Stream in which to enqueue the attach operation
 * \param devPtr  - Pointer to memory (must be a pointer to managed memory or
 *                  to a valid host-accessible region of system-allocated
 *                  memory)
 * \param length  - Length of memory (defaults to zero)
 * \param flags   - Must be one of ::lwdaMemAttachGlobal, ::lwdaMemAttachHost or ::lwdaMemAttachSingle (defaults to ::lwdaMemAttachSingle)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotReady,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate, ::lwdaStreamCreateWithFlags, ::lwdaStreamWaitEvent, ::lwdaStreamSynchronize, ::lwdaStreamAddCallback, ::lwdaStreamDestroy, ::lwdaMallocManaged,
 * ::lwStreamAttachMemAsync
 */
#if defined(__cplusplus)
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamAttachMemAsync(lwdaStream_t stream, void *devPtr, size_t length __dv(0), unsigned int flags = lwdaMemAttachSingle);
#else
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamAttachMemAsync(lwdaStream_t stream, void *devPtr, size_t length __dv(0), unsigned int flags);
#endif

/**
 * \brief Begins graph capture on a stream
 *
 * Begin graph capture on \p stream. When a stream is in capture mode, all operations
 * pushed into the stream will not be exelwted, but will instead be captured into
 * a graph, which will be returned via ::lwdaStreamEndCapture. Capture may not be initiated
 * if \p stream is ::lwdaStreamLegacy. Capture must be ended on the same stream in which
 * it was initiated, and it may only be initiated if the stream is not already in capture
 * mode. The capture mode may be queried via ::lwdaStreamIsCapturing. A unique id
 * representing the capture sequence may be queried via ::lwdaStreamGetCaptureInfo.
 *
 * If \p mode is not ::lwdaStreamCaptureModeRelaxed, ::lwdaStreamEndCapture must be
 * called on this stream from the same thread.
 *
 * \note Kernels captured using this API must not use texture and surface references.
 *       Reading or writing through any texture or surface reference is undefined
 *       behavior. This restriction does not apply to texture and surface objects.
 *
 * \param stream - Stream in which to initiate capture
 * \param mode    - Controls the interaction of this capture sequence with other API
 *                  calls that are potentially unsafe. For more details see
 *                  ::lwdaThreadExchangeStreamCaptureMode.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamCreate,
 * ::lwdaStreamIsCapturing,
 * ::lwdaStreamEndCapture,
 * ::lwdaThreadExchangeStreamCaptureMode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamBeginCapture(lwdaStream_t stream, enum lwdaStreamCaptureMode mode);

/**
 * \brief Swaps the stream capture interaction mode for a thread
 *
 * Sets the calling thread's stream capture interaction mode to the value contained
 * in \p *mode, and overwrites \p *mode with the previous mode for the thread. To
 * facilitate deterministic behavior across function or module boundaries, callers
 * are encouraged to use this API in a push-pop fashion: \code
     lwdaStreamCaptureMode mode = desiredMode;
     lwdaThreadExchangeStreamCaptureMode(&mode);
     ...
     lwdaThreadExchangeStreamCaptureMode(&mode); // restore previous mode
 * \endcode
 *
 * During stream capture (see ::lwdaStreamBeginCapture), some actions, such as a call
 * to ::lwdaMalloc, may be unsafe. In the case of ::lwdaMalloc, the operation is
 * not enqueued asynchronously to a stream, and is not observed by stream capture.
 * Therefore, if the sequence of operations captured via ::lwdaStreamBeginCapture
 * depended on the allocation being replayed whenever the graph is launched, the
 * captured graph would be invalid.
 *
 * Therefore, stream capture places restrictions on API calls that can be made within
 * or conlwrrently to a ::lwdaStreamBeginCapture-::lwdaStreamEndCapture sequence. This
 * behavior can be controlled via this API and flags to ::lwdaStreamBeginCapture.
 *
 * A thread's mode is one of the following:
 * - \p lwdaStreamCaptureModeGlobal: This is the default mode. If the local thread has
 *   an ongoing capture sequence that was not initiated with
 *   \p lwdaStreamCaptureModeRelaxed at \p lwStreamBeginCapture, or if any other thread
 *   has a conlwrrent capture sequence initiated with \p lwdaStreamCaptureModeGlobal,
 *   this thread is prohibited from potentially unsafe API calls.
 * - \p lwdaStreamCaptureModeThreadLocal: If the local thread has an ongoing capture
 *   sequence not initiated with \p lwdaStreamCaptureModeRelaxed, it is prohibited
 *   from potentially unsafe API calls. Conlwrrent capture sequences in other threads
 *   are ignored.
 * - \p lwdaStreamCaptureModeRelaxed: The local thread is not prohibited from potentially
 *   unsafe API calls. Note that the thread is still prohibited from API calls which
 *   necessarily conflict with stream capture, for example, attempting ::lwdaEventQuery
 *   on an event that was last recorded inside a capture sequence.
 *
 * \param mode - Pointer to mode value to swap with the current mode
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamBeginCapture
 */
extern __host__ lwdaError_t LWDARTAPI lwdaThreadExchangeStreamCaptureMode(enum lwdaStreamCaptureMode *mode);

/**
 * \brief Ends capture on a stream, returning the captured graph
 *
 * End capture on \p stream, returning the captured graph via \p pGraph.
 * Capture must have been initiated on \p stream via a call to ::lwdaStreamBeginCapture.
 * If capture was ilwalidated, due to a violation of the rules of stream capture, then
 * a NULL graph will be returned.
 *
 * If the \p mode argument to ::lwdaStreamBeginCapture was not
 * ::lwdaStreamCaptureModeRelaxed, this call must be from the same thread as
 * ::lwdaStreamBeginCapture.
 *
 * \param stream - Stream to query
 * \param pGraph - The captured graph
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorStreamCaptureWrongThread
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamCreate,
 * ::lwdaStreamBeginCapture,
 * ::lwdaStreamIsCapturing
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamEndCapture(lwdaStream_t stream, lwdaGraph_t *pGraph);

/**
 * \brief Returns a stream's capture status
 *
 * Return the capture status of \p stream via \p pCaptureStatus. After a successful
 * call, \p *pCaptureStatus will contain one of the following:
 * - ::lwdaStreamCaptureStatusNone: The stream is not capturing.
 * - ::lwdaStreamCaptureStatusActive: The stream is capturing.
 * - ::lwdaStreamCaptureStatusIlwalidated: The stream was capturing but an error
 *   has ilwalidated the capture sequence. The capture sequence must be terminated
 *   with ::lwdaStreamEndCapture on the stream where it was initiated in order to
 *   continue using \p stream.
 *
 * Note that, if this is called on ::lwdaStreamLegacy (the "null stream") while
 * a blocking stream on the same device is capturing, it will return
 * ::lwdaErrorStreamCaptureImplicit and \p *pCaptureStatus is unspecified
 * after the call. The blocking stream capture is not ilwalidated.
 *
 * When a blocking stream is capturing, the legacy stream is in an
 * unusable state until the blocking stream capture is terminated. The legacy
 * stream is not supported for stream capture, but attempted use would have an
 * implicit dependency on the capturing stream(s).
 *
 * \param stream         - Stream to query
 * \param pCaptureStatus - Returns the stream's capture status
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorStreamCaptureImplicit
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamCreate,
 * ::lwdaStreamBeginCapture,
 * ::lwdaStreamEndCapture
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamIsCapturing(lwdaStream_t stream, enum lwdaStreamCaptureStatus *pCaptureStatus);

/**
 * \brief Query capture status of a stream
 *
 * Note there is a later version of this API, ::lwdaStreamGetCaptureInfo_v2. It will
 * supplant this version in 12.0, which is retained for minor version compatibility.
 *
 * Query the capture status of a stream and get a unique id representing
 * the capture sequence over the lifetime of the process.
 *
 * If called on ::lwdaStreamLegacy (the "null stream") while a stream not created
 * with ::lwdaStreamNonBlocking is capturing, returns ::lwdaErrorStreamCaptureImplicit.
 *
 * A valid id is returned only if both of the following are true:
 * - the call returns ::lwdaSuccess
 * - captureStatus is set to ::lwdaStreamCaptureStatusActive
 *
 * \param stream         - Stream to query
 * \param pCaptureStatus - Returns the stream's capture status
 * \param pId            - Returns the unique id of the capture sequence
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorStreamCaptureImplicit
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamGetCaptureInfo_v2,
 * ::lwdaStreamBeginCapture,
 * ::lwdaStreamIsCapturing
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamGetCaptureInfo(lwdaStream_t stream, enum lwdaStreamCaptureStatus *pCaptureStatus, unsigned long long *pId);

/**
 * \brief Query a stream's capture state (11.3+)
 *
 * Query stream state related to stream capture.
 *
 * If called on ::lwdaStreamLegacy (the "null stream") while a stream not created 
 * with ::lwdaStreamNonBlocking is capturing, returns ::lwdaErrorStreamCaptureImplicit.
 *
 * Valid data (other than capture status) is returned only if both of the following are true:
 * - the call returns lwdaSuccess
 * - the returned capture status is ::lwdaStreamCaptureStatusActive
 *
 * This version of lwdaStreamGetCaptureInfo is introduced in LWCA 11.3 and will supplant the
 * previous version ::lwdaStreamGetCaptureInfo in 12.0. Developers requiring compatibility
 * across minor versions to LWCA 11.0 (driver version 445) can do one of the following:
 * - Use the older version of the API, ::lwdaStreamGetCaptureInfo
 * - Pass null for all of \p graph_out, \p dependencies_out, and \p numDependencies_out.
 *
 * \param stream - The stream to query
 * \param captureStatus_out - Location to return the capture status of the stream; required
 * \param id_out - Optional location to return an id for the capture sequence, which is
 *           unique over the lifetime of the process
 * \param graph_out - Optional location to return the graph being captured into. All
 *           operations other than destroy and node removal are permitted on the graph
 *           while the capture sequence is in progress. This API does not transfer
 *           ownership of the graph, which is transferred or destroyed at
 *           ::lwdaStreamEndCapture. Note that the graph handle may be ilwalidated before
 *           end of capture for certain errors. Nodes that are or become
 *           unreachable from the original stream at ::lwdaStreamEndCapture due to direct
 *           actions on the graph do not trigger ::lwdaErrorStreamCaptureUnjoined.
 * \param dependencies_out - Optional location to store a pointer to an array of nodes.
 *           The next node to be captured in the stream will depend on this set of nodes,
 *           absent operations such as event wait which modify this set. The array pointer
 *           is valid until the next API call which operates on the stream or until end of
 *           capture. The node handles may be copied out and are valid until they or the
 *           graph is destroyed. The driver-owned array may also be passed directly to
 *           APIs that operate on the graph (not the stream) without copying.
 * \param numDependencies_out - Optional location to store the size of the array
 *           returned in dependencies_out.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorStreamCaptureImplicit
 * \note_graph_thread_safety
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamGetCaptureInfo,
 * ::lwdaStreamBeginCapture,
 * ::lwdaStreamIsCapturing,
 * ::lwdaStreamUpdateCaptureDependencies
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamGetCaptureInfo_v2(lwdaStream_t stream, enum lwdaStreamCaptureStatus *captureStatus_out, unsigned long long *id_out __dv(0), lwdaGraph_t *graph_out __dv(0), const lwdaGraphNode_t **dependencies_out __dv(0), size_t *numDependencies_out __dv(0));

/**
 * \brief Update the set of dependencies in a capturing stream (11.3+)
 *
 * Modifies the dependency set of a capturing stream. The dependency set is the set
 * of nodes that the next captured node in the stream will depend on.
 *
 * Valid flags are ::lwdaStreamAddCaptureDependencies and
 * ::lwdaStreamSetCaptureDependencies. These control whether the set passed to
 * the API is added to the existing set or replaces it. A flags value of 0 defaults
 * to ::lwdaStreamAddCaptureDependencies.
 *
 * Nodes that are removed from the dependency set via this API do not result in
 * ::lwdaErrorStreamCaptureUnjoined if they are unreachable from the stream at
 * ::lwdaStreamEndCapture.
 *
 * Returns ::lwdaErrorIllegalState if the stream is not capturing.
 *
 * This API is new in LWCA 11.3. Developers requiring compatibility across minor
 * versions of the LWCA driver to 11.0 should not use this API or provide a fallback.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIllegalState
 * \notefnerr
 *
 * \sa
 * ::lwdaStreamBeginCapture,
 * ::lwdaStreamGetCaptureInfo,
 * ::lwdaStreamGetCaptureInfo_v2
 */
extern __host__ lwdaError_t LWDARTAPI lwdaStreamUpdateCaptureDependencies(lwdaStream_t stream, lwdaGraphNode_t *dependencies, size_t numDependencies, unsigned int flags __dv(0));
/** @} */ /* END LWDART_STREAM */

/**
 * \defgroup LWDART_EVENT Event Management
 *
 * ___MANBRIEF___ event management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the event management functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Creates an event object
 *
 * Creates an event object for the current device using ::lwdaEventDefault.
 *
 * \param event - Newly created event
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*, unsigned int) "lwdaEventCreate (C++ API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventRecord, ::lwdaEventQuery,
 * ::lwdaEventSynchronize, ::lwdaEventDestroy, ::lwdaEventElapsedTime,
 * ::lwdaStreamWaitEvent,
 * ::lwEventCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEventCreate(lwdaEvent_t *event);

/**
 * \brief Creates an event object with the specified flags
 *
 * Creates an event object for the current device with the specified flags. Valid
 * flags include:
 * - ::lwdaEventDefault: Default event creation flag.
 * - ::lwdaEventBlockingSync: Specifies that event should use blocking
 *   synchronization. A host thread that uses ::lwdaEventSynchronize() to wait
 *   on an event created with this flag will block until the event actually
 *   completes.
 * - ::lwdaEventDisableTiming: Specifies that the created event does not need
 *   to record timing data.  Events created with this flag specified and
 *   the ::lwdaEventBlockingSync flag not specified will provide the best
 *   performance when used with ::lwdaStreamWaitEvent() and ::lwdaEventQuery().
 * - ::lwdaEventInterprocess: Specifies that the created event may be used as an
 *   interprocess event by ::lwdaIpcGetEventHandle(). ::lwdaEventInterprocess must
 *   be specified along with ::lwdaEventDisableTiming.
 *
 * \param event - Newly created event
 * \param flags - Flags for new event
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventSynchronize, ::lwdaEventDestroy, ::lwdaEventElapsedTime,
 * ::lwdaStreamWaitEvent,
 * ::lwEventCreate
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventCreateWithFlags(lwdaEvent_t *event, unsigned int flags);

/**
 * \brief Records an event
 *
 * Captures in \p event the contents of \p stream at the time of this call.
 * \p event and \p stream must be on the same LWCA context.
 * Calls such as ::lwdaEventQuery() or ::lwdaStreamWaitEvent() will then
 * examine or wait for completion of the work that was captured. Uses of
 * \p stream after this call do not modify \p event. See note on default
 * stream behavior for what is captured in the default case.
 *
 * ::lwdaEventRecord() can be called multiple times on the same event and
 * will overwrite the previously captured state. Other APIs such as
 * ::lwdaStreamWaitEvent() use the most recently captured state at the time
 * of the API call, and are not affected by later calls to
 * ::lwdaEventRecord(). Before the first call to ::lwdaEventRecord(), an
 * event represents an empty set of work, so for example ::lwdaEventQuery()
 * would return ::lwdaSuccess.
 *
 * \param event  - Event to record
 * \param stream - Stream in which to record event
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorLaunchFailure
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventQuery,
 * ::lwdaEventSynchronize, ::lwdaEventDestroy, ::lwdaEventElapsedTime,
 * ::lwdaStreamWaitEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwEventRecord
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecord(lwdaEvent_t event, lwdaStream_t stream __dv(0));

/**
 * \brief Records an event
 *
 * Captures in \p event the contents of \p stream at the time of this call.
 * \p event and \p stream must be on the same LWCA context.
 * Calls such as ::lwdaEventQuery() or ::lwdaStreamWaitEvent() will then
 * examine or wait for completion of the work that was captured. Uses of
 * \p stream after this call do not modify \p event. See note on default
 * stream behavior for what is captured in the default case.
 *
 * ::lwdaEventRecordWithFlags() can be called multiple times on the same event and
 * will overwrite the previously captured state. Other APIs such as
 * ::lwdaStreamWaitEvent() use the most recently captured state at the time
 * of the API call, and are not affected by later calls to
 * ::lwdaEventRecordWithFlags(). Before the first call to ::lwdaEventRecordWithFlags(), an
 * event represents an empty set of work, so for example ::lwdaEventQuery()
 * would return ::lwdaSuccess.
 *
 * flags include:
 * - ::lwdaEventRecordDefault: Default event creation flag.
 * - ::lwdaEventRecordExternal: Event is captured in the graph as an external
 *   event node when performing stream capture.
 *
 * \param event  - Event to record
 * \param stream - Stream in which to record event
 * \param flags  - Parameters for the operation(See above)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorLaunchFailure
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventQuery,
 * ::lwdaEventSynchronize, ::lwdaEventDestroy, ::lwdaEventElapsedTime,
 * ::lwdaStreamWaitEvent,
 * ::lwdaEventRecord,
 * ::lwEventRecord,
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecordWithFlags(lwdaEvent_t event, lwdaStream_t stream __dv(0), unsigned int flags __dv(0));
#endif

/**
 * \brief Queries an event's status
 *
 * Queries the status of all work lwrrently captured by \p event. See
 * ::lwdaEventRecord() for details on what is captured by an event.
 *
 * Returns ::lwdaSuccess if all captured work has been completed, or
 * ::lwdaErrorNotReady if any captured work is incomplete.
 *
 * For the purposes of Unified Memory, a return value of ::lwdaSuccess
 * is equivalent to having called ::lwdaEventSynchronize().
 *
 * \param event - Event to query
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotReady,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorLaunchFailure
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventRecord,
 * ::lwdaEventSynchronize, ::lwdaEventDestroy, ::lwdaEventElapsedTime,
 * ::lwEventQuery
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEventQuery(lwdaEvent_t event);

/**
 * \brief Waits for an event to complete
 *
 * Waits until the completion of all work lwrrently captured in \p event.
 * See ::lwdaEventRecord() for details on what is captured by an event.
 *
 * Waiting for an event that was created with the ::lwdaEventBlockingSync
 * flag will cause the calling CPU thread to block until the event has
 * been completed by the device.  If the ::lwdaEventBlockingSync flag has
 * not been set, then the CPU thread will busy-wait until the event has
 * been completed by the device.
 *
 * \param event - Event to wait for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorLaunchFailure
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventRecord,
 * ::lwdaEventQuery, ::lwdaEventDestroy, ::lwdaEventElapsedTime,
 * ::lwEventSynchronize
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEventSynchronize(lwdaEvent_t event);

/**
 * \brief Destroys an event object
 *
 * Destroys the event specified by \p event.
 *
 * An event may be destroyed before it is complete (i.e., while
 * ::lwdaEventQuery() would return ::lwdaErrorNotReady). In this case, the
 * call does not block on completion of the event, and any associated
 * resources will automatically be released asynchronously at completion.
 *
 * \param event - Event to destroy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorLaunchFailure
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventQuery,
 * ::lwdaEventSynchronize, ::lwdaEventRecord, ::lwdaEventElapsedTime,
 * ::lwEventDestroy
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventDestroy(lwdaEvent_t event);

/**
 * \brief Computes the elapsed time between events
 *
 * Computes the elapsed time between two events (in milliseconds with a
 * resolution of around 0.5 microseconds).
 *
 * If either event was last recorded in a non-NULL stream, the resulting time
 * may be greater than expected (even if both used the same stream handle). This
 * happens because the ::lwdaEventRecord() operation takes place asynchronously
 * and there is no guarantee that the measured latency is actually just between
 * the two events. Any number of other different stream operations could execute
 * in between the two measured events, thus altering the timing in a significant
 * way.
 *
 * If ::lwdaEventRecord() has not been called on either event, then
 * ::lwdaErrorIlwalidResourceHandle is returned. If ::lwdaEventRecord() has been
 * called on both events but one or both of them has not yet been completed
 * (that is, ::lwdaEventQuery() would return ::lwdaErrorNotReady on at least one
 * of the events), ::lwdaErrorNotReady is returned. If either event was created
 * with the ::lwdaEventDisableTiming flag, then this function will return
 * ::lwdaErrorIlwalidResourceHandle.
 *
 * \param ms    - Time between \p start and \p end in ms
 * \param start - Starting event
 * \param end   - Ending event
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotReady,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorLaunchFailure
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaEventCreate(lwdaEvent_t*) "lwdaEventCreate (C API)",
 * ::lwdaEventCreateWithFlags, ::lwdaEventQuery,
 * ::lwdaEventSynchronize, ::lwdaEventDestroy, ::lwdaEventRecord,
 * ::lwEventElapsedTime
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEventElapsedTime(float *ms, lwdaEvent_t start, lwdaEvent_t end);

/** @} */ /* END LWDART_EVENT */

/**
 * \defgroup LWDART_EXTRES_INTEROP External Resource Interoperability
 *
 * ___MANBRIEF___ External resource interoperability functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the external resource interoperability functions of the LWCA
 * runtime application programming interface.
 *
 * @{
 */

/**
 * \brief Imports an external memory object
 *
 * Imports an externally allocated memory object and returns
 * a handle to that in \p extMem_out.
 *
 * The properties of the handle being imported must be described in
 * \p memHandleDesc. The ::lwdaExternalMemoryHandleDesc structure
 * is defined as follows:
 *
 * \code
        typedef struct lwdaExternalMemoryHandleDesc_st {
            lwdaExternalMemoryHandleType type;
            union {
                int fd;
                struct {
                    void *handle;
                    const void *name;
                } win32;
                const void *lwSciBufObject;
            } handle;
            unsigned long long size;
            unsigned int flags;
        } lwdaExternalMemoryHandleDesc;
 * \endcode
 *
 * where ::lwdaExternalMemoryHandleDesc::type specifies the type
 * of handle being imported. ::lwdaExternalMemoryHandleType is
 * defined as:
 *
 * \code
        typedef enum lwdaExternalMemoryHandleType_enum {
            lwdaExternalMemoryHandleTypeOpaqueFd         = 1,
            lwdaExternalMemoryHandleTypeOpaqueWin32      = 2,
            lwdaExternalMemoryHandleTypeOpaqueWin32Kmt   = 3,
            lwdaExternalMemoryHandleTypeD3D12Heap        = 4,
            lwdaExternalMemoryHandleTypeD3D12Resource    = 5,
	        lwdaExternalMemoryHandleTypeD3D11Resource    = 6,
		    lwdaExternalMemoryHandleTypeD3D11ResourceKmt = 7,
            lwdaExternalMemoryHandleTypeLwSciBuf         = 8
        } lwdaExternalMemoryHandleType;
 * \endcode
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeOpaqueFd, then
 * ::lwdaExternalMemoryHandleDesc::handle::fd must be a valid
 * file descriptor referencing a memory object. Ownership of
 * the file descriptor is transferred to the LWCA driver when the
 * handle is imported successfully. Performing any operations on the
 * file descriptor after it is imported results in undefined behavior.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeOpaqueWin32, then exactly one
 * of ::lwdaExternalMemoryHandleDesc::handle::win32::handle and
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalMemoryHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * references a memory object. Ownership of this handle is
 * not transferred to LWCA after the import operation, so the
 * application must release the handle using the appropriate system
 * call. If ::lwdaExternalMemoryHandleDesc::handle::win32::name
 * is not NULL, then it must point to a NULL-terminated array of
 * UTF-16 characters that refers to a memory object.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeOpaqueWin32Kmt, then
 * ::lwdaExternalMemoryHandleDesc::handle::win32::handle must
 * be non-NULL and
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name
 * must be NULL. The handle specified must be a globally shared KMT
 * handle. This handle does not hold a reference to the underlying
 * object, and thus will be invalid when all references to the
 * memory object are destroyed.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeD3D12Heap, then exactly one
 * of ::lwdaExternalMemoryHandleDesc::handle::win32::handle and
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalMemoryHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * is returned by ID3D12Device::CreateSharedHandle when referring to a
 * ID3D12Heap object. This handle holds a reference to the underlying
 * object. If ::lwdaExternalMemoryHandleDesc::handle::win32::name
 * is not NULL, then it must point to a NULL-terminated array of
 * UTF-16 characters that refers to a ID3D12Heap object.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeD3D12Resource, then exactly one
 * of ::lwdaExternalMemoryHandleDesc::handle::win32::handle and
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalMemoryHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * is returned by ID3D12Device::CreateSharedHandle when referring to a
 * ID3D12Resource object. This handle holds a reference to the
 * underlying object. If
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name
 * is not NULL, then it must point to a NULL-terminated array of
 * UTF-16 characters that refers to a ID3D12Resource object.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeD3D11Resource,then exactly one
 * of ::lwdaExternalMemoryHandleDesc::handle::win32::handle and
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalMemoryHandleDesc::handle::win32::handle is    
 * not NULL, then it must represent a valid shared NT handle that is  
 * returned by  IDXGIResource1::CreateSharedHandle when referring to a 
 * ID3D11Resource object. If
 * ::lwdaExternalMemoryHandleDesc::handle::win32::name
 * is not NULL, then it must point to a NULL-terminated array of
 * UTF-16 characters that refers to a ID3D11Resource object.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeD3D11ResourceKmt, then
 * ::lwdaExternalMemoryHandleDesc::handle::win32::handle must
 * be non-NULL and ::lwdaExternalMemoryHandleDesc::handle::win32::name
 * must be NULL. The handle specified must be a valid shared KMT
 * handle that is returned by IDXGIResource::GetSharedHandle when
 * referring to a ID3D11Resource object.
 *
 * If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeLwSciBuf, then
 * ::lwdaExternalMemoryHandleDesc::handle::lwSciBufObject must be NON-NULL
 * and reference a valid LwSciBuf object.
 * If the LwSciBuf object imported into LWCA is also mapped by other drivers, then the
 * application must use ::lwdaWaitExternalSemaphoresAsync or ::lwdaSignalExternalSemaphoresAsync
 * as approprriate barriers to maintain coherence between LWCA and the other drivers.
 * See ::lwdaExternalSemaphoreWaitSkipLwSciBufMemSync and ::lwdaExternalSemaphoreSignalSkipLwSciBufMemSync 
 * for memory synchronization.
 *
 * The size of the memory object must be specified in
 * ::lwdaExternalMemoryHandleDesc::size.
 *
 * Specifying the flag ::lwdaExternalMemoryDedicated in
 * ::lwdaExternalMemoryHandleDesc::flags indicates that the
 * resource is a dedicated resource. The definition of what a
 * dedicated resource is outside the scope of this extension.
 * This flag must be set if ::lwdaExternalMemoryHandleDesc::type
 * is one of the following:
 * ::lwdaExternalMemoryHandleTypeD3D12Resource
 * ::lwdaExternalMemoryHandleTypeD3D11Resource
 * ::lwdaExternalMemoryHandleTypeD3D11ResourceKmt
 *
 * \param extMem_out    - Returned handle to an external memory object
 * \param memHandleDesc - Memory import handle descriptor
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \note If the Vulkan memory imported into LWCA is mapped on the CPU then the
 * application must use vkIlwalidateMappedMemoryRanges/vkFlushMappedMemoryRanges
 * as well as appropriate Vulkan pipeline barriers to maintain coherence between
 * CPU and GPU. For more information on these APIs, please refer to "Synchronization
 * and Cache Control" chapter from Vulkan specification.
 *
 *
 * \sa ::lwdaDestroyExternalMemory,
 * ::lwdaExternalMemoryGetMappedBuffer,
 * ::lwdaExternalMemoryGetMappedMipmappedArray
 */
extern __host__ lwdaError_t LWDARTAPI lwdaImportExternalMemory(lwdaExternalMemory_t *extMem_out, const struct lwdaExternalMemoryHandleDesc *memHandleDesc);

/**
 * \brief Maps a buffer onto an imported memory object
 *
 * Maps a buffer onto an imported memory object and returns a device
 * pointer in \p devPtr.
 *
 * The properties of the buffer being mapped must be described in
 * \p bufferDesc. The ::lwdaExternalMemoryBufferDesc structure is
 * defined as follows:
 *
 * \code
        typedef struct lwdaExternalMemoryBufferDesc_st {
            unsigned long long offset;
            unsigned long long size;
            unsigned int flags;
        } lwdaExternalMemoryBufferDesc;
 * \endcode
 *
 * where ::lwdaExternalMemoryBufferDesc::offset is the offset in
 * the memory object where the buffer's base address is.
 * ::lwdaExternalMemoryBufferDesc::size is the size of the buffer.
 * ::lwdaExternalMemoryBufferDesc::flags must be zero.
 *
 * The offset and size have to be suitably aligned to match the
 * requirements of the external API. Mapping two buffers whose ranges
 * overlap may or may not result in the same virtual address being
 * returned for the overlapped portion. In such cases, the application
 * must ensure that all accesses to that region from the GPU are
 * volatile. Otherwise writes made via one address are not guaranteed
 * to be visible via the other address, even if they're issued by the
 * same thread. It is recommended that applications map the combined
 * range instead of mapping separate buffers and then apply the
 * appropriate offsets to the returned pointer to derive the
 * individual buffers.
 *
 * The returned pointer \p devPtr must be freed using ::lwdaFree.
 *
 * \param devPtr     - Returned device pointer to buffer
 * \param extMem     - Handle to external memory object
 * \param bufferDesc - Buffer descriptor
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaImportExternalMemory,
 * ::lwdaDestroyExternalMemory,
 * ::lwdaExternalMemoryGetMappedMipmappedArray
 */
extern __host__ lwdaError_t LWDARTAPI lwdaExternalMemoryGetMappedBuffer(void **devPtr, lwdaExternalMemory_t extMem, const struct lwdaExternalMemoryBufferDesc *bufferDesc);

/**
 * \brief Maps a LWCA mipmapped array onto an external memory object
 *
 * Maps a LWCA mipmapped array onto an external object and returns a
 * handle to it in \p mipmap.
 *
 * The properties of the LWCA mipmapped array being mapped must be
 * described in \p mipmapDesc. The structure
 * ::lwdaExternalMemoryMipmappedArrayDesc is defined as follows:
 *
 * \code
        typedef struct lwdaExternalMemoryMipmappedArrayDesc_st {
            unsigned long long offset;
            lwdaChannelFormatDesc formatDesc;
            lwdaExtent extent;
            unsigned int flags;
            unsigned int numLevels;
        } lwdaExternalMemoryMipmappedArrayDesc;
 * \endcode
 *
 * where ::lwdaExternalMemoryMipmappedArrayDesc::offset is the
 * offset in the memory object where the base level of the mipmap
 * chain is.
 * ::lwdaExternalMemoryMipmappedArrayDesc::formatDesc describes the
 * format of the data.
 * ::lwdaExternalMemoryMipmappedArrayDesc::extent specifies the
 * dimensions of the base level of the mipmap chain.
 * ::lwdaExternalMemoryMipmappedArrayDesc::flags are flags associated
 * with LWCA mipmapped arrays. For further details, please refer to
 * the documentation for ::lwdaMalloc3DArray. Note that if the mipmapped
 * array is bound as a color target in the graphics API, then the flag
 * ::lwdaArrayColorAttachment must be specified in 
 * ::lwdaExternalMemoryMipmappedArrayDesc::flags.
 * ::lwdaExternalMemoryMipmappedArrayDesc::numLevels specifies
 * the total number of levels in the mipmap chain.
 *
 * The returned LWCA mipmapped array must be freed using ::lwdaFreeMipmappedArray.
 *
 * \param mipmap     - Returned LWCA mipmapped array
 * \param extMem     - Handle to external memory object
 * \param mipmapDesc - LWCA array descriptor
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note On CheetAh devices, this API will always attempt to do a compressed mapping when the ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeOpaqueFd
 *
 * \sa ::lwdaImportExternalMemory,
 * ::lwdaDestroyExternalMemory,
 * ::lwdaExternalMemoryGetMappedBuffer
 *
 * \note If ::lwdaExternalMemoryHandleDesc::type is
 * ::lwdaExternalMemoryHandleTypeLwSciBuf, then
 * ::lwdaExternalMemoryMipmappedArrayDesc::numLevels must not be greater than 1.
 */
extern __host__ lwdaError_t LWDARTAPI lwdaExternalMemoryGetMappedMipmappedArray(lwdaMipmappedArray_t *mipmap, lwdaExternalMemory_t extMem, const struct lwdaExternalMemoryMipmappedArrayDesc *mipmapDesc);

/**
 * \brief Destroys an external memory object.
 *
 * Destroys the specified external memory object. Any existing buffers
 * and LWCA mipmapped arrays mapped onto this object must no longer be
 * used and must be explicitly freed using ::lwdaFree and
 * ::lwdaFreeMipmappedArray respectively.
 *
 * \param extMem - External memory object to be destroyed
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa ::lwdaImportExternalMemory,
 * ::lwdaExternalMemoryGetMappedBuffer,
 * ::lwdaExternalMemoryGetMappedMipmappedArray
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDestroyExternalMemory(lwdaExternalMemory_t extMem);

/**
 * \brief Imports an external semaphore
 *
 * Imports an externally allocated synchronization object and returns
 * a handle to that in \p extSem_out.
 *
 * The properties of the handle being imported must be described in
 * \p semHandleDesc. The ::lwdaExternalSemaphoreHandleDesc is defined
 * as follows:
 *
 * \code
        typedef struct lwdaExternalSemaphoreHandleDesc_st {
            lwdaExternalSemaphoreHandleType type;
            union {
                int fd;
                struct {
                    void *handle;
                    const void *name;
                } win32;
                const void* LwSciSyncObj;
            } handle;
            unsigned int flags;
        } lwdaExternalSemaphoreHandleDesc;
 * \endcode
 *
 * where ::lwdaExternalSemaphoreHandleDesc::type specifies the type of
 * handle being imported. ::lwdaExternalSemaphoreHandleType is defined
 * as:
 *
 * \code
        typedef enum lwdaExternalSemaphoreHandleType_enum {
            lwdaExternalSemaphoreHandleTypeOpaqueFd                = 1,
            lwdaExternalSemaphoreHandleTypeOpaqueWin32             = 2,
            lwdaExternalSemaphoreHandleTypeOpaqueWin32Kmt          = 3,
            lwdaExternalSemaphoreHandleTypeD3D12Fence              = 4,
            lwdaExternalSemaphoreHandleTypeD3D11Fence              = 5,
            lwdaExternalSemaphoreHandleTypeLwSciSync               = 6,
            lwdaExternalSemaphoreHandleTypeKeyedMutex              = 7,
            lwdaExternalSemaphoreHandleTypeKeyedMutexKmt           = 8,
            lwdaExternalSemaphoreHandleTypeTimelineSemaphoreFd     = 9,
            lwdaExternalSemaphoreHandleTypeTimelineSemaphoreWin32  = 10
        } lwdaExternalSemaphoreHandleType;
 * \endcode
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeOpaqueFd, then
 * ::lwdaExternalSemaphoreHandleDesc::handle::fd must be a valid file
 * descriptor referencing a synchronization object. Ownership of the
 * file descriptor is transferred to the LWCA driver when the handle
 * is imported successfully. Performing any operations on the file
 * descriptor after it is imported results in undefined behavior.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeOpaqueWin32, then exactly one of
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle and
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * references a synchronization object. Ownership of this handle is
 * not transferred to LWCA after the import operation, so the
 * application must release the handle using the appropriate system
 * call. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::name is
 * not NULL, then it must name a valid synchronization object.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeOpaqueWin32Kmt, then
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle must be
 * non-NULL and ::lwdaExternalSemaphoreHandleDesc::handle::win32::name
 * must be NULL. The handle specified must be a globally shared KMT
 * handle. This handle does not hold a reference to the underlying
 * object, and thus will be invalid when all references to the
 * synchronization object are destroyed.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeD3D12Fence, then exactly one of
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle and
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * is returned by ID3D12Device::CreateSharedHandle when referring to a
 * ID3D12Fence object. This handle holds a reference to the underlying
 * object. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::name
 * is not NULL, then it must name a valid synchronization object that
 * refers to a valid ID3D12Fence object.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeD3D11Fence, then exactly one of
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle and
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * is returned by ID3D11Fence::CreateSharedHandle. If 
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::name
 * is not NULL, then it must name a valid synchronization object that
 * refers to a valid ID3D11Fence object.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeLwSciSync, then
 * ::lwdaExternalSemaphoreHandleDesc::handle::lwSciSyncObj
 * represents a valid LwSciSyncObj.
 *
 * ::lwdaExternalSemaphoreHandleTypeKeyedMutex, then exactly one of
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle and
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle
 * is not NULL, then it represent a valid shared NT handle that
 * is returned by IDXGIResource1::CreateSharedHandle when referring to
 * a IDXGIKeyedMutex object.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeKeyedMutexKmt, then
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle must be
 * non-NULL and ::lwdaExternalSemaphoreHandleDesc::handle::win32::name
 * must be NULL. The handle specified must represent a valid KMT
 * handle that is returned by IDXGIResource::GetSharedHandle when
 * referring to a IDXGIKeyedMutex object.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeTimelineSemaphoreFd, then
 * ::lwdaExternalSemaphoreHandleDesc::handle::fd must be a valid file
 * descriptor referencing a synchronization object. Ownership of the
 * file descriptor is transferred to the LWCA driver when the handle
 * is imported successfully. Performing any operations on the file
 * descriptor after it is imported results in undefined behavior.
 *
 * If ::lwdaExternalSemaphoreHandleDesc::type is
 * ::lwdaExternalSemaphoreHandleTypeTimelineSemaphoreWin32, then exactly one of
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle and
 * ::lwdaExternalSemaphoreHandleDesc::handle::win32::name must not be
 * NULL. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::handle
 * is not NULL, then it must represent a valid shared NT handle that
 * references a synchronization object. Ownership of this handle is
 * not transferred to LWCA after the import operation, so the
 * application must release the handle using the appropriate system
 * call. If ::lwdaExternalSemaphoreHandleDesc::handle::win32::name is
 * not NULL, then it must name a valid synchronization object.
 *
 * \param extSem_out    - Returned handle to an external semaphore
 * \param semHandleDesc - Semaphore import handle descriptor
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDestroyExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaImportExternalSemaphore(lwdaExternalSemaphore_t *extSem_out, const struct lwdaExternalSemaphoreHandleDesc *semHandleDesc);

/**
 * \brief Signals a set of external semaphore objects
 *
 * Enqueues a signal operation on a set of externally allocated
 * semaphore object in the specified stream. The operations will be
 * exelwted when all prior operations in the stream complete.
 *
 * The exact semantics of signaling a semaphore depends on the type of
 * the object.
 *
 * If the semaphore object is any one of the following types:
 * ::lwdaExternalSemaphoreHandleTypeOpaqueFd,
 * ::lwdaExternalSemaphoreHandleTypeOpaqueWin32,
 * ::lwdaExternalSemaphoreHandleTypeOpaqueWin32Kmt
 * then signaling the semaphore will set it to the signaled state.
 *
 * If the semaphore object is any one of the following types:
 * ::lwdaExternalSemaphoreHandleTypeD3D12Fence,
 * ::lwdaExternalSemaphoreHandleTypeD3D11Fence,
 * ::lwdaExternalSemaphoreHandleTypeTimelineSemaphoreFd,
 * ::lwdaExternalSemaphoreHandleTypeTimelineSemaphoreWin32
 * then the semaphore will be set to the value specified in
 * ::lwdaExternalSemaphoreSignalParams::params::fence::value.
 *
 * If the semaphore object is of the type ::lwdaExternalSemaphoreHandleTypeLwSciSync
 * this API sets ::lwdaExternalSemaphoreSignalParams::params::lwSciSync::fence to a
 * value that can be used by subsequent waiters of the same LwSciSync object to
 * order operations with those lwrrently submitted in \p stream. Such an update
 * will overwrite previous contents of
 * ::lwdaExternalSemaphoreSignalParams::params::lwSciSync::fence. By deefault,
 * signaling such an external semaphore object causes appropriate memory synchronization
 * operations to be performed over all the external memory objects that are imported as
 * ::lwdaExternalMemoryHandleTypeLwSciBuf. This ensures that any subsequent accesses
 * made by other importers of the same set of LwSciBuf memory object(s) are coherent.
 * These operations can be skipped by specifying the flag
 * ::lwdaExternalSemaphoreSignalSkipLwSciBufMemSync, which can be used as a
 * performance optimization when data coherency is not required. But specifying this
 * flag in scenarios where data coherency is required results in undefined behavior.
 * Also, for semaphore object of the type ::lwdaExternalSemaphoreHandleTypeLwSciSync,
 * if the LwSciSyncAttrList used to create the LwSciSyncObj had not set the flags in
 * ::lwdaDeviceGetLwSciSyncAttributes to lwdaLwSciSyncAttrSignal, this API will return
 * lwdaErrorNotSupported.
 *
 * If the semaphore object is any one of the following types:
 * ::lwdaExternalSemaphoreHandleTypeKeyedMutex,
 * ::lwdaExternalSemaphoreHandleTypeKeyedMutexKmt,
 * then the keyed mutex will be released with the key specified in
 * ::lwdaExternalSemaphoreSignalParams::params::keyedmutex::key.
 *
 * \param extSemArray - Set of external semaphores to be signaled
 * \param paramsArray - Array of semaphore parameters
 * \param numExtSems  - Number of semaphores to signal
 * \param stream     - Stream to enqueue the signal operations in
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaImportExternalSemaphore,
 * ::lwdaDestroyExternalSemaphore,
 * ::lwdaWaitExternalSemaphoresAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaSignalExternalSemaphoresAsync(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreSignalParams *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));

/**
 * \brief Waits on a set of external semaphore objects
 *
 * Enqueues a wait operation on a set of externally allocated
 * semaphore object in the specified stream. The operations will be
 * exelwted when all prior operations in the stream complete.
 *
 * The exact semantics of waiting on a semaphore depends on the type
 * of the object.
 *
 * If the semaphore object is any one of the following types:
 * ::lwdaExternalSemaphoreHandleTypeOpaqueFd,
 * ::lwdaExternalSemaphoreHandleTypeOpaqueWin32,
 * ::lwdaExternalSemaphoreHandleTypeOpaqueWin32Kmt
 * then waiting on the semaphore will wait until the semaphore reaches
 * the signaled state. The semaphore will then be reset to the
 * unsignaled state. Therefore for every signal operation, there can
 * only be one wait operation.
 *
 * If the semaphore object is any one of the following types:
 * ::lwdaExternalSemaphoreHandleTypeD3D12Fence,
 * ::lwdaExternalSemaphoreHandleTypeD3D11Fence,
 * ::lwdaExternalSemaphoreHandleTypeTimelineSemaphoreFd,
 * ::lwdaExternalSemaphoreHandleTypeTimelineSemaphoreWin32
 * then waiting on the semaphore will wait until the value of the
 * semaphore is greater than or equal to
 * ::lwdaExternalSemaphoreWaitParams::params::fence::value.
 *
 * If the semaphore object is of the type ::lwdaExternalSemaphoreHandleTypeLwSciSync
 * then, waiting on the semaphore will wait until the
 * ::lwdaExternalSemaphoreSignalParams::params::lwSciSync::fence is signaled by the
 * signaler of the LwSciSyncObj that was associated with this semaphore object.
 * By default, waiting on such an external semaphore object causes appropriate
 * memory synchronization operations to be performed over all external memory objects
 * that are imported as ::lwdaExternalMemoryHandleTypeLwSciBuf. This ensures that
 * any subsequent accesses made by other importers of the same set of LwSciBuf memory
 * object(s) are coherent. These operations can be skipped by specifying the flag
 * ::lwdaExternalSemaphoreWaitSkipLwSciBufMemSync, which can be used as a
 * performance optimization when data coherency is not required. But specifying this
 * flag in scenarios where data coherency is required results in undefined behavior.
 * Also, for semaphore object of the type ::lwdaExternalSemaphoreHandleTypeLwSciSync,
 * if the LwSciSyncAttrList used to create the LwSciSyncObj had not set the flags in
 * ::lwdaDeviceGetLwSciSyncAttributes to lwdaLwSciSyncAttrWait, this API will return
 * lwdaErrorNotSupported.
 *
 * If the semaphore object is any one of the following types:
 * ::lwdaExternalSemaphoreHandleTypeKeyedMutex,
 * ::lwdaExternalSemaphoreHandleTypeKeyedMutexKmt,
 * then the keyed mutex will be acquired when it is released with the key specified 
 * in ::lwdaExternalSemaphoreSignalParams::params::keyedmutex::key or
 * until the timeout specified by
 * ::lwdaExternalSemaphoreSignalParams::params::keyedmutex::timeoutMs
 * has lapsed. The timeout interval can either be a finite value
 * specified in milliseconds or an infinite value. In case an infinite
 * value is specified the timeout never elapses. The windows INFINITE
 * macro must be used to specify infinite timeout
 *
 * \param extSemArray - External semaphores to be waited on
 * \param paramsArray - Array of semaphore parameters
 * \param numExtSems  - Number of semaphores to wait on
 * \param stream      - Stream to enqueue the wait operations in
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle
 * ::lwdaErrorTimeout
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaImportExternalSemaphore,
 * ::lwdaDestroyExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaWaitExternalSemaphoresAsync(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreWaitParams *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));

/**
 * \brief Destroys an external semaphore
 *
 * Destroys an external semaphore object and releases any references
 * to the underlying resource. Any outstanding signals or waits must
 * have completed before the semaphore is destroyed.
 *
 * \param extSem - External semaphore to be destroyed
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa ::lwdaImportExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDestroyExternalSemaphore(lwdaExternalSemaphore_t extSem);

/** @} */ /* END LWDART_EXTRES_INTEROP */

/**
 * \defgroup LWDART_EXELWTION Exelwtion Control
 *
 * ___MANBRIEF___ exelwtion control functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the exelwtion control functions of the LWCA runtime
 * application programming interface.
 *
 * Some functions have overloaded C++ API template versions dolwmented separately in the
 * \ref LWDART_HIGHLEVEL "C++ API Routines" module.
 *
 * @{
 */

/**
 * \brief Launches a device function
 *
 * The function ilwokes kernel \p func on \p gridDim (\p gridDim.x &times; \p gridDim.y
 * &times; \p gridDim.z) grid of blocks. Each block contains \p blockDim (\p blockDim.x &times;
 * \p blockDim.y &times; \p blockDim.z) threads.
 *
 * If the kernel has N parameters the \p args should point to array of N pointers.
 * Each pointer, from <tt>args[0]</tt> to <tt>args[N - 1]</tt>, point to the region
 * of memory from which the actual parameter will be copied.
 *
 * For templated functions, pass the function symbol as follows:
 * func_name<template_arg_0,...,template_arg_N>
 *
 * \p sharedMem sets the amount of dynamic shared memory that will be available to
 * each thread block.
 *
 * \p stream specifies a stream the invocation is associated to.
 *
 * \param func        - Device function symbol
 * \param gridDim     - Grid dimentions
 * \param blockDim    - Block dimentions
 * \param args        - Arguments
 * \param sharedMem   - Shared memory
 * \param stream      - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidConfiguration,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorLaunchTimeout,
 * ::lwdaErrorLaunchOutOfResources,
 * ::lwdaErrorSharedObjectInitFailed,
 * ::lwdaErrorIlwalidPtx,
 * ::lwdaErrorUnsupportedPtxVersion,
 * ::lwdaErrorNoKernelImageForDevice,
 * ::lwdaErrorJitCompilerNotFound,
 * ::lwdaErrorJitCompilationDisabled
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * \ref ::lwdaLaunchKernel(const T *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream) "lwdaLaunchKernel (C++ API)",
 * ::lwLaunchKernel
 */
extern __host__ lwdaError_t LWDARTAPI lwdaLaunchKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream);


extern __host__ lwdaError_t LWDARTAPI lwdaLaunchKernelEx(const lwdaLaunchConfig_t *config, const void *func, void **args);


/**
 * \brief Launches a device function where thread blocks can cooperate and synchronize as they execute
 *
 * The function ilwokes kernel \p func on \p gridDim (\p gridDim.x &times; \p gridDim.y
 * &times; \p gridDim.z) grid of blocks. Each block contains \p blockDim (\p blockDim.x &times;
 * \p blockDim.y &times; \p blockDim.z) threads.
 *
 * The device on which this kernel is ilwoked must have a non-zero value for
 * the device attribute ::lwdaDevAttrCooperativeLaunch.
 *
 * The total number of blocks launched cannot exceed the maximum number of blocks per
 * multiprocessor as returned by ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessor (or
 * ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags) times the number of multiprocessors
 * as specified by the device attribute ::lwdaDevAttrMultiProcessorCount.
 *
 * The kernel cannot make use of LWCA dynamic parallelism.
 *
 * If the kernel has N parameters the \p args should point to array of N pointers.
 * Each pointer, from <tt>args[0]</tt> to <tt>args[N - 1]</tt>, point to the region
 * of memory from which the actual parameter will be copied.
 *
 * For templated functions, pass the function symbol as follows:
 * func_name<template_arg_0,...,template_arg_N>
 *
 * \p sharedMem sets the amount of dynamic shared memory that will be available to
 * each thread block.
 *
 * \p stream specifies a stream the invocation is associated to.
 *
 * \param func        - Device function symbol
 * \param gridDim     - Grid dimentions
 * \param blockDim    - Block dimentions
 * \param args        - Arguments
 * \param sharedMem   - Shared memory
 * \param stream      - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidConfiguration,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorLaunchTimeout,
 * ::lwdaErrorLaunchOutOfResources,
 * ::lwdaErrorCooperativeLaunchTooLarge,
 * ::lwdaErrorSharedObjectInitFailed
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * \ref ::lwdaLaunchCooperativeKernel(const T *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream) "lwdaLaunchCooperativeKernel (C++ API)",
 * ::lwdaLaunchCooperativeKernelMultiDevice,
 * ::lwLaunchCooperativeKernel
 */
extern __host__ lwdaError_t LWDARTAPI lwdaLaunchCooperativeKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream);

/**
 * \brief Launches device functions on multiple devices where thread blocks can cooperate and synchronize as they execute
 *
 * \deprecated This function is deprecated as of LWCA 11.3.
 *
 * Ilwokes kernels as specified in the \p launchParamsList array where each element
 * of the array specifies all the parameters required to perform a single kernel launch.
 * These kernels can cooperate and synchronize as they execute. The size of the array is
 * specified by \p numDevices.
 *
 * No two kernels can be launched on the same device. All the devices targeted by this
 * multi-device launch must be identical. All devices must have a non-zero value for the
 * device attribute ::lwdaDevAttrCooperativeMultiDeviceLaunch.
 *
 * The same kernel must be launched on all devices. Note that any __device__ or __constant__
 * variables are independently instantiated on every device. It is the application's
 * responsiblity to ensure these variables are initialized and used appropriately.
 *
 * The size of the grids as specified in blocks, the size of the blocks themselves and the
 * amount of shared memory used by each thread block must also match across all launched kernels.
 *
 * The streams used to launch these kernels must have been created via either ::lwdaStreamCreate
 * or ::lwdaStreamCreateWithPriority or ::lwdaStreamCreateWithPriority. The NULL stream or
 * ::lwdaStreamLegacy or ::lwdaStreamPerThread cannot be used.
 *
 * The total number of blocks launched per kernel cannot exceed the maximum number of blocks
 * per multiprocessor as returned by ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessor (or
 * ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags) times the number of multiprocessors
 * as specified by the device attribute ::lwdaDevAttrMultiProcessorCount. Since the
 * total number of blocks launched per device has to match across all devices, the maximum
 * number of blocks that can be launched per device will be limited by the device with the
 * least number of multiprocessors.
 *
 * The kernel cannot make use of LWCA dynamic parallelism.
 *
 * The ::lwdaLaunchParams structure is defined as:
 * \code
        struct lwdaLaunchParams
        {
            void *func;
            dim3 gridDim;
            dim3 blockDim;
            void **args;
            size_t sharedMem;
            lwdaStream_t stream;
        };
 * \endcode
 * where:
 * - ::lwdaLaunchParams::func specifies the kernel to be launched. This same functions must
 *   be launched on all devices. For templated functions, pass the function symbol as follows:
 *   func_name<template_arg_0,...,template_arg_N>
 * - ::lwdaLaunchParams::gridDim specifies the width, height and depth of the grid in blocks.
 *   This must match across all kernels launched.
 * - ::lwdaLaunchParams::blockDim is the width, height and depth of each thread block. This
 *   must match across all kernels launched.
 * - ::lwdaLaunchParams::args specifies the arguments to the kernel. If the kernel has
 *   N parameters then ::lwdaLaunchParams::args should point to array of N pointers. Each
 *   pointer, from <tt>::lwdaLaunchParams::args[0]</tt> to <tt>::lwdaLaunchParams::args[N - 1]</tt>,
 *   point to the region of memory from which the actual parameter will be copied.
 * - ::lwdaLaunchParams::sharedMem is the dynamic shared-memory size per thread block in bytes.
 *   This must match across all kernels launched.
 * - ::lwdaLaunchParams::stream is the handle to the stream to perform the launch in. This cannot
 *   be the NULL stream or ::lwdaStreamLegacy or ::lwdaStreamPerThread.
 *
 * By default, the kernel won't begin exelwtion on any GPU until all prior work in all the specified
 * streams has completed. This behavior can be overridden by specifying the flag
 * ::lwdaCooperativeLaunchMultiDeviceNoPreSync. When this flag is specified, each kernel
 * will only wait for prior work in the stream corresponding to that GPU to complete before it begins
 * exelwtion.
 *
 * Similarly, by default, any subsequent work pushed in any of the specified streams will not begin
 * exelwtion until the kernels on all GPUs have completed. This behavior can be overridden by specifying
 * the flag ::lwdaCooperativeLaunchMultiDeviceNoPostSync. When this flag is specified,
 * any subsequent work pushed in any of the specified streams will only wait for the kernel launched
 * on the GPU corresponding to that stream to complete before it begins exelwtion.
 *
 * \param launchParamsList - List of launch parameters, one per device
 * \param numDevices       - Size of the \p launchParamsList array
 * \param flags            - Flags to control launch behavior
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidConfiguration,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorLaunchTimeout,
 * ::lwdaErrorLaunchOutOfResources,
 * ::lwdaErrorCooperativeLaunchTooLarge,
 * ::lwdaErrorSharedObjectInitFailed
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * \ref ::lwdaLaunchCooperativeKernel(const T *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream) "lwdaLaunchCooperativeKernel (C++ API)",
 * ::lwdaLaunchCooperativeKernel,
 * ::lwLaunchCooperativeKernelMultiDevice
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaLaunchCooperativeKernelMultiDevice(struct lwdaLaunchParams *launchParamsList, unsigned int numDevices, unsigned int flags  __dv(0));

/**
 * \brief Sets the preferred cache configuration for a device function
 *
 * On devices where the L1 cache and shared memory use the same hardware
 * resources, this sets through \p cacheConfig the preferred cache configuration
 * for the function specified via \p func. This is only a preference. The
 * runtime will use the requested configuration if possible, but it is free to
 * choose a different configuration if required to execute \p func.
 *
 * \p func is a device function symbol and must be declared as a
 * \c __global__ function. If the specified function does not exist,
 * then ::lwdaErrorIlwalidDeviceFunction is returned. For templated functions,
 * pass the function symbol as follows: func_name<template_arg_0,...,template_arg_N>
 *
 * This setting does nothing on devices where the size of the L1 cache and
 * shared memory are fixed.
 *
 * Launching a kernel with a different preference than the most recent
 * preference setting may insert a device-side synchronization point.
 *
 * The supported cache configurations are:
 * - ::lwdaFuncCachePreferNone: no preference for shared memory or L1 (default)
 * - ::lwdaFuncCachePreferShared: prefer larger shared memory and smaller L1 cache
 * - ::lwdaFuncCachePreferL1: prefer larger L1 cache and smaller shared memory
 * - ::lwdaFuncCachePreferEqual: prefer equal size L1 cache and shared memory
 *
 * \param func        - Device function symbol
 * \param cacheConfig - Requested cache configuration
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction
 * \notefnerr
 * \note_string_api_deprecation2
 * \note_init_rt
 * \note_callback
 *
 * \sa 
 * \ref ::lwdaFuncSetCacheConfig(T*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C++ API)",
 * \ref ::lwdaFuncGetAttributes(struct lwdaFuncAttributes*, const void*) "lwdaFuncGetAttributes (C API)",
 * \ref ::lwdaLaunchKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream) "lwdaLaunchKernel (C API)",
 * ::lwdaThreadGetCacheConfig,
 * ::lwdaThreadSetCacheConfig,
 * ::lwFuncSetCacheConfig
 */
extern __host__ lwdaError_t LWDARTAPI lwdaFuncSetCacheConfig(const void *func, enum lwdaFuncCache cacheConfig);

/**
 * \brief Sets the shared memory configuration for a device function
 *
 * On devices with configurable shared memory banks, this function will 
 * force all subsequent launches of the specified device function to have
 * the given shared memory bank size configuration. On any given launch of the
 * function, the shared memory configuration of the device will be temporarily
 * changed if needed to suit the function's preferred configuration. Changes in
 * shared memory configuration between subsequent launches of functions, 
 * may introduce a device side synchronization point.
 *
 * Any per-function setting of shared memory bank size set via 
 * ::lwdaFuncSetSharedMemConfig will override the device wide setting set by
 * ::lwdaDeviceSetSharedMemConfig.
 *
 * Changing the shared memory bank size will not increase shared memory usage
 * or affect oclwpancy of kernels, but may have major effects on performance. 
 * Larger bank sizes will allow for greater potential bandwidth to shared memory,
 * but will change what kinds of accesses to shared memory will result in bank 
 * conflicts.
 *
 * This function will do nothing on devices with fixed shared memory bank size.
 *
 * For templated functions, pass the function symbol as follows:
 * func_name<template_arg_0,...,template_arg_N>
 *
 * The supported bank configurations are:
 * - ::lwdaSharedMemBankSizeDefault: use the device's shared memory configuration
 *   when launching this function.
 * - ::lwdaSharedMemBankSizeFourByte: set shared memory bank width to be 
 *   four bytes natively when launching this function.
 * - ::lwdaSharedMemBankSizeEightByte: set shared memory bank width to be eight 
 *   bytes natively when launching this function.
 *
 * \param func   - Device function symbol
 * \param config - Requested shared memory configuration
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_string_api_deprecation2
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceSetSharedMemConfig,
 * ::lwdaDeviceGetSharedMemConfig,
 * ::lwdaDeviceSetCacheConfig,
 * ::lwdaDeviceGetCacheConfig,
 * ::lwdaFuncSetCacheConfig,
 * ::lwFuncSetSharedMemConfig
 */
extern __host__ lwdaError_t LWDARTAPI lwdaFuncSetSharedMemConfig(const void *func, enum lwdaSharedMemConfig config);

/**
 * \brief Find out attributes for a given function
 *
 * This function obtains the attributes of a function specified via \p func.
 * \p func is a device function symbol and must be declared as a
 * \c __global__ function. The fetched attributes are placed in \p attr.
 * If the specified function does not exist, then
 * ::lwdaErrorIlwalidDeviceFunction is returned. For templated functions, pass
 * the function symbol as follows: func_name<template_arg_0,...,template_arg_N>
 *
 * Note that some function attributes such as
 * \ref ::lwdaFuncAttributes::maxThreadsPerBlock "maxThreadsPerBlock"
 * may vary based on the device that is lwrrently being used.
 *
 * \param attr - Return pointer to function's attributes
 * \param func - Device function symbol
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction
 * \notefnerr
 * \note_string_api_deprecation2
 * \note_init_rt
 * \note_callback
 *
 * \sa 
 * \ref ::lwdaFuncSetCacheConfig(const void*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C API)",
 * \ref ::lwdaFuncGetAttributes(struct lwdaFuncAttributes*, T*) "lwdaFuncGetAttributes (C++ API)",
 * \ref ::lwdaLaunchKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream) "lwdaLaunchKernel (C API)",
 * ::lwFuncGetAttribute
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaFuncGetAttributes(struct lwdaFuncAttributes *attr, const void *func);


/**
 * \brief Set attributes for a given function
 *
 * This function sets the attributes of a function specified via \p func.
 * The parameter \p func must be a pointer to a function that exelwtes
 * on the device. The parameter specified by \p func must be declared as a \p __global__
 * function. The enumeration defined by \p attr is set to the value defined by \p value.
 * If the specified function does not exist, then ::lwdaErrorIlwalidDeviceFunction is returned.
 * If the specified attribute cannot be written, or if the value is incorrect, 
 * then ::lwdaErrorIlwalidValue is returned.
 *
 * Valid values for \p attr are:
 * - ::lwdaFuncAttributeMaxDynamicSharedMemorySize - The requested maximum size in bytes of dynamically-allocated shared memory. The sum of this value and the function attribute ::sharedSizeBytes
 *   cannot exceed the device attribute ::lwdaDevAttrMaxSharedMemoryPerBlockOptin. The maximal size of requestable dynamic shared memory may differ by GPU architecture.
 * - ::lwdaFuncAttributePreferredSharedMemoryCarveout - On devices where the L1 cache and shared memory use the same hardware resources, 
 *   this sets the shared memory carveout preference, in percent of the total shared memory. See ::lwdaDevAttrMaxSharedMemoryPerMultiprocessor.
 *   This is only a hint, and the driver can choose a different ratio if required to execute the function.
 *
 * \param func  - Function to get attributes of
 * \param attr  - Attribute to set
 * \param value - Value to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \ref ::lwdaLaunchKernel(const T *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream) "lwdaLaunchKernel (C++ API)",
 * \ref ::lwdaFuncSetCacheConfig(T*, enum lwdaFuncCache) "lwdaFuncSetCacheConfig (C++ API)",
 * \ref ::lwdaFuncGetAttributes(struct lwdaFuncAttributes*, const void*) "lwdaFuncGetAttributes (C API)",
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaFuncSetAttribute(const void *func, enum lwdaFuncAttribute attr, int value);





















































/**
 * \brief Enqueues a host function call in a stream
 *
 * Enqueues a host function to run in a stream.  The function will be called
 * after lwrrently enqueued work and will block work added after it.
 *
 * The host function must not make any LWCA API calls.  Attempting to use a
 * LWCA API may result in ::lwdaErrorNotPermitted, but this is not required.
 * The host function must not perform any synchronization that may depend on
 * outstanding LWCA work not mandated to run earlier.  Host functions without a
 * mandated order (such as in independent streams) execute in undefined order
 * and may be serialized.
 *
 * For the purposes of Unified Memory, exelwtion makes a number of guarantees:
 * <ul>
 *   <li>The stream is considered idle for the duration of the function's
 *   exelwtion.  Thus, for example, the function may always use memory attached
 *   to the stream it was enqueued in.</li>
 *   <li>The start of exelwtion of the function has the same effect as
 *   synchronizing an event recorded in the same stream immediately prior to
 *   the function.  It thus synchronizes streams which have been "joined"
 *   prior to the function.</li>
 *   <li>Adding device work to any stream does not have the effect of making
 *   the stream active until all preceding host functions and stream callbacks
 *   have exelwted.  Thus, for
 *   example, a function might use global attached memory even if work has
 *   been added to another stream, if the work has been ordered behind the
 *   function call with an event.</li>
 *   <li>Completion of the function does not cause a stream to become
 *   active except as described above.  The stream will remain idle
 *   if no device work follows the function, and will remain idle across
 *   conselwtive host functions or stream callbacks without device work in
 *   between.  Thus, for example,
 *   stream synchronization can be done by signaling from a host function at the
 *   end of the stream.</li>
 * </ul>
 *
 * Note that, in constrast to ::lwStreamAddCallback, the function will not be
 * called in the event of an error in the LWCA context.
 *
 * \param hStream  - Stream to enqueue function call in
 * \param fn       - The function to call once preceding stream operations are complete
 * \param userData - User-specified data to be passed to the function
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaStreamCreate,
 * ::lwdaStreamQuery,
 * ::lwdaStreamSynchronize,
 * ::lwdaStreamWaitEvent,
 * ::lwdaStreamDestroy,
 * ::lwdaMallocManaged,
 * ::lwdaStreamAttachMemAsync,
 * ::lwdaStreamAddCallback,
 * ::lwLaunchHostFunc
 */
extern __host__ lwdaError_t LWDARTAPI lwdaLaunchHostFunc(lwdaStream_t stream, lwdaHostFn_t fn, void *userData);

/** @} */ /* END LWDART_EXELWTION */

/**
 * \defgroup LWDART_OCLWPANCY Oclwpancy
 *
 * ___MANBRIEF___ oclwpancy callwlation functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the oclwpancy callwlation functions of the LWCA runtime
 * application programming interface.
 *
 * Besides the oclwpancy calculator functions
 * (\ref ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessor and \ref ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags),
 * there are also C++ only oclwpancy-based launch configuration functions dolwmented in
 * \ref LWDART_HIGHLEVEL "C++ API Routines" module.
 *
 * See
 * \ref ::lwdaOclwpancyMaxPotentialBlockSize(int*, int*, T, size_t, int) "lwdaOclwpancyMaxPotentialBlockSize (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeWithFlags(int*, int*, T, size_t, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSize (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMem(int*, int*, T, UnaryFunction, int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMem (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags(int*, int*, T, UnaryFunction, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMem (C++ API)"
 * \ref ::lwdaOclwpancyAvailableDynamicSMemPerBlock(size_t*, T, int, int) "lwdaOclwpancyAvailableDynamicSMemPerBlock (C++ API)",
 *
 * @{
 */

/**
 * \brief Returns oclwpancy for a device function
 *
 * Returns in \p *numBlocks the maximum number of active blocks per
 * streaming multiprocessor for the device function.
 *
 * \param numBlocks       - Returned oclwpancy
 * \param func            - Kernel function for which oclwpancy is callwlated
 * \param blockSize       - Block size the kernel is intended to be launched with
 * \param dynamicSMemSize - Per-block dynamic shared memory usage intended, in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown,
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags,
 * \ref ::lwdaOclwpancyMaxPotentialBlockSize(int*, int*, T, size_t, int) "lwdaOclwpancyMaxPotentialBlockSize (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeWithFlags(int*, int*, T, size_t, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeWithFlags (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMem(int*, int*, T, UnaryFunction, int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMem (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags(int*, int*, T, UnaryFunction, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags (C++ API)",
 * \ref ::lwdaOclwpancyAvailableDynamicSMemPerBlock(size_t*, T, int, int) "lwdaOclwpancyAvailableDynamicSMemPerBlock (C++ API)",
 * ::lwOclwpancyMaxActiveBlocksPerMultiprocessor
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, const void *func, int blockSize, size_t dynamicSMemSize);

/**
 * \brief Returns dynamic shared memory available per block when launching \p numBlocks blocks on SM.
 *
 * Returns in \p *dynamicSmemSize the maximum size of dynamic shared memory to allow \p numBlocks blocks per SM. 
 *
 * \param dynamicSmemSize - Returned maximum dynamic shared memory 
 * \param func            - Kernel function for which oclwpancy is callwlated
 * \param numBlocks       - Number of blocks to fit on SM 
 * \param blockSize       - Size of the block
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown,
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags,
 * \ref ::lwdaOclwpancyMaxPotentialBlockSize(int*, int*, T, size_t, int) "lwdaOclwpancyMaxPotentialBlockSize (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeWithFlags(int*, int*, T, size_t, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeWithFlags (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMem(int*, int*, T, UnaryFunction, int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMem (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags(int*, int*, T, UnaryFunction, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags (C++ API)",
 * ::lwdaOclwpancyAvailableDynamicSMemPerBlock
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaOclwpancyAvailableDynamicSMemPerBlock(size_t *dynamicSmemSize, const void *func, int numBlocks, int blockSize);

/**
 * \brief Returns oclwpancy for a device function with the specified flags
 *
 * Returns in \p *numBlocks the maximum number of active blocks per
 * streaming multiprocessor for the device function.
 *
 * The \p flags parameter controls how special cases are handled. Valid flags include:
 *
 * - ::lwdaOclwpancyDefault: keeps the default behavior as
 *   ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessor
 *
 * - ::lwdaOclwpancyDisableCachingOverride: This flag suppresses the default behavior
 *   on platform where global caching affects oclwpancy. On such platforms, if caching
 *   is enabled, but per-block SM resource usage would result in zero oclwpancy, the
 *   oclwpancy calculator will callwlate the oclwpancy as if caching is disabled.
 *   Setting this flag makes the oclwpancy calculator to return 0 in such cases.
 *   More information can be found about this feature in the "Unified L1/Texture Cache"
 *   section of the Maxwell tuning guide.
 *
 * \param numBlocks       - Returned oclwpancy
 * \param func            - Kernel function for which oclwpancy is callwlated
 * \param blockSize       - Block size the kernel is intended to be launched with
 * \param dynamicSMemSize - Per-block dynamic shared memory usage intended, in bytes
 * \param flags           - Requested behavior for the oclwpancy calculator
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidDeviceFunction,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown,
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaOclwpancyMaxActiveBlocksPerMultiprocessor,
 * \ref ::lwdaOclwpancyMaxPotentialBlockSize(int*, int*, T, size_t, int) "lwdaOclwpancyMaxPotentialBlockSize (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeWithFlags(int*, int*, T, size_t, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeWithFlags (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMem(int*, int*, T, UnaryFunction, int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMem (C++ API)",
 * \ref ::lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags(int*, int*, T, UnaryFunction, int, unsigned int) "lwdaOclwpancyMaxPotentialBlockSizeVariableSMemWithFlags (C++ API)",
 * \ref ::lwdaOclwpancyAvailableDynamicSMemPerBlock(size_t*, T, int, int) "lwdaOclwpancyAvailableDynamicSMemPerBlock (C++ API)",
 * ::lwOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags(int *numBlocks, const void *func, int blockSize, size_t dynamicSMemSize, unsigned int flags);

/** @} */ /* END LWDA_OCLWPANCY */

/**
 * \defgroup LWDART_MEMORY Memory Management
 *
 * ___MANBRIEF___ memory management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the memory management functions of the LWCA runtime
 * application programming interface.
 *
 * Some functions have overloaded C++ API template versions dolwmented separately in the
 * \ref LWDART_HIGHLEVEL "C++ API Routines" module.
 *
 * @{
 */

/**
 * \brief Allocates memory that will be automatically managed by the Unified Memory system
 *
 * Allocates \p size bytes of managed memory on the device and returns in
 * \p *devPtr a pointer to the allocated memory. If the device doesn't support
 * allocating managed memory, ::lwdaErrorNotSupported is returned. Support
 * for managed memory can be queried using the device attribute
 * ::lwdaDevAttrManagedMemory. The allocated memory is suitably
 * aligned for any kind of variable. The memory is not cleared. If \p size
 * is 0, ::lwdaMallocManaged returns ::lwdaErrorIlwalidValue. The pointer
 * is valid on the CPU and on all GPUs in the system that support managed memory.
 * All accesses to this pointer must obey the Unified Memory programming model.
 *
 * \p flags specifies the default stream association for this allocation.
 * \p flags must be one of ::lwdaMemAttachGlobal or ::lwdaMemAttachHost. The
 * default value for \p flags is ::lwdaMemAttachGlobal.
 * If ::lwdaMemAttachGlobal is specified, then this memory is accessible from
 * any stream on any device. If ::lwdaMemAttachHost is specified, then the
 * allocation should not be accessed from devices that have a zero value for the
 * device attribute ::lwdaDevAttrConlwrrentManagedAccess; an explicit call to
 * ::lwdaStreamAttachMemAsync will be required to enable access on such devices.
 *
 * If the association is later changed via ::lwdaStreamAttachMemAsync to
 * a single stream, the default association, as specifed during ::lwdaMallocManaged,
 * is restored when that stream is destroyed. For __managed__ variables, the
 * default association is always ::lwdaMemAttachGlobal. Note that destroying a
 * stream is an asynchronous operation, and as a result, the change to default
 * association won't happen until all work in the stream has completed.
 *
 * Memory allocated with ::lwdaMallocManaged should be released with ::lwdaFree.
 *
 * Device memory oversubscription is possible for GPUs that have a non-zero value for the
 * device attribute ::lwdaDevAttrConlwrrentManagedAccess. Managed memory on
 * such GPUs may be evicted from device memory to host memory at any time by the Unified
 * Memory driver in order to make room for other allocations.
 *
 * In a multi-GPU system where all GPUs have a non-zero value for the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess, managed memory may not be populated when this
 * API returns and instead may be populated on access. In such systems, managed memory can
 * migrate to any processor's memory at any time. The Unified Memory driver will employ heuristics to
 * maintain data locality and prevent excessive page faults to the extent possible. The application
 * can also guide the driver about memory usage patterns via ::lwdaMemAdvise. The application
 * can also explicitly migrate memory to a desired processor's memory via
 * ::lwdaMemPrefetchAsync.
 *
 * In a multi-GPU system where all of the GPUs have a zero value for the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess and all the GPUs have peer-to-peer support
 * with each other, the physical storage for managed memory is created on the GPU which is active
 * at the time ::lwdaMallocManaged is called. All other GPUs will reference the data at reduced
 * bandwidth via peer mappings over the PCIe bus. The Unified Memory driver does not migrate
 * memory among such GPUs.
 *
 * In a multi-GPU system where not all GPUs have peer-to-peer support with each other and
 * where the value of the device attribute ::lwdaDevAttrConlwrrentManagedAccess
 * is zero for at least one of those GPUs, the location chosen for physical storage of managed
 * memory is system-dependent.
 * - On Linux, the location chosen will be device memory as long as the current set of active
 * contexts are on devices that either have peer-to-peer support with each other or have a
 * non-zero value for the device attribute ::lwdaDevAttrConlwrrentManagedAccess.
 * If there is an active context on a GPU that does not have a non-zero value for that device
 * attribute and it does not have peer-to-peer support with the other devices that have active
 * contexts on them, then the location for physical storage will be 'zero-copy' or host memory.
 * Note that this means that managed memory that is located in device memory is migrated to
 * host memory if a new context is created on a GPU that doesn't have a non-zero value for
 * the device attribute and does not support peer-to-peer with at least one of the other devices
 * that has an active context. This in turn implies that context creation may fail if there is
 * insufficient host memory to migrate all managed allocations.
 * - On Windows, the physical storage is always created in 'zero-copy' or host memory.
 * All GPUs will reference the data at reduced bandwidth over the PCIe bus. In these
 * cirlwmstances, use of the environment variable LWDA_VISIBLE_DEVICES is recommended to
 * restrict LWCA to only use those GPUs that have peer-to-peer support.
 * Alternatively, users can also set LWDA_MANAGED_FORCE_DEVICE_ALLOC to a non-zero
 * value to force the driver to always use device memory for physical storage.
 * When this environment variable is set to a non-zero value, all devices used in
 * that process that support managed memory have to be peer-to-peer compatible
 * with each other. The error ::lwdaErrorIlwalidDevice will be returned if a device
 * that supports managed memory is used and it is not peer-to-peer compatible with
 * any of the other managed memory supporting devices that were previously used in
 * that process, even if ::lwdaDeviceReset has been called on those devices. These
 * environment variables are described in the LWCA programming guide under the
 * "LWCA environment variables" section.
 *
 * \param devPtr - Pointer to allocated device memory
 * \param size   - Requested allocation size in bytes
 * \param flags  - Must be either ::lwdaMemAttachGlobal or ::lwdaMemAttachHost (defaults to ::lwdaMemAttachGlobal)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorMemoryAllocation,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMallocPitch, ::lwdaFree, ::lwdaMallocArray, ::lwdaFreeArray,
 * ::lwdaMalloc3D, ::lwdaMalloc3DArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc, ::lwdaDeviceGetAttribute, ::lwdaStreamAttachMemAsync,
 * ::lwMemAllocManaged
 */
#if defined(__cplusplus)
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMallocManaged(void **devPtr, size_t size, unsigned int flags = lwdaMemAttachGlobal);
#else
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMallocManaged(void **devPtr, size_t size, unsigned int flags);
#endif

/**
 * \brief Allocate memory on the device
 *
 * Allocates \p size bytes of linear memory on the device and returns in
 * \p *devPtr a pointer to the allocated memory. The allocated memory is
 * suitably aligned for any kind of variable. The memory is not cleared.
 * ::lwdaMalloc() returns ::lwdaErrorMemoryAllocation in case of failure.
 *
 * The device version of ::lwdaFree cannot be used with a \p *devPtr
 * allocated using the host API, and vice versa.
 *
 * \param devPtr - Pointer to allocated device memory
 * \param size   - Requested allocation size in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMallocPitch, ::lwdaFree, ::lwdaMallocArray, ::lwdaFreeArray,
 * ::lwdaMalloc3D, ::lwdaMalloc3DArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::lwMemAlloc
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMalloc(void **devPtr, size_t size);

/**
 * \brief Allocates page-locked memory on the host
 *
 * Allocates \p size bytes of host memory that is page-locked and accessible
 * to the device. The driver tracks the virtual memory ranges allocated with
 * this function and automatically accelerates calls to functions such as
 * ::lwdaMemcpy*(). Since the memory can be accessed directly by the device,
 * it can be read or written with much higher bandwidth than pageable memory
 * obtained with functions such as ::malloc(). Allocating excessive amounts of
 * memory with ::lwdaMallocHost() may degrade system performance, since it
 * reduces the amount of memory available to the system for paging. As a
 * result, this function is best used sparingly to allocate staging areas for
 * data exchange between host and device.
 *
 * \param ptr  - Pointer to allocated host memory
 * \param size - Requested allocation size in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaMallocArray, ::lwdaMalloc3D,
 * ::lwdaMalloc3DArray, ::lwdaHostAlloc, ::lwdaFree, ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t, unsigned int) "lwdaMallocHost (C++ API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::lwMemAllocHost
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMallocHost(void **ptr, size_t size);

/**
 * \brief Allocates pitched memory on the device
 *
 * Allocates at least \p width (in bytes) * \p height bytes of linear memory
 * on the device and returns in \p *devPtr a pointer to the allocated memory.
 * The function may pad the allocation to ensure that corresponding pointers
 * in any given row will continue to meet the alignment requirements for
 * coalescing as the address is updated from row to row. The pitch returned in
 * \p *pitch by ::lwdaMallocPitch() is the width in bytes of the allocation.
 * The intended usage of \p pitch is as a separate parameter of the allocation,
 * used to compute addresses within the 2D array. Given the row and column of
 * an array element of type \p T, the address is computed as:
 * \code
    T* pElement = (T*)((char*)BaseAddress + Row * pitch) + Column;
   \endcode
 *
 * For allocations of 2D arrays, it is recommended that programmers consider
 * performing pitch allocations using ::lwdaMallocPitch(). Due to pitch
 * alignment restrictions in the hardware, this is especially true if the
 * application will be performing 2D memory copies between different regions
 * of device memory (whether linear memory or LWCA arrays).
 *
 * \param devPtr - Pointer to allocated pitched device memory
 * \param pitch  - Pitch for allocation
 * \param width  - Requested pitched allocation width (in bytes)
 * \param height - Requested pitched allocation height
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaFree, ::lwdaMallocArray, ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaMalloc3D, ::lwdaMalloc3DArray,
 * ::lwdaHostAlloc,
 * ::lwMemAllocPitch
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMallocPitch(void **devPtr, size_t *pitch, size_t width, size_t height);

/**
 * \brief Allocate an array on the device
 *
 * Allocates a LWCA array according to the ::lwdaChannelFormatDesc structure
 * \p desc and returns a handle to the new LWCA array in \p *array.
 *
 * The ::lwdaChannelFormatDesc is defined as:
 * \code
    struct lwdaChannelFormatDesc {
        int x, y, z, w;
    enum lwdaChannelFormatKind f;
    };
    \endcode
 * where ::lwdaChannelFormatKind is one of ::lwdaChannelFormatKindSigned,
 * ::lwdaChannelFormatKindUnsigned, or ::lwdaChannelFormatKindFloat.
 *
 * The \p flags parameter enables different options to be specified that affect
 * the allocation, as follows.
 * - ::lwdaArrayDefault: This flag's value is defined to be 0 and provides default array allocation
 * - ::lwdaArraySurfaceLoadStore: Allocates an array that can be read from or written to using a surface reference
 * - ::lwdaArrayTextureGather: This flag indicates that texture gather operations will be performed on the array.
 * - ::lwdaArraySparse: Allocates a LWCA array without physical backing memory. The subregions within this sparse array
 *   can later be mapped onto a physical memory allocation by calling ::lwMemMapArrayAsync. 
 *   The physical backing memory must be allocated via ::lwMemCreate.

 * - ::lwdaArrayDeferredMapping: Allocates a LWCA array without physical backing memory. The entire array can 
 *   later be mapped onto a physical memory allocation by calling ::lwMemMapArrayAsync. 
 *   The physical backing memory must be allocated via ::lwMemCreate.

 *
 * \p width and \p height must meet certain size requirements. See ::lwdaMalloc3DArray() for more details.
 *
 * \param array  - Pointer to allocated array in device memory
 * \param desc   - Requested channel format
 * \param width  - Requested array allocation width
 * \param height - Requested array allocation height
 * \param flags  - Requested properties of allocated array
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree, ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaMalloc3D, ::lwdaMalloc3DArray,
 * ::lwdaHostAlloc,
 * ::lwArrayCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMallocArray(lwdaArray_t *array, const struct lwdaChannelFormatDesc *desc, size_t width, size_t height __dv(0), unsigned int flags __dv(0));

/**
 * \brief Frees memory on the device
 *
 * Frees the memory space pointed to by \p devPtr, which must have been
 * returned by a previous call to one of the following memory allocation APIs -
 * ::lwdaMalloc(), ::lwdaMallocPitch(), ::lwdaMallocManaged(), ::lwdaMallocAsync(),
 * ::lwdaMallocFromPoolAsync().
 * 
 * Note - This API will not perform any implicit synchronization when the pointer was
 * allocated with ::lwdaMallocAsync or ::lwdaMallocFromPoolAsync. Callers must ensure
 * that all accesses to the pointer have completed before ilwoking ::lwdaFree. For
 * best performance and memory reuse, users should use ::lwdaFreeAsync to free memory
 * allocated via the stream ordered memory allocator.
 * 
 * If ::lwdaFree(\p devPtr) has already been called before,
 * an error is returned. If \p devPtr is 0, no operation is performed.
 * ::lwdaFree() returns ::lwdaErrorValue in case of failure.
 *
 * The device version of ::lwdaFree cannot be used with a \p *devPtr
 * allocated using the host API, and vice versa.
 *
 * \param devPtr - Device pointer to memory to free
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaMallocManaged, ::lwdaMallocArray, ::lwdaFreeArray, ::lwdaMallocAsync, ::lwdaMallocFromPoolAsync
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaMalloc3D, ::lwdaMalloc3DArray, ::lwdaFreeAsync
 * ::lwdaHostAlloc,
 * ::lwMemFree
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaFree(void *devPtr);

/**
 * \brief Frees page-locked memory
 *
 * Frees the memory space pointed to by \p hostPtr, which must have been
 * returned by a previous call to ::lwdaMallocHost() or ::lwdaHostAlloc().
 *
 * \param ptr - Pointer to memory to free
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree, ::lwdaMallocArray,
 * ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaMalloc3D, ::lwdaMalloc3DArray, ::lwdaHostAlloc,
 * ::lwMemFreeHost
 */
extern __host__ lwdaError_t LWDARTAPI lwdaFreeHost(void *ptr);

/**
 * \brief Frees an array on the device
 *
 * Frees the LWCA array \p array, which must have been returned by a
 * previous call to ::lwdaMallocArray(). If \p devPtr is 0,
 * no operation is performed.
 *
 * \param array - Pointer to array to free
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree, ::lwdaMallocArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::lwArrayDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaFreeArray(lwdaArray_t array);

/**
 * \brief Frees a mipmapped array on the device
 *
 * Frees the LWCA mipmapped array \p mipmappedArray, which must have been 
 * returned by a previous call to ::lwdaMallocMipmappedArray(). If \p devPtr
 * is 0, no operation is performed.
 *
 * \param mipmappedArray - Pointer to mipmapped array to free
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree, ::lwdaMallocArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::lwMipmappedArrayDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaFreeMipmappedArray(lwdaMipmappedArray_t mipmappedArray);


/**
 * \brief Allocates page-locked memory on the host
 *
 * Allocates \p size bytes of host memory that is page-locked and accessible
 * to the device. The driver tracks the virtual memory ranges allocated with
 * this function and automatically accelerates calls to functions such as
 * ::lwdaMemcpy(). Since the memory can be accessed directly by the device, it
 * can be read or written with much higher bandwidth than pageable memory
 * obtained with functions such as ::malloc(). Allocating excessive amounts of
 * pinned memory may degrade system performance, since it reduces the amount
 * of memory available to the system for paging. As a result, this function is
 * best used sparingly to allocate staging areas for data exchange between host
 * and device.
 *
 * The \p flags parameter enables different options to be specified that affect
 * the allocation, as follows.
 * - ::lwdaHostAllocDefault: This flag's value is defined to be 0 and causes
 * ::lwdaHostAlloc() to emulate ::lwdaMallocHost().
 * - ::lwdaHostAllocPortable: The memory returned by this call will be
 * considered as pinned memory by all LWCA contexts, not just the one that
 * performed the allocation.
 * - ::lwdaHostAllocMapped: Maps the allocation into the LWCA address space.
 * The device pointer to the memory may be obtained by calling
 * ::lwdaHostGetDevicePointer().
 * - ::lwdaHostAllocWriteCombined: Allocates the memory as write-combined (WC).
 * WC memory can be transferred across the PCI Express bus more quickly on some
 * system configurations, but cannot be read efficiently by most CPUs.  WC
 * memory is a good option for buffers that will be written by the CPU and read
 * by the device via mapped pinned memory or host->device transfers.
 *
 * All of these flags are orthogonal to one another: a developer may allocate
 * memory that is portable, mapped and/or write-combined with no restrictions.
 *
 * In order for the ::lwdaHostAllocMapped flag to have any effect, the LWCA context
 * must support the ::lwdaDeviceMapHost flag, which can be checked via
 * ::lwdaGetDeviceFlags(). The ::lwdaDeviceMapHost flag is implicitly set for
 * contexts created via the runtime API.
 *
 * The ::lwdaHostAllocMapped flag may be specified on LWCA contexts for devices
 * that do not support mapped pinned memory. The failure is deferred to
 * ::lwdaHostGetDevicePointer() because the memory may be mapped into other
 * LWCA contexts via the ::lwdaHostAllocPortable flag.
 *
 * Memory allocated by this function must be freed with ::lwdaFreeHost().
 *
 * \param pHost - Device pointer to allocated memory
 * \param size  - Requested allocation size in bytes
 * \param flags - Requested properties of allocated memory
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaSetDeviceFlags,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost,
 * ::lwdaGetDeviceFlags,
 * ::lwMemHostAlloc
 */
extern __host__ lwdaError_t LWDARTAPI lwdaHostAlloc(void **pHost, size_t size, unsigned int flags);

/**
 * \brief Registers an existing host memory range for use by LWCA
 *
 * Page-locks the memory range specified by \p ptr and \p size and maps it
 * for the device(s) as specified by \p flags. This memory range also is added
 * to the same tracking mechanism as ::lwdaHostAlloc() to automatically accelerate
 * calls to functions such as ::lwdaMemcpy(). Since the memory can be accessed 
 * directly by the device, it can be read or written with much higher bandwidth 
 * than pageable memory that has not been registered.  Page-locking excessive
 * amounts of memory may degrade system performance, since it reduces the amount
 * of memory available to the system for paging. As a result, this function is
 * best used sparingly to register staging areas for data exchange between
 * host and device.
 *
 * ::lwdaHostRegister is supported only on I/O coherent devices that have a non-zero
 * value for the device attribute ::lwdaDevAttrHostRegisterSupported.
 *
 * The \p flags parameter enables different options to be specified that
 * affect the allocation, as follows.
 *
 * - ::lwdaHostRegisterDefault: On a system with unified virtual addressing,
 *   the memory will be both mapped and portable.  On a system with no unified
 *   virtual addressing, the memory will be neither mapped nor portable.
 *
 * - ::lwdaHostRegisterPortable: The memory returned by this call will be
 *   considered as pinned memory by all LWCA contexts, not just the one that
 *   performed the allocation.
 *
 * - ::lwdaHostRegisterMapped: Maps the allocation into the LWCA address
 *   space. The device pointer to the memory may be obtained by calling
 *   ::lwdaHostGetDevicePointer().
 *
 * - ::lwdaHostRegisterIoMemory: The passed memory pointer is treated as
 *   pointing to some memory-mapped I/O space, e.g. belonging to a
 *   third-party PCIe device, and it will marked as non cache-coherent and
 *   contiguous.
 *
 * - ::lwdaHostRegisterReadOnly: The passed memory pointer is treated as
 *   pointing to memory that is considered read-only by the device.  On
 *   platforms without ::lwdaDevAttrPageableMemoryAccessUsesHostPageTables, this
 *   flag is required in order to register memory mapped to the CPU as
 *   read-only.  Support for the use of this flag can be queried from the device
 *   attribute lwdaDeviceAttrReadOnlyHostRegisterSupported.  Using this flag with
 *   a current context associated with a device that does not have this attribute
 *   set will cause ::lwdaHostRegister to error with lwdaErrorNotSupported.
 *
 * All of these flags are orthogonal to one another: a developer may page-lock
 * memory that is portable or mapped with no restrictions.
 *
 * The LWCA context must have been created with the ::lwdaMapHost flag in
 * order for the ::lwdaHostRegisterMapped flag to have any effect.
 *
 * The ::lwdaHostRegisterMapped flag may be specified on LWCA contexts for
 * devices that do not support mapped pinned memory. The failure is deferred
 * to ::lwdaHostGetDevicePointer() because the memory may be mapped into
 * other LWCA contexts via the ::lwdaHostRegisterPortable flag.
 *
 * For devices that have a non-zero value for the device attribute
 * ::lwdaDevAttrCanUseHostPointerForRegisteredMem, the memory
 * can also be accessed from the device using the host pointer \p ptr.
 * The device pointer returned by ::lwdaHostGetDevicePointer() may or may not
 * match the original host pointer \p ptr and depends on the devices visible to the
 * application. If all devices visible to the application have a non-zero value for the
 * device attribute, the device pointer returned by ::lwdaHostGetDevicePointer()
 * will match the original pointer \p ptr. If any device visible to the application
 * has a zero value for the device attribute, the device pointer returned by
 * ::lwdaHostGetDevicePointer() will not match the original host pointer \p ptr,
 * but it will be suitable for use on all devices provided Unified Virtual Addressing
 * is enabled. In such systems, it is valid to access the memory using either pointer
 * on devices that have a non-zero value for the device attribute. Note however that
 * such devices should access the memory using only of the two pointers and not both.
 *
 * The memory page-locked by this function must be unregistered with ::lwdaHostUnregister().
 *
 * \param ptr   - Host pointer to memory to page-lock
 * \param size  - Size in bytes of the address range to page-lock in bytes
 * \param flags - Flags for allocation request
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation,
 * ::lwdaErrorHostMemoryAlreadyRegistered,
 * ::lwdaErrorNotSupported
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaHostUnregister, ::lwdaHostGetFlags, ::lwdaHostGetDevicePointer,
 * ::lwMemHostRegister
 */
extern __host__ lwdaError_t LWDARTAPI lwdaHostRegister(void *ptr, size_t size, unsigned int flags);

/**
 * \brief Unregisters a memory range that was registered with lwdaHostRegister
 *
 * Unmaps the memory range whose base address is specified by \p ptr, and makes
 * it pageable again.
 *
 * The base address must be the same one specified to ::lwdaHostRegister().
 *
 * \param ptr - Host pointer to memory to unregister
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorHostMemoryNotRegistered
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaHostUnregister,
 * ::lwMemHostUnregister
 */
extern __host__ lwdaError_t LWDARTAPI lwdaHostUnregister(void *ptr);

/**
 * \brief Passes back device pointer of mapped host memory allocated by
 * lwdaHostAlloc or registered by lwdaHostRegister
 *
 * Passes back the device pointer corresponding to the mapped, pinned host
 * buffer allocated by ::lwdaHostAlloc() or registered by ::lwdaHostRegister().
 *
 * ::lwdaHostGetDevicePointer() will fail if the ::lwdaDeviceMapHost flag was
 * not specified before deferred context creation oclwrred, or if called on a
 * device that does not support mapped, pinned memory.
 *
 * For devices that have a non-zero value for the device attribute
 * ::lwdaDevAttrCanUseHostPointerForRegisteredMem, the memory
 * can also be accessed from the device using the host pointer \p pHost.
 * The device pointer returned by ::lwdaHostGetDevicePointer() may or may not
 * match the original host pointer \p pHost and depends on the devices visible to the
 * application. If all devices visible to the application have a non-zero value for the
 * device attribute, the device pointer returned by ::lwdaHostGetDevicePointer()
 * will match the original pointer \p pHost. If any device visible to the application
 * has a zero value for the device attribute, the device pointer returned by
 * ::lwdaHostGetDevicePointer() will not match the original host pointer \p pHost,
 * but it will be suitable for use on all devices provided Unified Virtual Addressing
 * is enabled. In such systems, it is valid to access the memory using either pointer
 * on devices that have a non-zero value for the device attribute. Note however that
 * such devices should access the memory using only of the two pointers and not both.
 *
 * \p flags provides for future releases.  For now, it must be set to 0.
 *
 * \param pDevice - Returned device pointer for mapped memory
 * \param pHost   - Requested host pointer mapping
 * \param flags   - Flags for extensions (must be 0 for now)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaSetDeviceFlags, ::lwdaHostAlloc,
 * ::lwMemHostGetDevicePointer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaHostGetDevicePointer(void **pDevice, void *pHost, unsigned int flags);

/**
 * \brief Passes back flags used to allocate pinned host memory allocated by
 * lwdaHostAlloc
 *
 * ::lwdaHostGetFlags() will fail if the input pointer does not
 * reside in an address range allocated by ::lwdaHostAlloc().
 *
 * \param pFlags - Returned flags word
 * \param pHost - Host pointer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaHostAlloc,
 * ::lwMemHostGetFlags
 */
extern __host__ lwdaError_t LWDARTAPI lwdaHostGetFlags(unsigned int *pFlags, void *pHost);

/**
 * \brief Allocates logical 1D, 2D, or 3D memory objects on the device
 *
 * Allocates at least \p width * \p height * \p depth bytes of linear memory
 * on the device and returns a ::lwdaPitchedPtr in which \p ptr is a pointer
 * to the allocated memory. The function may pad the allocation to ensure
 * hardware alignment requirements are met. The pitch returned in the \p pitch
 * field of \p pitchedDevPtr is the width in bytes of the allocation.
 *
 * The returned ::lwdaPitchedPtr contains additional fields \p xsize and
 * \p ysize, the logical width and height of the allocation, which are
 * equivalent to the \p width and \p height \p extent parameters provided by
 * the programmer during allocation.
 *
 * For allocations of 2D and 3D objects, it is highly recommended that
 * programmers perform allocations using ::lwdaMalloc3D() or
 * ::lwdaMallocPitch(). Due to alignment restrictions in the hardware, this is
 * especially true if the application will be performing memory copies
 * ilwolving 2D or 3D objects (whether linear memory or LWCA arrays).
 *
 * \param pitchedDevPtr  - Pointer to allocated pitched device memory
 * \param extent         - Requested allocation size (\p width field in bytes)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMallocPitch, ::lwdaFree, ::lwdaMemcpy3D, ::lwdaMemset3D,
 * ::lwdaMalloc3DArray, ::lwdaMallocArray, ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc, ::make_lwdaPitchedPtr, ::make_lwdaExtent,
 * ::lwMemAllocPitch
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMalloc3D(struct lwdaPitchedPtr* pitchedDevPtr, struct lwdaExtent extent);

/**
 * \brief Allocate an array on the device
 *
 * Allocates a LWCA array according to the ::lwdaChannelFormatDesc structure
 * \p desc and returns a handle to the new LWCA array in \p *array.
 *
 * The ::lwdaChannelFormatDesc is defined as:
 * \code
    struct lwdaChannelFormatDesc {
        int x, y, z, w;
        enum lwdaChannelFormatKind f;
    };
    \endcode
 * where ::lwdaChannelFormatKind is one of ::lwdaChannelFormatKindSigned,
 * ::lwdaChannelFormatKindUnsigned, or ::lwdaChannelFormatKindFloat.
 *
 * ::lwdaMalloc3DArray() can allocate the following:
 *
 * - A 1D array is allocated if the height and depth extents are both zero.
 * - A 2D array is allocated if only the depth extent is zero.
 * - A 3D array is allocated if all three extents are non-zero.
 * - A 1D layered LWCA array is allocated if only the height extent is zero and
 * the lwdaArrayLayered flag is set. Each layer is a 1D array. The number of layers is 
 * determined by the depth extent.
 * - A 2D layered LWCA array is allocated if all three extents are non-zero and 
 * the lwdaArrayLayered flag is set. Each layer is a 2D array. The number of layers is 
 * determined by the depth extent.
 * - A lwbemap LWCA array is allocated if all three extents are non-zero and the
 * lwdaArrayLwbemap flag is set. Width must be equal to height, and depth must be six. A lwbemap is
 * a special type of 2D layered LWCA array, where the six layers represent the six faces of a lwbe. 
 * The order of the six layers in memory is the same as that listed in ::lwdaGraphicsLwbeFace.
 * - A lwbemap layered LWCA array is allocated if all three extents are non-zero, and both,
 * lwdaArrayLwbemap and lwdaArrayLayered flags are set. Width must be equal to height, and depth must be 
 * a multiple of six. A lwbemap layered LWCA array is a special type of 2D layered LWCA array that consists 
 * of a collection of lwbemaps. The first six layers represent the first lwbemap, the next six layers form 
 * the second lwbemap, and so on.
 *
 *
 * The \p flags parameter enables different options to be specified that affect
 * the allocation, as follows.
 * - ::lwdaArrayDefault: This flag's value is defined to be 0 and provides default array allocation
 * - ::lwdaArrayLayered: Allocates a layered LWCA array, with the depth extent indicating the number of layers
 * - ::lwdaArrayLwbemap: Allocates a lwbemap LWCA array. Width must be equal to height, and depth must be six.
 *   If the lwdaArrayLayered flag is also set, depth must be a multiple of six.
 * - ::lwdaArraySurfaceLoadStore: Allocates a LWCA array that could be read from or written to using a surface
 *   reference.
 * - ::lwdaArrayTextureGather: This flag indicates that texture gather operations will be performed on the LWCA 
 *   array. Texture gather can only be performed on 2D LWCA arrays.
 * - ::lwdaArraySparse: Allocates a LWCA array without physical backing memory. The subregions within this sparse array 
 *   can later be mapped onto a physical memory allocation by calling ::lwMemMapArrayAsync. This flag can only be used for 
 *   creating 2D, 3D or 2D layered sparse LWCA arrays. The physical backing memory must be allocated via ::lwMemCreate.

 * - ::lwdaArrayDeferredMapping: Allocates a LWCA array without physical backing memory. The entire array can
 *   later be mapped onto a physical memory allocation by calling ::lwMemMapArrayAsync. The physical backing memory must be allocated
 *   via ::lwMemCreate.

 *
 * The width, height and depth extents must meet certain size requirements as listed in the following table.
 * All values are specified in elements.
 *
 * Note that 2D LWCA arrays have different size requirements if the ::lwdaArrayTextureGather flag is set. In that
 * case, the valid range for (width, height, depth) is ((1,maxTexture2DGather[0]), (1,maxTexture2DGather[1]), 0).
 *
 * \xmlonly
 * <table outputclass="xmlonly">
 * <tgroup cols="3" colsep="1" rowsep="1">
 * <colspec colname="c1" colwidth="1.0*"/>
 * <colspec colname="c2" colwidth="3.0*"/>
 * <colspec colname="c3" colwidth="3.0*"/>
 * <thead>
 * <row>
 * <entry>LWCA array type</entry>
 * <entry>Valid extents that must always be met {(width range in elements),
 * (height range), (depth range)}</entry>
 * <entry>Valid extents with lwdaArraySurfaceLoadStore set {(width range in
 * elements), (height range), (depth range)}</entry>
 * </row>
 * </thead>
 * <tbody>
 * <row>
 * <entry>1D</entry>
 * <entry>{ (1,maxTexture1D), 0, 0 }</entry>
 * <entry>{ (1,maxSurface1D), 0, 0 }</entry>
 * </row>
 * <row>
 * <entry>2D</entry>
 * <entry>{ (1,maxTexture2D[0]), (1,maxTexture2D[1]), 0 }</entry>
 * <entry>{ (1,maxSurface2D[0]), (1,maxSurface2D[1]), 0 }</entry>
 * </row>
 * <row>
 * <entry>3D</entry>
 * <entry>{ (1,maxTexture3D[0]), (1,maxTexture3D[1]), (1,maxTexture3D[2]) }
 * OR { (1,maxTexture3DAlt[0]), (1,maxTexture3DAlt[1]),
 * (1,maxTexture3DAlt[2]) }</entry>
 * <entry>{ (1,maxSurface3D[0]), (1,maxSurface3D[1]), (1,maxSurface3D[2]) }</entry>
 * </row>
 * <row>
 * <entry>1D Layered</entry>
 * <entry>{ (1,maxTexture1DLayered[0]), 0, (1,maxTexture1DLayered[1]) }</entry>
 * <entry>{ (1,maxSurface1DLayered[0]), 0, (1,maxSurface1DLayered[1]) }</entry>
 * </row>
 * <row>
 * <entry>2D Layered</entry>
 * <entry>{ (1,maxTexture2DLayered[0]), (1,maxTexture2DLayered[1]),
 * (1,maxTexture2DLayered[2]) }</entry>
 * <entry>{ (1,maxSurface2DLayered[0]), (1,maxSurface2DLayered[1]),
 * (1,maxSurface2DLayered[2]) }</entry>
 * </row>
 * <row>
 * <entry>Lwbemap</entry>
 * <entry>{ (1,maxTextureLwbemap), (1,maxTextureLwbemap), 6 }</entry>
 * <entry>{ (1,maxSurfaceLwbemap), (1,maxSurfaceLwbemap), 6 }</entry>
 * </row>
 * <row>
 * <entry>Lwbemap Layered</entry>
 * <entry>{ (1,maxTextureLwbemapLayered[0]), (1,maxTextureLwbemapLayered[0]),
 * (1,maxTextureLwbemapLayered[1]) }</entry>
 * <entry>{ (1,maxSurfaceLwbemapLayered[0]), (1,maxSurfaceLwbemapLayered[0]),
 * (1,maxSurfaceLwbemapLayered[1]) }</entry>
 * </row>
 * </tbody>
 * </tgroup>
 * </table>
 * \endxmlonly
 *
 * \param array  - Pointer to allocated array in device memory
 * \param desc   - Requested channel format
 * \param extent - Requested allocation size (\p width field in elements)
 * \param flags  - Flags for extensions
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc3D, ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree,
 * ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::make_lwdaExtent,
 * ::lwArray3DCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMalloc3DArray(lwdaArray_t *array, const struct lwdaChannelFormatDesc* desc, struct lwdaExtent extent, unsigned int flags __dv(0));

/**
 * \brief Allocate a mipmapped array on the device
 *
 * Allocates a LWCA mipmapped array according to the ::lwdaChannelFormatDesc structure
 * \p desc and returns a handle to the new LWCA mipmapped array in \p *mipmappedArray.
 * \p numLevels specifies the number of mipmap levels to be allocated. This value is
 * clamped to the range [1, 1 + floor(log2(max(width, height, depth)))].
 *
 * The ::lwdaChannelFormatDesc is defined as:
 * \code
    struct lwdaChannelFormatDesc {
        int x, y, z, w;
        enum lwdaChannelFormatKind f;
    };
    \endcode
 * where ::lwdaChannelFormatKind is one of ::lwdaChannelFormatKindSigned,
 * ::lwdaChannelFormatKindUnsigned, or ::lwdaChannelFormatKindFloat.
 *
 * ::lwdaMallocMipmappedArray() can allocate the following:
 *
 * - A 1D mipmapped array is allocated if the height and depth extents are both zero.
 * - A 2D mipmapped array is allocated if only the depth extent is zero.
 * - A 3D mipmapped array is allocated if all three extents are non-zero.
 * - A 1D layered LWCA mipmapped array is allocated if only the height extent is zero and
 * the lwdaArrayLayered flag is set. Each layer is a 1D mipmapped array. The number of layers is 
 * determined by the depth extent.
 * - A 2D layered LWCA mipmapped array is allocated if all three extents are non-zero and 
 * the lwdaArrayLayered flag is set. Each layer is a 2D mipmapped array. The number of layers is 
 * determined by the depth extent.
 * - A lwbemap LWCA mipmapped array is allocated if all three extents are non-zero and the
 * lwdaArrayLwbemap flag is set. Width must be equal to height, and depth must be six.
 * The order of the six layers in memory is the same as that listed in ::lwdaGraphicsLwbeFace.
 * - A lwbemap layered LWCA mipmapped array is allocated if all three extents are non-zero, and both,
 * lwdaArrayLwbemap and lwdaArrayLayered flags are set. Width must be equal to height, and depth must be 
 * a multiple of six. A lwbemap layered LWCA mipmapped array is a special type of 2D layered LWCA mipmapped
 * array that consists of a collection of lwbemap mipmapped arrays. The first six layers represent the 
 * first lwbemap mipmapped array, the next six layers form the second lwbemap mipmapped array, and so on.
 *
 *
 * The \p flags parameter enables different options to be specified that affect
 * the allocation, as follows.
 * - ::lwdaArrayDefault: This flag's value is defined to be 0 and provides default mipmapped array allocation
 * - ::lwdaArrayLayered: Allocates a layered LWCA mipmapped array, with the depth extent indicating the number of layers
 * - ::lwdaArrayLwbemap: Allocates a lwbemap LWCA mipmapped array. Width must be equal to height, and depth must be six.
 *   If the lwdaArrayLayered flag is also set, depth must be a multiple of six.
 * - ::lwdaArraySurfaceLoadStore: This flag indicates that individual mipmap levels of the LWCA mipmapped array 
 *   will be read from or written to using a surface reference.
 * - ::lwdaArrayTextureGather: This flag indicates that texture gather operations will be performed on the LWCA 
 *   array. Texture gather can only be performed on 2D LWCA mipmapped arrays, and the gather operations are
 *   performed only on the most detailed mipmap level.
 * - ::lwdaArraySparse: Allocates a LWCA mipmapped array without physical backing memory. The subregions within this sparse array
 *   can later be mapped onto a physical memory allocation by calling ::lwMemMapArrayAsync. This flag can only be used for creating 
 *   2D, 3D or 2D layered sparse LWCA mipmapped arrays. The physical backing memory must be allocated via ::lwMemCreate.

 * - ::lwdaArrayDeferredMapping: Allocates a LWCA mipmapped array without physical backing memory. The entire array can
 *   later be mapped onto a physical memory allocation by calling ::lwMemMapArrayAsync. The physical backing memory must be allocated
 *   via ::lwMemCreate.

 *
 * The width, height and depth extents must meet certain size requirements as listed in the following table.
 * All values are specified in elements.
 *
 * \xmlonly
 * <table outputclass="xmlonly">
 * <tgroup cols="3" colsep="1" rowsep="1">
 * <colspec colname="c1" colwidth="1.0*"/>
 * <colspec colname="c2" colwidth="3.0*"/>
 * <colspec colname="c3" colwidth="3.0*"/>
 * <thead>
 * <row>
 * <entry>LWCA array type</entry>
 * <entry>Valid extents that must always be met {(width range in elements),
 * (height range), (depth range)}</entry>
 * <entry>Valid extents with lwdaArraySurfaceLoadStore set {(width range in
 * elements), (height range), (depth range)}</entry>
 * </row>
 * </thead>
 * <tbody>
 * <row>
 * <entry>1D</entry>
 * <entry>{ (1,maxTexture1DMipmap), 0, 0 }</entry>
 * <entry>{ (1,maxSurface1D), 0, 0 }</entry>
 * </row>
 * <row>
 * <entry>2D</entry>
 * <entry>{ (1,maxTexture2DMipmap[0]), (1,maxTexture2DMipmap[1]), 0 }</entry>
 * <entry>{ (1,maxSurface2D[0]), (1,maxSurface2D[1]), 0 }</entry>
 * </row>
 * <row>
 * <entry>3D</entry>
 * <entry>{ (1,maxTexture3D[0]), (1,maxTexture3D[1]), (1,maxTexture3D[2]) }
 * OR { (1,maxTexture3DAlt[0]), (1,maxTexture3DAlt[1]),
 * (1,maxTexture3DAlt[2]) }</entry>
 * <entry>{ (1,maxSurface3D[0]), (1,maxSurface3D[1]), (1,maxSurface3D[2]) }</entry>
 * </row>
 * <row>
 * <entry>1D Layered</entry>
 * <entry>{ (1,maxTexture1DLayered[0]), 0, (1,maxTexture1DLayered[1]) }</entry>
 * <entry>{ (1,maxSurface1DLayered[0]), 0, (1,maxSurface1DLayered[1]) }</entry>
 * </row>
 * <row>
 * <entry>2D Layered</entry>
 * <entry>{ (1,maxTexture2DLayered[0]), (1,maxTexture2DLayered[1]),
 * (1,maxTexture2DLayered[2]) }</entry>
 * <entry>{ (1,maxSurface2DLayered[0]), (1,maxSurface2DLayered[1]),
 * (1,maxSurface2DLayered[2]) }</entry>
 * </row>
 * <row>
 * <entry>Lwbemap</entry>
 * <entry>{ (1,maxTextureLwbemap), (1,maxTextureLwbemap), 6 }</entry>
 * <entry>{ (1,maxSurfaceLwbemap), (1,maxSurfaceLwbemap), 6 }</entry>
 * </row>
 * <row>
 * <entry>Lwbemap Layered</entry>
 * <entry>{ (1,maxTextureLwbemapLayered[0]), (1,maxTextureLwbemapLayered[0]),
 * (1,maxTextureLwbemapLayered[1]) }</entry>
 * <entry>{ (1,maxSurfaceLwbemapLayered[0]), (1,maxSurfaceLwbemapLayered[0]),
 * (1,maxSurfaceLwbemapLayered[1]) }</entry>
 * </row>
 * </tbody>
 * </tgroup>
 * </table>
 * \endxmlonly
 *
 * \param mipmappedArray  - Pointer to allocated mipmapped array in device memory
 * \param desc            - Requested channel format
 * \param extent          - Requested allocation size (\p width field in elements)
 * \param numLevels       - Number of mipmap levels to allocate
 * \param flags           - Flags for extensions
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc3D, ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree,
 * ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::make_lwdaExtent,
 * ::lwMipmappedArrayCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMallocMipmappedArray(lwdaMipmappedArray_t *mipmappedArray, const struct lwdaChannelFormatDesc* desc, struct lwdaExtent extent, unsigned int numLevels, unsigned int flags __dv(0));

/**
 * \brief Gets a mipmap level of a LWCA mipmapped array
 *
 * Returns in \p *levelArray a LWCA array that represents a single mipmap level
 * of the LWCA mipmapped array \p mipmappedArray.
 *
 * If \p level is greater than the maximum number of levels in this mipmapped array,
 * ::lwdaErrorIlwalidValue is returned.
 *
 * If \p mipmappedArray is NULL,
 * ::lwdaErrorIlwalidResourceHandle is returned.
 *
 * \param levelArray     - Returned mipmap level LWCA array
 * \param mipmappedArray - LWCA mipmapped array
 * \param level          - Mipmap level
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc3D, ::lwdaMalloc, ::lwdaMallocPitch, ::lwdaFree,
 * ::lwdaFreeArray,
 * \ref ::lwdaMallocHost(void**, size_t) "lwdaMallocHost (C API)",
 * ::lwdaFreeHost, ::lwdaHostAlloc,
 * ::make_lwdaExtent,
 * ::lwMipmappedArrayGetLevel
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetMipmappedArrayLevel(lwdaArray_t *levelArray, lwdaMipmappedArray_const_t mipmappedArray, unsigned int level);

/**
 * \brief Copies data between 3D objects
 *
\code
struct lwdaExtent {
  size_t width;
  size_t height;
  size_t depth;
};
struct lwdaExtent make_lwdaExtent(size_t w, size_t h, size_t d);

struct lwdaPos {
  size_t x;
  size_t y;
  size_t z;
};
struct lwdaPos make_lwdaPos(size_t x, size_t y, size_t z);

struct lwdaMemcpy3DParms {
  lwdaArray_t           srcArray;
  struct lwdaPos        srcPos;
  struct lwdaPitchedPtr srcPtr;
  lwdaArray_t           dstArray;
  struct lwdaPos        dstPos;
  struct lwdaPitchedPtr dstPtr;
  struct lwdaExtent     extent;
  enum lwdaMemcpyKind   kind;
};
\endcode
 *
 * ::lwdaMemcpy3D() copies data betwen two 3D objects. The source and
 * destination objects may be in either host memory, device memory, or a LWCA
 * array. The source, destination, extent, and kind of copy performed is
 * specified by the ::lwdaMemcpy3DParms struct which should be initialized to
 * zero before use:
\code
lwdaMemcpy3DParms myParms = {0};
\endcode
 *
 * The struct passed to ::lwdaMemcpy3D() must specify one of \p srcArray or
 * \p srcPtr and one of \p dstArray or \p dstPtr. Passing more than one
 * non-zero source or destination will cause ::lwdaMemcpy3D() to return an
 * error.
 *
 * The \p srcPos and \p dstPos fields are optional offsets into the source and
 * destination objects and are defined in units of each object's elements. The
 * element for a host or device pointer is assumed to be <b>unsigned char</b>.
 *
 * The \p extent field defines the dimensions of the transferred area in
 * elements. If a LWCA array is participating in the copy, the extent is
 * defined in terms of that array's elements. If no LWCA array is
 * participating in the copy then the extents are defined in elements of
 * <b>unsigned char</b>.
 *
 * The \p kind field defines the direction of the copy. It must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * For ::lwdaMemcpyHostToHost or ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost
 * passed as kind and lwdaArray type passed as source or destination, if the kind
 * implies lwdaArray type to be present on the host, ::lwdaMemcpy3D() will
 * disregard that implication and silently correct the kind based on the fact that
 * lwdaArray type can only be present on the device.
 *
 * If the source and destination are both arrays, ::lwdaMemcpy3D() will return
 * an error if they do not have the same element size.
 *
 * The source and destination object may not overlap. If overlapping source
 * and destination objects are specified, undefined behavior will result.
 *
 * The source object must entirely contain the region defined by \p srcPos
 * and \p extent. The destination object must entirely contain the region
 * defined by \p dstPos and \p extent.
 *
 * ::lwdaMemcpy3D() returns an error if the pitch of \p srcPtr or \p dstPtr
 * exceeds the maximum allowed. The pitch of a ::lwdaPitchedPtr allocated
 * with ::lwdaMalloc3D() will always be valid.
 *
 * \param p - 3D memory copy parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc3D, ::lwdaMalloc3DArray, ::lwdaMemset3D, ::lwdaMemcpy3DAsync,
 * ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::make_lwdaExtent, ::make_lwdaPos,
 * ::lwMemcpy3D
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy3D(const struct lwdaMemcpy3DParms *p);

/**
 * \brief Copies memory between devices
 *
 * Perform a 3D memory copy according to the parameters specified in
 * \p p.  See the definition of the ::lwdaMemcpy3DPeerParms structure
 * for documentation of its parameters.
 *
 * Note that this function is synchronous with respect to the host only if
 * the source or destination of the transfer is host memory.  Note also 
 * that this copy is serialized with respect to all pending and future 
 * asynchronous work in to the current device, the copy's source device,
 * and the copy's destination device (use ::lwdaMemcpy3DPeerAsync to avoid 
 * this synchronization).
 *
 * \param p - Parameters for the memory copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpyPeer, ::lwdaMemcpyAsync, ::lwdaMemcpyPeerAsync,
 * ::lwdaMemcpy3DPeerAsync,
 * ::lwMemcpy3DPeer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy3DPeer(const struct lwdaMemcpy3DPeerParms *p);

/**
 * \brief Copies data between 3D objects
 *
\code
struct lwdaExtent {
  size_t width;
  size_t height;
  size_t depth;
};
struct lwdaExtent make_lwdaExtent(size_t w, size_t h, size_t d);

struct lwdaPos {
  size_t x;
  size_t y;
  size_t z;
};
struct lwdaPos make_lwdaPos(size_t x, size_t y, size_t z);

struct lwdaMemcpy3DParms {
  lwdaArray_t           srcArray;
  struct lwdaPos        srcPos;
  struct lwdaPitchedPtr srcPtr;
  lwdaArray_t           dstArray;
  struct lwdaPos        dstPos;
  struct lwdaPitchedPtr dstPtr;
  struct lwdaExtent     extent;
  enum lwdaMemcpyKind   kind;
};
\endcode
 *
 * ::lwdaMemcpy3DAsync() copies data betwen two 3D objects. The source and
 * destination objects may be in either host memory, device memory, or a LWCA
 * array. The source, destination, extent, and kind of copy performed is
 * specified by the ::lwdaMemcpy3DParms struct which should be initialized to
 * zero before use:
\code
lwdaMemcpy3DParms myParms = {0};
\endcode
 *
 * The struct passed to ::lwdaMemcpy3DAsync() must specify one of \p srcArray
 * or \p srcPtr and one of \p dstArray or \p dstPtr. Passing more than one
 * non-zero source or destination will cause ::lwdaMemcpy3DAsync() to return an
 * error.
 *
 * The \p srcPos and \p dstPos fields are optional offsets into the source and
 * destination objects and are defined in units of each object's elements. The
 * element for a host or device pointer is assumed to be <b>unsigned char</b>.
 * For LWCA arrays, positions must be in the range [0, 2048) for any
 * dimension.
 *
 * The \p extent field defines the dimensions of the transferred area in
 * elements. If a LWCA array is participating in the copy, the extent is
 * defined in terms of that array's elements. If no LWCA array is
 * participating in the copy then the extents are defined in elements of
 * <b>unsigned char</b>.
 *
 * The \p kind field defines the direction of the copy. It must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * For ::lwdaMemcpyHostToHost or ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost
 * passed as kind and lwdaArray type passed as source or destination, if the kind
 * implies lwdaArray type to be present on the host, ::lwdaMemcpy3DAsync() will
 * disregard that implication and silently correct the kind based on the fact that
 * lwdaArray type can only be present on the device.
 *
 * If the source and destination are both arrays, ::lwdaMemcpy3DAsync() will
 * return an error if they do not have the same element size.
 *
 * The source and destination object may not overlap. If overlapping source
 * and destination objects are specified, undefined behavior will result.
 *
 * The source object must lie entirely within the region defined by \p srcPos
 * and \p extent. The destination object must lie entirely within the region
 * defined by \p dstPos and \p extent.
 *
 * ::lwdaMemcpy3DAsync() returns an error if the pitch of \p srcPtr or
 * \p dstPtr exceeds the maximum allowed. The pitch of a
 * ::lwdaPitchedPtr allocated with ::lwdaMalloc3D() will always be valid.
 *
 * ::lwdaMemcpy3DAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally
 * be associated to a stream by passing a non-zero \p stream argument. If
 * \p kind is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and \p stream
 * is non-zero, the copy may overlap with operations in other streams.
 *
 * The device version of this function only handles device to device copies and
 * cannot be given local or shared pointers.
 *
 * \param p      - 3D memory copy parameters
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMalloc3D, ::lwdaMalloc3DArray, ::lwdaMemset3D, ::lwdaMemcpy3D,
 * ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, :::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::make_lwdaExtent, ::make_lwdaPos,
 * ::lwMemcpy3DAsync
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy3DAsync(const struct lwdaMemcpy3DParms *p, lwdaStream_t stream __dv(0));

/**
 * \brief Copies memory between devices asynchronously.
 *
 * Perform a 3D memory copy according to the parameters specified in
 * \p p.  See the definition of the ::lwdaMemcpy3DPeerParms structure
 * for documentation of its parameters.
 *
 * \param p      - Parameters for the memory copy
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpyPeer, ::lwdaMemcpyAsync, ::lwdaMemcpyPeerAsync,
 * ::lwdaMemcpy3DPeerAsync,
 * ::lwMemcpy3DPeerAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy3DPeerAsync(const struct lwdaMemcpy3DPeerParms *p, lwdaStream_t stream __dv(0));

/**
 * \brief Gets free and total device memory
 *
 * Returns in \p *total the total amount of memory available to the the current context.
 * Returns in \p *free the amount of memory on the device that is free according to the OS.
 * LWCA is not guaranteed to be able to allocate all of the memory that the OS reports as free.
 * In a multi-tenet situation, free estimate returned is prone to race condition where
 * a new allocation/free done by a different process or a different thread in the same
 * process between the time when free memory was estimated and reported, will result in
 * deviation in free value reported and actual free memory.
 *
 * The integrated GPU on CheetAh shares memory with CPU and other component
 * of the SoC. The free and total values returned by the API excludes
 * the SWAP memory space maintained by the OS on some platforms.
 * The OS may move some of the memory pages into swap area as the GPU or
 * CPU allocate or access memory. See CheetAh app note on how to callwlate
 * total and free memory on CheetAh.
 *
 * \param free  - Returned free memory in bytes
 * \param total - Returned total memory in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorLaunchFailure
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwMemGetInfo
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemGetInfo(size_t *free, size_t *total);

/**
 * \brief Gets info about the specified lwdaArray
 * 
 * Returns in \p *desc, \p *extent and \p *flags respectively, the type, shape 
 * and flags of \p array.
 *
 * Any of \p *desc, \p *extent and \p *flags may be specified as NULL.
 *
 * \param desc   - Returned array type
 * \param extent - Returned array shape. 2D arrays will have depth of zero
 * \param flags  - Returned array flags
 * \param array  - The ::lwdaArray to get info for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwArrayGetDescriptor,
 * ::lwArray3DGetDescriptor
 */
extern __host__ lwdaError_t LWDARTAPI lwdaArrayGetInfo(struct lwdaChannelFormatDesc *desc, struct lwdaExtent *extent, unsigned int *flags, lwdaArray_t array);

/**
 * \brief Gets a LWCA array plane from a LWCA array
 *
 * Returns in \p pPlaneArray a LWCA array that represents a single format plane
 * of the LWCA array \p hArray.
 *
 * If \p planeIdx is greater than the maximum number of planes in this array or if the array does
 * not have a multi-planar format e.g: ::lwdaChannelFormatKindLW12, then ::lwdaErrorIlwalidValue is returned.
 *
 * Note that if the \p hArray has format ::lwdaChannelFormatKindLW12, then passing in 0 for \p planeIdx returns
 * a LWCA array of the same size as \p hArray but with one 8-bit channel and ::lwdaChannelFormatKindUnsigned as its format kind.
 * If 1 is passed for \p planeIdx, then the returned LWCA array has half the height and width
 * of \p hArray with two 8-bit channels and ::lwdaChannelFormatKindUnsigned as its format kind.
 *
 * \param pPlaneArray   - Returned LWCA array referenced by the \p planeIdx
 * \param hArray        - LWCA array
 * \param planeIdx      - Plane index
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 *
 * \sa
 * ::lwArrayGetPlane
 */
extern __host__ lwdaError_t LWDARTAPI lwdaArrayGetPlane(lwdaArray_t *pPlaneArray, lwdaArray_t hArray, unsigned int planeIdx);


/**
 * \brief Returns the memory requirements of a LWCA array
 *
 * Returns the memory requirements of a LWCA array in \p memoryRequirements
 * If the LWCA array is not allocated with flag ::lwdaArrayDeferredMapping
 * ::lwdaErrorIlwalidValue will be returned.
 *
 * The returned value in ::lwdaArrayMemoryRequirements::size
 * represents the total size of the LWCA array.
 * The returned value in ::lwdaArrayMemoryRequirements::alignment
 * represents the alignment necessary for mapping the LWCA array.
 *
 * \return
 * ::lwdaSuccess
 * ::lwdaErrorIlwalidValue
 *
 * \param[out] memoryRequirements - Pointer to ::lwdaArrayMemoryRequirements
 * \param[in] array - LWCA array to get the memory requirements of
 * \param[in] device - Device to get the memory requirements for
 * \sa ::lwdaMipmappedArrayGetMemoryRequirements
 */
extern __host__ lwdaError_t LWDARTAPI lwdaArrayGetMemoryRequirements(struct lwdaArrayMemoryRequirements  *memoryRequirements, lwdaArray_t array, int device);

/**
 * \brief Returns the memory requirements of a LWCA mipmapped array
 *
 * Returns the memory requirements of a LWCA mipmapped array in \p memoryRequirements
 * If the LWCA mipmapped array is not allocated with flag ::lwdaArrayDeferredMapping
 * ::lwdaErrorIlwalidValue will be returned.
 *
 * The returned value in ::lwdaArrayMemoryRequirements::size
 * represents the total size of the LWCA mipmapped array.
 * The returned value in ::lwdaArrayMemoryRequirements::alignment
 * represents the alignment necessary for mapping the LWCA mipmapped
 * array.
 *
 * \return
 * ::lwdaSuccess
 * ::lwdaErrorIlwalidValue
 *
 * \param[out] memoryRequirements - Pointer to ::lwdaArrayMemoryRequirements
 * \param[in] mipmap - LWCA mipmapped array to get the memory requirements of
 * \param[in] device - Device to get the memory requirements for
 * \sa ::lwdaArrayGetMemoryRequirements
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMipmappedArrayGetMemoryRequirements(struct lwdaArrayMemoryRequirements *memoryRequirements, lwdaMipmappedArray_t mipmap, int device);


/**
 * \brief Returns the layout properties of a sparse LWCA array
 *
 * Returns the layout properties of a sparse LWCA array in \p sparseProperties.
 * If the LWCA array is not allocated with flag ::lwdaArraySparse
 * ::lwdaErrorIlwalidValue will be returned.
 *
 * If the returned value in ::lwdaArraySparseProperties::flags contains ::lwdaArraySparsePropertiesSingleMipTail,
 * then ::lwdaArraySparseProperties::miptailSize represents the total size of the array. Otherwise, it will be zero.
 * Also, the returned value in ::lwdaArraySparseProperties::miptailFirstLevel is always zero.
 * Note that the \p array must have been allocated using ::lwdaMallocArray or ::lwdaMalloc3DArray. For LWCA arrays obtained
 * using ::lwdaMipmappedArrayGetLevel, ::lwdaErrorIlwalidValue will be returned. Instead, ::lwdaMipmappedArrayGetSparseProperties
 * must be used to obtain the sparse properties of the entire LWCA mipmapped array to which \p array belongs to.
 *
 * \return
 * ::lwdaSuccess
 * ::lwdaErrorIlwalidValue
 *
 * \param[out] sparseProperties - Pointer to return the ::lwdaArraySparseProperties
 * \param[in] array             - The LWCA array to get the sparse properties of 
 *
 * \sa
 * ::lwdaMipmappedArrayGetSparseProperties,
 * ::lwMemMapArrayAsync
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaArrayGetSparseProperties(struct lwdaArraySparseProperties *sparseProperties, lwdaArray_t array);
#endif

/**
 * \brief Returns the layout properties of a sparse LWCA mipmapped array
 *
 * Returns the sparse array layout properties in \p sparseProperties.
 * If the LWCA mipmapped array is not allocated with flag ::lwdaArraySparse
 * ::lwdaErrorIlwalidValue will be returned.
 *
 * For non-layered LWCA mipmapped arrays, ::lwdaArraySparseProperties::miptailSize returns the
 * size of the mip tail region. The mip tail region includes all mip levels whose width, height or depth
 * is less than that of the tile.
 * For layered LWCA mipmapped arrays, if ::lwdaArraySparseProperties::flags contains ::lwdaArraySparsePropertiesSingleMipTail,
 * then ::lwdaArraySparseProperties::miptailSize specifies the size of the mip tail of all layers combined.
 * Otherwise, ::lwdaArraySparseProperties::miptailSize specifies mip tail size per layer.
 * The returned value of ::lwdaArraySparseProperties::miptailFirstLevel is valid only if ::lwdaArraySparseProperties::miptailSize is non-zero.
 *
 * \return
 * ::lwdaSuccess
 * ::lwdaErrorIlwalidValue
 *
 * \param[out] sparseProperties - Pointer to return ::lwdaArraySparseProperties
 * \param[in] mipmap            - The LWCA mipmapped array to get the sparse properties of
 *
 * \sa
 * ::lwdaArrayGetSparseProperties,
 * ::lwMemMapArrayAsync
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaMipmappedArrayGetSparseProperties(struct lwdaArraySparseProperties *sparseProperties, lwdaMipmappedArray_t mipmap);
#endif

/**
 * \brief Copies data between host and device
 *
 * Copies \p count bytes from the memory area pointed to by \p src to the
 * memory area pointed to by \p dst, where \p kind specifies the direction
 * of the copy, and must be one of ::lwdaMemcpyHostToHost,
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing. Calling
 * ::lwdaMemcpy() with dst and src pointers that do not match the direction of
 * the copy results in an undefined behavior.
 *
 * \param dst   - Destination memory address
 * \param src   - Source memory address
 * \param count - Size in bytes to copy
 * \param kind  - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \note_sync
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyDtoH,
 * ::lwMemcpyHtoD,
 * ::lwMemcpyDtoD,
 * ::lwMemcpy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy(void *dst, const void *src, size_t count, enum lwdaMemcpyKind kind);

/**
 * \brief Copies memory between two devices
 *
 * Copies memory from one device to memory on another device.  \p dst is the 
 * base device pointer of the destination memory and \p dstDevice is the 
 * destination device.  \p src is the base device pointer of the source memory 
 * and \p srcDevice is the source device.  \p count specifies the number of bytes 
 * to copy.
 *
 * Note that this function is asynchronous with respect to the host, but 
 * serialized with respect all pending and future asynchronous work in to the 
 * current device, \p srcDevice, and \p dstDevice (use ::lwdaMemcpyPeerAsync 
 * to avoid this synchronization).
 *
 * \param dst       - Destination device pointer
 * \param dstDevice - Destination device
 * \param src       - Source device pointer
 * \param srcDevice - Source device
 * \param count     - Size of memory copy in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpyAsync, ::lwdaMemcpyPeerAsync,
 * ::lwdaMemcpy3DPeerAsync,
 * ::lwMemcpyPeer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyPeer(void *dst, int dstDevice, const void *src, int srcDevice, size_t count);

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the memory
 * area pointed to by \p src to the memory area pointed to by \p dst, where
 * \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing. \p dpitch and
 * \p spitch are the widths in memory in bytes of the 2D arrays pointed to by
 * \p dst and \p src, including any padding added to the end of each row. The
 * memory areas may not overlap. \p width must not exceed either \p dpitch or
 * \p spitch. Calling ::lwdaMemcpy2D() with \p dst and \p src pointers that do
 * not match the direction of the copy results in an undefined behavior.
 * ::lwdaMemcpy2D() returns an error if \p dpitch or \p spitch exceeds
 * the maximum allowed.
 *
 * \param dst    - Destination memory address
 * \param dpitch - Pitch of destination memory
 * \param src    - Source memory address
 * \param spitch - Pitch of source memory
 * \param width  - Width of matrix transfer (columns in bytes)
 * \param height - Height of matrix transfer (rows)
 * \param kind   - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2D,
 * ::lwMemcpy2DUnaligned
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2D(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind);

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the memory
 * area pointed to by \p src to the LWCA array \p dst starting at
 * \p hOffset rows and \p wOffset bytes from the upper left corner,
 * where \p kind specifies the direction of the copy, and must be one
 * of ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * \p spitch is the width in memory in bytes of the 2D array pointed to by
 * \p src, including any padding added to the end of each row. \p wOffset +
 * \p width must not exceed the width of the LWCA array \p dst. \p width must
 * not exceed \p spitch. ::lwdaMemcpy2DToArray() returns an error if \p spitch
 * exceeds the maximum allowed.
 *
 * \param dst     - Destination memory address
 * \param wOffset - Destination starting X offset (columns in bytes)
 * \param hOffset - Destination starting Y offset (rows)
 * \param src     - Source memory address
 * \param spitch  - Pitch of source memory
 * \param width   - Width of matrix transfer (columns in bytes)
 * \param height  - Height of matrix transfer (rows)
 * \param kind    - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2D,
 * ::lwMemcpy2DUnaligned
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DToArray(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind);

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the LWCA
 * array \p src starting at \p hOffset rows and \p wOffset bytes from the
 * upper left corner to the memory area pointed to by \p dst, where
 * \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing. \p dpitch is the
 * width in memory in bytes of the 2D array pointed to by \p dst, including any
 * padding added to the end of each row. \p wOffset + \p width must not exceed
 * the width of the LWCA array \p src. \p width must not exceed \p dpitch.
 * ::lwdaMemcpy2DFromArray() returns an error if \p dpitch exceeds the maximum
 * allowed.
 *
 * \param dst     - Destination memory address
 * \param dpitch  - Pitch of destination memory
 * \param src     - Source memory address
 * \param wOffset - Source starting X offset (columns in bytes)
 * \param hOffset - Source starting Y offset (rows)
 * \param width   - Width of matrix transfer (columns in bytes)
 * \param height  - Height of matrix transfer (rows)
 * \param kind    - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2D,
 * ::lwMemcpy2DUnaligned
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DFromArray(void *dst, size_t dpitch, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t width, size_t height, enum lwdaMemcpyKind kind);

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the LWCA
 * array \p src starting at \p hOffsetSrc rows and \p wOffsetSrc bytes from the
 * upper left corner to the LWCA array \p dst starting at \p hOffsetDst rows
 * and \p wOffsetDst bytes from the upper left corner, where \p kind
 * specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * \p wOffsetDst + \p width must not exceed the width of the LWCA array \p dst.
 * \p wOffsetSrc + \p width must not exceed the width of the LWCA array \p src.
 *
 * \param dst        - Destination memory address
 * \param wOffsetDst - Destination starting X offset (columns in bytes)
 * \param hOffsetDst - Destination starting Y offset (rows)
 * \param src        - Source memory address
 * \param wOffsetSrc - Source starting X offset (columns in bytes)
 * \param hOffsetSrc - Source starting Y offset (rows)
 * \param width      - Width of matrix transfer (columns in bytes)
 * \param height     - Height of matrix transfer (rows)
 * \param kind       - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2D,
 * ::lwMemcpy2DUnaligned
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DArrayToArray(lwdaArray_t dst, size_t wOffsetDst, size_t hOffsetDst, lwdaArray_const_t src, size_t wOffsetSrc, size_t hOffsetSrc, size_t width, size_t height, enum lwdaMemcpyKind kind __dv(lwdaMemcpyDeviceToDevice));

/**
 * \brief Copies data to the given symbol on the device
 *
 * Copies \p count bytes from the memory area pointed to by \p src
 * to the memory area pointed to by \p offset bytes from the start of symbol
 * \p symbol. The memory areas may not overlap. \p symbol is a variable that
 * resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of
 * transfer is inferred from the pointer values. However, ::lwdaMemcpyDefault
 * is only allowed on systems that support unified virtual addressing.
 *
 * \param symbol - Device symbol address
 * \param src    - Source memory address
 * \param count  - Size in bytes to copy
 * \param offset - Offset from start of symbol in bytes
 * \param kind   - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorIlwalidMemcpyDirection,
 * ::lwdaErrorNoKernelImageForDevice
 * \notefnerr
 * \note_sync
 * \note_string_api_deprecation
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray,  ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy,
 * ::lwMemcpyHtoD,
 * ::lwMemcpyDtoD
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyToSymbol(const void *symbol, const void *src, size_t count, size_t offset __dv(0), enum lwdaMemcpyKind kind __dv(lwdaMemcpyHostToDevice));

/**
 * \brief Copies data from the given symbol on the device
 *
 * Copies \p count bytes from the memory area pointed to by \p offset bytes
 * from the start of symbol \p symbol to the memory area pointed to by \p dst.
 * The memory areas may not overlap. \p symbol is a variable that
 * resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyDeviceToHost, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of
 * transfer is inferred from the pointer values. However, ::lwdaMemcpyDefault
 * is only allowed on systems that support unified virtual addressing.
 *
 * \param dst    - Destination memory address
 * \param symbol - Device symbol address
 * \param count  - Size in bytes to copy
 * \param offset - Offset from start of symbol in bytes
 * \param kind   - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorIlwalidMemcpyDirection,
 * ::lwdaErrorNoKernelImageForDevice
 * \notefnerr
 * \note_sync
 * \note_string_api_deprecation
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy,
 * ::lwMemcpyDtoH,
 * ::lwMemcpyDtoD
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromSymbol(void *dst, const void *symbol, size_t count, size_t offset __dv(0), enum lwdaMemcpyKind kind __dv(lwdaMemcpyDeviceToHost));


/**
 * \brief Copies data between host and device
 *
 * Copies \p count bytes from the memory area pointed to by \p src to the
 * memory area pointed to by \p dst, where \p kind specifies the
 * direction of the copy, and must be one of ::lwdaMemcpyHostToHost,
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * 
 * The memory areas may not overlap. Calling ::lwdaMemcpyAsync() with \p dst and
 * \p src pointers that do not match the direction of the copy results in an
 * undefined behavior.
 *
 * ::lwdaMemcpyAsync() is asynchronous with respect to the host, so the call
 * may return before the copy is complete. The copy can optionally be
 * associated to a stream by passing a non-zero \p stream argument. If \p kind
 * is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and the \p stream is
 * non-zero, the copy may overlap with operations in other streams.
 *
 * The device version of this function only handles device to device copies and
 * cannot be given local or shared pointers.
 *
 * \param dst    - Destination memory address
 * \param src    - Source memory address
 * \param count  - Size in bytes to copy
 * \param kind   - Type of transfer
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyAsync,
 * ::lwMemcpyDtoHAsync,
 * ::lwMemcpyHtoDAsync,
 * ::lwMemcpyDtoDAsync
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpyAsync(void *dst, const void *src, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/**
 * \brief Copies memory between two devices asynchronously.
 *
 * Copies memory from one device to memory on another device.  \p dst is the 
 * base device pointer of the destination memory and \p dstDevice is the 
 * destination device.  \p src is the base device pointer of the source memory 
 * and \p srcDevice is the source device.  \p count specifies the number of bytes 
 * to copy.
 *
 * Note that this function is asynchronous with respect to the host and all work
 * on other devices.
 *
 * \param dst       - Destination device pointer
 * \param dstDevice - Destination device
 * \param src       - Source device pointer
 * \param srcDevice - Source device
 * \param count     - Size of memory copy in bytes
 * \param stream    - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpyPeer, ::lwdaMemcpyAsync,
 * ::lwdaMemcpy3DPeerAsync,
 * ::lwMemcpyPeerAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyPeerAsync(void *dst, int dstDevice, const void *src, int srcDevice, size_t count, lwdaStream_t stream __dv(0));

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the memory
 * area pointed to by \p src to the memory area pointed to by \p dst, where
 * \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * \p dpitch and \p spitch are the widths in memory in bytes of the 2D arrays
 * pointed to by \p dst and \p src, including any padding added to the end of
 * each row. The memory areas may not overlap. \p width must not exceed either
 * \p dpitch or \p spitch.
 *
 * Calling ::lwdaMemcpy2DAsync() with \p dst and \p src pointers that do not
 * match the direction of the copy results in an undefined behavior.
 * ::lwdaMemcpy2DAsync() returns an error if \p dpitch or \p spitch is greater
 * than the maximum allowed.
 *
 * ::lwdaMemcpy2DAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally
 * be associated to a stream by passing a non-zero \p stream argument. If
 * \p kind is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and
 * \p stream is non-zero, the copy may overlap with operations in other
 * streams.
 *
 * The device version of this function only handles device to device copies and
 * cannot be given local or shared pointers.
 *
 * \param dst    - Destination memory address
 * \param dpitch - Pitch of destination memory
 * \param src    - Source memory address
 * \param spitch - Pitch of source memory
 * \param width  - Width of matrix transfer (columns in bytes)
 * \param height - Height of matrix transfer (rows)
 * \param kind   - Type of transfer
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2DAsync
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy2DAsync(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the memory
 * area pointed to by \p src to the LWCA array \p dst starting at \p hOffset
 * rows and \p wOffset bytes from the upper left corner, where \p kind specifies
 * the direction of the copy, and must be one of ::lwdaMemcpyHostToHost,
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * \p spitch is the width in memory in bytes of the 2D array pointed to by
 * \p src, including any padding added to the end of each row. \p wOffset +
 * \p width must not exceed the width of the LWCA array \p dst. \p width must
 * not exceed \p spitch. ::lwdaMemcpy2DToArrayAsync() returns an error if
 * \p spitch exceeds the maximum allowed.
 *
 * ::lwdaMemcpy2DToArrayAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally
 * be associated to a stream by passing a non-zero \p stream argument. If
 * \p kind is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and
 * \p stream is non-zero, the copy may overlap with operations in other
 * streams.
 *
 * \param dst     - Destination memory address
 * \param wOffset - Destination starting X offset (columns in bytes)
 * \param hOffset - Destination starting Y offset (rows)
 * \param src     - Source memory address
 * \param spitch  - Pitch of source memory
 * \param width   - Width of matrix transfer (columns in bytes)
 * \param height  - Height of matrix transfer (rows)
 * \param kind    - Type of transfer
 * \param stream  - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 *
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2DAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DToArrayAsync(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/**
 * \brief Copies data between host and device
 *
 * Copies a matrix (\p height rows of \p width bytes each) from the LWCA
 * array \p src starting at \p hOffset rows and \p wOffset bytes from the
 * upper left corner to the memory area pointed to by \p dst,
 * where \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 * \p dpitch is the width in memory in bytes of the 2D
 * array pointed to by \p dst, including any padding added to the end of each
 * row. \p wOffset + \p width must not exceed the width of the LWCA array
 * \p src. \p width must not exceed \p dpitch. ::lwdaMemcpy2DFromArrayAsync()
 * returns an error if \p dpitch exceeds the maximum allowed.
 *
 * ::lwdaMemcpy2DFromArrayAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally be
 * associated to a stream by passing a non-zero \p stream argument. If \p kind
 * is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and \p stream is
 * non-zero, the copy may overlap with operations in other streams.
 *
 * \param dst     - Destination memory address
 * \param dpitch  - Pitch of destination memory
 * \param src     - Source memory address
 * \param wOffset - Source starting X offset (columns in bytes)
 * \param hOffset - Source starting Y offset (rows)
 * \param width   - Width of matrix transfer (columns in bytes)
 * \param height  - Height of matrix transfer (rows)
 * \param kind    - Type of transfer
 * \param stream  - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidPitchValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 * \note_memcpy
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 *
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpy2DAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DFromArrayAsync(void *dst, size_t dpitch, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/**
 * \brief Copies data to the given symbol on the device
 *
 * Copies \p count bytes from the memory area pointed to by \p src
 * to the memory area pointed to by \p offset bytes from the start of symbol
 * \p symbol. The memory areas may not overlap. \p symbol is a variable that
 * resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of transfer
 * is inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * ::lwdaMemcpyToSymbolAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally
 * be associated to a stream by passing a non-zero \p stream argument. If
 * \p kind is ::lwdaMemcpyHostToDevice and \p stream is non-zero, the copy
 * may overlap with operations in other streams.
 *
 * \param symbol - Device symbol address
 * \param src    - Source memory address
 * \param count  - Size in bytes to copy
 * \param offset - Offset from start of symbol in bytes
 * \param kind   - Type of transfer
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorIlwalidMemcpyDirection,
 * ::lwdaErrorNoKernelImageForDevice
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_string_api_deprecation
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyAsync,
 * ::lwMemcpyHtoDAsync,
 * ::lwMemcpyDtoDAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyToSymbolAsync(const void *symbol, const void *src, size_t count, size_t offset, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/**
 * \brief Copies data from the given symbol on the device
 *
 * Copies \p count bytes from the memory area pointed to by \p offset bytes
 * from the start of symbol \p symbol to the memory area pointed to by \p dst.
 * The memory areas may not overlap. \p symbol is a variable that resides in
 * global or constant memory space. \p kind can be either
 * ::lwdaMemcpyDeviceToHost, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of transfer
 * is inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * ::lwdaMemcpyFromSymbolAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally be
 * associated to a stream by passing a non-zero \p stream argument. If \p kind
 * is ::lwdaMemcpyDeviceToHost and \p stream is non-zero, the copy may overlap
 * with operations in other streams.
 *
 * \param dst    - Destination memory address
 * \param symbol - Device symbol address
 * \param count  - Size in bytes to copy
 * \param offset - Offset from start of symbol in bytes
 * \param kind   - Type of transfer
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorIlwalidMemcpyDirection,
 * ::lwdaErrorNoKernelImageForDevice
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_string_api_deprecation
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync,
 * ::lwMemcpyAsync,
 * ::lwMemcpyDtoHAsync,
 * ::lwMemcpyDtoDAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromSymbolAsync(void *dst, const void *symbol, size_t count, size_t offset, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));


/**
 * \brief Initializes or sets device memory to a value
 *
 * Fills the first \p count bytes of the memory area pointed to by \p devPtr
 * with the constant byte value \p value.
 *
 * Note that this function is asynchronous with respect to the host unless
 * \p devPtr refers to pinned host memory.
 *
 * \param devPtr - Pointer to device memory
 * \param value  - Value to set for each byte of specified memory
 * \param count  - Size in bytes to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_memset
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwMemsetD8,
 * ::lwMemsetD16,
 * ::lwMemsetD32
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemset(void *devPtr, int value, size_t count);

/**
 * \brief Initializes or sets device memory to a value
 *
 * Sets to the specified value \p value a matrix (\p height rows of \p width
 * bytes each) pointed to by \p dstPtr. \p pitch is the width in bytes of the
 * 2D array pointed to by \p dstPtr, including any padding added to the end
 * of each row. This function performs fastest when the pitch is one that has
 * been passed back by ::lwdaMallocPitch().
 *
 * Note that this function is asynchronous with respect to the host unless
 * \p devPtr refers to pinned host memory.
 *
 * \param devPtr - Pointer to 2D device memory
 * \param pitch  - Pitch in bytes of 2D device memory(Unused if \p height is 1)
 * \param value  - Value to set for each byte of specified memory
 * \param width  - Width of matrix set (columns in bytes)
 * \param height - Height of matrix set (rows)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_memset
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemset, ::lwdaMemset3D, ::lwdaMemsetAsync,
 * ::lwdaMemset2DAsync, ::lwdaMemset3DAsync,
 * ::lwMemsetD2D8,
 * ::lwMemsetD2D16,
 * ::lwMemsetD2D32
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemset2D(void *devPtr, size_t pitch, int value, size_t width, size_t height);

/**
 * \brief Initializes or sets device memory to a value
 *
 * Initializes each element of a 3D array to the specified value \p value.
 * The object to initialize is defined by \p pitchedDevPtr. The \p pitch field
 * of \p pitchedDevPtr is the width in memory in bytes of the 3D array pointed
 * to by \p pitchedDevPtr, including any padding added to the end of each row.
 * The \p xsize field specifies the logical width of each row in bytes, while
 * the \p ysize field specifies the height of each 2D slice in rows.
 * The \p pitch field of \p pitchedDevPtr is ignored when \p height and \p depth 
 * are both equal to 1. 
 *
 * The extents of the initialized region are specified as a \p width in bytes,
 * a \p height in rows, and a \p depth in slices.
 *
 * Extents with \p width greater than or equal to the \p xsize of
 * \p pitchedDevPtr may perform significantly faster than extents narrower
 * than the \p xsize. Secondarily, extents with \p height equal to the
 * \p ysize of \p pitchedDevPtr will perform faster than when the \p height is
 * shorter than the \p ysize.
 *
 * This function performs fastest when the \p pitchedDevPtr has been allocated
 * by ::lwdaMalloc3D().
 *
 * Note that this function is asynchronous with respect to the host unless
 * \p pitchedDevPtr refers to pinned host memory.
 *
 * \param pitchedDevPtr - Pointer to pitched device memory
 * \param value         - Value to set for each byte of specified memory
 * \param extent        - Size parameters for where to set device memory (\p width field in bytes)
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_memset
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemset, ::lwdaMemset2D,
 * ::lwdaMemsetAsync, ::lwdaMemset2DAsync, ::lwdaMemset3DAsync,
 * ::lwdaMalloc3D, ::make_lwdaPitchedPtr,
 * ::make_lwdaExtent
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemset3D(struct lwdaPitchedPtr pitchedDevPtr, int value, struct lwdaExtent extent);

/**
 * \brief Initializes or sets device memory to a value
 *
 * Fills the first \p count bytes of the memory area pointed to by \p devPtr
 * with the constant byte value \p value.
 *
 * ::lwdaMemsetAsync() is asynchronous with respect to the host, so
 * the call may return before the memset is complete. The operation can optionally
 * be associated to a stream by passing a non-zero \p stream argument.
 * If \p stream is non-zero, the operation may overlap with operations in other streams.
 *
 * The device version of this function only handles device to device copies and
 * cannot be given local or shared pointers.
 *
 * \param devPtr - Pointer to device memory
 * \param value  - Value to set for each byte of specified memory
 * \param count  - Size in bytes to set
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_memset
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemset, ::lwdaMemset2D, ::lwdaMemset3D,
 * ::lwdaMemset2DAsync, ::lwdaMemset3DAsync,
 * ::lwMemsetD8Async,
 * ::lwMemsetD16Async,
 * ::lwMemsetD32Async
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemsetAsync(void *devPtr, int value, size_t count, lwdaStream_t stream __dv(0));

/**
 * \brief Initializes or sets device memory to a value
 *
 * Sets to the specified value \p value a matrix (\p height rows of \p width
 * bytes each) pointed to by \p dstPtr. \p pitch is the width in bytes of the
 * 2D array pointed to by \p dstPtr, including any padding added to the end
 * of each row. This function performs fastest when the pitch is one that has
 * been passed back by ::lwdaMallocPitch().
 *
 * ::lwdaMemset2DAsync() is asynchronous with respect to the host, so
 * the call may return before the memset is complete. The operation can optionally
 * be associated to a stream by passing a non-zero \p stream argument.
 * If \p stream is non-zero, the operation may overlap with operations in other streams.
 *
 * The device version of this function only handles device to device copies and
 * cannot be given local or shared pointers.
 *
 * \param devPtr - Pointer to 2D device memory
 * \param pitch  - Pitch in bytes of 2D device memory(Unused if \p height is 1)
 * \param value  - Value to set for each byte of specified memory
 * \param width  - Width of matrix set (columns in bytes)
 * \param height - Height of matrix set (rows)
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_memset
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemset, ::lwdaMemset2D, ::lwdaMemset3D,
 * ::lwdaMemsetAsync, ::lwdaMemset3DAsync,
 * ::lwMemsetD2D8Async,
 * ::lwMemsetD2D16Async,
 * ::lwMemsetD2D32Async
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset2DAsync(void *devPtr, size_t pitch, int value, size_t width, size_t height, lwdaStream_t stream __dv(0));

/**
 * \brief Initializes or sets device memory to a value
 *
 * Initializes each element of a 3D array to the specified value \p value.
 * The object to initialize is defined by \p pitchedDevPtr. The \p pitch field
 * of \p pitchedDevPtr is the width in memory in bytes of the 3D array pointed
 * to by \p pitchedDevPtr, including any padding added to the end of each row.
 * The \p xsize field specifies the logical width of each row in bytes, while
 * the \p ysize field specifies the height of each 2D slice in rows.
 * The \p pitch field of \p pitchedDevPtr is ignored when \p height and \p depth 
 * are both equal to 1. 
 *
 * The extents of the initialized region are specified as a \p width in bytes,
 * a \p height in rows, and a \p depth in slices.
 *
 * Extents with \p width greater than or equal to the \p xsize of
 * \p pitchedDevPtr may perform significantly faster than extents narrower
 * than the \p xsize. Secondarily, extents with \p height equal to the
 * \p ysize of \p pitchedDevPtr will perform faster than when the \p height is
 * shorter than the \p ysize.
 *
 * This function performs fastest when the \p pitchedDevPtr has been allocated
 * by ::lwdaMalloc3D().
 *
 * ::lwdaMemset3DAsync() is asynchronous with respect to the host, so
 * the call may return before the memset is complete. The operation can optionally
 * be associated to a stream by passing a non-zero \p stream argument.
 * If \p stream is non-zero, the operation may overlap with operations in other streams.
 *
 * The device version of this function only handles device to device copies and
 * cannot be given local or shared pointers.
 *
 * \param pitchedDevPtr - Pointer to pitched device memory
 * \param value         - Value to set for each byte of specified memory
 * \param extent        - Size parameters for where to set device memory (\p width field in bytes)
 * \param stream - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_memset
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemset, ::lwdaMemset2D, ::lwdaMemset3D,
 * ::lwdaMemsetAsync, ::lwdaMemset2DAsync,
 * ::lwdaMalloc3D, ::make_lwdaPitchedPtr,
 * ::make_lwdaExtent
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset3DAsync(struct lwdaPitchedPtr pitchedDevPtr, int value, struct lwdaExtent extent, lwdaStream_t stream __dv(0));

/**
 * \brief Finds the address associated with a LWCA symbol
 *
 * Returns in \p *devPtr the address of symbol \p symbol on the device.
 * \p symbol is a variable that resides in global or constant memory space.
 * If \p symbol cannot be found, or if \p symbol is not declared in the
 * global or constant memory space, \p *devPtr is unchanged and the error
 * ::lwdaErrorIlwalidSymbol is returned.
 *
 * \param devPtr - Return device pointer associated with symbol
 * \param symbol - Device symbol address
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorNoKernelImageForDevice
 * \notefnerr
 * \note_string_api_deprecation
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * \ref ::lwdaGetSymbolAddress(void**, const T&) "lwdaGetSymbolAddress (C++ API)",
 * \ref ::lwdaGetSymbolSize(size_t*, const void*) "lwdaGetSymbolSize (C API)",
 * ::lwModuleGetGlobal
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetSymbolAddress(void **devPtr, const void *symbol);

/**
 * \brief Finds the size of the object associated with a LWCA symbol
 *
 * Returns in \p *size the size of symbol \p symbol. \p symbol is a variable that
 * resides in global or constant memory space. If \p symbol cannot be found, or
 * if \p symbol is not declared in global or constant memory space, \p *size is
 * unchanged and the error ::lwdaErrorIlwalidSymbol is returned.
 *
 * \param size   - Size of object associated with symbol
 * \param symbol - Device symbol address
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidSymbol,
 * ::lwdaErrorNoKernelImageForDevice
 * \notefnerr
 * \note_string_api_deprecation
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * \ref ::lwdaGetSymbolAddress(void**, const void*) "lwdaGetSymbolAddress (C API)",
 * \ref ::lwdaGetSymbolSize(size_t*, const T&) "lwdaGetSymbolSize (C++ API)",
 * ::lwModuleGetGlobal
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetSymbolSize(size_t *size, const void *symbol);

/**
 * \brief Prefetches memory to the specified destination device
 *
 * Prefetches memory to the specified destination device.  \p devPtr is the 
 * base device pointer of the memory to be prefetched and \p dstDevice is the 
 * destination device. \p count specifies the number of bytes to copy. \p stream
 * is the stream in which the operation is enqueued. The memory range must refer
 * to managed memory allocated via ::lwdaMallocManaged or declared via __managed__ variables.
 *
 * Passing in lwdaCpuDeviceId for \p dstDevice will prefetch the data to host memory. If
 * \p dstDevice is a GPU, then the device attribute ::lwdaDevAttrConlwrrentManagedAccess
 * must be non-zero. Additionally, \p stream must be associated with a device that has a
 * non-zero value for the device attribute ::lwdaDevAttrConlwrrentManagedAccess.
 *
 * The start address and end address of the memory range will be rounded down and rounded up
 * respectively to be aligned to CPU page size before the prefetch operation is enqueued
 * in the stream.
 *
 * If no physical memory has been allocated for this region, then this memory region
 * will be populated and mapped on the destination device. If there's insufficient
 * memory to prefetch the desired region, the Unified Memory driver may evict pages from other
 * ::lwdaMallocManaged allocations to host memory in order to make room. Device memory
 * allocated using ::lwdaMalloc or ::lwdaMallocArray will not be evicted.
 *
 * By default, any mappings to the previous location of the migrated pages are removed and
 * mappings for the new location are only setup on \p dstDevice. The exact behavior however
 * also depends on the settings applied to this memory range via ::lwdaMemAdvise as described
 * below:
 *
 * If ::lwdaMemAdviseSetReadMostly was set on any subset of this memory range,
 * then that subset will create a read-only copy of the pages on \p dstDevice.
 *
 * If ::lwdaMemAdviseSetPreferredLocation was called on any subset of this memory
 * range, then the pages will be migrated to \p dstDevice even if \p dstDevice is not the
 * preferred location of any pages in the memory range.
 *
 * If ::lwdaMemAdviseSetAccessedBy was called on any subset of this memory range,
 * then mappings to those pages from all the appropriate processors are updated to
 * refer to the new location if establishing such a mapping is possible. Otherwise,
 * those mappings are cleared.
 *
 * Note that this API is not required for functionality and only serves to improve performance
 * by allowing the application to migrate data to a suitable location before it is accessed.
 * Memory accesses to this range are always coherent and are allowed even when the data is
 * actively being migrated.
 *
 * Note that this function is asynchronous with respect to the host and all work
 * on other devices.
 *
 * \param devPtr    - Pointer to be prefetched
 * \param count     - Size in bytes
 * \param dstDevice - Destination device to prefetch to
 * \param stream    - Stream to enqueue prefetch operation
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpyPeer, ::lwdaMemcpyAsync,
 * ::lwdaMemcpy3DPeerAsync, ::lwdaMemAdvise,
 * ::lwMemPrefetchAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPrefetchAsync(const void *devPtr, size_t count, int dstDevice, lwdaStream_t stream __dv(0));

/**
 * \brief Advise about the usage of a given memory range
 *
 * Advise the Unified Memory subsystem about the usage pattern for the memory range
 * starting at \p devPtr with a size of \p count bytes. The start address and end address of the memory
 * range will be rounded down and rounded up respectively to be aligned to CPU page size before the
 * advice is applied. The memory range must refer to managed memory allocated via ::lwdaMallocManaged
 * or declared via __managed__ variables. The memory range could also refer to system-allocated pageable
 * memory provided it represents a valid, host-accessible region of memory and all additional constraints
 * imposed by \p advice as outlined below are also satisfied. Specifying an invalid system-allocated pageable
 * memory range results in an error being returned.
 *
 * The \p advice parameter can take the following values:
 * - ::lwdaMemAdviseSetReadMostly: This implies that the data is mostly going to be read
 * from and only occasionally written to. Any read accesses from any processor to this region will create a
 * read-only copy of at least the accessed pages in that processor's memory. Additionally, if ::lwdaMemPrefetchAsync
 * is called on this region, it will create a read-only copy of the data on the destination processor.
 * If any processor writes to this region, all copies of the corresponding page will be ilwalidated
 * except for the one where the write oclwrred. The \p device argument is ignored for this advice.
 * Note that for a page to be read-duplicated, the accessing processor must either be the CPU or a GPU
 * that has a non-zero value for the device attribute ::lwdaDevAttrConlwrrentManagedAccess.
 * Also, if a context is created on a device that does not have the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess set, then read-duplication will not occur until
 * all such contexts are destroyed.
 * If the memory region refers to valid system-allocated pageable memory, then the accessing device must
 * have a non-zero value for the device attribute ::lwdaDevAttrPageableMemoryAccess for a read-only
 * copy to be created on that device. Note however that if the accessing device also has a non-zero value for the
 * device attribute ::lwdaDevAttrPageableMemoryAccessUsesHostPageTables, then setting this advice
 * will not create a read-only copy when that device accesses this memory region.
 *
 * - ::lwdaMemAdviceUnsetReadMostly: Undoes the effect of ::lwdaMemAdviceReadMostly and also prevents the
 * Unified Memory driver from attempting heuristic read-duplication on the memory range. Any read-duplicated
 * copies of the data will be collapsed into a single copy. The location for the collapsed
 * copy will be the preferred location if the page has a preferred location and one of the read-duplicated
 * copies was resident at that location. Otherwise, the location chosen is arbitrary.
 *
 * - ::lwdaMemAdviseSetPreferredLocation: This advice sets the preferred location for the
 * data to be the memory belonging to \p device. Passing in lwdaCpuDeviceId for \p device sets the
 * preferred location as host memory. If \p device is a GPU, then it must have a non-zero value for the
 * device attribute ::lwdaDevAttrConlwrrentManagedAccess. Setting the preferred location
 * does not cause data to migrate to that location immediately. Instead, it guides the migration policy
 * when a fault oclwrs on that memory region. If the data is already in its preferred location and the
 * faulting processor can establish a mapping without requiring the data to be migrated, then
 * data migration will be avoided. On the other hand, if the data is not in its preferred location
 * or if a direct mapping cannot be established, then it will be migrated to the processor accessing
 * it. It is important to note that setting the preferred location does not prevent data prefetching
 * done using ::lwdaMemPrefetchAsync.
 * Having a preferred location can override the page thrash detection and resolution logic in the Unified
 * Memory driver. Normally, if a page is detected to be constantly thrashing between for example host and device
 * memory, the page may eventually be pinned to host memory by the Unified Memory driver. But
 * if the preferred location is set as device memory, then the page will continue to thrash indefinitely.
 * If ::lwdaMemAdviseSetReadMostly is also set on this memory region or any subset of it, then the
 * policies associated with that advice will override the policies of this advice, unless read accesses from
 * \p device will not result in a read-only copy being created on that device as outlined in description for
 * the advice ::lwdaMemAdviseSetReadMostly.
 * If the memory region refers to valid system-allocated pageable memory, then \p device must have a non-zero
 * value for the device attribute ::lwdaDevAttrPageableMemoryAccess. Additionally, if \p device has
 * a non-zero value for the device attribute ::lwdaDevAttrPageableMemoryAccessUsesHostPageTables,
 * then this call has no effect. Note however that this behavior may change in the future.
 *
 * - ::lwdaMemAdviseUnsetPreferredLocation: Undoes the effect of ::lwdaMemAdviseSetPreferredLocation
 * and changes the preferred location to none.
 *
 * - ::lwdaMemAdviseSetAccessedBy: This advice implies that the data will be accessed by \p device.
 * Passing in ::lwdaCpuDeviceId for \p device will set the advice for the CPU. If \p device is a GPU, then
 * the device attribute ::lwdaDevAttrConlwrrentManagedAccess must be non-zero.
 * This advice does not cause data migration and has no impact on the location of the data per se. Instead,
 * it causes the data to always be mapped in the specified processor's page tables, as long as the
 * location of the data permits a mapping to be established. If the data gets migrated for any reason,
 * the mappings are updated accordingly.
 * This advice is recommended in scenarios where data locality is not important, but avoiding faults is.
 * Consider for example a system containing multiple GPUs with peer-to-peer access enabled, where the
 * data located on one GPU is occasionally accessed by peer GPUs. In such scenarios, migrating data
 * over to the other GPUs is not as important because the accesses are infrequent and the overhead of
 * migration may be too high. But preventing faults can still help improve performance, and so having
 * a mapping set up in advance is useful. Note that on CPU access of this data, the data may be migrated
 * to host memory because the CPU typically cannot access device memory directly. Any GPU that had the
 * ::lwdaMemAdviceSetAccessedBy flag set for this data will now have its mapping updated to point to the
 * page in host memory.
 * If ::lwdaMemAdviseSetReadMostly is also set on this memory region or any subset of it, then the
 * policies associated with that advice will override the policies of this advice. Additionally, if the
 * preferred location of this memory region or any subset of it is also \p device, then the policies
 * associated with ::lwdaMemAdviseSetPreferredLocation will override the policies of this advice.
 * If the memory region refers to valid system-allocated pageable memory, then \p device must have a non-zero
 * value for the device attribute ::lwdaDevAttrPageableMemoryAccess. Additionally, if \p device has
 * a non-zero value for the device attribute ::lwdaDevAttrPageableMemoryAccessUsesHostPageTables,
 * then this call has no effect.
 *
 * - ::lwdaMemAdviseUnsetAccessedBy: Undoes the effect of ::lwdaMemAdviseSetAccessedBy. Any mappings to
 * the data from \p device may be removed at any time causing accesses to result in non-fatal page faults.
 * If the memory region refers to valid system-allocated pageable memory, then \p device must have a non-zero
 * value for the device attribute ::lwdaDevAttrPageableMemoryAccess. Additionally, if \p device has
 * a non-zero value for the device attribute ::lwdaDevAttrPageableMemoryAccessUsesHostPageTables,
 * then this call has no effect.
 *
 * \param devPtr - Pointer to memory to set the advice for
 * \param count  - Size in bytes of the memory range
 * \param advice - Advice to be applied for the specified memory range
 * \param device - Device to apply the advice for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpyPeer, ::lwdaMemcpyAsync,
 * ::lwdaMemcpy3DPeerAsync, ::lwdaMemPrefetchAsync,
 * ::lwMemAdvise
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemAdvise(const void *devPtr, size_t count, enum lwdaMemoryAdvise advice, int device);

/**
* \brief Query an attribute of a given memory range
*
* Query an attribute about the memory range starting at \p devPtr with a size of \p count bytes. The
* memory range must refer to managed memory allocated via ::lwdaMallocManaged or declared via
* __managed__ variables.
*
* The \p attribute parameter can take the following values:
* - ::lwdaMemRangeAttributeReadMostly: If this attribute is specified, \p data will be interpreted
* as a 32-bit integer, and \p dataSize must be 4. The result returned will be 1 if all pages in the given
* memory range have read-duplication enabled, or 0 otherwise.
* - ::lwdaMemRangeAttributePreferredLocation: If this attribute is specified, \p data will be
* interpreted as a 32-bit integer, and \p dataSize must be 4. The result returned will be a GPU device
* id if all pages in the memory range have that GPU as their preferred location, or it will be lwdaCpuDeviceId
* if all pages in the memory range have the CPU as their preferred location, or it will be lwdaIlwalidDeviceId
* if either all the pages don't have the same preferred location or some of the pages don't have a
* preferred location at all. Note that the actual location of the pages in the memory range at the time of
* the query may be different from the preferred location.
* - ::lwdaMemRangeAttributeAccessedBy: If this attribute is specified, \p data will be interpreted
* as an array of 32-bit integers, and \p dataSize must be a non-zero multiple of 4. The result returned
* will be a list of device ids that had ::lwdaMemAdviceSetAccessedBy set for that entire memory range.
* If any device does not have that advice set for the entire memory range, that device will not be included.
* If \p data is larger than the number of devices that have that advice set for that memory range,
* lwdaIlwalidDeviceId will be returned in all the extra space provided. For ex., if \p dataSize is 12
* (i.e. \p data has 3 elements) and only device 0 has the advice set, then the result returned will be
* { 0, lwdaIlwalidDeviceId, lwdaIlwalidDeviceId }. If \p data is smaller than the number of devices that have
* that advice set, then only as many devices will be returned as can fit in the array. There is no
* guarantee on which specific devices will be returned, however.
* - ::lwdaMemRangeAttributeLastPrefetchLocation: If this attribute is specified, \p data will be
* interpreted as a 32-bit integer, and \p dataSize must be 4. The result returned will be the last location
* to which all pages in the memory range were prefetched explicitly via ::lwdaMemPrefetchAsync. This will either be
* a GPU id or lwdaCpuDeviceId depending on whether the last location for prefetch was a GPU or the CPU
* respectively. If any page in the memory range was never explicitly prefetched or if all pages were not
* prefetched to the same location, lwdaIlwalidDeviceId will be returned. Note that this simply returns the
* last location that the applicaton requested to prefetch the memory range to. It gives no indication as to
* whether the prefetch operation to that location has completed or even begun.
*
* \param data      - A pointers to a memory location where the result
*                    of each attribute query will be written to.
* \param dataSize  - Array containing the size of data
* \param attribute - The attribute to query
* \param devPtr    - Start of the range to query
* \param count     - Size of the range to query
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemRangeGetAttributes, ::lwdaMemPrefetchAsync,
 * ::lwdaMemAdvise,
 * ::lwMemRangeGetAttribute
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemRangeGetAttribute(void *data, size_t dataSize, enum lwdaMemRangeAttribute attribute, const void *devPtr, size_t count);

/**
 * \brief Query attributes of a given memory range.
 *
 * Query attributes of the memory range starting at \p devPtr with a size of \p count bytes. The
 * memory range must refer to managed memory allocated via ::lwdaMallocManaged or declared via
 * __managed__ variables. The \p attributes array will be interpreted to have \p numAttributes
 * entries. The \p dataSizes array will also be interpreted to have \p numAttributes entries.
 * The results of the query will be stored in \p data.
 *
 * The list of supported attributes are given below. Please refer to ::lwdaMemRangeGetAttribute for
 * attribute descriptions and restrictions.
 *
 * - ::lwdaMemRangeAttributeReadMostly
 * - ::lwdaMemRangeAttributePreferredLocation
 * - ::lwdaMemRangeAttributeAccessedBy
 * - ::lwdaMemRangeAttributeLastPrefetchLocation
 *
 * \param data          - A two-dimensional array containing pointers to memory
 *                        locations where the result of each attribute query will be written to.
 * \param dataSizes     - Array containing the sizes of each result
 * \param attributes    - An array of attributes to query
 *                        (numAttributes and the number of attributes in this array should match)
 * \param numAttributes - Number of attributes to query
 * \param devPtr        - Start of the range to query
 * \param count         - Size of the range to query
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemRangeGetAttribute, ::lwdaMemAdvise,
 * ::lwdaMemPrefetchAsync,
 * ::lwMemRangeGetAttributes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemRangeGetAttributes(void **data, size_t *dataSizes, enum lwdaMemRangeAttribute *attributes, size_t numAttributes, const void *devPtr, size_t count);

/** @} */ /* END LWDART_MEMORY */

/**
 * \defgroup LWDART_MEMORY_DEPRECATED Memory Management [DEPRECATED]
 *
 * ___MANBRIEF___ deprecated memory management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes deprecated memory management functions of the LWCA runtime
 * application programming interface.
 *
 * Some functions have overloaded C++ API template versions dolwmented separately in the
 * \ref LWDART_HIGHLEVEL "C++ API Routines" module.
 *
 * @{
 */

/**
 * \brief Copies data between host and device
 *
 * \deprecated
 *
 * Copies \p count bytes from the memory area pointed to by \p src to the
 * LWCA array \p dst starting at \p hOffset rows and \p wOffset bytes from
 * the upper left corner, where \p kind specifies the direction
 * of the copy, and must be one of ::lwdaMemcpyHostToHost,
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * \param dst     - Destination memory address
 * \param wOffset - Destination starting X offset (columns in bytes)
 * \param hOffset - Destination starting Y offset (rows)
 * \param src     - Source memory address
 * \param count   - Size in bytes to copy
 * \param kind    - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpyFromArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpyArrayToArray, ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpyToArrayAsync, ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpyFromArrayAsync, ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyHtoA,
 * ::lwMemcpyDtoA
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaMemcpyToArray(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t count, enum lwdaMemcpyKind kind);

/**
 * \brief Copies data between host and device
 *
 * \deprecated
 *
 * Copies \p count bytes from the LWCA array \p src starting at \p hOffset rows
 * and \p wOffset bytes from the upper left corner to the memory area pointed to
 * by \p dst, where \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * \param dst     - Destination memory address
 * \param src     - Source memory address
 * \param wOffset - Source starting X offset (columns in bytes)
 * \param hOffset - Source starting Y offset (rows)
 * \param count   - Size in bytes to copy
 * \param kind    - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_sync
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D, ::lwdaMemcpyToArray,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpyArrayToArray, ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpyToArrayAsync, ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpyFromArrayAsync, ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyAtoH,
 * ::lwMemcpyAtoD
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromArray(void *dst, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t count, enum lwdaMemcpyKind kind);

/**
 * \brief Copies data between host and device
 *
 * \deprecated
 *
 * Copies \p count bytes from the LWCA array \p src starting at \p hOffsetSrc
 * rows and \p wOffsetSrc bytes from the upper left corner to the LWCA array
 * \p dst starting at \p hOffsetDst rows and \p wOffsetDst bytes from the upper
 * left corner, where \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * \param dst        - Destination memory address
 * \param wOffsetDst - Destination starting X offset (columns in bytes)
 * \param hOffsetDst - Destination starting Y offset (rows)
 * \param src        - Source memory address
 * \param wOffsetSrc - Source starting X offset (columns in bytes)
 * \param hOffsetSrc - Source starting Y offset (rows)
 * \param count      - Size in bytes to copy
 * \param kind       - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D, ::lwdaMemcpyToArray,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpyFromArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpyToArrayAsync, ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpyFromArrayAsync, ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyAtoA
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaMemcpyArrayToArray(lwdaArray_t dst, size_t wOffsetDst, size_t hOffsetDst, lwdaArray_const_t src, size_t wOffsetSrc, size_t hOffsetSrc, size_t count, enum lwdaMemcpyKind kind __dv(lwdaMemcpyDeviceToDevice));

/**
 * \brief Copies data between host and device
 *
 * \deprecated
 *
 * Copies \p count bytes from the memory area pointed to by \p src to the
 * LWCA array \p dst starting at \p hOffset rows and \p wOffset bytes from
 * the upper left corner, where \p kind specifies the
 * direction of the copy, and must be one of ::lwdaMemcpyHostToHost,
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * ::lwdaMemcpyToArrayAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally
 * be associated to a stream by passing a non-zero \p stream argument. If \p
 * kind is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and \p stream
 * is non-zero, the copy may overlap with operations in other streams.
 *
 * \param dst     - Destination memory address
 * \param wOffset - Destination starting X offset (columns in bytes)
 * \param hOffset - Destination starting Y offset (rows)
 * \param src     - Source memory address
 * \param count   - Size in bytes to copy
 * \param kind    - Type of transfer
 * \param stream  - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D, ::lwdaMemcpyToArray,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpyFromArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpyArrayToArray, ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpyFromArrayAsync, ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyHtoAAsync,
 * ::lwMemcpy2DAsync
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaMemcpyToArrayAsync(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/**
 * \brief Copies data between host and device
 *
 * \deprecated
 *
 * Copies \p count bytes from the LWCA array \p src starting at \p hOffset rows
 * and \p wOffset bytes from the upper left corner to the memory area pointed to
 * by \p dst, where \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * ::lwdaMemcpyFromArrayAsync() is asynchronous with respect to the host, so
 * the call may return before the copy is complete. The copy can optionally
 * be associated to a stream by passing a non-zero \p stream argument. If \p
 * kind is ::lwdaMemcpyHostToDevice or ::lwdaMemcpyDeviceToHost and \p stream
 * is non-zero, the copy may overlap with operations in other streams.
 *
 * \param dst     - Destination memory address
 * \param src     - Source memory address
 * \param wOffset - Source starting X offset (columns in bytes)
 * \param hOffset - Source starting Y offset (rows)
 * \param count   - Size in bytes to copy
 * \param kind    - Type of transfer
 * \param stream  - Stream identifier
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidMemcpyDirection
 * \notefnerr
 * \note_async
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaMemcpy, ::lwdaMemcpy2D, ::lwdaMemcpyToArray,
 * ::lwdaMemcpy2DToArray, ::lwdaMemcpyFromArray, ::lwdaMemcpy2DFromArray,
 * ::lwdaMemcpyArrayToArray, ::lwdaMemcpy2DArrayToArray, ::lwdaMemcpyToSymbol,
 * ::lwdaMemcpyFromSymbol, ::lwdaMemcpyAsync, ::lwdaMemcpy2DAsync,
 * ::lwdaMemcpyToArrayAsync, ::lwdaMemcpy2DToArrayAsync,
 * ::lwdaMemcpy2DFromArrayAsync,
 * ::lwdaMemcpyToSymbolAsync, ::lwdaMemcpyFromSymbolAsync,
 * ::lwMemcpyAtoHAsync,
 * ::lwMemcpy2DAsync
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromArrayAsync(void *dst, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));

/** @} */ /* END LWDART_MEMORY_DEPRECATED */

/**
 * \defgroup LWDART_MEMORY_POOLS Stream Ordered Memory Allocator 
 *
 * ___MANBRIEF___ Functions for performing allocation and free operations in stream order.
 *                Functions for controlling the behavior of the underlying allocator.
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 * 
 *
 * @{
 *
 * \section LWDART_MEMORY_POOLS_overview overview
 *
 * The asynchronous allocator allows the user to allocate and free in stream order.
 * All asynchronous accesses of the allocation must happen between
 * the stream exelwtions of the allocation and the free. If the memory is accessed
 * outside of the promised stream order, a use before allocation / use after free error
 * will cause undefined behavior.
 *
 * The allocator is free to reallocate the memory as long as it can guarantee
 * that compliant memory accesses will not overlap temporally.
 * The allocator may refer to internal stream ordering as well as inter-stream dependencies
 * (such as LWCA events and null stream dependencies) when establishing the temporal guarantee.
 * The allocator may also insert inter-stream dependencies to establish the temporal guarantee.
 *
 * \section LWDART_MEMORY_POOLS_support Supported Platforms
 *
 * Whether or not a device supports the integrated stream ordered memory allocator
 * may be queried by calling ::lwdaDeviceGetAttribute() with the device attribute
 * ::lwdaDevAttrMemoryPoolsSupported.
 */

/**
 * \brief Allocates memory with stream ordered semantics
 *
 * Inserts an allocation operation into \p hStream.
 * A pointer to the allocated memory is returned immediately in *dptr.
 * The allocation must not be accessed until the the allocation operation completes.
 * The allocation comes from the memory pool associated with the stream's device.
 *
 * \note The default memory pool of a device contains device memory from that device.
 * \note Basic stream ordering allows future work submitted into the same stream to use the allocation.
 *       Stream query, stream synchronize, and LWCA events can be used to guarantee that the allocation
 *       operation completes before work submitted in a separate stream runs.
 * \note During stream capture, this function results in the creation of an allocation node.  In this case,
 *       the allocation is owned by the graph instead of the memory pool. The memory pool's properties
 *       are used to set the node's creation parameters.
 *
 * \param[out] devPtr  - Returned device pointer
 * \param[in] size     - Number of bytes to allocate
 * \param[in] hStream  - The stream establishing the stream ordering contract and the memory pool to allocate from
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorOutOfMemory,
 * \notefnerr
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwMemAllocAsync,
 * \ref ::lwdaMallocAsync(void** ptr, size_t size, lwdaMemPool_t memPool, lwdaStream_t stream)  "lwdaMallocAsync (C++ API)", 
 * ::lwdaMallocFromPoolAsync, ::lwdaFreeAsync, ::lwdaDeviceSetMemPool, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceGetMemPool, ::lwdaMemPoolSetAccess, ::lwdaMemPoolSetAttribute, ::lwdaMemPoolGetAttribute
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMallocAsync(void **devPtr, size_t size, lwdaStream_t hStream);

/**
 * \brief Frees memory with stream ordered semantics
 *
 * Inserts a free operation into \p hStream.
 * The allocation must not be accessed after stream exelwtion reaches the free.
 * After this API returns, accessing the memory from any subsequent work launched on the GPU
 * or querying its pointer attributes results in undefined behavior.
 *
 * \note During stream capture, this function results in the creation of a free node and
 *       must therefore be passed the address of a graph allocation.
 *
 * \param dptr - memory to free
 * \param hStream - The stream establishing the stream ordering promise
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported
 * \notefnerr
 * \note_null_stream
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwMemFreeAsync, ::lwdaMallocAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaFreeAsync(void *devPtr, lwdaStream_t hStream);

/**
 * \brief Tries to release memory back to the OS
 *
 * Releases memory back to the OS until the pool contains fewer than minBytesToKeep
 * reserved bytes, or there is no more memory that the allocator can safely release.
 * The allocator cannot release OS allocations that back outstanding asynchronous allocations.
 * The OS allocations may happen at different granularity from the user allocations.
 *
 * \note: Allocations that have not been freed count as outstanding.
 * \note: Allocations that have been asynchronously freed but whose completion has
 *        not been observed on the host (eg. by a synchronize) can count as outstanding.
 *
 * \param[in] pool           - The memory pool to trim
 * \param[in] minBytesToKeep - If the pool has less than minBytesToKeep reserved,
 * the TrimTo operation is a no-op.  Otherwise the pool will be guaranteed to have
 * at least minBytesToKeep bytes reserved after the operation.
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_callback
 *
 * \sa ::lwMemPoolTrimTo, ::lwdaMallocAsync, ::lwdaFreeAsync, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceGetMemPool, ::lwdaMemPoolCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolTrimTo(lwdaMemPool_t memPool, size_t minBytesToKeep);

/**
 * \brief Sets attributes of a memory pool
 *
 * Supported attributes are:
 * - ::lwdaMemPoolAttrReleaseThreshold: (value type = lwuint64_t)
 *                    Amount of reserved memory in bytes to hold onto before trying
 *                    to release memory back to the OS. When more than the release
 *                    threshold bytes of memory are held by the memory pool, the
 *                    allocator will try to release memory back to the OS on the
 *                    next call to stream, event or context synchronize. (default 0)
 * - ::lwdaMemPoolReuseFollowEventDependencies: (value type = int)
 *                    Allow ::lwdaMallocAsync to use memory asynchronously freed
 *                    in another stream as long as a stream ordering dependency
 *                    of the allocating stream on the free action exists.
 *                    Lwca events and null stream interactions can create the required
 *                    stream ordered dependencies. (default enabled)
 * - ::lwdaMemPoolReuseAllowOpportunistic: (value type = int)
 *                    Allow reuse of already completed frees when there is no dependency
 *                    between the free and allocation. (default enabled)
 * - ::lwdaMemPoolReuseAllowInternalDependencies: (value type = int)
 *                    Allow ::lwdaMallocAsync to insert new stream dependencies
 *                    in order to establish the stream ordering required to reuse
 *                    a piece of memory released by ::lwdaFreeAsync (default enabled).
 * - ::lwdaMemPoolAttrReservedMemHigh: (value type = lwuint64_t)
 *                    Reset the high watermark that tracks the amount of backing memory that was
 *                    allocated for the memory pool. It is illegal to set this attribute to a non-zero value.
 * - ::lwdaMemPoolAttrUsedMemHigh: (value type = lwuint64_t)
 *                    Reset the high watermark that tracks the amount of used memory that was
 *                    allocated for the memory pool. It is illegal to set this attribute to a non-zero value.
 *
 * \param[in] pool  - The memory pool to modify
 * \param[in] attr  - The attribute to modify
 * \param[in] value - Pointer to the value to assign
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_callback
 *
 * \sa ::lwMemPoolSetAttribute, ::lwdaMallocAsync, ::lwdaFreeAsync, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceGetMemPool, ::lwdaMemPoolCreate

 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolSetAttribute(lwdaMemPool_t memPool, enum lwdaMemPoolAttr attr, void *value );

/**
 * \brief Gets attributes of a memory pool
 *
 * Supported attributes are:
 * - ::lwdaMemPoolAttrReleaseThreshold: (value type = lwuint64_t)
 *                    Amount of reserved memory in bytes to hold onto before trying
 *                    to release memory back to the OS. When more than the release
 *                    threshold bytes of memory are held by the memory pool, the
 *                    allocator will try to release memory back to the OS on the
 *                    next call to stream, event or context synchronize. (default 0)
 * - ::lwdaMemPoolReuseFollowEventDependencies: (value type = int)
 *                    Allow ::lwdaMallocAsync to use memory asynchronously freed
 *                    in another stream as long as a stream ordering dependency
 *                    of the allocating stream on the free action exists.
 *                    Lwca events and null stream interactions can create the required
 *                    stream ordered dependencies. (default enabled)
 * - ::lwdaMemPoolReuseAllowOpportunistic: (value type = int)
 *                    Allow reuse of already completed frees when there is no dependency
 *                    between the free and allocation. (default enabled)
 * - ::lwdaMemPoolReuseAllowInternalDependencies: (value type = int)
 *                    Allow ::lwdaMallocAsync to insert new stream dependencies
 *                    in order to establish the stream ordering required to reuse
 *                    a piece of memory released by ::lwdaFreeAsync (default enabled).
 * - ::lwdaMemPoolAttrReservedMemLwrrent: (value type = lwuint64_t)
 *                    Amount of backing memory lwrrently allocated for the mempool.
 * - ::lwdaMemPoolAttrReservedMemHigh: (value type = lwuint64_t)
 *                    High watermark of backing memory allocated for the mempool since
 *                    the last time it was reset.
 * - ::lwdaMemPoolAttrUsedMemLwrrent: (value type = lwuint64_t)
 *                    Amount of memory from the pool that is lwrrently in use by the application.
 * - ::lwdaMemPoolAttrUsedMemHigh: (value type = lwuint64_t)
 *                    High watermark of the amount of memory from the pool that was in use by the
 *                    application since the last time it was reset.
 *
 * \param[in] pool  - The memory pool to get attributes of 
 * \param[in] attr  - The attribute to get
 * \param[in] value - Retrieved value 
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_callback
 *
 * \sa ::lwMemPoolGetAttribute, ::lwdaMallocAsync, ::lwdaFreeAsync, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceGetMemPool, ::lwdaMemPoolCreate

 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolGetAttribute(lwdaMemPool_t memPool, enum lwdaMemPoolAttr attr, void *value );

/**
 * \brief Controls visibility of pools between devices
 *
 * \param[in] pool  - The pool being modified
 * \param[in] map   - Array of access descriptors. Each descriptor instructs the access to enable for a single gpu
 * \param[in] count - Number of descriptors in the map array.
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa ::lwMemPoolSetAccess, ::lwdaMemPoolGetAccess, ::lwdaMallocAsync, lwdaFreeAsync
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolSetAccess(lwdaMemPool_t memPool, const struct lwdaMemAccessDesc *descList, size_t count);

/**
 * \brief Returns the accessibility of a pool from a device
 *
 * Returns the accessibility of the pool's memory from the specified location.
 *
 * \param[out] flags   - the accessibility of the pool from the specified location
 * \param[in] memPool  - the pool being queried
 * \param[in] location - the location accessing the pool
 *
 * \sa ::lwMemPoolGetAccess, ::lwdaMemPoolSetAccess
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolGetAccess(enum lwdaMemAccessFlags *flags, lwdaMemPool_t memPool, struct lwdaMemLocation *location);

/**
 * \brief Creates a memory pool
 *
 * Creates a LWCA memory pool and returns the handle in \p pool.  The \p poolProps determines
 * the properties of the pool such as the backing device and IPC capabilities.
 *
 * By default, the pool's memory will be accessible from the device it is allocated on.
 *
 * \note Specifying lwdaMemHandleTypeNone creates a memory pool that will not support IPC.
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported
 *
 * \sa ::lwMemPoolCreate, ::lwdaDeviceSetMemPool, ::lwdaMallocFromPoolAsync, ::lwdaMemPoolExportToShareableHandle, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceGetMemPool

 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolCreate(lwdaMemPool_t *memPool, const struct lwdaMemPoolProps *poolProps);

/**
 * \brief Destroys the specified memory pool 
 *
 * If any pointers obtained from this pool haven't been freed or
 * the pool has free operations that haven't completed
 * when ::lwdaMemPoolDestroy is ilwoked, the function will return immediately and the
 * resources associated with the pool will be released automatically
 * once there are no more outstanding allocations.
 *
 * Destroying the current mempool of a device sets the default mempool of
 * that device as the current mempool for that device.
 *
 * \note A device's default memory pool cannot be destroyed.
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa lwMemPoolDestroy, ::lwdaFreeAsync, ::lwdaDeviceSetMemPool, ::lwdaDeviceGetDefaultMemPool, ::lwdaDeviceGetMemPool, ::lwdaMemPoolCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolDestroy(lwdaMemPool_t memPool);

/**
 * \brief Allocates memory from a specified pool with stream ordered semantics.
 *
 * Inserts an allocation operation into \p hStream.
 * A pointer to the allocated memory is returned immediately in *dptr.
 * The allocation must not be accessed until the the allocation operation completes.
 * The allocation comes from the specified memory pool.
 *
 * \note
 *    -  The specified memory pool may be from a device different than that of the specified \p hStream.
 *
 *    -  Basic stream ordering allows future work submitted into the same stream to use the allocation.
 *       Stream query, stream synchronize, and LWCA events can be used to guarantee that the allocation
 *       operation completes before work submitted in a separate stream runs.
 *
 * \note During stream capture, this function results in the creation of an allocation node.  In this case,
 *       the allocation is owned by the graph instead of the memory pool. The memory pool's properties
 *       are used to set the node's creation parameters.
 *
 * \param[out] ptr     - Returned device pointer
 * \param[in] bytesize - Number of bytes to allocate
 * \param[in] memPool  - The pool to allocate from
 * \param[in] stream   - The stream establishing the stream ordering semantic
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorOutOfMemory
 *
 * \sa ::lwMemAllocFromPoolAsync,
 * \ref ::lwdaMallocAsync(void** ptr, size_t size, lwdaMemPool_t memPool, lwdaStream_t stream)  "lwdaMallocAsync (C++ API)", 
 * ::lwdaMallocAsync, ::lwdaFreeAsync, ::lwdaDeviceGetDefaultMemPool, ::lwdaMemPoolCreate, ::lwdaMemPoolSetAccess, ::lwdaMemPoolSetAttribute
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMallocFromPoolAsync(void **ptr, size_t size, lwdaMemPool_t memPool, lwdaStream_t stream);

/**
 * \brief Exports a memory pool to the requested handle type.
 *
 * Given an IPC capable mempool, create an OS handle to share the pool with another process.
 * A recipient process can colwert the shareable handle into a mempool with ::lwdaMemPoolImportFromShareableHandle.
 * Individual pointers can then be shared with the ::lwdaMemPoolExportPointer and ::lwdaMemPoolImportPointer APIs.
 * The implementation of what the shareable handle is and how it can be transferred is defined by the requested
 * handle type.
 *
 * \note: To create an IPC capable mempool, create a mempool with a LWmemAllocationHandleType other than lwdaMemHandleTypeNone.
 *
 * \param[out] handle_out  - pointer to the location in which to store the requested handle 
 * \param[in] pool         - pool to export
 * \param[in] handleType   - the type of handle to create
 * \param[in] flags        - must be 0
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorOutOfMemory
 *
 * \sa ::lwMemPoolExportToShareableHandle, ::lwdaMemPoolImportFromShareableHandle, ::lwdaMemPoolExportPointer, ::lwdaMemPoolImportPointer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolExportToShareableHandle(
    void                            *shareableHandle,
    lwdaMemPool_t                    memPool,
    enum lwdaMemAllocationHandleType handleType,
    unsigned int                     flags);

/**
 * \brief imports a memory pool from a shared handle.
 *
 * Specific allocations can be imported from the imported pool with ::lwdaMemPoolImportPointer.
 *
 * \note Imported memory pools do not support creating new allocations.
 *       As such imported memory pools may not be used in ::lwdaDeviceSetMemPool
 *       or ::lwdaMallocFromPoolAsync calls.
 *
 * \param[out] pool_out    - Returned memory pool
 * \param[in] handle       - OS handle of the pool to open
 * \param[in] handleType   - The type of handle being imported
 * \param[in] flags        - must be 0
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorOutOfMemory
 *
 * \sa ::lwMemPoolImportFromShareableHandle, ::lwdaMemPoolExportToShareableHandle, ::lwdaMemPoolExportPointer, ::lwdaMemPoolImportPointer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolImportFromShareableHandle(
    lwdaMemPool_t                   *memPool,
    void                            *shareableHandle,
    enum lwdaMemAllocationHandleType handleType,
    unsigned int                     flags);

/**
 * \brief Export data to share a memory pool allocation between processes.
 *
 * Constructs \p shareData_out for sharing a specific allocation from an already shared memory pool.
 * The recipient process can import the allocation with the ::lwdaMemPoolImportPointer api.
 * The data is not a handle and may be shared through any IPC mechanism.
 *
 * \param[out] shareData_out - Returned export data
 * \param[in] ptr            - pointer to memory being exported
 *
 * \returns
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorOutOfMemory
 *
 * \sa ::lwMemPoolExportPointer, ::lwdaMemPoolExportToShareableHandle, ::lwdaMemPoolImportFromShareableHandle, ::lwdaMemPoolImportPointer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolExportPointer(struct lwdaMemPoolPtrExportData *exportData, void *ptr);

/**
 * \brief Import a memory pool allocation from another process.
 *
 * Returns in \p ptr_out a pointer to the imported memory.
 * The imported memory must not be accessed before the allocation operation completes
 * in the exporting process. The imported memory must be freed from all importing processes before
 * being freed in the exporting process. The pointer may be freed with lwdaFree
 * or lwdaFreeAsync.  If ::lwdaFreeAsync is used, the free must be completed
 * on the importing process before the free operation on the exporting process.
 *
 * \note The ::lwdaFreeAsync api may be used in the exporting process before
 *       the ::lwdaFreeAsync operation completes in its stream as long as the
 *       ::lwdaFreeAsync in the exporting process specifies a stream with
 *       a stream dependency on the importing process's ::lwdaFreeAsync.
 *
 * \param[out] ptr_out  - pointer to imported memory
 * \param[in] pool      - pool from which to import
 * \param[in] shareData - data specifying the memory to import
 *
 * \returns
 * ::LWDA_SUCCESS,
 * ::LWDA_ERROR_ILWALID_VALUE,
 * ::LWDA_ERROR_NOT_INITIALIZED,
 * ::LWDA_ERROR_OUT_OF_MEMORY
 *
 * \sa ::lwMemPoolImportPointer, ::lwdaMemPoolExportToShareableHandle, ::lwdaMemPoolImportFromShareableHandle, ::lwdaMemPoolExportPointer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaMemPoolImportPointer(void **ptr, lwdaMemPool_t memPool, struct lwdaMemPoolPtrExportData *exportData);

/** @} */ /* END LWDART_MEMORY_POOLS */

/**
 * \defgroup LWDART_UNIFIED Unified Addressing
 *
 * ___MANBRIEF___ unified addressing functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the unified addressing functions of the LWCA 
 * runtime application programming interface.
 *
 * @{
 *
 * \section LWDART_UNIFIED_overview Overview
 *
 * LWCA devices can share a unified address space with the host.  
 * For these devices there is no distinction between a device
 * pointer and a host pointer -- the same pointer value may be 
 * used to access memory from the host program and from a kernel 
 * running on the device (with exceptions enumerated below).
 *
 * \section LWDART_UNIFIED_support Supported Platforms
 * 
 * Whether or not a device supports unified addressing may be 
 * queried by calling ::lwdaGetDeviceProperties() with the device 
 * property ::lwdaDeviceProp::unifiedAddressing.
 *
 * Unified addressing is automatically enabled in 64-bit processes .
 *
 * \section LWDART_UNIFIED_lookup Looking Up Information from Pointer Values
 *
 * It is possible to look up information about the memory which backs a 
 * pointer value.  For instance, one may want to know if a pointer points
 * to host or device memory.  As another example, in the case of device 
 * memory, one may want to know on which LWCA device the memory 
 * resides.  These properties may be queried using the function 
 * ::lwdaPointerGetAttributes()
 *
 * Since pointers are unique, it is not necessary to specify information
 * about the pointers specified to ::lwdaMemcpy() and other copy functions.  
 * The copy direction ::lwdaMemcpyDefault may be used to specify that the 
 * LWCA runtime should infer the location of the pointer from its value.
 *
 * \section LWDART_UNIFIED_automaphost Automatic Mapping of Host Allocated Host Memory
 *
 * All host memory allocated through all devices using ::lwdaMallocHost() and
 * ::lwdaHostAlloc() is always directly accessible from all devices that 
 * support unified addressing.  This is the case regardless of whether or 
 * not the flags ::lwdaHostAllocPortable and ::lwdaHostAllocMapped are 
 * specified.
 *
 * The pointer value through which allocated host memory may be accessed 
 * in kernels on all devices that support unified addressing is the same 
 * as the pointer value through which that memory is accessed on the host.
 * It is not necessary to call ::lwdaHostGetDevicePointer() to get the device 
 * pointer for these allocations.  
 *
 * Note that this is not the case for memory allocated using the flag
 * ::lwdaHostAllocWriteCombined, as dislwssed below.
 *
 * \section LWDART_UNIFIED_autopeerregister Direct Access of Peer Memory
 
 * Upon enabling direct access from a device that supports unified addressing 
 * to another peer device that supports unified addressing using 
 * ::lwdaDeviceEnablePeerAccess() all memory allocated in the peer device using 
 * ::lwdaMalloc() and ::lwdaMallocPitch() will immediately be accessible 
 * by the current device.  The device pointer value through 
 * which any peer's memory may be accessed in the current device 
 * is the same pointer value through which that memory may be 
 * accessed from the peer device. 
 *
 * \section LWDART_UNIFIED_exceptions Exceptions, Disjoint Addressing
 * 
 * Not all memory may be accessed on devices through the same pointer
 * value through which they are accessed on the host.  These exceptions
 * are host memory registered using ::lwdaHostRegister() and host memory
 * allocated using the flag ::lwdaHostAllocWriteCombined.  For these 
 * exceptions, there exists a distinct host and device address for the
 * memory.  The device address is guaranteed to not overlap any valid host
 * pointer range and is guaranteed to have the same value across all devices
 * that support unified addressing.  
 * 
 * This device address may be queried using ::lwdaHostGetDevicePointer() 
 * when a device using unified addressing is current.  Either the host 
 * or the unified device pointer value may be used to refer to this memory 
 * in ::lwdaMemcpy() and similar functions using the ::lwdaMemcpyDefault 
 * memory direction.
 *
 */

/**
 * \brief Returns attributes about a specified pointer
 *
 * Returns in \p *attributes the attributes of the pointer \p ptr.
 * If pointer was not allocated in, mapped by or registered with context
 * supporting unified addressing ::lwdaErrorIlwalidValue is returned.
 *
 * \note In LWCA 11.0 forward passing host pointer will return ::lwdaMemoryTypeUnregistered
 * in ::lwdaPointerAttributes::type and call will return ::lwdaSuccess.
 *
 * The ::lwdaPointerAttributes structure is defined as:
 * \code
    struct lwdaPointerAttributes {
        enum lwdaMemoryType type;
        int device;
        void *devicePointer;
        void *hostPointer;
    }
    \endcode
 * In this structure, the individual fields mean
 *
 * - \ref ::lwdaPointerAttributes::type identifies type of memory. It can be
 *    ::lwdaMemoryTypeUnregistered for unregistered host memory,
 *    ::lwdaMemoryTypeHost for registered host memory, ::lwdaMemoryTypeDevice for device
 *    memory or  ::lwdaMemoryTypeManaged for managed memory.
 *
 * - \ref ::lwdaPointerAttributes::device "device" is the device against which
 *   \p ptr was allocated.  If \p ptr has memory type ::lwdaMemoryTypeDevice
 *   then this identifies the device on which the memory referred to by \p ptr
 *   physically resides.  If \p ptr has memory type ::lwdaMemoryTypeHost then this
 *   identifies the device which was current when the allocation was made
 *   (and if that device is deinitialized then this allocation will vanish
 *   with that device's state).
 *
 * - \ref ::lwdaPointerAttributes::devicePointer "devicePointer" is
 *   the device pointer alias through which the memory referred to by \p ptr
 *   may be accessed on the current device.
 *   If the memory referred to by \p ptr cannot be accessed directly by the 
 *   current device then this is NULL.  
 *
 * - \ref ::lwdaPointerAttributes::hostPointer "hostPointer" is
 *   the host pointer alias through which the memory referred to by \p ptr
 *   may be accessed on the host.
 *   If the memory referred to by \p ptr cannot be accessed directly by the
 *   host then this is NULL.
 *
 * \param attributes - Attributes for the specified pointer
 * \param ptr        - Pointer to get attributes for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaGetDeviceCount, ::lwdaGetDevice, ::lwdaSetDevice,
 * ::lwdaChooseDevice,
 * ::lwPointerGetAttributes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaPointerGetAttributes(struct lwdaPointerAttributes *attributes, const void *ptr);

/** @} */ /* END LWDART_UNIFIED */

/**
 * \defgroup LWDART_PEER Peer Device Memory Access
 *
 * ___MANBRIEF___ peer device memory access functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the peer device memory access functions of the LWCA runtime
 * application programming interface.
 *
 * @{
 */

/**
 * \brief Queries if a device may directly access a peer device's memory.
 *
 * Returns in \p *canAccessPeer a value of 1 if device \p device is capable of
 * directly accessing memory from \p peerDevice and 0 otherwise.  If direct
 * access of \p peerDevice from \p device is possible, then access may be
 * enabled by calling ::lwdaDeviceEnablePeerAccess().
 *
 * \param canAccessPeer - Returned access capability
 * \param device        - Device from which allocations on \p peerDevice are to
 *                        be directly accessed.
 * \param peerDevice    - Device on which the allocations to be directly accessed 
 *                        by \p device reside.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceEnablePeerAccess,
 * ::lwdaDeviceDisablePeerAccess,
 * ::lwDeviceCanAccessPeer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceCanAccessPeer(int *canAccessPeer, int device, int peerDevice);

/**
 * \brief Enables direct access to memory allocations on a peer device.
 *
 * On success, all allocations from \p peerDevice will immediately be accessible by
 * the current device.  They will remain accessible until access is explicitly
 * disabled using ::lwdaDeviceDisablePeerAccess() or either device is reset using
 * ::lwdaDeviceReset().
 *
 * Note that access granted by this call is unidirectional and that in order to access
 * memory on the current device from \p peerDevice, a separate symmetric call 
 * to ::lwdaDeviceEnablePeerAccess() is required.
 *
 * Note that there are both device-wide and system-wide limitations per system
 * configuration, as noted in the LWCA Programming Guide under the section
 * "Peer-to-Peer Memory Access".
 *
 * Returns ::lwdaErrorIlwalidDevice if ::lwdaDeviceCanAccessPeer() indicates
 * that the current device cannot directly access memory from \p peerDevice.
 *
 * Returns ::lwdaErrorPeerAccessAlreadyEnabled if direct access of
 * \p peerDevice from the current device has already been enabled.
 *
 * Returns ::lwdaErrorIlwalidValue if \p flags is not 0.
 *
 * \param peerDevice  - Peer device to enable direct access to from the current device
 * \param flags       - Reserved for future use and must be set to 0
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorPeerAccessAlreadyEnabled,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceCanAccessPeer,
 * ::lwdaDeviceDisablePeerAccess,
 * ::lwCtxEnablePeerAccess
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceEnablePeerAccess(int peerDevice, unsigned int flags);

/**
 * \brief Disables direct access to memory allocations on a peer device.
 *
 * Returns ::lwdaErrorPeerAccessNotEnabled if direct access to memory on
 * \p peerDevice has not yet been enabled from the current device.
 *
 * \param peerDevice - Peer device to disable direct access to
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorPeerAccessNotEnabled,
 * ::lwdaErrorIlwalidDevice
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa ::lwdaDeviceCanAccessPeer,
 * ::lwdaDeviceEnablePeerAccess,
 * ::lwCtxDisablePeerAccess
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceDisablePeerAccess(int peerDevice);

/** @} */ /* END LWDART_PEER */

/** \defgroup LWDART_OPENGL OpenGL Interoperability */

/** \defgroup LWDART_OPENGL_DEPRECATED OpenGL Interoperability [DEPRECATED] */

/** \defgroup LWDART_D3D9 Direct3D 9 Interoperability */

/** \defgroup LWDART_D3D9_DEPRECATED Direct3D 9 Interoperability [DEPRECATED] */

/** \defgroup LWDART_D3D10 Direct3D 10 Interoperability */

/** \defgroup LWDART_D3D10_DEPRECATED Direct3D 10 Interoperability [DEPRECATED] */

/** \defgroup LWDART_D3D11 Direct3D 11 Interoperability */

/** \defgroup LWDART_D3D11_DEPRECATED Direct3D 11 Interoperability [DEPRECATED] */

/** \defgroup LWDART_VDPAU VDPAU Interoperability */

/** \defgroup LWDART_EGL EGL Interoperability */

/**
 * \defgroup LWDART_INTEROP Graphics Interoperability
 *
 * ___MANBRIEF___ graphics interoperability functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the graphics interoperability functions of the LWCA
 * runtime application programming interface.
 *
 * @{
 */

/**
 * \brief Unregisters a graphics resource for access by LWCA
 *
 * Unregisters the graphics resource \p resource so it is not accessible by
 * LWCA unless registered again.
 *
 * If \p resource is invalid then ::lwdaErrorIlwalidResourceHandle is
 * returned.
 *
 * \param resource - Resource to unregister
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa
 * ::lwdaGraphicsD3D9RegisterResource,
 * ::lwdaGraphicsD3D10RegisterResource,
 * ::lwdaGraphicsD3D11RegisterResource,
 * ::lwdaGraphicsGLRegisterBuffer,
 * ::lwdaGraphicsGLRegisterImage,
 * ::lwGraphicsUnregisterResource
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsUnregisterResource(lwdaGraphicsResource_t resource);

/**
 * \brief Set usage flags for mapping a graphics resource
 *
 * Set \p flags for mapping the graphics resource \p resource.
 *
 * Changes to \p flags will take effect the next time \p resource is mapped.
 * The \p flags argument may be any of the following:
 * - ::lwdaGraphicsMapFlagsNone: Specifies no hints about how \p resource will
 *     be used. It is therefore assumed that LWCA may read from or write to \p resource.
 * - ::lwdaGraphicsMapFlagsReadOnly: Specifies that LWCA will not write to \p resource.
 * - ::lwdaGraphicsMapFlagsWriteDiscard: Specifies LWCA will not read from \p resource and will
 *   write over the entire contents of \p resource, so none of the data
 *   previously stored in \p resource will be preserved.
 *
 * If \p resource is presently mapped for access by LWCA then ::lwdaErrorUnknown is returned.
 * If \p flags is not one of the above values then ::lwdaErrorIlwalidValue is returned.
 *
 * \param resource - Registered resource to set flags for
 * \param flags    - Parameters for resource mapping
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown,
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphicsMapResources,
 * ::lwGraphicsResourceSetMapFlags
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsResourceSetMapFlags(lwdaGraphicsResource_t resource, unsigned int flags);

/**
 * \brief Map graphics resources for access by LWCA
 *
 * Maps the \p count graphics resources in \p resources for access by LWCA.
 *
 * The resources in \p resources may be accessed by LWCA until they
 * are unmapped. The graphics API from which \p resources were registered
 * should not access any resources while they are mapped by LWCA. If an
 * application does so, the results are undefined.
 *
 * This function provides the synchronization guarantee that any graphics calls
 * issued before ::lwdaGraphicsMapResources() will complete before any subsequent LWCA
 * work issued in \p stream begins.
 *
 * If \p resources contains any duplicate entries then ::lwdaErrorIlwalidResourceHandle
 * is returned. If any of \p resources are presently mapped for access by
 * LWCA then ::lwdaErrorUnknown is returned.
 *
 * \param count     - Number of resources to map
 * \param resources - Resources to map for LWCA
 * \param stream    - Stream for synchronization
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwdaGraphicsSubResourceGetMappedArray,
 * ::lwdaGraphicsUnmapResources,
 * ::lwGraphicsMapResources
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsMapResources(int count, lwdaGraphicsResource_t *resources, lwdaStream_t stream __dv(0));

/**
 * \brief Unmap graphics resources.
 *
 * Unmaps the \p count graphics resources in \p resources.
 *
 * Once unmapped, the resources in \p resources may not be accessed by LWCA
 * until they are mapped again.
 *
 * This function provides the synchronization guarantee that any LWCA work issued
 * in \p stream before ::lwdaGraphicsUnmapResources() will complete before any
 * subsequently issued graphics work begins.
 *
 * If \p resources contains any duplicate entries then ::lwdaErrorIlwalidResourceHandle
 * is returned. If any of \p resources are not presently mapped for access by
 * LWCA then ::lwdaErrorUnknown is returned.
 *
 * \param count     - Number of resources to unmap
 * \param resources - Resources to unmap
 * \param stream    - Stream for synchronization
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \note_null_stream
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphicsMapResources,
 * ::lwGraphicsUnmapResources
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsUnmapResources(int count, lwdaGraphicsResource_t *resources, lwdaStream_t stream __dv(0));

/**
 * \brief Get an device pointer through which to access a mapped graphics resource.
 *
 * Returns in \p *devPtr a pointer through which the mapped graphics resource
 * \p resource may be accessed.
 * Returns in \p *size the size of the memory in bytes which may be accessed from that pointer.
 * The value set in \p devPtr may change every time that \p resource is mapped.
 *
 * If \p resource is not a buffer then it cannot be accessed via a pointer and
 * ::lwdaErrorUnknown is returned.
 * If \p resource is not mapped then ::lwdaErrorUnknown is returned.
 * *
 * \param devPtr     - Returned pointer through which \p resource may be accessed
 * \param size       - Returned size of the buffer accessible starting at \p *devPtr
 * \param resource   - Mapped resource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphicsMapResources,
 * ::lwdaGraphicsSubResourceGetMappedArray,
 * ::lwGraphicsResourceGetMappedPointer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsResourceGetMappedPointer(void **devPtr, size_t *size, lwdaGraphicsResource_t resource);

/**
 * \brief Get an array through which to access a subresource of a mapped graphics resource.
 *
 * Returns in \p *array an array through which the subresource of the mapped
 * graphics resource \p resource which corresponds to array index \p arrayIndex
 * and mipmap level \p mipLevel may be accessed.  The value set in \p array may
 * change every time that \p resource is mapped.
 *
 * If \p resource is not a texture then it cannot be accessed via an array and
 * ::lwdaErrorUnknown is returned.
 * If \p arrayIndex is not a valid array index for \p resource then
 * ::lwdaErrorIlwalidValue is returned.
 * If \p mipLevel is not a valid mipmap level for \p resource then
 * ::lwdaErrorIlwalidValue is returned.
 * If \p resource is not mapped then ::lwdaErrorUnknown is returned.
 *
 * \param array       - Returned array through which a subresource of \p resource may be accessed
 * \param resource    - Mapped resource to access
 * \param arrayIndex  - Array index for array textures or lwbemap face
 *                      index as defined by ::lwdaGraphicsLwbeFace for
 *                      lwbemap textures for the subresource to access
 * \param mipLevel    - Mipmap level for the subresource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwGraphicsSubResourceGetMappedArray
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsSubResourceGetMappedArray(lwdaArray_t *array, lwdaGraphicsResource_t resource, unsigned int arrayIndex, unsigned int mipLevel);

/**
 * \brief Get a mipmapped array through which to access a mapped graphics resource.
 *
 * Returns in \p *mipmappedArray a mipmapped array through which the mapped
 * graphics resource \p resource may be accessed. The value set in \p mipmappedArray may
 * change every time that \p resource is mapped.
 *
 * If \p resource is not a texture then it cannot be accessed via an array and
 * ::lwdaErrorUnknown is returned.
 * If \p resource is not mapped then ::lwdaErrorUnknown is returned.
 *
 * \param mipmappedArray - Returned mipmapped array through which \p resource may be accessed
 * \param resource       - Mapped resource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwGraphicsResourceGetMappedMipmappedArray
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsResourceGetMappedMipmappedArray(lwdaMipmappedArray_t *mipmappedArray, lwdaGraphicsResource_t resource);

/** @} */ /* END LWDART_INTEROP */

/**
 * \defgroup LWDART_TEXTURE Texture Reference Management [DEPRECATED]
 *
 * ___MANBRIEF___ texture reference management functions of the LWCA runtime
 * API (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the low level texture reference management functions
 * of the LWCA runtime application programming interface.
 *
 * Some functions have overloaded C++ API template versions dolwmented separately in the
 * \ref LWDART_HIGHLEVEL "C++ API Routines" module.
 *
 * @{
 */

/**
 * \brief Binds a memory area to a texture
 *
 * \deprecated
 *
 * Binds \p size bytes of the memory area pointed to by \p devPtr to the
 * texture reference \p texref. \p desc describes how the memory is interpreted
 * when fetching values from the texture. Any memory previously bound to
 * \p texref is unbound.
 *
 * Since the hardware enforces an alignment requirement on texture base
 * addresses,
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture()"
 * returns in \p *offset a byte offset that
 * must be applied to texture fetches in order to read from the desired memory.
 * This offset must be divided by the texel size and passed to kernels that
 * read from the texture so they can be applied to the ::tex1Dfetch() function.
 * If the device memory pointer was returned from ::lwdaMalloc(), the offset is
 * guaranteed to be 0 and NULL may be passed as the \p offset parameter.
 *
 * The total number of elements (or texels) in the linear address range
 * cannot exceed ::lwdaDeviceProp::maxTexture1DLinear[0].
 * The number of elements is computed as (\p size / elementSize),
 * where elementSize is determined from \p desc.
 *
 * \param offset - Offset in bytes
 * \param texref - Texture to bind
 * \param devPtr - Memory area on device
 * \param desc   - Channel format
 * \param size   - Size of the memory area pointed to by devPtr
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidTexture
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct texture< T, dim, readMode>&, const void*, const struct lwdaChannelFormatDesc&, size_t) "lwdaBindTexture (C++ API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t, size_t, size_t) "lwdaBindTexture2D (C API)",
 * \ref ::lwdaBindTextureToArray(const struct textureReference*, lwdaArray_const_t, const struct lwdaChannelFormatDesc*) "lwdaBindTextureToArray (C API)",
 * \ref ::lwdaUnbindTexture(const struct textureReference*) "lwdaUnbindTexture (C API)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct textureReference*) "lwdaGetTextureAlignmentOffset (C API)",
 * ::lwTexRefSetAddress,
 * ::lwTexRefSetAddressMode,
 * ::lwTexRefSetFormat,
 * ::lwTexRefSetFlags,
 * ::lwTexRefSetBorderColor
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaBindTexture(size_t *offset, const struct textureReference *texref, const void *devPtr, const struct lwdaChannelFormatDesc *desc, size_t size __dv(UINT_MAX));

/**
 * \brief Binds a 2D memory area to a texture
 *
 * \deprecated
 *
 * Binds the 2D memory area pointed to by \p devPtr to the
 * texture reference \p texref. The size of the area is constrained by
 * \p width in texel units, \p height in texel units, and \p pitch in byte
 * units. \p desc describes how the memory is interpreted when fetching values
 * from the texture. Any memory previously bound to \p texref is unbound.
 *
 * Since the hardware enforces an alignment requirement on texture base
 * addresses, ::lwdaBindTexture2D() returns in \p *offset a byte offset that
 * must be applied to texture fetches in order to read from the desired memory.
 * This offset must be divided by the texel size and passed to kernels that
 * read from the texture so they can be applied to the ::tex2D() function.
 * If the device memory pointer was returned from ::lwdaMalloc(), the offset is
 * guaranteed to be 0 and NULL may be passed as the \p offset parameter.
 *
 * \p width and \p height, which are specified in elements (or texels), cannot
 * exceed ::lwdaDeviceProp::maxTexture2DLinear[0] and ::lwdaDeviceProp::maxTexture2DLinear[1]
 * respectively. \p pitch, which is specified in bytes, cannot exceed
 * ::lwdaDeviceProp::maxTexture2DLinear[2].
 *
 * The driver returns ::lwdaErrorIlwalidValue if \p pitch is not a multiple of
 * ::lwdaDeviceProp::texturePitchAlignment.
 *
 * \param offset - Offset in bytes
 * \param texref - Texture reference to bind
 * \param devPtr - 2D memory area on device
 * \param desc   - Channel format
 * \param width  - Width in texel units
 * \param height - Height in texel units
 * \param pitch  - Pitch in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidTexture
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture (C API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct texture< T, dim, readMode>&, const void*, const struct lwdaChannelFormatDesc&, size_t, size_t, size_t) "lwdaBindTexture2D (C++ API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct texture<T, dim, readMode>&, const void*, size_t, size_t, size_t) "lwdaBindTexture2D (C++ API, inherited channel descriptor)",
 * \ref ::lwdaBindTextureToArray(const struct textureReference*, lwdaArray_const_t, const struct lwdaChannelFormatDesc*) "lwdaBindTextureToArray (C API)",
 * \ref ::lwdaUnbindTexture(const struct textureReference*) "lwdaBindTextureToArray (C API)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct textureReference*) "lwdaGetTextureAlignmentOffset (C API)",
 * ::lwTexRefSetAddress2D,
 * ::lwTexRefSetFormat,
 * ::lwTexRefSetFlags,
 * ::lwTexRefSetAddressMode,
 * ::lwTexRefSetBorderColor
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaBindTexture2D(size_t *offset, const struct textureReference *texref, const void *devPtr, const struct lwdaChannelFormatDesc *desc, size_t width, size_t height, size_t pitch);

/**
 * \brief Binds an array to a texture
 *
 * \deprecated
 *
 * Binds the LWCA array \p array to the texture reference \p texref.
 * \p desc describes how the memory is interpreted when fetching values from
 * the texture. Any LWCA array previously bound to \p texref is unbound.
 *
 * \param texref - Texture to bind
 * \param array  - Memory array on device
 * \param desc   - Channel format
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidTexture
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture (C API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t, size_t, size_t) "lwdaBindTexture2D (C API)",
 * \ref ::lwdaBindTextureToArray(const struct texture< T, dim, readMode>&, lwdaArray_const_t, const struct lwdaChannelFormatDesc&) "lwdaBindTextureToArray (C++ API)",
 * \ref ::lwdaUnbindTexture(const struct textureReference*) "lwdaUnbindTexture (C API)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct textureReference*) "lwdaGetTextureAlignmentOffset (C API)",
 * ::lwTexRefSetArray,
 * ::lwTexRefSetFormat,
 * ::lwTexRefSetFlags,
 * ::lwTexRefSetAddressMode,
 * ::lwTexRefSetFilterMode,
 * ::lwTexRefSetBorderColor,
 * ::lwTexRefSetMaxAnisotropy
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaBindTextureToArray(const struct textureReference *texref, lwdaArray_const_t array, const struct lwdaChannelFormatDesc *desc);

/**
 * \brief Binds a mipmapped array to a texture
 *
 * \deprecated
 *
 * Binds the LWCA mipmapped array \p mipmappedArray to the texture reference \p texref.
 * \p desc describes how the memory is interpreted when fetching values from
 * the texture. Any LWCA mipmapped array previously bound to \p texref is unbound.
 *
 * \param texref         - Texture to bind
 * \param mipmappedArray - Memory mipmapped array on device
 * \param desc           - Channel format
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidTexture
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture (C API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t, size_t, size_t) "lwdaBindTexture2D (C API)",
 * \ref ::lwdaBindTextureToArray(const struct texture< T, dim, readMode>&, lwdaArray_const_t, const struct lwdaChannelFormatDesc&) "lwdaBindTextureToArray (C++ API)",
 * \ref ::lwdaUnbindTexture(const struct textureReference*) "lwdaUnbindTexture (C API)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct textureReference*) "lwdaGetTextureAlignmentOffset (C API)",
 * ::lwTexRefSetMipmappedArray,
 * ::lwTexRefSetMipmapFilterMode,
 * ::lwTexRefSetMipmapLevelClamp,
 * ::lwTexRefSetMipmapLevelBias,
 * ::lwTexRefSetFormat,
 * ::lwTexRefSetFlags,
 * ::lwTexRefSetAddressMode,
 * ::lwTexRefSetBorderColor,
 * ::lwTexRefSetMaxAnisotropy
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaBindTextureToMipmappedArray(const struct textureReference *texref, lwdaMipmappedArray_const_t mipmappedArray, const struct lwdaChannelFormatDesc *desc);

/**
 * \brief Unbinds a texture
 *
 * \deprecated
 *
 * Unbinds the texture bound to \p texref. If \p texref is not lwrrently bound, no operation is performed.
 *
 * \param texref - Texture to unbind
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidTexture
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture (C API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t, size_t, size_t) "lwdaBindTexture2D (C API)",
 * \ref ::lwdaBindTextureToArray(const struct textureReference*, lwdaArray_const_t, const struct lwdaChannelFormatDesc*) "lwdaBindTextureToArray (C API)",
 * \ref ::lwdaUnbindTexture(const struct texture< T, dim, readMode>&) "lwdaUnbindTexture (C++ API)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct textureReference*) "lwdaGetTextureAlignmentOffset (C API)"
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaUnbindTexture(const struct textureReference *texref);

/**
 * \brief Get the alignment offset of a texture
 *
 * \deprecated
 *
 * Returns in \p *offset the offset that was returned when texture reference
 * \p texref was bound.
 *
 * \param offset - Offset of texture reference in bytes
 * \param texref - Texture to get offset of
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidTexture,
 * ::lwdaErrorIlwalidTextureBinding
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture (C API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t, size_t, size_t) "lwdaBindTexture2D (C API)",
 * \ref ::lwdaBindTextureToArray(const struct textureReference*, lwdaArray_const_t, const struct lwdaChannelFormatDesc*) "lwdaBindTextureToArray (C API)",
 * \ref ::lwdaUnbindTexture(const struct textureReference*) "lwdaUnbindTexture (C API)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct texture< T, dim, readMode>&) "lwdaGetTextureAlignmentOffset (C++ API)"
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGetTextureAlignmentOffset(size_t *offset, const struct textureReference *texref);

/**
 * \brief Get the texture reference associated with a symbol
 *
 * \deprecated
 *
 * Returns in \p *texref the structure associated to the texture reference
 * defined by symbol \p symbol.
 *
 * \param texref - Texture reference associated with symbol
 * \param symbol - Texture to get reference for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidTexture
 * \notefnerr
 * \note_string_api_deprecation_50
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaGetChannelDesc,
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct textureReference*) "lwdaGetTextureAlignmentOffset (C API)",
 * \ref ::lwdaBindTexture(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t) "lwdaBindTexture (C API)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct textureReference*, const void*, const struct lwdaChannelFormatDesc*, size_t, size_t, size_t) "lwdaBindTexture2D (C API)",
 * \ref ::lwdaBindTextureToArray(const struct textureReference*, lwdaArray_const_t, const struct lwdaChannelFormatDesc*) "lwdaBindTextureToArray (C API)",
 * \ref ::lwdaUnbindTexture(const struct textureReference*) "lwdaUnbindTexture (C API)",
 * ::lwModuleGetTexRef
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGetTextureReference(const struct textureReference **texref, const void *symbol);

/** @} */ /* END LWDART_TEXTURE */

/**
 * \defgroup LWDART_SURFACE Surface Reference Management [DEPRECATED]
 *
 * ___MANBRIEF___ surface reference management functions of the LWCA runtime
 * API (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the low level surface reference management functions
 * of the LWCA runtime application programming interface.
 *
 * Some functions have overloaded C++ API template versions dolwmented separately in the
 * \ref LWDART_HIGHLEVEL "C++ API Routines" module.
 *
 * @{
 */

/**
 * \brief Binds an array to a surface
 *
 * \deprecated
 *
 * Binds the LWCA array \p array to the surface reference \p surfref.
 * \p desc describes how the memory is interpreted when fetching values from
 * the surface. Any LWCA array previously bound to \p surfref is unbound.
 *
 * \param surfref - Surface to bind
 * \param array  - Memory array on device
 * \param desc   - Channel format
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidSurface
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaBindSurfaceToArray(const struct surface< T, dim>&, lwdaArray_const_t, const struct lwdaChannelFormatDesc&) "lwdaBindSurfaceToArray (C++ API)",
 * \ref ::lwdaBindSurfaceToArray(const struct surface< T, dim>&, lwdaArray_const_t) "lwdaBindSurfaceToArray (C++ API, inherited channel descriptor)",
 * ::lwdaGetSurfaceReference,
 * ::lwSurfRefSetArray
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaBindSurfaceToArray(const struct surfaceReference *surfref, lwdaArray_const_t array, const struct lwdaChannelFormatDesc *desc);

/**
 * \brief Get the surface reference associated with a symbol
 *
 * \deprecated
 *
 * Returns in \p *surfref the structure associated to the surface reference
 * defined by symbol \p symbol.
 *
 * \param surfref - Surface reference associated with symbol
 * \param symbol - Surface to get reference for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidSurface
 * \notefnerr
 * \note_string_api_deprecation_50
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * \ref ::lwdaBindSurfaceToArray(const struct surfaceReference*, lwdaArray_const_t, const struct lwdaChannelFormatDesc*) "lwdaBindSurfaceToArray (C API)",
 * ::lwModuleGetSurfRef
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGetSurfaceReference(const struct surfaceReference **surfref, const void *symbol);

/** @} */ /* END LWDART_SURFACE */

/**
 * \defgroup LWDART_TEXTURE_OBJECT Texture Object Management
 *
 * ___MANBRIEF___ texture object management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the low level texture object management functions
 * of the LWCA runtime application programming interface. The texture
 * object API is only supported on devices of compute capability 3.0 or higher.
 *
 * @{
 */

/**
 * \brief Get the channel descriptor of an array
 *
 * Returns in \p *desc the channel descriptor of the LWCA array \p array.
 *
 * \param desc  - Channel format
 * \param array - Memory array on device
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa \ref ::lwdaCreateChannelDesc(int, int, int, int, lwdaChannelFormatKind) "lwdaCreateChannelDesc (C API)",
 * ::lwdaCreateTextureObject, ::lwdaCreateSurfaceObject
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetChannelDesc(struct lwdaChannelFormatDesc *desc, lwdaArray_const_t array);

/**
 * \brief Returns a channel descriptor using the specified format
 *
 * Returns a channel descriptor with format \p f and number of bits of each
 * component \p x, \p y, \p z, and \p w.  The ::lwdaChannelFormatDesc is
 * defined as:
 * \code
  struct lwdaChannelFormatDesc {
    int x, y, z, w;
    enum lwdaChannelFormatKind f;
  };
 * \endcode
 *
 * where ::lwdaChannelFormatKind is one of ::lwdaChannelFormatKindSigned,
 * ::lwdaChannelFormatKindUnsigned, or ::lwdaChannelFormatKindFloat.
 *
 * \param x - X component
 * \param y - Y component
 * \param z - Z component
 * \param w - W component
 * \param f - Channel format
 *
 * \return
 * Channel descriptor with format \p f
 *
 * \sa \ref ::lwdaCreateChannelDesc(void) "lwdaCreateChannelDesc (C++ API)",
 * ::lwdaGetChannelDesc, ::lwdaCreateTextureObject, ::lwdaCreateSurfaceObject
 */
extern __host__ struct lwdaChannelFormatDesc LWDARTAPI lwdaCreateChannelDesc(int x, int y, int z, int w, enum lwdaChannelFormatKind f);

/**
 * \brief Creates a texture object
 *
 * Creates a texture object and returns it in \p pTexObject. \p pResDesc describes
 * the data to texture from. \p pTexDesc describes how the data should be sampled.
 * \p pResViewDesc is an optional argument that specifies an alternate format for
 * the data described by \p pResDesc, and also describes the subresource region
 * to restrict access to when texturing. \p pResViewDesc can only be specified if
 * the type of resource is a LWCA array or a LWCA mipmapped array.
 *
 * Texture objects are only supported on devices of compute capability 3.0 or higher.
 * Additionally, a texture object is an opaque value, and, as such, should only be
 * accessed through LWCA API calls.
 *
 * The ::lwdaResourceDesc structure is defined as:
 * \code
        struct lwdaResourceDesc {
            enum lwdaResourceType resType;
            
            union {
                struct {
                    lwdaArray_t array;
                } array;
                struct {
                    lwdaMipmappedArray_t mipmap;
                } mipmap;
                struct {
                    void *devPtr;
                    struct lwdaChannelFormatDesc desc;
                    size_t sizeInBytes;
                } linear;
                struct {
                    void *devPtr;
                    struct lwdaChannelFormatDesc desc;
                    size_t width;
                    size_t height;
                    size_t pitchInBytes;
                } pitch2D;
            } res;
        };
 * \endcode
 * where:
 * - ::lwdaResourceDesc::resType specifies the type of resource to texture from.
 * LWresourceType is defined as:
 * \code
        enum lwdaResourceType {
            lwdaResourceTypeArray          = 0x00,
            lwdaResourceTypeMipmappedArray = 0x01,
            lwdaResourceTypeLinear         = 0x02,
            lwdaResourceTypePitch2D        = 0x03
        };
 * \endcode
 *
 * \par
 * If ::lwdaResourceDesc::resType is set to ::lwdaResourceTypeArray, ::lwdaResourceDesc::res::array::array
 * must be set to a valid LWCA array handle.
 *
 * \par
 * If ::lwdaResourceDesc::resType is set to ::lwdaResourceTypeMipmappedArray, ::lwdaResourceDesc::res::mipmap::mipmap
 * must be set to a valid LWCA mipmapped array handle and ::lwdaTextureDesc::normalizedCoords must be set to true.
 *
 * \par
 * If ::lwdaResourceDesc::resType is set to ::lwdaResourceTypeLinear, ::lwdaResourceDesc::res::linear::devPtr
 * must be set to a valid device pointer, that is aligned to ::lwdaDeviceProp::textureAlignment.
 * ::lwdaResourceDesc::res::linear::desc describes the format and the number of components per array element. ::lwdaResourceDesc::res::linear::sizeInBytes
 * specifies the size of the array in bytes. The total number of elements in the linear address range cannot exceed 
 * ::lwdaDeviceProp::maxTexture1DLinear. The number of elements is computed as (sizeInBytes / sizeof(desc)).
 *
 * \par
 * If ::lwdaResourceDesc::resType is set to ::lwdaResourceTypePitch2D, ::lwdaResourceDesc::res::pitch2D::devPtr
 * must be set to a valid device pointer, that is aligned to ::lwdaDeviceProp::textureAlignment.
 * ::lwdaResourceDesc::res::pitch2D::desc describes the format and the number of components per array element. ::lwdaResourceDesc::res::pitch2D::width
 * and ::lwdaResourceDesc::res::pitch2D::height specify the width and height of the array in elements, and cannot exceed
 * ::lwdaDeviceProp::maxTexture2DLinear[0] and ::lwdaDeviceProp::maxTexture2DLinear[1] respectively.
 * ::lwdaResourceDesc::res::pitch2D::pitchInBytes specifies the pitch between two rows in bytes and has to be aligned to 
 * ::lwdaDeviceProp::texturePitchAlignment. Pitch cannot exceed ::lwdaDeviceProp::maxTexture2DLinear[2].
 *
 *
 * The ::lwdaTextureDesc struct is defined as
 * \code
        struct lwdaTextureDesc {
            enum lwdaTextureAddressMode addressMode[3];
            enum lwdaTextureFilterMode  filterMode;
            enum lwdaTextureReadMode    readMode;
            int                         sRGB;
            float                       borderColor[4];
            int                         normalizedCoords;
            unsigned int                maxAnisotropy;
            enum lwdaTextureFilterMode  mipmapFilterMode;
            float                       mipmapLevelBias;
            float                       minMipmapLevelClamp;
            float                       maxMipmapLevelClamp;
            int                         disableTrilinearOptimization;
            int                         seamlessLwbemap;
        };
 * \endcode
 * where
 * - ::lwdaTextureDesc::addressMode specifies the addressing mode for each dimension of the texture data. ::lwdaTextureAddressMode is defined as:
 *   \code
        enum lwdaTextureAddressMode {
            lwdaAddressModeWrap   = 0,
            lwdaAddressModeClamp  = 1,
            lwdaAddressModeMirror = 2,
            lwdaAddressModeBorder = 3
        };
 *   \endcode
 *   This is ignored if ::lwdaResourceDesc::resType is ::lwdaResourceTypeLinear. Also, if ::lwdaTextureDesc::normalizedCoords
 *   is set to zero, ::lwdaAddressModeWrap and ::lwdaAddressModeMirror won't be supported and will be switched to ::lwdaAddressModeClamp.
 *
 * - ::lwdaTextureDesc::filterMode specifies the filtering mode to be used when fetching from the texture. ::lwdaTextureFilterMode is defined as:
 *   \code
        enum lwdaTextureFilterMode {
            lwdaFilterModePoint  = 0,
            lwdaFilterModeLinear = 1
        };
 *   \endcode
 *   This is ignored if ::lwdaResourceDesc::resType is ::lwdaResourceTypeLinear.
 *
 * - ::lwdaTextureDesc::readMode specifies whether integer data should be colwerted to floating point or not. ::lwdaTextureReadMode is defined as:
 *   \code
        enum lwdaTextureReadMode {
            lwdaReadModeElementType     = 0,
            lwdaReadModeNormalizedFloat = 1
        };
 *   \endcode
 *   Note that this applies only to 8-bit and 16-bit integer formats. 32-bit integer format would not be promoted, regardless of 
 *   whether or not this ::lwdaTextureDesc::readMode is set ::lwdaReadModeNormalizedFloat is specified.
 *
 * - ::lwdaTextureDesc::sRGB specifies whether sRGB to linear colwersion should be performed during texture fetch.
 *
 * - ::lwdaTextureDesc::borderColor specifies the float values of color. where:
 *   ::lwdaTextureDesc::borderColor[0] contains value of 'R', 
 *   ::lwdaTextureDesc::borderColor[1] contains value of 'G',
 *   ::lwdaTextureDesc::borderColor[2] contains value of 'B', 
 *   ::lwdaTextureDesc::borderColor[3] contains value of 'A'
 *   Note that application using integer border color values will need to <reinterpret_cast> these values to float.
 *   The values are set only when the addressing mode specified by ::lwdaTextureDesc::addressMode is lwdaAddressModeBorder.
 *
 * - ::lwdaTextureDesc::normalizedCoords specifies whether the texture coordinates will be normalized or not.
 *
 * - ::lwdaTextureDesc::maxAnisotropy specifies the maximum anistropy ratio to be used when doing anisotropic filtering. This value will be
 *   clamped to the range [1,16].
 *
 * - ::lwdaTextureDesc::mipmapFilterMode specifies the filter mode when the callwlated mipmap level lies between two defined mipmap levels.
 *
 * - ::lwdaTextureDesc::mipmapLevelBias specifies the offset to be applied to the callwlated mipmap level.
 *
 * - ::lwdaTextureDesc::minMipmapLevelClamp specifies the lower end of the mipmap level range to clamp access to.
 *
 * - ::lwdaTextureDesc::maxMipmapLevelClamp specifies the upper end of the mipmap level range to clamp access to.
 *
 * - ::lwdaTextureDesc::disableTrilinearOptimization specifies whether the trilinear filtering optimizations will be disabled.
 *
 * - ::lwdaTextureDesc::seamlessLwbemap specifies whether seamless lwbe map filtering is enabled. This flag can only be specified if the 
 *   underlying resource is a LWCA array or a LWCA mipmapped array that was created with the flag ::lwdaArrayLwbemap.
 *   When seamless lwbe map filtering is enabled, texture address modes specified by ::lwdaTextureDesc::addressMode are ignored.
 *   Instead, if the ::lwdaTextureDesc::filterMode is set to ::lwdaFilterModePoint the address mode ::lwdaAddressModeClamp will be applied for all dimensions.
 *   If the ::lwdaTextureDesc::filterMode is set to ::lwdaFilterModeLinear seamless lwbe map filtering will be performed when sampling along the lwbe face borders.
 *
 * The ::lwdaResourceViewDesc struct is defined as
 * \code
        struct lwdaResourceViewDesc {
            enum lwdaResourceViewFormat format;
            size_t                      width;
            size_t                      height;
            size_t                      depth;
            unsigned int                firstMipmapLevel;
            unsigned int                lastMipmapLevel;
            unsigned int                firstLayer;
            unsigned int                lastLayer;
        };
 * \endcode
 * where:
 * - ::lwdaResourceViewDesc::format specifies how the data contained in the LWCA array or LWCA mipmapped array should
 *   be interpreted. Note that this can inlwr a change in size of the texture data. If the resource view format is a block
 *   compressed format, then the underlying LWCA array or LWCA mipmapped array has to have a 32-bit unsigned integer format
 *   with 2 or 4 channels, depending on the block compressed format. For ex., BC1 and BC4 require the underlying LWCA array to have
 *   a 32-bit unsigned int with 2 channels. The other BC formats require the underlying resource to have the same 32-bit unsigned int
 *   format but with 4 channels.
 *
 * - ::lwdaResourceViewDesc::width specifies the new width of the texture data. If the resource view format is a block
 *   compressed format, this value has to be 4 times the original width of the resource. For non block compressed formats,
 *   this value has to be equal to that of the original resource.
 *
 * - ::lwdaResourceViewDesc::height specifies the new height of the texture data. If the resource view format is a block
 *   compressed format, this value has to be 4 times the original height of the resource. For non block compressed formats,
 *   this value has to be equal to that of the original resource.
 *
 * - ::lwdaResourceViewDesc::depth specifies the new depth of the texture data. This value has to be equal to that of the
 *   original resource.
 *
 * - ::lwdaResourceViewDesc::firstMipmapLevel specifies the most detailed mipmap level. This will be the new mipmap level zero.
 *   For non-mipmapped resources, this value has to be zero.::lwdaTextureDesc::minMipmapLevelClamp and ::lwdaTextureDesc::maxMipmapLevelClamp
 *   will be relative to this value. For ex., if the firstMipmapLevel is set to 2, and a minMipmapLevelClamp of 1.2 is specified,
 *   then the actual minimum mipmap level clamp will be 3.2.
 *
 * - ::lwdaResourceViewDesc::lastMipmapLevel specifies the least detailed mipmap level. For non-mipmapped resources, this value
 *   has to be zero.
 *
 * - ::lwdaResourceViewDesc::firstLayer specifies the first layer index for layered textures. This will be the new layer zero.
 *   For non-layered resources, this value has to be zero.
 *
 * - ::lwdaResourceViewDesc::lastLayer specifies the last layer index for layered textures. For non-layered resources, 
 *   this value has to be zero.
 *
 *
 * \param pTexObject   - Texture object to create
 * \param pResDesc     - Resource descriptor
 * \param pTexDesc     - Texture descriptor
 * \param pResViewDesc - Resource view descriptor
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDestroyTextureObject,
 * ::lwTexObjectCreate
 */

extern __host__ lwdaError_t LWDARTAPI lwdaCreateTextureObject(lwdaTextureObject_t *pTexObject, const struct lwdaResourceDesc *pResDesc, const struct lwdaTextureDesc *pTexDesc, const struct lwdaResourceViewDesc *pResViewDesc);

/**
 * \brief Destroys a texture object
 *
 * Destroys the texture object specified by \p texObject.
 *
 * \param texObject - Texture object to destroy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa
 * ::lwdaCreateTextureObject,
 * ::lwTexObjectDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDestroyTextureObject(lwdaTextureObject_t texObject);

/**
 * \brief Returns a texture object's resource descriptor
 *
 * Returns the resource descriptor for the texture object specified by \p texObject.
 *
 * \param pResDesc  - Resource descriptor
 * \param texObject - Texture object
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaCreateTextureObject,
 * ::lwTexObjectGetResourceDesc
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetTextureObjectResourceDesc(struct lwdaResourceDesc *pResDesc, lwdaTextureObject_t texObject);

/**
 * \brief Returns a texture object's texture descriptor
 *
 * Returns the texture descriptor for the texture object specified by \p texObject.
 *
 * \param pTexDesc  - Texture descriptor
 * \param texObject - Texture object
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaCreateTextureObject,
 * ::lwTexObjectGetTextureDesc
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetTextureObjectTextureDesc(struct lwdaTextureDesc *pTexDesc, lwdaTextureObject_t texObject);

/**
 * \brief Returns a texture object's resource view descriptor
 *
 * Returns the resource view descriptor for the texture object specified by \p texObject.
 * If no resource view was specified, ::lwdaErrorIlwalidValue is returned.
 *
 * \param pResViewDesc - Resource view descriptor
 * \param texObject    - Texture object
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaCreateTextureObject,
 * ::lwTexObjectGetResourceViewDesc
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetTextureObjectResourceViewDesc(struct lwdaResourceViewDesc *pResViewDesc, lwdaTextureObject_t texObject);

/** @} */ /* END LWDART_TEXTURE_OBJECT */

/**
 * \defgroup LWDART_SURFACE_OBJECT Surface Object Management
 *
 * ___MANBRIEF___ surface object management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the low level texture object management functions
 * of the LWCA runtime application programming interface. The surface object 
 * API is only supported on devices of compute capability 3.0 or higher.
 *
 * @{
 */

/**
 * \brief Creates a surface object
 *
 * Creates a surface object and returns it in \p pSurfObject. \p pResDesc describes
 * the data to perform surface load/stores on. ::lwdaResourceDesc::resType must be 
 * ::lwdaResourceTypeArray and  ::lwdaResourceDesc::res::array::array
 * must be set to a valid LWCA array handle.
 *
 * Surface objects are only supported on devices of compute capability 3.0 or higher.
 * Additionally, a surface object is an opaque value, and, as such, should only be
 * accessed through LWCA API calls.
 *
 * \param pSurfObject - Surface object to create
 * \param pResDesc    - Resource descriptor
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidChannelDescriptor,
 * ::lwdaErrorIlwalidResourceHandle
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDestroySurfaceObject,
 * ::lwSurfObjectCreate
 */

extern __host__ lwdaError_t LWDARTAPI lwdaCreateSurfaceObject(lwdaSurfaceObject_t *pSurfObject, const struct lwdaResourceDesc *pResDesc);

/**
 * \brief Destroys a surface object
 *
 * Destroys the surface object specified by \p surfObject.
 *
 * \param surfObject - Surface object to destroy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa
 * ::lwdaCreateSurfaceObject,
 * ::lwSurfObjectDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDestroySurfaceObject(lwdaSurfaceObject_t surfObject);

/**
 * \brief Returns a surface object's resource descriptor
 * Returns the resource descriptor for the surface object specified by \p surfObject.
 *
 * \param pResDesc   - Resource descriptor
 * \param surfObject - Surface object
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaCreateSurfaceObject,
 * ::lwSurfObjectGetResourceDesc
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetSurfaceObjectResourceDesc(struct lwdaResourceDesc *pResDesc, lwdaSurfaceObject_t surfObject);

/** @} */ /* END LWDART_SURFACE_OBJECT */

/**
 * \defgroup LWDART__VERSION Version Management
 *
 * @{
 */

/**
 * \brief Returns the latest version of LWCA supported by the driver
 *
 * Returns in \p *driverVersion the latest version of LWCA supported by
 * the driver. The version is returned as (1000 &times; major + 10 &times; minor).
 * For example, LWCA 9.2 would be represented by 9020. If no driver is installed,
 * then 0 is returned as the driver version.
 *
 * This function automatically returns ::lwdaErrorIlwalidValue
 * if \p driverVersion is NULL.
 *
 * \param driverVersion - Returns the LWCA driver version.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaRuntimeGetVersion,
 * ::lwDriverGetVersion
 */
extern __host__ lwdaError_t LWDARTAPI lwdaDriverGetVersion(int *driverVersion);

/**
 * \brief Returns the LWCA Runtime version
 *
 * Returns in \p *runtimeVersion the version number of the current LWCA
 * Runtime instance. The version is returned as
 * (1000 &times; major + 10 &times; minor). For example,
 * LWCA 9.2 would be represented by 9020.
 *
 * This function automatically returns ::lwdaErrorIlwalidValue if
 * the \p runtimeVersion argument is NULL.
 *
 * \param runtimeVersion - Returns the LWCA Runtime version.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDriverGetVersion,
 * ::lwDriverGetVersion
 */
extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaRuntimeGetVersion(int *runtimeVersion);

/** @} */ /* END LWDART__VERSION */

/**
 * \defgroup LWDART_GRAPH Graph Management
 *
 * ___MANBRIEF___ graph management functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the graph management functions of LWCA
 * runtime application programming interface.
 *
 * @{
 */

/**
 * \brief Creates a graph
 *
 * Creates an empty graph, which is returned via \p pGraph.
 *
 * \param pGraph - Returns newly created graph
 * \param flags   - Graph creation flags, must be 0
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode,
 * ::lwdaGraphInstantiate,
 * ::lwdaGraphDestroy,
 * ::lwdaGraphGetNodes,
 * ::lwdaGraphGetRootNodes,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphClone
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphCreate(lwdaGraph_t *pGraph, unsigned int flags);

/**
 * \brief Creates a kernel exelwtion node and adds it to a graph
 *
 * Creates a new kernel exelwtion node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies and arguments specified in \p pNodeParams.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * The lwdaKernelNodeParams structure is defined as:
 *
 * \code
 *  struct lwdaKernelNodeParams
 *  {
 *      void* func;
 *      dim3 gridDim;
 *      dim3 blockDim;
 *      unsigned int sharedMemBytes;
 *      void **kernelParams;
 *      void **extra;
 *  };
 * \endcode
 *
 * When the graph is launched, the node will ilwoke kernel \p func on a (\p gridDim.x x
 * \p gridDim.y x \p gridDim.z) grid of blocks. Each block contains
 * (\p blockDim.x x \p blockDim.y x \p blockDim.z) threads.
 *
 * \p sharedMem sets the amount of dynamic shared memory that will be
 * available to each thread block.
 *
 * Kernel parameters to \p func can be specified in one of two ways:
 *
 * 1) Kernel parameters can be specified via \p kernelParams. If the kernel has N
 * parameters, then \p kernelParams needs to be an array of N pointers. Each pointer,
 * from \p kernelParams[0] to \p kernelParams[N-1], points to the region of memory from which the actual
 * parameter will be copied. The number of kernel parameters and their offsets and sizes do not need
 * to be specified as that information is retrieved directly from the kernel's image.
 *
 * 2) Kernel parameters can also be packaged by the application into a single buffer that is passed in
 * via \p extra. This places the burden on the application of knowing each kernel
 * parameter's size and alignment/padding within the buffer. The \p extra parameter exists
 * to allow this function to take additional less commonly used arguments. \p extra specifies
 * a list of names of extra settings and their corresponding values. Each extra setting name is
 * immediately followed by the corresponding value. The list must be terminated with either NULL or
 * LW_LAUNCH_PARAM_END.
 *
 * - ::LW_LAUNCH_PARAM_END, which indicates the end of the \p extra
 *   array;
 * - ::LW_LAUNCH_PARAM_BUFFER_POINTER, which specifies that the next
 *   value in \p extra will be a pointer to a buffer
 *   containing all the kernel parameters for launching kernel
 *   \p func;
 * - ::LW_LAUNCH_PARAM_BUFFER_SIZE, which specifies that the next
 *   value in \p extra will be a pointer to a size_t
 *   containing the size of the buffer specified with
 *   ::LW_LAUNCH_PARAM_BUFFER_POINTER;
 *
 * The error ::lwdaErrorIlwalidValue will be returned if kernel parameters are specified with both
 * \p kernelParams and \p extra (i.e. both \p kernelParams and
 * \p extra are non-NULL).
 *
 * The \p kernelParams or \p extra array, as well as the argument values it points to,
 * are copied during this call.
 *
 * \note Kernels launched using graphs must not use texture and surface references. Reading or
 *       writing through any texture or surface reference is undefined behavior.
 *       This restriction does not apply to texture and surface objects.
 *
 * \param pGraphNode     - Returns newly created node
 * \param graph          - Graph to which to add the node
 * \param pDependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param pNodeParams      - Parameters for the GPU exelwtion node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDeviceFunction
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchKernel,
 * ::lwdaGraphKernelNodeGetParams,
 * ::lwdaGraphKernelNodeSetParams,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddKernelNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, const struct lwdaKernelNodeParams *pNodeParams);

/**
 * \brief Returns a kernel node's parameters
 *
 * Returns the parameters of kernel node \p node in \p pNodeParams.
 * The \p kernelParams or \p extra array returned in \p pNodeParams,
 * as well as the argument values it points to, are owned by the node.
 * This memory remains valid until the node is destroyed or its
 * parameters are modified, and should not be modified
 * directly. Use ::lwdaGraphKernelNodeSetParams to update the
 * parameters of this node.
 *
 * The params will contain either \p kernelParams or \p extra,
 * according to which of these was most recently set on the node.
 *
 * \param node        - Node to get the parameters for
 * \param pNodeParams - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDeviceFunction
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchKernel,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphKernelNodeSetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphKernelNodeGetParams(lwdaGraphNode_t node, struct lwdaKernelNodeParams *pNodeParams);

/**
 * \brief Sets a kernel node's parameters
 *
 * Sets the parameters of kernel node \p node to \p pNodeParams.
 *
 * \param node        - Node to set the parameters for
 * \param pNodeParams - Parameters to copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorMemoryAllocation
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchKernel,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphKernelNodeGetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphKernelNodeSetParams(lwdaGraphNode_t node, const struct lwdaKernelNodeParams *pNodeParams);

/**
 * \brief Copies attributes from source node to destination node.
 *
 * Copies attributes from source node \p src to destination node \p dst.
 * Both node must have the same context.
 *
 * \param[out] dst Destination node
 * \param[in] src Source node
 * For list of attributes see ::lwdaKernelNodeAttrID
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidContext
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphKernelNodeCopyAttributes(
        lwdaGraphNode_t hSrc,
        lwdaGraphNode_t hDst);

/**
 * \brief Queries node attribute.
 *
 * Queries attribute \p attr from node \p hNode and stores it in corresponding
 * member of \p value_out.
 *
 * \param[in] hNode
 * \param[in] attr
 * \param[out] value_out
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphKernelNodeGetAttribute(
    lwdaGraphNode_t hNode,
    lwdaKernelNodeAttrID attr,
    lwdaKernelNodeAttrValue *value_out);

/**
 * \brief Sets node attribute.
 *
 * Sets attribute \p attr on node \p hNode from corresponding attribute of
 * \p value.
 *
 * \param[out] hNode
 * \param[in] attr
 * \param[out] value
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle
 * \notefnerr
 *
 * \sa
 * ::lwdaAccessPolicyWindow
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphKernelNodeSetAttribute(
    lwdaGraphNode_t hNode,
    lwdaKernelNodeAttrID attr,
    const lwdaKernelNodeAttrValue *value);

/**
 * \brief Creates a memcpy node and adds it to a graph
 *
 * Creates a new memcpy node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * When the graph is launched, the node will perform the memcpy described by \p pCopyParams.
 * See ::lwdaMemcpy3D() for a description of the structure and its restrictions.
 *
 * Memcpy nodes have some additional restrictions with regards to managed memory, if the
 * system contains at least one device which has a zero value for the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess.
 *
 * \param pGraphNode     - Returns newly created node
 * \param graph          - Graph to which to add the node
 * \param pDependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param pCopyParams      - Parameters for the memory copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpy3D,
 * ::lwdaGraphAddMemcpyNodeToSymbol,
 * ::lwdaGraphAddMemcpyNodeFromSymbol,
 * ::lwdaGraphAddMemcpyNode1D,
 * ::lwdaGraphMemcpyNodeGetParams,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemsetNode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemcpyNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, const struct lwdaMemcpy3DParms *pCopyParams);

/**
 * \brief Creates a memcpy node to copy to a symbol on the device and adds it to a graph
 *
 * Creates a new memcpy node to copy to \p symbol and adds it to \p graph with
 * \p numDependencies dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * When the graph is launched, the node will copy \p count bytes from the memory area
 * pointed to by \p src to the memory area pointed to by \p offset bytes from the start
 * of symbol \p symbol. The memory areas may not overlap. \p symbol is a variable that
 * resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of
 * transfer is inferred from the pointer values. However, ::lwdaMemcpyDefault
 * is only allowed on systems that support unified virtual addressing.
 *
 * Memcpy nodes have some additional restrictions with regards to managed memory, if the
 * system contains at least one device which has a zero value for the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param symbol          - Device symbol address
 * \param src             - Source memory address
 * \param count           - Size in bytes to copy
 * \param offset          - Offset from start of symbol in bytes
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpyToSymbol,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemcpyNodeFromSymbol,
 * ::lwdaGraphMemcpyNodeGetParams,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemsetNode
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemcpyNodeToSymbol(
    lwdaGraphNode_t *pGraphNode,
    lwdaGraph_t graph,
    const lwdaGraphNode_t *pDependencies,
    size_t numDependencies,
    const void* symbol,
    const void* src,
    size_t count,
    size_t offset,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Creates a memcpy node to copy from a symbol on the device and adds it to a graph
 *
 * Creates a new memcpy node to copy from \p symbol and adds it to \p graph with
 * \p numDependencies dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * When the graph is launched, the node will copy \p count bytes from the memory area
 * pointed to by \p offset bytes from the start of symbol \p symbol to the memory area
 *  pointed to by \p dst. The memory areas may not overlap. \p symbol is a variable
 *  that resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyDeviceToHost, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of transfer
 * is inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * Memcpy nodes have some additional restrictions with regards to managed memory, if the
 * system contains at least one device which has a zero value for the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param dst             - Destination memory address
 * \param symbol          - Device symbol address
 * \param count           - Size in bytes to copy
 * \param offset          - Offset from start of symbol in bytes
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpyFromSymbol,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemcpyNodeToSymbol,
 * ::lwdaGraphMemcpyNodeGetParams,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemsetNode
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemcpyNodeFromSymbol(
    lwdaGraphNode_t* pGraphNode,
    lwdaGraph_t graph,
    const lwdaGraphNode_t* pDependencies,
    size_t numDependencies,
    void* dst,
    const void* symbol,
    size_t count,
    size_t offset,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Creates a 1D memcpy node and adds it to a graph
 *
 * Creates a new 1D memcpy node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * When the graph is launched, the node will copy \p count bytes from the memory
 * area pointed to by \p src to the memory area pointed to by \p dst, where
 * \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing. Launching a
 * memcpy node with dst and src pointers that do not match the direction of
 * the copy results in an undefined behavior.
 *
 * Memcpy nodes have some additional restrictions with regards to managed memory, if the
 * system contains at least one device which has a zero value for the device attribute
 * ::lwdaDevAttrConlwrrentManagedAccess.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param dst             - Destination memory address
 * \param src             - Source memory address
 * \param count           - Size in bytes to copy
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpy,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeGetParams,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParams1D,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemsetNode
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemcpyNode1D(
    lwdaGraphNode_t *pGraphNode,
    lwdaGraph_t graph,
    const lwdaGraphNode_t *pDependencies,
    size_t numDependencies,
    void* dst,
    const void* src,
    size_t count,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Returns a memcpy node's parameters
 *
 * Returns the parameters of memcpy node \p node in \p pNodeParams.
 *
 * \param node        - Node to get the parameters for
 * \param pNodeParams - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpy3D,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeSetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemcpyNodeGetParams(lwdaGraphNode_t node, struct lwdaMemcpy3DParms *pNodeParams);

/**
 * \brief Sets a memcpy node's parameters
 *
 * Sets the parameters of memcpy node \p node to \p pNodeParams.
 *
 * \param node        - Node to set the parameters for
 * \param pNodeParams - Parameters to copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpy3D,
 * ::lwdaGraphMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphMemcpyNodeSetParams1D,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeGetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemcpyNodeSetParams(lwdaGraphNode_t node, const struct lwdaMemcpy3DParms *pNodeParams);

/**
 * \brief Sets a memcpy node's parameters to copy to a symbol on the device
 *
 * Sets the parameters of memcpy node \p node to the copy described by the provided parameters.
 *
 * When the graph is launched, the node will copy \p count bytes from the memory area
 * pointed to by \p src to the memory area pointed to by \p offset bytes from the start
 * of symbol \p symbol. The memory areas may not overlap. \p symbol is a variable that
 * resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of
 * transfer is inferred from the pointer values. However, ::lwdaMemcpyDefault
 * is only allowed on systems that support unified virtual addressing.
 *
 * \param node            - Node to set the parameters for
 * \param symbol          - Device symbol address
 * \param src             - Source memory address
 * \param count           - Size in bytes to copy
 * \param offset          - Offset from start of symbol in bytes
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpyToSymbol,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeGetParams
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemcpyNodeSetParamsToSymbol(
    lwdaGraphNode_t node,
    const void* symbol,
    const void* src,
    size_t count,
    size_t offset,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Sets a memcpy node's parameters to copy from a symbol on the device
 *
 * Sets the parameters of memcpy node \p node to the copy described by the provided parameters.
 *
 * When the graph is launched, the node will copy \p count bytes from the memory area
 * pointed to by \p offset bytes from the start of symbol \p symbol to the memory area
 *  pointed to by \p dst. The memory areas may not overlap. \p symbol is a variable
 *  that resides in global or constant memory space. \p kind can be either
 * ::lwdaMemcpyDeviceToHost, ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault.
 * Passing ::lwdaMemcpyDefault is recommended, in which case the type of transfer
 * is inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing.
 *
 * \param node            - Node to set the parameters for
 * \param dst             - Destination memory address
 * \param symbol          - Device symbol address
 * \param count           - Size in bytes to copy
 * \param offset          - Offset from start of symbol in bytes
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpyFromSymbol,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeGetParams
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemcpyNodeSetParamsFromSymbol(
    lwdaGraphNode_t node,
    void* dst,
    const void* symbol,
    size_t count,
    size_t offset,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Sets a memcpy node's parameters to perform a 1-dimensional copy
 *
 * Sets the parameters of memcpy node \p node to the copy described by the provided parameters.
 *
 * When the graph is launched, the node will copy \p count bytes from the memory
 * area pointed to by \p src to the memory area pointed to by \p dst, where
 * \p kind specifies the direction of the copy, and must be one of
 * ::lwdaMemcpyHostToHost, ::lwdaMemcpyHostToDevice, ::lwdaMemcpyDeviceToHost,
 * ::lwdaMemcpyDeviceToDevice, or ::lwdaMemcpyDefault. Passing
 * ::lwdaMemcpyDefault is recommended, in which case the type of transfer is
 * inferred from the pointer values. However, ::lwdaMemcpyDefault is only
 * allowed on systems that support unified virtual addressing. Launching a
 * memcpy node with dst and src pointers that do not match the direction of
 * the copy results in an undefined behavior.
 *
 * \param node            - Node to set the parameters for
 * \param dst             - Destination memory address
 * \param src             - Source memory address
 * \param count           - Size in bytes to copy
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemcpy,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeGetParams
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemcpyNodeSetParams1D(
    lwdaGraphNode_t node,
    void* dst,
    const void* src,
    size_t count,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Creates a memset node and adds it to a graph
 *
 * Creates a new memset node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * The element size must be 1, 2, or 4 bytes.
 * When the graph is launched, the node will perform the memset described by \p pMemsetParams.
 *
 * \param pGraphNode     - Returns newly created node
 * \param graph          - Graph to which to add the node
 * \param pDependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param pMemsetParams    - Parameters for the memory set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidDevice
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemset2D,
 * ::lwdaGraphMemsetNodeGetParams,
 * ::lwdaGraphMemsetNodeSetParams,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemcpyNode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemsetNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, const struct lwdaMemsetParams *pMemsetParams);

/**
 * \brief Returns a memset node's parameters
 *
 * Returns the parameters of memset node \p node in \p pNodeParams.
 *
 * \param node        - Node to get the parameters for
 * \param pNodeParams - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemset2D,
 * ::lwdaGraphAddMemsetNode,
 * ::lwdaGraphMemsetNodeSetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemsetNodeGetParams(lwdaGraphNode_t node, struct lwdaMemsetParams *pNodeParams);

/**
 * \brief Sets a memset node's parameters
 *
 * Sets the parameters of memset node \p node to \p pNodeParams.
 *
 * \param node        - Node to set the parameters for
 * \param pNodeParams - Parameters to copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaMemset2D,
 * ::lwdaGraphAddMemsetNode,
 * ::lwdaGraphMemsetNodeGetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemsetNodeSetParams(lwdaGraphNode_t node, const struct lwdaMemsetParams *pNodeParams);

/**
 * \brief Creates a host exelwtion node and adds it to a graph
 *
 * Creates a new CPU exelwtion node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies and arguments specified in \p pNodeParams.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * When the graph is launched, the node will ilwoke the specified CPU function.
 * Host nodes are not supported under MPS with pre-Volta GPUs.
 *
 * \param pGraphNode     - Returns newly created node
 * \param graph          - Graph to which to add the node
 * \param pDependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param pNodeParams      - Parameters for the host node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchHostFunc,
 * ::lwdaGraphHostNodeGetParams,
 * ::lwdaGraphHostNodeSetParams,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddHostNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, const struct lwdaHostNodeParams *pNodeParams);

/**
 * \brief Returns a host node's parameters
 *
 * Returns the parameters of host node \p node in \p pNodeParams.
 *
 * \param node        - Node to get the parameters for
 * \param pNodeParams - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchHostFunc,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphHostNodeSetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphHostNodeGetParams(lwdaGraphNode_t node, struct lwdaHostNodeParams *pNodeParams);

/**
 * \brief Sets a host node's parameters
 *
 * Sets the parameters of host node \p node to \p nodeParams.
 *
 * \param node        - Node to set the parameters for
 * \param pNodeParams - Parameters to copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchHostFunc,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphHostNodeGetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphHostNodeSetParams(lwdaGraphNode_t node, const struct lwdaHostNodeParams *pNodeParams);

/**
 * \brief Creates a child graph node and adds it to a graph
 *
 * Creates a new node which exelwtes an embedded graph, and adds it to \p graph with
 * \p numDependencies dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * If \p hGraph contains allocation or free nodes, this call will return an error.
 *
 * The node exelwtes an embedded child graph. The child graph is cloned in this call.
 *
 * \param pGraphNode     - Returns newly created node
 * \param graph          - Graph to which to add the node
 * \param pDependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param childGraph      - The graph to clone into this node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphChildGraphNodeGetGraph,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode,
 * ::lwdaGraphClone
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddChildGraphNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, lwdaGraph_t childGraph);

/**
 * \brief Gets a handle to the embedded graph of a child graph node
 *
 * Gets a handle to the embedded graph in a child graph node. This call
 * does not clone the graph. Changes to the graph will be reflected in
 * the node, and the node retains ownership of the graph.
 *
 * Allocation and free nodes cannot be added to the returned graph.
 * Attempting to do so will return an error.
 *
 * \param node   - Node to get the embedded graph for
 * \param pGraph - Location to store a handle to the graph
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphNodeFindInClone
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphChildGraphNodeGetGraph(lwdaGraphNode_t node, lwdaGraph_t *pGraph);

/**
 * \brief Creates an empty node and adds it to a graph
 *
 * Creates a new node which performs no operation, and adds it to \p graph with
 * \p numDependencies dependencies specified via \p pDependencies.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p pGraphNode.
 *
 * An empty node performs no operation during exelwtion, but can be used for
 * transitive ordering. For example, a phased exelwtion graph with 2 groups of n
 * nodes with a barrier between them can be represented using an empty node and
 * 2*n dependency edges, rather than no empty node and n^2 dependency edges.
 *
 * \param pGraphNode     - Returns newly created node
 * \param graph          - Graph to which to add the node
 * \param pDependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddEmptyNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies);

/**
 * \brief Creates an event record node and adds it to a graph
 *
 * Creates a new event record node and adds it to \p hGraph with \p numDependencies
 * dependencies specified via \p dependencies and event specified in \p event.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p dependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p phGraphNode.
 *
 * Each launch of the graph will record \p event to capture exelwtion of the
 * node's dependencies.
 *
 * These nodes may not be used in loops or conditionals.
 *
 * \param phGraphNode     - Returns newly created node
 * \param hGraph          - Graph to which to add the node
 * \param dependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param event           - Event for the node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode,
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddEventRecordNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, lwdaEvent_t event);
#endif

/**
 * \brief Returns the event associated with an event record node
 *
 * Returns the event of event record node \p hNode in \p event_out.
 *
 * \param hNode     - Node to get the event for
 * \param event_out - Pointer to return the event
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphEventRecordNodeSetEvent,
 * ::lwdaGraphEventWaitNodeGetEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphEventRecordNodeGetEvent(lwdaGraphNode_t node, lwdaEvent_t *event_out);
#endif

/**
 * \brief Sets an event record node's event
 *
 * Sets the event of event record node \p hNode to \p event.
 *
 * \param hNode - Node to set the event for
 * \param event - Event to use
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphEventRecordNodeGetEvent,
 * ::lwdaGraphEventWaitNodeSetEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphEventRecordNodeSetEvent(lwdaGraphNode_t node, lwdaEvent_t event);
#endif

/**
 * \brief Creates an event wait node and adds it to a graph
 *
 * Creates a new event wait node and adds it to \p hGraph with \p numDependencies
 * dependencies specified via \p dependencies and event specified in \p event.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p dependencies may not have any duplicate entries.
 * A handle to the new node will be returned in \p phGraphNode.
 *
 * The graph node will wait for all work captured in \p event.  See ::lwEventRecord()
 * for details on what is captured by an event.  The synchronization will be performed
 * efficiently on the device when applicable.  \p event may be from a different context
 * or device than the launch stream.
 *
 * These nodes may not be used in loops or conditionals.
 *
 * \param phGraphNode     - Returns newly created node
 * \param hGraph          - Graph to which to add the node
 * \param dependencies    - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param event           - Event for the node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode,
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddEventWaitNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, lwdaEvent_t event);
#endif

/**
 * \brief Returns the event associated with an event wait node
 *
 * Returns the event of event wait node \p hNode in \p event_out.
 *
 * \param hNode     - Node to get the event for
 * \param event_out - Pointer to return the event
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphEventWaitNodeSetEvent,
 * ::lwdaGraphEventRecordNodeGetEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphEventWaitNodeGetEvent(lwdaGraphNode_t node, lwdaEvent_t *event_out);
#endif

/**
 * \brief Sets an event wait node's event
 *
 * Sets the event of event wait node \p hNode to \p event.
 *
 * \param hNode - Node to set the event for
 * \param event - Event to use
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphEventWaitNodeGetEvent,
 * ::lwdaGraphEventRecordNodeSetEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphEventWaitNodeSetEvent(lwdaGraphNode_t node, lwdaEvent_t event);
#endif

/**
 * \brief Creates an external semaphore signal node and adds it to a graph
 *
 * Creates a new external semaphore signal node and adds it to \p graph with \p
 * numDependencies dependencies specified via \p dependencies and arguments specified
 * in \p nodeParams. It is possible for \p numDependencies to be 0, in which case the
 * node will be placed at the root of the graph. \p dependencies may not have any
 * duplicate entries. A handle to the new node will be returned in \p pGraphNode.
 *
 * Performs a signal operation on a set of externally allocated semaphore objects
 * when the node is launched.  The operation(s) will occur after all of the node's
 * dependencies have completed.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param nodeParams      - Parameters for the node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphExternalSemaphoresSignalNodeGetParams,
 * ::lwdaGraphExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaImportExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode,
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddExternalSemaphoresSignalNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, const struct lwdaExternalSemaphoreSignalNodeParams *nodeParams);
#endif

/**
 * \brief Returns an external semaphore signal node's parameters
 *
 * Returns the parameters of an external semaphore signal node \p hNode in \p params_out.
 * The \p extSemArray and \p paramsArray returned in \p params_out,
 * are owned by the node.  This memory remains valid until the node is destroyed or its
 * parameters are modified, and should not be modified
 * directly. Use ::lwdaGraphExternalSemaphoresSignalNodeSetParams to update the
 * parameters of this node.
 *
 * \param hNode      - Node to get the parameters for
 * \param params_out - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchKernel,
 * ::lwdaGraphAddExternalSemaphoresSignalNode,
 * ::lwdaGraphExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExternalSemaphoresSignalNodeGetParams(lwdaGraphNode_t hNode, struct lwdaExternalSemaphoreSignalNodeParams *params_out);
#endif

/**
 * \brief Sets an external semaphore signal node's parameters
 *
 * Sets the parameters of an external semaphore signal node \p hNode to \p nodeParams.
 *
 * \param hNode      - Node to set the parameters for
 * \param nodeParams - Parameters to copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddExternalSemaphoresSignalNode,
 * ::lwdaGraphExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExternalSemaphoresSignalNodeSetParams(lwdaGraphNode_t hNode, const struct lwdaExternalSemaphoreSignalNodeParams *nodeParams);
#endif

/**
 * \brief Creates an external semaphore wait node and adds it to a graph
 *
 * Creates a new external semaphore wait node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p dependencies and arguments specified in \p nodeParams.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p dependencies may not have any duplicate entries. A handle
 * to the new node will be returned in \p pGraphNode.
 *
 * Performs a wait operation on a set of externally allocated semaphore objects
 * when the node is launched.  The node's dependencies will not be launched until
 * the wait operation has completed.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param nodeParams      - Parameters for the node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphExternalSemaphoresWaitNodeGetParams,
 * ::lwdaGraphExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphAddExternalSemaphoresSignalNode,
 * ::lwdaImportExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode,
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddExternalSemaphoresWaitNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, const struct lwdaExternalSemaphoreWaitNodeParams *nodeParams);
#endif

/**
 * \brief Returns an external semaphore wait node's parameters
 *
 * Returns the parameters of an external semaphore wait node \p hNode in \p params_out.
 * The \p extSemArray and \p paramsArray returned in \p params_out,
 * are owned by the node.  This memory remains valid until the node is destroyed or its
 * parameters are modified, and should not be modified
 * directly. Use ::lwdaGraphExternalSemaphoresSignalNodeSetParams to update the
 * parameters of this node.
 *
 * \param hNode      - Node to get the parameters for
 * \param params_out - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaLaunchKernel,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaGraphExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExternalSemaphoresWaitNodeGetParams(lwdaGraphNode_t hNode, struct lwdaExternalSemaphoreWaitNodeParams *params_out);
#endif

/**
 * \brief Sets an external semaphore wait node's parameters
 *
 * Sets the parameters of an external semaphore wait node \p hNode to \p nodeParams.
 *
 * \param hNode      - Node to set the parameters for
 * \param nodeParams - Parameters to copy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaGraphExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExternalSemaphoresWaitNodeSetParams(lwdaGraphNode_t hNode, const struct lwdaExternalSemaphoreWaitNodeParams *nodeParams);
#endif

/**
 * \brief Creates an allocation node and adds it to a graph
 *
 * Creates a new allocation node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies and arguments specified in \p nodeParams.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries. A handle
 * to the new node will be returned in \p pGraphNode.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param nodeParams      - Parameters for the node
 *
 * When ::lwdaGraphAddMemAllocNode creates an allocation node, it returns the address of the allocation in
 * \p nodeParams.dptr.  The allocation's address remains fixed across instantiations and launches.
 *
 * If the allocation is freed in the same graph, by creating a free node using ::lwdaGraphAddMemFreeNode,
 * the allocation can be accessed by nodes ordered after the allocation node but before the free node.
 * These allocations cannot be freed outside the owning graph, and they can only be freed once in the
 * owning graph.
 *
 * If the allocation is not freed in the same graph, then it can be accessed not only by nodes in the
 * graph which are ordered after the allocation node, but also by stream operations ordered after the
 * graph's exelwtion but before the allocation is freed.
 *
 * Allocations which are not freed in the same graph can be freed by:
 * - passing the allocation to ::lwdaMemFreeAsync or ::lwdaMemFree;
 * - launching a graph with a free node for that allocation; or
 * - specifying ::lwdaGraphInstantiateFlagAutoFreeOnLaunch during instantiation, which makes
 *   each launch behave as though it called ::lwdaMemFreeAsync for every unfreed allocation.
 *
 * It is not possible to free an allocation in both the owning graph and another graph.  If the allocation
 * is freed in the same graph, a free node cannot be added to another graph.  If the allocation is freed
 * in another graph, a free node can no longer be added to the owning graph.
 *
 * The following restrictions apply to graphs which contain allocation and/or memory free nodes:
 * - Nodes and edges of the graph cannot be deleted.
 * - The graph cannot be used in a child node.
 * - Only one instantiation of the graph may exist at any point in time.
 * - The graph cannot be cloned.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorLwdartUnloading,
 * ::lwdaErrorInitializationError,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorOutOfMemory
 * \note_graph_thread_safety
 * \notefnerr
 *
 * \sa
 * ::lwdaGraphAddMemFreeNode,
 * ::lwdaGraphMemAllocNodeGetParams,
 * ::lwdaDeviceGraphMemTrim,
 * ::lwdaDeviceGetGraphMemAttribute,
 * ::lwdaDeviceSetGraphMemAttribute,
 * ::lwdaMallocAsync,
 * ::lwdaFreeAsync,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphAddExternalSemaphoresSignalNode,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemAllocNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, struct lwdaMemAllocNodeParams *nodeParams);
#endif

/**
 * \brief Returns a memory alloc node's parameters
 *
 * Returns the parameters of a memory alloc node \p hNode in \p params_out.
 * The \p poolProps and \p accessDescs returned in \p params_out, are owned by the
 * node.  This memory remains valid until the node is destroyed.  The returned
 * parameters must not be modified.
 *
 * \param node       - Node to get the parameters for
 * \param params_out - Pointer to return the parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemAllocNode,
 * ::lwdaGraphMemFreeNodeGetParams
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemAllocNodeGetParams(lwdaGraphNode_t node, struct lwdaMemAllocNodeParams *params_out);
#endif

/**
 * \brief Creates a memory free node and adds it to a graph
 *
 * Creates a new memory free node and adds it to \p graph with \p numDependencies
 * dependencies specified via \p pDependencies and address specified in \p dptr.
 * It is possible for \p numDependencies to be 0, in which case the node will be placed
 * at the root of the graph. \p pDependencies may not have any duplicate entries. A handle
 * to the new node will be returned in \p pGraphNode.
 *
 * \param pGraphNode      - Returns newly created node
 * \param graph           - Graph to which to add the node
 * \param pDependencies   - Dependencies of the node
 * \param numDependencies - Number of dependencies
 * \param dptr            - Address of memory to free
 *
 * ::lwdaGraphAddMemFreeNode will return ::lwdaErrorIlwalidValue if the user attempts to free:
 * - an allocation twice in the same graph.
 * - an address that was not returned by an allocation node.
 * - an invalid address.
 *
 * The following restrictions apply to graphs which contain allocation and/or memory free nodes:
 * - Nodes and edges of the graph cannot be deleted.
 * - The graph cannot be used in a child node.
 * - Only one instantiation of the graph may exist at any point in time.
 * - The graph cannot be cloned.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorLwdartUnloading,
 * ::lwdaErrorInitializationError,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorOutOfMemory
 * \note_graph_thread_safety
 * \notefnerr
 *
 * \sa
 * ::lwdaGraphAddMemAllocNode,
 * ::lwdaGraphMemFreeNodeGetParams,
 * ::lwdaDeviceGraphMemTrim,
 * ::lwdaDeviceGetGraphMemAttribute,
 * ::lwdaDeviceSetGraphMemAttribute,
 * ::lwdaMallocAsync,
 * ::lwdaFreeAsync,
 * ::lwdaGraphCreate,
 * ::lwdaGraphDestroyNode,
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphAddExternalSemaphoresSignalNode,
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddMemFreeNode(lwdaGraphNode_t *pGraphNode, lwdaGraph_t graph, const lwdaGraphNode_t *pDependencies, size_t numDependencies, void *dptr);
#endif

/**
 * \brief Returns a memory free node's parameters
 *
 * Returns the address of a memory free node \p hNode in \p dptr_out.
 *
 * \param node     - Node to get the parameters for
 * \param dptr_out - Pointer to return the device address
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemFreeNode,
 * ::lwdaGraphMemFreeNodeGetParams
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaGraphMemFreeNodeGetParams(lwdaGraphNode_t node, void *dptr_out);
#endif

/**
 * \brief Free unused memory that was cached on the specified device for use with graphs back to the OS.
 *
 * Blocks which are not in use by a graph that is either lwrrently exelwting or scheduled to execute are
 * freed back to the operating system.
 *
 * \param device - The device for which cached memory should be freed.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemAllocNode,
 * ::lwdaGraphAddMemFreeNode,
 * ::lwdaDeviceGetGraphMemAttribute,
 * ::lwdaDeviceSetGraphMemAttribute,
 * ::lwdaMallocAsync,
 * ::lwdaFreeAsync,
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGraphMemTrim(int device);
#endif

/**
 * \brief Query asynchronous allocation attributes related to graphs
 *
 * Valid attributes are:
 *
 * - ::lwdaGraphMemAttrUsedMemLwrrent: Amount of memory, in bytes, lwrrently associated with graphs
 * - ::lwdaGraphMemAttrUsedMemHigh: High watermark of memory, in bytes, associated with graphs since the
 *   last time it was reset.  High watermark can only be reset to zero.
 * - ::lwdaGraphMemAttrReservedMemLwrrent: Amount of memory, in bytes, lwrrently allocated for use by
 *   the LWCA graphs asynchronous allocator.
 * - ::lwdaGraphMemAttrReservedMemHigh: High watermark of memory, in bytes, lwrrently allocated for use by
 *   the LWCA graphs asynchronous allocator.
 *
 * \param device - Specifies the scope of the query
 * \param attr - attribute to get
 * \param value - retrieved value
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceSetGraphMemAttribute,
 * ::lwdaGraphAddMemAllocNode,
 * ::lwdaGraphAddMemFreeNode,
 * ::lwdaDeviceGraphMemTrim,
 * ::lwdaMallocAsync,
 * ::lwdaFreeAsync,
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceGetGraphMemAttribute(int device, enum lwdaGraphMemAttributeType attr, void* value);
#endif

/**
 * \brief Set asynchronous allocation attributes related to graphs
 *
 * Valid attributes are:
 *
 * - ::lwdaGraphMemAttrUsedMemHigh: High watermark of memory, in bytes, associated with graphs since the
 *   last time it was reset.  High watermark can only be reset to zero.
 * - ::lwdaGraphMemAttrReservedMemHigh: High watermark of memory, in bytes, lwrrently allocated for use by
 *   the LWCA graphs asynchronous allocator.
 *
 * \param device - Specifies the scope of the query
 * \param attr - attribute to get
 * \param value - pointer to value to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaDeviceGetGraphMemAttribute,
 * ::lwdaGraphAddMemAllocNode,
 * ::lwdaGraphAddMemFreeNode,
 * ::lwdaDeviceGraphMemTrim,
 * ::lwdaMallocAsync,
 * ::lwdaFreeAsync,
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaDeviceSetGraphMemAttribute(int device, enum lwdaGraphMemAttributeType attr, void* value);
#endif

/**
 * \brief Clones a graph
 *
 * This function creates a copy of \p originalGraph and returns it in \p pGraphClone.
 * All parameters are copied into the cloned graph. The original graph may be modified 
 * after this call without affecting the clone.
 *
 * Child graph nodes in the original graph are relwrsively copied into the clone.
 *
 * \param pGraphClone  - Returns newly created cloned graph
 * \param originalGraph - Graph to clone
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorMemoryAllocation
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphCreate,
 * ::lwdaGraphNodeFindInClone
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphClone(lwdaGraph_t *pGraphClone, lwdaGraph_t originalGraph);

/**
 * \brief Finds a cloned version of a node
 *
 * This function returns the node in \p clonedGraph corresponding to \p originalNode 
 * in the original graph.
 *
 * \p clonedGraph must have been cloned from \p originalGraph via ::lwdaGraphClone. 
 * \p originalNode must have been in \p originalGraph at the time of the call to 
 * ::lwdaGraphClone, and the corresponding cloned node in \p clonedGraph must not have 
 * been removed. The cloned node is then returned via \p pClonedNode.
 *
 * \param pNode  - Returns handle to the cloned node
 * \param originalNode - Handle to the original node
 * \param clonedGraph - Cloned graph to query
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphClone
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphNodeFindInClone(lwdaGraphNode_t *pNode, lwdaGraphNode_t originalNode, lwdaGraph_t clonedGraph);

/**
 * \brief Returns a node's type
 *
 * Returns the node type of \p node in \p pType.
 *
 * \param node - Node to query
 * \param pType  - Pointer to return the node type
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphGetNodes,
 * ::lwdaGraphGetRootNodes,
 * ::lwdaGraphChildGraphNodeGetGraph,
 * ::lwdaGraphKernelNodeGetParams,
 * ::lwdaGraphKernelNodeSetParams,
 * ::lwdaGraphHostNodeGetParams,
 * ::lwdaGraphHostNodeSetParams,
 * ::lwdaGraphMemcpyNodeGetParams,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemsetNodeGetParams,
 * ::lwdaGraphMemsetNodeSetParams
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphNodeGetType(lwdaGraphNode_t node, enum lwdaGraphNodeType *pType);

/**
 * \brief Returns a graph's nodes
 *
 * Returns a list of \p graph's nodes. \p nodes may be NULL, in which case this
 * function will return the number of nodes in \p numNodes. Otherwise,
 * \p numNodes entries will be filled in. If \p numNodes is higher than the actual
 * number of nodes, the remaining entries in \p nodes will be set to NULL, and the
 * number of nodes actually obtained will be returned in \p numNodes.
 *
 * \param graph    - Graph to query
 * \param nodes    - Pointer to return the nodes
 * \param numNodes - See description
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphCreate,
 * ::lwdaGraphGetRootNodes,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphNodeGetType,
 * ::lwdaGraphNodeGetDependencies,
 * ::lwdaGraphNodeGetDependentNodes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphGetNodes(lwdaGraph_t graph, lwdaGraphNode_t *nodes, size_t *numNodes);

/**
 * \brief Returns a graph's root nodes
 *
 * Returns a list of \p graph's root nodes. \p pRootNodes may be NULL, in which case this
 * function will return the number of root nodes in \p pNumRootNodes. Otherwise,
 * \p pNumRootNodes entries will be filled in. If \p pNumRootNodes is higher than the actual
 * number of root nodes, the remaining entries in \p pRootNodes will be set to NULL, and the
 * number of nodes actually obtained will be returned in \p pNumRootNodes.
 *
 * \param graph       - Graph to query
 * \param pRootNodes    - Pointer to return the root nodes
 * \param pNumRootNodes - See description
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphCreate,
 * ::lwdaGraphGetNodes,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphNodeGetType,
 * ::lwdaGraphNodeGetDependencies,
 * ::lwdaGraphNodeGetDependentNodes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphGetRootNodes(lwdaGraph_t graph, lwdaGraphNode_t *pRootNodes, size_t *pNumRootNodes);

/**
 * \brief Returns a graph's dependency edges
 *
 * Returns a list of \p graph's dependency edges. Edges are returned via corresponding
 * indices in \p from and \p to; that is, the node in \p to[i] has a dependency on the
 * node in \p from[i]. \p from and \p to may both be NULL, in which
 * case this function only returns the number of edges in \p numEdges. Otherwise,
 * \p numEdges entries will be filled in. If \p numEdges is higher than the actual
 * number of edges, the remaining entries in \p from and \p to will be set to NULL, and
 * the number of edges actually returned will be written to \p numEdges.
 *
 * \param graph    - Graph to get the edges from
 * \param from     - Location to return edge endpoints
 * \param to       - Location to return edge endpoints
 * \param numEdges - See description
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphGetNodes,
 * ::lwdaGraphGetRootNodes,
 * ::lwdaGraphAddDependencies,
 * ::lwdaGraphRemoveDependencies,
 * ::lwdaGraphNodeGetDependencies,
 * ::lwdaGraphNodeGetDependentNodes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphGetEdges(lwdaGraph_t graph, lwdaGraphNode_t *from, lwdaGraphNode_t *to, size_t *numEdges);

/**
 * \brief Returns a node's dependencies
 *
 * Returns a list of \p node's dependencies. \p pDependencies may be NULL, in which case this
 * function will return the number of dependencies in \p pNumDependencies. Otherwise,
 * \p pNumDependencies entries will be filled in. If \p pNumDependencies is higher than the actual
 * number of dependencies, the remaining entries in \p pDependencies will be set to NULL, and the
 * number of nodes actually obtained will be returned in \p pNumDependencies.
 *
 * \param node           - Node to query
 * \param pDependencies    - Pointer to return the dependencies
 * \param pNumDependencies - See description
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphNodeGetDependentNodes,
 * ::lwdaGraphGetNodes,
 * ::lwdaGraphGetRootNodes,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphAddDependencies,
 * ::lwdaGraphRemoveDependencies
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphNodeGetDependencies(lwdaGraphNode_t node, lwdaGraphNode_t *pDependencies, size_t *pNumDependencies);

/**
 * \brief Returns a node's dependent nodes
 *
 * Returns a list of \p node's dependent nodes. \p pDependentNodes may be NULL, in which
 * case this function will return the number of dependent nodes in \p pNumDependentNodes.
 * Otherwise, \p pNumDependentNodes entries will be filled in. If \p pNumDependentNodes is
 * higher than the actual number of dependent nodes, the remaining entries in
 * \p pDependentNodes will be set to NULL, and the number of nodes actually obtained will
 * be returned in \p pNumDependentNodes.
 *
 * \param node             - Node to query
 * \param pDependentNodes    - Pointer to return the dependent nodes
 * \param pNumDependentNodes - See description
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphNodeGetDependencies,
 * ::lwdaGraphGetNodes,
 * ::lwdaGraphGetRootNodes,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphAddDependencies,
 * ::lwdaGraphRemoveDependencies
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphNodeGetDependentNodes(lwdaGraphNode_t node, lwdaGraphNode_t *pDependentNodes, size_t *pNumDependentNodes);

/**
 * \brief Adds dependency edges to a graph.
 *
 * The number of dependencies to be added is defined by \p numDependencies
 * Elements in \p pFrom and \p pTo at corresponding indices define a dependency.
 * Each node in \p pFrom and \p pTo must belong to \p graph.
 *
 * If \p numDependencies is 0, elements in \p pFrom and \p pTo will be ignored.
 * Specifying an existing dependency will return an error.
 *
 * \param graph - Graph to which dependencies are added
 * \param from - Array of nodes that provide the dependencies
 * \param to - Array of dependent nodes
 * \param numDependencies - Number of dependencies to be added
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphRemoveDependencies,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphNodeGetDependencies,
 * ::lwdaGraphNodeGetDependentNodes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphAddDependencies(lwdaGraph_t graph, const lwdaGraphNode_t *from, const lwdaGraphNode_t *to, size_t numDependencies);

/**
 * \brief Removes dependency edges from a graph.
 *
 * The number of \p pDependencies to be removed is defined by \p numDependencies.
 * Elements in \p pFrom and \p pTo at corresponding indices define a dependency.
 * Each node in \p pFrom and \p pTo must belong to \p graph.
 *
 * If \p numDependencies is 0, elements in \p pFrom and \p pTo will be ignored.
 * Specifying a non-existing dependency will return an error.
 *
 * \param graph - Graph from which to remove dependencies
 * \param from - Array of nodes that provide the dependencies
 * \param to - Array of dependent nodes
 * \param numDependencies - Number of dependencies to be removed
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddDependencies,
 * ::lwdaGraphGetEdges,
 * ::lwdaGraphNodeGetDependencies,
 * ::lwdaGraphNodeGetDependentNodes
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphRemoveDependencies(lwdaGraph_t graph, const lwdaGraphNode_t *from, const lwdaGraphNode_t *to, size_t numDependencies);

/**
 * \brief Remove a node from the graph
 *
 * Removes \p node from its graph. This operation also severs any dependencies of other nodes 
 * on \p node and vice versa.
 *
 * Dependencies cannot be removed from graphs which contain allocation or free nodes.
 * Any attempt to do so will return an error.
 *
 * \param node  - Node to remove
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphAddEmptyNode,
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemsetNode
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphDestroyNode(lwdaGraphNode_t node);

/**
 * \brief Creates an exelwtable graph from a graph
 *
 * Instantiates \p graph as an exelwtable graph. The graph is validated for any
 * structural constraints or intra-node constraints which were not previously
 * validated. If instantiation is successful, a handle to the instantiated graph
 * is returned in \p pGraphExec.
 *
 * If there are any errors, diagnostic information may be returned in \p pErrorNode and
 * \p pLogBuffer. This is the primary way to inspect instantiation errors. The output
 * will be null terminated unless the diagnostics overflow
 * the buffer. In this case, they will be truncated, and the last byte can be
 * inspected to determine if truncation oclwrred.
 *
 * \param pGraphExec - Returns instantiated graph
 * \param graph      - Graph to instantiate
 * \param pErrorNode - In case of an instantiation error, this may be modified to
 *                      indicate a node contributing to the error
 * \param pLogBuffer   - A character buffer to store diagnostic messages
 * \param bufferSize  - Size of the log buffer in bytes
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphInstantiateWithFlags,
 * ::lwdaGraphCreate,
 * ::lwdaGraphUpload,
 * ::lwdaGraphLaunch,
 * ::lwdaGraphExecDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphInstantiate(lwdaGraphExec_t *pGraphExec, lwdaGraph_t graph, lwdaGraphNode_t *pErrorNode, char *pLogBuffer, size_t bufferSize);

/**
 * \brief Creates an exelwtable graph from a graph
 *
 * Instantiates \p graph as an exelwtable graph. The graph is validated for any
 * structural constraints or intra-node constraints which were not previously
 * validated. If instantiation is successful, a handle to the instantiated graph
 * is returned in \p pGraphExec.
 *
 * The \p flags parameter controls the behavior of instantiation and subsequent
 * graph launches.  Valid flags are:
 *
 * - ::lwdaGraphInstantiateFlagAutoFreeOnLaunch, which configures a
 * graph containing memory allocation nodes to automatically free any
 * unfreed memory allocations before the graph is relaunched.
 *
 * If \p graph contains any allocation or free nodes, there can be at most one
 * exelwtable graph in existence for that graph at a time.
 *
 * An attempt to instantiate a second exelwtable graph before destroying the first
 * with ::lwdaGraphExecDestroy will result in an error.
 *
 * \param pGraphExec - Returns instantiated graph
 * \param graph      - Graph to instantiate
 * \param flags      - Flags to control instantiation.  See ::LWgraphInstantiate_flags.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphInstantiate,
 * ::lwdaGraphCreate,
 * ::lwdaGraphUpload,
 * ::lwdaGraphLaunch,
 * ::lwdaGraphExecDestroy
 */
#if __LWDART_API_VERSION >= 11040
extern __host__ lwdaError_t LWDARTAPI lwdaGraphInstantiateWithFlags(lwdaGraphExec_t *pGraphExec, lwdaGraph_t graph, unsigned long long flags);
#endif

/**
 * \brief Sets the parameters for a kernel node in the given graphExec
 *
 * Sets the parameters of a kernel node in an exelwtable graph \p hGraphExec. 
 * The node is identified by the corresponding node \p node in the 
 * non-exelwtable graph, from which the exelwtable graph was instantiated. 
 *
 * \p node must not have been removed from the original graph. The \p func field 
 * of \p nodeParams cannot be modified and must match the original value.
 * All other values can be modified. 
 *
 * The modifications only affect future launches of \p hGraphExec. Already 
 * enqueued or running launches of \p hGraphExec are not affected by this call. 
 * \p node is also not modified by this call.
 *
 * \param hGraphExec  - The exelwtable graph in which to set the specified node
 * \param node        - kernel node from the graph from which graphExec was instantiated
 * \param pNodeParams - Updated Parameters to set
 * 
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddKernelNode,
 * ::lwdaGraphKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecKernelNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t node, const struct lwdaKernelNodeParams *pNodeParams);

/**
 * \brief Sets the parameters for a memcpy node in the given graphExec.
 *
 * Updates the work represented by \p node in \p hGraphExec as though \p node had 
 * contained \p pNodeParams at instantiation.  \p node must remain in the graph which was 
 * used to instantiate \p hGraphExec.  Changed edges to and from \p node are ignored.
 *
 * The source and destination memory in \p pNodeParams must be allocated from the same 
 * contexts as the original source and destination memory.  Both the instantiation-time 
 * memory operands and the memory operands in \p pNodeParams must be 1-dimensional.
 * Zero-length operations are not supported.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued 
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also 
 * not modified by this call.
 *
 * Returns ::lwdaErrorIlwalidValue if the memory operands' mappings changed or
 * either the original or new memory operands are multidimensional.
 *
 * \param hGraphExec  - The exelwtable graph in which to set the specified node
 * \param node        - Memcpy node from the graph which was used to instantiate graphExec
 * \param pNodeParams - Updated Parameters to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphExecMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphExecMemcpyNodeSetParams1D,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecMemcpyNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t node, const struct lwdaMemcpy3DParms *pNodeParams);

/**
 * \brief Sets the parameters for a memcpy node in the given graphExec to copy to a symbol on the device
 *
 * Updates the work represented by \p node in \p hGraphExec as though \p node had 
 * contained the given params at instantiation.  \p node must remain in the graph which was 
 * used to instantiate \p hGraphExec.  Changed edges to and from \p node are ignored.
 *
 * \p src and \p symbol must be allocated from the same contexts as the original source and
 * destination memory.  The instantiation-time memory operands must be 1-dimensional.
 * Zero-length operations are not supported.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued 
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also 
 * not modified by this call.
 *
 * Returns ::lwdaErrorIlwalidValue if the memory operands' mappings changed or
 * the original memory operands are multidimensional.
 *
 * \param hGraphExec      - The exelwtable graph in which to set the specified node
 * \param node            - Memcpy node from the graph which was used to instantiate graphExec
 * \param symbol          - Device symbol address
 * \param src             - Source memory address
 * \param count           - Size in bytes to copy
 * \param offset          - Offset from start of symbol in bytes
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemcpyNodeToSymbol,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecMemcpyNodeSetParamsToSymbol(
    lwdaGraphExec_t hGraphExec,
    lwdaGraphNode_t node,
    const void* symbol,
    const void* src,
    size_t count,
    size_t offset,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Sets the parameters for a memcpy node in the given graphExec to copy from a symbol on the device
 *
 * Updates the work represented by \p node in \p hGraphExec as though \p node had 
 * contained the given params at instantiation.  \p node must remain in the graph which was 
 * used to instantiate \p hGraphExec.  Changed edges to and from \p node are ignored.
 *
 * \p symbol and \p dst must be allocated from the same contexts as the original source and
 * destination memory.  The instantiation-time memory operands must be 1-dimensional.
 * Zero-length operations are not supported.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued 
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also 
 * not modified by this call.
 *
 * Returns ::lwdaErrorIlwalidValue if the memory operands' mappings changed or
 * the original memory operands are multidimensional.
 *
 * \param hGraphExec      - The exelwtable graph in which to set the specified node
 * \param node            - Memcpy node from the graph which was used to instantiate graphExec
 * \param dst             - Destination memory address
 * \param symbol          - Device symbol address
 * \param count           - Size in bytes to copy
 * \param offset          - Offset from start of symbol in bytes
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemcpyNodeFromSymbol,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParamsFromSymbol,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParamsToSymbol,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecMemcpyNodeSetParamsFromSymbol(
    lwdaGraphExec_t hGraphExec,
    lwdaGraphNode_t node,
    void* dst,
    const void* symbol,
    size_t count,
    size_t offset,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Sets the parameters for a memcpy node in the given graphExec to perform a 1-dimensional copy
 *
 * Updates the work represented by \p node in \p hGraphExec as though \p node had 
 * contained the given params at instantiation.  \p node must remain in the graph which was 
 * used to instantiate \p hGraphExec.  Changed edges to and from \p node are ignored.
 *
 * \p src and \p dst must be allocated from the same contexts as the original source
 * and destination memory.  The instantiation-time memory operands must be 1-dimensional.
 * Zero-length operations are not supported.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued 
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also 
 * not modified by this call.
 *
 * Returns ::lwdaErrorIlwalidValue if the memory operands' mappings changed or
 * the original memory operands are multidimensional.
 *
 * \param hGraphExec      - The exelwtable graph in which to set the specified node
 * \param node            - Memcpy node from the graph which was used to instantiate graphExec
 * \param dst             - Destination memory address
 * \param src             - Source memory address
 * \param count           - Size in bytes to copy
 * \param kind            - Type of transfer
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemcpyNode,
 * ::lwdaGraphAddMemcpyNode1D,
 * ::lwdaGraphMemcpyNodeSetParams,
 * ::lwdaGraphMemcpyNodeSetParams1D,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecMemcpyNodeSetParams1D(
    lwdaGraphExec_t hGraphExec,
    lwdaGraphNode_t node,
    void* dst,
    const void* src,
    size_t count,
    enum lwdaMemcpyKind kind);
#endif

/**
 * \brief Sets the parameters for a memset node in the given graphExec.
 *
 * Updates the work represented by \p node in \p hGraphExec as though \p node had 
 * contained \p pNodeParams at instantiation.  \p node must remain in the graph which was 
 * used to instantiate \p hGraphExec.  Changed edges to and from \p node are ignored.
 *
 * The destination memory in \p pNodeParams must be allocated from the same 
 * context as the original destination memory.  Both the instantiation-time 
 * memory operand and the memory operand in \p pNodeParams must be 1-dimensional.
 * Zero-length operations are not supported.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued 
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also 
 * not modified by this call.
 *
 * Returns lwdaErrorIlwalidValue if the memory operand's mappings changed or
 * either the original or new memory operand are multidimensional.
 *
 * \param hGraphExec  - The exelwtable graph in which to set the specified node
 * \param node        - Memset node from the graph which was used to instantiate graphExec
 * \param pNodeParams - Updated Parameters to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddMemsetNode,
 * ::lwdaGraphMemsetNodeSetParams,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecMemsetNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t node, const struct lwdaMemsetParams *pNodeParams);

/**
 * \brief Sets the parameters for a host node in the given graphExec.
 *
 * Updates the work represented by \p node in \p hGraphExec as though \p node had 
 * contained \p pNodeParams at instantiation.  \p node must remain in the graph which was 
 * used to instantiate \p hGraphExec.  Changed edges to and from \p node are ignored.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued 
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also 
 * not modified by this call.
 *
 * \param hGraphExec  - The exelwtable graph in which to set the specified node
 * \param node        - Host node from the graph which was used to instantiate graphExec
 * \param pNodeParams - Updated Parameters to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddHostNode,
 * ::lwdaGraphHostNodeSetParams,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecHostNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t node, const struct lwdaHostNodeParams *pNodeParams);

/**
 * \brief Updates node parameters in the child graph node in the given graphExec.
 *
 * Updates the work represented by \p node in \p hGraphExec as though the nodes contained
 * in \p node's graph had the parameters contained in \p childGraph's nodes at instantiation.
 * \p node must remain in the graph which was used to instantiate \p hGraphExec.
 * Changed edges to and from \p node are ignored.
 *
 * The modifications only affect future launches of \p hGraphExec.  Already enqueued
 * or running launches of \p hGraphExec are not affected by this call.  \p node is also
 * not modified by this call.
 *
 * The topology of \p childGraph, as well as the node insertion order,  must match that
 * of the graph contained in \p node.  See ::lwdaGraphExelwpdate() for a list of restrictions
 * on what can be updated in an instantiated graph.  The update is relwrsive, so child graph
 * nodes contained within the top level child graph will also be updated.

 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param node       - Host node from the graph which was used to instantiate graphExec
 * \param childGraph - The graph supplying the updated parameters
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddChildGraphNode,
 * ::lwdaGraphChildGraphNodeGetGraph,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecChildGraphNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t node, lwdaGraph_t childGraph);
#endif

/**
 * \brief Sets the event for an event record node in the given graphExec
 *
 * Sets the event of an event record node in an exelwtable graph \p hGraphExec.
 * The node is identified by the corresponding node \p hNode in the
 * non-exelwtable graph, from which the exelwtable graph was instantiated.
 *
 * The modifications only affect future launches of \p hGraphExec. Already
 * enqueued or running launches of \p hGraphExec are not affected by this call.
 * \p hNode is also not modified by this call.
 *
 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param hNode      - Event record node from the graph from which graphExec was instantiated
 * \param event      - Updated event to use
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventRecordNode,
 * ::lwdaGraphEventRecordNodeGetEvent,
 * ::lwdaGraphEventWaitNodeSetEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecEventRecordNodeSetEvent(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t hNode, lwdaEvent_t event);
#endif

/**
 * \brief Sets the event for an event wait node in the given graphExec
 *
 * Sets the event of an event wait node in an exelwtable graph \p hGraphExec.
 * The node is identified by the corresponding node \p hNode in the
 * non-exelwtable graph, from which the exelwtable graph was instantiated.
 *
 * The modifications only affect future launches of \p hGraphExec. Already
 * enqueued or running launches of \p hGraphExec are not affected by this call.
 * \p hNode is also not modified by this call.
 *
 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param hNode      - Event wait node from the graph from which graphExec was instantiated
 * \param event      - Updated event to use
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddEventWaitNode,
 * ::lwdaGraphEventWaitNodeGetEvent,
 * ::lwdaGraphEventRecordNodeSetEvent,
 * ::lwdaEventRecordWithFlags,
 * ::lwdaStreamWaitEvent,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecEventWaitNodeSetEvent(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t hNode, lwdaEvent_t event);
#endif

/**
 * \brief Sets the parameters for an external semaphore signal node in the given graphExec
 *
 * Sets the parameters of an external semaphore signal node in an exelwtable graph \p hGraphExec.
 * The node is identified by the corresponding node \p hNode in the
 * non-exelwtable graph, from which the exelwtable graph was instantiated.
 *
 * \p hNode must not have been removed from the original graph.
 *
 * The modifications only affect future launches of \p hGraphExec. Already
 * enqueued or running launches of \p hGraphExec are not affected by this call.
 * \p hNode is also not modified by this call.
 *
 * Changing \p nodeParams->numExtSems is not supported.
 *
 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param hNode      - semaphore signal node from the graph from which graphExec was instantiated
 * \param nodeParams - Updated Parameters to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddExternalSemaphoresSignalNode,
 * ::lwdaImportExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresWaitNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecExternalSemaphoresSignalNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t hNode, const struct lwdaExternalSemaphoreSignalNodeParams *nodeParams);
#endif

/**
 * \brief Sets the parameters for an external semaphore wait node in the given graphExec
 *
 * Sets the parameters of an external semaphore wait node in an exelwtable graph \p hGraphExec.
 * The node is identified by the corresponding node \p hNode in the
 * non-exelwtable graph, from which the exelwtable graph was instantiated.
 *
 * \p hNode must not have been removed from the original graph.
 *
 * The modifications only affect future launches of \p hGraphExec. Already
 * enqueued or running launches of \p hGraphExec are not affected by this call.
 * \p hNode is also not modified by this call.
 *
 * Changing \p nodeParams->numExtSems is not supported.
 *
 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param hNode      - semaphore wait node from the graph from which graphExec was instantiated
 * \param nodeParams - Updated Parameters to set
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphAddExternalSemaphoresWaitNode,
 * ::lwdaImportExternalSemaphore,
 * ::lwdaSignalExternalSemaphoresAsync,
 * ::lwdaWaitExternalSemaphoresAsync,
 * ::lwdaGraphExecKernelNodeSetParams,
 * ::lwdaGraphExecMemcpyNodeSetParams,
 * ::lwdaGraphExecMemsetNodeSetParams,
 * ::lwdaGraphExecHostNodeSetParams,
 * ::lwdaGraphExecChildGraphNodeSetParams,
 * ::lwdaGraphExecEventRecordNodeSetEvent,
 * ::lwdaGraphExecEventWaitNodeSetEvent,
 * ::lwdaGraphExecExternalSemaphoresSignalNodeSetParams,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 */
#if __LWDART_API_VERSION >= 11020
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecExternalSemaphoresWaitNodeSetParams(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t hNode, const struct lwdaExternalSemaphoreWaitNodeParams *nodeParams);
#endif

/**
 * \brief Enables or disables the specified node in the given graphExec
 *
 * Sets \p hNode to be either enabled or disabled. Disabled nodes are functionally equivalent 
 * to empty nodes until they are reenabled. Existing node parameters are not affected by 
 * disabling/enabling the node.
 *  
 * The node is identified by the corresponding node \p hNode in the non-exelwtable 
 * graph, from which the exelwtable graph was instantiated.   
 *
 * \p hNode must not have been removed from the original graph.
 *
 * The modifications only affect future launches of \p hGraphExec. Already
 * enqueued or running launches of \p hGraphExec are not affected by this call.
 * \p hNode is also not modified by this call.
 *
 * \note Lwrrently only kernel nodes are supported. 
 *
 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param hNode      - Node from the graph from which graphExec was instantiated
 * \param isEnabled  - Node is enabled if != 0, otherwise the node is disabled
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphNodeGetEnabled,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 * ::lwdaGraphLaunch
 */
#if __LWDART_API_VERSION >= 11060
extern __host__ lwdaError_t LWDARTAPI lwdaGraphNodeSetEnabled(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t hNode, unsigned int isEnabled);
#endif

/**
 * \brief Query whether a node in the given graphExec is enabled
 *
 * Sets isEnabled to 1 if \p hNode is enabled, or 0 if \p hNode is disabled.
 *
 * The node is identified by the corresponding node \p hNode in the non-exelwtable 
 * graph, from which the exelwtable graph was instantiated.   
 *
 * \p hNode must not have been removed from the original graph.
 *
 * \note Lwrrently only kernel nodes are supported. 
 *
 * \param hGraphExec - The exelwtable graph in which to set the specified node
 * \param hNode      - Node from the graph from which graphExec was instantiated
 * \param isEnabled  - Location to return the enabled status of the node
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphNodeSetEnabled,
 * ::lwdaGraphExelwpdate,
 * ::lwdaGraphInstantiate
 * ::lwdaGraphLaunch
 */
#if __LWDART_API_VERSION >= 11060
extern __host__ lwdaError_t LWDARTAPI lwdaGraphNodeGetEnabled(lwdaGraphExec_t hGraphExec, lwdaGraphNode_t hNode, unsigned int *isEnabled);
#endif

/**
 * \brief Check whether an exelwtable graph can be updated with a graph and perform the update if possible
 *
 * Updates the node parameters in the instantiated graph specified by \p hGraphExec with the
 * node parameters in a topologically identical graph specified by \p hGraph.
 *
 * Limitations:
 *
 * - Kernel nodes:
 *   - The owning context of the function cannot change.
 *   - A node whose function originally did not use LWCA dynamic parallelism cannot be updated
 *     to a function which uses CDP.
 *   - A cooperative node cannot be updated to a non-cooperative node, and vice-versa.
 * - Memset and memcpy nodes:
 *   - The LWCA device(s) to which the operand(s) was allocated/mapped cannot change.
 *   - The source/destination memory must be allocated from the same contexts as the original
 *     source/destination memory.
 *   - Only 1D memsets can be changed.
 * - Additional memcpy node restrictions:
 *   - Changing either the source or destination memory type(i.e. LW_MEMORYTYPE_DEVICE,
 *     LW_MEMORYTYPE_ARRAY, etc.) is not supported.
 *
 * Note:  The API may add further restrictions in future releases.  The return code should always be checked.
 *
 * lwdaGraphExelwpdate sets \p updateResult_out to lwdaGraphExelwpdateErrorTopologyChanged under
 * the following conditions:
 *
 * - The count of nodes directly in \p hGraphExec and \p hGraph differ, in which case \p hErrorNode_out
 *   is NULL.
 * - A node is deleted in \p hGraph but not not its pair from \p hGraphExec, in which case \p hErrorNode_out
 *   is NULL.
 * - A node is deleted in \p hGraphExec but not its pair from \p hGraph, in which case \p hErrorNode_out is
 *   the pairless node from \p hGraph.
 * - The dependent nodes of a pair differ, in which case \p hErrorNode_out is the node from \p hGraph.
 *
 * lwdaGraphExelwpdate sets \p updateResult_out to:
 * - lwdaGraphExelwpdateError if passed an invalid value.
 * - lwdaGraphExelwpdateErrorTopologyChanged if the graph topology changed
 * - lwdaGraphExelwpdateErrorNodeTypeChanged if the type of a node changed, in which case
 *   \p hErrorNode_out is set to the node from \p hGraph.
 * - lwdaGraphExelwpdateErrorFunctionChanged if the function of a kernel node changed (LWCA driver < 11.2)
 * - lwdaGraphExelwpdateErrorUnsupportedFunctionChange if the func field of a kernel changed in an
 *   unsupported way(see note above), in which case \p hErrorNode_out is set to the node from \p hGraph
 * - lwdaGraphExelwpdateErrorParametersChanged if any parameters to a node changed in a way 
 *   that is not supported, in which case \p hErrorNode_out is set to the node from \p hGraph
 * - lwdaGraphExelwpdateErrorAttributesChanged if any attributes of a node changed in a way 
 *   that is not supported, in which case \p hErrorNode_out is set to the node from \p hGraph
 * - lwdaGraphExelwpdateErrorNotSupported if something about a node is unsupported, like 
 *   the node's type or configuration, in which case \p hErrorNode_out is set to the node from \p hGraph
 *
 * If \p updateResult_out isn't set in one of the situations described above, the update check passes
 * and lwdaGraphExelwpdate updates \p hGraphExec to match the contents of \p hGraph.  If an error happens
 * during the update, \p updateResult_out will be set to lwdaGraphExelwpdateError; otherwise,
 * \p updateResult_out is set to lwdaGraphExelwpdateSuccess.
 *
 * lwdaGraphExelwpdate returns lwdaSuccess when the updated was performed successfully.  It returns
 * lwdaErrorGraphExelwpdateFailure if the graph update was not performed because it included 
 * changes which violated constraints specific to instantiated graph update.
 *
 * \param hGraphExec The instantiated graph to be updated
 * \param hGraph The graph containing the updated parameters
 * \param hErrorNode_out The node which caused the permissibility check to forbid the update, if any
 * \param updateResult_out Whether the graph update was permitted.  If was forbidden, the reason why
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorGraphExelwpdateFailure,
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphInstantiate,
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExelwpdate(lwdaGraphExec_t hGraphExec, lwdaGraph_t hGraph, lwdaGraphNode_t *hErrorNode_out, enum lwdaGraphExelwpdateResult *updateResult_out);

/**
 * \brief Uploads an exelwtable graph in a stream
 *
 * Uploads \p hGraphExec to the device in \p hStream without exelwting it. Uploads of
 * the same \p hGraphExec will be serialized. Each upload is ordered behind both any
 * previous work in \p hStream and any previous launches of \p hGraphExec.
 * Uses memory cached by \p stream to back the allocations owned by \p graphExec.
 *
 * \param hGraphExec - Exelwtable graph to upload
 * \param hStream    - Stream in which to upload the graph
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * \notefnerr
 * \note_init_rt
 *
 * \sa
 * ::lwdaGraphInstantiate,
 * ::lwdaGraphLaunch,
 * ::lwdaGraphExecDestroy
 */
#if __LWDART_API_VERSION >= 11010
 extern __host__ lwdaError_t LWDARTAPI lwdaGraphUpload(lwdaGraphExec_t graphExec, lwdaStream_t stream);
#endif

/**
 * \brief Launches an exelwtable graph in a stream
 *
 * Exelwtes \p graphExec in \p stream. Only one instance of \p graphExec may be exelwting
 * at a time. Each launch is ordered behind both any previous work in \p stream
 * and any previous launches of \p graphExec. To execute a graph conlwrrently, it must be
 * instantiated multiple times into multiple exelwtable graphs.
 *
 * If any allocations created by \p graphExec remain unfreed (from a previous launch) and
 * \p graphExec was not instantiated with ::lwdaGraphInstantiateFlagAutoFreeOnLaunch,
 * the launch will fail with ::lwdaErrorIlwalidValue.
 *
 * \param graphExec - Exelwtable graph to launch
 * \param stream    - Stream in which to launch the graph
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwdaGraphInstantiate,
 * ::lwdaGraphUpload,
 * ::lwdaGraphExecDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphLaunch(lwdaGraphExec_t graphExec, lwdaStream_t stream);

/**
 * \brief Destroys an exelwtable graph
 *
 * Destroys the exelwtable graph specified by \p graphExec.
 *
 * \param graphExec - Exelwtable graph to destroy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa
 * ::lwdaGraphInstantiate,
 * ::lwdaGraphUpload,
 * ::lwdaGraphLaunch
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphExecDestroy(lwdaGraphExec_t graphExec);

/**
 * \brief Destroys a graph
 *
 * Destroys the graph specified by \p graph, as well as all of its nodes.
 *
 * \param graph - Graph to destroy
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 * \note_graph_thread_safety
 * \notefnerr
 * \note_init_rt
 * \note_callback
 * \note_destroy_ub
 *
 * \sa
 * ::lwdaGraphCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphDestroy(lwdaGraph_t graph);

/**
 * \brief Write a DOT file describing graph structure
 *
 * Using the provided \p graph, write to \p path a DOT formatted description of the graph.
 * By default this includes the graph topology, node types, node id, kernel names and memcpy direction.
 * \p flags can be specified to write more detailed information about each node type such as
 * parameter values, kernel attributes, node and function handles.
 *
 * \param graph - The graph to create a DOT file from
 * \param path  - The path to write the DOT file to
 * \param flags - Flags from lwdaGraphDebugDotFlags for specifying which additional node information to write
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorOperatingSystem
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphDebugDotPrint(lwdaGraph_t graph, const char *path, unsigned int flags);

/**
 * \brief Create a user object
 *
 * Create a user object with the specified destructor callback and initial reference count. The
 * initial references are owned by the caller.
 *
 * Destructor callbacks cannot make LWCA API calls and should avoid blocking behavior, as they
 * are exelwted by a shared internal thread. Another thread may be signaled to perform such
 * actions, if it does not block forward progress of tasks scheduled through LWCA.
 *
 * See LWCA User Objects in the LWCA C++ Programming Guide for more information on user objects.
 *
 * \param object_out      - Location to return the user object handle
 * \param ptr             - The pointer to pass to the destroy function
 * \param destroy         - Callback to free the user object when it is no longer in use
 * \param initialRefcount - The initial refcount to create the object with, typically 1. The
 *                          initial references are owned by the calling thread.
 * \param flags           - Lwrrently it is required to pass ::lwdaUserObjectNoDestructorSync,
 *                          which is the only defined flag. This indicates that the destroy
 *                          callback cannot be waited on by any LWCA API. Users requiring
 *                          synchronization of the callback should signal its completion
 *                          manually.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa
 * ::lwdaUserObjectRetain,
 * ::lwdaUserObjectRelease,
 * ::lwdaGraphRetainUserObject,
 * ::lwdaGraphReleaseUserObject,
 * ::lwdaGraphCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaUserObjectCreate(lwdaUserObject_t *object_out, void *ptr, lwdaHostFn_t destroy, unsigned int initialRefcount, unsigned int flags);

/**
 * \brief Retain a reference to a user object
 *
 * Retains new references to a user object. The new references are owned by the caller.
 *
 * See LWCA User Objects in the LWCA C++ Programming Guide for more information on user objects.
 *
 * \param object - The object to retain
 * \param count  - The number of references to retain, typically 1. Must be nonzero
 *                 and not larger than INT_MAX.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa
 * ::lwdaUserObjectCreate,
 * ::lwdaUserObjectRelease,
 * ::lwdaGraphRetainUserObject,
 * ::lwdaGraphReleaseUserObject,
 * ::lwdaGraphCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaUserObjectRetain(lwdaUserObject_t object, unsigned int count __dv(1));

/**
 * \brief Release a reference to a user object
 *
 * Releases user object references owned by the caller. The object's destructor is ilwoked if
 * the reference count reaches zero.
 *
 * It is undefined behavior to release references not owned by the caller, or to use a user
 * object handle after all references are released.
 *
 * See LWCA User Objects in the LWCA C++ Programming Guide for more information on user objects.
 *
 * \param object - The object to release
 * \param count  - The number of references to release, typically 1. Must be nonzero
 *                 and not larger than INT_MAX.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa
 * ::lwdaUserObjectCreate,
 * ::lwdaUserObjectRetain,
 * ::lwdaGraphRetainUserObject,
 * ::lwdaGraphReleaseUserObject,
 * ::lwdaGraphCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaUserObjectRelease(lwdaUserObject_t object, unsigned int count __dv(1));

/**
 * \brief Retain a reference to a user object from a graph
 *
 * Creates or moves user object references that will be owned by a LWCA graph.
 *
 * See LWCA User Objects in the LWCA C++ Programming Guide for more information on user objects.
 *
 * \param graph  - The graph to associate the reference with
 * \param object - The user object to retain a reference for
 * \param count  - The number of references to add to the graph, typically 1. Must be
 *                 nonzero and not larger than INT_MAX.
 * \param flags  - The optional flag ::lwdaGraphUserObjectMove transfers references
 *                 from the calling thread, rather than create new references. Pass 0
 *                 to create new references.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa
 * ::lwdaUserObjectCreate
 * ::lwdaUserObjectRetain,
 * ::lwdaUserObjectRelease,
 * ::lwdaGraphReleaseUserObject,
 * ::lwdaGraphCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphRetainUserObject(lwdaGraph_t graph, lwdaUserObject_t object, unsigned int count __dv(1), unsigned int flags __dv(0));

/**
 * \brief Release a user object reference from a graph
 *
 * Releases user object references owned by a graph.
 *
 * See LWCA User Objects in the LWCA C++ Programming Guide for more information on user objects.
 *
 * \param graph  - The graph that will release the reference
 * \param object - The user object to release a reference for
 * \param count  - The number of references to release, typically 1. Must be nonzero
 *                 and not larger than INT_MAX.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue
 *
 * \sa
 * ::lwdaUserObjectCreate
 * ::lwdaUserObjectRetain,
 * ::lwdaUserObjectRelease,
 * ::lwdaGraphRetainUserObject,
 * ::lwdaGraphCreate
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphReleaseUserObject(lwdaGraph_t graph, lwdaUserObject_t object, unsigned int count __dv(1));

/** @} */ /* END LWDART_GRAPH */

/**
 * \defgroup LWDART_DRIVER_ENTRY_POINT Driver Entry Point Access
 *
 * ___MANBRIEF___ driver entry point access functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the driver entry point access functions of LWCA
 * runtime application programming interface.
 *
 * @{
 */

/**
 * \brief Returns the requested driver API function pointer
 *
 * Returns in \p **funcPtr the address of the LWCA driver function for the requested flags.
 *
 * For a requested driver symbol, if the LWCA version in which the driver symbol was
 * introduced is less than or equal to the LWCA runtime version, the API will return
 * the function pointer to the corresponding versioned driver function.
 *
 * The pointer returned by the API should be cast to a function pointer matching the
 * requested driver function's definition in the API header file. The function pointer
 * typedef can be picked up from the corresponding typedefs header file. For example,
 * lwdaTypedefs.h consists of function pointer typedefs for driver APIs defined in lwca.h.
 *
 * The API will return ::lwdaErrorSymbolNotFound if the requested driver function is not
 * supported on the platform, no ABI compatible driver function exists for the LWCA runtime
 * version or if the driver symbol is invalid.
 *
 * The requested flags can be:
 * - ::lwdaEnableDefault: This is the default mode. This is equivalent to
 *   ::lwdaEnablePerThreadDefaultStream if the code is compiled with
 *   --default-stream per-thread compilation flag or the macro LWDA_API_PER_THREAD_DEFAULT_STREAM
 *   is defined; ::lwdaEnableLegacyStream otherwise.
 * - ::lwdaEnableLegacyStream: This will enable the search for all driver symbols
 *   that match the requested driver symbol name except the corresponding per-thread versions.
 * - ::lwdaEnablePerThreadDefaultStream: This will enable the search for all
 *   driver symbols that match the requested driver symbol name including the per-thread
 *   versions. If a per-thread version is not found, the API will return the legacy version
 *   of the driver function.
 *
 * \param symbol - The base name of the driver API function to look for. As an example,
 *                 for the driver API ::lwMemAlloc_v2, \p symbol would be lwMemAlloc.
 *                 Note that the API will use the LWCA runtime version to return the
 *                 address to the most recent ABI compatible driver symbol, ::lwMemAlloc
 *                 or ::lwMemAlloc_v2.
 * \param funcPtr - Location to return the function pointer to the requested driver function
 * \param flags -  Flags to specify search options.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorNotSupported,
 * ::lwdaErrorSymbolNotFound
 * \note_version_mixing
 * \note_init_rt
 * \note_callback
 *
 * \sa
 * ::lwGetProcAddress
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGetDriverEntryPoint(const char *symbol, void **funcPtr, unsigned long long flags);

/** @} */ /* END LWDART_DRIVER_ENTRY_POINT */

/** \cond impl_private */
extern __host__ lwdaError_t LWDARTAPI lwdaGetExportTable(const void **ppExportTable, const lwdaUUID_t *pExportTableId);
/** \endcond impl_private */

/**
 * \defgroup LWDART_HIGHLEVEL C++ API Routines
 *
 * ___MANBRIEF___ C++ high level API functions of the LWCA runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the C++ high level API functions of the LWCA runtime
 * application programming interface. To use these functions, your
 * application needs to be compiled with the \p lwcc compiler.
 *
 * \brief C++-style interface built on top of LWCA runtime API
 */

/**
 * \defgroup LWDART_DRIVER Interactions with the LWCA Driver API
 *
 * ___MANBRIEF___ interactions between LWCA Driver API and LWCA Runtime API
 * (___LWRRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the interactions between the LWCA Driver API and the LWCA Runtime API
 *
 * @{
 *
 * \section LWDART_LWDA_primary Primary Contexts
 *
 * There exists a one to one relationship between LWCA devices in the LWCA Runtime
 * API and ::LWcontext s in the LWCA Driver API within a process.  The specific
 * context which the LWCA Runtime API uses for a device is called the device's
 * primary context.  From the perspective of the LWCA Runtime API, a device and 
 * its primary context are synonymous.
 *
 * \section LWDART_LWDA_init Initialization and Tear-Down
 *
 * LWCA Runtime API calls operate on the LWCA Driver API ::LWcontext which is current to
 * to the calling host thread.  
 *
 * The function ::lwdaSetDevice() makes the primary context for the
 * specified device current to the calling thread by calling ::lwCtxSetLwrrent().
 *
 * The LWCA Runtime API will automatically initialize the primary context for
 * a device at the first LWCA Runtime API call which requires an active context.
 * If no ::LWcontext is current to the calling thread when a LWCA Runtime API call 
 * which requires an active context is made, then the primary context for a device 
 * will be selected, made current to the calling thread, and initialized.
 *
 * The context which the LWCA Runtime API initializes will be initialized using 
 * the parameters specified by the LWCA Runtime API functions
 * ::lwdaSetDeviceFlags(), 
 * ::lwdaD3D9SetDirect3DDevice(), 
 * ::lwdaD3D10SetDirect3DDevice(), 
 * ::lwdaD3D11SetDirect3DDevice(), 
 * ::lwdaGLSetGLDevice(), and
 * ::lwdaVDPAUSetVDPAUDevice().
 * Note that these functions will fail with ::lwdaErrorSetOnActiveProcess if they are 
 * called when the primary context for the specified device has already been initialized.
 * (or if the current device has already been initialized, in the case of 
 * ::lwdaSetDeviceFlags()). 
 *
 * Primary contexts will remain active until they are explicitly deinitialized 
 * using ::lwdaDeviceReset().  The function ::lwdaDeviceReset() will deinitialize the 
 * primary context for the calling thread's current device immediately.  The context 
 * will remain current to all of the threads that it was current to.  The next LWCA 
 * Runtime API call on any thread which requires an active context will trigger the 
 * reinitialization of that device's primary context.
 *
 * Note that primary contexts are shared resources. It is recommended that
 * the primary context not be reset except just before exit or to recover from an
 * unspecified launch failure.
 * 
 * \section LWDART_LWDA_context Context Interoperability
 *
 * Note that the use of multiple ::LWcontext s per device within a single process 
 * will substantially degrade performance and is strongly discouraged.  Instead,
 * it is highly recommended that the implicit one-to-one device-to-context mapping
 * for the process provided by the LWCA Runtime API be used.
 *
 * If a non-primary ::LWcontext created by the LWCA Driver API is current to a
 * thread then the LWCA Runtime API calls to that thread will operate on that 
 * ::LWcontext, with some exceptions listed below.  Interoperability between data
 * types is dislwssed in the following sections.
 *
 * The function ::lwdaPointerGetAttributes() will return the error 
 * ::lwdaErrorIncompatibleDriverContext if the pointer being queried was allocated by a 
 * non-primary context.  The function ::lwdaDeviceEnablePeerAccess() and the rest of 
 * the peer access API may not be called when a non-primary ::LWcontext is current.  
 * To use the pointer query and peer access APIs with a context created using the 
 * LWCA Driver API, it is necessary that the LWCA Driver API be used to access
 * these features.
 *
 * All LWCA Runtime API state (e.g, global variables' addresses and values) travels
 * with its underlying ::LWcontext.  In particular, if a ::LWcontext is moved from one 
 * thread to another then all LWCA Runtime API state will move to that thread as well.
 *
 * Please note that attaching to legacy contexts (those with a version of 3010 as returned
 * by ::lwCtxGetApiVersion()) is not possible. The LWCA Runtime will return
 * ::lwdaErrorIncompatibleDriverContext in such cases.
 *
 * \section LWDART_LWDA_stream Interactions between LWstream and lwdaStream_t
 *
 * The types ::LWstream and ::lwdaStream_t are identical and may be used interchangeably.
 *
 * \section LWDART_LWDA_event Interactions between LWevent and lwdaEvent_t
 *
 * The types ::LWevent and ::lwdaEvent_t are identical and may be used interchangeably.
 *
 * \section LWDART_LWDA_array Interactions between LWarray and lwdaArray_t 
 *
 * The types ::LWarray and struct ::lwdaArray * represent the same data type and may be used
 * interchangeably by casting the two types between each other.
 *
 * In order to use a ::LWarray in a LWCA Runtime API function which takes a struct ::lwdaArray *,
 * it is necessary to explicitly cast the ::LWarray to a struct ::lwdaArray *.
 *
 * In order to use a struct ::lwdaArray * in a LWCA Driver API function which takes a ::LWarray,
 * it is necessary to explicitly cast the struct ::lwdaArray * to a ::LWarray .
 *
 * \section LWDART_LWDA_graphicsResource Interactions between LWgraphicsResource and lwdaGraphicsResource_t
 *
 * The types ::LWgraphicsResource and ::lwdaGraphicsResource_t represent the same data type and may be used
 * interchangeably by casting the two types between each other.
 *
 * In order to use a ::LWgraphicsResource in a LWCA Runtime API function which takes a 
 * ::lwdaGraphicsResource_t, it is necessary to explicitly cast the ::LWgraphicsResource 
 * to a ::lwdaGraphicsResource_t.
 *
 * In order to use a ::lwdaGraphicsResource_t in a LWCA Driver API function which takes a
 * ::LWgraphicsResource, it is necessary to explicitly cast the ::lwdaGraphicsResource_t 
 * to a ::LWgraphicsResource.
 *
 * \section LWDART_LWDA_texture_objects Interactions between LWtexObject and lwdaTextureObject_t
 *
 * The types ::LWtexObject and ::lwdaTextureObject_t represent the same data type and may be used
 * interchangeably by casting the two types between each other.
 *
 * In order to use a ::LWtexObject in a LWCA Runtime API function which takes a ::lwdaTextureObject_t,
 * it is necessary to explicitly cast the ::LWtexObject to a ::lwdaTextureObject_t.
 *
 * In order to use a ::lwdaTextureObject_t in a LWCA Driver API function which takes a ::LWtexObject,
 * it is necessary to explicitly cast the ::lwdaTextureObject_t to a ::LWtexObject.
 *
 * \section LWDART_LWDA_surface_objects Interactions between LWsurfObject and lwdaSurfaceObject_t
 *
 * The types ::LWsurfObject and ::lwdaSurfaceObject_t represent the same data type and may be used
 * interchangeably by casting the two types between each other.
 *
 * In order to use a ::LWsurfObject in a LWCA Runtime API function which takes a ::lwdaSurfaceObject_t,
 * it is necessary to explicitly cast the ::LWsurfObject to a ::lwdaSurfaceObject_t.
 *
 * In order to use a ::lwdaSurfaceObject_t in a LWCA Driver API function which takes a ::LWsurfObject,
 * it is necessary to explicitly cast the ::lwdaSurfaceObject_t to a ::LWsurfObject.
 *
 * \section LWDART_LWDA_module Interactions between LWfunction and lwdaFunction_t
 *
 * The types ::LWfunction and ::lwdaFunction_t represent the same data type and may be used
 * interchangeably by casting the two types between each other.
 *
 * In order to use a ::lwdaFunction_t in a LWCA Driver API function which takes a ::LWfunction,
 * it is necessary to explicitly cast the ::lwdaFunction_t to a ::LWfunction.
 *
 */

 /**
  * \brief Get pointer to device entry function that matches entry function \p symbolPtr
  *
  * Returns in \p functionPtr the device entry function corresponding to the symbol \p symbolPtr.
  *
  * \param functionPtr     - Returns the device entry function
  * \param symbolPtr       - Pointer to device entry function to search for
  *
  * \return
  * ::lwdaSuccess
  *
  */
extern __host__ lwdaError_t LWDARTAPI_CDECL lwdaGetFuncBySymbol(lwdaFunction_t* functionPtr, const void* symbolPtr);

/** @} */ /* END LWDART_DRIVER */

#if defined(__LWDA_API_VERSION_INTERNAL)
    #undef lwdaMemcpy
    #undef lwdaMemcpyToSymbol
    #undef lwdaMemcpyFromSymbol
    #undef lwdaMemcpy2D
    #undef lwdaMemcpyToArray
    #undef lwdaMemcpy2DToArray
    #undef lwdaMemcpyFromArray
    #undef lwdaMemcpy2DFromArray
    #undef lwdaMemcpyArrayToArray
    #undef lwdaMemcpy2DArrayToArray
    #undef lwdaMemcpy3D
    #undef lwdaMemcpy3DPeer
    #undef lwdaMemset
    #undef lwdaMemset2D
    #undef lwdaMemset3D
    #undef lwdaMemcpyAsync
    #undef lwdaMemcpyToSymbolAsync
    #undef lwdaMemcpyFromSymbolAsync
    #undef lwdaMemcpy2DAsync
    #undef lwdaMemcpyToArrayAsync
    #undef lwdaMemcpy2DToArrayAsync
    #undef lwdaMemcpyFromArrayAsync
    #undef lwdaMemcpy2DFromArrayAsync
    #undef lwdaMemcpy3DAsync
    #undef lwdaMemcpy3DPeerAsync
    #undef lwdaMemsetAsync
    #undef lwdaMemset2DAsync
    #undef lwdaMemset3DAsync
    #undef lwdaStreamQuery
    #undef lwdaStreamGetFlags
    #undef lwdaStreamGetPriority
    #undef lwdaEventRecord
    #undef lwdaEventRecordWithFlags
    #undef lwdaStreamWaitEvent
    #undef lwdaStreamAddCallback
    #undef lwdaStreamAttachMemAsync
    #undef lwdaStreamSynchronize
    #undef lwdaLaunchKernel

    #undef lwdaLaunchKernelEx

    #undef lwdaLaunchHostFunc
    #undef lwdaMemPrefetchAsync
    #undef lwdaLaunchCooperativeKernel
    #undef lwdaSignalExternalSemaphoresAsync
    #undef lwdaWaitExternalSemaphoresAsync
    #undef lwdaGraphUpload
    #undef lwdaGraphLaunch
    #undef lwdaStreamBeginCapture
    #undef lwdaStreamEndCapture
    #undef lwdaStreamIsCapturing
    #undef lwdaStreamGetCaptureInfo
    #undef lwdaStreamGetCaptureInfo_v2
    #undef lwdaStreamCopyAttributes
    #undef lwdaStreamGetAttribute
    #undef lwdaStreamSetAttribute
    #undef lwdaMallocAsync
    #undef lwdaFreeAsync
    #undef lwdaMallocFromPoolAsync
    #undef lwdaGetDriverEntryPoint

    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy(void *dst, const void *src, size_t count, enum lwdaMemcpyKind kind);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyToSymbol(const void *symbol, const void *src, size_t count, size_t offset __dv(0), enum lwdaMemcpyKind kind __dv(lwdaMemcpyHostToDevice));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromSymbol(void *dst, const void *symbol, size_t count, size_t offset __dv(0), enum lwdaMemcpyKind kind __dv(lwdaMemcpyDeviceToHost));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2D(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyToArray(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t count, enum lwdaMemcpyKind kind);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DToArray(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromArray(void *dst, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t count, enum lwdaMemcpyKind kind);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DFromArray(void *dst, size_t dpitch, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t width, size_t height, enum lwdaMemcpyKind kind);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyArrayToArray(lwdaArray_t dst, size_t wOffsetDst, size_t hOffsetDst, lwdaArray_const_t src, size_t wOffsetSrc, size_t hOffsetSrc, size_t count, enum lwdaMemcpyKind kind __dv(lwdaMemcpyDeviceToDevice));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DArrayToArray(lwdaArray_t dst, size_t wOffsetDst, size_t hOffsetDst, lwdaArray_const_t src, size_t wOffsetSrc, size_t hOffsetSrc, size_t width, size_t height, enum lwdaMemcpyKind kind __dv(lwdaMemcpyDeviceToDevice));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy3D(const struct lwdaMemcpy3DParms *p);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy3DPeer(const struct lwdaMemcpy3DPeerParms *p);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemset(void *devPtr, int value, size_t count);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemset2D(void *devPtr, size_t pitch, int value, size_t width, size_t height);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemset3D(struct lwdaPitchedPtr pitchedDevPtr, int value, struct lwdaExtent extent);
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpyAsync(void *dst, const void *src, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyToSymbolAsync(const void *symbol, const void *src, size_t count, size_t offset, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromSymbolAsync(void *dst, const void *symbol, size_t count, size_t offset, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy2DAsync(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyToArrayAsync(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DToArrayAsync(lwdaArray_t dst, size_t wOffset, size_t hOffset, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpyFromArrayAsync(void *dst, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy2DFromArrayAsync(void *dst, size_t dpitch, lwdaArray_const_t src, size_t wOffset, size_t hOffset, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy3DAsync(const struct lwdaMemcpy3DParms *p, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaMemcpy3DPeerAsync(const struct lwdaMemcpy3DPeerParms *p, lwdaStream_t stream __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemsetAsync(void *devPtr, int value, size_t count, lwdaStream_t stream __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset2DAsync(void *devPtr, size_t pitch, int value, size_t width, size_t height, lwdaStream_t stream __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset3DAsync(struct lwdaPitchedPtr pitchedDevPtr, int value, struct lwdaExtent extent, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamQuery(lwdaStream_t stream);
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamGetFlags(lwdaStream_t hStream, unsigned int *flags);
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamGetPriority(lwdaStream_t hStream, int *priority);
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecord(lwdaEvent_t event, lwdaStream_t stream __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecordWithFlags(lwdaEvent_t event, lwdaStream_t stream __dv(0), unsigned int flags __dv(0));
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamWaitEvent(lwdaStream_t stream, lwdaEvent_t event, unsigned int flags);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamAddCallback(lwdaStream_t stream, lwdaStreamCallback_t callback, void *userData, unsigned int flags);
    extern __host__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamAttachMemAsync(lwdaStream_t stream, void *devPtr, size_t length, unsigned int flags);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamSynchronize(lwdaStream_t stream);
    extern __host__ lwdaError_t LWDARTAPI lwdaLaunchKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream);

    extern __host__ lwdaError_t LWDARTAPI lwdaLaunchKernelEx(const lwdaLaunchConfig_t *config, const void *func, void **args);

    extern __host__ lwdaError_t LWDARTAPI lwdaLaunchCooperativeKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args, size_t sharedMem, lwdaStream_t stream);
    extern __host__ lwdaError_t LWDARTAPI lwdaLaunchHostFunc(lwdaStream_t stream, lwdaHostFn_t fn, void *userData);
    extern __host__ lwdaError_t LWDARTAPI lwdaMemPrefetchAsync(const void *devPtr, size_t count, int dstDevice, lwdaStream_t stream);
    extern __host__ lwdaError_t LWDARTAPI lwdaSignalExternalSemaphoresAsync(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreSignalParams_v1 *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaSignalExternalSemaphoresAsync_ptsz(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreSignalParams_v1 *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaSignalExternalSemaphoresAsync_v2(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreSignalParams *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaWaitExternalSemaphoresAsync(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreWaitParams_v1 *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaWaitExternalSemaphoresAsync_ptsz(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreWaitParams_v1 *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaWaitExternalSemaphoresAsync_v2(const lwdaExternalSemaphore_t *extSemArray, const struct lwdaExternalSemaphoreWaitParams *paramsArray, unsigned int numExtSems, lwdaStream_t stream __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaGraphUpload(lwdaGraphExec_t graphExec, lwdaStream_t stream);
    extern __host__ lwdaError_t LWDARTAPI lwdaGraphLaunch(lwdaGraphExec_t graphExec, lwdaStream_t stream);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamBeginCapture(lwdaStream_t stream, enum lwdaStreamCaptureMode mode);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamEndCapture(lwdaStream_t stream, lwdaGraph_t *pGraph);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamIsCapturing(lwdaStream_t stream, enum lwdaStreamCaptureStatus *pCaptureStatus);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamGetCaptureInfo(lwdaStream_t stream, enum lwdaStreamCaptureStatus *captureStatus_out, unsigned long long *id_out);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamGetCaptureInfo_v2(lwdaStream_t stream, enum lwdaStreamCaptureStatus *captureStatus_out, unsigned long long *id_out __dv(0), lwdaGraph_t *graph_out __dv(0), const lwdaGraphNode_t **dependencies_out __dv(0), size_t *numDependencies_out __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamUpdateCaptureDependencies_ptsz(lwdaStream_t stream, lwdaGraphNode_t *dependencies, size_t numDependencies, unsigned int flags __dv(0));
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamCopyAttributes(lwdaStream_t dstStream, lwdaStream_t srcStream);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamGetAttribute(lwdaStream_t stream, lwdaStreamAttrID attr, lwdaStreamAttrValue *value);
    extern __host__ lwdaError_t LWDARTAPI lwdaStreamSetAttribute(lwdaStream_t stream, lwdaStreamAttrID attr, const lwdaStreamAttrValue *param);

    extern __host__ lwdaError_t LWDARTAPI lwdaMallocAsync(void **devPtr, size_t size, lwdaStream_t hStream);
    extern __host__ lwdaError_t LWDARTAPI lwdaFreeAsync(void *devPtr, lwdaStream_t hStream);
    extern __host__ lwdaError_t LWDARTAPI lwdaMallocFromPoolAsync(void **ptr, size_t size, lwdaMemPool_t memPool, lwdaStream_t stream);
    extern __host__ lwdaError_t LWDARTAPI lwdaGetDriverEntryPoint(const char *symbol, void **funcPtr, unsigned long long flags);

#elif defined(__LWDART_API_PER_THREAD_DEFAULT_STREAM)
    // lwcc stubs reference the 'lwdaLaunch'/'lwdaLaunchKernel' identifier even if it was defined
    // to 'lwdaLaunch_ptsz'/'lwdaLaunchKernel_ptsz'. Redirect through a static inline function.
    #undef lwdaLaunchKernel
    static __inline__ __host__ lwdaError_t lwdaLaunchKernel(const void *func, 
                                                            dim3 gridDim, dim3 blockDim, 
                                                            void **args, size_t sharedMem, 
                                                            lwdaStream_t stream)
    {
        return lwdaLaunchKernel_ptsz(func, gridDim, blockDim, args, sharedMem, stream);
    }
    #define lwdaLaunchKernel __LWDART_API_PTSZ(lwdaLaunchKernel)

    #undef lwdaLaunchKernelEx
    static __inline__ __host__ lwdaError_t lwdaLaunchKernelEx(const lwdaLaunchConfig_t *config,
                                                              const void *func,
                                                              void **args)
    {
        return lwdaLaunchKernelEx_ptsz(config, func, args);
    }
    #define lwdaLaunchKernelEx __LWDART_API_PTSZ(lwdaLaunchKernelEx)

#endif

#if defined(__cplusplus)
}

#endif /* __cplusplus */

#undef EXCLUDE_FROM_RTC
#endif /* !__LWDACC_RTC__ */

#undef __dv
#undef __LWDA_DEPRECATED

#if defined(__UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDA_RUNTIME_API_H__)
#undef __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#undef __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDA_RUNTIME_API_H__
#endif

#endif /* !__LWDA_RUNTIME_API_H__ */
