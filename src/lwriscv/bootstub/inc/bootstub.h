/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTSTUB_H
#define BOOTSTUB_H

/*!
 * @file   bootstub.h
 * @brief  Bootstub miscellaneous header.
 */

#ifndef NULL
#define NULL ((void*)0)
#endif

#define BOOTSTUB_ERROR_BAD_BOOTARGS       (0xBADB007A)
#define BOOTSTUB_ERROR_BAD_FUSING         (0xBADF5E)
#define BOOTSTUB_ERROR_EXCEPTION          (0xBADE)
#define BOOTSTUB_ERROR_FOOTPRINT_MISMATCH (0xBADF007)
#define BOOTSTUB_ERROR_HARDWARE_CHECK     (0xBADB175)
#define BOOTSTUB_ERROR_LOAD_FAILURE       (0xBAD10AD)
#define BOOTSTUB_ERROR_STACK_CORRUPTION   (0xBAD57ACC)

#endif // BOOTSTUB_H
