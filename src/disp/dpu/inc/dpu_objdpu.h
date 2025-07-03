/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_OBJDPU_H
#define DPU_OBJDPU_H

/* ------------------------ System includes -------------------------------- */
#include "lwdpu.h"
#include "rmflcncmdif.h"
#include "rmdpucmdif.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_dpu_hal.h"
#include "config/dpu-config.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/*!
 * DPU Object definition
 */
typedef struct
{
    DPU_HAL_IFACES hal;
} OBJDPU;

/* ------------------------ External Definitions --------------------------- */
extern OBJDPU Dpu;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void   constructDpu(void)
    GCC_ATTRIB_SECTION("imem_init", "constructDpu");
FLCN_STATUS dpuStartOsTicksTimer(void)
    GCC_ATTRIB_SECTION("imem_init", "dpuStartOsTicksTimer");
LwU32 libInterfaceRegRd32(LwU32 addr)
    GCC_ATTRIB_SECTION("imem_resident", "libInterfaceRegRd32");
FLCN_STATUS libInterfaceRegRd32ErrChk(LwU32 addr, LwU32 *pVal)
    GCC_ATTRIB_SECTION("imem_resident", "libInterfaceRegRd32ErrChk");
FLCN_STATUS libInterfaceRegWr32ErrChk(LwU32 addr, LwU32  data)
    GCC_ATTRIB_SECTION("imem_resident", "libInterfaceRegWr32ErrChk");
FLCN_STATUS libInterfaceRegRd32ErrChkHs(LwU32 addr, LwU32 *pVal)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "libInterfaceRegRd32ErrChkHs");
FLCN_STATUS libInterfaceRegWr32ErrChkHs(LwU32 addr, LwU32  data)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "libInterfaceRegWr32ErrChkHs");
FLCN_STATUS libInterfaceCsbRegRd32ErrChk(LwU32 addr, LwU32 *pVal)
    GCC_ATTRIB_SECTION("imem_resident", "libInterfaceCsbRegRd32ErrChk");
FLCN_STATUS libInterfaceCsbRegWr32ErrChk(LwU32 addr, LwU32  data)
    GCC_ATTRIB_SECTION("imem_resident", "libInterfaceCsbRegWr32ErrChk");
FLCN_STATUS libInterfaceCsbRegRd32ErrChkHs(LwU32 addr, LwU32 *pVal)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "libInterfaceCsbRegRd32ErrChkHs");
FLCN_STATUS libInterfaceCsbRegWr32ErrChkHs(LwU32 addr, LwU32  data)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "libInterfaceCsbRegWr32ErrChkHs");

/* ------------------------ Misc macro definitions ------------------------- */
#endif // DPU_OBJDPU_H
