/* Copyright 2012-2020 by LWPU Corporation.  All rights reserved.
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

#ifndef _lwmathtypes_h_
#define _lwmathtypes_h_

#include <assert.h>
#include <math.h>

#ifdef _WIN32
#include <limits>
#else
#include <limits.h>
#endif

#ifdef MACOS
#define sqrtf sqrt
#define sinf sin
#define cosf cos
#define tanf tan
#endif

#include <float.h>
#include <memory.h>
#include <stdlib.h>

namespace lwmath
{

typedef float lw_scalar;

#define lw_zero lwmath::lw_scalar(0)
#define lw_one_half lwmath::lw_scalar(0.5)
#define lw_one lwmath::lw_scalar(1.0)
#define lw_two lwmath::lw_scalar(2)
#define lw_half_pi lwmath::lw_scalar(3.14159265358979323846264338327950288419716939937510582 * 0.5)
#define lw_quarter_pi lwmath::lw_scalar(3.14159265358979323846264338327950288419716939937510582 * 0.25)
#define lw_pi lwmath::lw_scalar(3.14159265358979323846264338327950288419716939937510582)
#define lw_two_pi lwmath::lw_scalar(3.14159265358979323846264338327950288419716939937510582 * 2.0)
#define lw_oo_pi (lw_one / lw_pi)
#define lw_oo_two_pi (lw_one / lw_two_pi)
#define lw_oo_255 (lw_one / lwmath::lw_scalar(255))
#define lw_oo_128 (lw_one / lwmath::lw_scalar(128))
#define lw_to_rad (lw_pi / lwmath::lw_scalar(180))
#define lw_to_deg (lwmath::lw_scalar(180) / lw_pi)
#define rad2deg(a) ((a)*lw_to_deg)
#define deg2rad(a) ((a)*lw_to_rad)
#define lw_eps lwmath::lw_scalar(10e-6)
#define lw_double_eps (lwmath::lw_scalar(10e-6) * lw_two)
#define lw_big_eps lwmath::lw_scalar(10e-2)
#define lw_small_eps lwmath::lw_scalar(10e-6)
#define lw_sqrthalf lwmath::lw_scalar(0.7071067811865475244)

#define lw_scalar_max lwmath::lw_scalar(FLT_MAX)
#define lw_scalar_min lwmath::lw_scalar(FLT_MIN)

template <class T>
struct vector2;
template <class T>
struct vector3;
template <class T>
struct vector4;
template <class T>
struct matrix3;
template <class T>
struct matrix4;
template <class T>
struct quaternion;

template <class T>
struct vector2
{
  vector2() {}
  vector2(T x)
      : x(T(x))
      , y(T(x))
  {
  }
  template <typename T0, typename T1>
  vector2(T0 x, T1 y)
      : x(T(x))
      , y(T(y))
  {
  }
  vector2(const T* xy)
      : x(xy[0])
      , y(xy[1])
  {
  }
  vector2(const vector2& u)
      : x(u.x)
      , y(u.y)
  {
  }
  vector2(const vector3<T>&);
  vector2(const vector4<T>&);

  bool operator==(const vector2& u) const { return (u.x == x && u.y == y) ? true : false; }

  bool operator!=(const vector2& u) const { return !(*this == u); }

  vector2& operator*=(const T& lambda)
  {
    x *= lambda;
    y *= lambda;
    return *this;
  }
  // TL
  vector2& operator/=(const T& lambda)
  {
    x /= lambda;
    y /= lambda;
    return *this;
  }

  vector2& operator-=(const vector2& u)
  {
    x -= u.x;
    y -= u.y;
    return *this;
  }

  vector2& operator+=(const vector2& u)
  {
    x += u.x;
    y += u.y;
    return *this;
  }

  T& operator[](int i) { return vec_array[i]; }

  const T& operator[](int i) const { return vec_array[i]; }

  T sq_norm() const { return x * x + y * y; }
  T norm() const { return sqrtf(sq_norm()); }

  union
  {
    struct
    {
      T x, y;  // standard names for components
    };
    T vec_array[2];  // array access
  };

  T*       get_value() { return vec_array; }
  const T* get_value() const { return vec_array; }
};

template <class T>
struct vector3
{
  vector3()
      : x(T(0))
      , y(T(0))
      , z(T(0))
  {
  }
  vector3(T x)
      : x(T(x))
      , y(T(x))
      , z(T(x))
  {
  }
  template <typename T0, typename T1, typename T2>
  vector3(T0 x, T1 y, T2 z)
      : x(T(x))
      , y(T(y))
      , z(T(z))
  {
  }
  vector3(const T* xyz)
      : x(xyz[0])
      , y(xyz[1])
      , z(xyz[2])
  {
  }
  vector3(const vector2<T>& u)
      : x(u.x)
      , y(u.y)
      , z(1.0f)
  {
  }
  vector3(const vector2<T>& u, T v)
      : x(u.x)
      , y(u.y)
      , z(v)
  {
  }
  vector3(const vector3<T>& u)
      : x(u.x)
      , y(u.y)
      , z(u.z)
  {
  }
  vector3(const vector4<T>&);

  bool operator==(const vector3<T>& u) const { return (u.x == x && u.y == y && u.z == z) ? true : false; }

  bool operator!=(const vector3<T>& rhs) const { return !(*this == rhs); }

  // TL
  vector3<T>& operator*=(const matrix3<T>& M)
  {
    vector3<T> dst;
    mult(dst, M, *this);
    x = dst.x;
    y = dst.y;
    z = dst.z;
    return (*this);
  }
  // TL
  vector3<T>& operator*=(const matrix4<T>& M)
  {
    vector3<T> dst;
    mult(dst, M, *this);
    x = dst.x;
    y = dst.y;
    z = dst.z;
    return (*this);
  }
  // TL
  vector3<T>& operator/=(const vector3<T>& d)
  {
    x /= d.x;
    y /= d.y;
    z /= d.z;
    return *this;
  }
  vector3<T>& operator/=(const T& lambda)
  {
    x /= lambda;
    y /= lambda;
    z /= lambda;
    return *this;
  }

  vector3<T>& operator*=(const T& lambda)
  {
    x *= lambda;
    y *= lambda;
    z *= lambda;
    return *this;
  }
  // TL
  vector3<T>& operator*=(const vector3<T>& v)
  {
    x *= v.x;
    y *= v.y;
    z *= v.z;
    return *this;
  }

  vector3<T> operator-() const { return vector3<T>(-x, -y, -z); }

  vector3<T>& operator-=(const vector3<T>& u)
  {
    x -= u.x;
    y -= u.y;
    z -= u.z;
    return *this;
  }

  vector3<T>& operator+=(const vector3<T>& u)
  {
    x += u.x;
    y += u.y;
    z += u.z;
    return *this;
  }
  vector3<T>& rotateBy(const quaternion<T>& q);
  T           normalize();
  void        orthogonalize(const vector3<T>& v);
  void        orthonormalize(const vector3<T>& v)
  {
    orthogonalize(v);  //  just orthogonalize...
    normalize();       //  ...and normalize it
  }

  T sq_norm() const { return x * x + y * y + z * z; }
  T norm() const { return sqrtf(sq_norm()); }

  T& operator[](int i) { return vec_array[i]; }

  const T& operator[](int i) const { return vec_array[i]; }

  union
  {
    struct
    {
      T x, y, z;  // standard names for components
    };
    T vec_array[3];  // array access
  };

  T*       get_value() { return vec_array; }
  const T* get_value() const { return vec_array; }
};

template <class T>
inline vector2<T>::vector2(const vector3<T>& u)
{
  x = u.x;
  y = u.y;
}

template <class T>
struct vector4
{
  vector4()
      : x(T(0))
      , y(T(0))
      , z(T(0))
      , w(T(0))
  {
  }
  vector4(T x)
      : x(T(x))
      , y(T(x))
      , z(T(x))
      , w(T(x))
  {
  }
  template <typename T0, typename T1, typename T2, typename T3>
  vector4(T0 x, T1 y, T2 z, T3 w)
      : x(T(x))
      , y(T(y))
      , z(T(z))
      , w(T(w))
  {
  }
  vector4(const T* xyzw)
      : x(xyzw[0])
      , y(xyzw[1])
      , z(xyzw[2])
      , w(xyzw[3])
  {
  }
  vector4(const vector2<T>& u)
      : x(u.x)
      , y(u.y)
      , z(0.0f)
      , w(1.0f)
  {
  }
  vector4(const vector2<T>& u, const T zz)
      : x(u.x)
      , y(u.y)
      , z(zz)
      , w(1.0f)
  {
  }
  vector4(const vector2<T>& u, const T zz, const T ww)
      : x(u.x)
      , y(u.y)
      , z(zz)
      , w(ww)
  {
  }
  vector4(const vector3<T>& u)
      : x(u.x)
      , y(u.y)
      , z(u.z)
      , w(1.0f)
  {
  }
  vector4(const vector3<T>& u, const T w)
      : x(u.x)
      , y(u.y)
      , z(u.z)
      , w(w)
  {
  }
  vector4(const vector4<T>& u)
      : x(u.x)
      , y(u.y)
      , z(u.z)
      , w(u.w)
  {
  }

  bool operator==(const vector4<T>& u) const { return (u.x == x && u.y == y && u.z == z && u.w == w) ? true : false; }

  bool operator!=(const vector4<T>& rhs) const { return !(*this == rhs); }

  T sq_norm() const { return x * x + y * y + z * z + w * w; }
  T norm() const { return sqrtf(sq_norm()); }

  // TL
  vector4<T>& operator*=(const matrix3<T>& M)
  {
    vector4<T> dst;
    mult(dst, M, *this);
    x = dst.x;
    y = dst.y;
    z = dst.z;
    w = dst.z;
    return (*this);
  }
  // TL
  vector4<T>& operator*=(const matrix4<T>& M)
  {
    vector4<T> dst;
    mult(dst, M, *this);
    x = dst.x;
    y = dst.y;
    z = dst.z;
    w = dst.w;
    return (*this);
  }
  // TL
  vector4<T>& operator/=(const T& lambda)
  {
    x /= lambda;
    y /= lambda;
    z /= lambda;
    w /= lambda;
    return *this;
  }

  vector4<T>& operator*=(const T& lambda)
  {
    x *= lambda;
    y *= lambda;
    z *= lambda;
    w *= lambda;
    return *this;
  }

  vector4<T>& operator-=(const vector4<T>& u)
  {
    x -= u.x;
    y -= u.y;
    z -= u.z;
    w -= u.w;
    return *this;
  }

  vector4<T>& operator+=(const vector4<T>& u)
  {
    x += u.x;
    y += u.y;
    z += u.z;
    w += u.w;
    return *this;
  }

  vector4<T> operator-() const { return vector4<T>(-x, -y, -z, -w); }

  T& operator[](int i) { return vec_array[i]; }

  const T& operator[](int i) const { return vec_array[i]; }

  union
  {
    struct
    {
      T x, y, z, w;  // standard names for components
    };
    T vec_array[4];  // array access
  };

  T*       get_value() { return vec_array; }
  const T* get_value() const { return vec_array; }
};  //struct vector4

template <class T>
inline vector2<T>::vector2(const vector4<T>& u)
{
  x = u.x;
  y = u.y;
}

template <class T>
inline vector3<T>::vector3(const vector4<T>& u)
{
  x = u.x;
  y = u.y;
  z = u.z;
}

// quaternion
template <class T>
struct quaternion;

/*
    for all the matrices...a<x><y> indicates the element at row x, col y

    For example:
    a01 <-> row 0, col 1
*/

template <class T>
struct matrix3
{
  matrix3() {}
  matrix3(int one) { identity(); }
  matrix3(const T* array) { memcpy(mat_array, array, sizeof(T) * 9); }
  matrix3(const matrix3<T>& M) { memcpy(mat_array, M.mat_array, sizeof(T) * 9); }
  matrix3(const T& f0, const T& f1, const T& f2, const T& f3, const T& f4, const T& f5, const T& f6, const T& f7, const T& f8)  //$ max_line_size_100 failed
      : a00(f0)
      , a10(f1)
      , a20(f2)
      , a01(f3)
      , a11(f4)
      , a21(f5)
      , a02(f6)
      , a12(f7)
      , a22(f8)
  {
  }

  matrix3<T>& identity()
  {
    mat_array[0] = T(1);
    mat_array[1] = T(0);
    mat_array[2] = T(0);
    mat_array[3] = T(0);
    mat_array[4] = T(1);
    mat_array[5] = T(0);
    mat_array[6] = T(0);
    mat_array[7] = T(0);
    mat_array[8] = T(1);

    return *this;
  }

  const vector3<T> col(const int i) const { return vector3<T>(&mat_array[i * 3]); }

  const vector3<T> row(const int i) const { return vector3<T>(mat_array[i], mat_array[i + 3], mat_array[i + 6]); } //$ max_line_size_100 failed

  const vector3<T> operator[](int i) const { return vector3<T>(mat_array[i], mat_array[i + 3], mat_array[i + 6]); } //$ max_line_size_100 failed

  const T& operator()(const int& i, const int& j) const { return mat_array[j * 3 + i]; }

  T& operator()(const int& i, const int& j) { return mat_array[j * 3 + i]; }

  matrix3<T>& operator*=(const T& lambda)
  {
    for (int i = 0; i < 9; ++i)
      mat_array[i] *= lambda;
    return *this;
  }
  //TL
  matrix3<T>& operator*=(const matrix3<T>& M) { return *this; }

  matrix3<T>& operator-=(const matrix3<T>& M)
  {
    for (int i = 0; i < 9; ++i)
      mat_array[i] -= M.mat_array[i];
    return *this;
  }

  matrix3<T>& set_row(int i, const vector3<T>& v)
  {
    mat_array[i]     = v.x;
    mat_array[i + 3] = v.y;
    mat_array[i + 6] = v.z;
    return *this;
  }

  matrix3<T>& set_col(int i, const vector3<T>& v)
  {
    mat_array[i * 3]     = v.x;
    mat_array[i * 3 + 1] = v.y;
    mat_array[i * 3 + 2] = v.z;
    return *this;
  }

  matrix3<T>& set_rot(const T& theta, const vector3<T>& v);
  matrix3<T>& set_rot(const vector3<T>& u, const vector3<T>& v);

  // Matrix norms...
  // Compute || M ||
  //                1
  T norm_one();

  // Compute || M ||
  //                +inf
  T norm_inf();

  union
  {
    struct
    {
      T a00, a10, a20;  // standard names for components
      T a01, a11, a21;  // standard names for components
      T a02, a12, a22;  // standard names for components
    };
    T mat_array[9];  // array access
  };

  T*       get_value() { return mat_array; }
  const T* get_value() const { return mat_array; }
};  //struct matrix3

//
// Note : as_...() means that the whole matrix is being modified
// set_...() only changes the concerned fields of the matrix
//
// translate()/scale()/identity()/rotate() : act as OpenGL functions. Example :
// M.identity()
// M.translate(t)
// M.scale(s)
// drawMyVertex(vtx)
//
// is like : newVtx = Midentiry * Mt * Ms * vtx
//
template <class T>
struct matrix4
{
  matrix4() { memset(mat_array, 0, sizeof(T) * 16); }
  matrix4(int one) { identity(); }

  matrix4(const T* array) { memcpy(mat_array, array, sizeof(T) * 16); }
  matrix4(const matrix3<T>& M)
  {
    memcpy(mat_array, M.mat_array, sizeof(T) * 3);
    mat_array[3] = 0.0;
    memcpy(mat_array + 4, M.mat_array + 3, sizeof(T) * 3);
    mat_array[7] = 0.0;
    memcpy(mat_array + 8, M.mat_array + 6, sizeof(T) * 3);
    mat_array[11] = 0.0f;
    mat_array[12] = 0.0f;
    mat_array[13] = 0.0f;
    mat_array[14] = 0.0f;
    mat_array[15] = 1.0f;
  }
  matrix4(const matrix4<T>& M) { memcpy(mat_array, M.mat_array, sizeof(T) * 16); }

  matrix4(const T& f0,
          const T& f1,
          const T& f2,
          const T& f3,
          const T& f4,
          const T& f5,
          const T& f6,
          const T& f7,
          const T& f8,
          const T& f9,
          const T& f10,
          const T& f11,
          const T& f12,
          const T& f13,
          const T& f14,
          const T& f15)
      : a00(f0)
      , a10(f1)
      , a20(f2)
      , a30(f3)
      , a01(f4)
      , a11(f5)
      , a21(f6)
      , a31(f7)
      , a02(f8)
      , a12(f9)
      , a22(f10)
      , a32(f11)
      , a03(f12)
      , a13(f13)
      , a23(f14)
      , a33(f15)
  {
  }

  const vector4<T> col(const int i) const { return vector4<T>(&mat_array[i * 4]); }

  const vector4<T> row(const int i) const
  {
    return vector4<T>(mat_array[i], mat_array[i + 4], mat_array[i + 8], mat_array[i + 12]);
  }

  const vector4<T> operator[](const int& i) const
  {
    return vector4<T>(mat_array[i], mat_array[i + 4], mat_array[i + 8], mat_array[i + 12]);
  }

  const T& operator()(const int& i, const int& j) const { return mat_array[j * 4 + i]; }

  T& operator()(const int& i, const int& j) { return mat_array[j * 4 + i]; }

  matrix4<T>& set_col(int i, const vector4<T>& v)
  {
    mat_array[i * 4]     = v.x;
    mat_array[i * 4 + 1] = v.y;
    mat_array[i * 4 + 2] = v.z;
    mat_array[i * 4 + 3] = v.w;
    return *this;
  }

  matrix4<T>& set_row(int i, const vector4<T>& v)
  {
    mat_array[i]      = v.x;
    mat_array[i + 4]  = v.y;
    mat_array[i + 8]  = v.z;
    mat_array[i + 12] = v.w;
    return *this;
  }

  matrix3<T>    get_rot_mat3() const;
  quaternion<T> get_rot_quat() const;
  matrix4<T>&   set_rot(const quaternion<T>& q);
  matrix4<T>&   set_rot(const matrix3<T>& M);
  matrix4<T>&   set_rot(const T& theta, const vector3<T>& v);
  matrix4<T>&   set_rot(const vector3<T>& u, const vector3<T>& v);

  matrix4<T>& as_rot(const quaternion<T>& q)
  {
    a30 = a31 = a32 = 0.0;
    a33             = 1.0;
    a03 = a13 = a23 = 0.0;
    set_rot(q);
    return *this;
  }
  matrix4<T>& as_rot(const matrix3<T>& M)
  {
    a30 = a31 = a32 = 0.0;
    a33             = 1.0;
    a03 = a13 = a23 = 0.0;
    set_rot(M);
    return *this;
  }
  matrix4<T>& as_rot(const T& theta, const vector3<T>& v)
  {
    set_rot(theta, v);
    a30 = a31 = a32 = 0.0;
    a33             = 1.0;
    a03 = a13 = a23 = 0.0;
    return *this;
  }
  matrix4<T>& as_rot(const vector3<T>& u, const vector3<T>& v)
  {
    a30 = a31 = a32 = 0.0;
    a33             = 1.0;
    a03 = a13 = a23 = 0.0;
    set_rot(u, v);
    return *this;
  }

  matrix4<T>&        set_scale(const vector3<T>& s);
  vector3<T>&        get_scale(vector3<T>& s) const;
  matrix4<T>&        as_scale(const vector3<T>& s);
  matrix4<T>&        as_scale(const T& s);
  matrix4<T>&        set_translation(const vector3<T>& t);
  inline matrix4<T>& set_translate(const vector3<T>& t) { return set_translation(t); }
  vector3<T>&        get_translation(vector3<T>& t) const;
  matrix4<T>&        as_translation(const vector3<T>& t);

  matrix4<T> operator*(const matrix4<T>&)const;
  //TL
  matrix4<T>& operator*=(const matrix4<T>& M)
  {
    *this = mult(*this, M);
    return *this;
  }

  //TL: some additional methods that look like OpenGL...
  // they behave the same as the OpenGL matrix system
  // But: using vector3<T> class; rotation is in Radians
  // TODO: optimize
  matrix4<T>& identity()
  {
    mat_array[0]  = T(1);
    mat_array[1]  = T(0);
    mat_array[2]  = T(0);
    mat_array[3]  = T(0);
    mat_array[4]  = T(0);
    mat_array[5]  = T(1);
    mat_array[6]  = T(0);
    mat_array[7]  = T(0);
    mat_array[8]  = T(0);
    mat_array[9]  = T(0);
    mat_array[10] = T(1);
    mat_array[11] = T(0);
    mat_array[12] = T(0);
    mat_array[13] = T(0);
    mat_array[14] = T(0);
    mat_array[15] = T(1);

    return *this;
  }
  matrix4<T>& translate(vector3<T> t)
  {
    *this *= matrix4<T>().as_translation(t);
    return *this;
  }
  matrix4<T>& translate(T* t)
  {
    *this *= matrix4<T>().as_translation(*(vector3<T>*)t);
    return *this;
  }
  matrix4<T>& scale(vector3<T> s)
  {
    *this *= matrix4<T>().as_scale(s);
    return *this;
  }
  matrix4<T>& scale(T s)
  {
    *this *= matrix4<T>().as_scale(s);
    return *this;
  }
  matrix4<T>& rotate(const T& theta, const vector3<T>& v)
  {
    *this *= matrix4<T>().as_rot(theta, v);
    return *this;
  }
  matrix4<T>& rotate(quaternion<T>& q)
  {
    *this *= matrix4<T>().identity().set_rot(q);
    return *this;
  }

  union
  {
    struct
    {
      T a00, a10, a20, a30;  // standard names for components
      T a01, a11, a21, a31;  // standard names for components
      T a02, a12, a22, a32;  // standard names for components
      T a03, a13, a23, a33;  // standard names for components
    };
    T mat_array[16];  // array access
  };

  T*       get_value() { return mat_array; }
  const T* get_value() const { return mat_array; }
};  //struct matrix4

// quaternion<T>ernion
template <class T>
struct quaternion
{
public:
  quaternion() {}
  quaternion(T* q)
  {
    x = q[0];
    y = q[1];
    z = q[2];
    w = q[3];
  }
  template <typename T0, typename T1, typename T2, typename T3>
  quaternion(T0 x, T1 y, T2 z, T3 w)
      : x(T(x))
      , y(T(y))
      , z(T(z))
      , w(T(w))
  {
  }
  quaternion(const quaternion<T>& quaternion)
  {
    x = quaternion.x;
    y = quaternion.y;
    z = quaternion.z;
    w = quaternion.w;
  }
  quaternion(const vector3<T>& axis, T angle);
  quaternion(const vector3<T>& eulerXYZ);  // From Euler
  quaternion(const matrix3<T>& rot);
  quaternion(const matrix4<T>& rot);
  quaternion<T>& operator=(const quaternion<T>& quaternion);
  quaternion<T>  operator-() { return quaternion<T>(-x, -y, -z, -w); }
  quaternion<T>  ilwerse();
  quaternion<T>  conjugate();
  void           normalize();
  void           from_matrix(const matrix3<T>& mat);
  void           from_matrix(const matrix4<T>& mat);
  void           to_matrix(matrix3<T>& mat) const;
  void           to_matrix(matrix4<T>& mat) const;
  void           to_euler_xyz(vector3<T>& r);
  void           to_euler_xyz(T* r);
  void           from_euler_xyz(vector3<T> r);
  quaternion<T>& operator*=(const quaternion<T>& q)
  {
    *this = *this * q;
    return *this;
  }

  T&       operator[](int i) { return comp[i]; }
  const T& operator[](int i) const { return comp[i]; }
  union
  {
    struct
    {
      T x, y, z, w;
    };
    T comp[4];
  };
};  //struct quaternion

typedef vector2<lw_scalar>    vec2f;
typedef vector3<lw_scalar>    vec3f;
typedef vector4<lw_scalar>    vec4f;
typedef matrix3<lw_scalar>    mat3f;
typedef matrix4<lw_scalar>    mat4f;
typedef quaternion<lw_scalar> quatf;

typedef vector2<int> vec2i;
typedef vector3<int> vec3i;
typedef vector4<int> vec4i;

typedef vector2<unsigned int> vec2ui;
typedef vector3<unsigned int> vec3ui;
typedef vector4<unsigned int> vec4ui;

typedef unsigned int uint;

template <typename T>
vector2<T> make_vec2(T const* const ptr);
template <typename T>
vector3<T> make_vec3(T const* const ptr);
template <typename T>
vector4<T> make_vec4(T const* const ptr);

}  // namespace lwmath

#endif
