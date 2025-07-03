
#ifndef INCLUDE_GUARD_CASK_IR_TYPE_H
#define INCLUDE_GUARD_CASK_IR_TYPE_H

#include <cask/cask.h>

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace cask {

namespace internal {
namespace ir {

template<typename TypeId> struct MonoTypeExpr;
struct TensorTypeExpr;
using ScalarTypeExpr = MonoTypeExpr<cask::ScalarType>;

struct Node;
class GraphAST;
struct Expr;

struct TensorDesc;
struct ShapeExpr;

}
}

namespace ir {

namespace ast {

struct Type {
    Type() {}
    virtual ~Type() {}
};

}

/**
 * \defgroup Type Basic Types
 *
 * @{
 */

struct CType : public ast::Type {
    const char* name;
    const char* cpp;
    int32_t bits;
    int32_t alignment;

    CType(ScalarType scalar_type)
        : name(NumericTraits(scalar_type).name)
        , cpp(NumericTraits(scalar_type).cpp)
        , bits(NumericTraits(scalar_type).bits)
        , alignment(NumericTraits(scalar_type).bits / 8) { }
};

/**
 * @brief Minimum element for mathematical operations.
 *
 * It can represent a single element or vector of elements. This class is mainly used for following purpose:
 *  - Describe element data type of a tensor.
 *  - Describe customized device function input argument data type.
 *  - Describe customized device function output argument data type.
 *
 */
struct ElementType : public ast::Type {
    ElementType() : isAuto(true) { }

    ElementType(const cask::ScalarType scalar_type, const int multiple = 1)
        : isAuto(false), scalarType(scalar_type), multiple(multiple) { }

    // FIXME: Depcreated
    ElementType(const sass_private::elementDataType_t &scalar_type, const int multiple = 1)
        : ElementType(static_cast<cask::ScalarType>(scalar_type), multiple) { }

    virtual ~ElementType() { }

    friend bool operator==(const ElementType &left, const ElementType &right);

    bool       isAuto;
    ScalarType scalarType{ cask::ScalarType::FP32 };
    int        multiple{ 1 };
};

enum class TileKind {
    DEFAULT,
    GEMM_GMEM_TILE_A,
    GEMM_GMEM_TILE_B,
    GEMM_GMEM_TILE_EPILOGUE,
    GEMM_ACCUMULATOR_TILE,
    CONV2D_ACCUMULATOR_TILE,
};

struct TileType : public ast::Type {
public:
    TileType(TileKind kind_,
             LayoutID layout_,
             ScalarType scalar_type,
             int32_t size_m,
             int32_t size_n,
             int32_t loop_m,
             int32_t loop_n)
    : kind(kind_)
    , layout(layout_)
    , scalarType(scalar_type)
    , size{ size_m, size_n }
    , loop{ loop_m, loop_n } { }

    virtual ~TileType() { }

    bool isGmemTile() const {
        return kind == TileKind::GEMM_GMEM_TILE_A
               || kind == TileKind::GEMM_GMEM_TILE_B
               || kind == TileKind::GEMM_GMEM_TILE_EPILOGUE
               ;
    }

    TileKind   kind;
    LayoutID   layout;
    ScalarType scalarType;

    struct { int32_t m; int32_t n; } size;
    struct { int32_t m; int32_t n; } loop;
};

/**
 * @brief Defines the logic type information of a tensor without layout information
 *
 *  - The tensor rank
 *  - The tensor element type.
 */
struct TensorType : public ast::Type {
    TensorType();

    TensorType(const cask::ScalarType scalar_type);
    TensorType(int32_t rank, const cask::ScalarType scalar_type);

    // FIXME: Should we converage with cask::TensorType?
    TensorType(cask::TensorType const& tensor_type);
    TensorType(TensorType const& other);

    TensorType(int32_t rank, const sass_private::elementDataType_t scalar_type)
    : TensorType(rank, static_cast<cask::ScalarType>(scalar_type)) { }

    virtual ~TensorType();

    TensorType &operator=(TensorType const& other) = default;

    int32_t getRank() const;

    ScalarType dataType = cask::ScalarType::FP32;
    int32_t    rank = -1;

    friend class Tensor;
    friend class Shader;
};

/** @} */

} //namespace ir
} //namespace cask

/////////////////////////////////////////////////////////////////////////////////////////////////

#endif  // INCLUDE_GUARD_CASK_IR_TYPE_H
