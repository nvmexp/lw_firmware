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
 * @file    boardobj-test.c
 * @brief   Unit tests for BOARDOBJ.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "boardobj/boardobjgrp.h"
#include "perf/changeseq-mock.h" // for calloc mocking
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Static Function Prototypes ---------------------- */
// KO-TODO: Use this when BOARDOBJGRP_CONSTRUCT_E32 is ready
// static BoardObjGrpIfaceModel10SetEntry    (s_unitTestIfaceModel10SetEntry)
//     GCC_ATTRIB_USED();

static void* testSetup(void);
static BoardObjVirtualTableDynamicCast (s_unitUnitTestBoardObjDynamicCast_SUPER);
static BoardObjVirtualTableDynamicCast (s_unitUnitTestBoardObjDynamicCast_EXTENDED);

/* ------------------------ Defines and Macros ------------------------------ */
/*!
 * @brief   Macro to locate UT BOARDOBJGRP.
 *
 * @note    Allows @ref BOARDOBJGRP_GRP_GET to work for the UNIT_TEST class
 *          name.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define BOARDOBJGRP_DATA_LOCATION_UNIT_TEST_BOARDOBJ                           \
    (&unitTestBoardObjGrp.super.super)

/*!
 * @brief   ClassId for UNIT_TEST_BOARODBJGRP and its objects
 */
#define LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_UNIT_TEST_BOARDOBJ          (100U)

/*!
 * LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE
 */
#define LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_BASE                    (0x00U)
#define LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_FORK                    (0x01U)
#define LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_EXTENDED                (0x02U)
// Insert new types here and increment _MAX
#define LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_MAX                     (0x03U)

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   UNIT_TEST BOARDOBJGRP base class.
 *
 * @extends BOARDOBJGRP_E32
 */
typedef struct UNIT_TEST_BOARODBJGRP
{
    /*!
     * @brief BOARDOBJGRP_E32 super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    BOARDOBJGRP_E32 super;
} UNIT_TEST_BOARODBJGRP;

/*!
 * @brief   UNIT_TEST_BOARDOBJ base class.
 *
 * @extends BOARDOBJ
 */
typedef struct UNIT_TEST_BOARDOBJ
{
    /*!
     * @brief BOARDOBJ super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    BOARDOBJ    super;
} UNIT_TEST_BOARDOBJ;

typedef struct UNIT_TEST_BOARDOBJ UNIT_TEST_BOARDOBJ_BASE;

/*!
 * @brief   UNIT_TEST_BOARDOBJ fork class.
 *
 * @extends BOARDOBJ
 */
typedef struct UNIT_TEST_BOARDOBJ_FORK
{
    /*!
     * @brief UNIT_TEST_BOARDOBJ super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    UNIT_TEST_BOARDOBJ    super;

    /*!
     * @brief Some dummy variable.
     */
    LwU8 dummyData;
} UNIT_TEST_BOARDOBJ_FORK;

/*!
 * @brief   UNIT_TEST_BOARDOBJ extended class.
 *
 * @extends BOARDOBJ
 */
typedef struct UNIT_TEST_BOARDOBJ_EXTENDED
{
    /*!
     * @brief UNIT_TEST_BOARDOBJ super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    UNIT_TEST_BOARDOBJ    super;

    /*!
     * @brief Some dummy variable.
     */
    LwU8 dummyData;
} UNIT_TEST_BOARDOBJ_EXTENDED;

/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
static BOARDOBJ mock_boardobj = { 0 };

/*!
 * @brief   Local BOARDOBJGRP used strictly for unit testing.
 */
static UNIT_TEST_BOARODBJGRP        unitTestBoardObjGrp;

/*!
 * @brief   Local UNIT_TEST_BOARDOBJ instance
 */
static UNIT_TEST_BOARDOBJ           unitTestBoardObj;

/*!
 * @brief   Local UNIT_TEST_BOARDOBJ_FORK instance
 */
static UNIT_TEST_BOARDOBJ_FORK      unitTestBoardObjFork;

/*!
 * @brief   Local UNIT_TEST_BOARDOBJ_EXTENDED instance
 */
static UNIT_TEST_BOARDOBJ_EXTENDED  unitTestBoardObjExtended;

/*!
 * @brief   Local pointers to various object instances.
 */
static BOARDOBJ                   *ppBoardObjs[3] =
{
    &unitTestBoardObj.super,
    &unitTestBoardObjFork.super.super,
    &unitTestBoardObjExtended.super.super
};

/*!
 * Virtual table for UNIT_TEST_BOARDOBJ object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE unitUnitTestBoardObjVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_unitUnitTestBoardObjDynamicCast_SUPER)
};

/*!
 * Virtual table for UNIT_TEST_BOARDOBJ_EXTENDED object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE unitUnitTestBoardObjExtendedVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_unitUnitTestBoardObjDynamicCast_EXTENDED)
};
/*!
 * @brief   Array of all vtables pertaining to different UNIT_TEST_BOARDOBJ object types.
 */
static BOARDOBJ_VIRTUAL_TABLE *
unitUnitTestBoardObjVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, BASE)]     = &unitUnitTestBoardObjVirtualTable,
    [LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, FORK)]     = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, EXTENDED)] = &unitUnitTestBoardObjExtendedVirtualTable,
};

/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_BOARDOBJ,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_BOARDOBJ, DynamicCast,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJ, boardObjGrpIfaceModel10ObjSet_IMPL,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/* ------------------------ Test Suite Implementation ----------------------- */
/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJ, DynamicCast)
{
    // KO-TODO: Use BOARDOBJGRP_CONSTRUCT_E32 once DMA mocking is possible
    // status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,     // _grpType 
    //     UNIT_TEST,                          // _class
    //     _,                                  // _pBuffer
    //     boardObjGrpIfaceModel10SetHeader,         // _hdrFunc
    //     s_unitTestIfaceModel10SetEntry,           // _entryFunc
    //     _,                                  // _ssElement
    //     (LwU8)OVL_INDEX_DMEM(_),            // _ovlIdxDmem
    //     unitUnitTestBoardObjVirtualTables); // _ppObjectVtables

    // Locals
    UNIT_TEST_BOARDOBJ             *pUnitTestBoardobj           = NULL;
    UNIT_TEST_BOARDOBJ_FORK        *pUnitTestBoardobjFork       = NULL;
    UNIT_TEST_BOARDOBJ_EXTENDED    *pUnitTestBoardobjExtended   = NULL;

    // Setup the group
    unitTestBoardObjGrp.super.super.type                        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    unitTestBoardObjGrp.super.super.classId                     = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_UNIT_TEST_BOARDOBJ;
    unitTestBoardObjGrp.super.super.objSlots                    = 3;
    unitTestBoardObjGrp.super.super.ovlIdxDmem                  = 0;
    unitTestBoardObjGrp.super.super.bConstructed                = LW_TRUE;
    unitTestBoardObjGrp.super.super.ppObjects                   = ppBoardObjs;
    unitTestBoardObjGrp.super.super.vtables.numObjectVtables    = LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, MAX);
    unitTestBoardObjGrp.super.super.vtables.ppObjectVtables     = unitUnitTestBoardObjVirtualTables;
    unitTestBoardObjGrp.super.objMask.super.bitCount            = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    unitTestBoardObjGrp.super.objMask.super.maskDataCount       = 1;
    unitTestBoardObjGrp.super.objMask.super.pData[0]            = 0x7; // 0b111

    // Setup the group's objects
    unitTestBoardObj.super.type                     = LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_BASE;
    unitTestBoardObjFork.super.super.type           = LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_FORK;
    unitTestBoardObjExtended.super.super.type       = LW2080_CTRL_UNIT_UNIT_TEST_BOARDOBJ_TYPE_EXTENDED;

    unitTestBoardObj.super.classId                  = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_UNIT_TEST_BOARDOBJ;
    unitTestBoardObjFork.super.super.classId        = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_UNIT_TEST_BOARDOBJ;
    unitTestBoardObjExtended.super.super.classId    = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_UNIT_TEST_BOARDOBJ;

    unitTestBoardObj.super.grpIdx                   = 0;
    unitTestBoardObjFork.super.super.grpIdx         = 1;
    unitTestBoardObjExtended.super.super.grpIdx     = 2;

    // Cast to same class must work.
    pUnitTestBoardobj = BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObj.super,
                                              UNIT, UNIT_TEST_BOARDOBJ, BASE);
    UT_ASSERT_NOT_NULL(pUnitTestBoardobj);

    // Cast to a class outside of the hierarchy must fail.
    pUnitTestBoardobjExtended =  BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObj.super,
                                                       UNIT, UNIT_TEST_BOARDOBJ, EXTENDED);
    UT_ASSERT_NULL(pUnitTestBoardobjExtended);

    pUnitTestBoardobj           = NULL;
    pUnitTestBoardobjFork       = NULL;
    pUnitTestBoardobjExtended   = NULL;

    // Cast to class up the hierarchy must work.
    pUnitTestBoardobj = BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObjExtended.super.super,
                                              UNIT, UNIT_TEST_BOARDOBJ, BASE);
    UT_ASSERT_NOT_NULL(pUnitTestBoardobj);

    // Cast to same class must work.
    pUnitTestBoardobjExtended = BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObjExtended.super.super,
                                                      UNIT, UNIT_TEST_BOARDOBJ, EXTENDED);
    UT_ASSERT_NOT_NULL(pUnitTestBoardobjExtended);

    // Cast to a class outside of the hierarchy must fail.
    pUnitTestBoardobjFork =  BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObjExtended.super.super,
                                                  UNIT, UNIT_TEST_BOARDOBJ, FORK);
    UT_ASSERT_NULL(pUnitTestBoardobjFork);

    pUnitTestBoardobj           = NULL;
    pUnitTestBoardobjFork       = NULL;
    pUnitTestBoardobjExtended   = NULL;

    // Cast must fail for unimplemented dynamicCast interface.
    pUnitTestBoardobj = BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObjFork.super.super,
                                              UNIT, UNIT_TEST_BOARDOBJ, BASE);
    UT_ASSERT_NULL(pUnitTestBoardobj);

    // Cast must fail for unimplemented dynamicCast interface.
    pUnitTestBoardobjFork = BOARDOBJ_DYNAMIC_CAST(&unitTestBoardObjFork.super.super,
                                                  UNIT, UNIT_TEST_BOARDOBJ, FORK);
    UT_ASSERT_NULL(pUnitTestBoardobjFork);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJ, boardObjGrpIfaceModel10ObjSet_IMPL)
{
    // TODO-aherring: make sure that this works with the changes to an interface
    BOARDOBJGRP_IFACE_MODEL_10  localModel10  = { 0 };
    BOARDOBJ       *pBoardobj   = NULL;
    RM_PMU_BOARDOBJ rmPmuBoardobj;
    FLCN_STATUS     status;

    // Test in the case that *ppBoardObj is null, and lwosCalloc fails (expect FLCN_ERR_NO_FREE_MEM)
    callocMockAddEntry(0, NULL, NULL);
    status = boardObjGrpIfaceModel10ObjSet_IMPL(&localModel10,
                                    &pBoardobj,
                                    sizeof(BOARDOBJ),
                                    &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM);
    UT_ASSERT_EQUAL_PTR(pBoardobj, NULL);

    // Test in the case that *ppBoardObj is null, and everything works as expected
    callocMockAddEntry(1, &mock_boardobj, NULL);
    status = boardObjGrpIfaceModel10ObjSet_IMPL(&localModel10,
                                    &pBoardobj,
                                    sizeof(BOARDOBJ),
                                    &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_boardobj);

    // Test in the case that *ppBoardObj is already set, and there is a type mismatch (expect FLCN_ERROR)
    pBoardobj = &mock_boardobj;
    pBoardobj->type         = 1;
    pBoardobj->grpIdx       = 1;
    rmPmuBoardobj.type      = 2;
    rmPmuBoardobj.grpIdx    = 1;
    status = boardObjGrpIfaceModel10ObjSet_IMPL(&localModel10,
                                    &pBoardobj,
                                    sizeof(BOARDOBJ),
                                    &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_boardobj);

    // Test in the case that *ppBoardObj is already set, and there is a grpidx mismatch (expect FLCN_ERR_ILWALID_ARGUMENT)
    pBoardobj = &mock_boardobj;
    pBoardobj->type         = 1;
    pBoardobj->grpIdx       = 1;
    rmPmuBoardobj.type      = 1;
    rmPmuBoardobj.grpIdx    = 2;
    status = boardObjGrpIfaceModel10ObjSet_IMPL(&localModel10,
                                    &pBoardobj,
                                    sizeof(BOARDOBJ),
                                    &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_boardobj);

    // Test in the case everything goes as planned (expect FLCN_OK and correctly set *ppBoardObj)
    pBoardobj = &mock_boardobj;
    pBoardobj->type         = 1;
    pBoardobj->grpIdx       = 1;
    rmPmuBoardobj.type      = 1;
    rmPmuBoardobj.grpIdx    = 1;
    status = boardObjGrpIfaceModel10ObjSet_IMPL(&localModel10,
                                    &pBoardobj,
                                    sizeof(BOARDOBJ),
                                    &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_boardobj);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    callocMockInit();
    return NULL;
}

/*!
 * @brief   UNIT_UNIT_TEST_BOARDOBJ implementation of @ref
 *          BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_unitUnitTestBoardObjDynamicCast_SUPER
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                   *pObject             = NULL;
    UNIT_TEST_BOARDOBJ     *pUnitTestBoardobj   = (UNIT_TEST_BOARDOBJ *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, BASE))
    {
        PMU_BREAKPOINT();
        goto s_unitUnitTestBoardObjDynamicCast_SUPER_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, BASE):
        {
            pObject = (void *)pUnitTestBoardobj;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_unitUnitTestBoardObjDynamicCast_SUPER_exit:
    return pObject;
}

/*!
 * @brief   UNIT_UNIT_TEST_BOARDOBJ_EXTENDED implementation of @ref
 *          BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_unitUnitTestBoardObjDynamicCast_EXTENDED
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                           *pObject                     = NULL;
    UNIT_TEST_BOARDOBJ_EXTENDED    *pUnitTestBoardobjExtended   = (UNIT_TEST_BOARDOBJ_EXTENDED *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, EXTENDED))
    {
        PMU_BREAKPOINT();
        goto s_unitUnitTestBoardObjDynamicCast_EXTENDED_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, BASE):
        {
            pObject = (void *)&pUnitTestBoardobjExtended->super;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(UNIT, UNIT_TEST_BOARDOBJ, EXTENDED):
        {
            pObject = (void *)pUnitTestBoardobjExtended;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_unitUnitTestBoardObjDynamicCast_EXTENDED_exit:
    return pObject;
}

/* ------------------------ End of File ------------------------------------- */
