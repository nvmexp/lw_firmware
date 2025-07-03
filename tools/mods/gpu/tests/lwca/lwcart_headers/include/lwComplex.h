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

#if !defined(LW_COMPLEX_H_)
#define LW_COMPLEX_H_

#if !defined(__LWDACC_RTC__)
#if defined(__GNUC__)
#if defined(__clang__) || (!defined(__PGIC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)))
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#endif
#endif

/* When trying to include C header file in C++ Code extern "C" is required
 * But the Standard QNX headers already have ifdef extern in them when compiling C++ Code
 * extern "C" cannot be nested
 * Hence keep the header out of extern "C" block
 */

#if !defined(__LWDACC__)
#include <math.h>       /* import fabsf, sqrt */
#endif /* !defined(__LWDACC__) */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "vector_types.h"

typedef float2 lwFloatComplex;

__host__ __device__ static __inline__ float lwCrealf (lwFloatComplex x) 
{ 
    return x.x; 
}

__host__ __device__ static __inline__ float lwCimagf (lwFloatComplex x) 
{ 
    return x.y; 
}

__host__ __device__ static __inline__ lwFloatComplex make_lwFloatComplex 
                                                             (float r, float i)
{
    lwFloatComplex res;
    res.x = r;
    res.y = i;
    return res;
}

__host__ __device__ static __inline__ lwFloatComplex lwConjf (lwFloatComplex x)
{
    return make_lwFloatComplex (lwCrealf(x), -lwCimagf(x));
}
__host__ __device__ static __inline__ lwFloatComplex lwCaddf (lwFloatComplex x,
                                                              lwFloatComplex y)
{
    return make_lwFloatComplex (lwCrealf(x) + lwCrealf(y), 
                                lwCimagf(x) + lwCimagf(y));
}

__host__ __device__ static __inline__ lwFloatComplex lwCsubf (lwFloatComplex x,
                                                              lwFloatComplex y)
{
        return make_lwFloatComplex (lwCrealf(x) - lwCrealf(y), 
                                    lwCimagf(x) - lwCimagf(y));
}

/* This implementation could suffer from intermediate overflow even though
 * the final result would be in range. However, various implementations do
 * not guard against this (presumably to avoid losing performance), so we 
 * don't do it either to stay competitive.
 */
__host__ __device__ static __inline__ lwFloatComplex lwCmulf (lwFloatComplex x,
                                                              lwFloatComplex y)
{
    lwFloatComplex prod;
    prod = make_lwFloatComplex  ((lwCrealf(x) * lwCrealf(y)) - 
                                 (lwCimagf(x) * lwCimagf(y)),
                                 (lwCrealf(x) * lwCimagf(y)) + 
                                 (lwCimagf(x) * lwCrealf(y)));
    return prod;
}

/* This implementation guards against intermediate underflow and overflow
 * by scaling. Such guarded implementations are usually the default for
 * complex library implementations, with some also offering an unguarded,
 * faster version.
 */
__host__ __device__ static __inline__ lwFloatComplex lwCdivf (lwFloatComplex x,
                                                              lwFloatComplex y)
{
    lwFloatComplex quot;
    float s = fabsf(lwCrealf(y)) + fabsf(lwCimagf(y));
    float oos = 1.0f / s;
    float ars = lwCrealf(x) * oos;
    float ais = lwCimagf(x) * oos;
    float brs = lwCrealf(y) * oos;
    float bis = lwCimagf(y) * oos;
    s = (brs * brs) + (bis * bis);
    oos = 1.0f / s;
    quot = make_lwFloatComplex (((ars * brs) + (ais * bis)) * oos,
                                ((ais * brs) - (ars * bis)) * oos);
    return quot;
}

/* 
 * We would like to call hypotf(), but it's not available on all platforms.
 * This discrete implementation guards against intermediate underflow and 
 * overflow by scaling. Otherwise we would lose half the exponent range. 
 * There are various ways of doing guarded computation. For now chose the 
 * simplest and fastest solution, however this may suffer from inaclwracies 
 * if sqrt and division are not IEEE compliant. 
 */
__host__ __device__ static __inline__ float lwCabsf (lwFloatComplex x)
{
    float a = lwCrealf(x);
    float b = lwCimagf(x);
    float v, w, t;
    a = fabsf(a);
    b = fabsf(b);
    if (a > b) {
        v = a;
        w = b; 
    } else {
        v = b;
        w = a;
    }
    t = w / v;
    t = 1.0f + t * t;
    t = v * sqrtf(t);
    if ((v == 0.0f) || (v > 3.402823466e38f) || (w > 3.402823466e38f)) {
        t = v + w;
    }
    return t;
}

/* Double precision */
typedef double2 lwDoubleComplex;

__host__ __device__ static __inline__ double lwCreal (lwDoubleComplex x) 
{ 
    return x.x; 
}

__host__ __device__ static __inline__ double lwCimag (lwDoubleComplex x) 
{ 
    return x.y; 
}

__host__ __device__ static __inline__ lwDoubleComplex make_lwDoubleComplex 
                                                           (double r, double i)
{
    lwDoubleComplex res;
    res.x = r;
    res.y = i;
    return res;
}

__host__ __device__ static __inline__ lwDoubleComplex lwConj(lwDoubleComplex x)
{
    return make_lwDoubleComplex (lwCreal(x), -lwCimag(x));
}

__host__ __device__ static __inline__ lwDoubleComplex lwCadd(lwDoubleComplex x,
                                                             lwDoubleComplex y)
{
    return make_lwDoubleComplex (lwCreal(x) + lwCreal(y), 
                                 lwCimag(x) + lwCimag(y));
}

__host__ __device__ static __inline__ lwDoubleComplex lwCsub(lwDoubleComplex x,
                                                             lwDoubleComplex y)
{
    return make_lwDoubleComplex (lwCreal(x) - lwCreal(y), 
                                 lwCimag(x) - lwCimag(y));
}

/* This implementation could suffer from intermediate overflow even though
 * the final result would be in range. However, various implementations do
 * not guard against this (presumably to avoid losing performance), so we 
 * don't do it either to stay competitive.
 */
__host__ __device__ static __inline__ lwDoubleComplex lwCmul(lwDoubleComplex x,
                                                             lwDoubleComplex y)
{
    lwDoubleComplex prod;
    prod = make_lwDoubleComplex ((lwCreal(x) * lwCreal(y)) - 
                                 (lwCimag(x) * lwCimag(y)),
                                 (lwCreal(x) * lwCimag(y)) + 
                                 (lwCimag(x) * lwCreal(y)));
    return prod;
}

/* This implementation guards against intermediate underflow and overflow
 * by scaling. Such guarded implementations are usually the default for
 * complex library implementations, with some also offering an unguarded,
 * faster version.
 */
__host__ __device__ static __inline__ lwDoubleComplex lwCdiv(lwDoubleComplex x,
                                                             lwDoubleComplex y)
{
    lwDoubleComplex quot;
    double s = (fabs(lwCreal(y))) + (fabs(lwCimag(y)));
    double oos = 1.0 / s;
    double ars = lwCreal(x) * oos;
    double ais = lwCimag(x) * oos;
    double brs = lwCreal(y) * oos;
    double bis = lwCimag(y) * oos;
    s = (brs * brs) + (bis * bis);
    oos = 1.0 / s;
    quot = make_lwDoubleComplex (((ars * brs) + (ais * bis)) * oos,
                                 ((ais * brs) - (ars * bis)) * oos);
    return quot;
}

/* This implementation guards against intermediate underflow and overflow
 * by scaling. Otherwise we would lose half the exponent range. There are
 * various ways of doing guarded computation. For now chose the simplest
 * and fastest solution, however this may suffer from inaclwracies if sqrt
 * and division are not IEEE compliant.
 */
__host__ __device__ static __inline__ double lwCabs (lwDoubleComplex x)
{
    double a = lwCreal(x);
    double b = lwCimag(x);
    double v, w, t;
    a = fabs(a);
    b = fabs(b);
    if (a > b) {
        v = a;
        w = b; 
    } else {
        v = b;
        w = a;
    }
    t = w / v;
    t = 1.0 + t * t;
    t = v * sqrt(t);
    if ((v == 0.0) || 
        (v > 1.79769313486231570e+308) || (w > 1.79769313486231570e+308)) {
        t = v + w;
    }
    return t;
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */

/* aliases */
typedef lwFloatComplex lwComplex;
__host__ __device__ static __inline__ lwComplex make_lwComplex (float x, 
                                                                float y) 
{ 
    return make_lwFloatComplex (x, y); 
}

/* float-to-double promotion */
__host__ __device__ static __inline__ lwDoubleComplex lwComplexFloatToDouble
                                                      (lwFloatComplex c)
{
    return make_lwDoubleComplex ((double)lwCrealf(c), (double)lwCimagf(c));
}

__host__ __device__ static __inline__ lwFloatComplex lwComplexDoubleToFloat
(lwDoubleComplex c)
{
	return make_lwFloatComplex ((float)lwCreal(c), (float)lwCimag(c));
}


__host__ __device__ static __inline__  lwComplex lwCfmaf( lwComplex x, lwComplex y, lwComplex d)
{
    float real_res;
    float imag_res;
    
    real_res = (lwCrealf(x) *  lwCrealf(y)) + lwCrealf(d);
    imag_res = (lwCrealf(x) *  lwCimagf(y)) + lwCimagf(d);
            
    real_res = -(lwCimagf(x) * lwCimagf(y))  + real_res;  
    imag_res =  (lwCimagf(x) *  lwCrealf(y)) + imag_res;          
     
    return make_lwComplex(real_res, imag_res);
}

__host__ __device__ static __inline__  lwDoubleComplex lwCfma( lwDoubleComplex x, lwDoubleComplex y, lwDoubleComplex d)
{
    double real_res;
    double imag_res;
    
    real_res = (lwCreal(x) *  lwCreal(y)) + lwCreal(d);
    imag_res = (lwCreal(x) *  lwCimag(y)) + lwCimag(d);
            
    real_res = -(lwCimag(x) * lwCimag(y))  + real_res;  
    imag_res =  (lwCimag(x) *  lwCreal(y)) + imag_res;     
     
    return make_lwDoubleComplex(real_res, imag_res);
}

#endif /* !defined(LW_COMPLEX_H_) */
