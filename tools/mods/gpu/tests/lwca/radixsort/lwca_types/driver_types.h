/*
 * Copyright 2006-2013 LWPU Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to LWPU intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to LWPU and is being provided under the terms and
 * conditions of a form of LWPU software license agreement by and
 * between LWPU and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of LWPU is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, LWPU MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * LWPU DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL LWPU BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#if !defined(__DRIVER_TYPES_H__)
#define __DRIVER_TYPES_H__

#include "host_defines.h"

/**
 * \defgroup LWDART_TYPES Data types used by LWCA Runtime
 * \ingroup LWDART
 *
 * @{
 */

/*******************************************************************************
*                                                                              *
*  TYPE DEFINITIONS USED BY RUNTIME API                                        *
*                                                                              *
*******************************************************************************/

#if !defined(__LWDA_INTERNAL_COMPILATION__)

#include <limits.h>
#include <stddef.h>

#define lwdaHostAllocDefault           0x00  /**< Default page-locked allocation flag */
#define lwdaHostAllocPortable          0x01  /**< Pinned memory accessible by all LWCA contexts */
#define lwdaHostAllocMapped            0x02  /**< Map allocation into device space */
#define lwdaHostAllocWriteCombined     0x04  /**< Write-combined memory */

#define lwdaHostRegisterDefault        0x00  /**< Default host memory registration flag */
#define lwdaHostRegisterPortable       0x01  /**< Pinned memory accessible by all LWCA contexts */
#define lwdaHostRegisterMapped         0x02  /**< Map registered memory into device space */

#define lwdaPeerAccessDefault          0x00  /**< Default peer addressing enable flag */

#define lwdaStreamDefault              0x00  /**< Default stream flag */
#define lwdaStreamNonBlocking          0x01  /**< Stream does not synchronize with stream 0 (the NULL stream) */

#define lwdaEventDefault               0x00  /**< Default event flag */
#define lwdaEventBlockingSync          0x01  /**< Event uses blocking synchronization */
#define lwdaEventDisableTiming         0x02  /**< Event will not record timing data */
#define lwdaEventInterprocess          0x04  /**< Event is suitable for interprocess use. lwdaEventDisableTiming must be set */

#define lwdaDeviceScheduleAuto         0x00  /**< Device flag - Automatic scheduling */
#define lwdaDeviceScheduleSpin         0x01  /**< Device flag - Spin default scheduling */
#define lwdaDeviceScheduleYield        0x02  /**< Device flag - Yield default scheduling */
#define lwdaDeviceScheduleBlockingSync 0x04  /**< Device flag - Use blocking synchronization */
#define lwdaDeviceBlockingSync         0x04  /**< Device flag - Use blocking synchronization
                                               *  \deprecated This flag was deprecated as of LWCA 4.0 and
                                               *  replaced with ::lwdaDeviceScheduleBlockingSync. */
#define lwdaDeviceScheduleMask         0x07  /**< Device schedule flags mask */
#define lwdaDeviceMapHost              0x08  /**< Device flag - Support mapped pinned allocations */
#define lwdaDeviceLmemResizeToMax      0x10  /**< Device flag - Keep local memory allocation after launch */
#define lwdaDeviceMask                 0x1f  /**< Device flags mask */

#define lwdaArrayDefault               0x00   /**< Default LWCA array allocation flag */
#define lwdaArrayLayered               0x01   /**< Must be set in lwdaMalloc3DArray to create a layered LWCA array */
#define lwdaArraySurfaceLoadStore      0x02   /**< Must be set in lwdaMallocArray or lwdaMalloc3DArray in order to bind surfaces to the LWCA array */
#define lwdaArrayLwbemap               0x04   /**< Must be set in lwdaMalloc3DArray to create a lwbemap LWCA array */
#define lwdaArrayTextureGather         0x08   /**< Must be set in lwdaMallocArray or lwdaMalloc3DArray in order to perform texture gather operations on the LWCA array */

#define lwdaIpcMemLazyEnablePeerAccess 0x01   /**< Automatically enable peer access between remote devices as needed */

#define lwdaMemAttachGlobal            0x01   /**< Memory can be accessed by all streams on the device*/
#define lwdaMemAttachHost              0x02   /**< Memory cannot be accessed by any stream on the device */
#define lwdaMemAttachSingle            0x04   /**< Memory can only be accessed by a single stream on the device */

#endif /* !__LWDA_INTERNAL_COMPILATION__ */

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

/**
 * LWCA error types
 */
enum __device_builtin__ lwdaError
{
    /**
     * The API call returned with no errors. In the case of query calls, this
     * can also mean that the operation being queried is complete (see
     * ::lwdaEventQuery() and ::lwdaStreamQuery()).
     */
    lwdaSuccess                           =      0,

    /**
     * The device function being ilwoked (usually via ::lwdaLaunch()) was not
     * previously configured via the ::lwdaConfigureCall() function.
     */
    lwdaErrorMissingConfiguration         =      1,

    /**
     * The API call failed because it was unable to allocate enough memory to
     * perform the requested operation.
     */
    lwdaErrorMemoryAllocation             =      2,

    /**
     * The API call failed because the LWCA driver and runtime could not be
     * initialized.
     */
    lwdaErrorInitializationError          =      3,

    /**
     * An exception oclwrred on the device while exelwting a kernel. Common
     * causes include dereferencing an invalid device pointer and accessing
     * out of bounds shared memory. The device cannot be used until
     * ::lwdaThreadExit() is called. All existing device memory allocations
     * are invalid and must be reconstructed if the program is to continue
     * using LWCA.
     */
    lwdaErrorLaunchFailure                =      4,

    /**
     * This indicated that a previous kernel launch failed. This was previously
     * used for device emulation of kernel launches.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Device emulation mode was
     * removed with the LWCA 3.1 release.
     */
    lwdaErrorPriorLaunchFailure           =      5,

    /**
     * This indicates that the device kernel took too long to execute. This can
     * only occur if timeouts are enabled - see the device property
     * \ref ::lwdaDeviceProp::kernelExecTimeoutEnabled "kernelExecTimeoutEnabled"
     * for more information. The device cannot be used until ::lwdaThreadExit()
     * is called. All existing device memory allocations are invalid and must be
     * reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorLaunchTimeout                =      6,

    /**
     * This indicates that a launch did not occur because it did not have
     * appropriate resources. Although this error is similar to
     * ::lwdaErrorIlwalidConfiguration, this error usually indicates that the
     * user has attempted to pass too many arguments to the device kernel, or the
     * kernel launch specifies too many threads for the kernel's register count.
     */
    lwdaErrorLaunchOutOfResources         =      7,

    /**
     * The requested device function does not exist or is not compiled for the
     * proper device architecture.
     */
    lwdaErrorIlwalidDeviceFunction        =      8,

    /**
     * This indicates that a kernel launch is requesting resources that can
     * never be satisfied by the current device. Requesting more shared memory
     * per block than the device supports will trigger this error, as will
     * requesting too many threads or blocks. See ::lwdaDeviceProp for more
     * device limitations.
     */
    lwdaErrorIlwalidConfiguration         =      9,

    /**
     * This indicates that the device ordinal supplied by the user does not
     * correspond to a valid LWCA device.
     */
    lwdaErrorIlwalidDevice                =     10,

    /**
     * This indicates that one or more of the parameters passed to the API call
     * is not within an acceptable range of values.
     */
    lwdaErrorIlwalidValue                 =     11,

    /**
     * This indicates that one or more of the pitch-related parameters passed
     * to the API call is not within the acceptable range for pitch.
     */
    lwdaErrorIlwalidPitchValue            =     12,

    /**
     * This indicates that the symbol name/identifier passed to the API call
     * is not a valid name or identifier.
     */
    lwdaErrorIlwalidSymbol                =     13,

    /**
     * This indicates that the buffer object could not be mapped.
     */
    lwdaErrorMapBufferObjectFailed        =     14,

    /**
     * This indicates that the buffer object could not be unmapped.
     */
    lwdaErrorUnmapBufferObjectFailed      =     15,

    /**
     * This indicates that at least one host pointer passed to the API call is
     * not a valid host pointer.
     */
    lwdaErrorIlwalidHostPointer           =     16,

    /**
     * This indicates that at least one device pointer passed to the API call is
     * not a valid device pointer.
     */
    lwdaErrorIlwalidDevicePointer         =     17,

    /**
     * This indicates that the texture passed to the API call is not a valid
     * texture.
     */
    lwdaErrorIlwalidTexture               =     18,

    /**
     * This indicates that the texture binding is not valid. This oclwrs if you
     * call ::lwdaGetTextureAlignmentOffset() with an unbound texture.
     */
    lwdaErrorIlwalidTextureBinding        =     19,

    /**
     * This indicates that the channel descriptor passed to the API call is not
     * valid. This oclwrs if the format is not one of the formats specified by
     * ::lwdaChannelFormatKind, or if one of the dimensions is invalid.
     */
    lwdaErrorIlwalidChannelDescriptor     =     20,

    /**
     * This indicates that the direction of the memcpy passed to the API call is
     * not one of the types specified by ::lwdaMemcpyKind.
     */
    lwdaErrorIlwalidMemcpyDirection       =     21,

    /**
     * This indicated that the user has taken the address of a constant variable,
     * which was forbidden up until the LWCA 3.1 release.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Variables in constant
     * memory may now have their address taken by the runtime via
     * ::lwdaGetSymbolAddress().
     */
    lwdaErrorAddressOfConstant            =     22,

    /**
     * This indicated that a texture fetch was not able to be performed.
     * This was previously used for device emulation of texture operations.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Device emulation mode was
     * removed with the LWCA 3.1 release.
     */
    lwdaErrorTextureFetchFailed           =     23,

    /**
     * This indicated that a texture was not bound for access.
     * This was previously used for device emulation of texture operations.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Device emulation mode was
     * removed with the LWCA 3.1 release.
     */
    lwdaErrorTextureNotBound              =     24,

    /**
     * This indicated that a synchronization operation had failed.
     * This was previously used for some device emulation functions.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Device emulation mode was
     * removed with the LWCA 3.1 release.
     */
    lwdaErrorSynchronizationError         =     25,

    /**
     * This indicates that a non-float texture was being accessed with linear
     * filtering. This is not supported by LWCA.
     */
    lwdaErrorIlwalidFilterSetting         =     26,

    /**
     * This indicates that an attempt was made to read a non-float texture as a
     * normalized float. This is not supported by LWCA.
     */
    lwdaErrorIlwalidNormSetting           =     27,

    /**
     * Mixing of device and device emulation code was not allowed.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Device emulation mode was
     * removed with the LWCA 3.1 release.
     */
    lwdaErrorMixedDeviceExelwtion         =     28,

    /**
     * This indicates that a LWCA Runtime API call cannot be exelwted because
     * it is being called during process shut down, at a point in time after
     * LWCA driver has been unloaded.
     */
    lwdaErrorLwdartUnloading              =     29,

    /**
     * This indicates that an unknown internal error has oclwrred.
     */
    lwdaErrorUnknown                      =     30,

    /**
     * This indicates that the API call is not yet implemented. Production
     * releases of LWCA will never return this error.
     * \deprecated
     * This error return is deprecated as of LWCA 4.1.
     */
    lwdaErrorNotYetImplemented            =     31,

    /**
     * This indicated that an emulated device pointer exceeded the 32-bit address
     * range.
     * \deprecated
     * This error return is deprecated as of LWCA 3.1. Device emulation mode was
     * removed with the LWCA 3.1 release.
     */
    lwdaErrorMemoryValueTooLarge          =     32,

    /**
     * This indicates that a resource handle passed to the API call was not
     * valid. Resource handles are opaque types like ::lwdaStream_t and
     * ::lwdaEvent_t.
     */
    lwdaErrorIlwalidResourceHandle        =     33,

    /**
     * This indicates that asynchronous operations issued previously have not
     * completed yet. This result is not actually an error, but must be indicated
     * differently than ::lwdaSuccess (which indicates completion). Calls that
     * may return this value include ::lwdaEventQuery() and ::lwdaStreamQuery().
     */
    lwdaErrorNotReady                     =     34,

    /**
     * This indicates that the installed LWPU LWCA driver is older than the
     * LWCA runtime library. This is not a supported configuration. Users should
     * install an updated LWPU display driver to allow the application to run.
     */
    lwdaErrorInsufficientDriver           =     35,

    /**
     * This indicates that the user has called ::lwdaSetValidDevices(),
     * ::lwdaSetDeviceFlags(), ::lwdaD3D9SetDirect3DDevice(),
     * ::lwdaD3D10SetDirect3DDevice, ::lwdaD3D11SetDirect3DDevice(), or
     * ::lwdaVDPAUSetVDPAUDevice() after initializing the LWCA runtime by
     * calling non-device management operations (allocating memory and
     * launching kernels are examples of non-device management operations).
     * This error can also be returned if using runtime/driver
     * interoperability and there is an existing ::LWcontext active on the
     * host thread.
     */
    lwdaErrorSetOnActiveProcess           =     36,

    /**
     * This indicates that the surface passed to the API call is not a valid
     * surface.
     */
    lwdaErrorIlwalidSurface               =     37,

    /**
     * This indicates that no LWCA-capable devices were detected by the installed
     * LWCA driver.
     */
    lwdaErrorNoDevice                     =     38,

    /**
     * This indicates that an uncorrectable ECC error was detected during
     * exelwtion.
     */
    lwdaErrorECLWncorrectable             =     39,

    /**
     * This indicates that a link to a shared object failed to resolve.
     */
    lwdaErrorSharedObjectSymbolNotFound   =     40,

    /**
     * This indicates that initialization of a shared object failed.
     */
    lwdaErrorSharedObjectInitFailed       =     41,

    /**
     * This indicates that the ::lwdaLimit passed to the API call is not
     * supported by the active device.
     */
    lwdaErrorUnsupportedLimit             =     42,

    /**
     * This indicates that multiple global or constant variables (across separate
     * LWCA source files in the application) share the same string name.
     */
    lwdaErrorDuplicateVariableName        =     43,

    /**
     * This indicates that multiple textures (across separate LWCA source
     * files in the application) share the same string name.
     */
    lwdaErrorDuplicateTextureName         =     44,

    /**
     * This indicates that multiple surfaces (across separate LWCA source
     * files in the application) share the same string name.
     */
    lwdaErrorDuplicateSurfaceName         =     45,

    /**
     * This indicates that all LWCA devices are busy or unavailable at the current
     * time. Devices are often busy/unavailable due to use of
     * ::lwdaComputeModeExclusive, ::lwdaComputeModeProhibited or when long
     * running LWCA kernels have filled up the GPU and are blocking new work
     * from starting. They can also be unavailable due to memory constraints
     * on a device that already has active LWCA work being performed.
     */
    lwdaErrorDevicesUnavailable           =     46,

    /**
     * This indicates that the device kernel image is invalid.
     */
    lwdaErrorIlwalidKernelImage           =     47,

    /**
     * This indicates that there is no kernel image available that is suitable
     * for the device. This can occur when a user specifies code generation
     * options for a particular LWCA source file that do not include the
     * corresponding device configuration.
     */
    lwdaErrorNoKernelImageForDevice       =     48,

    /**
     * This indicates that the current context is not compatible with this
     * the LWCA Runtime. This can only occur if you are using LWCA
     * Runtime/Driver interoperability and have created an existing Driver
     * context using the driver API. The Driver context may be incompatible
     * either because the Driver context was created using an older version
     * of the API, because the Runtime API call expects a primary driver
     * context and the Driver context is not primary, or because the Driver
     * context has been destroyed. Please see \ref LWDART_DRIVER "Interactions
     * with the LWCA Driver API" for more information.
     */
    lwdaErrorIncompatibleDriverContext    =     49,

    /**
     * This error indicates that a call to ::lwdaDeviceEnablePeerAccess() is
     * trying to re-enable peer addressing on from a context which has already
     * had peer addressing enabled.
     */
    lwdaErrorPeerAccessAlreadyEnabled     =     50,

    /**
     * This error indicates that ::lwdaDeviceDisablePeerAccess() is trying to
     * disable peer addressing which has not been enabled yet via
     * ::lwdaDeviceEnablePeerAccess().
     */
    lwdaErrorPeerAccessNotEnabled         =     51,

    /**
     * This indicates that a call tried to access an exclusive-thread device that
     * is already in use by a different thread.
     */
    lwdaErrorDeviceAlreadyInUse           =     54,

    /**
     * This indicates profiler is not initialized for this run. This can
     * happen when the application is running with external profiling tools
     * like visual profiler.
     */
    lwdaErrorProfilerDisabled             =     55,

    /**
     * \deprecated
     * This error return is deprecated as of LWCA 5.0. It is no longer an error
     * to attempt to enable/disable the profiling via ::lwdaProfilerStart or
     * ::lwdaProfilerStop without initialization.
     */
    lwdaErrorProfilerNotInitialized       =     56,

    /**
     * \deprecated
     * This error return is deprecated as of LWCA 5.0. It is no longer an error
     * to call lwdaProfilerStart() when profiling is already enabled.
     */
    lwdaErrorProfilerAlreadyStarted       =     57,

    /**
     * \deprecated
     * This error return is deprecated as of LWCA 5.0. It is no longer an error
     * to call lwdaProfilerStop() when profiling is already disabled.
     */
     lwdaErrorProfilerAlreadyStopped       =    58,

    /**
     * An assert triggered in device code during kernel exelwtion. The device
     * cannot be used again until ::lwdaThreadExit() is called. All existing
     * allocations are invalid and must be reconstructed if the program is to
     * continue using LWCA.
     */
    lwdaErrorAssert                        =    59,

    /**
     * This error indicates that the hardware resources required to enable
     * peer access have been exhausted for one or more of the devices
     * passed to ::lwdaEnablePeerAccess().
     */
    lwdaErrorTooManyPeers                 =     60,

    /**
     * This error indicates that the memory range passed to ::lwdaHostRegister()
     * has already been registered.
     */
    lwdaErrorHostMemoryAlreadyRegistered  =     61,

    /**
     * This error indicates that the pointer passed to ::lwdaHostUnregister()
     * does not correspond to any lwrrently registered memory region.
     */
    lwdaErrorHostMemoryNotRegistered      =     62,

    /**
     * This error indicates that an OS call failed.
     */
    lwdaErrorOperatingSystem              =     63,

    /**
     * This error indicates that P2P access is not supported across the given
     * devices.
     */
    lwdaErrorPeerAccessUnsupported        =     64,

    /**
     * This error indicates that a device runtime grid launch did not occur
     * because the depth of the child grid would exceed the maximum supported
     * number of nested grid launches.
     */
    lwdaErrorLaunchMaxDepthExceeded       =     65,

    /**
     * This error indicates that a grid launch did not occur because the kernel
     * uses file-scoped textures which are unsupported by the device runtime.
     * Kernels launched via the device runtime only support textures created with
     * the Texture Object API's.
     */
    lwdaErrorLaunchFileScopedTex          =     66,

    /**
     * This error indicates that a grid launch did not occur because the kernel
     * uses file-scoped surfaces which are unsupported by the device runtime.
     * Kernels launched via the device runtime only support surfaces created with
     * the Surface Object API's.
     */
    lwdaErrorLaunchFileScopedSurf         =     67,

    /**
     * This error indicates that a call to ::lwdaDeviceSynchronize made from
     * the device runtime failed because the call was made at grid depth greater
     * than than either the default (2 levels of grids) or user specified device
     * limit ::lwdaLimitDevRuntimeSyncDepth. To be able to synchronize on
     * launched grids at a greater depth successfully, the maximum nested
     * depth at which ::lwdaDeviceSynchronize will be called must be specified
     * with the ::lwdaLimitDevRuntimeSyncDepth limit to the ::lwdaDeviceSetLimit
     * api before the host-side launch of a kernel using the device runtime.
     * Keep in mind that additional levels of sync depth require the runtime
     * to reserve large amounts of device memory that cannot be used for
     * user allocations.
     */
    lwdaErrorSyncDepthExceeded            =     68,

    /**
     * This error indicates that a device runtime grid launch failed because
     * the launch would exceed the limit ::lwdaLimitDevRuntimePendingLaunchCount.
     * For this launch to proceed successfully, ::lwdaDeviceSetLimit must be
     * called to set the ::lwdaLimitDevRuntimePendingLaunchCount to be higher
     * than the upper bound of outstanding launches that can be issued to the
     * device runtime. Keep in mind that raising the limit of pending device
     * runtime launches will require the runtime to reserve device memory that
     * cannot be used for user allocations.
     */
    lwdaErrorLaunchPendingCountExceeded   =     69,

    /**
     * This error indicates the attempted operation is not permitted.
     */
    lwdaErrorNotPermitted                 =     70,

    /**
     * This error indicates the attempted operation is not supported
     * on the current system or device.
     */
    lwdaErrorNotSupported                 =     71,

    /**
     * Device encountered an error in the call stack during kernel exelwtion,
     * possibly due to stack corruption or exceeding the stack size limit.
     * The context cannot be used, so it must be destroyed (and a new one should be created).
     * All existing device memory allocations from this context are invalid
     * and must be reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorHardwareStackError           =     72,

    /**
     * The device encountered an illegal instruction during kernel exelwtion
     * The context cannot be used, so it must be destroyed (and a new one should be created).
     * All existing device memory allocations from this context are invalid
     * and must be reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorIllegalInstruction           =     73,

    /**
     * The device encountered a load or store instruction
     * on a memory address which is not aligned.
     * The context cannot be used, so it must be destroyed (and a new one should be created).
     * All existing device memory allocations from this context are invalid
     * and must be reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorMisalignedAddress            =     74,

    /**
     * While exelwting a kernel, the device encountered an instruction
     * which can only operate on memory locations in certain address spaces
     * (global, shared, or local), but was supplied a memory address not
     * belonging to an allowed address space.
     * The context cannot be used, so it must be destroyed (and a new one should be created).
     * All existing device memory allocations from this context are invalid
     * and must be reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorIlwalidAddressSpace          =     75,

    /**
     * The device encountered an invalid program counter.
     * The context cannot be used, so it must be destroyed (and a new one should be created).
     * All existing device memory allocations from this context are invalid
     * and must be reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorIlwalidPc                    =     76,

    /**
     * The device encountered a load or store instruction on an invalid memory address.
     * The context cannot be used, so it must be destroyed (and a new one should be created).
     * All existing device memory allocations from this context are invalid
     * and must be reconstructed if the program is to continue using LWCA.
     */
    lwdaErrorIllegalAddress               =     77,

    /**
     * A PTX compilation failed. The runtime may fall back to compiling PTX if
     * an application does not contain a suitable binary for the current device.
     */
    lwdaErrorIlwalidPtx                   =     78,

    /**
     * This indicates an internal startup failure in the LWCA runtime.
     */
    lwdaErrorStartupFailure               =   0x7f,

    /**
     * Any unhandled LWCA driver error is added to this value and returned via
     * the runtime. Production releases of LWCA should not return such errors.
     * \deprecated
     * This error return is deprecated as of LWCA 4.1.
     */
    lwdaErrorApiFailureBase               =  10000
};

/**
 * Channel format kind
 */
enum __device_builtin__ lwdaChannelFormatKind
{
    lwdaChannelFormatKindSigned           =   0,      /**< Signed channel format */
    lwdaChannelFormatKindUnsigned         =   1,      /**< Unsigned channel format */
    lwdaChannelFormatKindFloat            =   2,      /**< Float channel format */
    lwdaChannelFormatKindNone             =   3       /**< No channel format */
};

/**
 * LWCA Channel format descriptor
 */
struct __device_builtin__ lwdaChannelFormatDesc
{
    int                        x; /**< x */
    int                        y; /**< y */
    int                        z; /**< z */
    int                        w; /**< w */
    enum lwdaChannelFormatKind f; /**< Channel format kind */
};

/**
 * LWCA array
 */
typedef struct lwdaArray *lwdaArray_t;

/**
 * LWCA array (as source copy argument)
 */
typedef const struct lwdaArray *lwdaArray_const_t;

struct lwdaArray;

/**
 * LWCA mipmapped array
 */
typedef struct lwdaMipmappedArray *lwdaMipmappedArray_t;

/**
 * LWCA mipmapped array (as source argument)
 */
typedef const struct lwdaMipmappedArray *lwdaMipmappedArray_const_t;

struct lwdaMipmappedArray;

/**
 * LWCA memory types
 */
enum __device_builtin__ lwdaMemoryType
{
    lwdaMemoryTypeHost   = 1, /**< Host memory */
    lwdaMemoryTypeDevice = 2  /**< Device memory */
};

/**
 * LWCA memory copy types
 */
enum __device_builtin__ lwdaMemcpyKind
{
    lwdaMemcpyHostToHost          =   0,      /**< Host   -> Host */
    lwdaMemcpyHostToDevice        =   1,      /**< Host   -> Device */
    lwdaMemcpyDeviceToHost        =   2,      /**< Device -> Host */
    lwdaMemcpyDeviceToDevice      =   3,      /**< Device -> Device */
    lwdaMemcpyDefault             =   4       /**< Default based unified virtual address space */
};

/**
 * LWCA Pitched memory pointer
 *
 * \sa ::make_lwdaPitchedPtr
 */
struct __device_builtin__ lwdaPitchedPtr
{
    void   *ptr;      /**< Pointer to allocated memory */
    size_t  pitch;    /**< Pitch of allocated memory in bytes */
    size_t  xsize;    /**< Logical width of allocation in elements */
    size_t  ysize;    /**< Logical height of allocation in elements */
};

/**
 * LWCA extent
 *
 * \sa ::make_lwdaExtent
 */
struct __device_builtin__ lwdaExtent
{
    size_t width;     /**< Width in elements when referring to array memory, in bytes when referring to linear memory */
    size_t height;    /**< Height in elements */
    size_t depth;     /**< Depth in elements */
};

/**
 * LWCA 3D position
 *
 * \sa ::make_lwdaPos
 */
struct __device_builtin__ lwdaPos
{
    size_t x;     /**< x */
    size_t y;     /**< y */
    size_t z;     /**< z */
};

/**
 * LWCA 3D memory copying parameters
 */
struct __device_builtin__ lwdaMemcpy3DParms
{
    lwdaArray_t            srcArray;  /**< Source memory address */
    struct lwdaPos         srcPos;    /**< Source position offset */
    struct lwdaPitchedPtr  srcPtr;    /**< Pitched source memory address */

    lwdaArray_t            dstArray;  /**< Destination memory address */
    struct lwdaPos         dstPos;    /**< Destination position offset */
    struct lwdaPitchedPtr  dstPtr;    /**< Pitched destination memory address */

    struct lwdaExtent      extent;    /**< Requested memory copy size */
    enum lwdaMemcpyKind    kind;      /**< Type of transfer */
};

/**
 * LWCA 3D cross-device memory copying parameters
 */
struct __device_builtin__ lwdaMemcpy3DPeerParms
{
    lwdaArray_t            srcArray;  /**< Source memory address */
    struct lwdaPos         srcPos;    /**< Source position offset */
    struct lwdaPitchedPtr  srcPtr;    /**< Pitched source memory address */
    int                    srcDevice; /**< Source device */

    lwdaArray_t            dstArray;  /**< Destination memory address */
    struct lwdaPos         dstPos;    /**< Destination position offset */
    struct lwdaPitchedPtr  dstPtr;    /**< Pitched destination memory address */
    int                    dstDevice; /**< Destination device */

    struct lwdaExtent      extent;    /**< Requested memory copy size */
};

/**
 * LWCA graphics interop resource
 */
struct lwdaGraphicsResource;

/**
 * LWCA graphics interop register flags
 */
enum __device_builtin__ lwdaGraphicsRegisterFlags
{
    lwdaGraphicsRegisterFlagsNone             = 0,  /**< Default */
    lwdaGraphicsRegisterFlagsReadOnly         = 1,  /**< LWCA will not write to this resource */
    lwdaGraphicsRegisterFlagsWriteDiscard     = 2,  /**< LWCA will only write to and will not read from this resource */
    lwdaGraphicsRegisterFlagsSurfaceLoadStore = 4,  /**< LWCA will bind this resource to a surface reference */
    lwdaGraphicsRegisterFlagsTextureGather    = 8   /**< LWCA will perform texture gather operations on this resource */
};

/**
 * LWCA graphics interop map flags
 */
enum __device_builtin__ lwdaGraphicsMapFlags
{
    lwdaGraphicsMapFlagsNone         = 0,  /**< Default; Assume resource can be read/written */
    lwdaGraphicsMapFlagsReadOnly     = 1,  /**< LWCA will not write to this resource */
    lwdaGraphicsMapFlagsWriteDiscard = 2   /**< LWCA will only write to and will not read from this resource */
};

/**
 * LWCA graphics interop array indices for lwbe maps
 */
enum __device_builtin__ lwdaGraphicsLwbeFace
{
    lwdaGraphicsLwbeFacePositiveX = 0x00, /**< Positive X face of lwbemap */
    lwdaGraphicsLwbeFaceNegativeX = 0x01, /**< Negative X face of lwbemap */
    lwdaGraphicsLwbeFacePositiveY = 0x02, /**< Positive Y face of lwbemap */
    lwdaGraphicsLwbeFaceNegativeY = 0x03, /**< Negative Y face of lwbemap */
    lwdaGraphicsLwbeFacePositiveZ = 0x04, /**< Positive Z face of lwbemap */
    lwdaGraphicsLwbeFaceNegativeZ = 0x05  /**< Negative Z face of lwbemap */
};

/**
 * LWCA resource types
 */
enum __device_builtin__ lwdaResourceType
{
    lwdaResourceTypeArray          = 0x00, /**< Array resource */
    lwdaResourceTypeMipmappedArray = 0x01, /**< Mipmapped array resource */
    lwdaResourceTypeLinear         = 0x02, /**< Linear resource */
    lwdaResourceTypePitch2D        = 0x03  /**< Pitch 2D resource */
};

/**
 * LWCA texture resource view formats
 */
enum __device_builtin__ lwdaResourceViewFormat
{
    lwdaResViewFormatNone                      = 0x00, /**< No resource view format (use underlying resource format) */
    lwdaResViewFormatUnsignedChar1             = 0x01, /**< 1 channel unsigned 8-bit integers */
    lwdaResViewFormatUnsignedChar2             = 0x02, /**< 2 channel unsigned 8-bit integers */
    lwdaResViewFormatUnsignedChar4             = 0x03, /**< 4 channel unsigned 8-bit integers */
    lwdaResViewFormatSignedChar1               = 0x04, /**< 1 channel signed 8-bit integers */
    lwdaResViewFormatSignedChar2               = 0x05, /**< 2 channel signed 8-bit integers */
    lwdaResViewFormatSignedChar4               = 0x06, /**< 4 channel signed 8-bit integers */
    lwdaResViewFormatUnsignedShort1            = 0x07, /**< 1 channel unsigned 16-bit integers */
    lwdaResViewFormatUnsignedShort2            = 0x08, /**< 2 channel unsigned 16-bit integers */
    lwdaResViewFormatUnsignedShort4            = 0x09, /**< 4 channel unsigned 16-bit integers */
    lwdaResViewFormatSignedShort1              = 0x0a, /**< 1 channel signed 16-bit integers */
    lwdaResViewFormatSignedShort2              = 0x0b, /**< 2 channel signed 16-bit integers */
    lwdaResViewFormatSignedShort4              = 0x0c, /**< 4 channel signed 16-bit integers */
    lwdaResViewFormatUnsignedInt1              = 0x0d, /**< 1 channel unsigned 32-bit integers */
    lwdaResViewFormatUnsignedInt2              = 0x0e, /**< 2 channel unsigned 32-bit integers */
    lwdaResViewFormatUnsignedInt4              = 0x0f, /**< 4 channel unsigned 32-bit integers */
    lwdaResViewFormatSignedInt1                = 0x10, /**< 1 channel signed 32-bit integers */
    lwdaResViewFormatSignedInt2                = 0x11, /**< 2 channel signed 32-bit integers */
    lwdaResViewFormatSignedInt4                = 0x12, /**< 4 channel signed 32-bit integers */
    lwdaResViewFormatHalf1                     = 0x13, /**< 1 channel 16-bit floating point */
    lwdaResViewFormatHalf2                     = 0x14, /**< 2 channel 16-bit floating point */
    lwdaResViewFormatHalf4                     = 0x15, /**< 4 channel 16-bit floating point */
    lwdaResViewFormatFloat1                    = 0x16, /**< 1 channel 32-bit floating point */
    lwdaResViewFormatFloat2                    = 0x17, /**< 2 channel 32-bit floating point */
    lwdaResViewFormatFloat4                    = 0x18, /**< 4 channel 32-bit floating point */
    lwdaResViewFormatUnsignedBlockCompressed1  = 0x19, /**< Block compressed 1 */
    lwdaResViewFormatUnsignedBlockCompressed2  = 0x1a, /**< Block compressed 2 */
    lwdaResViewFormatUnsignedBlockCompressed3  = 0x1b, /**< Block compressed 3 */
    lwdaResViewFormatUnsignedBlockCompressed4  = 0x1c, /**< Block compressed 4 unsigned */
    lwdaResViewFormatSignedBlockCompressed4    = 0x1d, /**< Block compressed 4 signed */
    lwdaResViewFormatUnsignedBlockCompressed5  = 0x1e, /**< Block compressed 5 unsigned */
    lwdaResViewFormatSignedBlockCompressed5    = 0x1f, /**< Block compressed 5 signed */
    lwdaResViewFormatUnsignedBlockCompressed6H = 0x20, /**< Block compressed 6 unsigned half-float */
    lwdaResViewFormatSignedBlockCompressed6H   = 0x21, /**< Block compressed 6 signed half-float */
    lwdaResViewFormatUnsignedBlockCompressed7  = 0x22  /**< Block compressed 7 */
};

/**
 * LWCA resource descriptor
 */
struct __device_builtin__ lwdaResourceDesc {
    enum lwdaResourceType resType;             /**< Resource type */

    union {
        struct {
            lwdaArray_t array;                 /**< LWCA array */
        } array;
        struct {
            lwdaMipmappedArray_t mipmap;       /**< LWCA mipmapped array */
        } mipmap;
        struct {
            void *devPtr;                      /**< Device pointer */
            struct lwdaChannelFormatDesc desc; /**< Channel descriptor */
            size_t sizeInBytes;                /**< Size in bytes */
        } linear;
        struct {
            void *devPtr;                      /**< Device pointer */
            struct lwdaChannelFormatDesc desc; /**< Channel descriptor */
            size_t width;                      /**< Width of the array in elements */
            size_t height;                     /**< Height of the array in elements */
            size_t pitchInBytes;               /**< Pitch between two rows in bytes */
        } pitch2D;
    } res;
};

/**
 * LWCA resource view descriptor
 */
struct __device_builtin__ lwdaResourceViewDesc
{
    enum lwdaResourceViewFormat format;           /**< Resource view format */
    size_t                      width;            /**< Width of the resource view */
    size_t                      height;           /**< Height of the resource view */
    size_t                      depth;            /**< Depth of the resource view */
    unsigned int                firstMipmapLevel; /**< First defined mipmap level */
    unsigned int                lastMipmapLevel;  /**< Last defined mipmap level */
    unsigned int                firstLayer;       /**< First layer index */
    unsigned int                lastLayer;        /**< Last layer index */
};

/**
 * LWCA pointer attributes
 */
struct __device_builtin__ lwdaPointerAttributes
{
    /**
     * The physical location of the memory, ::lwdaMemoryTypeHost or
     * ::lwdaMemoryTypeDevice.
     */
    enum lwdaMemoryType memoryType;

    /**
     * The device against which the memory was allocated or registered.
     * If the memory type is ::lwdaMemoryTypeDevice then this identifies
     * the device on which the memory referred physically resides.  If
     * the memory type is ::lwdaMemoryTypeHost then this identifies the
     * device which was current when the memory was allocated or registered
     * (and if that device is deinitialized then this allocation will vanish
     * with that device's state).
     */
    int device;

    /**
     * The address which may be dereferenced on the current device to access
     * the memory or NULL if no such address exists.
     */
    void *devicePointer;

    /**
     * The address which may be dereferenced on the host to access the
     * memory or NULL if no such address exists.
     */
    void *hostPointer;
};

/**
 * LWCA function attributes
 */
struct __device_builtin__ lwdaFuncAttributes
{
   /**
    * The size in bytes of statically-allocated shared memory per block
    * required by this function. This does not include dynamically-allocated
    * shared memory requested by the user at runtime.
    */
   size_t sharedSizeBytes;

   /**
    * The size in bytes of user-allocated constant memory required by this
    * function.
    */
   size_t constSizeBytes;

   /**
    * The size in bytes of local memory used by each thread of this function.
    */
   size_t localSizeBytes;

   /**
    * The maximum number of threads per block, beyond which a launch of the
    * function would fail. This number depends on both the function and the
    * device on which the function is lwrrently loaded.
    */
   int maxThreadsPerBlock;

   /**
    * The number of registers used by each thread of this function.
    */
   int numRegs;

   /**
    * The PTX virtual architecture version for which the function was
    * compiled. This value is the major PTX version * 10 + the minor PTX
    * version, so a PTX version 1.3 function would return the value 13.
    */
   int ptxVersion;

   /**
    * The binary architecture version for which the function was compiled.
    * This value is the major binary version * 10 + the minor binary version,
    * so a binary version 1.3 function would return the value 13.
    */
   int binaryVersion;

   /**
    * The attribute to indicate whether the function has been compiled with
    * user specified option "-Xptxas --dlcm=ca" set.
    */
   int cacheModeCA;
};

/**
 * LWCA function cache configurations
 */
enum __device_builtin__ lwdaFuncCache
{
    lwdaFuncCachePreferNone   = 0,    /**< Default function cache configuration, no preference */
    lwdaFuncCachePreferShared = 1,    /**< Prefer larger shared memory and smaller L1 cache  */
    lwdaFuncCachePreferL1     = 2,    /**< Prefer larger L1 cache and smaller shared memory */
    lwdaFuncCachePreferEqual  = 3     /**< Prefer equal size L1 cache and shared memory */
};

/**
 * LWCA shared memory configuration
 */

enum __device_builtin__ lwdaSharedMemConfig
{
    lwdaSharedMemBankSizeDefault   = 0,
    lwdaSharedMemBankSizeFourByte  = 1,
    lwdaSharedMemBankSizeEightByte = 2
};

/**
 * LWCA device compute modes
 */
enum __device_builtin__ lwdaComputeMode
{
    lwdaComputeModeDefault          = 0,  /**< Default compute mode (Multiple threads can use ::lwdaSetDevice() with this device) */
    lwdaComputeModeExclusive        = 1,  /**< Compute-exclusive-thread mode (Only one thread in one process will be able to use ::lwdaSetDevice() with this device) */
    lwdaComputeModeProhibited       = 2,  /**< Compute-prohibited mode (No threads can use ::lwdaSetDevice() with this device) */
    lwdaComputeModeExclusiveProcess = 3   /**< Compute-exclusive-process mode (Many threads in one process will be able to use ::lwdaSetDevice() with this device) */
};

/**
 * LWCA Limits
 */
enum __device_builtin__ lwdaLimit
{
    lwdaLimitStackSize                    = 0x00, /**< GPU thread stack size */
    lwdaLimitPrintfFifoSize               = 0x01, /**< GPU printf/fprintf FIFO size */
    lwdaLimitMallocHeapSize               = 0x02, /**< GPU malloc heap size */
    lwdaLimitDevRuntimeSyncDepth          = 0x03, /**< GPU device runtime synchronize depth */
    lwdaLimitDevRuntimePendingLaunchCount = 0x04  /**< GPU device runtime pending launch count */
};

/**
 * LWCA Profiler Output modes
 */
enum __device_builtin__ lwdaOutputMode
{
    lwdaKeyValuePair    = 0x00, /**< Output mode Key-Value pair format. */
    lwdaCSV             = 0x01  /**< Output mode Comma separated values format. */
};

/**
 * LWCA device attributes
 */
enum __device_builtin__ lwdaDeviceAttr
{
    lwdaDevAttrMaxThreadsPerBlock             = 1,  /**< Maximum number of threads per block */
    lwdaDevAttrMaxBlockDimX                   = 2,  /**< Maximum block dimension X */
    lwdaDevAttrMaxBlockDimY                   = 3,  /**< Maximum block dimension Y */
    lwdaDevAttrMaxBlockDimZ                   = 4,  /**< Maximum block dimension Z */
    lwdaDevAttrMaxGridDimX                    = 5,  /**< Maximum grid dimension X */
    lwdaDevAttrMaxGridDimY                    = 6,  /**< Maximum grid dimension Y */
    lwdaDevAttrMaxGridDimZ                    = 7,  /**< Maximum grid dimension Z */
    lwdaDevAttrMaxSharedMemoryPerBlock        = 8,  /**< Maximum shared memory available per block in bytes */
    lwdaDevAttrTotalConstantMemory            = 9,  /**< Memory available on device for __constant__ variables in a LWCA C kernel in bytes */
    lwdaDevAttrWarpSize                       = 10, /**< Warp size in threads */
    lwdaDevAttrMaxPitch                       = 11, /**< Maximum pitch in bytes allowed by memory copies */
    lwdaDevAttrMaxRegistersPerBlock           = 12, /**< Maximum number of 32-bit registers available per block */
    lwdaDevAttrClockRate                      = 13, /**< Peak clock frequency in kilohertz */
    lwdaDevAttrTextureAlignment               = 14, /**< Alignment requirement for textures */
    lwdaDevAttrGpuOverlap                     = 15, /**< Device can possibly copy memory and execute a kernel conlwrrently */
    lwdaDevAttrMultiProcessorCount            = 16, /**< Number of multiprocessors on device */
    lwdaDevAttrKernelExecTimeout              = 17, /**< Specifies whether there is a run time limit on kernels */
    lwdaDevAttrIntegrated                     = 18, /**< Device is integrated with host memory */
    lwdaDevAttrCanMapHostMemory               = 19, /**< Device can map host memory into LWCA address space */
    lwdaDevAttrComputeMode                    = 20, /**< Compute mode (See ::lwdaComputeMode for details) */
    lwdaDevAttrMaxTexture1DWidth              = 21, /**< Maximum 1D texture width */
    lwdaDevAttrMaxTexture2DWidth              = 22, /**< Maximum 2D texture width */
    lwdaDevAttrMaxTexture2DHeight             = 23, /**< Maximum 2D texture height */
    lwdaDevAttrMaxTexture3DWidth              = 24, /**< Maximum 3D texture width */
    lwdaDevAttrMaxTexture3DHeight             = 25, /**< Maximum 3D texture height */
    lwdaDevAttrMaxTexture3DDepth              = 26, /**< Maximum 3D texture depth */
    lwdaDevAttrMaxTexture2DLayeredWidth       = 27, /**< Maximum 2D layered texture width */
    lwdaDevAttrMaxTexture2DLayeredHeight      = 28, /**< Maximum 2D layered texture height */
    lwdaDevAttrMaxTexture2DLayeredLayers      = 29, /**< Maximum layers in a 2D layered texture */
    lwdaDevAttrSurfaceAlignment               = 30, /**< Alignment requirement for surfaces */
    lwdaDevAttrConlwrrentKernels              = 31, /**< Device can possibly execute multiple kernels conlwrrently */
    lwdaDevAttrEccEnabled                     = 32, /**< Device has ECC support enabled */
    lwdaDevAttrPciBusId                       = 33, /**< PCI bus ID of the device */
    lwdaDevAttrPciDeviceId                    = 34, /**< PCI device ID of the device */
    lwdaDevAttrTccDriver                      = 35, /**< Device is using TCC driver model */
    lwdaDevAttrMemoryClockRate                = 36, /**< Peak memory clock frequency in kilohertz */
    lwdaDevAttrGlobalMemoryBusWidth           = 37, /**< Global memory bus width in bits */
    lwdaDevAttrL2CacheSize                    = 38, /**< Size of L2 cache in bytes */
    lwdaDevAttrMaxThreadsPerMultiProcessor    = 39, /**< Maximum resident threads per multiprocessor */
    lwdaDevAttrAsyncEngineCount               = 40, /**< Number of asynchronous engines */
    lwdaDevAttrUnifiedAddressing              = 41, /**< Device shares a unified address space with the host */
    lwdaDevAttrMaxTexture1DLayeredWidth       = 42, /**< Maximum 1D layered texture width */
    lwdaDevAttrMaxTexture1DLayeredLayers      = 43, /**< Maximum layers in a 1D layered texture */
    lwdaDevAttrMaxTexture2DGatherWidth        = 45, /**< Maximum 2D texture width if lwdaArrayTextureGather is set */
    lwdaDevAttrMaxTexture2DGatherHeight       = 46, /**< Maximum 2D texture height if lwdaArrayTextureGather is set */
    lwdaDevAttrMaxTexture3DWidthAlt           = 47, /**< Alternate maximum 3D texture width */
    lwdaDevAttrMaxTexture3DHeightAlt          = 48, /**< Alternate maximum 3D texture height */
    lwdaDevAttrMaxTexture3DDepthAlt           = 49, /**< Alternate maximum 3D texture depth */
    lwdaDevAttrPciDomainId                    = 50, /**< PCI domain ID of the device */
    lwdaDevAttrTexturePitchAlignment          = 51, /**< Pitch alignment requirement for textures */
    lwdaDevAttrMaxTextureLwbemapWidth         = 52, /**< Maximum lwbemap texture width/height */
    lwdaDevAttrMaxTextureLwbemapLayeredWidth  = 53, /**< Maximum lwbemap layered texture width/height */
    lwdaDevAttrMaxTextureLwbemapLayeredLayers = 54, /**< Maximum layers in a lwbemap layered texture */
    lwdaDevAttrMaxSurface1DWidth              = 55, /**< Maximum 1D surface width */
    lwdaDevAttrMaxSurface2DWidth              = 56, /**< Maximum 2D surface width */
    lwdaDevAttrMaxSurface2DHeight             = 57, /**< Maximum 2D surface height */
    lwdaDevAttrMaxSurface3DWidth              = 58, /**< Maximum 3D surface width */
    lwdaDevAttrMaxSurface3DHeight             = 59, /**< Maximum 3D surface height */
    lwdaDevAttrMaxSurface3DDepth              = 60, /**< Maximum 3D surface depth */
    lwdaDevAttrMaxSurface1DLayeredWidth       = 61, /**< Maximum 1D layered surface width */
    lwdaDevAttrMaxSurface1DLayeredLayers      = 62, /**< Maximum layers in a 1D layered surface */
    lwdaDevAttrMaxSurface2DLayeredWidth       = 63, /**< Maximum 2D layered surface width */
    lwdaDevAttrMaxSurface2DLayeredHeight      = 64, /**< Maximum 2D layered surface height */
    lwdaDevAttrMaxSurface2DLayeredLayers      = 65, /**< Maximum layers in a 2D layered surface */
    lwdaDevAttrMaxSurfaceLwbemapWidth         = 66, /**< Maximum lwbemap surface width */
    lwdaDevAttrMaxSurfaceLwbemapLayeredWidth  = 67, /**< Maximum lwbemap layered surface width */
    lwdaDevAttrMaxSurfaceLwbemapLayeredLayers = 68, /**< Maximum layers in a lwbemap layered surface */
    lwdaDevAttrMaxTexture1DLinearWidth        = 69, /**< Maximum 1D linear texture width */
    lwdaDevAttrMaxTexture2DLinearWidth        = 70, /**< Maximum 2D linear texture width */
    lwdaDevAttrMaxTexture2DLinearHeight       = 71, /**< Maximum 2D linear texture height */
    lwdaDevAttrMaxTexture2DLinearPitch        = 72, /**< Maximum 2D linear texture pitch in bytes */
    lwdaDevAttrMaxTexture2DMipmappedWidth     = 73, /**< Maximum mipmapped 2D texture width */
    lwdaDevAttrMaxTexture2DMipmappedHeight    = 74, /**< Maximum mipmapped 2D texture height */
    lwdaDevAttrComputeCapabilityMajor         = 75, /**< Major compute capability version number */
    lwdaDevAttrComputeCapabilityMinor         = 76, /**< Minor compute capability version number */
    lwdaDevAttrMaxTexture1DMipmappedWidth     = 77, /**< Maximum mipmapped 1D texture width */
    lwdaDevAttrStreamPrioritiesSupported      = 78, /**< Device supports stream priorities */
    lwdaDevAttrGlobalL1CacheSupported         = 79, /**< Device supports caching globals in L1 */
    lwdaDevAttrLocalL1CacheSupported          = 80, /**< Device supports caching locals in L1 */
    lwdaDevAttrMaxSharedMemoryPerMultiprocessor = 81, /**< Maximum shared memory available per multiprocessor in bytes */
    lwdaDevAttrMaxRegistersPerMultiprocessor  = 82, /**< Maximum number of 32-bit registers available per multiprocessor */
    lwdaDevAttrManagedMemory                  = 83  /**< Device can allocate managed memory on this system */
};

/**
 * LWCA device properties
 */
struct __device_builtin__ lwdaDeviceProp
{
    char   name[256];                  /**< ASCII string identifying device */
    size_t totalGlobalMem;             /**< Global memory available on device in bytes */
    size_t sharedMemPerBlock;          /**< Shared memory available per block in bytes */
    int    regsPerBlock;               /**< 32-bit registers available per block */
    int    warpSize;                   /**< Warp size in threads */
    size_t memPitch;                   /**< Maximum pitch in bytes allowed by memory copies */
    int    maxThreadsPerBlock;         /**< Maximum number of threads per block */
    int    maxThreadsDim[3];           /**< Maximum size of each dimension of a block */
    int    maxGridSize[3];             /**< Maximum size of each dimension of a grid */
    int    clockRate;                  /**< Clock frequency in kilohertz */
    size_t totalConstMem;              /**< Constant memory available on device in bytes */
    int    major;                      /**< Major compute capability */
    int    minor;                      /**< Minor compute capability */
    size_t textureAlignment;           /**< Alignment requirement for textures */
    size_t texturePitchAlignment;      /**< Pitch alignment requirement for texture references bound to pitched memory */
    int    deviceOverlap;              /**< Device can conlwrrently copy memory and execute a kernel. Deprecated. Use instead asyncEngineCount. */
    int    multiProcessorCount;        /**< Number of multiprocessors on device */
    int    kernelExecTimeoutEnabled;   /**< Specified whether there is a run time limit on kernels */
    int    integrated;                 /**< Device is integrated as opposed to discrete */
    int    canMapHostMemory;           /**< Device can map host memory with lwdaHostAlloc/lwdaHostGetDevicePointer */
    int    computeMode;                /**< Compute mode (See ::lwdaComputeMode) */
    int    maxTexture1D;               /**< Maximum 1D texture size */
    int    maxTexture1DMipmap;         /**< Maximum 1D mipmapped texture size */
    int    maxTexture1DLinear;         /**< Maximum size for 1D textures bound to linear memory */
    int    maxTexture2D[2];            /**< Maximum 2D texture dimensions */
    int    maxTexture2DMipmap[2];      /**< Maximum 2D mipmapped texture dimensions */
    int    maxTexture2DLinear[3];      /**< Maximum dimensions (width, height, pitch) for 2D textures bound to pitched memory */
    int    maxTexture2DGather[2];      /**< Maximum 2D texture dimensions if texture gather operations have to be performed */
    int    maxTexture3D[3];            /**< Maximum 3D texture dimensions */
    int    maxTexture3DAlt[3];         /**< Maximum alternate 3D texture dimensions */
    int    maxTextureLwbemap;          /**< Maximum Lwbemap texture dimensions */
    int    maxTexture1DLayered[2];     /**< Maximum 1D layered texture dimensions */
    int    maxTexture2DLayered[3];     /**< Maximum 2D layered texture dimensions */
    int    maxTextureLwbemapLayered[2];/**< Maximum Lwbemap layered texture dimensions */
    int    maxSurface1D;               /**< Maximum 1D surface size */
    int    maxSurface2D[2];            /**< Maximum 2D surface dimensions */
    int    maxSurface3D[3];            /**< Maximum 3D surface dimensions */
    int    maxSurface1DLayered[2];     /**< Maximum 1D layered surface dimensions */
    int    maxSurface2DLayered[3];     /**< Maximum 2D layered surface dimensions */
    int    maxSurfaceLwbemap;          /**< Maximum Lwbemap surface dimensions */
    int    maxSurfaceLwbemapLayered[2];/**< Maximum Lwbemap layered surface dimensions */
    size_t surfaceAlignment;           /**< Alignment requirements for surfaces */
    int    conlwrrentKernels;          /**< Device can possibly execute multiple kernels conlwrrently */
    int    ECCEnabled;                 /**< Device has ECC support enabled */
    int    pciBusID;                   /**< PCI bus ID of the device */
    int    pciDeviceID;                /**< PCI device ID of the device */
    int    pciDomainID;                /**< PCI domain ID of the device */
    int    tccDriver;                  /**< 1 if device is a Tesla device using TCC driver, 0 otherwise */
    int    asyncEngineCount;           /**< Number of asynchronous engines */
    int    unifiedAddressing;          /**< Device shares a unified address space with the host */
    int    memoryClockRate;            /**< Peak memory clock frequency in kilohertz */
    int    memoryBusWidth;             /**< Global memory bus width in bits */
    int    l2CacheSize;                /**< Size of L2 cache in bytes */
    int    maxThreadsPerMultiProcessor;/**< Maximum resident threads per multiprocessor */
    int    streamPrioritiesSupported;  /**< Device supports stream priorities */
    int    globalL1CacheSupported;     /**< Device supports caching globals in L1 */
    int    localL1CacheSupported;      /**< Device supports caching locals in L1 */
    size_t sharedMemPerMultiprocessor; /**< Shared memory available per multiprocessor in bytes */
    int    regsPerMultiprocessor;      /**< 32-bit registers available per multiprocessor */
    int    managedMemory;              /**< Device supports allocating managed memory on this system */
};

#define lwdaDevicePropDontCare                             \
        {                                                  \
          {'\0'},    /* char   name[256];               */ \
          0,         /* size_t totalGlobalMem;          */ \
          0,         /* size_t sharedMemPerBlock;       */ \
          0,         /* int    regsPerBlock;            */ \
          0,         /* int    warpSize;                */ \
          0,         /* size_t memPitch;                */ \
          0,         /* int    maxThreadsPerBlock;      */ \
          {0, 0, 0}, /* int    maxThreadsDim[3];        */ \
          {0, 0, 0}, /* int    maxGridSize[3];          */ \
          0,         /* int    clockRate;               */ \
          0,         /* size_t totalConstMem;           */ \
          -1,        /* int    major;                   */ \
          -1,        /* int    minor;                   */ \
          0,         /* size_t textureAlignment;        */ \
          0,         /* size_t texturePitchAlignment    */ \
          -1,        /* int    deviceOverlap;           */ \
          0,         /* int    multiProcessorCount;     */ \
          0,         /* int    kernelExecTimeoutEnabled */ \
          0,         /* int    integrated               */ \
          0,         /* int    canMapHostMemory         */ \
          0,         /* int    computeMode              */ \
          0,         /* int    maxTexture1D             */ \
          0,         /* int    maxTexture1DMipmap       */ \
          0,         /* int    maxTexture1DLinear       */ \
          {0, 0},    /* int    maxTexture2D[2]          */ \
          {0, 0},    /* int    maxTexture2DMipmap[2]    */ \
          {0, 0, 0}, /* int    maxTexture2DLinear[3]    */ \
          {0, 0},    /* int    maxTexture2DGather[2]    */ \
          {0, 0, 0}, /* int    maxTexture3D[3]          */ \
          {0, 0, 0}, /* int    maxTexture3DAlt[3]       */ \
          0,         /* int    maxTextureLwbemap        */ \
          {0, 0},    /* int    maxTexture1DLayered[2]   */ \
          {0, 0, 0}, /* int    maxTexture2DLayered[3]   */ \
          {0, 0},    /* int    maxTextureLwbemapLayered[2] */ \
          0,         /* int    maxSurface1D             */ \
          {0, 0},    /* int    maxSurface2D[2]          */ \
          {0, 0, 0}, /* int    maxSurface3D[3]          */ \
          {0, 0},    /* int    maxSurface1DLayered[2]   */ \
          {0, 0, 0}, /* int    maxSurface2DLayered[3]   */ \
          0,         /* int    maxSurfaceLwbemap        */ \
          {0, 0},    /* int    maxSurfaceLwbemapLayered[2] */ \
          0,         /* size_t surfaceAlignment         */ \
          0,         /* int    conlwrrentKernels        */ \
          0,         /* int    ECCEnabled               */ \
          0,         /* int    pciBusID                 */ \
          0,         /* int    pciDeviceID              */ \
          0,         /* int    pciDomainID              */ \
          0,         /* int    tccDriver                */ \
          0,         /* int    asyncEngineCount         */ \
          0,         /* int    unifiedAddressing        */ \
          0,         /* int    memoryClockRate          */ \
          0,         /* int    memoryBusWidth           */ \
          0,         /* int    l2CacheSize              */ \
          0,         /* int    maxThreadsPerMultiProcessor */ \
          0,         /* int    streamPrioritiesSupported */ \
          0,         /* int    globalL1CacheSupported   */ \
          0,         /* int    localL1CacheSupported    */ \
          0,         /* size_t sharedMemPerMultiprocessor; */ \
          0,         /* int    regsPerMultiprocessor;   */ \
          0,         /* int    managedMemory            */ \
        } /**< Empty device properties */

/**
 * LWCA IPC Handle Size
 */
#define LWDA_IPC_HANDLE_SIZE 64

/**
 * LWCA IPC event handle
 */
typedef __device_builtin__ struct __device_builtin__ lwdaIpcEventHandle_st
{
    char reserved[LWDA_IPC_HANDLE_SIZE];
}lwdaIpcEventHandle_t;

/**
 * LWCA IPC memory handle
 */
typedef __device_builtin__ struct __device_builtin__ lwdaIpcMemHandle_st
{
    char reserved[LWDA_IPC_HANDLE_SIZE];
}lwdaIpcMemHandle_t;

/*******************************************************************************
*                                                                              *
*  SHORTHAND TYPE DEFINITION USED BY RUNTIME API                               *
*                                                                              *
*******************************************************************************/

/**
 * LWCA Error types
 */
typedef __device_builtin__ enum lwdaError lwdaError_t;

/**
 * LWCA stream
 */
typedef __device_builtin__ struct LWstream_st *lwdaStream_t;

/**
 * LWCA event types
 */
typedef __device_builtin__ struct LWevent_st *lwdaEvent_t;

/**
 * LWCA graphics resource types
 */
typedef __device_builtin__ struct lwdaGraphicsResource *lwdaGraphicsResource_t;

/**
 * LWCA UUID types
 */
typedef __device_builtin__ struct LWuuid_st lwdaUUID_t;

/**
 * LWCA output file modes
 */
typedef __device_builtin__ enum lwdaOutputMode lwdaOutputMode_t;

/** @} */
/** @} */ /* END LWDART_TYPES */

#endif /* !__DRIVER_TYPES_H__ */
