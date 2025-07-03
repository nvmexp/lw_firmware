#ifndef INCLUDE_GUARD_CASK_UTILS_H
#define INCLUDE_GUARD_CASK_UTILS_H

#include <cask/cask.h>

// We need this to ensure we pick up any namespace altering
// hacks
#if !defined(INCLUDE_GUARD_CASK_H)
# error "cask.h must be included ahead of cask utils"
#endif

namespace cask {

#if defined(__LWCC__)
#define LWDA_DEVICE_HOST_INLINE inline __device__ __host__
#else
#define LWDA_DEVICE_HOST_INLINE inline
#endif

TensorDesc::SparsityHint::sparseRatio_t
calcSparsityRatio(const TensorDesc& td, const void* src);
Error genDevPtrArray(void* devArr, void* base, size_t stride, size_t count, platformStream_t stream);

/// When working under std CASK, builderToRunnerRunInfo is used to
/// transfer necessary RunInfo of builder to runner, as runner's RunInfo
/// is supposed to be a subset of information to builder's.
/// When working under safe CASK (runner only), we use serialize/deserialize
/// to do the implicit colwertion.
void builderToRunnerRunInfo(const ::cask::RunInfo& ri, ::cask_safe::RunInfo& sri);
::cask::Error ColwertSafeErrToCaskErr(::cask_safe::Error err);

}

#endif // INCLUDE_GUARD_CASK_UTILS_H
