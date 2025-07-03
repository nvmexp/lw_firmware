#ifndef INCLUDE_GUARD_CASK_DGRAD_H
#define INCLUDE_GUARD_CASK_DGRAD_H

// enable doxygen:
//! @file

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/dgrad.h> directly; include <cask.h> instead."
#endif

#include <cassert>

//////////////////////////////////////////////////////////////////////////////

namespace cask {

//////////////////////////////////////////////////////////////////////////////

class ColwolutionDgrad;
struct ColwDgradShader;

typedef ShaderList<ColwDgradShader,ColwolutionDgrad> ColwDgradShaderList;

//////////////////////////////////////////////////////////////////////////////
/// \brief Describes the characteristics of the dgrad (data gradient)
/// operation you want to run (alpha, beta, tensor shapes).  It does not choose
/// which algorithm (kernel/shader) implements the operation.
//////////////////////////////////////////////////////////////////////////////

class
ColwolutionDgrad : public Operation {
public:
    /// \deprecated use cask::Error
    typedef cask::Error Error;  // for existing code relying on Colwolution::Error

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor.
    ///
    /// ColwolutionDgrad constructor takes no parameters and expects the
    /// colwolution dgrad data to be filled in via the setter methods.
    /////////////////////////////////////////////////////////////////////////
    ColwolutionDgrad(); 

    /////////////////////////////////////////////////////////////////////////
    /// \brief Dtor. Does nothing
    ///
    /// ColwolutionDgrad does not allocate or own resources, so the destructor
    /// does nothing.
    /////////////////////////////////////////////////////////////////////////
    virtual ~ColwolutionDgrad() {} 
    
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
    /// \returns Error::BATCH_SIZE if the input and output batch sizes (N)
    ///          are not equal.
    /// \returns Error::SIZE_MISMATCH if filterK  is not an even multiple
    ///          of outputK * simd
    /// \returns Error::CANT_PAD if the output sizes are not compatible
    ///          with the input sizes
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return codes.
    /////////////////////////////////////////////////////////////////////////
    virtual Error isConsistent() const;

    /////////////////////////////////////////////////////////////////////////
    /// \brief returns the colwolution dgrad description.
    /////////////////////////////////////////////////////////////////////////
    inline const Description& getDescription() const { return mDescription; }

    inline HardwareInformation const& getHardwareInformation() const {
        return mDescription.hardwareInfo;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set Description::inputADesc with dY tensor desc.
    /////////////////////////////////////////////////////////////////////////
    inline void setDYDesc(const TensorDesc &dYDesc) {
        mDescription.inputADesc = dYDesc;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set Description::inputBDesc with W tensor desc.
    /////////////////////////////////////////////////////////////////////////
    inline void setWDesc(const TensorDesc &wDesc) {
        mDescription.inputBDesc = wDesc;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set Description::outputDesc with dX tensor desc.
    /////////////////////////////////////////////////////////////////////////
    inline void setDXDesc(const TensorDesc &dXDesc) {
        mDescription.outputDesc = dXDesc;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    ///
    /// Set the scalar alpha parameter. The value may be colwerted by the
    /// specific kernel implementation.
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(float alpha) { mDescription.alpha = (double) alpha; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar alpha parameter.
    ///
    /// Set the scalar alpha parameter. Input type is double. 
    /////////////////////////////////////////////////////////////////////////
    inline void setAlpha(double alpha) { mDescription.alpha = alpha; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    ///
    /// Set the scalar beta parameter.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(float beta) { mDescription.beta = (double) beta; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the scalar beta parameter.
    ///
    /// Set the scalar beta parameter. Input type is double.
    /////////////////////////////////////////////////////////////////////////
    inline void setBeta(double beta) { mDescription.beta = beta; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set the per-channel scaling option.
    ///
    /// Set whether alpha/beta are vectors of length C (as opposed to scalar,
    /// which is the default).  If alpha is a vector then it is a device
    /// buffer specified in the ColwolutionRunConfig.
    ///
    /// \param[in] perChannelScaling if true, CASK expects alpha and beta
    ///            vectors rather than a scalar value
    //////////////////////////////////////////////////////////////////////////
    inline void setPerChannelScaling(bool perChannelScaling) {
        mDescription.perChannelScaling = perChannelScaling;
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set splitK
    ///
    /// Set the number of splits for a splitK kernel. The Shader must support
    /// split k as determined by the KernelInfo::lwDnnSplitK() function.
    /// Shaders may not support all Colwolution configurations with split k.
    /// If a configuration is not supported, canImplement will return
    /// Error::SPLIT_K.
    //////////////////////////////////////////////////////////////////////////
    inline void setSplitK(int32_t splitK) { mDescription.splitK = splitK; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKMode to specify number of kernels to call for splitk
    ///
    /// 1: ONE_KERNEL; 2: TWO_KERNELS
    //////////////////////////////////////////////////////////////////////////
    inline void setSplitKMode(SplitKMode splitKMode) { mDescription.splitKMode = splitKMode; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKBuffers to specify number of buffers used in inter-CTA splits
    ///
    /// splitKBuffers can be a factor of splitKSlices, multiple CTAs will write to the same buffer
    //////////////////////////////////////////////////////////////////////////
    inline void setSplitKBuffers(int32_t splitKBuffers) { mDescription.splitKBuffers = splitKBuffers; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKT to true if the kernel splits in filter T dimension
    ///
    /// Set splitKT. It cannot be set together with splitKR.
    //////////////////////////////////////////////////////////////////////////
    inline void setSplitKT(bool splitKT) { mDescription.splitKT = splitKT; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set splitKR to true if the kernel splits in filter R dimension
    ///
    /// Set splitKR. It cannot be set together with splitKR.
    //////////////////////////////////////////////////////////////////////////
    inline void setSplitKR(bool splitKR) { mDescription.splitKR = splitKR; }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution vertical padding.
    ///
    /// Set the colwolution vertical padding.
    //////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadH(int64_t padH) {
        mDescription.colwolutionPadTop    = padH;
        mDescription.colwolutionPadBottom = padH;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution horizontal padding.
    ///
    /// Set the colwolution horizontal padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadW(int64_t padW) {
        mDescription.colwolutionPadLeft  = padW;
        mDescription.colwolutionPadRight = padW;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution depth padding.
    ///
    /// Set the colwolution depth padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadD(int64_t padD) {
        mDescription.colwolutionPadFront  = padD;
        mDescription.colwolutionPadBack = padD;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Top padding.
    ///
    /// Set the colwolution Top padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadTop(int64_t padTop) {
        mDescription.colwolutionPadTop   = padTop;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Bottom padding.
    ///
    /// Set the colwolution Bottom padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadBottom(int64_t padBottom) {
        mDescription.colwolutionPadBottom = padBottom;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Left padding.
    ///
    /// Set the colwolution Left padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadLeft(int64_t padLeft) {
        mDescription.colwolutionPadLeft  = padLeft;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Right padding.
    ///
    /// Set the colwolution Right padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadRight(int64_t padRight) {
        mDescription.colwolutionPadRight = padRight;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Front padding.
    ///
    /// Set the colwolution Front padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadFront(int64_t padFront) {
        mDescription.colwolutionPadFront = padFront;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution Back padding.
    ///
    /// Set the colwolution Back padding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionPadBack(int64_t padBack) {
        mDescription.colwolutionPadBack = padBack;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution vertical striding.
    ///
    /// Set the colwolution vertical striding.
    /// \param[in] strideH the stride to set.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionStrideH(int64_t strideH) {
        mDescription.colwolutionStrideH = strideH;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution horizontal striding.
    ///
    /// Set the colwolution horizontal striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionStrideW(int64_t strideW) {
        mDescription.colwolutionStrideW = strideW;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution depth striding.
    ///
    /// Set the colwolution depth striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionStrideD(int64_t strideD) {
        mDescription.colwolutionStrideD = strideD;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution vertical dilaiton.
    ///
    /// Set the colwolution vertical dilaiton.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionDilationH(int64_t dilationH) {
        mDescription.colwolutionDilationH = dilationH;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution horizontal striding.
    ///
    /// Set the colwolution horizontal striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionDilationW(int64_t dilationW) {
        mDescription.colwolutionDilationW = dilationW;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution depth striding.
    ///
    /// Set the colwolution depth striding.
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionDilationD(int64_t dilationD) {
        mDescription.colwolutionDilationD = dilationD;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the colwolution to be a cross-correlation.
    ///
    /// By default colwolution filters are centered on individual values,
    /// when the cross-correlation flag is set, the filter starts in the
    /// top-left corner.
    /////////////////////////////////////////////////////////////////////////
    inline void setCrossCorrelationFlag(bool flag) {
        mDescription.isCrossCorrelation = flag;
    }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the num of groups for the colwolution
    ///
    /// Set the num of groups for the colwolution
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionGroups(int64_t colwGroups) { mDescription.colwGroups = colwGroups; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set the num of groups per packetfor the colwolution
    ///
    /// Set the num of groups for the colwolution, used for NC/32HW32 C/K = 4/8/16 cases
    /////////////////////////////////////////////////////////////////////////
    inline void setColwolutionGroupsPerPacket(int64_t colwGroupsPerPacket) { mDescription.colwGroupsPerPacket = colwGroupsPerPacket; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Enable dynamic resize feature.
    ///
    /// Enable dynamic resize feature.
    /////////////////////////////////////////////////////////////////////////
    inline void setDynamicResize(bool dynamicResize) { mDescription.dynamicResize = dynamicResize; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Define the number of CTAs per wave for CTA swizzling
    ///
    /// Explicitly define the number of CTAs per wave for CTA swizzling.
    /// Valid for SASS-generated implicit gemm.
    /// Must be greater than 0 and less than
    /// ceil(M / elementRowsPerCta) * ceil(N / elementColsPerCta *
    /// colwGroups - 1. If set to 0 (default), the value is callwlated
    /// using a heuristic for the given hardware information.
    /////////////////////////////////////////////////////////////////////////
    inline void setCtasPerWave(int64_t ctasPerWave) { mDescription.ctasPerWave = ctasPerWave; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Define whether enable callwlated CTAs per wave.
    /////////////////////////////////////////////////////////////////////////
    inline void setCtaSwizzle(bool ctaSwizzle) { mDescription.ctaSwizzle = ctaSwizzle; }

    using Operation::setHardwareInfo; // Bring into scope

    /////////////////////////////////////////////////////////////////////////
    /// \brief Set hardware info
    /// @deprecated Use @ref setHardwareInfo(const HardwareInfo&)
    /////////////////////////////////////////////////////////////////////////
    inline void setHardwareInfo(int32_t multiProcessorCount, int32_t sharedMemPerMultiprocessor, int32_t registersPerMultiprocessor) {
        HardwareInformation hardwareInfo(multiProcessorCount, sharedMemPerMultiprocessor, registersPerMultiprocessor);
        this->setHardwareInfo(hardwareInfo);
    }

#if defined(CASK_UNSAFE_DYNAMIC)
    // can only allow dynamic if compiled in "unsafe" mode
    inline void setAllowDynamic() { mDescription.dynamicProhibited = false; }
#endif // CASK_UNSAFE_DYNAMIC
};


///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// \brief The characterstics of an available colwolution data gradient kernel.
//////////////////////////////////////////////////////////////////////////////
struct
ColwDgradShader : public Shader { // Shader (has name and hash)

    //////////////////////////////////////////////////////////////////////////
    /// \brief Colwolution Dgrad Shader Constructor.
    ///
    /// This constructor should not be called directly by clients.
    /// Client code should find kernels by calling
    /// `ColwDgradShaderList::availableShaders()`.
    //////////////////////////////////////////////////////////////////////////
    ColwDgradShader(const KernelInfo* ki);

    //////////////////////////////////////////////////////////////////////////
    /// \brief Destructor.
    ///
    /// Trivial destructor. Shaders do not allocate or own any resources.
    //////////////////////////////////////////////////////////////////////////
    virtual ~ColwDgradShader();

    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the DY tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType dYScalarType() const;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the W tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType wScalarType() const;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Datatype of the DX tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual ScalarType dXScalarType() const;
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
    /// \brief Number of (packed) scalar values in a element on the W tensor
    //////////////////////////////////////////////////////////////////////////
    virtual int wScalarsPerElement() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Number of (packed) scalar values in a element on the DX tensor
    //////////////////////////////////////////////////////////////////////////
    virtual int dXScalarsPerElement() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Vectorized dimension of the DY tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual int dYVectorizedDim() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Vectorized dimension of the W tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual int wVectorizedDim() const = 0;
    //////////////////////////////////////////////////////////////////////////
    /// \brief Vectorized dimension of the DX tensor.
    //////////////////////////////////////////////////////////////////////////
    virtual int dXVectorizedDim() const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the `GemmChip` enumerant supported by this kernel.
    //////////////////////////////////////////////////////////////////////////
    virtual GemmChip chip() const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the kernel info instance
    //////////////////////////////////////////////////////////////////////////
    virtual const ColwDgradKernelInfo* getInfo() const;

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
    virtual lwdaError_t lastPlatformError(const serializableHostBuf hostBuf) const = 0;


    //////////////////////////////////////////////////////////////////////////
    /// \brief returns true for kernels that expect NHWC input format.
    //////////////////////////////////////////////////////////////////////////
    inline bool isNhwc() const {
        return getInfo()->dyLayout() == md::ActivationLayoutType::NHWC;
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief returns true for kernels that expect NHWC output format.
    //////////////////////////////////////////////////////////////////////////
    inline bool isNhwcOutput() const {
        return isNhwc();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return true if the B matrix is transposed
    //////////////////////////////////////////////////////////////////////////
    inline bool isBTransposed() const {
        GemmLayout layoutB(static_cast<GemmLayout::Label>(getKernelInfo()->layoutB()));
        return (layoutB == GemmLayout::T);
    }

    //////////////////////////////////////////////////////////////////////////
    // \brief Return true for kernels that use NCHW input/output formats
    //////////////////////////////////////////////////////////////////////////
    inline bool isNchw() const {
        return (!isNhwc());
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief True for stridedB kernels.
    //////////////////////////////////////////////////////////////////////////
    inline bool isStridedB() const {
        return this->getKernelInfo()->lwDnnStridedB();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief True for split-k kernels
    //////////////////////////////////////////////////////////////////////////
    inline bool isSplitK() const {
        return getKernelInfo()->lwDnnSplitK();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief True for kernels that support relu and bias parameters.
    //////////////////////////////////////////////////////////////////////////
    inline bool supportReluBias() const {
        return getKernelInfo()->reLuAndBias();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return Constant memory param offset for given a chip family
    //////////////////////////////////////////////////////////////////////////
    inline size_t getConstantMemParamOffset() const {
        // \todo: This doesn't seem like part of colwolution API
        GemmChip chip(static_cast<GemmChip::Label>(kernelInfo->family()));
        switch (chip) {
            case GemmChip::GEMM_CHIP_MAXWELL : return 0x140; // also PASCAL
            case GemmChip::GEMM_CHIP_VOLTA   : return 0x160;
            case GemmChip::GEMM_CHIP_TURING  : return 0x160;
            case GemmChip::GEMM_CHIP_AMPERE  : return 0x160;
            case GemmChip::GEMM_CHIP_HOPPER  : return 0x160; // TODO: please confirm
        }
        return (size_t)-1;              // Error!
    }


    //////////////////////////////////////////////////////////////////////////
    /// \brief The datatype of the bias.
    //////////////////////////////////////////////////////////////////////////
    inline ScalarType biasType() const {
        return getInfo()->biasScalarType();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Number of slices per row.
    //////////////////////////////////////////////////////////////////////////
    int  getSlicesPerRow() const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Number of slices per column.
    //////////////////////////////////////////////////////////////////////////
    int  getSlicesPerCol() const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Can this shader implement the given colwolution operation.
    ///
    /// Can this shader implement the requested operation?
    /// \returns Error::OK if the implementation can handle the request
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code if the Shader cannot compute the requested
    ///          colwolution.
    ///
    /// There's a default implementation of this one, which checks
    /// some basic facts about the colwolution, then individual
    /// colwolution kernels have additional checks
    //////////////////////////////////////////////////////////////////////////
    virtual Error         canImplement(const ColwolutionDgrad& dgrad) const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief CanImplement on a certain compute capability.
    ///
    /// Can this shader implement the operation on a certain compute capability
    /// \returns Error::OK if the implementation can handle the request
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code if the Shader cannot compute the requested
    ///          colwolution, or the hardware can not support it.
    //////////////////////////////////////////////////////////////////////////
    virtual Error         canImplement(const ColwolutionDgrad& dgrad,
                                       const ComputeCapabilityVersion ccv) const;

    //////////////////////////////////////////////////////////////////////////
    /// Callwlate the reserved space buffer sizes (in bytes) needed by
    /// host.  May be expensive operation, runs on host only.
    ///
    /// \pre Object must be in state such that isConsistent() would
    ///      return true
    ///
    /// \post hostBufSize and deviceBufSize have been initialized with
    ///       sizes appropriate for actually running the operation
    ///       given current object state
    ///
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    getHostReservedSize(RunInfo& ri) const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Init host reserved space data.
    ///
    /// May be an expensive operation.  Runs on host only.
    ///
    /// \pre hostBufSize is the same as the value returned by
    ///      getHostReservedSize()
    ///
    /// \pre hostBuf is a host-side buffer of size at least hostBufSize
    ///
    /// \post hostBuf is filled in with the appropriate data for
    ///       launching the operation.
    ///
    /// \post hostBuf is guaranteed to be serializable/deserializable
    ///       (i.e., it contains no pointers or values that would
    ///       change from run-to-run)
    ///
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    initHostReservedSpace(RunInfo& ri) const = 0;

    //////////////////////////////////////////////////////////////////////////
    /// Callwlate the reserved space buffer sizes (in bytes) needed by
    /// device.  May be expensive operation, runs on _host_ only.
    ///
    /// \pre hostBufSize is the same as the value returned by
    ///      getHostReservedSize()
    ///
    /// \pre hostBuf has been initialized either by calling
    ///      initHostReservedSpace(), or by deserializing a buffer
    ///      that was originally created by initHostReservedSpace in
    ///      the same version of this library.
    ///      (i.e. cask::getInternalVersion() must return the same
    ///      value it returned when initHostReservedSpace() was
    ///      originally called.)
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    getDeviceReservedSize(RunInfo& ri) const = 0;

    /// Init device reserved space data.  May be an expensive
    /// operation.  Runs on device.
    ///
    /// \pre hostBuf is previously initialized by initHostReservedSpace
    ///
    /// \post deviceBuf is filled in with the appropriate data for
    ///       launching the operation.
    ///
    /// \post deviceBuf is guaranteed to be
    ///       serializable/deserializable (i.e., it contains no
    ///       pointers or values that would change from run-to-run).
    ///       Thus rather than running this routine every time, you
    ///       are permitted to run it once, copy the data down to the
    ///       host, serialize it, then later deserialize it, copy it
    ///       to the device, and move immediately to calling run()
    ///       without recalling this routine.
    ///
    /// \returns Error::OK on success.
    /// \returns Subclasses may return any other enumerated `Error`
    ///          return code.
    //////////////////////////////////////////////////////////////////////////
    virtual
    Error
    initDeviceReservedSpace(RunInfo& ri,
                            platformStream_t          lwdaStream) = 0;

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
    /// \brief Run the kernel on the device.
    ///
    /// \pre deviceBufSize is the same as the value returned by
    /// getDeviceReservedSize()
    ///
    /// \pre deviceBuf is allocated and initialized on the device by a previous
    /// call to initDeviceReservedSpace() in a version of the library such that
    /// cask::getInternalVersion() returns the same value.
    ///
    /// deviceInputTensor, deviceOutputTensor, and deviceCTensor are the device
    /// buffers required to perform the colwolution operation.  The tensors
    /// must be *element* aligned.  The shapes of the tensors are given
    /// elsewhere (as part of the colw parameter).
    /////////////////////////////////////////////////////////////////////////
    virtual
    Error
    run(RunInfo& ri,
        void* deviceWorkspacePointer,
        void* deviceDXPointer,
        const void*  deviceDYPointer,
        const void*  deviceWPointer,
        const void*  deviceCPointer,
        const void*  deviceBiasPointer,
        const void*  deviceAlphaVector,
        const void*  deviceBetaVector,
        platformStream_t lwdaStream) = 0;

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
                     const void*           deviceFilterTensor,
                     void*                 deviceTransformedTensor,
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
                     const void*           deviceFilterTensor,
                     void*                 deviceTransformedTensor,
                     platformStream_t      platformStream) = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Returns the size necessary to hold the transformed filter.
    ///
    /// Returns the size in bytes of the transformed filter.
    ///
    /// \returns The number of bytes to be written in a subsequent call
    ///          to transformFilter.
    /// \returns 0 if no transformation is necessary.
    /////////////////////////////////////////////////////////////////////////
    virtual size_t
    transformedBTensorSize(RunInfo& ri) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// \brief If user-managed bias, perform any necessary transformation
    ///        operation, populating deviceTransformedTensor
    ///
    /// In typical usage, CASK manages the bias within the
    /// `hostBuf` blob. However, if the client chooses to manage
    /// the bias and calls the variant of `run()` that explicitly
    /// passes the filter and bias device pointers, the client must ensure
    /// that the filter has undergone any necessary transformations.
    /// This call writes the necessary transformed filter into
    /// deviceTransformedBias.
    ///
    /// \param[in] deviceFilterTensor the original input tensor
    ///
    /// \param[out] deviceTransformedTensor the output tensor to be
    ///             passed to run(), containing all necessary
    ///             transformations.
    ///
    /// \returns Error::OK if the transformed filter is written
    /// \returns Error::ALIGNMENT if called when transformedFilterSize()
    ///          returns 0.
    /////////////////////////////////////////////////////////////////////////
    virtual Error
    transformBias(RunInfo& ri,
                  const void*           deviceBias,
                  void*                 deviceTransformedBias,
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
                     const void*           deviceFilterTensor,
                     void*                 deviceTransformedTensor,
                     platformStream_t      platformStream) = 0;

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
                     const void*           deviceFilterTensor,
                     void*                 deviceTransformedTensor,
                     platformStream_t      platformStream) = 0;

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
    transformedBetaSize(RunInfo& ri) const = 0;

#if defined(ENABLE_FUNCTIONAL_TREE)
    virtual Error getKernelRecord(kernel_record_t * kr) const {
      return Error::FUNCTIONAL_TREE;
    }
#endif

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

    virtual int64_t getSerializedSize() const ;

    virtual Error serialize(uint8_t* buffer, int64_t size)const ;

    static ColwDgradShader* deserialize(const uint8_t* buffer, int64_t size);

};

} // namespace cask

#endif // INCLUDE_GUARD_CASK_DGRAD_H
