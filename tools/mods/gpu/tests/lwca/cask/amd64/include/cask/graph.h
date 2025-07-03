
#ifndef INCLUDE_GUARD_CASK_GRAPH_H
#define INCLUDE_GUARD_CASK_GRAPH_H

#include <map>
#include <vector>
#include <memory>
#include <cinttypes>
#include <cstring>

#include <cask/cask.h>

#include "cask/operation.h"

namespace cask {

template< class T, class R = void >
struct enable_if_type { typedef R type; };

struct ScalarTypeProperties {
    cask::ScalarType scalarType;
    int32_t sizeInBits;
    const char *name;

    ~ScalarTypeProperties() {}

    static ScalarTypeProperties const& get(cask::ScalarType scalar_type);

    static ScalarTypeProperties const& fromName(const char* name);

    template<typename T>
    static ScalarTypeProperties const& get();
};

class ScalarValue {
public:
    static constexpr int64_t storage_size = 16;

    ScalarValue();

    explicit ScalarValue(float value);
    explicit ScalarValue(double value);
    explicit ScalarValue(int32_t value);
    explicit ScalarValue(uint32_t value);
    explicit ScalarValue(int64_t value);
    explicit ScalarValue(uint64_t value);
    explicit ScalarValue(bool value);
    explicit ScalarValue(uint16_t value);
    explicit ScalarValue(int8_t value);
    explicit ScalarValue(uint8_t value);

    // explicit ScalarValue(BuiltinOperatorID value);

    ~ScalarValue() {}

    ScalarType getType() const { return this->scalar_type_; }

    int64_t sizeInBytes() const {
        return NumericTraits::from_type(this->scalar_type_).bits / 8;
    }

    template<typename T>
    T as() const {
        static_assert(sizeof(T) <= sizeof(this->data_), "Unsupported type");
        assert(make_NumericTraits<T>().type == this->scalar_type_);
        T tmp;
        std::memcpy(&tmp, this->data_, sizeof(T));
        return tmp;
    }

    template<typename T>
    Error getValue(T *value) const {
        if (make_NumericTraits<T>().type != this->scalar_type_) {
            return Error::ILWALID_PARAMETER;
        }

        std::memcpy(value, this->data_, sizeof(T));
        return Error::OK;
    }

    const void *data() const { return static_cast<const void *>(this->data_); }
    void *data() { return static_cast<void *>(this->data_); }

private:
    //FIXME: this will cause 'operator ==' is ambiguous on windows.
    // friend bool operator==(const ScalarValue& lhs, const ScalarValue& rhs);

    template<typename T>
    friend bool operator==(const ScalarValue& lhs, const T& rhs) {
        if (make_NumericTraits<T>().type == lhs.getType()) {
            return rhs == lhs.as<T>();
        }
        return false;
    }

    template<typename T>
    friend bool operator==(const T& lhs, const ScalarValue& rhs) {
        if (make_NumericTraits<T>().type == rhs.getType()) {
            return lhs == rhs.as<T>();
        }
        return false;
    }

    ScalarType scalar_type_;
    uint8_t data_[storage_size];          // Large enough to hold 128bit value
};

class ArrayValue {
public:
    ArrayValue(ScalarType element_type, int64_t length);
    ~ArrayValue();

    template<typename T>
    explicit ArrayValue(std::vector<T> const& values)
        : ArrayValue(ScalarValue(T()).getType(), static_cast<int64_t>(values.size())) {
        std::memcpy(this->data_, values.data(), this->size_in_bytes_);
    }

    template<typename T>
    explicit ArrayValue(std::initializer_list<T> values)
        : ArrayValue(ScalarValue(T()).getType(), static_cast<int64_t>(values.size())) {
        std::copy(values.begin(), values.end(), reinterpret_cast<T *>(this->data_));
    }

    inline int64_t sizeInElements() const {
        return this->size_in_elements_;
    }

    inline int64_t sizeInBytes() const {
        return this->size_in_bytes_;
    }

    inline ScalarType getElementType() const {
        return this->element_type_;
    }

    inline Error setValue(int64_t index, ScalarValue const& value) {
        if (index < this->size_in_elements_) {
            std::memcpy(
                    this->data_ + index * this->element_size_, value.data(), this->element_size_);
            return Error::OK;
        }
        return Error::ILWALID_PARAMETER;
    }

    inline Error getValue(int64_t index, ScalarValue *value) const {
        if (index < this->size_in_elements_) {
            std::memcpy(
                    value->data(), this->data_ + index * this->element_size_, this->element_size_);
            return Error::OK;
        }
        return Error::ILWALID_PARAMETER;
    }

    /**
     * @brief return pointer to first element of array
     *
     * @tparam T
     * @return T*
     */
    template<typename T>
    inline T *as() const {
        return reinterpret_cast<T *>(this->data_);
    }

    inline const void* data() const {
        return reinterpret_cast<const void*>(this->data_);
    }

    inline void* data() {
        return reinterpret_cast<void*>(this->data_);
    }

private:
    const ScalarType element_type_;
    const int32_t    element_size_;

    int64_t size_in_elements_{0};
    int64_t size_in_bytes_{0};
    uint8_t *data_;
};

// inline bool operator==(ScalarValue const& lhs, ScalarValue const& rhs) {
//     return (lhs.scalar_type_ == rhs.scalar_type_) &&
//            (lhs.data_[0] == rhs.data_[0]);
// }

namespace ir {
struct StructureType;
}

class ProblemDesc {
public:
    using Description = cask::ProblemDesc;

    // FIXME: Remove this from public header file
    ProblemDesc(cask::ir::StructureType *desc_ty = nullptr, void *data = nullptr);
    ~ProblemDesc();

    ProblemDesc(ProblemDesc &&other) noexcept;

    Error isValid() const noexcept {
        return this->error_;
    }

    Error set(char const *name, TensorDesc const &desc);
    Error get(char const *name, TensorDesc *desc) const;

    Error set(char const *name, ScalarValue const &value);
    Error get(char const *name, ScalarValue *value, bool allow_miss = false) const;

    /**
     * @brief set array value into Problem Descriptor
     *
     * @param name              field name
     * @param value             user supplied array of values
     * @return Error
     *  - OK
     *  - ILWALID_PARAMETER     invalid field name
     *                          || destination is not array type
     *                          || destination scalar type doesn't match
     *                          || destination size doesn't match
     */
    Error set(char const *name, ArrayValue const &value);

    /**
     * @brief get array value from Problem Descriptor
     *
     * @param name              field name
     * @param value             memcpy value into user buffer
     * @param allow_miss
     * @return Error
     *  - OK
     *  - ILWALID_PARAMETER     invalid field name
     *                          || destination is not array type
     *                          || destination scalar type doesn't match
     *                          || destination size doesn't match
     *
     */
    Error get(char const *name, ArrayValue *value, bool allow_miss = false) const;

    Error set(char const *name, BuiltinOperatorID const &value);
    Error get(char const *name, BuiltinOperatorID *value) const;

    /**
     * @brief perform std::memcpy from user provided value to destination
     *
     * WARNING: No type check. It's user's reponsibility to maintain correct semantics.
     *
     * @param name          name of key in problem description
     * @param value_ptr     pointer to source data
     * @return Error
     */
    Error set(char const *name, void const *value_ptr);

    template<typename T>
    Error get(char const *name, void *value) const;

    /**
     * @brief return plain buffer storage values
     *
     * @return void*
     */
    void *data() const;

    const Description& getDescription() const;

    ProblemDesc &operator=(ProblemDesc &&);

    void print() const;

private:
    class Details;
    std::unique_ptr<Details> details_;

    Error error_;
};

class PerformanceKnob {
public:
    PerformanceKnob();
    ~PerformanceKnob();

    Error set(char const *name, ScalarValue const &value);
    Error get(char const *name, ScalarValue *value) const;

private:
    class PerformanceDetails;
    std::unique_ptr<PerformanceDetails> details_;
};

class LaunchParams {
public:
    class Details;

    LaunchParams(void *params_data, int64_t size, Details const *detail = nullptr);
    ~LaunchParams();

    LaunchParams(LaunchParams &&other) noexcept;

    Error set(char const *name, void *data);
    Error set(char const *name, void const *data);
    Error set(char const *name, ScalarValue const &value);

    Error get(char const *name, ScalarValue *value) const;
    Error get(char const *name, void **data) const;

    inline void *getParamsData() const { return this->params_data_; }

    void print() const;

    friend class GraphShader;

private:
    Details const *details_;
    void *params_data_;
    int64_t params_data_size_;
};

class GraphShader : public Shader {
public:
    /**
     * @brief Tell user if this shader is valid shader or hold error information
     *
     * @return cask::Error
     *     - cask::Error::OK
     *     - cask::Error::BAD_LWBIN  bad lwbin caused by lwca -> lwbina compilation
     */
    cask::Error isValid() const { return this->error_; }

    virtual int32_t getNumInputs() const { return -1; }

    virtual int32_t getNumOutputs() const { return -1; }

    virtual const char* getInputName(int index) const { return nullptr; }

    virtual const char* getOutputName(int index) const { return nullptr; }

    // FIXME: change to pure virtual
    virtual const TensorType *getInputTensorType(int index) const {
        return nullptr;
    }

    // FIXME: change to pure virtual
    virtual const TensorType *getOutputTensorType(int index) const {
        return nullptr;
    }

    virtual ScalarType getSupportedType_ProblemDesc(char const *name) const {
        return ScalarType::FP32;
    }

    /// Get size of problem size buffer of the kernel
    ///   optional for Shader supporting pre-defined operations
    virtual int64_t getProblemDescSize() const;

    /// Get problem size descriptor with user provided data buffer
    virtual ProblemDesc getProblemDesc(void *data) const {
        return ProblemDesc(this->problem_desc_ty_, data);
    }

    virtual ScalarType getSupportedType_LaunchParams(char const *name) const {
        return ScalarType::FP32;
    }

    /// Get size of launch parameter buffer of defined by implementation kernel
    /// The size must be orthogonal to problem size
    /// NOTE: it’s not necessarily the sum of available attributes (it’s always larger)
    virtual int64_t getLaunchParamsSize() const = 0;

    /// Obtain launch params descriptor with user provided data buffer
    virtual LaunchParams getLaunchParams(void *data) const = 0;

    virtual int64_t getHostReservedSize
    (
        cask::ProblemDesc const& problem_desc,
        cask::HardwareInformation const& hardware_info
    ) const = 0;

    /////////////////////////////////////////////////////////////////////////
    /// initialize host reserved buffer including
    ///  * parameter buffer for per kernel
    ///  * opaque structure of host data
    ///    * number of kernels to launch
    ///    * launch configuration per kernel (e.g. split-k)
    ///      - grid/block dimensions
    ///      - shared memory size
    ///    * caskManaged*
    ///    * kernel specific data structure (optional)
    ///      - copy of problem size (optional)
    /////////////////////////////////////////////////////////////////////////
    virtual Error initHostReservedSpace
    (
        void              *launch_params_data,
        void              *host_data,
        ProblemDesc const& problem_desc,
        cask::HardwareInformation const& hardware_info
    ) const = 0;

    /**
     * @brief Get size of Reserved buffer
     *
     * @param problem_desc
     * @param hardware_info
     * @param host_data (optional) implementation should support callwlate reserved size without host buffer
     * @return int64_t
     */
    virtual int64_t getDeviceReservedSize
    (
        cask::ProblemDesc const& problem_desc,
        cask::HardwareInformation const& hardware_info,
        void const *host_data = nullptr
    ) const = 0;

    /**
     * @brief initialize device reserved buffer which is defined by kernel, e.g.
     *  * strided dgrad transformed buffer
     *
     * may ilwoke a lwca kernel to do the job
     * it’s optional for kernel developer to reuse data from initHostReservedSpace
     *
     * @param device_data
     * @param host_data
     * @param problem_desc
     * @param stream
     * @return Error
     */
    virtual Error initDeviceReservedSpace
    (
        void       *device_data,
        void const *host_data,
        cask::ProblemDesc const& problem_desc,
        cask::HardwareInformation const& hardware_info,
        platformStream_t stream
    ) const = 0;

    /**
     * @brief Get size of workspace or scratch buffer required by kernel run
     *
     *   Workspace/scratch is used for kernel to store intermediate data which can be throw away. E.g.
     *     split-k kernel uses temporary buffer to store batched result for reduction
     *
     * @param problem_desc
     * @param hardware_info
     * @param host_data
     * @return int64_t
     */
    virtual int64_t getDeviceWorkspaceSize
    (
        cask::ProblemDesc const &problem_desc,
        cask::HardwareInformation const &hardware_info,
        void const *host_data = nullptr
    ) const {
        return 0;
    }

    virtual Error finalizeLaunchParams
    (
        void *launch_params_data,
        void const *host_data,
        void *device_data
    ) const {
        return cask::Error::OK;
    }

    virtual Error run
    (
        void const *launch_params_data,
        void const *host_data,
        void *device_reserved_data,
        void *device_workspace_data,
        platformStream_t stream
    ) const = 0;

    /// \brief Returns the chip type supported by this kernel.
    /// Needed by ShaderList.
    virtual GemmChip chip() const;

    virtual Error isConsistent(const ProblemDesc& desc) const {
        return cask::Error::OK;
    }

    virtual Error canImplement(const ProblemDesc& desc) const {
        return cask::Error::OK;
    }

    virtual Error canImplement(const ProblemDesc& desc, const ComputeCapabilityVersion ccv) const {
        return cask::Error::OK;
    }

    virtual int64_t getSerializedSize() const { return -1; }

    virtual Error serialize(uint8_t* buf, const int64_t size) const {
        return cask::Error::INTERNAL;
    }

    static GraphShader* deserialize(const void* buf, const int64_t size);

    // TODO: @ben keep it here until we can get lwca code from other interface.
    virtual const char* getLwdaCode() const {
        return nullptr;
    }

    GraphShader(const std::string& name);

    GraphShader
    (
        const cask::GraphKernelInfo  *kernel_info,
        cask::ir::StructureType *problem_desc_ty = nullptr,
        bool delete_problem_desc = false
    );

    virtual ~GraphShader();

protected:
    const cask::GraphKernelInfo* kernel_info_;

    cask::ir::StructureType *problem_desc_ty_{nullptr};
    const bool delete_problem_desc_;

    Error error_{Error::OK};
};

typedef ShaderList<GraphShader, ProblemDesc> GraphShaderList;

inline
const GraphShaderList* availableGraphShaders() {
    return GraphShaderList::availableShaders();
}


} //namespace cask

#endif // INCLUDE_GUARD_CASK_GRAPH_H
