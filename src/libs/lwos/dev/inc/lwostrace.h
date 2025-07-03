/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file    lwostrace.h
 * @brief   Trace module
 *
 * This trace module is used to dump some debug traces 'a la printf' to FB.
 * On the contrary to falc_printf, the traces are not formatted by the falcon,
 * but dumped untouched into FB : this reduces considerably the size of the
 * function and is less intrusive. A small parser is necessary to interpret
 * the traces from FB into a lisible ASCII format.
 */

#ifndef LWOS_TRACE_H
#define LWOS_TRACE_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "flcnifcmn.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initialise the REG trace module
 *
 * TODO ...
 */
void osTraceRegInit(LwU32 addr);

/*!
 * @brief   REG trace function
 *
 * TODO ...
 */
void osTraceReg(char *str, LwU32 param1, LwU32 param2, LwU32 param3, LwU32 param4);

/*!
 * @brief   Initialise the FB trace module
 *
 * @param[in]   pBufDesc    Trace buffer DMA descriptor.
 */
void osTraceFbInit(RM_FLCN_MEM_DESC *pBufDesc);

/*!
 * @brief   FB trace function
 *
 * The trace function is limited to 4 32-bit parameters on top of the formatted
 * string. It means concretely that only %d , %x, %p  are supported. %s is NOT
 * supported. This function can be called under interrupt. The total buffer
 * written to FB is limited to 64 bytes. It means that the formatted string is
 * limited to 64-5*4 = 44 bytes. A trace buffer is written every 64 bytes,
 * whatever the size of the actual formatted string. The 32-bit written is
 * a trace counter, the next four 32-bits are the 4 actual parameters. These
 * five 32-bit values are followed by the formatted string. If the trace buffer
 * reaches the end, it is automatically wraps to the beginning.
 *
 * @param[in]   fmt     formatted string
 * @param[in]   arg1    32-bit parameter
 * @param[in]   arg2    32-bit parameter
 * @param[in]   arg3    32-bit parameter
 * @param[in]   arg4    32-bit parameter
 */
void osTraceFb(const char *fmt, LwU32 arg1, LwU32 arg2, LwU32 arg3, LwU32 arg4);

#endif // LWOS_TRACE_H

