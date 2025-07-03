/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSREG_H
#define LWOSREG_H

/*!
 * @file    lwosreg.h
 *
 * This library helps access the HW register when the bus used to access the
 * register can only be determined at runtime. The caller, at runtime, needs to
 * establish the mapping regarding what bus needs to be used to access a
 * register. When the caller wants to access a register, it calls this library
 * according to the mapping it maintains, then this library will help access the
 * register according to the bus type requested by the caller.
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "lwuproc.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief A register access on the CSB bus
 */
#define REG_BUS_CSB  0u
/*!
 * @brief A register access on the BAR0 bus
 */
#if !USE_CBB
#define REG_BUS_BAR0 1u
#endif // !USE_CBB
/*!
 * @brief A register access on the FECS bus
 */
#define REG_BUS_FECS 2u
/*!
 * @brief A register access on the SE bus (Only used on HS Falcons).
 */
#define REG_BUS_SE   3u

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
LwU32 osRegRd32(LwU8 bus, LwU32 addr)
    GCC_ATTRIB_SECTION("imem_resident", "osRegRd32");
void  osRegWr32(LwU8 bus, LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_resident", "osRegWr32");
#if defined(SEC2_RTOS)
LwU32 osRegRd32_Hs(LwU8 bus, LwU32 addr)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "osRegRd32_Hs");
void  osRegWr32_Hs(LwU8 bus, LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "osRegWr32_Hs");
#endif
/* ------------------------ Inline Functions -------------------------------- */

#endif // LWOSREG_H
