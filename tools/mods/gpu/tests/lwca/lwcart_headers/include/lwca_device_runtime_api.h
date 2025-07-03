/*
 * Copyright 1993-2021 LWPU Corporation.  All rights reserved.
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

#if !defined(__LWDA_DEVICE_RUNTIME_API_H__)
#define __LWDA_DEVICE_RUNTIME_API_H__

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LWDACC_RTC__)

#if !defined(__LWDACC_INTERNAL_NO_STUBS__) && !defined(__LWDACC_RDC__) && !defined(__LWDACC_EWP__) && defined(__LWDA_ARCH__) && (__LWDA_ARCH__ >= 350) && !defined(__LWDADEVRT_INTERNAL__)

#if defined(__cplusplus)
extern "C" {
#endif

struct lwdaFuncAttributes;


inline __device__  lwdaError_t LWDARTAPI lwdaMalloc(void **p, size_t s) 
{ 
  return lwdaErrorUnknown;
}

inline __device__  lwdaError_t LWDARTAPI lwdaFuncGetAttributes(struct lwdaFuncAttributes *p, const void *c) 
{ 
  return lwdaErrorUnknown;
}

inline __device__  lwdaError_t LWDARTAPI lwdaDeviceGetAttribute(int *value, enum lwdaDeviceAttr attr, int device)
{
  return lwdaErrorUnknown;
}

inline __device__  lwdaError_t LWDARTAPI lwdaGetDevice(int *device)
{
  return lwdaErrorUnknown;
}

inline __device__  lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, const void *func, int blockSize, size_t dynamicSmemSize)
{
  return lwdaErrorUnknown;
}

inline __device__  lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags(int *numBlocks, const void *func, int blockSize, size_t dynamicSmemSize, unsigned int flags)
{
  return lwdaErrorUnknown;
}


#if defined(__cplusplus)
}
#endif

#endif /* !defined(__LWDACC_INTERNAL_NO_STUBS__) && !defined(__LWDACC_RDC__) &&  !defined(__LWDACC_EWP__) && defined(__LWDA_ARCH__) && (__LWDA_ARCH__ >= 350) && !defined(__LWDADEVRT_INTERNAL__) */

#endif /* !defined(__LWDACC_RTC__) */

#if defined(__DOXYGEN_ONLY__) || defined(LWDA_ENABLE_DEPRECATED)
# define __DEPRECATED__(msg)
#elif defined(_WIN32)
# define __DEPRECATED__(msg) __declspec(deprecated(msg))
#elif (defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 5 && !defined(__clang__))))
# define __DEPRECATED__(msg) __attribute__((deprecated))
#else
# define __DEPRECATED__(msg) __attribute__((deprecated(msg)))
#endif

#if defined(__LWDA_ARCH__) && !defined(__CDPRT_SUPPRESS_SYNC_DEPRECATION_WARNING)
# define __CDPRT_DEPRECATED(func_name) __DEPRECATED__("Use of "#func_name" from device code is deprecated and will not be supported in a future release. Disable this warning with -D__CDPRT_SUPPRESS_SYNC_DEPRECATION_WARNING.")
#else
# define __CDPRT_DEPRECATED(func_name)
#endif

#if defined(__cplusplus) && defined(__LWDACC__)         /* Visible to lwcc front-end only */
#if !defined(__LWDA_ARCH__) || (__LWDA_ARCH__ >= 350)   // Visible to SM>=3.5 and "__host__ __device__" only

#include "driver_types.h"
#include "crt/host_defines.h"


#define lwdaStreamGraphTailLaunch             (lwdaStream_t)0x0100000000000000
#define lwdaStreamGraphFireAndForget          (lwdaStream_t)0x0200000000000000


extern "C"
{
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetAttribute(int *value, enum lwdaDeviceAttr attr, int device);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetLimit(size_t *pValue, enum lwdaLimit limit);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetCacheConfig(enum lwdaFuncCache *pCacheConfig);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaDeviceGetSharedMemConfig(enum lwdaSharedMemConfig *pConfig);
extern __device__ __lwdart_builtin__ __CDPRT_DEPRECATED(lwdaDeviceSynchronize) lwdaError_t LWDARTAPI lwdaDeviceSynchronize(void);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI __lwdaDeviceSynchronizeDeprecationAvoidance(void);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetLastError(void);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaPeekAtLastError(void);
extern __device__ __lwdart_builtin__ const char* LWDARTAPI lwdaGetErrorString(lwdaError_t error);
extern __device__ __lwdart_builtin__ const char* LWDARTAPI lwdaGetErrorName(lwdaError_t error);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetDeviceCount(int *count);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaGetDevice(int *device);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamCreateWithFlags(lwdaStream_t *pStream, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamDestroy(lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamWaitEvent(lwdaStream_t stream, lwdaEvent_t event, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaStreamWaitEvent_ptsz(lwdaStream_t stream, lwdaEvent_t event, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventCreateWithFlags(lwdaEvent_t *event, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecord(lwdaEvent_t event, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecord_ptsz(lwdaEvent_t event, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecordWithFlags(lwdaEvent_t event, lwdaStream_t stream, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventRecordWithFlags_ptsz(lwdaEvent_t event, lwdaStream_t stream, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaEventDestroy(lwdaEvent_t event);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaFuncGetAttributes(struct lwdaFuncAttributes *attr, const void *func);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaFree(void *devPtr);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMalloc(void **devPtr, size_t size);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpyAsync(void *dst, const void *src, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpyAsync_ptsz(void *dst, const void *src, size_t count, enum lwdaMemcpyKind kind, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy2DAsync(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy2DAsync_ptsz(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum lwdaMemcpyKind kind, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy3DAsync(const struct lwdaMemcpy3DParms *p, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemcpy3DAsync_ptsz(const struct lwdaMemcpy3DParms *p, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemsetAsync(void *devPtr, int value, size_t count, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemsetAsync_ptsz(void *devPtr, int value, size_t count, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset2DAsync(void *devPtr, size_t pitch, int value, size_t width, size_t height, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset2DAsync_ptsz(void *devPtr, size_t pitch, int value, size_t width, size_t height, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset3DAsync(struct lwdaPitchedPtr pitchedDevPtr, int value, struct lwdaExtent extent, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaMemset3DAsync_ptsz(struct lwdaPitchedPtr pitchedDevPtr, int value, struct lwdaExtent extent, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaRuntimeGetVersion(int *runtimeVersion);

/**
 * \ingroup LWDART_EXELWTION
 * \brief Obtains a parameter buffer
 *
 * Obtains a parameter buffer which can be filled with parameters for a kernel launch.
 * Parameters passed to ::lwdaLaunchDevice must be allocated via this function.
 *
 * This is a low level API and can only be accessed from Parallel Thread Exelwtion (PTX).
 * LWCA user code should use <<< >>> to launch kernels.
 *
 * \param alignment - Specifies alignment requirement of the parameter buffer
 * \param size      - Specifies size requirement in bytes
 *
 * \return
 * Returns pointer to the allocated parameterBuffer
 * \notefnerr
 *
 * \sa lwdaLaunchDevice
 */
extern __device__ __lwdart_builtin__ void * LWDARTAPI lwdaGetParameterBuffer(size_t alignment, size_t size);

/**
 * \ingroup LWDART_EXELWTION
 * \brief Launches a specified kernel
 *
 * Launches a specified kernel with the specified parameter buffer. A parameter buffer can be obtained
 * by calling ::lwdaGetParameterBuffer().
 *
 * This is a low level API and can only be accessed from Parallel Thread Exelwtion (PTX).
 * LWCA user code should use <<< >>> to launch the kernels.
 *
 * \param func            - Pointer to the kernel to be launched
 * \param parameterBuffer - Holds the parameters to the launched kernel. parameterBuffer can be NULL. (Optional)
 * \param gridDimension   - Specifies grid dimensions
 * \param blockDimension  - Specifies block dimensions
 * \param sharedMemSize   - Specifies size of shared memory
 * \param stream          - Specifies the stream to be used
 *
 * \return
 * ::lwdaSuccess, ::lwdaErrorIlwalidDevice, ::lwdaErrorLaunchMaxDepthExceeded, ::lwdaErrorIlwalidConfiguration,
 * ::lwdaErrorStartupFailure, ::lwdaErrorLaunchPendingCountExceeded, ::lwdaErrorLaunchOutOfResources
 * \notefnerr
 * \n Please refer to Exelwtion Configuration and Parameter Buffer Layout from the LWCA Programming
 * Guide for the detailed descriptions of launch configuration and parameter layout respectively.
 *
 * \sa lwdaGetParameterBuffer
 */
extern __device__ __lwdart_builtin__ void * LWDARTAPI lwdaGetParameterBufferV2(void *func, dim3 gridDimension, dim3 blockDimension, unsigned int sharedMemSize);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaLaunchDevice_ptsz(void *func, void *parameterBuffer, dim3 gridDimension, dim3 blockDimension, unsigned int sharedMemSize, lwdaStream_t stream);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaLaunchDeviceV2_ptsz(void *parameterBuffer, lwdaStream_t stream);

#if defined(LWDA_API_PER_THREAD_DEFAULT_STREAM) && defined(__LWDA_ARCH__)
    // When compiling for the device and per thread default stream is enabled, add
    // a static inline redirect to the per thread stream entry points.

    static __inline__ __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI
    lwdaLaunchDevice(void *func, void *parameterBuffer, dim3 gridDimension, dim3 blockDimension, unsigned int sharedMemSize, lwdaStream_t stream)
    {
        return lwdaLaunchDevice_ptsz(func, parameterBuffer, gridDimension, blockDimension, sharedMemSize, stream);
    }

    static __inline__ __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI
    lwdaLaunchDeviceV2(void *parameterBuffer, lwdaStream_t stream)
    {
        return lwdaLaunchDeviceV2_ptsz(parameterBuffer, stream);
    }
#else
    extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaLaunchDevice(void *func, void *parameterBuffer, dim3 gridDimension, dim3 blockDimension, unsigned int sharedMemSize, lwdaStream_t stream);
    extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaLaunchDeviceV2(void *parameterBuffer, lwdaStream_t stream);
#endif

extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, const void *func, int blockSize, size_t dynamicSmemSize);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags(int *numBlocks, const void *func, int blockSize, size_t dynamicSmemSize, unsigned int flags);

extern __device__ __lwdart_builtin__ unsigned long long LWDARTAPI lwdaCGGetIntrinsicHandle(enum lwdaCGScope scope);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaCGSynchronize(unsigned long long handle, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaCGSynchronizeGrid(unsigned long long handle, unsigned int flags);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaCGGetSize(unsigned int *numThreads, unsigned int *numGrids, unsigned long long handle);
extern __device__ __lwdart_builtin__ lwdaError_t LWDARTAPI lwdaCGGetRank(unsigned int *threadRank, unsigned int *gridRank, unsigned long long handle);
}

template <typename T> static __inline__ __device__ __lwdart_builtin__ lwdaError_t lwdaMalloc(T **devPtr, size_t size);
template <typename T> static __inline__ __device__ __lwdart_builtin__ lwdaError_t lwdaFuncGetAttributes(struct lwdaFuncAttributes *attr, T *entry);
template <typename T> static __inline__ __device__ __lwdart_builtin__ lwdaError_t lwdaOclwpancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, T func, int blockSize, size_t dynamicSmemSize);
template <typename T> static __inline__ __device__ __lwdart_builtin__ lwdaError_t lwdaOclwpancyMaxActiveBlocksPerMultiprocessorWithFlags(int *numBlocks, T func, int blockSize, size_t dynamicSmemSize, unsigned int flags);


#endif // !defined(__LWDA_ARCH__) || (__LWDA_ARCH__ >= 350)
#endif /* defined(__cplusplus) && defined(__LWDACC__) */

#undef __DEPRECATED__
#undef __CDPRT_DEPRECATED

#endif /* !__LWDA_DEVICE_RUNTIME_API_H__ */
