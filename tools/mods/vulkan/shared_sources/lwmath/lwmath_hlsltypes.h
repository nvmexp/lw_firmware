/* Copyright 2002-2020 by LWPU Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of LWPU CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROLWREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LW_HLSLSHADER_TYPES_H
#define LW_HLSLSHADER_TYPES_H

#include <LwFoundation.h>
#include "lwmath_types.h"

namespace lwmath
{

#if defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || defined(__AMD64__)
  // Matrices, must align to 4 vector (16 bytes)
  LW_ALIGN(16, typedef mat4f float4x4);

  // vectors, 4-tuples and 3-tuples must align to 16 bytes
  //  2-vectors must align to 8 bytes
  LW_ALIGN(16, typedef vec4f float4);
  LW_ALIGN(16, typedef vec3f float3);
  LW_ALIGN(8,  typedef vec2f float2);

  LW_ALIGN(16, typedef vec4i int4);
  LW_ALIGN(16, typedef vec3i int3);
  LW_ALIGN(8,  typedef vec2i int2);

  LW_ALIGN(16, typedef vec4ui uint4);
  LW_ALIGN(16, typedef vec3ui uint3);
  LW_ALIGN(8,  typedef vec2ui uint2);
#else
  // Matrices, must align to 4 vector (16 bytes)
  typedef  mat4f float4x4;

  // vectors, 4-tuples and 3-tuples must align to 16 bytes
  //  2-vectors must align to 8 bytes
  typedef  vec4f float4;
  typedef  vec3f float3;
  typedef  vec2f float2;

  typedef  vec4i int4;
  typedef  vec3i int3;
  typedef  vec2i int2;

  typedef  vec4ui uint4;
  typedef  vec3ui uint3;
  typedef  vec2ui uint2;
#endif

//class to make uint look like bool to make GLSL packing rules happy
struct boolClass
{
    unsigned int _rep;

    boolClass() : _rep(false) {}
    boolClass(bool b) : _rep(b) {}
    operator bool() { return _rep == 0 ? false : true; }
    boolClass& operator=(bool b) { _rep = b; return *this; }
};

LW_ALIGN(4, typedef boolClass bool32);

}

#endif
