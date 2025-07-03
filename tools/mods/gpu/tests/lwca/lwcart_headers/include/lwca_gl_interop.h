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

#if !defined(__LWDA_GL_INTEROP_H__)
#define __LWDA_GL_INTEROP_H__

#include "lwda_runtime_api.h"

#if defined(__APPLE__)

#include <OpenGL/gl.h>

#else /* __APPLE__ */

#if defined(__arm__) || defined(__aarch64__)
#ifndef GL_VERSION
#error Please include the appropriate gl headers before including lwda_gl_interop.h
#endif
#else
#include <GL/gl.h>
#endif

#endif /* __APPLE__ */

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
 * \addtogroup LWDART_OPENGL OpenGL Interoperability
 * This section describes the OpenGL interoperability functions of the LWCA
 * runtime application programming interface. Note that mapping of OpenGL
 * resources is performed with the graphics API agnostic, resource mapping 
 * interface described in \ref LWDART_INTEROP "Graphics Interopability".
 *
 * @{
 */

/**
 * LWCA devices corresponding to the current OpenGL context
 */
enum lwdaGLDeviceList
{
  lwdaGLDeviceListAll           = 1, /**< The LWCA devices for all GPUs used by the current OpenGL context */
  lwdaGLDeviceListLwrrentFrame  = 2, /**< The LWCA devices for the GPUs used by the current OpenGL context in its lwrrently rendering frame */
  lwdaGLDeviceListNextFrame     = 3  /**< The LWCA devices for the GPUs to be used by the current OpenGL context in the next frame  */
};

/**
 * \brief Gets the LWCA devices associated with the current OpenGL context
 *
 * Returns in \p *pLwdaDeviceCount the number of LWCA-compatible devices 
 * corresponding to the current OpenGL context. Also returns in \p *pLwdaDevices 
 * at most \p lwdaDeviceCount of the LWCA-compatible devices corresponding to 
 * the current OpenGL context. If any of the GPUs being used by the current OpenGL
 * context are not LWCA capable then the call will return ::lwdaErrorNoDevice.
 *
 * \param pLwdaDeviceCount - Returned number of LWCA devices corresponding to the 
 *                           current OpenGL context
 * \param pLwdaDevices     - Returned LWCA devices corresponding to the current 
 *                           OpenGL context
 * \param lwdaDeviceCount  - The size of the output device array \p pLwdaDevices
 * \param deviceList       - The set of devices to return.  This set may be
 *                           ::lwdaGLDeviceListAll for all devices, 
 *                           ::lwdaGLDeviceListLwrrentFrame for the devices used to
 *                           render the current frame (in SLI), or
 *                           ::lwdaGLDeviceListNextFrame for the devices used to
 *                           render the next frame (in SLI).
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorNoDevice,
 * ::lwdaErrorIlwalidGraphicsContext,
 * ::lwdaErrorUnknown
 *
 * \note This function is not supported on Mac OS X.
 * \notefnerr
 *
 * \sa 
 * ::lwdaGraphicsUnregisterResource,
 * ::lwdaGraphicsMapResources, 
 * ::lwdaGraphicsSubResourceGetMappedArray, 
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwGLGetDevices 
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGLGetDevices(unsigned int *pLwdaDeviceCount, int *pLwdaDevices, unsigned int lwdaDeviceCount, enum lwdaGLDeviceList deviceList);

/**
 * \brief Register an OpenGL texture or renderbuffer object
 *
 * Registers the texture or renderbuffer object specified by \p image for access by LWCA.
 * A handle to the registered object is returned as \p resource.
 *
 * \p target must match the type of the object, and must be one of ::GL_TEXTURE_2D, 
 * ::GL_TEXTURE_RECTANGLE, ::GL_TEXTURE_LWBE_MAP, ::GL_TEXTURE_3D, ::GL_TEXTURE_2D_ARRAY, 
 * or ::GL_RENDERBUFFER.
 *
 * The register flags \p flags specify the intended usage, as follows: 
 * - ::lwdaGraphicsRegisterFlagsNone: Specifies no hints about how this
 *   resource will be used. It is therefore assumed that this resource will be
 *   read from and written to by LWCA. This is the default value.
 * - ::lwdaGraphicsRegisterFlagsReadOnly: Specifies that LWCA
 *   will not write to this resource.
 * - ::lwdaGraphicsRegisterFlagsWriteDiscard: Specifies that
 *   LWCA will not read from this resource and will write over the
 *   entire contents of the resource, so none of the data previously
 *   stored in the resource will be preserved.
 * - ::lwdaGraphicsRegisterFlagsSurfaceLoadStore: Specifies that LWCA will
 *   bind this resource to a surface reference.
 * - ::lwdaGraphicsRegisterFlagsTextureGather: Specifies that LWCA will perform
 *   texture gather operations on this resource.
 *
 * The following image formats are supported. For brevity's sake, the list is abbreviated.
 * For ex., {GL_R, GL_RG} X {8, 16} would expand to the following 4 formats 
 * {GL_R8, GL_R16, GL_RG8, GL_RG16} :
 * - GL_RED, GL_RG, GL_RGBA, GL_LUMINANCE, GL_ALPHA, GL_LUMINANCE_ALPHA, GL_INTENSITY
 * - {GL_R, GL_RG, GL_RGBA} X {8, 16, 16F, 32F, 8UI, 16UI, 32UI, 8I, 16I, 32I}
 * - {GL_LUMINANCE, GL_ALPHA, GL_LUMINANCE_ALPHA, GL_INTENSITY} X
 * {8, 16, 16F_ARB, 32F_ARB, 8UI_EXT, 16UI_EXT, 32UI_EXT, 8I_EXT, 16I_EXT, 32I_EXT}
 *
 * The following image classes are lwrrently disallowed:
 * - Textures with borders
 * - Multisampled renderbuffers
 *
 * \param resource - Pointer to the returned object handle
 * \param image    - name of texture or renderbuffer object to be registered
 * \param target   - Identifies the type of object specified by \p image 
 * \param flags    - Register flags
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
 * ::lwGraphicsGLRegisterImage
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsGLRegisterImage(struct lwdaGraphicsResource **resource, GLuint image, GLenum target, unsigned int flags);

/**
 * \brief Registers an OpenGL buffer object
 *
 * Registers the buffer object specified by \p buffer for access by
 * LWCA.  A handle to the registered object is returned as \p
 * resource.  The register flags \p flags specify the intended usage,
 * as follows:
 *
 * - ::lwdaGraphicsRegisterFlagsNone: Specifies no hints about how this
 *   resource will be used. It is therefore assumed that this resource will be
 *   read from and written to by LWCA. This is the default value.
 * - ::lwdaGraphicsRegisterFlagsReadOnly: Specifies that LWCA
 *   will not write to this resource.
 * - ::lwdaGraphicsRegisterFlagsWriteDiscard: Specifies that
 *   LWCA will not read from this resource and will write over the
 *   entire contents of the resource, so none of the data previously
 *   stored in the resource will be preserved.
 *
 * \param resource - Pointer to the returned object handle
 * \param buffer   - name of buffer object to be registered
 * \param flags    - Register flags
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
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwGraphicsGLRegisterBuffer
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsGLRegisterBuffer(struct lwdaGraphicsResource **resource, GLuint buffer, unsigned int flags);

#ifdef _WIN32
#ifndef WGL_LW_gpu_affinity
typedef void* HGPULW;
#endif

/**
 * \brief Gets the LWCA device associated with hGpu
 *
 * Returns the LWCA device associated with a hGpu, if applicable.
 *
 * \param device - Returns the device associated with hGpu, or -1 if hGpu is
 * not a compute device.
 * \param hGpu   - Handle to a GPU, as queried via WGL_LW_gpu_affinity
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 *
 * \sa
 * ::WGL_LW_gpu_affinity,
 * ::lwWGLGetDevice
 */
extern __host__ lwdaError_t LWDARTAPI lwdaWGLGetDevice(int *device, HGPULW hGpu);
#endif

/** @} */ /* END LWDART_OPENGL */

/**
 * \addtogroup LWDART_OPENGL_DEPRECATED OpenGL Interoperability [DEPRECATED]
 * This section describes deprecated OpenGL interoperability functionality.
 *
 * @{
 */

/**
 * LWCA GL Map Flags
 */
enum lwdaGLMapFlags
{
  lwdaGLMapFlagsNone         = 0,  /**< Default; Assume resource can be read/written */
  lwdaGLMapFlagsReadOnly     = 1,  /**< LWCA kernels will not write to this resource */
  lwdaGLMapFlagsWriteDiscard = 2   /**< LWCA kernels will only write to and will not read from this resource */
};

/**
 * \brief Sets a LWCA device to use OpenGL interoperability
 *
 * \deprecated This function is deprecated as of LWCA 5.0. 
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a LWCA device with an OpenGL
 * context in order to achieve maximum interoperability performance.
 *
 * \param device - Device to use for OpenGL interoperability
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidDevice,
 * ::lwdaErrorSetOnActiveProcess
 * \notefnerr
 *
 * \sa ::lwdaGraphicsGLRegisterBuffer, ::lwdaGraphicsGLRegisterImage
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLSetGLDevice(int device);

/**
 * \brief Registers a buffer object for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Registers the buffer object of ID \p bufObj for access by
 * LWCA. This function must be called before LWCA can map the buffer
 * object.  The OpenGL context used to create the buffer, or another
 * context from the same share group, must be bound to the current
 * thread when this is called.
 *
 * \param bufObj - Buffer object ID to register
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorInitializationError
 * \notefnerr
 *
 * \sa ::lwdaGraphicsGLRegisterBuffer
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLRegisterBufferObject(GLuint bufObj);

/**
 * \brief Maps a buffer object for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Maps the buffer object of ID \p bufObj into the address space of
 * LWCA and returns in \p *devPtr the base pointer of the resulting
 * mapping.  The buffer must have previously been registered by
 * calling ::lwdaGLRegisterBufferObject().  While a buffer is mapped
 * by LWCA, any OpenGL operation which references the buffer will
 * result in undefined behavior.  The OpenGL context used to create
 * the buffer, or another context from the same share group, must be
 * bound to the current thread when this is called.
 *
 * All streams in the current thread are synchronized with the current
 * GL context.
 *
 * \param devPtr - Returned device pointer to LWCA object
 * \param bufObj - Buffer object ID to map
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorMapBufferObjectFailed
 * \notefnerr
 *
 * \sa ::lwdaGraphicsMapResources
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLMapBufferObject(void **devPtr, GLuint bufObj);

/**
 * \brief Unmaps a buffer object for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Unmaps the buffer object of ID \p bufObj for access by LWCA.  When
 * a buffer is unmapped, the base address returned by
 * ::lwdaGLMapBufferObject() is invalid and subsequent references to
 * the address result in undefined behavior.  The OpenGL context used
 * to create the buffer, or another context from the same share group,
 * must be bound to the current thread when this is called.
 *
 * All streams in the current thread are synchronized with the current
 * GL context.
 *
 * \param bufObj - Buffer object to unmap
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnmapBufferObjectFailed
 * \notefnerr
 *
 * \sa ::lwdaGraphicsUnmapResources
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLUnmapBufferObject(GLuint bufObj);

/**
 * \brief Unregisters a buffer object for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Unregisters the buffer object of ID \p bufObj for access by LWCA
 * and releases any LWCA resources associated with the buffer.  Once a
 * buffer is unregistered, it may no longer be mapped by LWCA.  The GL
 * context used to create the buffer, or another context from the
 * same share group, must be bound to the current thread when this is
 * called.
 *
 * \param bufObj - Buffer object to unregister
 *
 * \return
 * ::lwdaSuccess
 * \notefnerr
 *
 * \sa ::lwdaGraphicsUnregisterResource
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLUnregisterBufferObject(GLuint bufObj);

/**
 * \brief Set usage flags for mapping an OpenGL buffer
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Set flags for mapping the OpenGL buffer \p bufObj
 *
 * Changes to flags will take effect the next time \p bufObj is mapped.
 * The \p flags argument may be any of the following:
 *
 * - ::lwdaGLMapFlagsNone: Specifies no hints about how this buffer will
 * be used. It is therefore assumed that this buffer will be read from and
 * written to by LWCA kernels. This is the default value.
 * - ::lwdaGLMapFlagsReadOnly: Specifies that LWCA kernels which access this
 * buffer will not write to the buffer.
 * - ::lwdaGLMapFlagsWriteDiscard: Specifies that LWCA kernels which access
 * this buffer will not read from the buffer and will write over the
 * entire contents of the buffer, so none of the data previously stored in
 * the buffer will be preserved.
 *
 * If \p bufObj has not been registered for use with LWCA, then
 * ::lwdaErrorIlwalidResourceHandle is returned. If \p bufObj is presently
 * mapped for access by LWCA, then ::lwdaErrorUnknown is returned.
 *
 * \param bufObj    - Registered buffer object to set flags for
 * \param flags     - Parameters for buffer mapping
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorUnknown
 * \notefnerr
 *
 * \sa ::lwdaGraphicsResourceSetMapFlags
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLSetBufferObjectMapFlags(GLuint bufObj, unsigned int flags); 

/**
 * \brief Maps a buffer object for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Maps the buffer object of ID \p bufObj into the address space of
 * LWCA and returns in \p *devPtr the base pointer of the resulting
 * mapping.  The buffer must have previously been registered by
 * calling ::lwdaGLRegisterBufferObject().  While a buffer is mapped
 * by LWCA, any OpenGL operation which references the buffer will
 * result in undefined behavior.  The OpenGL context used to create
 * the buffer, or another context from the same share group, must be
 * bound to the current thread when this is called.
 *
 * Stream /p stream is synchronized with the current GL context.
 *
 * \param devPtr - Returned device pointer to LWCA object
 * \param bufObj - Buffer object ID to map
 * \param stream - Stream to synchronize
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorMapBufferObjectFailed
 * \notefnerr
 *
 * \sa ::lwdaGraphicsMapResources
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLMapBufferObjectAsync(void **devPtr, GLuint bufObj, lwdaStream_t stream);

/**
 * \brief Unmaps a buffer object for access by LWCA
 *
 * \deprecated This function is deprecated as of LWCA 3.0. 
 *
 * Unmaps the buffer object of ID \p bufObj for access by LWCA.  When
 * a buffer is unmapped, the base address returned by
 * ::lwdaGLMapBufferObject() is invalid and subsequent references to
 * the address result in undefined behavior.  The OpenGL context used
 * to create the buffer, or another context from the same share group,
 * must be bound to the current thread when this is called.
 *
 * Stream /p stream is synchronized with the current GL context.
 *
 * \param bufObj - Buffer object to unmap
 * \param stream - Stream to synchronize
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorUnmapBufferObjectFailed
 * \notefnerr
 *
 * \sa ::lwdaGraphicsUnmapResources
 */
extern __LWDA_DEPRECATED __host__ lwdaError_t LWDARTAPI lwdaGLUnmapBufferObjectAsync(GLuint bufObj, lwdaStream_t stream);

/** @} */ /* END LWDART_OPENGL_DEPRECATED */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#undef __LWDA_DEPRECATED

#endif /* __LWDA_GL_INTEROP_H__ */

