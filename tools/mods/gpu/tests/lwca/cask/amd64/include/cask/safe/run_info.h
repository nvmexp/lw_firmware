#ifndef SAFE_INCLUDE_GUARD_RUN_INFO_H
#define SAFE_INCLUDE_GUARD_RUN_INFO_H
// enable doxygen:
//! @file

#include <lwca_runtime.h>
#include <lwca.h>

#include "cask/safe/platform.h"

namespace cask_safe {

using char_t = char;
using AsciiChar = char_t;
// //////////////////////////////////////////////////////////////////////////////
// /// \enum Error
// /// \brief Error types used throughout the library
// ///
// /// The Error enum is the typical return type for member functions in
// /// CASK. Error::OK is used to indicate that a function completed
// /// as expected and all return parameters are valid.
// //////////////////////////////////////////////////////////////////////////////
enum class Error : uint32_t
{
        /// \brief No error to report, member function completed
        /// successfully.
        OK                           = caskMakeECC(0x0),
        ALIGNMENT                    = caskMakeECC(0x1), ///< Unsupported memory alignment or padding
        BATCH_SIZE                   = caskMakeECC(0x2), ///< Unsupported or inconsistent batch sizes
        CANT_PAD                     = caskMakeECC(0x3), ///< Unsupported or inconsistent convolution
        DIMS_UNSUPPORTED             = caskMakeECC(0x4), ///< Unsupported number of dimensions.
        NULL_PTR                     = caskMakeECC(0x5), ///< The user passed a null pointer.
        VECT_UNSUPPORTED             = caskMakeECC(0x6), ///< Unsupported vectorization.
        ///< "padding"
        STRIDE_UNSUPPORTED           = caskMakeECC(0x7), ///< Stride is not supportedf
        DILATION_UNSUPPORTED         = caskMakeECC(0x8), ///< Dilation is not supported
        CONST_SIZE                   = caskMakeECC(0x9), ///< Shader lacks sufficient constant memory
        ///< space
        PLATFORM                     = caskMakeECC(0xA), ///< Get specific error by calling
        ///< lwcaGetLastError() method
        FILTER_OK                    = caskMakeECC(0xB), ///< Filter ok, but needs transpose
        FILTER_SIZE                  = caskMakeECC(0xC), ///< Unsupported filter size
        FMT_UNSUPPORTED              = caskMakeECC(0xD), ///< Unsupported or inconsistent tensor format
        GROUPS                       = caskMakeECC(0xE), ///< Unsupported or inconsistent groups
        ///  specification
        INTERNAL                     = caskMakeECC(0xF), ///< An assertion in the cask implementation
        SCALAR_TYPES                 = caskMakeECC(0x10),///< Unsupported scalar type conversion
        SHADER_LOADED                = caskMakeECC(0x11), ///< Shader already loaded
        SHADER_NOT_LOADED            = caskMakeECC(0x12), ///< Shader not yet loaded
        SIMD_TYPES                   = caskMakeECC(0x13), ///< Unsupported simd type converions
        SIZE_MISMATCH                = caskMakeECC(0x14), ///< Two or more of the tensors have dimensions
        ///  that should match, but don't (For example A
        ///  should be MxK, B should be KxN, C should be
        ///  MxN, but either M, K, or N don't match, or
        ///  the C or K dimensions of the input and
        ///  filter, or filter and outputs don't match.
        STRIDES                      = caskMakeECC(0x15),   ///< Inconsistent tensor strides
        XCORRELATION_ONLY            = caskMakeECC(0x16),   ///< Shader only supports cross correlation
        PAD_C                        = caskMakeECC(0x17),   ///< the per group C is too small, needs padding
        NO_PER_CHANNEL_SCALING       = caskMakeECC(0x18),   ///< this kernel doesn't support per channel scaling
        PER_CHANNEL_SCALING          = caskMakeECC(0x19),   ///< this kernel only supports per channel scaling
        POOLING_MODE                 = caskMakeECC(0x1A),   ///< The pooling kernel mode is unsupported.
        RELU_BIAS                    = caskMakeECC(0x1B),   ///< this kernel does not support relu bias
        RELU_INVALID                 = caskMakeECC(0x1C),   ///< ReLu setting/value is not valid
        SOFTMAX_MODE                 = caskMakeECC(0x1D),   ///< The softmax kernel mode/algorithm is unsupported
        SPLIT_H                      = caskMakeECC(0x1E),   ///< Unsupported or inconsistent SplitH request
        SPLIT_K                      = caskMakeECC(0x1F),   ///< Unsupported or inconsistent SplitK request
        SPLIT_K_BETA                 = caskMakeECC(0x20),   ///< Unsupported SplitK beta vaule
        COMPUTE_CAPABILITY           = caskMakeECC(0x21),   ///< Unsupported operation under current compute capability
        PLANAR_COMPLEX                = caskMakeECC(0x22),   ///< Unsupported or wrong settings for planar complex
        BAD_RUNINFO                  = caskMakeECC(0x23),   ///< Problem with the provided runinfo struct.
        DYNAMIC_RESIZE_UNSUPPORTED   = caskMakeECC(0x24),   ///< Dynamic resize feature unsupported
        BUFFER_CORRUPTION            = caskMakeECC(0x25),   ///< The serialized host buffer is corrupted
        BAD_ALPHA_BETA               = caskMakeECC(0x26),   ///< Unsupported alpha or beta value
        TENSOR_SIZE_UNSUPPORTED      = caskMakeECC(0x27),   ///< Unsupported tensor size
        INVALID_RUNNER_SETUP         = caskMakeECC(0x28),   ///< Invalidate runner process
        NO_PADDING_VALUE             = caskMakeECC(0x29),   ///< this kernel doesn't support customized padding value
        RESERVED_BUFFER_SIZE         = caskMakeECC(0x30),   ///< Insufficient reserved buffer for parameters
        FEATURE_DISABLED_IN_COMPILATION         = caskMakeECC(0x31),   ///< Feature is disabled at compile time

};

//////////////////////////////////////////////////////////////////
/// \brief Get an error string for an Error.
///
/// \param error : the error to find its associated string.
/// \returns NULL if the error is unknown
/// \returns Const pointer to static string if the error is known. (The
/// caller may not free the static const string.)
//////////////////////////////////////////////////////////////////
AsciiChar const * getErrorString(Error error) noexcept;


//////////////////////////////////////////////////////////////////////////////
/// \brief serializableHostBuf is a opaque datatype.
/////////////////////////////////////////////////////////////////////////////
using serializableHostBuf = void*;
using serializableDeviceBuf = void*;

class RunInfo {
  public:

    RunInfo() noexcept = default;


     /// \brief Size of the host buffer as calculated
     ///        by getHostReservedSize().
    uint64_t hostBufSize {0U};


     /// \brief Size of the host buffer as calculated
     ///        by getDeviceReservedSize().
    int64_t deviceBufSize {0U};


     /// \brief Opaque handle to the host buffer workspace.
     ///
     /// Must be of size 'hostBufSize'. Must be set prior to calling
     /// initHostReservedSpace().
     ///
    serializableHostBuf hostBuf {nullptr};


     /// \brief Opaque handle to the device buffer workspace.
     ///
     /// Must be of size 'deviceBufSize'. Must be set prior to calling
     /// initDeviceReservedSpace().
    serializableDeviceBuf deviceBuf  {nullptr};

     /// \brief Is the A tensor cask-managed.
     ///
     /// True for configurations passing the A tensor as
     /// host data to initDeviceReservedSpace(). False
     /// for configurations passing the A tensor at runtime.
    bool caskManagedTensorA {false};

    /// \brief Is the A metadata tensor cask-managed.
    ///
    /// True for configurations passing the A tensor as
    /// host data to initDeviceReservedSpace(). False
    /// for configurations passing the A tensor at runtime.
    bool caskManagedTensorAm;


     /// \brief Is the B tensor cask-managed.
     ///
     /// True for configurations passing the B tensor as
     /// host data to initDeviceReservedSpace(). False
     /// for configurations passing the B tensor at runtime.
    bool caskManagedTensorB {false};


     /// \brief Is the bias tensor cask-managed.
     ///
     /// True for configurations passing the bias tensor as
     /// host data to initDeviceReservedSpace(). False
     /// for configurations passing the bias tensor at runtime.
    bool caskManagedBias {false};


     /// \brief Is the alpha and beta tensors cask-managed.
     ///
     /// True for configurations passing the alpha and beta tensors as
     /// host data to initDeviceReservedSpace(). False for
     /// configurations passing the alpha and beta tensors at runtime.
    bool caskManagedAlphaBeta {false};


     /// \brief Is the relu tensors cask-managed.
     ///
     /// True for configurations passing the relu tensors as
     /// host data to initDeviceReservedSpace(). False for
     /// configurations passing the alpha and beta tensors at runtime.
    bool caskManagedReLu {false};

    /// \@name Descriptors of L2 evict feature
    /// Ampere L2 evict feature
    /// please use cask::MemImplicitDescriptor, etc. in evict_descriptor.h to set descriptor
    /// high 32-bit
    int32_t descriptorA;

    /// high 32-bit
    int32_t descriptorB;

    /// low 32-bit
    int32_t descriptorC0;

    /// high 32-bit
    int32_t descriptorC1;

    /// low 32-bit
    int32_t descriptorD0;

    /// high 32-bit
    int32_t descriptorD1;

    /// \brief Value of the last lwca error
    lwcaError_t lastCudaError {lwcaSuccess};
};


} // namespace cask_safe

#endif
