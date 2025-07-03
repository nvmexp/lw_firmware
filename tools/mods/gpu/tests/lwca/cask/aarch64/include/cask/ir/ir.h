
#pragma once

#include <memory>

#include <cask/cask.h>
#include <cask/ir/type.h>

namespace cask {

// Forward declaration of internal APIs
namespace internal {
namespace ir {

struct Expr;
struct Node;
class GraphAST;

}
}

namespace ir {

/**
 * @brief this function will be deprecated in future, please use TensorType directly
 *
 * @param rank rank should be `rank >= 0 && rank <= TensorDesc::MAX_DIM`
 * @param scalar_type scalar_type must be one of FP64, FP32, FP16, INT64, INT32, INT16, INT8, UINT64, UINT32, UINT16, UINT8
 * @return const TensorType& if not meet above constraints, will return default tensor type (0, fp32)
 */
const TensorType& getTensorType(int rank, cask::ScalarType scalar_type);

/**
 * @brief This function will be deprecated, please use ElementType directory.
 *
 * @details * multi must be 1, 2, 4; for half and float multi can also be 8
 *
 * @param scalar_type scalar_type must be one of FP64, FP32, FP16, INT64, INT32, INT16, INT8, UINT64, UINT32, UINT16, UINT8
 * @param multi multi must be 1, 2, 4, for half, only support 1, 2.
 * @return const ElementType& if not meet above constraints, will return (fp32, 1).
 */
const ElementType& getElementType(cask::ScalarType scalar_type, int32_t multi);

/**
 * @brief pimpl implementation of IR objects
 *
 * https://en.cppreference.com/w/cpp/language/pimpl
 * Using pimpl for ABI compatibility
 */
template<typename T>
class Reference {
public:
    using Pointer = std::shared_ptr<T>;

    Reference();
    Reference(Pointer);
    Reference(Error error);
    Reference(Error::Label error) : error_(Error(error)) { }
    Reference(Reference const &other);

    ~Reference();

    Pointer& operator->();
    const Pointer& operator->() const;

    Reference &operator=(Reference const &rhs);

    operator bool() const;

    // NOTE: Regarding lifetime, there are two categories of IR objects
    //  * permanant object
    //  * temporary object which isn't shared cross threads
    // Based on above assumption. it's safe to use shared_ptr<T>::get() instead of lock() here
    T *get();

    const Pointer& getPtr() const { return this->ptr_; }

    /**
     * @brief Return Error code of current IR object
     *
     * @return cask::Error
     *   - Error::OK Yeah!
     *   - Error::IR_*  Please check for individual API for detailed explanation
     */
    cask::Error isValid() const;

protected:
    Pointer ptr_;

    cask::Error error_{cask::Error::OK};

    friend struct cask::internal::ir::Node;
    friend class cask::internal::ir::GraphAST;
};

class Element;
class Tile;
class Tensor;
class ElementwiseFunc;
class TileFunc;
class Shader;
class Gemm;
class Conv;
class ShaderTemplate;
class RawPointer;
class ShaderConfig;

using ElementwiseFuncRef = Reference<ElementwiseFunc>;
using TileFuncRef        = Reference<TileFunc>;
using RawPointerRef      = Reference<RawPointer>;
using ShaderConfigRef    = Reference<ShaderConfig>;

class TensorRef;

/**
 * \defgroup Primitives Primitives
 *
 * \brief Factory method creating tensor, element and tile for building fusion Graph
 * @{
 */

/**
 * @brief Generic variable object
 */
class Variable;
using VariableRef = Reference<Variable>;

/**
 * @brief Opaque C/C++ structure passed from customer
 *
 */
class CVar;
using CVarRef = Reference<CVar>;

/**
 * @brief Element of a tensor which can be
 *   - scalar
 *   - vector for simd instructions
 *
 */
using ElementRef = Reference<Element>;

/**
 * @brief Create a scalar or vector variable with name and vector size
 *
 * @param name          Unique name of variable within the same context
 *                      https://en.cppreference.com/w/cpp/language/identifiers
 * @param scalar_type   Data type defined by cask::ScalarType except for
 *  - ScalarType::VOID_TYPE
 *  - ScalarType::INTPTR
 * @param multiply      Size of vector. Support only { 1, 2, 4 }
 * @return ElementRef
 *  ElementRef::isValid() -> cask::Error
 *  - OK                           Valid, Yeah!
 *  - IR_INVALID_IDENTIFIER        Bad identifier
 *  - IR_UNSUPPORTED_SCALAR_TYPE   Bad ScalarType { VOID_TYPE, INTPTR }
 *  - IR_MULTIPLE_DEFINITION       Same identifier is already defined
 *  - IR_UNSUPPORTED_VECTOR_SIZE   Only support { 1, 2, 4 }
 */
ElementRef makeElement(const char* name, cask::ScalarType scalar_type, int multiply = 1);

/**
 * @brief 2-dimension struct represent a sub-region of a tensor
 *   - GEMM/IMPLICT_GEMM tile: 2-d for GEMM and 4-d (nhw x c)
 *   - Directed convolution tile: 2-d on H x W
 */
using TileRef = Reference<Tile>;

/**
 * @brief Create a reference to tile object for creating fusion graph
 *
 * @param name          Unique name of variable within the same context
 *                      https://en.cppreference.com/w/cpp/language/identifiers
 * @param tile_kind
 * @param scalar_type
 * @return TileRef
 */
TileRef makeTile(const char* name, int32_t tile_kind, cask::ScalarType scalar_type, int multiple = 1);

/**
 * @brief Reference to tensor object to describe data flow in IR construction
 *
 *   Possible Errors by TensorRef::isValid():
 *     - cask::Error::OK                          valid, yeah!
 *     - cask::Error::IR_INVALID_IDENTIFIER       bad, invalid name
 *     - cask::Error::NOT_IMPLEMENTED             bad, feature that isn't implemented (e.g. 0-rank tensor)
 *     - cask::Error::IR_UNSUPPORTED_SCALAR_TYPE  Unsupported Scalar Type
 *     - cask::Error::IR_TYPE_MISMATCH             rank must be >= 0
 *
 */
class TensorRef : public Reference<Tensor> {
public:
    using Base = Reference<Tensor>;

    TensorRef();
    // TensorRef(Error error);
    TensorRef(Base const& other);

    ~TensorRef();

    TensorRef& operator=(TensorRef const&) = delete;

    /**
     * @brief Get the Type expression of Tensor object
     *
     * @return const TensorType&
     */
    const TensorType& getType() const;

#if CASK_IR_SUPPORT_SHAPE_EXPRESSION
    std::vector<int> getShape() const {
        return type_.dims;
    }

    cask::ScalarType getScalarType() const {
        return type_.dataType;
    }
#endif
};

/**
 * @brief Create tensor variable with specified tensor type
 *
 * @param name
 * @param type
 * @return TensorRef
 */
TensorRef makeTensor(const char* name, TensorType const &type = TensorType());

/**
 * @brief Create tensor variable with specified rank and sclar type
 *
 * @param name          Variable name in IR
 * @param rank          Rank of tensor
 * @param scalar_type   Scalar Type of element
 * @return TensorRef
 */
TensorRef makeTensor(const char* name, int rank, cask::ScalarType scalar_type);

/**
 * @brief Create tensor variable with specified rank and sclar type
 *
 * @param name
 * @param scalar_type
 * @return TensorRef
 */
TensorRef makeTensor(const char* name, cask::ScalarType scalar_type);

/** @} */

// Utility functions to examine types.
const TensorType &getTensorType(const TensorRef&);
cask::ScalarType getScalarType(const TensorType&);

const ElementType &getElementType(ElementRef);
cask::ScalarType getScalarType(const ElementType&);
int getMultiple(const ElementType&);

template<typename T>
class List {
public:
    List();
    List(cask::Error error);
    List(List&&);
    List(const List&) = delete;
    List& operator=(const List&) = delete;

    ~List();

    /**
     * @brief Syntaxed sugar to allow creating List like this { a, b, c, ... }
     *
     * NOTE: This function must be inlined for ABI compatibility
     */
    List(std::initializer_list<T> list) : List() {
        for (auto item : list) { this->push_back(item); }
    }

    int64_t size() const;

    void push_back(const T&);

    const T& at(int index) const;
    const T& get(int index) const { return this->at(index); }

    /**
     * @brief return error code carried by current IR object
     *
     * @return cask::Error
     */
    cask::Error isValid() const;

    friend class Shader;
    friend class Tensor;

private:
    class Details;
    std::unique_ptr<Details> details_;

    cask::Error error_{cask::Error::OK};
};

// NOTE: use std::initializer_list as helper is safe for ABI compatibility here
template<typename T>
inline List<T> makeList(std::initializer_list<T> args) {
    List<T> list;
    for (auto entry : args) { list.push_back(entry); }
    return list;
}

using ElementList    = List<ElementRef>;
using TileList       = List<TileRef>;
using RawPointerList = List<RawPointerRef>;

/**
 * @brief List of TensorRef objects
 *
 * @code{cpp}
 *   TensorList gemm_outputs = gemm.apply(a, b);
 *   TensorList relu_outputs = relu.apply(gemm_outputs.at(0));
 * @endcode
 *
 */
/// NOTE: This class is merely added to hold ownership of underlying expression
class TensorList : public List<TensorRef> {
public:
    using Base = List<TensorRef>;

    using Base::Base;

    TensorList() : List() {}
    TensorList(List&& other) : List(std::move(other)) {}
    TensorList(TensorList &&other) : List(std::move(other)), expr_(other.expr_) {}

    TensorList(const TensorList&) = delete;
    TensorList& operator=(const TensorList&) = delete;

    TensorList(std::initializer_list<TensorRef> list) : List(list) { }

    ~TensorList();

    friend Shader;

    // FIXME: figure out a way to access this in test
    TensorList(std::initializer_list<TensorRef> list, internal::ir::Expr *expr) : List(list), expr_(expr) {}

private:
    TensorList(internal::ir::Expr *expr) : List(), expr_(expr) {}

    internal::ir::Expr *expr_{nullptr};
};

//////////////////////////////////////////////////////////////////////////////////////////
// ir::Shader
//////////////////////////////////////////////////////////////////////////////////////////

class ShaderTemplateRef;

/**
 * \defgroup Shader Shader Builder
 *
 * @code{.cpp}
 *     ShaderGraphDesc descriptor;
 *     TensorRef a = descritpor.makeTensor("a", 3, FP32);
 *     TensorRef b = descritpor.makeTensor("a", 3, FP32);
 *     TensorRef d = matmul(a, b);
 *     descriptor.bindOutput(d);
 *
 *     // Ownership of descriptor will be transfered to Shader.
 *     ShaderRef shader = makeShader("matmul", descriptor);
 *
 *     // BAD: Invalid use of descritpor after building Shader
 *     descriptor.isInlined = false;
 * @endcode
 *
 * @{
 */
class ShaderRef : public Reference<Shader const> {
  public:
    using Base = Reference<Shader const>;

    ShaderRef(std::shared_ptr<Shader const> shader = nullptr);
    ShaderRef(Error error);
    ShaderRef(ShaderRef const &);

    ~ShaderRef();

    ShaderRef &operator=(ShaderRef const &);

    TensorList apply(TensorList const &) const;

    TensorList apply(TensorRef) const;
    TensorList apply(TensorRef, TensorRef) const;
    TensorList apply(TensorRef, TensorRef, TensorRef) const;
    TensorList apply(TensorRef, TensorRef, TensorRef, TensorRef) const;

    size_t getInputSize() const;
    size_t getOutputSize() const;

    const char *getInputName(int) const;
    const char *getOutputName(int) const;

    const cask::BaseKernelInfo *getKernelInfo() const;

    friend ShaderRef
    makeShader(const char *name, const TensorList &outputs, const TensorList &inputs);
    friend class Shader;

};

class ShaderDesc {
public:
    ShaderDesc();
    ~ShaderDesc();

    /**
     * @brief define input Tensor of Shader
     *
     * @param name          Unique name of user defined tensor within the same context
     *                      https://en.cppreference.com/w/cpp/language/identifiers
     * @param rank          rank of tensor
     * @param scalar_type   Data type of tensor element
     * @return TensorRef
     */
    // FIXME: use Args...?
    TensorRef makeTensor(const char *name, int rank, cask::ScalarType scalar_type);

    /**
     * @brief bind output tensor to Shader
     *
     *  TBD: remove
     *
     * @param[in] name - name of the output tensor.
     * @param tensor
     * @return cask::Error
     */
    cask::Error bindOutput(const char* name, TensorRef tensor);

    /**
     * @brief Create element variable and insert into kernel launch parameter
     *
     * @code{.cpp}
     *   struct Params {
     *       ...
     *       <ScalarType>xN <name>;
     *   };
     *
     *   __global__ void kernel(Params params) { ... }
     * @endcode
     *
     * @param name          Unique name of user defined parameter within the same context
     *                      https://en.cppreference.com/w/cpp/language/identifiers
     * @param scalar_type   predefined numeric types
     * @param multiple      Vector size for SIMD
     * @return ElementRef
     */
    ElementRef defineParameter(const char *name, cask::ScalarType scalar_type, int multiple);

    /**
     * @brief Create user defined parameter and insert into kernel launch parameter
     *
     * @code{.cpp}
     *   struct Params {
     *       ...
     *       <c_type> <name>;
     *   };
     *
     *   __global__ void kernel(Params params) { ... }
     * @endcode
     *
     * @param name          Unique name of user defined C-type variable within the same context
     *                      https://en.cppreference.com/w/cpp/language/identifiers
     * @param c_type
     * @return CVarRef
     */
    CVarRef defineParameter(const char *name, const char *c_type);

#if CASK_FUSION_SUPPORT_INLINED_CUDA
    /**
     * @brief Insert inlined CUDA source code at top of generated CUDA code
     *
     * @param code
     */
    cask::Error addInlineCuda(const char *code) noexcept;
#endif

    struct Details;
    Details *details{nullptr};

    friend Shader;
};

/**
 * @brief Factory method create a Graph representing computation of sequence of fusion operations
 *
 * @param name      Unique name of user defined shader within the same context
 *                  https://en.cppreference.com/w/cpp/language/identifiers
 * @param outputs   list of output tensors
 * @param inputs    list of input tensors
 * @return ShaderRef
 */
ShaderRef makeShader(const char *name, const TensorList& outputs, const TensorList& inputs);

/**
 * @brief Factory method create a computation graph using descriptor
 *
 * @param name          Unique name of user defined shader within the same context
 *                      https://en.cppreference.com/w/cpp/language/identifiers
 * @param descriptor    descriptor of Shader
 * @return ShaderRef
 */
ShaderRef makeShader(const char *name, ShaderDesc const& descriptor);

ShaderRef makeShader(const TileFuncRef& tile_func);

/** @} */

//////////////////////////////////////////////////////////////////////////////////////////
// ir::ShaderTemplate
//////////////////////////////////////////////////////////////////////////////////////////

class ShaderTemplateRef : public Reference<const ShaderTemplate> {
public:
    using Base = Reference<const ShaderTemplate>;

    ShaderTemplateRef(std::shared_ptr<ShaderTemplate const> shader_template = nullptr) : Base(shader_template) {}

    ShaderRef configure(ShaderConfigRef const& config) const;
};

/**
 * \defgroup ElementwiseFunc Elementwise Function
 *
 * @{
 */

/**
 * @brief Define an elementwise function on list of tensors. Will generate __device__ function as
 *
 * @code{.cpp}
 *     __device__ <out_type> <name>(in_args[0].type in_args[0].name, ...) {
 *         <code>;
 *     }
 * @endcode
 *
 * @param name          Unique name of user defined function within the same context
 *                      https://en.cppreference.com/w/cpp/language/identifiers
 * @param out_type      return type of __device__ function
 * @param in_args       Declarations of input arguments
 * @param code          CUDA __device__ code by customer
 * @return ElementwiseFuncRef
 */
ElementwiseFuncRef makeElementwiseFunc
(
    char const         *name,
    const ElementType  &out_type,
    ElementList const  &in_args,
    char const         *code,
    bool               is_inline = true
) noexcept;

class ElementwiseFuncDesc {
  public:
    bool isInlined{true};

    ElementwiseFuncDesc();
    ~ElementwiseFuncDesc();

    ElementwiseFuncDesc(ElementwiseFuncDesc&& desc);

    /**
     * @brief Define function parameter of input
     *
     * @param name          Unique name of user defined function within the same context
     *                      https://en.cppreference.com/w/cpp/language/identifiers
     * @param scalar_type   ScalarType except for { VOID_TYPE, INTPTR }
     * @param vector_size   Supported vector sizes are { 1, 2, 4 }
     * @return cask::Error
     *   - OK                           valid. yeah!
     *   - IR_INVALID_DESCRIPTOR        not a valid descriptor
     *   - IR_MULTIPLE_DEFINITION       name variable is defined
     *   - IR_INVALID_IDENTIFIER        not a valid identifier
     *   - IR_UNSUPPORTED_SCALAR_TYPE   ScalarType not supported
     *   - IR_UNSUPPORTED_VECTOR_SIZE   vector size is not supported
     */
    cask::Error addParameter(const char *name, cask::ScalarType scalar_type, int vector_size = 1);

    /**
     * @brief bind global scalar variable to this elementwise function
     *
     * @param variable
     * @return cask::Error
     */
    cask::Error bindGlobalVariable(const ElementRef& element);

    struct Details;
    Details *details{nullptr};

  private:
    friend class ElementwiseFunc;
};

/**
 * @brief Define an elementwise function with global scalar variables.
 *
 * Global scalar variable is different from 0 dim tensor
 *   - scalar value stored on device memory
 *   - 0-rank tensor is stored on device memory as scalar value
 *
 * @param name        Unique name of user defined function within the same context
 *                    https://en.cppreference.com/w/cpp/language/identifiers
 * @param out_type    Return type of __device__ function
 * @param descriptor  Use descriptor to describe complex settings
 * @param code        CUDA __device__ code by customer
 * @return ElementwiseFuncRef
 */
ElementwiseFuncRef makeElementwiseFunc
(
    char const           *name,
    const ElementType    &out_type,
    ElementwiseFuncDesc  && descriptor,
    char const           *code
) noexcept;

/** @} */

/**
 * \defgroup TileFunc Tile Function
 *
 * @{
 */

/**
 * @brief
 *
 * @param name          Unique name of user defined function within the same context
 *                      https://en.cppreference.com/w/cpp/language/identifiers
 * @param outputs       Declaration of output tensors
 * @param inputs        Declaration of input tensors
 * @param input_raw_pointers
 * @param code
 * @return TileFuncRef
 */
TileFuncRef makeTileFunc
(
    const char* name,
    const TileList& outputs,
    const TileList& inputs,
    const RawPointerList& input_raw_pointers,
    const std::string &code
);

/**
 * @brief Add tensor pointer to function parameter
 *
 * IR sequence to construct tile function:
 *
 * @code{.cpp}
 *   const int bias_rank = 1;
 *   TensorRef bias = makeTensor("bias", bias_rank);
 *
 *   CVarRef misc_params = makeCVar("params", "CustomizedParams");
 *
 *   TensorType const& out_type = matmul->getOutputType();
 *
 *   TiledTensorType accumulator_type = ir::typeCast<TensorType>(out_type);
 *   // accumulator_type = {
 *   //     .kind == GEMM_TILE
 *   //     .m = 128
 *   //     .n = 128
 *   //     .fragment_type = "Fragment_accumulator"
 *   // }
 *
 *   TileFuncDesc descriptor;
 *   descriptor.addParameter("accumulator", accumulator_type, true);
 *   descriptor.bindGlobalVariable(bias, true);
 *   descriptor.bindGlobalVariable(misc_params, true);
 *
 *   const char *code = ...;
 *   TileFuncRef epilogue = makeTileFunc("epilogue", descriptor, code);
 *
 *   ShaderRef epilogue_shader = tensorize(epilogue);
 * @endcode
 *
 * Generated code:
 *
 * @code{.cpp}
 *
 *   #include <xmma/hopper/<module>.h>
 *
 *   using <alias> = <alias>;
 *
 *   struct Tensor_params {
 *       int32_t dims[rank];
 *       int64_t strides[rank];
 *       [const] void *data;
 *   };
 *
 *   struct Tile_func_<name>_traits {
 *       //
 *   };
 *
 *   struct Tile_func_<name>_params {
 *       //
 *   };
 *
 *   struct Tile_func_<name> {
 *       Tensor_params const& <tensors[0].name>;
 *       ...
 *
 *       [const] <c_type> &user_defined_param_0;
 *       ...
 *
 *       TileDistribution const& tile_info;
 *
 *       Tile_func_<name>(Params const& params, TileDistribution const& tile_info_)
 *           : <tensors[0].name>(params.<tensors[0].name>)
 *           , tile_info(tile_info_)
 *           , ...
 *           , <user_defined_param_0>(params.user_defined_param_0)
 *           , ... { }
 *
 *       __device__ [inline] void execute
 *       (
 *           Fragment_<outputs[0].type.fragment> <outputs[0].name>,  /// > Output[0]
 *           ...
 *           Fragment_<params[0].type.fragment> <params[0].name>,    /// < Input Fragment
 *           Fragment_<params[1].type.fragment> <params[0].name>,    /// < Input Fragment
 *           ...
 *       ) {
 *           ////////////////////////////////////////////////
 *           // User code
 *           ////////////////////////////////////////////////
 *           module_<name>.execute();
 *       }
 *
 *       __device__ inline void clear() {
 *       }
 *
 *       __device__ inline void move() {
 *           matmul_p.move();
 *       }
 *
 *   };
 *
 *   Tile_func_<name>(params.<name>, binfo);
 *
 *   <name>.execute(frag_out_0, ..., frag_in_0, frag_in_1, ...);
 *
 * @endcode
 *
 */
class TileFuncDesc {
public:
    bool isInlined{false};

    /**
     * @brief Add tile as input/output
     *
     * @param name      name of tile parameter
     * @param tile_type type of tile parameter
     * @param is_const
     * @return cask::Error
     *   - IR_MULTIPLE_DEFINITION
     */
    cask::Error addParameter(const char *name, TileType const& tile_type, bool is_const = true);

    /**
     * @brief bind global scalar variable to this elementwise function
     *
     * @param element
     * @return cask::Error
     *   - element.check()    Propagate error
     */
    cask::Error bindGlobalVariable(ElementRef element);

    /**
     * @brief bind global tensor variable as extra input/output
     *
     * @param tensor
     * @param is_const
     * @return cask::Error
     *   - tensor.isValid()      Propagate error
     *   - IR_UNDEFINED_VARIABLE
     */
    cask::Error bindGlobalVariable(TensorRef tensor, bool is_const = true);

    /**
     * @brief bind global variable as raw c/c++ data
     *
     * @param name
     * @param c_var
     * @param is_const
     * @return cask::Error
     */
#if CASK_NOT_IMPLEMENTED
    // cask::Error bindGlobalVariable(CVarRef c_var, bool is_const = true);
#endif

    /**
     * @brief bind a variable with shader configuration
     *
     * @code{.cpp}
     * struct Tile_func_<name>_traits {
     *     using <type_name>     = <c_type_name>;
     *     using <type_name>     = Config_<shader_name>::<key>;
     *     <var_type> <var_name> = Config_<shader_name>::<key>;
     * };
     * @endcode
     *
     * @param config_key
     * @return cask::Error
     */
    cask::Error addTrait(const char *name, ShaderConfigRef config, const char *key);

    cask::Error addTrait(const char *name, const char *c_type);

    template<typename T>
    cask::Error addTrait(const char *name, T const& value);

    TileFuncDesc();

    struct Details;
    std::unique_ptr<Details> details;

    friend class TileFunc;
};

/**
 * @brief Create tile function which can be tensorized into shader
 *
 * @param name
 * @param descriptor
 * @param code
 * @return TileFuncRef
 */
TileFuncRef makeTileFunc
(
    const char* name,
    TileFuncDesc && descriptor,
    const char* code
);

RawPointerRef makeRawPointer(const char* name, cask::ScalarType scalar_type = cask::ScalarType::INT8);

/** @} */

/**
 * \defgroup Transformation Transformation Functions
 *
 *  \brief Basic operators performans transforms on function or tensors
 *   - tensorize
 *   - convert
 *   - reshape
 *   - transpose
 *   - reduce
 *   - reducePerTile
 *
 * @{
 */

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Transformation Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * \defgroup tensorize tensorize
 *
 * `tensorize` lifts an ElementwiseFunc into Shader
 *
 * @code{.hs}
 *   function :: Element e => (e, ...) -> e
 *   shader   :: Element e => (Tensor< Shape<Rank, Dim>, e >, ...) -> Tensor< Shape<Rank, Dim>, e >
 *
 *   tensorize :: Element e =>
 *                # Element wise function
 *                (((e, ...) -> e), Int)
 *                # Shader with broadcasting
 *                -> ((Tensor< Shape<Rank, Dim>, e >, ...) -> Tensor< Shape<Rank, Dim>, e >)
 * @endcode
 *
 * @{
 */

/**
 *! \brief Takes a scalar function perform tensorization + broadcast and convert it into Shader
 *
 *  Below is pseudo code describes functional behavior of transformed function/shader
 *
 *  // ir::tensorize(func_add,  5, {{3}, {0, 1, 2, 3, 4}});
 *
 *  std::vector< std::vector<int> > in_coords;
 *  in_coords.resize(in_dims.size());
 *  for (int i = 0; i < in_dims.size(); i++) {
 *     in_coords[i].resize(in_dims[i].size());
 *  }
 *
 *  for (auto nd_coord : nd_range(rank)) {
 *      // For this case, nd_coord = {ni, ci, di, hi, wi}
 *
 *      // Compute coordinates of the 1st tensorized input data
 *      //   In this case, in_coords[0] == {ci} of bias tensor
 *      for (int i = 0; i < in_coords[0].size(); i++) {
 *          in_coords[0][i] = nd_coord[in_dims[0][i]];
 *      }
 *      // Load from 1st tensorized input data
 *      v[0] = in[0].load(in_coords[0]);
 *
 *      // Compute coordinates of the 2nd tensorized input data
 *      //   In this case, in_coords[1] == {ni, ci, di, hi, wi} of activation tensor
 *      for (int i = 0; i < in_coords[1].size(); i++) {
 *          in_coords[1][i] = nd_coord[in_dims[1][i]];
 *      }
 *      // Load from 2nd tensorized input data
 *      v[1] = in[1].load(in_coords[1]);
 *
 *      ....
 *
 *      out.store(func(v[0], v[1], ...);
 *  }
 *
 * @param func            user provided scalar function
 * @param out_rank        rank of output tensor of tensorized function
 * @param num_in_args     how many input arguments of user provide function
 * @param in_ranks        rank of each input tensor of tensorized function
 * @param in_dim_orders
 * @return ShaderRef
 */
ShaderRef tensorize
(
    ElementwiseFuncRef func,
    int         out_rank,
    int         num_in_args,
    int         in_ranks[],         // tensor ranks of each input arguments
    int         in_dim_orders[]     // segmented array storing dim orders of each input tensor
) noexcept;

ShaderRef tensorize
(
    ElementwiseFuncRef func,
    int out_rank,
    int in0_rank, int in0_dim_order[]
) noexcept;

ShaderRef tensorize
(
    ElementwiseFuncRef func,
    int out_rank,
    int in0_rank, int in0_dim_order[],
    int in1_rank, int in1_dim_order[]
) noexcept;

ShaderRef tensorize
(
    ElementwiseFuncRef func,
    int out_rank,
    int in0_rank, int in0_dim_order[],
    int in1_rank, int in1_dim_order[],
    int in2_rank, int in2_dim_order[]
) noexcept;

/**
 * @brief Take element wise function and lift it into Shader producing a tensor
 *
 * numpy rules: https://numpy.org/doc/stable/user/basics.broadcasting.html
 *
 * tensorize must be used together with reshape and transpose for broadcast
 *
 * Given function :: (a, b, c, d) -> out
 *
 * Without reshape & transpose
 * ==============================
 *
 * @code{.cpp}
 * TensorRef tensor_a = makeTensor("a", 0);
 * TensorRef tensor_b = makeTensor("b", 1);
 * TensorRef tensor_c = makeTensor("c", 2);
 * TensorRef tensor_c = makeTensor("d", 3);
 * ShaderRef shader = tensorize(function, 4);
 * TensorRef tensor_d = shader.apply(tensor_a, tensor_b, tensor_c, tensor_d);
 * @endcode
 *
 * InputA  (Scalar):                    0        //     Scalar
 * InputB  (1d array):               [128]       //          W
 * InputC  (3d array):      [32, 128, 128]       //    C, H, W
 * InputD  (4d array):  [64, 32, 128, 128]       // N, C, H, W
 * Output  (3d array):  [64, 32, 128, 128]       // N, C, H, W
 *
 * With reshape
 * ==============================
 *
 * @code{.cpp}
 * TensorRef tensor_a = makeTensor("a", 0);
 * TensorRef tensor_b = makeTensor("b", 1);
 * TensorRef tensor_c = makeTensor("c", 2);
 * TensorRef tensor_c = makeTensor("d", 3);
 * ShaderRef shader = tensorize(function, 4);
 * TensorRef tensor_d = shader.apply(tensor_a, reshape(tensor_b, reshape({-1, 1, 1}, tensor_b), tensor_c, tensor_d);
 * @endcode
 *
 * InputA  (Scalar):                    0       //
 * InputB  (3d array):      [32,   1,   1]      //    C
 * InputC  (3d array):      [32, 128, 128]      //    C, H, W
 * InputD  (4d array):  [64, 32, 128, 128]      // N, C, H, W
 * Output  (3d array):  [64, 32, 128, 128]      // N, C, H, W
 *
 * @param func          User defined elementwise function
 * @param out_rank      Output rank of Elementwise Shader
 * @return ShaderRef    Create an Elementwise Shader
 */
ShaderRef tensorize
(
    ElementwiseFuncRef func,
    int                out_rank
) noexcept;

#if CASK_FUSION_NOT_SUPPORTED
/**
 * @brief               Lift/broadcast element(scalar or vector) value into tensor with specified rank
 *
 * @param element       Element-wise variable
 * @param out_rank      Rank of output tensor
 * @return TensorRef
 */
TensorRef tensorize(ElementRef const& element, int out_rank) noexcept;
#endif

ShaderRef tensorize(TileFuncRef func) noexcept;

/**
 * @brief Lift element type into tensor type
 *
 * @param element_type
 * @param out_rank
 * @return TensorType
 */
TensorType tensorize(ElementType const& element_type, int out_rank) noexcept;

/** @} */

/**
 * @brief Create a tensor with new shape from input tensor without changing the data
 *
 * @param input      expression of input tensor
 * @param new_rank   rank of new tensor
 * @param dims       dimensions of new tensor.
 *                    - -1 means infered from other dimensions
 *                    - positive value means exact value
 *                    - TODO: today we don't support expressions of dimension at user inferface
 * @return TensorRef a new tensor with reshape expression
 */
TensorRef reshape(TensorRef input, int new_rank, int dims[]);

/**
 * @brief Permute the dimensions of an array; returns tensor expression with dimensions permuted
 *
 * @param input       expression of input tensor
 * @param dim_orders  reordered dimensions
 * @return TensorRef  a new tensor with tranposed dimensions
 */
TensorRef transpose(TensorRef input, int rank, int dim_orders[]);

/**
 * @brief represent conversion for data type of input tensor
 *
 * @param input
 * @param output_type
 * @return TensorRef
 */
TensorRef convert(TensorRef input, const cask::ScalarType output_type);

/**
 * @brief represent reduction on specified dimension per tile of input tensor
 *
 * Pesudo code for functional description:
 * @code{.cpp}
 *
 *     using AccumulatorType = UserFunction::ReturnType;
 *
 *     AccumulatorType acc = initial_val;
 *     for (int i = 0; i < 0; i < dimensions[dim]) {
 *         acc = func(acc, array[i]);
 *     }
 *
 *     return acc;
 *
 * @endcode
 *
 * @param input             input tensor to reduce
 * @param func              customer defined binary functor for reduction,
 *                            - return type and arguments type are the same and also define accumulator type
 *                            - can only have two arguments
 * @param indentity_value   func(identity_value, x) = x
 *                            - NOTE: when func is compound functor instead of simple reduction operation,
 *                            -  this doesn't necessarily hold
 * @param reduction_dim     dimension to perform reduction
 * @return TensorRef
 */
TensorRef reducePerTile
(
    TensorRef input,
    ElementwiseFuncRef func,
    ElementRef identity_value,
    int reduction_dim
);


/**
 * @brief reduce input tensor on specified dimensions
 *
 * @param input Input tensor
 * @param func  Customized reduce function
 * @param identity_value identity value of reduce function
 * @param reduction_dim dimension to perform reduction
 * @return TensorRef
 */
TensorRef reduce
(
    TensorRef input,
    ElementwiseFuncRef func,
    ElementRef identity_value,
    int reduction_dim
);

/** @} */

/**
 * @brief Takes IR representation of a Shader then compile/link into cubin and create runnable cask::Shader object
 *
 *  - Type check to make sure IR of the computation graph is legal
 *  - Create ABI as well as kernel information for generated shader
 *  - Call backend, e.g. xmma-jit, to generate CUDA code
 *  - Call nvrtc to generate cubin of fused kernel
 *
 * @param shader  IR represetnation of a shader
 * @param ccv     Target SM architecture to compile
 * @param options compiling options
 */
cask::GraphShader *compile(ShaderRef shader,
                           cask::ComputeCapabilityVersion ccv,
                           int num_options = 0,
                           const char **options = nullptr);

template<typename OperationType>
class ShaderList {
public:
    typedef ShaderList<OperationType> ShaderListType;

    ShaderList();
    ~ShaderList();
    ShaderList(ShaderList&& other);

    ShaderRef findByName(const char* name);
    ShaderRef findByHandle(uint64_t handle);

    ShaderList findCanImplement(const OperationType &operation,
                                GemmChip forChip = hostArchitecture()) const;

    //Only cask internal use
    void push_back(const ShaderRef& shader);

    class ShaderListImpl;

    class const_iterator {
    public:
        const_iterator(const ShaderListImpl* shader_list, size_t i): index(i), shaderList(shader_list) {}
        const_iterator operator++() {
            ++index;
            return *this;
        }

        bool operator!=(const const_iterator & other) const {
            if (index != other.index || shaderList != other.shaderList)
                return true;
            return false;
        }

        const ShaderRef& operator*() const;

    private:
        size_t index;
        const ShaderListImpl* shaderList;
    };

    const_iterator begin() const;
    const_iterator end() const;

private:
    bool isSorted;
    void sortHandles();
    std::unique_ptr<ShaderListImpl> impl;
};

using XmmaJITGemmShaderList = ShaderList<cask::Gemm>;
XmmaJITGemmShaderList* availableXmmaJITGemmShaders();

using LinkableGemmShaderList = ShaderList<cask::Gemm>;
LinkableGemmShaderList* availableLinkableGemmShaders();

using LinkableConvShaderList = ShaderList<cask::Convolution>;
LinkableConvShaderList* availableLinkableConvShaders();

using XmmaJITConvShaderList = ShaderList<cask::Convolution>;
XmmaJITConvShaderList* availableXmmaJITConvShaders();

} // namespace ir
} // namespace cask
