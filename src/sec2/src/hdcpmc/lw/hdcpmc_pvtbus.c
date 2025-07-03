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
/*!
 * @file    hdcpmc_pvtbus.c
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the HDCP task to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "hdcpmc/hdcpmc_pvtbus.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Write lc128 to HW.
 *
 * This function simply returns on big GPUs. Support to write lc128 to hardware
 * is limited to TSEC20.
 *
 * @return BUS_SUCCESS if the write was successful; otherwise, a detailed error
 *         code.
 */
BUS_STATUS hdcpWriteLc128(void)
{
    return BUS_SUCCESS;
}

/*!
 * @brief Write Ks to HW.
 *
 * This function simply returns on big GPUs. Support to write ks to hardware
 * is limited to TSEC20.
 *
 * @param[in]  pKs  Ks value to write.
 *
 * @return BUS_SUCCESS if the write was successful; otherwise, a detailed error
 *         code.
 */
BUS_STATUS hdcpWriteKs(LwU8 *pKs)
{
    return BUS_SUCCESS;
}

/*!
 * @brief Write Riv to HW
 *
 * This function simply returns on big GPUs. Support to write riv to hardware
 * is limited to TSEC20.
 *
 * @param[in]  pRiv  riv value to write.
 *
 * @return BUS_SUCCESS if the write was successful; otherwise, a detailed error
 *         code.
 */
BUS_STATUS hdcpWriteRiv(LwU8 *pRiv)
{
    return BUS_SUCCESS;
}

/*!
 * @brief Enable Encryption
 *
 * This function does nothing on big GPUs. Support to enable encryption is
 * limited to TSEC20.
 *
 * @return BUS_SUCCESS if the write was successful; otherwise, a detailed error
 *         code.
 */
BUS_STATUS hdcpEnableEncryption(void)
{
    return BUS_SUCCESS;
}

/*!
 * @brief Disable Encryption
 *
 * This function does nothing on big GPUs. Support to disable encryption is
 * limited to TSEC20.
 *
 * @return BUS_SUCCESS if the write was successful; otherwise, a detailed error
 *         code.
 */
BUS_STATUS hdcpDisableEncryption(void)
{
    return BUS_SUCCESS;
}
#endif
