/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_hostif.h
 * @brief  Defines structures and methods to interfaces with host.
 */
#ifndef SEC2_HOSTIF_H
#define SEC2_HOSTIF_H

/*!
 * Colwience macro to get method ID stored in method FIFO from the method tag
 * in the class header files
 */
#define SEC2_METHOD_TO_METHOD_ID(x) (x >> 2)

/*!
 * Colwience macro to get method tag (matching class file) from method ID
 */
#define SEC2_METHOD_ID_TO_METHOD(x) (x << 2)

#endif // SEC2_HOSTIF_H
