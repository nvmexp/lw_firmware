/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdbool.h>
#include <stddef.h>
#include <lwmisc.h>
#include <liblwriscv/io_local.h>
#include <liblwriscv/io_dio.h>

/* =============================================================================
 * DIO additional macros
 * ========================================================================== */
#ifdef UTF_UCODE_BUILD
uint32_t gDioMaxWait = DIO_MAX_WAIT_DEFAULT;
#define DIO_MAX_WAIT gDioMaxWait
#else
#define DIO_MAX_WAIT 0xfffffffe
#endif

//
// sw defines to unify hw manual defined dio registers fields
// LW_PRGNLCL_FALCON_x and
// LW_PRGNLCL_FALCON_DIO_x(i) with
// LW_PRGNLCL_DIO_x
//
#define LW_PRGNLCL_DIO_DOC_CTRL_COUNT                                        7:0
#define LW_PRGNLCL_DIO_DOC_CTRL_RESET                                      16:16
#define LW_PRGNLCL_DIO_DOC_CTRL_STOP                                       17:17
#define LW_PRGNLCL_DIO_DOC_CTRL_EMPTY                                      18:18
#define LW_PRGNLCL_DIO_DOC_CTRL_WR_FINISHED                                19:19
#define LW_PRGNLCL_DIO_DOC_CTRL_RD_FINISHED                                20:20
#define LW_PRGNLCL_DIO_DOC_CTRL_WR_ERROR                                   21:21
#define LW_PRGNLCL_DIO_DOC_CTRL_RD_ERROR                                   22:22
#define LW_PRGNLCL_DIO_DOC_CTRL_PROTOCOL_ERROR                             23:23
#define LW_PRGNLCL_DIO_DOC_D0_DATA                                          31:0
#define LW_PRGNLCL_DIO_DOC_D1_DATA                                          31:0
#define LW_PRGNLCL_DIO_DOC_D2_DATA                                          15:0
#define LW_PRGNLCL_DIO_DIC_CTRL_COUNT                                        7:0
#define LW_PRGNLCL_DIO_DIC_CTRL_RESET                                      16:16
#define LW_PRGNLCL_DIO_DIC_CTRL_VALID                                      19:19
#define LW_PRGNLCL_DIO_DIC_CTRL_POP                                        20:20
#define LW_PRGNLCL_DIO_DIC_D0_DATA                                          31:0
#define LW_PRGNLCL_DIO_DIC_D1_DATA                                          31:0
#define LW_PRGNLCL_DIO_DIC_D2_DATA                                          15:0
#define LW_PRGNLCL_DIO_DIO_ERR_INFO                                         31:0

//
// sw defines for specific DOC Dx interface
//
#if LWRISCV_HAS_DIO_SE
#define LW_PRGNLCL_DIO_DOC_D0_SEHUB_READ                                   16:16
#define LW_PRGNLCL_DIO_DOC_D0_SEHUB_ADDR                                    15:0
#define LW_PRGNLCL_DIO_DOC_D1_SEHUB_WDATA                                   31:0
#define LW_PRGNLCL_DIO_DIC_D0_SEHUB_RDATA                                   31:0
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
#define LW_PRGNLCL_DIO_DOC_D0_SNIC_WDATA                                    31:0
#define LW_PRGNLCL_DIO_DOC_D1_SNIC_ADDR                                     31:0
#define LW_PRGNLCL_DIO_DOC_D2_SNIC_READ                                      0:0
#define LW_PRGNLCL_DIO_DIC_D0_SNIC_RDATA                                    31:0
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
#define LW_PRGNLCL_DIO_DOC_D0_FBHUB_READ                                   16:16
#define LW_PRGNLCL_DIO_DOC_D0_FBHUB_ADDR                                    15:0
#define LW_PRGNLCL_DIO_DOC_D1_FBHUB_WDATA                                   31:0
#define LW_PRGNLCL_DIO_DIC_D0_FBHUB_RDATA                                   31:0
#endif // LWRISCV_HAS_DIO_FBHUB

/* =============================================================================
 * DIO control registers
 * ========================================================================== */
typedef struct
{
    uint32_t docCtrl;
    uint32_t docD0, docD1, docD2;
    uint32_t dicCtrl;
    uint32_t dicD0, dicD1, dicD2;
    uint32_t dioError;
} DIO_CTRL_REGS;

static const DIO_CTRL_REGS _dioCtrlRegsArray[] =
{
    // DIO_TYPE_ILWALID
    {},
#if LWRISCV_HAS_DIO_SE
    // DIO_TYPE_SE
    {
        LW_PRGNLCL_FALCON_DOC_CTRL,
        LW_PRGNLCL_FALCON_DOC_D0,
        LW_PRGNLCL_FALCON_DOC_D1,
        LW_PRGNLCL_FALCON_DOC_D2,
        LW_PRGNLCL_FALCON_DIC_CTRL,
        LW_PRGNLCL_FALCON_DIC_D0,
        LW_PRGNLCL_FALCON_DIC_D1,
        LW_PRGNLCL_FALCON_DIC_D2,
        LW_PRGNLCL_FALCON_DIO_ERR,
    },
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
    // DIO_TYPE_SNIC
    {
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIO_ERR(0),
    },
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
    // DIO_TYPE_FBHUB
    {
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIO_ERR(0),
    },
#endif // LWRISCV_HAS_DIO_FBHUB
};

/* =============================================================================
 * DIO core helper functions
 * ========================================================================== */
static LWRV_STATUS _dioSendRequest(DIO_TYPE dioType, DIO_OPERATION dioOp, uint32_t addr, uint32_t *pData);
static LWRV_STATUS _dioWaitForDocEmpty(DIO_TYPE dioType);
static LWRV_STATUS _dioWaitForOperationComplete(DIO_TYPE dioType, DIO_OPERATION dioOp, uint32_t *pData);
static bool _dioIfTimeout(uint32_t *pTimerCount);
static LWRV_STATUS _dioReset(DIO_TYPE dioType);

/* =============================================================================
 * DIO API implementation
 * ========================================================================== */
LWRV_STATUS dioReadWrite(DIO_PORT dioPort, DIO_OPERATION dioOp, uint32_t addr, uint32_t *pData)
{
    LWRV_STATUS status = LWRV_OK;

    if (dioPort.dioType == DIO_TYPE_ILWALID || dioPort.dioType >= DIO_TYPE_END)
    {
        return LWRV_ERR_NOT_SUPPORTED;
    }

    if ((dioOp == DIO_OPERATION_RD) && (pData == NULL))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    status = _dioWaitForDocEmpty(dioPort.dioType);
    if (status != LWRV_OK)
    {
        return status;
    }

    status = _dioSendRequest(dioPort.dioType, dioOp, addr, pData);
    if (status != LWRV_OK)
    {
        return status;
    }

    return _dioWaitForOperationComplete(dioPort.dioType, dioOp, pData);
}

/* =============================================================================
 * DIO core helper functions implementation
 * ========================================================================== */
static LWRV_STATUS _dioSendRequest(DIO_TYPE dioType, DIO_OPERATION dioOp, uint32_t addr, uint32_t *pData)
{
    switch (dioType)
    {
#if LWRISCV_HAS_DIO_SE
        case DIO_TYPE_SE:
            localWrite(_dioCtrlRegsArray[dioType].docD1, DRF_NUM(_PRGNLCL, _DIO_DOC_D1, _SEHUB_WDATA, (dioOp == DIO_OPERATION_RD) ? 0 : *pData));
            localWrite(_dioCtrlRegsArray[dioType].docD0,
                DRF_NUM(_PRGNLCL, _DIO_DOC_D0, _SEHUB_READ, (dioOp == DIO_OPERATION_RD ? 1 : 0)) |
                DRF_NUM(_PRGNLCL, _DIO_DOC_D0, _SEHUB_ADDR, addr));
            break;
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
        case DIO_TYPE_SNIC:
            localWrite(_dioCtrlRegsArray[dioType].docD2, DRF_NUM(_PRGNLCL, _DIO_DOC_D2, _SNIC_READ, (dioOp == DIO_OPERATION_RD)));
            localWrite(_dioCtrlRegsArray[dioType].docD1, DRF_NUM(_PRGNLCL, _DIO_DOC_D1, _SNIC_ADDR, addr));
            localWrite(_dioCtrlRegsArray[dioType].docD0, DRF_NUM(_PRGNLCL, _DIO_DOC_D0, _SNIC_WDATA, (dioOp == DIO_OPERATION_RD) ? 0 : *pData));
            break;
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
        case DIO_TYPE_FBHUB:
            localWrite(_dioCtrlRegsArray[dioType].docD1, DRF_NUM(_PRGNLCL, _DIO_DOC_D1, _FBHUB_WDATA, (dioOp == DIO_OPERATION_RD) ? 0 : *pData));
            localWrite(_dioCtrlRegsArray[dioType].docD0,
                DRF_NUM(_PRGNLCL, _DIO_DOC_D0, _FBHUB_READ, (dioOp == DIO_OPERATION_RD ? 1 : 0)) |
                DRF_NUM(_PRGNLCL, _DIO_DOC_D0, _FBHUB_ADDR, addr));
            break;
#endif // LWRISCV_HAS_DIO_FBHUB
        default:
            return LWRV_ERR_NOT_SUPPORTED;
    }
    return LWRV_OK;
}

/*!
 * @brief    Wait for free entry in DOC
 * @details  The function tries to take a free entry in DOC and exit with no DIO errors.
 *
 * @ref      https://confluence.lwpu.com/display/LW/liblwriscv+DIO+driver#liblwriscvDIOdriver-Processflowdiagram
 */
static LWRV_STATUS _dioWaitForDocEmpty(DIO_TYPE dioType)
{
    uint32_t timerCount = 0;
    uint32_t docCtrl = 0;
    uint32_t dioErrorCode = 0;
    do
    {
        docCtrl = localRead(_dioCtrlRegsArray[dioType].docCtrl);

        if (DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _WR_ERROR, docCtrl) |
            DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RD_ERROR, docCtrl) |
            DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _PROTOCOL_ERROR, docCtrl))
        {
            dioErrorCode = localRead(_dioCtrlRegsArray[dioType].dioError);
            _dioReset(dioType);
            return dioErrorCode ? dioErrorCode : LWRV_ERR_DIO_DOC_CTRL_ERROR;
        }
        if (_dioIfTimeout(&timerCount))
        {
            _dioReset(dioType);
            return LWRV_ERR_TIMEOUT;
        }
    } while (!DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _EMPTY, docCtrl));

    return LWRV_OK;
}

/*!
 * @brief    Wait for operation to complete and get response for read.
 * @details  We make sure no error is caused by the operation.
 *
 * @ref      https://confluence.lwpu.com/display/LW/liblwriscv+DIO+driver#liblwriscvDIOdriver-Processflowdiagram
 */
static LWRV_STATUS _dioWaitForOperationComplete(DIO_TYPE dioType, DIO_OPERATION dioOp, uint32_t *pData)
{
    uint32_t timerCount = 0;
    uint32_t docCtrl = 0;
    uint32_t dioErrorCode = 0;
    do
    {
        docCtrl = localRead(_dioCtrlRegsArray[dioType].docCtrl);
        if (DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _WR_ERROR, docCtrl) |
            DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RD_ERROR, docCtrl) |
            DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _PROTOCOL_ERROR, docCtrl))
        {
            dioErrorCode = localRead(_dioCtrlRegsArray[dioType].dioError);
            _dioReset(dioType);
            return dioErrorCode ? dioErrorCode : LWRV_ERR_DIO_DOC_CTRL_ERROR;
        }
        if (_dioIfTimeout(&timerCount))
        {
            _dioReset(dioType);
            return LWRV_ERR_TIMEOUT;
        }
    } while (!((dioOp == DIO_OPERATION_RD) ?
        DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RD_FINISHED, docCtrl) :
        DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _WR_FINISHED, docCtrl)));

    // check dicCtrl once docCtrl indicate operation done
    timerCount = 0;
    if (dioOp == DIO_OPERATION_RD)
    {
        uint32_t dicCtrl = 0;
        do
        {
            dicCtrl = localRead(_dioCtrlRegsArray[dioType].dicCtrl);
            if (_dioIfTimeout(&timerCount))
            {
                _dioReset(dioType);
                return LWRV_ERR_TIMEOUT;
            }

        } while (DRF_VAL(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, dicCtrl) == 0);

        // pop data and clear
        localWrite(_dioCtrlRegsArray[dioType].dicCtrl, DRF_NUM(_PRGNLCL, _DIO_DIC_CTRL, _POP, 0x1));
        *pData = localRead(_dioCtrlRegsArray[dioType].dicD0);
        localWrite(_dioCtrlRegsArray[dioType].dicCtrl, DRF_NUM(_PRGNLCL, _DIO_DIC_CTRL, _VALID, 0x1));
    }

    return LWRV_OK;
}

static bool _dioIfTimeout(uint32_t *pTimerCount)
{
    if (*pTimerCount >= DIO_MAX_WAIT)
    {
        return true;
    }
    (*pTimerCount)++;
    return false;
}

/*!
 * @brief  Set clear bit and wait it to be cleared by hw on finish
 */
static LWRV_STATUS _dioReset(DIO_TYPE dioType)
{
    LWRV_STATUS status = LWRV_OK;
    uint32_t timerCount = 0;
    localWrite(_dioCtrlRegsArray[dioType].docCtrl, DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _RESET, 0x1));
    while (FLD_TEST_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _RESET, 0x1, localRead(_dioCtrlRegsArray[dioType].docCtrl)))
    {
        if (_dioIfTimeout(&timerCount))
        {
            return LWRV_ERR_TIMEOUT;
        }
    }

    return status;
}
