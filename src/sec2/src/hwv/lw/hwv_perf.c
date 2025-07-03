
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file   hwv_perf.c
 * @brief  Implementations for HWV performance testing functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "hwv/hwv_mthds.h"
#include "sec2_objsec2.h"
#include "sec2_objhwv.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define GPTMR_MAX_INTERVAL  0xFFFFFFFF

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _hwvDoMeasureDma(LwU64 inGpuVA256, LwU64 outGpuVA256,
                                    LwU32 inOutBufSize,
                                    hwv_perf_eval_results *pPerfResults,
                                    LwU8 *pHwvBuffer256, LwU32 hwvBufferSize)
    GCC_ATTRIB_SECTION("imem_hwv", "_hwvDoMeasureDma");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * Global buffer used for staging of data during DMA read/write.
 */
static LwU8 g_dmaBuf[HWV_DMA_BUFFER_SIZE] GCC_ATTRIB_ALIGN(256);

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Gets measurements of DMA performance by performing copy of requested
 *          number of bytes from input to output external (SysMem/FB) addresses.
 *
 *          Uses local DMEM buffer as temporary staging ground, reading as much
 *          data as it can, then writing afterwards, all while measuring total
 *          time taken as well as number of clock ticks taken.
 *
 * @param[in]      inGpuVA256     Address of buffer in external memory (SysMem/FB)
 *                                from which data should be copied. 256-byte aligned.
 * @param[out]     outGpuVA256    Address of buffer in external memory (SysMem/FB)
 *                                to where data should be copied. 256-byte aligned.
 * @param[in]      inOutBufSize   Total number of bytes to read from inGpuVA256
 *                                and write to outGpuVA256.
 * @param[in, out] pHwvBuffer256  Pointer to local buffer to be used as staging
 *                                for DMA operation. 256-byte aligned.
 * @param[in]      hwvBufferSize  Size of pHwvBuffer256 in bytes.
 *
 * @return  FLCN_OK if DMA copy test is successful.
 *          Otherwise, relevant error indicated in returned FLCN_STATUS.
 */
FLCN_STATUS
_hwvDoMeasureDma
(
    LwU64                 inGpuVA256,
    LwU64                 outGpuVA256,
    LwU32                 inOutBufSize,
    hwv_perf_eval_results *pPerfResults,
    LwU8                  *pHwvBuffer256,
    LwU32                 hwvBufferSize
)
{
    FLCN_STATUS      status         = FLCN_OK;
    LwU32            readTimeNs     = 0;
    LwU32            writeTimeNs    = 0;
    LwU32            startTicks     = 0;
    LwU32            readTicks      = 0;
    LwU32            writeTicks     = 0;
    LwU32            bytesRead      = 0;
    LwU32            bytesToRead    = 0;
    RM_FLCN_MEM_DESC inputMemDesc;
    RM_FLCN_MEM_DESC outputMemDesc;
    FLCN_TIMESTAMP   startTimestamp;

    if (pPerfResults == NULL || pHwvBuffer256 == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Load GP timer overlay for clock tick measurements, and enable timer.
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libGptmr));
    if ((status = sec2GptmrEnable_HAL(&Sec2, GPTMR_MAX_INTERVAL, LW_TRUE,
                                      100)) != FLCN_OK)
    {
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGptmr));
        return status;
    }

    pPerfResults->dmaReadTotalTimeNs  = 0;
    pPerfResults->dmaWriteTotalTimeNs = 0;
    pPerfResults->dmaReadTotalTicks   = 0;
    pPerfResults->dmaWriteTotalTicks  = 0;

    // Initialize memory and result structs
    RM_FLCN_U64_PACK(&(inputMemDesc.address), &inGpuVA256);
    inputMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                          RM_SEC2_DMAIDX_VIRT, 0);
    inputMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                          inOutBufSize, inputMemDesc.params);

    RM_FLCN_U64_PACK(&(outputMemDesc.address), &outGpuVA256);
    outputMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                          RM_SEC2_DMAIDX_VIRT, 0);
    outputMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                          inOutBufSize, outputMemDesc.params);

    //
    // Read data from input and write to output in chunks,
    // while measuring performance of each chunk to get total performance.
    //
    while (bytesRead < inOutBufSize)
    {
        bytesToRead = inOutBufSize - bytesRead;

        if (bytesToRead > hwvBufferSize)
        {
            bytesToRead = hwvBufferSize;
        }

        // Reset timer interval to avoid underflow on large transfers.
        if ((status = sec2GptmrResetInterval_HAL()) != FLCN_OK)
        {
            goto label_return;
        }

        // Execute read and measure performance.
        startTicks = sec2GptmrReadValue_HAL();
        osPTimerTimeNsLwrrentGet(&startTimestamp);
        if ((status = dmaRead(pHwvBuffer256, &inputMemDesc,
                              bytesRead, bytesToRead)) != FLCN_OK)
        {
            goto label_return;
        }

        readTimeNs = osPTimerTimeNsElapsedGet(&startTimestamp);
        readTicks = startTicks - sec2GptmrReadValue_HAL();

        //
        // Execute write and measure performance.
        // Note that we ensure memory coherence with a membar.
        // We will take a perf hit but should be around even across runs.
        //
        startTicks = sec2GptmrReadValue_HAL();
        osPTimerTimeNsLwrrentGet(&startTimestamp);
        if ((status = dmaWrite(pHwvBuffer256, &outputMemDesc,
                               bytesRead, bytesToRead)) != FLCN_OK)
        {
            goto label_return;
        }
        sec2FbifFlush_HAL();

        writeTimeNs = osPTimerTimeNsElapsedGet(&startTimestamp);
        writeTicks = startTicks - sec2GptmrReadValue_HAL();

        // Update total performance counters
        pPerfResults->dmaReadTotalTimeNs  += readTimeNs;
        pPerfResults->dmaWriteTotalTimeNs += writeTimeNs;
        pPerfResults->dmaReadTotalTicks   += readTicks;
        pPerfResults->dmaWriteTotalTicks  += writeTicks;

        bytesRead += bytesToRead;
    }

label_return:

    // Disable GP timer and detach overlay.
    sec2GptmrDisable_HAL();
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGptmr));

    return status;
}

/*!
 * @brief   Runs performance test of DMA operation. Takes in pointer to
 *          parameter struct stored in external memory (SysMem/FB),
 *          which provides arguments for performance test.
 *          
 *          Lwrrently performs DMA read/write performance testing.
 *
 * @return  FLCN_OK if performance test runs successsfully.
 *          Otherwise, relevant error indicated in returned FLCN_STATUS.
 */
FLCN_STATUS
hwvRunPerfEval(void)
{
    FLCN_STATUS       status         = FLCN_OK;
    LwU64             perfCmdExtAddr = 0;
    hwv_perf_eval_cmd perfCmd;
    RM_FLCN_MEM_DESC  memDesc;

    // Get address of perf command structure & create memory descriptor.
    perfCmdExtAddr = HWV_GET_METHOD_PARAM_OFFSET_FROM_ID(PERF_EVAL);
    perfCmdExtAddr = perfCmdExtAddr << 8;

    RM_FLCN_U64_PACK(&(memDesc.address), &perfCmdExtAddr);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_VIRT, 0);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     sizeof(hwv_perf_eval_cmd), memDesc.params);

    // Retrieve perf command struct from FB
    if ((status = dmaRead((LwU8*)&perfCmd, &memDesc,
                          0, sizeof(hwv_perf_eval_cmd))) != FLCN_OK)
    {
        goto label_return;
    }

    // Get test parameters and run tests
    if ((status = _hwvDoMeasureDma((LwU64)(perfCmd.inGpuVA256 << 8),
                                   (LwU64)(perfCmd.outGpuVA256 << 8),
                                   perfCmd.inOutBufSize,
                                   &(perfCmd.perfResults),
                                   (LwU8*)&g_dmaBuf,
                                   HWV_DMA_BUFFER_SIZE)) != FLCN_OK)
    {
        goto label_return;
    }

    // Write back all results to original data buffer
    if ((status = dmaWrite((LwU8*)(&perfCmd), &memDesc,
                           0, sizeof(hwv_perf_eval_cmd))) != FLCN_OK)
    {
        goto label_return;
    }

label_return:

    // Notify COMPLETION or NACK
    if (status == FLCN_ERR_DMA_NACK)
    {
        NOTIFY_EXELWTE_NACK();
    }
    else
    {
        NOTIFY_EXELWTE_COMPLETE();
    }

    return status;
}
