/*
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

#ifndef LWOS_FALCON_H
#define LWOS_FALCON_H

/*!
 * NJ-TODO: Following defines to be reviewed, possibly renamed, moved to manual
 *          addendum, and who knows what else...
 */
#define LW_FLCN_IMBLK_BLK_ADDR                                              23:0
#define LW_FLCN_IMBLK_BLK_LINE_IDX                                          23:8
#define LW_FLCN_IMBLK_BLK_VALID                                            24:24
#define LW_FLCN_IMBLK_BLK_VALID_NO                                   0x00000000U
#define LW_FLCN_IMBLK_BLK_VALID_YES                                  0x00000001U
#define LW_FLCN_IMBLK_BLK_PENDING                                          25:25
#define LW_FLCN_IMBLK_BLK_PENDING_NO                                 0x00000000U
#define LW_FLCN_IMBLK_BLK_PENDING_YES                                0x00000001U
#define LW_FLCN_IMBLK_BLK_SELWRE                                           26:26
#define LW_FLCN_IMBLK_BLK_SELWRE_NO                                  0x00000000U
#define LW_FLCN_IMBLK_BLK_SELWRE_YES                                 0x00000001U

#define LW_FLCN_IMTAG_BLK_LINE_INDEX                                         7:0
#define LW_FLCN_IMTAG_BLK_VALID                                            24:24
#define LW_FLCN_IMTAG_BLK_VALID_NO                                   0x00000000U
#define LW_FLCN_IMTAG_BLK_VALID_YES                                  0x00000001U
#define LW_FLCN_IMTAG_BLK_PENDING                                          25:25
#define LW_FLCN_IMTAG_BLK_PENDING_NO                                 0x00000000U
#define LW_FLCN_IMTAG_BLK_PENDING_YES                                0x00000001U
#define LW_FLCN_IMTAG_BLK_SELWRE                                           26:26
#define LW_FLCN_IMTAG_BLK_SELWRE_NO                                  0x00000000U
#define LW_FLCN_IMTAG_BLK_SELWRE_YES                                 0x00000001U
#define LW_FLCN_IMTAG_BLK_MULTI_HIT                                        30:30
#define LW_FLCN_IMTAG_BLK_MULTI_HIT_NO                               0x00000000U
#define LW_FLCN_IMTAG_BLK_MULTI_HIT_YES                              0x00000001U
#define LW_FLCN_IMTAG_BLK_MISS                                             31:31
#define LW_FLCN_IMTAG_BLK_MISS_NO                                    0x00000000U
#define LW_FLCN_IMTAG_BLK_MISS_YES                                   0x00000001U

#define LW_FLCN_DMBLK_BLK_ADDR                                              23:0
#define LW_FLCN_DMBLK_BLK_LINE_IDX                                          23:8
#define LW_FLCN_DMBLK_BLK_SEC_LEVEL                                        26:24
#define LW_FLCN_DMBLK_BLK_VALID                                            28:28
#define LW_FLCN_DMBLK_BLK_VALID_NO                                   0x00000000U
#define LW_FLCN_DMBLK_BLK_VALID_YES                                  0x00000001U
#define LW_FLCN_DMBLK_BLK_PENDING                                          29:29
#define LW_FLCN_DMBLK_BLK_PENDING_NO                                 0x00000000U
#define LW_FLCN_DMBLK_BLK_PENDING_YES                                0x00000001U
#define LW_FLCN_DMBLK_BLK_DIRTY                                            30:30
#define LW_FLCN_DMBLK_BLK_DIRTY_NO                                   0x00000000U
#define LW_FLCN_DMBLK_BLK_DIRTY_YES                                  0x00000001U

#define LW_FLCN_DMTAG_BLK_LINE_INDEX                                         7:0
#define LW_FLCN_DMTAG_BLK_MISS                                             20:20
#define LW_FLCN_DMTAG_BLK_MISS_NO                                    0x00000000U
#define LW_FLCN_DMTAG_BLK_MISS_YES                                   0x00000001U
#define LW_FLCN_DMTAG_BLK_MULTI_HIT                                        21:21
#define LW_FLCN_DMTAG_BLK_MULTI_HIT_NO                               0x00000000U
#define LW_FLCN_DMTAG_BLK_MULTI_HIT_YES                              0x00000001U
#define LW_FLCN_DMTAG_BLK_SEC_LEVEL                                        26:24
#define LW_FLCN_DMTAG_BLK_VALID                                            28:28
#define LW_FLCN_DMTAG_BLK_VALID_NO                                   0x00000000U
#define LW_FLCN_DMTAG_BLK_VALID_YES                                  0x00000001U
#define LW_FLCN_DMTAG_BLK_PENDING                                          29:29
#define LW_FLCN_DMTAG_BLK_PENDING_NO                                 0x00000000U
#define LW_FLCN_DMTAG_BLK_PENDING_YES                                0x00000001U
#define LW_FLCN_DMTAG_BLK_DIRTY                                            30:30
#define LW_FLCN_DMTAG_BLK_DIRTY_NO                                   0x00000000U
#define LW_FLCN_DMTAG_BLK_DIRTY_YES                                  0x00000001U
#define LW_FLCN_DMTAG_BLK_NON_VA                                           31:31
#define LW_FLCN_DMTAG_BLK_NON_VA_NO                                  0x00000000U
#define LW_FLCN_DMTAG_BLK_NON_VA_YES                                 0x00000001U

#define LW_FLCN_DMREAD_PHYSICAL_ADDRESS                                     15:0
#define LW_FLCN_DMREAD_ENC_SIZE                                            18:16
#define LW_FLCN_DMREAD_ENC_SIZE_004                                  0x00000000U
#define LW_FLCN_DMREAD_ENC_SIZE_006                                  0x00000001U
#define LW_FLCN_DMREAD_ENC_SIZE_016                                  0x00000002U
#define LW_FLCN_DMREAD_ENC_SIZE_032                                  0x00000003U
#define LW_FLCN_DMREAD_ENC_SIZE_064                                  0x00000004U
#define LW_FLCN_DMREAD_ENC_SIZE_128                                  0x00000005U
#define LW_FLCN_DMREAD_ENC_SIZE_256                                  0x00000006U
#define LW_FLCN_DMREAD_ENC_SIZE_ILWALID                              0x00000007U
#define LW_FLCN_DMREAD_SEC_LEV_MASK                                        27:24
#define LW_FLCN_DMREAD_SET_TAG                                             31:31
#define LW_FLCN_DMREAD_SET_TAG_NO                                    0x00000000U
#define LW_FLCN_DMREAD_SET_TAG_YES                                   0x00000001U

#define LW_FLCN_DMWRITE_PHYSICAL_ADDRESS                                    15:0
#define LW_FLCN_DMWRITE_ENC_SIZE                                           18:16
#define LW_FLCN_DMWRITE_ENC_SIZE_004                                 0x00000000U
#define LW_FLCN_DMWRITE_ENC_SIZE_006                                 0x00000001U
#define LW_FLCN_DMWRITE_ENC_SIZE_016                                 0x00000002U
#define LW_FLCN_DMWRITE_ENC_SIZE_032                                 0x00000003U
#define LW_FLCN_DMWRITE_ENC_SIZE_064                                 0x00000004U
#define LW_FLCN_DMWRITE_ENC_SIZE_128                                 0x00000005U
#define LW_FLCN_DMWRITE_ENC_SIZE_256                                 0x00000006U
#define LW_FLCN_DMWRITE_ENC_SIZE_ILWALID                             0x00000007U

/*!
 * The number of bits required to address bytes within a Falcon IMEM block.
 */
#define IMEM_BLOCK_WIDTH                8U
#define DMEM_BLOCK_WIDTH                8U

/*!
 * The size of each Falcon IMEM block in bytes.
 */
#define IMEM_BLOCK_SIZE                 LWBIT32(IMEM_BLOCK_WIDTH)
#define DMEM_BLOCK_SIZE                 LWBIT32(DMEM_BLOCK_WIDTH)

/*!
 * Colwert an IMEM byte-address to an IMEM block-address (or block-index).
 */
#define IMEM_ADDR_TO_IDX(a)             ((LwU16)((a) >> IMEM_BLOCK_WIDTH))
#define DMEM_ADDR_TO_IDX(a)             ((LwU16)((a) >> DMEM_BLOCK_WIDTH))

/*!
 * Colwert an IMEM block-address (or block-index) to an IMEM byte-address.
 */
#define IMEM_IDX_TO_ADDR(i)             (((LwU32)(i)) << IMEM_BLOCK_WIDTH)
#define DMEM_IDX_TO_ADDR(i)             (((LwU32)(i)) << DMEM_BLOCK_WIDTH)

/*!
 * Determine if the result of imtag/dmtag instruction indicates a code/data-miss
 * (memory associated with the tag not present in the physical IMEM/DMEM).
 */
#define IMTAG_IS_MISS(s)    FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MISS, _YES, (s))
#define DMTAG_IS_MISS(s)    FLD_TEST_DRF(_FLCN, _DMTAG, _BLK_MISS, _YES, (s))

/*!
 * Determine if the result of imtag/dmtag instruction indicates a secure block.
 */
#define IMTAG_IS_SELWRE(s)  FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_SELWRE, _YES, (s))
#define DMTAG_IS_SELWRE(s)  (((s)&(0x1<<26))!=0) // NJ-TODO: Bits 26:24 ???, or do we need this at all?

/*!
 * Determine if the result of imblk instruction indicates a secure block.
 */
#define IMBLK_IS_SELWRE(s)  FLD_TEST_DRF(_FLCN, _IMBLK, _BLK_SELWRE, _YES, (s))

/*!
 * Determine if the result of imtag/dmtag instruction indicates valid presence
 * in the physical memory.
 */
#define IMTAG_IS_ILWALID(s)                                                    \
    (FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_VALID,   _NO, (s)) &&                    \
     FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_PENDING, _NO, (s)))
#define DMTAG_IS_ILWALID(s)                                                    \
    (FLD_TEST_DRF(_FLCN, _DMTAG, _BLK_VALID,   _NO, (s)) &&                    \
     FLD_TEST_DRF(_FLCN, _DMTAG, _BLK_PENDING, _NO, (s)))

/*!
 * Determine if the result of a dmtag instruction indicates that the memory
 * block is dirty.
 */
#define DMTAG_IS_DIRTY(s)   FLD_TEST_DRF(_FLCN, _DMTAG, _BLK_DIRTY, _YES, (s))

/*!
 * Look-up physical memory block holding virtual memory page containing virtual
 * address tested by imtag/dmtag instruction.
 *
 * @param[in]   s   Result of imtag/dmtag instruction
 *
 * @return  Index of a physical memory block holding tested VA.
 *
 * @pre Result is sane only if (!IMTAG_IS_MISS(s))
 * @pre This macro is correct as long as physical IMEM does not exceed 1MB.
 */
#define IMTAG_IDX_GET(s)    DRF_VAL(_FLCN, _IMTAG, _BLK_LINE_INDEX, (s))
#define DMTAG_IDX_GET(s)    DRF_VAL(_FLCN, _DMTAG, _BLK_LINE_INDEX, (s))

/*!
 * Extract the tag-value (or tag-address) of the result of an imblk/dmblk
 * instruction.
 *
 * @param[in]  b  Result of imblk/dmblk instruction
 *
 * @return
 *     Tag-value contained in the provided imblk return-value.
 */
#define IMBLK_TAG_ADDR(b)   DRF_VAL(_FLCN, _IMBLK, _BLK_ADDR, (b))
#define DMBLK_TAG_ADDR(b)   DRF_VAL(_FLCN, _DMBLK, _BLK_ADDR, (b))

/*!
 * Determine if the result of an imblk/dmblk instruction indicates that
 * the block is invalid.
 */
#define IMBLK_IS_ILWALID(b)                                                    \
    (FLD_TEST_DRF(_FLCN, _IMBLK, _BLK_VALID,   _NO, (b)) &&                    \
     FLD_TEST_DRF(_FLCN, _IMBLK, _BLK_PENDING, _NO, (b)))
#define DMBLK_IS_ILWALID(b)                                                    \
    (FLD_TEST_DRF(_FLCN, _DMBLK, _BLK_VALID,   _NO, (b)) &&                    \
     FLD_TEST_DRF(_FLCN, _DMBLK, _BLK_PENDING, _NO, (b)))

/*!
 * Determine if the result of a dmblk instr. indicates that the block is dirty.
 */
#define DMBLK_IS_DIRTY(b)                                                      \
    (FLD_TEST_DRF(_FLCN, _DMBLK, _BLK_DIRTY,  _YES, (b)))

/*!
 * The CSB base address may vary in different falcons, so each falcon ucode
 * needs to set this address based on its real condition so that the RTOS can
 * work across every falcon.
 */
extern unsigned int csbBaseAddress;

#if defined (SEC2_CSB_ACCESS)
/*!
 * SEC2 doesn't use the same offsets for CSB registers as other Falcons do.
 * We use CSEC register names to access CSB registers for SEC2.
 */
#define CSB_REG(name) ((unsigned int *)(LW_CSEC_FALCON##name))
#define CSB_FIELD(name) LW_CSEC_FALCON##name
#elif defined (SOE_CSB_ACCESS)
/*!
 * SOE doesn't use the same offsets for CSB registers as other Falcons do.
 * We use CSOE register names to access CSB registers for SOE.
 */
#define CSB_REG(name) ((unsigned int *)(LW_CSOE_FALCON##name))
#define CSB_FIELD(name) LW_CSOE_FALCON##name
#elif defined (GSPLITE_CSB_ACCESS)
/*!
 * GSP-Lite doesn't use the same offsets for CSB registers as other Falcons do.
 * We use CGSP register names to access CSB registers for GSP-Lite.
 */
#define CSB_REG(name) ((unsigned int *)(LW_CGSP_FALCON##name))
#define CSB_FIELD(name) LW_CGSP_FALCON##name
#else
/*!
 * Base address for the CSB access is only required in certain falcon
 * (e.g. DPU). BASEADDR_NEEDED will be set to 0 or 1 based on the makefile
 * setting in each falcon. When it's 0, csbBaseAddress will not be considered,
 * and it will not be compiled into the binary thus saving some resident code
 * space.
 */
#define CSB_REG(name) ((unsigned int *)(LwUPtr)(csbBaseAddress * BASEADDR_NEEDED + LW_CMSDEC_FALCON##name))
#define CSB_FIELD(name) LW_CMSDEC_FALCON##name
#endif

#endif // LWOS_FALCON_H

