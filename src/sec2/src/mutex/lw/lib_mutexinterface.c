/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  lib_mutexinterface.c
 * @brief Top-Level interfaces between the SEC2 app layer and the HW Mutex
 *        library
 *
 * This module is intended to serve as the top-level interface layer for dealing
 * with all application-specific aspects of the Libs.
 *
 * Note: All data-types in this module should be consistent with those used
 *       in the libs.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lw_mutex.h"
#include "sec2_bar0_hs.h"
#include "sec2_csb.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "secbus_hs.h"
#include "sec2_objsec2.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
LwU32       mutexRegRd32(LwU8 bus, LwU32 addr)
    GCC_ATTRIB_SECTION("imem_resident", "mutexRegRd32");
void        mutexRegWr32(LwU8 bus, LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_resident", "mutexRegWr32");
FLCN_STATUS mutexRegRd32_hs(LwU8 bus, LwU32 addr, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "mutexRegRd32_hs");
FLCN_STATUS mutexRegWr32_hs(LwU8 bus, LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "mutexRegWr32_hs");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief The lib interface to call the real register read accessor in SEC2
 *
 * @param[in]  bus    Which bus we will use to read the register
 * @param[in]  addr   Address of the register
 *
 * @return the value of the read register
 *         LW_U32_MAX if the bus is not supported
 */
LwU32
mutexRegRd32
(
    LwU8  bus,
    LwU32 addr
)
{
    if (bus == MUTEX_BUS_CSB)
    {
        return REG_RD32(CSB, addr);
    }
    else if (bus == MUTEX_BUS_BAR0)
    {
        return REG_RD32(BAR0, addr);
    }

    return LW_U32_MAX;
}

/*!
 * @brief The lib interface to call the real register write accessor in SEC2
 *
 * @param[in]  bus    Which bus we will use to write the register
 * @param[in]  addr   Address of the register
 * @param[in]  val    Value to be written to the register
 */
void
mutexRegWr32
(
    LwU8  bus,
    LwU32 addr,
    LwU32 val
)
{
    if (bus == MUTEX_BUS_CSB)
    {
        REG_WR32(CSB, addr, val);
    }
    else if (bus == MUTEX_BUS_BAR0)
    {
        REG_WR32(BAR0, addr, val);
    }
}

/*!
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS.  It needs to be removed once the LS/HS code sharing mechanism is
 * implemented)
 *
 * The lib interface to call the real register read accessor in SEC2 at HS mode
 *
 * @param[in]  bus    Which bus we will use to read the register
 * @param[in]  addr   Address of the register
 * @param[in]  pData  Pointer to the buffer storing the read data
 *
 * @return FLCN_OK if the request succeeds.
 * @return FLCN_ERR_NOT_SUPPORTED if the requested bus is not supported.
 * @return FLCN_ERR_SELWREBUS_REGISTER_READ_ERROR if there is any error reading SE bus.
 */
FLCN_STATUS
mutexRegRd32_hs
(
    LwU8   bus,
    LwU32  addr,
    LwU32 *pData
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_NOT_SUPPORTED;
    SE_STATUS   seStatus;

    if (bus == MUTEX_BUS_CSB)
    {
        CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(addr, pData));
    }
    else if (bus == MUTEX_BUS_BAR0)
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(addr, pData));
    }
    else if (bus == MUTEX_BUS_SE)
    {
        seStatus = SE_SECBUS_REG_RD32_HS_ERRCHK(addr, pData);
        if (seStatus != SE_OK)
        {
            CHECK_FLCN_STATUS(FLCN_ERR_SELWREBUS_REGISTER_READ_ERROR);
        }
        else
        {
            flcnStatus = FLCN_OK;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS.  It needs to be removed once the LS/HS code sharing mechanism is
 * implemented)
 *
 * The lib interface to call the real register write accessor in SEC2 at HS mode
 *
 * @param[in]  bus    Which bus we will use to write the register
 * @param[in]  addr   Address of the register
 * @param[in]  val    Value to be written to the register
 *
 * @return FLCN_OK if the request succeeds.
 * @return FLCN_ERR_NOT_SUPPORTED if the requested bus is not supported.
 * @return FLCN_ERR_SELWREBUS_REGISTER_WRITE_ERROR if there is any error writing SE bus.
 */
FLCN_STATUS
mutexRegWr32_hs
(
    LwU8  bus,
    LwU32 addr,
    LwU32 val
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_NOT_SUPPORTED;
    SE_STATUS   seStatus;

    if (bus == MUTEX_BUS_CSB)
    {
        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(addr, val));
    }
    else if (bus == MUTEX_BUS_BAR0)
    {
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(addr, val));
    }
    else if (bus == MUTEX_BUS_SE)
    {
        seStatus = SE_SECBUS_REG_WR32_HS_ERRCHK(addr, val);
        if (seStatus != SE_OK)
        {
            CHECK_FLCN_STATUS(FLCN_ERR_SELWREBUS_REGISTER_WRITE_ERROR);
        }
        else
        {
            flcnStatus = FLCN_OK;
        }
    }
ErrorExit:
    return flcnStatus;
}

