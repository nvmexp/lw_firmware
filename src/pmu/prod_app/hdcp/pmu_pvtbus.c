/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pvtbus.c
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the DPU/PMU to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

#include "dev_ext_devices.h"
#include "dev_pwr_csb.h"
#include "dev_disp.h"
#include "hdcp/pmu_pvtbus.h"
#include "pmusw.h"
#include "pmu_bar0.h"
#include "pmu_objtimer.h"

extern
LwU32 CtlOptions;

/* Function prototypes */
static LwBool s_hdcpCheckBusReady(LwU32 pData) GCC_ATTRIB_USED();


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
/*@}*/

#define LW_PDISP_PRIVATE_HDCP_M0_MSB    0x00000000
#define LW_PDISP_PRIVATE_HDCP_M0_LSB    0x00000004

/*! @brief Address of the SORx read-only register. */
#define LW_PDISP_PRIVATE_HDCP_STATUS_SORx_RO(i) \
    (0x100 + ((LW_PDISP_UPSTREAM_HDCP_CMODE_INDEX_SOR(i)) << 2))

/*! @brief Address of the SORx write-only register. */
#define LW_PDISP_PRIVATE_HDCP_STATUS_SORx_WO(i) \
    (0x80000200 + ((LW_PDISP_UPSTREAM_HDCP_CMODE_INDEX_SOR(i)) << 2))


/*
 * Purposefully not dolwmented to doxygen.
 */
extern LwU32 HdcpChipId;
#define HDCP_SECRET_ADDR_MSB    (HdcpChipId ^ 0x49CF8204)
#define HDCP_SECRET_ADDR_LSB    (HdcpChipId ^ 0x2176C49D)
#define HDCP_SECRET_DATA_MSB    (HdcpChipId ^ 0xCD117D80)
#define HDCP_SECRET_DATA_LSB    (HdcpChipId ^ 0x76588D33)



/*!
 * Time (in microseconds) to spend polling the data register for valid data
 */

#define HDCP_PRIVATE_BUS_POLL_TIME               5000

/*!
 * Time (in nanoseconds) to spend waiting before reading the private bus
 * See BUG 1267508
 */
#define HDCP_PRIVATE_BUS_DELAY_NS_BUG_1267508    (3 * 1000)

/*!
 * HDCP Wait function with YIELD to relinquish the control to other tasks
 * while waiting.
 *
 * @param[in]  delayNS  Delay in nanoseconds
 */
static void
s_hdcpYieldWaitNs
(
    LwU32 delayNs
)
{
    FLCN_TIMESTAMP  startTime;
    LwU32           elapsedTime = 0;

    osPTimerTimeNsLwrrentGet(&startTime);

    while (elapsedTime < delayNs)
    {
        lwrtosYIELD();
        elapsedTime = osPTimerTimeNsElapsedGet(&startTime);
    }
}

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
s_hdcpCheckBusReady(LwU32 pData)
{
    LwU32 regVal;

    //
    // This is not halified for 2 reasons:
    //   1. We are planning to move HDCP code to DPU during GM20x timeframe
    //   2. Delay is very short (with lwrtosYIELD) to have any real impact to perf
    // If (1) doesnt go according to plan and (2) is found to be a problem, then
    // it will be halified.
    // 
    s_hdcpYieldWaitNs(HDCP_PRIVATE_BUS_DELAY_NS_BUG_1267508);

    regVal = REG_RD32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_DATA);

    if (regVal == 0)
    {
        return LW_FALSE;
    }

    *((LwU32 *)(pData)) = regVal;
    return LW_TRUE;
}


/*!
 * Reads the value of a register using the private bus.
 *
 * @param[in]   address Address of the register to read.
 * @param[out]  pData   Value read from the register. If there was an error
 *      while reading the data, the value is set 0.
 *
 * @return BUS_OK if the read was successful; otherwise, a detailed error code.
 */

static BUS_STATUS
s_hdcpReadReg
(
    LwU32   address,
    LwU32  *pData
)
{
    LwU32 regVal;

    REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_MSB);
    REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_LSB);
    REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, address);

    if (!OS_PTIMER_SPIN_WAIT_US_COND(s_hdcpCheckBusReady,
        (LwU32)(&regVal), HDCP_PRIVATE_BUS_POLL_TIME))
    {
        *pData = 0;
        return BUS_TIMEOUT_ERROR;
    }

    // read the data or flush the fifo on an error
    *pData = REG_RD32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_DATA);

    if (HDCP_READ_ADDRESS_VALID != regVal)
    {
        *pData = 0;
        return BUS_READ_ERROR;
    }

    return BUS_SUCCESS;
}


/*!
 * Writes data to the private bus.
 *
 * @param[in]   address Address of the register to write.
 * @param[in]   data    Value to write to the register.
 *
 * @return BUS_OK if the write was successful; otherwise, a detailed error
 *      code.
 */

static BUS_STATUS
s_hdcpWriteReg
(
    LwU32 address,
    LwU32 data
)
{
    LwU32 regVal = 0;

    REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_MSB);
    REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, HDCP_SECRET_ADDR_LSB);
    REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, address);

    if (!OS_PTIMER_SPIN_WAIT_US_COND(s_hdcpCheckBusReady,
        (LwU32)(&regVal), HDCP_PRIVATE_BUS_POLL_TIME))
    {
        return BUS_TIMEOUT_ERROR;
    }

    // dummy read to flush the read fifo
    (void)REG_RD32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_DATA);

    // if the write address was successfully sent, send the write data
    if (HDCP_WRITE_ADDRESS_VALID == regVal)
    {
        REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, HDCP_SECRET_DATA_MSB);
        REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, HDCP_SECRET_DATA_LSB);
        REG_WR32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_ADDR, data);

        if (!OS_PTIMER_SPIN_WAIT_US_COND(s_hdcpCheckBusReady,
            (LwU32)(&regVal), HDCP_PRIVATE_BUS_POLL_TIME))
        {
            return BUS_TIMEOUT_ERROR;
        }

        // dummy read to flush the read fifo
        (void)REG_RD32(CSB, LW_CPWR_PMU_DISP_PRIVATE_BUS_DATA);

        if (HDCP_WRITE_DATA_VALID != regVal)
        {
            return BUS_WRITE_ERROR;
        }
    }
    else
    {
        return BUS_WRITE_ERROR;
    }

    return BUS_SUCCESS;
}


/*!
 * Reads the value of M0 off the private bus.
 *
 * @param[out] pM0  Value of M0. If an error oclwrred while reading M0, the
 *      value will be set to 0.
 *
 * @return BUS_OK if the read of M0 was successful; otherwise, a detailed
 *      error code.
 */

BUS_STATUS hdcpReadM0(LwU64 *pM0)
{
    BUS_STATUS status;
    LwU64 m0;

    if (FLD_TEST_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, CtlOptions))
    {
        //
        // If the control options are set to production keys, read the
        // production value of M0
        //
        LwU32 reg;

        status = s_hdcpReadReg(LW_PDISP_PRIVATE_HDCP_M0_MSB, &reg);
        if (BUS_SUCCESS == status)
        {
            m0 = (LwU64) reg << 32;
            status = s_hdcpReadReg(LW_PDISP_PRIVATE_HDCP_M0_LSB, &reg);
            if (BUS_SUCCESS == status)
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
        status = BUS_SUCCESS;
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
 * @return BUS_OK if the status register read was successful; otherwise,
 *      a detailed error code.
 */

BUS_STATUS hdcpReadStatus(LwU32 idx, LwU32 *pData)
{
    return s_hdcpReadReg(LW_PDISP_PRIVATE_HDCP_STATUS_SORx_RO(idx), pData);
}


/*!
 * Writes data to the specified status register.
 *
 * @param[in]   idx     Index of the status register to write.
 * @param[in]   data    The value to write.
 *
 * @return BUS_OK if the status register write was successful; otherwise,
 *      a detailed error code.
 */

BUS_STATUS hdcpWriteStatus(LwU32 idx, LwU32 data)
{
    return s_hdcpWriteReg(LW_PDISP_PRIVATE_HDCP_STATUS_SORx_WO(idx), data);
}
