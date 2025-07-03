/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_40_PROG_H
#define CLK_DOMAIN_40_PROG_H

/*!
 * @file clk_domain_40_prog.h
 * @brief @copydoc clk_domain_40_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x.h"
#include "clk/clk_domain_prog.h"

/*!
 * Forward definition of types so that they can be included in function
 * prototypes below.  clk_vf_rel*.h are already included the clk_domain*.h and
 * using the CLK_DOMAIN types in their function prototypes.
 */
typedef struct CLK_VF_REL CLK_VF_REL, CLK_VF_REL_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_DOMAIN_PROG structure from CLK_DOMAIN_40_PROG.
 */
#define CLK_DOMAIN_PROG_GET_FROM_40_PROG(_pDomain40Prog)                       \
    &((_pDomain40Prog)->prog)

/*!
 * Helper macro to return a pointer to CLK_DOMAIN object at a given index cast
 * to CLK_DOMAIN_40_PROG iff that CLK_DOMAIN implements CLK_DOMAIN_40_PROG.
 *
 * @note This could be improved by with general dynamic casting support in PMU
 * BOARDOBJ infrastructure.
 *
 * @param[in]  _idx
 *     Index  of the CLK_DOMAIN object to retrieve.
 */
#define CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(_idx)                                     \
    ((boardObjGrpMaskBitGet(&((CLK_CLK_DOMAINS_GET())->progDomainsMask), (_idx))) ? \
        (CLK_DOMAIN_40_PROG *)BOARDOBJGRP_OBJ_GET_BY_GRP_PTR(                       \
            &((CLK_CLK_DOMAINS_GET())->super.super), (_idx)) :                      \
         NULL)

/*!
 * Helper macro which returns whether frequency OC adjustment is enabled for the given
 * CLK_DOMAIN_40_PROG domain.
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return LW_TRUE   Frequency OC adjustment enabled
 * @return LW_FALSE  Frequency OC adjustment disabled
 */
#define clkDomain40ProgIsOCEnabled(pDomain40Prog)                              \
     ((!clkDomainsDebugModeEnabled())               &&                         \
      (LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK !=                                    \
            clkDomainApiDomainGet(pDomain40Prog))   &&                         \
      (clkDomainsOverrideOVOCEnabled()              ||                         \
        (((pDomain40Prog)->freqDeltaMinMHz != 0) ||                            \
         ((pDomain40Prog)->freqDeltaMaxMHz != 0))))

/*!
 * Helper macro which returns whether frequency OV adjustment is enabled for the given
 * CLK_DOMAIN_40_PROG domain.
 *
 * PP-TODO : Add new voltage range check support.
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return LW_TRUE   Frequency OV adjustment enabled
 * @return LW_FALSE  Frequency OV adjustment disabled
 */
#define clkDomain40ProgIsOVEnabled(pDomain40Prog)                              \
     (!clkDomainsDebugModeEnabled())

/*!
 * Helper macro to check whether frequency delta is non zero.
 *
 * @param[in] freqDelta   Frequency delta pointer.
 *
 * @return TRUE if non zero otherwise return FALSE.
 */
#define clkDomain40ProgIsFreqDeltaNonZero(pFreqDelta)                          \
      ((LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pFreqDelta) != 0) ||             \
       (LW2080_CTRL_CLK_FREQ_DELTA_GET_PCT(pFreqDelta) != 0))

/*!
 * Helper macro which returns whether Factory OC is enabled for the given
 * CLK_DOMAIN_40_PROG domain.
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return LW_TRUE   Factory OC enabled
 * @return LW_FALSE  Factory OC disabled
 */
#define clkDomain40ProgFactoryOCEnabled(pDomain40Prog)                         \
     ((!clkDomainsDebugModeEnabled()) &&                                       \
      (clkDomain40ProgIsFreqDeltaNonZero(                                      \
            clkDomain40ProgFinalFactoryDeltaGet(pDomain40Prog))))

/*!
 * Accessor macro for AIC factory OC delta -> CLK_DOMAIN_40_PROG::factoryDelta.
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::factoryDelta
 */
#define clkDomain40ProgFactoryDeltaGet(pDomain40Prog)                          \
    &((pDomain40Prog)->factoryDelta)


/*!
 * Accessor macro for GRD OC delta -> CLK_DOMAIN_40_PROG::grdFreqDelta.
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::grdFreqDelta
 */
#define clkDomain40ProgGrdFreqDeltaGet(pDomain40Prog)                          \
    &((pDomain40Prog)->grdFreqDelta)

/*!
 * Accessor macro for CLK_DOMAIN_40_PROG::deltas.freqDelta.
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::deltas.freqDelta
 */
#define clkDomain40ProgFreqDeltaGet(pDomain40Prog)                             \
    &((pDomain40Prog)->deltas.freqDelta)

/*!
 * Accessor macro for CLK_DOMAIN_40_PROG::deltas.pVoltDeltauV[(railIdx)]
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 * @param[in] railIdx         VOLTAGE_RAIL index
 *
 * @return @ref CLK_DOMAIN_40_PROG::deltas.pVoltDeltauV[(railIdx)]
 */
#define clkDomain40ProgVoltDeltauVGet(pDomain40Prog, railIdx)                  \
    ((pDomain40Prog)->deltas.pVoltDeltauV[(railIdx)])

/*!
 * Helper macro returning the scaling factor required to colwert to kHz
 * (commonly used in PERF code) from units used by the CLK_DOMAIN (usually MHz).
 *
 * For most CLK_DOMAINs, this will return 1000.  The main exception is
 * PCIEGENCLK, where the units aren't actually Hz, but the PCIE Gen Speed.
 *
 * @param[in]  _pDomain40Prog
 *     Pointer to the CLK_DOMAIN_40_PROG for which to retrieve the scaling
 *     factor.
 *
 * @return Scaling factor
 */
#define clkDomain40ProgFreqkHzScaleGet(_pDomain40Prog)                         \
    ((LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK !=                                     \
            clkDomainApiDomainGet(&((_pDomain40Prog)->super.super))) ?         \
                1000U : 1U)

/*!
 * Accessor macro for CLK_DOMAIN_40_PROG::clkEnumIdxFirst.
 *
 * @param[in] pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::clkEnumIdxFirst
 */
#define clkDomain40ProgClkEnumIdxFirstGet(pDomain40Prog)                       \
    ((pDomain40Prog)->clkEnumIdxFirst)

/*!
 * Accessor macro for CLK_DOMAIN_40_PROG::clkEnumIdxLast.
 *
 * @param[in] pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::clkEnumIdxLast
 */
#define clkDomain40ProgClkEnumIdxLastGet(pDomain40Prog)                        \
    ((pDomain40Prog)->clkEnumIdxLast)

/*!
 * Helper macro to get POR clock enum max.
 *
 * @param[in] pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 *
 * @return POR Clock enumeration max.
 */
#define clkDomain40ProgEnumFreqMaxMHzGet(pDomain40Prog)                        \
    (clkEnumFreqMaxMHzGet((CLK_ENUM *)BOARDOBJGRP_OBJ_GET(CLK_ENUM,            \
                          (pDomain40Prog)->clkEnumIdxLast)))

/*!
 * Accessor macro for CLK_DOMAIN_40_PROG::freqDeltaMinMHz.
 *
 * @param[in] pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::freqDeltaMinMHz
 */
#define clkDomain40ProgFreqDeltaMinMHzGet(pDomain40Prog)                       \
    ((pDomain40Prog)->freqDeltaMinMHz)

/*!
 * Accessor macro for CLK_DOMAIN_40_PROG::freqDeltaMaxMHz.
 *
 * @param[in] pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_40_PROG::freqDeltaMaxMHz
 */
#define clkDomain40ProgFreqDeltaMaxMHzGet(pDomain40Prog)                       \
    ((pDomain40Prog)->freqDeltaMaxMHz)

/*!
 * Accessor macro for CLK_DOMAIN's VF lwrve count.
 *
 * @param[in] pDomain40Prog CLK_DOMAIN_40_PROG pointer
 *
 * @return CLK_DOMAIN's VF lwrve count.
 */
#define clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog)                 \
    ((pDomain40Prog)->clkVFLwrveCount)

/*!
 * Accessor macro for secondary domains mask.
 *
 * @param[in] pDomain40Prog CLK_DOMAIN_40_PROG pointer
 * @param[in] railIdx       Voltage rail index
 *
 * @return Pointer to secondary domains index mask for given voltage rail index.
 */
#define clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx)         \
    ((((pDomain40Prog)->railVfItem[(railIdx)].type) !=                         \
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY) ? NULL :      \
     (&((pDomain40Prog)->railVfItem[(railIdx)].data.primary.secondaryDomainsMask)))

/*!
 * Accessor macro for primary-secondary group mask.
 *
 * @param[in] pDomain40Prog CLK_DOMAIN_40_PROG pointer
 * @param[in] railIdx       Voltage rail index
 *
 * @return Pointer to primary-secondary group domain index mask for given voltage rail index.
 */
#define clkDomain40ProgGetPrimarySecondaryDomainsMask(pDomain40Prog, railIdx)  \
    ((((pDomain40Prog)->railVfItem[(railIdx)].type) !=                         \
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY) ? NULL :      \
     (&((pDomain40Prog)->railVfItem[(railIdx)].data.primary.primarySecondaryDomainsMask.super)))

/*!
 * Accessor macro for CLK_DOMAIN position in VF frequency tuple.
 *
 * @param[in] pDomain40Prog CLK_DOMAIN_40_PROG pointer
 * @param[in] railIdx       Index of the VOLTAGE_RAIL.
 *
 * @return CLK_DOMAIN position based on input CLK_DOMAIN index clkIdx
 */
#define clkDomain40ProgGetClkDomainPosByIdx(pDomain40Prog, railIdx)            \
            ((pDomain40Prog)->railVfItem[(railIdx)].clkPos)

/*!
 * Clock Position ZERO in VF tuple is reserved for primary.
 */
#define CLK_VF_PRIMARY_POS   0U

/*!
 * Accessor macro for LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE.
 *
 * @param[in] pDomain40Prog CLK_DOMAIN_40_PROG pointer
 * @param[in] railIdx       Index of the VOLTAGE_RAIL.
 *
 * @return LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_<xyz>
 */
#define clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx)                   \
            ((pDomain40Prog)->railVfItem[(railIdx)].type)

/*!
 * @brief   Iterating macro to itereate over the programmable domains with
 *          primary characteristics. This macro iterate in increasing order
 *          of volt rail index.
 *
 * @param[in]   _pDomain40Prog  CLK_DOMAIN_40_PROG pointer
 * @param[out]  _pVfPrimary      LW2080_CTRL_CLK_CLK_DOMAIN_INFO_40_PROG_RAIL_VF_PRIMARY pointer
 * @param[out]  _index          LwU8 variable (LValue) to hold current index.
 */
#define CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(_pDomain40Prog, _pVfPrimary, _index)      \
    do {                                                                                            \
        FOR_EACH_INDEX_IN_MASK(32, (_index), (_pDomain40Prog)->railMask)                            \
        {                                                                                           \
            if (((_pDomain40Prog)->railVfItem[(_index)].type) !=                                    \
                    LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)                        \
            {                                                                                       \
                continue;                                                                           \
            }                                                                                       \
            (_pVfPrimary) = &((_pDomain40Prog)->railVfItem[(_index)].data.primary);

#define CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END                                             \
        }                                                                                           \
        FOR_EACH_INDEX_IN_MASK_END;                                                                 \
    } while (LW_FALSE)

/*!
 * @brief   Accessor macro to set the index at which the base lwrve
 *          frequency is trimmed by the enumeration max.
 *          @ref CLK_DOMAIN_40_PROG::baseFreqEnumMaxTrimVfPointIdx
 *
 * @param[in/out]   pDomain40Prog   CLK_DOMAIN_40_PROG pointer.
 * @param[in]       railIdx         Index into the per-voltrail information.
 * @param[in]       lwrveIdx        Index into the per-lwrve array.
 * @param[in]       newIdx          New value of the index to be set.
 */
#define clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxSet(pDomain40Prog, railIdx, lwrveIdx, newIdx)  \
    ((pDomain40Prog)->railVfItem[(railIdx)].data.secondary.baseFreqEnumMaxTrimVfPointIdx[(lwrveIdx)] = (newIdx))

/*!
 * @brief   Accessor macro to get the index at which the base lwrve
 *          frequency is trimmed by the enumeration max.
 *          @ref CLK_DOMAIN_40_PROG::baseFreqEnumMaxTrimVfPointIdx
 *
 * @param[in]   pDomain40Prog   CLK_DOMAIN_40_PROG pointer.
 * @param[in]   railIdx         Index into the per-volt-rail information.
 * @param[in]   lwrveIdx        Index into the per-lwrve array.
 *
 * @return  The index at which the base lwrve frequency was trimmed
 *          due to the enumeration max frequency.
 */
#define clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxGet(pDomain40Prog, railIdx, lwrveIdx)          \
    ((pDomain40Prog)->railVfItem[(railIdx)].data.secondary.baseFreqEnumMaxTrimVfPointIdx[(lwrveIdx)])

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Strct containing params specific to primary characteristics of programmable
 * clock domain.
 */
typedef struct
{
    /*!
     * First index into the Clock VF Relationship Table for this CLK_DOMAIN.
     */
    LwU8                clkVfRelIdxFirst;
    /*!
     * Last index into the Clock VF Relationship Table for this CLK_DOMAIN.
     */
    LwU8                clkVfRelIdxLast;
    /*!
     * Mask indexes of CLK_DOMAINs which are SECONDARIES to this PRIMARY CLK_DOMAIN.
     */
    BOARDOBJGRPMASK_E32 secondaryDomainsMask;
    /*!
     * Mask indexes of CLK_DOMAINs that belongs to this primary-secondary group.
     */
    BOARDOBJGRPMASK_E32 primarySecondaryDomainsMask;
} CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY;

/*!
 * Strct containing params specific to secondary characteristics of programmable
 * clock domain.
 */
typedef struct
{
    /*!
     * CLK_DOMAIN index of primary CLK_DOMAIN.
     */
    LwU8            primaryIdx;

    /*!
     * Index of the last point in the base lwrve of the domain before which
     * the base lwrve frequency starts to cross the domain's enumeration
     * maximum frequency. This is used to indicate when to start trimming
     * the offset-adjusted lwrve frequency.
     *
     * Within each clock domain, we have one for each Voltage rail, and within
     * each voltage rail, we have one for each VF lwrve (primary and secondary
     * slowdown lwrve). Hence this item is an array of size MAX_LWRVES and
     * stored within volt-rail data.
     *
     * Due to the nature of the problem, this only oclwrs with secondary
     * domains (since their lwrves are not "fresh" from VFE equations but
     * derived from their primary or primary domains), hence this is present
     * in the secondary domain voltrail information.
     *
     * Intended as a permanent non-WAR bugfix for bug 200694193
     * See here: https://lwbugs/200694193
     * and here: https://confluence.lwpu.com/display/RMPER/Additional+Information#AdditionalInformation-OffsetsandVFpointcachinginPMU-LwrrentSituation
     *
     * The value will be an index into the VF point array if it
     * is valid; and if invalid, it will point to
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID
     */
    LwBoardObjIdx   baseFreqEnumMaxTrimVfPointIdx[LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_MAX];
} CLK_DOMAIN_40_PROG_RAIL_VF_SECONDARY;

/*!
 * A union of all available characteristics.
 */
typedef union
{
    /*!
     * @ref CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
     */
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY      primary;
    /*!
     * @ref CLK_DOMAIN_40_PROG_RAIL_VF_SECONDARY
     */
    CLK_DOMAIN_40_PROG_RAIL_VF_SECONDARY    secondary;
} CLK_DOMAIN_40_PROG_RAIL_VF_DATA;

/*!
 * Strct containing params specific to secondary characteristics of programmable
 * clock domain.
 */
typedef struct
{
    /*!
     * @ref LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_<xyz>
     */
    LwU8    type;
    /*!
     * Position of clock domain in a tightly packed array of primary - secondary clock
     * domain V and/or F tuples.
     */
    LwU8    clkPos;

    /*!
     * @ref CLK_DOMAIN_40_PROG_RAIL_VF_DATA
     */
    CLK_DOMAIN_40_PROG_RAIL_VF_DATA data;
} CLK_DOMAIN_40_PROG_RAIL_VF_ITEM;

/*!
 * Clock Domain 40 Prog structure.  Contains information specific to a
 * Programmable Clock Domain.
 */
typedef struct
{
    /*!
     * CLK_DOMAIN_3X super class.  Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_3X   super;

    /*!
     * CLK_DOMAIN_PROG super class.
     */
    CLK_DOMAIN_PROG prog;

    /*!
     * First index into the Clock Enumeration Table for this CLK_DOMAIN.
     */
    LwU8    clkEnumIdxFirst;
    /*!
     * Last index into the Clock Enumeration Table for this CLK_DOMAIN.
     */
    LwU8    clkEnumIdxLast;
    /*!
     * Pre-Volt ordering index for clock programming changes.
     */
    LwU8    preVoltOrderingIndex;
    /*!
     * Post-Volt ordering index for clock programming changes.
     */
    LwU8    postVoltOrderingIndex;
   /*!
     * Count of total number of (primary + secondary) lwrves supported on this clock domain.
     */
    LwU8    clkVFLwrveCount;
    /*!
     * Minimum frequency delta which can be applied to the CLK_DOMAIN.
     */
    LwS16   freqDeltaMinMHz;
    /*!
     * Maximum frequency delta which can be applied to the CLK_DOMAIN.
     */
    LwS16   freqDeltaMaxMHz;
    /*!
     * Factory OC frequency delta. We always apply this delta regardless of the
     * frequency delta range reg@ freqDeltaMinMHz and ref@ freqDeltaMaxMHz as
     * long as the clock programming entry has the OC feature enabled in it.
     * ref@ CLK_VF_REL::bOCOVEnabled
     */
    LW2080_CTRL_CLK_FREQ_DELTA      factoryDelta;

    /*!
     * GRD OC frequency delta. This delta is programmed by POR team in clocks
     * table. We will respect this delta IFF client explicitly opted for it by
     * setting ref@ CLK_DOMAINS::bGrdFreqOCEnabled
     */
    LW2080_CTRL_CLK_FREQ_DELTA      grdFreqDelta;

    /*!
     * Mask of volt rails on which this clock domain has its Vmin.
     */
    LwU32                           railMask;
    /*!
     * Rail specific data for a given programmable clock domain.
     * ref@ CLK_DOMAIN_40_PROG_RAIL_VF_ITEM
     */
    CLK_DOMAIN_40_PROG_RAIL_VF_ITEM
            railVfItem[LW2080_CTRL_CLK_CLK_DOMAIN_PROG_RAIL_VF_ITEM_MAX];
    /*!
     * Local delta for given the CLK Domain
     */
    CLK_DOMAIN_DELTA                deltas;
    /*!
     * Clock Monitor specific information used to compute the threshold values.
     */
    LW2080_CTRL_CLK_CLK_DOMAIN_INFO_40_PROG_CLK_MON     clkMonInfo;
    /*!
     * Clock Monitors specific information used to override the threshold values.
     */
    LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON  clkMonCtrl;

    /*!
     * Data characterizing the FBVDD rail associated with this CLK_DOMAIN, if
     * valid.
     */
    LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_FBVDD_DATA fbvddData;
} CLK_DOMAIN_40_PROG;

/*!
 * @interface CLK_DOMAIN_40_PROG
 *
 * Helper interface to adjust the given voltage by the voltage delta of this
 * CLK_DOMAIN_40_PROG object.
 *
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in/out] pVoltuV
 *     Pointer in which caller specifies voltage to adjust and in which the
 *     adjusted voltage is returned.
 *
 * @return FLCN_OK
 *     Voltage successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain40ProgVoltAdjustDeltauV(fname) FLCN_STATUS (fname)(CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU32 *pVoltuV)

/*!
 * @interface CLK_DOMAIN_40_PROG
 *
 * Helper interface to adjust the given frequency by the frequency delta of this
 * CLK_DOMAIN_40_PROG object.
 *
 * @note This interface does NOT quantize the offset adjusted frequency.
 *
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies frequency to adjust and in which the
 *     adjusted frequency is returned.
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain40ProgFreqAdjustDeltaMHz(fname) FLCN_STATUS (fname)(CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU16 *pFreqMHz)

/*!
 * @interface CLK_DOMAIN_40_PROG
 *
 * Helper interface to adjust the given voltage by the voltage delta of this
 * CLK_DOMAIN_40_PROG object.
 *
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in/out] pVoltuV
 *     Pointer in which caller specifies voltage to adjust and in which the
 *     adjusted voltage is returned.
 *
 * @return FLCN_OK
 *     Voltage successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain40ProgOffsetVFVoltAdjustDeltauV(fname) FLCN_STATUS (fname)(CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwS32 *pVoltuV)

/*!
 * @interface CLK_DOMAIN_40_PROG
 *
 * Helper interface to adjust the given frequency by the frequency delta of this
 * CLK_DOMAIN_40_PROG object.
 *
 * @note This interface does NOT quantize the offset adjusted frequency.
 *
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies frequency to adjust and in which the
 *     adjusted frequency is returned.
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz(fname) FLCN_STATUS (fname)(CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwS16 *pFreqMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet                   (clkDomainGrpIfaceModel10ObjSet_40_PROG)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_40_PROG");

ClkDomainsCache                 (clkDomainsCache_40)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsCache_40");
ClkDomainsBaseVFCache           (clkDomainsBaseVFCache_40)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsBaseVFCache_40");
ClkDomainsOffsetVFCache         (clkDomainsOffsetVFCache_40)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsOffsetVFCache_40");

// CLK_DOMAIN interfaces
ClkDomainLoad                   (clkDomainLoad_40_PROG)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkDomainLoad_40_PROG");
ClkDomainCache                  (clkDomainCache_40_PROG)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainCache_40_PROG");
ClkDomainBaseVFCache            (clkDomainBaseVFCache_40_PROG)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainBaseVFCache_40_PROG");
ClkDomainOffsetVFCache          (clkDomainOffsetVFCache_40_PROG)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainOffsetVFCache_40_PROG");

// CLK DOMAIN PROG interfaces
ClkDomainProgVoltToFreq                 (clkDomainProgVoltToFreq_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltToFreq_40");
ClkDomainProgFreqToVolt                 (clkDomainProgFreqToVolt_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqToVolt_40");
ClkDomainProgFreqQuantize               (clkDomainProgFreqQuantize_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqQuantize_40");
ClkDomainProgFreqEnumIterate            (clkDomainProgFreqEnumIterate_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqEnumIterate_40");
ClkDomainProgVfMaxFreqMHzGet            (clkDomainProgVfMaxFreqMHzGet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVfMaxFreqMHzGet_40");
ClkDomainProgClientFreqDeltaAdj         (clkDomainProgClientFreqDeltaAdj_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgClientFreqDeltaAdj_40");
ClkDomainProgVoltToFreqTuple            (clkDomainProgVoltToFreqTuple_40)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkDomainProgVoltToFreqTuple_40");
ClkDomainProgPreVoltOrderingIndexGet    (clkDomainProgPreVoltOrderingIndexGet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgPreVoltOrderingIndexGet_40");
ClkDomainProgPostVoltOrderingIndexGet   (clkDomainProgPostVoltOrderingIndexGet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgPostVoltOrderingIndexGet_40");
ClkDomainProgIsFreqMHzNoiseAware        (clkDomainProgIsFreqMHzNoiseAware_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsFreqMHzNoiseAware_40");
ClkDomainsProgFreqPropagate             (clkDomainsProgFreqPropagate_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsProgFreqPropagate_40");
ClkDomainProgFactoryDeltaAdj            (clkDomainProgFactoryDeltaAdj_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFactoryDeltaAdj_40");
ClkDomainProgVoltRailIdxGet             (clkDomainProgVoltRailIdxGet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltRailIdxGet_40");
ClkDomainProgFbvddDataValid             (clkDomainProgFbvddDataValid_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFbvddDataValid_40");
ClkDomainProgFbvddPwrAdjust             (clkDomainProgFbvddPwrAdjust_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFbvddPwrAdjust_40");
ClkDomainProgFbvddFreqToVolt            (clkDomainProgFbvddFreqToVolt_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFbvddFreqToVolt_40");
ClkDomainProgIsSecVFLwrvesEnabled       (clkDomainProgIsSecVFLwrvesEnabled_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsSecVFLwrvesEnabled_40");

// CLK DOMAIN 40 PROG interfaces
ClkDomain40ProgVoltAdjustDeltauV            (clkDomain40ProgVoltAdjustDeltauV)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomain40ProgVoltAdjustDeltauV");
ClkDomain40ProgFreqAdjustDeltaMHz           (clkDomain40ProgFreqAdjustDeltaMHz)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomain40ProgFreqAdjustDeltaMHz");
ClkDomain40ProgOffsetVFVoltAdjustDeltauV    (clkDomain40ProgOffsetVFVoltAdjustDeltauV)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomain40ProgOffsetVFVoltAdjustDeltauV");
ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz   (clkDomain40ProgOffsetVFFreqAdjustDeltaMHz)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomain40ProgOffsetVFFreqAdjustDeltaMHz");
LW2080_CTRL_CLK_FREQ_DELTA *clkDomain40ProgFinalFactoryDeltaGet(CLK_DOMAIN_40_PROG *pDomain40Prog)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain40ProgFinalFactoryDeltaGet");
CLK_DOMAIN_40_PROG *        clkDomain40ProgGetByVfPointIdx(LwBoardObjIdx vfPointIdx, LwU8 lwrveIdx)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain40ProgGetByVfPointIdx");

/* ------------------------ Include Derived Types -------------------------- */

/* ------------------------ Inline Functions ------------------------------- */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
clkDomain40ProgRailIdxValid
(
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU8 railIdx
)
{
    return (railIdx < LW_ARRAY_ELEMENTS(pDomain40Prog->railVfItem)) &&
           (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) !=
                LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_NONE);
}

#endif // CLK_DOMAIN_40_PROG_H
