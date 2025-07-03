/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcp_pvtbus.c
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the FLCN to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

/* ------------------------ System Includes -------------------------------- */
#include "dev_ext_devices.h"
#include "dev_disp_falcon.h"
#include "dev_disp.h"
#include "dev_pwr_falcon_csb.h"
#include "lwoslayer.h"
#include "osptimer.h"

/* ------------------------ Application Includes --------------------------- */
#include "hdcp_pvtbus.h"
#include "hdcp_cmn.h"

/* ------------------------ Types Definitions ------------------------------ */
/*@{ Display Private Bus Status */
/*! @brief Secret code for the write address is valid. */
#define HDCP_WRITE_ADDRESS_VALID    0xFFFC0000

/*! @brief Secret code for the read address is valid. */
#define HDCP_READ_ADDRESS_VALID     0xFFFC0001

/*! @brief Secret code for the write data is valid. */
#define HDCP_WRITE_DATA_VALID       0xFFF30000

/*! Read/write address is invalid. */
#define HDCP_ADDRESS_ILWALID        0xDEAD0000

/*! Secret code for the read/write access is invalid. */
#define HDCP_SECRET_CODE_ILWALID    0xDEAD0001

/*! Read/write address is invalid. */
#define HDCP_READ_DATA_ILWALID      0xCAFEFACE

/*@}*/

#define LW_PDISP_PRIVATE_HDCP_M0_MSB    0x00000000
#define LW_PDISP_PRIVATE_HDCP_M0_LSB    0x00000004

/*! @brief Address of the SORx read-only register.
 * 'i' is the sorIndex that is passed down
 */
#define LW_PDISP_PRIVATE_HDCP_STATUS_SORx_RO(i) \
    (0x100 + ((LW_PDISP_UPSTREAM_HDCP_CMODE_INDEX_SOR(i)) << 2))

/*! @brief Address of the SORx write-only register.
 * 'i' is the sorIndex that is passed down
 */
#define LW_PDISP_PRIVATE_HDCP_STATUS_SORx_WO(i) \
    (0x80000200 + ((LW_PDISP_UPSTREAM_HDCP_CMODE_INDEX_SOR(i)) << 2))



/*
 * Purposefully not dolwmented to doxygen.
 */
#define HDCP_SECRET_ADDR_MSB    (HdcpChipId ^ 0x49CF8204)
#define HDCP_SECRET_ADDR_LSB    (HdcpChipId ^ 0x2176C49D)
#define HDCP_SECRET_DATA_MSB    (HdcpChipId ^ 0xCD117D80)
#define HDCP_SECRET_DATA_LSB    (HdcpChipId ^ 0x76588D33)

/*!
 * Time (in microseconds) to spend polling the data register for valid data
 */

#define HDCP_PRIVATE_BUS_POLL_TIME               5000

/* ------------------------ External Definitions --------------------------- */
extern LwU32 CtlOptions;
extern LwU32 HdcpChipId;

/* ------------------------ Function Prototypes ---------------------------- */
static LwBool _hdcpCheckBusReady_v02_05(LwU32 *pData) GCC_ATTRIB_USED()
                   GCC_ATTRIB_SECTION("imem_libHdcpbus", "_hdcpCheckBusReady_v02_05");
static HDCP_BUS_STATUS _hdcpReadReg_v02_05(LwU32 address, LwU32 *pData)
                   GCC_ATTRIB_SECTION("imem_libHdcpbus", "_hdcpReadReg_v02_05");
static HDCP_BUS_STATUS _hdcpWriteReg_v02_05(LwU32 address, LwU32 data)
                   GCC_ATTRIB_SECTION("imem_libHdcpbus", "_hdcpWriteReg_v02_05");
/* ------------------------ Global Variables ------------------------------- */

/* ------------------------ Macros & Defines ------------------------------- */

/* ------------------------ Static Variables ------------------------------- */

/*!
 * Callback function for timer condition check.
 *
 * @param[out]  pData The value of the register upon completion of the time
 *      interval.
 *
 * @return LW_TRUE if a valid value was present in the register;
 *         LW_FALSE otherwise.
 */

static LwBool
_hdcpCheckBusReady_v02_05(LwU32 *pData)
{
    *pData = DISP_REG_RD32(LW_PDISP_FALCON_PRIVATE_BUS_DATA);

    if (*pData == 0)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * Reads the value of a register using the private bus.
 *
 * @param[in]   address Address of the register to read.
 * @param[out]  pData   Value read from the register. If there was an error
 *      while reading the data, the value is set 0.
 *
 * @return HDCP_BUS_SUCCESS if the read was successful; otherwise, a detailed error code.
 */
static HDCP_BUS_STATUS
_hdcpReadReg_v02_05
(
    LwU32   address,
    LwU32  *pData
)
{
    HDCP_BUS_STATUS status = HDCP_BUS_SUCCESS;
    LwU32 regVal;

    // The file's regWrite is preVolta CSB access and always succeed.
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_MSB);
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_LSB);
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, address);

    if (!OS_PTIMER_SPIN_WAIT_US_COND(_hdcpCheckBusReady_v02_05,
        &regVal, HDCP_PRIVATE_BUS_POLL_TIME))
    {
        status = HDCP_BUS_TIMEOUT_ERROR;
        goto hdcpReadReg_Exit;
    }

    if (FLCNLIBCFG_FEATURE_ENABLED(HDCP_SW_WAR_BUG_1503447))
    {
        //
        // The first non zero value that is read is the actual data in the read operation due
        // to bug on GM20X because of which the 1st read to PRIVATE_BUS_DATA is dropped out
        //
        if (regVal == HDCP_READ_DATA_ILWALID)
        {
            status = HDCP_BUS_READ_ERROR;
            goto hdcpReadReg_Exit;
        }
        *pData = regVal;
    }
    else
    {
        // read the data or flush the fifo on an error
        *pData = DISP_REG_RD32(LW_PDISP_FALCON_PRIVATE_BUS_DATA);

        if (regVal != HDCP_READ_ADDRESS_VALID)
        {
            status = HDCP_BUS_READ_ERROR;
            goto hdcpReadReg_Exit;
        }
    }

    hdcpReadReg_Exit:
    if (status != HDCP_BUS_SUCCESS)
    {
        *pData = 0;
    }
    return status;
}

/*!
 * Writes data to the private bus.
 *
 * @param[in]   address Address of the register to write.
 * @param[in]   data    Value to write to the register.
 *
 * @return HDCP_BUS_SUCCESS if the write was successful; otherwise, a detailed error
 *      code.
 */
static HDCP_BUS_STATUS
_hdcpWriteReg_v02_05
(
    LwU32 address,
    LwU32 data
)
{
    LwU32 regVal = 0;

    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_MSB);
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_LSB);
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, address);

    if (!OS_PTIMER_SPIN_WAIT_US_COND(_hdcpCheckBusReady_v02_05,
        &regVal, HDCP_PRIVATE_BUS_POLL_TIME))
    {
        return HDCP_BUS_TIMEOUT_ERROR;
    }

    //
    // Ignore the dummy read for write operation because of the HW bug on
    // GM20X due to which the 1st read to PRIVATE_BUS_DATA is dropped out
    //
    if (!FLCNLIBCFG_FEATURE_ENABLED(HDCP_SW_WAR_BUG_1503447))
    {
        // dummy read to flush the read fifo
        (void)DISP_REG_RD32(LW_PDISP_FALCON_PRIVATE_BUS_DATA);

        if (HDCP_WRITE_ADDRESS_VALID != regVal)
        {
            return HDCP_BUS_WRITE_ERROR;
        }
    }

    // if the write address was successfully sent, send the write data
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, HDCP_SECRET_DATA_MSB);
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, HDCP_SECRET_DATA_LSB);
    DISP_REG_WR32(LW_PDISP_FALCON_PRIVATE_BUS_ADDR, data);

    if (!OS_PTIMER_SPIN_WAIT_US_COND(_hdcpCheckBusReady_v02_05,
        &regVal, HDCP_PRIVATE_BUS_POLL_TIME))
    {
        return HDCP_BUS_TIMEOUT_ERROR;
    }

    if (!FLCNLIBCFG_FEATURE_ENABLED(HDCP_SW_WAR_BUG_1503447))
    {
        // dummy read to flush the read fifo
        (void)DISP_REG_RD32(LW_PDISP_FALCON_PRIVATE_BUS_DATA);

        if (HDCP_WRITE_DATA_VALID != regVal)
        {
            return HDCP_BUS_WRITE_ERROR;
        }
    }
    return HDCP_BUS_SUCCESS;
}

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
HDCP_BUS_STATUS hdcpReadM0_v02_05(LwU64 *pM0)
{
    HDCP_BUS_STATUS status;
    LwU64 m0;

    if (FLD_TEST_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, CtlOptions))
    {
        //
        // If the control options are set to production keys, read the
        // production value of M0
        //
        LwU32 reg;

        status = _hdcpReadReg_v02_05(LW_PDISP_PRIVATE_HDCP_M0_MSB, &reg);
        if (HDCP_BUS_SUCCESS == status)
        {
            m0 = (LwU64) reg << 32;
            status = _hdcpReadReg_v02_05(LW_PDISP_PRIVATE_HDCP_M0_LSB, &reg);
            if (HDCP_BUS_SUCCESS == status)
            {
                m0 |= reg;
            }
            else
            {
                m0 = 0;
            }
        }
        else
        {
            m0 = 0;
        }
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
    return status;
}

/*!
 * Reads the status register value.
 *
 * @param[in]   idx     Index of the status register to read.
 * @param[out]  pData   The value of the status register. If an error oclwrred
 *      while reading the status register, the value will be set to 0.
 *
 * @return HDCP_BUS_SUCCESS if the status register read was successful; otherwise,
 *      a detailed error code.
 */
HDCP_BUS_STATUS hdcpReadStatus_v02_05(LwU32 idx, LwU32 *pData)
{
    return _hdcpReadReg_v02_05(LW_PDISP_PRIVATE_HDCP_STATUS_SORx_RO(idx), pData);
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
HDCP_BUS_STATUS hdcpWriteStatus_v02_05(LwU32 idx, LwU32 data)
{
    return _hdcpWriteReg_v02_05(LW_PDISP_PRIVATE_HDCP_STATUS_SORx_WO(idx), data);
}
