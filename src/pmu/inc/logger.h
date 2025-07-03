/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LOGGER_H
#define LOGGER_H

/*!
 * @file logger.h
 */

#include "lwtypes.h"

/*!
 * Structure to store Logger Buffer Information
 */
typedef struct
{
    RM_FLCN_MEM_DESC        dmaSurface;
    RM_PMU_LOGGER_SEGMENT   segments[RM_PMU_LOGGER_SEGMENT_ID__COUNT];
} LOGGER_BUFFER_INFO;

/* Function prototypes */
void loggerWrite(LwU8 segmentId, LwU16 writeSize, void *pLogData)
    GCC_ATTRIB_SECTION("imem_loggerWrite", "loggerWrite");

#endif // LOGGER_H

