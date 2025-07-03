/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTSTUB_LOAD_H
#define BOOTSTUB_LOAD_H

/*!
 * @file   load.h
 * @brief  Interfaces for transfering control to new exelwtable.
 */

#include <lwtypes.h>

/*!
 * @brief Begins exelwtion of new program.
 *
 * @param[in] entryPoint   Entry point of the new program.
 *
 * Clears the last bits of state required, sets up arguments, then jumps to the
 * new application.
 *
 * @return Does not return.
 */
void bootLoad(LwUPtr entryPoint)
    __attribute__((noreturn));

#endif // BOOTSTUB_LOAD_H
