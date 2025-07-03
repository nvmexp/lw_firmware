/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PR_MPK_H
#define PR_MPK_H

/*!
* @file pr_mpk.h
* This file contains all the defines/macros which will be used in MPK 
*/

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Types definitions ------------------------------ */

// Size of MPK
#define PR_MPK_SIZE_DWORD 8 // Size of MPK

// SALT used in MPK encryption
#define PR_MPK_ENCRYPT_SALT_0 0x73934871
#define PR_MPK_ENCRYPT_SALT_1 0x962534ab
#define PR_MPK_ENCRYPT_SALT_2 0xfd237854
#define PR_MPK_ENCRYPT_SALT_3 0xecf61209

// Secret key index used
#define PR_MPK_SECRET_KEY_INDEX 14

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS prDecryptMpk_GP10X(LwU32 *pOut);

#endif // _PR_MPK_H_
