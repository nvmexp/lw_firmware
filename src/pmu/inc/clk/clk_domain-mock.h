/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk_domain-mock.h
 * @brief   Data required for configuring mock clk_domain interfaces.
 */

#ifndef CLK_DOMAIN_MOCK_H
#define CLK_DOMAIN_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"
#include "clk/clk_domain.h"

/* ------------------------ Function Prototypes ----------------------------- */
void clkDomainLoadMockInit(void);
void clkDomainLoadMockAddEntry(LwU8 entry, FLCN_STATUS status);

void clkDomainIsFixedMockInit(void);
void clkDomainIsFixedMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CLK_DOMAIN_MOCK_H
