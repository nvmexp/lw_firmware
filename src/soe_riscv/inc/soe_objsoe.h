/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SOE_OBJSOE_H
#define SOE_OBJSOE_H

/*!
 * @file   soe_objsoe.h
 * @brief  Channel management task that is responsible for managing the
 *         work coming for channels from host method.
 */
/* ------------------------- System Includes -------------------------------- */


#include <syslib/syslib.h>
#include <shlib/syscall.h>

#include "rmsoecmdif.h"
#include "rmflcncmdif_lwswitch.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_soe_hal.h"
#include "config/soe-config.h"
/* ------------------------ Defines ---------------------------------------- */

/*!
 * Defines polling modes for soePollReg32Ns.
 */
#define LW_SOE_REG_POLL_REG_TYPE                                           0:0
#define LW_SOE_REG_POLL_REG_TYPE_CSB                                       (0)
#define LW_SOE_REG_POLL_REG_TYPE_BAR0                                      (1)
#define LW_SOE_REG_POLL_OP                                                 1:1
#define LW_SOE_REG_POLL_OP_EQ                                              (0)
#define LW_SOE_REG_POLL_OP_NEQ                                             (1)

#define SOE_REG_POLL_MODE_CSB_EQ   (DRF_DEF(_SOE, _REG_POLL, _REG_TYPE, _CSB)|\
                                     DRF_DEF(_SOE, _REG_POLL, _OP, _EQ))
#define SOE_REG_POLL_MODE_CSB_NEQ  (DRF_DEF(_SOE, _REG_POLL, _REG_TYPE, _CSB)|\
                                     DRF_DEF(_SOE, _REG_POLL, _OP, _NEQ))
#define SOE_REG_POLL_MODE_BAR0_EQ  (DRF_DEF(_SOE, _REG_POLL, _REG_TYPE, _BAR0)|\
                                     DRF_DEF(_SOE, _REG_POLL, _OP, _EQ))
#define SOE_REG_POLL_MODE_BAR0_NEQ (DRF_DEF(_SOE, _REG_POLL, _REG_TYPE, _BAR0)|\
                                     DRF_DEF(_SOE, _REG_POLL, _OP, _NEQ))

#define SOE_REG32_POLL_US(a,k,v,t,m)    soePollReg32Ns(a,k,v,t * 1000,m)

sysSYSLIB_CODE LwBool  soePollReg32Ns(LwU32 addr, LwU32 mask, LwU32 val, LwU32 timeoutNs, LwU32 mode);

#endif // SOE_OBJSOE_H
