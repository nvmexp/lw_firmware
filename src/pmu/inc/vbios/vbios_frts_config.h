/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief   Data structures for holding the configuration to access the VBIOS
 *          via the Firmware Runtime Security model
 */

#ifndef VBIOS_FRTS_CONFIG_H
#define VBIOS_FRTS_CONFIG_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @defgroup    VBIOS_FRTS_CONFIG_VERSION
 *
 * @brief   Defines the versions of FRTS that are supported.
 *
 * @details
 *      VBIOS_FRTS_CONFIG_VERSION_ILWALID
 *          FRTS is invalid. Generally set when FRTS has not been set up.
 *
 *      VBIOS_FRTS_CONFIG_VERSION_1
 *          Version 1.
 *
 * @note    @ref VBIOS_FRTS_CONFIG_VERSION_ILWALID should only be "necessary"
 *          temporarily. Right now, there are some pre-silicon system 
 *          configurations in which the VBIOS ucodes cannot be used to set up
 *          FRTS. Eventually, we will want to be able to support FRTS everywhere,
 *          but until that's possible, this can be used to skip FRTS
 *          functionality.
 *@{
 */
typedef LwU8 VBIOS_FRTS_CONFIG_VERSION;
#define VBIOS_FRTS_CONFIG_VERSION_ILWALID                                   (0U)
#define VBIOS_FRTS_CONFIG_VERSION_1                                         (1U)
/*!@}*/

/*!
 * @defgroup    VBIOS_FRTS_CONFIG_MEDIA_TYPE
 *
 * @brief   Defines the media types that can be used to access FRTS
 *
 * @details
 *      VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB
 *          FRTS region is in frame buffer memory
 *
 *      VBIOS_FRTS_CONFIG_MEDIA_TYPE_SYSMEM
 *          FTS region is in system memory
 *@{
 */
typedef LwU8 VBIOS_FRTS_CONFIG_MEDIA_TYPE;
#define VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB                                     (0U)
#define VBIOS_FRTS_CONFIG_MEDIA_TYPE_SYSMEM                                 (1U)
/*!@}*/

/*!
 * Represents the configuration needed to access FRTS, including the memory
 * aperture settings.
 */
typedef struct
{
    /*!
     * FRTS version
     */
    VBIOS_FRTS_CONFIG_VERSION version;

    /*!
     * Media type to be used to access FRTS
     */
    VBIOS_FRTS_CONFIG_MEDIA_TYPE mediaType;

    /*!
     * ID of the WPR in which FRTS resides; only necessary/valid if
     * @ref VBIOS_FRTS_CONFIG::mediaType is
     * @ref VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB
     */
    LwU8 wprId;

    /*!
     * Start offset of FRTS memory region.
     */
    LwU64 offset;

    /*!
     * Size of FRTS memory region.
     */
    LwU64 size;
} VBIOS_FRTS_CONFIG;

/*!
 * Structure to contain all of the metadata about locations of data in the
 * Firmware Runtime Security region.
 *
 * This is essentially a version-agnostic variation on @ref FW_IMAGE_DESCRIPTOR
 */
typedef struct
{
    /*!
     * Offset of the VDPA entry array in FRTS region
     */
    LwU64 vdpaEntryOffset;

    /*!
     * Count of entries in the VDPA array 
     */
    LwU64 vdpaEntryCount;

    /*!
     * Size of each entry within VDPA array
     */
    LwU64 vdpaEntrySize;

    /*!
     * Offset of the VBIOS image within FRTS region
     */
    LwU64 imageOffset;

    /*!
     * Size of the VBIOS image within FRTS
     */
    LwU64 imageSize;
} VBIOS_FRTS_METADATA;

/* ------------------------ Public Functions -------------------------------- */
#endif // VBIOS_FRTS_CONFIG_H
