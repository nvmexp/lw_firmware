#ifndef INCLUDE_GUARD_CASK_OPERATION_H
#define INCLUDE_GUARD_CASK_OPERATION_H
// enable doxygen:
//! @file

#include <lwComplex.h>
#include <lwda_runtime.h>
#include <lwca.h>
#include <limits>

#include "cask/cask_core.h"
#include "cask/evict_descriptor.h"
#include "cask/tensor_desc.h"

#include "cask/hardware_information.h"

namespace cask {

/*
 * \brief Information about the target device hardward
 * placeholder for runtime parameters
 */

struct RuntimeParameterArray {
    uint32_t _data[6];
    RuntimeParameterArray(){
        for (int i=0;i<6;i++){
            _data[i] = 0;
        }
    }
};

/**
 * \brief Runtime parameters for synthetic MODS kernel
 */
struct ModsRuntimeParameters {
    uint32_t periodMaskNs;
    uint32_t pulseNs;
    uint32_t minSleep;
    uint32_t mainloopCount;
};

/**
 * \brief Operation pure virtual base class to describe callwlations
 * performed by corresponding Shader classes.
 *
 * Operation provides minimal functionality beyond a default
 * Description definition targeted at Colwolution and GEMM.
 */
class
Operation {
public:
    typedef cask::Error Error;

    /**
     * \brief A POD struct describing the input and optional settings of
     * a gemm or colwolution operation. It is encapsulated in the
     * Operation object and the public interface is meant to be exposed
     * by setters and getters of the Operation subclasses.
     */
    struct Description {
        Description() = default;
        TensorDesc inputADesc  = {}; // aka  weightDesc
        TensorDesc inputAmDesc = {}; // aka  weightDesc
        TensorDesc inputBDesc  = {}; // aka filterDesc
        TensorDesc outputDesc  = {};
        ScalarType biasType = ScalarType::FP32;
        int64_t    biasSimdWidth = 1;
        double           alpha = 1.f;
        lwComplex        alphaCp = {1.f, 0.f};
        lwDoubleComplex  alphaCpDouble = {1.f, 0.f};
        double           beta = 0.f;
        lwComplex        betaCp = {0.f, 0.f};
        lwDoubleComplex  betaCpDouble = {0.f, 0.f};
        int32_t    applyReLu = eReLuType::eReLuDisable;
        float      reLuThreshold = 0.f;
        float      reLuUpperBound = std::numeric_limits<float>::infinity();
        bool       addBias = false;
        int64_t    batchBiasStride = 0;
        bool       perChannelScaling = false;
        PointerModeID   pointerMode = PointerModeID::HOST;
        int64_t    batchScalingStride = 0;
        int32_t    splitK = 1;          // aka splitK
        int32_t    splitKMode = 1;      // number of kernels to do splitk
        int32_t    splitKBuffers = 0;   // number of buffers used in inter-CTA splits
        bool       splitKT = false;     // true if split in filter T dimension
        bool       splitKR = false;     // true if split in filter R dimension
        size_t     mode = 0;            // mode: related to splitK, batchedGEMM, and etc.
        int64_t    arrayCount = 0;
        // implement wgrad1x1 using batchedGEMM splitk and spin loop method
        bool       isWgrad1x1BatchedGEMM = false;
        // if splitK without reduction(reduction in another kernel)
        bool       withoutReduction = false;
        int32_t    splitH = 1;
        /// \brief Define the number of CTAs per wave for CTA swizzling
        int64_t    ctasPerWave = 0;
        bool       ctaSwizzle = false;
        bool       isCrossCorrelation = false;
        int64_t    colwGroups = 1;
        // for NC/32HW32 layout, per group < 32
        int64_t    colwGroupsPerPacket = 1;
        bool       dynamicProhibited = true;
        // enable dynamic Resize feature
        bool       dynamicResize = false;
        bool       isDgrad = false;
        // For BLAS L3 kernels with triangular inputs
        bool isBlasl3InputTriangularRight = false;

        int64_t    colwolutionPadTop = 0;
        int64_t    colwolutionPadBottom = 0;
        int64_t    colwolutionPadLeft = 0;
        int64_t    colwolutionPadRight = 0;
        int64_t    colwolutionPadFront = 0;
        int64_t    colwolutionPadBack = 0;
        float      colwolutionPadValue = 0.f; // fill lwstomized value in pad pixels
        int64_t    colwolutionStrideH = 1;
        int64_t    colwolutionStrideW = 1;
        int64_t    colwolutionStrideD = 1;
        int64_t    colwolutionDilationH = 1;
        int64_t    colwolutionDilationW = 1;
        int64_t    colwolutionDilationD = 1;

        //! Describes the target exelwtion environment for the kernel.
        HardwareInformation hardwareInfo = {0, 0, 0};

        ModsRuntimeParameters modsRuntimeParams = {0, 0, 0, 0};

        //! Provides a hint as to the number of SMs a kernel should expect
        //! to be available for use during exelwtion.
        //!
        //! Kernels will be configured, if possible, to utilize this set of
        //! SMs most efficiently. This is purely an optimization hint, CASK
        //! cannot guarantee a kernel will not use more resources than this
        //! value.  If this value is <= 0, it is ignored.
        int32_t requestedSmUtilizationLimit = 0;

        struct SegmentK
        // For internal cask-tester usage only!. Use the @ref
        // cask::SegmentKSupport interface.
        {
            int32_t split1 = -1;      ///< Number of output tiles that are split by factor1 (-1 means unspecified).
            int32_t split2 = -1;      ///< Number of output tiles that are split by factor2 (-1 means unspecified).
            int32_t factor1 = -1;     ///< Size of each split1 reduction (number of CTAs; -1 means unspecified).
            int32_t factor2 = -1;     ///< Size of each split2 reduction (number of CTAs; -1 means unspecified).
            int64_t gmem_limit = -1;  ///< Max global memory for reductions (-1 means no limit).
            int32_t k_min = -1;       ///< Min GEMM K for a partial tile.
            int32_t factor_min = -1;  ///< Lower bound on the GEMM K splitting factor.
            int32_t factor_max = -1;  ///< Upper bound on the GEMM K splitting factor.

            SegmentK() = default;
            SegmentK(const SegmentK&) = default;
            SegmentK& operator=(const SegmentK&) = default;

	    SegmentK(
                int32_t split1,       ///< Number of output tiles that are split by factor1 (-1 means unspecified).
                int32_t split2,       ///< Number of output tiles that are split by factor2 (-1 means unspecified).
                int32_t factor1,      ///< Size of each split1 reduction (number of CTAs; -1 means unspecified).
                int32_t factor2,      ///< Size of each split2 reduction (number of CTAs; -1 means unspecified).
                int64_t gmem_limit,   ///< Max global memory for reductions (-1 means no limit).
                int32_t k_min,        ///< Min GEMM K for a partial tile.
                int32_t factor_min,   ///< Lower bound on the GEMM K splitting factor.
                int32_t factor_max    ///< Upper bound on the GEMM K splitting factor.
	    );
	};

        SegmentK segmentK {};  //< Internal use only by cask-tester.

        struct CgaTile
        // For internal cask-tester usage only! The base block cluster argument settings.
        // Any 0 or negative values supplied indicate that CASK is allowed to use heuristics to
        // choose that dimension based on other settings such as the @ref hardwareInfo and
        // @ref requestedSmUtilizationLimit among other things.
        {
            CgaTile() = default;
            CgaTile(const CgaTile&) = default;
            CgaTile& operator=(const CgaTile&) = default;

            CgaTile(int32_t m_, int32_t n_, int32_t k_ = 1)
	    : m(m_), n(n_), k(k_)
            { }

            int32_t m = 0;
            int32_t n = 0;
            int32_t k = 0;
        };

        CgaTile cgaTile = { };

        void print() const;
        bool isAsymmetricPadding() const;

        // helper aliases
        TensorDesc& weightDesc();
        TensorDesc& metaDataDesc();
        TensorDesc& filterDesc();

        const TensorDesc& weightDesc() const;
        const TensorDesc& metaDataDesc() const;
        const TensorDesc& filterDesc() const;
    };

    Operation() {}

    virtual ~Operation() {}

    inline HardwareInformation const& getHardwareInformation() const {
        return this->mDescription.hardwareInfo;
    }

    inline void setHardwareInfo(HardwareInformation const& hwi) {
        this->mDescription.hardwareInfo = hwi;
    }

    inline int32_t getRequestedSmUtilizationLimit() const {
        return this->mDescription.requestedSmUtilizationLimit;
    }

    inline void setRequestedSmUtilizationLimit(int32_t limit) {
        #ifndef NDEBUG
        if (limit > this->getHardwareInformation().multiProcessorCount) {
            printf("cask::Operation::setRequestedSmUtilizationLimit(%d) requested with HardwareInformation defining only %d SMs.\n",
                limit, this->getHardwareInformation().multiProcessorCount);
        }
        #endif
        this->mDescription.requestedSmUtilizationLimit = limit;
    }

    const Description::CgaTile & getClusterDims() const {
        return this->mDescription.cgaTile;
    }

    void setClusterDims(const Description::CgaTile & cluster) {
        this->mDescription.cgaTile = cluster;
    }

    const Description::SegmentK & getSegmentKSettings() const {
        return this->mDescription.segmentK;
    }

    void setSegmentKSettings(const Description::SegmentK & settings) {
        this->mDescription.segmentK = settings;
    }

    /**
     * \brief Identify if an operation is self-consistant.
     *
     * This is a pure virtual method implemented by specific
     * operations. Typically an Operation is self-consistant
     * if its `TensorDesc` are well-formed with acceptable
     * dimensions and strides, and no mutaually exclusive
     * options are selected.
     *
     * \returns `Error::OK` if the operation can possibly
     *          be callwlated.
     * \returns Subclasses may return any other enumerated `Error`
     *          return code if it is not a self-consistent
     *          operation.
     */
    virtual Error isConsistent() const = 0;

protected:

    Description mDescription;

};

/**
 * \brief RunInfo is a POD class that encapulates all thread-specific data
 * for a single run of a kernel.
 *
 * It is used as a parameter to the host and device buffer initializations
 * and the call to Shader::run(). RunInfo is not designed to be serialized
 * and may not be used by multiple threads for different ilwocations of
 * of Shader::run().
 */
class RunInfo {
  public:

    RunInfo() = default;
    RunInfo(const RunInfo &) = default;

    /**
     * \brief Operation to be callwlated for this run.
     */
    Operation* op = nullptr;

    /**
     * \brief Size of the host buffer as callwlated
     *        by getHostReservedSize().
     */
    size_t hostBufSize = 0;

    /**
     * \brief Size of the host buffer as callwlated
     *        by getDeviceReservedSize().
     */
    size_t deviceBufSize = 0;

    /**
     * \brief Opaque handle to the host buffer workspace.
     *
     * Must be of size `hostBufSize`. Must be set prior to calling
     * initHostReservedSpace().
     *
     */
    serializableHostBuf hostBuf = nullptr;

    /**
     * \brief Opaque handle to the device buffer workspace.
     *
     * Must be of size `deviceBufSize`. Must be set prior to calling
     * initDeviceReservedSpace().
     *
     */
    serializableDeviceBuf deviceBuf = nullptr;

    /**
     * \brief Kernel-specific mode setting. For SASS kernels, see
     * gemm_shader_params.h for layout.
     */
    int32_t mode = 0;

    // Runtime parameters
    /**
     * \brief Runtime update to batchsize.
     *
     * Must be less than or equal to gemm N in the operation.
     */
    int64_t batchSize = 0;

    /**
     * \brief Runtime update to the input A tensor.
     *
     * Must be smaller or equal in H/W dimensions defined in the operation.
     */
    TensorDesc runtimeInputADesc = { };

    /**
     * \brief Runtime update to the output tensor.
     *
     * Must be smaller or equal in H/W dimensions defined in the operation.
     */
    TensorDesc runtimeOutputDesc = { };

    /**
     * \brief Runtime update to the front pad value.
     *
     * Must be smaller or equal to operation left pad value.
     */
    int64_t runtimePadFront = 0;

    /**
     * \brief Runtime update to the back pad value.
     *
     * Must be smaller or equal to operation left pad value.
     */
    int64_t runtimePadBack = 0;

    /**
     * \brief Runtime update to the left pad value.
     *
     * Must be smaller or equal to operation left pad value.
     */
    int64_t runtimePadLeft = 0;

    /**
     * \brief Runtime update to the top pad value.
     *
     * Must be smaller or equal to operation top pad value.
     */
    int64_t runtimePadTop = 0;

    /**
     * \brief Runtime update to the right pad value.
     *
     * Must be smaller or equal to operation right pad value.
     */
    int64_t runtimePadRight = 0;

    /**
     * \brief Runtime update to the bottom pad value.
     *
     * Must be smaller or equal to operation bottom pad value.
     */
    int64_t runtimePadBottom = 0;

    /**
     * \brief Is the A tensor cask-managed.
     *
     * True for configurations passing the A tensor as
     * host data to initDeviceReservedSpace(). False
     * for configurations passing the A tensor at runtime.
     */
    bool caskManagedTensorA = false;

    /**
     * \brief Is the A metadata tensor cask-managed.
     *
     * True for configurations passing the A tensor as
     * host data to initDeviceReservedSpace(). False
     * for configurations passing the A tensor at runtime.
     */
    bool caskManagedTensorAm = false;

    /**
     * \brief Is the B tensor cask-managed.
     *
     * True for configurations passing the B tensor as
     * host data to initDeviceReservedSpace(). False
     * for configurations passing the B tensor at runtime.
     */
    bool caskManagedTensorB = false;

    /**
     * \brief Is the bias tensor cask-managed.
     *
     * True for configurations passing the bias tensor as
     * host data to initDeviceReservedSpace(). False
     * for configurations passing the bias tensor at runtime.
     */
    bool caskManagedBias = false;

    /**
     * \brief Is the alpha and beta tensors cask-managed.
     *
     * True for configurations passing the alpha and beta tensors as
     * host data to initDeviceReservedSpace(). False for
     * configurations passing the alpha and beta tensors at runtime.
     */
    bool caskManagedAlphaBeta = false;

    /**
     * \brief Is the relu tensors cask-managed.
     *
     * True for configurations passing the relu tensors as
     * host data to initDeviceReservedSpace(). False for
     * configurations passing the alpha and beta tensors at runtime.
     */
    bool caskManagedReLu = false;

    /**
     * \brief Host pointer to the A tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostTensorA = nullptr;

    /**
     * \brief Host pointer to the Am tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostTensorAm = nullptr;

    /**
     * \brief Host pointer to the B tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostTensorB = nullptr;

    /**
     * \brief Host pointer to the bias tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostBias = nullptr;

    /**
     * \brief Host pointer to the alpha tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostAlpha = nullptr;

    /**
     * \brief Host pointer to the beta tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostBeta = nullptr;

    /**
     * \brief Host pointer to the relu tensor.
     *
     * Only valid if corresponding caskManaged is true
     */
    void* hostReLu = nullptr;

    /**
     * \brief Device pointer to the A tensor.
     *
     * Only valid if corresponding caskManaged is true
     * This is mostly used for passing sparse tensor.
     */
    void* deviceTensorA = nullptr;

    /**
     * \brief Device pointer to the B tensor.
     *
     * Only valid if corresponding caskManaged is true
     * This is mostly used for passing sparse tensor.
     */
    void* deviceTensorB = nullptr;

    /** @name Descriptors of L2 evict feature
     * Ampere L2 evict feature
     * please use cask::MemImplicitDescriptor, etc. in evict_descriptor.h to set descriptor
     */
    ///@{
    int32_t descriptorA = MemExpliInterleavedDesc::DEFAULT_HI;   //high 32-bit
    int32_t descriptorB = MemExpliInterleavedDesc::DEFAULT_HI;   //high 32-bit
    int32_t descriptorC0 = MemExpliInterleavedDesc::DEFAULT_LOW; // low 32-bit
    int32_t descriptorC1 = MemExpliInterleavedDesc::DEFAULT_HI;  //high 32-bit
    int32_t descriptorD0 = MemExpliInterleavedDesc::DEFAULT_LOW; // low 32-bit
    int32_t descriptorD1 = MemExpliInterleavedDesc::DEFAULT_HI;  //high 32-bit
    ///@}

    /**
     * \brief Last seen error of LWCA calls
     */
    lwdaError_t lastLwdaError = lwdaSuccess;

    /**
     * \brief Scratch pad for kerenl runtime parameters
     */
    RuntimeParameterArray runtimePara = RuntimeParameterArray();

    /**
     * \brief if use lwca driver to launch kernel
     */
    bool useDriver = false;

    /**
     * \brief Pointer to lwbin data of the runtime kernel
     */
    char* kernelLwbin = nullptr;

    /**
     * \brief LWfunction kernel pointer
     */
    LWfunction kernelPtr = nullptr;

    /**
     * \brief Kernel name for runtime fusion.
     */
    std::string runtimeKernelName = std::string("");

private:

    friend class cask::BlockClusterSupport;

    struct BlockClusterOverrides {

        /**
         * \brief Set the maximum allowed block cluster size for a
         * launch.
         */
        OptionalValue<int32_t> maxAllowedSize {};

        /**
         * \brief If set, provides the required CGA launch size for
         * the given kernel. For internal use by @ref
         * cask::BlockClusterSupport
         */
        OptionalValue<dim3> specifiedSize {};

    };

    BlockClusterOverrides blockClusterOverrides {};

private:

    friend class cask::ProgrammaticGridSyncSupport;

    struct ProgrammaticGridSync {

        OptionalValue<ProgrammaticGridSyncSupport::Knobs> knobs {};

    };

    ProgrammaticGridSync programmaticGridSync {};

private: // SEGMENT-K OVERRIDES

    friend class cask::SegmentKSupport;

    struct SegmentK {

        OptionalValue<bool>                           enabled {};
        OptionalValue<SegmentKSupport::PlanningKnobs> planningKnobs {};
        OptionalValue<SegmentKSupport::ControlKnobs>  controlKnobs {};

    };

    SegmentK segmentK {};
};

} // namespace cask

#endif
