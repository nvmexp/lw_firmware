/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_hdcp22wired0300.c
 * @brief  Hdcp22 wired 03.00 Hal Functions
 *
 * HDCP2.2 Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes --------------------------------- */
#include "dev_disp.h"
#include "dev_disp_seb.h"
#include "osptimer.h"

/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_timer.h"
#include "seapi.h"
#include "setypes.h"
#include "scp_common.h"
#include "secbus_hs.h"
#if defined(HDCP22_USE_SCP_ENCRYPT_SECRET)
#include "hdcp22wired_selwreaction.h"
#include "scp_internals.h"
#include "scp_crypt.h"
#include "scp_secret_usage.h"
#include "mem_hs.h"
#if defined(HS_OVERLAYS_ENABLED)
#include "lwosselwreovly.h"
#endif
#endif

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
extern SE_STATUS seSelwreBusWriteRegisterErrChk(LwU32, LwU32);
extern SE_STATUS seSelwreBusReadRegisterErrChk(LwU32, LwU32*);
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Defines ----------------------------------------- */
#define SUBLINK_MASK_LINK_A         0x1
#define SUBLINK_MASK_LINK_B         0x2
#define SUBLINK_MASK_LINK_AB        0x3

#define SECRET_SCP_CRYPT_SALT_OFFSET    (0)
#define SECRET_SCP_CRYPT_SALT_SIZE      (SCP_BUF_ALIGNMENT)
#define SECRET_SCP_CRYPT_IV_OFFSET      (SECRET_SCP_CRYPT_SALT_OFFSET + SECRET_SCP_CRYPT_SALT_SIZE)
#define SECRET_SCP_CRYPT_IV_SIZE        (SCP_BUF_ALIGNMENT)
#define SECRET_SCP_CRYPT_INPUT_OFFSET   (SECRET_SCP_CRYPT_IV_OFFSET + SECRET_SCP_CRYPT_IV_SIZE)
#define SECRET_SCP_CRYPT_OUTPUT_OFFSET  (SECRET_SCP_CRYPT_INPUT_OFFSET + MAX_SECRET_SCP_CRYPT_SIZE)
#define SECRET_SCP_CRYPT_BUFF_SIZE      (SECRET_SCP_CRYPT_SALT_SIZE + SECRET_SCP_CRYPT_IV_SIZE + MAX_SECRET_SCP_CRYPT_SIZE + MAX_SECRET_SCP_CRYPT_SIZE)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Prototypes  ------------------------------------- */
static FLCN_STATUS _hdcp22wiredWriteMstKs_v03_00(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 *pKs)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "_hdcp22wiredWriteMstKs_v03_00");
static FLCN_STATUS _hdcp22WriteMstRiv_v03_00(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 bytesToWrite0, LwU32 bytesToWrite1)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "_hdcp22WriteMstRiv_v03_00");
static FLCN_STATUS _hdcp22wiredWriteMstStreamType_v03_00(LwU8 sorNum, LwU8 linkIndex, HDCP22_STREAM streamIdType[HDCP22_NUM_STREAMS_MAX], LwU32 dpTypeMask[HDCP22_NUM_DP_TYPE_MASK])
    GCC_ATTRIB_SECTION("imem_hdcp22ControlEncryption", "_hdcp22wiredWriteMstStreamType_v03_00");
static FLCN_STATUS _hdcp22wiredHwDrmType1LockedWarWaitVblankEdge(LwU8 sorNum, LwBool bApplyHwDrmType1LockedWar)
    GCC_ATTRIB_SECTION("imem_hdcp22ControlEncryption", "_hdcp22wiredHwDrmType1LockedWarWaitVblankEdge");
#ifdef HDCP22_SUPPORT_MST
static FLCN_STATUS _hdcp22SetDpLockEcfHs_v03_00(LwU32 sorIndex, LwU32 linkIndex, LwBool lock)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22SetDpLockEcfHs_v03_00");
#endif

FLCN_STATUS hdcp22wiredGetRandNumber_v03_00(LwU32 *pDest, LwS32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredGetRandNumber_v03_00");
FLCN_STATUS hdcp22wiredSwRsaModularExpHs_v03_00(const LwU32 keySize, const LwU8 *pModulus, const LwU32 *pExponent, LwU32 *pBase, LwU32 *pOutput)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredSwRsaModularExpHs_v03_00");
FLCN_STATUS hdcp22wiredConfidentialSecretDoCrypt_v03_00(LwU32 *pRn, LwU32 *pSecret, LwU32 *pDest, LwU32 size, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredConfidentialSecretDoCrypt_v03_00");
FLCN_STATUS hdcp22wiredWriteToSelwreMemory_v03_00(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret, LwBool bIsReset)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredWriteToSelwreMemory_v03_00");
FLCN_STATUS hdcp22wiredReadFromSelwreMemory_v03_00(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredReadFromSelwreMemory_v03_00");
FLCN_STATUS hdcp22wiredSaveSessionSecrets_v03_00(LwU8 sorNum)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredSaveSessionSecrets_v03_00");
FLCN_STATUS hdcp22wiredRetrieveSessionSecrets_v03_00(LwU8 sorNum)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredRetrieveSessionSecrets_v03_00");
#if defined(UPROC_RISCV)
extern FLCN_STATUS hdcp22wiredInitSorHdcp22CtrlReg_v02_05(void);
extern FLCN_STATUS hdcp22wiredGetRandNumber_v02_05(LwU32 *pDest, LwS32 size);
extern FLCN_STATUS hdcp22wiredWriteToSelwreMemory_v02_05(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret, LwBool bIsReset);
extern FLCN_STATUS hdcp22wiredReadFromSelwreMemory_v02_05(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret);
extern FLCN_STATUS hdcp22wiredSaveSessionSecrets_v02_05(LwU8 sorNum);
extern FLCN_STATUS hdcp22wiredRetrieveSessionSecrets_v02_05(LwU8 sorNum);
#else // UPROC_RISCV
    #ifdef HDCP22_SUPPORT_MST
static FLCN_STATUS _hdcp22SetDpLockEcf_v03_00(LwU32 sorIndex, LwU32 linkIndex, LwBool lock)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SetDpLockEcf_v03_00");
    #endif // HDCP22_SUPPORT_MST
    #ifndef HDCP22_KMEM_ENABLED
static FLCN_STATUS _hdcp22wiredReadConfidentialSecret_v03_00(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "_hdcp22wiredReadConfidentialSecret_v03_00");
static FLCN_STATUS _hdcp22wiredWriteConfidentialSecret_v03_00(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "_hdcp22wiredWriteConfidentialSecret_v03_00");
    #endif // !HDCP22_KMEM_ENABLED
    #if !defined(HDCP22_USE_SCP_ENCRYPT_SECRET)
extern FLCN_STATUS hdcp22wiredReadConfidentialSecretXor(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size);
extern FLCN_STATUS hdcp22wiredWriteConfidentialSecretXor(LwU32 *pData, LwU32 *pSecret, LwU32 *pRn, LwU32 size);
    #endif // HDCP22_USE_SCP_ENCRYPT_SECRET
#endif // UPROC_RISCV

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief      This function writes Ks value for MST case
 *
 * @param[in]  sorNum       SOR number index
 * @param[in]  linkIndex    link index number
 * @param[in]  bIsMst       flag to tell if DP MST
 * @param[in]  pKs          Pointer to Ks values
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredWriteMstKs_v03_00
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32   *pKs
)
{
    FLCN_STATUS status = FLCN_OK;

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
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_LSB1(sorNum, !linkIndex),
                        pKs[0]));
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_LSB2(sorNum, !linkIndex),
                        pKs[1]));
       CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_LSB3(sorNum, !linkIndex),
                        pKs[2]));
       CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_MSB(sorNum, !linkIndex),
                        pKs[3]));
    }

label_return:

#endif

    return status;
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
static FLCN_STATUS
_hdcp22WriteMstRiv_v03_00
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32   bytesToWrite0,
    LwU32   bytesToWrite1
)
{
    FLCN_STATUS status = FLCN_OK;

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
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_DATA_LSB(sorNum, !linkIndex),
                        bytesToWrite1));
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_DATA_MSB(sorNum, !linkIndex),
                        bytesToWrite0));
    }

label_return:
#endif

    return status;
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
_hdcp22wiredWriteMstStreamType_v03_00
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
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_DP_TYPE_LSB(sorNum),
                                                dpTypeMask[0]));
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_DP_TYPE_MSB(sorNum),
                                                dpTypeMask[1]));
    }
    else
    {
        // Return error for SST case.
        status = FLCN_ERR_NOT_SUPPORTED;
    }

label_return:
#else
    status = FLCN_ERR_NOT_SUPPORTED;
#endif

    return status;
}

/*!
 * @brief      This function is to wait vblank rising edge for HWDRM type1locked WAR.
 *
 * @param[in]  sorNum                     SOR number index
 * @param[in]  bApplyHwDrmType1LockedWar  flag to tell if applying HwDRM type1Locked WAR
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredHwDrmType1LockedWarWaitVblankEdge
(
    LwU8    sorNum,
    LwBool  bApplyHwDrmType1LockedWar
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 indx = 0;
    LwU32 data = 0;

    if (bApplyHwDrmType1LockedWar)
    {
        //
        // Want to write INIT on positive edge of vblank which must happen
        // before vsync.
        // LW_PDISP_RG_STATUS is read only register updated by HW, so we can
        // assume it can't be tampered.
        //
        CHECK_STATUS(DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_RG_STATUS(sorNum), &data));
        while (FLD_TEST_DRF(_PDISP, _RG_STATUS, _VBLNK, _ACTIVE, data))
        {
            HDCP22WIRED_SPIN_WAIT_US(DP_AUX_DELAY_US * 1000U);
            if (indx++ > HDCP22_HWDRM_WAR_MAX_POLL_ATTEMPTS)
            {
                status = FLCN_ERR_TIMEOUT;
                goto label_return;
            }
            CHECK_STATUS(DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_RG_STATUS(sorNum), &data));
        }

        indx = 0;
        // Now sure vblank is inactive then wait till active as positive edge.
        while (FLD_TEST_DRF(_PDISP, _RG_STATUS, _VBLNK, _INACTIVE, data))
        {
            HDCP22WIRED_SPIN_WAIT_US(DP_AUX_DELAY_US * 1000U);
            if (indx++ > HDCP22_HWDRM_WAR_MAX_POLL_ATTEMPTS)
            {
                status = FLCN_ERR_TIMEOUT;
                goto label_return;
            }
            CHECK_STATUS(DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_RG_STATUS(sorNum), &data));
        }

        //
        // Per HW suggest, as we are reading RG_STATUS from an earlier stage
        // hence there are a few pipeline stages before all pixels are flushed
        // out of the HDCP stage. Wait 1us before writing _INIT.
        //
        HDCP22WIRED_SPIN_WAIT_US(1000U);
    }

label_return:
    return status;
}

#ifdef HDCP22_SUPPORT_MST
/*!
 * @brief Set Sor HDCP2.2 Control register _LOCK_ECF setting HS version.
 *
 * @param[in] sorIndex    sor index number
 * @param[in] linkIndex   link index number
 * @param[in] lock        enable or disable ecf protection
 * @returns   FLCN_STATUS FLCN_OK on successfull exelwtion
 *                        Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22SetDpLockEcfHs_v03_00
(
    LwU32       sorIndex,
    LwU32       linkIndex,
    LwBool      lock
)
{
    LwU32 data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), &data32));

    //
    // Set ecf lock that ECF register can only be written by secure SW
    // (FLCN uCode) or not.
    //
    if (lock)
    {
        data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _LOCK_ECF, _LOCKED, data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _LOCK_ECF, _UNLOCKED, data32);
    }

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), data32));

label_return:
    return status;
}
#endif // HDCP22_SUPPORT_MST

#if !defined(UPROC_RISCV)

#ifdef HDCP22_SUPPORT_MST
/*!
 * @brief Set Sor HDCP2.2 Control register _LOCK_ECF setting.
 *
 * @param[in] sorIndex    sor index number
 * @param[in] linkIndex   link index number
 * @param[in] lock        enable or disable ecf protection
 * @returns   FLCN_STATUS FLCN_OK on successfull exelwtion
 *                        Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22SetDpLockEcf_v03_00
(
    LwU32       sorIndex,
    LwU32       linkIndex,
    LwBool      lock
)
{
    LwU32 data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), &data32));

    //
    // Set ecf lock that ECF register can only be written by secure SW
    // (FLCN uCode) or not.
    //
    if (lock)
    {
        data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _LOCK_ECF, _LOCKED, data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _LOCK_ECF, _UNLOCKED, data32);
    }

    CHECK_STATUS(hdcp22wiredWriteRegister(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), data32));

label_return:
    return status;
}
#endif

#ifndef HDCP22_KMEM_ENABLED
/*!
 * @brief       Read confidential secrets
 * @param[in]   pData           Pointer to encrypted secret.
 * @param[out]  pSecret         Pointer to read secret data.
 * @param[in]   pRn             Pointer to random number for encryption.
 * @param[in]   size            Size to encrypt or decrypt
 *
 * @returns     FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredReadConfidentialSecret_v03_00
(
    LwU32  *pData,
    LwU32  *pSecret,
    LwU32  *pRn,
    LwU32   size
)
{
    FLCN_STATUS status = FLCN_OK;

#if defined(HDCP22_USE_SCP_ENCRYPT_SECRET)
    status = hdcp22wiredConfidentialSecretDoCrypt_v03_00(pRn, pData, pSecret,
                                                         size, LW_FALSE);
#else
    status = hdcp22wiredReadConfidentialSecretXor(pSecret, pData, pRn, size);
#endif

    return status;
}

/*!
 * @brief       Write confidential secrets
 * @param[out]  pData           Pointer to write secret destination.
 * @param[in]   pSecret         Pointer to confidential secret data.
 * @param[in]   pRn             Pointer to random number for encryption.
 * @param[in]   size            Size to encrypt or decrypt
 *
 * @returns     FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredWriteConfidentialSecret_v03_00
(
    LwU32  *pData,
    LwU32  *pSecret,
    LwU32  *pRn,
    LwU32   size
)
{
    FLCN_STATUS status = FLCN_OK;

#if defined(HDCP22_USE_SCP_ENCRYPT_SECRET)
    status = hdcp22wiredConfidentialSecretDoCrypt_v03_00(pRn, pSecret, pData,
                                                         size, LW_TRUE);
#else
    status = hdcp22wiredWriteConfidentialSecretXor(pData, pSecret, pRn, size);
#endif

    return status;
}
#endif // !HDCP22_KMEM_ENABLED
#endif // !UPROC_RISCV

/* ------------------------- Public Functions ------------------------------- */
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
hdcp22wiredWriteKs_v03_00
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pKs
)
{
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                    LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_LSB1(sorNum, linkIndex),
                    pKs[0]));
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                    LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_LSB2(sorNum, linkIndex),
                    pKs[1]));
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                    LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_LSB3(sorNum, linkIndex),
                    pKs[2]));
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                    LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_KEY_MSB(sorNum, linkIndex),
                    pKs[3]));

    status = _hdcp22wiredWriteMstKs_v03_00(sorNum, linkIndex, bIsMst, pKs);

label_return:
    return status;
}

/*!
 * @brief      This function writes Riv value
 *
 * @param[in]  sorNum       SOR number index
 * @param[in]  linkIndex    link index number
 * @param[in]  bIsMst       flag to tell if DP MST
 * @param[in]  pRiv         Pointer to Riv
 *
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredWriteRiv_v03_00
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pRiv
)
{
    FLCN_STATUS status = FLCN_OK;

    LwU32 bytesToWrite0 = 0;
    LwU32 bytesToWrite1 = 0;

    swapEndiannessHs(&bytesToWrite1, &pRiv[1], sizeof(LwU32));
    swapEndiannessHs(&bytesToWrite0, &pRiv[0], sizeof(LwU32));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                    LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_DATA_LSB(sorNum, linkIndex),
                    bytesToWrite1));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
                        LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_AES_CTR_DATA_MSB(sorNum, linkIndex),
                        bytesToWrite0));

    status = _hdcp22WriteMstRiv_v03_00(sorNum, linkIndex, bIsMst,
                                       bytesToWrite0, bytesToWrite1);

label_return:
    return status;
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
 */
FLCN_STATUS
hdcp22wiredWriteStreamType_v03_00
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
        status = _hdcp22wiredWriteMstStreamType_v03_00(sorNum, linkIndex, streamIdType, dpTypeMask);
        if (status == FLCN_ERR_NOT_SUPPORTED)
        {
            // SST type1Locked case is already processed at StartSession.
            status = FLCN_OK;
            CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_SST_DP_TYPE(sorNum, linkIndex),
                                                    streamIdType[0].streamType));
        }
    }
    else
    {
        // HDMI type1Locked case is already processed at StartSession.
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_HDMI_TYPE(sorNum),
                                                streamIdType[0].streamType));
    }

label_return:
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
hdcp22wiredEncryptionActive_v03_00
(
    LwU8 sorNumber,
    LwBool *pBEncActive
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32;

    // Check Encryption status on both the links and Report encryption enabled
    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNumber, 0), &data32));
    *pBEncActive = FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, data32);

    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNumber, 1), &data32));
    *pBEncActive = (*pBEncActive) | FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, data32);

label_return:
    return status;
}

/*!
 * @brief Checks if the link encryption is active HS version
 * @param[in]  sorNumber    sor Number
 * @param[out] pBEncActive  Pointer to fill if encryption is active
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.

 */
FLCN_STATUS
hdcp22wiredEncryptionActiveHs_v03_00
(
    LwU8 sorNumber,
    LwBool *pBEncActive
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32;

    // Check Encryption status on both the links and Report encryption enabled
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNumber, 0), &data32));
    *pBEncActive = FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, data32);

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNumber, 1), &data32));
    *pBEncActive = (*pBEncActive) | FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, data32);

label_return:
    return status;
}

/*!
 * @brief      Checks whether given link is 8 Lane DP
 * @param[in]  pSession pointer to HDCP22 Session
 * @param[out] pBSor8LaneDp Pointer to fill if sor drives 8 lane DP.
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsSorDrivesEightLaneDp_v03_00
(
    LwU8 sorNumber,
    LwBool *pBSor8LaneDp
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_DP_LINKCTL0(sorNumber), &data32));

    *pBSor8LaneDp = FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_LINKCTL0,
        _LANECOUNT, _EIGHT, data32);

label_return:
    return status;
}

/*!
 * @brief      Checks whether given link is 8 Lane DP
 * @param[in]  pSession     Pointer to HDCP22 Session
 * @param[out] pBSor8LaneDp Pointer to fill if sor drives 8 lane DP.
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsSorDrivesEightLaneDpHs_v03_00
(
    LwU8    sorNumber,
    LwBool *pBSor8LaneDp
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_LINKCTL0(sorNumber), &data32));

    *pBSor8LaneDp = FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_DP_LINKCTL0,
        _LANECOUNT, _EIGHT, data32);

label_return:
    return status;
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
hdcp22wiredHandleEnableEnc_v03_00
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsRepeater,
    LwBool  bApplyHwDrmType1LockedWar
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       hdcpCtrl = 0;
    LwU32       ctrlReg = LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorNum, linkIndex);
    LwU32       statusReg = LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNum, linkIndex);
    LwU32       cryptStatus = 0;
    LwU32       tickCount = 0;

    CHECK_STATUS(_hdcp22wiredHwDrmType1LockedWarWaitVblankEdge(sorNum, bApplyHwDrmType1LockedWar));

    // Trigger HDCP22 CTRL INIT
    CHECK_STATUS(hdcp22wiredReadRegisterHs(ctrlReg, &hdcpCtrl));
    hdcpCtrl = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _INIT, _TRIGGER, hdcpCtrl);

    if (bIsRepeater)
    {
        hdcpCtrl = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _REPEATER, _YES, hdcpCtrl);
    }
    else
    {
        hdcpCtrl = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _REPEATER, _NO, hdcpCtrl);
    }

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(ctrlReg, hdcpCtrl));

    // Poll LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL_INIT_DONE
    do
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(ctrlReg, &hdcpCtrl));
    } while (!FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _INIT, _DONE, hdcpCtrl));

    if (!bApplyHwDrmType1LockedWar)
    {
        // Write LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL_ENABLE_YES
        hdcpCtrl = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _CRYPT, _ENABLE, hdcpCtrl);
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(ctrlReg, hdcpCtrl));
    }

    // Poll LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL_CRYPT_STS = _ACTIVE
    CHECK_STATUS(hdcp22wiredReadRegisterHs(statusReg, &cryptStatus));
    while (!FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, cryptStatus))
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(statusReg, &cryptStatus));
        HDCP22WIRED_SPIN_WAIT_US(100000U);
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

label_return:
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
hdcp22wiredHandleDisableEnc_v03_00
(
    LwU8    maxNoOfSors,
    LwU8    maxNoOfHeads,
    LwU8    sorNum,
    LwBool  bCheckCryptStatus
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32 = 0;
    LwU8        totalLinks = 0;
    LwU8        linkIndex  = 0;
    LwU16       tickCount  = 0;
    LwBool      pBSor8LaneDp;

    // Check whether we also need to disable it for secondary link.
    CHECK_STATUS(hdcp22wiredIsSorDrivesEightLaneDpHs_HAL(&Hdcp22wired, sorNum, &pBSor8LaneDp));
    totalLinks = pBSor8LaneDp ? 2 : 1;

    for (linkIndex = 0; linkIndex < totalLinks; linkIndex++)
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorNum, linkIndex), &data32));
        data32 = FLD_SET_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_CTRL, _CRYPT, _DISABLE, data32);
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL(sorNum, linkIndex), data32));

        if (bCheckCryptStatus)
        {
            CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNum, linkIndex), &data32));
            while(!FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS,
                                 _INACTIVE, data32))
            {
                CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNum, linkIndex), &data32));
                HDCP22WIRED_SPIN_WAIT_US(100000U);
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

#if defined(HDCP22_SUPPORT_MST) && !defined(UPROC_RISCV)
    //
    // We clear ECF irrespective of MST or not, if time slots are zero it will not have any affect
    // TODO: Add selwreEcf support to RISC-V.
    //
    CHECK_STATUS(hdcp22wiredClearDpEcf_HAL(&Hdcp22wired, maxNoOfSors, maxNoOfHeads, sorNum));
#endif

label_return:
    return status;
}

/*!
 * @brief Checks whether a SOR is powered down.
 *
 * @param[in]   sorNum          SOR number
 * @param[in]   dfpSublinkMask  Sublink mask of display
 * @param[out]  pBSorPowerDown  Pointer to fill if a SOR is powered down
 * @returns     FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsSorPoweredDown_v03_00
(
    LwU8   sorNum,
    LwU8   dfpSublinkMask,
    LwBool *pBSorPowerDown
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data32;
    LwU32       mask    = 0;

    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_LANE_SEQ_CTL(sorNum), &data32));

    switch (dfpSublinkMask)
    {
        case SUBLINK_MASK_LINK_A:
        {
            mask = (DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE0_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE1_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE2_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE4_STATE, _POWERDOWN));
            break;
        }
        case SUBLINK_MASK_LINK_B:
        {
            mask = (DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE5_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE6_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE7_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE9_STATE, _POWERDOWN));
            break;
        }
        case SUBLINK_MASK_LINK_AB:
        {
            mask = (DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE0_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE1_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE2_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE4_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE5_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE6_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE7_STATE, _POWERDOWN) |
                    DRF_DEF(_SSE_SE_SWITCH_DISP, _SOR_LANE_SEQ_CTL, _LANE9_STATE, _POWERDOWN));
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

label_return:
    return status;
}

#ifdef HDCP22_SUPPORT_MST
/*!
 *  @brief Init LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_CTRL reg setting.
 */
FLCN_STATUS
hdcp22wiredInitSorHdcp22CtrlReg_v03_00(void)
{
#if defined(UPROC_RISCV)
    // TODO: keep same to _v03_00 once selwreECF supported.
    return hdcp22wiredInitSorHdcp22CtrlReg_v02_05();
#else
    LwU32       sorIndex;
    LwU32       linkIndex;
    FLCN_STATUS status  = FLCN_OK;
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
            CHECK_STATUS(_hdcp22SetDpLockEcf_v03_00(sorIndex, linkIndex, LW_TRUE));
        }
    }

label_return:
    return status;
#endif // UPROC_RISCV
}

/*!
 * @brief Writes the 2 32 bits ecf value to HW. The ecf value contains a mask of
 * which timeSlots require encryption.
 *
 * @param[in] sorIndex      sor index number
 * @param[in] pEcfTimeslot  Mask array of timeSlots requiring encryption
 */
FLCN_STATUS
hdcp22wiredWriteDpEcf_v03_00
(
    LwU8    sorIndex,
    LwU32   *pEcfTimeslot
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data32;
    LwU32       timeSlots;

    // TODO: Error handling need to be done
    // Unlock ecf protection.
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v03_00(sorIndex, 0, LW_FALSE));
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v03_00(sorIndex, 1, LW_FALSE));

    // Update the _ECF0 register with the corresponding ECF bits
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), &data32));

    timeSlots = pEcfTimeslot[0];
    data32 = FLD_SET_DRF_NUM(_SSE_SE_SWITCH_DISP_SOR, _DP_ECF0, _VALUE, timeSlots, data32);
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF0(sorIndex), data32));

    // Update the _ECF1 register with the corresponding ECF bits
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), &data32));
    timeSlots = pEcfTimeslot[1];
    data32 = FLD_SET_DRF_NUM(_SSE_SE_SWITCH_DISP_SOR, _DP_ECF1, _VALUE, timeSlots, data32);
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_SSE_SE_SWITCH_DISP_SOR_DP_ECF1(sorIndex), data32));

    // Lock ecf protection again.
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v03_00(sorIndex, 0, LW_TRUE));
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v03_00(sorIndex, 1, LW_TRUE));

label_return:
    return status;
}
#endif

/*!
 * @brief  Reads blanking register, set by SEC2
 *         and returns the same
 * @param[out] pBType1LockActive Pointer to fill if type1 is locked or not
 * @returns    FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsType1LockActive_v03_00
(
    LwBool *pBType1LockActive
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    *pBType1LockActive = LW_TRUE;

    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, &data32));

    if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _UPSTREAM_HDCP_VPR_POLICY_CTRL , _BLANK_VPR_ON_HDCP22_TYPE0, _DISABLE, data32))
    {
        *pBType1LockActive = LW_FALSE;
    }

label_return:
    return status;
}

/*!
 * @brief  Reads blanking register, set by SEC2
 *         and returns the same
 * @param[out] pBType1LockActive Pointer to fill if type1 is locked or not
 * @returns    FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsType1LockActiveHs_v03_00
(
    LwBool *pBType1LockActive
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    *pBType1LockActive = LW_TRUE;

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_SSE_SE_SWITCH_DISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, &data32));

    if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _UPSTREAM_HDCP_VPR_POLICY_CTRL , _BLANK_VPR_ON_HDCP22_TYPE0, _DISABLE, data32))
    {
        *pBType1LockActive = LW_FALSE;
    }

label_return:
    return status;
}

/*!
 * @brief This function do AES-CBC encryption/decryption to hdcp22 confidential
 * secrets.
 * @param[in]  pRn              Pointer to crypt random number.
 * @param[in]  pSecret          Pointer to confidential secret data.
 * @param[out] pDest            Pointer to destination data.
 * @param[in]  size             Size to encrypt or decrypt
 * @param[in]  bIsEncrypt       Flag to tell crypt operation is encrypt or decrypt
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredConfidentialSecretDoCrypt_v03_00
(
    LwU32              *pRandom,
    LwU32              *pSecret,
    LwU32              *pDest,
    LwU32               size,
    LwBool              bIsEncrypt
)
{
    FLCN_STATUS     status = FLCN_OK;
#if defined(HDCP22_USE_SCP_ENCRYPT_SECRET)
    LwU8            tmpBuff[SECRET_SCP_CRYPT_BUFF_SIZE + SCP_BUF_ALIGNMENT - 1];
    LwU8           *pScpBuff = (LwU8 *)LW_ALIGN_UP((LwUPtr)tmpBuff, SCP_BUF_ALIGNMENT);
    LwU32          *pSalt;
    LwU8           *pIv;
    LwU8           *pSrc;
    LwU8           *pOutput;

    if((pRandom == NULL) ||
       (pSecret == NULL) ||
       (pDest == NULL) ||
       (size % SCP_BUF_ALIGNMENT) ||
       (size > MAX_SECRET_SCP_CRYPT_SIZE))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Assign salt 16bytes.
    pSalt = (LwU32*)&pScpBuff[SECRET_SCP_CRYPT_SALT_OFFSET];
    pSalt[0] = LW_SCP_SECRET_KDF_SALT_HDCP22_PART_1;
    pSalt[1] = LW_SCP_SECRET_KDF_SALT_HDCP22_PART_2;
    pSalt[2] = LW_SCP_SECRET_KDF_SALT_HDCP22_PART_3;
    pSalt[3] = LW_SCP_SECRET_KDF_SALT_HDCP22_PART_4;

    // Assign input vector as 16bytes random number.
    pIv = &pScpBuff[SECRET_SCP_CRYPT_IV_OFFSET];
    memcpy_hs(pIv, pRandom, SECRET_SCP_CRYPT_IV_SIZE);

    // Assign input, max 32bytes.
    pSrc = &pScpBuff[SECRET_SCP_CRYPT_INPUT_OFFSET];
    memcpy_hs(pSrc, pSecret, size);

    pOutput = &pScpBuff[SECRET_SCP_CRYPT_OUTPUT_OFFSET];

    status = scpCbcCrypt((LwU8 *)pSalt,                     // Salt
                         bIsEncrypt,                        // Encrypt or Decrypt
                         LW_FALSE,                          // bShortLwt
                         LW_SCP_SECRET_IDX_HDCP22_KEY,      // SCP key index
                         pSrc,                              // Source
                         size,                              // size
                         pOutput,                           // Destination
                         pIv);                              // Input vector
    if (status != FLCN_OK)
    {
        return status;
    }

    // Copy output
    memcpy_hs(pDest, pOutput, size);

    // Scrub stack array.
    memset_hs(tmpBuff, sizeof(tmpBuff), 0);
#endif

    return status;
}

/*!
 * @brief      Check or update integrity required vars' sanity
 * @param[in]  pData            Pointer to input data.
 * @param[out] pHashValue       Pointer to hash value for integrity check.
 * @param[in]  size             size of input data.
 * @param[in]  bIsUpdate        Flag to tell if check or update.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredCheckOrUpdateStateIntegrity_v03_00
(
    LwU32  *pData,
    LwU32  *pHashValue,
    LwU32   size,
    LwBool  bIsUpdate
)
{
    FLCN_STATUS status = FLCN_OK;

#if defined(HDCP22_CHECK_STATE_INTEGRITY)
    LwU32       calHashVal[HDCP22_SIZE_INTEGRITY_HASH_U32];

    if((pData == NULL) || (pHashValue == NULL) ||
       (size > MAX_HDCP22_INTEGRITY_CHECK_SIZE))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

    hdcpmemset(calHashVal, 0, sizeof(calHashVal));

    status = hdcp22wiredDoCallwlateHash(calHashVal, pData, size);
    if (status != FLCN_OK)
    {
        goto label_return;
    }

    if (bIsUpdate)
    {
        hdcpmemcpy(pHashValue, calHashVal, HDCP22_SIZE_INTEGRITY_HASH);
    }
    else
    {
        if (hdcpmemcmp(pHashValue, calHashVal, HDCP22_SIZE_INTEGRITY_HASH))
        {
            status = FLCN_ERR_ILWALID_STATE;
        }
    }

label_return:
    // scrub from stack.
    hdcpmemset(calHashVal, 0, sizeof(calHashVal));
#endif

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
hdcp22wiredGetRandNumber_v03_00
(
    LwU32  *pDest,
    LwS32   size
)
{
#if defined(UPROC_RISCV)
    LwU32               cnt;
    LwU32               val;
    LwU8                sorNum = 0;
    FLCN_STATUS status  = FLCN_OK;

    // TODO: To replace this with RISCV APIs of SCP RNG/LWRNG.

    for (cnt = 0; cnt < size; cnt+=2)
    {
        LwU32 msb = 0;
        LwU32 lsb = 0;
        LwU32 data = 0;
        LwU16 offset = 0;
        LwU8  ranCount = 0;

        data = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _RUN, _NO, data);
        data = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _DONE, _NO, data);

        DISP_REG_WR32_ERR_CHK_HS(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum), data);

        data = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _RUN, _YES, data);
        DISP_REG_WR32_ERR_CHK_HS(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum), data);

        while (LW_TRUE)
        {
            DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum), &data);

            if (FLD_TEST_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _DONE, _YES, data))
            {
                break;
            }
            if (ranCount++ > HDCP22_RND_NUM_WAIT_CNT)
            {
                break;
            }
        }

        DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_SOR_HDCP22_RNDGEN_NUM_MSB(sorNum), &msb);
        DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_SOR_HDCP22_RNDGEN_NUM_LSB(sorNum), &lsb);

        offset = cnt;

        *((LwU32 *) (pDest + offset)) = msb;
        *((LwU32 *) (pDest + offset + 1)) = lsb;
    }

    val = 0;
    val = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _RUN, _NO, val);
    val = FLD_SET_DRF(_PDISP, _SOR_HDCP22_RNDGEN_CTRL, _DONE, _NO, val);

    DISP_REG_WR32_ERR_CHK_HS(LW_PDISP_SOR_HDCP22_RNDGEN_CTRL(sorNum), val);

    return status;
#else
#ifdef LIB_CCC_PRESENT    
    // TODO: Call LibCCC wrapper once implemented in LibCCC. Tracking Bug: 200762634
    if (FLCN_OK != hdcp22LibCccGetRandomNumberHs(pDest, size))
    {
        return FLCN_ERR_HS_GEN_RANDOM;
    }
#else    
    if (SE_OK != seTrueRandomGetNumberHs(pDest, size))
    {
        return FLCN_ERR_HS_GEN_RANDOM;
    }
#endif
    return FLCN_OK;
#endif
}

/*!
 * @brief       This function gets maximum number of sor, head.
 * @param[out]  pMaxNoOfSors  Pointer to get maximum no of sor
 * @param[out]  pMaxNoOfHeads Pointer to get maximum no of heads
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetMaxNoOfSorsHeads_v03_00
(
    LwU8   *pMaxNoOfSors,
    LwU8   *pMaxNoOfHeads
)
{
    LwU32 data = 0;

    if (FLCN_OK != DISP_REG_RD32_ERR_CHK(LW_PDISP_FE_MISC_CONFIGA, &data))
    {
        return FLCN_ERR_BAR0_PRIV_READ_ERROR;
    }
    else
    {
        if (pMaxNoOfSors)
        {
            *pMaxNoOfSors = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_SORS, data);
        }

        if (pMaxNoOfHeads)
        {
            *pMaxNoOfHeads = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_HEADS, data);
        }
    }

    return FLCN_OK;
}

/*!
 * @brief       This function gets maximum number of sor, head in HS
 * @param[out]  pMaxNoOfSors  Pointer to get maximum no of sor
 * @param[out]  pMaxNoOfHeads Pointer to get maximum no of heads
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetMaxNoOfSorsHeadsHs_v03_00
(
    LwU8   *pMaxNoOfSors,
    LwU8   *pMaxNoOfHeads
)
{
    LwU32 data = 0;

    if (!pMaxNoOfSors || !pMaxNoOfHeads)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (FLCN_OK != DISP_REG_RD32_ERR_CHK_HS(LW_PDISP_FE_MISC_CONFIGA, &data))
    {
        *pMaxNoOfSors = 0;
        *pMaxNoOfHeads = 0;
        return FLCN_ERR_BAR0_PRIV_READ_ERROR;
    }
    else
    {
        *pMaxNoOfSors = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_SORS, data);
        *pMaxNoOfHeads = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_HEADS, data);
    }

    return FLCN_OK;
}

/*!
 * @brief Check if encryption disabled due to SOR lane count is set to 0 in DP mode.
 * @param[in] sorIndex                  sor index number
 * @param[out] pbDisabledWithLaneCount0   Pointer to check result.
 *
 * @return FLCN_OK if supported.
 */
FLCN_STATUS
hdcp22wiredIsAutoDisabledWithLaneCount0_v03_00
(
    LwU8    sorNumber,
    LwBool *pbDisabledWithLaneCount0
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data32    = 0;

    if ((NULL == pbDisabledWithLaneCount0) || (sorNumber > LW_PDISP_MAX_SOR))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Volta BAR0 access may return HW error.
    CHECK_STATUS(hdcp22wiredReadRegister(LW_SSE_SE_SWITCH_DISP_SOR_HDCP22_STATUS(sorNumber, 0),
                                         &data32));
    *pbDisabledWithLaneCount0 = FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS,
                                             _AUTODIS_STATE, _DISABLE_LC_0,
                                             data32);

label_return:
    return status;
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
hdcp22wiredWriteToSelwreMemory_v03_00
(
    HDCP22_SECRET   hdcp22Secret,
    LwU8           *pSecret,
    LwBool          bIsReset
)
{
#if defined(UPROC_RISCV)
    // TODO: To replace this with RISCV APIs of SCP, KMEM.
    return hdcp22wiredWriteToSelwreMemory_v02_05(hdcp22Secret, pSecret, bIsReset);
#else
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

    if (bIsConfidentialData)
    {
        if (bIsReset)
        {
            // To reset case, clear secret value.
            memset_hs(pDest, 0, size);
        }
        else
        {
            // Confidential secret must be LwU32 aligned.
            if (((LwUPtr)pDest % sizeof(LwU32)) || ((LwUPtr)pSecret % sizeof(LwU32)))
            {
                status = FLCN_ERR_ILLEGAL_OPERATION;
                goto label_return;
            }

#ifndef HDCP22_KMEM_ENABLED
            CHECK_STATUS(_hdcp22wiredWriteConfidentialSecret_v03_00((LwU32 *)pDest,
                                                                    (LwU32 *)pSecret,
                                                                    hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
                                                                    size));
#endif
        }
    }
    else
    {
        if (bIsReset)
        {
            // To reset case, clear secret value.
            memset_hs(pDest, 0, size);
        }
#if defined(HDCP22_CHECK_STATE_INTEGRITY)
        else
        {
            LwU32   hash[HDCP22_SIZE_INTEGRITY_HASH_U32];

            // To not-reset case, need to check previous integrity hash before updating.
            memcpy_hs(hash, hdcp22SelwreMemory.IntegrityHash, HDCP22_SIZE_INTEGRITY_HASH);
            memset_hs(hdcp22SelwreMemory.IntegrityHash, 0, HDCP22_SIZE_INTEGRITY_HASH);
            status = hdcp22CallwateHashHs(hdcp22SelwreMemory.IntegrityHash,
                                          (LwU32 *)&hdcp22SelwreMemory.secAssetsIntegrity,
                                          sizeof(SECRET_ASSETS_INTEGRITY));
            if (status != FLCN_OK)
            {
                goto label_return;
            }

            if (memcmp_hs(hdcp22SelwreMemory.IntegrityHash, hash,
                          HDCP22_SIZE_INTEGRITY_HASH))
            {
                memcpy_hs(hdcp22SelwreMemory.IntegrityHash, hash,
                          HDCP22_SIZE_INTEGRITY_HASH);
                status = FLCN_ERR_HS_HDCP22_INTEGRITY_ERROR;
                goto label_return;
            }

            memcpy_hs(pDest, pSecret, size);
        }

        // Generate integrity hash and save that.
        memset_hs(hdcp22SelwreMemory.IntegrityHash, 0, HDCP22_SIZE_INTEGRITY_HASH);
        status = hdcp22CallwateHashHs(hdcp22SelwreMemory.IntegrityHash,
                                      (LwU32 *)&hdcp22SelwreMemory.secAssetsIntegrity,
                                      sizeof(SECRET_ASSETS_INTEGRITY));
        if (status != FLCN_OK)
        {
            goto label_return;
        }
#else
        else
        {
            memcpy_hs(pDest, pSecret, size);
        }
#endif // HDCP22_CHECK_STATE_INTEGRITY
    }

label_return:
    return status;
#endif // UPROC_RISCV
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
hdcp22wiredReadFromSelwreMemory_v03_00
(
    HDCP22_SECRET   hdcp22Secret,
    LwU8           *pSecret
)
{
#if defined(UPROC_RISCV)
    // TODO: To replace this with RISCV APIs of SCP, KMEM.
    return hdcp22wiredReadFromSelwreMemory_v02_05(hdcp22Secret, pSecret);
#else
    FLCN_STATUS status = FLCN_OK;
    LwU32       size = 0;
    LwBool      bIsConfidentialData = LW_FALSE; // TRUE is confidential data otherwise data need integrity checked.
    LwU8       *pSrc = NULL;
    LwU32       decData[sizeof(SECRET_ASSETS_CONFIDENTIAL)/sizeof(LwU32)];

    memset_hs(decData, 0, sizeof(decData));

    switch (hdcp22Secret)
    {
    case HDCP22_SECRET_DKEY:
        {
            bIsConfidentialData = LW_TRUE;
            pSrc = (LwU8 *)decData;
            size = HDCP22_SIZE_DKEY;
#ifndef HDCP22_KMEM_ENABLED
            CHECK_STATUS(_hdcp22wiredReadConfidentialSecret_v03_00(hdcp22SelwreMemory.secAssetsConfidential.encDkey,
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
            CHECK_STATUS(_hdcp22wiredReadConfidentialSecret_v03_00(hdcp22SelwreMemory.secAssetsConfidential.encKd,
                                                                   decData,
                                                                   hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
                                                                   size));
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
#if defined(HDCP22_CHECK_STATE_INTEGRITY)
        LwU32   hash[HDCP22_SIZE_INTEGRITY_HASH_U32];

        memcpy_hs(hash, hdcp22SelwreMemory.IntegrityHash, HDCP22_SIZE_INTEGRITY_HASH);
        memset_hs(hdcp22SelwreMemory.IntegrityHash, 0, HDCP22_SIZE_INTEGRITY_HASH);
        status = hdcp22CallwateHashHs(hdcp22SelwreMemory.IntegrityHash,
                                      (LwU32 *)&hdcp22SelwreMemory.secAssetsIntegrity,
                                      sizeof(SECRET_ASSETS_INTEGRITY));
        if (status != FLCN_OK)
        {
            goto label_return;
        }

        if (memcmp_hs(hdcp22SelwreMemory.IntegrityHash, hash,
                      HDCP22_SIZE_INTEGRITY_HASH))
        {
            memcpy_hs(hdcp22SelwreMemory.IntegrityHash, hash,
                      HDCP22_SIZE_INTEGRITY_HASH);
            status = FLCN_ERR_HS_HDCP22_INTEGRITY_ERROR;
            goto label_return;
        }
#endif
    }

    memcpy_hs(pSecret, pSrc, size);

label_return:
    return status;
#endif // UPROC_RISCV
}

/*!
 * @brief       Save active session secrets per SOR.
 * @param[in]   sorNum      SOR number
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredSaveSessionSecrets_v03_00
(
    LwU8 sorNum
)
{
#if defined(UPROC_RISCV)
    return hdcp22wiredSaveSessionSecrets_v02_05(sorNum);
#else
    #if !defined(HDCP22_KMEM_ENABLED)
    memcpy_hs(hdcp22SelwreMemory.secAssetsActiveSession[sorNum].rnSesCrypt,
              hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
              HDCP22_SIZE_SESSION_CRYPT);
    #endif // !HDCP22_KMEM_ENABLED

    memcpy_hs(hdcp22SelwreMemory.secAssetsActiveSession[sorNum].encKd,
              hdcp22SelwreMemory.secAssetsConfidential.encKd,
              HDCP22_SIZE_KD);

    return FLCN_OK;
#endif // UPROC_RISCV
}

/*!
 * @brief       Retrieve active session secrets per SOR.
 * @param[in]   sorNum                  SOR number
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredRetrieveSessionSecrets_v03_00
(
    LwU8                            sorNum
)
{
#if defined(UPROC_RISCV)
    return hdcp22wiredRetrieveSessionSecrets_v02_05(sorNum);
#else
    #if !defined(HDCP22_KMEM_ENABLED)
    memcpy_hs(hdcp22SelwreMemory.secAssetsIntegrity.rnSesCrypt,
              hdcp22SelwreMemory.secAssetsActiveSession[sorNum].rnSesCrypt,
              HDCP22_SIZE_SESSION_CRYPT);
    #endif // !HDCP22_KMEM_ENABLED

    memcpy_hs(hdcp22SelwreMemory.secAssetsConfidential.encKd,
              hdcp22SelwreMemory.secAssetsActiveSession[sorNum].encKd,
              HDCP22_SIZE_KD);

    return FLCN_OK;
#endif // UPROC_RISCV
}

