/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

/**
 *  @brief Status codes for Libos kernel syscalls and standard daemons.
 * 
 */
typedef enum __attribute__ ((__packed__)) {
  LibosOk              = 0u,

  LibosErrorArgument   = 1u,
  LibosErrorAccess     = 2u,
  LibosErrorTimeout    = 3u,
  LibosErrorIncomplete = 4u,
  LibosErrorFailed     = 5u,
} LibosStatus;