#ifndef INCLUDE_GUARD_CASK_ELEMENT_H
#define INCLUDE_GUARD_CASK_ELEMENT_H

#include <cassert>
#include <cmath>     // for isnan(float)

#ifndef __LWDA_ARCH__
#include <cstdio>               // for the host side versions of the print() routines
#endif


namespace cask {

///////////////////////////////////////////////////////////////////////////////////////////////////

enum RoundingMode {
    ROUNDING_MODE_RN = 0,
    ROUNDING_MODE_RZ
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template< typename T >
class Element {
public:
    // The type of the native type.
    typedef T Type;

    // The size of the vector.
    static LWDA_DEVICE_HOST_INLINE int getVecSize() {
        return 1;
    }
    
    // Is it an FP16 type?
    static LWDA_DEVICE_HOST_INLINE bool isFp16() {
        return false;
    }

    // Is it an FP32 type?
    static LWDA_DEVICE_HOST_INLINE bool isFp32() {
        return false;
    }

    // Is it an INT8 type?
    static LWDA_DEVICE_HOST_INLINE bool isInt8() {
        return false;
    }

    // Is it an INT32 type?
    static LWDA_DEVICE_HOST_INLINE bool isInt32() {
        return false;
    }

private:
    // The data.
    T mData;

public:
    // Ctor.
    LWDA_DEVICE_HOST_INLINE Element() 
        : mData(0) {
    }

    // Ctor.
    LWDA_DEVICE_HOST_INLINE Element(T data) 
        : mData(data) {
    }

    // Ctor.
    template< typename Ta >
    LWDA_DEVICE_HOST_INLINE Element(const Element<Ta> &a) {
        this->colwert(a);
    }

    // Do an add such that we have this = a+b.
    template< typename Ta, typename Tb >
    LWDA_DEVICE_HOST_INLINE Element& add(Ta a, Tb b) {
        Element<T> one(Element<float>(1.f));
        return this->fma(one, a, b);
    }

    // Do an add such that we have this = this+a.
    template< typename Ta >
    LWDA_DEVICE_HOST_INLINE Element& add(Ta a) {
        return this->add(*this, a);
    }

    // Get the associated native type.
    LWDA_DEVICE_HOST_INLINE T& asNative() {
        return mData;
    }

    // Get the associated native type.
    LWDA_DEVICE_HOST_INLINE T asNative() const {
        return mData;
    }

    // Set the value from another value.
    template< typename Ta >
    LWDA_DEVICE_HOST_INLINE Element& colwert(const Element<Ta> &a, 
            RoundingMode mode = ROUNDING_MODE_RN) {
        mData = (T) a.asNative();
        return *this;
    }

    // Do we have equality?
    LWDA_DEVICE_HOST_INLINE bool equals(const Element<T> &other, float eps=0.f) const {
        if(eps == 0.f) {
            //bit-precise comparison
            return mData == other.mData;
        } else {
            T diff = mData - other.mData;
            T abs = (diff < 0.f) ? -diff : diff;
            return abs < eps;
        }
    }

    // Do a FMA where we have this = a*b + c.
    template< typename Ta, typename Tb >
    LWDA_DEVICE_HOST_INLINE Element& fma(Ta a, Tb b) {
        return this->fma(a, b, *this);
    }

    // Do a FMA where we have this = a*b + c.
    template< typename Ta, typename Tb, typename Tc >
    LWDA_DEVICE_HOST_INLINE Element& fma(Ta a, Tb b, Tc c) {
        Element x, y, z;
        x.colwert(a);
        y.colwert(b);
        z.colwert(c);
        mData = x.mData * y.mData + z.mData;
        return *this;
    }

    // Is the number a NaN?
    LWDA_DEVICE_HOST_INLINE bool isNaN() const {
        return false;
    }

    // Compute the max/min between two elements.
    template< typename Ta, typename Tb >
    LWDA_DEVICE_HOST_INLINE Element& max(Ta a, Tb b) {
        mData = a.asNative() >= b.asNative() ? a.asNative() : b.asNative();
        return *this;
    }
    
    template< typename Ta >
    LWDA_DEVICE_HOST_INLINE Element& max(Ta a) {
        return this->max(*this, a);
    }

	template< typename Ta, typename Tb >
	LWDA_DEVICE_HOST_INLINE Element& min(Ta a, Tb b) {
		mData = a.asNative() <= b.asNative() ? a.asNative() : b.asNative();
		return *this;
	}

	template< typename Ta >
	LWDA_DEVICE_HOST_INLINE Element& min(Ta a) {
		return this->min(*this, a);
	}

    // Do a mul such that we have this = a*b.
    template< typename Ta, typename Tb >
    LWDA_DEVICE_HOST_INLINE Element& mul(Ta a, Tb b) {
        Element<T> c;
        return this->fma(a, b, c);
    }

    // Do a subtraction such that we have this = a-b.
    template< typename Ta, typename Tb >
    LWDA_DEVICE_HOST_INLINE Element& sub(Ta a, Tb b) {
        Element<T> minusOne(Element<float>(-1.f));
        return this->fma(minusOne, b, a);
    }

    // Do a subtraction such that we have this = this-a.
    template< typename Ta >
    LWDA_DEVICE_HOST_INLINE Element& sub(Ta a) {
        return this->sub(*this, a);
    }

    // Compare with a native value.
    LWDA_DEVICE_HOST_INLINE bool operator==(const T &data) const {
        return mData == data;
    }

    // Compare with a native value.
    LWDA_DEVICE_HOST_INLINE bool operator==(const Element<T> &other) const {
        return this->operator==(other.mData);
    }

    // Print a value.
    LWDA_DEVICE_HOST_INLINE void print(const char *msg = "", void *file = NULL) const {
    }

    // Randomized numbers.
    inline void randomize(int range) {
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fp32

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
LWDA_DEVICE_HOST_INLINE bool Element<float>::isFp32() {
    return true;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float>& Element<float>::colwert(const Element<half> &a,
        RoundingMode mode) {
    mData = half2Float(a.asNative());
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float>& Element<float>::colwert(const Element<half2> &a,
        RoundingMode mode) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float>& Element<float>::colwert(const Element<char4> &a,
        RoundingMode mode) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<float>::equals(const Element &other, float eps) const {
    if( ::isnan(mData) || ::isnan(other.mData) ) {
        return false;
    }
    float den = fabsf(mData) + fabsf(other.mData); 
    float err = den <= eps ? fabsf(mData-other.mData) : fabsf(mData-other.mData) / den;
    return err <= eps;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<float>& Element<float>::fma(Element<half> a, Element<half> b, Element<float> c) {
    Element<float> x, y;
    x.colwert(a);
    y.colwert(b);
    return this->fma(x, y, c);
}

// ------------------------------------------------------------------------------------------------

// template<>
// template<>
// LWDA_DEVICE_HOST_INLINE 
// Element<float>& Element<float>::fma(Element<float> a, Element<int> b, Element<int> c) {
//     mData = a.mData * (float) b.asNative() + (float) c.asNative();
//     return *this;
// }
// 
// // ------------------------------------------------------------------------------------------------
// 
// template<>
// template<>
// LWDA_DEVICE_HOST_INLINE 
// Element<float>& Element<float>::fma(Element<float> a, Element<float> b, Element<int> c) {
//     mData = a.mData * b.mData + (float) c.asNative();
//     return *this;
// }
// 
// // ------------------------------------------------------------------------------------------------
// 
// template<>
// template<>
// LWDA_DEVICE_HOST_INLINE 
// Element<float>& Element<float>::fma(Element<float> a, Element<half> b, Element<float> c) {
//     assert(false);
//     return *this;
// }
// 
// // ------------------------------------------------------------------------------------------------
// 
// template<>
// template<>
// LWDA_DEVICE_HOST_INLINE 
// Element<float>& Element<float>::fma(Element<float> a, Element<char4> b, Element<float> c) {
//     assert(false);
//     return *this;
// }
// 
// // ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<float>::isNaN() const {
    return ::isnan(mData);
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<float>::print(const char *msg, void *file) const {
#ifdef __LWDA_ARCH__
    printf(
#else
    fprintf(file ? (FILE*) file : stdout,
#endif
            "%s: %8.3f (0x%08x)", msg, mData, bitwise_cast<uint32_t>(mData));
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<float>::randomize(int range) {
    float x = 0.f;
    if( range < 0 ) {
        x = 1.f;
    } else if( range == 0 ) {
        x = (float) (2.0*((double) rand() / (double) RAND_MAX) - 1.0);
    } else {
        x = (float) ((rand() % (2*range)) - range);
    }
    mData = x;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fp32x2

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
LWDA_DEVICE_HOST_INLINE int Element<float2>::getVecSize() { 
    return 2;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<float2>::isFp32() {
    return true;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE Element<float2>::Element() {
    mData = make_float2(0.f, 0.f);
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float2>::Element(const Element<half2> &a) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 530
    mData = __half22float2(a.asNative());
#else
#endif
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float2>& Element<float2>::colwert(const Element<float> &a,
        RoundingMode mode) {
    mData = make_float2(a.asNative(), a.asNative());
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float2>& Element<float2>::colwert(const Element<half2> &a,
        RoundingMode mode) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 530
    mData = __half22float2(a.asNative());
#else
#endif
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float2>& Element<float2>::max(Element<float2> a, 
        Element<float> b) {
    mData.x = fmaxf(a.mData.x, b.asNative());
    mData.y = fmaxf(a.mData.y, b.asNative());
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<float2>& Element<float2>::min(Element<float2> a,
	Element<float> b) {
	mData.x = fminf(a.mData.x, b.asNative());
	mData.y = fminf(a.mData.y, b.asNative());
	return *this;
}


// ------------------------------------------------------------------------------------------------
// 
// template<>
// LWDA_DEVICE_HOST_INLINE bool Element<half2>::equals(const Element &other, float eps) const {
//     float ax = half2Float((mData.x            ) & 0xffff);
//     float ay = half2Float((mData.x       >> 16) & 0xffff);
//     float bx = half2Float((other.mData.x      ) & 0xffff);
//     float by = half2Float((other.mData.x >> 16) & 0xffff);
// 
//     Element<float> a(ax), b(bx), c(ay), d(by);
//     return a.equals(b, fmaxf(eps, 1.e-4f)) && c.equals(d, fmaxf(eps, 1.e-4f));
// }
// 
// // ------------------------------------------------------------------------------------------------
// 
// template<>
// LWDA_DEVICE_HOST_INLINE bool Element<half2>::isNaN() const {
// #if defined __LWDA_ARCH__ && __LWDA_ARCH__ >= 530
//     half2 r = __hisnan2(mData);
//     return reinterpret_cast<int&>(r.x) != 0;
// #else
//     // Is either half a NaN? A NaN has 11111 for the exponent and a non-zero value for the
//     // mantissa. The mantissa has 10 bits and the exponent has 5 bits.
//     return 
//         (mData.x & 0x00007c00) == 0x00007c00 && (mData.x & 0x000003ff) ||
//         (mData.x & 0x7c000000) == 0x7c000000 && (mData.x & 0x03ff0000);
// #endif
// }
// 
// // ------------------------------------------------------------------------------------------------
// 
// template<>
// template<>
// LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::max(Element<half2> a, Element<float> b) {
// 
//     // The decomposition of the half numbers.
//     __half ax, ay;
// 
//     // Extract x and y from half2s.
//     ax.x = (a.asNative().x      ) & 0xffff;
//     ay.x = (a.asNative().x >> 16) & 0xffff;
// 
//     // Colwert to float.
//     float x = half2Float(ax);
//     float y = half2Float(ay);
// 
//     // Select the max for each components.
//     ax = x >= b.asNative() ? ax : float2Half(b.asNative());
//     ay = y >= b.asNative() ? ay : float2Half(b.asNative());
// 
//     mData = make_half2(ax, ay);
//     return *this;
// }
//     
// // ------------------------------------------------------------------------------------------------
// 
// template<>
// LWDA_DEVICE_HOST_INLINE bool Element<half2>::operator==(const half2 &data) const {
//     return mData.x == data.x;
// }

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<float2>& Element<float2>::fma(Element<float2> a, Element<float2> b, Element<float2> c) {
    mData = make_float2(a.mData.x*b.mData.x + c.mData.x, a.mData.y*b.mData.y + c.mData.y);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<float2>& Element<float2>::fma(Element<float> a, Element<float2> b, Element<float2> c) {
    mData = make_float2(a.asNative()*b.mData.x + c.mData.x, a.asNative()*b.mData.y + c.mData.y);
    return *this;
}

// ------------------------------------------------------------------------------------------------

// template<>
// template<>
// LWDA_DEVICE_HOST_INLINE 
// Element<half2>& Element<half2>::fma(Element<half2> a, Element<half> b, Element<half2> c) {
// #if defined __LWDA_ARCH__ && __LWDA_ARCH__ >= 530
//     mData = __hfma2(a.mData, __half2half2(b.asNative()), c.mData);
// #else
//     float x, y, z;
// 
//     x = half2Float(a.mData.x & 0xffff);
//     y = half2Float(b.asNative().x & 0xffff);
//     z = half2Float(c.mData.x & 0xffff);
// 
//     float lo = x*y + z;
// 
//     x = half2Float((a.mData.x >> 16) & 0xffff);
//     z = half2Float((c.mData.x >> 16) & 0xffff);
// 
//     float hi = x*y + z;
// 
//     mData = make_half2(lo, hi);
// #endif
//     return *this;
// }

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<float2>::print(const char *msg, void *file) const {
#ifdef __LWDA_ARCH__
    printf(
#else
    fprintf(file ? (FILE*) file : stdout,
#endif
            "%s: (%8.3f, %8.3f) [0x%08x, 0x%08x]", msg, mData.x, mData.y, bitwise_cast<uint32_t>(mData.x), 
            bitwise_cast<uint32_t>(mData.y));
}

// ------------------------------------------------------------------------------------------------

template<>
inline void Element<float2>::randomize(int range) {
    Element<float> x, y;
    x.randomize(range);
    y.randomize(range);
    mData = make_float2(x.asNative(), y.asNative());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fp16

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half>::isFp16() {
    return true;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE Element<half>::Element() {
    mData = float2Half(0.f);
}

// ------------------------------------------------------------------------------------------------

template<>
template< typename Ta >
LWDA_DEVICE_HOST_INLINE Element<half>& Element<half>::colwert(const Element<Ta> &a,
        RoundingMode mode) {
    Element<float> f(a);
    if( mode == ROUNDING_MODE_RN ) {
        mData = float2Half(f.asNative());
    } else if( mode == ROUNDING_MODE_RZ ) {
        float x = f.asNative();
        assert(false);
        // FIXME: compiler is saying this is an impossible constraint:
        // asm volatile("cvt.rz.f16.f32 %0, %1;" : "=h"(mData.x) : "f"(x));
    }
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half>::equals(const Element &other, float eps) const {
    Element<float> a(half2Float(mData));
    Element<float> b(half2Float(other.mData));
    return a.equals(b, fmaxf(eps, 1.e-4f));
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<half>& Element<half>::fma(Element<half> a, Element<half> b, Element<half> c) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 530
    mData = __hfma(a.mData, b.mData, c.mData);
#else
    // float x = cpuHalf2Float(a.mData);
    // float y = cpuHalf2Float(b.mData);
    // float z = cpuHalf2Float(c.mData);
    // z += x*y;
    // mData = cpuFloat2Half(z);
#endif
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<half>& Element<half>::fma(Element<float> a, Element<float> b, Element<half> c) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<half>& Element<half>::fma(Element<half> a, Element<float> b, Element<float> c) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half>::isNaN() const {
    return isnan(mData);
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half>& Element<half>::max(Element<half> a, Element<float> b) {
    Element<float> f(a);
    mData = f.asNative() >= b.asNative() ? a.asNative() : float2Half(b.asNative());
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half>& Element<half>::min(Element<half> a, Element<float> b) {
	Element<float> f(a);
	mData = f.asNative() <= b.asNative() ? a.asNative() : float2Half(b.asNative());
	return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half>::operator==(const half &data) const {
    return cask::operator==(mData, data);
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<half>::print(const char *msg, void *file) const {
    float x = half2Float(mData);
#ifdef __LWDA_ARCH__
    printf(
#else
    fprintf(file ? (FILE*) file : stdout,
#endif
            "%s: %8.3f [0x%04x]", msg, x, static_cast<uint32_t>(bitwise_cast<uint16_t>(mData)));
}

// ------------------------------------------------------------------------------------------------

template<>
inline void Element<half>::randomize(int range) {
    Element<float> x;
    x.randomize(range);
    mData = float2Half(x.asNative());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Int32

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
LWDA_DEVICE_HOST_INLINE bool Element<int>::isInt32() {
    return true;
}

// ------------------------------------------------------------------------------------------------

// Needed to make Winograd compile! Not valid. TODO: Remove when we have a clean fix!!!
template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<int>& Element<int>::colwert(const Element<char4> &x,
        RoundingMode mode) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<int>& Element<int>::fma(Element<char4> a, Element<char4> b, Element<int> c) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ == 610
    int ai = reinterpret_cast<int&>(a.asNative());
    int bi = reinterpret_cast<int&>(b.asNative());
    int ci = reinterpret_cast<int&>(c.asNative());
    asm volatile ("dp4a.s32.s32 %0, %1, %2, %3;" : "=r"(mData) : "r"(ai), "r"(bi), "r"(ci));
#else
    mData  = (int) a.asNative().x * (int) b.asNative().x + c.mData;
    mData += (int) a.asNative().y * (int) b.asNative().y;
    mData += (int) a.asNative().z * (int) b.asNative().z;
    mData += (int) a.asNative().w * (int) b.asNative().w;
#endif
    return *this;
}

// ------------------------------------------------------------------------------------------------

// Needed to make Winograd compile! Not valid. TODO: Remove when we have a clean fix!!!
template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<int>& Element<int>::fma(Element<int> a, Element<char4> b, Element<int> c) {
    assert(false);
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<int>& Element<int>::fma(Element<int> a, Element<char4> b, Element<char4> c) {
    assert(false);
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<int>& Element<int>::fma(Element<int> a, Element<int> b, Element<char4> c) {
    assert(false);
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<int>& Element<int>::fma(Element<char4> a, Element<int> b, Element<int> c) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<int>::print(const char *msg, void *file) const {
#ifdef __LWDA_ARCH__
    printf(
#else
    fprintf(file ? (FILE*) file : stdout,
#endif
    "%s: %d (0x%08x)", msg, mData, mData);
}

// ------------------------------------------------------------------------------------------------

template<>
inline void Element<int>::randomize(int range) {
    mData = (rand() % (2*range)) - range;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fp16x2

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
LWDA_DEVICE_HOST_INLINE int Element<half2>::getVecSize() { 
    return 2;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half2>::isFp16() {
    return true;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE Element<half2>::Element() {
    mData = make_half2(0.f, 0.f);
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half2>::Element(const Element<float2> &a) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 530
    mData = __float22half2_rn(a.asNative());
#else
#endif
}

// ------------------------------------------------------------------------------------------------

template<>
template< typename Ta >
LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::colwert(const Element<Ta> &a,
        RoundingMode mode) {
    Element<float> f(a);
    mData = make_half2(f.asNative(), f.asNative());
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::colwert(const Element<half2> &a,
        RoundingMode mode) {
    mData = a.mData;
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::colwert(const Element<half> &a,
        RoundingMode mode) {
    mData = make_half2(a.asNative(), a.asNative());
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::colwert(const Element<float2> &a,
        RoundingMode mode) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 530
    if( mode == ROUNDING_MODE_RN ) {
        mData = __float22half2_rn(a.asNative());
    }
#else
#endif
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half2>::equals(const Element &other, float eps) const {
    float ax = half2Float(pairX<half>(mData));
    float ay = half2Float(pairY<half>(mData));
    float bx = half2Float(pairX<half>(other.mData));
    float by = half2Float(pairY<half>(other.mData));

    Element<float> a(ax), b(bx), c(ay), d(by);
    return a.equals(b, fmaxf(eps, 1.e-4f)) && c.equals(d, fmaxf(eps, 1.e-4f));
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<half2>::isNaN() const {
    return cask::isnan(mData);
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::max(Element<half2> a, Element<float> b) {

    // The decomposition of the half numbers.
    half ax = pairX<half>(a.asNative());
    half ay = pairY<half>(a.asNative());

    // Colwert to float.
    float x = half2Float(ax);
    float y = half2Float(ay);

    // Select the max for each components.
    ax = x >= b.asNative() ? ax : float2Half(b.asNative());
    ay = y >= b.asNative() ? ay : float2Half(b.asNative());

    mData = make_half2(ax, ay);
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<half2>& Element<half2>::min(Element<half2> a, Element<float> b) {

	// The decomposition of the half numbers.
	half ax = pairX<half>(a.asNative());
	half ay = pairY<half>(a.asNative());

	// Colwert to float.
	float x = half2Float(ax);
	float y = half2Float(ay);

	// Select the max for each components.
	ax = x <= b.asNative() ? ax : float2Half(b.asNative());
	ay = y <= b.asNative() ? ay : float2Half(b.asNative());

	mData = make_half2(ax, ay);
	return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<half2>& Element<half2>::fma(Element<half2> a, Element<half2> b, Element<half2> c) {
#if defined __LWDA_ARCH__ && __LWDA_ARCH__ >= 530
    mData = __hfma2(a.mData, b.mData, c.mData);
#else
    float x, y, z;

    x = half2Float(pairX<half>(a.mData));
    y = half2Float(pairX<half>(b.mData));
    z = half2Float(pairX<half>(c.mData));

    float lo = x*y + z;

    x = half2Float(pairY<half>(a.mData));
    y = half2Float(pairY<half>(b.mData));
    z = half2Float(pairY<half>(c.mData));

    float hi = x*y + z;

    mData = make_half2(lo, hi);
#endif
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template<>
LWDA_DEVICE_HOST_INLINE 
Element<half2>& Element<half2>::fma(Element<half2> a, Element<half> b, Element<half2> c) {
#if defined __LWDA_ARCH__ && __LWDA_ARCH__ >= 530
    mData = __hfma2(a.mData, __half2half2(b.asNative()), c.mData);
#else
    float x, y, z;

    x = half2Float(pairX<half>(a.mData));
    y = half2Float(b.asNative());
    z = half2Float(pairX<half>(c.mData));

    float lo = x*y + z;

    x = half2Float(pairY<half>(a.mData));
    z = half2Float(pairY<half>(c.mData));

    float hi = x*y + z;

    mData = make_half2(lo, hi);
#endif
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<half2>::print(const char *msg, void *file) const {
    float x = half2Float(pairX<half>(mData));
    float y = half2Float(pairY<half>(mData));
#ifdef __LWDA_ARCH__
    printf(
#else
    fprintf(file ? (FILE*) file : stdout,
#endif
            "%s: (%8.3f, %8.3f) [0x%08x]", msg, x, y, bitwise_cast<uint32_t>(mData));
}

// ------------------------------------------------------------------------------------------------

template<>
inline void Element<half2>::randomize(int range) {
    Element<float> x, y;
    x.randomize(range);
    y.randomize(range);
    mData = make_half2(x.asNative(), y.asNative());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Int8x4

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
LWDA_DEVICE_HOST_INLINE int Element<char4>::getVecSize() { 
    return 4;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<char4>::isInt8() {
    return true;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE Element<char4>::Element() {
    mData = make_char4(0, 0, 0, 0);
}

// ------------------------------------------------------------------------------------------------

// Needed to make Winograd compile! Not valid. TODO: Remove when we have a clean fix!!!
template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<char4>& Element<char4>::colwert(const Element<int> &x,
        RoundingMode mode) {
    assert(false);
    return *this;
}

template<>
template<>
LWDA_DEVICE_HOST_INLINE Element<char4>& Element<char4>::colwert(const Element<float> &x,
        RoundingMode mode) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
template< typename Ta, typename Tb, typename Tc >
LWDA_DEVICE_HOST_INLINE Element<char4>& Element<char4>::fma(Ta a, Tb b, Tc c) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<char4>::equals(const Element &other, float eps) const {
    return reinterpret_cast<const int&>(mData) == reinterpret_cast<const int&>(other.mData);
}

// ------------------------------------------------------------------------------------------------

template<>
template< typename Ta, typename Tb >
LWDA_DEVICE_HOST_INLINE Element<char4>& Element<char4>::max(Ta a, Tb b) {
    assert(false);
    return *this;
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE bool Element<char4>::operator==(const char4 &data) const {
    return reinterpret_cast<const int&>(mData) == reinterpret_cast<const int&>(data);
}

// ------------------------------------------------------------------------------------------------

template<>
LWDA_DEVICE_HOST_INLINE void Element<char4>::print(const char *msg, void *file) const {
#ifdef __LWDA_ARCH__
    printf(
#else
    fprintf(file ? (FILE*) file : stdout,
#endif
            "%s: (%4d, %4d, %4d, %4d) [0x%08x]", msg, mData.x, mData.y, mData.z, mData.w, bitwise_cast<uint32_t>(mData));
}

// ------------------------------------------------------------------------------------------------

template<>
inline void Element<char4>::randomize(int range) {
    Element<int> x, y, z, w;
    x.randomize(range);
    y.randomize(range);
    z.randomize(range);
    w.randomize(range);
    mData = make_char4(
        (signed char) x.asNative(), 
        (signed char) y.asNative(),
        (signed char) z.asNative(), 
        (signed char) w.asNative());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef Element<half>   Fp16;
typedef Element<float>  Fp32;
typedef Element<float2> Fp32x2;
typedef Element<int>    Int32;
typedef Element<half2>  Fp16x2;
typedef Element<char4>  Int8x4;

///////////////////////////////////////////////////////////////////////////////////////////////////

static LWDA_DEVICE_HOST_INLINE Fp16 horizAdd(const Fp16x2 &a) {
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 530 
    Fp16 res;
    res.asNative() = __hadd(__low2half(a.asNative()), __high2half(a.asNative()));
#else
    float x = half2Float(pairX<half>(a.asNative()));
    float y = half2Float(pairY<half>(a.asNative()));
    Fp16 res = float2Half(x + y);
#endif
    return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template< typename T >
LWDA_DEVICE_HOST_INLINE bool operator==(const T &a, const Element<T> &b) {
    return b.operator ==(a);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template< typename T >
LWDA_DEVICE_HOST_INLINE bool operator!=(const Element<T> &a, const Element<T> &b) {
    return !(a == b);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

} // end namespace cask

#endif // INCLUDE_GUARD_CASK_ELEMENT_H
