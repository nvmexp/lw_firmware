/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_HDCP_H
#define LIB_HDCP_H

#include "flcntypes.h"
#include "flcnifhdcp.h"
#include "lwos_dma.h"

/*!
 * @brief Context information for the HDCP
 */
typedef struct
{
    LwU8                status;
    LwU8                seqNumId;
    LwU8                cmdQueueId;
    LwU8                l[20];          // Add for packing our own L
    LwU8                reserved;       //Padding
    RM_FLCN_HDCP_CMD    hdcpCmd;
    RM_FLCN_HDCP_MSG    hdcpMsg;        // Reply message
    RM_FLCN_CMD        *pOriginalCmd;   // Original Command for clearing the Queue
} HDCP_CONTEXT;

/*! @brief Bus error code */
typedef LwU8 HDCP_BUS_STATUS;

/* ------------------------ Macros & Defines ------------------------------- */
#define CHECK_STATUS(x)                 if ((status = (x)) != FLCN_OK){goto label_return;}

/*! @brief Successful operation */
#define HDCP_BUS_SUCCESS                (0x00)

/*! @brief Read error */
#define HDCP_BUS_READ_ERROR             (0x11)

/*! @brief Write error */
#define HDCP_BUS_WRITE_ERROR            (0x12)

/*! @brief Timeout error */
#define HDCP_BUS_TIMEOUT_ERROR          (0x13)

#define hdcpMemcpy(x,y,z)               memcpy(x,y,z)
#define hdcpMemcmp(x,y,z)               memcmp(x,y,z)
#define hdcpMemset(x,y,z)               memset(x,y,z)

#endif // LIB_HDCP_H
