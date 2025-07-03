/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <unistd.h>
#include <dev_riscv_pri.h>
#include <dev_riscv_addendum.h>
#include <dev_falcon_v4.h>
#include <dev_fbif_v4.h>
#include <dev_fbif_addendum.h>
#include <lwmisc.h>

#include "misc.h"
#include "brom.h"

#define MK_D(X, V) {V, #X}

typedef struct {
    LwU32 val;
    const char *desc;
} reg_decodes;

#define MK_RES(X) {LW_PRISCV_RISCV_BR_RETCODE_RESULT_##X, #X}
static reg_decodes br_result[] = {
    MK_RES(INIT),
    MK_RES(RUNNING),
    MK_RES(FAIL),
    MK_RES(PASS),
    {0, NULL}
};

#define MK_PH(X) {LW_PRISCV_RISCV_BR_RETCODE_PHASE_##X, #X}
static reg_decodes br_phase[] = {
    MK_PH(ENTRY),
    MK_PH(INIT_DEVICE),
    MK_PH(LOAD_PUBLIC_KEY),
    MK_PH(LOAD_PKC_BOOT_PARAM),
    MK_PH(SHA_MANIFEST),
    MK_PH(VERIFY_MANIFEST_SIGNATURE),
    MK_PH(DECRYPT_MANIFEST),
    MK_PH(SANITIZE_MANIFEST),
    MK_PH(LOAD_FMC),
    MK_PH(VERIFY_FMC_DIGEST),
    MK_PH(DECRYPT_FMC),
    MK_PH(DECRYPT_FUSEKEY),
    MK_PH(REVOKE_RESOURCE),
    MK_PH(CONFIGURE_FMC_ELW),
    {0, NULL}
};

#define MK_SY(X) {LW_PRISCV_RISCV_BR_RETCODE_SYNDROME_##X, #X}
static reg_decodes br_syndromes[] = {
    MK_SY(INIT),
    MK_SY(DMA_FB_ADDRESS_ERROR),
    MK_SY(DMA_NACK_ERROR),
    MK_SY(SHA_ACQUIRE_MUTEX_ERROR),
    MK_SY(SHA_EXELWTION_ERROR),
    MK_SY(DIO_READ_ERROR),
    MK_SY(DIO_WRITE_ERROR),
    MK_SY(SE_PDI_ILWALID_ERROR),
    MK_SY(SE_PKEYHASH_ILWALID_ERROR),
    MK_SY(SW_PKEY_DIGEST_ERROR),
    MK_SY(SE_PKA_RETURN_CODE_ERROR),
    MK_SY(SCP_LOAD_SECRET_ERROR),
    MK_SY(SCP_TRAPPED_DMA_NOT_ALIGNED_ERROR),
    MK_SY(MANIFEST_CODE_SIZE_ERROR),
    MK_SY(MANIFEST_CORE_PMP_RESERVATION_ERROR),
    MK_SY(MANIFEST_DATA_SIZE_ERROR),
    MK_SY(MANIFEST_DEVICEMAP_BR_UNLOCK_ERROR),
    MK_SY(MANIFEST_FAMILY_ID_ERROR),
    MK_SY(MANIFEST_MSPM_VALUE_ERROR),
    MK_SY(MANIFEST_PAD_INFO_MASK_ERROR),
    MK_SY(MANIFEST_REG_PAIR_ADDRESS_ERROR),
    MK_SY(MANIFEST_REG_PAIR_ENTRY_NUM_ERROR),
    MK_SY(MANIFEST_SECRET_MASK_ERROR),
    MK_SY(MANIFEST_SECRET_MASK_LOCK_ERROR),
    MK_SY(MANIFEST_SIGNATURE_ERROR),
    MK_SY(MANIFEST_UCODE_ID_ERROR),
    MK_SY(MANIFEST_UCODE_VERSION_ERROR),
    MK_SY(FMC_DIGEST_ERROR),
    MK_SY(FUSEKEY_BAD_HEADER_ERROR),
    MK_SY(FUSEKEY_KEYGLOB_ILWALID_ERROR),
    MK_SY(FUSEKEY_PROTECT_INFO_ERROR),
    MK_SY(FUSEKEY_SIGNATURE_ERROR),
    MK_SY(KMEM_DISPOSE_KSLOT_ERROR),
    MK_SY(KMEM_KEY_SLOT_K3_ERROR),
    MK_SY(KMEM_LOAD_KSLOT2SCP_ERROR),
    MK_SY(KMEM_READ_ERROR),
    MK_SY(KMEM_WRITE_ERROR),
    MK_SY(IOPMP_ERROR),
    MK_SY(MMIO_ERROR),
    MK_SY(OK),
    {0, NULL}
};

#define MK_IN(X) {LW_PRISCV_RISCV_BR_RETCODE_INFO_##X, #X}

static reg_decodes br_infos[] = {
    MK_IN(INIT),
    MK_IN(DMA_WAIT_FOR_IDLE_HANG),
    MK_IN(SHA_HANG),
    MK_IN(DIO_READ_HANG),
    MK_IN(DIO_WAIT_FREE_ENTRY_HANG),
    MK_IN(DIO_WRITE_HANG),
    MK_IN(SE_ACQUIRE_MUTEX_HANG),
    MK_IN(SE_PDI_LOAD_HANG),
    MK_IN(SE_PKEYHASH_LOAD_HANG),
    MK_IN(PKA_POLL_RESULT_HANG),
    MK_IN(SCP_PIPELINE_RESET_HANG),
    MK_IN(TRAPPED_DMA_HANG),
    MK_IN(FUSEKEY_KEYGLOB_LOAD_HANG),
    MK_IN(KMEM_CMD_EXELWTE_HANG),
    {0, NULL}
};


static const char * lookupReg(LwU32 reg, reg_decodes *arr)
{
    while (arr->desc != NULL)
    {
        if (arr->val == reg)
            return arr->desc;
        arr++;
    }
    return "(UNKNOWN)";
}

static LwU64 rd64(Engine *pEngine, LwU32 addr)
{
    return (LwU64)riscvRead(pEngine, addr) | (((LwU64)riscvRead(pEngine, addr + 4)) << 32);
}

void bromDumpStatus(Engine *pEngine)
{
    LwU32 reg;

    reg = riscvRead(pEngine, LW_PRISCV_RISCV_BCR_CTRL);
    printf("Bootrom Configuration: 0x%08x ", reg);
    printf("(%s) ", DRF_VAL(_PRISCV_RISCV, _BCR_CTRL, _VALID, reg) ? "VALID" : "INVALID");
    printf("core: %s ", FLD_TEST_DRF(_PRISCV_RISCV, _BCR_CTRL, _CORE_SELECT, _RISCV, reg) ? "RISC-V" : "FALCON");
    printf("brfetch: %s ", FLD_TEST_DRF(_PRISCV_RISCV, _BCR_CTRL, _BRFETCH, _TRUE, reg) ? "ENABLED" : "DISABLED");
    printf("\n");
    const char *DMACFG_TARGETS[] = {"localFB", "cohSysmem", "nonCohSysmem", "invalid"};
    reg = riscvRead(pEngine, LW_PRISCV_RISCV_BCR_DMACFG_SEC);
    printf("Bootrom DMA SEC configuration: 0x%08x ", reg);
    printf("wprid: 0x%x ", DRF_VAL(_PRISCV_RISCV, _BCR_DMACFG_SEC, _WPRID, reg));
    reg = riscvRead(pEngine, LW_PRISCV_RISCV_BCR_DMACFG);
    printf("(%s) ", DMACFG_TARGETS[DRF_VAL(_PRISCV_RISCV, _BCR_DMACFG, _TARGET, reg)]);
    printf("%s\n", FLD_TEST_DRF(_PRISCV_RISCV, _BCR_DMACFG, _LOCK, _LOCKED, reg) ? "LOCKED" : "UNLOCKED");
    printf("Big hammer lockdown is %s\n", riscvRead(pEngine, LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN) & 0x1 ? "ENABLED" : "DISABLED");
    reg = riscvRead(pEngine, LW_PRISCV_RISCV_BR_RETCODE);
    printf("RETCODE: 0x%08x Result: %s Phase: %s Syndrome: %s Info: %s\n",
            reg,
            lookupReg(DRF_VAL(_PRISCV_RISCV, _BR_RETCODE, _RESULT, reg), br_result),
            lookupReg(DRF_VAL(_PRISCV_RISCV, _BR_RETCODE, _PHASE, reg), br_phase),
            lookupReg(DRF_VAL(_PRISCV_RISCV, _BR_RETCODE, _SYNDROME, reg), br_syndromes),
            lookupReg(DRF_VAL(_PRISCV_RISCV, _BR_RETCODE, _INFO, reg), br_infos)
          );
    printf("FMC Code addr: 0x%llx\n", rd64(pEngine, LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO));
    printf("FMC Data addr: 0x%llx\n", rd64(pEngine, LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO));
    printf("PKC addr     : 0x%llx\n", rd64(pEngine, LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO));
    printf("PUBKEY addr  : 0x%llx\n", rd64(pEngine, LW_PRISCV_RISCV_BCR_DMAADDR_PUBKEY_LO));
}

void bromConfigurePmbBoot(Engine *pEngine)
{
        LwU32 bcr = 0;

        bcr = DRF_DEF(_PRISCV_RISCV, _BCR_CTRL, _CORE_SELECT, _RISCV)   |
          DRF_DEF(_PRISCV_RISCV, _BCR_CTRL, _VALID,       _TRUE)    |
          DRF_DEF(_PRISCV_RISCV, _BCR_CTRL, _BRFETCH,     _FALSE);
        riscvWrite(pEngine, LW_PRISCV_RISCV_BCR_CTRL, bcr);
}

void bromBoot(Engine *pEngine)
{
    // MK TODO: no longer possible with lockdown mode
    riscvWrite(pEngine, LW_PRISCV_RISCV_CPUCTL, DRF_DEF(_PRISCV_RISCV, _CPUCTL, _STARTCPU, _TRUE));
}

LwBool bromWaitForCompletion(Engine *pEngine, LwU32 timeoutMs)
{
    LwU32 reg = 0;

    do {
        reg = riscvRead(pEngine, LW_PRISCV_RISCV_BR_RETCODE);
        if (FLD_TEST_DRF(_PRISCV, _RISCV_BR_RETCODE, _RESULT, _PASS, reg) ||
            FLD_TEST_DRF(_PRISCV, _RISCV_BR_RETCODE, _RESULT, _FAIL, reg))
            break;

        if (timeoutMs > 0)
        {
            if (timeoutMs >= 10)
            {
                usleep(10 * 1000);
                timeoutMs -= 10;
            } else
            {
                timeoutMs = 0;
                usleep(timeoutMs * 1000);
            }
        } else
        {
            return LW_FALSE;
        }
    } while (1);

    return LW_TRUE;
}
