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
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/ssp.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/gcc_attrs.h>
#include <lwpu-sdk/lwmisc.h>
#include "partitions.h"
#include "misc.h"
#include "gsp/gspifspdm.h"
#include "flcnretval.h"
#include "dev_se_seb.h"
#include "cc_spdm.h"
#include "cc_dma.h"
#include "rpc.h"

#ifdef USE_LIBSPDM
#include "spdm_responder_lib_internal.h"
#include "spdm_selwred_message_lib_internal.h"
#include "lw_gsp_spdm_lib_config.h"
#include "tegra_se.h"
#include "tegra_lwpka_mod.h"
#include "tegra_lwpka_gen.h"
#endif

#ifdef USE_LIBSPDM
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#define LIBSPDM_CONTEXT_SIZE_BYTES (sizeof(spdm_context_t) +                \
                                    sizeof(spdm_selwred_message_context_t))

/* ------------------------- Prototypes ------------------------------------- */

//
// libspdm keeps state in large context structure. Session state is kept in
// separate structure, but libspdm expects it placed immediately after
// context structure in memory. Therefore, store both in large buffer.
//
static LwU8 g_deviceContext[LIBSPDM_CONTEXT_SIZE_BYTES];

#endif // USE_LIBSPDM

CC_SPDM_PART_CTX     g_ccSpdmPartCtx;

// 4 is the minimal required aligned address for DMA.
LW_SPDM_DESC_HEADER  g_spdmDescHdr                                   ATTR_ALIGNED(LW_RISCVLIB_DMA_ALIGNMENT);

// 4 is the minimal required aligned address for DMA.
LwU8                 g_spdmMsg[CC_SPDM_MESSAGE_BUFFER_SIZE_IN_BYTE]  ATTR_ALIGNED(LW_RISCVLIB_DMA_ALIGNMENT);

static LwBool
spdmCtrlSanityCheck
(
    PRM_GSP_SPDM_CC_CTRL_CTX   pCtrlCtx
)
{
    if (pCtrlCtx->guestId != g_ccSpdmPartCtx.guestId ||
        pCtrlCtx->endpointId != g_ccSpdmPartCtx.endpointId)
    {
        pPrintf("spdmCtrlSanityCheck: id mismatch: guest Id (0x%x vs 0x%0x), endpointId (0x%x vs 0x%x)\n",
                pCtrlCtx->guestId, g_ccSpdmPartCtx.guestId, pCtrlCtx->endpointId, g_ccSpdmPartCtx.endpointId);
        return LW_FALSE;
    }

    return LW_TRUE;
}

static FLCN_STATUS
spdmCtrlHanlder
(
    PRM_GSP_SPDM_CC_CTRL_CTX   pCtrlCtx
)
{
    LwU32 ctrlCode = pCtrlCtx->ctrlCode;
    FLCN_STATUS status = FLCN_OK;

    if (g_ccSpdmPartCtx.state != PART_STATE_READY)
    {
        pPrintf("spdmCtrlHandeler: error g_ccSpdmPartCtx.state = 0x%x \n", g_ccSpdmPartCtx.state);
        return FLCN_ERROR;
    }

    g_ccSpdmPartCtx.state = PART_STATE_CTRL_PROCESSING;

    switch (ctrlCode)
    {
        case CC_CTRL_CODE_SPDM_MESSAGE_PROCESS:
        {
            if (spdmCtrlSanityCheck(pCtrlCtx))
            {
                // TODO: Should be moved to HAL
#ifdef USE_LIBSPDM
                status = spdmMsgProcess(pCtrlCtx, (void *)g_deviceContext);
#else
                status = spdmMsgProcess(pCtrlCtx, NULL);
#endif
            }
            else
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
            }
        }
        break;

        case CC_CTRL_CODE_SESSION_MESSAGE_PROCESS:
            // TODO: Need to process session message ??
            status = FLCN_OK;
        break;

        default:
            status = FLCN_ERR_ILLEGAL_OPERATION;
    }

    g_ccSpdmPartCtx.state = PART_STATE_READY;
    return status;
}

// TODO: Should be moved to HAL
static FLCN_STATUS spdmProgramCeKeys(GCC_ATTR_UNUSED LwU32 ceIndex,
                                     GCC_ATTR_UNUSED LwU32 keyIndex,
                                     GCC_ATTR_UNUSED LwU32 ivSlotIndex)
{
    // Example code to access CE key slot
    pPrintf("Example output for programming CE keys\n");

/*
    LwU32 randVal = 0x12345678;
    LwU32 plm     = 0;
    LwU32 readVal = 0;

    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;

    // TODO: Check return value for error
    dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_SCE_LCE_ENCRYPT_KEY_A(0,0), &randVal);
    dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_SCE_LCE_ENCRYPT_KEY_A(0,0), &readVal);
    dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_SCE_KEY_AND_RNG_PRIV_LEVEL_MASK, &plm);

    pPrintf("spdmProgramCeKeys values - randVal=%x, key_a(0,0)=%x, plm=%x\n", randVal, readVal, plm);
*/

    return FLCN_OK;
}

static FLCN_STATUS spdmPartitionEntry(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3)
{
    FLCN_STATUS  status = FLCN_OK;

    pPrintf("SPDM partition entry triggered. 0=%llx, 1=%llx, 2=%llx, 3=%llx\n", arg0, arg1, arg2, arg3);

    // Handle received command
    switch(arg0)
    {
        // Program CE Keys
        case RM_GSP_SPDM_CMD_ID_PROGRAM_CE_KEYS:
        {
            RM_GSP_SPDM_CMD_PROGRAM_CE_KEYS programCeKeys;

            programCeKeys.ceKeyInfo.ceIndex     = (LwU32)arg1;
            programCeKeys.ceKeyInfo.keyIndex    = (LwU32)arg2;
            programCeKeys.ceKeyInfo.ivSlotIndex = (LwU32)arg3;

            status = spdmProgramCeKeys(programCeKeys.ceKeyInfo.ceIndex, programCeKeys.ceKeyInfo.keyIndex, programCeKeys.ceKeyInfo.ivSlotIndex);
        }
        break;

        case RM_GSP_SPDM_CMD_ID_CC_INIT:
        {
                PCC_DMA_PROP              pDmaProp       = &g_ccSpdmPartCtx.dmaProp;
                PRM_GSP_SPDM_CMD          pRpcGspSpdmCmd = rpcGetGspSpdmCmd();
                PRM_GSP_SPDM_CC_INIT_CTX  pInitCtx       = (PRM_GSP_SPDM_CC_INIT_CTX)&pRpcGspSpdmCmd->ccInit.ccInitCtx;
                PLW_SPDM_DESC_HEADER      pDescHdrRsp    = &g_spdmDescHdr;

                pPrintf("RM_GSP_SPDM_CMD_ID_CC_INIT receive \n");

                if (g_ccSpdmPartCtx.state == PART_STATE_UNKNOWN ||
                    g_ccSpdmPartCtx.state == PART_STATE_FREE)
                {

                #ifdef USE_LIBSPDM
                    if (init_crypto_devices() != NO_ERROR) 
                    {
                        pPrintf("Init Crypto Devices failed!\n");
                    }
                    status = spdmLibContextInit((void *)g_deviceContext);

                    if (status != FLCN_OK)
                    {
                        pPrintf("SPDM library initialization failed (0x%x), \n", status);
                        goto _ccInitError;
                    }
                #endif

                    g_ccSpdmPartCtx.guestId            = pInitCtx->guestId;
                    g_ccSpdmPartCtx.rmBufferSizeInByte = pInitCtx->rmBufferSizeInByte;
                    // Lwrrently, we only have 1 SPDM partition. so we just assign endpoint Id to CC_GSP_SPDM_ENDPOINT_ID_0.
                    g_ccSpdmPartCtx.endpointId         = CC_GSP_SPDM_ENDPOINT_ID_0;
                    pDmaProp->dmaIdx                   = pInitCtx->dmaIdx;
                    pDmaProp->dmaAddr                  = pInitCtx->dmaAddr;
                    pDmaProp->regionId                 = pInitCtx->regionId;
                    pDmaProp->addrSpace                = pInitCtx->addrSpace;

                    // Setup the aperture that is to be used for DMA transaction.
                    if ((status = confComputeSetupApertureCfg(pDmaProp->regionId,
                                                              pDmaProp->dmaIdx,
                                                              pDmaProp->addrSpace)) == FLCN_OK)
                    {
                        g_ccSpdmPartCtx.state              = PART_STATE_READY;
                        status = FLCN_OK;
                    }
                }
                else
                {
                    status = FLCN_ERROR;
                }
                pDescHdrRsp->guestId    = pInitCtx->guestId;
                //
                // Because we only have 1 SPDM partition in GSP, the default SPDM endpoint is 0.
                // Once we create multi SPDM partitions in GSP, we need to give different endpoint ID
                //
                pDescHdrRsp->endpointId = CC_GSP_SPDM_ENDPOINT_ID_0;
#ifdef USE_LIBSPDM
_ccInitError:
#endif
                pDescHdrRsp->status    = status;
                spdmWriteDescHdr(pDescHdrRsp);
        }
        break;

        case RM_GSP_SPDM_CMD_ID_CC_DEINIT:
        {
                PRM_GSP_SPDM_CMD            pRpcGspSpdmCmd = rpcGetGspSpdmCmd();
                PLW_SPDM_DESC_HEADER        pDescHdrRsp    = &g_spdmDescHdr;
                PRM_GSP_SPDM_CC_DEINIT_CTX  pDeinitCtx     = (PRM_GSP_SPDM_CC_DEINIT_CTX)&pRpcGspSpdmCmd->ccDeinit.ccDeinitCtx;
                LwBool                      bValid         = LW_FALSE;

                pPrintf("RM_GSP_SPDM_CMD_ID_CC_DEINIT receive \n");

                if (pDeinitCtx->flags & DEINIT_FLAGS_FORCE_CLEAR)
                {
                    bValid = LW_TRUE;
                }
                else if (g_ccSpdmPartCtx.guestId   == pDeinitCtx->guestId &&
                         g_ccSpdmPartCtx.endpointId == pDeinitCtx->endpointId)
                {
                    bValid = LW_TRUE;
                }
                else
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                }

                if (bValid)
                {
                    g_ccSpdmPartCtx.state = PART_STATE_FREE;
                    g_ccSpdmPartCtx.guestId = CC_SPDM_GUEST_ID_ILWALID;
                    g_ccSpdmPartCtx.endpointId = CC_SPDM_ENDPOINT_ID_ILWALID;
                    status = FLCN_OK;
                }
                pDescHdrRsp->guestId    = pDeinitCtx->guestId;
                pDescHdrRsp->endpointId = pDeinitCtx->endpointId;
                pDescHdrRsp->status     = status;
                spdmWriteDescHdr(pDescHdrRsp);
        }
        break;

        case RM_GSP_SPDM_CMD_ID_CC_CTRL:
        {
            PRM_GSP_SPDM_CMD          pRpcGspSpdmCmd = rpcGetGspSpdmCmd();
            PRM_GSP_SPDM_CC_CTRL_CTX  pCcCtrlCtx  = (PRM_GSP_SPDM_CC_CTRL_CTX)&pRpcGspSpdmCmd->ccCtrl.ccCtrlCtx;
            pPrintf("RM_GSP_SPDM_CMD_ID_CC_CTRL receive \n");

            status = spdmCtrlHanlder(pCcCtrlCtx);
        }
        break;

        // Invalid command
        default:
            status = FLCN_ERR_ILLEGAL_OPERATION;
            break;

    }

    pPrintf("Command %llu processing done. status=%d\n", arg0, status);

    return status;
}

// C "entry" to partition, has stack and liblwriscv
int partition_spdm_main(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 partition_origin)
{
    LwU32 status;

    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        ccPanic();
    }

    if (partition_origin == PARTITION_ID_RM)
    {
        pPrintf("Hello from SPDM partition.\n");
        pPrintf("My args are %llx %llx %llx %llx\n", arg0, arg1, arg2, arg3);
        pPrintf("My SSPM is %lx ucode id %lx\n", csr_read(LW_RISCV_CSR_SSPM), csr_read(LW_RISCV_CSR_SLWRRUID));
        status = spdmPartitionEntry(arg0, arg1, arg2, arg3);
        pPrintf("Returning back.\n");
        partitionSwitchNoreturn(status, 0, 0, 0, PARTITION_ID_SPDM, partition_origin);
    }
    else
    {
        pPrintf("Hello from SPDM partition.\n");
        pPrintf("Called from incorrect partition.\n");
        status = FLCN_ERROR;
        pPrintf("Returning back.\n");
        partitionSwitchNoreturn(status, 0, 0, 0, PARTITION_ID_SPDM, partition_origin);
    }

    // We wont reach to this code
    pPrintf("SPDM shutting down\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return 0;
}

void partition_spdm_trap_handler(void)
{
    pPrintf("In Partition spdm trap handler at 0x%lx, cause 0x%lx val 0x%lx\n",
            csr_read(LW_RISCV_CSR_SEPC),
            csr_read(LW_RISCV_CSR_SCAUSE),
            csr_read(LW_RISCV_CSR_STVAL));

    ccPanic();
}

