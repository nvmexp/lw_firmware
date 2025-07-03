/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_REL_H
#define CLK_PROP_TOP_REL_H

/*!
 * @file clk_prop_top_rel.h
 * @brief @copydoc clk_prop_top_rel.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_PROP_TOP_REL CLK_PROP_TOP_REL, CLK_PROP_TOP_REL_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_PROP_TOP_RELS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_PROP_TOP_REL                            \
    (&Clk.propTopRels.super.super)

/*!
 * Helper macro to return a pointer to the CLK_PROP_TOP_RELS object.
 *
 * @return Pointer to the CLK_PROP_TOP_RELS object.
 */
#define CLK_CLK_PROP_TOP_RELS_GET()                                           \
    (&Clk.propTopRels)

/*!
 * @brief       Accesor for a CLK_PROP_TOP_REL BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _idx  BOARDOBJGRP index for a CLK_PROP_TOP_REL BOARDOBJ.
 *
 * @return      Pointer to a CLK_PROP_TOP_REL object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLK_PROP_TOP_REL
 *
 * @public
 */
#define CLK_PROP_TOP_REL_GET(_idx)                                            \
    (BOARDOBJGRP_OBJ_GET(CLK_PROP_TOP_REL, (_idx)))

/*!
 * @brief       Accesor for a src frequency in the table relationship tuple.
 *
 * @param[in]   tableRelIdx  Table Relationship index.
 *
 * @return      source frequency in MHz.
 *
 * @memberof    CLK_PROP_TOP_RELS
 *
 * @public
 */
#define clkPropTopRelsTableRelTupleFreqMHzSrcGet(tableRelIdx)                 \
    (((tableRelIdx) < Clk.propTopRels.tableRelTupleCount) ?                   \
        Clk.propTopRels.tableRelTuple[(tableRelIdx)].freqMHzSrc : LW_U16_MIN)

/*!
 * @brief       Accesor for a src offset adj. frequency in the table relationship tuple.
 *
 * @param[in]   tableRelIdx  Table Relationship index.
 *
 * @return      source frequency in MHz.
 *
 * @memberof    CLK_PROP_TOP_RELS
 *
 * @public
 */
#define clkPropTopRelsTableRelOffAdjTupleFreqMHzSrcGet(tableRelIdx)           \
    (((tableRelIdx) < Clk.propTopRels.tableRelTupleCount) ?                   \
        Clk.propTopRels.offAdjTableRelTuple[(tableRelIdx)].freqMHzSrc : LW_U16_MIN)

/*!
 * @brief       Accesor for a dst frequency in the table relationship tuple.
 *
 * @param[in]   tableRelIdx  Table Relationship index.
 *
 * @return      dst frequency in MHz.
 *
 * @memberof    CLK_PROP_TOP_RELS
 *
 * @public
 */
#define clkPropTopRelsTableRelTupleFreqMHzDstGet(tableRelIdx)                 \
    (((tableRelIdx) < Clk.propTopRels.tableRelTupleCount) ?                   \
        Clk.propTopRels.tableRelTuple[(tableRelIdx)].freqMHzDst : LW_U16_MIN)

/*!
 * @brief       Accesor for a dst offset adj. frequency in the table relationship tuple.
 *
 * @param[in]   tableRelIdx  Table Relationship index.
 *
 * @return      dst frequency in MHz.
 *
 * @memberof    CLK_PROP_TOP_RELS
 *
 * @public
 */
#define clkPropTopRelsTableRelOffAdjTupleFreqMHzDstGet(tableRelIdx)           \
    (((tableRelIdx) < Clk.propTopRels.tableRelTupleCount) ?                   \
        Clk.propTopRels.offAdjTableRelTuple[(tableRelIdx)].freqMHzDst : LW_U16_MIN)

/*!
 * @brief       Accesor for a source clock domain index.
 *
 * @param[in]   pPropTopRel  CLK_PROP_TOP_REL pointer.
 *
 * @return      Source clock domain index.
 *
 * @memberof    CLK_PROP_TOP_REL
 *
 * @public
 */
#define clkPropTopRelClkDomainIdxSrcGet(pPropTopRel)                          \
    ((pPropTopRel)->clkDomainIdxSrc)

/*!
 * @brief       Accesor for a destination clock domain index.
 *
 * @param[in]   pPropTopRel  CLK_PROP_TOP_REL pointer.
 *
 * @return      Destination clock domain index.
 *
 * @memberof    CLK_PROP_TOP_REL
 *
 * @public
 */
#define clkPropTopRelClkDomainIdxDstGet(pPropTopRel)                          \
    ((pPropTopRel)->clkDomainIdxDst)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Propagation Topology Relationship class params.
 */
struct CLK_PROP_TOP_REL
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ    super;

    /*!
     * Source Clock Domain Index.
     */
    LwU8        clkDomainIdxSrc;

    /*!
     * Destination Clock Domain Index.
     */
    LwU8        clkDomainIdxDst;

    /*!
     * Boolean tracking whether bidirectional relationship enabled.
     */
    LwBool      bBiDirectional;
};

/*!
 * Clock Propagation TopRel group params.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E255 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E255    super;

    /*!
     * Count of valid table relationship tuple array entries.
     */
    LwU8                tableRelTupleCount;

    /*!
     * Array of frequency tuple for table based clock propagation topology relationships.
     * Here valid indexes corresponds to [0, tableRelTupleCount]
     */
    LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE
        tableRelTuple[LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE_MAX];

    /*!
     * Offset adjusted array of frequency tuple for table based clock propagation
     * topology relationships. Here valid indexes corresponds to [0, tableRelTupleCount]
     */
    LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE
        offAdjTableRelTuple[LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE_MAX];
} CLK_PROP_TOP_RELS;

/*!
 * @interface CLK_PROP_TOP_RELS
 *
 * Helper interface to cache the clock propagation frequency after all adjustement
 * with frequency offsets.
 *
 * @return FLCN_OK
 *     The propagation frequency was successfully cached.
 * @return Other errors
 *     An unexpected error coming from higher functions.
 */
#define ClkPropTopRelsCache(fname) FLCN_STATUS (fname)(void)

/*!
 * @interface CLK_PROP_TOP_REL
 *
 * Helper interface to cache the clock propagation frequency after all adjustement
 * with frequency offsets.
 *
 * @param[in]     pPropTopRel   CLK_PROP_TOP_REL pointer
 *
 * @return FLCN_OK
 *     The propagation frequency was successfully cached.
 * @return Other errors
 *     An unexpected error coming from higher functions.
 */
#define ClkPropTopRelCache(fname) FLCN_STATUS (fname)(CLK_PROP_TOP_REL *pPropTopRel)

/*!
 * @interface CLK_PROP_TOP_REL
 *
 * @brief   This interface will be called from clock propagation topology interface
 *          to propagate frequency from either Src -> Dst or Dst -> Src. The Src and
 *          Dst clock domain are fixed as per the VBIOS POR of this clock propagation
 *          topology relationship.
 *
 * @param[in]     pPropTopRel   CLK_PROP_TOP_REL pointer
 * @param[in]     bSrcToDstProp
 *      Set if client requesting Src -> Dst frequency propagation.
 *      LW_TRUE   Src -> Dst frequency propagation
 *      LW_FALSE  Dst -> Src frequency propagation
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies the input frequency to propagate and in
 *     which the function will return the propagated frequency.
 *
 * @return FLCN_OK
 *     Frequency successfully propagated per the clk propagation relationship.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     This CLK_PROP_TOP_REL object does not implement this interface. This
 *     is a coding error.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     bSrcToDstProp is FALSE but bBiDirectional is also FLASE.
 * @return FLCN_ERR_OUT_OF_RANGE
 *     If we cannot propagate the frequency due to out of range input frequency.
 */
#define ClkPropTopRelFreqPropagate(fname) FLCN_STATUS (fname)(CLK_PROP_TOP_REL *pPropTopRel, LwBool bSrcToDstProp, LwU16 *pFreqMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler       (clkPropTopRelBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopRelGrpSet");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkPropTopRelGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopRelGrpIfaceModel10ObjSet_SUPER");

// CLK_PROP_TOP_RELS interfaces
ClkPropTopRelsCache          (clkPropTopRelsCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkPropTopRelsCache");

// CLK_PROP_TOP_REL interfaces
ClkPropTopRelFreqPropagate  (clkPropTopRelFreqPropagate)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopRelFreqPropagate");
ClkPropTopRelCache          (clkPropTopRelCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkPropTopRelCache");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prop_top_rel_1x_ratio.h"

#endif // CLK_PROP_TOP_REL_H
