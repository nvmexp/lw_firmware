/*
 * Copyright 2021-2021 LWPU Corporation.  All rights reserved.
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

#if !defined(__LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__)
#if defined(_MSC_VER)
#pragma message("crt/lwdacc_ext.h is an internal header file and must not be used directly.  Please use lwda_runtime_api.h or lwda_runtime.h instead.")
#else
#warning "crt/lwdacc_ext.h is an internal header file and must not be used directly.  Please use lwda_runtime_api.h or lwda_runtime.h instead."
#endif
#define __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#define __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDACC_EXT_H__
#endif

#if !defined(__LWDACC_EXT_H__)
#define __LWDACC_EXT_H__

// move this to host_defines.h
#if defined(_MSC_VER)
#define __cluster_dims__(...) \
        __declspec(cluster_dims(__VA_ARGS__))
        
#else
#define __cluster_dims__(...) \
        __attribute__((cluster_dims(__VA_ARGS__)))
#endif
      

// move this to sm_90_rt.h/hpp
        
//NOTE: when creating new sm_90_rt.h/hpp, match linkage structure as 
//found for sm_70.h/hpp, and update sw/lwca/tools/lwrtc/lwrtc-builtins.lw
//to explicitly include the .h file.

#if !defined(__LWDA_ARCH__) || (__LWDA_ARCH__ >= 900)
extern "C" {
  __device__ unsigned  __lw_isClusterShared_impl(const void *);
  __device__ void * __lw_cluster_map_shared_rank_impl(const void *, unsigned);
  __device__ unsigned __lw_cluster_query_shared_rank_impl(const void *);
  __device__ unsigned __lw_clusterDimIsSpecifed_impl();
  __device__ void __lw_clusterDim_impl(unsigned *, unsigned *, unsigned *);
  __device__ void __lw_clusterRelativeBlockIdx_impl(unsigned *, 
                                                    unsigned *, unsigned *);
  __device__ void __lw_clusterGridDimInClusters_impl(unsigned *, 
                                                     unsigned *, unsigned *);
  __device__ void __lw_clusterIdx_impl(unsigned *, unsigned *, unsigned *);
  __device__ unsigned __lw_clusterRelativeBlockRank_impl();
  __device__ unsigned __lw_clusterSizeInBlocks_impl();
  __device__ void __lw_cluster_barrier_arrive_impl();
  __device__ void __lw_cluster_barrier_arrive_relaxed_impl();
  __device__ void __lw_cluster_barrier_wait_impl();
  __device__ void __lw_threadfence_cluster_impl();
} // extern "C"

inline __device__  unsigned int __isCtaShared(const void *ptr) 
{
  return __isShared(ptr);
}

inline __device__ unsigned int __isClusterShared(const void *ptr) 
{
  return __lw_isClusterShared_impl(ptr);
}

inline __device__ void *__cluster_map_shared_rank(const void *ptr, 
                                                  unsigned target_block_rank)
{
  return __lw_cluster_map_shared_rank_impl(ptr, target_block_rank);
}

inline __device__ unsigned int __cluster_query_shared_rank(const void *ptr)
{
  return __lw_cluster_query_shared_rank_impl(ptr);
}

inline __device__ uint2 __cluster_map_shared_multicast(const void *ptr, 
                                                 unsigned int cluster_cta_mask)
{
  return make_uint2((unsigned)__cvta_generic_to_shared(ptr), cluster_cta_mask);
}

inline __device__ unsigned int __clusterDimIsSpecified()
{
  return __lw_clusterDimIsSpecifed_impl();
}  

inline __device__ dim3 __clusterDim()
{
  unsigned x, y, z;
  __lw_clusterDim_impl(&x, &y, &z);
  return dim3(x,y,z);
}

inline __device__ dim3 __clusterRelativeBlockIdx()
{
  unsigned x, y, z;
  __lw_clusterRelativeBlockIdx_impl(&x, &y, &z);
  return dim3(x,y,z);
}

inline __device__ dim3 __clusterGridDimInClusters()
{
  unsigned x, y, z;
  __lw_clusterGridDimInClusters_impl(&x, &y, &z);
  return dim3(x,y,z);
}

inline __device__ dim3 __clusterIdx()
{
  unsigned x, y, z;
  __lw_clusterIdx_impl(&x, &y, &z);
  return dim3(x,y,z);
}

inline __device__ unsigned int __clusterRelativeBlockRank()
{
  return __lw_clusterRelativeBlockRank_impl();
}

inline __device__ unsigned int __clusterSizeInBlocks()
{
  return __lw_clusterSizeInBlocks_impl();
}

inline __device__ void __cluster_barrier_arrive()
{
  __lw_cluster_barrier_arrive_impl();
}

inline __device__ void __cluster_barrier_arrive_relaxed()
{
  __lw_cluster_barrier_arrive_relaxed_impl();
}

inline __device__ void __cluster_barrier_wait()
{
  __lw_cluster_barrier_wait_impl();
}

inline __device__ void __threadfence_cluster()
{
  __lw_threadfence_cluster_impl();
}
#endif  /* !defined(__LWDA_ARCH__) || (__LWDA_ARCH__ >= 900) */

        
#endif /* !__LWDACC_EXT_H__ */

#if defined(__UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDACC_EXT_H__)
#undef __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#undef __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_LWDACC_EXT_H__
#endif
