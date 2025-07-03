/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SCP_COMMON_H
#define SCP_COMMON_H

/* ------------------------ System Includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ Defines ----------------------------------------- */

/*!
 * AES block size used by SCP
 */
#define SCP_AES_SIZE_IN_BITS      128
#define SCP_AES_SIZE_IN_BYTES     (SCP_AES_SIZE_IN_BITS / 8)

/*!
 * Minimum alignment for buffers used by SCP engine
 */
#define SCP_BUF_ALIGNMENT         16

/*!
 * @brief   Clears all SCP general-purpose registers
 */
void scpRegistersClearAll(void)
    GCC_ATTRIB_SECTION("imem_libScpCommon", "scpRegistersClearAll");

#ifdef UPROC_RISCV

/*!
 * @brief A function to poll on the SCPDMA state. Used to wait for SCP to go idle.
 *
 * @param[in]  pArgs  Ignored parameter to satisfy osptimer interface.
 *
 * @return LW_TRUE  - when SCP DMA is idle
 * @return LW_FALSE - otherwise
 */
LwBool scpDmaPollIsIdle(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libScpCommon", "scpDmaPollIsIdle");

#endif // UPROC_RISCV

#endif // SCP_COMMON_H
