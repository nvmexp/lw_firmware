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

#if !defined(__LWDA_D3D11_INTEROP_H__)
#define __LWDA_D3D11_INTEROP_H__

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

#include <d3d11.h>

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
 * \addtogroup LWDART_D3D11 Direct3D 11 Interoperability
 * This section describes the Direct3D 11 interoperability functions of the LWCA
 * runtime application programming interface. Note that mapping of Direct3D 11
 * resources is performed with the graphics API agnostic, resource mapping 
 * interface described in \ref LWDART_INTEROP "Graphics Interopability".
 *
 * @{
 */

/**
 * LWCA devices corresponding to a D3D11 device
 */
enum lwdaD3D11DeviceList
{
  lwdaD3D11DeviceListAll           = 1, /**< The LWCA devices for all GPUs used by a D3D11 device */
  lwdaD3D11DeviceListLwrrentFrame  = 2, /**< The LWCA devices for the GPUs used by a D3D11 device in its lwrrently rendering frame */
  lwdaD3D11DeviceListNextFrame     = 3  /**< The LWCA devices for the GPUs to be used by a D3D11 device in the next frame  */
};

/**
 * \brief Register a Direct3D 11 resource for access by LWCA
 * 
 * Registers the Direct3D 11 resource \p pD3DResource for access by LWCA.  
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
 * - ::ID3D11Buffer: may be accessed via a device pointer
 * - ::ID3D11Texture1D: individual subresources of the texture may be accessed via arrays
 * - ::ID3D11Texture2D: individual subresources of the texture may be accessed via arrays
 * - ::ID3D11Texture3D: individual subresources of the texture may be accessed via arrays
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
 * ::lwGraphicsD3D11RegisterResource 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsD3D11RegisterResource(struct lwdaGraphicsResource **resource, ID3D11Resource *pD3DResource, unsigned int flags);

/**
 * \brief Gets the device number for an adapter
 *
 * Returns in \p *device the LWCA-compatible device corresponding to the
 * adapter \p pAdapter obtained from ::IDXGIFactory::EnumAdapters. This call
 * will succeed only if a device on adapter \p pAdapter is LWCA-compatible.
 *
 * \param device   - Returns the device corresponding to pAdapter
 * \param pAdapter - D3D11 adapter to get device for
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa 
 * ::lwdaGraphicsUnregisterResource,
 * ::lwdaGraphicsMapResources, 
 * ::lwdaGraphicsSubResourceGetMappedArray, 
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwD3D11GetDevice 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaD3D11GetDevice(int *device, IDXGIAdapter *pAdapter);

/**
 * \brief Gets the LWCA devices corresponding to a Direct3D 11 device
 * 
 * Returns in \p *pLwdaDeviceCount the number of LWCA-compatible devices corresponding 
 * to the Direct3D 11 device \p pD3D11Device.
 * Also returns in \p *pLwdaDevices at most \p lwdaDeviceCount of the the LWCA-compatible devices 
 * corresponding to the Direct3D 11 device \p pD3D11Device.
 *
 * If any of the GPUs being used to render \p pDevice are not LWCA capable then the
 * call will return ::lwdaErrorNoDevice.
 *
 * \param pLwdaDeviceCount - Returned number of LWCA devices corresponding to \p pD3D11Device
 * \param pLwdaDevices     - Returned LWCA devices corresponding to \p pD3D11Device
 * \param lwdaDeviceCount  - The size of the output device array \p pLwdaDevices
 * \param pD3D11Device     - Direct3D 11 device to query for LWCA devices
 * \param deviceList       - The set of devices to return.  This set may be
 *                           ::lwdaD3D11DeviceListAll for all devices, 
 *                           ::lwdaD3D11DeviceListLwrrentFrame for the devices used to
 *                           render the current frame (in SLI), or
 *                           ::lwdaD3D11DeviceListNextFrame for the devices used to
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
 * ::lwD3D11GetDevices 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaD3D11GetDevices(unsigned int *pLwdaDeviceCount, int *pLwdaDevices, unsigned int lwdaDeviceCount, ID3D11Device *pD3D11Device, enum lwdaD3D11DeviceList deviceList);

/** @} */ /* END LWDART_D3D11 */

/**
 * \addtogroup LWDART_D3D11_DEPRECATED Direct3D 11 Interoperability [DEPRECATED]
 * This section describes deprecated Direct3D 11 interoperability functions.
 *
 * @{
 */

/**
 * \brief Gets the Direct3D device against which the current LWCA context was
 * created
 *
 * \deprecated This function is deprecated as of LWCA 5.0. 
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a LWCA device with a D3D11
 * device in order to achieve maximum interoperability performance.
 *
 * \param ppD3D11Device - Returns the Direct3D device for this thread
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa 
 * ::lwdaD3D11SetDirect3DDevice
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D11GetDirect3DDevice(ID3D11Device **ppD3D11Device);

/**
 * \brief Sets the Direct3D 11 device to use for interoperability with 
 * a LWCA device
 *
 * \deprecated This function is deprecated as of LWCA 5.0. 
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a LWCA device with a D3D11
 * device in order to achieve maximum interoperability performance.
 *
 * \param pD3D11Device - Direct3D device to use for interoperability
 * \param device       - The LWCA device to use.  This device must be among the devices
 *                       returned when querying ::lwdaD3D11DeviceListAll from ::lwdaD3D11GetDevices,
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
 * ::lwdaD3D11GetDevice,
 * ::lwdaGraphicsD3D11RegisterResource,
 * ::lwdaDeviceReset
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaD3D11SetDirect3DDevice(ID3D11Device *pD3D11Device, int device __dv(-1));

/** @} */ /* END LWDART_D3D11_DEPRECATED */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#undef __dv
#undef __LWDA_DEPRECATED

#endif /* __LWDA_D3D11_INTEROP_H__ */
