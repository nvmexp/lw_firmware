/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file cl9096.cpp
//! \brief ZBC clear test
//!
//! This test exercises ZBC clear functionality, allowing one or multiple ZBC
//! color, depth and stencil clear values to be specified to a table in HW,
//! and then removed.
//!
//! Class 9096 contains bookkeeping support for sharing duplicate clear values,
//! by indexUsed, among multiple clients.  If all entries in the table are
//! filled, no more clear values are accepted.  Both these features are fully
//! exercised also.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "lwRmApi.h"
#include "gpu/perf/powermgmtutil.h"
#include "gpu/utility/zbccolwertutil.h"

#include "class/cl9096.h" // GF100_ZBC_CLEAR
#include "ctrl/ctrl9096.h"
#include "core/include/memcheck.h"

//
// N.B.: ZBC Classs is a Sub-Device class hence for each Sub-Device the object
// allocated will be different as per the SubDevice Handle.Also presently we
// only support one object per client.
//
#define MAX_OBJECTS_PER_CHANNEL     40
#define DEPTH_START_INDEX           20

#define HMEM1                       0xdead0001

// Disable Suspend Resume Test case untill its stable on F-model.
#define ENABLE_SR_TEST               1

#define ZERO_STRUCT(struct_ptr) (void) memset((struct_ptr), 0, sizeof(*(struct_ptr)))

//
// Size of the pre-generated color table data which will be used to test driver's
// color table functionality
//
#define COLOR_DATA_SIZE 30

//!-----------------------------------------------------------------------------
//! List of possible FB formats.
//!-----------------------------------------------------------------------------
UINT32 FMTList[19] = {
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ZERO,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_UNORM_ONE,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RF32_GF32_BF32_AF32,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_R16_G16_B16_A16,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RN16_GN16_BN16_AN16,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RS16_GS16_BS16_AS16,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RU16_GU16_BU16_AU16,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RF16_GF16_BF16_AF16,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8R8G8B8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8RL8GL8BL8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2B10G10R10,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AU2BU10GU10RU10,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8B8G8R8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8BL8GL8RL8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AN8BN8GN8RN8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AS8BS8GS8RS8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AU8BU8GU8RU8,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2R10G10B10,
 LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_BF10GF11RF11
};

//!-----------------------------------------------------------------------------
//! Rand32 :Randomly generate 32 bit integer.
//!-----------------------------------------------------------------------------
static int Rand32()
{
   int a =  rand() ^ (rand() << 12);
   return a;
}

//! Structure to hold input DS color data and format information.
typedef struct
{
    fluint color_DS[4];
    UINT32 format;
}COLORDATA;

class Class9096Test : public RmTest
{
public:
    Class9096Test();
    virtual ~Class9096Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle         m_hZBCClearTest;
    UINT32               hMem1;
    UINT32               m_entry_color_num;
    UINT32               m_entry_depth_num;
    UINT32               m_entry_stencil_num;
    PowerMgmtUtil        m_powerMgmtUtil;
    zbcColorColwertUtil  m_colorColwUtil;

    // private memeber functions
    RC ZbcTestFixedTableSize( );
    RC ZbcTestVariableTableSize( );
    void initColorDataArrayWithFormat(COLORDATA *inputData);
    void initColorDataArrayWithoutFormat(COLORDATA *inputData);
    RC getColorDepth(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS  *pGetZBCTableParams);
    RC getTableSize(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS  *pGetZBCTableSizeParams);
    RC getTableEntry(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS  *pGetZBCTableEntryParams);
    RC colwertDStoFB(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS  *pZBCColorParams);
    RC callSuspendResumeOperation( );
};

//!
//! \brief Class9096Test (ZBC clear test) constructor
//!
//! Class9096Test (ZBC clear test) constructor does not do much.  Functionality
//! mostly lies in Setup().
//!
//! \sa Setup
//------------------------------------------------------------------------------
Class9096Test::Class9096Test()
{
    m_hZBCClearTest = 0;
    m_entry_color_num = 0;
    m_entry_depth_num = 0;
    m_entry_stencil_num = 0;

    SetName("Class9096Test");
}

//!
//! \brief Class9096Test (ZBC clear test) destructor
//!
//! Class9096Test (ZBC clear test) destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Class9096Test::~Class9096Test()
{

}

//!
//! \brief Is Class9096Test (ZBC clear test) supported?
//!
//! Verify if Class9096Test (ZBC clear test) is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Class9096Test::IsTestSupported()
{
    LwRmPtr pLwRm;
    UINT32 retVal;
    struct fbInfo
    {
        LW2080_CTRL_FB_INFO fbSize;
        LW2080_CTRL_FB_INFO fbBroken;
    } fbInfo;
    LW2080_CTRL_FB_GET_INFO_PARAMS fbInfoParams;
    fbInfo.fbSize.index = LW2080_CTRL_FB_INFO_INDEX_RAM_SIZE;
    fbInfo.fbBroken.index = LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN;

    fbInfoParams.fbInfoListSize = sizeof (fbInfo) / sizeof (LW2080_CTRL_FB_INFO);
    fbInfoParams.fbInfoList = LW_PTR_TO_LwP64(&fbInfo);

    retVal = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
        LW2080_CTRL_CMD_FB_GET_INFO,
        &fbInfoParams,
        sizeof (fbInfoParams));
    if (OK != RmApiStatusToModsRC(retVal))
    {
        return "Unable to query FB info";
    }

    if ((fbInfo.fbBroken.data == 1) || (fbInfo.fbSize.data == 0))
    {
        return "ZBC should not be run on broken FB or 0FB chip";
    }

    if (IsClassSupported(GF100_ZBC_CLEAR))
    {
        return RUN_RMTEST_TRUE;
    }

    return "GF100_ZBC_CLEAR class is not supported on current platform";
}

//!
//! \brief Class9096Test (ZBC clear test) Setup
//!
//! Most importantly, allocate GF100_ZBC_CLEAR object.
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC Class9096Test::Setup()
{
    RC              rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//!
//! \brief Class9096Test (ZBC clear test) Run
//! \Lwrrently the tset has only basic functionality and yet to be developed more.
//! \TODO: Expand the test to check the read writes for both DS and L2 writes and RM
//! \book-keeping...
//! \return  - returns the RC Type value for the test run
//------------------------------------------------------------------------------
RC Class9096Test::Run()
{
    RC                                              rc;
    LwRmPtr                                         pLwRm;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS     getZBCTableSizeParams;
    UINT32                                          attr;
    UINT32                                          attr2;

    m_powerMgmtUtil.BindGpuSubdevice(GetBoundGpuSubdevice());

    // Allocate the one and only ZBC Clear Object per subdevice.
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                              &m_hZBCClearTest,
                              GF100_ZBC_CLEAR,
                              NULL));

    // Second allocation should fail
    LwRm::Handle m_hZBCClearTest2;
    m_hZBCClearTest2 = 0;
    rc = pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                              &m_hZBCClearTest2,
                              GF100_ZBC_CLEAR,
                              NULL);
    if (rc == OK)
    {
        Printf(Tee::PriHigh, "TEST:  Error could allocate 2nd object per subdevice.\n");
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }
    rc.Clear();

    // Allocate a ZBC surface to trigger refcount
    attr |= (DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _64) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4) |
            DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT));
    attr2 = DRF_DEF(OS32, _ATTR2, _ZBC, _PREFER_ZBC);

    hMem1 = HMEM1;

    rc  = pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                    LWOS32_ALLOC_FLAGS_NO_SCANOUT,
                                    &attr,
                                    &attr2, // pAttr2
                                    1024*1024ull,
                                    1,    // alignment
                                    NULL, // pFormat
                                    NULL, // pCoverage
                                    NULL, // pPartitionStride
                                    &hMem1,
                                    NULL, // poffset,
                                    NULL, // pLimit
                                    NULL, // pRtnFree
                                    NULL, // pRtnTotal
                                    0,    // width
                                    0,    // height
                                    GetBoundGpuDevice());
    CHECK_RC(RmApiStatusToModsRC(rc));

    ZERO_STRUCT(&getZBCTableSizeParams);
    getZBCTableSizeParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
    rc = getTableSize(&getZBCTableSizeParams);
    //
    // If LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_SIZE failed or if color table size
    // is 15, then we will use legacy way (fixed size for all tables) to test.
    //
    if ((rc == OK) && (getZBCTableSizeParams.indexEnd != 15))
    {
        Printf(Tee::PriHigh, "TEST: Start variable table size testing!\n");
        CHECK_RC(ZbcTestVariableTableSize());
    }
    else
    {
        Printf(Tee::PriHigh, "TEST: Start fixed table size testing!\n");
        CHECK_RC(ZbcTestFixedTableSize());
    }

    return rc;
}

//!
//! \brief ZbcTestFixedTableSize
//! \This is the legacy test which assumes the table size is fixed for all tables.
//! \Only pre_Ampere should be tested by this way.
//! \return  - returns the RC Type value for the test run
//------------------------------------------------------------------------------
RC Class9096Test::ZbcTestFixedTableSize()
{
    RC                                        rc;
    LwRmPtr                                   pLwRm;
    UINT32                                    i;
    UINT32                                    index;
    UINT32                                    color_Count;
    UINT32                                    depth_Count;
    UINT32                                    table_Size;
    UINT32                                    counter;
    bool                                      add_color_entry = true;
    bool                                      add_depth_entry = true;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS    getZBCTableData;// Variable used for all general access to the tables.
    COLORDATA                                 *inputData = NULL;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS    *storeZBCColorTable = NULL;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS    *storeZBCDepthTable = NULL;

    inputData = new COLORDATA[COLOR_DATA_SIZE];
    if (inputData == NULL)
    {
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    initColorDataArrayWithFormat(inputData);

    ZERO_STRUCT(&getZBCTableData);
    // First fetch the ZBC Table Size...
    getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_ILWALID;
    CHECK_RC(getColorDepth(&getZBCTableData));
    // We need one extra entry for the reserved 0th entry
    table_Size = getZBCTableData.indexSize + 1;

    // Create a running repository of the table state ..
    storeZBCColorTable = new LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS[table_Size];
    storeZBCDepthTable = new LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS[table_Size];

    // Just print the current table state....
    for (i = 1;i < table_Size;i++)
    {
        ZERO_STRUCT(&getZBCTableData);
        getZBCTableData.indexSize = i;

        getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_COLOR;
        CHECK_RC(getColorDepth(&getZBCTableData));

        for (UINT32 j = 0; j<LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE ; j++)
        {
            Printf(Tee::PriHigh, "TEST:  COLOR_FB value allocated at index 0x%x is 0x%x\n",
                   j, getZBCTableData.value.colorFB[j]);
            Printf(Tee::PriHigh, "TEST:  COLOR_DS value allocated at index 0x%x is 0x%x\n",
                   j, getZBCTableData.value.colorDS[j]);
        }

        Printf(Tee::PriHigh, "TEST:  indexUsed value allocated at index 0x%x is 0x%x\n",
               i, getZBCTableData.indexUsed);
        Printf(Tee::PriHigh, "TEST:  COLOR FORMAT   allocated at index 0x%x is 0x%x\n",
               i, getZBCTableData.format);

        getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_DEPTH;
        CHECK_RC(getColorDepth(&getZBCTableData));

        Printf(Tee::PriHigh, "TEST:  DEPTH value    allocated at index 0x%x is  0x%x\n",
               i,  getZBCTableData.value.depth);
    }

    // Check for  the availiable space in table for further allocation.
    color_Count = 0;
    depth_Count = 0;

    for (i = 1;i < table_Size;i++)
    {
        ZERO_STRUCT(&getZBCTableData);
        getZBCTableData.indexSize = i;

        getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_COLOR;
        CHECK_RC(getColorDepth(&getZBCTableData));

        if (!getZBCTableData.indexUsed)
        {
            color_Count = color_Count + 1;
        }
        else
        {
            // save the default values for future in table store
            for (UINT32 j = 0; j< LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
            {
                storeZBCColorTable[i].value.colorFB[j] = getZBCTableData.value.colorFB[j];
                storeZBCColorTable[i].value.colorDS[j] = getZBCTableData.value.colorDS[j];
            }

            storeZBCColorTable[i].indexSize = i;
            storeZBCColorTable[i].format = getZBCTableData.format;
            storeZBCColorTable[i].valType = getZBCTableData.valType;
            storeZBCColorTable[i].indexUsed = true;
        }

        getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_DEPTH;
        CHECK_RC(getColorDepth(&getZBCTableData));

        if (!getZBCTableData.indexUsed)
        {
           depth_Count = depth_Count + 1;
        }
        else
        {
            // save the default values for future in table store
            storeZBCDepthTable[i].value.depth = getZBCTableData.value.depth;
            storeZBCDepthTable[i].indexSize = i;
            storeZBCDepthTable[i].format = getZBCTableData.format;
            storeZBCDepthTable[i].valType = getZBCTableData.valType;
            storeZBCDepthTable[i].indexUsed = true;
        }
    }

    // Default written values are 0 -> (table_Size - color_Count/depth_Count)

    Printf(Tee::PriHigh, "TEST: CASE I:\n");
    Printf(Tee::PriHigh, "TEST: Setting 15 different color and depth patterns using 1 value per object.\n");
    // CASE I:
    ////////////
    // Setting 15 different color and depth patterns using 1 value per object.
    counter = 0;
    Printf(Tee::PriHigh, "\n");
    while (add_color_entry || add_depth_entry)
    {
        LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS colorParams;
        LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS depthParams;

        ZERO_STRUCT(&colorParams);
        ZERO_STRUCT(&depthParams);

        // Set the color Params.....
        colorParams.colorDS[0] = inputData[counter].color_DS[0].ui;
        colorParams.colorDS[1] = inputData[counter].color_DS[1].ui;
        colorParams.colorDS[2] = inputData[counter].color_DS[2].ui;
        colorParams.colorDS[3] = inputData[counter].color_DS[3].ui;
        colorParams.format     = inputData[counter].format;

        colwertDStoFB(&colorParams);

        // set the depth Params...
        depthParams.depth = 0x03ff0000 + 5*(m_entry_depth_num+1);
        // Only possible format option in depth
        depthParams.format = LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR_FMT_FP32;

        // Just a sanity check for access......
        CHECK_RC(pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_NULL,
            NULL,
            0));

        if (add_color_entry)
        {
            rc = pLwRm->Control(m_hZBCClearTest,
                LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
                &colorParams,
                sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

            if (rc == OK)
            {
                index = (table_Size + m_entry_color_num - color_Count);

                // Check if the sent data was updated in ZBC Table or not!!
                ZERO_STRUCT(&getZBCTableData);
                getZBCTableData.indexSize = index;
                getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_COLOR;
                CHECK_RC(getColorDepth(&getZBCTableData));

                if (!getZBCTableData.indexUsed)
                {
                    // The current LUT values already exist in ZBC Table hence
                    // no need updating the running store table.
                    Printf(Tee::PriHigh, "TEST:Given entry already exists in the Color table!!!\n");
                }
                else
                {
                    // Update the running table store...
                    for (UINT32 j = 0; j< LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
                    {
                        storeZBCColorTable[index].value.colorFB[j] = colorParams.colorFB[j];
                        storeZBCColorTable[index].value.colorDS[j] = colorParams.colorDS[j];
                    }

                    storeZBCColorTable[index].indexSize = index;
                    storeZBCColorTable[index].format = colorParams.format;
                    storeZBCColorTable[index].valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_COLOR;
                    storeZBCColorTable[index].indexUsed = true;
                    m_entry_color_num++;

                    Printf(Tee::PriHigh, "TEST: Color Value successfully written to index %d\n",index);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 0 0x%x\n",(UINT32)colorParams.colorDS[0]);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 1 0x%x\n",(UINT32)colorParams.colorDS[1]);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 2 0x%x\n",(UINT32)colorParams.colorDS[2]);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 3 0x%x\n",(UINT32)colorParams.colorDS[3]);
                    Printf(Tee::PriHigh, "TEST: Input FB Format is  0x%x\n",(UINT32)colorParams.format);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 0 0x%x\n",(UINT32)colorParams.colorFB[0]);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 1 0x%x\n",(UINT32)colorParams.colorFB[1]);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 2 0x%x\n",(UINT32)colorParams.colorFB[2]);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 3 0x%x\n",(UINT32)colorParams.colorFB[3]);
                    Printf(Tee::PriHigh, "\n");
                }
            }
            else
            {
                add_color_entry = false;
                rc.Clear();
            }
        }

        if (add_depth_entry)
        {
            rc = pLwRm->Control(m_hZBCClearTest,
                LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR,
                &depthParams,
                sizeof(LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS));

            if (rc == OK)
            {
                index = (table_Size + m_entry_depth_num - depth_Count);

                // Check if the sent data was updated in ZBC Table or not!!
                ZERO_STRUCT(&getZBCTableData);
                getZBCTableData.indexSize = index;
                getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_DEPTH;
                CHECK_RC(getColorDepth(&getZBCTableData));

                if (!getZBCTableData.indexUsed)
                {
                    //
                    // The current LUT values already exist in ZBC Table hence
                    // no need updating the running store table.
                    //
                    Printf(Tee::PriHigh, "TEST:Given entry already exists in the Depth table!!!\n");
                }
                else
                {
                    // Update the running table store...
                    storeZBCDepthTable[index].value.depth = depthParams.depth;
                    storeZBCDepthTable[index].indexSize = index;
                    storeZBCDepthTable[index].format = depthParams.format;
                    storeZBCDepthTable[index].valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_DEPTH;
                    storeZBCDepthTable[index].indexUsed = true;
                    m_entry_depth_num++;
                    Printf(Tee::PriHigh, "TEST: Depth Value successfully written to index %d\n",index);
                    Printf(Tee::PriHigh, "TEST: Depth Value written is  0x%x\n",(UINT32)depthParams.depth);
                    Printf(Tee::PriHigh, "\n");
                }
            }
            else
            {
                add_depth_entry = false;
                rc.Clear();
            }
        }
        counter++;
    }

    if ((color_Count != (m_entry_color_num)) || (depth_Count != (m_entry_depth_num)) || (counter < table_Size))
    {
         Printf(Tee::PriHigh, "ERROR: Counter = %d : tabe_size = %d\n", counter, table_Size);
         Printf(Tee::PriHigh, "ERROR: Number of color/depth values allocated is not same as the availiable slot in table\n");
         Printf(Tee::PriHigh, "ERROR: Number of COLOR values allocated is %d\n", m_entry_color_num);
         Printf(Tee::PriHigh, "ERROR: Number of DEPTH values allocated is %d\n", m_entry_depth_num);
         Printf(Tee::PriHigh, "ERROR: Number of availiable slots in COLOR table is %d\n", color_Count);
         Printf(Tee::PriHigh, "ERROR: Number of availiable slots in DEPTH table is %d\n", depth_Count);
         rc = RC::SOFTWARE_ERROR;
         return rc;
    }
    else
    {
         Printf(Tee::PriHigh, "Filled all the empty slots in both the tables with fresh values\n");
         Printf(Tee::PriLow,  "Initially number of COLOR values allocated is %d\n", m_entry_color_num);
         Printf(Tee::PriLow,  "Initially number of DEPTH values allocated is %d\n", m_entry_depth_num);
    }

    //
    // Note: any object_index [i] has the index
    // (LW9096_CTRL_ZBC_CLEAR_TABLE_SIZE - color_Count/depth_Count + i)
    // in the color/depth table.
    // Object allocation starts at
    // "LW9096_CTRL_ZBC_CLEAR_TABLE_SIZE - color_Count/depth_Count" index in the table.
    //

    Printf(Tee::PriHigh, "TEST: CASE II:\n");
    Printf(Tee::PriHigh, "TEST: Check if the read COLOR/DEPTH data the same as written.\n");
    //
    // CASE II
    /////////////
    // Now try reading the written values...
    // This also checks for the Endian Swap problem...
    // In case of Big endian machine any read or write to L2 directly from the
    // HW register will automatically be swapped hence any read or write to
    // the L2 in Big endian machine should be accompanied by a endian swap
    // before (Write) or  after (Read).
    //
    for (i = 1;i < table_Size;i++)
    {
        ZERO_STRUCT(&getZBCTableData);
        getZBCTableData.indexSize = i;

        getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_COLOR;
        CHECK_RC(getColorDepth(&getZBCTableData));

        for (UINT32 j = 0; j<LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE ; j++)
        {
            if ((getZBCTableData.value.colorFB[j] != storeZBCColorTable[i].value.colorFB[j]) ||
                (getZBCTableData.value.colorDS[j] != storeZBCColorTable[i].value.colorDS[j]))
            {
                rc = RC::SOFTWARE_ERROR;
                Printf(Tee::PriHigh, "TEST: Read COLOR values at index 0x%x is not same as written\n",i);
                return rc;
            }
        }

        getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_DEPTH;
        CHECK_RC(getColorDepth(&getZBCTableData));

        if (getZBCTableData.value.depth != storeZBCDepthTable[i].value.depth)
        {
            rc = RC::SOFTWARE_ERROR;
            Printf(Tee::PriHigh, "TEST: Read DEPTH values at index 0x%x is not same as written\n",i);
            return rc;
        }
    }

    Printf(Tee::PriHigh, "TEST: Written COLOR/DEPTH values are read properly\n");

    Printf(Tee::PriHigh, "TEST: CASE III:\n");
    Printf(Tee::PriHigh, "TEST: Check if data still matches after a suspend resume operation.\n");
    //
    // case III
    ////////////
    // Now read after a suspend resume operation...
    // This case gaurantees that the table restoration work is going fine
    //
#if ENABLE_SR_TEST

    if (callSuspendResumeOperation( ) == OK)
    {
        for (i = 1;i < table_Size;i++)
        {
            ZERO_STRUCT(&getZBCTableData);
            getZBCTableData.indexSize = i;

            getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_COLOR;
            CHECK_RC(getColorDepth(&getZBCTableData));

            for (UINT32 j = 0; j<LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE ; j++)
            {
                if ((getZBCTableData.value.colorFB[j] != storeZBCColorTable[i].value.colorFB[j]) ||
                    (getZBCTableData.value.colorDS[j] != storeZBCColorTable[i].value.colorDS[j]))
                {
                    rc = RC::SOFTWARE_ERROR;
                    Printf(Tee::PriHigh, "TEST: Read COLOR values after resume at index 0x%x is not same as written\n",i);
                    return rc;
                }
            }

            getZBCTableData.valType = LW9096_CTRL_ZBC_CLEAR_OBJECT_TYPE_DEPTH;
            CHECK_RC(getColorDepth(&getZBCTableData));

            if (getZBCTableData.value.depth != storeZBCDepthTable[i].value.depth)
            {
                rc = RC::SOFTWARE_ERROR;
                Printf(Tee::PriHigh, "TEST: Read DEPTH values after resume at index 0x%x is not same as written\n",i);
                return rc;
            }
        }
    }

    Printf(Tee::PriHigh, "TEST: COLOR/DEPTH values still match after resume\n");

#endif

    Printf(Tee::PriHigh, "TEST: CASE IV:\n");
    Printf(Tee::PriHigh, "TEST: Check if we can still add new entry while table is already full.\n");
    //
    // CASE IV
    ////////////
    // See if we can add more entries in the table .
    // Since the table is full it should not be possible now.
    //

    // Set the color Params.....
    LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS  colorParamsAdd;
    ZERO_STRUCT(&colorParamsAdd);
    colorParamsAdd.colorDS[0] = inputData[counter].color_DS[0].ui;
    colorParamsAdd.colorDS[1] = inputData[counter].color_DS[1].ui;
    colorParamsAdd.colorDS[2] = inputData[counter].color_DS[2].ui;
    colorParamsAdd.colorDS[3] = inputData[counter].color_DS[3].ui;
    colorParamsAdd.format     = inputData[counter].format;

    colwertDStoFB(&colorParamsAdd);

    rc = pLwRm->Control(m_hZBCClearTest,
        LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
        &colorParamsAdd,
        sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Failed to add new entry through old object in the table. Going Fine\n");
        rc.Clear();
    }
    else
    {
        Printf(Tee::PriHigh, "ERROR:Added entry to an already filled table\n");
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    Printf(Tee::PriHigh, "TEST: CASE V:\n");
    Printf(Tee::PriHigh, "TEST: Add old values which should be allowed (refcount increased).\n");
    //
    // CASE V
    ///////////
    // Adding old values using same object again should be fine
    // as it only increases HW refcount ...
    //

    for (i = 1; i < table_Size; i++)
    {
        LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS colorParams;
        LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS depthParams;

        ZERO_STRUCT(&colorParams);
        ZERO_STRUCT(&depthParams);

        // Set the color Params.....
        for (UINT32 j = 0; j < LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
        {
            colorParams.colorFB[j] = storeZBCColorTable[i].value.colorFB[j];
            colorParams.colorDS[j] = storeZBCColorTable[i].value.colorDS[j];
        }
        colorParams.format = storeZBCColorTable[i].format;

        rc = pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
            &colorParams,
            sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR:Failed to add same color entry in the table\n");
            rc.Clear();
            rc = RC::SOFTWARE_ERROR;
            break;
        }

        // set the depth Params...
        depthParams.depth = storeZBCDepthTable[i].value.depth;
        // Only possible format option in depth
        depthParams.format = storeZBCDepthTable[i].format;

        rc = pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR,
            &depthParams,
            sizeof(LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS));

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR:Failed to add same depth entry in the table\n");
            rc.Clear();
            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    Printf(Tee::PriHigh, "TEST: Add old values are successfully done.\n");

    delete [] inputData;
    delete [] storeZBCColorTable;
    delete [] storeZBCDepthTable;

    return rc;
}

//!
//! \brief ZbcTestVariableTableSize
//! \This test is used in chips who supported variable sizes of tables.
//! \AMPERE_and_later should be tested by this way.
//! \return  - returns the RC Type value for the test run
//------------------------------------------------------------------------------
RC Class9096Test::ZbcTestVariableTableSize()
{
    RC                                              rc;
    LwRmPtr                                         pLwRm;
    UINT32                                          i;
    UINT32                                          index;
    UINT32                                          unused_color_count = 0;
    UINT32                                          unused_depth_count = 0;
    UINT32                                          unused_stencil_count = 0;
    UINT32                                          table_start_color = 0;
    UINT32                                          table_start_depth = 0;
    UINT32                                          table_start_stencil = 0;
    UINT32                                          table_end_color = 0;
    UINT32                                          table_end_depth = 0;
    UINT32                                          table_end_stencil = 0;
    UINT32                                          counter_color;
    UINT32                                          counter_depth;
    UINT32                                          counter_stencil;
    bool                                            add_color_entry = true;
    bool                                            add_depth_entry = true;
    bool                                            add_stencil_entry = true;
    COLORDATA                                      *inputData = NULL;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS     getZBCTableSizeParams;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS    getZBCTableEntryParams;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS   *storeZBCColorTable = NULL;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS   *storeZBCDepthTable = NULL;
    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS   *storeZBCStencilTable = NULL;

    inputData = new COLORDATA[COLOR_DATA_SIZE];
    if (inputData == NULL)
    {
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    // For AMPERE_and_later COLOR table, format field is not supported anymore
    initColorDataArrayWithoutFormat(inputData);

    // First fetch the ZBC Table Size for each table
    ZERO_STRUCT(&getZBCTableSizeParams);
    getZBCTableSizeParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
    CHECK_RC(getTableSize(&getZBCTableSizeParams));
    table_start_color = getZBCTableSizeParams.indexStart;
    table_end_color   = getZBCTableSizeParams.indexEnd;
    Printf(Tee::PriHigh, "TEST: color table indexStart = 0x%x, indexEnd = 0x%x\n", table_start_color, table_end_color);

    ZERO_STRUCT(&getZBCTableSizeParams);
    getZBCTableSizeParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
    CHECK_RC(getTableSize(&getZBCTableSizeParams));
    table_start_depth = getZBCTableSizeParams.indexStart;
    table_end_depth   = getZBCTableSizeParams.indexEnd;
    Printf(Tee::PriHigh, "TEST: depth table indexStart = 0x%x, indexEnd = 0x%x\n", table_start_depth, table_end_depth);

    ZERO_STRUCT(&getZBCTableSizeParams);
    getZBCTableSizeParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
    CHECK_RC(getTableSize(&getZBCTableSizeParams));
    table_start_stencil = getZBCTableSizeParams.indexStart;
    table_end_stencil   = getZBCTableSizeParams.indexEnd;
    Printf(Tee::PriHigh, "TEST: stencil table indexStart = 0x%x, indexEnd = 0x%x\n", table_start_stencil, table_end_stencil);

    // Create a running repository of the table state (size = table_start + table_end to take reserved entry into account)
    storeZBCColorTable   = new LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS[table_start_color   + table_end_color];
    storeZBCDepthTable   = new LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS[table_start_depth   + table_end_depth];
    storeZBCStencilTable = new LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS[table_start_stencil + table_end_stencil];

    // Just print the current table state....
    for (i = table_start_color; i <= table_end_color; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        for (UINT32 j = 0; j<LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE ; j++)
        {
            Printf(Tee::PriHigh, "TEST:  COLOR_FB value allocated at index 0x%x is 0x%x\n",
                   j, getZBCTableEntryParams.value.colorFB[j]);
        }

        Printf(Tee::PriHigh, "TEST:  bIndexValid value allocated at index 0x%x is 0x%x\n",
               i, getZBCTableEntryParams.bIndexValid);
    }

    for (i = table_start_depth; i <= table_end_depth; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        Printf(Tee::PriHigh, "TEST:  DEPTH value    allocated at index 0x%x is  0x%x\n",
               i,  getZBCTableEntryParams.value.depth);
    }

    for (i = table_start_stencil; i <= table_end_stencil; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        Printf(Tee::PriHigh, "TEST:  STENCIL value  allocated at index 0x%x is  0x%x\n",
               i,  getZBCTableEntryParams.value.stencil);
    }

    // Check for the availiable space in table for further allocation.
    unused_color_count = 0;
    unused_depth_count = 0;
    unused_stencil_count = 0;

    for (i = table_start_color; i <= table_end_color; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        if (!getZBCTableEntryParams.bIndexValid)
        {
            unused_color_count = unused_color_count + 1;
        }
        else
        {
            // save the default values for future in table store
            for (UINT32 j = 0; j< LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
            {
                storeZBCColorTable[i].value.colorFB[j] = getZBCTableEntryParams.value.colorFB[j];
            }

            storeZBCColorTable[i].index       = getZBCTableEntryParams.index;
            storeZBCColorTable[i].tableType   = getZBCTableEntryParams.tableType;
            storeZBCColorTable[i].bIndexValid = getZBCTableEntryParams.bIndexValid;
        }
    }

    for (i = table_start_depth; i <= table_end_depth; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        if (!getZBCTableEntryParams.bIndexValid)
        {
            unused_depth_count = unused_depth_count + 1;
        }
        else
        {
            // save the default values for future in table store
            storeZBCDepthTable[i].value.depth = getZBCTableEntryParams.value.depth;
            storeZBCDepthTable[i].index       = getZBCTableEntryParams.index;
            storeZBCDepthTable[i].format      = getZBCTableEntryParams.format;
            storeZBCDepthTable[i].tableType   = getZBCTableEntryParams.tableType;
            storeZBCDepthTable[i].bIndexValid = getZBCTableEntryParams.bIndexValid;
        }
    }

    for (i = table_start_stencil; i <= table_end_stencil; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        if (!getZBCTableEntryParams.bIndexValid)
        {
            unused_stencil_count = unused_stencil_count + 1;
        }
        else
        {
            // save the default values for future in table store
            storeZBCStencilTable[i].value.stencil = getZBCTableEntryParams.value.stencil;
            storeZBCStencilTable[i].index         = getZBCTableEntryParams.index;
            storeZBCStencilTable[i].format        = getZBCTableEntryParams.format;
            storeZBCStencilTable[i].tableType     = getZBCTableEntryParams.tableType;
            storeZBCStencilTable[i].bIndexValid   = getZBCTableEntryParams.bIndexValid;
        }
    }

    Printf(Tee::PriHigh, "TEST: CASE I:\n");
    Printf(Tee::PriHigh, "TEST: Setting 15 different color/depth/stencil patterns using 1 value per object.\n");
    // CASE I:
    ////////////
    counter_color = 0;
    counter_depth = 0;
    counter_stencil = 0;
    Printf(Tee::PriHigh, "\n");
    while (add_color_entry || add_depth_entry || add_stencil_entry)
    {
        LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS   colorParams;
        LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS   depthParams;
        LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS stencilParams;

        if (add_color_entry)
        {
            ZERO_STRUCT(&colorParams);
            // Set the color Params.....
            colorParams.colorDS[0] = inputData[counter_color].color_DS[0].ui;
            colorParams.colorDS[1] = inputData[counter_color].color_DS[1].ui;
            colorParams.colorDS[2] = inputData[counter_color].color_DS[2].ui;
            colorParams.colorDS[3] = inputData[counter_color].color_DS[3].ui;

            colwertDStoFB(&colorParams);
        }
        if (add_depth_entry)
        {
            ZERO_STRUCT(&depthParams);
            // set the depth Params...
            depthParams.depth = 0x03ff0000 + 5*(m_entry_depth_num+1);
            // Only possible format option in depth
            depthParams.format = LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR_FMT_FP32;
        }
        if (add_stencil_entry)
        {
            ZERO_STRUCT(&stencilParams);
            // set the stencil Params...
            stencilParams.stencil = 0x03ff0000 + 5*(m_entry_stencil_num+1);
            // Only possible format option in stencil
            stencilParams.format = LW9096_CTRL_CMD_SET_ZBC_STENCIL_CLEAR_FMT_U8;
        }
        // Just a sanity check for access......
        CHECK_RC(pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_NULL,
            NULL,
            0));

        if (add_color_entry)
        {
            rc = pLwRm->Control(m_hZBCClearTest,
                LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
                &colorParams,
                sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

            if (rc == OK)
            {
                // Table size is "table_start_color + table_end_color" which takes the reserved entry into account
                index = (table_start_color + table_end_color - unused_color_count + m_entry_color_num);

                // Check if the sent data was updated in ZBC Table or not!!
                ZERO_STRUCT(&getZBCTableEntryParams);
                getZBCTableEntryParams.index = index;
                getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
                CHECK_RC(getTableEntry(&getZBCTableEntryParams));

                if (!getZBCTableEntryParams.bIndexValid)
                {
                    // The current LUT values already exist in ZBC Table hence
                    // no need updating the running store table.
                    Printf(Tee::PriHigh, "TEST: Given entry already exists in the Color table!!!\n");
                }
                else
                {
                    // Update the running table store...
                    for (UINT32 j = 0; j< LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
                    {
                        storeZBCColorTable[index].value.colorFB[j] = colorParams.colorFB[j];
                    }

                    storeZBCColorTable[index].index       = index;
                    storeZBCColorTable[index].tableType   = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
                    storeZBCColorTable[index].bIndexValid = true;
                    m_entry_color_num++;

                    Printf(Tee::PriHigh, "TEST: Color Value successfully written to index %d\n",index);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 0 0x%x\n",(UINT32)colorParams.colorDS[0]);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 1 0x%x\n",(UINT32)colorParams.colorDS[1]);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 2 0x%x\n",(UINT32)colorParams.colorDS[2]);
                    Printf(Tee::PriHigh, "TEST: Input DS value at subindex 3 0x%x\n",(UINT32)colorParams.colorDS[3]);
                    Printf(Tee::PriHigh, "TEST: Input FB Format is  0x%x\n",(UINT32)colorParams.format);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 0 0x%x\n",(UINT32)colorParams.colorFB[0]);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 1 0x%x\n",(UINT32)colorParams.colorFB[1]);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 2 0x%x\n",(UINT32)colorParams.colorFB[2]);
                    Printf(Tee::PriHigh, "TEST: Output FB value for index 3 0x%x\n",(UINT32)colorParams.colorFB[3]);
                    Printf(Tee::PriHigh, "\n");
                }
                counter_color++;
            }
            else
            {
                add_color_entry = false;
                rc.Clear();
            }
        }

        if (add_depth_entry)
        {
            rc = pLwRm->Control(m_hZBCClearTest,
                LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR,
                &depthParams,
                sizeof(LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS));

            if (rc == OK)
            {
                // Table size is "table_start_depth + table_end_depth" which takes the reserved entry into account
                index = (table_start_depth + table_end_depth - unused_depth_count + m_entry_depth_num);

                // Check if the sent data was updated in ZBC Table or not!!
                ZERO_STRUCT(&getZBCTableEntryParams);
                getZBCTableEntryParams.index = index;
                getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
                CHECK_RC(getTableEntry(&getZBCTableEntryParams));

                if (!getZBCTableEntryParams.bIndexValid)
                {
                    //
                    // The current LUT values already exist in ZBC Table hence
                    // no need updating the running store table.
                    //
                    Printf(Tee::PriHigh, "TEST: Given entry already exists in the Depth table!!!\n");
                }
                else
                {
                    // Update the running table store...
                    storeZBCDepthTable[index].value.depth = depthParams.depth;
                    storeZBCDepthTable[index].index       = index;
                    storeZBCDepthTable[index].format      = depthParams.format;
                    storeZBCDepthTable[index].tableType   = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
                    storeZBCDepthTable[index].bIndexValid = true;
                    m_entry_depth_num++;
                    Printf(Tee::PriHigh, "TEST: Depth Value successfully written to index %d\n",index);
                    Printf(Tee::PriHigh, "TEST: Depth Value written is  0x%x\n",(UINT32)depthParams.depth);
                    Printf(Tee::PriHigh, "\n");
                }
                counter_depth++;
            }
            else
            {
                add_depth_entry = false;
                rc.Clear();
            }
        }


        if (add_stencil_entry)
        {
            rc = pLwRm->Control(m_hZBCClearTest,
                LW9096_CTRL_CMD_SET_ZBC_STENCIL_CLEAR,
                &stencilParams,
                sizeof(LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS));

            if (rc == OK)
            {
                // Table size is "table_start_stencil + table_end_stencil" which takes the reserved entry into account
                index = (table_start_stencil + table_end_stencil - unused_stencil_count + m_entry_stencil_num);

                // Check if the sent data was updated in ZBC Table or not!!
                ZERO_STRUCT(&getZBCTableEntryParams);
                getZBCTableEntryParams.index = index;
                getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
                CHECK_RC(getTableEntry(&getZBCTableEntryParams));

                if (!getZBCTableEntryParams.bIndexValid)
                {
                    //
                    // The current LUT values already exist in ZBC Table hence
                    // no need updating the running store table.
                    //
                    Printf(Tee::PriHigh, "TEST: Given entry already exists in the Stencil table!!!\n");
                }
                else
                {
                    // Update the running table store...
                    storeZBCStencilTable[index].value.stencil = stencilParams.stencil;
                    storeZBCStencilTable[index].index         = index;
                    storeZBCStencilTable[index].format        = stencilParams.format;
                    storeZBCStencilTable[index].tableType     = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
                    storeZBCStencilTable[index].bIndexValid   = true;
                    m_entry_stencil_num++;
                    Printf(Tee::PriHigh, "TEST: Stencil Value successfully written to index %d\n",index);
                    Printf(Tee::PriHigh, "TEST: Stencil Value written is  0x%x\n",(UINT32)stencilParams.stencil);
                    Printf(Tee::PriHigh, "\n");
                }
                counter_stencil++;
            }
            else
            {
                add_stencil_entry = false;
                rc.Clear();
            }
        }
    }

    //
    // The first three color entries in RM will match the first three color entries in inputData[] array, so counter_color
    // should equal to "unused_color_count + 3"
    //
    if ((unused_color_count != (m_entry_color_num)) || (unused_depth_count != (m_entry_depth_num)) || (unused_stencil_count != (m_entry_stencil_num)) ||
        (counter_color != (unused_color_count + 3)) || (counter_depth != unused_depth_count) || (counter_stencil != unused_stencil_count))
    {
         Printf(Tee::PriHigh, "ERROR: counter_color=%d, counter_depth=%d, counter_stencil=%d\n", counter_color, counter_depth, counter_stencil);
         Printf(Tee::PriHigh, "ERROR: Number of color/depth/stencil values allocated is not same as the availiable slot in table\n");
         Printf(Tee::PriHigh, "ERROR: Number of COLOR values allocated is %d\n", m_entry_color_num);
         Printf(Tee::PriHigh, "ERROR: Number of DEPTH values allocated is %d\n", m_entry_depth_num);
         Printf(Tee::PriHigh, "ERROR: Number of STENCIL values allocated is %d\n", m_entry_stencil_num);
         Printf(Tee::PriHigh, "ERROR: Number of availiable slots in COLOR table is %d\n", unused_color_count);
         Printf(Tee::PriHigh, "ERROR: Number of availiable slots in DEPTH table is %d\n", unused_depth_count);
         Printf(Tee::PriHigh, "ERROR: Number of availiable slots in STENCIL table is %d\n", unused_stencil_count);
         rc = RC::SOFTWARE_ERROR;
         return rc;
    }
    else
    {
         Printf(Tee::PriHigh, "Filled all the empty slots in both the tables with fresh values\n");
         Printf(Tee::PriLow,  "Initially number of COLOR values allocated is %d\n", m_entry_color_num);
         Printf(Tee::PriLow,  "Initially number of DEPTH values allocated is %d\n", m_entry_depth_num);
         Printf(Tee::PriLow,  "Initially number of STENCIL values allocated is %d\n", m_entry_stencil_num);
    }

    //
    // Note: any object_index [i] has the index
    // (LW9096_CTRL_ZBC_CLEAR_TABLE_SIZE - color_Count/depth_Count + i)
    // in the color/depth table.
    // Object allocation starts at
    // "LW9096_CTRL_ZBC_CLEAR_TABLE_SIZE - color_Count/depth_Count" index in the table.
    //

    Printf(Tee::PriHigh, "TEST: CASE II:\n");
    Printf(Tee::PriHigh, "TEST: Check if the read COLOR/DEPTH/STENCIL data the same as written.\n");
    //
    // CASE II
    /////////////
    // Now try reading the written values...
    // This also checks for the Endian Swap problem...
    // In case of Big endian machine any read or write to L2 directly from the
    // HW register will automatically be swapped hence any read or write to
    // the L2 in Big endian machine should be accompanied by a endian swap
    // before (Write) or  after (Read).
    //
    for (i = table_start_color; i <= table_end_color; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        for (UINT32 j = 0; j<LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE ; j++)
        {
            if (getZBCTableEntryParams.value.colorFB[j] != storeZBCColorTable[i].value.colorFB[j])
            {
                rc = RC::SOFTWARE_ERROR;
                Printf(Tee::PriHigh, "ERROR: Read COLOR values at index 0x%x is not same as written\n",i);
                return rc;
            }
        }
    }

    for (i = table_start_depth; i <= table_end_depth; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        if (getZBCTableEntryParams.value.depth != storeZBCDepthTable[i].value.depth)
        {
            rc = RC::SOFTWARE_ERROR;
            Printf(Tee::PriHigh, "ERROR: Read DEPTH values at index 0x%x is not same as written\n",i);
            return rc;
        }
    }

    for (i = table_start_stencil; i <= table_end_stencil; i++)
    {
        ZERO_STRUCT(&getZBCTableEntryParams);
        getZBCTableEntryParams.index = i;
        getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
        CHECK_RC(getTableEntry(&getZBCTableEntryParams));

        if (getZBCTableEntryParams.value.stencil != storeZBCStencilTable[i].value.stencil)
        {
            rc = RC::SOFTWARE_ERROR;
            Printf(Tee::PriHigh, "ERROR: Read STENCIL values at index 0x%x is not same as written\n",i);
            return rc;
        }
    }

    Printf(Tee::PriHigh, "TEST: Written COLOR/DEPTH/STENCIL values are read properly\n\n");

    Printf(Tee::PriHigh, "TEST: CASE III:\n");
    Printf(Tee::PriHigh, "TEST: Check if data still matches after a suspend resume operation.\n");
#if ENABLE_SR_TEST

    //
    // case III
    ////////////
    // Now read after a suspend resume operation...
    // This case gaurantees that the table restoration work is going fine
    //
    if (callSuspendResumeOperation( ) == OK)
    {
        for (i = table_start_color; i <= table_end_color; i++)
        {
            ZERO_STRUCT(&getZBCTableEntryParams);
            getZBCTableEntryParams.index = i;
            getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;
            CHECK_RC(getTableEntry(&getZBCTableEntryParams));

            for (UINT32 j = 0; j<LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE ; j++)
            {
                if (getZBCTableEntryParams.value.colorFB[j] != storeZBCColorTable[i].value.colorFB[j])
                {
                    rc = RC::SOFTWARE_ERROR;
                    Printf(Tee::PriHigh, "ERROR: Read COLOR values after resume at index 0x%x is not same as written\n",i);
                    return rc;
                }
            }
        }

        for (i = table_start_depth; i <= table_end_depth; i++)
        {
            ZERO_STRUCT(&getZBCTableEntryParams);
            getZBCTableEntryParams.index = i;
            getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;
            CHECK_RC(getTableEntry(&getZBCTableEntryParams));

            if (getZBCTableEntryParams.value.depth != storeZBCDepthTable[i].value.depth)
            {
                rc = RC::SOFTWARE_ERROR;
                Printf(Tee::PriHigh, "ERROR: Read DEPTH values after resume at index 0x%x is not same as written\n",i);
                return rc;
            }
        }

        for (i = table_start_stencil; i <= table_end_stencil; i++)
        {
            ZERO_STRUCT(&getZBCTableEntryParams);
            getZBCTableEntryParams.index = i;
            getZBCTableEntryParams.tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;
            CHECK_RC(getTableEntry(&getZBCTableEntryParams));

            if (getZBCTableEntryParams.value.stencil != storeZBCStencilTable[i].value.stencil)
            {
                rc = RC::SOFTWARE_ERROR;
                Printf(Tee::PriHigh, "ERROR: Read STENCIL values after resume at index 0x%x is not same as written\n",i);
                return rc;
            }
        }

        Printf(Tee::PriHigh, "TEST: COLOR/DEPTH/STENCIL values still match after resume\n\n");
    }
    else
    {
        Printf(Tee::PriHigh, "ERROR: Suspend resume failed\n\n");
    }
#else

    Printf(Tee::PriHigh, "TEST: ENABLE_SR_TEST not set, ignoring SR test\n\n");

#endif

    Printf(Tee::PriHigh, "TEST: CASE IV:\n");
    Printf(Tee::PriHigh, "TEST: Check if we can still add new entry while it's already full.\n");
    //
    // CASE IV
    ////////////
    // See if we can add more entries in the table .
    // Since the table is full it should not be possible now.
    //

    // Set the color Params.....
    LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS  colorParamsAdd;
    ZERO_STRUCT(&colorParamsAdd);
    colorParamsAdd.colorDS[0] = inputData[counter_color].color_DS[0].ui;
    colorParamsAdd.colorDS[1] = inputData[counter_color].color_DS[1].ui;
    colorParamsAdd.colorDS[2] = inputData[counter_color].color_DS[2].ui;
    colorParamsAdd.colorDS[3] = inputData[counter_color].color_DS[3].ui;

    colwertDStoFB(&colorParamsAdd);

    rc = pLwRm->Control(m_hZBCClearTest,
        LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
        &colorParamsAdd,
        sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Failed to add new entry through old object in the table. Going Fine\n\n");
        rc.Clear();
    }
    else
    {
        Printf(Tee::PriHigh, "ERROR: Added entry to an already filled table\n\n");
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    Printf(Tee::PriHigh, "TEST: CASE V:\n");
    Printf(Tee::PriHigh, "TEST: Add old values which should be allowed (refcount increased).\n");
    //
    // CASE V
    ///////////
    // Adding old values using same object again should be fine
    // as it only increases HW refcount ...
    //

    for (i = table_start_color; i <= table_end_color; i++)
    {
        LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS colorParams;

        ZERO_STRUCT(&colorParams);

        // Set the color Params.....
        for (UINT32 j = 0; j < LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
        {
            colorParams.colorFB[j] = storeZBCColorTable[i].value.colorFB[j];
        }
        colorParams.format = storeZBCColorTable[i].format;

        rc = pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
            &colorParams,
            sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: Failed to add same color entry in the table\n");
            rc.Clear();
            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    for (i = table_start_depth; i <= table_end_depth; i++)
    {
        LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS depthParams;

        ZERO_STRUCT(&depthParams);

        // set the depth Params...
        depthParams.depth = storeZBCDepthTable[i].value.depth;
        depthParams.format = storeZBCDepthTable[i].format;

        rc = pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR,
            &depthParams,
            sizeof(LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS));

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: Failed to add same depth entry in the table\n");
            rc.Clear();
            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    for (i = table_start_stencil; i <= table_end_stencil; i++)
    {
        LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS stencilParams;

        ZERO_STRUCT(&stencilParams);

        // set the stencil Params...
        stencilParams.stencil = storeZBCStencilTable[i].value.stencil;
        stencilParams.format = storeZBCStencilTable[i].format;

        rc = pLwRm->Control(m_hZBCClearTest,
            LW9096_CTRL_CMD_SET_ZBC_STENCIL_CLEAR,
            &stencilParams,
            sizeof(LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS));

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: Failed to add same stencil entry in the table\n");
            rc.Clear();
            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    Printf(Tee::PriHigh, "TEST: Adding old values is successfully done.\n\n");

    delete [] inputData;
    delete [] storeZBCColorTable;
    delete [] storeZBCDepthTable;
    delete [] storeZBCStencilTable;

    return rc;
}

//!
//! \brief Class9096Test (ZBC clear test) Cleanup
//!
//! Most importantly, free GF100_ZBC_CLEAR object.
//! Deallocating/Freeing all the previously allocated objects and channels.
//!
//------------------------------------------------------------------------------
RC Class9096Test::Cleanup()
{
    LwRmPtr    pLwRm;
    RC         rc;

    pLwRm->Free(hMem1);

    if (m_hZBCClearTest)
    {
        pLwRm->Free(m_hZBCClearTest);
        m_hZBCClearTest = 0;
    }
    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//!
//! getColorDepth--- This function is used to get the updated state and data
//! contained in the Color/Depth table maintained in HW and set by SW using the
//! ZBC clear functionality supported in RM .
// The choice ot type of data COLOR/DEPTH depends on the parameter valType.
//-----------------------------------------------------------------------------

RC Class9096Test::getColorDepth( LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS  *pGetZBCTableParams)
{
    RC            rc;
    LwRmPtr       pLwRm;

    // Getting the latest Color/Depth set parameters
    CHECK_RC(pLwRm->Control(m_hZBCClearTest,
        LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE,
        pGetZBCTableParams,
        sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_PARAMS)));

    return rc;
}

//!
//! getTableSize--- This function is used to get the size of COLOR/DEPTH/STENCIL
//! table respectively. It should only be used in chips suppoting variable table size.
//! The choice ot type of data COLOR/DEPTH/STENCIL depends on the parameter tableType.
//-----------------------------------------------------------------------------
RC Class9096Test::getTableSize( LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS  *pGetZBCTableSizeParams)
{
    RC            rc;
    LwRmPtr       pLwRm;

    CHECK_RC(pLwRm->Control(m_hZBCClearTest,
        LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_SIZE,
        pGetZBCTableSizeParams,
        sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS)));

    return rc;
}

//!
//! getTableEntry--- This function is used to get specific entry of COLOR/DEPTH/STENCIL table.
//! The choice ot type of data COLOR/DEPTH/STENCIL depends on the parameter tableType.
//-----------------------------------------------------------------------------
RC Class9096Test::getTableEntry( LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS  *pGetZBCTableEntryParams)
{
    RC            rc;
    LwRmPtr       pLwRm;

    CHECK_RC(pLwRm->Control(m_hZBCClearTest,
        LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_ENTRY,
        pGetZBCTableEntryParams,
        sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS)));

    return rc;
}

//!
//! \brief Class9096Test (ZBC clear test) initColorDataArrayWithFormat
//! \This function is called intially to load the loacal data array with possible
//! \color value and formats. Making the first 3 elements same as the default
//! \ones to verify for the duplicate entry restriction in the table.
//! \While keeping the Color value for other indices same we verify for color
//! \equivalence.
//!
void Class9096Test::initColorDataArrayWithFormat(COLORDATA *inputData)
{
    int i, rand;
    UINT32 j;
    float rand_r, rand_g, rand_b, rand_a;

    rand = Rand32();
    rand_r = rand_g = rand_b = rand_a = (float)(rand);

    rand_r = rand_r/210;
    rand_g = rand_g/211;
    rand_b = rand_b/212;
    rand_a = rand_a/213;

    // Default table value hence should not consume any more index in real table.
    inputData[0].color_DS[0].f = 0;
    inputData[0].color_DS[1].f = 0;
    inputData[0].color_DS[2].f = 0;
    inputData[0].color_DS[3].f = 0;
    inputData[0].format     = FMTList[0];

    // Default table value hence should not consume any more index in real table.
    inputData[1].color_DS[0].f = 1.0;
    inputData[1].color_DS[1].f = 1.0;
    inputData[1].color_DS[2].f = 1.0;
    inputData[1].color_DS[3].f = 1.0;
    inputData[1].format     = FMTList[1];

    // Default table value hence should not consume any more index in real table.
    inputData[2].color_DS[0].f = 1.0;
    inputData[2].color_DS[1].f = 1.0;
    inputData[2].color_DS[2].f = 1.0;
    inputData[2].color_DS[3].f = 1.0;
    inputData[2].format     = FMTList[2];

    //-----------------------Non Default Value --------------------------------
    // Test the possible color CT Formats for a single DS color value .RGBA pack.
    //-------------------------------------------------------------------------
    for (i = 3; i < 19; i++)
    {
        inputData[i].color_DS[0].f = rand_r;
        inputData[i].color_DS[1].f = rand_g;
        inputData[i].color_DS[2].f = rand_b;
        inputData[i].color_DS[3].f = rand_a;
        inputData[i].format     = FMTList[i];
    }

    // If any space is still left in the table fill it with all random values.
    for (i = 19; i < COLOR_DATA_SIZE; i++)
    {
        for ( j =0; j < LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
        {
            int rand = Rand32();
            inputData[i].color_DS[j].f = ((float)rand)/31;
        }
        inputData[i].format     = FMTList[37 - i];
    }
    return;
}

//!
//! \brief Class9096Test (ZBC clear test) initColorDataArrayWithoutFormat
//! \This function is called intially to load the loacal data array with possible
//! \color values (This function will be used in AMPERE_and_later, so format value
//! \is ignored). Making the first 3 elements same as the default ones to verify
//! \for the duplicate entry restriction in the table. While keeping the Color
//! \value for other indices same we verify for color equivalence.
//!
void Class9096Test::initColorDataArrayWithoutFormat(COLORDATA *inputData)
{
    UINT32 i, j;

    // Default table value hence should not consume any more index in real table.
    inputData[0].color_DS[0].f = 0;
    inputData[0].color_DS[1].f = 0;
    inputData[0].color_DS[2].f = 0;
    inputData[0].color_DS[3].f = 0;
    // Still needs this format value so that we can correctly colwert DS value to same default FB value as RM
    inputData[0].format        = FMTList[0];

    // Default table value hence should not consume any more index in real table.
    inputData[1].color_DS[0].f = 1.0;
    inputData[1].color_DS[1].f = 1.0;
    inputData[1].color_DS[2].f = 1.0;
    inputData[1].color_DS[3].f = 1.0;
    // Still needs this format value so that we can correctly colwert DS value to same default FB value as RM
    inputData[1].format        = FMTList[1];

    // Default table value hence should not consume any more index in real table.
    inputData[2].color_DS[0].f = 1.0;
    inputData[2].color_DS[1].f = 1.0;
    inputData[2].color_DS[2].f = 1.0;
    inputData[2].color_DS[3].f = 1.0;
    // Still needs this format value so that we can correctly colwert DS value to same default FB value as RM
    inputData[2].format        = FMTList[2];

    //-----------------------Non Default Value --------------------------------
    // If any space is still left in the table fill it with all random values.
    for (i = 3; i < COLOR_DATA_SIZE; i++)
    {
        for ( j =0; j < LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE; j++)
        {
            int rand = Rand32();
            inputData[i].color_DS[j].f = ((float)rand)/31;
        }
        inputData[i].format     = 0;
    }
    return;
}

//!
//! colwertDStoFB--- This function is used to get FB(L2) values corresponding to an
//! input DS value and the DS color Format.
//! For depth values the FB clear values are same as DS clear values hence no
//! colwersion needed in that case.
//! Note : We assume that all data being transferred from the client is in
//         RGBA order only .
//-----------------------------------------------------------------------------

RC Class9096Test::colwertDStoFB( LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS  *pZBCColorParams)
{
    RC            rc;
    cARGB         colorVal, colorColwerted;

    colorVal.red.ui   = pZBCColorParams->colorDS[0];
    colorVal.green.ui = pZBCColorParams->colorDS[1];
    colorVal.blue.ui  = pZBCColorParams->colorDS[2];
    colorVal.alpha.ui = pZBCColorParams->colorDS[3];

    if (m_colorColwUtil.bytesPerElementCTFMT(pZBCColorParams->format) < 4)
    {
        Printf(Tee::PriLow, "INVALID CT FORMAT\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else if (OK != m_colorColwUtil.ColorColwertToFB(colorVal, pZBCColorParams->format, &colorColwerted))
    {
            rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        pZBCColorParams->colorFB[0] = colorColwerted.cout[0].ui;
        pZBCColorParams->colorFB[1] = colorColwerted.cout[1].ui;
        pZBCColorParams->colorFB[2] = colorColwerted.cout[2].ui;
        pZBCColorParams->colorFB[3] = colorColwerted.cout[3].ui;
    }

    return rc;
}

RC Class9096Test::callSuspendResumeOperation( )
{
    RC            rc = OK;
    // Fermi TODO: Write the suspend resume code here...
    rc = m_powerMgmtUtil.RmSetPowerState(m_powerMgmtUtil.GPU_PWR_SUSPEND);
    if (rc != OK)
    {
        Printf(Tee::PriLow, "Failed SUSPEND-GPU Operation\n");
        return rc;
    }

    rc = m_powerMgmtUtil.RmSetPowerState(m_powerMgmtUtil.GPU_PWR_RESUME);
    if (rc != OK)
    {
        Printf(Tee::PriLow, "Failed RESUME-GPU Operation\n");
        return rc;
    }
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------
//!
//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class9096Test, RmTest,
    "ZBC functionality test.");

