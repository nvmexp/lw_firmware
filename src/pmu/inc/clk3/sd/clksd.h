/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_SD_H
#define CLK3_SD_H

/*!
 * @brief       Schematic Construction
 * @see         clkSchematicDag
 * @see         clkFreqDomainArray
 *
 * @details     Construct the schematic dag for this chip.  Specifically, the
 *              private 'clkSchematicDag' structure is initialized for anything
 *              that can not be determined at compile time.  Most fields in the
 *              structure are initialized at compile time.
 */
FLCN_STATUS clkConstruct_SchematicDag(void)
        GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkConstruct_SchematicDag");

#endif // CLK3_SD_H

