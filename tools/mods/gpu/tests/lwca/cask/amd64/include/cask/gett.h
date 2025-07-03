
#ifndef INCLUDE_GUARD_CASK_GETT_H
#define INCLUDE_GUARD_CASK_GETT_H

#include <cask/cask.h>
#include <map>
#include <vector>
#include <memory>
#include <cinttypes>
#include <cstring>

#include "cask/operation_gett.h"

namespace cask {

class Gett;
struct GettShader;

typedef ShaderList<GettShader,Gett> GettShaderList;

///////////////////////////////////////////////////////////////////////////////

inline
const GettShaderList* availableGettShaders() {
    return GettShaderList::availableShaders();
}

///////////////////////////////////////////////////////////////////////////////
class GettShader : public Shader {
public:
    const GettKernelInfo* getInfo() const {
        return dynamic_cast<const GettKernelInfo*>(kernelInfo); 
    }
    // pure virtual interface to get every tensor's type. 
    virtual const TensorType *aTensorType() const = 0;

    virtual const TensorType *bTensorType() const = 0;

    virtual const TensorType *cTensorType() const = 0;

    virtual const TensorType *dTensorType() const = 0;

    virtual ScalarType getSupportedType_ProblemDesc(char const *name) const {
        return ScalarType::FP32;
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
        cask::Gett const& operation,
        cask::HardwareInformation const &hardware_info
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
        cask::Gett const& operation,
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
        cask::Gett const& operation,
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
        cask::Gett const& operation,
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
        cask::Gett const &operation,
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
    virtual GemmChip chip() const = 0;

    virtual Error canImplement(const Gett& operation) const {
        return cask::Error::OK;
    }

    virtual Error canImplement(const Gett& operation, const ComputeCapabilityVersion ccv) const {
        return cask::Error::OK;
    }

    virtual int64_t getSerializedSize() const { return -1; }

    virtual Error serialize(uint8_t* buf, const int64_t size) const {
        return cask::Error::INTERNAL;
    }

    static GettShader* deserialize(const void* buf, const int64_t size);

    // TODO: @ben keep it here until we can get lwca code from other interface.
    virtual const char* getLwdaCode() const {
        return nullptr;
    }
    
    GettShader
    (
        const cask::KernelInfo  *kernel_info
    ): Shader(kernel_info)
    {
    }

    virtual ~GettShader() {
    }

};

} //namespace cask

#endif // INCLUDE_GUARD_CASK_GETT_H
