/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJSEC2_H
#define SEC2_OBJSEC2_H

/* ------------------------ System includes -------------------------------- */
#include "rmsec2cmdif.h"
#include "rmflcncmdif.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_bar0.h"
#include "sec2_gpuarch.h"

#include "config/g_sec2_hal.h"
#include "config/sec2-config.h"

/* ------------------------ Defines ---------------------------------------- */

/*!
 * Wrapper macro for sec2PollReg32Ns
 */
#define SEC2_REG32_POLL_NS(a,k,v,t,m)    sec2PollReg32Ns(a,k,v,t,m)
#define SEC2_REG32_POLL_US(a,k,v,t,m)    sec2PollReg32Ns(a,k,v,t * 1000,m)

/*!
 * Defines polling modes for sec2PollReg32Ns.
 */
#define LW_SEC2_REG_POLL_REG_TYPE                                          0:0
#define LW_SEC2_REG_POLL_REG_TYPE_CSB                                       (0)
#define LW_SEC2_REG_POLL_REG_TYPE_BAR0                                      (1)
#define LW_SEC2_REG_POLL_OP                                                1:1
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


/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/*!
 * SEC2 object Definition
 */
typedef struct
{
    SEC2_HAL_IFACES hal;
    SEC2_CHIP_INFO  chipInfo;
    LwU8            flcnPrivLevelExtDefault;
    LwU8            flcnPrivLevelCsbDefault;
    LwBool          bDebugMode;
} OBJSEC2;

extern OBJSEC2 Sec2;

extern LwrtosSemaphoreHandle GptimerLibMutex;
extern LwBool                GptimerLibEnabled;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void        sec2GptmrLibPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "sec2GptmrLibPreInit");
void        sec2StartOsTicksTimer(void)
    GCC_ATTRIB_SECTION("imem_init", "sec2StartOsTicksTimer");
LwBool      sec2PollReg32Ns(LwU32 addr, LwU32 mask, LwU32 val, LwU32 timeoutNs, LwU32 mode)
    GCC_ATTRIB_SECTION("imem_resident", "sec2PollReg32Ns");
FLCN_STATUS sec2CheckFuseRevocationHS(void)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "sec2CheckFuseRevocationHS");
FLCN_STATUS sec2CheckForLSRevocation(void)
    GCC_ATTRIB_SECTION("imem_init", "sec2CheckForLSRevocation");

/* ------------------------ Misc macro definitions ------------------------- */

#endif // SEC2_OBJSEC2_H
