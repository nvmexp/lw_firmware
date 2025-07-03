/*
 * Copyright 1993-2012 LWPU Corporation.  All rights reserved.
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

#if !defined(__LWDA_D3D10_INTEROP_H__)
#define __LWDA_D3D10_INTEROP_H__

#include "lwda_runtime_api.h"

/** \cond impl_private */
#if !defined(__dv)

#if defined(__cplusplus)

#define __dv(v) \
        = v

#else /* __cplusplus */

#define __dv(v)

#endif /* __cplusplus */

#endif /* !__dv */
/** \endcond impl_private */

#include <d3d10_1.h>

/** \cond impl_private */
#if defined(__DOXYGEN_ONLY__) || defined(LWDA_ENABLE_DEPRECATED)
#define __LWDA_DEPRECATED
#elif defined(_MSC_VER)
#define __LWDA_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__)
#define __LWDA_DEPRECATED __attribute__((deprecated))
#else
#define __LWDA_DEPRECATED
#endif
/** \endcond impl_private */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * \addtogroup LWDART_D3D10 Direct3D 10 Interoperability
 * This section describes the Direct3D 10 interoperability functions of the LWCA
 * runtime application programming interface. Note that mapping of Direct3D 10
 * resources is performed with the graphics API agnostic, resource mapping 
 * interface described in \ref LWDART_INTEROP "Graphics Interopability".
 *
 * @{
 */

/**
 * LWCA devices corresponding to a D3D10 device
 */
enum lwdaD3D10DeviceList
{
  lwdaD3D10DeviceListAll           = 1, /**< The LWCA devices for all GPUs used by a D3D10 device */
  lwdaD3D10DeviceListLwrrentFrame  = 2, /**< The LWCA devices for the GPUs used by a D3D10 device in its lwrrently rendering frame */
  lwdaD3D10DeviceListNextFrame     = 3  /**< The LWCA devices for the GPUs to be used by a D3D10 device in the next frame  */
};

/**
 * \brief Registers a Direct3D 10 resource for access by LWCA
 * 
 * Registers the Direct3D 10 resource \p pD3DResource for access by LWCA.  
 *
 * If this call is successful, then the application will be able to map and
 * unmap this resource until it is unregistered through
 * ::lwdaGraphicsUnregisterResource(). Also on success, this call will increase the
 * internal reference count on \p pD3DResource. This reference count will be
 * decremented when this resource is unregistered through
 * ::lwdaGraphicsUnregisterResource().
 *
 * This call potentially has a high-overhead and should not be called every frame
 * in interactive applications.
 *
 * The type of \p pD3DResource must be one of the following.
 *
 * - ::ID3D10Buffer: may be accessed via a device pointer
 * - ::ID3D10Texture1D: individual subresources of the texture may be accessed via arrays
 * - ::ID3D10Texture2D: individual subresources of the texture may be accessed via arrays
 * - ::ID3D10Texture3D: individual subresources of the texture may be accessed via arrays
 *
 * The \p flags argument may be used to specify additional parameters at register
 * time.  The valid values for this parameter are 
 *
 * - ::lwdaGraphicsRegisterFlagsNone: Specifies no hints about how this
 *   resource will be used.
 * - ::lwdaGraphicsRegisterFlagsSurfaceLoadStore: Specifies that LWCA will
 *   bind this resource to a surface reference.
 * - ::lwdaGraphicsRegisterFlagsTextureGather: Specifies that LWCA will perform
 *   texture gather operations on this resource.
 *
 * Not all Direct3D resources of the above types may be used for
 * interoperability with LWCA.  The following are some limitations.
 *
 * - The primary rendertarget may not be registered with LWCA.
 * - Textures which are not of a format which is 1, 2, or 4 channels of 8, 16,
 *   or 32-bit integer or floating-point data cannot be shared.
 * - Surfaces of depth or stencil formats cannot be shared.
 *
 * A complete list of supported DXGI formats is as follows. For compactness the
 * notation A_{B,C,D} represents A_B, A_C, and A_D.
 * - DXGI_FORMAT_A8_UNORM
 * - DXGI_FORMAT_B8G8R8A8_UNORM
 * - DXGI_FORMAT_B8G8R8X8_UNORM
 * - DXGI_FORMAT_R16_FLOAT
 * - DXGI_FORMAT_R16G16B16A16_{FLOAT,SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R16G16_{FLOAT,SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R16_{SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R32_FLOAT
 * - DXGI_FORMAT_R32G32B32A32_{FLOAT,SINT,UINT}
 * - DXGI_FORMAT_R32G32_{FLOAT,SINT,UINT}
 * - DXGI_FORMAT_R32_{SINT,UINT}
 * - DXGI_FORMAT_R8G8B8A8_{SINT,SNORM,UINT,UNORM,UNORM_SRGB}
 * - DXGI_FORMAT_R8G8_{SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R8_{SINT,SNORM,UINT,UNORM}
 *
 * If \p pD3DResource is of incorrect type or is already registered, then 
 * ::lwdaErrorIlwalidResourceHandle is returned. 
 * If \p pD3DResource cannot be registered, then ::lwdaErrorUnknown is returned.
 *
 * \param resource - Pointer to returned resource handle
 * \param pD3DResource - Direct3D resource to register
 * \param flags        - Parameters for resource registration
 * 
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa 
 * ::lwdaGraphicsUnregisterResource,
 * ::lwdaGraphicsMapResources, 
 * ::lwdaGraphicsSubResourceGetMappedArray, 
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwGraphicsD3D10RegisterResource 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsD3D10RegisterResource(struct lwdaGraphicsResource **resource, ID3D10Resource *pD3DResource, unsigned int flags);

/**
 * \brief Gets the device number for an adapter
 *
 * Returns in \p *device the LWCA-compatible device corresponding to the
 * adapter \p pAdapter obtained from ::IDXGIFactory::EnumAdapters. This call
 * will succeed only if a device on adapter \p pAdapter is LWCA-compatible.
 *
 * \param device   - Returns the device corresponding to pAdapter
 * \param pAdapter - D3D10 adapter to get device for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa 
 * ::lwdaGraphicsD3D10RegisterResource,
 * ::lwD3D10GetDevice 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaD3D10GetDevice(int *device, IDXGIAdapter *pAdapter);

/**
 * \brief Gets the LWCA devices corresponding to a Direct3D 10 device
 * 
 * Returns in \p *pLwdaDeviceCount the number of LWCA-compatible devices corresponding 
 * to the Direct3D 10 device \p pD3D10Device.
 * Also returns in \p *pLwdaDevices at most \p lwdaDeviceCount of the the LWCA-compatible devices 
 * corresponding to the Direct3D 10 device \p pD3D10Device.
 *
 * If any of the GPUs being used to render \p pDevice are not LWCA capable then the
 * call will return ::lwdaErrorNoDevice.
 *
 * \param pLwdaDeviceCount - Returned number of LWCA devices corresponding to \p pD3D10Device
 * \param pLwdaDevices     - Returned LWCA devices corresponding to \p pD3D10Device
 * \param lwdaDeviceCount  - The size of the output device array \p pLwdaDevices
 * \param pD3D10Device     - Direct3D 10 device to query for LWCA devices
 * \param deviceList       - The set of devices to return.  This set may be
 *                           ::lwdaD3D10DeviceListAll for all devices, 
 *                           ::lwdaD3D10DeviceListLwrrentFrame for the devices used to
 *                           render the current frame (in SLI), or
 *                           ::lwdaD3D10DeviceListNextFrame for the devices used to
 *                           render the next frame (in SLI).
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNoDevice,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa 
 * ::lwdaGraphicsUnregisterResource,
 * ::lwdaGraphicsMapResources, 
 * ::lwdaGraphicsSubResourceGetMappedArray, 
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwD3D10GetDevices 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaD3D10GetDevices(unsigned int *pLwdaDeviceCount, int *pLwdaDevices, unsigned int lwdaDeviceCount, ID3D10Device *pD3D10Device, enum lwdaD3D10DeviceList deviceList);

/** @} */ /* END LWDART_D3D10 */

/**
 * \addtogroup LWDART_D3D10_DEPRECATED Direct3D 10 Interoperability [DEPRECATED]
 * This section describes deprecated Direct3D 10 interoperability functions.
 *
 * @{
 */

/**
 * LWCA D3D10 Register Flags
 */
enum lwdaD3D10RegisterFlags
{
  lwdaD3D10RegisterFlagsNone  = 0,  /**< Default; Resource can be accessed through a void* */
  lwdaD3D10RegisterFlagsArray = 1   /**< Resource can be accessed through a LWarray* */
};

/**
 * LWCA D3D10 Map Flags
 */
enum lwdaD3D10MapFlags
{
  lwdaD3D10MapFlagsNone         = 0,  /**< Default; Assume resource can be read/written */
  lwdaD3D10MapFlagsReadOnly     = 1,  /**< LWCA kernels will not write to this resource */
  lwdaD3D10MapFlagsWriteDiscard = 2   /**< LWCA kernels will only write to and will not read from this resource */
};

/**
 * \brief Gets the Direct3D device against which the current LWCA context was
 * created
 *
 * \deprecated This function is deprecated as of LWCA 5.0.
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a LWCA device with a D3D10
 * device in order to achieve maximum interoperability performance.
 *
 * \param ppD3D10Device - Returns the Direct3D device for this thread
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa 
 * ::lwdaD3D10SetDirect3DDevice
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10GetDirect3DDevice(ID3D10Device **ppD3D10Device);

/**
 * \brief Sets the Direct3D 10 device to use for interoperability with 
 * a LWCA device
 *
 * \deprecated This function is deprecated as of LWCA 5.0.
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a LWCA device with a D3D10
 * device in order to achieve maximum interoperability performance.
 *
 * \param pD3D10Device - Direct3D device to use for interoperability
 * \param device       - The LWCA device to use.  This device must be among the devices
 *                       returned when querying ::lwdaD3D10DeviceListAll from ::lwdaD3D10GetDevices,
 *                       may be set to -1 to automatically select an appropriate LWCA device.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorInitializationError,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorSetOnActiveProcess
 * \notefnerr
 *
 * \sa 
 * ::lwdaD3D10GetDevice,
 * ::lwdaGraphicsD3D10RegisterResource,
 * ::lwdaDeviceReset
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10SetDirect3DDevice(ID3D10Device *pD3D10Device, int device __dv(-1));

/**
 * \brief Registers a Direct3D 10 resource for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0.
 *
 * Registers the Direct3D resource \p pResource for access by LWCA.
 *
 * If this call is successful, then the application will be able to map and
 * unmap this resource until it is unregistered through
 * ::lwdaD3D10UnregisterResource(). Also on success, this call will increase
 * the internal reference count on \p pResource. This reference count will be
 * decremented when this resource is unregistered through
 * ::lwdaD3D10UnregisterResource().
 *
 * This call potentially has a high-overhead and should not be called every frame
 * in interactive applications.
 *
 * The type of \p pResource must be one of the following:
 *
 * - ::ID3D10Buffer: Cannot be used with \p flags set to
 * \p lwdaD3D10RegisterFlagsArray.
 * - ::ID3D10Texture1D: No restrictions.
 * - ::ID3D10Texture2D: No restrictions.
 * - ::ID3D10Texture3D: No restrictions.
 *
 * The \p flags argument specifies the mechanism through which LWCA will
 * access the Direct3D resource. The following values are allowed.
 *
 * - ::lwdaD3D10RegisterFlagsNone: Specifies that LWCA will access this
 * resource through a \p void*. The pointer, size, and pitch for each
 * subresource of this resource may be queried through
 * ::lwdaD3D10ResourceGetMappedPointer(), ::lwdaD3D10ResourceGetMappedSize(),
 * and ::lwdaD3D10ResourceGetMappedPitch() respectively. This option is valid
 * for all resource types.
 * - ::lwdaD3D10RegisterFlagsArray: Specifies that LWCA will access this
 * resource through a \p LWarray queried on a sub-resource basis through
 * ::lwdaD3D10ResourceGetMappedArray(). This option is only valid for resources
 * of type ::ID3D10Texture1D, ::ID3D10Texture2D, and ::ID3D10Texture3D.
 *
 * Not all Direct3D resources of the above types may be used for
 * interoperability with LWCA. The following are some limitations.
 *
 * - The primary rendertarget may not be registered with LWCA.
 * - Resources allocated as shared may not be registered with LWCA.
 * - Textures which are not of a format which is 1, 2, or 4 channels of 8, 16,
 *   or 32-bit integer or floating-point data cannot be shared.
 * - Surfaces of depth or stencil formats cannot be shared.
 *
 * If Direct3D interoperability is not initialized on this context then
 * ::lwdaErrorIlwalidDevice is returned. If \p pResource is of incorrect type
 * or is already registered then ::lwdaErrorIlwalidResourceHandle is returned.
 * If \p pResource cannot be registered then ::lwdaErrorUnknown is returned.
 *
 * \param pResource - Resource to register
 * \param flags     - Parameters for resource registration
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsD3D10RegisterResource
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10RegisterResource(ID3D10Resource *pResource, unsigned int flags);

/**
 * \brief Unregisters a Direct3D resource
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Unregisters the Direct3D resource \p resource so it is not accessible by
 * LWCA unless registered again.
 *
 * If \p pResource is not registered, then ::lwdaErrorIlwalidResourceHandle
 * is returned.
 *
 * \param pResource - Resource to unregister
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsUnregisterResource
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10UnregisterResource(ID3D10Resource *pResource);

/**
 * \brief Maps Direct3D Resources for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0.  
 *
 * Maps the \p count Direct3D resources in \p ppResources for access by LWCA.
 *
 * The resources in \p ppResources may be accessed in LWCA kernels until they
 * are unmapped. Direct3D should not access any resources while they are
 * mapped by LWCA. If an application does so, the results are undefined.
 *
 * This function provides the synchronization guarantee that any Direct3D
 * calls issued before ::lwdaD3D10MapResources() will complete before any LWCA
 * kernels issued after ::lwdaD3D10MapResources() begin.
 *
 * If any of \p ppResources have not been registered for use with LWCA or if
 * \p ppResources contains any duplicate entries then ::lwdaErrorIlwalidResourceHandle
 * is returned. If any of \p ppResources are presently mapped for access by
 * LWCA then ::lwdaErrorUnknown is returned.
 *
 * \param count       - Number of resources to map for LWCA
 * \param ppResources - Resources to map for LWCA
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsMapResources
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10MapResources(int count, ID3D10Resource **ppResources);

/**
 * \brief Unmaps Direct3D resources
 *
 * \deprecated This function is deprecated as of LWCA 3.0.   
 *
 * Unmaps the \p count Direct3D resource in \p ppResources.
 *
 * This function provides the synchronization guarantee that any LWCA kernels
 * issued before ::lwdaD3D10UnmapResources() will complete before any Direct3D
 * calls issued after ::lwdaD3D10UnmapResources() begin.
 *
 * If any of \p ppResources have not been registered for use with LWCA or if
 * \p ppResources contains any duplicate entries, then
 * ::lwdaErrorIlwalidResourceHandle is returned. If any of \p ppResources are
 * not presently mapped for access by LWCA then ::lwdaErrorUnknown is returned.
 *
 * \param count       - Number of resources to unmap for LWCA
 * \param ppResources - Resources to unmap for LWCA
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsUnmapResources
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10UnmapResources(int count, ID3D10Resource **ppResources);

/**
 * \brief Gets an array through which to access a subresource of a Direct3D
 * resource which has been mapped for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Returns in \p *ppArray an array through which the subresource of the mapped
 * Direct3D resource \p pResource which corresponds to \p subResource may be
 * accessed. The value set in \p ppArray may change every time that
 * \p pResource is mapped.
 *
 * If \p pResource is not registered, then ::lwdaErrorIlwalidResourceHandle is
 * returned. If \p pResource was not registered with usage flags
 * ::lwdaD3D10RegisterFlagsArray, then ::lwdaErrorIlwalidResourceHandle is
 * returned. If \p pResource is not mapped then ::lwdaErrorUnknown is returned.
 *
 * For usage requirements of the \p subResource parameter, see
 * ::lwdaD3D10ResourceGetMappedPointer().
 *
 * \param ppArray     - Returned array corresponding to subresource
 * \param pResource   - Mapped resource to access
 * \param subResource - Subresource of pResource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsSubResourceGetMappedArray
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10ResourceGetMappedArray(lwdaArray **ppArray, ID3D10Resource *pResource, unsigned int subResource);

/**
 * \brief Set usage flags for mapping a Direct3D resource
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Set usage flags for mapping the Direct3D resource \p pResource.  
 *
 * Changes to flags will take effect the next time \p pResource is mapped.
 * The \p flags argument may be any of the following:
 *
 * - ::lwdaD3D10MapFlagsNone: Specifies no hints about how this resource will
 * be used. It is therefore assumed that this resource will be read from and
 * written to by LWCA kernels. This is the default value.
 * - ::lwdaD3D10MapFlagsReadOnly: Specifies that LWCA kernels which access
 * this resource will not write to this resource.
 * - ::lwdaD3D10MapFlagsWriteDiscard: Specifies that LWCA kernels which access
 * this resource will not read from this resource and will write over the
 * entire contents of the resource, so none of the data previously stored in
 * the resource will be preserved.
 *
 * If \p pResource has not been registered for use with LWCA then
 * ::lwdaErrorIlwalidHandle is returned. If \p pResource is presently mapped
 * for access by LWCA then ::lwdaErrorUnknown is returned.
 *
 * \param pResource - Registered resource to set flags for
 * \param flags     - Parameters for resource mapping
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown,
 * \notefnerr
 *
 * \sa ::lwdaGraphicsResourceSetMapFlags
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10ResourceSetMapFlags(ID3D10Resource *pResource, unsigned int flags); 

/**
 * \brief Gets the dimensions of a registered Direct3D surface
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Returns in \p *pWidth, \p *pHeight, and \p *pDepth the dimensions of the
 * subresource of the mapped Direct3D resource \p pResource which corresponds
 * to \p subResource.
 *
 * Since anti-aliased surfaces may have multiple samples per pixel, it is
 * possible that the dimensions of a resource will be an integer factor larger
 * than the dimensions reported by the Direct3D runtime.
 *
 * The parameters \p pWidth, \p pHeight, and \p pDepth are optional. For 2D
 * surfaces, the value returned in \p *pDepth will be 0.
 *
 * If \p pResource is not of type ::ID3D10Texture1D, ::ID3D10Texture2D, or
 * ::ID3D10Texture3D, or if \p pResource has not been registered for use with
 * LWCA, then ::lwdaErrorIlwalidHandle is returned.

 * For usage requirements of \p subResource parameters see
 * ::lwdaD3D10ResourceGetMappedPointer().
 *
 * \param pWidth      - Returned width of surface
 * \param pHeight     - Returned height of surface
 * \param pDepth      - Returned depth of surface
 * \param pResource   - Registered resource to access
 * \param subResource - Subresource of pResource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * \notefnerr
 *
 * \sa ::lwdaGraphicsSubResourceGetMappedArray
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10ResourceGetSurfaceDimensions(size_t *pWidth, size_t *pHeight, size_t *pDepth, ID3D10Resource *pResource, unsigned int subResource); 

/**
 * \brief Gets a pointer through which to access a subresource of a Direct3D
 * resource which has been mapped for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Returns in \p *pPointer the base pointer of the subresource of the mapped
 * Direct3D resource \p pResource which corresponds to \p subResource. The
 * value set in \p pPointer may change every time that \p pResource is mapped.
 *
 * If \p pResource is not registered, then ::lwdaErrorIlwalidResourceHandle is
 * returned. If \p pResource was not registered with usage flags
 * ::lwdaD3D9RegisterFlagsNone, then ::lwdaErrorIlwalidResourceHandle is
 * returned. If \p pResource is not mapped then ::lwdaErrorUnknown is returned.
 *
 * If \p pResource is of type ::ID3D10Buffer then \p subResource must be 0.
 * If \p pResource is of any other type, then the value of \p subResource must
 * come from the subresource callwlation in ::D3D10CalcSubResource().
 *
 * \param pPointer    - Returned pointer corresponding to subresource
 * \param pResource   - Mapped resource to access
 * \param subResource - Subresource of pResource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsResourceGetMappedPointer
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10ResourceGetMappedPointer(void **pPointer, ID3D10Resource *pResource, unsigned int subResource);

/**
 * \brief Gets the size of a subresource of a Direct3D resource which has been
 * mapped for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Returns in \p *pSize the size of the subresource of the mapped Direct3D
 * resource \p pResource which corresponds to \p subResource. The value set in
 * \p pSize may change every time that \p pResource is mapped.
 *
 * If \p pResource has not been registered for use with LWCA then
 * ::lwdaErrorIlwalidHandle is returned. If \p pResource was not registered
 * with usage flags ::lwdaD3D10RegisterFlagsNone, then
 * ::lwdaErrorIlwalidResourceHandle is returned. If \p pResource is not mapped for
 * access by LWCA then ::lwdaErrorUnknown is returned.
 *
 * For usage requirements of the \p subResource parameter see
 * ::lwdaD3D10ResourceGetMappedPointer().
 *
 * \param pSize       - Returned size of subresource
 * \param pResource   - Mapped resource to access
 * \param subResource - Subresource of pResource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsResourceGetMappedPointer
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10ResourceGetMappedSize(size_t *pSize, ID3D10Resource *pResource, unsigned int subResource);

/**
 * \brief Gets the pitch of a subresource of a Direct3D resource which has been
 * mapped for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Returns in \p *pPitch and \p *pPitchSlice the pitch and Z-slice pitch of
 * the subresource of the mapped Direct3D resource \p pResource, which
 * corresponds to \p subResource. The values set in \p pPitch and
 * \p pPitchSlice may change every time that \p pResource is mapped.
 *
 * The pitch and Z-slice pitch values may be used to compute the location of a
 * sample on a surface as follows.
 *
 * For a 2D surface, the byte offset of the sample at position \b x, \b y from
 * the base pointer of the surface is:
 *
 * \b y * \b pitch + (<b>bytes per pixel</b>) * \b x
 *
 * For a 3D surface, the byte offset of the sample at position \b x, \b y,
 * \b z from the base pointer of the surface is:
 *
 * \b z* \b slicePitch + \b y * \b pitch + (<b>bytes per pixel</b>) * \b x
 *
 * Both parameters \p pPitch and \p pPitchSlice are optional and may be set to
 * NULL.
 *
 * If \p pResource is not of type ::ID3D10Texture1D, ::ID3D10Texture2D, or
 * ::ID3D10Texture3D, or if \p pResource has not been registered for use with
 * LWCA, then ::lwdaErrorIlwalidResourceHandle is returned. If \p pResource was
 * not registered with usage flags ::lwdaD3D10RegisterFlagsNone, then
 * ::lwdaErrorIlwalidResourceHandle is returned. If \p pResource is not mapped
 * for access by LWCA then ::lwdaErrorUnknown is returned.
 *
 * For usage requirements of the \p subResource parameter see
 * ::lwdaD3D10ResourceGetMappedPointer().
 *
 * \param pPitch      - Returned pitch of subresource
 * \param pPitchSlice - Returned Z-slice pitch of subresource
 * \param pResource   - Mapped resource to access
 * \param subResource - Subresource of pResource to access
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsSubResourceGetMappedArray
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D10ResourceGetMappedPitch(size_t *pPitch, size_t *pPitchSlice, ID3D10Resource *pResource, unsigned int subResource);

/** @} */ /* END LWDART_D3D10_DEPRECATED */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#undef __dv
#undef __LWDA_DEPRECATED

#endif /* __LWDA_D3D10_INTEROP_H__ */
