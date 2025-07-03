/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef APM_KEYS_H
#define APM_KEYS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "scp_common.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define APM_KEY_SIZE_IN_BYTES          SCP_AES_SIZE_IN_BYTES
#define APM_KEY_ALIGNMENT              SCP_BUF_ALIGNMENT

//
// APM encryption/decryption IV's are defined as 128-bits. The message and
// channel counters comprise 96-bits, and the remaining 32-bits make up the
// per-message block counter.
//
// We will call the first 96-bits the IV base. The 32-bit block counter will be
// (re)set to 0 by SEC2 for each new instance of message encryption and decryption.
//
// RM provides the IV base for encryption IVs via SPDM messages, and provides
// the IV base for decryption IVs via the channel instance block. SEC2 then is
// responsible for updating the message counter.
//
#define APM_IV_BASE_SIZE_IN_BYTES      (12)
#define APM_IV_BLOCK_CTR_SIZE_IN_BYTES (4)

#define APM_IV_SIZE_IN_BYTES           (APM_IV_BASE_SIZE_IN_BYTES + \
                                        APM_IV_BLOCK_CTR_SIZE_IN_BYTES)
#define APM_IV_ALIGNMENT               SCP_BUF_ALIGNMENT

// Ensure we reach 128-bit IV in total.
ct_assert(APM_IV_SIZE_IN_BYTES == SCP_AES_SIZE_IN_BYTES);

//
// Defines the secure channel IV context, allocated per-channel. These values
// are saved and restored along the rest of the channel context. SEC2 is
// responsible for updating the associated IVs as necessary.
//
typedef struct
{
    // The value of the decryption IV to use for this channel.
    LwU8   decIv[APM_IV_SIZE_IN_BYTES];
    // Should be 4B aligned.

    //
    // The ID of the slot in the encryption IV table which holds
    // the encryption IV for this channel.
    //
    LwU32  encIvSlotId;
} SELWRE_CHANNEL_IV_CTX;

/* ------------------------- Global Variables ------------------------------- */
extern SELWRE_CHANNEL_IV_CTX g_selwreIvCtx;

// APM-TODO CONFCOMP-377: Remove the below hardcoded values.
extern const LwU8 g_apmSymmetricKey[APM_KEY_SIZE_IN_BYTES];
extern const LwU8 g_apmIV[APM_IV_SIZE_IN_BYTES];

#endif // APM_KEYS_H
