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
#ifndef HDCPMC_MEM_H
#define HDCPMC_MEM_H

#include "hdcpmc/hdcpmc_types.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Required alignment for DMA transfers of HDCP method parameters.
 */
#define HDCP_DMA_BUFFER_SIZE                                                256
#define HDCP_DMA_ALIGNMENT                                DMA_MIN_READ_ALIGNMENT

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS hdcpDmaRead(void *pBuffer, LwU32 fbOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpDmaRead");
FLCN_STATUS hdcpDmaWrite(const void *pBuffer, LwU32 fbOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpDmaWrite");

FLCN_STATUS hdcpDmaRead_hs(void *pBuffer, LwU32 fbOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_hdcpEncrypt", "hdcpDmaRead_hs");
FLCN_STATUS hdcpDmaWrite_hs(const void *pBuffer, LwU32 fbOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_hdcpEncrypt", "hdcpDmaWrite_hs");

#endif // HDCPMC_MEM_H
#endif
