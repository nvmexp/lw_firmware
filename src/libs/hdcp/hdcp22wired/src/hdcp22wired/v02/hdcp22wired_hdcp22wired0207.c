/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_hdcp22wired0207.c
 * @brief  Hdcp22 GP10X and Later Hal Functions
 *
 * HDCP2.2 Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes --------------------------------- */
#include "dev_disp.h"
#include "dev_disp_addendum.h"

/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "lw_mutex.h"
#include "lib_mutex.h"
#include "setestapi.h"
#include "setypes.h"
#include "config/g_hdcp22wired_private.h"

/* ------------------------- Types definitions ------------------------------ */
/* ------------------------- External definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Definitions ------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Calls into SE library to do an RSA operation verification
 * returns FLCN_OK if the operation is successful
 *         FLCN_ERROR if the operation did not work
 */
FLCN_STATUS
hdcp22wiredSelwrityEngineRsaVerif_v02_07(void)
{
    LwU32 status;
    status = seSampleRsaCode();
    if (status != SE_OK)
    {
        return FLCN_ERROR;
    }
    else
    {
        return FLCN_OK;
    }
}

#ifdef DPU_RTOS
/*!
 * @brief      This function writes StreamType value to a scratch register.
 * @param[in]  sorNum               SOR index number.
 * @param[in]  streamIdType         Array of stream IdType values.
 *
 * return      returns True if Scracth reg is succesfully written.
 */
FLCN_STATUS
hdcp22wiredWriteStreamTypeInScratch_v02_07
(
    LwU8            sorNum,
    HDCP22_STREAM   streamIdType[HDCP22_NUM_STREAMS_MAX]
)
{
    LwU32        data = 0;
    LwU8         mutexOwner = LW_U8_MAX;
    LwU8         mutexIndex = LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH;
    FLCN_STATUS  status = FLCN_OK;
    LwBool       bIsType1 = LW_FALSE;

    // preVolta CSB access always succeed in the function.

    // Acquire PMGR mutex to modify DISP_SCRATCH register
    status = mutexAcquire(mutexIndex, HDCP22_MUTEX_ACQUIRE_TIMEOUT_NS, &mutexOwner);

    // Make sure that Mutex is acquired.
    if ((status != FLCN_OK) || (mutexOwner == LW_U8_MAX))
    {
        return FLCN_ERROR;
    }

    data = DISP_REG_RD32(LW_PDISP_SELWRE_WR_SCRATCH(0));

    if (streamIdType[0].streamType == 0x1)
    {
        bIsType1 = LW_TRUE;
    }

    data = FLD_IDX_SET_DRF_NUM(_PDISP, _SELWRE_WR_SCRATCH_0, _STREAM_TYPE1_SOR, sorNum, bIsType1, data);

    DISP_REG_WR32(LW_PDISP_SELWRE_WR_SCRATCH(0), data);

    // Release the Mutex.
    status = mutexRelease(mutexIndex, mutexOwner);
    if (status != FLCN_OK)
    {
        return FLCN_ERROR;
    }

    // Read the register back and confirm if the StreamType was written correclty
    data = DISP_REG_RD32(LW_PDISP_SELWRE_WR_SCRATCH(0));

    // Make sure that Data is written in DISP SCRATCH register correctly.
    if ((bIsType1 && (FLD_IDX_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _STREAM_TYPE1_SOR, sorNum, _FALSE, data))) ||
           (!bIsType1 && (FLD_IDX_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _STREAM_TYPE1_SOR, sorNum, _TRUE, data))))
    {
        return FLCN_ERROR;
    }
    else
    {
        return FLCN_OK;
    }
}

/*!
 * @brief  Sets resposne bit to intimate SEC2 that lock/unlock
 *         request is completed and whether it is succesful or not.
 *         This feature is only supported from gp10x
 * @returns    FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS hdcp22wiredSendType1LockRspToSec2_v02_07
(
    HDCP22_SEC2LOCK_STATUS hdcp22Sec2LockStatus
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 data32;
    LwU8  tokenId;

    // preVolta CSB access always succeed in the function.

    if (hdcp22WiredIsSec2TypeReqSupported())
    {
        CHECK_STATUS(mutexAcquire(LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH, HDCP22_TYPELOCK_MUTEX_ACQUIRE_TIMEOUT_NS, &tokenId));

        data32 = DISP_REG_RD32(LW_PDISP_SELWRE_WR_SCRATCH_0);

        switch (hdcp22Sec2LockStatus)
        {
            case HDCP22_SEC2LOCK_SUCCESS:
                data32 = FLD_SET_DRF(_PDISP, _SELWRE_WR_SCRATCH_0,
                                  _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_TYPE1_ENGAGED, data32);
                break;
            case HDCP22_SEC2LOCK_SUCCESS_HDCP22_OFF:
                data32 = FLD_SET_DRF(_PDISP, _SELWRE_WR_SCRATCH_0,
                                  _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_HDCP22_OFF, data32);
                break;
            case HDCP22_SEC2LOCK_SUCCESS_TYPE1_DISABLED:
                data32 = FLD_SET_DRF(_PDISP, _SELWRE_WR_SCRATCH_0,
                                  _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_TYPE1_OFF, data32);
                break;
            case HDCP22_SEC2LOCK_FAILURE:
                data32 = FLD_SET_DRF(_PDISP, _SELWRE_WR_SCRATCH_0,
                                  _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _FAILED, data32);
                break;
        }

        DISP_REG_WR32(LW_PDISP_SELWRE_WR_SCRATCH_0, data32);

        CHECK_STATUS(mutexRelease(LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH, tokenId));
    }

label_return:
    return status;
}

/*!
 * @brief  Reads lock/unlock bit, set by SEC2
 *         and returns the same
 * @param[out] pBType1LockActive Pointer to fill if type1 is locked or not
 * @returns    FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS hdcp22wiredIsType1LockActive_v02_07(LwBool *pBType1LockActive)
{
    LwU32  data32;

    if (hdcp22WiredIsSec2TypeReqSupported())
    {
        // preVolta CSB access always succeed.
        data32 = DISP_REG_RD32(LW_PDISP_SELWRE_SCRATCH_0);

        if (FLD_TEST_DRF(_PDISP, _SELWRE_SCRATCH_0,
                         _VPR_HDCP22_TYPE1_LOCK, _ENABLE, data32))
        {
            *pBType1LockActive = LW_TRUE;
            return FLCN_OK;
        }
    }

    *pBType1LockActive = LW_FALSE;
    return FLCN_OK;
}
#endif
