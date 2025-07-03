/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>
#include <lwstatus.h>
#include <libc/string.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/dma.h>
#include <liblwriscv/fbif.h>
#include <liblwriscv/ssp.h>
#include <libc/string.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/fence.h>
#include <lwriscv/gcc_attrs.h>
#include <dev_gsp.h>
#include <flcnifcmn.h>
#include <riscvifriscv.h>
#include "dev_fb.h"
#include "dev_se_seb.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "misc.h"
#include "partitions.h"
#include "gsp/gsp_proxy_reg.h"
#include "dev_vm.h"
#include "dev_top.h"
#include "gsp/gspifacr.h"
#include "gsp/gspifgsp.h"
#include "flcnifcmn.h"

#include "rpc.h"

#if CC_PERMIT_DEBUG
extern LwU8 __dmem_print_start[];
extern LwU8 __dmem_print_size[];
#endif

static void setupHostInterrupts(void)
{
    LwU32 irqDest = 0U;
    LwU32 irqMset = 0U;
    LwU32 irqMode = localRead(LW_PRGNLCL_FALCON_IRQMODE);

    // HALT: CPU transitioned from running into halt
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _HALT, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _HALT, _SET, irqMset);

    // SWGEN0: for communication with RM via Message Queue
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN0, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN0, _SET, irqMset);

    // SWGEN1: for the PMU's print buffer
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN1, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN1, _SET, irqMset);

    // MEMERR: for external mem access error interrupt (level-triggered)
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _MEMERR, _RISCV, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _MEMERR, _SET, irqMset);
    irqMode = FLD_SET_DRF(_PRGNLCL, _FALCON_IRQMODE, _LVL_MEMERR, _TRUE, irqMode);

    localWrite(LW_PRGNLCL_FALCON_IRQMODE, irqMode);
    localWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
    localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
    localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
}

#ifndef CC_FMODEL_BUILD
/*!
 * @brief  Initialize the GSP Engine reset PLM's.
 * More details are here: https://confluence.lwpu.com/display/LW/GH100+-+Protect+against+reset+attacks
 */
static void setupEngineResetPlms(void)
{
    LwU32 sourceMask = 0;
    LwU32 regval     = 0;
    sourceMask       = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP);

    // Initialize FALCON_RESET PLM to L3 protected and source enable it to GSP.
    regval = localRead(LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK);   
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, regval);
    regval = FLD_SET_DRF_NUM( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, regval);
    localWrite(LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK, regval);

    //
    // Initialize RISCV_CPUCTL PLM to L3 write protected protected, LO read protected and source enable it to
    // As CPUCTL is used in RM Code we are setting the _SOURCE_READ_CONTROL to Lowered as suggested on BUG https://lwbugswb.lwpu.com/LWBugs5/redir.aspx?url=/3481455/17
    //
    regval = localRead(LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ONLY_LEVEL3_ENABLED, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _READ_PROTECTION,      _ALL_LEVELS_ENABLED,  regval);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED,             regval);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED,             regval);
    regval = FLD_SET_DRF_NUM( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, regval);
    localWrite(LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK, regval);

    // Initialize FALCON_EXE PLM to L3 protected and source enable it to GSP.
    regval = localRead(LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK);   
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, regval);
    regval = FLD_SET_DRF_NUM( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, regval);
    localWrite(LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK, regval);
}
#endif // CC_FMODEL_BUILD

static void initEngine(void)
{
#ifndef CC_FMODEL_BUILD
    // Initialize GSP Engine PLM values to protect against reset attacks.
    setupEngineResetPlms();
#endif // CC_FMODEL_BUILD

    // Setup interrupts routed to host
    setupHostInterrupts();

#if CC_PERMIT_DEBUG
    // configure print buffer as pointed by linker script.
    printInitEx((LwU8*)__dmem_print_start,
                (LwU16)((LwUPtr)__dmem_print_size & 0xFFFF),
                LW_PGSP_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE),
                LW_PGSP_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE), 1);
#endif // CC_PERMIT_DEBUG

    // Unlock DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB,
               FLD_SET_DRF(_PRGNLCL, _FALCON_LOCKPMB, _DMEM, _UNLOCK,
                           localRead(LW_PRGNLCL_FALCON_LOCKPMB)));
}

#if defined(HW_CC_ENABLED)
#define LW_CE_NUM_AES_SRAM_SLOTS 512

#define LW_CE_AES_SRAM_SLOT_LO_ENCRYPT_MESSAGE_CTR                   31:0 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_LO_ENCRYPT_MESSAGE_CTR_FIELD             31:0 /*       */

#define LW_CE_AES_SRAM_SLOT_LO_ENCRYPT_CHANNEL_CTR_LO               63:32 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_LO_ENCRYPT_CHANNEL_CTR_LO_FIELD   63-32:32-32 /*       */

#define LW_CE_AES_SRAM_SLOT_LO_ENCRYPT_CHANNEL_CTR_HI               95:64 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_LO_ENCRYPT_CHANNEL_CTR_HI_FIELD   95-64:64-64 /*       */

#define LW_CE_AES_SRAM_SLOT_LO_KEY_IDX                              97:96 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_LO_KEY_IDX_FIELD                  97-96:96-96 /*       */
#define LW_CE_AES_SRAM_SLOT_LO_PREEMPTED                            98:98 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_LO_PREEMPTED_FALSE                          0 /* RW--V */
#define LW_CE_AES_SRAM_SLOT_LO_PREEMPTED_TRUE                           1 /* RW--V */
#define LW_CE_AES_SRAM_SLOT_LO_PREEMPTED_FIELD                98-96:98-96 /*       */
#define LW_CE_AES_SRAM_SLOT_LO_AES_BLOCK_CTR                       127:99 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_LO_AES_BLOCK_CTR_MAX_LEGAL_VAL     0x10000001 /* RW--V */
#define LW_CE_AES_SRAM_SLOT_LO_AES_BLOCK_CTR_FIELD           127-96:99-96 /*       */

#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG0                     31:0 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG0_FIELD               31:0 /*       */

#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG1                    63:32 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG1_FIELD        63-32:32-32 /*       */

#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG2                    95:64 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG2_FIELD        95-64:64-64 /*       */

#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG3                   127:96 /* RWXUF */
#define LW_CE_AES_SRAM_SLOT_HI_PARTIAL_AUTH_TAG3_FIELD       127-96:96-96 /*       */

/*!
 * @brief Enable SELWRE_COPY on all Async LCEs
 */
static LWRV_STATUS ceEnableSelwreCopy_GH100(void)
{

    DIO_PORT dioPort;
    LwU32 val = 0;
    LwU32 index = 0;
    LWRV_STATUS status = LWRV_OK;

    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;

    val = FLD_SET_DRF(_SSE_SCE, _LCE_CFG_A, _SELWRE_COPY_ENABLE, _TRUE, val);
    while (index < LW_SSE_SCE_LCE_CFG_A__SIZE_1)
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_CFG_A(index), &val);
        if (status != LWRV_OK)
        {
            pPrintf("DIO RW failed 0x%0x\n", status);
            return status;
        }
        index++;
    }

    //
    // Program Hard-code keys & IVs in all Key slots
    // TODO: Remove this once SPDM has been properly enabled
    // Steps:
    // 1. Program ENCRYPT Keys
    // 2. Program DECRYPT Keys
    // 3. Program ENCRYPT & DECRYPT RNG mask
    // 4. Program KEY VALID
    // 5. Program IVs in SRAM slots
    //

    LwU32 eKey[8] = {0x566B5970, 0x33733676, 0x39792442, 0x26452948, 0x404D6251, 0x65546857, 0x6D5A7134, 0x74377721};
    LwU32 dKey[8] = {0x38782F41, 0x3F442847, 0x2B4B6250, 0x65536756, 0x6B597033, 0x73367639, 0x79244226, 0x45294840};
    LwU32 ivmk[3] = {0,0,0};

    // Program ENCRYPT KEYS
    for (index=0; index < LW_SSE_SCE_LCE_ENCRYPT_KEY_A__SIZE_1; index++)
    {
        LwU32 j = 0;
        for (j = 0; j < LW_SSE_SCE_LCE_ENCRYPT_KEY_A__SIZE_2; j++)
        {
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_KEY_A(index, j), &(eKey[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_KEY_B(index, j), &(eKey[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_KEY_C(index, j), &(eKey[j]));
            if (status != LWRV_OK)
                return status;
        }
    }

    // Program DECRYPT KEYS
    for (index=0; index < LW_SSE_SCE_LCE_DECRYPT_KEY_A__SIZE_1; index++)
    {
        LwU32 j = 0;
        for (j = 0; j < LW_SSE_SCE_LCE_DECRYPT_KEY_A__SIZE_2; j++)
        {
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_DECRYPT_KEY_A(index, j), &(dKey[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_DECRYPT_KEY_B(index, j), &(dKey[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_DECRYPT_KEY_C(index, j), &(dKey[j]));
            if (status != LWRV_OK)
                return status;
        }
    }

    // Program ENCRYPT RNG Masks
    for (index=0; index < LW_SSE_SCE_LCE_ENCRYPT_RNG_A__SIZE_1; index++)
    {
        LwU32 j = 0;
        for (j = 0; j < LW_SSE_SCE_LCE_ENCRYPT_RNG_A__SIZE_2; j++)
        {
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_RNG_A(index, j), &(ivmk[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_RNG_B(index, j), &(ivmk[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_RNG_C(index, j), &(ivmk[j]));
            if (status != LWRV_OK)
                return status;
        }

    }

    // Program DECRYPT RNG Masks
    for (index=0; index < LW_SSE_SCE_LCE_DECRYPT_RNG_A__SIZE_1; index++)
    {
        LwU32 j = 0;
        for (j = 0; j < LW_SSE_SCE_LCE_DECRYPT_RNG_A__SIZE_2; j++)
        {
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_DECRYPT_RNG_A(index, j), &(ivmk[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_DECRYPT_RNG_B(index, j), &(ivmk[j]));
            if (status != LWRV_OK)
                return status;

            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_DECRYPT_RNG_C(index, j), &(ivmk[j]));
            if (status != LWRV_OK)
                return status;
        }
    }

    // Program KEY VALID
    LwU32 data = 0;

    data = FLD_SET_DRF(_SSE_SCE, _LCE_KEY_VALID,_ENCRYPT_A, _TRUE, data);
    data = FLD_SET_DRF(_SSE_SCE, _LCE_KEY_VALID,_ENCRYPT_B, _TRUE, data);
    data = FLD_SET_DRF(_SSE_SCE, _LCE_KEY_VALID,_ENCRYPT_C, _TRUE, data);
    data = FLD_SET_DRF(_SSE_SCE, _LCE_KEY_VALID,_DECRYPT_A, _TRUE, data);
    data = FLD_SET_DRF(_SSE_SCE, _LCE_KEY_VALID,_DECRYPT_B, _TRUE, data);
    data = FLD_SET_DRF(_SSE_SCE, _LCE_KEY_VALID,_DECRYPT_C, _TRUE, data);

    for (index=0; index < LW_SSE_SCE_LCE_KEY_VALID__SIZE_1; index++)
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_KEY_VALID(index), &(data));
        if (status != LWRV_OK)
            return status;
    }

    //
    // Program IVs in SRAM Slots
    // SRAM structure: /hw/lwgpu/ip/system/ce/3.0/defs/public/registers/dev_ce_aes_sram.ref
    //

    for (index=0; index < LW_SSE_SCE_LCE_SRAM_DATA__SIZE_1; index++)
    {
        LwU32 j = 0;
        LwU32 numChannels = 0;
        LwU32 data = 0;
        LwU32 platform = DRF_VAL(_PTOP, _PLATFORM, _TYPE, priRead(LW_PTOP_PLATFORM));

        // GSP RM Proxy init in fmodel takes too long due to 512 slots, so set
        // to 10 on fmodel.
        if (platform == LW_PTOP_PLATFORM_TYPE_FMODEL)
        {
            numChannels = 10;
        }
        else
        {
            numChannels = LW_CE_NUM_AES_SRAM_SLOTS;
        }

        for (j=0; j< numChannels; j++)
        {
            // Set AUTH TAG to 0
            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_HI, _PARTIAL_AUTH_TAG0, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 0), &(data));
            if (status != LWRV_OK)
                return status;

            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_HI, _PARTIAL_AUTH_TAG1, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 1), &(data));
            if (status != LWRV_OK)
                return status;

            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_HI, _PARTIAL_AUTH_TAG2, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 2), &(data));
            if (status != LWRV_OK)
                return status;

            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_HI, _PARTIAL_AUTH_TAG3, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 3), &(data));
            if (status != LWRV_OK)
                return status;

            data = 0;
            data = FLD_SET_DRF(_SSE_SCE, _LCE_SRAM_INDEX, _PART, _UPPER, data);
            data = FLD_SET_DRF_NUM(_SSE_SCE, _LCE_SRAM_INDEX, _VAL, j, data);
            data = FLD_SET_DRF(_SSE_SCE, _LCE_SRAM_INDEX, _OP, _WRITE, data);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_INDEX(index), &(data));
            if (status != LWRV_OK)
                return status;

            // Set message counter to 0
            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_LO, _ENCRYPT_MESSAGE_CTR_FIELD, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 0), &(data));
            if (status != LWRV_OK)
                return status;

            // Set Channel Ctr LO to 0
            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_LO, _ENCRYPT_CHANNEL_CTR_LO_FIELD, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 1), &(data));
            if (status != LWRV_OK)
                return status;

            // ChannelCTR Hi to 0.
            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_LO, _ENCRYPT_CHANNEL_CTR_HI_FIELD, 0, 0);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 2), &(data));
            if (status != LWRV_OK)
                return status;

            // Key IDX set to 0
            data = FLD_SET_DRF_NUM(_CE_AES, _SRAM_SLOT_LO, _KEY_IDX_FIELD, 0, 0);
            data = FLD_SET_DRF(_CE_AES, _SRAM_SLOT_LO, _PREEMPTED, _FALSE, data);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_DATA(index, 3), &(data));
            if (status != LWRV_OK)
                return status;

            data = 0;
            data = FLD_SET_DRF(_SSE_SCE, _LCE_SRAM_INDEX, _PART, _LOWER, data);
            data = FLD_SET_DRF_NUM(_SSE_SCE, _LCE_SRAM_INDEX, _VAL, j, data);
            data = FLD_SET_DRF(_SSE_SCE, _LCE_SRAM_INDEX, _OP, _WRITE, data);
            status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_SRAM_INDEX(index), &(data));
            if (status != LWRV_OK)
                return status;

        }

    }


    return status;
}

/*!
 * @brief Check if CC Mode is enabled. In Fmodel & in simulation elwironments,
 * RM will handle the regkey and set a 'code' in MAILBOX for GSP to read. In
 * production cases, VBIOS will be setting a mode bit in secure scratch.
 */

static
LwBool ccIsEnabled_GH100(LwU32 gspProxyRegkeys)
{
    if (DRF_VAL(GSP, _PROXY_REG, _CONFIDENTIAL_COMPUTE, gspProxyRegkeys)
        == LWGSP_PROXY_REG_CONFIDENTIAL_COMPUTE_ENABLE)
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Initialize CC settings here. Hardcoding most of the settings for now
 */
static GCC_ATTR_UNUSED LWRV_STATUS
ccInit_GH100(LwU32 gspProxyRegKeys)
{
    LWRV_STATUS status  = LWRV_OK;

    if (ccIsEnabled_GH100(gspProxyRegKeys))
    {
        LwU32 trust = 0;
        pPrintf("Initializing CC\n");

        trust = priRead(LW_PFB_PRI_MMU_CC_CONFIG);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _CC_MODE, _ENABLED, trust);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _CPU_BAR1_TRUSTED, _ENABLED, trust);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _SWIZID_CHECK, _ENABLED, trust);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _ATS_FORCE_SWIZID_0, _ENABLED, trust);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _CC_LOCK_FB, _ENABLED, trust);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _CC_LOCK_HWPM, _ENABLED, trust);
        trust = FLD_SET_DRF(_PFB_PRI, _MMU_CC_CONFIG, _CC_LOCK_MISC, _ENABLED, trust);

        priWrite(LW_PFB_PRI_MMU_CC_CONFIG, trust);

        trust=0;
        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _NISO,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _ISO,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _DNISO,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _DWBIF,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _LWDEC,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _LWENC,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _LWJPG,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _OFA,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _GSP,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_TRUSTED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _FSP,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _SEC,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_TRUSTED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _FBFALCON,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _PMU,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_TRUSTED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _PERF,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _ESC,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_A,  _MMU,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_TRUSTED, trust);

        priWrite(LW_PFB_PRI_MMU_CC_TRUST_LEVEL_A, trust);

        trust = 0;
        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _CE,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_CC_AWARE, trust);

        // CE-SHIM cannot pass WPR_ID
        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _CE_SHIM,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _FECS,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _GPCCS,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _FE,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _SKED,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _PD,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _SCC,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _GPM,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _GCC,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _PE,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _ROP,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _RAST,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _TEX,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_QUARANTINED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _XV_XAL_CPU,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_TRUSTED, trust);

        trust = FLD_SET_DRF_NUM(_PFB_PRI, _MMU_CC_TRUST_LEVEL_B,  _P2P,
                          LW_PFB_PRI_MMU_CC_TRUST_LEVEL_UNTRUSTED, trust);

        priWrite(LW_PFB_PRI_MMU_CC_TRUST_LEVEL_B, trust);

#if CC_PERMIT_DEBUG
        {
            LwU32       ccmode  = 0;

            ccmode = priRead(LW_PFB_PRI_MMU_CC_CONFIG);
            pPrintf("CC Config 0x%0x \n", ccmode);
            ccmode = priRead(LW_PFB_PRI_MMU_CC_TRUST_LEVEL_A);
            pPrintf("CC TrustA 0x%0x \n", ccmode);
            ccmode = priRead(LW_PFB_PRI_MMU_CC_TRUST_LEVEL_B);
            pPrintf("CC TrustB 0x%0x \n", ccmode);
            ccmode = priRead(LW_PFB_PRI_MMU_VMMU_SEGMENT_PROTECTION_BITMASK(0));
            pPrintf("CC SPB0  0x%0x \n", ccmode);
            ccmode = priRead(LW_PFB_PRI_MMU_VMMU_SEGMENT_PROTECTION_BITMASK(5));
            pPrintf("CC SPB5  0x%0x \n", ccmode);
            ccmode = priRead(LW_PFB_PRI_MMU_VMMU_SEGMENT_PROTECTION_BITMASK(25));
            pPrintf("CC SPB25 0x%0x \n", ccmode);
        }
#endif // CC_PERMIT_DEBUG

        status = ceEnableSelwreCopy_GH100();
    }

    return status;
}
#endif

static void initRaisePLM(void)
{
    LwU64 srsp;

    srsp = csr_read(LW_RISCV_CSR_SRSP);
    srsp = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL3, srsp);
    srsp = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC, srsp);
    csr_write(LW_RISCV_CSR_SRSP, srsp);
}

static void initLowerPLM(void)
{
    LwU64 srsp;

    srsp = csr_read(LW_RISCV_CSR_SRSP);
    srsp = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL1, srsp);
    srsp = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRSEC, _INSEC, srsp);
    csr_write(LW_RISCV_CSR_SRSP, srsp);
}

extern void partition_init_wipe_and_switch(void);
// C "entry" to partition, has stack and liblwriscv
int partition_init_main(GCC_ATTR_UNUSED LwU64 arg0,
                        GCC_ATTR_UNUSED LwU64 arg1,
                        GCC_ATTR_UNUSED LwU64 arg2,
                        GCC_ATTR_UNUSED LwU64 arg3,
                        GCC_ATTR_UNUSED LwU64 partition_origin)
{
    RM_CC_INIT_PARAMS ccInitArgs;

    initRaisePLM();

    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        ccPanic();
    }

    initEngine();

    initLowerPLM();
    pPrintf("GSP CC Init...\n");

    // Copy CC bootargs
    {
        RM_GSP_BOOT_PARAMS *pBootargsUnsafe = NULL;
        LwU64 bootargsPhys;

        bootargsPhys = ((LwU64)localRead(LW_PRGNLCL_FALCON_MAILBOX1)) << 32 |
                               localRead(LW_PRGNLCL_FALCON_MAILBOX0);
        pBootargsUnsafe = (RM_GSP_BOOT_PARAMS *)bootargsPhys;

        pPrintf("My bootargs at %p size %ld\n",
                pBootargsUnsafe, sizeof(RM_GSP_BOOT_PARAMS));

        // Make sure we copy args from sysmem
        if (bootargsPhys < LW_RISCV_AMAP_SYSGPA_START ||
            bootargsPhys > (LW_RISCV_AMAP_SYSGPA_END - sizeof(RM_GSP_BOOT_PARAMS)))
        {
            ccPanic();
        }

#ifdef CC_GSPRM_BUILD
        memcpy(&ccInitArgs.gspRmDesc, (void*)&pBootargsUnsafe->ccInit.gspRmDesc,
               sizeof(ccInitArgs.gspRmDesc));

        // Patch the mailboxes to point to GSP-RM's boot arguments
        pPrintf("Loading 0x%llx into LW_PRGNLCL_FALCON_MAILBOX(0|1)...\n",
                ccInitArgs.gspRmDesc.bootArgsPa);

        if ((ccInitArgs.gspRmDesc.bootArgsPa < LW_RISCV_AMAP_SYSGPA_START) ||
            (ccInitArgs.gspRmDesc.bootArgsPa > LW_RISCV_AMAP_SYSGPA_END))
        {
            ccPanic();
        }

        localWrite(LW_PRGNLCL_FALCON_MAILBOX0, LwU64_LO32(ccInitArgs.gspRmDesc.bootArgsPa));
        localWrite(LW_PRGNLCL_FALCON_MAILBOX1, LwU64_HI32(ccInitArgs.gspRmDesc.bootArgsPa));
#else // CC_GSPRM_BUILD
        memcpy(&ccInitArgs, &pBootargsUnsafe->ccInit, sizeof(ccInitArgs));
#endif // CC_GSPRM_BUILD
    }

    pPrintf("GSP CC Init...\n");

#if defined(HW_CC_ENABLED) && !CC_GSPRM_BUILD
    initRaisePLM();
    if (ccInit_GH100(ccInitArgs.regkeys) != LWRV_OK)
    {
        ccPanic();
    }
    initLowerPLM();
#endif // defined(HW_CC_ENABLED) && !CC_GSPRM_BUILD

    {
        RM_GSP_ACR_CMD  *pCmd = rpcGetRequestAcr();

        pCmd->cmdType = RM_GSP_ACR_CMD_ID_BOOT_GSP_RM;
#ifdef CC_GSPRM_BUILD
        memcpy(&pCmd->bootGspRm.gspRmDesc, (void*)(&ccInitArgs.gspRmDesc), sizeof(pCmd->bootGspRm.gspRmDesc));

        pPrintf("gspRmDesc { 0x%llx, 0x%llx, %d, %d} \n",
            pCmd->bootGspRm.gspRmDesc.wprMetaPa,
            pCmd->bootGspRm.gspRmDesc.bootArgsPa,
            pCmd->bootGspRm.gspRmDesc.wprMetaSize,
            (LwU32)pCmd->bootGspRm.gspRmDesc.target);
#else
        memcpy(&pCmd->bootGspRm.gspRmProxyDesc, (void*)(&ccInitArgs.gspRmProxyDesc), sizeof(pCmd->bootGspRm.gspRmProxyDesc));

        pPrintf("gspRmProxyDesc { 0x%llx, 0x%llx, 0x%x, 0x%x, %d, %d} \n",
            pCmd->bootGspRm.gspRmProxyDesc.gspRmProxyBlobPa,
            pCmd->bootGspRm.gspRmProxyDesc.wprCarveoutBasePa,
            pCmd->bootGspRm.gspRmProxyDesc.gspRmProxyBlobSize,
            pCmd->bootGspRm.gspRmProxyDesc.wprCarveoutSize,
            (LwU32)pCmd->bootGspRm.gspRmProxyDesc.target,
            (LwU32)pCmd->bootGspRm.gspRmProxyDesc.isVirtual);
#endif
        pPrintf("Switching to ACR...\n");
        partition_init_wipe_and_switch();
    }
    // We should never reach this location
    ccPanic();
    return 0;
}


void partition_init_trap_handler(void)
{
    ccPanic();
}
