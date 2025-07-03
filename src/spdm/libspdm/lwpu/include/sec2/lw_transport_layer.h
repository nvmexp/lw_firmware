/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored but uses libspdm code.

/*!
 * @file   lw_transport_layer.h
 * @brief  File that provides function declarations for libspdm on SEC2
 *         transport layer functionality.
 */

#ifndef _LW_TRANSPORT_LAYER_H_
#define _LW_TRANSPORT_LAYER_H_

/* ------------------------- System Includes -------------------------------- */
#include "base.h"

/* ------------------------- Prototypes ------------------------------------- */
/**
  Initialize libspdm payload buffer information with location and size
  of EMEM payload buffer, shared between RM and SEC2.

  @note Must be called before libspdm can process messages, the behavior
  is undefined otherwise.

  @param buffer_addr  Address of payload buffer.
  @param buffer_size  Size of payload buffer, in bytes.

  @retval boolean signifying success of operation.
**/
boolean spdm_transport_layer_initialize(IN uint8 *buffer_addr,
    IN uint32 buffer_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_transport_layer_initialize");

#endif // _LW_TRANSPORT_LAYER_H_
