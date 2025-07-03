/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_hdcp22wired0401.c
 * @brief  Hdcp22 wired 04.01 Hal Functions
 *
 * HDCP2.2 Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes --------------------------------- */
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_disp.h"
#include "dev_disp_seb.h"
#include "dev_disp_addendum.h"
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "seapi.h"
#include "setypes.h"
#include "scp_common.h"
#if defined(HS_OVERLAYS_ENABLED)
#include "lwosselwreovly.h"
#endif
#if defined(UPROC_RISCV) && LWRISCV_HAS_DIO_SE
#include "stdint.h"
#include "liblwriscv/io_dio.h"
#endif
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
#if defined(UPROC_RISCV)
extern FLCN_STATUS hdcp22wiredInitSorHdcp22CtrlReg_v02_05(void);
#endif // UPROC_RISCV

extern SE_STATUS seSelwreBusWriteRegisterErrChk(LwU32, LwU32);
extern SE_STATUS seSelwreBusReadRegisterErrChk(LwU32, LwU32*);
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Defines ----------------------------------------- */
#define DP_MST_MAX_TIME_SLOTS       (64)
#define DP_MST_ACT_TIMEOUT_US       (30) // 17 us is enough, keeping some buffer time

//
// SF registers of display are not mapped to secure bus so need to access them using BAR0
// We should not read any confidential information (intermediate keys) using this interface
//
#define hdcp22wiredBar0ReadRegister(addr, pDataOut)  DISP_REG_RD32_ERR_CHK_HS(addr, pDataOut)
#define hdcp22wiredBar0WriteRegister(addr, dataIn)   DISP_REG_WR32_ERR_CHK_HS(addr, dataIn)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Prototypes  ------------------------------------- */
#if defined(HDCP22_SUPPORT_MST)
static FLCN_STATUS _hdcp22SetDpLockEcf_v04_01(LwU8 sorIndex)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SetDpLockEcf_v04_01");

    #if defined(HDCP22_WAR_ECF_SELWRE_ENABLED)
static LwU64 _arithLeftShiftLwU64Hs(LwU64 a, LwU64 b)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_arithShiftLeftLwU64Hs");
static FLCN_STATUS _hdcp22WiredLockBeforeProcessingEcf(LwU8 sorIndex, LwU8 headIndex, LwBool bAct)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredLockBeforeProcessingEcf");
static LW_STATUS _hdcp22WiredUnlockAfterEcfRequest(LwU8 sorIndex, LwU8 headIndex, LwBool bUnlockTimeSlot, LwBool bAct)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredUnlockAfterEcfRequest");
static FLCN_STATUS _hdcp22WiredGetTimeSlotForHead(LwU32 timeSlotAddress, LwU64 *pTimeSlot, LwU8 *pTimeSlotStart, LwU8 *pTimeSlotLength, LwBool bIsTimeSlotModified)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredGetTimeSlotForHead");
static FLCN_STATUS _hdcp22WiredTriggerACT(LwU8 sorIndex)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredTriggerACT");
static FLCN_STATUS _hdcp22WiredSetTimeSlotForHead(LwU32 timeSlotAddress, LwU8 timeSlotStart, LwU8 timeSlotLength)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredSetTimeSlotForHead");
static void _hdcp22WiredMstGetRequestType(LwU64 timeSlotMask, LwU64 ecfReq, LwU64  ecfLwrValue, HDCP22_MST_ENC_REQ *pEcfReqType)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredMstGetRequestType");
static FLCN_STATUS _hdcp22wiredHandleStreamEncryption(LwU8 sorIndex, LwU8 headIndex, LwU32 *pEcfTimeSlot, LwBool  bForceClearEcf, LwBool  bAddStreamBack)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22wiredHandleStreamEncryption");
static FLCN_STATUS _hdcp22WiredHandleNoHeadEcfCleanUp(LwU8 maxNoOfSors, LwU8 maxNoOfHeads, LwU8 sorIndex, LwU8 *pHeadOwnerMask)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22WiredHandleNoHeadEcfCleanUp");
    #endif // HDCP22_WAR_ECF_SELWRE_ENABLED
#endif // HDCP22_SUPPORT_MST

/* ------------------------ Private Functions ------------------------------- */
#if defined(HDCP22_SUPPORT_MST)
/*!
 * @brief From Ampere onwards ECF bit is priv protected, rising priv level to L3 only.
 *
 * @param[in] sorIndex    sor index number
 *
 * @returns   FLCN_STATUS FLCN_OK on successfull exelwtion
 *                        Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22SetDpLockEcf_v04_01
(
    LwU8    sorIndex
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32 = 0;

    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_ECF_PRIV_LEVEL_MASK(sorIndex), &data32));

    //
    // Set ecf lock (raise prive level write protection to L3) that ECF register can only be written by secure SW
    // Never unlock/downgrade  it.
    //
    data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MST_ECF_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, data32);
    data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MST_ECF_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _GSP, data32);

    CHECK_STATUS(hdcp22wiredWriteRegister(LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_ECF_PRIV_LEVEL_MASK(sorIndex), data32));

label_return:
    return status;
}

#if defined(HDCP22_WAR_ECF_SELWRE_ENABLED)
static LwU64 _arithLeftShiftLwU64Hs(LwU64 a, LwU64 b)
{
    const int bits_in_word = (int)(sizeof(LwU32) * 8);
    LwU32 low = 0;
    LwU32 high = 0;

    if (b >= bits_in_word)
    {
        // bits_in_word <= b < bits_in_dword
        low = 0;
        high = ((LwU32)a) << (b - bits_in_word);
    }
    else
    {
        // 0 <= b < bits_in_word
        if (b == 0)
        {
            return a;
        }

        low  = ((LwU32)a) << b;
        high = (((LwU32)(a>>bits_in_word)) << b) | (((LwU32)a) >> (bits_in_word - b));
    }

    return ((LwU64)high << bits_in_word) | low ;
}

/*!
 * @brief This function Locks/rises priv level mask of Timeslot and ACT register before processing ECF request
 *
 * @param[in] sorIndex          sor index number
 * @param[in] headIndex         head index number
 * @param[in] bAct              If this flag is set it will lock ACT register otherwise locks time slot register
 *
 * @return                      Return FLCN_OK if request is handled successfully
 */

static FLCN_STATUS
_hdcp22WiredLockBeforeProcessingEcf
(
    LwU8    sorIndex,
    LwU8    headIndex,
    LwBool  bAct
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32 = 0;
    LwU8        tickCount = 0;

    if (bAct)
    {
        //
        // Lock ACT Register by disabling write access to L0 and Source Id is already restricted to GSP and CPU
        // So, after locking only GSP LS/HS code can access
        //
        CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK(sorIndex), &data32));
        data32 =  FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, data32);
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK(sorIndex), data32));
        // Poll LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_CTRL_TRIG_IMMEDIATE = DONE, to make sure ongoing requests are done
        CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MS_CTL(sorIndex), &data32));
        while (!FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MS_CTL, _TRIG_IMMEDIATE, _DONE, data32))
        {
            CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MS_CTL(sorIndex), &data32));
            osPTimerCondSpinWaitNs_hs(NULL, NULL, 1000U);
            tickCount ++;
            if (tickCount > DP_MST_ACT_TIMEOUT_US)
            {
                // Time out waiting for ACT to be done
                status = FLCN_ERR_TIMEOUT;
                break;
            }
        }
    }
    else
    {
        ///
        // Lock Time Slot Register by disabling write access to L0 and Source Id is already restricted to GSP and CPU
        // So, after locking only GSP LS/HS code can access
        //
        CHECK_STATUS(hdcp22wiredBar0ReadRegister(LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK(headIndex), &data32));
        data32 = FLD_SET_DRF(_PDISP, _SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, data32);
        hdcp22wiredBar0WriteRegister(LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK(headIndex), data32);
    }

label_return:
    return status;
}

/*!
 * @brief This function unlocks/reduces priv level mask of Timeslot and ACT register after processing ECF request
 *        Time slot register will only unlocked if corresponding bits of ECF are cleared
 *
 * @param[in] sorIndex          sor index number
 * @param[in] headIndex         head index number
 * @param[in] bUnlockTimeSlot   If this is not set it will not try to unlock time slots
 * @param[in] bAct              If this flag is set it will unlock ACT register otherwise unlocks time slot register
 *
 * @return                      Return FLCN_OK if request is handled successfully
 */
static LW_STATUS
_hdcp22WiredUnlockAfterEcfRequest
(
    LwU8    sorIndex,
    LwU8    headIndex,
    LwBool  bUnlockTimeSlot,
    LwBool  bAct
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data32  = 0;
    LwU64       timeSlotMask_0 = 0;
    LwU64       timeSlotMask_1 = 0;
    LwU64       ecfLwrVal;

    if (!bAct)
    {
        if (bUnlockTimeSlot)
        {
             // Unlock Time Slot Register
             CHECK_STATUS(_hdcp22WiredGetTimeSlotForHead(LW_PDISP_SF_DP_STREAM_CTL(headIndex), &timeSlotMask_0, NULL, NULL, LW_TRUE));
             CHECK_STATUS(_hdcp22WiredGetTimeSlotForHead(LW_PDISP_SF_DP_2ND_STREAM_CTL(headIndex), &timeSlotMask_1, NULL, NULL, LW_TRUE));

             CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), &data32));
             ecfLwrVal = data32;
             CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), &data32));
             ecfLwrVal = ((LwU64)(data32) << 32) | ecfLwrVal;

             if (!((timeSlotMask_0 | timeSlotMask_1) & ecfLwrVal))
             {
                 // Unlock Time Slot register
                 CHECK_STATUS(hdcp22wiredBar0ReadRegister(LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK(headIndex), &data32));
                 data32 = FLD_SET_DRF(_PDISP, _SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
                 hdcp22wiredBar0WriteRegister(LW_PDISP_SF_DP_MST_STREAM_CTL_PRIV_LEVEL_MASK(headIndex), data32);
             }
         }
    }
    else
    {
         // Unlock ACT Register
         CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK(sorIndex), &data32));
         data32 =  FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
         CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_MS_CTL_PRIV_LEVEL_MASK(sorIndex), data32));
    }

 label_return:
    return status;
}

/*!
 * @brief This function gets time slot information for a given head and also does some sanity
 *
 * @param[in]   timeSlotAddress   Address of time slot of a head
 * @param[in]   bTimeSlotModified If this is FALSE then it expects active and programmed time slots to be same
 *
 * @param[out]  pTimeSlot         Time slot mask of a given head
 * @param[out]  pTimeSlotStart    Time slot start of a given head
 * @param[out]  pTimeSlotLength   Time slot length of a given head
 *
 * @return                        Return FLCN_OK if request is handled successfully
 */
static FLCN_STATUS
_hdcp22WiredGetTimeSlotForHead
(
    LwU32   timeSlotAddress,
    LwU64   *pTimeSlot,
    LwU8    *pTimeSlotStart,
    LwU8    *pTimeSlotLength,
    LwBool  bIsTimeSlotModified
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32 = 0;
    LwU8        start = 0;
    LwU8        startActive = 0;
    LwU8        length = 0;
    LwU8        lengthActive = 0;

    CHECK_STATUS(hdcp22wiredBar0ReadRegister(timeSlotAddress, &data32));

    start = DRF_VAL(_PDISP, _SF_DP_STREAM_CTL, _START, data32);
    length = DRF_VAL(_PDISP, _SF_DP_STREAM_CTL, _LENGTH, data32);

    startActive = DRF_VAL(_PDISP, _SF_DP_STREAM_CTL, _START_ACTIVE, data32);
    lengthActive = DRF_VAL(_PDISP, _SF_DP_STREAM_CTL, _LENGTH_ACTIVE, data32);

    if (((bIsTimeSlotModified != LW_TRUE) && ( start != startActive || length != lengthActive)) ||
        ((startActive + lengthActive) > DP_MST_MAX_TIME_SLOTS))
    {
        status = FLCN_ERROR;
        goto label_return;
    }

    if (pTimeSlot != NULL)
    {
        if ((lengthActive == 0) || (startActive == 0))
        {
            *pTimeSlot = 0;
        }
        else
        {
            LwU8 startSlotBit;
            LwU64 maskValue;

            //
            // Time Slot Mask with respect to ECF0/ECF1 registers,
            // Time slot 0 is reserved for MTPH and ECF0 register's 0th bit is correspoding to Time slot1
            // ECF1 register's 31st bit is for triggering/applying new settings
            // *pTimeSlot will have ECF register mask here, that is the reason for "startActive -1"
            //
            // *pTimeSlot = (LwU64)(((LwU64)(0x1) << lengthActive) - 0x1) << (startActive-1);
            startSlotBit = (LwU8)(startActive - 1);
            maskValue = _arithLeftShiftLwU64Hs(0x1, lengthActive) - 1;
            *pTimeSlot = _arithLeftShiftLwU64Hs(maskValue, startSlotBit);
        }
    }

    if (pTimeSlotStart != NULL)
    {
        *pTimeSlotStart = startActive;
    }

    if (pTimeSlotLength != NULL)
    {
        *pTimeSlotLength = lengthActive;
    }

label_return:
    return status;
}

/*!
 * @brief This function triggers ACT on a SOR and waits for it to take affect
 *        ECF and time slots will be set applied to HW by triggering ACT
 *
 * @param[in]  sorIndex        sor number for which ACT need to be triggered
 *
 * @return                     Return FLCN_OK if request is handled successfully
 */
static FLCN_STATUS
_hdcp22WiredTriggerACT
(
    LwU8    sorIndex
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data32  = 0;
    LwU8        tickCount = 0;

    // Trigger ACT
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MS_CTL(sorIndex), &data32));
    data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MS_CTL, _TRIG_IMMEDIATE, _TRIGGER, data32);
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MS_CTL(sorIndex), data32));

    // Poll LW_SSE_SE_SWITCH_DISP_SOR_DP_MST_CTRL_TRIG_IMMEDIATE = DONE
    do
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_MS_CTL(sorIndex), &data32));
        osPTimerCondSpinWaitNs_hs(NULL, NULL, 1000U);
        tickCount ++;
        if (tickCount > DP_MST_ACT_TIMEOUT_US)
        {
            // Time out waiting for ACT to be done
            status = FLCN_ERR_TIMEOUT;
            break;
        }

    } while (!FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_MS_CTL, _TRIG_IMMEDIATE, _DONE, data32));

label_return:
    return status;
}

/*!
 * @brief This function programs time slots of a given head with requested start and length
 *
 * @param[in]  timeSlotAddress Address of time slot of a head
 * @param[in]  timeSlotStart   Start of time slot that needs to be programmed
 * @param[in]  timeSlotLength  Length of time slot that needs to be programmed
 *
 * @return                     Return FLCN_OK if request is handled successfully
 */
static FLCN_STATUS
_hdcp22WiredSetTimeSlotForHead
(
    LwU32   timeSlotAddress,
    LwU8    timeSlotStart,
    LwU8    timeSlotLength
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32 = 0;

    CHECK_STATUS(hdcp22wiredBar0ReadRegister(timeSlotAddress, &data32));

    data32 = FLD_SET_DRF_NUM(_PDISP, _SF_DP_STREAM_CTL, _START, timeSlotStart, data32);
    data32 = FLD_SET_DRF_NUM(_PDISP, _SF_DP_STREAM_CTL, _LENGTH, timeSlotLength, data32);

    CHECK_STATUS(hdcp22wiredBar0WriteRegister(timeSlotAddress, data32));

label_return:
    return status;
}

/*!
 * @brief This function gets the HDCP22 MST encryption request type.
 *        If it is Enable/Disable/No Change/No Time SLots/Invalid
 *
 * @param[in]  timeslotMask    Mask array of timeSlots of a head
 * @param[in]  ecfReq          Mask array of ECF timeSlots requested
 * @param[in]  ecfLwrValue     Mask array of ECF timeSlots lwrrently set
 *
 * @param[out] pEcfReqType     It sends request type to the caller
 *
 * @return                     Return FLCN_OK if request is handled successfully
 */
static void
_hdcp22WiredMstGetRequestType
(
    LwU64               timeSlotMask,
    LwU64               ecfReq,
    LwU64               ecfLwrVal,
    HDCP22_MST_ENC_REQ  *pEcfReqType
)
{
    if (timeSlotMask != 0)
    {
        if ((ecfReq & timeSlotMask) == (ecfLwrVal & timeSlotMask))
        {
            *pEcfReqType = HDCP22_MST_ENC_NO_CHANGE;
        }
        else if ((ecfReq & timeSlotMask) == timeSlotMask)
        {
            // All time slots of this stream are requested to enable encryption
            *pEcfReqType = HDCP22_MST_ENC_ENABLE;
        }
        else if ((ecfReq & timeSlotMask) == 0)
        {
            // All time slots of this stream are requested to disable encryption
            *pEcfReqType = HDCP22_MST_ENC_DISABLE;
        }
        else
        {
            //
            // If Requested ECF for this stream is neither all 0's nor all 1's
            // This is not a valid request to the head.
            //
            *pEcfReqType = HDCP22_MST_ENC_ENABLE_MISMATCH;
        }
    }
    else
    {
        *pEcfReqType = HDCP22_MST_ENC_NO_ACTIVE_TIMESLOTS;
    }
}

/*!
 * @brief Handle a case where Head is not attached to SOR and clean up all unattached heads.
 *        This function prepares detached head mask for further processing
 *
 * @param[in]  maxNoOfSors      Max number of sors
 * @param[in]  maxNoOfHeads     Max number of heads
 * @param[in]  sorIndex         SOR index number
 * @param[out] pHeadOwnerMask   Prepares detached head owner mask.
 *
 * @return                      Return FLCN_OK if request is handled successfully
 */
static FLCN_STATUS
_hdcp22WiredHandleNoHeadEcfCleanUp
(
    LwU8    maxNoOfSors,
    LwU8    maxNoOfHeads,
    LwU8    sorIndex,
    LwU8    *pHeadOwnerMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        headOwnerMask = 0;
    LwU8        sorNum = 0;
    LwU32       data32 = 0;

    for (sorNum = 0; sorNum < maxNoOfSors; sorNum++)
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_TEST(sorNum), &data32));
        headOwnerMask = headOwnerMask | DRF_VAL(_SSE_SE_SWITCH_DISP, _SOR_TEST, _OWNER_MASK, data32);
    }

    *pHeadOwnerMask = (~headOwnerMask) & ((0x1 << maxNoOfHeads) - 0x1);

label_return:
    return status;
}

/*!
 * @brief Handle ECF request of a head selwrely
 *
 * @param[in] sorIndex        sor index number
 * @param[in] headIndex       head index number
 * @param[in] pEcfTimeslot    Mask array of timeSlots requiring encryption
 * @param[in] bForceClearEcf  If set, will delete time slots and then clear ECF
 * @param[in] bAddStreamBack  If set, will add back streams after deletion,
 *                            this is only valid if bForceClearEcf is set
 *
 * @return                    Return FLCN_OK if request is handled successfully
 */
static FLCN_STATUS
_hdcp22wiredHandleStreamEncryption
(
    LwU8    sorIndex,
    LwU8    headIndex,
    LwU32   *pEcfTimeSlot,
    LwBool  bForceClearEcf,
    LwBool  bAddStreamBack
)
{
    FLCN_STATUS         status  = FLCN_OK;
    HDCP22_MST_ENC_REQ  ecfReqType = HDCP22_MST_ENC_ILWALID;
    LwU64               ecfReq = ((LwU64)(pEcfTimeSlot[1]) << 32) | pEcfTimeSlot[0];
    LwU64               ecfLwrVal = 0;
    LwU32               data32 = 0;
    LwU64               timeSlotMask_0 = 0;
    LwU64               timeSlotMask_1 = 0;
    LwBool              bIsHdcp22EncEnableOnBranchDevice = LW_TRUE;
    LwU64               ecfValToBeProgrammed = 0;
    LwU8                timeSlotStart_0 = 0;
    LwU8                timeSlotLength_0 = 0;
    LwU8                timeSlotStart_1 = 0;
    LwU8                timeSlotLength_1 = 0;
    LwBool              bUnlockTimeSlot = LW_TRUE;

    // First thing is to Lock Time slot registers of given head
    CHECK_STATUS(_hdcp22WiredLockBeforeProcessingEcf(sorIndex, headIndex, LW_FALSE));
    // Check hdcp22 encryption status on branch device
    CHECK_STATUS(hdcp22wiredEncryptionActiveHs_HAL(&Hdcp22wired, sorIndex, &bIsHdcp22EncEnableOnBranchDevice));

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), &data32));
    ecfLwrVal = data32;
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), &data32));
    ecfLwrVal = ((LwU64)(data32) << 32) | ecfLwrVal;

    // Read primary time slot of a given head
    CHECK_STATUS(_hdcp22WiredGetTimeSlotForHead(LW_PDISP_SF_DP_STREAM_CTL(headIndex), &timeSlotMask_0, &timeSlotStart_0, &timeSlotLength_0, LW_FALSE));
    // In case of DUAL MST second stream will be used
    CHECK_STATUS(_hdcp22WiredGetTimeSlotForHead(LW_PDISP_SF_DP_2ND_STREAM_CTL(headIndex), &timeSlotMask_1, &timeSlotStart_1, &timeSlotLength_1, LW_FALSE));

    // Get request type for primary and secondary stream
    _hdcp22WiredMstGetRequestType((timeSlotMask_0 | timeSlotMask_1), ecfReq, ecfLwrVal, &ecfReqType);

    // This is enable encryption request on a stream
    if (ecfReqType == HDCP22_MST_ENC_ENABLE)
    {
        ecfValToBeProgrammed = (ecfLwrVal | (timeSlotMask_0 | timeSlotMask_1));

        data32 = (LwU32)ecfValToBeProgrammed;
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), data32));
        data32 = (LwU32)(ecfValToBeProgrammed >> 32);
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), data32));
        bUnlockTimeSlot = LW_FALSE;
    }
    // This is Disable encryption request on a stream
    else if (ecfReqType == HDCP22_MST_ENC_DISABLE)
    {
        //
        // Force clear is only set in case of Detach Stream or Flush mode where ECF has to be cleared
        // In these cases stream deletion will not affect user experience as any way streams will be deleted
        //
        if (bForceClearEcf == LW_TRUE)
        {
            // Clear Time Slots along with ECF to avoid pixel leak
            _hdcp22WiredSetTimeSlotForHead(LW_PDISP_SF_DP_STREAM_CTL(headIndex), 1, 0);
            _hdcp22WiredSetTimeSlotForHead(LW_PDISP_SF_DP_2ND_STREAM_CTL(headIndex), 1, 0);

            // Clear ECF
            ecfValToBeProgrammed = (ecfLwrVal & (~(timeSlotMask_0 | timeSlotMask_1)));
            data32 = (LwU32)ecfValToBeProgrammed;
            CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), data32));
            data32 = (LwU32)(ecfValToBeProgrammed >> 32);
            CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), data32));

            //Trigger ACT
            CHECK_STATUS(_hdcp22WiredTriggerACT(sorIndex));

            // In case of QSES failure we clear ECF, we need to add back stream for user experience
            if (bAddStreamBack == LW_TRUE)
            {
                 //Restore the Time Slots back and send ACT again
                _hdcp22WiredSetTimeSlotForHead(LW_PDISP_SF_DP_STREAM_CTL(headIndex), timeSlotStart_0, timeSlotLength_0);
                _hdcp22WiredSetTimeSlotForHead(LW_PDISP_SF_DP_2ND_STREAM_CTL(headIndex), timeSlotStart_1, timeSlotLength_1);

                //Trigger ACT
                CHECK_STATUS(_hdcp22WiredTriggerACT(sorIndex));
            }
        }
        else
        {
            //
            // If it is not HDCP22 then just clear ECF without delting Time SLots
            // In case of HDCP22, ECF clear is ignored until unless it is a force clear
            // In case of HDCP1.X Reauth, ECF has to be cleared without clearing time slots
            // For HDCP22 it can be cleared while reauth in ucode.
            //
            if (!bIsHdcp22EncEnableOnBranchDevice)
            {
                // Clear ECF
                ecfValToBeProgrammed = (ecfLwrVal & (~(timeSlotMask_0 | timeSlotMask_1)));
                data32 = (LwU32)ecfValToBeProgrammed;
                CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), data32));
                data32 = (LwU32)(ecfValToBeProgrammed >> 32);
                CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), data32));

                //Trigger ACT
                CHECK_STATUS(_hdcp22WiredTriggerACT(sorIndex));
            }
            else
            {
                bUnlockTimeSlot = LW_FALSE;
            }
        }
    }
    else
    {
        if (ecfReqType == HDCP22_MST_ENC_ENABLE_MISMATCH)
        {
            status = FLCN_ERR_HDCP22_ECF_TIMESLOT_MISMATCH;
        }
        else if (ecfReqType != HDCP22_MST_ENC_NO_ACTIVE_TIMESLOTS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }
        goto label_return;
    }

label_return:

    (void)_hdcp22WiredUnlockAfterEcfRequest(sorIndex, headIndex, bUnlockTimeSlot, LW_FALSE);
    return status;
}
    #endif // HDCP22_WAR_ECF_SELWRE_ENABLED
#endif // HDCP22_SUPPORT_MST

/* ------------------------- Public Functions ------------------------------- */
#ifdef HDCP22_SUPPORT_MST
/*!
 * @brief Initialze priv level of ECF register and Scratch0 regsiter.
 *
 * @return                  Return FLCN_OK if request is handled successfully
 */

FLCN_STATUS
hdcp22wiredInitSorHdcp22CtrlReg_v04_01(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        sorIndex;
    LwU8        maxNoOfSors = 0;
#ifdef HDCP22_WAR_ECF_SELWRE_ENABLED
    LwU32       data32 = 0;
#endif

    CHECK_STATUS(hdcp22wiredGetMaxNoOfSorsHeads_HAL(&Hdcp22wired, &maxNoOfSors, NULL));

    //
    // Set that ECF register so that it can be written by only protected SW (GSP).
    // Default is all levels enabled and set to write L3 protected after GSP loaded.
    //
    for (sorIndex = 0; sorIndex < maxNoOfSors; sorIndex ++ )
    {
        CHECK_STATUS(_hdcp22SetDpLockEcf_v04_01(sorIndex));
    }

#ifdef HDCP22_WAR_ECF_SELWRE_ENABLED
    //
    // Lower the priv level of LW_SSE_SE_SWITCH_DISP_SEC_SELWRE_SCRATCH0 register to L0 Read/Write and source enable to all,
    // So that it can be used for ECF response
    //
    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SEC_SELWRE_SCRATCH0_PRIV_LEVEL_MASK(0), &data32));
    data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SEC_SELWRE_SCRATCH0_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, data32);
    data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SEC_SELWRE_SCRATCH0_PRIV_LEVEL_MASK, _READ_PROTECTION, _ALL_LEVELS_ENABLED, data32);
    data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SEC_SELWRE_SCRATCH0_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _GSP_CPU, data32);
    CHECK_STATUS(hdcp22wiredWriteRegister(LW_SSE_SE_SWITCH_DISP_SEC_SELWRE_SCRATCH0_PRIV_LEVEL_MASK(0), data32));
#endif // HDCP22_WAR_ECF_SELWRE_ENABLED

label_return:
    return status;
}

/*!
 * @brief Clear ECF of a SOR after disabling encryption at SOR level.
 *
 * @param[in]  sorIndex     sor index number
 * @param[in]  maxNoOfSors  Max number of sors
 * @param[in]  maxNoOfHeads Max number of heads
 *
 * @return                  Return FLCN_OK if request is handled successfully
 */
FLCN_STATUS
hdcp22wiredClearDpEcf_v04_01
(
    LwU8    maxNoOfSors,
    LwU8    maxNoOfHeads,
    LwU8    sorIndex
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       ecfTimeSlot[2] = {0x00, 0x00};
    LwU32       ecfLwrVal[2];
    LwBool      bIsHdcp22EncEnableOnBranchDevice = LW_TRUE;

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex),
                                           &ecfLwrVal[0]));
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex),
                                           &ecfLwrVal[1]));

    // Only try to clear ECF when it's not cleared.
    if ((ecfLwrVal[0] != 0) || (ecfLwrVal[1] != 0))
    {
        CHECK_STATUS(hdcp22wiredEncryptionActiveHs_HAL(&Hdcp22wired, sorIndex,
                                                       &bIsHdcp22EncEnableOnBranchDevice));

        // If HDCP22 is already dsiabled at SOR level, we can can clear ECF and trigger
        if (!bIsHdcp22EncEnableOnBranchDevice)
        {
            CHECK_STATUS(hdcp22wiredSelwreWriteDpEcf_HAL(&Hdcp22wired, maxNoOfSors,
                                                         maxNoOfHeads, sorIndex,
                                                         ecfTimeSlot, LW_FALSE,
                                                         LW_FALSE, LW_TRUE));
        }
        else
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

 label_return:
    return status;
}

/*!
 * @brief Writes the 2 32 bits ecf value to HW selwrely. The ecf value contains a mask of
 *        which timeSlots require encryption.
 *
 * @param[in] maxNoOfSors       Max number of sors
 * @param[in] maxNoOfHeads      Max number of heads
 * @param[in] sorIndex          sor index number
 * @param[in] pEcfTimeslot      Mask array of timeSlots requiring encryption
 * @param[in] bForceClearEcf    If set, will delete time slots and then clear ECF
 * @param[in] bAddStreamBack    If set, will add back streams after deletion,
 *                              this is only valid if bForceClearEcf is set
 * @param[in] bInternalReq      This indicates if it is internal request or not
 *
 * @return                      Return FLCN_OK if request is handled successfully
 */
FLCN_STATUS
hdcp22wiredSelwreWriteDpEcf_v04_01
(
    LwU8    maxNoOfSors,
    LwU8    maxNoOfHeads,
    LwU8    sorIndex,
    LwU32  *pEcfTimeslot,
    LwBool  bForceClearEcf,
    LwBool  bAddStreamBack,
    LwBool  bInternalReq
)
{
 #if defined(HDCP22_WAR_ECF_SELWRE_ENABLED)
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32 = 0;
    LwU8        headIndex = 0;
    LwU8        headOwnerMask = 0;

    // Lock ACT
    CHECK_STATUS(_hdcp22WiredLockBeforeProcessingEcf(sorIndex, 0, LW_TRUE));

    // Get Heads attached to the given SOR
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_TEST(sorIndex), &data32));
    headOwnerMask = DRF_VAL(_SSE_SE_SWITCH_DISP, _SOR_TEST, _OWNER_MASK, data32);

    if (headOwnerMask == 0)
    {
        //
        // This is clean up case where heads are already detached from SOR
        // Still we need to clear ECF and unlock time slot registers
        //
        if (*pEcfTimeslot == 0)
        {
            // It will get head mask with no owners, these heads needs to be cleared if ECF is set
            CHECK_STATUS(_hdcp22WiredHandleNoHeadEcfCleanUp(maxNoOfSors, maxNoOfHeads, sorIndex, &headOwnerMask));
            bForceClearEcf = LW_TRUE;
        }
        else
        {
           status = FLCN_ERR_ILWALID_ARGUMENT;
           goto label_return;
        }
    }

    for (headIndex = 0; headIndex < maxNoOfHeads; headIndex++)
    {
        if (headOwnerMask & ((LwU8)(0x1) << headIndex))
        {
            // Handle ECF request for each head index assocaited with this SOR
            status = _hdcp22wiredHandleStreamEncryption(sorIndex, headIndex, pEcfTimeslot, bForceClearEcf, bAddStreamBack);
            if (status == FLCN_ERR_HDCP22_ECF_TIMESLOT_MISMATCH)
            {
                //
                // If requesting ECF doesn't match to head's timeslot, it's possible the SOR's other head case.
                // continue to check other heads till all heads checked.
                //
                continue;
            }
        }
     }

label_return:
     // Unlock ACT
     (void)_hdcp22WiredUnlockAfterEcfRequest(sorIndex, 0, LW_FALSE, LW_TRUE);

     if (!bInternalReq)
     {
         (void)hdcp22wiredBar0ReadRegister(LW_PDISP_SEC_SELWRE_SCRATCH0_0, &data32);
         data32 = FLD_SET_DRF(_PDISP, _SEC_SELWRE_SCRATCH0_0, _ECF_REQUEST_STATUS, _DONE, data32);
         (void)hdcp22wiredBar0WriteRegister(LW_PDISP_SEC_SELWRE_SCRATCH0_0, data32);
     }

    return status;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif // HDCP22_WAR_ECF_SELWRE_ENABLED
}
#endif

/*!
 * @brief Read an register using the DIO
 *
 * @param[in]  addr     Address
 * @param[out] pData Data out for read request
 *
 * @return FLCN_OK if request is successful
 */
FLCN_STATUS
hdcp22wiredDioSeReadHs
(
    LwU32       addr,
    LwU32      *pData
)
{
    // Only FMC config liblwriscv supports DIO_SE.
#if LWRISCV_HAS_DIO_SE
    DIO_PORT dioPort;

    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    dioReadWrite(dioPort, DIO_OPERATION_RD, addr, pData);
    return FLCN_OK;
#else
    // RISCV-V RTOS partition should not call the func.
    return FLCN_ERR_ILLEGAL_OPERATION;
#endif
}

/*!
 * @brief Write an register using the DIO
 *
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written
 *
 * @return FLCN_OK if request is successful
 */
FLCN_STATUS
hdcp22wiredDioSeWriteHs
(
    LwU32       addr,
    LwU32       val
)
{
    // Only FMC config liblwriscv supports DIO_SE.
#if LWRISCV_HAS_DIO_SE
    DIO_PORT dioPort;

    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    dioReadWrite(dioPort, DIO_OPERATION_WR, addr, &val);
    return FLCN_OK;
#else
    // RISCV-V RTOS partition should not call the func.
    return FLCN_ERR_ILLEGAL_OPERATION;
#endif
}

