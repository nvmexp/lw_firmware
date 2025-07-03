/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_PRIV_H
#define LWOS_OVL_PRIV_H

/*!
 * @file    lwos_ovl_priv.h
 * @brief   LWOS Overlay related defines that are not part of the LWOS public
 *          interface (used only within LWOS code).
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwos_utility.h"
#include "lwtypes.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */

#ifdef DMEM_OVL_SIZE_LONG
typedef LwU32 DmemOvlSize;
#define DMEM_OVL_SIZE_LIMIT LW_U32_MAX
#else // DMEM_OVL_SIZE_LONG
typedef LwU16 DmemOvlSize;
#define DMEM_OVL_SIZE_LIMIT LW_U16_MAX
#endif // DMEM_OVL_SIZE_LONG

/*!
 * Structure tracking IMEM/DMEM overlay recycling.
 */
typedef struct
{
    LwU8    ovlIdxRecycled;
    LwU8    ovlIdxDoNotRecycle;
    LwU16   tagStart;
    LwU16   tagEnd;
} OS_TASK_MRU_RECYCLE_TRACKING;

/* ------------------------ External Definitions ---------------------------- */
/*!
 * Linker-generated symbols.
 */
extern LwU32 _num_imem_blocks[];
extern LwU32 _num_dmem_blocks[];

/*!
 * Aligned end of the resident code and start of the code overlay region.
 */
extern LwU32 _overlay_base_imem[];

/*!
 * End of the code overlay region.
 */
extern LwU32 _overlay_end_imem[];

/*!
 * Linker generated total number of IMEM overlays
 */
extern LwU32 _num_imem_overlays[];

/*!
 * Line index of _overlay_base_imem.
 */
extern LwU32 _overlay_base_imem_idx[];

/*!
 * IMEM overlay descriptors { tag, block_size }.
 */
extern LwU16 _imem_ovl_tag[];
extern LwU8  _imem_ovl_size[];
extern LwU32 _imem_ovl_end_addr[];

/*!
 * DMEM overlay descriptors { start, size_lwrrent, size_max }.
 */
extern LwUPtr      _dmem_ovl_start_address[];
extern DmemOvlSize _dmem_ovl_size_lwrrent[];
extern DmemOvlSize _dmem_ovl_size_max[];

#ifdef MRU_OVERLAYS
/*!
 * Linker-generated lists of 8-bit overlay indexes defining a doubly linked list
 * of an overlays at least partially being present in the physical memory, while
 * maintaining the order from most to least recently used ones.
 * Indexes 'zero' & 'ovl_count + 1' are not valid overlays and are being used as
 * a 'head' & 'tail' elements respectively, ensuring that routines updating such
 * list does not have to handle special cases (i.e. empty list).
 */
extern LwU8 _imem_ovl_mru_prev[];
extern LwU8 _imem_ovl_mru_next[];
#ifdef DMEM_VA_SUPPORTED
extern LwU8 _dmem_ovl_mru_prev[];
extern LwU8 _dmem_ovl_mru_next[];
#endif // DMEM_VA_SUPPORTED
#endif // MRU_OVERLAYS

#ifdef HS_OVERLAYS_ENABLED
extern LwU32 _ls_resident_end_imem[];
extern LwU8  _imem_ovl_attr_resident[];
#endif

#ifdef DMEM_VA_SUPPORTED
/*!
 * Bitmask tracking which IMEM overlays are lwrrently loaded.
 */
extern LwU32 LwosOvlImemLoaded[];
extern LwU8  _dmem_ovl_attr_resident[];
extern LwU32 _dmem_va_blocks_max[];
#endif // DMEM_VA_SUPPORTED

#ifdef HS_UCODE_ENCRYPTION
#ifdef BOOT_FROM_HS    
extern LwU16 _patched_imem_ovl_tag[];
#else
extern LwU16 _patched_dbg_imem_ovl_tag[];
extern LwU16 _patched_prd_imem_ovl_tag[];
#endif
#endif


/*!
 * Linker-generated variable defining the number of HS overlays. The HS overlays
 * are at overlay IDs 1 through num(HS overlays), if they exist. The NS/LS
 * overlays follow them in their IDs. We use this number to decide whether we
 * should set the secure bit while loading an overlay (should only be done for
 * HS overlays).
 */
extern LwU8 _num_imem_hs_overlays;

/* ------------------------ BUG Workarounds --------------------------------- */
/*!
 * @brief   Macros to operate with the DMTAG valid bit mask (bug 1845883).
 *
 * Attempt to query DMTAG that would result in miss may corrupt HW state.
 * Hence SW tracking of hit/miss was introduced for the entire VA range.
 *  If Bit = 1, then there is no tag miss
 *  If Bit = 0, then there will be a tag miss
 */
#ifdef DMTAG_WAR_1845883
#define DMTAG_WAR_1845883_VALID_SET(idx)                                       \
    LWOS_BM_SET(pOsDmtagStateMask, idx, 32)
#define DMTAG_WAR_1845883_VALID_CLR(idx)                                       \
    LWOS_BM_CLR(pOsDmtagStateMask, idx, 32)
#define DMTAG_WAR_1845883_VALID_GET(idx)                                       \
    LWOS_BM_GET(pOsDmtagStateMask, idx, 32)
extern LwU32 *pOsDmtagStateMask;
#else // DMTAG_WAR_1845883
#define DMTAG_WAR_1845883_VALID_SET(idx)    lwosNOP()
#define DMTAG_WAR_1845883_VALID_CLR(idx)    lwosNOP()
#define DMTAG_WAR_1845883_VALID_GET(idx)    LW_TRUE
#endif // DMTAG_WAR_1845883

/*!
 * @brief   falc_dmtag() has HW issue described in bug 1845883.
 *
 * @param[out]  _dmtag  Resulting DMTAG.
 * @param[in]   _vAddr  Queried virtual address.
 */
#define LWOS_DMTAG(_dmtag,_vAddr)                                              \
    do {                                                                       \
        if (!DMTAG_WAR_1845883_VALID_GET(DMEM_ADDR_TO_IDX(_vAddr)))            \
        {                                                                      \
            _dmtag = DRF_DEF(_FLCN, _DMTAG_BLK, _MISS, _YES);                  \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            falc_dmtag(&_dmtag, (_vAddr));                                     \
        }                                                                      \
    } while (LW_FALSE)

/*!
 * falc_dmread() has HW issue described in bug 200142015.
 * After reading into the DMEM line it must be set clean.
 * Using falc_setdtag() until FMODEL for falc_dmclean() is fixed.
 */
#ifdef DMREAD_WAR_200142015
#define LWOS_DMREAD_WAR_200142015(_vAddr,_lineIdx)                             \
    falc_dmwait();                                                             \
    falc_setdtag((_vAddr), (_lineIdx))
#else // DMREAD_WAR_200142015
#define LWOS_DMREAD_WAR_200142015(_vAddr,_lineIdx)  lwosNOP()
#endif // DMREAD_WAR_200142015

/*!
 * @brief   Read entire line from the VM into the DMEM.
 *
 * @param[in]   _vAddr      Source virtual address.
 * @param[in]   _lineIdx    Destination DMEM line.
 */
#define LWOS_DMREAD_LINE(_vAddr,_lineIdx)                                      \
    do {                                                                       \
        falc_dmread((_vAddr),                                                  \
                    DRF_NUM(_FLCN, _DMREAD, _PHYSICAL_ADDRESS,                 \
                            DMEM_IDX_TO_ADDR(_lineIdx))      |                 \
                    DRF_DEF(_FLCN, _DMREAD, _ENC_SIZE, _256) |                 \
                    DRF_DEF(_FLCN, _DMREAD, _SET_TAG, _YES));                  \
        LWOS_DMREAD_WAR_200142015((_vAddr), (_lineIdx));                       \
    } while (LW_FALSE)

/* ------------------------ Function Prototypes ----------------------------- */
FLCN_STATUS lwosInitOvl(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosInitOvl");
#ifdef DMEM_VA_SUPPORTED
FLCN_STATUS lwosInitOvlDmem(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosInitOvlDmem");
#endif // DMEM_VA_SUPPORTED

LwS32       lwosOvlImemLoad(LwU8 ovlIdx, LwU8 *pOvlList, LwU8 ovlCnt, LwBool bHS);
#ifdef DMEM_VA_SUPPORTED
LwU16       lwosOvlDmemFindBestBlock(void);
LwS32       lwosOvlDmemLoad(LwU8 ovlIdx, LwBool bDMA);
FLCN_STATUS lwosOvlDmemStackSetup(LwU8 ovlIdxStack, LwU16 *pStackDepth, LwUPtr **pStack)
    GCC_ATTRIB_SECTION("imem_init", "lwosOvlDmemStackSetup");

#ifdef UPROC_RISCV
FLCN_STATUS mmTaskStackSetup(LwU8 sectionIdxStack, LwU16 *pStackDepth, LwUPtr **pStack)
    GCC_ATTRIB_SECTION("imem_init", "mmTaskStackSetup");
#endif // UPROC_RISCV
#endif // DMEM_VA_SUPPORTED

#ifdef DMEM_VA_SUPPORTED
#ifdef HS_OVERLAYS_ENABLED
void        lwosOvlImemRefresh(LwU8 *pOvlList, LwU8 ovlCnt, LwBool bIntoHs);
#endif // HS_OVERLAYS_ENABLED
#endif // DMEM_VA_SUPPORTED

#ifdef MRU_OVERLAYS
void        lwosOvlMruListRemove(LwU8 ovlIdx, LwBool bDmem);
void        lwosOvlMruListMoveToFront(LwU8 ovlIdx, LwBool bDmem);
void        lwosOvlMruListMoveToFrontAll(LwU8 *pOvlList, LwU8 ovlCnt, LwBool bDmem);
#endif // MRU_OVERLAYS


#endif // LWOS_OVL_PRIV_H
