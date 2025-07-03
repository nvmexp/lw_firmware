/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    odp.c
 * @brief   On-demand paging implementation
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwmisc.h>
#include <lwuproc.h>

#include <dev_top.h>
#include <riscv_csr.h>
#include <lwriscv-mpu.h>
#include <engine.h>
#include <memops.h>

#include <dmemovl.h>
#include <osptimer.h>

#if defined(INSTRUMENTATION) || defined(USTREAMER)
#include "lwrtosHooks.h"
#endif // INSTRUMENTATION || USTREAMER

/* ------------------------ SafeRTOS Includes ------------------------------ */
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>
#include <portfeatures.h>
// for pxLwrrentTCB
#include <task.h>

/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include "drivers/cache.h"
#include "drivers/drivers.h"
#include "drivers/mpu.h"
#include "drivers/odp.h"
#include "config/g_profile.h"
#include "config/g_sections_riscv.h"

/* ------------------------ Optimization settings -------------------------- */
// Set higher optimization level here because it strongly impacts performance.
#pragma GCC optimize ("O3")

/*!
 * @brief Force inlining of a function.
 *
 * lwtypes.h only defines FORCEINLINE when not in debug build.
 * For such a performance-critical module, we actually want it.
 */
#undef LW_FORCEINLINE
#define LW_FORCEINLINE __inline__ __attribute__((__always_inline__))

/* ------------------------ Constants -------------------------------------- */

#define ENGINE_DMA_BLOCK_SIZE_MAX           0x100U
#define ODP_PAGE_MAX_COUNT                  (PROFILE_ODP_DATA_PAGE_MAX_COUNT + \
                                             PROFILE_ODP_CODE_PAGE_MAX_COUNT)

/*!
 * @brief Defines for embedding the backing storage ID in the section val
 *
 * An 8-bit section val is mapped for each page VA. For each of these the high
 * bit can represent the backing storage identity for this page
 * (0 == ELF, 1 == FB). All section IDs start with _BACKING_ID == _ELF.
 * If elf-in-place ODP copy-on-write is enabled, this is switched to _FB
 * on first flush.
 */
#define LW_ODP_SECTION_VAL_INDEX            6:0
#define LW_ODP_SECTION_VAL_INDEX_ILWALID    0x7F /* 8'b0111_1111 */
#define LW_ODP_SECTION_VAL_BACKING_ID       7:7
#define LW_ODP_SECTION_VAL_BACKING_ID_ELF   0
#define LW_ODP_SECTION_VAL_BACKING_ID_FB    1


#define ODP_PAGE_DATA_SECTION_ILWALID       LW_ODP_SECTION_VAL_INDEX_ILWALID
#define ODP_PAGE_DATA_LAST_TCM_ID_ILWALID   (LW_U8_MAX)
#define ODP_MPU_TRACK_INDEX_UNSET           ((MpuTrackIndex)-1)
#define ODP_MPU_ASSOCIATED_PAGE_ILWALID     ((TcmPageIndex)-1)

#define MPU_GRANULARITY                     (LWBIT64(DRF_BASE(LW_RISCV_CSR_SMPUPA_BASE)))

enum
{
    ODP_REGION_DTCM,
    ODP_REGION_ITCM,
    ODP_REGION_COUNT,
};

/* ------------------------ Macros ----------------------------------------- */
#define ODP_VA_PAGE_IDX(_addr, _startVA, _pageShift)  (((_addr) - (_startVA)) >> (_pageShift))
#define ODP_VA_MAPPING_IDX(_addr, _mappingData) \
            ODP_VA_PAGE_IDX((_addr), (_mappingData)->startVA, (_mappingData)->pageShift)
#define ODP_RGN_VA_MAPPING_IDX(_rgn, _addr) \
            ODP_VA_MAPPING_IDX(_addr, (_rgn) ? &_odpPageVaMappingData[ODP_REGION_ITCM] : \
                                               &_odpPageVaMappingData[ODP_REGION_DTCM])

#ifdef PROFILE_ODP_MPU_SHARED_COUNT
// When sharing MPUs, the tracking structure is always the same.
# define ODP_MPU_TRACKING(_bIsItcm) (&_odpMpuTracking)
#else
// When _not_ sharing MPUs, the tracking structures are split.
# define ODP_MPU_TRACKING(_bIsItcm) (&_odpMpuTracking[_bIsItcm])
#endif

#define ODP_TCM_PAGE_FROM_INDEX(_bIsItcm, _tcmIndex) \
            (&((_bIsItcm) ? _itcmOdpPage : _dtcmOdpPage)[_tcmIndex])

#define ODP_TCM_PAGE_PHYSADDR(_pPage) \
            ((FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _REGION, _ITCM, (_pPage)->flags) ? \
                    LW_RISCV_AMAP_IMEM_START : LW_RISCV_AMAP_DMEM_START) + \
             (_pPage)->tcmPhysOffset)

/* ------------------------ Local structures and types ---------------------- */
/*!
 * @brief MPU tracking structure index
 *
 * This type is used for indices into the pEntryList array of an MPU tracking structure.
 *
 * This type must be sized to contain the maximum number of MPU entries, plus one.
 */
#ifdef PROFILE_ODP_MPU_SHARED_COUNT
# if PROFILE_ODP_MPU_SHARED_COUNT <= LW_U8_MAX
typedef LwU8 MpuTrackIndex;
# elif PROFILE_ODP_MPU_SHARED_COUNT <= LW_U16_MAX
typedef LwU16 MpuTrackIndex;
# else
#  error "Invalid configuration (MPU shared count is likely incorrect)"
# endif
#else
# if (PROFILE_ODP_MPU_DTCM_COUNT <= LW_U8_MAX) && (PROFILE_ODP_MPU_ITCM_COUNT <= LW_U8_MAX)
typedef LwU8 MpuTrackIndex;
# elif (PROFILE_ODP_MPU_DTCM_COUNT <= LW_U16_MAX) && (PROFILE_ODP_MPU_ITCM_COUNT <= LW_U16_MAX)
typedef LwU16 MpuTrackIndex;
# else
#  error "Invalid configuration (MPU count is likely incorrect)"
# endif
#endif

/*!
 * @brief TCM page tracking index
 *
 * This type is used for indices into the _tcmOdpPage array.
 *
 * This type must be sized to contain the total maximum number of TCM pages, plus one.
 */
#if ODP_PAGE_MAX_COUNT <= LW_U8_MAX
typedef LwU8 TcmPageIndex;
#elif ODP_PAGE_MAX_COUNT <= LW_U16_MAX
typedef LwU16 TcmPageIndex;
#else
# error "Invalid configuration (TCM page count is likely incorrect)"
#endif

/*!
 * @brief Hardware MPU index
 *
 * This type is used for hardware MPU indices, as written to smpuidx.
 *
 * This type must be sized to contain the total number of hardware MPU entries.
 */
#if LW_RISCV_CSR_MPU_ENTRY_COUNT <= LW_U8_MAX
typedef LwU8 MpuEntryIndex;
#elif LW_RISCV_CSR_MPU_ENTRY_COUNT <= LW_U16_MAX
typedef LwU16 MpuEntryIndex;
#else
# error "Invalid configuration (CSR MPU count is likely incorrect)"
#endif

/*!
 * @brief If true, this is an ITCM page. Otherwise, DTCM.
 *
 * Note that this should stay as bit 0!
 */
#define LW_ODP_TCM_PAGE_FLAG_REGION         0:0
#define LW_ODP_TCM_PAGE_FLAG_REGION_DTCM    0
#define LW_ODP_TCM_PAGE_FLAG_REGION_ITCM    1

/*!
 * @brief If true, this TCM page contains active section data.
 *
 * This is only cleared by ilwalidating the page.
 */
#define LW_ODP_TCM_PAGE_FLAG_VALID          1:1
#define LW_ODP_TCM_PAGE_FLAG_VALID_FALSE    0
#define LW_ODP_TCM_PAGE_FLAG_VALID_TRUE     1

//! \brief If true, this TCM page may not be recycled.
#define LW_ODP_TCM_PAGE_FLAG_PINNED         2:2
#define LW_ODP_TCM_PAGE_FLAG_PINNED_FALSE   0
#define LW_ODP_TCM_PAGE_FLAG_PINNED_TRUE    1

//! \brief If true, this (D)TCM page is dirty and needs to be flushed
#define LW_ODP_TCM_PAGE_FLAG_DIRTY          3:3
#define LW_ODP_TCM_PAGE_FLAG_DIRTY_FALSE    0
#define LW_ODP_TCM_PAGE_FLAG_DIRTY_TRUE     1

typedef struct
{
    //! @brief RISCV-PA of the first byte of the backing storage.
    LwU64 physAddr;

    /*!
     * @brief Offset into the TCM region of the first byte of the TCM page.
     *
     * Since this is 32 bits, which isn't enough to store a full TCM physical
     * address on some chips, this is an offset inside the TCM determined by
     * flags.ITCM.
     */
    LwU32 tcmPhysOffset;

    /*!
     * @brief RISCV-VA of the first byte of this page.
     *
     * While unmapped, this value should be zero.
     *
     * This is 32 bits to save memory, since our virtual memory space never exceeds 4GB.
     */
    LwU32 virtAddr;

    /*!
     * @brief Assigned MPU index.
     *
     * This is only valid if mpuTrackIdx != ODP_MPU_TRACK_INDEX_UNSET.
     */
    MpuEntryIndex mpuIdx;

    //! @brief Index to this entry in the MPU tracking table, or ODP_MPU_TRACK_INDEX_UNSET.
    MpuTrackIndex mpuTrackIdx;

    //! @brief Flags, using the LW_ODP_TCM_PAGE_FLAG DRF.
    LwU8 flags;
} ODP_TCM_PAGE;

/*!
 * @brief Structure to track MPU entries for allocation.
 *
 * When not shared, there are two of these: one for ITCM, and one for DTCM.
 */
typedef struct
{
#ifdef PROFILE_ODP_MPU_SHARED_COUNT
    //! @brief List of usable MPU entries.
    MpuEntryIndex pEntryList[PROFILE_ODP_MPU_SHARED_COUNT];
    //! @brief List of TCM pages associated to MPU entries.
    TcmPageIndex pAssocTcmPage[PROFILE_ODP_MPU_SHARED_COUNT];
#else
    //! @brief List of usable MPU entries.
    MpuEntryIndex * const pEntryList;
    //! @brief List of TCM pages associated to MPU entries.
    TcmPageIndex * const pAssocTcmPage;
#endif
    //! @brief Next index to allocate.
    LwU16 next;
    //! @brief Total number of entries.
    LwU16 limit;
} ODP_MPU_TRACK;

/*!
 * @brief The configuration data for a mapping from a VA range to
 *        some data (for example, the section idx).
 *
 * This metadata can be re-used for mapping the same VA range to different
 * sets of data. It is generic and can be used for either code or data
 * VA range mappings with different page sizes.
 */
typedef struct
{
    /*!
     * @brief   Number of mapping entries created (corresponds to number of pages
     *          that fit into the range between startVA and endVA).
     */
    LwLength        numEntries;

    //! @brief  Starting VA of the mapped region.
    LwUPtr          startVA;

    //! @brief  VA past the end of the mapped region.
    LwUPtr          endVA;

    //! @brief  Starting index of section range where paged sections are located.
    LwU8            sectionIdxFirst;

    //! @brief  Index one past the end of section range where paged sections are located.
    LwU8            sectionIdxLast;

    /*!
     * @brief   Initialized statically - start of the full section range containing the
     *          section range with paged sections.
     */
    const LwU8      sectionIdxFullFirst;

    /*!
     * @brief   Initialized statically - length of the full section range containing the
     *          section range with paged sections.
     */
    const LwU8      sectionIdxFullCount;

    /*!
     * @brief   Initialized statically - identifier for paged sections
     *          in SectionDescLocation[*]. Sections matching this identifier
     *          will be used when initializing this mapping.
     */
    const LwU8      pagedSectionLoc;

    /*!
     * @brief   log(2) of the size of pages being mapped for this section range.
     *
     * This is how many bits a VA must be shifted down to become an index.
     */
    const LwU8      pageShift;
} ODP_VA_MAPPING_METADATA;

/*!
 *  Physical Memory Base address
 *  Stores the base address of the ucode in FB/SYSMEM
 *  based on values in lw_riscv_address_map.h
 *  Defaulting to FBGPA since GSP uses odp too
 */
static LwU64 riscvPhysAddrBase;

/* ------------------------ External symbols -------------------------------- */
/*!
 * @brief Must be stored this way to work around relocation distance issues on TU10X.
 *
 * volatile is required otherwise GCC tries to inline the access (and fails).
 * This is only accessed in the initialization function.
 */
extern const LwU8 __odp_imem_base[];
extern const LwU8 __odp_dmem_base[];
static volatile const LwUPtr _dtcmBase = (LwUPtr)__odp_dmem_base;
static volatile const LwUPtr _itcmBase = (LwUPtr)__odp_imem_base;

#ifdef DMA_SUSPENSION
extern LwBool lwosDmaSuspensionNoticeISR(void);
extern void lwosDmaExitSuspension(void);
#endif // DMA_SUSPENSION

//! @brief General DMA index for all DMA operations.
extern LwU32 dmaIndex;

/* ------------------------ Local variables --------------------------------- */
//! @copydoc ODP_STATS
sysSHARED_DATA ODP_STATS riscvOdpStats = { 0 };

//! @brief Tracking structure for each TCM page that may be used.
static ODP_TCM_PAGE _tcmOdpPage[ODP_PAGE_MAX_COUNT];
//! @brief Tracking structure for each DTCM page that may be used.
#define _dtcmOdpPage _tcmOdpPage
//! @brief Tracking structure for each ITCM page that may be used.
#define _itcmOdpPage (&_dtcmOdpPage[PROFILE_ODP_DATA_PAGE_MAX_COUNT])

//! @brief Replacement page tracking index for TCM pages.
static LwU8 _tcmPageIndex[ODP_REGION_COUNT];

/*!
 * @brief Number of valid TCM pages.
 *
 * This may not reflect the profile's MAX_COUNT if not enough TCM could be reserved.
 */
static LwU8 _tcmPageCount[ODP_REGION_COUNT];

/*!
 * @brief Virtual address of last paged in page.
 *
 * This is used for handling the case where an instruction is spread across two pages.
 * It should not be relied on for any other case, as it may be cleared in some cases.
 */
static LwU64 _lastPagedVA;

/*!
 * @brief Mappings for VA page -> section idx.
 *
 * Also uses the hi-bit to map data VA page -> backing storage ID (0 == ELF, 1 == FB)
 */
static LwU8 *_odpPageVaSectionMapping[ODP_REGION_COUNT];

/*!
 * @brief Mappings for VA page -> last paged TCM PA idx
 */
static LwU8 *_odpPageVaTcmPageIdxMapping[ODP_REGION_COUNT];

//! @brief Precomputed DMA cmd for DTCM->FB DMA transfers.
static LwU32 _dmaCmdDtcmToFb = 0;

//! @brief Precomputed DMA cmd for FB->ITCM DMA transfers.
static LwU32 _dmaCmdFbToItcm = 0;

//! @brief Precomputed DMA cmd for FB->DTCM DMA transfers.
static LwU32 _dmaCmdFbToDtcm = 0;

/*!
 * @brief Metadata for VA page -> page data mappings
 */
static ODP_VA_MAPPING_METADATA _odpPageVaMappingData[ODP_REGION_COUNT] = {
    [ODP_REGION_DTCM] = {
        .sectionIdxFullFirst = UPROC_DATA_SECTION_FIRST,
        .sectionIdxFullCount = UPROC_DATA_SECTION_COUNT,
        .pagedSectionLoc     = UPROC_SECTION_LOCATION_DMEM_ODP,
        .pageShift           = BIT_IDX_32(PROFILE_ODP_DATA_PAGE_SIZE),
    },
    [ODP_REGION_ITCM] = {
        .sectionIdxFullFirst = UPROC_CODE_SECTION_FIRST,
        .sectionIdxFullCount = UPROC_CODE_SECTION_COUNT,
        .pagedSectionLoc     = UPROC_SECTION_LOCATION_IMEM_ODP,
        .pageShift           = BIT_IDX_32(PROFILE_ODP_CODE_PAGE_SIZE),
    },
};

#ifdef PROFILE_ODP_MPU_SHARED_COUNT
//! @brief Tracking structure for the shared MPU pool.
static ODP_MPU_TRACK _odpMpuTracking = {
    .next = 0,
    .limit = PROFILE_ODP_MPU_SHARED_COUNT,
};
#else
//! @brief List of usable MPU entries for TCM pages.
static MpuEntryIndex _mpuDtcmEntries[PROFILE_ODP_MPU_DTCM_COUNT];
static MpuEntryIndex _mpuItcmEntries[PROFILE_ODP_MPU_ITCM_COUNT];

//! @brief List of TCM pages associated to TCM MPU entries.
static TcmPageIndex _mpuDtcmAssoc[PROFILE_ODP_MPU_DTCM_COUNT];
static TcmPageIndex _mpuItcmAssoc[PROFILE_ODP_MPU_ITCM_COUNT];

//! @brief Tracking structure for the TCM MPU pools.
static ODP_MPU_TRACK _odpMpuTracking[ODP_REGION_COUNT] = {
    [ODP_REGION_DTCM] = {
        .pEntryList = _mpuDtcmEntries,
        .pAssocTcmPage = _mpuDtcmAssoc,
        .next = 0,
        .limit = PROFILE_ODP_MPU_DTCM_COUNT,
    },
    [ODP_REGION_ITCM] = {
        .pEntryList = _mpuItcmEntries,
        .pAssocTcmPage = _mpuItcmAssoc,
        .next = 0,
        .limit = PROFILE_ODP_MPU_ITCM_COUNT,
    },
};
#endif

#ifdef DMA_SUSPENSION
/*!
 * @brief Pointer to task which most recently had its stack pinned.
 *
 * This is used for unpinning the current task stack out on context switch.
 */
static xTCB *pLastPinnedTask = NULL;
#endif // DMA_SUSPENSION

/* ------------------------ Static prototypes ------------------------------- */
/*!
 * @brief DMA function optimized for ODP.
 *
 * This DMA function has been written to make certain assumptions
 * about how the DMAs are being done:
 * 1) We will only either do DTCM->FB, FB->DTCM or FB->ITCM transfers.
 * 2) All transfers will have source addr, destination addr and size
 *    aligned to max DMA transfer size (256 bytes).
 * 3) DMA queue is always empty/idle upon entering the DMA function.
 * 4) The ODP page size is not bigger than max DMA transfer size * queue size
 *    (i.e. any page should fit into an empty DMA queue).
 */
static FLCN_STATUS s_odpDmaXfer(LwU64  sourcePhys,
                                LwU64  destPhys,
                                LwU64  sizeBytes,
                                LwBool bDtcmToFb,
                                LwBool bIsItcm);

/*!
 * @brief Initialization function to dynamically initialize a metadata mapping.
 *        Given a pointer to a metadata struct with some pre-populated fields,
 *        it will use them to callwlate and initialize the rest of the fields.
 *
 * This function can be used to create mapping metadata for code or data VA ranges.
 * Given a range of sections, it will crop off non-paged sections at the beginning
 * and end of the range, and record the VA and section id bounds of the new cropped
 * range, as well as the number of mapping entries (which is the same as the number
 * of pages that comprise that cropped VA range).
 *
 * @param[in,out]  pMetadata  Pointer to VA mapping metadata being initialized.
 *
 * @return  LW_TRUE if the metadata has been successfully initialized, or
 *          LW_FALSE if initialization failed due to section range being empty or
 *          not containing paged sections.
 */
static LwBool s_odpInitVaMappingMetadata(ODP_VA_MAPPING_METADATA *pMetadata);

/*!
 * @brief Function to allocate and initialize a page VA->page data mapping for ODP.
 *        This function implements allocation for the mapping scheme where the
 *        array of mappings is initially filled with 0xFF to indicate invalid values
 *        (such as VMA holes).
 *
 * @param[in]  heapSectionIdx  Index of section in which the mapping can be allocated.
 * @param[in]  numEntries      Number of mapping entries to be created.
 * @param[in]  pageDataSize    Size of data that the pages are being mapped to.
 *
 * @return  the created mapping, or NULL on failure.
 */
static void  *s_odpAllocVaMapping(const LwU8     heapSectionIdx,
                                  const LwLength numEntries,
                                  const LwLength pageDataSize);

/*!
 * @brief Function to initialize/populate a page VA->section id mapping for ODP.
 *        This function uses a pre-populated VA mapping metadata to initialize
 *        a mapping pre-allocated via s_odpAllocVaMapping.
 *
 * Using the "trimmed" section range and VA range from the mapping metadata,
 * this function maps page VAs to the section IDs (or to VMA holes, identified
 * by the section data specifying ODP_SECTION_INDEX_ILWALID as the section index;
 * this also identifies non-paged sections located inside the range, if there are any).
 *
 * Because of this, to get the most optimal mapping, it is recommended to avoid
 * interspersing paged and non-paged sections.
 *
 * @param[in]  pPageVaSectionMapping  The pre-allocated mapping to populate.
 * @param[in]  pMetadata              Pointer (const) to the metadata indicating
 *                                    the mapping's section range, VA range, page size,
 *                                    and paged section identifier.
 *
 * @return  LW_TRUE if successful, LW_FALSE if sections pointed to by the metadata
 *          can't be mapped correctly (for example if some sections are misaligned).
 */
static LwBool s_odpPopulateVaToSectionMapping(LwU8                    *      pPageVaSectionMapping,
                                              ODP_VA_MAPPING_METADATA *const pMetadata);

/*!
 * @brief Look up the last TCM physical address a given virtual address
 *        has last been mapped to.
 *
 * @param[in]  addr     The VA to find the last mapped TCM PA for.
 * @param[in]  bIsItcm  LW_TRUE if address is in a code page, LW_FALSE otherwise.
 *
 * @return  the index of the last mapped TCM PA in _dtcmOdpPage or _itcmOdpPage,
 *          or -1 for addresses in VMA holes or not in paged TCM.
 */
static LwS64 s_odpVirtAddrGetLastTcmPageIdx(LwUPtr virtAddr, LwBool bIsItcm);

/*!
 * @brief Look up the last TCM physical address a given virtual address
 *        has last been mapped to.
 *
 * @param[in]  addr        The VA to find the last mapped TCM PA for.
 * @param[in]  tcmPageIdx  The index in _dtcmOdpPage or _itcmOdpPage to map.
 * @param[in]  bIsItcm     LW_TRUE if address is in a code page, LW_FALSE otherwise.
 */
static void  s_odpVirtAddrSetLastTcmPageIdx(LwUPtr virtAddr, LwU8 tcmPageIdx, LwBool bIsItcm);

/*!
 * @brief Look up information about a virtual address.
 *
 * If any ODP miss is serviced between this call and its usage, the results are not guaranteed
 * to be correct.
 *
 * @param[in]   addr        The VA to find the information for.
 * @param[out]  ppPage      Filled with a pointer to the TCM page's tracking structure.
 * @param[out]  pSection    Filled with the section index of the VA.
 *
 * @return FLCN_OK if the output variables were filled, otherwise an error code.
 */
static FLCN_STATUS s_odpVirtAddrToPage(LwUPtr addr, ODP_TCM_PAGE **ppPage, LwS64 *pSection);

/*!
 * @brief Look up the section idx for a given address in O(1) time.
 *
 * @param[in]  addr                 The VA to find the section for.
 * @param[in]  hiBitFlag            Pointer to a value for the hi-bit flag
 *                                  (used as the backing storage ID), not used if NULL.
 * @param[in]  setBitFlagInMapping  If LW_TRUE, we use the value pointed to by hiBitFlag
 *                                  to update the value in the mapping. Otherwise we fetch
 *                                  the in-mapping value into hiBitFlag.
 *
 *
 * @return the section ID, or -1 for addresses in VMA holes or not in paged TCM.
 */
static LwS64 s_odpVirtAddrToSection(LwUPtr addr, LwBool *hiBitFlag, LwBool setBitFlagInMapping);

/*!
 * @brief Initialize MPU tracking structure.
 *
 * @param[inout]    pMpuTrack       MPU tracking structure to finish initializing.
 * @param[inout]    pPrevMpuEntry   First MPU entry to allocate. This is filled with the last used
 *                                  MPU entry if the initialization succeeded.
 *
 * @return True if the initializiation succeeded.
 */
static LwBool s_odpMpuInitialize(ODP_MPU_TRACK *pMpuTrack, LwU64 *pPrevMpuEntry);

/*!
 * @brief Unhook an MPU entry from an ODP page, allowing it to be reassigned to another page.
 *
 * @param[inout]    pMpuTrack   Relevant MPU tracking structure.
 * @param[inout]    idx         Index of MPU entry to unhook.
 */
static void s_odpMpuPageUnhook(ODP_MPU_TRACK *pMpuTrack, LwU64 idx);

/*!
 * @brief Allocate an MPU entry for an ODP page.
 *
 * @param[inout]    pMpuTrack   Relevant MPU tracking structure.
 * @param[inout]    pPage       Page to assign the allocation to.
 */
static void s_odpMpuAllocate(ODP_MPU_TRACK *pMpuTrack, ODP_TCM_PAGE *pPage);

/*!
 * @brief Free an MPU entry from an ODP page.
 *
 * @param[inout]    pMpuTrack   Relevant MPU tracking structure.
 * @param[inout]    pPage       Page from which to free the MPU entry.
 */
static void s_odpMpuFree(ODP_MPU_TRACK *pMpuTrack, ODP_TCM_PAGE *pPage);

/*!
 * @brief Check if an ODP page is dirty.
 *
 * A TCM page can either be explicitly marked dirty, or,
 * if it has an associated MPU entry, its dirty bit is checked.
 *
 * @param[inout]    pPage       Page that we do the is-dirty check for.
 */
static LwBool s_odpIsPageDirty(ODP_TCM_PAGE *pPage);

/*!
 * @brief Reset an ODP page's dirty state.
 *
 * Explicitly mark the TCM page as not-dirty, and, if it has an associated
 * MPU entry, clean that MPU entry's dirty bit.
 *
 * @param[inout]    pPage       Page that we reset the dirty state for.
 */
static void s_odpPageDirtyReset(ODP_TCM_PAGE *pPage);

/*!
 * @brief Flush an ODP page to backing storage.
 *
 * This should only be called on a mapped page.
 * This should only be called under critical section.
 *
 * @param[inout]    pPage       Page to flush.
 * @return Error code on failure, otherwise FLCN_OK.
 */
static FLCN_STATUS s_odpFlushPage(ODP_TCM_PAGE *pPage);

/*!
 * @brief Ilwalidate an ODP page.
 *
 * This should only be called on a mapped page.
 * This should only be called under critical section.
 *
 * @param[inout]    pPage       Page to ilwalidate.
 */
static void s_odpIlwalidatePage(ODP_TCM_PAGE *pPage);

/*!
 * @brief Prepare an ODP page for recycling.
 *
 * This performs the necessary processing on a page to allow it to be reused
 * for a different page. This is essentially a special case optimized flush and ilwalidate.
 *
 * @param[inout]    pPage       Page to ilwalidate.
 * @return Error code on failure, otherwise FLCN_OK.
 */
static FLCN_STATUS s_odpRecyclePage(ODP_TCM_PAGE *pPage);

/*!
 * @brief Allocate a TCM page index
 *
 * Only allocates an index, actual TCM page reservation is done elsewhere.
 *
 * @param[in]   bIsItcm     True if the TCM page to allocate is ITCM, otherwise DTCM.
 *
 * @return The TCM page index to be used.
 */
static LwU64 s_odpPolicyNextTcmEntry(LwBool bIsItcm);

/*!
 * @brief Allocate a MPU index
 *
 * Only allocates an index, actual MPU reservation is done elsewhere.
 *
 * @param[inout]    pMpuTrack   MPU tracking structure from which to allocate.
 *
 * @return The MPU index to be used.
 */
static LwU64 s_odpPolicyNextMpuEntry(ODP_MPU_TRACK *pMpuTrack);

/* ------------------------ Static assertions ------------------------------- */
#if PROFILE_ODP_DATA_PAGE_SIZE % ENGINE_DMA_BLOCK_SIZE_MAX != 0
# error "PROFILE_ODP_DATA_PAGE_SIZE should  be aligned to max DMA transfer size!"
#endif // if PROFILE_ODP_DATA_PAGE_SIZE % ENGINE_DMA_BLOCK_SIZE_MAX != 0

#if PROFILE_ODP_CODE_PAGE_SIZE % ENGINE_DMA_BLOCK_SIZE_MAX != 0
# error "PROFILE_ODP_CODE_PAGE_SIZE should  be aligned to max DMA transfer size!"
#endif // if PROFILE_ODP_CODE_PAGE_SIZE % ENGINE_DMA_BLOCK_SIZE_MAX != 0


/*!
 * @brief Verify the sections can be handled by the VA mappings.
 *        We use >= instead of > because ODP_PAGE_DATA_SECTION_ILWALID
 *        is reserved for holes!
 */
#if UPROC_CODE_SECTION_COUNT >= ODP_PAGE_DATA_SECTION_ILWALID
# error "Unable to create VA to section mapping, too many sections!"
#endif // if UPROC_SECTION_COUNT >= ODP_PAGE_DATA_SECTION_ILWALID

#if UPROC_DATA_SECTION_COUNT >= ODP_PAGE_DATA_SECTION_ILWALID
# error "Unable to create VA to section mapping, too many sections!"
#endif // if UPROC_SECTION_COUNT >= ODP_PAGE_DATA_SECTION_ILWALID


/*!
 * @brief Verify the TCM PAs can be handled by the VA mappings
 */
#if PROFILE_ODP_CODE_PAGE_MAX_COUNT >= ODP_PAGE_DATA_LAST_TCM_ID_ILWALID
# error "Unable to create VA to last TCM PA mapping, too many code TCM pages!"
#endif // if UPROC_SECTION_COUNT >= ODP_PAGE_DATA_LAST_TCM_ID_ILWALID

#if PROFILE_ODP_DATA_PAGE_MAX_COUNT >= ODP_PAGE_DATA_LAST_TCM_ID_ILWALID
# error "Unable to create VA to last TCM PA mapping, too many data TCM pages!"
#endif // if UPROC_SECTION_COUNT >= ODP_PAGE_DATA_LAST_TCM_ID_ILWALID

// Ensure this doesn't change without our assumptions around it also changing!
ct_assert(ODP_REGION_COUNT == 2);

//-------------------------------------------------------------------------------------------------
LwBool odpInit(LwU8 heapSectionIdx, LwU64 baseAddr)
{
    LwUPtr dmemBase = _dtcmBase;
    LwUPtr imemBase = _itcmBase;
    LwUPtr dmemEnd = DtcmRegionStart + DtcmRegionSize;
    LwUPtr imemEnd = ItcmRegionStart + ItcmRegionSize;

    if (!LW_IS_ALIGNED64(dmemBase, MPU_GRANULARITY))
    {
        dbgPrintf(LEVEL_ERROR,
                    "ODP init failed: ODP DMEM region start (0x%llX) not aligned to MPU granularity (0x%llX)!\n",
                    dmemBase, MPU_GRANULARITY);
        return LW_FALSE;
    }

    if (!LW_IS_ALIGNED64(imemBase, MPU_GRANULARITY))
    {
        dbgPrintf(LEVEL_ERROR,
                    "ODP init failed: ODP IMEM region start (0x%llX) not aligned to MPU granularity (0x%llX)!\n",
                    imemBase, MPU_GRANULARITY);
        return LW_FALSE;
    }

    // Store the RISCV physical base address
    riscvPhysAddrBase = baseAddr;

    // Verify that the DMA queue can fit an ODP page
    LwU32 dmaQueueSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG, _DMAQUEUE_DEPTH,
        intioRead(LW_PRGNLCL_FALCON_HWCFG));

    if (PROFILE_ODP_DATA_PAGE_SIZE > (dmaQueueSize * ENGINE_DMA_BLOCK_SIZE_MAX))
    {
        dbgPrintf(LEVEL_ERROR,
                  "ODP init failed: PROFILE_ODP_DATA_PAGE_SIZE (0x%lX) does not fit into DMA queue (size %d * 0x%X)!\n",
                  PROFILE_ODP_DATA_PAGE_SIZE, dmaQueueSize, ENGINE_DMA_BLOCK_SIZE_MAX);
        return LW_FALSE;
    }

    if (PROFILE_ODP_CODE_PAGE_SIZE > (dmaQueueSize * ENGINE_DMA_BLOCK_SIZE_MAX))
    {
        dbgPrintf(LEVEL_ERROR,
                  "ODP init failed: PROFILE_ODP_CODE_PAGE_SIZE (0x%lX) does not fit into DMA queue (size %d * 0x%X)!\n",
                  PROFILE_ODP_CODE_PAGE_SIZE, dmaQueueSize, ENGINE_DMA_BLOCK_SIZE_MAX);
        return LW_FALSE;
    }

    _lastPagedVA = 0;

    // Precompute dmaCmd-s for fast ITCM and DTCM DMA transfers
    _dmaCmdDtcmToFb = 0;
    _dmaCmdDtcmToFb = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE, _dmaCmdDtcmToFb);
    _dmaCmdDtcmToFb = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE, _dmaCmdDtcmToFb);
    _dmaCmdDtcmToFb = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dmaIndex, _dmaCmdDtcmToFb);
    _dmaCmdDtcmToFb = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, _256B, _dmaCmdDtcmToFb);

    _dmaCmdFbToItcm = 0;
    _dmaCmdFbToItcm = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _TRUE, _dmaCmdFbToItcm);
    _dmaCmdFbToItcm = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _FALSE, _dmaCmdFbToItcm);
    _dmaCmdFbToItcm = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dmaIndex, _dmaCmdFbToItcm);
    _dmaCmdFbToItcm = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, _256B, _dmaCmdFbToItcm);

    _dmaCmdFbToDtcm = 0;
    _dmaCmdFbToDtcm = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE, _dmaCmdFbToDtcm);
    _dmaCmdFbToDtcm = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _FALSE, _dmaCmdFbToDtcm);
    _dmaCmdFbToDtcm = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dmaIndex, _dmaCmdFbToDtcm);
    _dmaCmdFbToDtcm = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, _256B, _dmaCmdFbToDtcm);

    // Initialize an ODP statistics (non-zero fields only).
    for (LwU64 i = 0; i < ODP_STATS_ENTRY_ID__CAPTURE_TIME; i++)
    {
        riscvOdpStats.timeMin[i] = LW_U32_MAX;
    }

    // Initialize tracking structures
    for (LwU64 i = 0; i < ODP_REGION_COUNT; i++)
    {
        _tcmPageCount[i] = 0;
        _tcmPageIndex[i] = 0;
        if (!s_odpInitVaMappingMetadata(&_odpPageVaMappingData[i]))
        {
            dbgPrintf(LEVEL_ERROR,
                      "ODP init failed: couldn't init section mapping metadata for %llu!\n",
                      i);
            return LW_FALSE;
        }

        // Create page VA mapping to corresponding sections
        _odpPageVaSectionMapping[i] = s_odpAllocVaMapping(heapSectionIdx,
                                                          _odpPageVaMappingData[i].numEntries,
                                                          sizeof(LwU8));
        if (_odpPageVaSectionMapping[i] == NULL)
        {
            dbgPrintf(LEVEL_ERROR,
                      "ODP init failed: couldn't alloc page section mapping for %llu!\n",
                      i);
            return LW_FALSE;
        }

        if (!s_odpPopulateVaToSectionMapping(_odpPageVaSectionMapping[i],
                                             &_odpPageVaMappingData[i]))
        {
            dbgPrintf(LEVEL_ERROR,
                      "ODP init failed: couldn't populate page section mapping for %llu!\n",
                      i);
            return LW_FALSE;
        }

        // Create VA mapping to the last-paged TCM PA
        _odpPageVaTcmPageIdxMapping[i] = s_odpAllocVaMapping(heapSectionIdx,
                                                             _odpPageVaMappingData[i].numEntries,
                                                             sizeof(LwU8));
        if (_odpPageVaTcmPageIdxMapping[i] == NULL)
        {
            dbgPrintf(LEVEL_ERROR,
                      "ODP init failed: couldn't alloc code page last-TCM-PA mapping!\n");
            return LW_FALSE;
        }
    }

    // Initialize page tracking structures.
    memset(_tcmOdpPage, 0, sizeof(_tcmOdpPage));

    for (LwU64 i = 0; (i < PROFILE_ODP_DATA_PAGE_MAX_COUNT) &&
                      (dmemBase + PROFILE_ODP_DATA_PAGE_SIZE <= dmemEnd); i++)
    {
        _dtcmOdpPage[i].mpuTrackIdx = ODP_MPU_TRACK_INDEX_UNSET;
        _dtcmOdpPage[i].tcmPhysOffset = dmemBase - LW_RISCV_AMAP_DMEM_START;
        _dtcmOdpPage[i].flags = DRF_DEF(_ODP, _TCM_PAGE_FLAG, _REGION, _DTCM);

        dmemBase += PROFILE_ODP_DATA_PAGE_SIZE;
        _tcmPageCount[ODP_REGION_DTCM]++;
    }

    for (LwU64 i = 0; (i < PROFILE_ODP_CODE_PAGE_MAX_COUNT) &&
                      (imemBase + PROFILE_ODP_CODE_PAGE_SIZE <= imemEnd); i++)
    {
        _itcmOdpPage[i].mpuTrackIdx = ODP_MPU_TRACK_INDEX_UNSET;
        _itcmOdpPage[i].tcmPhysOffset = imemBase - LW_RISCV_AMAP_IMEM_START;
        _itcmOdpPage[i].flags = DRF_DEF(_ODP, _TCM_PAGE_FLAG, _REGION, _ITCM);

        imemBase += PROFILE_ODP_CODE_PAGE_SIZE;
        _tcmPageCount[ODP_REGION_ITCM]++;
    }

    // Initialize MPU tracking structures.
    {
#ifdef PROFILE_ODP_MPU_FIRST
        LwU64 prevMpuEntry = PROFILE_ODP_MPU_FIRST;
#else
        LwU64 prevMpuEntry = 0;
#endif

#ifdef PROFILE_ODP_MPU_SHARED_COUNT
        // We must restrict the total number of MPU entries to <= total TCM entries
        LwU64 tcmTotal = _tcmPageCount[ODP_REGION_DTCM] + _tcmPageCount[ODP_REGION_ITCM];
        if (_odpMpuTracking.limit > tcmTotal)
        {
            _odpMpuTracking.limit = tcmTotal;
        }

        if (!s_odpMpuInitialize(&_odpMpuTracking, &prevMpuEntry))
        {
            dbgPrintf(LEVEL_ERROR, "ODP init failed: couldn't initialize MPU\n");
            return LW_FALSE;
        }
#else
        for (LwU64 i = 0; i < ODP_REGION_COUNT; i++)
        {
            // We must restrict the total number of MPU entries to <= total TCM entries
            if (_odpMpuTracking[i].limit > _tcmPageCount[i])
            {
                _odpMpuTracking[i].limit = _tcmPageCount[i];
            }

            if (!s_odpMpuInitialize(&_odpMpuTracking[i], &prevMpuEntry))
            {
                dbgPrintf(LEVEL_ERROR,
                          "ODP init failed: couldn't initialize MPU for %llu!\n",
                          i);
                return LW_FALSE;
            }
        }
#endif
    }

    dbgPrintf(LEVEL_INFO, "ODP initialized:\n"
           "- TCM pages: %d DTCM, %d ITCM\n"
#if PROFILE_ODP_MPU_SHARED_COUNT
           "- MPU: %d shared\n"
#else
           "- MPU: %d data, %d code\n"
#endif
           "- VA pages: %llu data, %llu code\n",
           _tcmPageCount[ODP_REGION_DTCM], _tcmPageCount[ODP_REGION_ITCM],
#if PROFILE_ODP_MPU_SHARED_COUNT
           _odpMpuTracking.limit,
#else
           _odpMpuTracking[ODP_REGION_DTCM].limit, _odpMpuTracking[ODP_REGION_ITCM].limit,
#endif
           _odpPageVaMappingData[ODP_REGION_DTCM].numEntries,
           _odpPageVaMappingData[ODP_REGION_ITCM].numEntries);
    return (_tcmPageCount[ODP_REGION_DTCM] > 0) && (_tcmPageCount[ODP_REGION_ITCM] > 0);
}

//-------------------------------------------------------------------------------------------------
void odpFlushAll(void)
{
    lwrtosENTER_CRITICAL();

    // Walk all ODP pages
    ODP_TCM_PAGE *pPage = _tcmOdpPage;
    for (LwU64 i = 0; i < ODP_PAGE_MAX_COUNT; i++, pPage++)
    {
        if (FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _VALID, _FALSE, pPage->flags))
        {
            continue;
        }
        s_odpFlushPage(pPage);
    }

    // This tracking may no longer be valid, clear it.
    _lastPagedVA = 0;

    lwrtosEXIT_CRITICAL();
}

void odpFlushIlwalidateAll(void)
{
    lwrtosENTER_CRITICAL();

    // Walk all ODP pages
    ODP_TCM_PAGE *pPage = _tcmOdpPage;
    for (LwU64 i = 0; i < ODP_PAGE_MAX_COUNT; i++, pPage++)
    {
        if (FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _VALID, _FALSE, pPage->flags))
        {
            continue;
        }
        s_odpFlushPage(pPage);
        s_odpIlwalidatePage(pPage);
    }

    // This tracking may no longer be valid, clear it.
    _lastPagedVA = 0;

    lwrtosEXIT_CRITICAL();
}

void odpFlushRange(void *pPtr, LwLength length)
{
    lwrtosENTER_CRITICAL();

    LwUPtr addrStart = (LwUPtr)pPtr;
    LwUPtr addrEnd = addrStart + length;

    // Walk all ODP pages
    ODP_TCM_PAGE *pPage = _tcmOdpPage;
    for (LwU64 i = 0; i < ODP_PAGE_MAX_COUNT; i++, pPage++)
    {
        if ((pPage->virtAddr >= addrStart) &&
            (pPage->virtAddr < addrEnd))
        {
            s_odpFlushPage(pPage);
        }
    }

    // This tracking may no longer be valid, clear it.
    _lastPagedVA = 0;

    lwrtosEXIT_CRITICAL();
}

void odpFlushIlwalidateRange(void *pPtr, LwLength length)
{
    lwrtosENTER_CRITICAL();

    LwUPtr addrStart = (LwUPtr)pPtr;
    LwUPtr addrEnd = addrStart + length;

    // Walk all ODP pages
    ODP_TCM_PAGE *pPage = _tcmOdpPage;
    for (LwU64 i = 0; i < ODP_PAGE_MAX_COUNT; i++, pPage++)
    {
        if ((pPage->virtAddr >= addrStart) &&
            (pPage->virtAddr < addrEnd))
        {
            s_odpFlushPage(pPage);
            s_odpIlwalidatePage(pPage);
        }
    }

    // This tracking may no longer be valid, clear it.
    _lastPagedVA = 0;

    lwrtosEXIT_CRITICAL();
}

//-------------------------------------------------------------------------------------------------
FLCN_STATUS odpPin(const void *pBase, LwLength length)
{
    LwUPtr          virtAddr = (LwUPtr)pBase;
    LwLength        remaining = length;
    ODP_TCM_PAGE   *pPage;
    LwS64           index;
    FLCN_STATUS     status;
    LwBool          bLastByte = LW_FALSE;
    LwLength        pageSize = 0;

    index = s_odpVirtAddrToSection(virtAddr, NULL, LW_FALSE);
    if (index < 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (SectionDescLocation[index])
    {
        case UPROC_SECTION_LOCATION_DMEM_ODP:
            pageSize = PROFILE_ODP_DATA_PAGE_SIZE;
            break;
        case UPROC_SECTION_LOCATION_IMEM_ODP:
            pageSize = PROFILE_ODP_CODE_PAGE_SIZE;
            break;

        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Iterate the whole memory region
    while (remaining > 0)
    {
        if ((status = s_odpVirtAddrToPage(virtAddr, &pPage, &index)) != FLCN_OK)
        {
            goto odpPin_error;
        }

        if (pPage == NULL)
        {
#ifdef DMA_SUSPENSION
            if (lwosDmaSuspensionNoticeISR())
            {
                status = FLCN_ERR_DMA_SUSPENDED;
                goto odpPin_error;
            }
#endif // DMA_SUSPENSION
            // Bring the address into TCM
            if (!odpIsrHandler(virtAddr))
            {
                // Couldn't load in the page!
                status = FLCN_ERROR;
                goto odpPin_error;
            }

            // Try to get the page pointer again
            if ((status = s_odpVirtAddrToPage(virtAddr, &pPage, &index)) != FLCN_OK)
            {
                goto odpPin_error;
            }

            //
            // This should not happen because odpIsrHandler should've brought the page in.
            // Better safe than sorry.
            //
            if (pPage == NULL)
            {
                // Page is refusing to be paged in, something must be wrong.
                status = FLCN_ERROR;
                goto odpPin_error;
            }
        }

        if (!bLastByte && FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _PINNED, _TRUE, pPage->flags))
        {
            // If the page was already pinned, parameters are overlapping an already pinned area.
            status = FLCN_ERR_ILLEGAL_OPERATION;
            goto odpPin_error;
        }

        // Mark the page as pinned
        pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _PINNED, _TRUE, pPage->flags);

        //
        // We will want to make sure that the last byte is also pinned (to avoid problems with
        // memory areas straddling multiple pages unaligned.)
        //
        if (pageSize > remaining)
        {
            pageSize = remaining - 1;
            bLastByte = LW_TRUE;
            if (pageSize == 0)
            {
                // This is the case after pinning the last byte.
                break;
            }
        }

        // Advance to the next page
        remaining -= pageSize;
        virtAddr += pageSize;
    }

    return FLCN_OK;

odpPin_error:
    // Unwind any pinning that oclwrred prior to this failure.
    if (remaining != length)
    {
        odpUnpin(pBase, length - remaining);
    }
    return status;
}

FLCN_STATUS odpUnpin(const void *pBase, LwLength length)
{
    LwUPtr          virtAddr = (LwUPtr)pBase;
    LwLength        remaining = length;
    ODP_TCM_PAGE   *pPage;
    LwS64           index;
    FLCN_STATUS     status;
    LwBool          bLastByte = LW_FALSE;
    LwLength        pageSize = 0;

    index = s_odpVirtAddrToSection(virtAddr, NULL, LW_FALSE);
    if (index < 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (SectionDescLocation[index])
    {
        case UPROC_SECTION_LOCATION_DMEM_ODP:
            pageSize = PROFILE_ODP_DATA_PAGE_SIZE;
            break;
        case UPROC_SECTION_LOCATION_IMEM_ODP:
            pageSize = PROFILE_ODP_CODE_PAGE_SIZE;
            break;

        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Iterate the whole memory region
    while (remaining > 0)
    {
        if ((status = s_odpVirtAddrToPage(virtAddr, &pPage, &index)) != FLCN_OK)
        {
            return status;
        }

        if (pPage == NULL)
        {
            // If the page was not present, parameters are bad.
            return FLCN_ERR_ILLEGAL_OPERATION;
        }

        if (!bLastByte && FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _PINNED, _FALSE, pPage->flags))
        {
            // If the page was already pinned, parameters are bad.
            return FLCN_ERR_ILLEGAL_OPERATION;
        }

        // Unmark the page as pinned.
        pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _PINNED, _FALSE, pPage->flags);

        //
        // We will want to make sure that the last byte is also unpinned (to avoid problems with
        // memory areas straddling multiple pages unaligned.)
        //
        if (pageSize > remaining)
        {
            pageSize = remaining - 1;
            bLastByte = LW_TRUE;
            if (pageSize == 0)
            {
                // This is the case after pinning the last byte.
                break;
            }
        }

        // Advance to the next page
        remaining -= pageSize;
        virtAddr += pageSize;
    }

    return FLCN_OK;
}

//-------------------------------------------------------------------------------------------------
LwBool odpVirtToPhys(const void *pVirt, LwU64 *pPhys, LwU64 *pBytesRemaining)
{
    LwUPtr          virtAddr = (LwUPtr)pVirt;
    ODP_TCM_PAGE   *pPage;
    LwS64           index;
    FLCN_STATUS     status;

    if ((status = s_odpVirtAddrToPage(virtAddr, &pPage, &index)) != FLCN_OK)
    {
        return LW_FALSE;
    }

    LwLength pageSize = (SectionDescLocation[index] == UPROC_SECTION_LOCATION_IMEM_ODP) ?
                            PROFILE_ODP_CODE_PAGE_SIZE : PROFILE_ODP_DATA_PAGE_SIZE;

    if (pPage == NULL)
    {
        // Not in TCM, error out
        return LW_FALSE;
    }
    else
    {
        LwUPtr pageOffset = virtAddr - pPage->virtAddr;
        if (pPhys != NULL)
        {
            *pPhys = pageOffset + ODP_TCM_PAGE_PHYSADDR(pPage);
        }
        if (pBytesRemaining != NULL)
        {
            *pBytesRemaining = pageSize - pageOffset;
        }

        //
        // Assume that whoever obtains the PA to this page can do things to it
        // outside the MPU's control, so mark it as dirty just in case.
        //
        pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _DIRTY, _TRUE, pPage->flags);

        return LW_TRUE;
    }
}

//-------------------------------------------------------------------------------------------------
/*!
 * odpIsrHandler, despite the name, can actually be called from non-pagefault ISRs (page pinning
 * on context switches), and it can also be called in syscall call chains (i.e. bringing in a
 * buffer page for dmaMemTransfer).
 */
LwBool odpIsrHandler(LwUPtr virtAddr)
{
    ODP_TCM_PAGE   *pPage;
    LwU64           pageSize;
    LwS64           tcmIndex;
    FLCN_STATUS     status;
    LwBool          bIsItcm;
    LwUPtr          backingStorageAddr;
    LwBool          bIsBackingRegionFb = LW_TRUE;
    LwU64           statsEntryId = ODP_STATS_ENTRY_ID__COUNT;
    LwU64           statsTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);

    // Find section for the virtual address
    LwS64 index = s_odpVirtAddrToSection(virtAddr, &bIsBackingRegionFb, LW_FALSE);

    // If no section was found, this isn't an ODP fault
    if (index < 0)
    {
        dbgPrintf(LEVEL_CRIT, "Not ODP section, maybe invalid pointer or permissions fault.\n");
        return LW_FALSE;
    }

    switch (SectionDescLocation[index])
    {
        case UPROC_SECTION_LOCATION_DMEM_ODP:
            bIsItcm = LW_FALSE;
            statsEntryId = ODP_STATS_ENTRY_ID_DATA_MISS_CLEAN;
#if defined(INSTRUMENTATION) || defined(USTREAMER)
            lwrtosHookOdpInstrumentationEvent(bIsItcm, LW_TRUE);
#endif // INSTRUMENTATION || USTREAMER

            pageSize = PROFILE_ODP_DATA_PAGE_SIZE;
            // Mask away the offset within the ODP page
            virtAddr = LW_ALIGN_DOWN64(virtAddr, pageSize);

            // Data is never allowed to be unaligned, thus we don't have to care about page
            // straddling like code does.
            break;
        case UPROC_SECTION_LOCATION_IMEM_ODP:
            bIsItcm = LW_TRUE;
            statsEntryId = ODP_STATS_ENTRY_ID_CODE_MISS;
#if defined(INSTRUMENTATION) || defined(USTREAMER)
            lwrtosHookOdpInstrumentationEvent(bIsItcm, LW_TRUE);
#endif // INSTRUMENTATION || USTREAMER

            pageSize = PROFILE_ODP_CODE_PAGE_SIZE;
            // Mask away the offset within the ODP page
            virtAddr = LW_ALIGN_DOWN64(virtAddr, pageSize);

            // We may have instructions straddling two pages, and so we need to check if this page
            // was the most recently paged in. If it is, we want to page in _the next page_ instead.
            // If this causes an eviction of the first page, this is fine as we'll re-enter this
            // trap and it'll eventually shake out.
            if (virtAddr == _lastPagedVA)
            {
                // Page already existed! Request the next page instead.
                virtAddr += pageSize;
            }
            break;
        default:
            // This isn't a pageable section, some other failure must've happened?
            dbgPrintf(LEVEL_CRIT, "ODP section (idx %lld) not paged type\n",
                      index);
            return LW_FALSE;
    }

    // We only need to allocate a new TCM page if the page isn't already present.
    tcmIndex = s_odpVirtAddrGetLastTcmPageIdx(virtAddr, bIsItcm);
    if (tcmIndex >= 0)
    {
        statsEntryId = ODP_STATS_ENTRY_ID_MPU_MISS;

        // Page was already loaded, so we don't need to do anything complicated
        pPage = ODP_TCM_PAGE_FROM_INDEX(bIsItcm, tcmIndex);

        if (pPage->virtAddr != virtAddr)
        {
            dbgPrintf(LEVEL_CRIT, "VA 0x%llx mapped to wrong page (tracked as 0x%llx)\n", virtAddr, (LwUPtr)pPage->virtAddr);
            return LW_FALSE;
        }
    }
    else
    {
#ifdef DMA_SUSPENSION
        //
        // If we're in MSCG and we need to DMA, we should leave before touching structures.
        // Otherwise, we may end up ruining our state and causing problems later.
        //
        if (lwosDmaSuspensionNoticeISR())
        {
            statsEntryId = ODP_STATS_ENTRY_ID_DMA_SUSPENDED;
            portYIELD_IMMEDIATE();
            goto odpIsrHandler_exit_success;
        }
#endif // DMA_SUSPENSION

        // Allocate TCM page
        tcmIndex = s_odpPolicyNextTcmEntry(bIsItcm);
        pPage = ODP_TCM_PAGE_FROM_INDEX(bIsItcm, tcmIndex);

        if (FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _VALID, _TRUE, pPage->flags))
        {
            if (!bIsItcm && s_odpIsPageDirty(pPage))
            {
                //
                // Separate statistics when page selected for the replacement
                // is drity from the case when write-back is not required.
                //
                statsEntryId = ODP_STATS_ENTRY_ID_DATA_MISS_DIRTY;
            }
            status = s_odpRecyclePage(pPage);
            if (status != FLCN_OK)
            {
                // Recycle failed, abort
                dbgPrintf(LEVEL_CRIT, "Failed to recycle ODP TCM page %lld\n",
                          index);
                return LW_FALSE;
            }
        }

#if ELF_IN_PLACE_FULL_ODP_COW
        backingStorageAddr = !bIsItcm && !bIsBackingRegionFb ? SectionDescStartDataInElf[index] : SectionDescStartPhysical[index];
#else // ELF_IN_PLACE_FULL_ODP_COW
        backingStorageAddr = SectionDescStartPhysical[index];
#endif // ELF_IN_PLACE_FULL_ODP_COW

        // It is ok for this to be here even if we fail out, as we'll ilwalidate the page.
        pPage->physAddr = virtAddr - SectionDescStartVirtual[index] + backingStorageAddr;

        // DMA FB->TCM
        status = s_odpDmaXfer(pPage->physAddr,
                              pPage->tcmPhysOffset,
                              pageSize,
                              LW_FALSE,
                              bIsItcm);
        if (status != FLCN_OK)
        {
            // DMA transfer failed, ilwalidate the broken page and abort
            s_odpIlwalidatePage(pPage);

            dbgPrintf(LEVEL_CRIT, "ODP DMA failed: 0x%X\n", status);
            return LW_FALSE;
        }

        // These must be written only after we're sure we can do the mapping
        pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _VALID, _TRUE, pPage->flags);
        pPage->virtAddr = virtAddr;

        // Update the last-mapped TCM page for the new VA
        s_odpVirtAddrSetLastTcmPageIdx(pPage->virtAddr, tcmIndex, bIsItcm);
    }

    if (pPage->mpuTrackIdx == ODP_MPU_TRACK_INDEX_UNSET)
    {
        // No MPU index for this page, allocate it now.
        s_odpMpuAllocate(ODP_MPU_TRACKING(bIsItcm), pPage);
    }

    // Map into MPU
    mpuWriteRaw(pPage->mpuIdx, pPage->virtAddr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1),
                ODP_TCM_PAGE_PHYSADDR(pPage), pageSize, SectionDescPermission[index]);

    _lastPagedVA = virtAddr;
#if defined(INSTRUMENTATION) || defined(USTREAMER)
    lwrtosHookOdpInstrumentationEvent(bIsItcm, LW_FALSE);
#endif // INSTRUMENTATION

#ifdef DMA_SUSPENSION
odpIsrHandler_exit_success:
#endif // DMA_SUSPENSION
    if (statsEntryId >= ODP_STATS_ENTRY_ID__COUNT)
    {
        dbgPrintf(LEVEL_CRIT, "Some code path fails to properly set statsEntryId\n");
        return LW_FALSE;
    }

    // Update ODP counters.
    riscvOdpStats.missCnt[statsEntryId]++;
    riscvOdpStats.missCnt[ODP_STATS_ENTRY_ID_COMBINED]++;

    // Update ODP timers (when applicable).
    if (statsEntryId < ODP_STATS_ENTRY_ID__CAPTURE_TIME)
    {
        // Time it took to service this ODP miss (in arch dependant units).
        statsTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - statsTime;

        riscvOdpStats.timeSum[statsEntryId] += statsTime;
        riscvOdpStats.timeMin[statsEntryId] =
            LW_MIN(riscvOdpStats.timeMin[statsEntryId], LwU64_LO32(statsTime));
        riscvOdpStats.timeMax[statsEntryId] =
            LW_MAX(riscvOdpStats.timeMax[statsEntryId], LwU64_LO32(statsTime));
    }

    return LW_TRUE;
}

//-------------------------------------------------------------------------------------------------
FLCN_STATUS odpLoadPage(const void *pInPage)
{
    LwUPtr          virtAddr = (LwUPtr)pInPage;
    FLCN_STATUS     status = FLCN_OK;
    ODP_TCM_PAGE   *pPage;

    if ((status = s_odpVirtAddrToPage(virtAddr, &pPage, NULL)) != FLCN_OK)
    {
        goto odpLoadPage_exit;
    }

    if (pPage == NULL)
    {
        // Bring the address into TCM
        if (!odpIsrHandler(virtAddr))
        {
            // Couldn't load in the page!
            status = FLCN_ERROR;
            goto odpLoadPage_exit;
        }
    }

odpLoadPage_exit:
    return status;
}

//-------------------------------------------------------------------------------------------------
FLCN_STATUS odpPinSection(LwU64 index)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    if ((SectionDescLocation[index] == UPROC_SECTION_LOCATION_DMEM_ODP) ||
        (SectionDescLocation[index] == UPROC_SECTION_LOCATION_IMEM_ODP))
    {
        status = odpPin((void*)SectionDescStartVirtual[index], SectionDescMaxSize[index]);
    }

    return status;
}

//-------------------------------------------------------------------------------------------------
FLCN_STATUS odpUnpinSection(LwU64 index)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    if ((SectionDescLocation[index] == UPROC_SECTION_LOCATION_DMEM_ODP) ||
        (SectionDescLocation[index] == UPROC_SECTION_LOCATION_IMEM_ODP))
    {
        status = odpUnpin((void*)SectionDescStartVirtual[index], SectionDescMaxSize[index]);
    }

    return status;
}

//-------------------------------------------------------------------------------------------------
#ifdef DMA_SUSPENSION
void lwrtosHookPinTaskStack(void)
{
    FLCN_STATUS status;

    // No need to re-pin if it's the same task
    if (pLastPinnedTask == pxLwrrentTCB)
    {
        return;
    }

    // Unpin previously active task
    if (pLastPinnedTask != NULL)
    {
        status = odpUnpin(pLastPinnedTask->pcStackLastAddress - pLastPinnedTask->uxStackSizeBytes,
                          pLastPinnedTask->uxStackSizeBytes);
        if (status != FLCN_OK)
        {
            // Unpin shouldn't fail or we're in trouble.
            dbgPrintf(LEVEL_CRIT, "Unpin failed! %d\n", status);
            appHalt();
        }
        pLastPinnedTask = NULL;
    }

    // Only try to pin the new stack if it's pageable memory.
    void *pStackBase = pxLwrrentTCB->pcStackLastAddress - pxLwrrentTCB->uxStackSizeBytes;
    LwS64 index = s_odpVirtAddrToSection((LwUPtr)pStackBase, NULL, LW_FALSE);
    if ((index >= 0) && ((SectionDescLocation[index] == UPROC_SECTION_LOCATION_DMEM_ODP) ||
                         (SectionDescLocation[index] == UPROC_SECTION_LOCATION_IMEM_ODP)))
    {
        status = odpPin(pStackBase, pxLwrrentTCB->uxStackSizeBytes);
        if (status == FLCN_OK)
        {
            // Only mark the current task as pinned on success.
            pLastPinnedTask = pxLwrrentTCB;
        }
        else if (status == FLCN_ERR_DMA_SUSPENDED)
        {
            //
            // DMA suspension is active, thus we were unable to pin the stack.
            // We will mark to yield so that we can try to switch to LPWR to wake from MSCG.
            //
            portYIELD_IMMEDIATE();
        }
        else
        {
            // Other errors aren't expected, so we're in trouble.
            dbgPrintf(LEVEL_CRIT, "Pin failed! %d\n", status);
            appHalt();
        }
    }
}
#endif // DMA_SUSPENSION

//-------------------------------------------------------------------------------------------------
static FLCN_STATUS s_odpVirtAddrToPage(LwUPtr virtAddr, ODP_TCM_PAGE **ppPage, LwS64 *pSection)
{
    LwBool bIsItcm;
    *ppPage = NULL;

    // Find section for the virtual address
    LwS64 index = s_odpVirtAddrToSection(virtAddr, NULL, LW_FALSE);
    // If no section was found, this isn't real memory probably
    if (index < 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check if the section is an ODP section
    switch (SectionDescLocation[index])
    {
        case UPROC_SECTION_LOCATION_DMEM_ODP:
            bIsItcm = LW_FALSE;
            break;

        case UPROC_SECTION_LOCATION_IMEM_ODP:
            bIsItcm = LW_TRUE;
            break;

        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
    }

    LwS64 tcmIndex = s_odpVirtAddrGetLastTcmPageIdx(virtAddr, bIsItcm);
    if (tcmIndex >= 0)
    {
        *ppPage = ODP_TCM_PAGE_FROM_INDEX(bIsItcm, tcmIndex);
    }
    if (pSection != NULL)
    {
        *pSection = index;
    }
    // When TCM is not found, return is ok but *ppPage is NULL.
    return FLCN_OK;
}

//-------------------------------------------------------------------------------------------------
static LwBool
s_odpInitVaMappingMetadata(ODP_VA_MAPPING_METADATA *pMetadata)
{
    if (pMetadata == NULL)
    {
        return LW_FALSE;
    }

    const LwU8 sectionIdxFullLast = pMetadata->sectionIdxFullFirst +
                                    pMetadata->sectionIdxFullCount;

    for (pMetadata->sectionIdxFirst = pMetadata->sectionIdxFullFirst;
         (SectionDescLocation[pMetadata->sectionIdxFirst] != pMetadata->pagedSectionLoc) &&
         (pMetadata->sectionIdxFirst < sectionIdxFullLast);
         pMetadata->sectionIdxFirst++)
    {
        // NOP
    }

    pMetadata->startVA = SectionDescStartVirtual[pMetadata->sectionIdxFirst];

    for (pMetadata->sectionIdxLast = sectionIdxFullLast - 1U;
         (SectionDescLocation[pMetadata->sectionIdxLast] != pMetadata->pagedSectionLoc) &&
         (pMetadata->sectionIdxLast > 0U);
         pMetadata->sectionIdxLast--)
    {
        // NOP
    }

    pMetadata->endVA = SectionDescStartVirtual[pMetadata->sectionIdxLast] +
                       SectionDescMaxSize[pMetadata->sectionIdxLast];

    pMetadata->numEntries = ODP_VA_MAPPING_IDX(pMetadata->endVA, pMetadata);

    if (pMetadata->numEntries == 0U)
    {
        dbgPrintf(LEVEL_CRIT, "ODP page mapping failed: no sections to map VAs for!\n");
        return LW_FALSE;
    }

    return LW_TRUE;
}

static void *
s_odpAllocVaMapping
(
    const LwU8     heapSectionIdx,
    const LwLength numEntries,
    const LwLength pageDataSize
)
{
    void *pPageVaMapping = lwosCalloc(heapSectionIdx, numEntries * pageDataSize);

    if (pPageVaMapping == NULL)
    {
        dbgPrintf(LEVEL_CRIT, "ODP page mapping failed: can't allocate VA mappings for "
                              "numEntries = 0x%llX and pageDataSize = 0x%llX\n",
                  numEntries, pageDataSize);
        return NULL;
    }

    //
    // This allocator represents a mappping scheme where the
    // default section idx value is filled with all-1-bits, identifying a VMA hole.
    //
    memset(pPageVaMapping, 0xFF, numEntries * pageDataSize);

    return pPageVaMapping;
}

static LwBool
s_odpPopulateVaToSectionMapping
(
    LwU8                          *pPageVaSectionMapping,
    ODP_VA_MAPPING_METADATA *const pMetadata
)
{
    LwU8 sectionIdx = 0;

    for (sectionIdx = pMetadata->sectionIdxFirst;
         sectionIdx <= pMetadata->sectionIdxLast;
         sectionIdx++)
    {
        LwUPtr startAddr = SectionDescStartVirtual[sectionIdx];
        LwUPtr offset = 0;

        if (SectionDescLocation[sectionIdx] != pMetadata->pagedSectionLoc)
        {
            // Just skip resident sections! Treat them as more VMA holes!
            continue;
        }

        if ((startAddr % LWBIT64(pMetadata->pageShift)) != 0U)
        {
            dbgPrintf(LEVEL_CRIT, "ODP page-to-section mapping failed:"
                                  " start addr of section ID %d not aligned to page size 0x%llX!\n",
                      sectionIdx, LWBIT64(pMetadata->pageShift));
            return LW_FALSE;
        }

        while (offset < SectionDescMaxSize[sectionIdx])
        {
            pPageVaSectionMapping[ODP_VA_MAPPING_IDX(startAddr + offset, pMetadata)] = sectionIdx;
            offset += LWBIT64(pMetadata->pageShift);
        }

        if (offset != SectionDescMaxSize[sectionIdx])
        {
            dbgPrintf(LEVEL_CRIT, "ODP page-to-section mapping failed:"
                                  " max size of section ID %d not aligned to page size 0x%llX!\n",
                      sectionIdx, LWBIT64(pMetadata->pageShift));
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

//-------------------------------------------------------------------------------------------------
static LW_FORCEINLINE LwS64
s_odpVirtAddrToSection(LwUPtr addr, LwBool *pbHiBitFlagIsFb, LwBool bSetBitFlagInMapping)
{
    LwU8 res;

    for (LwU64 i = 0; i < ODP_REGION_COUNT; i++)
    {
        if ((addr >= _odpPageVaMappingData[i].startVA) &&
            (addr < _odpPageVaMappingData[i].endVA))
        {
            res = _odpPageVaSectionMapping[i][ODP_RGN_VA_MAPPING_IDX(i, addr)];

            //
            // ODP_PAGE_DATA_SECTION_ILWALID can be used as a mask,
            // since it has all the bits we care about set!
            //
            if (pbHiBitFlagIsFb != NULL)
            {
                if (bSetBitFlagInMapping) // Set the hi-bit flag for this section entry!
                {
                    _odpPageVaSectionMapping[i][ODP_RGN_VA_MAPPING_IDX(i, addr)] =
                        FLD_SET_DRF_NUM(_ODP, _SECTION_VAL, _BACKING_ID, *pbHiBitFlagIsFb, res);
                }
                else // Fetch the hi-bit from this section entry.
                {
                    *pbHiBitFlagIsFb = DRF_VAL(_ODP, _SECTION_VAL, _BACKING_ID, res);
                }
            }

            return FLD_TEST_DRF(_ODP, _SECTION_VAL, _INDEX, _ILWALID, res) ?
                        -1 : (LwS64)DRF_VAL(_ODP, _SECTION_VAL, _INDEX, res);
        }
    }
    return -1;
}

//-------------------------------------------------------------------------------------------------
static LW_FORCEINLINE LwS64
s_odpVirtAddrGetLastTcmPageIdx(LwUPtr addr, LwBool bIsItcm)
{
    LwU8 res = _odpPageVaTcmPageIdxMapping[bIsItcm][ODP_RGN_VA_MAPPING_IDX(bIsItcm, addr)];

    return (res == ODP_PAGE_DATA_LAST_TCM_ID_ILWALID) ? -1 : (LwS64)res;
}

static LW_FORCEINLINE void
s_odpVirtAddrSetLastTcmPageIdx(LwUPtr addr, LwU8 tcmPageIdx, LwBool bIsItcm)
{
    _odpPageVaTcmPageIdxMapping[bIsItcm][ODP_RGN_VA_MAPPING_IDX(bIsItcm, addr)] = tcmPageIdx;
}

//-------------------------------------------------------------------------------------------------
static LW_FORCEINLINE FLCN_STATUS
s_odpDmaXfer
(
    LwU64 sourcePhys,
    LwU64 destPhys,
    LwU64 sizeBytes,
    LwBool bDtcmToFb,
    LwBool bIsItcm
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU64       offset;
    LwU64       fbBase;
    LwU32       dmaCmd = 0;

    if (sizeBytes == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_odpDmaXfer_exit;
    }

#ifdef DMA_SUSPENSION
    if (fbhubGatedGet())
    {
        status = FLCN_ERR_DMA_SUSPENDED;
        goto s_odpDmaXfer_exit;
    }
#endif // DMA_SUSPENSION

    if (bDtcmToFb)
    {
        LwU64 srcOffset;
        LwU64 dstOffset;

        // Source is DTCM
        srcOffset = sourcePhys;

        //
        // Get physical address
        //
        dstOffset = destPhys - riscvPhysAddrBase;

        offset = srcOffset;
        // fbBase must compensate for offset that we pass in TRFFBOFFS
        fbBase = dstOffset - offset;

        dmaCmd = _dmaCmdDtcmToFb;
    }
    else
    {
        LwU64 srcOffset;
        LwU64 dstOffset = destPhys;

        // dest is TCM
        if (bIsItcm)
        {
            dmaCmd = _dmaCmdFbToItcm;
        }
        else
        {
            dmaCmd = _dmaCmdFbToDtcm;
        }

        //
        // Get physical address
        //
        srcOffset = sourcePhys - riscvPhysAddrBase;

        offset = dstOffset;
        // fbBase must compensate for offset that we pass in TRFFBOFFS
        fbBase = srcOffset - offset;
    }

    //
    // Wait for completion of any preceding transfers.
    // Even though all of our DMA code waits for idle after finishing,
    // ODP DMA can still be requested, for example, in the middle of
    // dmaMemTransfer (to load a page for DMA targeting paged memory),
    // where the DMA queue will not be emty at this point.
    // But, most times when this is called, the DMA queue will be empty
    // at this point, so this will just give us a small overhead.
    //
    dmaWaitForIdle();

    // Assume no transfers ongoing at this point
    intioWrite(LW_PRGNLCL_FALCON_DMATRFBASE,
        LwU64_LO32(fbBase >> 8));
    intioWrite(LW_PRGNLCL_FALCON_DMATRFBASE1,
        LwU64_HI32(fbBase >> 8));

    while (sizeBytes != 0)
    {
        intioWrite(LW_PRGNLCL_FALCON_DMATRFMOFFS, offset);
        intioWrite(LW_PRGNLCL_FALCON_DMATRFFBOFFS, LwU64_LO32(offset));
        intioWrite(LW_PRGNLCL_FALCON_DMATRFCMD, dmaCmd);

        sizeBytes -= ENGINE_DMA_BLOCK_SIZE_MAX;
        offset += ENGINE_DMA_BLOCK_SIZE_MAX;
    }

    // Wait for completion of any remaining transfers
    dmaWaitForIdle();

    //
    // Stall the pipeline to make sure we don't spelwlatively execute
    // outside this function until _DMATRFCMD actually reports _IDLE.
    // This isn't really necessary because at this point the page is
    // not yet mapped in the MPU, but just to feel safe.
    // MMINTS-TODO: think about this carefully and then remove!
    //
    lwFenceAll();

#ifdef DMA_NACK_SUPPORTED
    status = dmaNackCheckAndClear();
#endif // DMA_NACK_SUPPORTED

s_odpDmaXfer_exit:
    return status;
}

static LwBool s_odpIsPageDirty(ODP_TCM_PAGE *pPage)
{
    if (FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _DIRTY, _TRUE, pPage->flags))
    {
        // Page has been explicitly marked as dirty
        return LW_TRUE;
    }

    // Check if page has dirty MPU
    if (pPage->mpuTrackIdx != ODP_MPU_TRACK_INDEX_UNSET)
    {
        //
        // We don't ilwalidate mpuIdx when ilwalidating mpuTrackIdx, so make sure
        // to check it first!
        //
        return mpuIsDirty(pPage->mpuIdx);
    }

    //
    // Page has not explicitly been marked as dirty and has no MPU entry
    // associated, so it must be clean.
    //
    return LW_FALSE;
}

static void s_odpPageDirtyReset(ODP_TCM_PAGE *pPage)
{
    pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _DIRTY, _FALSE, pPage->flags);

    // Check if page has an MPU
    if (pPage->mpuTrackIdx != ODP_MPU_TRACK_INDEX_UNSET)
    {
        //
        // We don't ilwalidate mpuIdx when ilwalidating mpuTrackIdx, so make sure
        // to check it first!
        //
        mpuDirtyReset(pPage->mpuIdx);
    }
}

//-------------------------------------------------------------------------------------------------
static FLCN_STATUS s_odpFlushPage(ODP_TCM_PAGE *pPage)
{
    FLCN_STATUS status = FLCN_OK;
    // ITCM never needs to flush
    if (FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _REGION, _ITCM, pPage->flags))
    {
        return status;
    }

    if (!s_odpIsPageDirty(pPage))
    {
        //
        // If neither the page nor the MPU entry are dirty, then
        // we have nothing to write, so just return.
        //
        return status;
    }

#if ELF_IN_PLACE_FULL_ODP_COW
    {
        LwBool bIsBackingRegionFb = LW_TRUE;
        LwS64 index = s_odpVirtAddrToSection(pPage->virtAddr, &bIsBackingRegionFb, LW_FALSE);

        if (index < 0)
        {
            return FLCN_ERROR;
        }

        // Only need to check flag, we know that this is a data page already!
        if (!bIsBackingRegionFb)
        {
            bIsBackingRegionFb = LW_TRUE;

            // Set backing section bit for this section to 0, we use FB now
            index = s_odpVirtAddrToSection(pPage->virtAddr, &bIsBackingRegionFb, LW_TRUE);

            if (index < 0)
            {
                return FLCN_ERROR;
            }

            // Switch page physical addr from ELF to FB
            pPage->physAddr = pPage->physAddr - SectionDescStartDataInElf[index] + SectionDescStartPhysical[index];
        }
    }
#endif // ELF_IN_PLACE_FULL_ODP_COW

    // DMA TCM->FB
    status = s_odpDmaXfer(pPage->tcmPhysOffset,
                          pPage->physAddr,
                          PROFILE_ODP_DATA_PAGE_SIZE,
                          LW_TRUE, LW_FALSE);
    if (status == FLCN_OK)
    {
        // Ilwalidate the page's dirty flag since we've written it back
        s_odpPageDirtyReset(pPage);
    }
    else
    {
        dbgPrintf(LEVEL_CRIT, "ODP DMA failed: 0x%X\n", status);
    }

    return status;
}

static void s_odpIlwalidatePage(ODP_TCM_PAGE *pPage)
{
    // Unmap from MPU
    s_odpMpuFree(ODP_MPU_TRACKING(FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _REGION, _ITCM, pPage->flags)),
                 pPage);

    if (pPage->virtAddr != 0U)
    {
        // Ilwalidate the last-mapped TCM page for the page's VA
        s_odpVirtAddrSetLastTcmPageIdx(pPage->virtAddr,
                                       ODP_PAGE_DATA_LAST_TCM_ID_ILWALID,
                                       FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _REGION, _ITCM,
                                                    pPage->flags));
    }

    // Mark as ilwalidated (and remove pinning and dirty flags)
    pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _VALID, _FALSE, pPage->flags);
    pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _PINNED, _FALSE, pPage->flags);
    pPage->flags = FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _DIRTY, _FALSE, pPage->flags);
    pPage->virtAddr = 0;
}

static FLCN_STATUS s_odpRecyclePage(ODP_TCM_PAGE *pPage)
{
    FLCN_STATUS status = s_odpFlushPage(pPage);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (pPage->virtAddr == 0U)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Ilwalidate the last-mapped TCM page for the page's previous VA, as we
    // will reuse the page for a new VA.
    //
    s_odpVirtAddrSetLastTcmPageIdx(pPage->virtAddr,
                                   ODP_PAGE_DATA_LAST_TCM_ID_ILWALID,
                                   FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _REGION, _ITCM,
                                                pPage->flags));

    //
    // We do not need to free the MPU entry, as we will reuse it if it was attached.
    //
    // Similarly, the virtual address and flags don't need to be touched as they will be
    // overwritten almost immediately.
    //

    return FLCN_OK;
}

//-------------------------------------------------------------------------------------------------
static LwBool s_odpMpuInitialize(ODP_MPU_TRACK *pMpuTrack, LwU64 *pPrevMpuEntry)
{
    LwS64 prevEntry = *pPrevMpuEntry;
    pMpuTrack->next = 0;
    for (LwU64 i = 0; i < pMpuTrack->limit; i++)
    {
        prevEntry = mpuReserveEntry(prevEntry);
        if (prevEntry < 0)
        {
            dbgPrintf(LEVEL_CRIT, "ODP init failed: couldn't alloc MPU entries\n");
            return LW_FALSE;
        }
        pMpuTrack->pEntryList[i] = prevEntry;
        pMpuTrack->pAssocTcmPage[i] = ODP_MPU_ASSOCIATED_PAGE_ILWALID;
    }
    *pPrevMpuEntry = prevEntry;
    return LW_TRUE;
}

static void s_odpMpuPageUnhook(ODP_MPU_TRACK *pMpuTrack, LwU64 idx)
{
    if (pMpuTrack->pAssocTcmPage[idx] != ODP_MPU_ASSOCIATED_PAGE_ILWALID)
    {
        LwU64 assocPageIdx = pMpuTrack->pAssocTcmPage[idx];
        _tcmOdpPage[assocPageIdx].mpuTrackIdx = ODP_MPU_TRACK_INDEX_UNSET;

        if (FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _REGION, _DTCM,
                         _tcmOdpPage[assocPageIdx].flags) &&
            mpuIsDirty(pMpuTrack->pEntryList[idx]))
        {
            //
            // Set page to dirty. We don't care about updating the MPU entry
            // since reusing it via mpuWriteRaw will automatically reset it
            // (since the dirty bit is part of the VA field)
            //
            _tcmOdpPage[assocPageIdx].flags =
                FLD_SET_DRF(_ODP, _TCM_PAGE_FLAG, _DIRTY, _TRUE,
                            _tcmOdpPage[assocPageIdx].flags);
        }
    }
}

static void LW_FORCEINLINE s_odpMpuAllocate(ODP_MPU_TRACK *pMpuTrack, ODP_TCM_PAGE *pPage)
{
    LwU64 idx = s_odpPolicyNextMpuEntry(pMpuTrack);

    // Unhook this MPU entry from its current page, if it was in use.
    s_odpMpuPageUnhook(pMpuTrack, idx);

    // Save index into _tcmOdpPage for pPage
    pMpuTrack->pAssocTcmPage[idx] = pPage - _tcmOdpPage;

    pPage->mpuTrackIdx = idx;
    pPage->mpuIdx = pMpuTrack->pEntryList[idx];
}

static void LW_FORCEINLINE s_odpMpuFree(ODP_MPU_TRACK *pMpuTrack, ODP_TCM_PAGE *pPage)
{
    LwU64 idx = pPage->mpuTrackIdx;
    if (idx == ODP_MPU_TRACK_INDEX_UNSET)
    {
        return;
    }

    // Unhook this MPU entry from its current page, if it was in use.
    s_odpMpuPageUnhook(pMpuTrack, idx);

    pMpuTrack->pAssocTcmPage[idx] = ODP_MPU_ASSOCIATED_PAGE_ILWALID;

    // Disable the MPU entry
    mpuRemoveRaw(pPage->mpuIdx);

    // Mark as having no MPU index
    pPage->mpuTrackIdx = ODP_MPU_TRACK_INDEX_UNSET;
}

//-------------------------------------------------------------------------------------------------
static LW_FORCEINLINE LwU64 s_odpPolicyNextTcmEntry(LwBool bIsItcm)
{
    // Round-robin allocation
    LwU64 idx;
    do
    {
        idx = _tcmPageIndex[bIsItcm]++;
        if (_tcmPageIndex[bIsItcm] == _tcmPageCount[bIsItcm])
        {
            _tcmPageIndex[bIsItcm] = 0;
        }
    } while(FLD_TEST_DRF(_ODP, _TCM_PAGE_FLAG, _PINNED, _TRUE,
                         ODP_TCM_PAGE_FROM_INDEX(bIsItcm, idx)->flags));

    return idx;
}

//-------------------------------------------------------------------------------------------------
static LW_FORCEINLINE LwU64 s_odpPolicyNextMpuEntry(ODP_MPU_TRACK *pMpuTrack)
{
    // Round-robin allocation
    LwU64 idx = pMpuTrack->next++;
    if (pMpuTrack->next == pMpuTrack->limit)
    {
        pMpuTrack->next = 0;
    }
    return idx;
}
