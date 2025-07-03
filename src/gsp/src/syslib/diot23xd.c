/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   diot23xd.c
 * @brief  DIO Interface T23XD Hal Functions
 *
 * DIO Hal Functions implement chip specific, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "gspsw.h"
#include "engine.h"
/* ------------------------ Application includes --------------------------- */
#include "config/g_dio_hal.h"
#include "config/g_dio_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
sysSYSLIB_CODE static void _dioPollCount_T23XD(void);
sysSYSLIB_CODE static void _dioPollWriteComplete_T23XD(void);
sysSYSLIB_CODE static void _dioPollReadComplete_T23XD(void);

/*!
 * @brief   Poll for free space in output FIFO
 *
 * @return  Void
 */
sysSYSLIB_CODE static void
_dioPollCount_T23XD()
{
    LwU32 count = 0;
    LwU32 ctrl_data;
    while(!count) {
        ctrl_data = intioRead(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0));
        count = DRF_VAL(_PRGNLCL, _FALCON_DIO_DOC_CTRL, _COUNT, ctrl_data);
    }
}

/*!
 * @brief   Poll for Write to Complete
 *
 * @return  Void
 */
sysSYSLIB_CODE static void
_dioPollWriteComplete_T23XD()
{
    LwU32 status = 0;
    LwU32 ctrl_data;
    while(!status) {
        ctrl_data = intioRead(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0));
        status = DRF_VAL(_PRGNLCL, _FALCON_DIO_DOC_CTRL, _WR_FINISHED, ctrl_data);
    }
}

/*!
 * @brief   Poll for Read to Complete
 *
 * @return  Void
 */
sysSYSLIB_CODE static void
_dioPollReadComplete_T23XD()
{
    LwU32 status = 0;
    LwU32 ctrl_data;
    while(!status) {
        ctrl_data = intioRead(LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0));
        status = DRF_VAL(_PRGNLCL, _FALCON_DIO_DIC_CTRL, _COUNT, ctrl_data);
    }
    /* now poll on RD_FINISHED */
    status = 0;
    while(!status) {
        ctrl_data = intioRead(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0));
        status = DRF_VAL(_PRGNLCL, _FALCON_DIO_DOC_CTRL, _RD_FINISHED, ctrl_data);
    }
}

/*!
 * @brief   DIO Register read to SNIC
 *
 * @In      address  Address of the register to be read
 *
 * @out     pData    Buffer to hold read data
 * @return  Void
 */
sysSYSLIB_CODE FLCN_STATUS
dioReadReg_T23XD
(
    LwU32 address,
    LwU32 *pData
)
{
    LwU32 ctrl_val;

    //TODO: 200586859 Need to do error handling
    _dioPollCount_T23XD();
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_D2(0), 0x1);
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_D1(0), address);
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_D0(0), 0x0);
    _dioPollReadComplete_T23XD();

    ctrl_val = intioRead(LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0));
    /* Retain every thing of DIC_CTRL except "RESET" bit. Make the "RESET" bit 0.
    * If "RESET" bit not made zero, it would clear the DIC input FIFO and we
    * would lose the read data. so the data read would be zero
    */
    ctrl_val = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DIO_DIC_CTRL, _RESET, 0x0, ctrl_val);
    intioWrite(LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0), FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DIO_DIC_CTRL, _POP, 0x1, ctrl_val));
    *pData = intioRead(LW_PRGNLCL_FALCON_DIO_DIC_D0(0));

    /* Reset DOC and DIC regs */
    ctrl_val = intioRead(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0));
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0), FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DIO_DOC_CTRL, _RESET, 0x1, ctrl_val));	
    ctrl_val = intioRead(LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0));

    return FLCN_OK;
}


/*!
 * @brief   DIO Register write to SNIC
 *
 * @In      address  Address of the register to be written
 * @In      data     Data to be written
 *
 * @return  FLCN_STATUS
 */
sysSYSLIB_CODE FLCN_STATUS
dioWriteReg_T23XD
(
    LwU32 address,
    LwU32 data
)
{
    LwU32 ctrl_val;

    //TODO: Need to do error handling
    _dioPollCount_T23XD();
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_D2(0), 0x0);
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_D1(0), address);
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_D0(0), data);

    _dioPollWriteComplete_T23XD();

    //Reset DOC and DIC regs
    ctrl_val = intioRead(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0));
    intioWrite(LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0), FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DIO_DOC_CTRL, _RESET, 0x1, ctrl_val));	
    ctrl_val = intioRead(LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0));
    intioWrite(LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0), FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DIO_DIC_CTRL, _RESET, 0x1, ctrl_val));

    return FLCN_OK;
}
