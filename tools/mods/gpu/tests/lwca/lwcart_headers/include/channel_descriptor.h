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

#if !defined(__CHANNEL_DESCRIPTOR_H__)
#define __CHANNEL_DESCRIPTOR_H__

#if defined(__cplusplus)

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#include "lwda_runtime_api.h"

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

/**
 * \addtogroup LWDART_HIGHLEVEL
 *
 * @{
 */

/**
 * \brief \hl Returns a channel descriptor using the specified format
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
 * ::lwdaChannelFormatKindUnsigned, lwdaChannelFormatKindFloat,
 * ::lwdaChannelFormatKindSignedNormalized8X1, ::lwdaChannelFormatKindSignedNormalized8X2,
 * ::lwdaChannelFormatKindSignedNormalized8X4,
 * ::lwdaChannelFormatKindUnsignedNormalized8X1, ::lwdaChannelFormatKindUnsignedNormalized8X2,
 * ::lwdaChannelFormatKindUnsignedNormalized8X4,
 * ::lwdaChannelFormatKindSignedNormalized16X1, ::lwdaChannelFormatKindSignedNormalized16X2,
 * ::lwdaChannelFormatKindSignedNormalized16X4,
 * ::lwdaChannelFormatKindUnsignedNormalized16X1, ::lwdaChannelFormatKindUnsignedNormalized16X2,
 * ::lwdaChannelFormatKindUnsignedNormalized16X4
 * or ::lwdaChannelFormatKindLW12.
 *
 * The format is specified by the template specialization.
 *
 * The template function specializes for the following scalar types:
 * char, signed char, unsigned char, short, unsigned short, int, unsigned int, long, unsigned long, and float.
 * The template function specializes for the following vector types:
 * char{1|2|4}, uchar{1|2|4}, short{1|2|4}, ushort{1|2|4}, int{1|2|4}, uint{1|2|4}, long{1|2|4}, ulong{1|2|4}, float{1|2|4}.
 * The template function specializes for following lwdaChannelFormatKind enum values:
 * ::lwdaChannelFormatKind{Uns|S}ignedNormalized{8|16}X{1|2|4}, and ::lwdaChannelFormatKindLW12.
 *
 * Ilwoking the function on a type without a specialization defaults to creating a channel format of kind ::lwdaChannelFormatKindNone
 *
 * \return
 * Channel descriptor with format \p f
 *
 * \sa \ref ::lwdaCreateChannelDesc(int,int,int,int,lwdaChannelFormatKind) "lwdaCreateChannelDesc (Low level)",
 * ::lwdaGetChannelDesc, ::lwdaGetTextureReference,
 * \ref ::lwdaBindTexture(size_t*, const struct texture< T, dim, readMode>&, const void*, const struct lwdaChannelFormatDesc&, size_t) "lwdaBindTexture (High level)",
 * \ref ::lwdaBindTexture(size_t*, const struct texture< T, dim, readMode>&, const void*, size_t) "lwdaBindTexture (High level, inherited channel descriptor)",
 * \ref ::lwdaBindTexture2D(size_t*, const struct texture< T, dim, readMode>&, const void*, const struct lwdaChannelFormatDesc&, size_t, size_t, size_t) "lwdaBindTexture2D (High level)",
 * \ref ::lwdaBindTextureToArray(const struct texture< T, dim, readMode>&, lwdaArray_const_t, const struct lwdaChannelFormatDesc&) "lwdaBindTextureToArray (High level)",
 * \ref ::lwdaBindTextureToArray(const struct texture< T, dim, readMode>&, lwdaArray_const_t) "lwdaBindTextureToArray (High level, inherited channel descriptor)",
 * \ref ::lwdaUnbindTexture(const struct texture< T, dim, readMode>&) "lwdaUnbindTexture (High level)",
 * \ref ::lwdaGetTextureAlignmentOffset(size_t*, const struct texture< T, dim, readMode>&) "lwdaGetTextureAlignmentOffset (High level)"
 */
template<class T> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc(void)
{
  return lwdaCreateChannelDesc(0, 0, 0, 0, lwdaChannelFormatKindNone);
}

static __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDescHalf(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindFloat);
}

static __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDescHalf1(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindFloat);
}

static __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDescHalf2(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindFloat);
}

static __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDescHalf4(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindFloat);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<char>(void)
{
  int e = (int)sizeof(char) * 8;

#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<signed char>(void)
{
  int e = (int)sizeof(signed char) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<unsigned char>(void)
{
  int e = (int)sizeof(unsigned char) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<char1>(void)
{
  int e = (int)sizeof(signed char) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<uchar1>(void)
{
  int e = (int)sizeof(unsigned char) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<char2>(void)
{
  int e = (int)sizeof(signed char) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<uchar2>(void)
{
  int e = (int)sizeof(unsigned char) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<char4>(void)
{
  int e = (int)sizeof(signed char) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<uchar4>(void)
{
  int e = (int)sizeof(unsigned char) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<short>(void)
{
  int e = (int)sizeof(short) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<unsigned short>(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<short1>(void)
{
  int e = (int)sizeof(short) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<ushort1>(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<short2>(void)
{
  int e = (int)sizeof(short) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<ushort2>(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<short4>(void)
{
  int e = (int)sizeof(short) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<ushort4>(void)
{
  int e = (int)sizeof(unsigned short) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<int>(void)
{
  int e = (int)sizeof(int) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<unsigned int>(void)
{
  int e = (int)sizeof(unsigned int) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<int1>(void)
{
  int e = (int)sizeof(int) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<uint1>(void)
{
  int e = (int)sizeof(unsigned int) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<int2>(void)
{
  int e = (int)sizeof(int) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<uint2>(void)
{
  int e = (int)sizeof(unsigned int) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<int4>(void)
{
  int e = (int)sizeof(int) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<uint4>(void)
{
  int e = (int)sizeof(unsigned int) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindUnsigned);
}

#if !defined(__LP64__)

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<long>(void)
{
  int e = (int)sizeof(long) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<unsigned long>(void)
{
  int e = (int)sizeof(unsigned long) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<long1>(void)
{
  int e = (int)sizeof(long) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<ulong1>(void)
{
  int e = (int)sizeof(unsigned long) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<long2>(void)
{
  int e = (int)sizeof(long) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<ulong2>(void)
{
  int e = (int)sizeof(unsigned long) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindUnsigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<long4>(void)
{
  int e = (int)sizeof(long) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindSigned);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<ulong4>(void)
{
  int e = (int)sizeof(unsigned long) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindUnsigned);
}

#endif /* !__LP64__ */

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<float>(void)
{
  int e = (int)sizeof(float) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindFloat);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<float1>(void)
{
  int e = (int)sizeof(float) * 8;

  return lwdaCreateChannelDesc(e, 0, 0, 0, lwdaChannelFormatKindFloat);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<float2>(void)
{
  int e = (int)sizeof(float) * 8;

  return lwdaCreateChannelDesc(e, e, 0, 0, lwdaChannelFormatKindFloat);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<float4>(void)
{
  int e = (int)sizeof(float) * 8;

  return lwdaCreateChannelDesc(e, e, e, e, lwdaChannelFormatKindFloat);
}

static __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDescLW12(void)
{
    int e = (int)sizeof(char) * 8;

    return lwdaCreateChannelDesc(e, e, e, 0, lwdaChannelFormatKindLW12);
}

template<lwdaChannelFormatKind> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc(void)
{
    return lwdaCreateChannelDesc(0, 0, 0, 0, lwdaChannelFormatKindNone);
}

/* Signed 8-bit normalized integer formats */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedNormalized8X1>(void)
{
    return lwdaCreateChannelDesc(8, 0, 0, 0, lwdaChannelFormatKindSignedNormalized8X1);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedNormalized8X2>(void)
{
    return lwdaCreateChannelDesc(8, 8, 0, 0, lwdaChannelFormatKindSignedNormalized8X2);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedNormalized8X4>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindSignedNormalized8X4);
}

/* Unsigned 8-bit normalized integer formats */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedNormalized8X1>(void)
{
    return lwdaCreateChannelDesc(8, 0, 0, 0, lwdaChannelFormatKindUnsignedNormalized8X1);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedNormalized8X2>(void)
{
    return lwdaCreateChannelDesc(8, 8, 0, 0, lwdaChannelFormatKindUnsignedNormalized8X2);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedNormalized8X4>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedNormalized8X4);
}

/* Signed 16-bit normalized integer formats */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedNormalized16X1>(void)
{
    return lwdaCreateChannelDesc(16, 0, 0, 0, lwdaChannelFormatKindSignedNormalized16X1);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedNormalized16X2>(void)
{
    return lwdaCreateChannelDesc(16, 16, 0, 0, lwdaChannelFormatKindSignedNormalized16X2);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedNormalized16X4>(void)
{
    return lwdaCreateChannelDesc(16, 16, 16, 16, lwdaChannelFormatKindSignedNormalized16X4);
}

/* Unsigned 16-bit normalized integer formats */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedNormalized16X1>(void)
{
    return lwdaCreateChannelDesc(16, 0, 0, 0, lwdaChannelFormatKindUnsignedNormalized16X1);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedNormalized16X2>(void)
{
    return lwdaCreateChannelDesc(16, 16, 0, 0, lwdaChannelFormatKindUnsignedNormalized16X2);
}

template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedNormalized16X4>(void)
{
    return lwdaCreateChannelDesc(16, 16, 16, 16, lwdaChannelFormatKindUnsignedNormalized16X4);
}

/* LW12 format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindLW12>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 0, lwdaChannelFormatKindLW12);
}

/* BC1 format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed1>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed1);
}

/* BC1sRGB format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed1SRGB>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed1SRGB);
}

/* BC2 format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed2>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed2);
}

/* BC2sRGB format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed2SRGB>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed2SRGB);
}

/* BC3 format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed3>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed3);
}

/* BC3sRGB format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed3SRGB>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed3SRGB);
}

/* BC4 unsigned format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed4>(void)
{
    return lwdaCreateChannelDesc(8, 0, 0, 0, lwdaChannelFormatKindUnsignedBlockCompressed4);
}

/* BC4 signed format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedBlockCompressed4>(void)
{
    return lwdaCreateChannelDesc(8, 0, 0, 0, lwdaChannelFormatKindSignedBlockCompressed4);
}

/* BC5 unsigned format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed5>(void)
{
    return lwdaCreateChannelDesc(8, 8, 0, 0, lwdaChannelFormatKindUnsignedBlockCompressed5);
}

/* BC5 signed format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedBlockCompressed5>(void)
{
    return lwdaCreateChannelDesc(8, 8, 0, 0, lwdaChannelFormatKindSignedBlockCompressed5);
}

/* BC6H unsigned format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed6H>(void)
{
    return lwdaCreateChannelDesc(16, 16, 16, 0, lwdaChannelFormatKindUnsignedBlockCompressed6H);
}

/* BC6H signed format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindSignedBlockCompressed6H>(void)
{
    return lwdaCreateChannelDesc(16, 16, 16, 0, lwdaChannelFormatKindSignedBlockCompressed6H);
}

/* BC7 format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed7>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed7);
}

/* BC7sRGB format */
template<> __inline__ __host__ lwdaChannelFormatDesc lwdaCreateChannelDesc<lwdaChannelFormatKindUnsignedBlockCompressed7SRGB>(void)
{
    return lwdaCreateChannelDesc(8, 8, 8, 8, lwdaChannelFormatKindUnsignedBlockCompressed7SRGB);
}

#endif /* __cplusplus */

/** @} */
/** @} */ /* END LWDART_TEXTURE_HL */

#endif /* !__CHANNEL_DESCRIPTOR_H__ */
