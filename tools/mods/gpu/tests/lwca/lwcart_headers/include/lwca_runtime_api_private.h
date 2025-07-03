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

#if !defined(__LWDA_RUNTIME_API_PRIVATE_H__)
#define __LWDA_RUNTIME_API_PRIVATE_H__

#if !defined(__LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__)
#define __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#define __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDA_RUNTIME_API_PRIVATE_H__
#endif

#include "crt/host_defines.h"
#include "builtin_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

void** LWDARTAPI __lwdaRegisterFatBinary(
        void *fatBin
);

void LWDARTAPI __lwdaRegisterFatBinaryEnd(
        void **fatLwbinHandle
);

void LWDARTAPI __lwdaUnregisterFatBinary(
        void **fatLwbinHandle
);

void LWDARTAPI __lwdaRegisterVar(
        void       **fatLwbinHandle,
        char        *hostVar,
        char        *deviceAddress,
        const char  *deviceName,
        int          ext,
        size_t       size,
        int          constant,
        int          global
);

void LWDARTAPI __lwdaRegisterManagedVar(
        void       **fatLwbinHandle,
        void       **hostVarPtrAddress,
        char        *deviceAddress,
        const char  *deviceName,
        int          ext,
        size_t       size,
        int          constant,
        int          global
);

void LWDARTAPI __lwdaRegisterHostVar(
        void       **fatLwbinHandle,
        const char  *deviceSymbolName,
        void        *hostVarAddress,
        size_t       size
);

void LWDARTAPI __lwdaRegisterTexture(
        void                          **fatLwbinHandle,
        const struct textureReference  *hostVar,
        const void                    **deviceAddress,
        const char                     *deviceName,
        int                             dim,
        int                             read_normalized_float,
        int                             ext
);

void LWDARTAPI __lwdaRegisterSurface(
        void                          **fatLwbinHandle,
        const struct surfaceReference  *hostVar,
        const void                    **deviceAddress,
        const char                     *deviceName,
        int                             dim,
        int                             ext
);

void LWDARTAPI __lwdaRegisterFunction(
        void        **fatLwbinHandle,
        const char   *hostFun,
        char         *deviceFun,
        const char   *deviceName,
        int           thread_limit,
        uint3        *tid,
        uint3        *bid,
        dim3         *bDim,
        dim3         *gDim,
        int          *wSize
);

char LWDARTAPI __lwdaInitModule(
        void **fatLwbinHandle
);

lwdaError_t LWDARTAPI __lwdaPushCallConfiguration(
        dim3         gridDim,
        dim3         blockDim,
        size_t       sharedMem,
        lwdaStream_t stream
);

lwdaError_t LWDARTAPI __lwdaPopCallConfiguration(
        dim3         *gridDim,
        dim3         *blockDim,
        size_t       *sharedMem,
        lwdaStream_t *stream
);

#if defined(__cplusplus)
}
#endif

#if defined(__UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDA_RUNTIME_API_PRIVATE_H__)
#undef __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#undef __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDA_RUNTIME_API_PRIVATE_H__
#endif

#endif /* !defined(__LWDA_RUNTIME_API_PRIVATE_H__) */

