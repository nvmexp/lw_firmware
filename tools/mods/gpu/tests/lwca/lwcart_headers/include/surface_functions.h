/*
 * Copyright 1993-2017 LWPU Corporation.  All rights reserved.
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

#if !defined(__SURFACE_FUNCTIONS_H__)
#define __SURFACE_FUNCTIONS_H__

#if defined(__cplusplus) && defined(__LWDACC__)

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#include "lwda_runtime_api.h"
#include "lwda_surface_types.h"

#if defined(_WIN32)
# define __DEPRECATED__ __declspec(deprecated)
#else
# define __DEPRECATED__  __attribute__((deprecated))
#endif



#ifdef __LWDA_ARCH__
template <typename T> struct __lw_surf_trait {  typedef void * cast_type; };

template<> struct __lw_surf_trait<char> {  typedef char * cast_type; };
template<> struct __lw_surf_trait<signed char> {  typedef signed char * cast_type; };
template<> struct __lw_surf_trait<unsigned char> {  typedef unsigned char * cast_type; };
template<> struct __lw_surf_trait<char1> {  typedef char1 * cast_type; };
template<> struct __lw_surf_trait<uchar1> {  typedef uchar1 * cast_type; };
template<> struct __lw_surf_trait<char2> {  typedef char2 * cast_type; };
template<> struct __lw_surf_trait<uchar2> {  typedef uchar2 * cast_type; };
template<> struct __lw_surf_trait<char4> {  typedef char4 * cast_type; };
template<> struct __lw_surf_trait<uchar4> {  typedef uchar4 * cast_type; };
template<> struct __lw_surf_trait<short> {  typedef short * cast_type; };
template<> struct __lw_surf_trait<unsigned short> {  typedef unsigned short * cast_type; };
template<> struct __lw_surf_trait<short1> {  typedef short1 * cast_type; };
template<> struct __lw_surf_trait<ushort1> {  typedef ushort1 * cast_type; };
template<> struct __lw_surf_trait<short2> {  typedef short2 * cast_type; };
template<> struct __lw_surf_trait<ushort2> {  typedef ushort2 * cast_type; };
template<> struct __lw_surf_trait<short4> {  typedef short4 * cast_type; };
template<> struct __lw_surf_trait<ushort4> {  typedef ushort4 * cast_type; };
template<> struct __lw_surf_trait<int> {  typedef int * cast_type; };
template<> struct __lw_surf_trait<unsigned int> {  typedef unsigned int * cast_type; };
template<> struct __lw_surf_trait<int1> {  typedef int1 * cast_type; };
template<> struct __lw_surf_trait<uint1> {  typedef uint1 * cast_type; };
template<> struct __lw_surf_trait<int2> {  typedef int2 * cast_type; };
template<> struct __lw_surf_trait<uint2> {  typedef uint2 * cast_type; };
template<> struct __lw_surf_trait<int4> {  typedef int4 * cast_type; };
template<> struct __lw_surf_trait<uint4> {  typedef uint4 * cast_type; };
template<> struct __lw_surf_trait<long long> {  typedef long long * cast_type; };
template<> struct __lw_surf_trait<unsigned long long> {  typedef unsigned long long * cast_type; };
template<> struct __lw_surf_trait<longlong1> {  typedef longlong1 * cast_type; };
template<> struct __lw_surf_trait<ulonglong1> {  typedef ulonglong1 * cast_type; };
template<> struct __lw_surf_trait<longlong2> {  typedef longlong2 * cast_type; };
template<> struct __lw_surf_trait<ulonglong2> {  typedef ulonglong2 * cast_type; };
#if !defined(__LP64__)
template<> struct __lw_surf_trait<long> {  typedef int * cast_type; };
template<> struct __lw_surf_trait<unsigned long> {  typedef unsigned int * cast_type; };
template<> struct __lw_surf_trait<long1> {  typedef int1 * cast_type; };
template<> struct __lw_surf_trait<ulong1> {  typedef uint1 * cast_type; };
template<> struct __lw_surf_trait<long2> {  typedef int2 * cast_type; };
template<> struct __lw_surf_trait<ulong2> {  typedef uint2 * cast_type; };
template<> struct __lw_surf_trait<long4> {  typedef uint4 * cast_type; };
template<> struct __lw_surf_trait<ulong4> {  typedef int4 * cast_type; };
#endif
template<> struct __lw_surf_trait<float> {  typedef float * cast_type; };
template<> struct __lw_surf_trait<float1> {  typedef float1 * cast_type; };
template<> struct __lw_surf_trait<float2> {  typedef float2 * cast_type; };
template<> struct __lw_surf_trait<float4> {  typedef float4 * cast_type; };
#endif /* defined(__LWDA_ARCH__) */

template <typename T>
static __DEPRECATED__ __device__  __forceinline__ void surf1Dread(T *res, surface<void, lwdaSurfaceType1D> surf, int x, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf1Dread_v2", (void *)res, s, surf, x, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surf1Dread(surface<void, lwdaSurfaceType1D> surf, int x, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;
  __lw_tex_surf_handler("__surf1Dread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, mode);
  return temp;
#endif
}
  
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf1Dread(T *res, surface<void, lwdaSurfaceType1D> surf, int x, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  *res = surf1Dread<T>(surf, x, mode);
#endif /* __LWDA_ARCH__ */  
}


template <typename T>
static __DEPRECATED__ __device__  __forceinline__ void surf2Dread(T *res, surface<void, lwdaSurfaceType2D> surf, int x, int y, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf2Dread_v2", (void *)res, s, surf, x, y, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surf2Dread(surface<void, lwdaSurfaceType2D> surf, int x, int y, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;
  __lw_tex_surf_handler("__surf2Dread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, y, mode);
  return temp;
#endif
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf2Dread(T *res, surface<void, lwdaSurfaceType2D> surf, int x, int y, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  *res = surf2Dread<T>(surf, x, y, mode);
#endif /* __LWDA_ARCH__ */  
}


template <typename T>
static __DEPRECATED__ __device__  __forceinline__ void surf3Dread(T *res, surface<void, lwdaSurfaceType3D> surf, int x, int y, int z, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf3Dread_v2", (void *)res, s, surf, x, y, z, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surf3Dread(surface<void, lwdaSurfaceType3D> surf, int x, int y, int z, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;
  __lw_tex_surf_handler("__surf3Dread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, y, z, mode);
  return temp;
#endif
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf3Dread(T *res, surface<void, lwdaSurfaceType3D> surf, int x, int y, int z, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  *res = surf3Dread<T>(surf, x, y, z, mode);
#endif /* __LWDA_ARCH__ */  
}



template <typename T>
static __DEPRECATED__ __device__  __forceinline__ void surf1DLayeredread(T *res, surface<void, lwdaSurfaceType1DLayered> surf, int x, int  layer, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf1DLayeredread_v2", (void *)res, s, surf, x,  layer, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surf1DLayeredread(surface<void, lwdaSurfaceType1DLayered> surf, int x, int layer, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;
  __lw_tex_surf_handler("__surf1DLayeredread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, layer, mode);
  return temp;
#endif
}


template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf1DLayeredread(T *res, surface<void, lwdaSurfaceType1DLayered> surf, int x, int layer, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  *res = surf1DLayeredread<T>(surf, x, layer, mode);
#endif /* __LWDA_ARCH__ */  
}


template <typename T>
static __DEPRECATED__ __device__  __forceinline__ void surf2DLayeredread(T *res, surface<void, lwdaSurfaceType2DLayered> surf, int x,  int y, int  layer, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf2DLayeredread_v2", (void *)res, s, surf, x, y, layer, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surf2DLayeredread(surface<void, lwdaSurfaceType2DLayered> surf, int x, int y, int layer, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;
  __lw_tex_surf_handler("__surf2DLayeredread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, y, layer, mode);
  return temp;
#endif
}


template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf2DLayeredread(T *res, surface<void, lwdaSurfaceType2DLayered> surf, int x, int y, int layer, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  *res = surf2DLayeredread<T>(surf, x, y, layer, mode);
#endif /* __LWDA_ARCH__ */  
}


template <typename T>
static __device__  __forceinline__ void surfLwbemapread(T *res, surface<void, lwdaSurfaceTypeLwbemap> surf, int x,  int y, int  face, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surfLwbemapread_v2", (void *)res, s, surf, x, y, face, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surfLwbemapread(surface<void, lwdaSurfaceTypeLwbemap> surf, int x, int y, int face, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;

  __lw_tex_surf_handler("__surfLwbemapread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, y, face, mode);
  return temp;
#endif
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surfLwbemapread(T *res, surface<void, lwdaSurfaceTypeLwbemap> surf, int x, int y, int face, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__  
  *res = surfLwbemapread<T>(surf, x, y, face, mode);
#endif /* __LWDA_ARCH__ */  
}


template <typename T>
static __DEPRECATED__ __device__  __forceinline__ void surfLwbemapLayeredread(T *res, surface<void, lwdaSurfaceTypeLwbemapLayered> surf, int x,  int y, int  layerFace, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surfLwbemapLayeredread_v2", (void *)res, s, surf, x, y, layerFace, mode);
#endif     
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__  T surfLwbemapLayeredread(surface<void, lwdaSurfaceTypeLwbemapLayered> surf, int x, int y, int layerFace, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  T temp;
  __lw_tex_surf_handler("__surfLwbemapLayeredread_v2", (typename __lw_surf_trait<T>::cast_type)&temp, (int)sizeof(T), surf, x, y, layerFace, mode);
  return temp;
#endif
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surfLwbemapLayeredread(T *res, surface<void, lwdaSurfaceTypeLwbemapLayered> surf, int x, int y, int layerFace, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__  
  *res = surfLwbemapLayeredread<T>(surf, x, y, layerFace, mode);
#endif /* __LWDA_ARCH__ */  
}

//surf1Dwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf1Dwrite(T val, surface<void, lwdaSurfaceType1D> surf, int x, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf1Dwrite_v2", (void *)&val, s, surf, x, mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf1Dwrite(T val, surface<void, lwdaSurfaceType1D> surf, int x, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surf1Dwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val, (int)sizeof(T), surf, x,  mode);
#endif /* __LWDA_ARCH__ */  
}


//surf2Dwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf2Dwrite(T val, surface<void, lwdaSurfaceType2D> surf, int x, int y, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf2Dwrite_v2", (void *)&val,  s, surf, x, y, mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf2Dwrite(T val, surface<void, lwdaSurfaceType2D> surf, int x, int y, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surf2Dwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val, (int)sizeof(T), surf, x, y,  mode);
#endif /* __LWDA_ARCH__ */  
}

//surf3Dwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf3Dwrite(T val, surface<void, lwdaSurfaceType3D> surf, int x, int y, int z, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf3Dwrite_v2", (void *)&val,  s, surf, x, y, z,mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf3Dwrite(T val, surface<void, lwdaSurfaceType3D> surf, int x, int y, int z, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surf3Dwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val, (int)sizeof(T), surf, x, y, z,  mode);
#endif /* __LWDA_ARCH__ */  
}

//surf1DLayeredwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf1DLayeredwrite(T val, surface<void, lwdaSurfaceType1DLayered> surf, int x, int layer, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf1DLayeredwrite_v2", (void *)&val,  s, surf, x, layer,mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf1DLayeredwrite(T val, surface<void, lwdaSurfaceType1DLayered> surf, int x, int layer, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surf1DLayeredwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val,  (int)sizeof(T), surf, x, layer, mode);
#endif /* __LWDA_ARCH__ */  
}

//surf2DLayeredwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf2DLayeredwrite(T val, surface<void, lwdaSurfaceType2DLayered> surf, int x, int y, int layer, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surf2DLayeredwrite_v2", (void *)&val, s, surf, x, y, layer,mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surf2DLayeredwrite(T val, surface<void, lwdaSurfaceType2DLayered> surf, int x, int y, int layer, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surf2DLayeredwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val,  (int)sizeof(T), surf, x, y, layer, mode);
#endif /* __LWDA_ARCH__ */  
}

//surfLwbemapwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surfLwbemapwrite(T val, surface<void, lwdaSurfaceTypeLwbemap> surf, int x, int y, int face, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surfLwbemapwrite_v2", (void *)&val, s, surf, x, y, face, mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surfLwbemapwrite(T val, surface<void, lwdaSurfaceTypeLwbemap> surf, int x, int y, int face, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surfLwbemapwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val, (int)sizeof(T), surf, x, y, face,  mode);
#endif /* __LWDA_ARCH__ */  
}


//surfLwbemapLayeredwrite
template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surfLwbemapLayeredwrite(T val, surface<void, lwdaSurfaceTypeLwbemapLayered> surf, int x, int y, int layerFace, int s, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__
  __lw_tex_surf_handler("__surfLwbemapLayeredwrite_v2", (void *)&val, s, surf, x, y, layerFace,  mode);
#endif  
}

template<class T>
static __DEPRECATED__ __device__ __forceinline__ void surfLwbemapLayeredwrite(T val, surface<void, lwdaSurfaceTypeLwbemapLayered> surf, int x, int y, int layerFace, enum lwdaSurfaceBoundaryMode mode = lwdaBoundaryModeTrap)
{
#ifdef __LWDA_ARCH__ 
  __lw_tex_surf_handler("__surfLwbemapLayeredwrite_v2", (typename __lw_surf_trait<T>::cast_type)&val, (int)sizeof(T), surf, x, y, layerFace,  mode);
#endif /* __LWDA_ARCH__ */  
}

#undef __DEPRECATED__


#endif /* __cplusplus && __LWDACC__ */
#endif /* !__SURFACE_FUNCTIONS_H__ */
