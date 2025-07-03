/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_DEVICEMAP_H
#define LIBLWRISCV_DEVICEMAP_H

/*!
 * @file        devicemap.h
 * @brief       Interfaces for the Peregrine Device Map.
 */

// SDK includes.
#include <lwmisc.h>

// LIBLWRISCV includes.
#include <liblwriscv/io.h>


/*!
 * @brief Checks access to a given device-map group.
 *
 * @param[in] mode  The operating mode to check (e.g. MMODE).
 * @param[in] group The target device-map group (e.g. SCP).
 * @param[in] type  The desired access type (e.g. READ).
 *
 * @return
 *    true  if access is permitted.
 *    false if access is denied.
 *
 * Checks whether the given operating mode (MMODE / SUBMMODE) has the
 * requested access rights (READ / WRITE) to the specified device-map group.
 */
#define DEVICEMAP_HAS_ACCESS(mode, group, type) (                           \
    FLD_IDX_TEST_DRF(_PRGNLCL_RISCV, _DEVICEMAP_RISCV##mode, _##type,       \
        LW_PRGNLCL_DEVICE_MAP_GROUP_##group, _ENABLE,                       \
        localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCV##mode(                   \
            LW_PRGNLCL_DEVICE_MAP_GROUP_##group /                           \
            LW_PRGNLCL_RISCV_DEVICEMAP_RISCV##mode##_##type##__SIZE_1)))    \
)

#endif // LIBLWRISCV_DEVICEMAP_H
