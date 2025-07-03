//
// Copyright (c) 2019, LWPU CORPORATION.  All rights reserved.
//
// LWPU CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from LWPU CORPORATION is strictly prohibited.
//

#ifndef INCLUDE_GUARD_CASK_POOLING_H
#define INCLUDE_GUARD_CASK_POOLING_H

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/pooling.h> directly; include <cask.h> instead."
#endif

#include "cask/operation.h"

namespace cask {

class PoolingOperation;
class PoolingShader;

using Pooling = PoolingOperation;

// Use this typedef to access pooling shaders. ShaderList does a fancy
// cast based on the type.
typedef ShaderList<PoolingShader, PoolingOperation> PoolingShaderList;

// TODO: Now that I've got the static member, this should get deprecated:
inline
const PoolingShaderList* availablePoolingShaders() {
    return PoolingShaderList::availableShaders();
}

////////////////////////////////////////////////////////////////////////////////

/// \brief A class that describes a pooling problem.
class
PoolingOperation : public Operation {
public:
    /// \brief An enumeration of all supported pooling modes.
    typedef enum {
        /// Max pooling.
        POOLING_MAX                           = 0,
        /// Average pooling. Includes padded region.
        POOLING_AVERAGE_COUNT_INCLUDE_PADDING = 1,
        /// Average pooling. Excludes padded region.
        POOLING_AVERAGE_COUNT_EXCLUDE_PADDING = 2,
        /// Max pooling, with deterministic numerics.
        POOLING_MAX_AVERAGE_BLEND             = 3,
        /// Unknown pooling type.
        POOLING_UNKNOWN                       = 4,
    } poolingMode_t;

    /// \brief A descriptor for the pooling operation.
    struct poolingDescription {

        /// Trivial constructor.
        poolingDescription();

        /// Input tensor.
        TensorDesc inputDesc;
        /// Output tensor.
        TensorDesc outputDesc;
        /// Pooling Mode.
        poolingMode_t mode;
        /// The number of spatial dimensions to perform pooilng over.
        uint32_t nbDims;
        /// Window spatial dimensions.
        int64_t windowDimA[TensorDesc::MAX_DIMS - 2];
        /// Input logical pre-padding.
        int64_t paddingPreA[TensorDesc::MAX_DIMS - 2];
        /// Input logical post-padding.
        int64_t paddingPostA[TensorDesc::MAX_DIMS - 2];
        /// Pooling stride per spatial dimension.
        int64_t strideA[TensorDesc::MAX_DIMS - 2];

        /// Specifies whether alpha/beta are per-channel.
        bool perChannelScaling;

        // TODO: support doubles (use templates)!!!
        /// Output scaling factor.
        float alpha;
        /// Input scaling factor.
        float beta;
        /// Device pointer to per-channel alphas.
        float *devicePerChannelAlpha;
        /// Device pointer to per-channel betas.
        float *devicePerChannelBeta;

        /// Blend factor for IMMA max average blend pooling
        float blendFactor;

        /// TODO: is this needed?
        HardwareInformation hardwareInfo;

    };

    using Description = poolingDescription;

    /// \brief Trivial constructor.
    PoolingOperation(void);

    /// \brief Trivial destructor.
    ~PoolingOperation(void) { }

    /// \brief Returns the poolingDescription from this operation.
    /// We're a bit lazy with getters. This is all we have for now.
    inline const poolingDescription& getDescription() const { return mPoolingDescription; }

    /// \brief returns the poolingDescription as a non-const reference
    inline poolingDescription& getDescription() { return mPoolingDescription; }

    /// \brief Set the pooling descriptor.
    /// \param[in] desc The pooling descriptor.
    inline void setDescription(const poolingDescription& desc) { mPoolingDescription = desc; }

    /// \brief Checks whether the problem described in this operation makes sense.
    Error isConsistent() const;

    // Setters

    /// \brief Set the alpha scaling parameter.
    inline void setAlpha(const float alpha) { mPoolingDescription.alpha = alpha; }
    /// \brief Set the beta scaling parameter.
    inline void setBeta(const float beta) { mPoolingDescription.beta = beta; }

    /// \brief Set per-channel alpha device pointer.
    inline void setPerChannelAlphaDptr(float *alphaDptr) {
        mPoolingDescription.devicePerChannelAlpha = alphaDptr;
    }
    /// \brief Set per-channel beta device pointer.
    inline void setPerChannelBetaDptr(float *betaDptr) {
        mPoolingDescription.devicePerChannelBeta = betaDptr;
    }

    /// \brief Select whether alpha and beta are single values, or per-channel.
    inline void setScalingMode(const bool perChannelScaling) {
        mPoolingDescription.perChannelScaling = perChannelScaling;
    }

    /// \brief Set the blend factor.
    inline void setBlendFactor(float blendFactor) {
        mPoolingDescription.blendFactor = blendFactor;
    }

    /// \brief Set the input tensor descriptor.
    inline void setInputDesc(const TensorDesc &inputDesc) {
        mPoolingDescription.inputDesc = inputDesc;
    }

    /// \brief Set the output descriptor.
    inline void setOutputDesc(const TensorDesc &outputDesc) {
        mPoolingDescription.outputDesc = outputDesc;
    }

    /// \brief Set the pooling mode.
    inline void setPoolingMode(const poolingMode_t mode) {
        mPoolingDescription.mode = mode;
    }

    /// \brief Set the number of spatial dimensions.
    /// \returns DIMS_UNSUPPORTED if \param nbDims is too large. OK otherwise.
    inline Error setNbDims(const uint32_t nbDims) {
        if (nbDims > TensorDesc::MAX_DIMS - 2) {
            return Error::DIMS_UNSUPPORTED;
        }
        mPoolingDescription.nbDims = nbDims;
        return Error::OK;
    }

    /// \brief Set the pooling-window dimensions.
    /// \returns DIMS_UNSUPPORTED if \param nbDims is too large. OK otherwise.
    inline Error setWindowDims(const int64_t windowDimA[], const uint32_t nbDims) {
        if (nbDims > TensorDesc::MAX_DIMS - 2) {
            return Error::DIMS_UNSUPPORTED;
        }
        for (uint32_t i = 0U; i < nbDims; i++) {
            mPoolingDescription.windowDimA[i] = windowDimA[i];
        }
        return Error::OK;
    }

    // TODO: Note the relieance on the Tensor::Dim enum for padding and strides.
    // This might not be a great thing, since N and C don't make sense here.

    /// \brief Set input top-padding.
    inline void setPaddingTop(const int64_t pad) {
        mPoolingDescription.paddingPreA[TensorDesc::Dim::H] = pad;
    }

    /// \brief Set input bottom-padding.
    inline void setPaddingBottom(const int64_t pad) {
        mPoolingDescription.paddingPostA[TensorDesc::Dim::H] = pad;
    }

    /// \brief Set input left-padding.
    inline void setPaddingLeft(const int64_t pad) {
        mPoolingDescription.paddingPreA[TensorDesc::Dim::W] = pad;
    }

    /// \brief Set input right-padding.
    inline void setPaddingRight(const int64_t pad) {
        mPoolingDescription.paddingPostA[TensorDesc::Dim::W] = pad;
    }

    /// \brief Set input pre-padding for spatial dimensions.
    /// \returns DIMS_UNSUPPORTED if \param nbDims is too large. OK otherwise.
    inline Error setPaddingPre(const int64_t paddingA[], const uint32_t nbDims) {
        if (nbDims > TensorDesc::MAX_DIMS) {
            return Error::DIMS_UNSUPPORTED;
        }
        for (uint32_t i = 0U; i < nbDims; i++) {
            mPoolingDescription.paddingPreA[i] = paddingA[i];
        }
        return Error::OK;
    }

    /// \brief Set input post-padding for spatial dimensions.
    /// \returns DIMS_UNSUPPORTED if \param nbDims is too large. OK otherwise.
    inline Error setPaddingPost(const int64_t paddingA[], const uint32_t nbDims) {
        if (nbDims > TensorDesc::MAX_DIMS) {
            return Error::DIMS_UNSUPPORTED;
        }
        for (uint32_t i = 0U; i < nbDims; i++) {
            mPoolingDescription.paddingPostA[i] = paddingA[i];
        }
        return Error::OK;
    }

    /// \brief Set pooling-window strides.
    /// \returns DIMS_UNSUPPORTED if \param nbDims is too large. OK otherwise.
    inline Error setStrides(const int64_t strideA[], const uint32_t nbDims) {
        if (nbDims > TensorDesc::MAX_DIMS) {
            return Error::DIMS_UNSUPPORTED;
        }
        for (uint32_t i = 0U; i < nbDims; i++) {
            mPoolingDescription.strideA[i] = strideA[i];
        }
        return Error::OK;
    }

    /// \brief Get the pooling mode by parsing the name of the shader
    /// \returns the mode of the pooling by parsing the \param name
    inline poolingMode_t getPoolingMode(std::string name) const {
        if (name.find("max_avg_blend") != std::string::npos) {
            return POOLING_MAX_AVERAGE_BLEND;
        } else if (name.find("avg_exclude_padding") != std::string::npos) {
            return POOLING_AVERAGE_COUNT_EXCLUDE_PADDING;
        } else if (name.find("avg") != std::string::npos) {
            return POOLING_AVERAGE_COUNT_INCLUDE_PADDING;
        } else if (name.find("max") != std::string::npos) {
            return POOLING_MAX;
        }
        return POOLING_UNKNOWN;
    }

private:
    /// \brief This is where we store the pooling parameters for this operation.
    struct poolingDescription mPoolingDescription;
};

/// \brief A wrapper around a pooling kernel.
/// Each instance is specialized by input/output types, direction of the operation
/// (fwd/bwd), and by the pooling mode (max/avg).
class
PoolingShader : public Shader {
  struct TraitsEnum {
    enum Label { Unknown,
                 Fp32,
                 Fp16,
                 Fp16x2,
                 Int8x4,
                 Int8x32
            };
  };

  typedef SafeEnum<TraitsEnum> Traits;

  /// Accessor for the traits structure
  static Traits getTraits(const std::string shaderName);

  const Traits traits;
public:
    /// The signature of our kernel launcher.
    typedef Error (*poolingLauncher_pf)(
            struct PoolingOperation::poolingDescription&,
            const void*,
            void*,
            lwdaStream_t,
            lwdaError_t&);

    /// \brief Constrlwtor
    /// \param ki All the infromation needed to instantiate a pooling shader.
    PoolingShader(const KernelInfo* ki, const poolingLauncher_pf launcher);

    PoolingShader(const KernelInfo* ki);

    /// \brief Trivial Destructor.
    ~PoolingShader() { }

    /// \brief Return the kernel info instance
    virtual const PoolingKernelInfo* getInfo() const;

    /// \brief Check whether this shader can implement \param poolOp.
    /// \returns OK if everything is ok, otherwise:
    ///     SCALAR_TYPES if the tensor data-types aren't supported.
    virtual Error canImplement(const PoolingOperation& poolOp) const = 0;

    /// \brief Check whether this shader can implement \param poolOp for a
    /// particular \ccv.
    /// \returns OK if everything is ok, otherwise:
    ///     SCALAR_TYPES if the tensor data-types aren't supported.
    ///     COMPUTE_CAPABILITY if this shader isn't supported given \param ccv.
    virtual Error canImplement(const PoolingOperation& poolOp,
            const ComputeCapabilityVersion ccv) const = 0;

    /// \brief Return the specific `platformError_t` value associated with a
    ///        Error::PLATFORM return code. Typically this is lwdaError_t.
    ///
    /// If a CASK call results in a LWCA error, it is captured and
    /// CASK will return Error:PLATFORM. Calling `lastPlatformError` will then
    /// return the specific LWCA error for debugging and reporting purposes.
    /// \pre a CASK call by this class that returns `Error::PLATFORM`
    /// \returns a `lwdaError_t` value captured by CASK that caused a
    ///          `Error::PLATFORM` return code.
    virtual platformError_t lastPlatformError(const serializableHostBuf hostBuf) const;

    /// \brief Returns the chip type supported by this kernel.
    /// Needed by ShaderList.
    virtual GemmChip chip() const = 0;

    /// \brief returns the size of the reserved buffer that this Shader needs
    /// on the host side.
    virtual Error getHostReservedSize(RunInfo& ri) const;

    /// \brief returns the size of the reserved buffer that this Shader needs
    /// on the device side.
    virtual Error getDeviceReservedSize(RunInfo& ri) const;

    /// \brief returns the size of the workspace that this Shader needs
    /// on the device side.
    virtual int64_t getDeviceWorkspaceSize(RunInfo& ri) const;

    /// \brief Initializes any needed reserved space.
    virtual Error initHostReservedSpace(RunInfo& ri) const;

    /// \brief Initializes any needed reserved space.
    virtual Error initDeviceReservedSpace(RunInfo& ri,
             platformStream_t lwdaStream);

    //////////////////////////////////////////////////////////////////////////
    /// \brief Check the validity of precomputed values in cask_safe::RunInfo.
    ///
    /// \details Check the validity of precomputed values in cask_safe::RunInfo.
    /// There's a default implementation of this one, which always return
    /// Error::OK. Derivations for safety could determine if ilwolving input
    /// check or not.
    ///
    /// \returns Error::OK if the precomputed value(s) are legal, otherwise
    /// return Error::BUFFER_CORRUPTION.
    //////////////////////////////////////////////////////////////////////////
    virtual Error checkInput(const RunInfo& ri) const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return grid size of the main kernel launch.
    ///
    /// Only query the grid size of the main kernel. Auxiliary kernels and
    /// splitK kernels are not supported.
    ///
    /// \pre Kernels params must be initialized before calling this API. In
    /// other words, users need to call initHostReservedSpace() first.
    ///
    /// \returns queried grid size, or (0, 0, 0) if not implememted.
    /////////////////////////////////////////////////////////////////////////
    virtual dim3 getGridSize(RunInfo& ri) const;

    /// \brief Run the pooling kernel.
    /// \param ri Contains the pooling operation among other things.
    /// \param inputData A device pointer to the input tensor.
    /// \param outputData A device pointer to the output tensor.
    /// \param stream The stream on which the kernel is to be launched.
    virtual Error run(RunInfo& ri,
                      void* deviceWorkspacePointer,
                      const void* inputData,
                      void* outputData,
                      platformStream_t lwdaStream);

    /// \brief Datatype of the input tensor.
    virtual ScalarType inputScalarType() const;

    /// \brief Datatype of the output tensor.
    virtual ScalarType outputScalarType() const;

    /// \brief Number of (packed) scalar values in a element on the input
    virtual int inputScalarsPerElement() const;

    /// \brief Number of (packed) scalar values per element on the output
    virtual int outputScalarsPerElement() const;

    /// \brief Dimension of the input tensor.
    virtual int inputVectorizedDim() const;

    /// \brief Dimension of the output tensor.
    virtual int outputVectorizedDim() const;

private:
    const poolingLauncher_pf launcher;

    // Disallow the trivial constructor.
    PoolingShader(void);

};

}  // namespace cask

#endif //  INCLUDE_GUARD_CASK_POOLING_H
