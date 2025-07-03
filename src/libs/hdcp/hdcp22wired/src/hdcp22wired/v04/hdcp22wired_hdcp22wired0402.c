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
 * @file   hdcp22wired_hdcp22wired0402.c
 * @brief  Hdcp22 wired 04.02 Hal Functions
 *
 * HDCP2.2 Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dev_disp.h"
#include "osptimer.h"

/* ------------------------ OpenRTOS includes ------------------------------ */
/* ------------------------ Application includes --------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_timer.h"
#include "seapi.h"
#include "setypes.h"
#include "scp_common.h"
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
#include "address_map_new.h"
#include "arfuse.h"
#include <syslib/syslib.h>
#include <lwriscv/print.h>
#include "lib_intfci2c.h"
#if defined(UPROC_RISCV) && LWRISCV_HAS_DIO_SNIC
#include "stdint.h"
#include "liblwriscv/io_dio.h"
#endif

/* ------------------------ Types definitions ------------------------------ */
#define DISP_ADDR_MAP_TEGRA(addr)   ((LW_ADDRESS_MAP_DISP_BASE) + addr - (DRF_BASE(LW_PDISP)))

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
#define SUBLINK_MASK_LINK_A                        0x1
#define SUBLINK_MASK_LINK_B                        0x2
#define SUBLINK_MASK_LINK_AB                       0x3

#define SECRET_SCP_CRYPT_SALT_OFFSET               (0)
#define SECRET_SCP_CRYPT_SALT_SIZE                 (SCP_BUF_ALIGNMENT)
#define SECRET_SCP_CRYPT_IV_OFFSET                 (SECRET_SCP_CRYPT_SALT_OFFSET + SECRET_SCP_CRYPT_SALT_SIZE)
#define SECRET_SCP_CRYPT_IV_SIZE                   (SCP_BUF_ALIGNMENT)
#define SECRET_SCP_CRYPT_INPUT_OFFSET              (SECRET_SCP_CRYPT_IV_OFFSET + SECRET_SCP_CRYPT_IV_SIZE)
#define SECRET_SCP_CRYPT_OUTPUT_OFFSET             (SECRET_SCP_CRYPT_INPUT_OFFSET + MAX_SECRET_SCP_CRYPT_SIZE)
#define SECRET_SCP_CRYPT_BUFF_SIZE                 (SECRET_SCP_CRYPT_SALT_SIZE + SECRET_SCP_CRYPT_IV_SIZE + MAX_SECRET_SCP_CRYPT_SIZE + MAX_SECRET_SCP_CRYPT_SIZE)

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
static FLCN_STATUS _hdcp22wiredWriteMstKs_v04_02(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 *pKs)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22wiredWriteMstKs_v04_02");
static FLCN_STATUS _hdcp22WriteMstRiv_v04_02(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 bytesToWrite0, LwU32 bytesToWrite1)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22WriteMstRiv_v04_02");
FLCN_STATUS hdcp22wiredWriteRiv_v04_02(LwU8 sorNum, LwU8 linkIndex, LwBool bIsMst, LwU32 *pRiv)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredWriteRiv_v04_02");
static FLCN_STATUS _hdcp22wiredWriteMstStreamType_v04_02(LwU8 sorNum, LwU8 linkIndex, HDCP22_STREAM streamIdType[HDCP22_NUM_STREAMS_MAX], LwU32 dpTypeMask[HDCP22_NUM_DP_TYPE_MASK])
    GCC_ATTRIB_SECTION("imem_hdcp22ControlEncryption", "_hdcp22wiredWriteMstStreamType_v04_02");

#ifdef HDCP22_SUPPORT_MST
static FLCN_STATUS _hdcp22SetDpLockEcf_v04_02(LwU32 sorIndex, LwU32 linkIndex, LwBool lock)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SetDpLockEcf_v04_02");
static FLCN_STATUS _hdcp22SetDpLockEcfHs_v04_02(LwU32 sorIndex, LwU32 linkIndex, LwBool lock)
    GCC_ATTRIB_SECTION("imem_hdcp22WriteDpEcf", "_hdcp22SetDpLockEcfHs_v04_02");
#endif

FLCN_STATUS hdcp22wiredConfidentialSecretDoCrypt_v04_02(LwU32 *pRn, LwU32 *pSecret, LwU32 *pDest, LwU32 size, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredConfidentialSecretDoCrypt_v04_02");
FLCN_STATUS hdcp22wiredGetRandNumber_v04_02(LwU32 *pDest, LwS32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredGetRandNumber_v04_02");

/* ------------------------- Private Functions ------------------------------ */
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
static FLCN_STATUS
_hdcp22wiredWriteMstKs_v04_02
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pKs
)
{
    FLCN_STATUS status = FLCN_OK;

#ifdef HDCP22_SUPPORT_MST
    // HDMI, SST's streamId must be 0, while MST must not be 0.
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
           LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB1(sorNum, !linkIndex),
           pKs[0]));

        CHECK_STATUS(hdcp22wiredWriteRegisterHs(
           LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB2(sorNum, !linkIndex),
           pKs[1]));

       CHECK_STATUS(hdcp22wiredWriteRegisterHs(
           LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB3(sorNum, !linkIndex),
           pKs[2]));

       CHECK_STATUS(hdcp22wiredWriteRegisterHs(
           LW_PDISP_SOR_HDCP22_AES_CTR_KEY_MSB(sorNum, !linkIndex),
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
_hdcp22WriteMstRiv_v04_02
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
            LW_PDISP_SOR_HDCP22_AES_CTR_DATA_LSB(sorNum, !linkIndex),
            bytesToWrite1));

        CHECK_STATUS(hdcp22wiredWriteRegisterHs(
            LW_PDISP_SOR_HDCP22_AES_CTR_DATA_MSB(sorNum, !linkIndex),
            bytesToWrite0));
    }

label_return:
#endif

    return status;
}

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
_hdcp22SetDpLockEcf_v04_02
(
    LwU32       sorIndex,
    LwU32       linkIndex,
    LwBool      lock
)
{
    LwU32 data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_SOR_HDCP22_CTRL(sorIndex, linkIndex)),
                                         &data32));

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

    CHECK_STATUS(hdcp22wiredWriteRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_SOR_HDCP22_CTRL(sorIndex, linkIndex)),
                                          data32));

label_return:
    return status;
}

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
_hdcp22SetDpLockEcfHs_v04_02
(
    LwU32       sorIndex,
    LwU32       linkIndex,
    LwBool      lock
)
{
    LwU32 data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), &data32));

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

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_HDCP22_CTRL(sorIndex, linkIndex), data32));

label_return:
    return status;
}

#endif

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
_hdcp22wiredWriteMstStreamType_v04_02
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
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_HDCP22_DP_TYPE_LSB(sorNum), dpTypeMask[0]));
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_HDCP22_DP_TYPE_MSB(sorNum), dpTypeMask[1]));
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
hdcp22wiredWriteKs_v04_02
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsMst,
    LwU32  *pKs
)
{
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
        LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB1(sorNum, linkIndex),
        pKs[0]));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
        LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB2(sorNum, linkIndex),
        pKs[1]));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
        LW_PDISP_SOR_HDCP22_AES_CTR_KEY_LSB3(sorNum, linkIndex),
        pKs[2]));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
        LW_PDISP_SOR_HDCP22_AES_CTR_KEY_MSB(sorNum, linkIndex),
        pKs[3]));

    _hdcp22wiredWriteMstKs_v04_02(sorNum, linkIndex, bIsMst, pKs);

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
hdcp22wiredWriteRiv_v04_02
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

    swapEndianness(&bytesToWrite1, &pRiv[1], sizeof(LwU32));
    swapEndianness(&bytesToWrite0, &pRiv[0], sizeof(LwU32));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
        LW_PDISP_SOR_HDCP22_AES_CTR_DATA_LSB(sorNum, linkIndex),
        bytesToWrite1));

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(
        LW_PDISP_SOR_HDCP22_AES_CTR_DATA_MSB(sorNum, linkIndex),
        bytesToWrite0));

    status = _hdcp22WriteMstRiv_v04_02(sorNum, linkIndex, bIsMst,
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
hdcp22wiredWriteStreamType_v04_02
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
        status = _hdcp22wiredWriteMstStreamType_v04_02(sorNum, linkIndex, streamIdType, dpTypeMask);
        if (status == FLCN_ERR_NOT_SUPPORTED)
        {
            // SST and type1Locked case is already processed at StartSession.
            status = FLCN_OK;
            CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_HDCP22_SST_DP_TYPE(sorNum, linkIndex),
                                                     streamIdType[0].streamType));
        }
    }
    else
    {
        // HDMI type1Locked case is already processed at StartSession.
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_HDCP22_HDMI_TYPE(sorNum),
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
hdcp22wiredEncryptionActive_v04_02
(
    LwU8 sorNumber,
    LwBool *pBEncActive
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32;

    CHECK_STATUS(hdcp22wiredReadRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_SOR_HDCP22_STATUS(sorNumber,0)),
                                         &data32));
    *pBEncActive = FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, data32);

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
hdcp22wiredEncryptionActiveHs_v04_02
(
    LwU8 sorNumber,
    LwBool *pBEncActive
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       data32;
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_HDCP22_STATUS(sorNumber, 0), &data32));
    *pBEncActive = FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, data32);

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
hdcp22wiredIsSorDrivesEightLaneDp_v04_02
(
    LwU8 sorNumber,
    LwBool *pBSor8LaneDp
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_SOR_DP_LINKCTL0(sorNumber)), 
                                         &data32));

    *pBSor8LaneDp = FLD_TEST_DRF(_PDISP, _SOR_DP_LINKCTL0,
        _LANECOUNT, _EIGHT, data32);

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
hdcp22wiredIsSorDrivesEightLaneDpHs_v04_02
(
    LwU8 sorNumber,
    LwBool *pBSor8LaneDp
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_DP_LINKCTL0(sorNumber), &data32));

    *pBSor8LaneDp = FLD_TEST_DRF(_PDISP, _SOR_DP_LINKCTL0,
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
hdcp22wiredHandleEnableEnc_v04_02
(
    LwU8    sorNum,
    LwU8    linkIndex,
    LwBool  bIsRepeater,
    LwBool  bApplyHwDrmType1LockedWar
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       hdcpCtrl = 0;
    LwU32       ctrlReg = LW_PDISP_SOR_HDCP22_CTRL(sorNum, linkIndex);
    LwU32       statusReg = LW_PDISP_SOR_HDCP22_STATUS(sorNum, linkIndex);
    LwU32       cryptStatus = 0;
    LwU32       tickCount = 0;

    // Trigger HDCP22 CTRL INIT
    CHECK_STATUS(hdcp22wiredReadRegisterHs(ctrlReg, &hdcpCtrl));
    hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _INIT, _TRIGGER, hdcpCtrl);

    if (bIsRepeater)
    {
        hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _REPEATER, _YES, hdcpCtrl);
    }
    else
    {
        hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _REPEATER, _NO, hdcpCtrl);
    }

    CHECK_STATUS(hdcp22wiredWriteRegisterHs(ctrlReg, hdcpCtrl));

    // Poll LW_PDISP_SOR_HDCP22_CTRL_INIT_DONE
    do
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(ctrlReg, &hdcpCtrl));
    } while (!FLD_TEST_DRF(_PDISP, _SOR_HDCP22_CTRL, _INIT, _DONE, hdcpCtrl));

    // Write LW_PDISP_SOR_HDCP22_CTRL_ENABLE_YES
    hdcpCtrl = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _CRYPT, _ENABLE, hdcpCtrl);
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(ctrlReg, hdcpCtrl));

    // Poll LW_PDISP_SOR_HDCP22_CTRL_CRYPT_STS = _ACTIVE
    CHECK_STATUS(hdcp22wiredReadRegisterHs(statusReg, &cryptStatus));
    while (!FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS, _ACTIVE, cryptStatus))
    {
        CHECK_STATUS(hdcp22wiredReadRegisterHs(statusReg, &cryptStatus));
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
hdcp22wiredHandleDisableEnc_v04_02
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
        CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_HDCP22_CTRL(sorNum, linkIndex), &data32));
        data32 = FLD_SET_DRF(_PDISP, _SOR_HDCP22_CTRL, _CRYPT, _DISABLE, data32);
        CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_HDCP22_CTRL(sorNum, linkIndex), data32));

        if (bCheckCryptStatus)
        {
            CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_HDCP22_STATUS(sorNum, linkIndex), &data32));
            while(!FLD_TEST_DRF(_PDISP, _SOR_HDCP22_STATUS, _CRYPT_STATUS,
                                 _INACTIVE, data32))
            {
                CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_HDCP22_STATUS(sorNum, linkIndex), &data32));
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
hdcp22wiredIsSorPoweredDown_v04_02
(
    LwU8    sorNum,
    LwU8    dfpSublinkMask,
    LwBool *pBSorPowerDown
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data32;
    LwU32       mask    = 0;

    CHECK_STATUS(hdcp22wiredReadRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_SOR_LANE_SEQ_CTL(sorNum)),
                                         &data32));

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

label_return:
    return status;
}

#ifdef HDCP22_SUPPORT_MST
/*!
 *  @brief Init LW_PDISP_SOR_HDCP22_CTRL reg setting.
 */
FLCN_STATUS
hdcp22wiredInitSorHdcp22CtrlReg_v04_02(void)
{
    LwU32       sorIndex;
    LwU32       linkIndex;
    FLCN_STATUS status  = FLCN_OK;
    //
    // Set that ECF register can be written by protected SW (DPU).
    // Default is unlocked and set to lock after DPU loaded.
    //
    for (sorIndex = 0; sorIndex < LW_PDISP_MAX_SOR; sorIndex ++ )
    {
        for (linkIndex = 0; linkIndex < 2; linkIndex ++)
        {
            CHECK_STATUS(_hdcp22SetDpLockEcf_v04_02(sorIndex, linkIndex, LW_TRUE));
        }
    }

label_return:
    return status;
}

/*!
 * @brief Writes the 2 32 bits ecf value to HW. The ecf value contains a mask of
 * which timeSlots require encryption.
 *
 * @param[in] sorIndex      sor index number
 * @param[in] pEcfTimeslot  Mask array of timeSlots requiring encryption
 */
FLCN_STATUS
hdcp22wiredWriteDpEcf_v04_02
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
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v04_02(sorIndex, 0, LW_FALSE));
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v04_02(sorIndex, 1, LW_FALSE));

    // Update the _ECF0 register with the corresponding ECF bits
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_DP_ECF0(sorIndex), &data32));
    timeSlots = pEcfTimeslot[0];
    data32 = FLD_SET_DRF_NUM(_PDISP_SOR, _DP_ECF0, _VALUE, timeSlots, data32);
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_DP_ECF0(sorIndex), data32));

    // Update the _ECF1 register with the corresponding ECF bits
    CHECK_STATUS(hdcp22wiredReadRegisterHs(LW_PDISP_SOR_DP_ECF1(sorIndex), &data32));
    timeSlots = pEcfTimeslot[1];
    data32 = FLD_SET_DRF_NUM(_PDISP_SOR, _DP_ECF1, _VALUE, timeSlots, data32);
    CHECK_STATUS(hdcp22wiredWriteRegisterHs(LW_PDISP_SOR_DP_ECF1(sorIndex), data32));

    // Lock ecf protection again.
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v04_02(sorIndex, 0, LW_TRUE));
    CHECK_STATUS(_hdcp22SetDpLockEcfHs_v04_02(sorIndex, 1, LW_TRUE));

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
hdcp22wiredIsType1LockActive_v04_02
(
    LwBool *pBType1LockActive
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    *pBType1LockActive = LW_TRUE;

    CHECK_STATUS(hdcp22wiredReadRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL), 
                                         &data32));

    if (FLD_TEST_DRF(_PDISP, _UPSTREAM_HDCP_VPR_POLICY_CTRL , _BLANK_VPR_ON_HDCP22_TYPE0, _DISABLE, data32))
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
hdcp22wiredConfidentialSecretDoCrypt_v04_02
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
hdcp22wiredCheckOrUpdateStateIntegrity_v04_02
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
 * @param[out]  pDest        Address in which random number will be written
 * @param[in]   size         size of random number in dwords
 * @returns     FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetRandNumber_v04_02
(
    LwU32  *pDest,
    LwS32   size
)
{
    // TODO 200586859: Need to define RNG function for CHEETAH
    *pDest = 0x1234;

    return FLCN_OK;
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
hdcp22wiredI2cPerformTransaction_v04_02
(
     LwU8       port,
     LwU32      dataOffset,
     LwU32      dataSize,
     LwU8      *pData,
     LwBool     bRead
)
{
    // TODO 200586859: Need to define I2C functionality for CHEETAH
    *pData = 0;
    return FLCN_OK;
}

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
hdcp22wiredAuxPerformTransaction_v04_02
(
     LwU8       port,
     LwU32      dataOffset,
     LwU32      dataSize,
     LwU8      *pData,
     LwBool     bRead
)
{
    // TODO 200586859: Need to define AUX functionality for CHEETAH
    *pData = 0;
    return FLCN_OK;
}

/*!
 * @brief       This function gets maximum number of sor.
 * @param[out]  pMaxNoOfSors  Pointer to get maximum no of sor
 * @param[out]  pMaxNoOfHeads Pointer to get maximum no of heads
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetMaxNoOfSorsHeads_v04_02
(
    LwU8   *pMaxNoOfSors,
    LwU8   *pMaxNoOfHeads
)
{
    LwU32 data = 0;

    if (FLCN_OK != hdcp22wiredReadRegister(DISP_ADDR_MAP_TEGRA(LW_PDISP_FE_MISC_CONFIGA),
                                           &data))
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
 * @brief       This function gets maximum number of sor.
 * @param[out]  pMaxNoOfSors  Pointer to get maximum no of sor
 * @param[out]  pMaxNoOfHeads Pointer to get maximum no of heads
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredGetMaxNoOfSorsHeadsHs_v04_02
(
    LwU8   *pMaxNoOfSors,
    LwU8   *pMaxNoOfHeads
)
{
    LwU32 data = 0;

    if (FLCN_OK != hdcp22wiredReadRegisterHs(LW_PDISP_FE_MISC_CONFIGA, &data))
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
 * @brief Read an register using the DIO
 *
 * @param[in]  addr     Address
 * @param[out] pData Data out for read request
 *
 * @return FLCN_OK if request is successful
 */
FLCN_STATUS
hdcp22wiredDioSnicReadHs
(
    LwU32       addr,
    LwU32      *pData
)
{
    // Only FMC config liblwriscv supports DIO_SNIC
#if LWRISCV_HAS_DIO_SNIC
    DIO_PORT dioPort;

    dioPort.dioType = DIO_TYPE_SNIC;
    dioPort.portIdx = 0;
    dioReadWrite(dioPort, DIO_OPERATION_RD, DISP_ADDR_MAP_TEGRA(addr), pData);
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
hdcp22wiredDioSnicWriteHs
(
    LwU32       addr,
    LwU32       val
)
{
    // Only FMC config liblwriscv supports DIO_SNIC.
#if LWRISCV_HAS_DIO_SNIC
    DIO_PORT dioPort;

    dioPort.dioType = DIO_TYPE_SNIC;
    dioPort.portIdx = 0;
    dioReadWrite(dioPort, DIO_OPERATION_WR, DISP_ADDR_MAP_TEGRA(addr), &val);
    return FLCN_OK;
#else
    // RISCV-V RTOS partition should not call the func.
    return FLCN_ERR_ILLEGAL_OPERATION;
#endif
}

