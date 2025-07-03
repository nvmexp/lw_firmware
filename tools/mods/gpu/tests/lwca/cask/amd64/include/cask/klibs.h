#ifndef CASK_CORE_KLIBS_API_H
#define CASK_CORE_KLIBS_API_H
// enable doxygen:
//! @file
#include <cask/cask.h>
namespace cask {
/**
 * \defgroup klibs Kernel Library API
 *
 * \brief A set of APIs to load/write kernel library file.

 * @{
 */

/**
 * @brief Returns size of kernel library file header.
 *
 */
int32_t klibHeaderSize();

/**
 * @brief Returns size of a kernel header.
 * @return int32_t
 *
 */
int32_t klibKernelHeaderSize();

/**
 * @brief Returns the total size of kernel library header plus size of kernel headers for `kernel_count` kernels.
 *
 * @param kernel_count  how many kernels will be put into the kernel library 
 * @return size
 */
int32_t klibHeaderAndAllKernelHeaderSize(const int32_t kernel_count);

/**
 * @brief Initialize a buffer which is used to store kernel library header and kernel headers.
 *
 * @param klib_header_and_all_kernel_headers   the buffer to store kernel library header and kernel headers
 * @param kernel_count                         how many kernels will be put into the kernel library 
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::NULL_PTR
 *
 */
Error klibInitialize(     void*    klib_header_and_all_kernel_headers,
                    const int32_t  kernel_count);

/**
 * @brief Update kernel header of specified kernel
 *
 * @param klib_header_and_all_kernel_headers    the buffer to store kernel library header and kernel headers
 * @param kernel_index                          index (0-based) of the kernel whose header will be updated
 * @param op_def                                OperationDefinition of the kernel
 * @param kernel_info_data                      pointer to the buffer which contains kernel info
 * @param kernel_info_data_size                 size of the buffer which contains kernel info
 * @param kernel_data_size                      size of serialized shader
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *   - cask::Error::KLIB_KERNEL_INDEX_ILWALID
 *   - cask::Error::KLIB_KERNEL_HEADER_ALREADY_UPDATED
 *   - cask::Error::KLIB_KERNEL_HEADER_UPDATE_OUT_OF_SEQUENCE
 *   - cask::Error::KLIB_KERNEL_TYPE_ILWALID
 *   - cask::Error::KLIB_KERNEL_INFO_DATA_SIZE_ILWALID
 *   - cask::Error::KLIB_KERNEL_DATA_SIZE_ILWALID
 *
 *
 */
Error klibUpdateKernelHeader(      void                *klib_header_and_all_kernel_headers,
                             const int32_t              kernel_index,
                             const cask::OpDefinition   op_def,
                             const void                *kernel_info_data,
                             const int32_t              kernel_info_data_size,
                             const int32_t              kernel_data_size);

/**
 * @brief Finalize the kernel library header. No more updates of kernels are allowed.
 *
 * @param klib_header_and_all_kernel_headers      the buffer to store kernel library header and kernel headers
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *   - cask::Error::KLIB_NOT_ALL_KERNEL_HEADER_UPDATED
 *
 */
Error klibFinalize(void* klib_header_and_all_kernel_headers);

/**
 * @brief Get the total kernel count in the specified kernel library
 *
 * @param klib_header_and_all_kernel_headers      the buffer contains kernel library header and kernel headers
 * @param kernel_count                            how many kernels are in the kernel library 
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *
 */
Error klibKernelCount(const void *klib_header, int32_t& kernel_count);

/**
 * @brief Get the size of kernel library header and all kernel headers
 *
 * @param klib_header                the buffer which contains kernel library header
 * @param all_kernel_header_size     the size of all kernel headers
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *
 */
Error klibAllKernelHeaderSize(const void *klib_header, int32_t& all_kernel_header_size);

/**
 * @brief Checked if the kernel library header and kernels headers are valid.
 *
 * @param klib_header                the buffer which contains kernel library header
 * @param klib_all_kernel_headers    the buffer which contains all kernel headers
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *   - cask::Error::KLIB_VERSION_MISMATCH
 *   - cask::Error::KLIB_CRC_ERROR
 *
 */
Error klibCheckValidity(const void *klib_header, const void *klib_all_kernel_headers);

/**
 * @brief Get OpDefinition of the specified kernel.
 *
 * @param klib_header                the buffer which contains kernel library header
 * @param klib_all_kernel_headers    the buffer which contains all kernel headers
 * @param index                      index (0-based) of the specified kernel
 * @param op_def                     OpDefinition of the specified kernel
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *   - cask::Error::KLIB_KERNEL_INDEX_ILWALID
 *
 */
Error klibKernelOpDefinition(const void                 *klib_header,
                             const void                 *klib_all_kernel_headers,
                             const int32_t               index,
                                   cask::OpDefinition&   op_def);

/**
 * @brief Create a KernelInfo object for the specified kernel.
 * The object should be destroried by the caller if it is no longer needed by using @ref KernelInfo::destroy().
 *
 * @param klib_header                the buffer which contains kernel library header
 * @param klib_all_kernel_headers    the buffer which contains all kernel headers
 * @param index                      index (0-based) of the specified kernel
 * @param kernel_info                the pointer will point to the newly created KernelInfo object if success
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *   - cask::Error::KLIB_KERNEL_INDEX_ILWALID
 *   - cask::Error::NOT_IMPLEMENTED
 *
 */
Error klibLoadKernelInfo(const void              *klib_header,
                         const void              *klib_all_kernel_headers,
                         const int32_t            index,
                               cask::KernelInfo **kernel_info);

/**
 * @brief Get offset and size of specified kernel.
 *
 * @param klib_header                the buffer which contains kernel library header
 * @param klib_all_kernel_headers    the buffer which contains all kernel headers
 * @param index                      index (0-based) of the specified kernel
 * @param offset                     offset will be set to offset of the serialized dat a for the specified kernel
 * @param size                       size will be set to size of the serialized dat a for the specified kernel
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_HEADER_ILWALID
 *   - cask::Error::KLIB_KERNEL_INDEX_ILWALID
 *
 */
Error klibKernelDataOffsetAndSize(const void     *klib_header,
                                  const void     *klib_all_kernel_headers,
                                  const int32_t   index,
                                        int64_t&  offset,
                                        int32_t&  size);

/**
 * @brief  Create a shader object with the specified KernelInfo object.
 * The shader object needs to be destroyed by the caller if it is no longer needed by using @ref Shader::destroy().
 *
 * @param kernel_info         pointer to the specified KernelInfo object
 * @param buff                buffer which contains serialized data of the shader
 * @param size                size of the buffer
 * @param shader              shader will point to the Shader object created if success.
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::KLIB_ILWALID_KERNEL_INFO
 *   - cask::Error::NOT_IMPLEMENTED
 *
 */
Error klibLoadShader(const KernelInfo    *kernel_info,
                     const void          *buff,
                     const int32_t        size,
                           cask::Shader **shader);

/**
 * @brief  Create a shader object with the specified KernelInfo object.
 * The shader object needs to be destroyed by the caller if it is no longer needed.
 *
 * @param op_def              OpDefinition of the specified shader
 * @param buff                buffer which contains serialized data of the shader
 * @param size                size of the buffer
 * @param shader              shader will point to the Shader object created if success.
 * @return cask::Error
 *   - cask::Error::OK
 *   - cask::Error::NOT_IMPLEMENTED
 *
 */
Error klibLoadShader(cask::OpDefinition   op_def,
                     const void          *buff,
                     const int32_t        size,
                           cask::Shader **shader);

}
/** @} */
#endif
