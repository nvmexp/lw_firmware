/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SEC2_OBJSEC2_H
#define SEC2_OBJSEC2_H

/*!
 * @file   sec2_objsec2.h
 * @brief  Channel management task that is responsible for managing the
 *         work coming for channels from host method.
 */
/* ------------------------- System Includes -------------------------------- */


#include <syslib/syslib.h>
#include <shlib/syscall.h>
/*!
 * Defines polling modes for sec2PollReg32Ns.
 */
#define LW_SEC2_REG_POLL_REG_TYPE                                           0:0
#define LW_SEC2_REG_POLL_REG_TYPE_CSB                                       (0)
#define LW_SEC2_REG_POLL_REG_TYPE_BAR0                                      (1)
#define LW_SEC2_REG_POLL_OP                                                 1:1
#define LW_SEC2_REG_POLL_OP_EQ                                              (0)
#define LW_SEC2_REG_POLL_OP_NEQ                                             (1)

#define SEC2_REG_POLL_MODE_CSB_EQ   (DRF_DEF(_SEC2, _REG_POLL, _REG_TYPE, _CSB)|\
                                     DRF_DEF(_SEC2, _REG_POLL, _OP, _EQ))
#define SEC2_REG_POLL_MODE_CSB_NEQ  (DRF_DEF(_SEC2, _REG_POLL, _REG_TYPE, _CSB)|\
                                     DRF_DEF(_SEC2, _REG_POLL, _OP, _NEQ))
#define SEC2_REG_POLL_MODE_BAR0_EQ  (DRF_DEF(_SEC2, _REG_POLL, _REG_TYPE, _BAR0)|\
                                     DRF_DEF(_SEC2, _REG_POLL, _OP, _EQ))
#define SEC2_REG_POLL_MODE_BAR0_NEQ (DRF_DEF(_SEC2, _REG_POLL, _REG_TYPE, _BAR0)|\
                                     DRF_DEF(_SEC2, _REG_POLL, _OP, _NEQ))

#define SEC2_REG32_POLL_US(a,k,v,t,m)    sec2PollReg32Ns(a,k,v,t * 1000,m)

sysSYSLIB_CODE LwBool  sec2PollReg32Ns(LwU32 addr, LwU32 mask, LwU32 val, LwU32 timeoutNs, LwU32 mode);

#endif
