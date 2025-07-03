/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PR_LASSAHS_HS_H
#define PR_LASSAHS_HS_H

/*!
 * @file pr_lassahs_hs.h
 * This file contains all the defines/macros which will be used in LASSAHS at HS
 * mode.  LASSAHS = LASS As HS (LS At Same Security As HS).
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_sec_csb.h"
#include "dev_sec_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Check the scratch register to know if the specific action has been done
 */
#define PR_LASSAHS_IS_STATE(a)   (REG_RD32(CSB, LW_CSEC_SELWRE_SCRATCH_0) == LW_CSEC_SELWRE_SCRATCH_0_PR_##a)

/*!
 * Set the scratch register to a new state after an action has been done
 */
#define PR_LASSAHS_SET_STATE(a)  REG_WR32(CSB, LW_CSEC_SELWRE_SCRATCH_0, LW_CSEC_SELWRE_SCRATCH_0_PR_##a)

/*!
 * Blocking DMEM access depending on the secure level
 */
#define PR_DMEM_ACCESS_ALLOW_LEVEL3           0x0
#define PR_DMEM_ACCESS_ALLOW_LEVEL2           0x4

/* ------------------------- Types definitions ------------------------------ */
/*!
 * Defines a type that used to hold data needed for LASSAHS.
 */
typedef struct GCC_ATTRIB_ALIGN(256)
{
    /* 
     * DO NOT MOVE FROM FIRST ENTRY. This parameter is accessed by assembly 
     * code that depends upon it being the first entry.
     * Save the current SP. 
     */
    LwU32 saveSp;

    /* Get the bottom of the stack from STACKCFG (in bytes) if supported */
    LwBool bStackCfgSupported;
    LwU32  stackCfgBottom;

    /* Original PLM of DMEM before lowered to level 2 */
    LwU32 falconDmemMask;
} PR_LASSAHS_DATA_HS;

/* ------------------------- Global Variables ------------------------------- */
extern PR_LASSAHS_DATA_HS g_prLassahsData_Hs;

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS prSbHsEntry(void)                                     GCC_ATTRIB_NO_STACK_PROTECT();
FLCN_STATUS prSaveStackValues(void);
FLCN_STATUS prSbHsDecryptMPK(LwU32 *pRgbModelPrivKey);
FLCN_STATUS prSbHsExit(void)                                      GCC_ATTRIB_NO_STACK_PROTECT();
void        prStkScrub(void);
LwBool      prIsImemBlockPartOfLassahsOverlays(LwU32 blockAddr);
void        prIlwalidateBlocksOutsideLassahs(void);
void        prMarkDataBlocksAsSelwre(void);
void        prUnmarkDataBlocksAsSelwre(void);

#endif // PR_LASSAHS_HS_H
