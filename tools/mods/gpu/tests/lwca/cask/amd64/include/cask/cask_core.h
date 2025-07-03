#ifndef INCLUDE_GUARD_CASK_CORE_H
#define INCLUDE_GUARD_CASK_CORE_H

// enable doxygen:
//! @file

#if !defined(INCLUDE_GUARD_CASK_H)
# error "Never use <cask/cask_core.h> directly; include <cask.h> instead."
#endif

#include <stdint.h>             // cstdint is not in c++ 03
#include <vector>               // for the declaration of ShaderList
#include <map>
#include <array>
#include <string>
#include <memory>
#include <lwca.h>               // for LWmodule and LWfunction
#include <cassert>

#include "cask/safe/shared_types.h"

//////////////////////////////////////////////////////////////////////////////
//
// General cask stuff that almost all the headers depend on
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// MACROS
//
//////////////////////////////////////////////////////////////////////////////

#ifdef CASK_NAMESPACE
#define concat_tok(a, b) a ## b
#define mkcasknamespace(pre, ns) concat_tok(pre, ns)
#define cask mkcasknamespace(cask_, CASK_NAMESPACE)
#define cask_plugin mkcasknamespace(cask_plugin_, CASK_NAMESPACE)
#ifndef cask_safe
#define cask_safe mkcasknamespace(cask_safe_, CASK_NAMESPACE)
#endif
#endif

#ifndef CASK_UNUSED
#define CASK_UNUSED(expr) do { (void)(expr); } while (0)
#endif

//////////////////////////////////////////////////////////////////////////////

#if !defined(DLL_EXPORT)
#if defined(_WIN32)             // see https://sourceforge.net/p/predef/wiki/OperatingSystems/
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif  // _WIN32
#endif  // DLL_EXPORT

#if !defined(CASK_INIT_DLL_EXPORT)
#if (defined(_WIN32) && defined(CASK_INIT_DLL_EXPORT_DEF))
#define CASK_INIT_DLL_EXPORT __declspec(dllexport)
#else
#define CASK_INIT_DLL_EXPORT
#endif  // _WIN32
#endif  // DLL_EXPORT
//////////////////////////////////////////////////////////////////////////////

// enable doxygen:
//! This is the namespace for all CASK functions, class declarations, and objects
//////////////////////////////////////////////////////////////////////////////
/// \brief The CASK namespace contains all CASK functions and classes.
///
/// See the CASK API main page for details.
///
//////////////////////////////////////////////////////////////////////////////
namespace cask {

//////////////////////////////////////////////////////////////////////////////

class KernelInfo;
class BaseKernelInfo;
class GemmKernelInfo;
class ColwKernelInfo;
class ColwWgradKernelInfo;
class ColwDgradKernelInfo;
class PoolingKernelInfo;
class GettKernelInfo;

using PreferentialBool = cask_safe::PreferentialBool;

//////////////////////////////////////////////////////////////////////////////
/// \breif serializableHostBuf is a opaque datatype.
///
/// Just typedefing void* so that it is clearer in the declarations which kind
/// of buffer is needed in each case.  (Host side or device side).
typedef void* serializableHostBuf;
typedef void* serializableDeviceBuf;

//////////////////////////////////////////////////////////////////////////////
//
// Enum definitions
//
// Note: we are using C++03-friendly wrapper which serves as a drop-in
// replacement for the C++11 enum class.
//
//////////////////////////////////////////////////////////////////////////////

enum SMid {
    SM_50 = 0x0001,
    SM_52 = 0x0002,
    SM_53 = 0x0004,
    SM_60 = 0x0008,
    SM_61 = 0x0010,
    SM_62 = 0x0020,
    SM_70 = 0x0040,
    SM_72 = 0x0080,
    SM_75 = 0x0100,
    SM_80 = 0x0200,
    SM_86 = 0X0400,
    SM_87 = 0x0800,
    SM_90 = 0x1000
};

/**
 * \brief This enum maps to Operation direct
 *        subclasses.
 */
enum OpDefinition {
    OP_GEMM,
    OP_COLW,
    OP_DGRAD,
    OP_WGRAD,
    OP_DECOLW,
    OP_POOL,
    OP_SOFTMAX,
    OP_ELEMENTWISE,
    OP_GETT,
    OP_REDUCE,
    OP_GRAPH,
    OP_REDUCE_PER_TILE,
    OP_ILWALID,
};

template<typename D>
class
SafeEnum : public D {

public:
    typedef typename D::Label Label;
    SafeEnum() : mValue(static_cast<Label>(0)) { }
    explicit SafeEnum(int32_t value) : mValue(static_cast<Label>(value)) { }
    SafeEnum(Label value) : mValue(value) { }
    operator Label() const { return this->mValue; }

    bool operator==(SafeEnum const &rhs) const {
        return this->mValue == rhs.mValue;
    }

    bool operator==(Label const & rhs) const {
        return this->mValue == rhs;
    }

    bool operator!=(SafeEnum const &rhs) const {
        return this->mValue != rhs.mValue;
    }

    bool operator!=(Label const & rhs) const {
        return this->mValue != rhs;
    }

private:
    Label mValue;
};

//#if __cplusplus < 201103L
// NOTE: add DLL_EXPORT to remove warning on MS-build
#define ENUMCLASS( name, ... )                                                                     \
    struct DLL_EXPORT name##_ENUMCLASS_SCOPEWRAPPER {                                              \
        enum Label __VA_ARGS__;                                                                    \
    }; \
    typedef SafeEnum<name ## _ENUMCLASS_SCOPEWRAPPER> name
//#else
//#define ENUMCLASS(name, ...) enum class name __VA_ARGS__;
//#endif


//////////////////////////////////////////////////////////////////////////////
/// \enum Error
/// \brief Error types used throughout the library
///
/// The Error enum is the typical return type for member functions in
/// CASK. `Error::OK` is used to indicate that a function completed
/// as expected and all return parameters are valid.
//////////////////////////////////////////////////////////////////////////////
struct ErrorEnum {
    enum Label {
        /// \brief No error to report, member function completed
        /// successfully.
        OK = 0,
        ALIGNMENT,           ///< Unsupported memory alignment or padding
        BATCH_SIZE,          ///< Unsupported or inconsistent batch sizes
        CANT_PAD,            ///< Unsupported or inconsistent colwolution
        SPARSITY,            ///< Unsupported tensor sparse ratio
        DIMS_UNSUPPORTED,    ///< Unsupported number of dimensions.
        NULL_PTR,            ///< The user passed a null pointer.
        VECT_UNSUPPORTED,    ///< Unsupported vectorization.
        ///< "padding"
        STRIDE_UNSUPPORTED,  ///< Stride is not supported
        DILATION_UNSUPPORTED,///< Dilation is not supported
        CONST_SIZE,          ///< Shader lacks sufficient constant memory
        ///< space
        PLATFORM,            ///< Get specific error by calling
        ///< `lwdaGetLastError()`` method
        FILTER_OK,           ///< Filter ok, but needs transpose
        FILTER_SIZE,         ///< Unsupported filter size
        FMT_UNSUPPORTED,     ///< Unsupported or inconsistent tensor format
        GROUPS,              ///< Unsupported or inconsistent groups
        ///  specification
        INTERNAL,            ///< An assertion in the cask implementation
        SCALAR_TYPES,        ///< Unsupported scalar type colwersion
        SHADER_LOADED,       ///< Shader already loaded
        SHADER_NOT_LOADED,   ///< Shader not yet loaded
        SIMD_TYPES,          ///< Unsupported simd type colwerions
        SIZE_MISMATCH,       ///< Two or more of the tensors have dimensions
        ///  that should match, but don't (For example A
        ///  should be MxK, B should be KxN, C should be
        ///  MxN, but either M, K, or N don't match, or
        ///  the C or K dimensions of the input and
        ///  filter, or filter and outputs don't match.
        STRIDES,                ///< Inconsistent tensor strides
        XCORRELATION_ONLY,      ///< Shader only supports cross correlation
        PAD_C,                  ///< the per group C is too small, needs padding
        NO_PER_CHANNEL_SCALING, ///< this kernel doesn't support per channel scaling
        PER_CHANNEL_SCALING,    ///< this kernel only supports per channel scaling
        POOLING_MODE,           ///< The pooling kernel mode is unsupported.
        RELU_BIAS,              ///< this kernel does not support relu bias
        RELU_ILWALID,           ///< ReLu setting/value is not valid
        SOFTMAX_MODE,           ///< The softmax kernel mode/algorithm is unsupported
        SPLIT_H,                ///< Unsupported or inconsistent SplitH request
        SPLIT_K,                ///< Unsupported or inconsistent SplitK request
        SPLIT_K_BETA,           ///< Unsupported SplitK beta vaule
        COMPUTE_CAPABILITY,     ///< Unsupported operation under current compute capability
        PLANAR_COMPLEX,          ///< Unsupported or wrong settings for planar complex
        BAD_RUNINFO,            ///< Problem with the provided runinfo struct.
        DYNAMIC_RESIZE_UNSUPPORTED, ///< Dynamic resize feature unsupported
        BUFFER_CORRUPTION,      ///< The serialized host buffer is corrupted
        BAD_ALPHA_BETA,         ///< Unsupported alpha or beta value
        FUNCTIONAL_TREE_UNSUPPORTED,  ///< Nothing wrong per se, but the given request cannot be fulfilled by the tree
        FUNCTIONAL_TREE,        ///< Problem with the functional tree
        TENSOR_SIZE,            ///< Tensor size is not supported
        TENSOR_SIZE_UNSUPPORTED,///< Unsupported tensor size
        BIAS_TYPE,              ///< Unsupported bias type
        ILWALID_RUNNER_SETUP,   ///< Fail to setup runner
        ILWALID_PARAMETER,
        NO_PADDING_VALUE,       ///< this kernel doesn't support lwstomized padding value
        RESERVED_BUFFER_SIZE,   ///< Insufficient reserved buffer for parameters
        CGA_ERRORS_BEGIN,       ///< Denotes the beginning of the CGA related errors
        CGA_NOT_SUPPORTED = CGA_ERRORS_BEGIN, ///< CGA capabilities are not supported
        CGA_SIZE_UNSUPPORTED,   ///< The configured CGA dimensions are not supported
        CGA_SIZE_INCONSISTENT,  ///< The configured CGA dimensions are inconsistent
        CGA_ERRORS_END,         ///< Denotes the end of the CGA related errors
        DEVICE_POINTER_MODE,    ///< Denotes an issue with utilizing device pointer mode
        PGS_ERRORS_BEGIN,       ///< Denotes the beginning of programmatic grid sync errors
        PGS_NOT_SUPPORTED = PGS_ERRORS_BEGIN, //< Programmatic grid sync features are not supported
        PGS_ERRORS_END,         ///< Denotes the end of the programmatic grid sync errors
        NOT_IMPLEMENTED,           ///< Unsupported feature
        /// Below are error code returned by cask::ir APIs
        IR_ERRORS_BEGIN,
        IR_NULL_REFERENCE = IR_ERRORS_BEGIN, ///< nullptr contained by cask::ir::Reference<T>
        IR_ILWALID_DESCRIPTOR,          ///< Descriptor is invalid
        IR_ILWALID_IDENTIFIER,          ///< Bad name for variables, e.g. tensor, element, etc.
        IR_MULTIPLE_DEFINITION,         ///< A variable/function that is already defined
        IR_MULTIPLE_BINDING,            ///< A variable that is already bound to a context
        IR_UNSUPPORTED_SCALAR_TYPE,     ///< ScalarType isn't support
        IR_UNSUPPORTED_VECTOR_SIZE,
        IR_UNSUPPORTED_TILE_KIND,
        IR_JIT_COMPILE,
        IR_TYPE_MISMATCH,
        IR_INCOMPLETE_TYPE,
        IR_ILWALID_ARGUMENT,
        IR_ERRORS_END,
        SEGMENTK_ERRORS_BEGIN,  ///< Denotes the start of the Segment-K related errors
        SEGMENTK_NOT_SUPPORTED = SEGMENTK_ERRORS_BEGIN, ///< Segment-K not supported
        SEGMENTK_ILWALID_ARGUMENTS, ///< Invalid arguments were given.
        SEGMENTK_ERRORS_END,     ///< Denotes the end of the Segment-K related errors.
        FEATURE_DISABLED_IN_COMPILATION, ///< Feature is disabled at compile time
        BAD_SHADER,              ///< A shader from shaderlist cannot dynamic_cast to valid specific shader
        OPERATION_UNSUPPORTED,   ///< An operation that is not be fully supported
        KLIB_CRC_ERROR,                                ///< Kernel lib CRC value is incorrect
        KLIB_VERSION_MISMATCH,                         ///< Kernel lib version is supported in this CASK build
        KLIB_KERNEL_TYPE_ILWALID,                      ///< Kernel type value is incorrect
        KLIB_HEADER_ILWALID,                           ///< Kernel lib header is invalid
        KLIB_KERNEL_INDEX_ILWALID,                     ///< The kernel index is invalid
        KLIB_KERNEL_INFO_DATA_SIZE_ILWALID,            ///< The size of kernel info data is invalid
        KLIB_ILWALID_KERNEL_INFO,                      ///< The kernel info specified is invalid
        KLIB_KERNEL_DATA_SIZE_ILWALID,                 ///< The size of kernel data is invalid
        KLIB_NOT_ALL_KERNEL_HEADER_UPDATED,            ///< Some kernels' offesets are not updated in the kernel lib header
        KLIB_KERNEL_HEADER_ALREADY_UPDATED,            ///< The kernel's offset has been updated
        KLIB_KERNEL_HEADER_UPDATE_OUT_OF_SEQUENCE,     ///< The updates to kernels' offsets are not updated in order
        ILWALID_NUM_MODES,    ///< The number of modes in Gett kernels is ilwald
    };

};
typedef SafeEnum<ErrorEnum> Error;

/// Get an error string for an Error.
///
/// \returns NULL if the error is unknown
/// \returns Const pointer to static string if the error is known. (The
/// caller may not free the static const string.)
const char* getErrorString(Error error);

//////////////////////////////////////////////////////////////////////////////
// We don't actually want to expose the definitions in
// kernels/gemm_shader_params.hpp to the client, but we want to use the same
// enum values to make implementation less error prone (so we won't need to
// colwert back and forth with Sass kernels that use the struct ShaderParams.
//////////////////////////////////////////////////////////////////////////////

enum class PointerModeID {
    HOST,                                 ///< Scalar refer to host-side data
    DEVICE                                ///< Pointers refer to device-side data
};

namespace sass_private {
#if !defined(assert)
// gemm_shader_params has implementation in it, and the implementation has
// assert(), but if user of this header hasn't defined assert, we can't polute
// their namespace with it.  So do this awfulness:
#define cask_assert_hack 1
#define assert(x)
#endif
#include "kernels/gemm_shader_params.hpp"
#if defined(cask_assert_hack)
#undef assert
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Collection of built-in operators used by some multi-mode kernels. The integer values
/// for each enumerant are driven by lwTENSOR's definition.
enum class BuiltinOperatorID {

    NOP = 0,                            ///< No operation; same as identity operation. Interacts well with
                                        ///< zero initialization of BuiltinOperatorIDs
    //
    // Unary
    //
    IDENTITY = 1,                       ///< Identity operator (i.e., elements are not changed)
    SQRT = 2,                           ///< Square root
    RELU = 8,                           ///< Rectified linear unit
    CONJ = 9,                           ///< Complex conjugate
    RCP = 10,                           ///< Reciprocal
    SIGMOID = 11,                       ///< y=1/(1+exp(-x))
    TANH = 12,                          ///< y=tanh(x)
    EXP = 22,                           ///< Exponentiation.
    LOG = 23,                           ///< Log (base e).
    ABS = 24,                           ///< Absolute value.
    NEG = 25,                           ///< Negation.
    SIN = 26,                           ///< Sine.
    COS = 27,                           ///< Cosine.
    TAN = 28,                           ///< Tangent.
    SINH = 29,                          ///< Hyperbolic sine.
    COSH = 30,                          ///< Hyperbolic cosine.
    ASIN = 31,                          ///< Ilwerse sine.
    ACOS = 32,                          ///< Ilwerse cosine.
    ATAN = 33,                          ///< Ilwerse tangent.
    ASINH = 34,                         ///< Ilwerse hyperbolic sine.
    ACOSH = 35,                         ///< Ilwerse hyperbolic cosine.
    ATANH = 36,                         ///< Ilwerse hyperbolic tangent.
    CEIL = 37,                          ///< Ceiling.
    FLOOR = 38,                         ///< Floor.
    LOGICAL_NOT = 39,                   ///< Returns 1 if the element is zero.
                                        ///< Returns zero otherwise
    //
    // Binary
    //

    ADD = 3,                            ///< Addition of two elements
    MUL = 5,                            ///< Multiplication of two elements
    MAX = 6,                            ///< Maximum of two elements
    MIN = 7,                            ///< Minimum of two elements

    LOGICAL_AND = 50,                   ///< Returns 1 if both operands are non-zero.
    LOGICAL_NAND = 51,                  ///< Returns 0 if both operands are non-zero
    LOGICAL_OR = 52,                    ///< Returns 1 if any operands is non-zero
    LOGICAL_NOR = 53,                   ///< Returns 0 if any operands is non-zero
    LOGICAL_XOR = 54,                   ///< Exclusive OR of two operands

    CMP_GT = 60,                        ///< Greater than          (a >  b)
    CMP_GE = 61,                        ///< Greater than or equal (a >= b)
    CMP_EQ = 62,                        ///< Equal                 (a == b)
    CMP_NE = 64,                        ///< Not equal             (a != b)
    GMP_LT = 65,                        ///< Less than             (a <  b)
    GMP_LE = 66,                        ///< Less than or equal    (a <= b)

    //
    // Misc
    //

    UNKNOWN = 126,                      ///< reserved for internal use only

    INLINE = 128,                       ///< Operator is compile-time inlined
    DEVICE_FUNCTION = 129               ///< Operator is a device-side function call
} ;

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Layout ID enumerant
ENUMCLASS(LayoutID, {
  DEGENERATE = 0,
  AFFINE_1 = 1,
  AFFINE_2 = 2,
  AFFINE_3 = 3,
  AFFINE_4 = 4,
  AFFINE_5 = 5,
  AFFINE_6 = 6,
  AFFINE_7 = 7,
  AFFINE_8 = 8,
  AFFINE_9 = 9,
  AFFINE_10 = 10,
  AFFINE_11 = 11,
  AFFINE_12 = 12,
  AFFINE_16 = 16,
  AFFINE_20 = 20,
  AFFINE_28 = 28,
  VECTOR    = 100,

  COLUMN_MAJOR       = 200,
  COLUMN_MAJOR_K32   = 201,

  ROW_MAJOR          = 300,
  ROW_MAJOR_K32      = 301,

  COLUMN_MAJOR_SPARSE_2_1             = 410,
  COLUMN_MAJOR_COMPRESSED_2_1_R1      = 411,
  COLUMN_MAJOR_COMPRESSED_2_1_R2      = 412,
  COLUMN_MAJOR_SPARSE_4_2             = 420,
  COLUMN_MAJOR_COMPRESSED_4_2_R1      = 421,
  COLUMN_MAJOR_COMPRESSED_4_2_R2      = 422,
  COLUMN_MAJOR_COMPRESSED_4_2_R3      = 423,
  COLUMN_MAJOR_COMPRESSED_4_2_R4      = 424,
  COLUMN_MAJOR_K32_SPARSE_4_2         = 430,
  COLUMN_MAJOR_K32_COMPRESSED_4_2_R1  = 431,

  ROW_MAJOR_SPARSE_2_1                = 510,
  ROW_MAJOR_COMPRESSED_2_1_R1         = 511,
  ROW_MAJOR_COMPRESSED_2_1_R2         = 512,
  ROW_MAJOR_SPARSE_4_2                = 520,
  ROW_MAJOR_COMPRESSED_4_2_R1         = 521,
  ROW_MAJOR_COMPRESSED_4_2_R2         = 522,
  ROW_MAJOR_COMPRESSED_4_2_R3         = 523,
  ROW_MAJOR_COMPRESSED_4_2_R4         = 524,
  ROW_MAJOR_K32_SPARSE_4_2            = 530,
  ROW_MAJOR_K32_COMPRESSED_4_2_R1     = 531,

  NHWC      = 1000,
  NCHW      = 1001,
  NCxHW2    = 1002,
  NCxHW4    = 1003,
  NCxHW8    = 1004,
  NCxHW16   = 1005,
  NCxHW32   = 1006,
  NCxHW64   = 1007,

  NDHWC     = 1010,
  NCDHW     = 1011,
  NCxDHW2   = 1012,
  NCxDHW4   = 1013,
  NCxDHW8   = 1014,
  NCxDHW16  = 1015,
  NCxDHW32  = 1016,
  NCxDHW64  = 1017,

  NxCHW2    = 1020,

  NHWCx4    = 1031,
  NHWCx8    = 1032,

  KRSC      = 2000,
  KCRS      = 2001,
  CRSK      = 2002,
  KCxRS2    = 2003,
  KCxRS4    = 2004,
  KCxRS8    = 2005,
  KCxRS16   = 2006,
  KCxRS32   = 2007,

  KxCRS2    = 2010,
  KxCRS4    = 2011,
  KxCRS8    = 2012,
  KxCRS16   = 2013,
  KxCRS32   = 2014,

  KRSCx4    = 2021,
  KRSCx8    = 2022,

  KTRSC     = 2030,
  KCTRS     = 2031,

  KCRS_SPARSE_4_2                = 2100,
  KCRS_COMPRESSED_4_2_R1         = 2101,
  KRSC_SPARSE_4_2                = 2110,
  KRSC_COMPRESSED_4_2_R1         = 2111,
  KCxRS8_SPARSE_4_2              = 2120,
  KCxRS8_COMPRESSED_4_2_R1       = 2121,
  KCxRS16_SPARSE_4_2             = 2130,
  KCxRS16_COMPRESSED_4_2_R1      = 2131,
  KCxRS32_SPARSE_4_2             = 2140,
  KCxRS32_COMPRESSED_4_2_R1      = 2141,
});

enum class TensorSparsityRatio {
    R_1_1 = 0,  // stand for dense tensor
    R_4_2 = 1,  // 4:2 sparsity
    R_2_1 = 2   // 2:1 sparsity
};

/// Layout offset function. Returns the linear offset of a coordinate in a tensor given a
/// stride vector
using LayoutOffsetFunc = int64_t (*)(int64_t const coord[], int64_t const stride[]);

/// Determines the strides of a packed tensor of a given size.
using LayoutPackedFunc = Error (*)(int64_t stride[], int64_t const tensor_size[]);

/// Structure describing layout properties
struct LayoutAttributes {

  static int const MAX_RANK = 12;

  LayoutID          layout{LayoutID::DEGENERATE};  ///< Layout ID enumerant
  char const *      name{nullptr};                 ///< Name (e.g. "ColumnMajor" or "Affine12")
  char const *      short_name{nullptr};           ///< Short name (e.g. "n" or "t")
  int32_t           rank{-1};                      ///< Number of dimensions of tensor coordinate
  int32_t           stride_rank{-1};               ///< Number of elements in the stride vector
  int32_t           vectorized_dim{-1};            ///< The dimension that has vectorized elements
  int32_t           scalars_per_element{1};        ///< The amount of contiguous scalars treated as single element
  TensorSparsityRatio sparsity{TensorSparsityRatio::R_1_1}; ///< The sparsity ratio of tensor
  LayoutOffsetFunc  offset{nullptr};               ///< See `LayoutOffsetFunc`
  LayoutPackedFunc  packed{nullptr};               ///< See `LayoutPackedFunc`

  //
  // Methods
  //

  LayoutAttributes();
  LayoutAttributes(LayoutID layout);
};

//////////////////////////////////////////////////////////////////////////////
/*
ENUMCLASS(GettLayout, {
    AFFINE_28 = LayoutID::AFFINE_28
});

MAKE_EQ_OPERATOR(LayoutID, GettLayout)
MAKE_NE_OPERATOR(LayoutID, GettLayout)
*/
//////////////////////////////////////////////////////////////////////////////
// A new set of metadata type defintions for substituting the ones defined
// in namespace sass_private
//////////////////////////////////////////////////////////////////////////////
namespace md {
#include "kernel_metadata.h"
}

//////////////////////////////////////////////////////////////////////////////

// FIXME: all these operator==() and operator!=() between cask enums and
// gemm_shader_params enums should be hidden down in the layers that actually
// implement sass kernels (and removed from the global view here).

// I can't figure out how to do this with a template without it getting
// implicitly instantiated where I don't want it (e.g. with int <-> void*
// colwersions).
//#if __cplusplus < 201103L
// Explicitly force colwersion to base structs's Label type to make C++[03] happy.
#define MAKE_EQ_OPERATOR(X, Y)                     \
    inline bool operator==(X x, Y y) {             \
        Y##_ENUMCLASS_SCOPEWRAPPER::Label tmp = y; \
        return x == static_cast<X>(tmp);           \
    }
#define MAKE_FLIP_EQ_OPERATOR(X, Y) \
    inline bool operator==(X x, Y y) { return y == x; }
//#else
//#define MAKE_EQ_OPERATOR(X, Y) inline bool operator==(X x, Y y) { return x == static_cast<X>(y); }
//#endif
#define MAKE_NE_OPERATOR(X, Y) inline bool operator!=(X x, Y y) { return !(x == y); }


//////////////////////////////////////////////////////////////////////////////
/// Used to specify the scalar type of a tensor.
ENUMCLASS(ScalarType, {
    FP64  = sass_private::R_64F,
    FP32  = sass_private::R_32F,
    FP16  = sass_private::R_16F,
    INT32 = sass_private::R_32I,
    INT8  = sass_private::R_8I,
    UINT8 = sass_private::R_8U,
    UINT16= sass_private::R_16U,
    CPINT8= sass_private::C_8I,
    CPINT32  = sass_private::C_32I,
    CPINT64 = sass_private::C_64I,
    CPUINT8 = sass_private::C_8U,
    CPUINT32 = sass_private::C_32U,
    CPUINT64 = sass_private::C_64U,
    CP16  = sass_private::C_16F,
    CP32  = sass_private::C_32F,
    CP64  = sass_private::C_64F,
    BF16  = sass_private::BF16_16F,
    CF16  = sass_private::C_BF16_16F,
    TF32  = sass_private::TF32_19F,
    CF32  = sass_private::C_TF32_19F,
    INT4  = sass_private::R_4I,
    UINT4 = sass_private::R_4U,
    UINT1 = sass_private::R_1U,
    INT64 = sass_private::R_64I,
    INT16 = sass_private::R_16I,
    UINT64 = sass_private::R_64U,
    UINT32 = sass_private::R_32U,
    E5M2 = sass_private::E5M2_8F,
    E4M3 = sass_private::E4M3_8F,
    INTPTR = sass_private::GEMM_DATATYPE_COUNT,
    VOID_TYPE,
    BOOL
});

//#if __cplusplus < 201103L
MAKE_EQ_OPERATOR(sass_private::elementDataType_t, ScalarType)
MAKE_FLIP_EQ_OPERATOR(ScalarType, sass_private::elementDataType_t)
//#else
//MAKE_EQ_OPERATOR(sass_private::elementDataType_t, ScalarType)
//MAKE_EQ_OPERATOR(ScalarType, sass_private::elementDataType_t)
//#endif
MAKE_NE_OPERATOR(ScalarType, sass_private::elementDataType_t)
MAKE_NE_OPERATOR(sass_private::elementDataType_t, ScalarType)
//////////////////////////////////////////////////////////////////////////////
/// \brief Return type size in bytes
int32_t
sizeInBytes(ScalarType sType);

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Indicates whether the numeric type is integer or floating point
enum class NumericClassID {
    INTEGER,
    FLOAT
};

/// Distinguishes between real and complex
enum class ComplexClassID {
    REAL,
    COMPLEX
};


/// Complex transformation
enum class ComplexTransform {
    NONE,                               ///< No transformation
    CONJ                                ///< Complex conjugate
};

/// Numeric traits structure describes properties of a numeric type
struct NumericTraits {

    ScalarType  type;                   ///< The `ScalarType` enumerant described here
    ScalarType  base;                   ///< The base 'real' type. Matches 'type' if real.
    NumericClassID numeric_class;       ///< Indicates whether float, int, or possibly fixed-point
    ComplexClassID complex;             ///< Indicates whether real or complex
    bool           is_signed;           ///< True if type is signed
    int32_t        bits;                ///< Total number of bits to store the type
    char const    *name;                ///< String representation of 'type' enumerant
    char const    *cpp;                 ///< Name of corresponding C++ type
    char const    *ptx;                 ///< Name of corresponding PTX type if it exists

    //
    // Methods
    //

    /// Default ctor for a void type
    NumericTraits();

    /// Default ctor for a void type
    NumericTraits(
        ScalarType type,
        ScalarType base_,
        NumericClassID numeric_,
        ComplexClassID complex_,
        bool signed_,
        int32_t bits_,
        char const *name_,
        char const *cpp_,
        char const *ptx_);

    /// Helper to construct a NumericTraits based on hard-coded traits known about a type
    NumericTraits(ScalarType type_);

    /// Helper to construct a NumericTraits based on hard-coded traits known about a type
    NumericTraits(char const *name_);

    /// Helper to construct a NumericTraits based on hard-coded traits known about a type
    NumericTraits(std::string const &name_);

    /// Determines the largest lossless type used to store losslessly within a Variant
    ScalarType variant_type() const;

    /// Gets the traits object from type
    static NumericTraits from_type(ScalarType type);

    /// Gets from string
    static NumericTraits from_string(char const *str);

    /// Gets from string
    static NumericTraits from_string(std::string const &str);
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Gets numeric traits for a given data type
template <typename T>
NumericTraits make_NumericTraits();

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Hasher implementing a left rotation. This is useful for composing other hash functions.
struct HasherBase {
    static size_t rotl(size_t x, int32_t shl) {
        auto kBits = int32_t(sizeof(size_t) * 8);
        return ((x << shl) | (x >> (kBits - shl)));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////

/// Shape of a rank=2 matrix tile
struct MatrixShape {

    int32_t row;                  ///< Number of rows
    int32_t column;               ///< Number of columns

    //
    // Methods
    //

    /// Default ctor
    MatrixShape();

    /// Ctor
    MatrixShape(int32_t row_, int32_t column_);

    /// Computes the sum of all elements
    int32_t sum() const;

    /// Computes the product of all elements
    int32_t product() const;

    /// Returns true if any element is nonzero
    bool any() const;

    /// Returns true if all elements are zero
    bool not_any() const;

    /// Returns true if all elements are nonzero
    bool all() const;

    /// Returns true if some elements are zero
    bool not_all() const;

    /// Returns the transpose
    MatrixShape transpose() const;

    /// Elementwise add
    MatrixShape operator+(MatrixShape const &rhs) const;

    /// Elementwise add
    MatrixShape operator-(MatrixShape const &rhs) const;

    /// Negate
    MatrixShape operator-() const;

    /// Elementwise multiply
    MatrixShape operator*(MatrixShape const &rhs) const;

    /// Elementwise divide
    MatrixShape operator/(MatrixShape const &rhs) const;

    /// Elementwise modulus
    MatrixShape operator%(MatrixShape const &rhs) const;

    /// Elementwise ceiling quotient
    MatrixShape ceil_div(MatrixShape const &rhs) const;

    /// Equality operator
    bool operator==(MatrixShape const &rhs) const;

    /// Inequality operator
    bool operator!=(MatrixShape const &rhs) const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct MatrixShapeHasher {
    size_t operator()(MatrixShape const &shape) const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

/// Matrix multiply-accumulate shape (m-by-n-by-k matrix product)
struct MmaShape {

    int32_t m;                              ///< number of rows in matrix multiply-accumulate
    int32_t n;                              ///< number of columns in matrix multiply-accumulate
    int32_t k;                              ///< inner dimension of matrix multiply-accumulate

    //
    // Methods
    //

    /// Dfault ctor
    MmaShape();

    /// Ctor
    MmaShape(int32_t m_ , int32_t n_, int32_t k_);

    /// Ctor
    MmaShape(MatrixShape const &mn_, int32_t k_ = 1);

    /// Computes the sum of all elements
    int32_t sum() const;

    /// Computes the product of all elements
    int32_t product() const;

    /// Returns true if any element is nonzero
    bool any() const;

    /// Returns true if all elements are zero
    bool not_any() const;

    /// Returns true if all elements are nonzero
    bool all() const;

    /// Returns true if some elements are zero
    bool not_all() const;

    /// Elementwise add
    MmaShape operator+(MmaShape const &rhs) const;

    /// Elementwise add
    MmaShape operator-(MmaShape const &rhs) const;

    /// Negate
    MmaShape operator-() const;

    /// Elementwise multiply
    MmaShape operator*(MmaShape const &rhs) const;

    /// Elementwise divide
    MmaShape operator/(MmaShape const &rhs) const;

    /// Elementwise modulus
    MmaShape operator%(MmaShape const &rhs) const;

    /// Elementwise ceiling quotient
    MmaShape ceil_div(MmaShape const &rhs) const;

    /// Returns a permutation of MmaShape
    MmaShape knm() const;

    /// Returns a permutation of MmaShape
    MmaShape kmn() const;

    /// Returns a permutation of MmaShape
    MmaShape nmk() const;

    /// Returns m and n as a MatrixShape
    MatrixShape mn() const;

    /// Returns n and m as a MatrixShape
    MatrixShape nm() const;

    /// Returns m and k as a MatrixShape
    MatrixShape mk() const;

    /// Returns k and m as a MatrixShape
    MatrixShape km() const;

    /// Returns n and k as a MatrixShape
    MatrixShape nk() const;

    /// Returns k and n as a MatrixShape
    MatrixShape kn() const;

    /// Equality operator
    bool operator==(MmaShape const &rhs) const;

    /// Inequality operator
    bool operator!=(MmaShape const &rhs) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

struct MmaShapeHasher {
    size_t operator()(MmaShape const &shape) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// This is really a stand-in for being able to test for specific ISA features.
/// The one example that doesn't fit lwrrently is int8 instruction support.
/// Some Pascal chips have it, some not.
ENUMCLASS(GemmChip, {
    GEMM_CHIP_MAXWELL = sass_private::GEMM_CHIP_MAXWELL,
    GEMM_CHIP_PASCAL = sass_private::GEMM_CHIP_PASCAL,
    GEMM_CHIP_VOLTA = sass_private::GEMM_CHIP_VOLTA,
    GEMM_CHIP_TURING = sass_private::GEMM_CHIP_TURING,
    GEMM_CHIP_AMPERE = sass_private::GEMM_CHIP_AMPERE,
    GEMM_CHIP_HOPPER = sass_private::GEMM_CHIP_HOPPER
});

MAKE_EQ_OPERATOR(sass_private::chipFamilyType_t, GemmChip)
MAKE_NE_OPERATOR(sass_private::chipFamilyType_t, GemmChip)

//////////////////////////////////////////////////////////////////////////////

ENUMCLASS(GemmType, {
    HLWDNN = sass_private::HLWDNN,
    HLWDNN_WINOGRAD = sass_private::HLWDNN_WINOGRAD,
    H884LWDNN = sass_private::H884LWDNN,
    S884LWDNN = sass_private::S884LWDNN,
    SLWDNN = sass_private::SLWDNN,
    SLWDNN_WINOGRAD = sass_private::SLWDNN_WINOGRAD,
    ILWDNN = sass_private::ILWDNN,
    I8816LWDNN = sass_private::I8816LWDNN,
    HGEMM = sass_private::HGEMM,
    H884GEMM = sass_private::H884GEMM,
    S844GEMM = sass_private::S884GEMM,
    SGEMM = sass_private::SGEMM,
    I8816GEMM = sass_private::I8816GEMM,
    IGEMM = sass_private::IGEMM,
    CGEMM = sass_private::CGEMM,
    DGEMM = sass_private::DGEMM,
    ZGEMM = sass_private::ZGEMM
});

//////////////////////////////////////////////////////////////////////////////

ENUMCLASS(GemmShape, {
    RECT = sass_private::RECT,
    LOWER = sass_private::LOWER,
    UPPER = sass_private::UPPER
});

//////////////////////////////////////////////////////////////////////////////

ENUMCLASS(GemmLayout, {
    N = sass_private::N,
    T = sass_private::T,
    C = sass_private::C
});

MAKE_EQ_OPERATOR(sass_private::layoutType_t, GemmLayout)
MAKE_NE_OPERATOR(sass_private::layoutType_t, GemmLayout)

//////////////////////////////////////////////////////////////////////////////

// Compute Capability only supports version >= 5.0
ENUMCLASS(ComputeCapabilityVersion, {
    CCV_5_0 = 0x0001,
    CCV_5_2 = 0x0002,
    CCV_5_3 = 0x0004,
    CCV_6_0 = 0x0008,
    CCV_6_1 = 0x0010,
    CCV_6_2 = 0x0020,
    CCV_7_0 = 0x0040,
    CCV_7_2 = 0x0080,
    CCV_7_5 = 0x0100,
    CCV_8_0 = 0x0200,
    CCV_8_6 = 0x0400,
    CCV_8_7 = 0x0800,
    CCV_9_0 = 0x1000,
    CCV_0_0 = 0x0000,   // Used for unsupported compute architecture
});

ENUMCLASS(ComputeCapability, {
    GRID_X_MAXIMUM = 0,
    GRID_Y_MAXIMUM,
    GRID_Z_MAXIMUM,
    SHMEM_SIZE,
});

// Define ReLu operation flag. Lwrrentl support three types:
//     eReLuDisable:              no ReLu, users don't need set any relu value.
//     eReLuScalar:               scalar ReLu, users need specify upperBound if there is.
//     eReLuPerchannel(Up/Low):   per-channel ReLu, users need set relu data.
ENUMCLASS(eReLuType, {
    eReLuDisable = 0,
    eReLuScalar = 1,
    eReLuPerchannelLow = 2,
    eReLuPerchannelUp  = 4,
});

ENUMCLASS(SplitKMode, {
    ONE_KERNEL = 1,     // run splitk with one kernel
    TWO_KERNELS = 2,    // run splitk with two kernels
    ERROR_MODE = 3,     // not supported mode
});


///////////////////////////////////////////////////////////////////////////////
/// \brief Return specific compute capability data.
///////////////////////////////////////////////////////////////////////////////

int32_t getComputeCapability(ComputeCapabilityVersion ccv, ComputeCapability cc);

///////////////////////////////////////////////////////////////////////////////
/// \brief Return a shared memory version based on Shader name
///////////////////////////////////////////////////////////////////////////////
ComputeCapabilityVersion getComputeCapabilityFromShader(const char* shaderName);

///////////////////////////////////////////////////////////////////////////////
/// \brief Return a shared memory version based on Chip Family
///////////////////////////////////////////////////////////////////////////////
ComputeCapabilityVersion getComputeCapabilityFromChipFamily(sass_private::chipFamilyType_t chip);



///////////////////////////////////////////////////////////////////////////////
/// \brief Return Shared memory limits by compute capabililty
///////////////////////////////////////////////////////////////////////////////
int32_t getShmemSize(ComputeCapabilityVersion ccv);


//////////////////////////////////////////////////////////////////////////////
/// \brief Current internal version of the library
///
/// \details Serialized data structures are only guaranteed to work
/// with same version of the library that created them. This
/// version returns a hash of the branch name plus the CL as reported
/// by buildVersion(). This uniquely identifies a single source build
/// of cask.
///
/// \returns The current version of the library.
//////////////////////////////////////////////////////////////////////////////
int getInternalVersion();

//////////////////////////////////////////////////////////////////////////////
/// \brief Return the CASK major version.
///
/// \details CASK version follows semantic versioning. All CASK releases
/// with the same major version are backwards compatibile (though not necessarily
/// binary compatible).
///
/// \returns The CASK library major version
//////////////////////////////////////////////////////////////////////////////
int majorVersion();

//////////////////////////////////////////////////////////////////////////////
/// \brief Return the CASK minor version.
///
/// \details CASK version follows semantic versioning. All CASK releases
/// with the same major and version are backwards compatibile, and higher
/// minor releases only add features.
///
/// \returns The CASK library minor version
//////////////////////////////////////////////////////////////////////////////
int minorVersion();

//////////////////////////////////////////////////////////////////////////////
/// \brief Return the CASK patch version.
///
/// \details CASK version follows semantic versioning. All CASK releases
/// with the same major and version are backwards compatibile. Increments
/// of the patch version indicate bug fixes.
///
/// \returns The CASK library patch version
//////////////////////////////////////////////////////////////////////////////
int patchVersion();

//////////////////////////////////////////////////////////////////////////////
/// \brief Perforce changelist for the current release.
///
/// \details Intended for debugging, the build version returns the
/// current p4hw CL. On non-release builds, this function may return
/// 0 if p4 build information was unavailable at build-time.
///
/// Note, the CL does not necessiarly uniquely identify a CASK build
/// Uses should also check the branchName().
///
/// \returns
//////////////////////////////////////////////////////////////////////////////
int buildVersion();

//////////////////////////////////////////////////////////////////////////////
/// \brief String of the development branch this version of cask
/// was built from.
///
/// \details If CASK was built from the "trunk" and development branch
/// or a release branch, this function return the name of the branch it
/// is comming from.
///
/// \returns the string
//////////////////////////////////////////////////////////////////////////////
const char* branchName();

//////////////////////////////////////////////////////////////////////////////
/// \brief Output-friendly string of the CASK version.
///
/// \details Output-friendly stirng of the form:
/// `"CASK build <MAJOR>.<MINOR>.<PATCH> (Build <BRANCH> @ <CL>)"`
///
/// \return Human-readable version string.
//////////////////////////////////////////////////////////////////////////////
const char* versionString();

//////////////////////////////////////////////////////////////////////////////
/// the architecture of GPU0 on the host
GemmChip hostArchitecture();

//////////////////////////////////////////////////////////////////////////////
/// \brief The struct with APIs that callwlates oclwpancy.
//////////////////////////////////////////////////////////////////////////////
struct OclwpancyProvider
{
    //////////////////////////////////////////////////////////////////////////////
    /// \brief Kernel info for sass/xmma/... shader to callwlate oclwpancy
    //////////////////////////////////////////////////////////////////////////////
    struct kernelInfoForGetOclwpancy
    {
        int32_t sharedMemSize;  // per CTA
        int32_t numRegisters;   // per Thread
        int32_t threadsPerCta;  // per CTA
    };

    //////////////////////////////////////////////////////////////////////////////
    /// \brief To inquire oclwpancy on current device
    //////////////////////////////////////////////////////////////////////////////
    virtual int32_t getOclwpancy(int32_t deviceNum = 0) const;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief To inquire oclwpancy on other device
    //////////////////////////////////////////////////////////////////////////////
    virtual int32_t getOclwpancy(const lwdaDeviceProp& deviceProps) const;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief To inquire kernel resource usage
    //////////////////////////////////////////////////////////////////////////////
    virtual kernelInfoForGetOclwpancy getKernelInfoForGetOclwpancy() const { return kInfo; }

protected:
    //////////////////////////////////////////////////////////////////////////////
    /// \brief For sass/xmma/... shader to callwlate oclwpancy, should be called by subclass of this like SassOclwpancyProviderImpl does
    //////////////////////////////////////////////////////////////////////////////
    int32_t callwlateOclwpancy(const kernelInfoForGetOclwpancy& kernelInfo, const lwdaDeviceProp& deviceProps) const;
    kernelInfoForGetOclwpancy kInfo = {-1, -1, -1};
};

class RunInfo;
struct HardwareInformation;

template<typename T>
class OptionalValue {

    bool hasValue_ = false;
    T value_;

public:

    inline OptionalValue() = default;
    inline OptionalValue(const OptionalValue &) = default;

    inline OptionalValue(const T & v)
    : hasValue_(true)
    , value_(v)
    { }

    inline bool hasValue() const {
      return hasValue_;
    }

    inline const T & value() const {
        assert(hasValue_ && "Requesting value that has not been set.");
        return value_;
    }

    inline void set(const T & v) {
        hasValue_ = true;
        value_ = v;
    }

    inline bool reset() {
        bool hadValue = hasValue_;
        *this = { };
        return hadValue;
    }

    inline OptionalValue & operator=(const T & v) {
        this->set(v);
        return *this;
    }

};

struct OptionalFeature
//! Defines the base interface for optional kernel features.
{
    //! Returns true if at least some portion of this feature is supported.
    virtual bool supported() const = 0;

    //! Colwenience colwersion to bool for quick determination of feature support.
    //! See @ref supported() for details.
    //!
    //! if (auto & feature = queryFeature()) {
    //!   auto f = feature.querySomething();
    //!   feature.setSomething(...);
    //! }
    inline operator bool () const { return supported(); }

    inline virtual ~OptionalFeature() { }
};

struct ProgrammaticGridSyncSupport
: public virtual OptionalFeature
//! Provides a query and update abstraction for managing features associated with
//! programmatic grid sync and related functionality.
{
    struct Knobs {

	//! If true, the kernel will configure the launch to use programmatic grid
        //! synchronization.
        bool LaunchWithProgrammaticSync;

    };

    //! Get the default recommended knob settings.
    virtual Error getDefaultKnobs(Knobs*) const = 0;

    //! Returns the lwrrently configured knob settings for the given
    //! @ref RunInfo.
    virtual Error getKnobs(const RunInfo&, Knobs*) const = 0;

    //! Set the desired knob settings for the kernel launch.
    //! @post Workspaces must be (re-)initialized.
    virtual Error setKnobs(RunInfo&, const Knobs&) const = 0;

    //! Get the current programmatic synchronization event for the
    //! launch previously applied with @ref setSychronizationEvent.
    //! @pre All workspaces must be initialized.
    virtual Error getSynchronizationEvent(const RunInfo&, lwdaEvent_t*) const = 0;

    //! Sets the synchronization launch event for the next invocation
    //! of run(). The @ref lwdaEvent_t will be passed to LWCA launch API
    //! and configured to trigger dependent events on kernel completion.
    //! @pre All workspacse must be initialized.
    virtual Error setSynchronizationEvent(RunInfo&, const lwdaEvent_t&) const = 0;

    inline virtual ~ProgrammaticGridSyncSupport() = default;

protected:

    const OptionalValue<Knobs> & accessKnobs(const RunInfo&) const;
    OptionalValue<Knobs> & accessKnobs(RunInfo&) const;

};

struct BlockClusterSupport
: public virtual OptionalFeature
//! Provides a query and update abstraction for managing features associated with
//! the Hopper block cluster (CGA) support.
{
    //! Determine if the kernel supports runtime configurable block cluster configuration. If
    //! a kernel supports runtime cluster size configuration, the @ref getMaximumSupportedClusterConfiguration()
    //! can be used to determine the upper bound on the allowable configurations. If this function returns false,
    //! then the kernel only supports a single block cluster size as denoted by getMaximumSupportedClusterConfiguration.
    virtual bool supportsRuntimeClusterConfiguration() const = 0;

    //! Returns the maximum block cluster (CGA) configuration supported by the shader. This
    //! is the kernels compiled limitations, not necessarily a cluster configuration
    //! that can launch on a target device. See @ref getMaximumSupportedClusterConfigurationForHardware(const HardwareInformation&)
    //! for querying device and configuration specific information.
    //!
    //! @returns Valid per-dimension limit bounds if supported otherwise the result is undefined.
    virtual dim3 getMaximumSupportedClusterConfiguration() const = 0;

    //! Computes the maximum block cluster size supported for the given @ref HardwareInformation.
    //! This method is provided to assist in early filtering of shaders when a desired CGA size
    //! is known but before a fully-constructed RunInfo is available. For shaders that support
    //! only a single fixed CGA tile configuration, this will be the same total configuration
    //! produced from the return value of @ref getMaximumSupportedClusterConfiguration().
    //!
    //! @returns The maximum CGA size for the associated shader with the given @ref HardwareInformation. If
    //! the shader cannot execute on the given hardware, the returned value will be 0.
    virtual int32_t getMaximumSupportedClusterConfigurationForHardware(const HardwareInformation&) const = 0;

    //! Outputs the lwrrently configured block cluster launch dimensions. If not set, the default
    //! output is @ref getRecommendedClusterLaunchConfiguration for the given @ref RunInfo.
    //!
    //! @pre Either @ref setClusterLaunchConfiguration() must be called OR problem-related fields
    //! and @ref HardwareInformation should be set in @ref RunInfo 's target @ref Operation.
    virtual Error getClusterConfiguration(const RunInfo&, dim3* cluster) const = 0;

    //! Computes the recommended block cluster size based on the current @ref RunInfo configuration.
    //!
    //! @pre All problem-related @Ref Operation fields in RunInfo as well as the target @ref HardwareInformation
    //! fields should be fully initialized.
    //! @returns @ref cask::Error::OK if a cluster size could be generated, otherwise any other @ref cask::Error.
    virtual Error getRecommendedClusterConfiguration(const RunInfo&, dim3* cluster) const = 0;

    //! The maximum block cluster size to utilize for kernels that support multiple possible
    //! sizes. CASK may choose the final cluster layout as long as it does not exceed this size.
    //!
    //! @pre @ref supported() must be true
    //! @post @ref initHostReservedSpace() and other initialization routines must be (re)called.
    //! @return @ref cask::Error as cask::Error::OK if successfully, otherwise any other cask::Error.
    virtual Error setMaximumClusterConfiguration(RunInfo&, int32_t maxClusterSize) const = 0;

    //! Sets the desired block cluster launch configuration to the given @param cluster size. This size
    //! must be less than the @ref dim3 returned from @ref getMaximumSupportedClusterConfigurationForHardware(const HardwareInformation&)
    //! or the kernel will fail to launch.
    //!
    //! @pre @ref supported() must be true. If @ref supportsRuntimeClusterConfiguration() returns false,
    //! the only supported value for @param cluster is the return value of @ref getMaximumSupportedClusterConfiguration().
    //! @post @ref initHostReservedSpace() must be (re)called for the settings to take effect.
    //! @return @ref cask::Error as cask::Error::OK if successfuly, otherwise any other cask::Error.
    virtual Error setClusterConfiguration(RunInfo&, const dim3 & cluster) const = 0;

    inline virtual ~BlockClusterSupport() { }

protected:

    const OptionalValue<dim3> & accessBlockClusterSize(const RunInfo&) const;
    OptionalValue<dim3> & accessBlockClusterSize(RunInfo&) const;

    const OptionalValue<int32_t> & accessMaxBlockClusterSize(const RunInfo&) const;
    OptionalValue<int32_t> & accessMaxBlockClusterSize(RunInfo&) const;

};

struct SegmentKSupport
: public virtual OptionalFeature
//! Provides a query and update abstraction for managing features associated with
//! the Segment-K implementation of Split-K for persistent kernels.
{
    //! Query if Segment-K is enabled for the given @ref RunInfo.
    virtual Error enabled(const RunInfo&, bool* enabled) const = 0;

    //! Enable or disable the segment-k feature. Actual runtime activation is still dependent on the
    //! the other underlying restrictions imposed by the Segment-K knobs.
    //! @post Workspaces must be re(initialized) and may change size based on this setting.
    virtual Error enable(RunInfo&, bool enable) const = 0;

    //! Get the current workspace size required by SegmentK for the current @ref RunInfo
    //! configuration. The will be a portion of the allocation required by the
    //! @ref Shader::getDeviceWorkspaceSize(RunInfo&) if SegmentK is enabled.
    //! @pre @ref Shader::initHostWorkspace must be called to initialize relevant @ref RunInfo fields.
    virtual Error getSegmentKDeviceWorkspaceSize(const RunInfo&, int64_t* sizeInBytes) const = 0;

    struct PlanningKnobs
    //! Client adjustable settings for Segment-K planning.
    {
        //! Sets the maximum allowed SegmentK device workpace
        //! allocation. The actual allocation may be less than
        //! requested, call @ref getDeviceWorkspaceSize(RunInfo&) to
        //! see that actual allocation size after calling @ref
        //! initHostWorkspace(). Setting the maximum workspace size to
        //! 0 will effectively disable the feature. A negative value
        //! allows CASK to deside the optimal configuration.
        int64_t maxSegmentKDeviceWorkspaceSizeInBytes = -1;

        //! Specifies the minimum value for K before Segment-K will be
        //! enabled. This can be used to remove the overhead for
        //! activating Segment-K when setup would be longer than just
        //! not Splitting the K dimension in the first place. A
        //! negative value indicates CASK may ignore this value for
        //! planning.
        int32_t minimumThresholdForSplit = -1;

        //! The minimum number of parts in which to split an output
        //! tile. A negative value indicates CASK may ignore this
        //! value for planning.
        int32_t minimumSplitFactor = -1;

        //! The maximum number of parts in which to split an output
        //! tile. A negative value indicates CASK may ignore this
        //! value for planning.
        int32_t maximumSplitFactor = -1;

        PlanningKnobs() = default;
        PlanningKnobs(const PlanningKnobs&) = default;
        PlanningKnobs& operator=(const PlanningKnobs&) = default;

        PlanningKnobs(
            int64_t maxSegmentKDeviceWorkspaceSizeInBytes_,
            int32_t minimumThresholdForSplit_,
            int32_t minimumSplitFactor_,
            int32_t maximumSplitFactor_)
        : maxSegmentKDeviceWorkspaceSizeInBytes(maxSegmentKDeviceWorkspaceSizeInBytes_)
        , minimumThresholdForSplit(minimumThresholdForSplit_)
        , minimumSplitFactor(minimumSplitFactor_)
        , maximumSplitFactor(maximumSplitFactor_)
        { }

	bool operator==(const PlanningKnobs&) const;
	bool operator!=(const PlanningKnobs&) const;
	bool operator< (const PlanningKnobs&) const;
	bool operator> (const PlanningKnobs&) const;
	bool operator<=(const PlanningKnobs&) const;
	bool operator>=(const PlanningKnobs&) const;
    };

    //! Applies the planning knobs to the run configuration. Must be
    //! configured before calling initHostReservedSpace().
    //!
    //! @post Workspaces must be re(initialized)
    virtual Error setPlanningKnobs(RunInfo&, const PlanningKnobs&) const = 0;

    struct ControlKnobs
    //! Control knobs directly specify the behavior of Segment-K. If
    //! specified by the client, and the knobs are valid, the internal
    //! Segment-K planning will be bypassed and the given ControlKnobs
    //! will be used.
    {
        struct SplitConfig
        //! Defines the split configuration for a set of tiles.
        {
            //! The number of output tiles to split. A value of 0
            //! disables splitting.
            int32_t numTilesToSplit = 0;

             //! The number of partititions to split each output tile
             //! along the k-dimension. The value must be greater or
             //! equal to 1.
            int32_t perTileSplitFactor = 1;

            SplitConfig() = default;
            SplitConfig(const SplitConfig&) = default;
            SplitConfig& operator=(const SplitConfig&) = default;

            SplitConfig(int32_t numTilesToSplit_, int32_t perTileSplitFactor_)
            : numTilesToSplit(numTilesToSplit_), perTileSplitFactor(perTileSplitFactor_)
            { }

        };

        ControlKnobs() = default;
        ControlKnobs(const ControlKnobs&) = default;
        ControlKnobs& operator=(const ControlKnobs&) = default;

        ControlKnobs(const SplitConfig& primary, const SplitConfig& secondary)
        : primarySplitConfig(primary), secondarySplitConfig(secondary)
        {}

        //! The primary split configuration. This configuration will
        //! be used to split up to but no more than the number of
        //! final-wave thread blocks or block clusters. For block
        //! cluster computations, the number of tiles to split will be
        //! rounded down to the nearest block cluster boundary.
        SplitConfig primarySplitConfig;

        //! A secondary split configuration used only if the @ref
        //! primarySplitConfig does not cover the entire final wave of
        //! computation. Splitting behavior is the same as the primary
        //! split config.
        SplitConfig secondarySplitConfig;

	bool operator==(const ControlKnobs&) const;
	bool operator!=(const ControlKnobs&) const;
	bool operator< (const ControlKnobs&) const;
	bool operator> (const ControlKnobs&) const;
	bool operator<=(const ControlKnobs&) const;
	bool operator>=(const ControlKnobs&) const;
    };

    //! Get the current Segment-k settings for the given @ref
    //! RunInfo. If not previously set, the return value will be be
    //! the default settings.
    //!
    //! @pre initHostReservedSpace() or @ref setControlKnobs() must have been called.
    virtual Error getControlKnobs(const RunInfo&, ControlKnobs*) const = 0;

    //! Generate the recommended Segment-K settings for the given @ref
    //! RunInfo. This will ignore any existing lwstomizations already
    //! set for Segment-K but may take into account other settings in
    //! RunInfo. Any @ref PlanningKnobs applied previously to RunInfo
    //! will be ignored. Use @ref setControlKnobs to apply the desired
    //! values.
    virtual Error getRecommendedControlKnobs(const RunInfo&, ControlKnobs*, const PlanningKnobs&) const = 0;

    //! A simplification of @ref getRecommendedControlKnobs(const
    //! RunInfo&, ControlKnobs*, const PlanningKnobs&) that uses
    //! previously applied @ref PlanningKnobs or if not applied allows
    //! CASK to choose a preferred implementation strategy.
    virtual Error getRecommendedControlKnobs(const RunInfo&, ControlKnobs*) const = 0;

    //! Apply the given @ref ControlKnobs to the @ref RunInfo configuration.
    //! @return Error::OK if settings are valid, or another @ref Error if not. Additional checks may be done during
    //! calls to @ref Shader::canImplement().
    //!
    //! @post Workspaces must be re(initialized)
    virtual Error setControlKnobs(RunInfo&, const ControlKnobs&) const = 0;

    inline virtual ~SegmentKSupport() = default;

protected:

    const OptionalValue<bool> & accessEnabled(const RunInfo&) const;
    OptionalValue<bool> & accessEnabled(RunInfo&) const;

    const OptionalValue<PlanningKnobs> & accessPlanningKnobs(const RunInfo&) const;
    OptionalValue<PlanningKnobs> & accessPlanningKnobs(RunInfo&) const;

    const OptionalValue<ControlKnobs> & accessControlKnobs(const RunInfo&) const;
    OptionalValue<ControlKnobs> & accessControlKnobs(RunInfo&) const;
};

//////////////////////////////////////////////////////////////////////////////
///
/// \brief Base class for all the shaders.
///
/// Shader implements shared functionality across all shader (aka kernel)
/// implmentations.
/// In particular, each shader must have a unique name, identifying handle
/// (based on a hash of the name), and the ability to query parameters.
///
//////////////////////////////////////////////////////////////////////////////
class
Shader {
protected:
    const KernelInfo* kernelInfo;

public:
    typedef cask::Error Error;  // for existing code relying on Shader::Error

    //////////////////////////////////////////////////////////////////////////
    /// \brief Name of the kernel.
    ///
    /// Names should match the kernel name processed by the rest of our tools.
    /// see https://confluence.lwpu.com/display/GCA/Kernel+name+decoder for
    /// details on expected kernel names.
    //////////////////////////////////////////////////////////////////////////////
    const std::string name;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Unique hash of name
    ///
    /// Provides a unique, easily compariable value to identify the kernel.
    /// By default it is based on a crc of the name.
    //////////////////////////////////////////////////////////////////////////////
    const uint64_t    handle;      // unique hash of name

    /**
     * Data members related to linkable shader
     */
    ///@{
    /// Linked lwbin data
    std::vector<char> linkedLwbin;
    /// Flag to show if this shader is linkable
    bool isLinkable;

    /// Linkable kenrel module
    LWmodule linkedKernelModule;
    /// Linkable kernel function pointer
    LWfunction linkedKernelFuncPtr;
    ///@}

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with kernel name as argument
    //////////////////////////////////////////////////////////////////////////////
    Shader(const std::string& n);
    //////////////////////////////////////////////////////////////////////////////
    /// \brief Ctor with a pointer to KernelInfo as argument
    //////////////////////////////////////////////////////////////////////////////
    Shader(const KernelInfo* ki);
    //////////////////////////////////////////////////////////////////////////////
    /// \brief Default Dtor, does nothing.
    //////////////////////////////////////////////////////////////////////////////
    inline virtual ~Shader() { }

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Return the pointer to KernelInfo
    //////////////////////////////////////////////////////////////////////////////
    const KernelInfo* getKernelInfo() const;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Return the pointer to base KernelInfo
    //////////////////////////////////////////////////////////////////////////////
    const BaseKernelInfo* getBaseInfo() const;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Return an empty OclwpancyProvider
    //////////////////////////////////////////////////////////////////////////////
    virtual const OclwpancyProvider &QueryOclwpancyProvider() { return voidOclwpancyProvider; }

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Return the size in bytes required to serialize the shader
    //////////////////////////////////////////////////////////////////////////////
    virtual int64_t getSerializedSize() const ;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Serialize the shader to the buffer specified.
    //////////////////////////////////////////////////////////////////////////////
    virtual Error serialize(uint8_t* buffer, int64_t size) const ;

    //////////////////////////////////////////////////////////////////////////////
    /// \brief Destroy the shader object
    //////////////////////////////////////////////////////////////////////////////
    virtual void destroy() { }

private:

    struct BlockClusterNotSupportedImpl final
    : public BlockClusterSupport
    //! Implements a no-operation class when block cluster support is not available.
    {
        BlockClusterNotSupportedImpl() = default;
        inline virtual ~BlockClusterNotSupportedImpl() = default;

        virtual bool supported() const override;
        virtual bool supportsRuntimeClusterConfiguration() const override;
        virtual dim3 getMaximumSupportedClusterConfiguration() const override;
        virtual int32_t getMaximumSupportedClusterConfigurationForHardware(const HardwareInformation&) const override;
        virtual Error getClusterConfiguration(const RunInfo&, dim3* cluster) const override;
        virtual Error getRecommendedClusterConfiguration(const RunInfo&, dim3* cluster) const override;
        virtual Error setMaximumClusterConfiguration(RunInfo&, int32_t maxClusterSize) const override;
        virtual Error setClusterConfiguration(RunInfo&, const dim3& cluster) const override;
    };

public:

    virtual const cask::BlockClusterSupport & blockClusterSupport() const;
    virtual cask::BlockClusterSupport & blockClusterSupport();

private:

    struct ProgrammaticGridSyncNotImplementedImpl final
    : public ProgrammaticGridSyncSupport
    //! Trivial implementation of @ref ProgrammaticGridSyncSupport for operations that do not
    //! support programmatic grid synchronization features.
    {
        virtual bool supported() const override;
        virtual Error getDefaultKnobs(Knobs*) const override;
        virtual Error getKnobs(const RunInfo&, Knobs*) const override;
        virtual Error setKnobs(RunInfo&, const Knobs&) const override;
        virtual Error getSynchronizationEvent(const RunInfo&, lwdaEvent_t*) const override;
        virtual Error setSynchronizationEvent(RunInfo&, const lwdaEvent_t&) const override;

        ProgrammaticGridSyncNotImplementedImpl() = default;
        virtual ~ProgrammaticGridSyncNotImplementedImpl() = default;
    };

public:

    virtual const ProgrammaticGridSyncSupport & programmaticGridSyncSupport() const;
    virtual ProgrammaticGridSyncSupport & programmaticGridSyncSupport();

private:

    struct SegmentKNotSupportedImpl
    : public virtual SegmentKSupport
    //! A default no-op implementation for segment-k when not supported.
    {
        virtual bool supported() const override;
        virtual Error enabled(const RunInfo& ri, bool* enabled) const override;
        virtual Error enable(RunInfo& ri, bool enable) const override;
        virtual Error getSegmentKDeviceWorkspaceSize(const RunInfo&, int64_t* sizeInBytes) const override;
        virtual Error setPlanningKnobs(RunInfo&, const PlanningKnobs&) const override;
        virtual Error getControlKnobs(const RunInfo&, ControlKnobs*) const override;
        virtual Error getRecommendedControlKnobs(const RunInfo&, ControlKnobs*, const PlanningKnobs&) const override;
        virtual Error getRecommendedControlKnobs(const RunInfo&, ControlKnobs*) const override;
        virtual Error setControlKnobs(RunInfo&, const ControlKnobs&) const override;
    };

public:

    virtual const SegmentKSupport & segmentKSupport() const;
    virtual SegmentKSupport & segmentKSupport();

private:

    OclwpancyProvider voidOclwpancyProvider;
    static ProgrammaticGridSyncNotImplementedImpl mPgsSupport;
    static BlockClusterNotSupportedImpl mBlockClusterSupport;
    static SegmentKNotSupportedImpl mSegmentKSupport;

};


//////////////////////////////////////////////////////////////////////////////
/// \brief Template class for organizing lists of shaders
///
/// There is one template instance for each shader type.  It is used to
/// find the available Shaders.
//////////////////////////////////////////////////////////////////////////////
template<typename ShaderType, typename OperationType>
class
ShaderList {
    typedef std::vector<ShaderType*> ImplType;
public:
    typedef ShaderList< ShaderType, OperationType > ShaderListType;
    /// ctor
    ShaderList() : impl(), isSorted(false) { }

    /// dtor
    ~ShaderList() { }

    // a forward iterator (input iterator that can be reused)
    typedef typename ImplType::const_iterator const_iterator;

    /// Typical STL list iterator
    inline const_iterator begin() const { sortHandles(); return impl.begin(); }

    /// Typical STL list iterator
    inline const_iterator end() const { sortHandles(); return impl.end(); }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Find a kernel by its implementation name.
    ///
    /// All Shader instances have an associated name that is unique to
    /// the implementation of the kernel.
    ///
    /// \returns a Shader subtype (as determined by the template) if
    ///          the desired shader is found.
    /// \returns NULL if the desired shader is not found.
    ///
    //////////////////////////////////////////////////////////////////////////
    ShaderType* findByName(const std::string& name) const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Find a kernel by its unique handle.
    ///
    /// All Shader instances have a unique handle that identifies the
    /// kernel.
    ///
    /// \returns a Shader subtype (as determined by the template) if
    ///          the desired shader is found.
    /// \returns NULL if the desired shader is not found.
    ///
    //////////////////////////////////////////////////////////////////////////
    ShaderType* findByHandle(uint64_t handle) const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Find kernels that can implement the operation
    ///
    /// \returns a ShaderList of all kernels that can implement this operation.
    //////////////////////////////////////////////////////////////////////////
    ShaderList findCanImplement(const OperationType& operation,
                                GemmChip forChip=hostArchitecture()) const;

    //////////////////////////////////////////////////////////////////////////
    /// \brief Add a Shader to the list (internal).
    //////////////////////////////////////////////////////////////////////////
    // API cleanup: This really shouldn't be exposed in the API
    inline void push_back(ShaderType* shader) {
        isSorted = false;
        impl.push_back(shader);
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief Return the per-Shader-subclass instance of this class.
    ///
    /// ShaderList<Shader, Op> creates a singleton instance per
    /// template instance. This static function provides access
    /// to that instance.
    ///
    /// \returns A global ShaderList<> object. Will allocate a new
    ///          instance if none exists.
    //////////////////////////////////////////////////////////////////////////
    static inline const ShaderListType* availableShaders() {
        return internalGetShaderList().get();
    }

    //////////////////////////////////////////////////////////////////////////
    /// \brief internal function; should not be called directly.
    //////////////////////////////////////////////////////////////////////////
    // this method should be private, and only accessible by class ShaderType,
    // but in C++ 03 it is illegal to declare "friend class ShaderType"!!!  (In
    // C++11 it is legal with syntax "friend ShaderType".
    static std::shared_ptr<ShaderListType> internalGetShaderList();
private:
    ImplType impl;
    bool isSorted;
    // sortHandles modifies the order of the impl vector, but does not change
    // its semantics, so it is semantically const.  (Implementation uses
    // const_cast())
    void sortHandles() const;

    static std::shared_ptr<ShaderList< ShaderType, OperationType >> theShaderList;
};

//! Deprecated: Hacky side channel for turning on debugging messages
void setPrintParams(bool);

//! Hacky side channel for turning on debugging messages
bool shouldPrintParams();

//! Deprecated: Hacky side channel for turning off kernel launches while debugging
void setSuppressGPULaunch(bool);

//! Deprecated: Hacky side channel for turning off kernel launches while debugging
bool shouldSuppressGpuLaunch();

void initCask();
Error populateShaderLists(Shader** shaders, size_t shaderCount);
Error populateShaderLists(std::vector<Shader*> shaders);

bool getStage(const std::string& shader_name, size_t* stage);

} // namespace cask

#endif // INCLUDE_GUARD_CASK_CORE_H
