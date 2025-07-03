/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_H
#define CLK_VF_POINT_H

/*!
 * @file clk_vf_point.h
 * @brief @copydoc clk_vf_point.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_VF_POINT CLK_VF_POINT, CLK_VF_POINT_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_VF_POINTS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 *
 * The type colwersion code expects the class ID to match the boardObjGrp
 * data location. However, the CLK_VF_POINT does not follow that approach.
 * Instead of the one data location for a boardObjGrp, it has two (one for PRI
 * and one for SEC).
 *
 * Since the vtable is the same for both the PRI and SEC groups, this group
 * is created to get around the limitations of the macros in the boardObjGrp
 * type-casting functionality.
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_VF_POINT                                 \
    BOARDOBJGRP_DATA_LOCATION_CLK_VF_POINT_PRI

/*!
 * Macro to locate primary CLK_VF_POINTS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_VF_POINT_PRI                             \
    (&Clk.vfPoints.super.super)

/*!
 * Macro to locate secondary CLK_VF_POINTS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_VF_POINT_SEC                             \
    (&Clk.vfPoints.sec.super.super)

/*!
 * Macro to get BOARDOBJ pointer from CLK_VF_POINTS BOARDOBJGRP.
 *
 * @param[in]  lwrveIdx    Vf lwrve index.
 * @param[in]  idx         CLK_VF_POINT index.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, idx)                         \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI) ?  \
        (CLK_VF_POINT_GET(PRI, (idx))) : (CLK_VF_POINT_GET(SEC, (idx))))

/*!
 * Macro to get BOARDOBJ pointer from primary CLK_VF_POINTS BOARDOBJGRP.
 *
 * @param[in]  type        CLK_VF_POINT BOARDOBJGRP type (PRI | SEC).
 * @param[in]  idx         CLK_VF_POINT index.
 *
 * We can't use BOARDOBJGRP_OBJ_GET() because the return type is different
 * from the type passed to BOARDOBJGRP_GRP_GET(). We still want the parameter
 * verification from boardObjGrpObjGet(), so just mimic what
 * BOARDOBJGRP_OBJ_GET() does.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define CLK_VF_POINT_GET(type, _objIdx)                                       \
    ((CLK_VF_POINT *)(boardObjGrpObjGet(                                       \
        BOARDOBJGRP_GRP_GET(CLK_VF_POINT##_##type), (_objIdx))))

/*!
 * _SUPER wrapper for boardObjIfaceModel10GetStatus
 */
#define clkVfPointIfaceModel10GetStatus_SUPER(pModel10, pBuf)                            \
    (boardObjIfaceModel10GetStatus(pModel10, pBuf))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Accessor to the Vf points cache counter. @ref vfPointsCacheCounter
 */
#define clkVfPointsVfPointsCacheCounterGet()                                  \
            Clk.vfPoints.vfPointsCacheCounter


/*!
 * @memberof CLK_VF_POINTS
 *
 * Mutator to the Vf points cache counter. @ref vfPointsCacheCounter
 * Increment the vf points cache counter by one.
 */
#define clkVfPointsVfPointsCacheCounterIncrement()                            \
    do {                                                                      \
        (Clk.vfPoints.vfPointsCacheCounter) =                                 \
            ((Clk.vfPoints.vfPointsCacheCounter) ==                           \
                (LW2080_CTRL_CLK_CLK_VF_POINT_CACHE_COUNTER_TOOLS - 1U)) ?    \
                0U : (Clk.vfPoints.vfPointsCacheCounter) + 1U;                \
    } while (LW_FALSE)

// Following functionality depends on ODP support.
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_BASE_VF_CACHE))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Accessor to the validity of cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 */
#define clkVfPointsVfCacheIsValidGet(_lwrveIdx, _cacheIdx)                    \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?           \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].bValid) :                         \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].bValid))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Accessor to the frequency of given clock domain from cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 * @param[in]  _vfIdx    VF point index.
 * @param[in]  _clkPos   Clock domain position in base VF tuple.
 */
#define clkVfPointsBaseVfCacheFreqGet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos)                            \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?                                     \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.freqTuple[(_clkPos)].freqMHz) :   \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.super.freqTuple[(_clkPos)].freqMHz))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Accessor to the voltage of given clock domain from cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 * @param[in]  _vfIdx    VF point index.
 * @param[in]  _clkPos   Clock domain position in base VF tuple.
 */
#define clkVfPointsBaseVfCacheVoltGet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos)            \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?                     \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.voltageuV) :     \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.super.voltageuV))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Mutator to the validity of cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 */
#define clkVfPointsVfCacheIsValidSet(_lwrveIdx, _cacheIdx)                      \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?             \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].bValid     = LW_TRUE) :             \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].bValid = LW_TRUE))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Mutator to the validity of cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 */
#define clkVfPointsVfCacheIsValidClear(_lwrveIdx, _cacheIdx)                    \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?             \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].bValid     = LW_FALSE) :            \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].bValid = LW_FALSE))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Mutator to the frequency of given clock domain from cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 * @param[in]  _vfIdx    VF point index.
 * @param[in]  _clkPos   Clock domain position in base VF tuple.
 */
#define clkVfPointsBaseVfCacheFreqSet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos, _freqMHz)                                      \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?                                                         \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.freqTuple[(_clkPos)].freqMHz     = (_freqMHz)) :     \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.super.freqTuple[(_clkPos)].freqMHz = (_freqMHz)))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Mutator to the voltage of given clock domain from cached VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 * @param[in]  _vfIdx    VF point index.
 * @param[in]  _clkPos   Clock domain position in base VF tuple.
 */
#define clkVfPointsBaseVfCacheVoltSet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos, _voltageuV)                    \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?                                         \
        (Clk.vfPoints.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.voltageuV     = (_voltageuV)) :      \
        (Clk.vfPoints.sec.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.super.voltageuV = (_voltageuV)))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Accessor to the base VF tuple of cached primary/secondary VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 * @param[in]  _vfIdx    VF point index.
 */
#define clkVfPointsBaseVfCacheBaseVFTupleGet(_lwrveIdx, _cacheIdx, _vfIdx)              \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?                     \
        (&Clk.vfPoints.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple) :              \
        (&Clk.vfPoints.sec.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple.super))

/*!
 * @memberof CLK_VF_POINTS
 *
 * Accessor to the base VF tuple of cached secondary VF lwrves.
 *
 * @param[in]  _lwrveIdx VF lwrve index (Primary vs Secondary).
 * @param[in]  _cacheIdx VF lwrve cache buffer index (temperature / step size).
 * @param[in]  _vfIdx    VF point index.
 */
#define clkVfPointsSecBaseVfCacheBaseVFTupleGet(_lwrveIdx, _cacheIdx, _vfIdx)               \
    (((_lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?                         \
        (NULL) :                                                                            \
        (&Clk.vfPoints.sec.pVfCache[(_cacheIdx)].data[(_vfIdx)].baseVFTuple))

#else
#define clkVfPointsVfCacheIsValidGet(_lwrveIdx, _cacheIdx)                                  \
            (LW_FALSE)  // Feature not supported!
#define clkVfPointsBaseVfCacheFreqGet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos)                \
            0U; do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsBaseVfCacheVoltGet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos)                \
            0U; do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsVfCacheIsValidSet(_lwrveIdx, _cacheIdx)                                  \
            do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsVfCacheIsValidClear(_lwrveIdx, _cacheIdx)                                \
            do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsBaseVfCacheFreqSet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos, _freqMHz)      \
            (void) (_freqMHz); do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsBaseVfCacheVoltSet(_lwrveIdx, _cacheIdx, _vfIdx, _clkPos, _voltageuV)    \
            (void) _voltageuV; do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsSecBaseVfCacheBaseVFTupleGet(_lwrveIdx, _cacheIdx, _vfIdx)               \
            (NULL); do { PMU_BREAKPOINT(); } while (LW_FALSE)
#define clkVfPointsBaseVfCacheBaseVFTupleGet(_lwrveIdx, _cacheIdx, _vfIdx)                  \
            (NULL); do { PMU_BREAKPOINT(); } while (LW_FALSE)
#endif

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point structure.  Defines a point on a PRIMARY CLK_DOMAIN's VF
 * lwrve - i.e. the lwrve is a collection of points as described by the set of
 * CLK_PROG entries corresponding to the CLK_DOMAIN.
 *
 * The RM will generate a set of CLK_VF_POINT objects corresponding to each
 * CLK_PROG object.  These objects will be evaluated and cached per
 * the VFE polling loop for later reference to the complete VF lwrve.
 */
struct CLK_VF_POINT
{
    /*!
     * BOARDOBJ super class.  Must always be the first element in the structure.
     */
    BOARDOBJ super;
};

/*!
 * @memberof CLK_VF_POINT
 *
 * Accessor to the voltage of a particular CLK_VF_POINT.
 *
 * @param[in]  pVfPoint     CLK_VF_POINT pointer
 * @param[in]  voltageType  Type of voltage requested - @ref LW2080_CTRL_CLK_VOLTAGE_TYPE_<XYZ>
 * @param[out] pVoltageUV   Pointer in which to return the requested voltage value.
 *
 * @note    This interface is ONLY supported on primary clock domains. This interface should
 * only be used to get the source voltage. For all other purposes, please use the regular
 * vf point accessor interface @ref ClkVfPointAccessor
 *
 * @return FLCN_OK
 *     Voltage successfully retrieved.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The CLK_VF_POINT object does not support the specified voltageType.
 */
#define ClkVfPointVoltageuVGet(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, LwU8 voltageType, LwU32 *pVoltageuV)

/*!
 * @interface CLK_VF_POINT
 *
 * Accessor to the base or offset tuple of a particular CLK_VF_POINT
 *
 * @param[in]  pVfPoint          CLK_VF_POINT pointer
 * @param[in]  bOffset           Type of tuple requested
 * @param[out] pVfPointTuple     LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TUPLE pointer
 *
 * @return FLCN_OK
 *     Tuple successfully retrieved
 * @return FLCN_... DM TODO : after implementation
 */
#define ClkVfPointClientVfTupleGet(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, LwBool bOffset, LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TUPLE *pVfPointTuple)

/*!
 * @interface CLK_VF_POINT
 *
 * Set freqDelta for particular CLK_VF_POINT
 *
 * @param[in] pVfPoint      CLK_VF_POINT pointer
 * @param[in] pFreqDelta    LW2080_CTRL_CLK_FREQ_DELTA pointer
 *
 * @return FLCN_OK
 *     Freq delta successfully updated
 * @return FLCN_... DM TODO : after implementation
 */
#define ClkVfPointClientFreqDeltaSet(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, LW2080_CTRL_CLK_FREQ_DELTA *pFreqDelta)

/*!
 * @interface CLK_VF_POINT
 *
 * Set volt delta for particular CLK_VF_POINT
 *
 * @param[in] pVfPoint      CLK_VF_POINT pointer
 * @param[in] voltDelta     Voltage delta that needs to be set
 *
 * @return FLCN_OK
 *     Voltage delta successfully updated
 * @return FLCN_... DM TODO : after implementation
 */
#define ClkVfPointClientVoltDeltaSet(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, LwS32 voltDelta)

/*!
 * @interface CLK_VF_POINT
 *
 * Set CPM offset for particular CLK_VF_POINT
 *
 * @param[in] pVfPoint                        CLK_VF_POINT pointer
 * @param[in] cpmMaxFreqOffsetOverrideMHz     CPM offset that needs to be set
 *
 * @return FLCN_OK
 *     CPM offset successfully updated
 * @return FLCN_... DM TODO : after implementation
 */
#define ClkVfPointClientCpmMaxOffsetOverrideSet(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, LwU16 cpmMaxFreqOffsetOverrideMHz)

/*!
 * Macro representing invalid VF cache index.
 */
#define CLK_CLK_VF_POINT_VF_CACHE_IDX_ILWALID       LW_U8_MAX

/*!
 * Clock VF Point base (POR) VF lwrve cache for primary VF lwrve.
 */
typedef struct
{
    LwBool bValid;

    struct
    {
        /*!
         * @ref LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE
         * Base VF Tuple represent the values that are input / output of VFE.
         */
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  baseVFTuple;
    } data[LW2080_CTRL_BOARDOBJGRP_E512_MAX_OBJECTS];

} CLK_VF_POINT_PRI_VF_CACHE;

/*!
 * Clock VF Point base (POR) VF lwrve cache for secondary VF lwrve.
 */
typedef struct
{
    LwBool bValid;

    struct
    {
        /*!
         * @ref LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE_SEC
         * Base VF Tuple represent the values that are input / output of VFE.
         */
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE_SEC  baseVFTuple;
    } data[LW2080_CTRL_BOARDOBJGRP_E512_MAX_OBJECTS];

} CLK_VF_POINT_SEC_VF_CACHE;

/*!
 * Set of secondary CLK_VF_POINTS.  Implements BOARDOBJGRP_E512.
 *
 * @ref CLK_VF_POINTS
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E512 super class representing secondary VF lwrve.
     * Must always be the first element in the structure.
     */
    BOARDOBJGRP_E512    super;

    /*!
     * Base VF lwrve cache array indexed by temperature.
     * index = roundDown(temperature / 5)
     */
    CLK_VF_POINT_SEC_VF_CACHE  *pVfCache;
} CLK_VF_POINTS_SEC;

/*!
 * Set of CLK_VF_POINTS.  Implements BOARDOBJGRP_E512.
 *
 * This is the collection of all CLK_VF_POINTS with no special ordering or
 * meaning beyond how they are indexed by CLK_PROG entries.  The set will be
 * evaluated and cached per the VFE polling loop for later reference for VF lwrve
 * lookup.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E512 super class representing primary VF lwrve.
     * Must always be the first element in the structure.
     */
    BOARDOBJGRP_E512    super;

    /*!
     * BOARDOBJGRP containing secondary VF lwrve.
     */
    CLK_VF_POINTS_SEC   sec;

    /*!
     * VF lwrve cache array indexed by temperature.
     * index = roundDown(temperature / 5)
     */
    CLK_VF_POINT_PRI_VF_CACHE  *pVfCache;

    /*!
     * Global param, Used to check if VF data is stale.
     * This counter will be incremented for every vf lwrve changes including
     * the changes in frequency or voltage offsets.
     */
    LwU32               vfPointsCacheCounter;
} CLK_VF_POINTS;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkVfPointBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler  (clkVfPointBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointBoardObjGrpIfaceModel10GetStatus");

BoardObjGrpIfaceModel10SetHeader (clkVfPointIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry  (clkVfPointIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointIfaceModel10SetEntry");
BoardObjGrpIfaceModel10GetStatusHeader (clkVfPointIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatusHeader");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfPointGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_SUPER");
BoardObjIfaceModel10GetStatus     (clkVfPointIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus");

// CLK_VF_POINT interfaces
ClkVfPointVoltageuVGet                     (clkVfPointVoltageuVGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet");
ClkVfPointClientVfTupleGet                 (clkVfPointClientVfTupleGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientVfTupleGet");
ClkVfPointClientVoltDeltaSet               (clkVfPointClientVoltDeltaSet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientVoltDeltaSet");
ClkVfPointClientFreqDeltaSet               (clkVfPointClientFreqDeltaSet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientFreqDeltaSet");
ClkVfPointClientCpmMaxOffsetOverrideSet    (clkVfPointClientCpmMaxOffsetOverrideSet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientCpmMaxOffsetOverrideSet");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_3x.h"
#include "clk/clk_vf_point_40.h"

#endif // CLK_VF_POINT_H
