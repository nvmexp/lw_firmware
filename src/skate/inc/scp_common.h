/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SCP_COMMON_H
#define SCP_COMMON_H

/* ------------------------ System Includes --------------------------------- */

/* ------------------------ Defines ----------------------------------------- */

/*!
 * AES block size used by SCP
 */
#define SCP_AES_SIZE_IN_BITS      128
#define SCP_AES_SIZE_IN_BYTES     (SCP_AES_SIZE_IN_BITS / 8)

/*!
 * Minimum alignment for buffers used by SCP engine
 */
#define SCP_BUF_ALIGNMENT         16


#endif
