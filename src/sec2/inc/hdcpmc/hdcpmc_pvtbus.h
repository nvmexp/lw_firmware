/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#ifndef HDCPMC_PVTBUS_H
#define HDCPMC_PVTBUS_H

/*!
 * @file    hdcpmc_pvtbus.h
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the TSEC to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*! @brief Bus error code */
typedef LwU8 BUS_STATUS;

/*! @brief Successful operation */
#define BUS_SUCCESS                                                        0x00

/*! @brief Read error */
#define BUS_READ_ERROR                                                     0x11

/*! @brief Write error */
#define BUS_WRITE_ERROR                                                    0x12

/*! @brief Timeout error */
#define BUS_TIMEOUT_ERROR                                                  0x13

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
BUS_STATUS hdcpWriteLc128(void)
    GCC_ATTRIB_SECTION("imem_hdcpPvtBus", "hdcpWriteLc128");
BUS_STATUS hdcpWriteKs(LwU8 *pKs)
    GCC_ATTRIB_SECTION("imem_hdcpPvtBus", "hdcpWriteKs");
BUS_STATUS hdcpWriteRiv(LwU8 *pRiv)
    GCC_ATTRIB_SECTION("imem_hdcpPvtBus", "hdcpWriteRiv");
BUS_STATUS hdcpEnableEncryption(void)
    GCC_ATTRIB_SECTION("imem_hdcpPvtBus", "hdcpEnableEncryption");
BUS_STATUS hdcpDisableEncryption(void)
    GCC_ATTRIB_SECTION("imem_hdcpPvtBus", "hdcpDisableEncryption");

#endif // HDCPMC_PVTBUS_H
#endif
