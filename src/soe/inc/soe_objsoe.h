/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJSOE_H
#define SOE_OBJSOE_H

/* ------------------------ System includes -------------------------------- */
#include "rmsoecmdif.h"
#include "rmflcncmdif_lwswitch.h"

/* ------------------------ Application includes --------------------------- */
#include "soe_gpuarch.h"

#include "config/g_soe_hal.h"
#include "config/soe-config.h"

/* ------------------------ Defines ---------------------------------------- */

/*!
 * Wrapper macro for soePollReg32Ns
 */
#define SOE_REG32_POLL_NS(a,k,v,t,m)    soePollReg32Ns(a,k,v,t,m)
#define SOE_REG32_POLL_US(a,k,v,t,m)    soePollReg32Ns(a,k,v,t * 1000,m)

/*!
 * Defines polling modes for soePollReg32Ns.
 */
#define LW_SOE_REG_POLL_REG_TYPE                                          0:0
#define LW_SOE_REG_POLL_REG_TYPE_CSB                                       (0)
#define LW_SOE_REG_POLL_REG_TYPE_BAR0                                      (1)
#define LW_SOE_REG_POLL_OP                                                1:1
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


/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/*!
 * SOE object Definition
 */
typedef struct
{
    SOE_HAL_IFACES hal;
    SOE_CHIP_INFO  chipInfo;
    LwU8            flcnPrivLevelExtDefault;
    LwU8            flcnPrivLevelCsbDefault;
    LwBool          bDebugMode;
} OBJSOE;

extern OBJSOE Soe;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void        constructSoe(void)
    GCC_ATTRIB_SECTION("imem_init", "constructSoe");
void        soeStartOsTicksTimer(void)
    GCC_ATTRIB_SECTION("imem_init", "soeStartOsTicksTimer");
LwBool      soePollReg32Ns(LwU32 addr, LwU32 mask, LwU32 val, LwU32 timeoutNs, LwU32 mode)
    GCC_ATTRIB_SECTION("imem_resident", "soePollReg32Ns");
FLCN_STATUS soeCheckFuseRevocationHS(void)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "soeCheckFuseRevocationHS");
void        libInterfaceRegWr32(LwU32 addr, LwU32  data)
    GCC_ATTRIB_SECTION("imem_resident", "libIntfcFlcnRegWr32");
LwU32       libInterfaceRegRd32(LwU32 addr)
    GCC_ATTRIB_SECTION("imem_resident", "libIntfcFlcnRegRd32");
void        libInterfaceRegWr32Hs(LwU32 addr, LwU32  data)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "libIntfcFlcnRegWr32Hs");
LwU32       libInterfaceRegRd32Hs(LwU32 addr)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "libIntfcFlcnRegRd32Hs");

/* ------------------------ Misc macro definitions ------------------------- */

#endif // SOE_OBJSOE_H
