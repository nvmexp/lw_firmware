/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcp_secbus0300.c
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the FLCN to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

/* ------------------------ System Includes -------------------------------- */
#include "dev_ext_devices.h"
#include "dev_disp.h"
#include "dev_pwr_falcon_csb.h"
#include "lwoslayer.h"
#include "dev_sec_csb.h"
/* ------------------------ Application Includes --------------------------- */
#include "hdcp_secbus.h"
#include "hdcp_cmn.h"
#include "dev_disp_seb.h"

/* ------------------------ Types Definitions ------------------------------ */

/* ------------------------ External Definitions --------------------------- */
extern LwU32 CtlOptions;
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */

/* ------------------------ Macros & Defines ------------------------------- */
#define hdcpReadRegister_hs(addr, pDataOut)  libIntfcHdcpSecBusAccess(LW_TRUE, addr, 0, pDataOut)
#define hdcpWriteRegister_hs(addr, dataIn)   libIntfcHdcpSecBusAccess(LW_FALSE, addr, dataIn, NULL)

/* ------------------------ Static Variables ------------------------------- */

/* ------------------------ Public Functions ------------------------------- */

/*!
 * Reads the value of M0 off the private bus.
 *
 * @param[out] pM0  Value of M0. If an error oclwrred while reading M0, the
 *      value will be set to 0.
 *
 * @return HDCP_BUS_SUCCESS if the read of M0 was successful; otherwise, a detailed
 *      error code.
 */
HDCP_BUS_STATUS
hdcpReadM0_v03_00
(
    LwU64 *pM0
)
{
    HDCP_BUS_STATUS status;
    LwU64 m0;
    LwU32 data1, data2;

    if (FLD_TEST_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, CtlOptions))
    {
        //
        // If the control options are set to production keys, read the
        // production value of M0
        //
        LwU32 reg;

        CHECK_STATUS(hdcpReadRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_M0_MSB, &reg));

        m0 = (LwU64)reg << 32;
        CHECK_STATUS(hdcpReadRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_M0_LSB, &reg));
        m0 |= reg;
        status = HDCP_BUS_SUCCESS;
    }
    else
    {
        //
        // The control options are set to test keys. Use the test vector
        // value of M0.
        //
        m0 = 0x372d3dce38bbe78fLL;
        status = HDCP_BUS_SUCCESS;
    }

    *pM0 = m0;

    CHECK_STATUS(hdcpReadRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_STATUS_SOR(0), &data1));
    CHECK_STATUS(hdcpWriteRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_STATUS_SOR(0), 0xffffffff));
    CHECK_STATUS(hdcpReadRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_STATUS_SOR(0), &data2));
    CHECK_STATUS(hdcpWriteRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_STATUS_SOR(0), data1));

label_return:
    return status;
}

/*!
 * Reads the status register value.
 *
 * @param[in]   idx     Index of the status register to read.
 * @param[out]  pData   The value of the status register. If an error oclwrred
 *                      while reading the status register, the value will be set to 0.
 *
 * @return HDCP_BUS_SUCCESS if the status register read was successful; otherwise,
 *      a detailed error code.
 */
HDCP_BUS_STATUS
hdcpReadStatus_v03_00
(
    LwU32 idx,
    LwU32 *pData
)
{
    HDCP_BUS_STATUS status = HDCP_BUS_SUCCESS;

    CHECK_STATUS(hdcpReadRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_STATUS_SOR(idx), pData));

label_return:
    return status;
}

/*!
 * Writes data to the specified status register.
 *
 * @param[in]   idx     Index of the status register to write.
 * @param[in]   data    The value to write.
 *
 * @return HDCP_BUS_SUCCESS if the status register write was successful; otherwise,
 *      a detailed error code.
 */
HDCP_BUS_STATUS
hdcpWriteStatus_v03_00
(
    LwU32 idx,
    LwU32 data
)
{
    HDCP_BUS_STATUS status = HDCP_BUS_SUCCESS;

    CHECK_STATUS(hdcpWriteRegister_hs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_STATUS_SOR(idx), data));

label_return:
    return status;
}
