/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#ifndef HDCPMC_MTHDS_H
#define HDCPMC_MTHDS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lwuproc.h"
#include "sec2_chnmgmt.h"
#include "class/cl95a1.h"
#include "tsec_drv.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * WAR: A colwenience macro to use as a workaround for HDCP that has more than
 * 64 non-contiguous app methods. By subtracting this offset from the method ID
 * we can map it to a contiguous array that is greater than 64 bytes. No app
 * other than HDCP should require to use this macro, today.
 */
#define HDCP_MTHD_OFFSET_WAR (0x180) // ((0x700>>2)-0x40) or ((0x704>>2)-0x41)

/*!
 * @brief The SEC2 HDCP short-hand notation for the LW95A1 HDCP class method.
 *
 * @param[in]   mthd    SEC2 HDCP short-hand notation.
 *
 * @return  LW95A1 HDCP class method.
 */
#define HDCP_METHOD_ID(mthd)                                 LW95A1_HDCP_##mthd

/*!
 * @brief Colwerts an index in appMthdArray into the LW95A1 HDCP method value.
 *
 * Indices in the appMethodArray are contiguous, while HDCP method values are
 * not. Lwrrently, the HDCP method values range from INIT (0x500) to DECRYPT
 * (0x560), and from VALIDATE_SRM (0x700) to ENCRYPT (0x73C).
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  LW95A1 HDCP class method.
 */
#define HDCP_GET_METHOD_ID(idx)                                                \
    (((idx) < 0x40) ?                                                          \
    (HDCP_METHOD_ID(INIT) + ((idx) * 4)) :                                     \
    (HDCP_METHOD_ID(VALIDATE_SRM) + (((idx) - 0x40) * 4)))

/*!
 * @brief Obtains a pointer to the method's parameter which is stored within the
 *        frame buffer.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define HDCP_GET_METHOD_PARAM_OFFSET(idx)                      appMthdArray[idx]

/*!
 * @brief Colwerts an LW95A1 HDCP method value into an index into appMthdArray.
 *
 * Indices in the appMethodArray are contiguous, while HDCP method values are
 * not. Lwrrently, the HDCP method values range from INIT (0x500) to DECRYPT
 * (0x560), and from VALIDATE_SRM (0x700) to ENCRYPT (0x73C).
 *
 * @param[in]  mthd     LW95A1 HDCP method value.
 *
 * @return  Index within appMthdArray.
 */
#define HDCP_GET_METHOD_INDEX(mthd)                                            \
   ((((mthd) >= LW95A1_HDCP_VALIDATE_SRM) && ((mthd) <= LW95A1_HDCP_ENCRYPT)) ?\
       (((mthd) >> 2) - HDCP_MTHD_OFFSET_WAR) :                                \
        (((mthd) - HDCP_METHOD_ID(INIT)) >> 2))

/*!
 * @brief Specifies if the specified LW95A1 HDCP method is set in
 *        appMthdValidMask.
 *
 * @param[in]  mthd     LW95A1 HDCP method valie.
 *
 * @return Non-zero value if the method is valid; 0 if the method is not valid.
 */
#define HDCP_IS_METHOD_VALID(mthd)                                             \
    (appMthdValidMask.mthdGrp[HDCP_GET_METHOD_INDEX(mthd) / 32] &              \
    BIT(HDCP_GET_METHOD_INDEX(mthd) % 32))

/*!
 * @brief Obtains a pointer to the method's parameter which is stored within the
 *        frame buffer.
 *
 * The client sends down the offset with a left-shift of 8 bits.
 *
 * @param[in]   idx     LW95A1 HDCP method value.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define HDCP_GET_METHOD_PARAM(mthd)                                            \
    HDCP_GET_METHOD_PARAM_OFFSET(HDCP_GET_METHOD_INDEX(mthd))

/* ------------------------- Prototypes ------------------------------------- */
void hdcpMethodHandleReadCaps(hdcp_read_caps_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdReadCaps", "hdcpMethodHandleReadCaps");
void hdcpMethodHandleInit(hdcp_init_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdInit", "hdcpMethodHandleInit");
void hdcpMethodHandleCreateSession(hdcp_create_session_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdCreateSession", "hdcpMethodHandleCreateSession");
void hdcpMethodHandleVerifyCertRx(hdcp_verify_cert_rx_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyCertRx", "hdcpMethodHandleVerifyCertRx");
void hdcpMethodHandleGenerateEkm(hdcp_generate_ekm_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdGenerateEkm", "hdcpMethodHandleGenerateEkm");
void hdcpMethodHandleVerifyHprime(hdcp_verify_hprime_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyHprime", "hdcpMethodHandleVerifyHprime");
void hdcpMethodHandleGenerateLcInit(hdcp_generate_lc_init_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdGenerateLcInit", "hdcpMethodHandleGenerateLcInit");
void hdcpMethodHandleUpdateSession(hdcp_update_session_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdUpdateSession", "hdcpMethodHandleUpdateSession");
void hdcpMethodHandleVerifyLprime(hdcp_verify_lprime_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyLprime", "hdcpMethodHandleVerifyLprime");
void hdcpMethodHandleGenerateSkeInit(hdcp_generate_ske_init_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdGenerateSkeInit", "hdcpMethodHandleGenerateSkeInit");
void hdcpMethodHandleEncryptPairingInfo(hdcp_encrypt_pairing_info_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdEncryptPairingInfo", "hdcpMethodHandleEncryptPairingInfo");
void hdcpMethodHandleDecryptPairingInfo(hdcp_decrypt_pairing_info_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdDecryptPairingInfo", "hdcpMethodHandleDecryptPairingInfo");
void hdcpMethodHandleEncrypt(hdcp_encrypt_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdEncrypt", "hdcpMethodHandleEncrypt");
void hdcpMethodHandleValidateSrm(hdcp_validate_srm_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdValidateSrm", "hdcpMethodHandleValidateSrm");
void hdcpMethodHandleRevocationCheck(hdcp_revocation_check_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdRevocationCheck", "hdcpMethodHandleRevocationCheck");
void hdcpMethodHandleSessionCtrl(hdcp_session_ctrl_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdSessionCtrl", "hdcpMethodHandleSessionCtrl");
void hdcpMethodHandleVerifyVprime(hdcp_verify_vprime_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyVprime", "hdcpMethodHandleVerifyVprime");
void hdcpMethodHandleGetRttChallenge(hdcp_get_rtt_challenge_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdGetRttChallenge", "hdcpMethodHandleGetRttChallenge");
void hdcpMethodHandleStreamManage(hdcp_stream_manage_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdStreamManage", "hdcpMethodHandleStreamManage");
void hdcpMethodHandleComputeSprime(hdcp_compute_sprime_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdComputeSprime", "hdcpMethodHandleComputeSprime");
void hdcpMethodHandleExchangeInfo(hdcp_exchange_info_param *pParam)
    GCC_ATTRIB_SECTION("imem_hdcpMthdExchangeInfo", "hdcpMethodHandleExchangeInfo");

#endif // HDCPMC_MTHDS_H
#endif
