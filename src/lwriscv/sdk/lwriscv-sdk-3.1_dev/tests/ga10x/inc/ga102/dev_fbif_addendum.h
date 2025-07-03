/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_fbif_addendum_h__
#define __ga102_dev_fbif_addendum_h__

#define LW_PFALCON_FBIF_CTL2                                   0x00000084      /* RW-4R */
#define LW_PFALCON_FBIF_CTL2_NACK_MODE                         0:0             /* RWIVF */
#define LW_PFALCON_FBIF_CTL2_NACK_MODE_INIT                    0x00000000      /* RWI-V */
#define LW_PFALCON_FBIF_CTL2_NACK_MODE_NACK_AS_ACK             0x00000001      /* RW--V */

#endif // __ga102_dev_fbif_addendum_h__
