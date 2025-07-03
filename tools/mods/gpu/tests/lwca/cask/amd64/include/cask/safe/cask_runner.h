// enable doxygen:
//! @file
#ifndef SAFE_INCLUDE_GUARD_CASK_RUNNER_SAFE_H
#define SAFE_INCLUDE_GUARD_CASK_RUNNER_SAFE_H

#include <cstdint>
#include <limits>
#include <string>
#include <map>
#include <lwca_runtime.h>

#include "cask/safe/platform.h"
#include "cask/safe/run_info.h"

namespace cask_safe {

using platformStream_t = lwcaStream_t;
using platformError_t = lwcaError_t;

// MAX KERNELS WITHIN A runner_utils
const uint32_t RUNNER_MAX_KERNELS = 16;

// INVALIDATE PARAMETERS OFFSET
const uint32_t INVALIDATE_PARAM_OFFSET = 0xffffffff;

//////////////////////////////////////////////////////////////////
/// \brief Base class for the runner. The class need to be replace after KernelRunner ready.
///
//////////////////////////////////////////////////////////////////
// class Runner {
// public:
//     Runner() = default;
// protected:
//     ~Runner() = default;
//     Runner(Runner const&) = default;
//     Runner(Runner&&) = default;
//     Runner& operator=(Runner const&) & = default;
//     Runner& operator=(Runner&&) & = default;
// };

class KernelRunner {
  public:
    virtual void finalizeLaunchParams( RunInfo &ri,
                                       void *,       /* params */
                                       void *,       /* hostData */
                                       void *,       /* deviceOutputTensor */
                                       const void *, /* deviceATensor */
                                       const void *, /* deviceBTensor */
                                       const void *, /* deviceCTensor */
                                       const void *, /* deviceAmTensor */
                                       const void *, /* deviceBiasTensor */
                                       const void *, /* deviceAlphaVector */
                                       const void *  /* deviceBetaVector */
                                       ) const noexcept = 0;

    virtual Error funcGetAttribute( FunctionAttribute attr, int *p_value ) const noexcept = 0;
    virtual Error funcSetAttribute( FunctionAttribute attr, int value ) const noexcept = 0;
    KernelRunner() = default;
    virtual ~KernelRunner() = default;
  protected:
    KernelRunner( KernelRunner const & ) = default;
    KernelRunner( KernelRunner && ) = default;
    KernelRunner &operator=( KernelRunner const & ) & = default;
    KernelRunner &operator=( KernelRunner && ) & = default;
};
class KernelLauncher {
public:
    virtual Error run( const void* params,
                      const void* host_data,
                      void *device_data,
                      platformStream_t lwcaStream) const noexcept = 0;

    virtual Error funcGetAttribute( FunctionAttribute attr, int *p_value ) const noexcept = 0;

    virtual Error funcSetAttribute( FunctionAttribute attr, int value ) const noexcept = 0;

    KernelLauncher() = default;
    virtual ~KernelLauncher() = default;

protected:
    KernelLauncher( KernelLauncher const& ) = default;
    KernelLauncher( KernelLauncher&& ) = default;
    KernelLauncher& operator=( KernelLauncher const& ) & = default;
    KernelLauncher& operator=( KernelLauncher && ) & = default;

};

} // namespace cask_safe
#endif // INCLUDE_GUARD_CASK_RUNNER_SAFE_H
