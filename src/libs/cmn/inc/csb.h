/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CSB_H
#define CSB_H

/*!
 * @file csb.h
 */

#include "regmacros.h"

/*!
 * @brief Read a 32-bit register on the CSB bus
 * 
 * @param[in]  addr    The address to access
 * 
 * @return The value which was read
 */
#define CSB_REG_RD32(addr)                    lwuc_ldx_i ((LwU32*)(LwUPtr)(addr), 0)

/*!
 * @brief write a 32-bit register on the CSB bus
 * 
 * @param[in]  addr    The address to access
 * @param[in]  data    The value to write
 */
#define CSB_REG_WR32(addr, data)              lwuc_stx_i ((LwU32*)(LwUPtr)(addr), 0, data)

/*!
 * @brief Do a stalling read of a 32-bit register on the CSB bus
 * 
 * @param[in]  addr    The address to access
 * 
 * @return The value which was read
 */
#define CSB_REG_RD32_STALL(addr)              lwuc_ldxb_i((LwU32*)(LwUPtr)(addr), 0)

/*!
 * @brief Do a stalling write of a 32-bit register on the CSB bus
 * 
 * @param[in]  addr    The address to access
 * @param[in]  data    The value to write
 */
#define CSB_REG_WR32_STALL(addr, data)        lwuc_stxb_i((LwU32*)(LwUPtr)(addr), 0, data)

#endif // CSB_H

