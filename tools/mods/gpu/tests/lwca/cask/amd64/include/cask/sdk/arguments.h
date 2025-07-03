/*!
    \file arguments.h
    \brief Trivially copyable structures for passing arguments and metadata between kernels and caller.

    These classes are mirrored with IR definitions.
*/

#pragma once

#include <array>

#include "cask/cask.h"
#include "cask/ir/type.h"

#include "cask/internal/cpp/core.h"

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace cask {
namespace sdk {

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Kind of operation
enum class OperationKindID {
    NOP = 0,                              ///< Operation is a no-op
    GEMM,                                 ///< Operation is a GEMM
    GETT,                                 ///< Operation is a General Tensor-Tensor contraction
    CONVOLUTION,                          ///< Operation is a Convolution
    POOLING,                              ///< Operation is a Pooling operator.
    REDUCTION,                            ///< Operation is a Tensor Reduction
    BROADCAST,                            ///< Operation is a broadcast operation
    TRANSFORM,                            ///< Operation is a layout transformation
    ELEMENTWISE                           ///< Operation is an elementwise operation
};

/// Kind of GEMM operation
enum class GemmKindID {
    GEMM,                                 ///< General matrix-matrix multiply
    SYRK,                                 ///< Symmetric Rank-K update
    SYR2K,                                ///< Symmetric rank-2K update
};


/// GEMM mode
enum class GemmModeID {
    GEMM,                                 ///< Operation is regular GEMM with optional serial split-K
    BATCHED,                              ///< Operation is batched strided GEMM
    ARRAY,                                ///< Operation is batched array GEMM
    PARALLEL_SPLITK                       ///< Operation is GEMM with parallel split-K
};

/// Kind of convolution
enum class ConvolutionKindID {
    FPROP,                                ///< Forward propagation
    DGRAD,                                ///< Data gradient (dx = dgrad(dy, w))
    WGRAD,                                ///< Weight gradient (dw = wgrad(dy, x))
};

/// Convolution mode
enum class ConvolutionModeID {
    CONV,                                 ///< Operator configured as convolution
    CORR                                  ///< Operator configured as cross correlation
};

/// Describes the kind of functor
enum class FunctorKindID {
    INLINE,                               ///< Functor is inlined
    DEVICE_FUNCTION,                      ///< Functor is a non-inline device function
    BUILTIN                               ///< Functor is an enum-selectable built-in function
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Argument structure for a tensor
struct TensorArgument {

    TensorDesc  desc;                                     ///< Tensor descriptor
    void      * pointer;                                  ///< Pointer to tensor in device memory
    int64_t     batch_stride;                             ///< Batch stride (units of elements)
    uint64_t    descriptor;                               ///< Memory descriptor

    //
    // Methods
    //

    /// Default ctor
    TensorArgument();

    /// Ctor
    TensorArgument(
        TensorDesc  desc_,                                ///< Tensor descriptor
        void      * pointer_,                             ///< Pointer to tensor in device memory
        int64_t     batch_stride_ = 0ll,                  ///< Batch stride (in elements)
        uint64_t    descriptor_ = 0ull                    ///< Memory descriptor
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Tensor description structure
struct TensorDescription {

    ir::NumericTypeID element;                            ///< Data type of numeric element
    LayoutID          layout;                             ///< Layout of tensor
    int32_t           alignment;                          ///< Alignment constraint (in units of elements)

    //
    // Methods
    //

    /// Default ctor
    TensorDescription();

    /// Ctor
    TensorDescription(
        ir::NumericTypeID element_,                       ///< Data type of numeric element
        LayoutID          layout_,                        ///< Layout of tensor
        int32_t           alignment_  = 1                 ///< Alignment constraint (in units of elements)
    );

    bool operator==(TensorDescription const &rhs) const ;
    bool operator!=(TensorDescription const &rhs) const ;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Argument structure for a tensor
struct SparseTensorArgument : public TensorArgument {

    void       *metadata;                                 ///< Pointer to metadata (device memory)

    //
    // Methods
    //

    /// Default ctor
    SparseTensorArgument();

    SparseTensorArgument(
        TensorArgument const &tensor_,
        void                 *metadata_
    );

    /// Ctor
    SparseTensorArgument(
        TensorDesc  desc_,                                ///< Tensor descriptor
        void      * pointer_,                             ///< Pointer to tensor in device memory
        void      * metadata_,                            ///< Pointer to metadata (device memory)
        int64_t     batch_stride_ = 0ll,                  ///< Batch stride (in elements)
        uint64_t    descriptor_ = 0ull                    ///< Memory descriptor
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Sparse tensor description structure
struct SparseTensorDescription : public TensorDescription {


    //
    // Methods
    //

    /// Default ctor
    SparseTensorDescription();
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Argument structure for a tensor
struct PlanarComplexTensorArgument {
    TensorArgument real;                                  ///< Real tensor
    TensorArgument imag;                                  ///< Imaginary tensor

    //
    // Methods
    //

    /// Default ctor
    PlanarComplexTensorArgument();

    /// Ctor
    PlanarComplexTensorArgument(
        TensorArgument const &real_,
        TensorArgument const &imag_
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Scalar arguments
struct ScalarArgument {
    void          * pointer;                              ///< Pointer to memory
    PointerModeID   pointer_mode;                         ///< Indicates mode of specific pointer
    uint64_t        descriptor;                           ///< Memory descriptor

    //
    // Methods
    //

    /// Default ctor
    ScalarArgument();

    /// Ctor
    ScalarArgument(
        void          * pointer_,                             ///< Pointer to memory
        PointerModeID   pointer_mode_ = PointerModeID::HOST,  ///< Indicates mode of specific pointer
        uint64_t        descriptor_ = 0ull                    ///< Memory descriptor
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Scalar description object
struct ScalarDescription {

    ir::NumericTypeID element;                                  ///< Element type

    //
    // Methods
    //

    ScalarDescription();

    ScalarDescription(ir::NumericTypeID element_);

    bool operator==(ScalarDescription const &rhs) const;
    bool operator!=(ScalarDescription const &rhs) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Vector arguments
struct VectorArgument {
    void      * pointer;                                        ///< Device memory pointer
    int64_t     batch_stride;                                   ///< Batch stride (units of elements)
    uint64_t    descriptor;                                     ///< Memory descriptor

    //
    // Methods
    //

    /// Default ctor
    VectorArgument();

    /// Ctor
    VectorArgument(
        void      * pointer_,                                   ///< Device memory pointer
        int64_t     batch_stride_ = 0ll,                        ///< Batch stride
        uint64_t    descriptor_ = 0ull                          ///< Memory descriptor
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

struct VectorDescription {
    ir::NumericTypeID element;                                  ///< Element type
    int32_t           alignment;                                ///< Alignment requirement on pointer (units of elements)

    //
    // Methods
    //

    VectorDescription();

    VectorDescription(
        ir::NumericTypeID element_,
        int32_t alignment_ = 1
    );

    bool operator==(VectorDescription const &rhs) const;
    bool operator!=(VectorDescription const &rhs) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Multimode vector argument - can represent a scalar or vector operand
struct MultimodeVectorArgument {

    void           *pointer;                                    ///< Pointer to the scalar or vector
    int64_t         batch_stride;                               ///< Batch stride (used by batch mode operations)
    PointerModeID   pointer_mode;                               ///< Indicates whether pointer is in host or device memory
    int32_t         rank;                                       ///< rank=0 indicates scalar. rank=1 indicates vector.
    uint64_t        descriptor;                                 ///< Memoryd escriptor

    //
    // Methods
    //

    MultimodeVectorArgument();

    MultimodeVectorArgument(
        void           *pointer_,
        int64_t         batch_stride_,
        PointerModeID   pointer_mode_,
        int32_t         rank_,
        uint64_t        descriptor_ = 0ull
    );

    /// Helper to create a MultimodeVectorArgument as a scalar
    static MultimodeVectorArgument as_scalar(
        void            *pointer_,
        PointerModeID    pointer_mode_ = PointerModeID::HOST
    );

    /// Helper to create a MultimodeVectorArgument as a scalar
    static MultimodeVectorArgument as_vector(
        void            *pointer_,
        int64_t          batch_stride_,
        PointerModeID    pointer_mode_ = PointerModeID::HOST
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Arguments for a built in functor
struct FunctorArgument {

    ir::BuiltinOperatorID op_id;
    ScalarArgument    state;

    //
    // Methods
    //

    /// Default ctor
    FunctorArgument();

    /// Ctor
    FunctorArgument(
        ir::BuiltinOperatorID   op_id_,
        ScalarArgument          state_ = ScalarArgument()
    );
};

struct FunctorDescription {
    FunctorKindID     kind;
    ScalarDescription state;
    //
    // Methods
    //

    FunctorDescription();

    FunctorDescription(
        FunctorKindID kind_,
        ScalarDescription state_ = ScalarDescription()
    );

    bool operator==(FunctorDescription const &rhs) const;
    bool operator!=(FunctorDescription const &rhs) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Arguments used by kernels supporting Split-K
struct SplitKArguments {
    int32_t slices;                                       ///< Number of slices to partition the serial reduction into
    int32_t buffers;                                      ///< Number of output workspaces to write into
    int32_t kernels;                                      ///< Number of kernels needed overall (1 or 2)

    //
    // Methods
    //

    /// Default ctor
    SplitKArguments();

    /// Ctor
    SplitKArguments(
        int32_t slices_,                                  ///< Number of slices to partition the serial reduction into
        int32_t buffers_,                                 ///< Number of output workspaces to write into
        int32_t kernels_                                  ///< Number of kernels needed overall (1 or 2)
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

using SegmentKArguments = cask::Operation::Description::SegmentK;
  
/////////////////////////////////////////////////////////////////////////////////////////////////

/// Matrix multiply accumulate instruction description
struct MmaInstructionDescription {
    MmaShape            shape;                            ///< Shape of the matrix multiply
    int32_t             threads;                          ///< Number of participating threads
    ir::NumericTypeID   input;                            ///< Data type of A and B
    ir::NumericTypeID   accumulator;                      ///< Accumulation type (C)

    //
    // Methods
    //

    /// Default ctor
    MmaInstructionDescription();

    /// Ctor
    MmaInstructionDescription(
        MmaShape            shape_,
        int32_t             threads_,
        ir::NumericTypeID   input_,
        ir::NumericTypeID   accumulator_
    );

    bool operator==(MmaInstructionDescription const &rhs) const ;
    bool operator!=(MmaInstructionDescription const &rhs) const ;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

struct MmaDescription {
    MmaShape                    cta_shape;                ///< CTA tile shape in elements
    MmaShape                    warp_count;               ///< Number of warps dividing CTA tile
    MmaInstructionDescription   instruction;              ///< Description of the matrix multiply instruction
    int32_t                     stages;                   ///< Number of stages in pipeline

    //
    // Methods
    //

    /// Default ctor
    MmaDescription();

    /// Ctor
    MmaDescription(
        MmaShape         const &            cta_shape_,           ///< CTA tile shape in elements
        MmaShape         const &            warp_count_,          ///< Number of warps dividing CTA tile
        MmaInstructionDescription   const & instruction_,         ///< Description of the matrix multiply instruction
        int32_t                             stages_               ///< Number of stages in pipeline
    );

    bool operator==(MmaDescription const &rhs) const;
    bool operator!=(MmaDescription const &rhs) const ;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Base class for
struct OperationDescription {

    ir::CudaScopeID             lwca_scope;                       ///< Describes the CUDA scope
    OperationKindID             operation_kind;                   ///< Operation Kind
    ir::ComputeCapabilityRange  cc_range;                         ///< Compute capability range

    //
    // Methods
    //

    /// Default ctor
    OperationDescription();

    /// Ctor
    OperationDescription(
        ir::CudaScopeID lwca_scope_,
        OperationKindID operation_kind_,
        ir::ComputeCapabilityRange cc_range_
    );

    bool operator==(OperationDescription const &rhs) const;
    bool operator!=(OperationDescription const &rhs) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Description structure for GEMMs
struct GemmDescription : public OperationDescription {
    GemmKindID              gemm_kind;                    ///< Kind of GEMM
    MmaDescription          mma;                          ///< Description
    TensorDescription       A;
    TensorDescription       B;
    TensorDescription       C;
    TensorDescription       D;
    VectorDescription       alpha;
    VectorDescription       beta;
    VectorDescription       bias;

    //
    // Methods
    //

    /// Default ctor
    GemmDescription();

    /// Ctor
    GemmDescription(
        GemmKindID              gemm_kind_,
        MmaDescription          mma_,
        TensorDescription       A_,
        TensorDescription       B_,
        TensorDescription       C_,
        TensorDescription       D_,
        VectorDescription       alpha_,
        VectorDescription       beta_,
        VectorDescription       bias_
    );

    bool operator==(GemmDescription const &rhs) const;
    bool operator!=(GemmDescription const &rhs) const;
};

/// Arguments for matrix multiply operations (the basis of GEMMs)
struct GemmArguments {
    MmaShape                    problem;                      ///< M-by-N-by-K problem size
    int32_t                     batch_count;                  ///< Number of batch indices
    GemmModeID                  gemm_mode;                    ///< describes the interpretation of batching and poitners
    SplitKArguments             split_k;                      ///< Split-K arguments
    SegmentKArguments           segment_k;                    ///< Segment-K arguments
    TensorArgument              A;                            ///< Arguments for A tensor
    TensorArgument              B;                            ///< Arguments for B tensor
    TensorArgument              C;                            ///< Arguments for C tensor
    TensorArgument              D;                            ///< Arguments for D tensor
    MultimodeVectorArgument     alpha;
    MultimodeVectorArgument     beta;
    VectorArgument              bias;

    //
    // Methods
    //

    /// Default ctor
    GemmArguments();

    /// Ctor
    GemmArguments(
        MmaShape         const & problem_,
        int32_t                  batch_count_,
        GemmModeID               gemm_mode_,
        SplitKArguments  const & split_k_,
        SegmentKArguments  const & segment_k_,
        TensorArgument   const & A_,
        TensorArgument   const & B_,
        TensorArgument   const & C_,
        TensorArgument   const & D_,
        MultimodeVectorArgument const & alpha_,
        MultimodeVectorArgument const & beta_,
        VectorArgument          const & bias_
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Description for convolution kernels
struct ConvolutionDescription : public GemmDescription {

    static int const COLW_RANK = 3;                       ///< Number of spatial dimensions

    ConvolutionKindID       convolution_kind;             ///< Indicates convolution kind

    int32_t                 threadblock_groups;           ///< Number of groups per threadblock

    int32_t                 filter_size[3];               ///< Filter size. If uniformly zero,
                                                          ///< filter size is a runtime argument.

    int32_t                 stride[3];                    ///< Stride. If uniformly zero, stride is
                                                          ///< a runtime argument.

    int32_t                 dilation[3];                  ///< Dilation. If uniformly zero, dilation
                                                          ///< is a runtime argument.

    //
    // Methods
    //

    /// Default ctor
    ConvolutionDescription();

    /// Ctor
    ConvolutionDescription(
        ConvolutionKindID               convolution_kind_,
        int32_t                         threadblock_groups_,
        std::array<int32_t, COLW_RANK>  filter_size_
    );

    /// Ctor
    ConvolutionDescription(
        ConvolutionKindID               convolution_kind_,
        int32_t                         threadblock_groups_,
        std::array<int32_t, COLW_RANK>  filter_size_,
        std::array<int32_t, COLW_RANK>  stride_,
        std::array<int32_t, COLW_RANK>  dilation_
    );

    bool operator==(ConvolutionDescription const &rhs) const;
    bool operator!=(ConvolutionDescription const &rhs) const;
};

/// Convolution problem size tuple
struct ConvolutionProblemSize {

    int64_t N;                                            ///< Activation batch count
    int64_t D;                                            ///< Activations tensor depth
    int64_t H;                                            ///< Activations tensor height
    int64_t W;                                            ///< Activations tensor width
    int64_t C;                                            ///< Activations tensor channels
    int64_t O;                                            ///< Output layer depth
    int64_t P;                                            ///< Output layer height
    int64_t Q;                                            ///< Output layer width
    int64_t K;                                            ///< Filter count
    int64_t T;                                            ///< Filter tensor depth
    int64_t R;                                            ///< Filter tensor height
    int64_t S;                                            ///< Filter tensor width
    int64_t G;                                            ///< Groups

    //
    // Methods
    //

    /// Default ctor
    ConvolutionProblemSize();

    /// Ctor
    ConvolutionProblemSize(
        int64_t N_,
        int64_t H_,
        int64_t W_,
        int64_t C_,
        int64_t P_,
        int64_t Q_,
        int64_t K_,
        int64_t R_,
        int64_t S_
    );

    /// Ctor
    ConvolutionProblemSize(
        int64_t N_,
        int64_t D_,
        int64_t H_,
        int64_t W_,
        int64_t C_,
        int64_t O_,
        int64_t P_,
        int64_t Q_,
        int64_t K_,
        int64_t T_,
        int64_t R_,
        int64_t S_,
        int64_t G_
    );

    /// Helper to construct a 2D convolution problem size
    ConvolutionProblemSize(
        std::array<int64_t, 4> const &activation_,       ///< {N, H, W, C}
        std::array<int64_t, 2> const &filter_,           ///< {R, S}
        std::array<int64_t, 4> const &output_,           ///< {N, P, Q, K}
        int64_t G_ = 1                                   ///< Groups
    );

    /// Helper to construct a 3D convolution problem size
    ConvolutionProblemSize(
        std::array<int64_t, 5> const &activation_,       ///< {N, D, H, W, C}
        std::array<int64_t, 3> const &filter_,           ///< {T, R, S}
        std::array<int64_t, 5> const &output_,           ///< {N, O, P, Q, K}
        int64_t G_ = 1                                   ///< Groups
    );

    /// Gets the activation tensor extent {N, D, H, W, C}
    std::array<int64_t, 5> activation() const;

    /// Gets the filter shape ignoring filter count and channels {T, R, S}
    std::array<int64_t, 3> filter() const;

    /// Gets the output tensor extent {N, O, P, Q, K}
    std::array<int64_t, 5> output() const;

    bool operator==(ConvolutionProblemSize const &rhs) const;
    bool operator!=(ConvolutionProblemSize const &rhs) const;
};

/// Convolution arguments
struct ConvolutionArguments {

    static int const COLW_RANK = ConvolutionDescription::COLW_RANK;

    ConvolutionProblemSize              problem;        ///< Convolution problem size
    SplitKArguments                     split_k;        ///< Split-K arguments
    SegmentKArguments                   segment_k;      ///< Segment-K arguments
    std::array<int32_t, COLW_RANK>      pad;            ///< Padding tuple
    std::array<int32_t, COLW_RANK>      stride;         ///< Stride tuple
    std::array<int32_t, COLW_RANK>      dilation;       ///< Dilation tuple
    ConvolutionModeID                   mode;           ///< Convolution mode

    TensorArgument                      A;              ///< Arguments for A tensor
    TensorArgument                      B;              ///< Arguments for B tensor
    TensorArgument                      C;              ///< Arguments for C tensor
    TensorArgument                      D;              ///< Arguments for D tensor

    ScalarArgument                      alpha;          ///< Alpha scalar
    ScalarArgument                      beta;           ///< Beta scalar

    //
    // Methods
    //

    /// Default ctor
    ConvolutionArguments();

    /// Ctor
    ConvolutionArguments(
        ConvolutionProblemSize              const & problem_,
        SplitKArguments                     split_k_,
        SegmentKArguments                   segment_k_,
        std::array<int32_t, COLW_RANK>      pad_,
        std::array<int32_t, COLW_RANK>      stride_,
        std::array<int32_t, COLW_RANK>      dilation_,
        ConvolutionModeID                   mode_,

        TensorArgument                      A_,
        TensorArgument                      B_,

        TensorArgument                      C_,
        TensorArgument                      D_,

        ScalarArgument                      alpha_,
        ScalarArgument                      beta_
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Reduction kernels
struct ReductionDescription : public OperationDescription {

    TensorDescription       A;
    TensorDescription       B;
    TensorDescription       C;
    TensorDescription       D;
    TensorDescription       workspace;

    //
    // Methods
    //

    /// Default ctor
    ReductionDescription();

    /// Ctor
    ReductionDescription(
        TensorDescription       A_,
        TensorDescription       B_,
        TensorDescription       C_,
        TensorDescription       D_,
        TensorDescription       workspace_
    );

    bool operator==(ReductionDescription const &rhs) const;
    bool operator!=(ReductionDescription const &rhs) const;
};

/// Reduction arguments
struct ReductionArguments {

    TensorArgument     D;                                 ///< D tensor argument
    FunctorArgument    op_output;                         ///< Fused built-in operator D

    TensorArgument     A;                                 ///< A tensor argument
    FunctorArgument    op_A;                              ///< Fused built-in operator A

    TensorArgument     B;                                 ///< B tensor argument
    FunctorArgument    op_B;

    TensorArgument     C;                                 ///< C tensor argument
    FunctorArgument    op_C;

    FunctorArgument    op_binary;                         ///< Binary operator ID
    FunctorArgument    op_reduce;                         ///< Reduction operator ID

    ScalarArgument     reduction_identity;                ///< Reduction identity element

    //
    // Methods
    //

    /// Default ctor
    ReductionArguments();

    /// Ctor
    ReductionArguments(
        TensorArgument     D_,
        FunctorArgument    op_output_,

        TensorArgument     A_,
        FunctorArgument    op_A_,

        TensorArgument     B_,
        FunctorArgument    op_B_,

        TensorArgument     C_,
        FunctorArgument    op_C_,

        FunctorArgument    op_binary_,
        FunctorArgument    op_reduce_,

        ScalarArgument     reduction_identity_
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Block description for elementwise operations
struct ElementwiseBlockDescription {
    uint32_t    ndim_tile;              /// Dimensionality of the tile (currently 1D or 2D)
    int32_t     tile_size_0;            /// Size of a 3D tile
    int32_t     tile_size_1;
    int32_t     tile_size_2;
    uint32_t    num_threads;            /// Number of participating threads
    uint32_t    vec;                    /// Unknown
    int32_t     transpose;              ///

    //
    // Methods
    //

    ElementwiseBlockDescription();

    ElementwiseBlockDescription(
        uint32_t    ndim_tile_,
        int32_t     tile_size_0_,
        int32_t     tile_size_1_,
        int32_t     tile_size_2_,
        uint32_t    num_threads_,
        uint32_t    vec_,
        int32_t     transpose_
    );

    bool operator==(ElementwiseBlockDescription const &rhs) const;
    bool operator!=(ElementwiseBlockDescription const &rhs) const;
};

struct ElementwiseDescription : public OperationDescription {

    FunctorDescription              op_ABC;
    FunctorDescription              op_AB;

    TensorDescription               A;
    FunctorDescription              op_A;

    TensorDescription               B;
    FunctorDescription              op_B;

    TensorDescription               C;
    FunctorDescription              op_C;

    ir::NumericTypeID               compute;
    ElementwiseBlockDescription     block;

    //
    // Methods
    //

    /// Default ctor
    ElementwiseDescription();

    /// Ctor
    ElementwiseDescription(

        FunctorDescription              op_ABC_,
        FunctorDescription              op_AB_,

        TensorDescription               A_,
        FunctorDescription              op_A_,

        TensorDescription               B_,
        FunctorDescription              op_B_,

        TensorDescription               C_,
        FunctorDescription              op_C_,

        ir::NumericTypeID               compute_,
        ElementwiseBlockDescription     block_ = ElementwiseBlockDescription()
    );

    bool operator==(ElementwiseDescription const &rhs) const;
    bool operator!=(ElementwiseDescription const &rhs) const;
};

/// Problem size structure for Elementwise operations
struct ElementwiseProblemSize {
    static int32_t const MAX_RANK = 12;

    int32_t rank;                                   ///< Number of non-zero extents
    int32_t extent[MAX_RANK];                       ///< Tensor extent  (a.k.a. shape)


    //
    // Methods
    //

    /// Default construct a zero-rank problem size
    ElementwiseProblemSize();

    /// Ctor from rank and extent array
    ElementwiseProblemSize(
        int32_t rank_,
        int32_t const *extent_
    );

    /// Ctor from half-open range
    ElementwiseProblemSize(
        int32_t const *start,
        int32_t const *end
    );
};

/// Elementwise arguments
struct ElementwiseArguments {

    ElementwiseProblemSize problem;

    TensorArgument D;

    FunctorArgument    op_ABC;
    FunctorArgument    op_AB;

    TensorArgument     A;
    FunctorArgument    op_A;

    TensorArgument     B;
    FunctorArgument    op_B;

    TensorArgument     C;
    FunctorArgument    op_C;

    //
    // Methods
    //

    /// Default ctor
    ElementwiseArguments();

    /// Ctor
    ElementwiseArguments(
        ElementwiseProblemSize const & problem_,
        TensorArgument      D_,
        FunctorArgument     op_ABC_,
        FunctorArgument     op_AB_,
        TensorArgument      A_,
        FunctorArgument     op_A_,
        TensorArgument      B_,
        FunctorArgument     op_B_,
        TensorArgument      C_,
        FunctorArgument     op_C_
    );
};

/////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace sdk
} // namespace cask

/////////////////////////////////////////////////////////////////////////////////////////////////
