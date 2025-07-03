/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTSTUB_SSP_H
#define BOOTSTUB_SSP_H

/*!
 * @file   ssp.h
 * @brief  SSP header for bootstub
 */

/* Initializes the __stack_chk_guard to a pseudo-random value
 * derived from ptimers.
 */
void sspCanarySetup(void);

#endif // BOOTSTUB_SSP_H
