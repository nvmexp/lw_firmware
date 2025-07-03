/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PR_LASSAHS_H
#define PR_LASSAHS_H

/*!
 * @file pr_lassahs.h
 * This file contains all the defines/macros which will be used in LASSAHS at LS
 * mode.  LASSAHS = LASS As HS (LS At Same Security As HS).
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pr/pr_common.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define PR_LASSAHS_PREALLOC_MEMORY_SIZE                                  4096

/* ------------------------- Types definitions ------------------------------ */
typedef struct
{
    /* 
     * Flag indicates we are under LASSAHS mode.
     */
    LwBool       bActive;

    /* 
     * This variable holds the address of a large block of allocated memory.
     * It is allocated and freed in LS mode before jumping to HS mode to
     * ensure we have a large enough block available before entering HS mode.
     */
    void        *pPreAllocHeapAddress;
} PR_LASSAHS_DATA;

/*
 * Structure to save ilwalidated block info during LASSAHS
 */
typedef struct
{
    /*
     * Bitmap for storing imem block index of ilwalidated blocks during LASSAHS
     */
    LwU32 bitmap[8];

    /*
     * Buffer for storing addresses of ilwalidated blocks at particular index
     */
    LwU32 blkAddr[256];
} PR_ILWALIDATE_BLOCKS_DATA;

/* ------------------------- Global Variables ------------------------------- */
extern PR_LASSAHS_DATA           g_prLassahsData;
extern PR_ILWALIDATE_BLOCKS_DATA g_ilwalidatedBlocks;
extern LwU32                     _num_imem_blocks[];

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS prPreAllocMemory(LwU32 allocSize);
FLCN_STATUS prRevalidateImemBlocks(void);
void        prFreePreAllocMemory(void);
void        prLoadLassahsImemOverlays(void);  
void        prLoadLassahsDmemOverlays(void);
void        prUnloadOverlaysIlwalidatedDuringLassahs(void);

#endif // _PR_LASSAHS_H_
