/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/*!
 * @file   bootloader.h
 * @brief  Bootloader miscellaneous header.
 */

#include <lwtypes.h>
#include <lwmisc.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define BOOTLOADER_ERROR_BAD_ELF            (0xBADE1F)
#define BOOTLOADER_ERROR_LOAD_FAILURE       (0xBAD10AD)
#define BOOTLOADER_ERROR_BAD_BOOTARGS       (0xBADB007A)
#define BOOTLOADER_ERROR_STACK_CORRUPTION   (0xBAD57ACC)
#define BOOTLOADER_ERROR_BAD_FUSING         (0xBADF5E)

#endif // BOOTLOADER_H
