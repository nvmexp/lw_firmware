
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

    template<typename T>
    T as() const {
        static_assert(sizeof(T) <= sizeof(this->data_), "Unsupported type");
        assert(ScalarTypeProperties::get<T>().scalarType == this->scalar_type_);
        T tmp;
        std::memcpy(&tmp, this->data_, sizeof(T));
        return tmp;
    }

    const void *data() const { return static_cast<const void *>(this->data_); }
    void *data() { return static_cast<void *>(this->data_); }

    ScalarType getType() const { return this->scalar_type_; }

    template<typename T>
    Error getValue(T *value) const {
        if (ScalarTypeProperties::get<T>().scalarType != this->scalar_type_) {
            return Error::ILWALID_PARAMETER;
        }

        std::memcpy(value, this->data_, sizeof(T));
    }

private:
    //FIXME: this will cause 'operator ==' is ambiguous on windows.
    // friend bool operator==(const ScalarValue& lhs, const ScalarValue& rhs);

    template<typename T>
    friend bool operator==(const ScalarValue& lhs, const T& rhs) {
        if (ScalarTypeProperties::get<T>().scalarType == lhs.getType()) {
            return rhs == lhs.as<T>();
        }
        return false;
    }

    template<typename T>
    friend bool operator==(const T& lhs, const ScalarValue& rhs) {
        if (ScalarTypeProperties::get<T>().scalarType == rhs.getType()) {
            return lhs == rhs.as<T>();
        }
        return false;
    }

    ScalarType scalar_type_;
    uint8_t data_[16];          // Large enough to hold 128bit value
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

    Error set(char const *name, TensorDesc const &desc);
    Error get(char const *name, TensorDesc *desc) const;

    Error set(char const *name, ScalarValue const &value);
    Error get(char const *name, ScalarValue *value, bool allow_miss = false) const;

    Error set(char const *name, BuiltinOperatorID const &value);
    Error get(char const *name, BuiltinOperatorID *value) const;

    Error set(char const *name, void const *value);

    template<typename T>
    Error get(char const *name, void *value) const;

    void *data() const;

    const Description& getDescription() const;

    ProblemDesc &operator=(ProblemDesc &&);

    void print() const;

  private:
    class ProblemDescDetails;
    std::unique_ptr<ProblemDescDetails> details_;
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
     *     - cask::Error::OK  valid shader, yeah!
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
        int owner_struct_ty = 0
    );

    virtual ~GraphShader();

protected:
    cask::ir::StructureType *problem_desc_ty_{nullptr};
    const cask::GraphKernelInfo* kernel_info_;
    const int owner_struct_ty_;

    Error error_{Error::OK};
};

typedef ShaderList<GraphShader, ProblemDesc> GraphShaderList;

const GraphKernelInfo* toGraphKernelInfo(const KernelInfo* kernelInfo);

inline
const GraphShaderList* availableGraphShaders() {
    return GraphShaderList::availableShaders();
}


} //namespace cask

#endif // INCLUDE_GUARD_CASK_GRAPH_H
