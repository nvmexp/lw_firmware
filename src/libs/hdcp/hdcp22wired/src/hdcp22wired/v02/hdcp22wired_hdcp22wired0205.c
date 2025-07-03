/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_hdcp22wired0205.c
 * @brief  Hdcp22 wired 02.05 Hal Functions
 *
 * HDCP2.2 Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes --------------------------------- */
#include "dev_disp.h"
#include "osptimer.h"

/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_timer.h"
#include "setypes.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */


/* ------------------------ Defines ----------------------------------------- */
#define SUBLINK_MASK_LINK_A         0x1
#define SUBLINK_MASK_LINK_B         0x2
#define SUBLINK_MASK_LINK_AB        0x3

/* ------------------------ Global Variables -------------------------------- */
HDCP22_SELWRE_MEMORY  hdcp22SelwreMemory
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "hdcp22SelwreMemory")
    GCC_ATTRIB_ALIGN(HDCP22_INTEGRITY_ALIGNMENT);    // Alignment for HW integrity computation

/* ------------------------ Prototypes  ------------------------------------- */
#if defined(DPU_RTOS) || defined(UPROC_RISCV)
static void _hdcp22wiredWriteMstKs_v02_05(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 *pKs)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "_hdcp22wiredWriteMstKs_v02_05");
static void _hdcp22WriteMstRiv_v02_05(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 bytesToWrite0, LwU32 bytesToWrite1)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "_hdcp22WriteMstRiv_v02_05");
static void _hdcp22SetDpLockEcf_v02_05(LwU32 sorIndex, LwU32 linkIndex, LwBool lock)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SetDpLockEcf_v02_05");
static FLCN_STATUS _hdcp22wiredWriteMstStreamType_v02_05(LwU8 sorNum, LwU8 linkIndex, HDCP22_STREAM streamIdType[HDCP22_NUM_STREAMS_MAX], LwU32 dpTypeMask[HDCP22_NUM_DP_TYPE_MASK])
    GCC_ATTRIB_SECTION("imem_hdcp22ControlEncryption", "_hdcp22wiredWriteMstStreamType_v02_05");

FLCN_STATUS hdcp22wiredReadAuditTimer_v02_05(LwU32* pLwrrentTimeUs)
    GCC_ATTRIB_SECTION("imem_resident", "hdcp22wiredReadAuditTimer_v02_05");            // Put to resident for timer callback handler.
#endif // DPU_RTOS

#ifdef HDCP22_USE_SCP_ENCRYPT_SECRET
FLCN_STATUS hdcp22wiredReadConfidentialSecretXor(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredReadConfidentialSecretXor");
FLCN_STATUS hdcp22wiredWriteConfidentialSecretXor(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredWriteConfidentialSecretXor");
#else
FLCN_STATUS hdcp22wiredReadConfidentialSecretXor(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredReadConfidentialSecretXor");
FLCN_STATUS hdcp22wiredWriteConfidentialSecretXor(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredWriteConfidentialSecretXor");
#endif
FLCN_STATUS hdcp22wiredWriteToSelwreMemory_v02_05(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret, LwBool bIsReset)
    GCC_ATTRIB_SECTION("imem_hdcp22SelwreAction", "hdcp22wiredWriteToSelwreMemory_v02_05");
FLCN_STATUS hdcp22wiredReadFromSelwreMemory_v02_05(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret)
    GCC_ATTRIB_SECTION("imem_hdcp22SelwreAction", "hdcp22wiredReadFromSelwreMemory_v02_05");
FLCN_STATUS hdcp22wiredSaveSessionSecrets_v02_05(LwU8 sorNum)
    GCC_ATTRIB_SECTION("imem_hdcp22SelwreAction", "hdcp22wiredSaveSessionSecrets_v02_05");
FLCN_STATUS hdcp22wiredRetrieveSessionSecrets_v02_05(LwU8 sorNum)
    GCC_ATTRIB_SECTION("imem_hdcp22SelwreAction", "hdcp22wiredRetrieveSessionSecrets_v02_05");

/* ------------------------- Private Functions ------------------------------- */
#if defined(DPU_RTOS) || defined(UPROC_RISCV)
/*!
 * @brief      This function writes Ks value for MST case
 *
 * @param[in]  sorNum       SOR number index
 * @param[in]  linkIndex    link index number
 * @param[in]  bIsMst       flag to tell if DP MST device
 * @param[in]  pKs          Pointer to Ks values
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
static void
_hdcp22wiredWriteMstKs_v02_05
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pKs
)
{
#ifdef HDCP22_SUPPORT_MST
    if (bIsMst)
    {
        //
        // From our HDCP 2.2 IAS, we have two ciphers running in parallel in
        // MST mode. One cipher is used for timeslots encrypted as Type 0 and
        // one cipher is used for timeslots encrypted as Type 1. Both ciphers
        // advance in lockstep. We need program Riv to both ciphers and hw
        // decide which cipher to use according to type.
        //
        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB1(sorNum, !linkIndex),
                      pKs[0]);

        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB2(sorNum, !linkIndex),
                      pKs[1]);

        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB3(sorNum, !linkIndex),
                      pKs[2]);

        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_MSB(sorNum, !linkIndex),
                      pKs[3]);
    }
#endif
}

/*!
 * @brief      This function writes Riv value for MST case
 *
 * @param[in]  sorNum           SOR number index
 * @param[in]  linkIndex        link index number
 * @param[in]  bIsMst           flag to tell if DP MST
 * @param[in]  bytesToWrite0    MSB value to write reg
 * @param[in]  bytesToWrite1    LSB value to write reg
 *
 */
static void
_hdcp22WriteMstRiv_v02_05
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32   bytesToWrite0,
    LwU32   bytesToWrite1
)
{
#ifdef HDCP22_SUPPORT_MST
    if (bIsMst)
    {
        //
        // From our HDCP 2.2 IAS, we have two ciphers running in parallel in
        // MST mode. One cipher is used for timeslots encrypted as Type 0 and
        // one cipher is used for timeslots encrypted as Type 1. Both ciphers
        // advance in lockstep. We need program Riv to both ciphers and hw
        // decide which cipher to use according to type.
        //
        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_DATA_LSB(sorNum, !linkIndex),
                      bytesToWrite1);

        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_DATA_MSB(sorNum, !linkIndex),
                      bytesToWrite0);
    }
#endif
}

/*!
 * @brief Set Sor HDCP2.2 Control register _LOCK_ECF setting.
 *
 * @param[in] sorIndex  sor index number
 * @param[in] linkIndex link index number
 * @param[in] lock      enable or disable ecf protection
 *
 */
static void
_hdcp22SetDpLockEcf_v02_05
(
    LwU32       sorIndex,
    LwU32       linkIndex,
    LwBool      lock
)
{
    LwU32 data32;

    data32 = DISP_REG_RD32(LW_PDISP_SOR_HDCP22_CTRL(sorIndex, linkIndex));

    //
    // Set ecf lock that ECF register can only be written by secure SW
    // (FLCN uCode) or not.
    //
    if (lock)
    {
        data32 = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _LOCK_ECF, _LOCKED, data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _LOCK_ECF, _UNLOCKED, data32);
    }

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), data32);
}

/*!
 * @brief      This function writes cipher StreamType value
 * @param[in]  sorNum       SOR index number.
 * @param[in]  linkIndex    Link index number.
 * @param[in]  streamIdType Array of streamIdType.
 * @param[in]  dpTypeMask   Array of DP typeMask value.
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 *
 */
static FLCN_STATUS
_hdcp22wiredWriteMstStreamType_v02_05
(
    LwU8            sorNum,
    LwU8            linkIndex,
    HDCP22_STREAM   streamIdType[HDCP22_NUM_STREAMS_MAX],
    LwU32           dpTypeMask[HDCP22_NUM_DP_TYPE_MASK]
)
{
    FLCN_STATUS status = FLCN_OK;

#ifdef HDCP22_SUPPORT_MST
    // SST's streamId must be 0, while MST must not be 0.
    if (0 != streamIdType[0].streamId)
    {
        // MST type1Locked case is already processed at StartSession.
        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_DP_TYPE_LSB(sorNum), dpTypeMask[0]);
        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_DP_TYPE_MSB(sorNum), dpTypeMask[1]);
    }
    else
    {
        // Return error for SST case.
        status = FLCN_ERR_NOT_SUPPORTED;
    }
#else
    status = FLCN_ERR_NOT_SUPPORTED;
#endif

    return status;
}

#endif // DPU_RTOS

/*!
 * @brief      Read confidential secrets
 * @param[in]  pData            Pointer to output read secret.
 * @param[in]  pSecret          Pointer to confidential secret data.
 * @param[out] pRn              Pointer to random number for encryption.
 * @param[in]  size             Size to encrypt or decrypt
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredReadConfidentialSecretXor
(
    LwU32  *pData,
    LwU32  *pSecret,
    LwU32  *pRn,
    LwU32   size
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 i;

    for (i=0; i<size/sizeof(LwU32); i++)
    {
        pSecret[i] = pData[i] ^ pRn[i % HDCP22_SIZE_SESSION_CRYPT_U32];
    }

    return status;
}

/*!
 * @brief      Write confidential secrets
 * @param[in]  pData            Pointer to write secret destination.
 * @param[in]  pSecret          Pointer to confidential secret data.
 * @param[out] pRn              Pointer to random number for encryption.
 * @param[in]  size             Size to encrypt or decrypt
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredWriteConfidentialSecretXor
(
    LwU32  *pData,
    LwU32  *pSecret,
    LwU32  *pRn,
    LwU32   size
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 i;

    for (i=0; i<size/sizeof(LwU32); i++)
    {
        pData[i] = pSecret[i] ^ pRn[i % HDCP22_SIZE_SESSION_CRYPT_U32];
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------- */
#if defined(DPU_RTOS) || defined(UPROC_RISCV)
/*!
 * @brief      This function writes Ks value
 *
 * @param[in]  sorNum       SOR number index
 * @param[in]  linkIndex    link index number
 * @param[in]  bIsMst       flag to tell if DP MST device
 * @param[in]  pKs          Pointer to Ks values
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredWriteKs_v02_05
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pKs
)
{
    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB1(sorNum, linkIndex),
                  pKs[0]);

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB2(sorNum, linkIndex),
                  pKs[1]);

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB3(sorNum, linkIndex),
                  pKs[2]);

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_KEY_MSB(sorNum, linkIndex),
                  pKs[3]);

    _hdcp22wiredWriteMstKs_v02_05(sorNum, linkIndex, bIsMst, pKs);

    // preVolta CSB access always succeed.
    return FLCN_OK;
}

/*!
 * @brief      This function writes Riv value
 *
 * @param[in]  sorNum       SOR number index
 * @param[in]  linkIndex    link index number
 * @param[in]  bIsMst       flag to tell if DP MST
 * @param[in]  pRiv         Pointer to Riv
 *
 */
FLCN_STATUS
hdcp22wiredWriteRiv_v02_05
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pRiv
)
{
    LwU32 bytesToWrite0 = 0;
    LwU32 bytesToWrite1 = 0;

    swapEndianness(&bytesToWrite1, &pRiv[1], sizeof(LwU32));
    swapEndianness(&bytesToWrite0, &pRiv[0], sizeof(LwU32));

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_DATA_LSB(sorNum, linkIndex),
                  bytesToWrite1);

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_AES_CTR_DATA_MSB(sorNum, linkIndex),
                  bytesToWrite0);

    _hdcp22WriteMstRiv_v02_05(sorNum, linkIndex, bIsMst,
                              bytesToWrite0, bytesToWrite1);

    return FLCN_OK;
}

/*!
 * @brief      This function writes cipher StreamType value
 * @param[in]  sorNum       SOR index number.
 * @param[in]  linkIndex    Link index number.
 * @param[in]  streamIdType Array of streamIdType.
 * @param[in]  dpTypeMask   Array of DP typeMask value.
 * @param[in]  bIsAux       Flag to tell if DP connection.
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 *
 */
FLCN_STATUS
hdcp22wiredWriteStreamType_v02_05
(
    LwU8            sorNum,
    LwU8            linkIndex,
    HDCP22_STREAM   streamIdType[HDCP22_NUM_STREAMS_MAX],
    LwU32           dpTypeMask[HDCP22_NUM_DP_TYPE_MASK],
    LwBool          bIsAux
)
{
    FLCN_STATUS status = FLCN_OK;

    // If It is DP then need to set the Stream Type Register for cipher input.
    if (bIsAux)
    {
        status = _hdcp22wiredWriteMstStreamType_v02_05(sorNum, linkIndex,
                                                       streamIdType, dpTypeMask);
        if (status == FLCN_ERR_NOT_SUPPORTED)
        {
            //
            // SST only has stream0, and type1Lock condition is already
            // processed at StartSession.
            //
            DISP_REG_WR32(LW_PDISP_SOR_HDCP22_SST_DP_TYPE(sorNum, linkIndex),
                          streamIdType[0].streamType);
            status = FLCN_OK;
        }
    }

    return status;
}

/*!
 * @brief Checks if the link encryption is active
 * @param[in]  sorNumber sorNumber
 * @param[out] pBEncActive Pointer to fill if encryption is active
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredEncryptionActive_v02_05
(
    LwU8 sorNumber,
    LwBool *pBEncActive
)
{
    *pBEncActive = FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE,
        DISP_REG_RD32(LW_PDISP_SOR_HDCP22_STATUS(sorNumber,0)));

    // preVolta CSB access always succeed.
    return FLCN_OK;
}

/*!
 * @brief      Checks whether given link is 8 Lane DP
 * @param[in]  pSession pointer to HDCP22 Session
 * @param[out] pBSor8LaneDp Pointer to fill if sor drives 8 lane DP.
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsSorDrivesEightLaneDp_v02_05
(
    LwU8 sorNumber,
    LwBool *pBSor8LaneDp
)
{
    *pBSor8LaneDp = FLD_TEST_DRF(_PDISP, _SOR_DP_LINKCTL0, _LANECOUNT, _EIGHT,
        DISP_REG_RD32(LW_PDISP_SOR_DP_LINKCTL0(sorNumber)));

    // preVolta CSB access always succeed.
    return FLCN_OK;
}

/*!
 * @brief      This function enables HDCP22 Encryption
 * @param[in]  sorNum                       SOR number index
 * @param[in]  linkIndex                    link index number
 * @param[in]  bIsRepeater                  flag to tell if repeater
 * @param[in]  bApplyHwDrmType1LockedWar    flag to tell if applying HwDRM type1Locked WAR
* @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredHandleEnableEnc_v02_05
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsRepeater,
    LwBool  bApplyHwDrmType1LockedWar
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU32       hdcpCtrl    = 0;
    LwU32       ctrlReg     = LW_PDISP_SOR_HDCP22_CTRL(sorNum, linkIndex);
    LwU32       statusReg   = LW_PDISP_SOR_HDCP22_STATUS(sorNum, linkIndex);
    LwU32       cryptStatus = 0;
    LwU32       tickCount   = 0;

    // Trigger HDCP22 CTRL INIT
    hdcpCtrl = DISP_REG_RD32(ctrlReg);
    hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _INIT, _TRIGGER, hdcpCtrl);

    if (bIsRepeater)
    {
        hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _REPEATER, _YES, hdcpCtrl);
    }
    else
    {
        hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _REPEATER, _NO, hdcpCtrl);
    }

    DISP_REG_WR32(ctrlReg, hdcpCtrl);

    // Poll LW_PDISP_SOR_HDCP22_CTRL_INIT_DONE
    do
    {
        hdcpCtrl = DISP_REG_RD32(ctrlReg);
    } while (!FLD_TEST_DRF(_PDISP, _SOR_HDCP22_CTRL, _INIT, _DONE, hdcpCtrl));

    // Write LW_PDISP_SOR_HDCP22_CTRL_ENABLE_YES
    hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _CRYPT, _ENABLE, hdcpCtrl);
    DISP_REG_WR32(ctrlReg, hdcpCtrl);

    // Poll LW_PDISP_SOR_HDCP22_CTRL_CRYPT_STS = _ACTIVE
    cryptStatus = DISP_REG_RD32(statusReg);
    while (!FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, cryptStatus))
    {
        cryptStatus = DISP_REG_RD32(statusReg);
        HDCP22WIRED_SPIN_WAIT_US(100);
        tickCount ++;
        if (tickCount > 5000)
        {
            //
            // Encryption enablement got failed, HW unable to enable encryption.
            // if 500ms then timeout.
            //
            status = FLCN_ERR_TIMEOUT;
            break;
        }
    }

    // Move streamType scratch reg write to cmd.c.
    // TODO: Error Handling need to be done

    return status;
}

/*!
 * @brief      This function disables HDCP22 encryption
 * @param[in]  sorNUm               Sor number.
 * @param[in]  maxNoOfSors          Max number of sors
 * @param[in]  maxNoOfHeads         Max number of heads
 * @param[in]  bCheckCryptStatus    Flag to check CRYPT_STATUS reg or not.
 * @returns    FLCN_STATUS          FLCN_OK on successfull exelwtion
 *                                  Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredHandleDisableEnc_v02_05
(
    LwU8    maxNoOfSors,
    LwU8    maxNoOfHeads,
    LwU8    sorNum,
    LwBool  bCheckCryptStatus
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU32       data32     = 0;
    LwU8        totalLinks = 0;
    LwU8        linkIndex  = 0;
    LwU16       tickCount  = 0;
    LwBool      pBSor8LaneDp;

    // Check whether we also need to disable it for secondary link.
    CHECK_STATUS(hdcp22wiredIsSorDrivesEightLaneDp_HAL(&Hdcp22wired, sorNum, &pBSor8LaneDp));
    totalLinks = pBSor8LaneDp ? 2 : 1;

    for (linkIndex = 0; linkIndex < totalLinks; linkIndex++)
    {
        data32 = DISP_REG_RD32(LW_PDISP_SOR_HDCP22_CTRL(sorNum, linkIndex));
        data32 = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _CRYPT, _DISABLE, data32);
        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_CTRL(sorNum, linkIndex), data32);

        if (bCheckCryptStatus)
        {
            data32 = DISP_REG_RD32(LW_PDISP_SOR_HDCP22_STATUS(sorNum, linkIndex));
            while(!FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS,
                                _INACTIVE, data32))
            {
                data32 = DISP_REG_RD32(LW_PDISP_SOR_HDCP22_STATUS(sorNum, linkIndex));
                HDCP22WIRED_SPIN_WAIT_US(100);
                tickCount ++;
                if (tickCount > 5000)
                {
                    // Timed out waiting for HW to disable encryption.
                    status = FLCN_ERR_TIMEOUT;
                    return status;
                }
            }
        }
    }
    // TODO: Error Handling need to be done
label_return:
    return status;
}

/*!
 * @brief Checks whether a SOR is powered down.
 *
 * @param[in]   sorNum          SOR number
 * @param[in]   dfpSublinkMask  Sublink mask of display
 * @param[out]  pBSorPowerDown  Pointer to fill if SOR is powered down
 * @returns    FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsSorPoweredDown_v02_05
(
    LwU8   sorNum,
    LwU8   dfpSublinkMask,
    LwBool *pBSorPowerDown
)
{
    LwU32   data32;
    LwU32   mask = 0;

    data32 = DISP_REG_RD32(LW_PDISP_SOR_LANE_SEQ_CTL(sorNum));

    switch (dfpSublinkMask)
    {
        case SUBLINK_MASK_LINK_A:
        {
            mask = (DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE0_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE1_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE2_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE4_STATE, _POWERDOWN));
            break;
        }
        case SUBLINK_MASK_LINK_B:
        {
            mask = (DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE5_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE6_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE7_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE9_STATE, _POWERDOWN));
            break;
        }
        case SUBLINK_MASK_LINK_AB:
        {
            mask = (DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE0_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE1_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE2_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE4_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE5_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE6_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE7_STATE, _POWERDOWN) |
                    DRF_DEF(_PDISP, _SOR_LANE_SEQ_CTL, _LANE9_STATE, _POWERDOWN));
            break;
        }
        default:
        {
            // Ideally shouldn't enter here.
            *pBSorPowerDown = LW_FALSE;
        }
    }

    if ((data32 & mask) == mask)
    {
        *pBSorPowerDown = LW_TRUE;
    }
    else
    {
        *pBSorPowerDown = LW_FALSE;
    }

    // preVolta CSB access always succeed.
    return FLCN_OK;
}

/*!
 *  @brief Init LW_PDISP_SOR_HDCP22_CTRL reg setting.
 */
FLCN_STATUS
hdcp22wiredInitSorHdcp22CtrlReg_v02_05(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       sorIndex;
    LwU32       linkIndex;
    LwU8        maxNoOfSors = 0;
    LwU8        maxNoOfHeads = 0;

    CHECK_STATUS(hdcp22wiredGetMaxNoOfSorsHeads_HAL(&Hdcp22wired, &maxNoOfSors, &maxNoOfHeads));
    //
    // Set that ECF register can be written by protected SW (DPU).
    // Default is unlocked and set to lock after DPU loaded.
    //
    for (sorIndex = 0; sorIndex < maxNoOfSors; sorIndex ++ )
    {
        for (linkIndex = 0; linkIndex < 2; linkIndex ++)
        {
            // preVolta CSB access always succeed.
            _hdcp22SetDpLockEcf_v02_05(sorIndex, linkIndex, LW_TRUE);
        }
    }

label_return:
    return status;
}

#ifdef HDCP22_SUPPORT_MST
/*!
 * @brief Writes the 2 32 bits ecf value to HW. The ecf value contains a mask of
 * which timeslots require encryption.
 *
 * @param[in] sorIndex      sor index number
 * @param[in] pEcfTimeslot  Mask array of timeslots requiring encryption
 */
FLCN_STATUS
hdcp22wiredWriteDpEcf_v02_05
(
    LwU8    sorIndex,
    LwU32   *pEcfTimeslot
)
{
    LwU32   data32;
    LwU32   timeslots;

    // Unlock ecf protection.
    _hdcp22SetDpLockEcf_v02_05(sorIndex, 0, LW_FALSE);
    _hdcp22SetDpLockEcf_v02_05(sorIndex, 1, LW_FALSE);

    // Update the _ECF0 register with the corresponding ECF bits
    data32 = DISP_REG_RD32(LW_PDISP_SOR_DP_ECF0(sorIndex));
    timeslots = pEcfTimeslot[0];
    data32 = FLD_SET_DRF_NUM(_PDISP_SOR, _DP_ECF0, _VALUE, timeslots, data32);
    DISP_REG_WR32(LW_PDISP_SOR_DP_ECF0(sorIndex), data32);

    // Update the _ECF1 register with the corresponding ECF bits
    data32 = DISP_REG_RD32(LW_PDISP_SOR_DP_ECF1(sorIndex));
    timeslots = pEcfTimeslot[1];
    data32 = FLD_SET_DRF_NUM(_PDISP_SOR, _DP_ECF1, _VALUE, timeslots, data32);
    DISP_REG_WR32(LW_PDISP_SOR_DP_ECF1(sorIndex), data32);

    // Lock ecf protection again.
    _hdcp22SetDpLockEcf_v02_05(sorIndex, 0, LW_TRUE);
    _hdcp22SetDpLockEcf_v02_05(sorIndex, 1, LW_TRUE);

    // preVolta CSB access always succeed.
    return FLCN_OK;
}
#endif

/*!
 * @brief Read audit timer LW_PDISP_AUDIT value for timer cross check.
 *
 * @param[in] pLwrrentTimeUs    pointer to current time in usec.
 *
 * @return FLCN_OK if supported.
 */
FLCN_STATUS
hdcp22wiredReadAuditTimer_v02_05
(
    LwU32* pLwrrentTimeUs
)
{
#ifdef LW_PDISP_AUDIT
    *pLwrrentTimeUs = DISP_REG_RD32(LW_PDISP_AUDIT);

    return FLCN_OK;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif
}

/*!
 * @brief Check if encryption disabled due to SOR lane count is set to 0 in DP mode.
 * @param[in] sorIndex                  sor index number
 * @param[out] pbDisabledWithLaneCount0   Pointer to check result.
 *
 * @return FLCN_OK if supported.
 */
FLCN_STATUS
hdcp22wiredIsAutoDisabledWithLaneCount0_v02_05
(
    LwU8    sorNumber,
    LwBool *pbDisabledWithLaneCount0
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        maxNoOfSors = 0;

    CHECK_STATUS(hdcp22wiredGetMaxNoOfSorsHeads_HAL(&Hdcp22wired, &maxNoOfSors, NULL));

    if ((NULL == pbDisabledWithLaneCount0) || (sorNumber >= maxNoOfSors))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // preVolta CSB access always succeed.
    *pbDisabledWithLaneCount0 = FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS,
                                            _AUTODIS_STATE, _DISABLE_LC_0,
                                            DISP_REG_RD32(LW_PDISP_SOR_HDCP22_STATUS(sorNumber, 0)));

label_return:
    return status;
}

/*!
 * @brief       This function gets a random number
 * @param[out]  pDest     Address in which random number will be written
 * @param[in]   size      size of random number in dwords
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetRandNumber_v02_05
(
    LwU32  *pDest,
    LwS32   size
)
{
    LwU32 cnt;
    LwU32 val;
    LwU8  sorNum = 0;

    for (cnt = 0; cnt < size; cnt+=2)
    {
        LwU32 msb = 0;
        LwU32 lsb = 0;
        LwU32 data = 0;
        LwU16 offset = 0;
        LwU8  ranCount = 0;

        data = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _RUN, _NO, data);
        data = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _DONE, _NO, data);

        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum),data);

        data = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _RUN, _YES, data);
        DISP_REG_WR32(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum), data);

        while (LW_TRUE)
        {
            if (FLD_TEST_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _DONE, _YES,
                DISP_REG_RD32(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum))))
            {
                break;
            }
            if (ranCount++ > HDCP22_RND_NUM_WAIT_CNT)
            {
                break;
            }
        }

        msb = DISP_REG_RD32(LW_PDISP_SOR_HDCP22_RNDGEN_NUM_MSB(sorNum));
        lsb = DISP_REG_RD32(LW_PDISP_SOR_HDCP22_RNDGEN_NUM_LSB(sorNum));

        offset = cnt;

        *((LwU32 *) (pDest + offset)) = msb;
        *((LwU32 *) (pDest + offset + 1)) = lsb;
    }

    val = 0;
    val = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _RUN, _NO, val);
    val = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _DONE, _NO, val);

    DISP_REG_WR32(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum), val);

    // preVolta CSB access always succeed.
    return FLCN_OK;
}
#endif

/*!
 *  @brief This function does aux transaction(read/writes) on given offset and port.
 *  @param[in]     port         physical DP aux port number.
 *  @param[in]     dataOffset   offset to  aux read/write data.
 *  @param[in]     dataSize     size in bytes to aux read/write data.
 *  @param[in/out] pData        pointer to read/write data buffer.
 *  @param[in]     bRead        bRead true holds aux read transaction,
 *                              bRead false holds aux write transaction.
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredAuxPerformTransaction_v02_05
(
     LwU8       port,
     LwU32      dataOffset,
     LwU32      dataSize,
     LwU8      *pData,
     LwBool     bRead
)
{
    FLCN_STATUS status;
    DPAUX_CMD auxTransactionCmd;

    auxTransactionCmd.addr  = dataOffset;
    auxTransactionCmd.size  = dataSize;
    auxTransactionCmd.pData = pData;
    auxTransactionCmd.cmd   = (bRead) ? HDCP22_DP_AUXCTL_CMD_AUXRD : HDCP22_DP_AUXCTL_CMD_AUXWR;
    auxTransactionCmd.port  = port;

    status = dpauxAccess(&auxTransactionCmd, bRead);
    if ((status != FLCN_OK) && (status != FLCN_ERR_HPD_UNPLUG))
    {
        status = FLCN_ERR_AUX_ERROR;
    }

    return status;
}

/*!
 *  @brief This function does i2c transaction(read/writes) on given offset and port.
 *  @param[in]     port         Physical I2C port number.
 *  @param[in]     dataOffset   offset to  i2c read/write data.
 *  @param[in]     dataSize     size in bytes to i2c read/write data.
 *  @param[in/out] pData        pointer to read/write data buffer.
 *  @param[in]     bRead        bRead true holds i2c read transaction,
 *                              bRead false holds i2c write transaction.
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredI2cPerformTransaction_v02_05
(
     LwU8       port,
     LwU32      dataOffset,
     LwU32      dataSize,
     LwU8      *pData,
     LwBool     bRead
)
{
    I2C_TRANSACTION i2cTransaction;

    i2cTransaction.i2cAddress    = LW_HDMI_VIDEO_LINK0_ADDRESS;
    i2cTransaction.messageLength = dataSize;
    i2cTransaction.pMessage      = pData;
    i2cTransaction.index[0]      = dataOffset;
    i2cTransaction.bRead         = bRead;

    i2cTransaction.flags         = DRF_DEF(_I2C, _FLAGS, _START, _SEND)         |
                                   DRF_DEF(_I2C, _FLAGS, _ADDRESS_MODE, _7BIT)  |
                                   DRF_DEF(_I2C, _FLAGS, _INDEX_LENGTH, _ONE)   |
                                   DRF_DEF(_I2C, _FLAGS, _SPEED_MODE, _100KHZ)  |
                                   DRF_NUM(_I2C, _FLAGS, _PORT, (LwU16)port);

    if (bRead)
    {
        i2cTransaction.flags    |= DRF_DEF(_I2C, _FLAGS, _RESTART, _SEND);
    }

    return i2cPerformTransaction(&i2cTransaction);
}

/*!
 * @brief       This function gets maximum number of sor.
 * @param[out]  pMaxNoOfSors  Pointer to get maximum no of sor
 * @param[out]  pMaxNoOfHeads Pointer to get maximum no of heads
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetMaxNoOfSorsHeads_v02_05
(
    LwU8   *pMaxNoOfSors,
    LwU8   *pMaxNoOfHeads
)
{
    if (pMaxNoOfSors)
    {
        *pMaxNoOfSors = LW_PDISP_MAX_SOR;
    }

    if (pMaxNoOfHeads)
    {
        *pMaxNoOfHeads = LW_PDISP_MAX_HEAD;
    }

    return FLCN_OK;
}

/*!
 * @brief       Write confidential secrets
 * @param[in]   pSession    Pointer to HDCP22 Session.
 * @param[in]   pSecret     Pointer to secret array.
 * @param[in]   bIsReset    flag to tell if reset without check integrity.
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredWriteToSelwreMemory_v02_05
(
    HDCP22_SECRET   hdcp22Secret,
    LwU8           *pSecret,
    LwBool          bIsReset
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       size = 0;
    LwBool      bIsConfidentialData = LW_FALSE; // TRUE is confidential data otherwise data need integrity checked.
    LwU8       *pDest = NULL;

    switch (hdcp22Secret)
    {
    case HDCP22_SECRET_DKEY:
        {
            bIsConfidentialData = LW_TRUE;
            pDest = (LwU8 *)hdcp22SelwreMemory.secAssetsConfidential.encDkey;
            size = HDCP22_SIZE_DKEY;
        }
        break;

    case HDCP22_SECRET_KD:
        {
            bIsConfidentialData = LW_TRUE;
            pDest = (LwU8 *)hdcp22SelwreMemory.secAssetsConfidential.encKd;
            size = HDCP22_SIZE_KD;
        }
        break;

    // Below fields are integrity check required.
    case HDCP22_SECRET_SOR_NUM:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.sorNum;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.sorNum);
        }
        break;

    case HDCP22_SECRET_LINK_IDX:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.linkIndex;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.linkIndex);
        }
        break;

    case HDCP22_SECRET_IS_MST:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.bIsMst;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.bIsMst);
        }
        break;

    case HDCP22_SECRET_IS_REPEATER:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.bIsRepeater;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.bIsRepeater);
        }
        break;

    case HDCP22_SECRET_NUM_STREAMS:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.numStreams;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.numStreams);
        }
        break;

    case HDCP22_SECRET_STREAM_ID_TYPE:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)hdcp22SelwreMemory.secAssetsIntegrity.streamIdType;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.streamIdType);
        }
        break;

    case HDCP22_SECRET_DP_TYPE_MASK:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)hdcp22SelwreMemory.secAssetsIntegrity.dpTypeMask;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.dpTypeMask);
        }
        break;

    case HDCP22_SECRET_RX_ID:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)hdcp22SelwreMemory.secAssetsIntegrity.rxId;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.rxId);
        }
        break;

    case HDCP22_SECRET_RTX:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.Rtx;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.Rtx);
        }
        break;

    case HDCP22_SECRET_RN:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.Rn;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.Rn);
        }
        break;

#ifndef HDCP22_KMEM_ENABLED
    case HDCP22_SECRET_RSESSION:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt);
        }
        break;
#endif

    case HDCP22_SECRET_MAX_NO_SORS:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfSors;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfSors);
        }
        break;

    case HDCP22_SECRET_MAX_NO_HEADS:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfHeads;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfHeads);
        }
        break;

    case HDCP22_SECRET_PREV_STATE:
        {
            bIsConfidentialData = LW_FALSE;
            pDest = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.previousState;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.previousState);
        }
        break;

    default:
        status = FLCN_ERR_NOT_SUPPORTED;
        goto label_return;
    }

    if (bIsReset)
    {
        // To reset case, no matter confidential, integrity secrets can be reset.
        hdcpmemset(pDest, 0, size);

        // TODO: reset hash to integrity secret reset write.
    }
    else
    {
        if (bIsConfidentialData)
        {
            // Confidential secret must be LwU32 aligned.
            if (((LwUPtr)pDest % sizeof(LwU32)) || ((LwUPtr)pSecret % sizeof(LwU32)))
            {
                status = FLCN_ERR_ILLEGAL_OPERATION;
                goto label_return;
            }

#ifndef HDCP22_KMEM_ENABLED
            CHECK_STATUS(hdcp22wiredWriteConfidentialSecretXor((LwU32 *)pDest,
                                                               (LwU32 *)pSecret,
                                                               hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
                                                               size));
#endif
        }
        else
        {
            // TODO: check hash if not reset case.
            hdcpmemcpy(pDest, pSecret, size);
            // TODO: Callwlate Hash and update new hash value
        }
    }

label_return:
    return status;
}

/*!
 * @brief       Read confidential secrets
 * @param[in]   pSession    Pointer to HDCP22 Session.
 * @param[in]   pSecret     Pointer to secret array.
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredReadFromSelwreMemory_v02_05
(
    HDCP22_SECRET   hdcp22Secret,
    LwU8           *pSecret
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       size = 0;
    LwBool      bIsConfidentialData = LW_FALSE; // TRUE is confidential data otherwise data need integrity checked.
    LwU8       *pSrc = NULL;
    LwU32       decData[sizeof(SECRET_ASSETS_CONFIDENTIAL)/sizeof(LwU32)];

    hdcpmemset(decData, 0, sizeof(decData));

    switch (hdcp22Secret)
    {
    case HDCP22_SECRET_DKEY:
        {
            bIsConfidentialData = LW_TRUE;
            pSrc = (LwU8 *)decData;
            size = HDCP22_SIZE_DKEY;
#ifndef HDCP22_KMEM_ENABLED
            CHECK_STATUS(hdcp22wiredReadConfidentialSecretXor(hdcp22SelwreMemory.secAssetsConfidential.encDkey,
                                                              decData,
                                                              hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
                                                              size));
#endif
        }
        break;

    case HDCP22_SECRET_KD:
        {
            bIsConfidentialData = LW_TRUE;
            pSrc = (LwU8 *)decData;
            size = HDCP22_SIZE_KD;
#ifndef HDCP22_KMEM_ENABLED
            CHECK_STATUS(hdcp22wiredReadConfidentialSecretXor(hdcp22SelwreMemory.secAssetsConfidential.encKd,
                                                              decData,
                                                              hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
                                                              HDCP22_SIZE_KD));
#endif
        }
        break;

    // Below fields are integrity check required.
    case HDCP22_SECRET_SOR_NUM:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.sorNum;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.sorNum);
        }
        break;

    case HDCP22_SECRET_LINK_IDX:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.linkIndex;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.linkIndex);
        }
        break;

    case HDCP22_SECRET_IS_MST:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.bIsMst;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.bIsMst);
        }
        break;

    case HDCP22_SECRET_IS_REPEATER:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.bIsRepeater;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.bIsRepeater);
        }
        break;

    case HDCP22_SECRET_NUM_STREAMS:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.numStreams;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.numStreams);
        }
        break;

    case HDCP22_SECRET_STREAM_ID_TYPE:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.streamIdType;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.streamIdType);
        }
        break;

    case HDCP22_SECRET_DP_TYPE_MASK:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.dpTypeMask;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.dpTypeMask);
        }
        break;

    case HDCP22_SECRET_RX_ID:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)hdcp22SelwreMemory.secAssetsIntegrity.rxId;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.rxId);
        }
        break;

    case HDCP22_SECRET_RTX:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.Rtx;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.Rtx);
        }
        break;

    case HDCP22_SECRET_RN:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.Rn;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.Rn);
        }
        break;

#ifndef HDCP22_KMEM_ENABLED
    case HDCP22_SECRET_RSESSION:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt);
        }
        break;
#endif

    case HDCP22_SECRET_MAX_NO_SORS:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfSors;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfSors);
        }
        break;

    case HDCP22_SECRET_MAX_NO_HEADS:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfHeads;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.maxNoOfHeads);
        }
        break;

    case HDCP22_SECRET_PREV_STATE:
        {
            bIsConfidentialData = LW_FALSE;
            pSrc = (LwU8 *)&hdcp22SelwreMemory.secAssetsIntegrity.previousState;
            size = sizeof(hdcp22SelwreMemory.secAssetsIntegrity.previousState);
        }
        break;

    default:
        status = FLCN_ERR_NOT_SUPPORTED;
        goto label_return;

    }

    if (!bIsConfidentialData)
    {
        // TODO: Add simple integrity check ( xor? )
    }

    hdcpmemcpy(pSecret, pSrc, size);

label_return:
    return status;
}

/*!
 * @brief       Save active session secrets per SOR.
 * @param[in]   sorNum      SOR number
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredSaveSessionSecrets_v02_05
(
    LwU8 sorNum
)
{
#ifndef HDCP22_KMEM_ENABLED
    hdcpmemcpy(hdcp22SelwreMemory.secAssetsActiveSession[sorNum].rnSesCrypt,
               hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
               HDCP22_SIZE_SESSION_CRYPT);
#endif

    hdcpmemcpy(hdcp22SelwreMemory.secAssetsActiveSession[sorNum].encKd,
               hdcp22SelwreMemory.secAssetsConfidential.encKd,
               HDCP22_SIZE_KD);

    return FLCN_OK;
}

/*!
 * @brief       Retrieve active session secrets per SOR.
 * @param[in]   sorNum                  SOR number
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredRetrieveSessionSecrets_v02_05
(
    LwU8                            sorNum
)
{
#ifndef HDCP22_KMEM_ENABLED
    hdcpmemcpy(hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
               hdcp22SelwreMemory.secAssetsActiveSession[sorNum].rnSesCrypt,
               HDCP22_SIZE_SESSION_CRYPT);
#endif

    hdcpmemcpy(hdcp22SelwreMemory.secAssetsConfidential.encKd,
               hdcp22SelwreMemory.secAssetsActiveSession[sorNum].encKd,
               HDCP22_SIZE_KD);

    return FLCN_OK;
}
