/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#ifndef _BOOTER_SIGNATURE_H_
#define _BOOTER_SIGNATURE_H_

#define BOOTER_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE    512
#define BOOTER_LS_HASH_BUFFER_SIZE_MAX_BYTE          64
#define BOOTER_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE   4

#define LS_SIG_VALIDATION_ID_ILWALID             0
#define LS_SIG_VALIDATION_ID_1                   1
#define LS_SIG_VALIDATION_ID_2                   2
#define LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE 12  // falconId + ucodeVer + ucodeId = 12 bytes

#endif
