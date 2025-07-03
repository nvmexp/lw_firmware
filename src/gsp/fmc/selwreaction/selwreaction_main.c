/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ System includes --------------------------------- */
#include <stdint.h>
#include <lwtypes.h>

/* ------------------------ OpenRTOS includes ------------------------------- */
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>

/* ------------------------ Application includes ---------------------------- */
#include "flcnretval.h"
#include "partswitch.h"
#include "hdcp22wired_selwreaction.h"

/* ------------------------ Types definitions ------------------------------- */
#if PARTITION_SWITCH_TEST
static struct switch_time_measurement measurements
    __attribute__((section(".partition_shared_data.measurements")));
#endif
static LwU8 _gHdcp22wiredHsArg[HDCP22_SELWRE_ACTION_ARG_BUF_SIZE]
    __attribute__((section(".partition_shared_data._gHdcp22wiredHsArg")));

/* ------------------------ External definitions ---------------------------- */
extern uint64_t __partition_shared_data_start[];

/* ------------------------ Static declarations ----------------------------- */
static void _selwrityInitCorePrivilege(void);
#if PARTITION_SWITCH_TEST
static void _partSwitchMeasureTime(struct switch_time_measurement* pMeasurememt);
#endif
static FLCN_STATUS _hdcp22SelwreActionHandler(SELWRE_ACTION_ARG  *pArg);

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief initialize partition privlege level setting.
 *
 */
static void
_selwrityInitCorePrivilege(void)
{
    uint64_t data64;

    // Raise to Level3 that allow access HDCP privleged regs.
    data64 = csr_read(LW_RISCV_CSR_SSPM);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SSPM, _UPLM, _LEVEL3, data64);
    csr_write(LW_RISCV_CSR_SSPM, data64);

    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL3, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);

    data64 = csr_read(LW_RISCV_CSR_RSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _RSP, _URPL, _LEVEL3, data64);
    csr_write(LW_RISCV_CSR_RSP, data64);
}

#if PARTITION_SWITCH_TEST
/*!
 * @brief partition switch handler of measuring time.
 *
 */
static void
_partSwitchMeasureTime(
    struct switch_time_measurement* pMeasurememt
)
{
    pMeasurememt->time    = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    pMeasurememt->instret = csr_read(LW_RISCV_CSR_HPMCOUNTER_INSTRET);
    pMeasurememt->cycle   = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
}
#endif

/*!
 * @brief Entry function of new LS/HS SelwreActions.
 *
 * @param[in]   pArg        Pointer to secure action argument.
 *
 *  This function provides new secure action handler according to action type:
 *  - SELWRE_ACTION_REG_ACCESS: privleged reg access handler.
 *  - SELWRE_ACTION_START_SESSION: start hdcp22 authentication session and initialize internal secrets.
 *  - SELWRE_ACTION_KMKD_GEN: hdcp22 authentication AKE stage to generate km, dkey.
 *  - SELWRE_ACTION_HPRIME_VALIDATION: hdcp22 authentication AKE stage to validate sink replied hprime.
 *  - SELWRE_ACTION_LPRIME_VALIDATION: hdcp22 authentication LC stage to validate sink replied lprime.
 *  - SELWRE_ACTION_EKS_GEN: hdcp22 authentication SKE stage to generate encrypted ks.
 *  - SELWRE_ACTION_CONTROL_ENCRYPTION: hdcp22 HW control to enable/disable encryption.
 *  - SELWRE_ACTION_VPRIME_VALIDATION: hdcp22 authentication REPEATER stage to validate sink replied vprime.
 *  - SELWRE_ACTION_MPRIME_VALIDATION: hdcp22 authentication REPEATER stage to validate sink replied mprime.
 *  - SELWRE_ACTION_WRITE_DP_ECF: hdcp22 write DP ECF.
 *  - SELWRE_ACTION_END_SESSION: end hdcp22 authentication session and reset internal secrets.
 *
 * @return      FLCN_STATUS FLCN_OK when succeed else error.
 */
static FLCN_STATUS
_hdcp22SelwreActionHandler
(
    SELWRE_ACTION_ARG  *pArg
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (pArg->actionType)
    {
        //
        // TODO: remove SELWRE_ACTION_REG_ACCESS handler after revsit hdcp22
        // Initialize, selwreEcf regWrite, and and hdcp1x testSelwreStatus.
        //
        case SELWRE_ACTION_REG_ACCESS:
            status = hdcp22wiredSelwreRegAccessHandler(pArg);
            break;
        case SELWRE_ACTION_START_SESSION:
            status = hdcp22wiredStartSessionHandler(pArg);
            break;
        case SELWRE_ACTION_VERIFY_CERTIFICATE:
            status = hdcp22wiredVerifyRxCertificateHandler(pArg);
            break;
        case SELWRE_ACTION_KMKD_GEN:
            status = hdcp22wiredKmKdGenHandler(pArg);
            break;
        case SELWRE_ACTION_HPRIME_VALIDATION:
            status = hdcp22wiredHprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_LPRIME_VALIDATION:
            status = hdcp22wiredLprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_EKS_GEN:
            status = hdcp22wiredEksGenHandler(pArg);
            break;
        case SELWRE_ACTION_CONTROL_ENCRYPTION:
            status = hdcp22wiredControlEncryptionHandler(pArg);
            break;
        case SELWRE_ACTION_VPRIME_VALIDATION:
            status = hdcp22wiredVprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_MPRIME_VALIDATION:
            status = hdcp22wiredMprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_WRITE_DP_ECF:
            status = hdcp22wiredWriteDpEcfHandler(pArg);
            break;
        case SELWRE_ACTION_END_SESSION:
            status = hdcp22wiredEndSessionHandler(pArg);
            break;
        case SELWRE_ACTION_SRM_REVOCATION:
            status = hdcp22wiredSrmRevocationHandler(pArg);
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief partition entry main.
 *
 * arg0-arg4 are parameters passed by partition 0
 */
void
selwreaction_main
(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4
)
{
    FLCN_STATUS         status = FLCN_OK;
    SBI_RETURN_VALUE    ret;
    uint64_t            retOffset = 0;

    _selwrityInitCorePrivilege();

    switch (arg0)
    {
#if PARTITION_SWITCH_TEST
        case PARTSWITCH_FUNC_MEASURE_SWITCHING_TIME:
            {
                _partSwitchMeasureTime(&measurements);

                // Return measured time back to partition0 as shared data.
                retOffset = ((uint64_t)&measurements) - (uint64_t)&__partition_shared_data_start;
            }
            break;
#endif
        case PARTSWITCH_FUNC_HDCP_SELWREACTION:
            {
                SELWRE_ACTION_ARG  *pArg = (SELWRE_ACTION_ARG *)_gHdcp22wiredHsArg;

                // Dispatch to selwreAction handler.
                status = _hdcp22SelwreActionHandler(pArg);

                retOffset = ((uint64_t)&_gHdcp22wiredHsArg) - (uint64_t)&__partition_shared_data_start;
                pArg->actionStatus.state.actionState = SELWRE_ACTION_STATE_COMPLETED;
                pArg->actionStatus.state.actionResult = status;
            }
            break;
        default:
            printf("Secure partition got unexpected func 0x%lx. request.\n", arg0);
            break;
    }

    //
    // The following 'switch_to' SBI call allows up to 5 opaque parameters to be passed using a0-a4 argument registers
    // a5 argument register is used for partition ID we're switching to
    //
    ret = sbicall6(SBI_EXTENSION_LWIDIA,
                   SBI_LWFUNC_FIRST,
                   retOffset,
                   0,
                   0,
                   0,
                   0,
                   PARTITION_RTOS_ID);

    printf("Secure partition critical error: returned from SBI call. Error: 0x%lx, value: 0x%lx. Shutting down\n",
           ret.error, ret.value);
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
}

/*!
 * @brief trap handler.
 *
 */
void
selwreaction_trap_handler(void)
{
    printf("In Secure partition trap handler at %lu, cause 0x%lx val 0x%lx\n",
           csr_read(LW_RISCV_CSR_SEPC),
           csr_read(LW_RISCV_CSR_SCAUSE),
           csr_read(LW_RISCV_CSR_STVAL));

    printf("Shutting down\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
    return;
}

