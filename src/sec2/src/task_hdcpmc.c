/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_hdcpmc.c
 * @brief  Task that performs validation services related to Miracast HDCP
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objhdcpmc.h"
#include "hdcpmc/hdcpmc_data.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_mthds.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * The size of the buffer used to store a method's parameter data. This value
 * must a multiple of @ref HDCP_DMA_ALIGNMENT.
 */
#define HDCP_METHOD_PARAMETER_BUFFER_SIZE                                   256

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _hdcpPreMethodHandler(void)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpPreMethodHandler");
static void _hdcpProcessMethod(void)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpProcessMethod");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be HDCP commands.
 */
LwrtosQueueHandle HdcpmcQueue;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Buffer to store the method's parameter data.
 *
 * Before processing the method, the parameter data is read from the frame
 * buffer to this buffer. Upon successful completion of processing the method,
 * the parameter data is written back to the frame buffer.
 */
static LwU8 hdcpMthdParamBuffer[HDCP_METHOD_PARAMETER_BUFFER_SIZE]
   GCC_ATTRIB_ALIGN(HDCP_DMA_ALIGNMENT)
   GCC_ATTRIB_SECTION("dmem_hdcpmc", "hdcpMthdParamBuffer");

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Initializes the HDCP task to process methods sent to it via the
 * channel.
 *
 * The HDCP global data is first read from the FB and the necessary buffers are
 * zeroed out.
 *
 * @return FLCN_OK if the initialization completed successfully; otherwise,
 *         a detailed error code is returned.
 */
static FLCN_STATUS
_hdcpPreMethodHandler(void)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       sbOffset = 0;

    if (HDCP_IS_METHOD_VALID(HDCP_METHOD_ID(SET_SCRATCH_BUFFER)))
    {
        LwU32 mthdIndex = HDCP_GET_METHOD_INDEX(HDCP_METHOD_ID(SET_SCRATCH_BUFFER));

        // Clear corresponding method valid mask.
        appMthdValidMask.mthdGrp[mthdIndex / 32] &= ~BIT(mthdIndex % 32);

        //
        // READ_CAPS shouldn't together with SET_SCRATCH_BUFFER, but video driver
        // mistake to send that together with READ_CAPS method.
        // TODO: request video driver to fix that.
        //
        if (!HDCP_IS_METHOD_VALID(HDCP_METHOD_ID(READ_CAPS)))
        {
            // Read the scratch buffer offset and copy the global hdcp data from FB.
            sbOffset = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_SCRATCH_BUFFER));

            status = hdcpDmaRead(&hdcpGlobalData, sbOffset + (HDCP_SB_HEADER_OFFSET >> 8),
                                 ALIGN_UP(sizeof(HDCP_GLOBAL_DATA),
                                        HDCP_DMA_ALIGNMENT));
            if (FLCN_OK != status)
            {
                goto _hdcpPreMethodHandler_end;
            }

            hdcpGlobalData.sbOffset = sbOffset;
        }
    }
    else
    {
        hdcpGlobalData.sbOffset = 0;
    }

    // Reset buffers
    memset((LwU8*)hdcpSessionBuffer, 0, HDCP_SB_SESSION_SIZE);

_hdcpPreMethodHandler_end:
    return status;
}

/*!
 * @brief Processes the HDCP methods received via the channel.
 *
 * Iterates over all the methods received from the channel. For each method,
 * the function reads in the parameter for the method, calls the actual handler
 * to process the method, then writes the parameter back to the channel.
 */
static void
_hdcpProcessMethod(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       mthd;
    LwU32       paramOffset = 0;
    LwU32       i;

    status = _hdcpPreMethodHandler();
    if (FLCN_OK != status)
    {
        goto _hdcpProcessMethod_end;
    }

    FOR_EACH_VALID_METHOD(i, appMthdValidMask)
    {
        mthd = HDCP_GET_METHOD_ID(i);

        //
        // Get the location of the method's parameter in the FB. Perform a
        // sanity check.
        //
        paramOffset = HDCP_GET_METHOD_PARAM_OFFSET(i);
        if (0 == paramOffset)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }

        memset(hdcpMthdParamBuffer, 0, HDCP_METHOD_PARAMETER_BUFFER_SIZE);
        if (HDCP_METHOD_ID(READ_CAPS) != mthd)
        {
            status = hdcpDmaRead(hdcpMthdParamBuffer, paramOffset,
                                 HDCP_METHOD_PARAMETER_BUFFER_SIZE);
            if (FLCN_OK != status)
            {
                break;
            }
        }

        switch (mthd)
        {
            case HDCP_METHOD_ID(READ_CAPS):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdReadCaps));
                hdcpMethodHandleReadCaps((hdcp_read_caps_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdReadCaps));
                break;
            }
            case HDCP_METHOD_ID(INIT):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdInit));
                hdcpMethodHandleInit((hdcp_init_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdInit));
                break;
            }
            case HDCP_METHOD_ID(CREATE_SESSION):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdCreateSession));
                hdcpMethodHandleCreateSession((hdcp_create_session_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdCreateSession));
                break;
            }
            case HDCP_METHOD_ID(VERIFY_CERT_RX):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyCertRx));
                hdcpMethodHandleVerifyCertRx((hdcp_verify_cert_rx_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyCertRx));
                break;
            }
            case HDCP_METHOD_ID(GENERATE_EKM):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGenerateEkm));
                hdcpMethodHandleGenerateEkm((hdcp_generate_ekm_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGenerateEkm));
                break;
            }
            case HDCP_METHOD_ID(VERIFY_HPRIME):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyHprime));
                hdcpMethodHandleVerifyHprime((hdcp_verify_hprime_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyHprime));
                break;
            }
            case HDCP_METHOD_ID(UPDATE_SESSION):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdUpdateSession));
                hdcpMethodHandleUpdateSession((hdcp_update_session_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdUpdateSession));
                break;
            }
            case HDCP_METHOD_ID(GENERATE_LC_INIT):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGenerateLcInit));
                hdcpMethodHandleGenerateLcInit((hdcp_generate_lc_init_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGenerateLcInit));
                break;
            }
            case HDCP_METHOD_ID(VERIFY_LPRIME):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyLprime));
                hdcpMethodHandleVerifyLprime((hdcp_verify_lprime_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyLprime));
                break;
            }
            case HDCP_METHOD_ID(GENERATE_SKE_INIT):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGenerateSkeInit));
                hdcpMethodHandleGenerateSkeInit((hdcp_generate_ske_init_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGenerateSkeInit));
                break;
            }
            case HDCP_METHOD_ID(ENCRYPT_PAIRING_INFO):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdEncryptPairingInfo));
                hdcpMethodHandleEncryptPairingInfo((hdcp_encrypt_pairing_info_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdEncryptPairingInfo));
                break;
            }
            case HDCP_METHOD_ID(DECRYPT_PAIRING_INFO):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdDecryptPairingInfo));
                hdcpMethodHandleDecryptPairingInfo((hdcp_decrypt_pairing_info_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdDecryptPairingInfo));
                break;
            }
            case HDCP_METHOD_ID(ENCRYPT):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdEncrypt));
                hdcpMethodHandleEncrypt((hdcp_encrypt_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdEncrypt));
                break;
            }
            case HDCP_METHOD_ID(VALIDATE_SRM):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdValidateSrm));
                hdcpMethodHandleValidateSrm((hdcp_validate_srm_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdValidateSrm));
                break;
            }
            case HDCP_METHOD_ID(REVOCATION_CHECK):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdRevocationCheck));
                hdcpMethodHandleRevocationCheck((hdcp_revocation_check_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdRevocationCheck));
                break;
            }
            case HDCP_METHOD_ID(SESSION_CTRL):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdSessionCtrl));
                hdcpMethodHandleSessionCtrl((hdcp_session_ctrl_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdSessionCtrl));
                break;
            }
            case HDCP_METHOD_ID(VERIFY_VPRIME):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyVprime));
                hdcpMethodHandleVerifyVprime((hdcp_verify_vprime_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdVerifyVprime));
                break;
            }
            case HDCP_METHOD_ID(GET_RTT_CHALLENGE):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGetRttChallenge));
                hdcpMethodHandleGetRttChallenge((hdcp_get_rtt_challenge_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdGetRttChallenge));
                break;
            }
            case HDCP_METHOD_ID(STREAM_MANAGE):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdStreamManage));
                hdcpMethodHandleStreamManage((hdcp_stream_manage_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdStreamManage));
                break;
            }
            case HDCP_METHOD_ID(COMPUTE_SPRIME):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdComputeSprime));
                hdcpMethodHandleComputeSprime((hdcp_compute_sprime_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdComputeSprime));
                break;
            }
            case HDCP_METHOD_ID(EXCHANGE_INFO):
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdExchangeInfo));
                hdcpMethodHandleExchangeInfo((hdcp_exchange_info_param*)hdcpMthdParamBuffer);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpMthdExchangeInfo));
                break;
            }
            default:
            {
                // Unknown method encountered. Continue processing.
                break;
            }
        }

        status = hdcpDmaWrite((void*)hdcpMthdParamBuffer, paramOffset,
                                HDCP_METHOD_PARAMETER_BUFFER_SIZE);
        if (FLCN_OK != status)
        {
            break;
        }
    }

_hdcpProcessMethod_end:
    // Copy back the HDCP global data to the FB.
    if ((FLCN_OK == status) && (0 != hdcpGlobalData.sbOffset))
    {
        LwU32 sbOffset = hdcpGlobalData.sbOffset;
        hdcpGlobalData.sbOffset = 0;

        status = hdcpDmaWrite(&hdcpGlobalData, sbOffset + (HDCP_SB_HEADER_OFFSET >> 8),
                              ALIGN_UP(sizeof(HDCP_GLOBAL_DATA),
                                       HDCP_DMA_ALIGNMENT));
    }

    // Signal to the channel manager that we are done with the host.
    NOTIFY_EXELWTE_COMPLETE();
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_hdcpmc, pvParameters)
{
    DISPATCH_HDCPMC dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(HdcpmcQueue, &dispatch))
        {
            //
            // Process the method(s) sent via the host/channel. This is
            // lwrrently the only way the HDCP task receives commands.
            //
            _hdcpProcessMethod();
        }
    }
}
#endif
