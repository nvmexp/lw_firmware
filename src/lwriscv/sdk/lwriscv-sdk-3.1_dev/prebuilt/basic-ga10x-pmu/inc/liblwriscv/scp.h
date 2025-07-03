/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_H
#define LIBLWRISCV_SCP_H

/*!
 * @file        scp.h
 * @brief       Interfaces for the Security Co-Processor (SCP).
 *
 * @details     This is the top-level header file for the liblwriscv SCP
 *              driver, which provides access to all supported SCP 
 *              functionality. For more information, please refer to the driver
 *              documentation at https://confluence.lwpu.com/x/9y6gJQ.
 *
 * @note        Client applications should include only this top-level file
 *              and should not directly include any other SCP headers.
 */

#if !LWRISCV_FEATURE_SCP
#error "This header cannot be used because the SCP driver is disabled."
#endif // !LWRISCV_FEATURE_SCP

#include <liblwriscv/scp/scp_common.h>
#include <liblwriscv/scp/scp_crypt.h>
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_general.h>
#include <liblwriscv/scp/scp_rand.h>

#endif // LIBLWRISCV_SCP_H
