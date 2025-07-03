//
// Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// enable doxygen:
//! @file

#ifndef SAFE_INCLUDE_GUARD_CASK_SOFTMAX_RUNNER_H
#define SAFE_INCLUDE_GUARD_CASK_SOFTMAX_RUNNER_H

#include "cask/safe/cask_runner.h"
#include <string>
#include <cstdint>

namespace cask_safe {

enum SoftmaxMode {
    /// Softmax over an entire image.
    SOFTMAX_MODE_INSTANCE = 0U,
    /// Per-channel softmax.
    SOFTMAX_MODE_CHANNEL  = 1U,

    SOFTMAX_MODE_UNKNOWN  = 2U,
};

enum SoftmaxAlgorithm {
    /// Straightforward implementation.
    SOFTMAX_FAST     = 0U,
    /// Subtract max from every point to avoid overflow
    SOFTMAX_ACCURATE = 1U,
    /// Apply a logarthim to the softmax result.
    SOFTMAX_LOG      = 2U,

    SOFTMAX_UNKNOWN  = 3U,
};


/////////////////////////////////////////////////////////////////////////
/// \brief The SoftmaxRunner is a cask runner that can perform a softmax operation.
///
/// \details The SoftmaxRunner is the base class for all runners that can
///          perform softmax. Each runner instance corresponds to a single
///          kernel implementation.
///
/////////////////////////////////////////////////////////////////////////
class SoftmaxRunner : public KernelRunner
{
  public:
    /////////////////////////////////////////////////////////////////////////
    /// \brief Constructor for the SoftmaxRunner class
    /// \details Runners are not intended to be created by users. They are created
    ///         internally in the cask library.
    /// \param name: Name of the softmax runner that is being constructed
    /////////////////////////////////////////////////////////////////////////
    SoftmaxRunner(std::string const &name) noexcept;

    ///////////////////////////////////////////////////////////////////////////////
    /// \brief Run the Softmax kernel on the device.
    ///
    /// \details The softmax kernel performs a normalized exponential function operation
    ///          over the input.
    ///
    /// \pre ri.deviceBufSize is the same as the value returned by getDeviceReservedSize().
    ///
    /// \pre deviceBuf is allocated and initialized on the device by a previous
    /// call to initDeviceReservedSpace() using the CASK builder of the same version
    /// and passes the input checker.
    ///
    /// \pre hostBuf is allocated and initialized on the host by a previous
    /// call to initHostReservedSpace() using the CASK builder of the same version
    /// and passes the input checker.
    ///
    /// \pre srcData and destData are the device buffers required to perform the softmax operation.
    /// The tensors must be *element* aligned.  The shapes of the tensors are given
    /// elsewhere (as part of the softmax parameters)
    ///
    /// \param ri: ri.op must be null while calling this function.
    ///
    ///            ri.hostBuf must point to a serializedHostBuf of size ri.hostBufSize
    ///            that was previously initialized using the initHostReservedSpace function
    ///
    ///            ri.deviceBuf must point to a serializedDeviceBuf of size ri.deviceBufSize
    ///            that was previously initialized using the initDeviceReservedSpace function
    ///
    ///            For safe cask - runInfo must be configured as follows:
    ///            ri.mode must be 0
    ///            ri.caskManagedTensorA must be false
    ///            ri.caskManagedTensorB must be false
    ///            ri.caskManagedAlphaBeta must be true
    ///            ri.caskManagedBias must be false
    ///            ri.caskManagedRelu must be false
    ///            ri.hostTensorA must be NULL
    ///            ri.hostTensorB must be NULL
    ///            ri.hostBias must be NULL
    ///            ri.hostAlpha must be NULL
    ///            ri.hostBeta must be NULL
    ///            ri.hostReLu must be NULL
    ///            ri.lastCudaError must be lwcaSuccess
    ///
    /// \param srcData: points to the device buffer that holds the input.
    /// \param destData: points to the device buffer that holds the output.
    ///                               device buffer that contains the per channel beta scales(float values).
    /// \param lwcaStream: the lwcaStream in which the kernel will be run.
    ///
    /// \returns a cask Error
    /// \retval Error::OK if the kernel was launched successfully
    /// \retval Error::BAD_RUNINFO if ri.op is not null
    /// \retval Error::BAD_RUNINFO if caskManagedAlphaBeta is true, but deviceBuf is NULL.
    /// \retval Error::PER_CHANNEL_SCALING if ri.caskManagedAlphaBeta is false and perChannelScaling is true
    ///                                    in hostBuf, but devicePerChannelAlpha or devicePerChannelBeta is NULL.
    /// \retval Error::BUFFER_CORRUPTION if the sanity check on the contents of ri.hostBuf fails.
    /// \retval Error::PLATFORM if the kernel launch fails.  Get specific error
    ///                         by calling lwcaGetLastError() method.
    /// \exception-guarantee Does not throw any exceptions.
    /// \behaviour calls asynchronous functions, thread safe, reentrant.
    ///////////////////////////////////////////////////////////////////////////////

    virtual Error run(RunInfo& ri,
                      const void* srcData,
                      void* destData,
                      lwcaStream_t stream) const = 0;

    /// Name of the softmax runner.
    std::string const name;

    /// A unique handle for this softmax runner - can be used to find this
    /// runner in a list
    uint64_t const handle;

  protected:
    ~SoftmaxRunner() = default;
    SoftmaxRunner(SoftmaxRunner const&) = default;
    SoftmaxRunner(SoftmaxRunner&&) = default;
};

///////////////////////////////////////////////////////////////////////////////
/// \brief This function returns a SoftmaxRunner corresponding to the given handle
/// \details This function gets the list of all available softmax runners and finds
///          the runner with this specific handle.
/// \param handle: The unique handle of softmax runner.
///
/// \returns A constant pointer to the SoftmaxRunner.
/// \retval nullptr if there is no SoftmaxRunner with the given handle.
/// \exception-guarantee Does not throw exceptions.
/// \behaviour blocking, thread safe, reentrant.
///////////////////////////////////////////////////////////////////////////////

const SoftmaxRunner*
findSoftmaxRunnerByHandle(uint64_t handle) noexcept;

}

#endif // SAFE_INCLUDE_GUARD_CASK_SOFTMAX_RUNNER_H
