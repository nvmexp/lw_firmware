#ifndef INCLUDE_GUARD_CASK_WGRAD_H
#define INCLUDE_GUARD_CASK_WGRAD_H

// enable doxygen:
//! @file

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/wgrad.h> directly; include <cask.h> instead."
#endif

#include <cassert>

///////////////////////////////////////////////////////////////////////////////

namespace cask {

///////////////////////////////////////////////////////////////////////////////

struct ColwWgradShader;
class ColwolutionWgrad;

// the real entry point is ColwWgradShaderList::availableShaders()
typedef
ShaderList<ColwWgradShader,ColwolutionWgrad>
ColwWgradShaderList;

///////////////////////////////////////////////////////////////////////////////

/// \brief Describes the characteristics of the wgrad (weight gradient)
/// operation you want to run (alpha, beta, tensor shapes).  It does not choose
/// which algorithm (kernel/shader) implements the operation.
///
/// The forward colwolution operation is of the form \f$Z \leftarrow \alpha (A
/// * B) + \beta C + \mathrm{bias}\f$, where \f$A\f$ is the input activations,
/// \f$B\f$ is the weights, \f$C\f$ is a tensor of the same size and shape as
/// the output \f$Z\f$, and \f$\mathrm{bias}\f$ is a bias vector of the same
/// length as the number of channels in the output tensor.  The weight gradient
/// is the derivative of an error function (on the output) with respect to the
/// weights \f$B\f$.
class
ColwolutionWgrad : public Operation {
public:
    typedef cask::Error Error;  // for existing code relying on ColwolutionWgrad::Error


    ColwolutionWgrad();
    ~ColwolutionWgrad() { }

    /////////////////////////////////////////////////////////////////////////
    /// \brief returns the ColwolutionWgrad description.
    /////////////////////////////////////////////////////////////////////////
    inline const Description& getDescription() const { return mDescription; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Check tensor consistency.
    ///
    /// Checks the tensor shapes, and the padding and strides, make sense
    /// together.  Specifically: input and output N have to match.  Filter C
    /// must match Input C.  Filter K must match Output C.  Given input H
    /// (respectively W), output P (respectively Q), and filter R (respectively
    /// S), stride, and padding then we check that P =
    /// ceil((H-R+2*pad+1)/stride)
    ///
    /// \pre The setFilterDesc(), setInputDesc(), and setOutputDesc() methods
    /// have already been called.
    /// \returns Error::OK: if the tensor sizes, padding and strides are
    /// consistent.
    /// \returns Error::???: otherwise.
    /////////////////////////////////////////////////////////////////////////
    virtual Error isConsistent() const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(float alpha) { mDescription.alpha = (double) alpha; }
    inline void setAlpha(double alpha) { mDescription.alpha = alpha; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(float beta) { mDescription.beta = (double) beta; }
    inline void setBeta(double beta) { mDescription.beta = beta; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the DY TensorDesc to Description::inputADesc. 
    /////////////////////////////////////////////////////////////////////////
    inline void setDYDesc(const TensorDesc &dYDesc) {
        mDescription.inputADesc = dYDesc;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the X TensorDesc to Description::inputBDesc. 
    /////////////////////////////////////////////////////////////////////////
    inline void setXDesc(const TensorDesc &xDesc) {
        mDescription.inputBDesc = xDesc;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the DW TensorDesc to Description::outputDesc. 
    /////////////////////////////////////////////////////////////////////////
    inline void setDWDesc(const TensorDesc &dWDesc) {
        mDescription.outputDesc = dWDesc;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the bias flag to add the bias.
    /////////////////////////////////////////////////////////////////////////
    inline void setBiasFlag(bool addBias) { mDescription.addBias = addBias; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar type of the bias.
    /////////////////////////////////////////////////////////////////////////
    inline void setBiasType(ScalarType sT) { mDescription.biasType = sT; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution vertical zero padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadH(int64_t padH) {
        mDescription.colwolutionPadTop = padH; 
        mDescription.colwolutionPadBottom = padH; 
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution horizontal zero padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadW(int64_t padW) {
        mDescription.colwolutionPadLeft = padW; 
        mDescription.colwolutionPadRight = padW; 
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution depth zero padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadD(int64_t padD) {
        mDescription.colwolutionPadFront = padD; 
        mDescription.colwolutionPadBack = padD; 
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Top padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadTop(int64_t padTop) { mDescription.colwolutionPadTop = padTop; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Bottom padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadBottom(int64_t padBottom) { mDescription.colwolutionPadBottom = padBottom; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Left padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadLeft(int64_t padLeft) { mDescription.colwolutionPadLeft = padLeft; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Right padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadRight(int64_t padRight) { mDescription.colwolutionPadRight = padRight; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Front padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadFront(int64_t padFront) { mDescription.colwolutionPadFront = padFront; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Back padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadBack(int64_t padBack) { mDescription.colwolutionPadBack = padBack; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution vertical dilation.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionDilationH(int64_t colwDilationH) { mDescription.colwolutionDilationH = colwDilationH; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution horizontal dilation.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionDilationW(int64_t colwDilationW) { mDescription.colwolutionDilationW = colwDilationW; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution depth dilation.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionDilationD(int64_t colwDilationD) { mDescription.colwolutionDilationD = colwDilationD; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution vertical striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionStrideH(int64_t strideH) {
        mDescription.colwolutionStrideH = strideH;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution horizontal striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionStrideW(int64_t strideW) {
        mDescription.colwolutionStrideW = strideW;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution depth striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionStrideD(int64_t strideD) {
        mDescription.colwolutionStrideD = strideD;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution to be a cross-correlation.
    /////////////////////////////////////////////////////////////////////////
    inline void setCrossCorrelationFlag(bool flag) {
        mDescription.isCrossCorrelation = flag;
    }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the num of groups for the colwolution
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionGroups(int64_t colwGroups) { mDescription.colwGroups = colwGroups; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the ReLU flag to apply ReLU.
    /////////////////////////////////////////////////////////////////////////
    inline void setReLuFlag(int32_t applyReLu) { mDescription.applyReLu = applyReLu; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the number of splits on k dimension.  It is 1 by default.
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitK(int32_t splitK) { mDescription.splitK = splitK; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the number of splits on h dimension.  It is 1 by default.
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitH(int32_t splitH) { mDescription.splitH = splitH; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKMode to specify number of kernels to call for splitk
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitKMode(SplitKMode splitKMode) { mDescription.splitKMode = splitKMode; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKBuffers to specify number of buffers used in inter-CTA splits
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitKBuffers(int32_t splitKBuffers) { mDescription.splitKBuffers = splitKBuffers; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKT to true if the kernel splits in filter T dimension
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitKT(bool splitKT) { mDescription.splitKT = splitKT; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKR to true if the kernel splits in filter R dimension
    /////////////////////////////////////////////////////////////////////////
    inline void setSplitKR(bool splitKR) { mDescription.splitKR = splitKR; }

    using Operation::setHardwareInfo; // Bring into scope

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set hardware info
    /// @deprecated Use @ref setHardwareInfo(const HardwareInfo&)
    /////////////////////////////////////////////////////////////////////////
    inline void setHardwareInfo(int32_t multiProcessorCount, int32_t sharedMemPerMultiprocessor, int32_t registersPerMultiprocessor) {
        HardwareInformation hardwareInfo(multiProcessorCount, sharedMemPerMultiprocessor, registersPerMultiprocessor);
        this->setHardwareInfo(hardwareInfo);
    }
};

///////////////////////////////////////////////////////////////////////////////
/// \brief The characterstics of an available colwolution weight gradient kernel.
///////////////////////////////////////////////////////////////////////////////

struct
ColwWgradShader : public Shader { // Shader (has name and hash)
  struct TraitsEnum {
    enum Label {
      Unknown,
      Fp32,
      Fp16,
      Fp16ToFp16,
      Fp16ToFp32,
      Bf16ToFp32
    };
  };
  typedef SafeEnum<TraitsEnum> Traits;
  static Traits  getTraits(const std::string shaderName);
const Traits traits;
 public:

    //////////////////////////////////////////////////////////////////////////
    /// \brief ColwolutionWgrad Shader Constructor.
    ///
    /// This constructor should not be called directly by clients.
    /// Client code should find kernels by calling
    /// `ColwWgradShaderList::availableShaders()`.
    //////////////////////////////////////////////////////////////////////////
    ColwWgradShader(const KernelInfo* ki);

    //////////////////////////////////////////////////////////////////////////
    /// \brief Destructor.
    ///
    /// Trivial destructor. Shaders do not allocate or own any resources.
    //////////////////////////////////////////////////////////////////////////
    virtual ~ColwWgradShader();

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the kernel info instance
    //////////////////////////////////////////////////////////////////////////
    virtual const ColwWgradKernelInfo* getInfo() const;

    // The necessary information about what this kernel does
    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the `GemmChip` enumerant supported by this kernel.
    //////////////////////////////////////////////////////////////////////////
    virtual GemmChip chip() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief returns true for kernels that expect NHWC input format.
    //////////////////////////////////////////////////////////////////////////
    virtual bool isNhwc() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Return true for kernels that use NCHW input/output formats
    //////////////////////////////////////////////////////////////////////////
    inline virtual bool isNchw() const { return (!isNhwc()); }
    //////////////////////////////////////////////////////////////////////////
    /// \brief returns true for kernels that expect NHWC output format.
    //////////////////////////////////////////////////////////////////////////
    virtual bool isNhwcOutput() const {
      return isNhwc();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the DY tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType dYScalarType() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the X tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType xScalarType() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the DW tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType dWScalarType() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the bias.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType biasType() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the alpha and beta in epilog.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType epilogType() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the internal aclwmulation
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType aclwmScalarType() const {
      assert(false && "aclwmScalarType default implementation called!");
      return ScalarType::FP32;
    }
    //////////////////////////////////////////////////////////////////////////
    /// \brief Number of (packed) scalar values in a element on the DY tensor
    //////////////////////////////////////////////////////////////////////////
    virtual int dYScalarsPerElement() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Number of (packed) scalar values in a element on the X tensor
    //////////////////////////////////////////////////////////////////////////
    virtual int xScalarsPerElement() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Number of (packed) scalar values in a element on the DW tensor
    //////////////////////////////////////////////////////////////////////////
    virtual int dWScalarsPerElement() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Vectorized dimension of the DY tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual int dYVectorizedDim() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Vectorized dimension of the X tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual int xVectorizedDim() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Vectorized dimension of the DW tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual int dWVectorizedDim() const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the specific `lwdaError_t` value associated with a
    ///        Error::PLATFORM return code.
    ///
    /// If a CASK call results in a LWCA error, it is captured and
    /// CASK will return Error:LWCA. Calling `lastLwdaError` will then
    /// return the specific LWCA error for debugging and reporting purposes.
    /// \pre a CASK call by this class that returns `Error::PLATFORM`
    /// \returns a `platformError_t` value captured by CASK that caused a
    ///          `Error::PLATFORM` return code.
    //////////////////////////////////////////////////////////////////////////
    virtual platformError_t lastPlatformError(const serializableHostBuf hostBuf) const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// Can this shader implement the requested operation?
    /// \returns Error::OK if the implementation can handle the request.
    /// Subclasses may return any other enumerated Error return code if the
    /// Shader cannot compute the requested ColwolutionWgrad.
    //////////////////////////////////////////////////////////////////////////
    virtual Error         canImplement(const ColwolutionWgrad& colw) const = 0;
    virtual Error         canImplement(const ColwolutionWgrad& colw,
                                       const ComputeCapabilityVersion ccv) const = 0; 

    
    //////////////////////////////////////////////////////////////////////////
    /// \brief returns a usable filter tensor shape for the shader.
    ///
    /// \pre Required that the user has called colw.isConsistent() and
    ///      gotten Error::OK
    ///
    /// What is the required tensorDesc shape for the filter given the
    /// Colwolution request?  The newShape argument will be filled with the
    /// correct shape.  Returns: Error::OK if a tensor reshuffle is required,
    /// and Error::FILTER_OK if a tensor reshuffle is not required (in which
    /// case newShape will just contain a copy of
    /// colw.getDescription().inputBDesc.)  Error::FILTER_FMT will be returned
    /// when a reshuffle is too diffilwlt (for example if you already simdized
    /// the filter tensor, but not in the way required by this shader), in
    /// which case the contents of newShape are undefined.
    ///
    /// If needed you can reshuffle the filter data on the host with
    ///
    ///     cask::transformTensor(destData, srcData,
    ///                           colw.getDescription().inputBDesc,
    ///                           newShape);
    ///
    /// and then the Colwolution object should be fixed by calling
    ///
    ///     colw.setFilterDesc(newShape);
    ///
    /// The reason we expose this to the user is that CASK APIs can't allocate
    /// memory for themselves, thus can't call cask::transformTensor() from
    /// initDeviceReservedSpace() unless the client has preallocated
    /// inference-time writable host memory for the purpose.  But that would
    /// mean figuring this out in getHostReservedSize() (which is const and
    /// before we have any writable host memory).  Thus the client has to do
    /// the transform itself.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    getRequiredFilterShape(TensorDesc* newShape,
                           const ColwolutionWgrad& oepration)
        const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return host reserved buffer size. 
    ///
    /// Callwlate the reserved space buffer sizes (in bytes) needed by host.
    /// May be expensive operation, runs on host only.
    ///
    /// \pre Object must be in state such that isConsistent() would return true
    ///
    /// \post hostBufSize and deviceBufSize have been initialized with sizes
    /// appropriate for actually running the operation given current object
    /// state
    ///
    /// \returns Error::OK on success.
    /// \returns Error::??? otherwise.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    getHostReservedSize(RunInfo& ri) const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Init host reserved space data.
    ///
    /// Init host reserved space data.  May be an expensive operation.  Runs on
    /// host only.
    ///
    /// \pre hostBufSize is the same as the value returned by getHostReservedSize()
    ///
    /// \pre hostBuf is a host-side buffer of size at least hostBufSize
    ///
    /// \post hostBuf is filled in with the appropriate data for launching the
    /// operation.
    ///
    /// \post hostBuf is guaranteed to be serializable/deserializable (i.e., it
    /// contains no pointers or values that would change from run-to-run)
    ///
    /// \returns Error::OK on success.
    /// \returns Error::??? otherwise.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    initHostReservedSpace(RunInfo& ri) const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return device reserved buffer size.
    ///
    /// Callwlate the reserved space buffer sizes (in bytes) needed by device.
    /// "Reserved" means read-only data that persists across multiple calls of
    /// the run() call.  May be expensive operation, runs on _host_ only.
    ///
    /// \pre hostBufSize is the same as the value returned by getHostReservedSize()
    /// \pre hostBuf has been initialized either by calling
    ///  initHostReservedSpace(), or by deserializing a buffer that was
    ///  originally created by initHostReservedSpace in the same version of
    ///  this library.  (i.e. cask::getInternalVersion() must return the same
    ///  value it returned when initHostReservedSpace() was originally called.)
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    getDeviceReservedSize(RunInfo& ri) const = 0;

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

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed bias tensor, perform any necessary 
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
    transformBias(RunInfo& ri,
                  const void*           devicePointer,
                  void*                 deviceTransformedPointer,
                  platformStream_t      platformStream) = 0;                  

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
    transformedBiasSize(RunInfo& ri) const = 0;

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
                     const void*           devicePointer,
                     void*                 deviceTransformedPointer,
                     platformStream_t      platformStream) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed alpha.
    ///
    /// Returns the size in bytes of the transformed alpha.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformBias.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedAlphaSize(RunInfo& ri) const = 0;

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
                     const void*           devicePointer,
                     void*                 deviceTransformedPointer,
                     platformStream_t      platformStream) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed beta.
    ///
    /// Returns the size in bytes of the transformed beta.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformBias.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedBetaSize(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed inputA tensor, perform any necessary 
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
    transformATensor(RunInfo& ri,
                     const void*           devicePointer,
                     void*                 deviceTransformedPointer,
                     platformStream_t      platformStream) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed tensor.
    ///
    /// Returns the size in bytes of the transformed inputA tensor.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformFilter.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedATensorSize(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed inputB tensor, perform any necessary 
    ///        transformation operation, populating deviceTransformedTensor
    ///
    /// In typical usage, CASK manages the filter tensor within the
    /// `hostBuf` blob. However, if the client chooses to manage
    /// the filter and calls the variant of `run()` that explicitly
    /// passes the filter and bias device pointers, the client must ensure
    /// that the filter has undergone any necessary transformations.
    /// This call writes the necessary transformed filter into
    /// deviceTransformedTensor.
    ///
    /// \param[in] deviceFilterTensor the original input tensor
    ///
    /// \param[out] deviceTransformedTensor the output tensor to be
    ///             passed to run(), containing all necessary
    ///             transformations.
    ///
    /// \returns Error::OK if the transformed filter is written
    /// \returns Error::FILTER_SIZE if called when transformedFilterSize()
    ///          returns 0.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformBTensor(RunInfo& ri,
                     const void*           devicePointer,
                     void*                 deviceTransformedPointer,
                     platformStream_t      platformStream) = 0;
    
    //////////////////////////////////////////////////////////////////////////
    /// Callwlate the temporary space buffer sizes (in bytes) needed by device
    /// during the call to initDeviceFilter.  Runs on _host_ only.
    ///
    /// \pre hostBufSize is the same as the value returned by getHostReservedSize()
    /// \pre hostBuf has been initialized either by calling
    ///  initHostReservedSpace(), or by deserializing a buffer that was
    ///  originally created by initHostReservedSpace in the same version of
    ///  this library.  (i.e. cask::getInternalVersion() must return the same
    ///  value it returned when initHostReservedSpace() was originally called.)
    //////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedBTensorSize(RunInfo& ri) const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Init device reserved space data.
    ///
    /// Init device reserved space data.  May be an expensive
    /// operation.  Runs on device.
    ///
    /// \pre deviceBufSize is the same as the value returned by getDeviceReservedSize()
    /// \pre deviceBuf is an allocated device-side buffer of size at least deviceBufSize
    /// \pre hostBufSize is the same as the value returned by getHostReservedSize()
    /// \pre hostBuf is previously initialized by initHostReservedSpace
    ///
    /// \post deviceBuf is filled in with the appropriate data for launching
    /// the operation.
    ///
    /// \post deviceBuf is guaranteed to be serializable/deserializable (i.e.,
    /// it contains no pointers or values that would change from run-to-run).
    /// Thus rather than running this routine every time, you are permitted to
    /// run it once, copy the data down to the host, serialize it, then later
    /// deserialize it, copy it to the device, and move immediately to calling
    /// run() without recalling this routine.
    ///
    /// \returns Error::OK on success.
    /// \returns Error::??? otherwise.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
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

    //////////////////////////////////////////////////////////////////////////
    /// \brief Run the kernel on the device.
    ///
    /// \pre deviceBufSize is the value returned by getDeviceReservedSize()
    /// \pre deviceTmpSize is the value returned by getDeviceRunTempSize()
    ///
    /// \pre deviceBuf is allocated and initialized on the device by a previous
    /// call to initDeviceReservedSpace() in a version of the library such that
    /// cask::getInternalVersion() returns the same value.
    ///
    /// deviceInputTensor, deviceOutputTensor, and deviceCTensor are the device
    /// buffers required to perform the colwolution operation.  The tensors
    /// must be *element* aligned.  The shapes of the tensors are given
    /// elsewhere (as part of the colw parameter).
    ///
    /// Operations are of the form \f$\mathrm{Output} \leftarrow \alpha
    /// (\mathrm{Input} * \mathrm{Filter}) + \beta C + \mathrm{bias}\f$.
    ///
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    run(RunInfo& ri, 
        void*                 deviceWorkspacePointer,
        void*                 deviceDWPointer,
        const void*           deviceDYPointer, //!< must be size NKPQ
        const void*           deviceXPointer, //!< must be size NCHW
        const void*           deviceCPointer, //!< May be same as OutputTensor
        void*                 deviceBiasPointer,
        lwdaStream_t          lwdaStream) = 0;

#if defined(ENABLE_FUNCTIONAL_TREE)
    virtual Error getKernelRecord(kernel_record_t * kr) const {
      return Error::FUNCTIONAL_TREE;
    }
#endif

    virtual int64_t getSerializedSize() const ;

    virtual Error serialize(uint8_t* buffer, int64_t size)const ;

    static ColwWgradShader* deserialize(const uint8_t* buffer, int64_t size);

};

} // namespace cask

#endif // INCLUDE_GUARD_CASK_H
