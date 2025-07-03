/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SSP_H
#define SSP_H

/*!
 * @file   ssp.h
 * @brief  SSP header for bootloader
 */

/* Initializes the __stack_chk_guard to a random value
 * derived from ptimers
 */
void sspCanarySetup(void);

#endif // SSP_H
