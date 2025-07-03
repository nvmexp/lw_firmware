// enable doxygen:
//! @file
#ifndef SAFE_INCLUDE_GUARD_CASK_CONVOLUTION_RUNNER_H
#define SAFE_INCLUDE_GUARD_CASK_CONVOLUTION_RUNNER_H

#include <string>
#include "cask/safe/cask_runner.h"

namespace cask_safe {
/////////////////////////////////////////////////////////////////////////
/// \brief The ConvolutionRunner is a cask runner that can perform a convolution
///
/// \details The ConvolutionRunner is the base class for all runners that can
///          perform a convolution. Each runner instance corresponds to a single
///          kernel implementation.
///
/////////////////////////////////////////////////////////////////////////
class ConvolutionRunner : public KernelRunner {
public:
    /////////////////////////////////////////////////////////////////////////
    /// \brief Constructor for the ConvolutionRunner class
    /// \details Runners are not intended to be created by users. They are created
    ///         internally in the cask library.
    /// \param name: Name of the convolution runner that is being constructed
    /////////////////////////////////////////////////////////////////////////
    ConvolutionRunner(std::string const &name) noexcept ;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Run the convolution kernel on the device.
    ///
    /// \details The convolution kernel performs the computation \f$\mathrm{Output} \leftarrow \alpha
    /// (\mathrm{Input} * \mathrm{Filter}) + \beta C + \mathrm{bias}\f$. Where '*' denotes the convolution
    /// operation
    ///
    /// \pre ri.deviceBufSize is the same as the value returned by
    /// getDeviceReservedSize()
    ///
    /// \pre deviceBuf is allocated and initialized on the device by a previous
    /// call to initDeviceReservedSpace() using the CASK builder of the same version 
    /// and passes the input checker 
    ///
    /// \pre hostBuf is allocated and initialized on the host by a previous
    /// call to initHostReservedSpace() using the CASK builder of the same version 
    /// and passes the input checker
    ///
    /// \pre deviceInputTensor, deviceOutputTensor, and deviceCTensor are the device
    /// buffers required to perform the convolution operation.  The tensors
    /// must be *element* aligned.  The shapes of the tensors are given
    /// elsewhere (as part of the conv parameter).
    ///
    /// \param ri: ri.op must be null while calling this function \n
    ///            ri.hostBuf must point to a serializedHostBuf of size ri.hostBufSize
    ///            was previously initialized using the initHostReservedSpace function \n
    ///            ri.deviceBuf must point to a serializedDeviceBuf of size ri.deviceBufSize
    ///            that was previously initialized using the initDeviceReservedSpace function \n
    ///            For safe cask - runInfo must be configured as follows: \n
    ///            ri.caskManagedTensorA must be false \n
    ///            ri.caskManagedTensorB must be true \n
    ///            ri.caskManagedAlphaBeta must be true \n
    ///            ri.caskManagedBias must be true \n
    ///            ri.caskManagedRelu must be false \n
    ///            ri.mode must be 0
    ///
    /// \param deviceOutputTensor: Points to the device buffer where the output will be written.
    /// \param deviceATensor: Points to the device buffer that holds the input
    /// \param deviceBTensor: nullptr if ri.caskManagedTensorB is true. Otherwise points to the device
    ///                     buffer that contains the filter data
    /// \param deviceCTensor: Points to the device buffer that contains the C tensor data.
    /// \param deviceBiasTensor: nullptr is ri.caskManagedBias is true. Otherwise points to the device
    ///                     buffer that contains the bias data
    /// \param deviceAlphaVector: nullptr if ri.caskManagedAlphaBeta is true. Otherwise points to the
    ///                     device buffer that contains the per channel alpha scales(float values)
    /// \param deviceBetaVector: nullptr if ri.caskManagedAlphaBeta is true. Otherwise points to the
    ///                     device buffer that contains the per channel beta scales(float values)
    /// \param lwcaStream: the lwcaStream in which the kernel will be run
    ///
    /// \returns a cask Error
    /// \retval Error::OK if the kernel was launched successfully
    /// \retval Error::BAD_RUNINFO if ri.op is not null
    /// \retval Error::BAD_RUNINFO if ri.caskManagedTensorA is true
    /// \retval Error::PLATFORM if the kernel launch fails, and CudaError is set in RunInfo.
    /// \retval Error::BATCH_SIZE if ri.batchSize does not match the batch size provided in build time
    /// \retval Error::BUFFER_CORRUPTION if the sanity check on the contents of ri.hostBuf fails
    /// \exception-guarantee Does not throw any exceptions
    /// \behaviour calls asynchronous functions, thread safe
    /////////////////////////////////////////////////////////////////////////
    virtual Error run(RunInfo& ri,
                      void* deviceWorkspacePointer,
                      void* deviceOutputTensor,
                      const void* deviceATensor,
                      const void* deviceBTensor,
                      const void* deviceCTensor,
                      const void* deviceAmTensor,
                      const void* deviceBiasTensor,
                      const void* deviceAlphaVector,
                      const void* deviceBetaVector,
                      platformStream_t lwcaStream) const noexcept = 0;


    /// Name of the convolution runner
    std::string const name;

    /// A unique handle for this convolution runner - can be used to find this runner in a list
    uint64_t const handle;

    ~ConvolutionRunner() = default;
protected:
    ConvolutionRunner(ConvolutionRunner const&) = default;
    ConvolutionRunner(ConvolutionRunner&&) = default;
};


/////////////////////////////////////////////////////////////////////////
/// \brief This function returns a ConvolutionRunner corresponding to the given handle
/// \details This function gets the list of all available convolution runners and finds
///          the runner with the specific handle.
/// \param handle: The unique handle of the convolution runner
///
/// \returns A constant pointer to the ConvolutionRunner
/// \retval nullptr if there is no ConvolutionRunner with the given handle
/// \exception-guarantee Does not throw exceptions
/// \behaviour blocking, thread safe
/////////////////////////////////////////////////////////////////////////
const ConvolutionRunner*
findConvolutionRunnerByHandle(uint64_t handle) noexcept;

}

#endif  // INCLUDE_GUARD_CASK_CONVOLUTION_RUNNER_H
