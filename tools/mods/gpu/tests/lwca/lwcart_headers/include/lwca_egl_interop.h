/*
 * Copyright 1993-2019 LWPU Corporation.  All rights reserved.
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

#if !defined(__LWDA_EGL_INTEROP_H__)
#define __LWDA_EGL_INTEROP_H__

#include "lwda_runtime_api.h"
#include "lwda_runtime.h"
#include "lwdart_platform.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * \addtogroup LWDART_TYPES
 * @{
 */

 /**
 * Maximum number of planes per frame
 */
#define LWDA_EGL_MAX_PLANES 3

/**
 * LWCA EglFrame type - array or pointer
 */
typedef enum lwdaEglFrameType_enum
{
    lwdaEglFrameTypeArray = 0,  /**< Frame type LWCA array */
    lwdaEglFrameTypePitch = 1,  /**< Frame type LWCA pointer */
} lwdaEglFrameType;

/**
 * Resource location flags- sysmem or vidmem
 *
 * For LWCA context on iGPU, since video and system memory are equivalent -
 * these flags will not have an effect on the exelwtion.
 *
 * For LWCA context on dGPU, applications can use the flag ::lwdaEglResourceLocationFlags
 * to give a hint about the desired location.
 *
 * ::lwdaEglResourceLocationSysmem - the frame data is made resident on the system memory
 * to be accessed by LWCA.
 *
 * ::lwdaEglResourceLocatiolwidmem - the frame data is made resident on the dedicated
 * video memory to be accessed by LWCA.
 *
 * There may be an additional latency due to new allocation and data migration,
 * if the frame is produced on a different memory.
 */
typedef enum lwdaEglResourceLocationFlags_enum {
    lwdaEglResourceLocationSysmem   = 0x00,       /**< Resource location sysmem */
    lwdaEglResourceLocatiolwidmem   = 0x01,       /**< Resource location vidmem */
} lwdaEglResourceLocationFlags;

/**
 * LWCA EGL Color Format - The different planar and multiplanar formats lwrrently supported for LWDA_EGL interops.
 */
typedef enum lwdaEglColorFormat_enum {
    lwdaEglColorFormatYUV420Planar            = 0,  /**< Y, U, V in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYUV420SemiPlanar        = 1,  /**< Y, UV in two surfaces (UV as one surface) with VU byte ordering, width, height ratio same as YUV420Planar. */
    lwdaEglColorFormatYUV422Planar            = 2,  /**< Y, U, V  each in a separate  surface, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYUV422SemiPlanar        = 3,  /**< Y, UV in two surfaces with VU byte ordering, width, height ratio same as YUV422Planar. */
    lwdaEglColorFormatARGB                    = 6,  /**< R/G/B/A four channels in one surface with BGRA byte ordering. */
    lwdaEglColorFormatRGBA                    = 7,  /**< R/G/B/A four channels in one surface with ABGR byte ordering. */
    lwdaEglColorFormatL                       = 8,  /**< single luminance channel in one surface. */
    lwdaEglColorFormatR                       = 9,  /**< single color channel in one surface. */
    lwdaEglColorFormatYUV444Planar            = 10, /**< Y, U, V in three surfaces, each in a separate surface, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYUV444SemiPlanar        = 11, /**< Y, UV in two surfaces (UV as one surface) with VU byte ordering, width, height ratio same as YUV444Planar. */
    lwdaEglColorFormatYUYV422                 = 12, /**< Y, U, V in one surface, interleaved as UYVY in one channel. */
    lwdaEglColorFormatUYVY422                 = 13, /**< Y, U, V in one surface, interleaved as YUYV in one channel. */
    lwdaEglColorFormatABGR                    = 14, /**< R/G/B/A four channels in one surface with RGBA byte ordering. */
    lwdaEglColorFormatBGRA                    = 15, /**< R/G/B/A four channels in one surface with ARGB byte ordering. */
    lwdaEglColorFormatA                       = 16, /**< Alpha color format - one channel in one surface. */
    lwdaEglColorFormatRG                      = 17, /**< R/G color format - two channels in one surface with GR byte ordering */
    lwdaEglColorFormatAYUV                    = 18, /**< Y, U, V, A four channels in one surface, interleaved as VUYA. */
    lwdaEglColorFormatYVU444SemiPlanar        = 19, /**< Y, VU in two surfaces (VU as one surface) with UV byte ordering, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU422SemiPlanar        = 20, /**< Y, VU in two surfaces (VU as one surface) with UV byte ordering, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU420SemiPlanar        = 21, /**< Y, VU in two surfaces (VU as one surface) with UV byte ordering, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY10V10U10_444SemiPlanar = 22, /**< Y10, V10U10 in two surfaces (VU as one surface) with UV byte ordering, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatY10V10U10_420SemiPlanar = 23, /**< Y10, V10U10 in two surfaces (VU as one surface) with UV byte ordering, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY12V12U12_444SemiPlanar = 24, /**< Y12, V12U12 in two surfaces (VU as one surface) with UV byte ordering, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatY12V12U12_420SemiPlanar = 25, /**< Y12, V12U12 in two surfaces (VU as one surface) with UV byte ordering, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatVYUY_ER                 = 26, /**< Extended Range Y, U, V in one surface, interleaved as YVYU in one channel. */
    lwdaEglColorFormatUYVY_ER                 = 27, /**< Extended Range Y, U, V in one surface, interleaved as YUYV in one channel. */
    lwdaEglColorFormatYUYV_ER                 = 28, /**< Extended Range Y, U, V in one surface, interleaved as UYVY in one channel. */
    lwdaEglColorFormatYVYU_ER                 = 29, /**< Extended Range Y, U, V in one surface, interleaved as VYUY in one channel. */
    lwdaEglColorFormatYUVA_ER                 = 31, /**< Extended Range Y, U, V, A four channels in one surface, interleaved as AVUY. */
    lwdaEglColorFormatAYUV_ER                 = 32, /**< Extended Range Y, U, V, A four channels in one surface, interleaved as VUYA. */
    lwdaEglColorFormatYUV444Planar_ER         = 33, /**< Extended Range Y, U, V in three surfaces, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYUV422Planar_ER         = 34, /**< Extended Range Y, U, V in three surfaces, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYUV420Planar_ER         = 35, /**< Extended Range Y, U, V in three surfaces, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYUV444SemiPlanar_ER     = 36, /**< Extended Range Y, UV in two surfaces (UV as one surface) with VU byte ordering, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYUV422SemiPlanar_ER     = 37, /**< Extended Range Y, UV in two surfaces (UV as one surface) with VU byte ordering, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYUV420SemiPlanar_ER     = 38, /**< Extended Range Y, UV in two surfaces (UV as one surface) with VU byte ordering, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYVU444Planar_ER         = 39, /**< Extended Range Y, V, U in three surfaces, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU422Planar_ER         = 40, /**< Extended Range Y, V, U in three surfaces, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU420Planar_ER         = 41, /**< Extended Range Y, V, U in three surfaces, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYVU444SemiPlanar_ER     = 42, /**< Extended Range Y, VU in two surfaces (VU as one surface) with UV byte ordering, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU422SemiPlanar_ER     = 43, /**< Extended Range Y, VU in two surfaces (VU as one surface) with UV byte ordering, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU420SemiPlanar_ER     = 44, /**< Extended Range Y, VU in two surfaces (VU as one surface) with UV byte ordering, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatBayerRGGB               = 45, /**< Bayer format - one channel in one surface with interleaved RGGB ordering. */
    lwdaEglColorFormatBayerBGGR               = 46, /**< Bayer format - one channel in one surface with interleaved BGGR ordering. */
    lwdaEglColorFormatBayerGRBG               = 47, /**< Bayer format - one channel in one surface with interleaved GRBG ordering. */
    lwdaEglColorFormatBayerGBRG               = 48, /**< Bayer format - one channel in one surface with interleaved GBRG ordering. */
    lwdaEglColorFormatBayer10RGGB             = 49, /**< Bayer10 format - one channel in one surface with interleaved RGGB ordering. Out of 16 bits, 10 bits used 6 bits No-op. */
    lwdaEglColorFormatBayer10BGGR             = 50, /**< Bayer10 format - one channel in one surface with interleaved BGGR ordering. Out of 16 bits, 10 bits used 6 bits No-op. */
    lwdaEglColorFormatBayer10GRBG             = 51, /**< Bayer10 format - one channel in one surface with interleaved GRBG ordering. Out of 16 bits, 10 bits used 6 bits No-op. */
    lwdaEglColorFormatBayer10GBRG             = 52, /**< Bayer10 format - one channel in one surface with interleaved GBRG ordering. Out of 16 bits, 10 bits used 6 bits No-op. */
    lwdaEglColorFormatBayer12RGGB             = 53, /**< Bayer12 format - one channel in one surface with interleaved RGGB ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12BGGR             = 54, /**< Bayer12 format - one channel in one surface with interleaved BGGR ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12GRBG             = 55, /**< Bayer12 format - one channel in one surface with interleaved GRBG ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12GBRG             = 56, /**< Bayer12 format - one channel in one surface with interleaved GBRG ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer14RGGB             = 57, /**< Bayer14 format - one channel in one surface with interleaved RGGB ordering. Out of 16 bits, 14 bits used 2 bits No-op. */
    lwdaEglColorFormatBayer14BGGR             = 58, /**< Bayer14 format - one channel in one surface with interleaved BGGR ordering. Out of 16 bits, 14 bits used 2 bits No-op. */
    lwdaEglColorFormatBayer14GRBG             = 59, /**< Bayer14 format - one channel in one surface with interleaved GRBG ordering. Out of 16 bits, 14 bits used 2 bits No-op. */
    lwdaEglColorFormatBayer14GBRG             = 60, /**< Bayer14 format - one channel in one surface with interleaved GBRG ordering. Out of 16 bits, 14 bits used 2 bits No-op. */
    lwdaEglColorFormatBayer20RGGB             = 61, /**< Bayer20 format - one channel in one surface with interleaved RGGB ordering. Out of 32 bits, 20 bits used 12 bits No-op. */
    lwdaEglColorFormatBayer20BGGR             = 62, /**< Bayer20 format - one channel in one surface with interleaved BGGR ordering. Out of 32 bits, 20 bits used 12 bits No-op. */
    lwdaEglColorFormatBayer20GRBG             = 63, /**< Bayer20 format - one channel in one surface with interleaved GRBG ordering. Out of 32 bits, 20 bits used 12 bits No-op. */
    lwdaEglColorFormatBayer20GBRG             = 64, /**< Bayer20 format - one channel in one surface with interleaved GBRG ordering. Out of 32 bits, 20 bits used 12 bits No-op. */
    lwdaEglColorFormatYVU444Planar            = 65, /**< Y, V, U in three surfaces, each in a separate surface, U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU422Planar            = 66, /**< Y, V, U in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = Y height. */
    lwdaEglColorFormatYVU420Planar            = 67, /**< Y, V, U in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatBayerIspRGGB            = 68, /**< Lwpu proprietary Bayer ISP format - one channel in one surface with interleaved RGGB ordering and mapped to opaque integer datatype. */
    lwdaEglColorFormatBayerIspBGGR            = 69, /**< Lwpu proprietary Bayer ISP format - one channel in one surface with interleaved BGGR ordering and mapped to opaque integer datatype. */
    lwdaEglColorFormatBayerIspGRBG            = 70, /**< Lwpu proprietary Bayer ISP format - one channel in one surface with interleaved GRBG ordering and mapped to opaque integer datatype. */
    lwdaEglColorFormatBayerIspGBRG            = 71, /**< Lwpu proprietary Bayer ISP format - one channel in one surface with interleaved GBRG ordering and mapped to opaque integer datatype. */
    lwdaEglColorFormatBayerBCCR               = 72, /**< Bayer format - one channel in one surface with interleaved BCCR ordering. */
    lwdaEglColorFormatBayerRCCB               = 73, /**< Bayer format - one channel in one surface with interleaved RCCB ordering. */
    lwdaEglColorFormatBayerCRBC               = 74, /**< Bayer format - one channel in one surface with interleaved CRBC ordering. */
    lwdaEglColorFormatBayerCBRC               = 75, /**< Bayer format - one channel in one surface with interleaved CBRC ordering. */
    lwdaEglColorFormatBayer10CCCC             = 76, /**< Bayer10 format - one channel in one surface with interleaved CCCC ordering. Out of 16 bits, 10 bits used 6 bits No-op. */
    lwdaEglColorFormatBayer12BCCR             = 77, /**< Bayer12 format - one channel in one surface with interleaved BCCR ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12RCCB             = 78, /**< Bayer12 format - one channel in one surface with interleaved RCCB ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12CRBC             = 79, /**< Bayer12 format - one channel in one surface with interleaved CRBC ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12CBRC             = 80, /**< Bayer12 format - one channel in one surface with interleaved CBRC ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatBayer12CCCC             = 81, /**< Bayer12 format - one channel in one surface with interleaved CCCC ordering. Out of 16 bits, 12 bits used 4 bits No-op. */
    lwdaEglColorFormatY                       = 82, /**< Color format for single Y plane. */
    lwdaEglColorFormatYUV420SemiPlanar_2020   = 83, /**< Y, UV in two surfaces (UV as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYVU420SemiPlanar_2020   = 84, /**< Y, VU in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYUV420Planar_2020       = 85, /**< Y, U, V in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYVU420Planar_2020       = 86, /**< Y, V, U in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYUV420SemiPlanar_709    = 87, /**< Y, UV in two surfaces (UV as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYVU420SemiPlanar_709    = 88, /**< Y, VU in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYUV420Planar_709        = 89, /**< Y, U, V in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatYVU420Planar_709        = 90, /**< Y, V, U in three surfaces, each in a separate surface, U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY10V10U10_420SemiPlanar_709  = 91, /**< Y10, V10U10 in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY10V10U10_420SemiPlanar_2020 = 92, /**< Y10, V10U10 in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY10V10U10_422SemiPlanar_2020 = 93, /**< Y10, V10U10  in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height =  Y height. */
    lwdaEglColorFormatY10V10U10_422SemiPlanar      = 94, /**< Y10, V10U10  in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height =  Y height. */
    lwdaEglColorFormatY10V10U10_422SemiPlanar_709  = 95, /**< Y10, V10U10  in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height =  Y height. */
    lwdaEglColorFormatY_ER                         = 96, /**< Extended Range Color format for single Y plane. */
    lwdaEglColorFormatY_709_ER                     = 97, /**< Extended Range Color format for single Y plane. */
    lwdaEglColorFormatY10_ER                       = 98, /**< Extended Range Color format for single Y10 plane. */
    lwdaEglColorFormatY10_709_ER                   = 99, /**< Extended Range Color format for single Y10 plane. */
    lwdaEglColorFormatY12_ER                       = 100, /**< Extended Range Color format for single Y12 plane. */
    lwdaEglColorFormatY12_709_ER                   = 101, /**< Extended Range Color format for single Y12 plane. */
    lwdaEglColorFormatYUVA                         = 102, /**< Y, U, V, A four channels in one surface, interleaved as AVUY. */
    lwdaEglColorFormatYVYU                         = 104, /**< Y, U, V in one surface, interleaved as YVYU in one channel. */
    lwdaEglColorFormatVYUY                         = 105, /**< Y, U, V in one surface, interleaved as VYUY in one channel. */
    lwdaEglColorFormatY10V10U10_420SemiPlanar_ER     = 106, /**< Extended Range Y10, V10U10 in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY10V10U10_420SemiPlanar_709_ER = 107, /**< Extended Range Y10, V10U10 in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY10V10U10_444SemiPlanar_ER     = 108, /**< Extended Range Y10, V10U10 in two surfaces (VU as one surface) U/V width = Y width, U/V height = Y height. */ 
    lwdaEglColorFormatY10V10U10_444SemiPlanar_709_ER = 109, /**< Extended Range Y10, V10U10 in two surfaces (VU as one surface) U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatY12V12U12_420SemiPlanar_ER     = 110, /**< Extended Range Y12, V12U12 in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY12V12U12_420SemiPlanar_709_ER = 111, /**< Extended Range Y12, V12U12 in two surfaces (VU as one surface) U/V width = 1/2 Y width, U/V height = 1/2 Y height. */
    lwdaEglColorFormatY12V12U12_444SemiPlanar_ER     = 112, /**< Extended Range Y12, V12U12 in two surfaces (VU as one surface) U/V width = Y width, U/V height = Y height. */
    lwdaEglColorFormatY12V12U12_444SemiPlanar_709_ER = 113, /**< Extended Range Y12, V12U12 in two surfaces (VU as one surface) U/V width = Y width, U/V height = Y height. */
} lwdaEglColorFormat;

/**
 * LWCA EGL Plane Descriptor - structure defining each plane of a LWCA EGLFrame
 */
typedef struct lwdaEglPlaneDesc_st {
    unsigned int width;                         /**< Width of plane */
    unsigned int height;                        /**< Height of plane */
    unsigned int depth;                         /**< Depth of plane */
    unsigned int pitch;                         /**< Pitch of plane */
    unsigned int numChannels;                   /**< Number of channels for the plane */
    struct lwdaChannelFormatDesc channelDesc;   /**< Channel Format Descriptor */
    unsigned int reserved[4];                   /**< Reserved for future use */
} lwdaEglPlaneDesc;

/**
 * LWCA EGLFrame Descriptor - structure defining one frame of EGL.
 *
 * Each frame may contain one or more planes depending on whether the surface is Multiplanar or not.
 * Each plane of EGLFrame is represented by ::lwdaEglPlaneDesc which is defined as:
 * \code
 * typedef struct lwdaEglPlaneDesc_st {
 *     unsigned int width;
 *     unsigned int height;
 *     unsigned int depth;
 *     unsigned int pitch;
 *     unsigned int numChannels;
 *     struct lwdaChannelFormatDesc channelDesc;
 *     unsigned int reserved[4];
 * } lwdaEglPlaneDesc;
 * \endcode

*/
typedef struct lwdaEglFrame_st {
   union {
       lwdaArray_t            pArray[LWDA_EGL_MAX_PLANES];     /**< Array of LWCA arrays corresponding to each plane*/
       struct lwdaPitchedPtr  pPitch[LWDA_EGL_MAX_PLANES];     /**< Array of Pointers corresponding to each plane*/
   } frame;
   lwdaEglPlaneDesc planeDesc[LWDA_EGL_MAX_PLANES];     /**< LWCA EGL Plane Descriptor ::lwdaEglPlaneDesc*/
   unsigned int planeCount;                             /**< Number of planes */
   lwdaEglFrameType frameType;                          /**< Array or Pitch */
   lwdaEglColorFormat eglColorFormat;                   /**< LWCA EGL Color Format*/
} lwdaEglFrame;

/**
 * LWCA EGLSream Connection
 */
typedef struct  LWeglStreamConnection_st *lwdaEglStreamConnection;

/** @} */ /* END LWDART_TYPES */

/**
 * \addtogroup LWDART_EGL EGL Interoperability
 * This section describes the EGL interoperability functions of the LWCA
 * runtime application programming interface.
 *
 * @{
 */

/**
 * \brief Registers an EGL image
 *
 * Registers the EGLImageKHR specified by \p image for access by
 * LWCA. A handle to the registered object is returned as \p pLwdaResource.
 * Additional Mapping/Unmapping is not required for the registered resource and
 * ::lwdaGraphicsResourceGetMappedEglFrame can be directly called on the \p pLwdaResource.
 *
 * The application will be responsible for synchronizing access to shared objects.
 * The application must ensure that any pending operation which access the objects have completed
 * before passing control to LWCA. This may be accomplished by issuing and waiting for
 * glFinish command on all GLcontexts (for OpenGL and likewise for other APIs).
 * The application will be also responsible for ensuring that any pending operation on the
 * registered LWCA resource has completed prior to exelwting subsequent commands in other APIs
 * accesing the same memory objects.
 * This can be accomplished by calling lwCtxSynchronize or lwEventSynchronize (preferably).
 *
 * The surface's intended usage is specified using \p flags, as follows:
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
 * The EGLImageKHR is an object which can be used to create EGLImage target resource. It is defined as a void pointer.
 * typedef void* EGLImageKHR
 *
 * \param pLwdaResource   - Pointer to the returned object handle
 * \param image           - An EGLImageKHR image which can be used to create target resource.
 * \param flags           - Map flags
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidResourceHandle,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaGraphicsUnregisterResource,
 * ::lwdaGraphicsResourceGetMappedEglFrame,
 * ::lwGraphicsEGLRegisterImage
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsEGLRegisterImage(struct lwdaGraphicsResource **pLwdaResource, EGLImageKHR image, unsigned int flags);

/**
 * \brief Connect LWCA to EGLStream as a consumer.
 *
 * Connect LWCA as a consumer to EGLStreamKHR specified by \p eglStream.
 *
 * The EGLStreamKHR is an EGL object that transfers a sequence of image frames from one
 * API to another.
 *
 * \param conn              - Pointer to the returned connection handle
 * \param eglStream         - EGLStreamKHR handle
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamConsumerDisconnect,
 * ::lwdaEGLStreamConsumerAcquireFrame,
 * ::lwdaEGLStreamConsumerReleaseFrame,
 * ::lwEGLStreamConsumerConnect
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamConsumerConnect(lwdaEglStreamConnection *conn, EGLStreamKHR eglStream);

/**
 * \brief Connect LWCA to EGLStream as a consumer with given flags.
 *
 * Connect LWCA as a consumer to EGLStreamKHR specified by \p stream with specified \p flags defined by
 * ::lwdaEglResourceLocationFlags.
 *
 * The flags specify whether the consumer wants to access frames from system memory or video memory.
 * Default is ::lwdaEglResourceLocatiolwidmem.
 *
 * \param conn              - Pointer to the returned connection handle
 * \param eglStream         - EGLStreamKHR handle
 * \param flags             - Flags denote intended location - system or video.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamConsumerDisconnect,
 * ::lwdaEGLStreamConsumerAcquireFrame,
 * ::lwdaEGLStreamConsumerReleaseFrame,
 * ::lwEGLStreamConsumerConnectWithFlags
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamConsumerConnectWithFlags(lwdaEglStreamConnection *conn, EGLStreamKHR eglStream, unsigned int flags);

/**
 * \brief Disconnect LWCA as a consumer to EGLStream .
 *
 * Disconnect LWCA as a consumer to EGLStreamKHR.
 *
 * \param conn            - Conection to disconnect.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamConsumerConnect,
 * ::lwdaEGLStreamConsumerAcquireFrame,
 * ::lwdaEGLStreamConsumerReleaseFrame,
 * ::lwEGLStreamConsumerDisconnect
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamConsumerDisconnect(lwdaEglStreamConnection *conn);

/**
 * \brief Acquire an image frame from the EGLStream with LWCA as a consumer.
 *
 * Acquire an image frame from EGLStreamKHR.
 * ::lwdaGraphicsResourceGetMappedEglFrame can be called on \p pLwdaResource to get
 * ::lwdaEglFrame.
 *
 * \param conn            - Connection on which to acquire
 * \param pLwdaResource   - LWCA resource on which the EGLStream frame will be mapped for use.
 * \param pStream         - LWCA stream for synchronization and any data migrations
 * implied by ::lwdaEglResourceLocationFlags.
 * \param timeout         - Desired timeout in usec.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown,
 * ::lwdaErrorLaunchTimeout
 *
 * \sa
 * ::lwdaEGLStreamConsumerConnect,
 * ::lwdaEGLStreamConsumerDisconnect,
 * ::lwdaEGLStreamConsumerReleaseFrame,
 * ::lwEGLStreamConsumerAcquireFrame
 */

extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamConsumerAcquireFrame(lwdaEglStreamConnection *conn,
        lwdaGraphicsResource_t *pLwdaResource, lwdaStream_t *pStream, unsigned int timeout);
/**
 * \brief Releases the last frame acquired from the EGLStream.
 *
 * Release the acquired image frame specified by \p pLwdaResource to EGLStreamKHR.
 *
 * \param conn            - Connection on which to release
 * \param pLwdaResource   - LWCA resource whose corresponding frame is to be released
 * \param pStream         - LWCA stream on which release will be done.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamConsumerConnect,
 * ::lwdaEGLStreamConsumerDisconnect,
 * ::lwdaEGLStreamConsumerAcquireFrame,
 * ::lwEGLStreamConsumerReleaseFrame
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamConsumerReleaseFrame(lwdaEglStreamConnection *conn,
                                                  lwdaGraphicsResource_t pLwdaResource, lwdaStream_t *pStream);

/**
 * \brief Connect LWCA to EGLStream as a producer.
 *
 * Connect LWCA as a producer to EGLStreamKHR specified by \p stream.
 *
 * The EGLStreamKHR is an EGL object that transfers a sequence of image frames from one
 * API to another.
 *
 * \param conn   - Pointer to the returned connection handle
 * \param eglStream - EGLStreamKHR handle
 * \param width  - width of the image to be submitted to the stream
 * \param height - height of the image to be submitted to the stream
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamProducerDisconnect,
 * ::lwdaEGLStreamProducerPresentFrame,
 * ::lwdaEGLStreamProducerReturnFrame,
 * ::lwEGLStreamProducerConnect
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamProducerConnect(lwdaEglStreamConnection *conn,
                                                EGLStreamKHR eglStream, EGLint width, EGLint height);

/**
 * \brief Disconnect LWCA as a producer  to EGLStream .
 *
 * Disconnect LWCA as a producer to EGLStreamKHR.
 *
 * \param conn            - Conection to disconnect.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamProducerConnect,
 * ::lwdaEGLStreamProducerPresentFrame,
 * ::lwdaEGLStreamProducerReturnFrame,
 * ::lwEGLStreamProducerDisconnect
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamProducerDisconnect(lwdaEglStreamConnection *conn);

/**
 * \brief Present a LWCA eglFrame to the EGLStream with LWCA as a producer.
 *
 * The ::lwdaEglFrame is defined as:
 * \code
 * typedef struct lwdaEglFrame_st {
 *     union {
 *         lwdaArray_t            pArray[LWDA_EGL_MAX_PLANES];
 *         struct lwdaPitchedPtr  pPitch[LWDA_EGL_MAX_PLANES];
 *     } frame;
 *     lwdaEglPlaneDesc planeDesc[LWDA_EGL_MAX_PLANES];
 *     unsigned int planeCount;
 *     lwdaEglFrameType frameType;
 *     lwdaEglColorFormat eglColorFormat;
 * } lwdaEglFrame;
 * \endcode
 *
 * For ::lwdaEglFrame of type ::lwdaEglFrameTypePitch, the application may present sub-region of a memory
 * allocation. In that case, ::lwdaPitchedPtr::ptr will specify the start address of the sub-region in
 * the allocation and ::lwdaEglPlaneDesc will specify the dimensions of the sub-region.
 *
 * \param conn            - Connection on which to present the LWCA array
 * \param eglframe        - LWCA Eglstream Proucer Frame handle to be sent to the consumer over EglStream.
 * \param pStream         - LWCA stream on which to present the frame.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamProducerConnect,
 * ::lwdaEGLStreamProducerDisconnect,
 * ::lwdaEGLStreamProducerReturnFrame,
 * ::lwEGLStreamProducerPresentFrame
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamProducerPresentFrame(lwdaEglStreamConnection *conn,
                                                 lwdaEglFrame eglframe, lwdaStream_t *pStream);

/**
 * \brief Return the LWCA eglFrame to the EGLStream last released by the consumer.
 * 
 * This API can potentially return lwdaErrorLaunchTimeout if the consumer has not 
 * returned a frame to EGL stream. If timeout is returned the application can retry.
 *
 * \param conn            - Connection on which to present the LWCA array
 * \param eglframe        - LWCA Eglstream Proucer Frame handle returned from the consumer over EglStream.
 * \param pStream         - LWCA stream on which to return the frame.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorLaunchTimeout,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \sa
 * ::lwdaEGLStreamProducerConnect,
 * ::lwdaEGLStreamProducerDisconnect,
 * ::lwdaEGLStreamProducerPresentFrame,
 * ::lwEGLStreamProducerReturnFrame
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEGLStreamProducerReturnFrame(lwdaEglStreamConnection *conn,
                                                lwdaEglFrame *eglframe, lwdaStream_t *pStream);

/**
 * \brief Get an eglFrame through which to access a registered EGL graphics resource.
 *
 * Returns in \p *eglFrame an eglFrame pointer through which the registered graphics resource
 * \p resource may be accessed.
 * This API can only be called for EGL graphics resources.
 *
 * The ::lwdaEglFrame is defined as
 * \code
 * typedef struct lwdaEglFrame_st {
 *     union {
 *         lwdaArray_t             pArray[LWDA_EGL_MAX_PLANES];
 *         struct lwdaPitchedPtr   pPitch[LWDA_EGL_MAX_PLANES];
 *     } frame;
 *     lwdaEglPlaneDesc planeDesc[LWDA_EGL_MAX_PLANES];
 *     unsigned int planeCount;
 *     lwdaEglFrameType frameType;
 *     lwdaEglColorFormat eglColorFormat;
 * } lwdaEglFrame;
 * \endcode
 *
 *
 * \param eglFrame   - Returned eglFrame.
 * \param resource   - Registered resource to access.
 * \param index      - Index for lwbemap surfaces.
 * \param mipLevel   - Mipmap level for the subresource to access.
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorUnknown
 *
 * \note Note that in case of multiplanar \p *eglFrame, pitch of only first plane (unsigned int lwdaEglPlaneDesc::pitch) is to be considered by the application.
 *
 * \sa
 * ::lwdaGraphicsSubResourceGetMappedArray,
 * ::lwdaGraphicsResourceGetMappedPointer,
 * ::lwGraphicsResourceGetMappedEglFrame
 */
extern __host__ lwdaError_t LWDARTAPI lwdaGraphicsResourceGetMappedEglFrame(lwdaEglFrame* eglFrame,
                                        lwdaGraphicsResource_t resource, unsigned int index, unsigned int mipLevel);

/**
 * \brief Creates an event from EGLSync object
 *
 * Creates an event *phEvent from an EGLSyncKHR eglSync with the flages specified
 * via \p flags. Valid flags include:
 * - ::lwdaEventDefault: Default event creation flag.
 * - ::lwdaEventBlockingSync: Specifies that the created event should use blocking
 * synchronization.  A CPU thread that uses ::lwdaEventSynchronize() to wait on
 * an event created with this flag will block until the event has actually
 * been completed.
 *
 * ::lwdaEventRecord and TimingData are not supported for events created from EGLSync.
 *
 * The EGLSyncKHR is an opaque handle to an EGL sync object.
 * typedef void* EGLSyncKHR
 *
 * \param phEvent - Returns newly created event
 * \param eglSync - Opaque handle to EGLSync object
 * \param flags   - Event creation flags
 *
 * \return
 * ::lwdaSuccess,
 * ::lwdaErrorInitializationError,
 * ::lwdaErrorIlwalidValue,
 * ::lwdaErrorLaunchFailure,
 * ::lwdaErrorMemoryAllocation
 *
 * \sa
 * ::lwdaEventQuery,
 * ::lwdaEventSynchronize,
 * ::lwdaEventDestroy
 */
extern __host__ lwdaError_t LWDARTAPI lwdaEventCreateFromEGLSync(lwdaEvent_t *phEvent, EGLSyncKHR eglSync, unsigned int flags);

/** @} */ /* END LWDART_EGL */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* __LWDA_EGL_INTEROP_H__ */

