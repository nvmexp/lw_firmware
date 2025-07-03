//
// Copyright (c) 2019, LWPU CORPORATION.  All rights reserved.
//
// LWPU CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from LWPU CORPORATION is strictly prohibited.
//

#ifndef INCLUDE_GUARD_CASK_SOFTMAX_H
#define INCLUDE_GUARD_CASK_SOFTMAX_H

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/softmax.h> directly; include <cask.h> instead."
#endif

namespace cask {

class SoftmaxOperation;
class SoftmaxShader;

// Use this typedef to access softmax shaders. ShaderList does a fancy
// cast based on the type.
typedef ShaderList<SoftmaxShader, SoftmaxOperation> SoftmaxShaderList;

// TODO: Now that I've got the static member, this should get deprecated:
inline
const SoftmaxShaderList* availableSoftmaxShaders() {
    return SoftmaxShaderList::availableShaders();
}

/// \brief A descriptor for a softmax problem.
class
SoftmaxOperation : public Operation {
public:
    using softmaxMode_t = cask_safe::SoftmaxMode;
    using softmaxAlgorithm_t = cask_safe::SoftmaxAlgorithm;

    /**
     * \brief A POD struct describing the input and optional settings of
     * a softmax operation. It is encapsulated in the
     * Operation object and the public interface is meant to be exposed
     * by setters and getters of the Operation subclasses.
     */

    struct softmaxDescription {
        /// Input Tensor Description.
        TensorDesc inputDesc;
        /// Output Tensor Description
        TensorDesc outputDesc;

        bool perChannelScaling;
        /// Softmax Mode
        softmaxMode_t mode;
        /// Softmax Algorithm
        softmaxAlgorithm_t algorithm;

        /// Output scaling factor.
        float alpha;
        /// Input scaling factor
        float beta;

        float *devicePerChannelAlpha;
        float *devicePerChannelBeta;

        HardwareInformation hardwareInfo;

        softmaxDescription(void);
    };

    /// \brief Default ctor.
    SoftmaxOperation(void);
    /// \brief Default dtor.
    ~SoftmaxOperation(void) { }

    /// \brief Return Softmax Description.
    inline const softmaxDescription& getDescription() const {
        return mSoftmaxDescription;
    }

    /// \brief Set the softmax descriptor
    inline void setDescription(const softmaxDescription& desc) { mSoftmaxDescription = desc; }

    /// \brief Check if problem sizes are consistent.
    Error isConsistent() const;

    /// \brief Set the alpha scaling parameter.
    inline void setAlpha(const float alpha) { mSoftmaxDescription.alpha = alpha; }
    /// \brief Set the beta scaling parameter.
    inline void setBeta(const float beta) { mSoftmaxDescription.beta = beta; }

    /// \brief Set per-channel alpha device pointer.
    inline void setPerChannelAlphaDptr(float *alphaDptr) {
        mSoftmaxDescription.devicePerChannelAlpha = alphaDptr;
    }
    /// \brief Set per-channel beta device pointer.
    inline void setPerChannelBetaDptr(float *betaDptr) {
        mSoftmaxDescription.devicePerChannelBeta = betaDptr;
    }

    /// \brief Select whether alpha and beta are single values, or per-channel.
    inline void setScalingMode(const bool perChannelScaling) {
        mSoftmaxDescription.perChannelScaling = perChannelScaling;
    }

    /// \brief Set the input tensor descriptor.
    inline void setInputDesc(const TensorDesc &inputDesc) {
        mSoftmaxDescription.inputDesc = inputDesc;
    }

    /// \brief Set the output descriptor.
    inline void setOutputDesc(const TensorDesc &outputDesc) {
        mSoftmaxDescription.outputDesc = outputDesc;
    }

    /// \brief Set the Softmax mode.
    inline void setSoftmaxMode(const softmaxMode_t mode) {
        mSoftmaxDescription.mode = mode;
    }

    /// \brief Set the Softmax algorithm.
    inline void setSoftmaxAlgorithm(const softmaxAlgorithm_t algorithm) {
        mSoftmaxDescription.algorithm = algorithm;
    }

    /// \brief Get the softmax mode by parsing the name of the shader
    /// \returns the mode of the softmax by parsing the \param name
    inline softmaxMode_t getSoftmaxMode(std::string name) const {
        if (name.find("instance") != std::string::npos) {
            return softmaxMode_t::SOFTMAX_MODE_INSTANCE;
        } else if (name.find("channel") != std::string::npos) {
            return softmaxMode_t::SOFTMAX_MODE_CHANNEL;
        }
        return softmaxMode_t::SOFTMAX_MODE_UNKNOWN;
    }

    /// \brief Get the softmax algorithm by parsing the name of the shader
    /// \returns the algorithm of the softmax by parsing the \param name
    inline softmaxAlgorithm_t getSoftmaxAlgorithm(std::string name) const {
        if (name.find("fast") != std::string::npos) {
            return softmaxAlgorithm_t::SOFTMAX_FAST;
        } else if (name.find("accurate") != std::string::npos) {
            return softmaxAlgorithm_t::SOFTMAX_ACLWRATE;
        } else if (name.find("log") != std::string::npos) {
            return softmaxAlgorithm_t::SOFTMAX_LOG;
        }
        return softmaxAlgorithm_t::SOFTMAX_UNKNOWN;
    }

private:
    /// \brief This is where we store the softmax parameters for this operation.
    struct softmaxDescription mSoftmaxDescription;

};

class SoftmaxShader : public Shader {
    enum class Traits {
        Unknown,
        Fp32,
        Fp16,
        Fp16x2,
        Int8x4,
        Int8x32
    };

    /// Accessor for the traits structure
    static Traits getTraits(const std::string shaderName);
    const Traits traits;

public:
    /// The signature of our kernel launcher.
    typedef Error (*softmaxLauncher_pf)(
            struct SoftmaxOperation::softmaxDescription&,
            const void*,
            void*,
            lwdaStream_t,
            lwdaError_t&);

    /// \brief Constrlwtor
    /// \param ki All the infromation needed to instantiate a softmax shader.
    SoftmaxShader(const KernelInfo* ki, const softmaxLauncher_pf launcher);

    /// \brief Trivial Destructor.
    ~SoftmaxShader() { }

    /// \brief Check whether this shader can implement \param poolOp.
    /// \returns OK if everything is ok, otherwise:
    ///     SCALAR_TYPES if the tensor data-types aren't supported.
    virtual Error canImplement(const SoftmaxOperation& softmaxOp) const = 0;

    /// \brief Check whether this shader can implement \param poolOp for a
    /// particular \ccv.
    /// \returns OK if everything is ok, otherwise:
    ///     SCALAR_TYPES if the tensor data-types aren't supported.
    ///     COMPUTE_CAPABILITY if this shader isn't supported given \param ccv.
    virtual Error canImplement(const SoftmaxOperation& softmaxOp,
            const ComputeCapabilityVersion ccv) const = 0;

    /// \brief Returns the chip type supported by this kernel.
    /// Needed by ShaderList.
    virtual GemmChip chip() const = 0;

    /// \brief returns the size of the reserved buffer that this Shader needs
    /// on the host side.
    virtual Error getHostReservedSize(RunInfo& ri) const;

    /// \brief Initializes any needed reserved host space
    virtual Error initHostReservedSpace(RunInfo& ri) const;

    /// \brief returns the size of the reserved buffer that this Shader needs
    /// on the device side.
    virtual Error getDeviceReservedSize(RunInfo& ri) const;

    /// \brief Initializes any needed reserved space.
    virtual Error initDeviceReservedSpace(RunInfo& ri,
                const platformStream_t lwdaStream);

    /// \brief Callwlate the device workspace buffer sizes (in bytes).
    virtual int64_t getDeviceWorkspaceSize(RunInfo& ri) const;

    /// \brief Update runtime parameters before calling run().
    virtual Error updateRuntimeParameters(RunInfo& ri, platformStream_t lwdaStream) const;

    /// \brief Run the softmax kernel.
    /// \param ri Contains the softmax operation among other things.
    /// \param inputData A device pointer to the input tensor.
    /// \param outputData A device pointer to the output tensor.
    /// \param stream The stream on which the kernel is to be launched.
    virtual Error run(RunInfo& ri,
                      const void* inputData,
                      void* outputData,
                      lwdaStream_t lwdaStream);

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

    //////////////////////////////////////////////////////////////////////////
    /// \brief Check the validity of precomputed values in RunInfo.
    ///
    /// \details Check the validity of precomputed values in RunInfo.
    /// There's a default implementation of this one, which always return
    /// Error::OK. Derivations for safety could determine if ilwolving input
    /// check or not.
    ///
    /// \returns Error::OK if the precomputed value(s) are legal, otherwise
    /// return Error::BUFFER_CORRUPTION.
    //////////////////////////////////////////////////////////////////////////
    virtual Error checkInput(const RunInfo& ri) const;
private:
    const softmaxLauncher_pf launcher;

    // Disallow the trivial constructor.
    SoftmaxShader(void);
};

} // namespace cask

#endif
