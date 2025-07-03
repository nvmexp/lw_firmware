/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RMFLCNBL_H
#define RMFLCNBL_H

/*!
 * @file    rmflcnbl.h
 * @brief   Data structures and interfaces used for generic falcon boot-loader.
 *
 * @details This generic boot-loader is designed to load both non-secure and
 *          secure code taking care of signature as well. This bootloader
 *          should be loaded at the end of the IMEM so that it doesnt overwrite
 *          itself when it tries to load the code into IMEM starting at blk 0.
 *          The data will be loaded into DMEM offset 0.
 */

#include "flcnifcmn.h"

/*!
 * @brief Structure used by the boot-loader to load the rest of the code.
 *
 * This has to be filled by the GPU driver and copied into DMEM at offset
 * provided in the @ref _def_rm_flcn_bl_desc.blDmemDescLoadOff.
 */
typedef struct def_rm_flcn_bl_dmem_desc
{
    /*!
     * reserved should be always first element
     */
    LwU32       reserved[4];
    /*!
     * signature should follow reserved 16B signature for secure code.
     * 0s if no secure code
     */
    LwU32       signature[4];
    /*!
     * ctxDma is used by the bootloader while loading code/data.
     */
    LwU32       ctxDma;
    /*!
     * 256B aligned physical FB address where code is located.
     */
    RM_FLCN_U64 codeDmaBase;
    /*!
     * Offset from codeDmaBase where the nonSelwre code is located.
     * The offset must be multiple of 256 to help performance.
     */
    LwU32       nonSelwreCodeOff;
    /*!
     * The size of the nonSelwre code part.
     */
    LwU32       nonSelwreCodeSize;
    /*!
     * Offset from codeDmaBase where the secure code is located.
     * The offset must be multiple of 256 to help performance.
     */
    LwU32       selwreCodeOff;
    /*!
     * The size of the elwre code part.
     */
    LwU32       selwreCodeSize;
    /*!
     * Code entry point which will be ilwoked by BL after code is loaded.
     */
    LwU32       codeEntryPoint;
    /*!
     * 256B aligned Physical FB Address where data is located.
     */
    RM_FLCN_U64 dataDmaBase;
    /*!
     * Size of data block. Should be multiple of 256B.
     */
    LwU32       dataSize;
    /*!
     * Arguments to be passed to the target firmware being loaded.
     */
    LwU32       argc;
    /*!
     * Number of arguments to be passed to the target firmware being loaded.
     */
    LwU32       argv;
} RM_FLCN_BL_DMEM_DESC, *PRM_FLCN_BL_DMEM_DESC;

/*!
 * @brief The header used by the GPU driver to figure out code and data
 * sections of bootloader.
 */
typedef struct def_rm_flcn_bl_img_header
{
    /*!
     * Offset of code section in the image.
     */
    LwU32 blCodeOffset;
    /*!
     * Size of code section in the image.
     */
    LwU32 blCodeSize;
    /*!
     * Offset of data section in the image.
     */
    LwU32 blDataOffset;
    /*!
     * Size of data section in the image.
     */
    LwU32 blDataSize;
} RM_FLCN_BL_IMG_HEADER, *PRM_FLCN_BL_IMG_HEADER;

/*!
 * @brief The descriptor used by RM to figure out the requirements of boot loader.
 */
typedef struct def_rm_flcn_bl_desc
{
    /*!
     *  Starting tag of bootloader
     */
    LwU32 blStartTag;
    /*!
     *  Dmem offset where _def_rm_flcn_bl_dmem_desc to be loaded
     */
    LwU32 blDmemDescLoadOff;
    /*!
     *  Description of the image
     */
    RM_FLCN_BL_IMG_HEADER blImgHeader;
} RM_FLCN_BL_DESC, *PRM_FLCN_BL_DESC;

/*!
 * @brief Legacy structure used by the current PMU/DPU bootloader.
 */
typedef struct def_loader_config
{
    /*!
     *  Should always be first (used by ACR HS ucode to mitigate an attack)
     */
    LwU32       reserved;
    LwU32       dmaIdx;
    RM_FLCN_U64 codeDmaBase;
    LwU32       codeSizeTotal;
    LwU32       codeSizeToLoad;
    LwU32       codeEntryPoint;
    RM_FLCN_U64 dataDmaBase;
    LwU32       dataSize;
    RM_FLCN_U64 overlayDmaBase;
    LwU32       argc;
    LwU32       argv;
} LOADER_CONFIG, *PLOADER_CONFIG;

/*!
 * @brief Union of all supported structures used by bootloaders.
 */
typedef union def_rm_flcn_bl_generic_desc
{
    RM_FLCN_BL_DMEM_DESC flcnBlDmemDesc;
    LOADER_CONFIG loaderConfig;
} RM_FLCN_BL_GENERIC_DESC, *PRM_FLCN_BL_GENERIC_DESC;

#endif // RMFLCNBL_H
