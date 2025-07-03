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
/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaPeerRegister_v4000();

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaPeerUnregister_v4000();

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaPeerGetDevicePointer_v4000();

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaStreamDestroy_v3020(lwdaStream_t stream);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaOclwpancyMaxActiveBlocksPerMultiprocessor_v6000(int *numBlocks, const void *func, size_t numDynamicSmemBytes);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaConfigureCall_v3020(dim3 gridDim, dim3 blockDim, size_t sharedMem __dv(0), lwdaStream_t stream __dv(0));

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaSetupArgument_v3020(const void *arg, size_t size, size_t offset);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaLaunch_v3020(const void *func);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaLaunch_ptsz_v7000(const void *func);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaStreamSetFlags_v10200(lwdaStream_t hStream, unsigned int flags);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaStreamSetFlags_ptsz_v10200(lwdaStream_t hStream, unsigned int flags);


/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaThreadExit_v3020(void);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaThreadSynchronize_v3020(void);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaThreadSetLimit_v3020(enum lwdaLimit limit, size_t value);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaThreadGetLimit_v3020(size_t *pValue, enum lwdaLimit limit);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaThreadGetCacheConfig_v3020(enum lwdaFuncCache *pCacheConfig);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaThreadSetCacheConfig_v3020(enum lwdaFuncCache cacheConfig);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaSetDoubleForDevice_v3020(double *d);

/// \brief file=lwda_runtime_api.h
extern __host__ lwdaError_t LWDARTAPI lwdaSetDoubleForHost_v3020(double *d);


/** lwdart_removed.h 
 *
 * All functions which have interface changes or which are removed from lwdart should have 
 * prototypes in this file. For example, if the arguments of lwdaStreamCreate are changed from 
 *
 *     extern __host__ lwdaError_t LWDARTAPI lwdaStreamCreate(lwdaStream_t *pStream);
 * to
 *     extern __host__ lwdaError_t LWDARTAPI lwdaStreamCreate(lwdaStream_t *pStream, int flags);
 *
 * then the older prototype of the function should be included in this file with the function name
 * appended with _v<version_number>. <version_number> should be the original version of 
 * lwdaStreamCreate specified in lwdart_master.def with the "version=" tag. Also, a doxygen comment 
 * before the function which provides the file in which this prototype used to reside is mandatory. 
 * For lwdaStreamCreate, the prototype included in this file should look like: 
 *
 *    /// \brief file=lwda_runtime_api.h 
 *    extern __host__ lwdaError_t LWDARTAPI lwdaStreamCreate_v3020(lwdaStream_t *pStream);
 *  
 * Note that we move the comments to the end of the file to WAR 
 * a doxygen bug in which the comments might be messed up with 
 * the brief comment, and fails the build (see bug 798787). 
 */ 

