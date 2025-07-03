/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACRUTILS_H
#define ACRUTILS_H

#include <lwtypes.h>
#include <lwmisc.h>

#include "acrutils_riscv.h"
#include <rpc.h>
//
// BAR0/CSB register access macros
// Replaced all CSB non blocking call with blocking calls
//

#define ACR_REG_RD32(bus,addr)                      ACR_##bus##_REG_RD32(addr)
#define ACR_REG_WR32(bus,addr,data)                 ACR_##bus##_REG_WR32(addr, data)
#define ACR_REG_WR32_NON_BLOCKING(bus,addr,data)    ACR_##bus##_REG_WR32_NON_BLOCKING(addr,data)

// GCC attributes
#define ATTR_OVLY(ov)          __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)    __attribute__ ((aligned(align)))

// SIZEOF macros
#ifndef LW_SIZEOF32
#define LW_SIZEOF32(v) (sizeof(v))
#endif

#ifndef NULL
#define NULL                                 (void*)(0)
#endif

// Unsigned integer wrap checking macro for addition
#define CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(arg1, arg2, maxVal)                 \
    do                                                                                                  \
    {                                                                                                   \
        if(((maxVal) - (arg1)) < (arg2))                                                                \
        {                                                                                               \
            return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;                                                    \
        }                                                                                               \
    } while (0)

#define CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS(arg1, arg2, maxVal)           \
    do                                                                                                  \
    {                                                                                                   \
        if(((maxVal) - (arg1)) < (arg2))                                                                \
        {                                                                                               \
            status =  ACR_ERROR_VARIABLE_SIZE_OVERFLOW;                                                 \
            goto Cleanup;                                                                               \
        }                                                                                               \
    } while (0)

// Status checking macros
#define CHECK_STATUS_AND_RETURN_IF_NOT_OK(x)                          \
    do                                                                \
    {                                                                 \
        if((status=(x)) != ACR_OK)                                    \
        {                                                             \
            return status;                                            \
         }                                                            \
    } while (0)

#define CHECK_STATUS_OK_OR_GOTO_CLEANUP(x)                            \
    do                                                                \
    {                                                                 \
        if((status=(x)) != ACR_OK)                                    \
        {                                                             \
            goto Cleanup;                                             \
        }                                                             \
    } while (0)

// CCC status checking macros
#define CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(status_ccc, status_acr) \
    do                                                                \
    {                                                                 \
        if((statusCcc =(status_ccc)) != NO_ERROR)                     \
        {                                                             \
            return status_acr;                                        \
        }                                                             \
    } while (0)

#define CHECK_STATUS_CCC_OK_OR_GOTO_CLEANUP(status_ccc, status_acr)   \
    do                                                                \
    {                                                                 \
        if((statusCcc =(status_ccc)) != NO_ERROR)                     \
        {                                                             \
            status = status_acr;                                      \
            goto Cleanup;                                             \
        }                                                             \
    } while (0)

// liblwriscv Status checking macros
#define CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(status_lwrv, status_acr)                       \
    do                                                                                              \
    {                                                                                               \
        LWRV_STATUS lwrv_status = status_lwrv;                                                      \
        if((lwrv_status) != LWRV_OK)                                                                \
        {                                                                                           \
            PRM_GSP_ACR_MSG        pRpcAcrMsg = rpcGetReplyAcr();                                   \
            PRM_GSP_ACR_MSG_STATUS pAcrStatus = (PRM_GSP_ACR_MSG_STATUS)&pRpcAcrMsg->acrStatus;     \
            pAcrStatus->libLwRiscvError       = lwrv_status;                                        \
            return status_acr;                                                                      \
        }                                                                                           \
    } while (0)


#define CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(status_lwrv, status_acr)                         \
    do                                                                                              \
    {                                                                                               \
        LWRV_STATUS lwrv_status = status_lwrv;                                                      \
        if((lwrv_status) != LWRV_OK)                                                                \
        {                                                                                           \
            PRM_GSP_ACR_MSG        pRpcAcrMsg = rpcGetReplyAcr();                                   \
            PRM_GSP_ACR_MSG_STATUS pAcrStatus = (PRM_GSP_ACR_MSG_STATUS)&pRpcAcrMsg->acrStatus;     \
            pAcrStatus->libLwRiscvError       = lwrv_status;                                        \
            status                            = status_acr;                                         \
            goto Cleanup;                                                                           \
        }                                                                                           \
    } while (0)

// DMA
#define DMA_XFER_ESIZE_MIN               (0x00U) //   4-bytes
#define DMA_XFER_ESIZE_MAX               (0x06U) // 256-bytes
#ifndef DMA_XFER_SIZE_BYTES
#define DMA_XFER_SIZE_BYTES(e)           (LWBIT32((e) + 2U))
#endif
#define VAL_IS_ALIGNED   LW_IS_ALIGNED


#define POWER_OF_2_FOR_4K                                   (12U)
#define POWER_OF_2_FOR_128K                                 (17U)
#define POWER_OF_2_FOR_1MB                                  (20U)
#define COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT              (POWER_OF_2_FOR_1MB  - POWER_OF_2_FOR_4K)
#define COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT            (POWER_OF_2_FOR_128K - POWER_OF_2_FOR_4K)

// Timeouts
#define ACR_TIMEOUT_FOR_PIPE_RESET                          (0x400)

// Infinite loop handler
#define INFINITE_LOOP()  while(LW_TRUE)

//
// Theoretically these parameters should be characterized on silicon and may change for
// each generation. However SCP RNG has many configuration options and no one would like
// to characterize all the possibilities and pick the best one. SW therefore uses the
// option which was recommended by HW for an earlier generation.
// These values should be good for Turing too(even though not the best). Ideally we should
// generate random numbers with this option and compare against NIST test suite.
// However if we boost performance by doing some post processing e.g. generating 2 conselwtive
// random numbers and encrypting one with the other to derive the final random number, we
// should be good.
//
#define SCP_HOLDOFF_INIT_LOWER_VAL (0x7fff)
#define SCP_HOLDOFF_INTRA_MASK     (0x03ff)
#define SCP_AUTOCAL_STATIC_TAPA    (0x000f)
#define SCP_AUTOCAL_STATIC_TAPB    (0x000f)

//*****************************************************************************
// Memory Routines
//*****************************************************************************
#define DMSIZE_4B       (0x0000)
#define DMSIZE_8B       (0x0001)
#define DMSIZE_16B      (0x0002)
#define DMSIZE_32B      (0x0003)
#define DMSIZE_64B      (0x0004)
#define DMSIZE_128B     (0x0005)
#define DMSIZE_256B     (0x0006)

#endif //ACRUTILS_H
