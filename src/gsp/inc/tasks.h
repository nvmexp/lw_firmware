/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TASKS_H
#define TASKS_H

#include <lwuproc.h>
#include <rmgspcmdif.h>

#define DO_TASK_DEBUGGER        1
#define DO_TASK_RM_CMDMGMT      1
#define DO_TASK_RM_MSGMGMT      1
#define DO_TASK_TEST            1

#if SCHEDULER_ENABLED
    #define DO_TASK_SCHEDULER   1
#endif

#if DISPLAY_ENABLED
    #define DO_TASK_HDCP1X      1
    #define DO_TASK_HDCP22WIRED 1
#endif

#define TASK_STACK_SIZE                 1024
#define TASK_STACK_SIZE_WORDS           (TASK_STACK_SIZE/sizeof(LwU64))

#if DO_TASK_HDCP1X
#define TASK_HDCP1X_STACK_SIZE          3072
#define TASK_HDCP1X_STACK_SIZE_WORDS    (TASK_HDCP1X_STACK_SIZE/sizeof(LwU64))
#endif

#if DO_TASK_HDCP22WIRED
#define TASK_HDCP22_STACK_SIZE          4864
#define TASK_HDCP22_STACK_SIZE_WORDS    (TASK_HDCP22_STACK_SIZE/sizeof(LwU64))
#endif

// Task (HW) CMD/MSG queue allocation (message has the same #)
#define GSP_CMD_QUEUE_RM       0
#define GSP_CMD_QUEUE_DEBUG    1

#define DEFINE_TASK_HANDLE(name) static LwrtosTaskHandle name##TaskHandle
// AM-TODO: make g_tasks_data.c for this
#define DEFINE_STACK(name, sectionName, size) \
  GCC_ATTRIB_ALIGN(16) GCC_ATTRIB_SECTION("taskData" #sectionName, #name "Stack") static LwU8 name##Stack[size]
#define DEFINE_MPUHANDLES(name, size) \
  GCC_ATTRIB_SECTION("kernel_data", #name "MpuInfo") static TaskMpuInfo name##TaskMpuInfo; \
  GCC_ATTRIB_SECTION("kernel_data", #name "MpuHandles") static MpuHandle name##MpuHandles[size]

#define DEFINE_TASK(name, sectionName, stackSize, mpuCount) \
    DEFINE_TASK_HANDLE(name); \
    DEFINE_STACK(name, sectionName, stackSize);\
    DEFINE_MPUHANDLES(name, mpuCount); \
    lwrtosTASK_FUNCTION(name##Main, pvParameters);

#if DO_TASK_RM_MSGMGMT
extern LwrtosQueueHandle rmMsgRequestQueue;
#endif

#if DO_TASK_TEST
extern LwrtosQueueHandle testRequestQueue;
#endif

#if DO_TASK_HDCP1X
extern LwrtosQueueHandle Hdcp1xQueue;
#endif

#if DO_TASK_HDCP22WIRED
extern LwrtosQueueHandle Hdcp22WiredQueue;
#endif

#if DO_TASK_SCHEDULER
extern LwrtosQueueHandle schedulerRequestQueue;
#endif

// MK TODO: this is copy of DISP2UNIT
// We assume tasks have R/O access to EMEM, otherwise we need to copy messages
typedef struct
{
   const RM_GSP_COMMAND *pCmd;
} MSG_TO_TASK;

// We have to copy that to EMEM anyway
// MK TODO: we assume command management can peek at task data
typedef struct
{
    RM_FLCN_QUEUE_HDR hdr;
    const void *payload;
} MSG_FROM_TASK;

#endif // TASKS_H
