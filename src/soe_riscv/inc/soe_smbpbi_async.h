/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef _SMBPBI_ASYNC_H
#define _SMBPBI_ASYNC_H

#include "soesw.h"
#include "oob/smbpbi.h"

#define LW_MSGBOX_SCRATCH_PAGE_SIZE_SOE                                     256
// Page size in dword
#define LW_MSGBOX_SCRATCH_PAGE_SIZE_D     (LW_MSGBOX_SCRATCH_PAGE_SIZE_SOE /  \
                                                sizeof(LwU32))
// expressed as a power of 2, must be >=2
#define LW_MSGBOX_SCRATCH_NUM_PAGES_P                                         2
#define LW_MSGBOX_SCRATCH_NUM_PAGES       (1 << LW_MSGBOX_SCRATCH_NUM_PAGES_P)
#define LW_MSGBOX_SCRATCH_BUFFER_SIZE_SOE (LW_MSGBOX_SCRATCH_NUM_PAGES        \
                                             * LW_MSGBOX_SCRATCH_PAGE_SIZE_SOE)
// Buffer size in dword
#define LW_MSGBOX_SCRATCH_BUFFER_SIZE_D  (LW_MSGBOX_SCRATCH_BUFFER_SIZE_SOE / \
                                                sizeof(LwU32))
#define LW_MSGBOX_REGISTER_FILE_SIZE     (LW_MSGBOX_CMD_ARG2_REG_INDEX_MAX + 1)

// This cap code plugs into LW_MSGBOX_DATA_CAP_2_NUM_SCRATCH_BANKS              
#define LW_MSGBOX_SCRATCH_NUM_PAGES_CAP_CODE                \
                (LW_MSGBOX_SCRATCH_NUM_PAGES_P > 0 ?        \
                LW_MSGBOX_SCRATCH_NUM_PAGES_P - 1 : 0) 

//! This enum lists states of exelwtion for an asynchronous request
enum SOE_SMBPBI_ASYNC_REQUEST_STATE
{
    //! No request filed
    SOE_SMBPBI_ASYNC_REQUEST_STATE_IDLE = 0,

    //! Request sent to the RM, waiting to hear back
    SOE_SMBPBI_ASYNC_REQUEST_STATE_IN_FLIGHT,

    //! Waiting for the client to collect status of the completed request
    SOE_SMBPBI_ASYNC_REQUEST_STATE_COMPLETED,
};

sysSYSLIB_CODE LwU8 smbpbiHandleScratchRegAccess(LwU32 cmd, LwU32 *pData);
sysSYSLIB_CODE LwU8 smbpbiHandleAsyncRequest(LwU32 cmd, LwU32 *pData);

#endif
