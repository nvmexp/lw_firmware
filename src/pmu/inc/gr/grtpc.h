/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  grtpc.h
 * @brief GR TPC interface.
 */

#ifndef GRTPC_H
#define GRTPC_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */

/*!
 * SM Routing Mode selection on Ampere and later.
 *
 * @brief On Ampere_and_later, SM1 register fields are deleted (Ampere-370).
 * To access SM1 register, one needs to program the SM_ROUTING_MODE
 * register (LW_PTPC_PRI_TEX_M_ROUTING) to PIPE0 or PIPE1 or BROADCAST
 * and read the respective SM0 / SM1 fields in SM register.
 *
 * Here are the 3 modes supported by LW_PTPC_PRI_TEX_M_ROUTING:
 *
 * GR_TPC_SM_ROUTING_MODE_DEFAULT
 *      Default Mode: Writes are broadcasted and reads are directed to PIPE0
 *
 * GR_TPC_SM_ROUTING_MODE_PIPE0
 *      PIPE0 Mode: Route PRI reads/writes to SM 0
 *
 * GR_TPC_SM_ROUTING_MODE_PIPE1
 *      PIPE1 Mode: Route PRI reads/writes to SM 1
 */
typedef enum
{
    GR_TPC_SM_ROUTING_MODE_DEFAULT = 0x0,
    GR_TPC_SM_ROUTING_MODE_PIPE0   = 0x1,
    GR_TPC_SM_ROUTING_MODE_PIPE1   = 0x2
} GR_TPC_SM_ROUTING_MODE;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Number of SM Pipes per TPC.
 */
#define GR_TPC_SM_PIPES_COUNT                                           (0x2)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // GRTPC_H
