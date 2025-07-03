/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwosreg.c
 * @copydoc lwosreg.h
 */

/* ------------------------ System includes --------------------------------- */
#include "lwosreg.h"
#include "csb.h"

#if defined(PMU_RTOS)
    #include "pmu_bar0.h"
#elif defined(SEC2_RTOS)
    #include "sec2_bar0_hs.h"
    #include "secbus_hs.h"
#elif defined(SOE_RTOS)
    #include "soe_bar0.h"
#elif defined(GSPLITE_RTOS) && !defined(GSP_RTOS)
    #include "flcntypes.h"
    #include "gsplite_regmacros.h"
#elif defined(GSP_RTOS)
    #include "gsp_bar0.h"
#elif defined(DPU_RTOS)
    //
    // DPU only supports CSB access, thus including csb.h is enough, so no more
    // header is needed here.
    //
#else
    #error !!! Unsupported falcon !!!
#endif

#if defined(UPROC_RISCV)
    #include "syslib/syslib.h"
#endif

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Type definitions -------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Prototypes -------------------------------------- */
/* ------------------------ Global variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Read a register via the requested bus
 *
 * @param[in]  bus    Which bus we will use to read the register
 * @param[in]  addr   Address of the register
 *
 * @return the value which was read
 * @return LW_U32_MAX if the bus is not supported
 */
LwU32
osRegRd32
(
    LwU8  bus,
    LwU32 addr
)
{
    if (bus == REG_BUS_CSB)
    {
        return REG_RD32(CSB, addr);
    }
#if defined(REG_BUS_BAR0) && (defined(PMU_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS) || defined(GSPLITE_RTOS) || defined(GSP_RTOS))
    // BAR0 access is supported by PMU, SEC2, GSPLite, and GSP
    else if (bus == REG_BUS_BAR0)
    {
#if defined(PMU_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS) || defined(GSP_RTOS)
        return REG_RD32(BAR0, addr);
#elif defined(GSPLITE_RTOS)
        return EXT_REG_RD32(addr);
#endif
    }
#endif
#if defined(PMU_RTOS)
    // FECS access is only supported by PMU
    else if (bus == REG_BUS_FECS)
    {
        return REG_RD32(FECS, addr);
    }
#endif
    else
    {
        return LW_U32_MAX;
    }
}

/*!
 * @brief Write the register via the requested bus
 *
 * @param[in]  bus    Which bus we will use to write the register
 * @param[in]  addr   Address of the register
 * @param[in]  val    Value to be written to the register
 */
void
osRegWr32
(
    LwU8  bus,
    LwU32 addr,
    LwU32 val
)
{
    if (bus == REG_BUS_CSB)
    {
        REG_WR32(CSB, addr, val);
    }
#if defined(REG_BUS_BAR0) && (defined(PMU_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS) || defined(GSPLITE_RTOS) || defined(GSP_RTOS))
    // BAR0 access is supported by PMU, SEC2, SOE, GSPLite, and GSP
    else if (bus == REG_BUS_BAR0)
    {
#if defined(PMU_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS) || defined(GSP_RTOS)
        REG_WR32(BAR0, addr, val);
#elif defined(GSPLITE_RTOS)
        EXT_REG_WR32(addr, val);
#endif
    }
#endif
#if defined(PMU_RTOS)
    // FECS access is only supported by PMU
    else if (bus == REG_BUS_FECS)
    {
        REG_WR32(FECS, addr, val);
    }
#endif
    else
    {
        // This is here to appease MISRA
        return;
    }
}

#if defined(SEC2_RTOS) && !defined(UPROC_RISCV)
/*!
 * @brief The HS lib to read the register via the requested bus
 *
 * @param[in]  bus    Which bus we will use to read the register
 * @param[in]  addr   Address of the register
 *
 * @return the value of the read register
 * @return LW_U32_MAX if the bus is not supported
 */
LwU32
osRegRd32_Hs
(
    LwU8  bus,
    LwU32 addr
)
{
    if (bus == REG_BUS_CSB)
    {
        return REG_RD32_STALL(CSB, addr);
    }
#ifdef REG_BUS_BAR0
    else if (bus == REG_BUS_BAR0)
    {
        return BAR0_REG_RD32_HS(addr);
    }
#endif // REG_BUG_BAR0
    else if (bus == REG_BUS_SE)
    {
        LwU32 data32 = LW_U32_MAX;
        SE_SECBUS_REG_RD32_HS_ERRCHK(addr, &data32);
        return data32;
    }

    return LW_U32_MAX;
}

/*!
 * @brief The HS lib to write the register via the requested bus
 *
 * @param[in]  bus    Which bus we will use to write the register
 * @param[in]  addr   Address of the register
 * @param[in]  val    Value to be written to the register
 */
void
osRegWr32_Hs
(
    LwU8  bus,
    LwU32 addr,
    LwU32 val
)
{
    if (bus == REG_BUS_CSB)
    {
        REG_WR32_STALL(CSB, addr, val);
    }
#ifdef REG_BUS_BAR0
    else if (bus == REG_BUS_BAR0)
    {
        BAR0_REG_WR32_HS(addr, val);
    }
#endif // REG_BUS_BAR0
    else if (bus == REG_BUS_SE)
    {
        SE_SECBUS_REG_WR32_HS_ERRCHK(addr, val);
    }
}
#endif

/* ------------------------ Static Functions -------------------------------- */

