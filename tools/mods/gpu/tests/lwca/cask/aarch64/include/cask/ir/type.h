
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

    const bool isAuto;
    const cask::ScalarType scalarType{ cask::ScalarType::FP32 };
    const int multiple{ 1 };
};

struct TileType : public ast::Type {
  public:
    TileType(int tile_kind, const ElementType &element_type)
        : tile_kind_(tile_kind), element_type_(element_type) { }

    virtual ~TileType() { }

    const ElementType &getElementType() const {
        return this->element_type_;
    }

    int getThreadM() const;
    int getThreadN() const;

    int getTileM() const;
    int getTileN() const;

  private:
    const int tile_kind_;
    const ElementType element_type_;
};

/**
 * @brief Defines the logic type information of a tensor without layout information
 *
 *  - The tensor dimensions.
 *  - The tensor rank
 *  - The tensor element type.
 */
struct TensorType : public ast::Type {
    TensorType();

    TensorType(const cask::ScalarType scalar_type);
    TensorType(int32_t rank, const cask::ScalarType scalar_type);

    TensorType(TensorType const &other);

    TensorType(int32_t rank, const sass_private::elementDataType_t scalar_type)
    : TensorType(rank, static_cast<cask::ScalarType>(scalar_type)) { }

    virtual ~TensorType();

    int32_t getRank() const;

    const cask::ScalarType dataType = cask::ScalarType::FP32;
    const int32_t rank = -1;

    friend class Tensor;
    friend class Shader;
};

/** @} */

} //namespace ir
} //namespace cask

/////////////////////////////////////////////////////////////////////////////////////////////////

#endif  // INCLUDE_GUARD_CASK_IR_TYPE_H
