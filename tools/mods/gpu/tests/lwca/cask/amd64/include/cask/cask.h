#ifndef INCLUDE_GUARD_CASK_H
#define INCLUDE_GUARD_CASK_H

#include <lwda_runtime_api.h>   // for lwdaError_t and lwdaStream_t

// safety core library header
#include <cask/safe/cask_api.safe.h>

// macros, enums, functions, the ShaderList template, and the ShaderParams
// struct (from Fast Kernels)
#include "cask_core.h"
// getPlatform, platformError_t

#include "metadata_collection.h"
#include "operation_kernel_info.h"
#include "kernels/kernel_info.h"
#include "lwda_driver_wrapper.h"

#if defined(CASK_FUSION_API_ENABLED)
#include "lwrtc_wrapper.h"
#endif

#include "cask_platform.h"
// tensor descriptor class and functions that operate on it
#include "tensor_desc.h"
// Operation superclass
#include "operation.h"

// Operation for GETT
#include "operation_gett.h"

#include "api_logging.h"

// ENABLE_FUNCTIONAL_TREE set via CMAKE
#if defined(ENABLE_FUNCTIONAL_TREE)
// some ampere features are present in the tree, but they may not be functionally necessary
#undef FT_AMPERE_TREE_LEVELS
// "functional tree" data structure files
#include "ft_kernel_record.hpp"
#include "ft_sass_level.hpp"
#include "ft_sass_gemm_level.hpp"
#include "ft_xmma_level.hpp"
#include "ft_first_layer_level.hpp"
#include "ft_lwtlass_level.hpp"
#endif

// Colwolution and ColwShader and availableColwShaders()
#include "colwolution.h"
// Gemm and GemmShader and availableGemmShaders()
#include "gemm.h"
// FullyConnected and FullyConnectedShader and availableFullyConnectedShaders()
#include "fully_connected.h"
// DataGradient and ColwDgradShader and
// ColwDgradShaderList::availableShaders()
#include "dgrad.h"
// Decolwolution and DecolwShader and
// DecolwShaderList::availableShaders()
#include "decolw.h"
// WeightedGradient and WeightedGradientShader and
// ColwWgradShaderList::availableShaders()
#include "wgrad.h"
// PoolingOperation and PoolingShader
#include "pooling.h"
// SoftmaxOperation and SoftmaxShader
#include "softmax.h"

#if defined(ENABLE_FUNCTIONAL_TREE)
// "functional tree" API files
#include "ft_nary_node.hpp"
#include "ft_tree.hpp"
#include "ft_search.hpp"
#endif

// FIXME: linkable shader need to be folded into graph shader
// Runtime fusion APIs
#include "linkable_shader.h"

#include "graph.h"
// Gett and GettShader
#include "gett.h"

#include "kernel_lib.h"
#include "klib_loader.h"
#include "klibs.h"

#include "cask_init_kernels.h"
//////////////////////////////////////////////////////////////////////////////
//
// !!!!!!!! NOTE IF YOU'RE ADDING ADDITIONAL INCLUDES: !!!!!!!!!!!
//
// Doxygen doesn't add #included headers to the list of files it processes (it
// does *preprocess* them for macros defines, but doesn't actually think you
// want them dolwmented).  So if you want your header dolwmented you need to
// add it to the INPUT variable in Doxyfile.  The correct Doxyfile is the one
// in //sw/gpgpu/MachineLearning/Cask/docs/Doxyfile.
//
//////////////////////////////////////////////////////////////////////////////

//! This file contains the declarations of the cask namespace.
//! HTML documentation can be produced by running Doxygen

/// \mainpage Cask: The Deep Learning Kernel Library
///
/// \section intro Overview
///
/// The CASK API, is the internal C++
/// interface wrapping our SASS and LWCA colwolution and gemm kernels for use
/// by LWPU library teams such as lwDNN, TensorRT, and lwBLAS.  CASK abstracts
/// implementation-specific details of ilwoking the kernels, including filling
/// in all the parameters we pass to the kernels, and pre-callwlating book-keeping
/// details like the generated offset tables.
///
///
/// \section get_start Getting Started
///
/// Ilwoking the CASK API is a multi-step process so the client (caller) has
/// control over buffer management on both the host and device.
///
/// \subsection init_cask Initialize CASK
/// Initialize Cask by calling initCask(). It will load shaders and populate
/// various Shaderlists based on shader type.
///
///     initCask();
///
/// \subsection pre_tensor Prepare Tensors
/// Create descriptors for the tensors and log them in an Operation object
///
///     // params to the constructor are always N,C,H,W (even if the
///     // strides make the tensor N,H,W,C)
///     TensorDesc inputDesc(128, 256, 196, 196);
///     TensorDesc outputDesc(128, 10, 194, 194);
///     TensorDesc filterDesc(10, 256, 3, 3);
///
///     // Create an object of the operation Colwolution,
///     // which contains info specified by users.
///     Colwolution colwolution;
///
///     colwolution.setInputDesc(inputDesc);
///     colwolution.setOutputDesc(outputDesc);
///     colwolution.setFilterDesc(filterDesc);
///
/// Fill in `inputData` and `filterData`. Depending on the settings in RunInfo,
/// users may have to prepare device buffers of data. (often `inputData` will be the `outputData` of
/// the previously run kernel.)
///
///     void* inputData = lwdamalloc(inputDesc.sizeInBytes());
///     void* outputData = lwdamalloc(outputDesc.sizeInBytes());
///     void* filterData = malloc(filterDesc.sizeInBytes());   // filter starts on host, not device
///
/// Check tensor dimension consistency
///
///     if (colwolution.isConsistent() != Error::OK) error();
///
/// \subsection find_shader Find the Right Shader
/// Find an appropriate shader by searching through availableShaders().
///
///     const ColwShaderList* shaderList = availableShaders();
///
///     const Shader* shader = 0;
///     for (ShaderList::const_iterator i = shaderList->begin();
///          i != shaderList->end();
///          i++) {
///       if (*i->canImplement(colwolution) && *i->has some desirable property) {
///         shader = *i;
///         break;
///       }
///     }
///     assert (shader && shader->canImplement(colwolution));
///
/// One could also find a shader directly with its name.
///
///     const ShaderList* shaderList = ShaderList::availableShaders();
///     shader = shaderList->findByName(kernel_name);
///
/// \subsection setup_runinfo Set up RunInfo
/// RunInfo is the datastructure that encapsulates all problem-specific info
/// for a single run. This includes the operation (colwolution or gemm),
/// host and device workspaces, and info on tensor layout. The initHost, initDevice
/// and run APIs all take the RunInfo object that is specified by clients.
///
///     RunInfo ri;
///     ri.op = static_cast<Operation*>(colwolution);
///     ri.caskManagedTensorB = true;
///     ri.caskManagedBias = true;
///     ri.hostTensorB = filterData;
///     ri.hostBias = biasData;
///     ri.batchSize = batchSize
///
/// \subsection init_buffers Initialize host and device buffers.
/// Depending on the kernel and the case, Cask initialize host
/// and device buffers to carry params and additional buffers.
///
/// Initialize the host
///
///     if (shader->getHostReservedSize(ri) != Error::OK) error();
///     ri.hostBuf = malloc(ri.hostBufSize);
///     if (shader->initHostReservedSpace(ri) != Error::OK) error();
///
/// Initialize the device with the filter and other reserved space data.
///
///     if (shader->getDeviceReservedSize(ri) != Error::OK) error();
///     ri.deviceBuf = lwdamalloc(ri.deviceBufSize);
///     if (shader->initDeviceReservedSpace(ri,
///                                         lwdaStream) != Error::OK) error();
///
/// \subsection run_shader Run the kernel
/// And actually run the colwolution with the data
///
///     if (shader->run(ri,
///                     outputData,
///                     inputData,
///                     NULL, // B is in device workspace
///                     outputData,
///                     NULL,
///                     NULL,
///                     NULL,
///                     lwdaStream) != Error::OK) error();
///
/// It is the client's responsibility to copy the `outputData` off the device back to
/// the host (or leave it as the input buffer for the next layer).
///
/// If the serializable host and/or device buffers have been previously saved
/// and then are deserialized into appropriate buffers, several of the steps
/// can be skipped.  We assume here that `deviceBufSize`, `hostBufSize`, and
/// `hostBuf` have all been deserialized onto the host, and that `deviceBuf`
/// has been deserialized, and copied to the device.
///
///     if (cask::getInternalVersion() != (*version number at serialization time*)) recallwlateBuffers();
///     Shader* shader = availableShaders()->findByName(shaderName);
///     assert(shader && shader->canImplement(colwolution));
///
///     if (shader->run(colwolution,
///                     deviceBufSize, deviceBuff, hostBufSize, hostBuf,
///                     batchSize,
///                     outputData,
///                     inputData,
///                     outputData,
///                     lwdaStream) != Error::OK) error();
///
/// \section details CASK Core Components
/// \subsection op Operations
/// CASK \ref cask::Operation "Operation" classes model the callwlations that are performed by CASK shaders.
/// Typical operations include \ref cask::Gemm "Gemm",
/// \ref cask::Colwolution "Colwolution", \ref cask::ColwolutionDgrad "ColwolutionDgrad",
/// \ref cask::ColwolutionWgrad "ColwolutionWgrad" and more.
/// Each operation class in CASK maintains a Description struct that describes
/// problem sizes and other optional settings specified by CASK users.
///
/// \subsection shader Shaders
/// The CASK \ref cask::Shader "Shader" classes are abstractions of the real GPU kernel implemenations.
/// CASK defines various Shader classes, such as
/// \ref cask::GemmShader "GemmShader" and \ref cask::ColwShader "ColwShader",
/// to work with a certain type of operation.
/// Thus, many APIs of a Shader class take an Operation object.
/// The APIs would verify if the GPU kernel can support this operation,
/// prepare params and optional data, then finally launch the kernel in run().
///
/// \subsection kernel_info KernelInfo
/// CASK maintains a library of kernels, and KernelInfo is
/// designed as the metadata of every kernel.
/// Each Shader object holds a pointer to KernelInfo object
/// that describes the properties of the underlying GPU kernel.
/// The properties include the data type and layout of each tensor
/// supported Spas, shared memory usage, and more.
/// The KernelInfo instances are constructed in CASK initialization.
/// \sa \ref cask::BaseKernelInfo "BaseKernelInfo", \ref cask::ColwKernelInfo "ColwKernelInfo",
/// \ref cask::ColwDgradKernelInfo "ColwDgradKernelInfo",
/// \ref cask::ColwWgradKernelInfo "ColwWgradKernelInfo",
/// \ref cask::GemmKernelInfo "GemmKernelInfo",
/// \ref cask::PoolingKernelInfo "PoolingKernelInfo"
///
/// \subsection tensor_desc TensorDesc
/// \ref cask::TensorDesc "TensorDesc", as the name suggests, is the description of a CASK tensor.
/// CASK users construct TensorDesc objects according to their problem sizes.
/// TensorDesc contains info about number of dimensions,
/// size and stride of each dimensions, and more.
/// An Operation class constains multiple TensorDesc objects to specify input and output.
///
/// \subsection run_info RunInfo
/// \ref cask::RunInfo "RunInfo" is a POD class encapsulating all info that is needed at runtime.
/// It holds pointers to a few host and device buffers,
/// as well as params that are configurable at runtime.
/// It is used as a parameter to the Shaders host and device initializations
/// and run() APIs.
///
/// \subsection thread_safe Thread-safe Model
/// The Shader objects are stateless and can be shared by threads at runtime.
/// Operations can be shared for use in the run() APIs.
/// Host and device buffers contain data that can be updated at runtime,
/// so it is not thread-safe to share them among multiple threads.
///
/// For more details, please refer to this
/// <a href="https://confluence.lwpu.com/display/GCA/CASK+Conlwrrency">page</a>.
///
/// \subsection error Functional Verification and Error Handling
/// Operation classes implement isConsistent() API that verifies
/// the problem sizes described by TensorDesc instances.
/// Shader classes define canImplement() API that takes
/// the corresponding Operation object and verifies if
/// the underlying shader can support this operation.
/// CASK defines an enum type of Error to indicate any error that oclwrs
/// in a shader's functional verification, initialzation and run.
///
/// \section examples Code Examples
///
#endif // INCLUDE_GUARD_CASK_H
