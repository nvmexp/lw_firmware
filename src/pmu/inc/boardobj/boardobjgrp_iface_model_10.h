/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRP_IFACE_MODEL_10_H
#define BOARDOBJGRP_IFACE_MODEL_10_H

#include "g_boardobjgrp_iface_model_10.h"

#ifndef G_BOARDOBJGRP_IFACE_MODEL_10_H
#define G_BOARDOBJGRP_IFACE_MODEL_10_H

/*!
 * @file    boardobjgrp_iface_model_10.h
 *
 * @brief   Provides PMU-specific definitions for version 1.0 of the BOARDOBJGRP
 *          infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobj_fwd.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrpmask.h"
#include "boardobj/boardobj_iface_model_10.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc BoardObjGrpIfaceModel10Set
 *
 * Wrapper allowing caller to specify the type of the target group.
 *
 * @pre     Caller must ensure that requested class indeed uses BOARDOBJGRP.
 */
#define BOARDOBJGRP_IFACE_MODEL_10_SET(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables) \
    boardObjGrpIfaceModel10Set_##_grpType(                                     \
        boardObjGrpIfaceModel10FromBoardObjGrpGet(                             \
            (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(_class)),                       \
        LW2080_CTRL_BOARDOBJGRP_TYPE_##_grpType, (_pBuffer),                   \
        (_hdrFunc), RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.hdr),          \
        (_entryFunc), RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.objects[0]), \
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_ssElement), (_ovlIdxDmem), NULL,   \
        BOARDOBJGRP_CONSTRUCT_VIRTUAL_TABLES_ARGS(_ppObjectVtables))


/*!
 * @copydoc BoardObjGrpIfaceModel10Set
 *
 * Wrapper allowing caller to specify the type of the target group. Uses the
 * Auto DMA feature, which automatically DMAs the entire subsurface into / out
 * of DMEM.
 *
 * @pre     Caller must ensure that requested class indeed uses BOARDOBJGRP.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
#define BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables) \
        boardObjGrpIfaceModel10SetAutoDma_##_grpType(                              \
            boardObjGrpIfaceModel10FromBoardObjGrpGet(                             \
                (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(_class)),                       \
            LW2080_CTRL_BOARDOBJGRP_TYPE_##_grpType, (_pBuffer),                   \
            (_hdrFunc), RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.hdr),          \
            (_entryFunc), RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.objects[0]), \
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_ssElement), (_ovlIdxDmem), NULL,   \
            BOARDOBJGRP_CONSTRUCT_VIRTUAL_TABLES_ARGS(_ppObjectVtables))
#else
#define BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables) \
    BOARDOBJGRP_IFACE_MODEL_10_SET(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables)
#endif

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatus
 *
 * Wrapper allowing caller to specify target class rather than group pointer.
 *
 * @pre     Caller must ensure that requested class indeed uses BOARDOBJGRP.
 */
#define BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement) \
    boardObjGrpIfaceModel10GetStatus_##_grpType(                               \
        boardObjGrpIfaceModel10FromBoardObjGrpGet(                             \
            (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(_class)),                   \
        _pBuffer,                                                              \
        _hdrFunc, RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.hdr),            \
        _entryFunc, RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.objects[0]),   \
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_ssElement), NULL)

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatus
 *
 * Wrapper allowing caller to specify target class rather than group pointer.
 * Uses the AutoDMA feature, which prefetches / writes back entire content
 * of subsurface buffer before / after the RPC handler exelwtes.
 *
 * @pre     Caller must ensure that requested class indeed uses BOARDOBJGRP.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
#define BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement) \
    boardObjGrpIfaceModel10GetStatusAutoDma_##_grpType(                                               \
        boardObjGrpIfaceModel10FromBoardObjGrpGet(                                                    \
            (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(_class)),                                              \
        _pBuffer,                                                                                     \
        _hdrFunc, RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.hdr),                                   \
        _entryFunc, RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_ssElement.objects[0]),                          \
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_ssElement), NULL)
#else
#define BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement) \
    BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(_grpType, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement)
#endif

/* ------------------------ Datatypes --------------------------------------- */
/*!
 * @brief   Implementation-specific handler for the RM_PMU_BOARDOBJ_CMD_GRP
 *          interface.
 *
 * @param[in]   pBuffer Pointer to the PMU_DMEM_BUFFER structure which describes
 *                      the DMEM buffer used by the PMU while processing
 *                      BOARDOBJGRP elements.
 *
 * @return FLCN_OK  if RM payload structure successfully handled for this
 *                  BOARDOBJGRP.
 * @return OTHERS   unexpected errors, some error oclwrred during parsing the RM
 *                  payload.
 */
#define BoardObjGrpIfaceModel10CmdHandler(fname) FLCN_STATUS (fname)(PMU_DMEM_BUFFER *pBuffer)
#define BoardObjGrpIfaceModel10CmdHandlerFunc(fname) FLCN_STATUS (fname)(PMU_DMEM_BUFFER *pBuffer)

/*!
 * @brief  Specifies a subsurface location for the managed DMA feature, as well
 *         as how many bytes to read from / write back to this subsurface.
 */
typedef struct
{
    /*!
     * Memory offset of the start of the subsurface used by the BOARDOBJGRP
     */
    LwU32 offset;

    /*!
     * Maximum number of BOARDOBJs in this group (fixed at compile time).
     */
    LwBoardObjIdx grpSize;

    /*!
     * Size of the header of the relevant BOARDOBJGRP
     */
    LwU16 hdrSize;

    /*!
     * Size of each BOARDOBJ entry in the relevant BOARDOBJGRP
     */
    LwU32 entrySize;
} BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SUBSURF_LOC;

/*!
 * @brief Structure containing an RPC command handler and any associated
 *        metadata (such as which subsurface structure is used).
 */
typedef struct
{
    /*!
     * @brief Function pointer handling the BOARD_OBJ_GRP_CMD RPC
     */
    BoardObjGrpIfaceModel10CmdHandlerFunc            (*pHandler);

#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
    /*!
     * @brief The subsurface location to pre-fetch / write-back from
     *        (if the Auto DMA feature is enabled).
     */
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SUBSURF_LOC   subsurfLoc;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_AUTO_DMA)
} BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_DESC;

/*!
 * @brief   Structure defining GRP CMD handlers.
 *
 * An array of these structures will be passed to
 * @ref BoardObjGrpIfaceModel10CmdHandlerDispatch() by the implementing engine, specifying
 * how the various BOARDOBJGRP types should be handled.
 */
typedef struct
{
    /*!
     * @brief   Unique ID of BOARDOBJGRP type per the implementing engine.
     */
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    classId;

    /*!
     * @brief   Array of handler descriptors handling the BOARD_OBJ_GRP_CMD RPCs
     *          indexed by the RPC's commandId (@ref
     *          RM_PMU_BOARDOBJGRP_CMD_XYZ).
     */
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_DESC  cmdDesc[RM_PMU_BOARDOBJGRP_CMD__COUNT];
} BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY;


/*!
 * @brief Creates an empty SUBSURF_LOC when Auto DMA will not be used
 *        for a particular RPC
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY {0, 0, 0, 0},
#else
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY
#endif

/*!
 * @brief Creates a SUBSURF_LOC for a BoardObjGrp operation
 *        to be used with the Auto DMA feature.
 *
 * @param[in]       setName     The subsurface name for RPC
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC(setName)                                \
    {                                                                          \
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(setName),                           \
        LW_ARRAY_ELEMENTS(((RM_PMU_SUPER_SURFACE *)NULL)->setName.objects),   \
        RM_PMU_SUPER_SURFACE_MEMBER_SIZE(setName.hdr),                         \
        RM_PMU_SUPER_SURFACE_MEMBER_SIZE(setName.objects[0]),                  \
    },
#else
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC(setName)
#endif

/*!
 * @brief Creates an instance of a BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY
 *
 * @param[in]   _unit           Unit of the created GRP CMD handler
 * @param[in]   _class          Class of the created GRP CMD handler
 * @param[in]   _funcSet        Function to handle BoardObjGrpSet RPC
 * @param[in]   _funcGetStatus  Function to handle BoardObjGrpIfaceModel10GetStatus RPC
 */
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(_unit, _class, _funcSet, _funcGetStatus)          \
    {                                                                                    \
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(_unit, _class),                                 \
        {                                                                                \
            {                                                                            \
                _funcSet,                                                                \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY                                     \
            },                                                                           \
            {                                                                            \
                _funcGetStatus,                                                          \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY                                     \
            },                                                                           \
        },                                                                               \
    }

/*!
 * @brief Creates an instance of a BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY that only contains a Set
 *        Function
 *
 * @param[in]   _unit           Unit of the created GRP CMD handler
 * @param[in]   _class          Class of the created GRP CMD handler
 * @param[in]   _funcSet        Function to handle BoardObjGrpSet RPC
 */
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_SET_ONLY(_unit, _class, _funcSet)                 \
    {                                                                                    \
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(_unit, _class),                                 \
        {                                                                                \
            {                                                                            \
                _funcSet,                                                                \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY                                     \
            },                                                                           \
            {                                                                            \
                NULL,                                                                    \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY                                     \
            },                                                                           \
        },                                                                               \
    }

/*!
 * @brief Creates an instance of a BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY
 *
 * @param[in]   _unit                 Unit of the created GRP CMD handler
 * @param[in]   _class                Class of the created GRP CMD handler
 * @param[in]   _funcSet              Function to handle BoardObjGrpSet RPC
 * @param[in]   _subsurfaceSet        Subsurface used by BoardObjGrpSet RPC
 * @param[in]   _funcGetStatus        Function to handle BoardObjGrpIfaceModel10GetStatus RPC
 * @param[in]   _subsurfaceGetStatus  Subsurface used by BoardObjGrpIfaceModel10GetStatus RPC
 */
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(_unit, _class, _funcSet, _subsurfaceSet, \
    _funcGetStatus, _subsurfaceGetStatus)                                                \
    {                                                                                    \
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(_unit, _class),                                 \
        {                                                                                \
            {                                                                            \
                _funcSet,                                                                \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC(_subsurfaceSet)                           \
            },                                                                           \
            {                                                                            \
                _funcGetStatus,                                                          \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC(_subsurfaceGetStatus)                     \
            },                                                                           \
        },                                                                               \
    }

/*!
 * @brief Creates an instance of a BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY that only contains a Set
 *        Function
 *
 * @param[in]   _unit           Unit of the created GRP CMD handlerb
 * @param[in]   _class          Class of the created GRP CMD handler
 * @param[in]   _funcSet        Function to handle BoardObjGrpSet RPC
 * @param[in]   _subsurfaceSet  Subsurface used by BoardObjGrpSet RPC
 */
#define BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(_unit, _class,                  \
        _funcSet, _subsurfaceSet)                                                        \
    {                                                                                    \
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(_unit, _class),                                 \
        {                                                                                \
            {                                                                            \
                _funcSet,                                                                \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC(_subsurfaceSet)                           \
            },                                                                           \
            {                                                                            \
                NULL,                                                                    \
                BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_SS_LOC_EMPTY                                     \
            },                                                                           \
        },                                                                               \
    }

/*!
 * @brief   BOARDOBJGRP global dispatcher for handlers of
 *          RM_PMU_BOARDOBJGRP_CMD_GRP_SET/_GET.
 *
 * This is called from BoardObjGrpCmd specifying a RM_PMU_DMEM to be dispatched
 * to the handler associated with classId.
 *
 * @param[in]   classId     Unique ID of BOARDOBJGRP type per the implementing
 *                          engine.
 * @param[in]   pBuffer     Pointer to the PMU_DMEM_BUFFER structure which
 *                          describes the DMEM buffer used by the PMU while
 *                          processing BOARDOBJGRP elements.
 * @param[in]   numEntries  Number of elements specified in the @ref pEntries
 *                          array.
 * @param[in]   pEntries    Array of BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY structures
 *                          specifying how the BOARDOBJGRP types should be
 *                          handled.
 * @param[in]   commandId   Requested command ID (@ref
 *                          RM_PMU_BOARDOBJGRP_CMD_ENUM).
 *
 * @return FLCN_OK                  PMU successfully handled the BOARDOBJGRP
 *                                  sent from RM.
 * @return FLCN_ERR_NOT_SUPPORTED   No handler found for the specified @ref
 *                                  RM_PMU_BOARDOBJ_CMD_GRP::type.
 * @return OTHERS                   unexpected errors, errors returned by the
 *                                  engine-specific handlers.
 */
#define BoardObjGrpIfaceModel10CmdHandlerDispatch(fname) FLCN_STATUS (fname)(LW2080_CTRL_BOARDOBJGRP_CLASS_ID classId, PMU_DMEM_BUFFER *pBuffer, LwU8 numEntries, BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY *pEntries, RM_PMU_BOARDOBJGRP_CMD commandId)

/*!
 * @brief   BoardObjGrp header construct function prototype.
 *
 * BOARDOBJGRP child objects can extend this function to parse child specific
 * data.
 *
 * @param[out]  pBObjGrp    Pointer to the group receiving parsed data.
 * @param[in]   pHdrDesc    Pointer to the group header RM_PMU interface data.
 *
 * @return FLCN_OK  Header is parsed successfully.
 * @return OTHERS   Implementation specific error code.
 */
#define BoardObjGrpIfaceModel10SetHeader(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, RM_PMU_BOARDOBJGRP *pHdrDesc)

/*!
 * @brief   BoardObjGrp Entry construction prototype.
 *
 * @param[out]  pBObjGrp    Pointer to the group receiving parsed data.
 * @param[out]  ppBoardObj  Location to store pointer to newly created object.
 * @param[in]   pBuf        Buffer read from DMA to be parsed.
 *
 * @return FLCN_OK  Entry is parsed successfully.
 */
#define BoardObjGrpIfaceModel10SetEntry(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, BOARDOBJ **ppBoardObj, RM_PMU_BOARDOBJ *pBuf)

/*!
 * @brief   Constructor prototype for BOARDOBJGRP.
 *
 * All Objects should implement the @ref BoardObjGrpIfaceModel10Set, @ref
 * BoardObjGrpIfaceModel10SetHeader(), and @ref BoardObjGrpIfaceModel10SetEntry() interfaces
 * for their own construction code.
 *
 * @param[in]   pBoardObjGrp        Pointer to the allocated BOARDOBJGRP to
 *                                  construct
 * @param[in]   groupType           LW2080_CTRL_BOARDOBJGRP_TYPE_E<xyz> type ID
 *                                  of the allocated group.
 * @param[in]   pBuffer             Pointer to the PMU_DMEM_BUFFER structure
 *                                  used while parsing.
 * @param[in]   hdrFunc             Pointer to Header function that will parse
 *                                  the header info.
 * @param[in]   hdrSize             Size of Header.
 * @param[in]   entryFunc           Pointer to Entry function that will parse
 *                                  each entry.
 * @param[in]   entrySize           Size of Entry.
 * @param[in]   ssOffset            Offset into the Super Surface where the
 *                                  object is located. An offset of 0 indicates
 *                                  it is not in the Super Surface.
 * @param[in]   ovlIdxDmem          Index of the DMEM overlay which is used
 *                                  during memory allocation as well as to
 *                                  access data from that overlay by
 *                                  attaching/detaching it.
 * @param[in]   pMask               Pointer to mask of objects to be traversed.
 *                                  Can by modified, if needed.
 * @param[in]   ppObjectVtables     Array of BOARDOBJ_VIRTUAL_TABLE pointers
 *                                  representing the vtables for each object
 *                                  type belonging to the class.
 * @param[in]   numObjectVtables    The number of entries in ppObjectVtables.
 *
 * @return FLCN_OK  This BOARDOBJGRP is constructed successfully.
 * @return OTHERS   Implementation specific error oclwrs.
 */
#define BoardObjGrpIfaceModel10Set(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, LwU8 groupType, PMU_DMEM_BUFFER *pBuffer, BoardObjGrpIfaceModel10SetHeader(hdrFunc), LwLength hdrSize, BoardObjGrpIfaceModel10SetEntry(entryFunc), LwLength entrySize, LwU32 ssOffset, LwU8 ovlIdxDmem, BOARDOBJGRPMASK *pMask, BOARDOBJ_VIRTUAL_TABLE **ppObjectVtables, LwLength numObjectVtables)

/*!
 * @brief   BoardObjGrp Header GetStatus prototype.
 *
 * @param[in]   pBoardObjGrp    BOARDOBJGRP pointer
 * @param[out]  pBuf            Buffer for DMA read/write the header info.
 * @param[in]   pMask           Pointer to mask of objects to be traversed. Can
 *                              be modified, if needed.
 *
 * @return FLCN_OK  Header is queried successfully.
 */
#define BoardObjGrpIfaceModel10GetStatusHeader(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, RM_PMU_BOARDOBJGRP *pBuf, BOARDOBJGRPMASK *pMask)

/*!
 * @brief BOARDOBJGRP GET_STATUS CMD handler.
 *
 * Iterates over the BOARDOBJGRP to call the GET_STATUS (formerly known as
 * "QUERY") interface to retrieve both global and per-BOARDOBJ dynamic status
 * data to be sent back to the RM.
 *
 * Classes which implement BOARDOBJGRP should also implement handlers for @ref
 * BoardObjGrpIfaceModel10GetStatusHeader and @ref BoardObjIfaceModel10GetStatus to pass to this
 * function.
 *
 * Queries the latest state of Object functionality.
 * It will first call hdrFunc to issue DMA read to fetch header data, parse it
 * to get the entryMask and DMA Write the Header data. DMA Write: We found that
 * in some cases like PWR_Policy, they modifies the header thus we are using
 * DMA write to write the header, if no modification, it will overwrite the same
 * info again. Next, Using the entryMask obtained from Header Function, do
 * multiple call to Entry Function for querying each entry and DMA write the
 * query results.
 *
 * To use the default implementation, the boardObjGrp must:
 *   -# Be querying only 1 entry per one DMA read. Some tables have smaller
 *      yet more entries, where issuing one DMA for each entry results in
 *      too many DMA transactions should implement its own BoardObjGrpQuery
 *      interface.
 *   -# Put each header and corresponding entry array adjacent to each other,
 *      so that to Query the whole boardObjGrp, a series of calls into
 *      boardObjGrpQuery() could be triggered, each with its header structure
 *      offset as readOffset. The hdrFunc should parse each _HEADER structure
 *      and return entryMask for subsequent entry Query to use.
 *
 * @note    It attaches the DMEM overlay which is used during construct
 *          @ref BoardObjGrpIfaceModel10Set so that every caller doesn't need to
 *          explicitly attach/detach the DMEM overlay around the call to
 *          this function.
 *
 * @param[in]   pBoardObjGrp    BOARDOBJGRP pointer
 * @param[in]   pBuffer         Pointer to the PMU_DMEM_BUFFER structure which
 *                              describes the DMEM buffer used by the PMU while
 *                              processing BOARDOBJGRP elements.
 * @param[in]   hdrFunc         Pointer to Header function that will retrieve
 *                              the Header status.
 * @param[in]   hrdSize         Size of Header.
 * @param[in]   entryFunc       Pointer to Entry function that will retrieve
 *                              each entry's status.
 * @param[in]   entrySize       Size of entry.
 * @param[in]   ssOffset        Offset into the Super Surface where the object
 *                              is located. An offset of 0 indicates it is not
 *                              in the Super Surface.
 * @param[in]   pMask           Pointer to the group type-specific BOARDOBJGRP
 *                              mask. Used internally by
 *                              boardObjGrpGetStatus_E<xyz> when calling SUPER
 *                              implementation and clients should use wrapper
 *                              macros BOARDOBJGRP_GET_STATUS_E<xyz>() that set
 *                              this parameter to NULL.
 *
 * @return FLCN_OK  BOARDOBJGRP status was successfully retrieved.
 * @return OTHERS   Implementation specific error oclwrs.
 */
#define BoardObjGrpIfaceModel10GetStatus(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, PMU_DMEM_BUFFER *pBuffer, BoardObjGrpIfaceModel10GetStatusHeader(hdrFunc), LwLength hdrSize, BoardObjIfaceModel10GetStatus(entryFunc), LwLength entrySize, LwU32 ssOffset, BOARDOBJGRPMASK *pMask)

/*!
 * @brief   Reads the BoardObjGrp header from FB.
 *
 * @param[out]  pHeader     Pointer to the buffer to read the header into.
 * @param[in]   hdrSize     Size of Header.
 * @param[in]   ssOffset    Offset into the Super Surface where the object is
 *                          located. An offset of 0 indicates it is not in the
 *                          Super Surface.
 * @param[in]   readOffset  Initial offset this construct should apply.
 *
 * @return FLCN_OK  Header was read in from FB.
 * @return OTHERS   Implementation specific error code.
 */
#define BoardObjGrpIfaceModel10ReadHeader(fname) FLCN_STATUS (fname)(void *pHeader, LwLength hdrSize, LwU32 ssOffset, LwU32 readOffset)

/*!
 * @brief   Writes the BoardObjGrp header to FB.
 *
 * @param[out]  pHeader     Pointer to the buffer to write.
 * @param[in]   hdrSize     Size of Header.
 * @param[in]   ssOffset    Offset into the Super Surface where the object is
 *                          located. An offset of 0 indicates it is not in the
 *                          Super Surface.
 * @param[in]   writeOffset Initial offset this construct should apply.
 *
 * @return FLCN_OK  Header was written to the FB.
 * @return OTHERS   Implementation specific error code.
 */
#define BoardObjGrpIfaceModel10WriteHeader(fname) FLCN_STATUS (fname)(void *pHeader, LwLength hdrSize, LwU32 ssOffset, LwU32 writeOffset)

/*!
 * Structure representing an implementable interface indicating that a
 * @ref BOARDOBJGRP implements the "1.0" version of the @ref BOARDOBJGRP
 * "model." This is the "legacy" model that was based upon Resource
 * Manager-based initialization of the @ref BOARDOBJGRP.
 */
struct BOARDOBJGRP_IFACE_MODEL_10
{
    /*!
     * Member so that @ref BOARDOBJGRP_IFACE_MODEL_10 is a strict wrapper around
     * the @ref BOARDOBJGRP type. This allows for colwersion between the two by
     * direct casts.
     */
    BOARDOBJGRP boardObjGrp;
};

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * @brief   Given a @ref BOARDOBJGRP_IFACE_MODEL_10, returns its corresponding
 *          @ref BOARDOBJGRP
 *
 * @param[in]   pModel10
 *  @ref BOARDOBJGRP_IFACE_MODEL_10 for which to retrieve @ref BOARDOBJGRP
 * 
 * @return @ref BOARDOBJGRP corresponding to pModel10, or @ref NULL if not
 * available
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJGRP *
boardObjGrpIfaceModel10BoardObjGrpGet
(
    BOARDOBJGRP_IFACE_MODEL_10 *pModel10
)
{
    return &pModel10->boardObjGrp;
}

/*!
 * @brief   Given a @ref BOARDOBJGRP, returns its corresponding
 *          @ref BOARDOBJGRP_IFACE_MODEL_10
 *
 * @param[in]   pBoardObjGrp
 *  @ref BOARDOBJGRP for which to retrieve @ref BOARDOBJGRP_IFACE_MODEL_10
 * 
 * @return @ref BOARDOBJGRP_IFACE_MODEL_10 corresponding to pBoardObjGrp, or
 * @ref NULL if not available
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJGRP_IFACE_MODEL_10 *
boardObjGrpIfaceModel10FromBoardObjGrpGet
(
    BOARDOBJGRP *pBoardObjGrp
)
{
    return (BOARDOBJGRP_IFACE_MODEL_10 *)pBoardObjGrp;
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
BoardObjGrpIfaceModel10Set                    (boardObjGrpIfaceModel10Set);
BoardObjGrpIfaceModel10Set                    (boardObjGrpIfaceModel10SetAutoDma);
mockable BoardObjGrpIfaceModel10CmdHandlerDispatch  (boardObjGrpIfaceModel10CmdHandlerDispatch);
BoardObjGrpIfaceModel10SetHeader              (boardObjGrpIfaceModel10SetHeader);
BoardObjGrpIfaceModel10GetStatus                    (boardObjGrpIfaceModel10GetStatus_SUPER);
BoardObjGrpIfaceModel10GetStatus                    (boardObjGrpIfaceModel10GetStatusAutoDma_SUPER);
BoardObjGrpIfaceModel10ReadHeader                   (boardObjGrpIfaceModel10ReadHeader);
BoardObjGrpIfaceModel10WriteHeader                  (boardObjGrpIfaceModel10WriteHeader);

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // G_BOARDOBJGRP_IFACE_MODEL_10_H
#endif // BOARDOBJGRP_IFACE_MODEL_10_H
