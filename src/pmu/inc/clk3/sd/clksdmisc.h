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
 */

#ifndef CLK3_SD_MISC_H
#define CLK3_SD_MISC_H



/******************************************************************************
 * Shared Frequency Source Names
 ******************************************************************************/

/*!
 * @brief   The name of the sys2clk ClkFreqSrc_Object
 *
 * @details This is the name of the ClkFreqSrc_Object that is to be used to
 *          represent the output of the sys2clkpll that is an input to all
 *          OSMs.  It is a member of the global schemtic dag struct.
 *
 *          For example, this is how macros would be used to get a pointer
 *          to the ClkFreqSrc_Object and ClkFreqSrc_Final that represent the
 *          output of the sys2clkpll.
 *
 *          ClkFreqSrc_Object* sys2pllObj = CLK_ADDRESS__OBJECT(CLK_SYS2CLK_NAME);
 *          ClkFreqSrc_Final*  sys2pllFin = CLK_ADDRESS__FINAL(CLK_SYS2CLK_NAME);
 *
 * @todo    Remove this aliasing
 */
#define CLK_SYS2CLK_NAME    sys2clk.readonly

/*!
 * @brief   The name of the sppll0 ClkFreqSrc_Object
 *
 * @details This is the name of the ClkFreqSrc_Object that is to be used to
 *          represent the output of the sppll0 that is an input to all
 *          OSMs.  It is a member of the global schemtic dag struct.
 *
 *          For example, this is how macros would be used to get a pointer
 *          to the ClkFreqSrc_Object and ClkFreqSrc_Final that represent the
 *          output of the sppll0.
 *
 *          ClkFreqSrc_Object* sppll0Obj = CLK_ADDRESS__OBJECT(CLK_SPPLL0_NAME);
 *          ClkFreqSrc_Final*  sppll0Fin = CLK_ADDRESS__FINAL(CLK_SPPLL1_NAME);
 *
 * @todo    Remove this aliasing
 */
#define CLK_SPPLL0_NAME     sppll0.readonly

/*!
 * @brief   The name of the sppll1 ClkFreqSrc_Object
 *
 * @details This is the name of the ClkFreqSrc_Object that is to be used to
 *          represent the output of the sppll1 that is an input to all
 *          OSMs.  It is a member of the global schemtic dag struct.
 *
 *          For example, this is how macros would be used to get a pointer
 *          to the ClkFreqSrc_Object and ClkFreqSrc_Final that represent the
 *          output of the sppll1.
 *
 *          ClkFreqSrc_Object* sppll1Obj = CLK_ADDRESS__OBJECT(CLK_SPPLL1_NAME);
 *          ClkFreqSrc_Final*  sppll1Fin = CLK_ADDRESS__FINAL(CLK_SPPLL1_NAME);
 *
 * @todo    Remove this aliasing
 */
#define CLK_SPPLL1_NAME     sppll1.readonly

#endif // CLK3_SD_MISC_H

