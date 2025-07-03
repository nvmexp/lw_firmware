#ifndef INCLUDE_GUARD_CASK_GEMM_H
#define INCLUDE_GUARD_CASK_GEMM_H

// enable doxygen:
//! @file

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/gemm.h> directly; include <cask.h> instead."
#endif

namespace cask {

///////////////////////////////////////////////////////////////////////////////

class Gemm;
struct GemmShader;

typedef ShaderList<GemmShader,Gemm> GemmShaderList;
typedef ShaderList<GemmShader,Gemm> FullyConnectedShaderList;

///////////////////////////////////////////////////////////////////////////////

inline
const GemmShaderList* availableFullyConnectedShaders() {
    return GemmShaderList::availableShaders();
}


// TODO: Now that I've got the static member, this should get deprecated:
inline
const GemmShaderList* availableGemmShaders() {
    return GemmShaderList::availableShaders();
}

///////////////////////////////////////////////////////////////////////////////

/// \brief Describes the characteristics of the GEMM operation you want to run
/// (alpha, beta, matrix shapes).
///
/// Generalized Matrix Multiply (GEMM) computes
/// \f$C \leftarrow \alpha A B + \beta C\f$.
/// This class encapsulates the data needed to perform a GEMM operation.
///  It does not choose which algorithm
/// (kernel/shader) implements the operation.
///
///////////////////////////////////////////////////////////////////////////////
class
Gemm : public Operation {
public:
    /// \deprecated Use `cask::Error`.
    typedef cask::Error Error;  // for existing code relying on Gemm::Error
    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor. Does nothing special.
    ///
    /// The constructor takes no parameters. Inputs to the GEMM must be
    /// set via the setter functions.
    /////////////////////////////////////////////////////////////////////////
    Gemm();
    /////////////////////////////////////////////////////////////////////////
    /// \brief Dtor.
    ///
    /// Gemm does not own any allocated data, and does not delete anything.
    /////////////////////////////////////////////////////////////////////////
    virtual ~Gemm() { }

    /////////////////////////////////////////////////////////////////////////
    /// \brief returns the gemm description.
    /////////////////////////////////////////////////////////////////////////
    inline const Description& getDescription() const { return mDescription; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief returns the gemm description as a non-const reference
    /////////////////////////////////////////////////////////////////////////
    inline Description & getDescription() { return mDescription; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    ///
    /// Set the scalar alpha parameter. Input type is float.
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(float alpha) { mDescription.alpha = (double) alpha; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    ///
    /// Set the scalar alpha parameter. Input type is double.
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(double alpha) { mDescription.alpha = alpha; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    ///
    /// Set the scalar alpha parameter. Input type is lwComplex.
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(lwComplex alpha) { mDescription.alphaCp = alpha; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    ///
    /// Set the scalar alpha parameter. Input type is lwDoubleComplex.
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(lwDoubleComplex alpha) { mDescription.alphaCpDouble = alpha; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    ///
    /// Set the scalar beta parameter. Input type is float.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(float beta) { mDescription.beta = (double) beta; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    ///
    /// Set the scalar beta parameter. Input type is double.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(double beta) { mDescription.beta = beta; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    ///
    /// Set the scalar beta parameter. Input type is lwComplex.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(lwComplex beta) { mDescription.betaCp = beta; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    ///
    /// Set the scalar beta parameter. Input type is lwDoubleComplex.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(lwDoubleComplex beta) { mDescription.betaCpDouble = beta; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar type of the bias.
    ///
    /// Set the scalar type of the bias to be passed to the kernel.
    /////////////////////////////////////////////////////////////////////////
    inline void setBiasType(ScalarType sT) { mDescription.biasType = sT; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the bias simd width.
    ///
    /// Set the bias simd width.
    /////////////////////////////////////////////////////////////////////////
    inline void setBiasSimdWidth(int64_t simd=1) { mDescription.biasSimdWidth = simd; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the descriptor for the first input ("A") matrix.
    /////////////////////////////////////////////////////////////////////////
    inline void setADesc(const TensorDesc &inADesc) { mDescription.inputADesc = inADesc; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the descriptor for the input ("A") metadata matrix.
    /////////////////////////////////////////////////////////////////////////
    inline void setAmDesc(const TensorDesc &inAmDesc) { mDescription.inputAmDesc = inAmDesc; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Alias for setADesc
    /////////////////////////////////////////////////////////////////////////
    inline void setWeightDesc(const TensorDesc &wDesc) { setADesc(wDesc); }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the descriptor for the second input ("B") matrix.
    /////////////////////////////////////////////////////////////////////////
    inline void setBDesc(const TensorDesc &inBDesc) { mDescription.inputBDesc = inBDesc; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Alias for setBDesc
    /////////////////////////////////////////////////////////////////////////
    inline void setInputDesc(const TensorDesc &iDesc) { setBDesc(iDesc);}
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the output ("C") descriptor.
    /////////////////////////////////////////////////////////////////////////
    inline void setOutputDesc(const TensorDesc &outputDesc) { mDescription.outputDesc = outputDesc; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Define the split-k factor. It is 1 by default.
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitK(int32_t splitK) { mDescription.splitK = splitK; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKMode to specify number of kernels to call for splitk
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitKMode(SplitKMode splitKMode) { mDescription.splitKMode = splitKMode; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKBuffers to specify number of buffers used in inter-CTA splits
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitKBuffers(int32_t splitKBuffers) { mDescription.splitKBuffers = splitKBuffers; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Define the length of the pointer arrays. Its default value is 0.
    /////////////////////////////////////////////////////////////////////////
    inline void setArrayCount(int64_t arrayCount) { mDescription.arrayCount = arrayCount; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief set isWgrad1x1BatchedGEMM flag. It is 0 by default
    /////////////////////////////////////////////////////////////////////////
    inline void setIsWgrad1x1BatchedGEMM(int isWgrad1x1BatchedGEMM) { mDescription.isWgrad1x1BatchedGEMM = isWgrad1x1BatchedGEMM == 1; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Define the split K without reduction flag. It is 0 by default
    /////////////////////////////////////////////////////////////////////////
    inline void setWithoutReduction(int withoutReduction) {mDescription.withoutReduction = withoutReduction == 1; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Define the number of CTAs per wave for CTA swizzling.
    /////////////////////////////////////////////////////////////////////////
    inline void setCtasPerWave(int64_t ctasPerWave) { mDescription.ctasPerWave = ctasPerWave; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Define whether enable callwlated CTAs per wave.
    /////////////////////////////////////////////////////////////////////////
    inline void setCtaSwizzle(bool ctaSwizzle) { mDescription.ctaSwizzle = ctaSwizzle; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief set isInputTriangularRight flag. It is 0 by default
    /////////////////////////////////////////////////////////////////////////
    inline void setIsInputTriangularRight(int isInputTriangularRight) { mDescription.isInputTriangularRight = isInputTriangularRight == 1; }

    using Operation::setHardwareInfo; // Bring into scope

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set hardware info
    /// @deprecated Use @ref setHardwareInfo(const HardwareInfo&)
    /////////////////////////////////////////////////////////////////////////
    inline void setHardwareInfo(int32_t multiProcessorCount, int32_t sharedMemPerMultiprocessor, int32_t registersPerMultiprocessor) {
        HardwareInformation hardwareInfo(multiProcessorCount, sharedMemPerMultiprocessor, registersPerMultiprocessor);
        this->setHardwareInfo(hardwareInfo);
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set MODS runtime parameter
    /////////////////////////////////////////////////////////////////////////
    inline void setModsRuntimeParams(uint32_t periodMaskNs, uint32_t pulseNs, uint32_t minSleep, uint32_t mainloopCount) {
        mDescription.modsRuntimeParams.periodMaskNs = periodMaskNs;
        mDescription.modsRuntimeParams.pulseNs = pulseNs;
        mDescription.modsRuntimeParams.minSleep = minSleep;
        mDescription.modsRuntimeParams.mainloopCount = mainloopCount;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the bias flag to add the bias.
    /////////////////////////////////////////////////////////////////////////
    inline void setBiasFlag(bool addBias) { mDescription.addBias = addBias; }
    //////////////////////////////////////////////////////////////////////////
    /// \brief Set the stride of bias of contiguous batches.
    //////////////////////////////////////////////////////////////////////////
    inline void setBatchBiasStride(int64_t stride) { mDescription.batchBiasStride = stride; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the ReLU flag to apply ReLU.
    ///    0 (eReLuDisable):              no ReLu, users don't need set any relu value.
    ///    1 (eReLuScalar):               scalar ReLu, users need specify upperBound if there is.
    /////////////////////////////////////////////////////////////////////////
    inline void setReLuFlag(int32_t applyReLu) { mDescription.applyReLu = applyReLu; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the ReLU upperBound.
    ///
    /// Set the ReLU upperBound.
    /// The value of upperBound should not ecxceed the maximum value of INT8/UINT8.
    /// The value of upperBound will set to the maximum value of its type (INT8/UINT8)
    /// when upperBound is negtive or not set.
    /////////////////////////////////////////////////////////////////////////
    inline void setReLuUpperBound(float reLuUpperBound) { mDescription.reLuUpperBound = reLuUpperBound; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the threshold ReLu.
    /////////////////////////////////////////////////////////////////////////
    inline void setReLuThreshold(float threshold) { mDescription.reLuThreshold = threshold; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set the per channel scaling option.
    ///
    /// Set whether alpha/beta are vectors of length C (as opposed to
    /// scalar, which is the default).  If alpha is a vector then it
    /// is a device buffer specified in the ColwolutionRunConfig.
    //////////////////////////////////////////////////////////////////////////
    inline void setPerChannelScaling(bool perChannelScaling) {
        mDescription.perChannelScaling = perChannelScaling;
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set the stride of scaling params of contiguous batches.
    ///
    /// If scaling stride is 0, the scalar or vector alpha/beta will be
    /// broadcast to all batches, otherwise each batch has different alpha/beta.
    //////////////////////////////////////////////////////////////////////////
    inline void setBatchScalingStride(int64_t stride) {
        mDescription.batchScalingStride = stride;
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set to pass alpha and beta by device pointer.
    ///
    /// Set to true to pass alpha and beta through valpha/vbeta pointers.
    /// This is as opposed to setting alpha and beta with a value from the
    /// host. This is not needed to be set if usinng valpha/vbeta as vectors.
    //////////////////////////////////////////////////////////////////////////
    inline void setPointerMode(PointerModeID mode) {
        mDescription.pointerMode = mode;
    }


    /////////////////////////////////////////////////////////////////////////
    /// \brief Check tensor consistency.
    ///
    /// Checks the tensor shapes make sense together.
    ///
    /// \pre The setADesc(), setBDesc(), and setOutputDesc()
    /// methods have already been called.
    /// \returns Error::OK: if the tensor sizes, padding and strides are
    /// consistent.
    /// \returns Error::SIZE_MISMATCH if the tensor dimensions do not align.
    ///          or if the batch sizes are not equal for all tensors.
    /// \returns Error::STRIDES if A, B, or C tensors do not have a dimensions
    ///          of 2 or 3, or if they cannot be identified as row-major or
    ///          column-major.
    /// \returns Error::BATCH_SIZE if A, B and C tensors do not have the
    ///          same dimension.
    /////////////////////////////////////////////////////////////////////////
    virtual Error isConsistent() const;
};

/////////////////////////////////////////////////////////////////////////////
/// \brief The characterstics of an available gemm kernel.
///
/// GemmShader encapulaes a specific GEMM kernel.
/// There will be a different instance of this class for each variant of the
/// kernel (small, medium, large, tile sizes, etc).
/////////////////////////////////////////////////////////////////////////////
struct
GemmShader : public Shader {    // Shader (has name and handle)

    //////////////////////////////////////////////////////////////////////////
    /// \brief Constructor for GEMM kernels using KernelInfo
    ///
    /// This constructor is expected to be called by subclasses utilizing
    /// the KernelInfo datastructure, i.e., the sass-generator concrete
    /// class, `GemmImpl`.
    /// All constructors add themselves to the global `ShaderList`.
    /////////////////////////////////////////////////////////////////////////
    GemmShader(const KernelInfo* ki);

    //////////////////////////////////////////////////////////////////////////
    /// \brief Constructor for GEMM kernels not using ShaderParams.
    ///
    /// This constructor is expected to be called by subclasses that do not
    /// need ShaderParam metadata, i.e., `LwtlassGemmImpl`.
    /// All constructors add themselves to the global `ShaderList`.
    /////////////////////////////////////////////////////////////////////////
    GemmShader(const std::string _name);


    /////////////////////////////////////////////////////////////////////////
    /// \brief Destructor. No-op.
    ///
    /// GemmShader does not allocate or own any resources, therefore
    /// the destructor does nothing.
    /////////////////////////////////////////////////////////////////////////
    virtual ~GemmShader() { };

    //////////////////////////////////////////////////////////////////////////
    /// \brief Unload a linkable shader.
    ///
    /// This only works for linkable shader.
    //////////////////////////////////////////////////////////////////////////
    virtual void destroy();

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the kernel info instance
    //////////////////////////////////////////////////////////////////////////
    virtual const GemmKernelInfo* getInfo() const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set up lwca module and kernel pointer
    //////////////////////////////////////////////////////////////////////////
    virtual void deployRuntimeInfo(const char* name, const char* lwbin);

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set up runtime parameters
    //////////////////////////////////////////////////////////////////////////
    virtual Error setRuntimeParam(RunInfo &ri, const void* data, uint32_t size);
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the specific `platformError_t` value associated with a
    ///        Error::PLATFORM return code. Typically this is lwdaError_t.
    ///
    /// If a CASK call results in a LWCA error, it is captured and
    /// CASK will return Error:PLATFORM. Calling `lastPlatformError` will then
    /// return the specific LWCA error for debugging and reporting purposes.
    /// \pre a CASK call by this class that returns `Error::PLATFORM`
    /// \returns a `lwdaError_t` value captured by CASK that caused a
    ///          `Error::PLATFORM` return code.
    /////////////////////////////////////////////////////////////////////////
    virtual platformError_t lastPlatformError(const serializableHostBuf hostBuf) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the chip enum value for this kernel.
    ///
    /// Return the chip enum value for this kernel.
    /// \returns enum value for the supported architelwre of the kernel.
    /////////////////////////////////////////////////////////////////////////
    virtual GemmChip chip() const;


    //////////////////////////////////////////////////////////////////////////
    /// \brief Return if the Shader follows the "FulyConnected" flow
    ///
    /// GEMMs implementing ReLu and bias return true
    //////////////////////////////////////////////////////////////////////////
    virtual bool isFullyConnected() const = 0;


    //////////////////////////////////////////////////////////////////////////
    /// \brief True for kernels using imma8816 opcodes.
    //////////////////////////////////////////////////////////////////////////
    inline bool isImma8816() const {
        return std::string(kernelInfo->groupName()) == "i8816gemm";
    }


    //////////////////////////////////////////////////////////////////////////
    /// \brief True for kernels using imma16832 opcodes.
    //////////////////////////////////////////////////////////////////////////
    inline bool isImma16832() const {
        return std::string(kernelInfo->groupName()) == "i16832gemm";
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief True for kernels using sparse hmma16832 opcodes.
    //////////////////////////////////////////////////////////////////////////
    inline bool isHmmaSp16832() const {
        return (std::string(kernelInfo->groupName()) == "s16832gemm" &&
                kernelInfo->typeAm() == sass_private::R_16U);
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief True for kernels using sparse Imma16864 opcodes.
    //////////////////////////////////////////////////////////////////////////
    inline bool isImmaSp16864() const {
        return ((std::string(kernelInfo->groupName()) == "i16864gemm" &&
                kernelInfo->typeAm() == sass_private::R_16U) || 
                //FIXME
                (this->name.find("sptensor_igmma") != std::string::npos));
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief True for sparse kernels
    //////////////////////////////////////////////////////////////////////////
    inline bool isSparse() const {
        return getInfo()->optional().getMetadata<SP_GRANULARITY>(md::MmaInstrSpRatio::INSTR_R_1_1)
                   != md::MmaInstrSpRatio::INSTR_R_1_1;
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief layout of Output
    //////////////////////////////////////////////////////////////////////////
    inline GemmLayout outputLayout() const {
        if (getInfo()->dLayout() == md::MatrixLayoutType::T) {
            return GemmLayout::T;
        }
        else {
            return GemmLayout::N;
        }
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the number of slices per row for sliced kernels.
    ///
    /// Typically if a kernel name includes "sliced[X]x[Y]", this function
    /// should return [Y].
    /// \returns 1 for non-sliced kernels
    /// \returns The number of slices per row for sliced kernels.
    /////////////////////////////////////////////////////////////////////////
    virtual int           getSlicesPerRow() const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the number of slices per column for sliced kernels.
    ///
    /// Typically if a kernel name includes "sliced[X]x[Y]", this function
    /// should return [X].
    /// \returns 1 for non-sliced kernels
    /// \returns The number of slices per column for sliced kernels.
    /////////////////////////////////////////////////////////////////////////
    virtual int           getSlicesPerCol() const;


    /////////////////////////////////////////////////////////////////////////
    /// \brief return the kernel version (modular or non-modular for SASS
    ///        kernels).
    ///
    /// Returns if the kernels is listed as v0 (non-modular) or v1
    /// (modular).
    ///
    /// \returns 0 for a v0 (non-modular) kernel
    /// \returns 1 for a v1 (modular) kernel
    /////////////////////////////////////////////////////////////////////////
    inline int  getVersion() const {
        return ((this->name.find("_v1") != std::string::npos) ? 1 : 0);
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief returns a usable gemm under the constraits of compute capability.
    ///
    /// \pre Required that the user has called gemm.canImplement() and
    ///  gotten error.
    ///
    /// GEMM operation may get failure from canImplement() for not satisfying
    /// the constraits of compute capability rather than shader.
    /// newOp will copy the input operation and correct the shapes of
    /// inputA and inputB tensors.
    /// NOTE:
    /// newOp cannot point to the input operation.
    /// Now `getUsableGemm` only corrects the M, N, K and batch size to the maximum value
    /// satisfies the constraits of compute capability.
    /// \returns A useable gemm that satisfies the compute capability.
    ///
    /////////////////////////////////////////////////////////////////////////
    virtual Error getUsableGemm(const Gemm& operation, Gemm* newOp,
                                const ComputeCapabilityVersion v) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns true of the shader can implement the requested
    ///        operation
    ///
    /// Can this shader implement the requested operation? Returns true
    /// if this `GemmShader` and input `Gemm` are compatable under a
    /// compute capability version.
    /// The compute capability version is guessed from shader arch.
    /// \returns Error::OK if the implementation can handle the request
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code if the Shader cannot compute the requested
    ///          Gemm.
    /////////////////////////////////////////////////////////////////////////
    virtual Error canImplement(const Gemm& operation) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns true of the shader can implement the requested
    ///        operation under a compute capability version.
    ///
    /// Can this shader implement the requested operation? Returns true
    /// if this `GemmShader` and input `Gemm` are compatable under a
    /// compute capability version.
    /// \returns Error::OK if the implementation can handle the request
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code if the Shader cannot compute the requested
    ///          Gemm.
    /////////////////////////////////////////////////////////////////////////
    virtual Error canImplement(const Gemm& operation,
                               const ComputeCapabilityVersion ccv) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the required buffer size for host data.
    ///
    /// Callwlate the reserved space buffer sizes (in bytes) needed by host.
    /// May be expensive operation, runs on host only.
    ///
    /// \pre Object must be in state such that isConsistent() would return
    ///      true.
    /// \post hostBufSize and deviceBufSize have been initialized with sizes
    /// appropriate for actually running the operation given current object
    /// state.
    /// \param[in]  operation The Gemm problem to be run.
    /// \param[out] hostBufSize A pointer to a size_t location that holds
    ///             the number of bytes required by this GemmShader on the
    ///             host.
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    getHostReservedSize(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Init host reserved space data.
    ///
    /// Does (subclass-specific) work to initialize the
    /// host data. Runs on host only. May be an expensive operation.
    /// \pre hostBufSize is the same as the value returned by
    ///      getHostReservedSize()
    /// \pre hostBuf is a host-side buffer of size at least hostBufSize
    /// \post hostBuf is filled in with the appropriate data for launching
    ///       the operation.
    /// \post hostBuf is guaranteed to be serializable/deserializable
    ///       (i.e., it contains no pointers or values that would change
    ///        from run-to-run)
    /// \param[in] operation The Gemm problem to be run
    /// \param[in] hostBufSize The size of hostBuf in bytes. Must be
    ///            at least as large as the value returned by
    ///            `getHostReservedSize()`
    /// \param[out] hostBuf A writeable buffer to be used by this GemmShader
    ///             as a serializable opaque host data store.
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return codes.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    initHostReservedSpace(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Callwlate the reserved space buffer sizes (in bytes) needed by
    ///        device.
    ///
    /// May be expensive operation, runs on _host_ only.
    ///
    /// \pre hostBufSize is the same as the value returned by getHostReservedSize()
    /// \pre hostBuf has been initialized either by calling
    ///  initHostReservedSpace(), or by deserializing a buffer that was
    ///  originally created by initHostReservedSpace in the same version of
    ///  this library.  (i.e. cask::getInternalVersion() must return the same
    ///  value it returned when initHostReservedSpace() was originally called.)
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return codes.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    getDeviceReservedSize(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Init device reserved space data.
    ///
    /// Init the deviceBuf with kernel-specific data. This may be an
    /// expensive operation.  This function may ilwoke auxillery kernels on
    //  the device, using the given lwdaStream.
    ///
    /// \pre deviceBufSize is the same as the value returned by
    ///      getDeviceReservedSize().
    /// \pre deviceBuf is an allocated device-side buffer of size at least
    ///      deviceBufSize
    /// \pre hostBufSize is the same as the value returned by
    ///      getHostReservedSize()
    /// \pre hostBuf is previously initialized by initHostReservedSpace
    /// \post deviceBuf is filled in with the appropriate data for launching
    ///       the operation.
    /// \post deviceBuf is guaranteed to be serializable/deserializable (i.e.,
    /// it contains no pointers or values that would change from run-to-run).
    /// Thus rather than running this routine every time, you are permitted to
    /// run it once, copy the data down to the host, serialize it, then later
    /// deserialize it, copy it to the device, and move immediately to calling
    /// run() without recalling this routine.
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return codes.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    initDeviceReservedSpace(RunInfo& ri,
                            lwdaStream_t          lwdaStream) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Callwlate the device workspace buffer sizes (in bytes).
    ///
    /// Device Workspace holds the helper data for kernel launch.
    /// 
    /// The difference between reserved space and workspace is that
    /// reserved space data are read only in kernels' exelwtion and clients
    /// are supposed to serialize and deserialize the buffer, while 
    /// workspace holds writable data during kernels' exelwtion and clients
    /// do not need to serialize and deserialize the buffer.
    /// 
    /// Device workspace data needs to be initialized before kernel launch
    ///
    /// \pre hostBufSize is the same as the value returned by getHostReservedSize()
    /// \pre hostBuf has been initialized either by calling
    ///  initHostReservedSpace(), or by deserializing a buffer that was
    ///  originally created by initHostReservedSpace in the same version of
    ///  this library.  (i.e. cask::getInternalVersion() must return the same
    ///  value it returned when initHostReservedSpace() was originally called.)
    ///
    /// \returns Device workspace size.
    ///
    /////////////////////////////////////////////////////////////////////////
    virtual int64_t
    getDeviceWorkspaceSize(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Update runtime parameters before calling run().
    ///
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code.
    /////////////////////////////////////////////////////////////////////////
    virtual
    Error
    updateRuntimeParameters(RunInfo& ri,
        platformStream_t lwdaStream);

    /////////////////////////////////////////////////////////////////////////
    /// \brief Callwlate the batch size after pedded.
    ///  For the shader that support viariable-sized batch size,
    ///  the batch size must a multiple of 8, 6 or other number according
    ///  to the shders' requirement.
    ///
    /// \pre hostBuf has been initialized either by calling
    ///  initHostReservedSpace(), or by deserializing a buffer that was
    ///  originally created by initHostReservedSpace in the same version of
    ///  this library.
    ///
    /// \returns the batch size after padded.
    /// NOTE: paddedBatchSize() should be called before passing batchSize
    /// to run().
    /////////////////////////////////////////////////////////////////////////

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
    virtual
    dim3
    getGridSize(RunInfo& ri) const;

    virtual
    size_t
    paddedBatchSize(size_t                hostBufSize,
                    serializableHostBuf   hostBuf,
                    size_t                batchSize) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Run the kernel on the device.
    ///
    /// run() is the main entrypoint for ilwoking the kernel.
    ///
    /// A, B, C, and Output are the device buffers required to perform the
    /// matrix multiply.
    ///
    /// The matrices must be *element* aligned.  The shapes of the tensors will
    /// be given elsewhere (to the Gemm class).
    ///
    /// Operations are of the form
    /// \f$Z \leftarrow \alpha (A \times B) + \beta C\f$ based on the values
    /// of A, B, C, \f$ \alpha \f$, and \f$ \beta \f$ given by the gemm
    /// parameter.
    ///
    /// \param[in] gemm `Gemm` object describing the input and output
    ///            tensors.
    /// \param[in] deviceBufSize the size of deviceBuf in bytes. Must be
    ///            equal to the value returned by getDeviceReservedSize().
    /// \param[in] deviceBuf pointer to an opaque data structure used by
    ///            this GemmShader on the device. Can be constructed from
    ///            a serialized version using the same version of CASK.
    /// \param[in] hostBufSize the size of hostBuf in bytes. Must be equal
    ///            to the value returned by getHostReservedSize().
    /// \param[in] hostBuf pointer to an opaque data structure used by this
    ///            GemmShader on the host. Can be constructed from a
    ///            serialized version using the same version of CASK.
    /// \param[out] deviceOutput a device pointer to the data of the
    ///             aclwmulator C matrix.
    /// \param[in] deviceA A device pointer to the data of the A input
    ///            matrix.
    /// \param[in] deivceB A device pointer to the data of the B input
    ///            matrix.
    /// \param[in] lwdaStream A `lwdaStream_t` object to execute the kernel
    ///            on. This must be the same stream as is used to call
    ///            initDeviceReservedSpace().
    /// \pre deviceBuf is allocated and initialized on the device by a previous
    ///      call to initDeviceReservedSpace() in a version of the library
    ///       such that cask::getInternalVersion() returns the same value.
    /// \pre hostBuf is allocated and initialized on the device by a previous
    ///      call to initHostReservedSpace() in a version of the library
    ///       such that cask::getInternalVersion() returns the same value.
    /// \pre deviceA deviceB and deviceOutput are allocated and populated on
    ///      the device.
    /// \post deviceOutput contains the resulting tensor with the GEMM
    ///       operation as described by the `gemm` object.
    /// \returns Error::OK on success.
    /// \returns Error::PLATFORM on an error in the LWCA call.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return codes.
    ///
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    run(RunInfo& ri,
        void* deviceWorkspacePointer,
        void* deviceOutputPointer,
        const void*  deviceAPointer,
        const void*  deviceBPointer,
        const void*  deviceCPointer,
        const void*  deviceBiasPointer,
        const void*  deviceAlphaPointer,
        const void*  deviceBetaPointer,
        platformStream_t lwdaStream) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Overloaded run() with a additional metadata pointer.
    ///
    /// The implementation is similar to the original run().
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    run(RunInfo& ri,
        void* deviceWorkspacePointer,
        void* deviceOutputPointer,
        const void*  deviceAPointer,
        const void*  deviceAmPointer,
        const void*  deviceBPointer,
        const void*  deviceCPointer,
        const void*  deviceBiasPointer,
        const void*  deviceAlphaPointer,
        const void*  deviceBetaPointer,
        platformStream_t lwdaStream) = 0;

#if defined(ENABLE_FUNCTIONAL_TREE)
    virtual Error getKernelRecord(kernel_record_t * kr) const {
      return Error::FUNCTIONAL_TREE;
    }
#endif

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed fitler, perform any necessary transformation
    ///        operation, populating deviceTransformedTensor
    ///
    /// In typical usage, CASK manages the tensor within the
    /// `hostBuf` blob. However, if the client chooses to manage
    /// the weights and calls the variant of `run()` that explicitly
    /// passes the weights and bias device pointers, the client must ensure
    /// that the weights has undergone any necessary transformations.
    /// This call writes the necessary transformed weights into
    /// deviceTransformedTensor.
    ///
    /// \param[in] deviceRawTensor the original input tensor
    ///
    /// \param[out] deviceTransformedTensor the output tensor to be
    ///             passed to run(), containing all necessary
    ///             transformations.
    ///
    /// \returns Error::OK if the transformed filter is written
    /// \returns Error::FILTER_SIZE if called when doesn't match
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformATensor(RunInfo& ri,
                     const void*           deviceRawTensor,
                     void*                 deviceTransformedTensor,
                     platformStream_t      platformStream) {
        return Error::FILTER_SIZE;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed tensor.
    ///
    /// Returns the size in bytes of the transformed tensor.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformATensor.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedATensorSize(RunInfo& ri) const {
        return 0;
    }

    /////////////////////////////////////////////////////////////////////////
    /// Returns the size in bytes of the transformed B tensor.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformBTensor.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedBTensorSize(RunInfo& ri) const {
        return 0;
    };

    /////////////////////////////////////////////////////////////////////////
    /// Transforms tensor B.
    ///
    /// \returns Return Error::OK if the transform succeed.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformBTensor(RunInfo&         ri,
                     const void*      deviceRawTensor,
                     void*            deviceTransformedTensor,
                     platformStream_t platformStream) {
        return Error::FILTER_SIZE;
    };

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed bias, perform any necessary transformation
    ///        operation, populating deviceTransformedTensor
    ///
    /// In typical usage, CASK manages the bias within the
    /// `hostBuf` blob. However, if the client chooses to manage
    /// the bias and calls the variant of `run()` that explicitly
    /// passes the filter and bias device pointers, the client must ensure
    /// that the filter has undergone any necessary transformations.
    /// This call writes the necessary transformed bias into
    /// deviceTransformedBias.
    ///
    /// \param[in] deviceBias the original input tensor
    ///
    /// \param[out] deviceTransformedBias the output tensor to be
    ///             passed to run(), containing all necessary
    ///             transformations.
    ///
    /// \returns Error::OK if the transformed filter is written
    /// \returns Error::FILTER_SIZE if called when doesn't match
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformBias(RunInfo& ri,
                  const void*           deviceRawTensor,
                     void*            deviceTransformedTensor,
                     platformStream_t platformStream) {
        return Error::FILTER_SIZE;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed bias.
    ///
    /// Returns the size in bytes of the transformed bias.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformBias.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedBiasSize(RunInfo& ri) const {
        return 0;
    };

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed alpha tensor, perform any necessary
    ///        transformation operation, populating deviceTransformedTensor
    ///
    /// \param[in] deviceFilterTensor the original input tensor
    ///
    /// \param[out] deviceTransformedTensor the output tensor to be
    ///             passed to run(), containing all necessary
    ///             transformations.
    ///
    /// \returns Error::OK if the transformed tensor is written
    /// \returns Error::FILTER_SIZE if called when transformedFilterSize()
    ///          returns 0.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformAlpha(RunInfo& ri,
                     const void*           deviceRawTensor,
                     void*                 deviceTransformedTensor,
                     platformStream_t      platformStream) {
        return Error::FILTER_SIZE;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed alpha.
    ///
    /// Returns the size in bytes of the transformed alpha.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformAlpha.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedAlphaSize(RunInfo& ri) const {
        return 0;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed beta tensor, perform any necessary
    ///        transformation operation, populating deviceTransformedTensor
    ///
    /// \param[in] deviceFilterTensor the original input tensor
    ///
    /// \param[out] deviceTransformedTensor the output tensor to be
    ///             passed to run(), containing all necessary
    ///             transformations.
    ///
    /// \returns Error::OK if the transformed tensor is written
    /// \returns Error::FILTER_SIZE if called when transformedFilterSize()
    ///          returns 0.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformBeta(RunInfo& ri,
                  const void*           deviceRawTensor,
                  void*                 deviceTransformedTensor,
                  platformStream_t      platformStream) {
        return Error::FILTER_SIZE;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed beta.
    ///
    /// Returns the size in bytes of the transformed beta.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformBeta.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedBetaSize(RunInfo& ri) const {
        return 0;
    }

    /////////////////////////////////////////////////////////////////////////
    /// Returns the size in bytes of buffer containing SmIDs.
    ///
    /// \returns 0 if dumping SmIDs is not supported by the impl of GemmShader
    /////////////////////////////////////////////////////////////////////////
    virtual size_t getSMidSize(RunInfo& ri) {
        return 0;
    }

    /////////////////////////////////////////////////////////////////////////
    /// Returns buffer containing SmIDs
    ///
    /// \returns Error::INTERNAL if dumping SmIDs is not supported by the
    /// impl of GemmShader
    /////////////////////////////////////////////////////////////////////////
    virtual Error populateSMid(RunInfo& ri,
                               void * SMidMem,
                               size_t size) {
        return Error::INTERNAL;
    }

    virtual int64_t getSerializedSize() const ;

    virtual Error serialize(uint8_t* buffer, int64_t size)const ;

    static GemmShader* deserialize(const uint8_t* buffer, int64_t size);
};

} // namespace cask

#endif // INCLUDE_GUARD_CASK_GEMM_H
