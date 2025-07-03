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
 * @file clk_domain_prog-mock.h
 * @brief Mock interface for clk_domain_prog public interface.
 */

#ifndef CLK_DOMAIN_PROG_MOCK_H
#define CLK_DOMAIN_PROG_MOCK_H

#include "pmusw.h"
#include "clk/clk_domain_prog.h"

/* ------------------------ Data Types -------------------------------------- */
typedef FLCN_STATUS (clkDomainProgIsNoiseAware_MOCK_CALLBACK)(CLK_DOMAIN_PROG  *pDomainProg, LwBool *pbIsNoiseAware);

/* ------------------------ Public Functions -------------------------------- */
void clkDomainProgIsNoiseAwareMockInit();
void clkDomainProgIsNoiseAwareMockAddEntry(LwU8 entry, FLCN_STATUS status);
void clkDomainProgIsNoiseAware_StubWithCallback(LwU8 entry, clkDomainProgIsNoiseAware_MOCK_CALLBACK *pCallback);

#endif // CLK_DOMAIN_PROG_MOCK_H
