#ifndef SAFE_INCLUDE_PLATFORM_H
#define SAFE_INCLUDE_PLATFORM_H

#include <cstdint>
#include <limits>
#include <lwca_runtime.h>
#include <lwca.h>

/// noexcept, constexpr
#if (!defined(_MSC_VER) && (__cplusplus < 201103L)) || (defined(_MSC_VER) && (_MSC_VER < 1900))
#ifndef noexcept
#define noexcept
#endif
#ifndef constexpr
#define constexpr
#endif
#endif

/// nullptr
#if (!defined(_MSC_VER) && (__cplusplus < 201103L)) || (defined(_MSC_VER) && (_MSC_VER < 1310))
#ifndef nullptr
#define nullptr 0
#endif
#endif

/// static_assert
#if (!defined(_MSC_VER) && (__cplusplus < 201103L)) || (defined(_MSC_VER) && (_MSC_VER < 1600))
#ifndef static_assert
#define __platform_cat_(a, b) a##b
#define __platform_cat(a, b) __platform_cat_(a, b)
#define static_assert(__e, __m) typedef int __platform_cat(AsSeRt, __LINE__)[(__e) ? 1 : -1]
#endif
#endif

/// base data type
#if defined(_MSC_VER)
#ifndef uint64_t
using uint64_t = unsigned long long ;
#endif
#ifndef int64_t
using int64_t  = signed long long ;
#endif
#ifndef uint32_t
using uint32_t = unsigned int ;
#endif
#ifndef int32_t
using int32_t  = signed int ;
#endif
#endif

namespace cask_safe {

////////////////////////////////////////////////////////////////////
/// \ Vectorize the enums.
////////////////////////////////////////////////////////////////////
using enum_type      = uint32_t;
using enum_elem_type = uint8_t;
static constexpr uint32_t enum_width      = static_cast<uint32_t>(std::numeric_limits<enum_type>::digits);
static constexpr uint32_t enum_elem_width = static_cast<uint32_t>(std::numeric_limits<enum_elem_type>::digits); // enum_width divided by 4
static constexpr enum_type enum_elem_mask = ((static_cast<enum_type>(1U)) << (enum_elem_width)) - 1U;

inline
constexpr enum_type caskMakeECC(enum_elem_type value) noexcept {
    return
        (static_cast<enum_type>(value) & enum_elem_mask) |
        ((static_cast<enum_type>(value) & enum_elem_mask) << (enum_elem_width * 1U)) |
        ((static_cast<enum_type>(value) & enum_elem_mask) << (enum_elem_width * 2U)) |
        ((static_cast<enum_type>(value) & enum_elem_mask) << (enum_elem_width * 3U));
}


// //////////////////////////////////////////////////////////////////////////////
// /// \enum FunctionAttribute
// /// \brief This enum maps to lwcaFuncAttributes
// //////////////////////////////////////////////////////////////////////////////
enum class FunctionAttribute : uint32_t
{
    BINARY_VERSION,
    CACHE_MODE_CA,
    CONST_SIZE_BYTES,
    LOCAL_SIZE_BYTES,
    MAX_DYNAMIC_SHARED_SIZE_BYTES,
    MAX_THREADS_PER_BLOCK,
    NUM_REGS,
    PREFERRED_SHARED_MEMORY_CARVEOUT,
    PTX_VERSION,
    SHARED_SIZE_BYTES,
};

static inline lwcaError_t getCudaFuncAttribute(FunctionAttribute attr,
                                               int *p_value, const void *func) {
    lwcaError_t err;
    if (nullptr == p_value) {
        err = lwcaError_t::lwcaErrorInvalidValue;
    } else {
        lwcaFuncAttributes attributes;
        err = lwcaFuncGetAttributes(&attributes, func);
        if (err == lwcaSuccess) {
            switch (attr) {
                case FunctionAttribute::BINARY_VERSION:
                    *p_value = attributes.binaryVersion;
                    break;
                case FunctionAttribute::CACHE_MODE_CA:
                    *p_value = attributes.cacheModeCA;
                    break;
                case FunctionAttribute::CONST_SIZE_BYTES:
                    *p_value = static_cast<int>(attributes.constSizeBytes);
                    break;
                case FunctionAttribute::LOCAL_SIZE_BYTES:
                    *p_value = static_cast<int>(attributes.localSizeBytes);
                    break;
                case FunctionAttribute::MAX_DYNAMIC_SHARED_SIZE_BYTES:
                    *p_value = attributes.maxDynamicSharedSizeBytes;
                    break;
                case FunctionAttribute::MAX_THREADS_PER_BLOCK:
                    *p_value = attributes.maxThreadsPerBlock;
                    break;
                case FunctionAttribute::NUM_REGS:
                    *p_value = attributes.numRegs;
                    break;
                case FunctionAttribute::PREFERRED_SHARED_MEMORY_CARVEOUT:
                    *p_value = attributes.preferredShmemCarveout;
                    break;
                case FunctionAttribute::PTX_VERSION:
                    *p_value = attributes.ptxVersion;
                    break;
                case FunctionAttribute::SHARED_SIZE_BYTES:
                    *p_value = static_cast<int>(attributes.sharedSizeBytes);
                    break;
                default:
                    err = lwcaError_t::lwcaErrorInvalidValue;
                    break;
            }
        }
    }
    return err;
}

static inline lwcaError_t setCudaFuncAttribute(FunctionAttribute attr,
                                               int value, const void *func) {
    lwcaError_t err;
    switch (attr) {
        case FunctionAttribute::MAX_DYNAMIC_SHARED_SIZE_BYTES:
            err = lwcaFuncSetAttribute(func, lwcaFuncAttributeMaxDynamicSharedMemorySize, value);
            break;
        case FunctionAttribute::PREFERRED_SHARED_MEMORY_CARVEOUT:
            err = lwcaFuncSetAttribute(func, lwcaFuncAttributePreferredSharedMemoryCarveout, value);
            break;
        default:
            err = lwcaError_t::lwcaErrorInvalidValue;
            break;
    }
    return err;
}

} // namespace cask_safe

#endif  // SAFE_INCLUDE_PLATFORM_H
