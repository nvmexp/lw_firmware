#ifndef INCLUDE_GUARD_CASK_OPERATION_KERNEL_INFO_H
#define INCLUDE_GUARD_CASK_OPERATION_KERNEL_INFO_H

#include <map>
#include <list>
#include <string>

#include "cask/metadata_collection.h"

namespace cask {

typedef enum {
    LINKABLE,
    ALGORITHM,
    SP_GRANULARITY,
    MMA_INSTR_SRC_FMT,
    MMA_INSTR_SHAPE,
    SUPPORT_SPLIT_K,
    SUPPORT_SEGMENT_K,
    SPLIT_K_MODE,
    BLOCK_TILE_M,
    BLOCK_TILE_N,
    BLOCK_TILE_K,
    BLOCK_TILE_G,
    WARP_SIZE_M,
    WARP_SIZE_N,
    WARP_SIZE_K,
    STRIDE_U,
    STRIDE_V,
    XCORRELATION_ONLY,
    ALIGNMENT_C,
    ALIGNMENT_BIAS,
    ALIGNMENT_VALPHA,
    ALIGNMENT_VBETA,
    LDGSTS_BYPASS_L1_A,
    LDGSTS_BYPASS_L1_B,
    LDGSTS_BYPASS_L1_X,
    LDGSTS_BYPASS_L1_W,
    LDGSTS_BYPASS_L1_DY,
    OUTPUT_TRIANGULAR,
    EDGE,
    IS_EPIFADD,
    IS_PLANAR_COMPLEX,
    PROGRAMMATIC_GRID_SYNC_SUPPORT,
    BLOCK_CLUSTER_MAX_M,
    BLOCK_CLUSTER_MAX_N,
    MULTIPLICATION_ALGORITHM,
    BLASL3_INPUT_TRIANGULAR,
    BLASL3_INPUT_TRIANGULAR_RIGHT,
    OPTIONAL_MD_COUNT // Must the last value in this enum!
} OptionalMetadataName;

template<OptionalMetadataName> struct OptionalMetadata;

template<> struct OptionalMetadata< LINKABLE                 > { using type = bool;                };
template<> struct OptionalMetadata< ALGORITHM                > { using type = uint64_t;            }; // bit set of enum flag from "md::Algorithm"
template<> struct OptionalMetadata< SP_GRANULARITY           > { using type = md::MmaInstrSpRatio; };
template<> struct OptionalMetadata< MMA_INSTR_SRC_FMT        > { using type = ScalarType;          };
template<> struct OptionalMetadata< MMA_INSTR_SHAPE          > { using type = md::MmaInstrShape;   };
template<> struct OptionalMetadata< SUPPORT_SPLIT_K          > { using type = bool;                };
template<> struct OptionalMetadata< SUPPORT_SEGMENT_K        > { using type = bool;                };
template<> struct OptionalMetadata< SPLIT_K_MODE             > { using type = uint64_t;            }; // bit set of enum flag from "md::SplitKMode"
template<> struct OptionalMetadata< BLOCK_TILE_M             > { using type = int32_t;             };
template<> struct OptionalMetadata< BLOCK_TILE_N             > { using type = int32_t;             };
template<> struct OptionalMetadata< BLOCK_TILE_K             > { using type = int32_t;             };
template<> struct OptionalMetadata< BLOCK_TILE_G             > { using type = int32_t;             };
template<> struct OptionalMetadata< WARP_SIZE_M              > { using type = int32_t;             };
template<> struct OptionalMetadata< WARP_SIZE_N              > { using type = int32_t;             };
template<> struct OptionalMetadata< WARP_SIZE_K              > { using type = int32_t;             };
template<> struct OptionalMetadata< STRIDE_U                 > { using type = int32_t;             };
template<> struct OptionalMetadata< STRIDE_V                 > { using type = int32_t;             };
template<> struct OptionalMetadata< XCORRELATION_ONLY        > { using type = bool;                };
template<> struct OptionalMetadata< ALIGNMENT_C              > { using type = int32_t;             };
template<> struct OptionalMetadata< ALIGNMENT_BIAS           > { using type = int32_t;             };
template<> struct OptionalMetadata< ALIGNMENT_VALPHA         > { using type = int32_t;             };
template<> struct OptionalMetadata< ALIGNMENT_VBETA          > { using type = int32_t;             };
template<> struct OptionalMetadata< LDGSTS_BYPASS_L1_A       > { using type = bool;                };
template<> struct OptionalMetadata< LDGSTS_BYPASS_L1_B       > { using type = bool;                };
template<> struct OptionalMetadata< LDGSTS_BYPASS_L1_X       > { using type = bool;                };
template<> struct OptionalMetadata< LDGSTS_BYPASS_L1_W       > { using type = bool;                };
template<> struct OptionalMetadata< LDGSTS_BYPASS_L1_DY      > { using type = bool;                };
template<> struct OptionalMetadata< OUTPUT_TRIANGULAR        > { using type = md::TriangularKind;  };
template<> struct OptionalMetadata< EDGE                     > { using type = md::EdgeType;        };
template<> struct OptionalMetadata< IS_EPIFADD               > { using type = bool;                };
template<> struct OptionalMetadata< IS_PLANAR_COMPLEX        > { using type = bool;                };
template<> struct OptionalMetadata< PROGRAMMATIC_GRID_SYNC_SUPPORT > { using type = bool;          };
template<> struct OptionalMetadata< BLOCK_CLUSTER_MAX_M      > { using type = uint32_t;            };
template<> struct OptionalMetadata< BLOCK_CLUSTER_MAX_N      > { using type = uint32_t;            };
template<> struct OptionalMetadata< MULTIPLICATION_ALGORITHM > { using type = md::MultiplicationAlgorithm;                };
template<> struct OptionalMetadata< BLASL3_INPUT_TRIANGULAR   > { using type = bool;                };
template<> struct OptionalMetadata< BLASL3_INPUT_TRIANGULAR_RIGHT   > { using type = bool;                };

typedef MetadataCollection<OptionalMetadataName, OptionalMetadata, OPTIONAL_MD_COUNT> OptionalMdCollection;


//////////////////////////////////////////////////////////////////////////////
/// \brief TensorType class
//////////////////////////////////////////////////////////////////////////////
struct TensorType {
    cask::ScalarType scalarType;

    union LayoutType {
        md::MatrixLayoutType matrix;
        md::ActivationLayoutType activation;
        md::WeightLayoutType weight;
        md::GenericLayoutType generic;

        LayoutType() {}
        ~LayoutType() = default;
    } layoutType;

    /////////////////////////////////////////////////////////////////////////
    /// \brief tensor's layout type.
    /////////////////////////////////////////////////////////////////////////
    LayoutID layoutId{LayoutID::DEGENERATE};

    int32_t vectorizedDim{-1};
    int32_t scalarsPerElement{1};
    int32_t alignment{1};

    /////////////////////////////////////////////////////////////////////////
    /// \brief A builtin unary operation that will be applied to each scalar
    /// from the input tensor before the kernel computes the main operation.
    /////////////////////////////////////////////////////////////////////////
    BuiltinOperatorID builtinOp{BuiltinOperatorID::NOP};

    TensorType() { this->layoutType.matrix = md::MatrixLayoutType::N; }

    TensorType(ScalarType scalar_type,
               LayoutID layout_id,
               int32_t align_bytes,
               BuiltinOperatorID op = BuiltinOperatorID::NOP);

    TensorType(
            cask::ScalarType scalar_type,
            md::MatrixLayoutType layout_type,
            int32_t vect_dim,
            int32_t vect_size,
            int32_t align_bytes)
        : scalarType(scalar_type)
        , vectorizedDim(vect_dim)
        , scalarsPerElement(vect_size)
        , alignment(align_bytes) {
        this->layoutType.matrix = layout_type;
        initLayoutID(layout_type, vect_dim, vect_size);
    }

    TensorType(
            cask::ScalarType scalar_type,
            md::ActivationLayoutType layout_type,
            int32_t vect_dim,
            int32_t vect_size,
            int32_t align_bytes)
        : scalarType(scalar_type)
        , vectorizedDim(vect_dim)
        , scalarsPerElement(vect_size)
        , alignment(align_bytes) {
        this->layoutType.activation = layout_type;
        initLayoutID(layout_type, vect_dim, vect_size);
    }

    TensorType(
            cask::ScalarType scalar_type,
            md::WeightLayoutType layout_type,
            int32_t vect_dim,
            int32_t vect_size,
            int32_t align_bytes)
        : scalarType(scalar_type)
        , vectorizedDim(vect_dim)
        , scalarsPerElement(vect_size)
        , alignment(align_bytes) {
        this->layoutType.weight = layout_type;
        initLayoutID(layout_type, vect_dim, vect_size);
    }

    TensorType(
            cask::ScalarType scalar_type,
            md::GenericLayoutType layout_type,
            int32_t vect_dim,
            int32_t vect_size,
            int32_t align_bytes)
        : scalarType(scalar_type)
        , vectorizedDim(vect_dim)
        , scalarsPerElement(vect_size)
        , alignment(align_bytes) {
        this->layoutType.generic = layout_type;
    }

    ~TensorType() = default;

protected:

    void initLayoutID(md::MatrixLayoutType layout_type, int32_t vect_dim, int32_t vect_size);
    void initLayoutID(md::ActivationLayoutType layout_type, int32_t vect_dim, int32_t vect_size);
    void initLayoutID(md::WeightLayoutType layout_type, int32_t vect_dim, int32_t vect_size);

};

//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo base class
//////////////////////////////////////////////////////////////////////////////
class BaseKernelInfo {

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Default Ctor.
    /////////////////////////////////////////////////////////////////////////
    BaseKernelInfo() {}

    /////////////////////////////////////////////////////////////////////////
    /// \brief Default Dtor.
    /////////////////////////////////////////////////////////////////////////
    virtual ~BaseKernelInfo() = default;

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with params as input.
    ///
    /// Build a KernelInfo instance with operation, name and other params.
    /////////////////////////////////////////////////////////////////////////
    BaseKernelInfo(
        OpDefinition    operation,
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters = -1
    );

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return optional metadata collection.
    /////////////////////////////////////////////////////////////////////////
    const OptionalMdCollection& optional() const noexcept { return optional_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief return all the supported metadata information in list struct
    ///  for easier printing.
    /////////////////////////////////////////////////////////////////////////
    typedef std::list< std::pair<const char*, std::string> > mdList_t;
    mdList_t toList() const;

 //---------------------------- Metadata APIs -----------------------------------

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return operation type.
    /////////////////////////////////////////////////////////////////////////
    OpDefinition   operation()                const noexcept { return operation_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return kernel name.
    /////////////////////////////////////////////////////////////////////////
    const char*    name()                     const noexcept { return name_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return supported SM versions.
    /////////////////////////////////////////////////////////////////////////
    uint64_t       sms()                      const noexcept { return sms_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return number of registers.
    /////////////////////////////////////////////////////////////////////////
    int32_t        numRegisters()               const noexcept { return numRegisters_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return number of threads per CTA.
    /////////////////////////////////////////////////////////////////////////
    int32_t        ctaThreads()               const noexcept { return ctaThreads_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the size of shared mem used by this Kernel.
    /////////////////////////////////////////////////////////////////////////
    int32_t        sharedMemBytes()           const noexcept { return sharedMemBytes_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the handle of the kernel
    /////////////////////////////////////////////////////////////////////////
    uint64_t       handle()                   const noexcept { return handle_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return size required for serializing kernel info
    /////////////////////////////////////////////////////////////////////////
    virtual int64_t getSerializedSize()       const;
    /////////////////////////////////////////////////////////////////////////
    /// \brief Serialize kernel info to the specified buffer
    /////////////////////////////////////////////////////////////////////////
    virtual cask::Error serialize(uint8_t* buffer, int64_t size) const;

protected:

    OpDefinition          operation_;
    const char*           name_;
    uint64_t              sms_;
    int32_t               ctaThreads_;
    int32_t               sharedMemBytes_;
    int32_t               numRegisters_;
    uint64_t              handle_;

    OptionalMdCollection  optional_;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for Gemm Kernels.
//////////////////////////////////////////////////////////////////////////////
class GemmKernelInfo : public BaseKernelInfo {

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    GemmKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        bool            supportBias,
        ScalarType      biasScalarType,
        uint64_t        activationType,
        TensorType const& aTensorType,
        TensorType const& bTensorType,
        TensorType const& cTensorType,
        TensorType const& dTensorType
    ) :
        BaseKernelInfo(
            OP_GEMM,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        supportBias_(supportBias),
        biasScalarType_(biasScalarType),
        activationType_(activationType),
        aTensorType_(aTensorType),
        bTensorType_(bTensorType),
        cTensorType_(cTensorType),
        dTensorType_(dTensorType) {}

    GemmKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        bool            supportBias,
        ScalarType      biasScalarType,
        uint64_t        activationType,
        ScalarType      aScalarType,
        ScalarType      bScalarType,
        ScalarType      cScalarType,
        int32_t         aScalarsPerElement,
        int32_t         bScalarsPerElement,
        int32_t         cScalarsPerElement,
        int32_t         aVectorizedDim,
        int32_t         bVectorizedDim,
        int32_t         cVectorizedDim,
        int32_t         aAlignment,
        int32_t         bAlignment,
        int32_t         cAlignment,
        md::MatrixLayoutType  aLayout,
        md::MatrixLayoutType  bLayout,
        md::MatrixLayoutType  cLayout,
        md::MatrixLayoutType  dLayout
    ):
        BaseKernelInfo(
            OP_GEMM,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        supportBias_(supportBias),
        biasScalarType_(biasScalarType),
        activationType_(activationType),
        aTensorType_(aScalarType, aLayout, aVectorizedDim, aScalarsPerElement, aAlignment),
        bTensorType_(bScalarType, bLayout, bVectorizedDim, bScalarsPerElement, bAlignment),
        cTensorType_(cScalarType, cLayout, cVectorizedDim, cScalarsPerElement, cAlignment),
        dTensorType_(cScalarType, dLayout, cVectorizedDim, cScalarsPerElement, cAlignment) {}


    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports per-channel scaling.
    /////////////////////////////////////////////////////////////////////////
    bool           supportPerChannelScaling() const noexcept { return supportPerChannelScaling_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports bias.
    /////////////////////////////////////////////////////////////////////////
    bool           supportBias()              const noexcept { return supportBias_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's bias data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     biasScalarType()           const noexcept { return biasScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's activation type.
    ///
    /// Return a bit map indicating ReLu, Gelu and more activation
    /// types are enabled.
    /////////////////////////////////////////////////////////////////////////
    uint64_t       activationType()           const noexcept { return activationType_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of matrix A.
    /////////////////////////////////////////////////////////////////////////
    md::MatrixLayoutType aLayout()            const noexcept { return aTensorType_.layoutType.matrix; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of matrix B.
    /////////////////////////////////////////////////////////////////////////
    md::MatrixLayoutType bLayout()            const noexcept { return bTensorType_.layoutType.matrix; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of matrix C (Output).
    /////////////////////////////////////////////////////////////////////////
    md::MatrixLayoutType cLayout()            const noexcept { return cTensorType_.layoutType.matrix; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of matrix D (scaling).
    /////////////////////////////////////////////////////////////////////////
    md::MatrixLayoutType dLayout()            const noexcept { return dTensorType_.layoutType.matrix; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of matrix A.
    /////////////////////////////////////////////////////////////////////////
    LayoutID aLayoutId()                      const noexcept { return aTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of matrix B.
    /////////////////////////////////////////////////////////////////////////
    LayoutID bLayoutId()                      const noexcept { return bTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of matrix C (Output).
    /////////////////////////////////////////////////////////////////////////
    LayoutID cLayoutId()                      const noexcept { return cTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of matrix D (scaling).
    /////////////////////////////////////////////////////////////////////////
    LayoutID dLayoutId()                      const noexcept { return dTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix A.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& aTensorType()           const noexcept { return aTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix B.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& bTensorType()           const noexcept { return bTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix C.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& cTensorType()           const noexcept { return cTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix D.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& dTensorType()           const noexcept { return dTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    bool                  supportPerChannelScaling_;
    bool                  supportBias_;
    ScalarType            biasScalarType_;
    uint64_t              activationType_; // bit set of enum flag from "md::ActivationFuncType"
    TensorType            aTensorType_;
    TensorType            bTensorType_;
    TensorType            cTensorType_;
    TensorType            dTensorType_;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for Colwolution Kernels.
//////////////////////////////////////////////////////////////////////////////
class ColwKernelInfo : public BaseKernelInfo {

public:


    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    ColwKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        bool            supportBias,
        ScalarType      biasScalarType,
        uint64_t        activationType,
        ScalarType      xScalarType,
        ScalarType      wScalarType,
        ScalarType      yScalarType,
        int32_t         xScalarsPerElement,
        int32_t         wScalarsPerElement,
        int32_t         yScalarsPerElement,
        int32_t         xVectorizedDim,
        int32_t         wVectorizedDim,
        int32_t         yVectorizedDim,
        int32_t         xAlignment,
        int32_t         wAlignment,
        int32_t         yAlignment,
        md::ActivationLayoutType xLayout,
        md::WeightLayoutType     wLayout,
        md::ActivationLayoutType yLayout
    ) :
        BaseKernelInfo(
            OP_COLW,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        supportBias_(supportBias),
        biasScalarType_(biasScalarType),
        activationType_(activationType),
        xTensorType_(xScalarType, xLayout, xVectorizedDim, xScalarsPerElement, xAlignment),
        wTensorType_(wScalarType, wLayout, wVectorizedDim, wScalarsPerElement, wAlignment),
        yTensorType_(yScalarType, yLayout, yVectorizedDim, yScalarsPerElement, yAlignment) {}

    ColwKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        bool            supportBias,
        ScalarType      biasScalarType,
        uint64_t        activationType,
        TensorType const& xTensorType,
        TensorType const& wTensorType,
        TensorType const& yTensorType
    ) :
        BaseKernelInfo(
            OP_COLW,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        supportBias_(supportBias),
        biasScalarType_(biasScalarType),
        activationType_(activationType),
        xTensorType_(xTensorType),
        wTensorType_(wTensorType),
        yTensorType_(yTensorType) {}


    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports per-channel scaling.
    /////////////////////////////////////////////////////////////////////////
    bool           supportPerChannelScaling() const noexcept { return supportPerChannelScaling_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports bias.
    /////////////////////////////////////////////////////////////////////////
    bool           supportBias()              const noexcept { return supportBias_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's bias data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     biasScalarType()           const noexcept { return biasScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's activation type.
    ///
    /// Return a bit map indicating ReLu, Gelu and more activation
    /// types are enabled.
    /////////////////////////////////////////////////////////////////////////
    uint64_t       activationType()           const noexcept { return activationType_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor X.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType xLayout()        const noexcept { return xTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor W.
    /////////////////////////////////////////////////////////////////////////
    md::WeightLayoutType     wLayout()        const noexcept { return wTensorType_.layoutType.weight; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor Y.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType yLayout()        const noexcept { return yTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of tensor X.
    /////////////////////////////////////////////////////////////////////////
    LayoutID xLayoutId()                      const noexcept { return xTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of tensor W.
    /////////////////////////////////////////////////////////////////////////
    LayoutID wLayoutId()                      const noexcept { return wTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout id of tensor Y.
    /////////////////////////////////////////////////////////////////////////
    LayoutID yLayoutId()                      const noexcept { return yTensorType_.layoutId; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix X.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& xTensorType()           const noexcept { return xTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix W.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& wTensorType()           const noexcept { return wTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix Y.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& yTensorType()           const noexcept { return yTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    bool                  supportPerChannelScaling_;
    bool                  supportBias_;
    ScalarType            biasScalarType_;
    uint64_t              activationType_; // bit set of enum flag from "md::ActivationFuncType"
    TensorType            xTensorType_;
    TensorType            wTensorType_;
    TensorType            yTensorType_;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for ColwolutionWgrad Kernels.
//////////////////////////////////////////////////////////////////////////////
class ColwWgradKernelInfo : public BaseKernelInfo {

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    ColwWgradKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        TensorType const& dyTensorType,
        TensorType const&  xTensorType,
        TensorType const& dwTensorType
    ) :
        BaseKernelInfo(
            OP_WGRAD,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        dyTensorType_(dyTensorType),
         xTensorType_( xTensorType),
        dwTensorType_(dwTensorType) {}

    /////////////////////////////////////////////////////////////////////////
    /// \brief LEGACY Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    ColwWgradKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        ScalarType      dyScalarType,
        ScalarType       xScalarType,
        ScalarType      dwScalarType,
        int32_t         dyScalarsPerElement,
        int32_t          xScalarsPerElement,
        int32_t         dwScalarsPerElement,
        int32_t         dyVectorizedDim,
        int32_t          xVectorizedDim,
        int32_t         dwVectorizedDim,
        int32_t         dyAlignment,
        int32_t          xAlignment,
        int32_t         dwAlignment,
        md::ActivationLayoutType dyLayout,
        md::ActivationLayoutType xLayout,
        md::WeightLayoutType     dwLayout
    ) :
        BaseKernelInfo(
            OP_WGRAD,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        dyTensorType_(dyScalarType, dyLayout, dyVectorizedDim, dyScalarsPerElement, dyAlignment),
         xTensorType_( xScalarType,  xLayout,  xVectorizedDim,  xScalarsPerElement,  xAlignment),
        dwTensorType_(dwScalarType, dwLayout, dwVectorizedDim, dwScalarsPerElement, dwAlignment) {}


    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports per-channel scaling.
    /////////////////////////////////////////////////////////////////////////
    bool           supportPerChannelScaling() const noexcept { return supportPerChannelScaling_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor DY.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType dyLayout()       const noexcept { return dyTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor X.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType  xLayout()       const noexcept { return  xTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor DW.
    /////////////////////////////////////////////////////////////////////////
    md::WeightLayoutType     dwLayout()       const noexcept { return dwTensorType_.layoutType.weight; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix DY.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& dyTensorType()           const noexcept { return dyTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix X.
    /////////////////////////////////////////////////////////////////////////
    TensorType const&  xTensorType()           const noexcept { return  xTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix DW.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& dwTensorType()           const noexcept { return dwTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    bool                  supportPerChannelScaling_;
    TensorType            dyTensorType_;
    TensorType             xTensorType_;
    TensorType            dwTensorType_;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for ColwolutionDgrad Kernels.
//////////////////////////////////////////////////////////////////////////////

// Temporary hack to support legacy 3.x CASK APIs, which use
// the same shader class to expose both colw and dgrad kernels
#define KI_COMPATIBLE_WITH_3X_CASK_APIS   1

#if KI_COMPATIBLE_WITH_3X_CASK_APIS
class ColwDgradKernelInfo : public ColwKernelInfo {
#else
class ColwDgradKernelInfo : public BaseKernelInfo {
#endif

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    ColwDgradKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        TensorType const& dyTensorType,
        TensorType const&  wTensorType,
        TensorType const& dxTensorType
    ) :
#if KI_COMPATIBLE_WITH_3X_CASK_APIS
        ColwKernelInfo(
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters,
            accScalarType,
            epilogScalarType,
            supportPerChannelScaling,
            false,
            epilogScalarType,
            0,
            dyTensorType,
             wTensorType,
            dxTensorType
        ),
#else
        BaseKernelInfo(
            OP_DGRAD,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
#endif
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        dyTensorType_( dyTensorType),
         wTensorType_(  wTensorType),
        dxTensorType_( dxTensorType) {}

    /////////////////////////////////////////////////////////////////////////
    /// \brief LEGACY
    /////////////////////////////////////////////////////////////////////////
    ColwDgradKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        ScalarType      dyScalarType,
        ScalarType       wScalarType,
        ScalarType      dxScalarType,
        int32_t         dyScalarsPerElement,
        int32_t          wScalarsPerElement,
        int32_t         dxScalarsPerElement,
        int32_t         dyVectorizedDim,
        int32_t          wVectorizedDim,
        int32_t         dxVectorizedDim,
        int32_t         dyAlignment,
        int32_t          wAlignment,
        int32_t         dxAlignment,
        md::ActivationLayoutType  dyLayout,
        md::WeightLayoutType       wLayout,
        md::ActivationLayoutType  dxLayout
    ):
#if KI_COMPATIBLE_WITH_3X_CASK_APIS
        ColwKernelInfo(
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters,
            accScalarType,
            epilogScalarType,
            supportPerChannelScaling,
            false,
            epilogScalarType,
            0,
            dyScalarType,
             wScalarType,
            dxScalarType,
            dyScalarsPerElement,
             wScalarsPerElement,
            dxScalarsPerElement,
            dyVectorizedDim,
             wVectorizedDim,
            dxVectorizedDim,
            dyAlignment,
             wAlignment,
            dxAlignment,
            dyLayout,
             wLayout,
            dxLayout),
#else
        BaseKernelInfo(
            OP_DGRAD,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
#endif
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        dyTensorType_(dyScalarType, dyLayout, dyVectorizedDim, dyScalarsPerElement, dyAlignment),
         wTensorType_( wScalarType,  wLayout,  wVectorizedDim,  wScalarsPerElement,  wAlignment),
        dxTensorType_(dxScalarType, dxLayout, dxVectorizedDim, dxScalarsPerElement, dxAlignment) {}


    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports per-channel scaling.
    /////////////////////////////////////////////////////////////////////////
    bool           supportPerChannelScaling() const noexcept { return supportPerChannelScaling_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor DY.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType  dyLayout()      const noexcept { return dyTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor W.
    /////////////////////////////////////////////////////////////////////////
    md::WeightLayoutType       wLayout()      const noexcept { return  wTensorType_.layoutType.weight; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor DX.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType  dxLayout()      const noexcept { return dxTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix DY.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& dyTensorType()           const noexcept { return dyTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix W.
    /////////////////////////////////////////////////////////////////////////
    TensorType const&  wTensorType()           const noexcept { return  wTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix DX.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& dxTensorType()           const noexcept { return dxTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    bool                  supportPerChannelScaling_;
    TensorType            dyTensorType_;
    TensorType             wTensorType_;
    TensorType            dxTensorType_;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for Decolwolution Kernels.
//////////////////////////////////////////////////////////////////////////////

class DecolwKernelInfo : public BaseKernelInfo {

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    DecolwKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        bool            supportBias,
        ScalarType      biasScalarType,
        uint64_t        activationType,
        TensorType const& yTensorType,
        TensorType const& wTensorType,
        TensorType const& xTensorType
    ) :
        BaseKernelInfo(
            OP_DECOLW,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        supportBias_(supportBias),
        biasScalarType_(biasScalarType),
        activationType_(activationType),
        yTensorType_(yTensorType),
        wTensorType_(wTensorType),
        xTensorType_(xTensorType) {}


    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports per-channel scaling.
    /////////////////////////////////////////////////////////////////////////
    bool           supportPerChannelScaling() const noexcept { return supportPerChannelScaling_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports bias.
    /////////////////////////////////////////////////////////////////////////
    bool           supportBias()              const noexcept { return supportBias_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's bias data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     biasScalarType()           const noexcept { return biasScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's activation type.
    ///
    /// Return a bit map indicating ReLu, Gelu and more activation
    /// types are enabled.
    /////////////////////////////////////////////////////////////////////////
    uint64_t       activationType()           const noexcept { return activationType_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor Y.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType  yLayout()       const noexcept { return yTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor W.
    /////////////////////////////////////////////////////////////////////////
    md::WeightLayoutType      wLayout()       const noexcept { return wTensorType_.layoutType.weight; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor X.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType  xLayout()       const noexcept { return xTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix Y.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& yTensorType()           const noexcept { return yTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix W.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& wTensorType()           const noexcept { return wTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix X.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& xTensorType()           const noexcept { return xTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    bool                  supportPerChannelScaling_;
    bool                  supportBias_;
    ScalarType            biasScalarType_;
    uint64_t              activationType_;
    TensorType            yTensorType_;
    TensorType            wTensorType_;
    TensorType            xTensorType_;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for Pooling Kernels.
//////////////////////////////////////////////////////////////////////////////
class PoolingKernelInfo : public BaseKernelInfo {

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    PoolingKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        TensorType const& xTensorType,
        TensorType const& yTensorType
    ):
        BaseKernelInfo(
            OP_POOL,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        xTensorType_(xTensorType),
        yTensorType_(yTensorType) {}

    PoolingKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType,
        bool            supportPerChannelScaling,
        ScalarType      xScalarType,
        ScalarType      yScalarType,
        int32_t         xScalarsPerElement,
        int32_t         yScalarsPerElement,
        int32_t         xVectorizedDim,
        int32_t         yVectorizedDim,
        int32_t         xAlignment,
        int32_t         yAlignment,
        md::ActivationLayoutType xLayout,
        md::ActivationLayoutType yLayout
    ):
        BaseKernelInfo(
            OP_POOL,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        supportPerChannelScaling_(supportPerChannelScaling),
        xTensorType_(xScalarType, xLayout, xVectorizedDim, xScalarsPerElement, xAlignment),
        yTensorType_(yScalarType, yLayout, yVectorizedDim, yScalarsPerElement, yAlignment) {}


    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return true if this kernel supports per-channel scaling.
    /////////////////////////////////////////////////////////////////////////
    bool           supportPerChannelScaling() const noexcept { return supportPerChannelScaling_; }

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor X.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType xLayout()        const noexcept { return xTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return the layout of tensor Y.
    /////////////////////////////////////////////////////////////////////////
    md::ActivationLayoutType yLayout()        const noexcept { return yTensorType_.layoutType.activation; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix X.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& xTensorType()           const noexcept { return xTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix Y.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& yTensorType()           const noexcept { return yTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    bool                  supportPerChannelScaling_;
    TensorType            xTensorType_;
    TensorType            yTensorType_;

};

//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for Layout Transformation Kernels.
class LayoutTransformationKernelInfo : public BaseKernelInfo {
public:

    LayoutTransformationKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      alphaScalarType,
        int32_t         NDIM,
        int32_t         tile_size_0,
        int32_t         tile_size_1,
        int32_t         tile_size_2,
        int32_t         vec,
        bool            transpose
    ):
        BaseKernelInfo(
            OP_ELEMENTWISE,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters){}

    ScalarType alphaScalarType() const noexcept { return alphaScalarType_; }
    int32_t NDIM() const noexcept {return NDIM_;}
    int32_t tile_size_0() const noexcept {return tile_size_0_;}
    int32_t tile_size_1() const noexcept {return tile_size_1_;}
    int32_t tile_size_2() const noexcept {return tile_size_2_;}
    int32_t vec() const noexcept {return vec_;}
    bool transpose() const noexcept {return transpose_;}

protected:
    ScalarType alphaScalarType_;
    int32_t NDIM_;
    int32_t tile_size_0_;
    int32_t tile_size_1_;
    int32_t tile_size_2_;
    int32_t vec_;
    bool transpose_;
};
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for GraphKernelInfo Kernels.
//////////////////////////////////////////////////////////////////////////////


class GraphKernelInfo : public BaseKernelInfo {
public:
    using BaseKernelInfo::BaseKernelInfo;
    virtual ~GraphKernelInfo() {}

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return input size
    /////////////////////////////////////////////////////////////////////////
    virtual int32_t inputSize()                                 const { return 0; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return output size
    /////////////////////////////////////////////////////////////////////////
    virtual int32_t outputSize()                                const { return 0; }

    virtual TensorType const& inputTensorType(int index)        const {
        static TensorType null_type;
        return null_type;
    }

    virtual TensorType const& outputTensorType(int index)       const {
        static TensorType null_type;
        return null_type;
    }

};

//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for Gett Kernels.
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
/// \brief KernelInfo class for GraphKernelInfo Kernels.
//////////////////////////////////////////////////////////////////////////////

class GettKernelInfo : public BaseKernelInfo {

public:

    /////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with complete list of params.
    /////////////////////////////////////////////////////////////////////////
    GettKernelInfo(
        const char*     name,
        uint64_t        sms,
        int32_t         ctaThreads,
        int32_t         sharedMemBytes,
        int32_t         numRegisters,
        ScalarType      accScalarType,
        ScalarType      epilogScalarType, // need alpha/betaType as well?TODO
        TensorType const& aTensorType,
        TensorType const& bTensorType,
        TensorType const& cTensorType,
        TensorType const& dTensorType
    ) :
        BaseKernelInfo(
            OP_GETT,
            name,
            sms,
            ctaThreads,
            sharedMemBytes,
            numRegisters),
        accScalarType_(accScalarType),
        epilogScalarType_(epilogScalarType),
        aTensorType_(aTensorType),
        bTensorType_(bTensorType),
        cTensorType_(cTensorType),
        dTensorType_(dTensorType) {}

    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's aclwmulator data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     accScalarType()            const noexcept { return accScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return this kernel's epilog data type.
    /////////////////////////////////////////////////////////////////////////
    ScalarType     epilogScalarType()         const noexcept { return epilogScalarType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix A.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& aTensorType()           const noexcept { return aTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix B.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& bTensorType()           const noexcept { return bTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix C.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& cTensorType()           const noexcept { return cTensorType_; }
    /////////////////////////////////////////////////////////////////////////
    /// \brief Return TensorType of matrix D.
    /////////////////////////////////////////////////////////////////////////
    TensorType const& dTensorType()           const noexcept { return dTensorType_; }

protected:

    ScalarType            accScalarType_;
    ScalarType            epilogScalarType_;
    TensorType            aTensorType_;
    TensorType            bTensorType_;
    TensorType            cTensorType_;
    TensorType            dTensorType_;

};
} // end namespace cask

#endif // INCLUDE_GUARD_CASK_OPERATION_KERNEL_INFO_H
