/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CMDMGMT_H
#define CMDMGMT_H

#include <tasks.h>

#define PROFILE_CMD_QUEUE_LENGTH    (0x80ul)
#define PROFILE_MSG_QUEUE_LENGTH    (0x80ul)
#define RM_DMEM_EMEM_OFFSET         0x1000000U

// MK TODO: that probably should be derived from hal or profile
#define GSP_MSG_QUEUE_RM       GSP_CMD_QUEUE_RM

ct_assert(RM_GSP_CMDQ_LOG_ID == 0);
ct_assert(RM_GSP_MSGQ_LOG_ID == 1);
ct_assert(RM_GSP_LOG_QUEUE_NUM == 2);

#endif // CMDMGMT_H
