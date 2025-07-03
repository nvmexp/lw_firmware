 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubtypes.h
 * @brief: typedefs and macros for selwrescrub file is defined
 */
#ifndef SELWRESCRUB_TYPES_H
#define SELWRESCRUB_TYPES_H

#include <falcon-intrinsics.h>
#include <lwtypes.h>

/* ------------------------- Macros and Defines ----------------------------- */

//
// @brief: VPR command enum to specify GET/SET operation for VPR range.
//
typedef enum
{
    /*!
     * @brief: Get VPR region settings in MMU
     */
    VPR_SETTINGS_GET,

    /*!
     * @brief: Set new VPR settings
     */
    VPR_SETTINGS_SET,

}VPR_CMD;

//
// @brief: VPR access structure to GET/SET VPR range and VPR state.
//
typedef struct
{
    /*!
     * @brief: CMD_ID specifier
     */
    VPR_CMD cmd;

    /*!
     * @brief: Enable/Disable VPR Bit
     */
    LwBool  bVprEnabled;

    /*!
     * @brief: VPR range start
     */
    LwU64   vprRangeStartInBytes;

    /*!
     * @brief: VPR range end address
     */
    LwU64   vprRangeEndInBytes;
} VPR_ACCESS_CMD, *PVPR_ACCESS_CMD;


//
// @brief: Mutex structure containing all info related to a particular mutex.
//
typedef struct
{
    /*!
     * @brief: ID of the mutex
     */
    const LwU8 mutexId;

    /*!
     * @brief: Token returned by HW
     */
    LwU8 hwToken;

    /*!
     * @brief: Token valid or not
     */
    LwBool bValid;
} FLCNMUTEX, *PFLCNMUTEX;

typedef enum
{
    SELWREBUS_REQUEST_TYPE_WRITE_REQUEST,
    SELWREBUS_REQUEST_TYPE_READ_REQUEST,
}SELWREBUS_REQUEST_TYPE;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#endif // SELWRESCRUB_TYPES_H

