 /***************************************************************************\
|*                                                                           *|
|*      Copyright 2007-2008 LWPU Corporation.  All rights reserved.        *|
|*                                                                           *|
|*   THE SOFTWARE AND INFORMATION CONTAINED HEREIN IS PROPRIETARY AND        *|
|*   CONFIDENTIAL TO LWPU CORPORATION. THIS SOFTWARE IS FOR INTERNAL USE   *|
|*   ONLY AND ANY REPRODUCTION OR DISCLOSURE TO ANY PARTY OUTSIDE OF LWPU  *|
|*   IS STRICTLY PROHIBITED.                                                 *|
|*                                                                           *|
\***************************************************************************/
#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "lwtypes.h"

/*

    DO NOT EDIT - THIS FILE WAS AUTOMATICALLY GENERATED

*/

extern LwU32 cl95a1_secop_ucode_img[5056];
extern LwU32 cl95a1_secop_ucode_offset[4];
extern LwU32 cl95a1_secop_ucode_size[4];
extern LwU32 cl95a1_secop_ucode_addr[4];

// for unit tests and emulation only
// generated with debug SCP secrets and debug SECOP keys
extern LwU32 cl95a1_secop_const_data[192];
extern LwU32 cl95a1_secop_wrapped_signing_key[8];
extern LwU32 cl95a1_secop_wrapped_decryption_key[8];

// for silicon bringup and ISV testing only
// generated with prod SCP secrets and debug SECOP keys
extern LwU32 cl95a1_secop_const_data_bringup[192];
extern LwU32 cl95a1_secop_wrapped_signing_key_bringup[8];
extern LwU32 cl95a1_secop_wrapped_decryption_key_bringup[8];

// for final production only
// generated with prod SCP secrets and prod SECOP keys
extern LwU32 cl95a1_secop_const_data_prod[192];
extern LwU32 cl95a1_secop_wrapped_signing_key_prod[8];
extern LwU32 cl95a1_secop_wrapped_decryption_key_prod[8];

#ifdef __cplusplus
};     /* extern "C" */
#endif
// End of tsec app release /home/msdec/app/tsec/121115_secop
