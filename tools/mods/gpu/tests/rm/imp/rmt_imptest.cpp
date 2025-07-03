/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_ImpTest.cpp
//! \brief This test verify whether the SW DispImpCode matches with HW DispImpCode
//!
//! Right now we have two different code base for the DispImp one in HW and other
//! in SW. We just port the code written by HW guys into the style of RM.
//! After porting in RM we don't have any test to verify whether it is matching
//! with HW DispImpCode or not.This test verify the SW DispImpCode in the same way
//! the HW guys used to verify there code by comparing with golden values

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include "gpu/tests/rmtest.h"
#include "class/clc372sw.h"
#include "ctrl/ctrl5070/ctrl5070chnc.h"
#include "ctrl/ctrlc372/ctrlc372chnc.h"
#include "ctrl/ctrlc372/ctrlc372verif.h"
#include "disp/v02_01/dev_disp.h"

#include "core/include/display.h"
#include "gpu/tests/rm/utility/dtiutils.h"

#include "display_imp.h"
#include "display_imp_core.h"

#define TEST_CONFIG_DIR           "fbimp/configs"
#define TEST_CONFIG_FMODEL_DIR    "fbimp/fmodelconfigs"
#define TEST_CONFIG_SW_GOLDEN     "fbimp/golden/"
#define TEST_FILES_DIR            "fbimp"

#define DEBUG_MSGS      0   // Set to 1 to enable messages for parser debug; 0 to disable

#define IGNORE_VALUE   0xBBBBBBBB
#define ILWALID_VALUE  0xCCCCCCCC
#define RETVAL_SUCCESS 0xEEEEEEEE
#define RETVAL_FAILURE 0xFFFFFFFF
#define RETVAL_NOTINSW 0xFEFEFEFE

//----------------------------------------------------------------------

// Structure used while finding displayIDs for each head
typedef struct
{
    string orProtocol;
    bool   orTaken;
} OUTPUT_RESOURCE;

enum CHIP_HAL_ID
{
    FBIMP_GF100,
    FBIMP_GF106,
    FBIMP_GF110D,
    FBIMP_GF110F,
    FBIMP_GF110F2,
    FBIMP_GF110F3,
    FBIMP_GF119,
    FBIMP_GK104,
    FBIMP_GK110,
    FBIMP_GV100,
    FBIMP_T234D
};

typedef enum configFileSection
{
    SECTION_NONE,
    SECTION_CLOCKS,
    SECTION_TEST
} CONFIG_FILE_SECTION;

#define MAX_IMP_PARAMS      500
#define ERROR_DELTA_AUTO    5

typedef struct
{
    UINT32  enumValue;
    char    *pName;
} ENUM_PAIR;

static const ENUM_PAIR enumRamType[] =
{
    { LW2080_CTRL_FB_INFO_RAM_TYPE_UNKNOWN,      "unknown"   },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_SDRAM,        "sdram"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_DDR1,         "ddr1"      },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR2,        "sys_sddr2" },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR2,        "gddr2"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR3,        "gddr3_0"   },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR4,        "gddr4"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR3,        "sys_ddr3"  },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5,        "gddr5"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_LPDDR2,       "lpddr2"    },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR3_BGA144, "gddr3"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR3_BGA136, "gddr3_1"   },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR4,        "sddr4"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X,       "gddr5x"    },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6,        "gddr6"     },
    { LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6X,       "gddr6x"    }
};

static const ENUM_PAIR enumPixelDepth[] =
{
    { LW_PIXEL_DEPTH_8,  "LW_PIXEL_DEPTH_8"  },
    { LW_PIXEL_DEPTH_16, "LW_PIXEL_DEPTH_16" },
    { LW_PIXEL_DEPTH_32, "LW_PIXEL_DEPTH_32" },
    { LW_PIXEL_DEPTH_64, "LW_PIXEL_DEPTH_64" }
};

static const ENUM_PAIR enumSuperSample[] =
{
    { LW_SUPER_SAMPLE_X1AA, "LW_SUPER_SAMPLE_X1AA" },
    { LW_SUPER_SAMPLE_X4AA, "LW_SUPER_SAMPLE_X4AA" }
};

static const ENUM_PAIR enumLutUsage[] =
{
    { LW_LUT_USAGE_NONE, "LW_LUT_USAGE_NONE" },
    { LW_LUT_USAGE_257,  "LW_LUT_USAGE_257"  },
    { LW_LUT_USAGE_1025, "LW_LUT_USAGE_1025" }
};

static const ENUM_PAIR enumLutMode[] =
{
    { LW_LUT_LO_MODE_LORES,                         "LW_LUT_LO_MODE_LORES"                         },
    { LW_LUT_LO_MODE_HIRES,                         "LW_LUT_LO_MODE_HIRES"                         },
    { LW_LUT_LO_MODE_INDEX_1025_UNITY_RANGE,        "LW_LUT_LO_MODE_INDEX_1025_UNITY_RANGE"        },
    { LW_LUT_LO_MODE_INTERPOLATE_1025_UNITY_RANGE,  "LW_LUT_LO_MODE_INTERPOLATE_1025_UNITY_RANGE"  },
    { LW_LUT_LO_MODE_INTERPOLATE_1025_XRBIAS_RANGE, "LW_LUT_LO_MODE_INTERPOLATE_1025_XRBIAS_RANGE" },
    { LW_LUT_LO_MODE_INTERPOLATE_1025_XVYCC_RANGE,  "LW_LUT_LO_MODE_INTERPOLATE_1025_XVYCC_RANGE"  },
    { LW_LUT_LO_MODE_INTERPOLATE_257_UNITY_RANGE,   "LW_LUT_LO_MODE_INTERPOLATE_257_UNITY_RANGE"   },
    { LW_LUT_LO_MODE_INTERPOLATE_257_LEGACY_RANGE,  "LW_LUT_LO_MODE_INTERPOLATE_257_LEGACY_RANGE"  }
};

static const ENUM_PAIR enumLutEnable[] =
{
    { LW_LUT_LO_DISABLE, "LW_LUT_LO_DISABLE" },
    { LW_LUT_LO_ENABLE,  "LW_LUT_LO_ENABLE"  }
};

static const ENUM_PAIR enumVerticalTaps[] =
{
    { LW_VERTICAL_TAPS_1,          "LW_VERTICAL_TAPS_1" },
    { LW_VERTICAL_TAPS_2,          "LW_VERTICAL_TAPS_2" },
    { LW_VERTICAL_TAPS_3,          "LW_VERTICAL_TAPS_3" },
    { LW_VERTICAL_TAPS_ADAPTIVE_3, "LW_VERTICAL_TAPS_ADAPTIVE_3" },
    { LW_VERTICAL_TAPS_5,          "LW_VERTICAL_TAPS_5" },
};

static const ENUM_PAIR enumForce422[] =
{
    { LW_OUTPUT_SCALER_FORCE422_MODE_DISABLE, "LW_OUTPUT_SCALER_FORCE422_MODE_DISABLE" },
    { LW_OUTPUT_SCALER_FORCE422_MODE_ENABLE,  "LW_OUTPUT_SCALER_FORCE422_MODE_ENABLE"  }
};

static const ENUM_PAIR enumOutputPixelDepth[] =
{
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_DEFAULT,    "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_DEFAULT"    },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_18_444, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_18_444" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_20_422, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_20_422" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_422, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_422" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_32_422, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_32_422" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444" },
    { LW5070_CTRL_IS_MODE_POSSIBLE_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_48_444, "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_48_444" }
};

static const ENUM_PAIR enumOwner[] =
{
    { static_cast<UINT32>(LW_OR_OWNER_HEAD0), "LW_OR_OWNER_HEAD0" },
    { static_cast<UINT32>(LW_OR_OWNER_HEAD1), "LW_OR_OWNER_HEAD1" },
    { static_cast<UINT32>(LW_OR_OWNER_HEAD2), "LW_OR_OWNER_HEAD2" },
    { static_cast<UINT32>(LW_OR_OWNER_HEAD3), "LW_OR_OWNER_HEAD3" },
    { static_cast<UINT32>(orOwner_None),      "orOwner_None"      }
};

static const ENUM_PAIR enumDacProtocol[] =
{
    { LW_DAC_PROTOCOL_RGB_CRT,        "LW_DAC_PROTOCOL_RGB_CRT"        },
};

static const ENUM_PAIR enumSorProtocol[] =
{
    { LW_SOR_PROTOCOL_SINGLE_TMDS_A, "LW_SOR_PROTOCOL_SINGLE_TMDS_A" },
    { LW_SOR_PROTOCOL_SINGLE_TMDS_B, "LW_SOR_PROTOCOL_SINGLE_TMDS_B" },
    { LW_SOR_PROTOCOL_DUAL_TMDS,     "LW_SOR_PROTOCOL_DUAL_TMDS"     },
    { LW_SOR_PROTOCOL_LVDS_LWSTOM,   "LW_SOR_PROTOCOL_LVDS_LWSTOM"   },
    { LW_SOR_PROTOCOL_DP_A,          "LW_SOR_PROTOCOL_DP_A"          },
    { LW_SOR_PROTOCOL_DP_B,          "LW_SOR_PROTOCOL_DP_B"          },
    { LW_SOR_PROTOCOL_SUPPORTED,     "LW_SOR_PROTOCOL_SUPPORTED"     }
};

static const ENUM_PAIR enumPiorProtocol[] =
{
    { LW_PIOR_PROTOCOL_EXT_TMDS_ENC,      "LW_PIOR_PROTOCOL_EXT_TMDS_ENC"      },
    { LW_PIOR_PROTOCOL_EXT_SDI_SD_ENC,    "LW_PIOR_PROTOCOL_EXT_SDI_SD_ENC"    },
    { LW_PIOR_PROTOCOL_EXT_SDI_HD_ENC,    "LW_PIOR_PROTOCOL_EXT_SDI_HD_ENC"    },
    { LW_PIOR_PROTOCOL_DIST_RENDER_OUT,   "LW_PIOR_PROTOCOL_DIST_RENDER_OUT"   },
    { LW_PIOR_PROTOCOL_DIST_RENDER_IN,    "LW_PIOR_PROTOCOL_DIST_RENDER_IN"    },
    { LW_PIOR_PROTOCOL_DIST_RENDER_INOUT, "LW_PIOR_PROTOCOL_DIST_RENDER_INOUT" },
    { LW_PIOR_PROTOCOL_EXT_TMDS_ENC,      "LW_PIOR_PROTOCOL_EXT_TMDS_ENC"      }
};

static const ENUM_PAIR enumPixelReplicateMode[] =
{
    { LW_PIXEL_REPLICATE_MODE_OFF, "LW_PIXEL_REPLICATE_MODE_OFF" },
    { LW_PIXEL_REPLICATE_MODE_X2,  "LW_PIXEL_REPLICATE_MODE_X2"  },
    { LW_PIXEL_REPLICATE_MODEP_X4, "LW_PIXEL_REPLICATE_MODEP_X4" }
};

static const ENUM_PAIR enumImpResult[] =
{
    { LWC372_CTRL_IMP_MODE_POSSIBLE,              "MODE_POSSIBLE"              },
    { LWC372_CTRL_IMP_NOT_ENOUGH_MEMPOOL,         "NOT_ENOUGH_MEMPOOL"         },
    { LWC372_CTRL_IMP_REQ_LIMIT_TOO_HIGH,         "REQ_LIMIT_TOO_HIGH"         },
    { LWC372_CTRL_IMP_VBLANK_TOO_SMALL,           "VBLANK_TOO_SMALL"           },
    { LWC372_CTRL_IMP_HUBCLK_TOO_LOW,             "HUBCLK_TOO_LOW"             },
    { LWC372_CTRL_IMP_INSUFFICIENT_BANDWIDTH,     "INSUFFICIENT_BANDWIDTH"     },
    { LWC372_CTRL_IMP_DISPCLK_TOO_LOW,            "DISPCLK_TOO_LOW"            },
    { LWC372_CTRL_IMP_ELV_START_TOO_HIGH,         "ELV_START_TOO_HIGH"         },
    { LWC372_CTRL_IMP_INSUFFICIENT_THREAD_GROUPS, "INSUFFICIENT_THREAD_GROUPS" },
    { LWC372_CTRL_IMP_ILWALID_PARAMETER,          "ILWALID_PARAMETER"          },
    { LWC372_CTRL_IMP_UNRECOGNIZED_FORMAT,        "UNRECOGNIZED_FORMAT"        },
    { LWC372_CTRL_IMP_UNSPECIFIED,                "UNSPECIFIED"                }
};

#define MAX_WINDOWS 8

class ImpTest : public RmTest
{
public:
    ImpTest();
    virtual ~ImpTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(fbConfigTestNumber, UINT32); //Config Test No
    SETGET_PROP(infile, string);
    SETGET_PROP(outfile,string);
    SETGET_PROP(workdir,string);
    
private:
    // Arch input parameters
    sysParams   params;
    headState   heads[MAX_HEADS];
    lwrsorState lwrsors[MAX_HEADS];
    windowState windows[MAX_WINDOWS];

    UINT32 m_fbConfigTestNumber;
    string  m_infile;
    string  m_outfile;
    string  m_workdir;
    string  m_ModsDirPath;
    FLOAT64 m_largestRmArchPercentDiff;
    Display     *m_pDisplay;
    LwRm::Handle                          m_hClient;
    LwRm::Handle                          m_hDisplayHandle;
    LwRm::Handle                          m_hDevice;
    UINT32                                m_gpuId;
    FILE                                  *m_ofile;
    UINT32                                m_NumHeads;
    UINT32                                m_activeHeads;
    UINT32                                m_activeWindows;
    bool                                  bIsEvoObject;

    LW5070_CTRL_CMD_VERIF_QUERY_FERMI_IS_MODE_POSSIBLE_PARAMS *m_pFermiImpParams;
    LW5070_CTRL_FERMI_IMP_FB_INPUT *m_pFermiFbImpIn;
    LW5070_CTRL_IMP_ISOHUB_INPUT *m_pFermiIsohubImpIn;

    LWC372_CTRL_IS_MODE_POSSIBLE_PARAMS                *m_pVoltaImpInParams;
    LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS               *m_pVoltaImpOutParams;

    // m_pVoltaImpOutArchParams0 and m_pVoltaImpOutArchParams1 are initialized
    // with 0's and 0xFF's respectively, as a simple way to tell which fields
    // contain valid arch values.  If a field in m_pVoltaImpOutArchParams0 has
    // 0, while the corresponding field in m_pVoltaImpOutArchParams1 has 0xFF,
    // then the field has not been loaded with a valid arch value.  If they
    // both contain the same value, then it is assumed to be a valid arch
    // value.  (This is perhaps not the most elegant way to know if the value
    // of a field is valid, but it makes it simple to add/remove fields for
    // comparison by simply adding/removing a single line in
    // CopyArchStructToRmStruct.)
    LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS               *m_pVoltaImpOutArchParams0;
    LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS               *m_pVoltaImpOutArchParams1;

    LWC372_CTRL_CMD_IMP_STATE_LOAD_FORCE_CLOCKS_PARAMS *m_pImpStateLoadForceClocksParams;

typedef struct {
    int X;
    int Y;
} ptXY;

typedef struct {
    int width;
    int height;
} dim2D;

typedef struct {
    float maxPixelClkMHz;
    ptXY  rasterBlankStart;
    ptXY  rasterBlankEnd;
    dim2D rasterSize;
} Raster;

typedef struct {
    int   leadingRasterLines;
    int   trailingRasterLines;

    int   lwrsorSize32p;
    int   olutOk;

    int   maxDownscaleFactorH;
    int   maxDownscaleFactorV;
    int   bUpscalingAllowedV;
    int   maxTapsV;
} HeadUb;

typedef struct {
    int formatUsageBound;
    int rotatedFormatUsageBound;
    int maxPixelsFetchedPerLine;
    int maxDownscaleFactorH;
    int maxDownscaleFactorV;
    int bUpscalingAllowedV;
    int maxTapsV;

    int ilutOk;
    int tmoOk;
} WindowUb; 

typedef enum
{
    IMP_PARAM_TYPE_COMMENT,
    IMP_PARAM_TYPE_FIELD,
    IMP_PARAM_TYPE_FIELD_ARRAY,
    IMP_PARAM_TYPE_ENUM,
    IMP_PARAM_TYPE_STRUCT,
    IMP_PARAM_TYPE_STRUCT_ARRAY,
    IMP_PARAM_TYPE_END_STRUCT,
    IMP_PARAM_TYPE_END
} IMP_PARAM_TYPE;

//
// PARAM_FLAGS_ARRAY_HEAD indicates that an IMP data array is a head array.  If
// the array is a head array, inactive heads can be skipped in the comparison
// and output.
//
#define PARAM_FLAGS_ARRAY_HEAD      1

//
// PARAM_FLAGS_ARRAY_WINDOW indicates that an IMP data array is a window array.
// If the array is a window array, inactive windows can be skipped in the
// comparison and output.
//
#define PARAM_FLAGS_ARRAY_WINDOW    2

typedef struct
{
    char    *pName;
    char    *pData;
    const ENUM_PAIR   *pTable;
    IMP_PARAM_TYPE  type;
    // size is the number of IMP_PARAM elements...
    UINT08  size;
    UINT08  numElements;
    UINT16  offset;
    UINT16  sizeArrayElement;
    UINT08  flags;
} IMP_PARAM;

    IMP_PARAM   impParams[MAX_IMP_PARAMS];
    IMP_PARAM   *m_pImpOutParams;
    IMP_PARAM   *m_pImpForceClocksParams;

    inline bool IsAMPEREorBetter();

    string StripComment (const string &line);
    string StripWhiteSpace (const string &line);
    vector<string> Tokenize(const string &str, const string &delimiters = " ");

    void   InitIMPstruct();

    ifstream * alloc_ifstream (vector<string> & searchpath, const string & fname);
    ifstream * FindConfigFileUpdatePath();

    RC     ProcessConfigFile();

    void   StripCommentWhiteSpaceAndTokenize(string origConfigLine, vector<string> *configToks);

    RC     InitForceClocksParseData(IMP_PARAM **ppImpParam);
    RC     InitLwDisplayInParseData(IMP_PARAM **ppImpParam);

    RC     InitLwDisplayOutParseData(IMP_PARAM **ppImpParam);
    RC     InitEvoInParseData(IMP_PARAM **ppImpParam);
    void   CallwlateDataOffsets(IMP_PARAM **ppImpParam);
    RC     InitImpDataList();

    RC     ParseInputString(string paramName, string paramValue, UINT32 lineNum, IMP_PARAM *pImpParam, void *pDataStruct);

    void   DisableHeads();
    void   DisableWindows();
    void   InitializeArchSysParamsT234(sysParams *pSysParams);
    void   setConfig(float rrrb_latency_nsec, float isoBW, float hubclkMHz, float dispclkMHz, int bIsRM);
    void   setUpHeadRM(int headId, Raster *rstr, HeadUb *ub);
    void   InitFormatUsageBound(int *pArchFormatUsageBound, LwU32 rmFormatUsageBound);
    void   setUpWindowRM(int windowId, int headId, WindowUb *ub);
    void   CopyArchStructToRmStruct(LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS *pImpOutArchParams);
    void   CallArchImp();
    void   TokenizePassByArg(string &str, string &delimiters, vector<string> *tokens);

    // Fermi DispImp
    void InitializeFermiDispInORParameters();
    string GetDacToString(UINT32 protocol);

    void PrintFieldLine(FILE *ofile, string *pNameStr, void *pData, void *pData2Copy0, void *pData2Copy1, UINT16 offset, UINT08 size);
    UINT32 PrintImpParam(string namePrefix, void *pData, void *pData2Copy0, void *pData2Copy1, IMP_PARAM *pImpParam, FILE* ofile, bool bSingleLineOut);

    FLOAT64 GetPercentDiff(UINT64 dataValue1, UINT64 dataValue2);
};

inline bool ImpTest::IsAMPEREorBetter()
{
#ifdef TEGRA_MODS
    return true;
#else
    return m_gpuId >= Gpu::GA100;
#endif
}

void ImpTest::DisableHeads()
{
    headState *pHead;
    lwrsorState *pLwrsor;    
    
    for (int h=0; h < MAX_HEADS; h++) {
        pHead = &heads[h];
        pHead->attachedOr = 0;

        pLwrsor = &lwrsors[h];
        pLwrsor->active = 0;
        pLwrsor->size = 256;
        pLwrsor->olutEn = 0;        
    }     
}

void ImpTest::DisableWindows()
{
    windowState *pWindow;
    
    for(int w=1; w < MAX_WINDOWS; w++) {
        pWindow =  &windows[w];
        pWindow->active = 0;
    }
}


//! \brief ImpTest constructor.
//!
//! \sa Setup
//----------------------------------------------------------------------------
ImpTest::ImpTest():m_fbConfigTestNumber(0)
{
    SetName("ImpTest");
    m_infile = "";
    m_outfile = "";
    m_workdir = "";
    m_ModsDirPath = "";
    m_ofile = NULL;
    m_NumHeads = 0;
    m_hClient       = 0;
    m_hDevice       = 0;
    m_hDisplayHandle = 0;
    bIsEvoObject = false;
    m_largestRmArchPercentDiff = 0.0L;
}

//! \brief ImpTest destructor.
//!
//! \sa Cleanup
//----------------------------------------------------------------------------
ImpTest::~ImpTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//! returns true implying that all platforms are supported
//----------------------------------------------------------------------------
string ImpTest::IsTestSupported()
{
    bool bGpusubHasIsoDisplay =
        GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY);
    bool bLwDisplay =
        (GetDisplay()->GetDisplayClassFamily() == Display::LWDISPLAY);

    m_gpuId = GetBoundGpuSubdevice()->DeviceId();
    Printf(Tee::PriHigh, "m_gpuId = %d\n", m_gpuId);
    //
    // m_gpuId is equal to a value from the "Constant" column of the table in
    // diag/mods/gpu/include/gpulist.h.
    //

    if (bGpusubHasIsoDisplay)
    {
        Printf(Tee::PriHigh, "GPUSUB_HAS_ISO_DISPLAY feature = true\n");
    }
    else
    {
        Printf(Tee::PriHigh, "GPUSUB_HAS_ISO_DISPLAY feature = false\n");
    }
    if (bLwDisplay)
    {
        Printf(Tee::PriHigh, "LwDisplay class = true\n");
    }
    else
    {
        Printf(Tee::PriHigh, "LwDisplay class = false\n");
    }

    //
    // Previously, we checked GPUSUB_HAS_ISO_DISPLAY to determine if the test
    // should be supported.  But the GPUSUB_HAS_ISO_DISPLAY feature flag is
    // lwrrently not set on T234D VDK (which seems strange, because T234D does
    // support display).  Anyway, to work around this, and to enable the test
    // for T234D, we also check for LwDisplay.
    //
    if (bGpusubHasIsoDisplay || bLwDisplay)
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "GPUSUB_HAS_ISO_DISPLAY is false and the Display Class is not LWDISPLAY";
    }
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup is used to reserve all the required resources used by this test,
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa Run
//----------------------------------------------------------------------------
RC ImpTest::Setup()
{
    RC rc;
    LwRmPtr   pLwRm;

    LwU32 classPreference[] = { LWC372_DISPLAY_SW };
    LwU32 classDesired = 0;

    Printf(Tee::PriHigh,"\n");

    m_pDisplay = GetDisplay();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pFermiImpParams = new LW5070_CTRL_CMD_VERIF_QUERY_FERMI_IS_MODE_POSSIBLE_PARAMS;
    m_pVoltaImpInParams = new LWC372_CTRL_IS_MODE_POSSIBLE_PARAMS;
    m_pVoltaImpOutParams = new LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS;
    m_pVoltaImpOutArchParams0 = new LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS;
    m_pVoltaImpOutArchParams1 = new LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS;
    m_pImpStateLoadForceClocksParams = new LWC372_CTRL_CMD_IMP_STATE_LOAD_FORCE_CLOCKS_PARAMS;

    if (m_pFermiImpParams == NULL ||
        m_pVoltaImpInParams == NULL ||
        m_pVoltaImpOutParams == NULL ||
        m_pVoltaImpOutArchParams0 == NULL ||
        m_pVoltaImpOutArchParams1 == NULL ||
        m_pImpStateLoadForceClocksParams == NULL)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    m_hClient = pLwRm->GetClientHandle();

    // There won't be any object assigned when using fmodel which is under RMTest handle.
    if (m_pDisplay != NULL && m_pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
          m_hDisplayHandle = m_pDisplay->GetDisplayHandle();
          bIsEvoObject = true;
    }
    else
    {
         // Allocate Device handle
         m_hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());

         for (unsigned int i = 0; i < sizeof(classPreference)/sizeof(LwU32); i++)
         {
             if (pLwRm->IsClassSupported(classPreference[i], GetBoundGpuDevice()))
             {
                 classDesired = classPreference[i];
                 break;
             }
         }

         // allocate Evo Object, the root is picked up automatically
         rc =  pLwRm->Alloc(
             m_hDevice,
             &m_hDisplayHandle,
             classDesired,   // Defined in sdk/lwpu/inc/class/cl5070.h
             NULL);

         if (rc != RC::OK)
         {
             Printf(Tee::PriHigh,
                 "***ERROR: DispTest::Initialize - Can't allocate display object from the RM\n");
             return rc;
         }
    } // end else

    return rc;
}

//! \brief Run the test!
//!
//! After the setup is successful the allocated resources can be used.
//! Run function uses the allocated resources.
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ImpTest::Run()
{
    RC rc;

    CHECK_RC(InitImpDataList());

    CHECK_RC(ProcessConfigFile());

    return rc;
}

//! \brief Initialize parameters required by T234 Arch IMP
void ImpTest::InitializeArchSysParamsT234
(
    sysParams   *pSysParams
)
{
    int tegra_ms;

    // Initialize parameters from LWDispIMPBase::LWDispIMPBase

    pSysParams->num_heads       = MAX_HEADS;
    pSysParams->num_windows     = MAX_WINDOWS;

    // Display System Parameters
    pSysParams->mc_design       = DGPU_MS; // Defaults here
    pSysParams->blx2_fetch      = 0;       // Default to BLx4
    pSysParams->bytes_per_line_per_pitch_req = 128;
    pSysParams->bytes_per_line_per_blxx_req   =  pSysParams->blx2_fetch ? 64 : 32;
    pSysParams->rrrb_size  = 1870;   // rrrb size in turing.

    //
    // Display Capabilities (initialzed as not all elwironments read the
    // capability registers)
    //
    pSysParams->osclr_cap_2tap_max_pixels = 5120;
    pSysParams->osclr_cap_5tap_max_pixels = 2560;
    pSysParams->isclr_cap_2tap_max_pixels = 5120;
    pSysParams->isclr_cap_5tap_max_pixels = 2560;

    // DGPU MS Parameters
    pSysParams->rrrb_latency_ns = 2000.0;    // Not used unless mc_design == DGPU_MS

    // CheetAh MSS Paramters
    pSysParams->peak_dramclk                                         = 3200;
    pSysParams->lwrrent_dramclk                                      = m_pVoltaImpOutParams->requiredDramClkKHz / 1000;
    pSysParams->lwrrent_mcclk                                        = m_pVoltaImpOutParams->requiredMcClkKHz / 1000;
    pSysParams->lwrrent_mchubclk                                     = m_pVoltaImpOutParams->requiredMchubClkKHz / 1000;
    pSysParams->Max_mtlb_misses_per_smmu_miss_period                 = 4000;    // Bug 1956805

    pSysParams->conservative_memory_efficiency                       = 0.5;
    pSysParams->iso_guaranteed_fraction_of_peak_memory_BW            = 1.0;
    pSysParams->display_catchup_factor                               = 1.1;
    pSysParams->bytes_per_dramclk                                    = 64;       // corresponds to 16 channels of memory
    pSysParams->row_sorter_size                                      = 114688;  // 16 channels * 112 entries/channel * 64 B/entry (see bug 2218658)
    pSysParams->static_latency_snap_arb_to_row_sorter_mcclks         = 14;      // Arch has 12
    pSysParams->static_latency_snap_arb_to_row_sorter_mchubclks      = 32;      // Arch has 31
    pSysParams->static_latency_minus_snap_arb_to_row_sorter_dramclks = 241;     // Arch has 217
    pSysParams->static_latency_minus_snap_arb_to_row_sorter_mcclks   = 14;      // Arch has 12
    pSysParams->static_latency_minus_snap_arb_to_row_sorter_mchubclks = 257;    // Arch has 209
    pSysParams->static_latency_minus_snap_arb_to_row_sorter_displayhubclks = 6;
    pSysParams->expiration_time_dramclks                             = 152;
    pSysParams->expiration_time_ns                                   = 72;
    pSysParams->bandwidth_disruption_dramclks                        = 14565;
    pSysParams->bandwidth_disruption_mcclks                          = 207;     // Arch has 63
    pSysParams->bandwidth_disruption_ns                              = 2590.5;  // Arch has 2620.5
    pSysParams->smmu_component_of_bw_disruption_time_dramclks        = 9005;
    pSysParams->max_drain_time                                       = 10;
    pSysParams->max_latency_allowance                                = 7.65;
    pSysParams->dvfs_min_spacing                                     = 4000;
    pSysParams->dvfs_time_at_lwrrent_dramclk                         = 0;
    pSysParams->thermal_management_latency_usec                      = 0;
    pSysParams->smmu_miss_period_bw_MBps                             = 10000;
    pSysParams->mccif_buffer_size                                    = 64 * 512;
    pSysParams->max_thread_groups                                    = 1;         // Maximum number of thread groups supported

    pSysParams->dda_min                                              = -5;
    pSysParams->mchub_dda_div                                        = 0.5;
    pSysParams->iso_dda_multiplier                                   = 1.2;
    pSysParams->max_grant_decrement                                  = 1.5 - 1.0 / 4096; // 1.5 - pow(2.0, -1.0 * PTSA_reg_length_bits) = 1.5 - 2^(-12)
    pSysParams->pcfifo_size                                          = 45;
    pSysParams->row_sorter_entries_per_channel                       = 112;
    pSysParams->max_dram_freq_for_dram_type_MHz                      = 3200.0;    

    // DDA Settings
    pSysParams->ddaRate     = 1.0;
    pSysParams->ddaGntDec   = 2;
    pSysParams->dda_min     = DDA_ACLWM_MIN;
    pSysParams->dda_max     = DDA_ACLWM_MAX;
    pSysParams->mccifSize   = MCCIF_SIZE;

    pSysParams->ddaRate_lwrrent = 0.0;
    pSysParams->mccifSize_lwrrent = 0;

    // Debug Level
    pSysParams->debug_level = 2;

    // IHUB Parameters
    pSysParams->batch_size                   = pSysParams->blx2_fetch ? 2 : 4;
    pSysParams->ihub_internal_latency        = 40;             // request and response pipeline latency in clocks
    pSysParams->ihub_response_latency_clocks = 10;             // response pipe latency in hubclks
    //
    // Fetch Metering Latency: According to bug 1608655, emperical worst case
    // is when 1 window has a metering slots value of 8 & all others have a
    // metering slots value of 15.  That window will have to wait for half the
    // metering cycle to pass before it is able to make a request.  During that
    // time, it will drain 4 requests (half its metering value) of data from
    // the pool.
    pSysParams->static_ihub_latency_bytes    =  pSysParams->batch_size * 4 * 128;   // 2 request/batch (batch size) * 4 slots * 129 bytes per request. Worst case callwlated emperically
    pSysParams->static_ihub_latency_clocks   = 15 * 11 * pSysParams->batch_size;    // Worst case metering latency = 11 clients * 15 slots * 2 requests per batch
    pSysParams->ihub_formatter_buffer_window = 1024;   // 8 MEMPOOL entries
    pSysParams->ihub_formatter_buffer_lwrsor = 1024;   // 8 MEMPOOL entries
    pSysParams->ihub_mempool_output_buffer   = 832;    // HOLD FIFO = (64 bytes * 13 entries)
    pSysParams->ihub_decomp_bytes_thread     = 1536;   // 12 MEMPOOL entries
    pSysParams->ihub_fbuf_bytes_thread       = 2048;   // 16 MEMPOOL entries
    pSysParams->mempool_size                 = pSysParams->blx2_fetch ? 655360 : 1413120; // MEMPOOL size in bytes = 5120 entries * 128 bytes

    // IMP Limits
    pSysParams->max_dispclk = m_pVoltaImpOutParams->maxDispClkKHz / 1000;
    pSysParams->max_hubclk = m_pVoltaImpOutParams->maxHubClkKHz / 1000;

    // Fetch Metering
    pSysParams->use_enhanced_fetch_meter = 0;
    pSysParams->batchIneff_disable  =pSysParams->use_enhanced_fetch_meter ? 0 : 1;
    pSysParams->ihubRefClkMHz = 108.0; // Reference clock for BW measurement
    pSysParams->hubclk_min    = 42.86;    // Min hubclk with legacy fetch meter
    pSysParams->catchup_factor_enable = 0;  // Enable catchup factor (CheetAh FD IMP)

    // Drain BW Knobs
    pSysParams->amortizeDrain       = 1;
    pSysParams->marginAmortizeDrain = 32;

    // Drain Metering    
    pSysParams->use_drain_metering = 1;   // default enables drain metering and disables pipe metering
    pSysParams->max_drain_slots    = 32;  // MAX Value for DRAIN METERING slots
    pSysParams->ihub_drain_meter_latency_clocks = pSysParams->max_drain_slots * 11; // Worst case drain metering latency = 11 clients * 15 requests per client

    // MSCG, MCLK Swithc Settings
    pSysParams->lowpower_program_insv2 = 1;  // Program watermarks in SV2
    pSysParams->dispclk_lwrrent = 0;
    pSysParams->hubclk_lwrrent = 0;

    for (int i = 0; i < MAX_WINDOWS; i++)
    {
        windows[i].active = false;
    }

    for (int i = 0; i < MAX_HEADS; i++)
    {
        heads[i].pclkMHz = 0;
        heads[i].active = 0;
        // IMP Transition Tests - initialize variables
        heads[i].h_total_lwrrent = 0;
        heads[i].raster_height_lwrrent = 0;
        heads[i].leadRstrLines_lwrrent = 0;
        heads[i].elv_start_lwrrent = 0;            
        heads[i].elv_advance_for_spoolup_lwrrent_usecs = 0;
        heads[i].elv_start_transition = 0;

        lwrsors[i].active = 0;
        lwrsors[i].olutEn = 0;
    }

    for (int i = 0; i < MAX_HEADS; i++)
    {
        lwrsors[i].active = 0;
        lwrsors[i].olutEn = 0;
    }
    // Spool up Optimizations
    pSysParams->ilutSpoolUpOptEnable = 0;
    // IMP Transition Tests
    pSysParams->glitchlessModeSet = 1;

    pSysParams->num_active_heads = 0;

    // Low Power Results
    for(int h = 0; h < MAX_HEADS; h++)
    {
        heads[h].vblankSwitchPossible   = 0;
        heads[h].midFrameSwitchPossible = 0;
        heads[h].dwcf_time              = 0;
    }

    // GV100 Additions.
    pSysParams->skipIMP = 0;
    pSysParams->useProdElvSetting = 0;

    // Full Display v/s IMP
    pSysParams->isFullDispTest = 1;

    // Memsu Test, Rm Test, TegraMss present
    pSysParams->isMemsuTest    = 0;
    pSysParams->isRmTest        = 1;
    pSysParams->hasTegraMss     = 1;

    // FD IMP queueing limit
    pSysParams->max_requests_in_fb = 0;

    // SW IMP Comparison
    pSysParams->use_por_hubclock       = 0;
    pSysParams->use_por_dispclk        = 0;
    pSysParams->available_fb_bw        = 0;
    pSysParams->min_mempool_fits_olut  = 0;

    // CheetAh Limits
    pSysParams->fetch_bw_limit         = 0;

    // Initialize parameters from LWDispIMPBase::m_UpdateBlFetchRelatedParams
    pSysParams->batch_size                   = pSysParams->blx2_fetch ? 2 : 4;
    pSysParams->mempool_size                 = pSysParams->blx2_fetch ? 655360 : 1413120; // MEMPOOL size in bytes = 5120 entries * 128 bytes
    pSysParams->static_ihub_latency_bytes    =  pSysParams->batch_size * 4 * 128;   // 2 request/batch (batch size) * 4 slots * 128 bytes per request. Worst case callwlated emperically
    pSysParams->static_ihub_latency_clocks   = 15 * 11 * pSysParams->batch_size;    // Worst case metering latency = 11 clients * 15 slots * 2 requests per batch

    if (pSysParams->debug_level > 1)
        printf("Batch Size: %d, Memmpool size: %d\n", pSysParams->batch_size, pSysParams->mempool_size);

    // Initialize parameters from LWDispIMPBase::m_UpdateSysParams
    tegra_ms = 1;
    if (tegra_ms)
    {
        pSysParams->mc_design = TEGRA_MS;
        pSysParams->rrrb_size = 0;
        pSysParams->compression_enable = 0;
        pSysParams->lwrsor_compression_enable = 0;
        pSysParams->batch_size = 16;
        pSysParams->static_ihub_latency_bytes    =  pSysParams->batch_size * 4 * 128;
        pSysParams->static_ihub_latency_clocks   = 15 * 11 * pSysParams->batch_size;
        pSysParams->use_enhanced_fetch_meter = 0;    // legacy fetch meter
    }
}

//! \brief Set config required for the ARCH test!
//!
//! We set the all configs required by ARCH apart from config file.
//!
//-----------------------------------------------------------------------------
void ImpTest::setConfig(float rrrb_latency_nsec, float isoBW, float hubclkMHz, float dispclkMHz, int bIsRM)
{
    memset(&params, 0, sizeof(params));

    // Display System Parameters
    params.mc_design       =  DGPU_MS;
    params.blx2_fetch      =  0;       // Default to BLx4
    params.bytes_per_line_per_pitch_req = 128;
    params.bytes_per_line_per_blxx_req   =  params.blx2_fetch ? 64 : 32;

    // DGPU MS Parameters
    params.rrrb_latency_ns = rrrb_latency_nsec;

    params.max_thread_groups            = 1;         // Maximum number of thread groups supported

    // Request Parametes
    params.batch_size                   = params.blx2_fetch ? 2 : 4;
    
    // IHUB Parameters
    params.ihub_internal_latency        = 23;   // request and response pipeline latency in clocks
    // Fetch Metering Latency: According to bug 1608655, emperical worst case is when 1 window has a metering slots value of 8 & all others have a metering slots value of 15.
    // That window will have to wait for half the metering cycle to pass before it is able to make a request.
    // During that time, it will drain 4 requests (half its metering value) of data from the pool.
    params.static_ihub_latency_bytes    =  params.batch_size * 4 * 128;
    params.static_ihub_latency_clocks   = 15 * 11 * params.batch_size;    // Worst case metering latency = 11 clients * 15 slots * 2 requests per batch
    params.ihub_formatter_buffer_window = 1024;    // 8 MEMPOOL entries
    params.ihub_formatter_buffer_lwrsor = 1024;    // 8 MEMPOOL entries
    params.ihub_mempool_output_buffer   = 832;    // HOLD FIFO = (64 bytes * 13 entries)
    params.ihub_decomp_bytes_thread     = 1536;   // 12 MEMPOOL entries
    params.ihub_fbuf_bytes_thread       = 2048;   // 16 MEMPOOL entries
    params.mempool_size                 = params.blx2_fetch ? 655360 : 1413120; // MEMPOOL size in bytes = 5120 entries * 128 bytes
    
    // Compression Parameters
    params.compression_enable        = 1;
    params.lwrsor_compression_enable = 1;
    params.comp_inefficiency_4bpp    = 1034.0 / 1024.0;
    params.comp_inefficiency_fp16    = 1032.0 / 1024.0;
    params.comp_latency_opt_enable   = 0;

    // Critical Watermark
    params.cwm_enable        = 0;

    // MEMPOOL Configuration
    params.use_min_buffering         = 1;

    // IMP limits
    params.max_dispclk               = dispclkMHz;
    params.max_hubclk                = hubclkMHz;

    // Indicates the Supervisor Interrrupt ID for IMP to have differnt code paths
    params.sv_id = 1;
    params.debug_level = 2;

    // Fetch Metering Computation
    params.use_enhanced_fetch_meter    = 0;     // This will be flipped once FD IMP testing is complete

    // ISOHUB Response Parameters
    params.use_drain_metering           = 1;
    params.max_drain_slots              = 32;  // MAX Value for DRAIN METERING slots
    params.ihub_drain_meter_latency_clocks = params.max_drain_slots * 11; // Worst case drain metering latency = 11 clients * 15 requests per client

    // MSCG
    params.mscg_enable = 0;

    // MCLK Switch
    params.mclkSw_enable = 0;

    // MSCG, MCLK Switch
    params.skip_enable_programming = 0;

    // Spool up Optimizations
    params.ilutSpoolUpOptEnable = 1;

    // IMP Transition Tests (default other than for PERF testing)
    params.impForPerf           = 0; // IMP for Perf enablement is skipping the set all heads active. So use this for now? Check with Sreenivas.
    params.skipIMP              = 0;

    // SW IMP Comparison
    params.use_por_hubclock     = 1;
    params.use_por_dispclk     = 1;
    params.por_dispClkMHz       = dispclkMHz;
    params.por_hubclk           = hubclkMHz;
    params.available_fb_bw      = isoBW;
    params.useProdElvSetting    = 0;

    //Lowpower features
    params.use_p8_clocks        = 0;
    params.use_max_mempool      = 0;
    
    // Full Display Tests
    params.isFullDispTest       = 0;

    // MemsuTests
    params.isMemsuTest          = bIsRM ? 0 : 1;
    params.num_heads           = m_pVoltaImpInParams->numHeads;
    params.num_windows         = m_pVoltaImpInParams->numWindows;

    // CheetAh Limits
    params.fetch_bw_limit       = 0;  // "0" means "don't check this limit"
}

//! \brief Set config for the head as per the config file
void ImpTest::setUpHeadRM(int headId, Raster *rstr, HeadUb *ub)
{
    headState *pHead;
    lwrsorState *pLwrsor;    

    pHead   =  &heads[headId];
    pLwrsor =  &lwrsors[headId];

    // Set Raster Parameters -- head 0

    pHead->pclkMHz       = rstr->maxPixelClkMHz;
    pHead->h_active      = (rstr->rasterBlankStart.X - rstr->rasterBlankEnd.X);
    pHead->h_blank       = (rstr->rasterSize.width - rstr->rasterBlankStart.X) + rstr->rasterBlankEnd.X;
    pHead->h_blank_start = rstr->rasterBlankStart.X;
    pHead->h_blank_end   = rstr->rasterBlankEnd.X;
    pHead->v_active      = (rstr->rasterBlankStart.Y - rstr->rasterBlankEnd.Y);
    pHead->v_blank       = (rstr->rasterSize.height - rstr->rasterBlankStart.Y) + rstr->rasterBlankEnd.Y;
    pHead->v_blank_start = rstr->rasterBlankStart.Y;
    pHead->v_blank_end   = rstr->rasterBlankEnd.Y;

    // Raster Lines UB
    pHead->leadRstrLines  = ub->leadingRasterLines;
    pHead->trailRstrLines = ub->trailingRasterLines;

    // Cursor and OLUT UB
    pHead->lwrsor_active     = ub->lwrsorSize32p != 0;
    pHead->lwrsor_max_pixels = ub->lwrsorSize32p * 32; 
    pHead->olutOk            = ub->olutOk;

    // Postcomp Scaler UB
    pHead->maxScaleFactorH = (float) ub->maxDownscaleFactorH / 1024;
    pHead->maxScaleFactorV = (float) ub->maxDownscaleFactorV / 1024;
    pHead->upSclOk = ub->bUpscalingAllowedV;
    pHead->maxTaps = ub->maxTapsV;

    // Light up the head
    pHead->attachedOr = 1;             // Enables the head

    // Set up Cursor on the head
    pLwrsor->active = ub->lwrsorSize32p != 0;
    pLwrsor->size   = ub->lwrsorSize32p * 32;
    pLwrsor->olutEn = ub->olutOk;

}

//! \brief Translate usage bound from RM format to Arch format
void ImpTest::InitFormatUsageBound
(
    int *pArchFormatUsageBound,
    LwU32 rmFormatUsageBound
)
{
    int archFormatUsageBound = 0;

    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_RGB_PACKED_1_BPP)
    {
        archFormatUsageBound |= FORMAT_1BPP_PACKED;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_RGB_PACKED_2_BPP)
    {
        archFormatUsageBound |= FORMAT_2BPP_PACKED;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_RGB_PACKED_4_BPP)
    {
        archFormatUsageBound |= FORMAT_4BPP_PACKED;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_RGB_PACKED_8_BPP)
    {
        archFormatUsageBound |= FORMAT_8BPP_PACKED;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_PACKED_422)
    {
        archFormatUsageBound |= FORMAT_422_PACKED;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_PLANAR_420)
    {
        archFormatUsageBound |= FORMAT_420_PLANAR;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_PLANAR_444)
    {
        archFormatUsageBound |= FORMAT_444_PLANAR;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_SEMI_PLANAR_420)
    {
        archFormatUsageBound |= FORMAT_420_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_SEMI_PLANAR_422)
    {
        archFormatUsageBound |= FORMAT_422_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_SEMI_PLANAR_422R)
    {
        archFormatUsageBound |= FORMAT_422R_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_YUV_SEMI_PLANAR_444)
    {
        archFormatUsageBound |= FORMAT_444_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_EXT_YUV_PLANAR_420)
    {
        archFormatUsageBound |= FORMAT_EXT_420_PLANAR;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_EXT_YUV_PLANAR_444)
    {
        archFormatUsageBound |= FORMAT_EXT_444_PLANAR;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_EXT_YUV_SEMI_PLANAR_420)
    {
        archFormatUsageBound |= FORMAT_EXT_420_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_EXT_YUV_SEMI_PLANAR_422)
    {
        archFormatUsageBound |= FORMAT_EXT_422_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_EXT_YUV_SEMI_PLANAR_422R)
    {
        archFormatUsageBound |= FORMAT_EXT_422R_SP;
    }
    if (rmFormatUsageBound & LWC372_CTRL_FORMAT_EXT_YUV_SEMI_PLANAR_444)
    {
        archFormatUsageBound |= FORMAT_EXT_444_SP;
    }
    *pArchFormatUsageBound = archFormatUsageBound;
}

//! \brief Set config for the window as per the config file
void ImpTest::setUpWindowRM(int windowId, int headId, WindowUb *ub)
{

    windowState *pWindow;

    pWindow = &windows[windowId];

    pWindow->active = 1;
    pWindow->headId = headId;                // Set by the test case
    pWindow->head   = &heads[headId];

    pWindow->fmtUb = ub->formatUsageBound;   // Set by the test case
    pWindow->rotFmtUb = ub->rotatedFormatUsageBound;
    pWindow->layout = SURFACE_BL4;           // Set to BlockLinear 4 line mode.

    pWindow->maxPixelsPerLine = ub->maxPixelsFetchedPerLine;  // Set by the test case
    pWindow->ilutOk = ub->ilutOk;
    pWindow->tmoOk  = ub->tmoOk;

    pWindow->maxScaleFactorH = (float) ub->maxDownscaleFactorH / 1024;
    pWindow->maxScaleFactorV = (float) ub->maxDownscaleFactorV / 1024;
    pWindow->upSclOk         = ub->bUpscalingAllowedV;
    pWindow->maxTaps         = ub->maxTapsV;
}

//! \brief Copies IMP data from the Arch structure to an RM structure
//!
//! The RM IMP data structure is well-defined; we have IMP_PARAM entries for
//! each field, and can easily process the entire structure in a loop.  After
//! the Arch data is copied to an instance of the IMP data structure, the
//! comparison of Arch and RM values can be done as part of the same loop.
//!
//! Any necessary colwersions (e.g., from float to unsigned int, or from
//! microseconds to nanoseconds) can be done here in this function.
//!
//! Any Arch values that are not copied over will not be compared.  This
//! provides a colwenient way to avoid cluttering up the output with
//! comparisons that we are not interested in.
void ImpTest::CopyArchStructToRmStruct
(
    LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS *pImpOutArchParams
)
{
    pImpOutArchParams->requiredDownstreamHubClkKHz = (int) floor(params.required_hubclk * 1000.0 + 0.5);
    //Since the ARCH is not using but we outputting let's keep them same for automation.
    pImpOutArchParams->availableBandwidthMBPS = m_pVoltaImpOutParams->availableBandwidthMBPS;
    pImpOutArchParams->rrrbLatencyNs = m_pVoltaImpOutParams->rrrbLatencyNs;
    pImpOutArchParams->requiredDispClkKHz = m_pVoltaImpOutParams->requiredDispClkKHz;

    for (int w=0; w < MAX_WINDOWS; w++)
    {
        pImpOutArchParams->minFBFetchRateWindowKBPS[w] = (int) floor(windows[w].fetchBw * 1000.0 + 0.5); 
        pImpOutArchParams->minMempoolAllocWindow[w] = windows[w].pool_config_min;
        pImpOutArchParams->mempoolAllocWindow[w] = windows[w].pool_config_max;
        pImpOutArchParams->fetchMeterWindow[w] = windows[w].metering_slots_value;
        if (IsAMPEREorBetter())
        {
            pImpOutArchParams->drainMeterWindow[w] = windows[w].drain_meter_value; 
        }
        if (!IsAMPEREorBetter())
        {
            pImpOutArchParams->requestLimitWindow[w] = m_pVoltaImpOutParams->requestLimitWindow[w];
            pImpOutArchParams->pipeMeterWindow[w] = (windows[w].pipe_meter_value & 0x3FFF);
            pImpOutArchParams->pipeMeterRatioWin[w] = (windows[w].pipe_meter_value >> 14) & 0x3;
        }

        int ownerHead = windows[w].headId;
        pImpOutArchParams->window[w].inputScalerInRateLinesPerSec = windows[w].active ? (int) floor(windows[w].scaler_input_rate * 1000000.0 / (heads[ownerHead].h_active + heads[ownerHead].h_blank) + 0.5) : 0;
    } 

    for (int h=0; h< MAX_HEADS; h++)
    {
        pImpOutArchParams->minFBFetchRateLwrsorKBPS[h] = (int) floor(lwrsors[h].fetchBw * 1000.0 + 0.5);
        pImpOutArchParams->minMempoolAllocLwrsor[h] = lwrsors[h].pool_config_min;
        pImpOutArchParams->mempoolAllocLwrsor[h] = lwrsors[h].pool_config_max;
        pImpOutArchParams->fetchMeterLwrsor[h] = lwrsors[h].metering_slots_value; 
        if (IsAMPEREorBetter())
        {
            pImpOutArchParams->drainMeterLwrsor[h] = lwrsors[h].drain_meter_value; 
        }
        if (!IsAMPEREorBetter())
        {
            pImpOutArchParams->requestLimitLwrsor[h] = m_pVoltaImpOutParams->requestLimitLwrsor[h];
            pImpOutArchParams->pipeMeterLwrsor[h] = (lwrsors[h].pipe_meter_value & 0x3FFF); 
            pImpOutArchParams->pipeMeterRatioLwrsor[h] = (lwrsors[h].pipe_meter_value >> 14) & 0x3; 
        }
        pImpOutArchParams->spoolUpTimeNs[h] = (heads[h].spoolup_time*1000);
        pImpOutArchParams->elvStart[h] = heads[h].elv_start;
        pImpOutArchParams->memfetchVblankDurationUs[h] = heads[h].vblank_duration;
        pImpOutArchParams->head[h].outputScalerInRateLinesPerSec = heads[h].active ? (int) floor(heads[h].pclkMHz * heads[h].maxScaleFactorVMod * 1000000.0 / (heads[h].h_active + heads[h].h_blank) + 0.5) : 0;
    }
}

//! \brief Set both window and head as per config file and call ARCH fucntion
void ImpTest::CallArchImp()
{
    DisableHeads();
    DisableWindows();
    memset(windows, 0, sizeof(windows));
    memset(lwrsors, 0, sizeof(lwrsors));
    memset(heads, 0, sizeof(heads));
    
    int bIsRM = 1U;

#if defined(TEGRA_MODS)
    if ((m_gpuId == Gpu::Txxx) || (m_gpuId == Gpu::T234))
    {
        InitializeArchSysParamsT234(&params);
    }
    else
#endif
    {
        setConfig(m_pVoltaImpOutParams->rrrbLatencyNs
                 ,m_pVoltaImpOutParams->availableBandwidthMBPS
                 ,m_pImpStateLoadForceClocksParams->hubClkHz
                 ,m_pImpStateLoadForceClocksParams->dispClkHz
                 ,bIsRM); 
        params.mempool_size = 1380000;// In ampere the mempool has been increased to 1380Kb from 640 in Turing.
    }

    for(int h=0; h < m_pVoltaImpInParams->numHeads ; h++) 
    {
        int   headId;
        
        headId  = h;                                 // Program:  Set this to the head #

        Raster rstr;
        rstr.maxPixelClkMHz   = m_pVoltaImpInParams->head[headId].maxPixelClkKHz / 1000;            // Program:  Set this to the PCLK Frequency (MHz)        
        ptXY blkStart;
        blkStart.X     = m_pVoltaImpInParams->head[headId].rasterBlankStart.X;
        blkStart.Y     =  m_pVoltaImpInParams->head[headId].rasterBlankStart.Y;
        rstr.rasterBlankStart = blkStart;            // Program: Set this to the Blanking start
        ptXY blkEnd;
        blkEnd.X       =  m_pVoltaImpInParams->head[headId].rasterBlankEnd.X;
        blkEnd.Y       =   m_pVoltaImpInParams->head[headId].rasterBlankEnd.Y;
        rstr.rasterBlankEnd = blkEnd;                // Program: Set this to the Blanking End
        dim2D rstrSz;
        rstrSz.width  = m_pVoltaImpInParams->head[headId].rasterSize.width;
        rstrSz.height =  m_pVoltaImpInParams->head[headId].rasterSize.height;
        rstr.rasterSize = rstrSz;                    // Program: Set this to the Size of the raster

        HeadUb ubH;
        ubH.leadingRasterLines    =   m_pVoltaImpInParams->head[headId].minFrameIdle.leadingRasterLines;            // Program:  Set this to the leading raster lines UB
        ubH.trailingRasterLines   =   m_pVoltaImpInParams->head[headId].minFrameIdle.trailingRasterLines;            // Program:  Set this to the trailing raster lines UB
        ubH.lwrsorSize32p         =    m_pVoltaImpInParams->head[headId].lwrsorSize32p;            // Program:  Set this to the cursor size
        ubH.olutOk                =    (m_pVoltaImpInParams->head[headId].lut != LW_LUT_USAGE_NONE)? 1 : 0;             // Program:  Set this to 1 if OLUT is required
        ubH.maxDownscaleFactorH   =     m_pVoltaImpInParams->head[headId].maxDownscaleFactorH;            // Program:  Set this to the Horizontal Scale Factor
        ubH.maxDownscaleFactorV   = m_pVoltaImpInParams->head[headId].maxDownscaleFactorV;            // Program:  Set this to the Vertical Scale Factor
        ubH.bUpscalingAllowedV    =    m_pVoltaImpInParams->head[headId].bUpscalingAllowedV;            // Program:  Set this if vertical upscaling is allowed
        ubH.maxTapsV              =    m_pVoltaImpInParams->head[headId].outputScalerVerticalTaps;            // Program:  Set this to the maximum number of vertical taps

        setUpHeadRM(headId, &rstr, &ubH);        
    }

    // Set up the Windows

    for(int w=0; w <  m_pVoltaImpInParams->numWindows; w++) {

        int ownerHeadId = w / 2;
        WindowUb ubW;

        InitFormatUsageBound(&ubW.formatUsageBound,
                             m_pVoltaImpInParams->window[w].formatUsageBound);
        InitFormatUsageBound(&ubW.rotatedFormatUsageBound,
                             m_pVoltaImpInParams->window[w].rotatedFormatUsageBound);

        ubW.maxPixelsFetchedPerLine = m_pVoltaImpInParams->window[w].maxPixelsFetchedPerLine;                  // Program: Set this to the maxPixelsPerLine UB
        ubW.ilutOk                  = (m_pVoltaImpInParams->window[w].lut != LW_LUT_USAGE_NONE)? 1 : 0;                     // Program: Set this to 1 if ILUT is required
        ubW.tmoOk                   = (m_pVoltaImpInParams->window[w].tmoLut != LW_LUT_USAGE_NONE)? 1 : 0;                     // Program: Set this to 1 if OLUT is required
        ubW.maxDownscaleFactorH     = m_pVoltaImpInParams->window[w].maxDownscaleFactorH;                  // Program: Set this to the horizontal down scaler factor
        ubW.maxDownscaleFactorV     =  m_pVoltaImpInParams->window[w].maxDownscaleFactorV;                  // Program: Set this to the vertical down scale factor
        ubW.bUpscalingAllowedV      =  m_pVoltaImpInParams->window[w].bUpscalingAllowedV;                     // Program: Set this if upscaling is allowed
        ubW.maxTapsV                = m_pVoltaImpInParams->window[w].inputScalerVerticalTaps;                     // Program: Set this to the max Taps UB

        setUpWindowRM(w, ownerHeadId, &ubW);
    }

    // Call IMP function
    int sv_id = 1;     // Program: Set to params.sv_id to get more debug output
    int isr_state = 0;
    DisplayIsModePossibleCore(sv_id, isr_state, heads, lwrsors, windows, &params);

    //
    // Copy the arch output values to the corresponding fields in an RM output
    // structure, for easy comparison with RM output values.
    //
    // The output values are actually copied to two structures,
    // m_pVoltaImpOutArchParams0 and m_pVoltaImpOutArchParams1, initialized
    // with 0's and 0xFF's respectively, as a simple way to tell which fields
    // contain valid arch values.  If a field in m_pVoltaImpOutArchParams0 has
    // 0, while the corresponding field in m_pVoltaImpOutArchParams1 has 0xFF,
    // then the field has not been loaded with a valid arch value.  If they
    // both contain the same value, then it is assumed to be a valid arch
    // value.  (This is perhaps not the most elegant way to know if the value
    // of a field is valid, but it makes it simple to add/remove fields for
    // comparison by simply adding/removing a single line in
    // CopyArchStructToRmStruct.)
    //
    memset(m_pVoltaImpOutArchParams0, 0, sizeof(*m_pVoltaImpOutArchParams0));
    CopyArchStructToRmStruct(m_pVoltaImpOutArchParams0);
    memset(m_pVoltaImpOutArchParams1, 0xFF, sizeof(*m_pVoltaImpOutArchParams1));
    CopyArchStructToRmStruct(m_pVoltaImpOutArchParams1);
}

//! \brief Strip the comments and white spaces
//!  This returns strings ans values in both strings
void ImpTest::StripCommentWhiteSpaceAndTokenize(string origConfigLine, vector<string> *configToks)
{
    string configLine;
    string delimiters = " ";

    configToks->clear();
    configLine = StripComment(origConfigLine);
    configLine = StripWhiteSpace(configLine);
    TokenizePassByArg (configLine,delimiters, configToks);
}

//! \brief Parses the input file and runs IMP.
//!
//! This function collects the description for one or more display configs
//! from a config file, runs RM IMP and Arch IMP on the configs, and logs the
//! results of the callwlations in the output file for comparison.
RC ImpTest::ProcessConfigFile()
{
    LwRmPtr pLwRm;
    RC rc;
    UINT32 lineNum = 0;
    std::vector<std::string> searchpath;
    IMP_PARAM   *pImpParam = impParams;
    string configLine;
    vector<string> configToks;
    string origConfigLine;
    UINT32 argNum;
    bool bFirstTest = true;
    bool bSectionChange;
    CONFIG_FILE_SECTION newSection;
    CONFIG_FILE_SECTION oldSection = SECTION_NONE;
    // Initialize to silence unexplained "uninitialized variable" warning.
    void *pOldDataStruct = nullptr;
    void *pNewDataStruct;
    string oldTestString;
    string newTestString;
    bool bGotInput;
    bool bForceClocks = false;

    InitIMPstruct();

    if (m_infile == "")
    {
        Printf(Tee::PriHigh, "No input file specified\n");
        return RC::CANNOT_OPEN_FILE;
    }

    if (m_outfile == "")
    {
        Printf(Tee::PriHigh, "No output file specified\n");
        return RC::CANNOT_OPEN_FILE;
    }

    searchpath.push_back(m_workdir.c_str());
    searchpath.push_back("");
    searchpath.push_back(TEST_CONFIG_DIR);
    searchpath.push_back(TEST_FILES_DIR);

    ifstream *pConfigFile = NULL;

    Printf(Tee::PriHigh, "Processing file %s\n", m_infile.c_str());
    pConfigFile = alloc_ifstream(searchpath, m_infile);
    if (pConfigFile == NULL)
    {
        Printf(Tee::PriHigh, "Cannot open config file: %s\n", m_infile.c_str());
        rc = RC::CANNOT_OPEN_FILE;
        goto Cleanup;
    }

    m_ofile = fopen(m_outfile.c_str(), "w");
    if (m_ofile == NULL)
    {
        Printf(Tee::PriHigh, "Cannot open %s\n", m_outfile.c_str());
        rc = RC::CANNOT_OPEN_FILE;
        goto Cleanup;
    }

    do
    {
        bSectionChange = false;

        getline(*pConfigFile, origConfigLine);
        bGotInput = !pConfigFile->fail();

        if (bGotInput)
        {
            lineNum++;
            StripCommentWhiteSpaceAndTokenize(origConfigLine, &configToks);
            if (configToks.size() == 0)
            {
                // Skip empty line.
                continue;
            }
            if (configToks[0][0] == '#')
            {
                // Skip comment line.
                continue;
            }

            if (configToks[0] == "Clocks")
            {
                if (configToks.size() > 1)
                {
                    Printf(Tee::PriHigh
                          ,"Line %d: \"Clocks\" statement must be on a line by itself\n"
                          ,lineNum);
                    rc = RC::CANNOT_PARSE_FILE;
                    goto Cleanup;
                }
                bSectionChange = true;
                newSection = SECTION_CLOCKS;
                pNewDataStruct = m_pImpStateLoadForceClocksParams;
            }
            else if (configToks[0] == "Test")
            {
                bSectionChange = true;
                newSection = SECTION_TEST;
                newTestString = configToks[1];
                if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
                {
                    pNewDataStruct = m_pVoltaImpInParams;
                }
                else
                {
                    pNewDataStruct = m_pFermiImpParams;
                }
            }
        }
        else
        {
            newSection = SECTION_NONE;
            if (newSection != oldSection)
            {
                bSectionChange = true;
            }
        }


        if (bSectionChange)
        {
            //
            // If we have come to a new section in the config file, then we
            // have probably also finished reading all of the data for an old
            // section (unless we are starting the first section of the file).
            // The old section can be processed now.  For a Clocks section, we
            // call an RmCtrl to send the clock values that were read from the
            // config file to RM.  For a Test section, we run IMP (both RM and
            // Arch) and output the results.
            //
            if (oldSection == SECTION_CLOCKS)
            {
                if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
                {
                    rc = pLwRm->Control(m_hDisplayHandle,
                                        LWC372_CTRL_CMD_IMP_STATE_LOAD_FORCE_CLOCKS,
                                        m_pImpStateLoadForceClocksParams,
                                        sizeof(LWC372_CTRL_CMD_IMP_STATE_LOAD_FORCE_CLOCKS_PARAMS));

                    if (rc != RC::OK)
                    {
                        Printf(Tee::PriHigh,
                               "LWC372_CTRL_CMD_IMP_STATE_LOAD_FORCE_CLOCKS failed (rc = %s)\n",
                               rc.Message());
                        goto Cleanup;
                    }
                    bForceClocks = true;
                }
            }
            else if (oldSection == SECTION_TEST)
            {
                if (bFirstTest)
                {
                    bFirstTest = false;
                }
                else
                {
                    fprintf(m_ofile, "\n\n");
                }
                fprintf(m_ofile, "Test %s\n", oldTestString.c_str());

                m_activeHeads = m_pVoltaImpInParams->numHeads;
                m_activeWindows = m_pVoltaImpInParams->numWindows;

                if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
                {
                    if (!bForceClocks)
                    {
                        m_pVoltaImpInParams->options |=
                            LWC372_CTRL_IS_MODE_POSSIBLE_OPTIONS_NEED_MIN_VPSTATE;
                    }
                    //
                    // Call RM IMP to callwlate IMP values for the specified
                    // mode.
                    //
                    rc = pLwRm->Control(m_hDisplayHandle
                                       ,LWC372_CTRL_CMD_IS_MODE_POSSIBLE
                                       ,m_pVoltaImpInParams
                                       ,sizeof(LWC372_CTRL_IS_MODE_POSSIBLE_PARAMS));

                    if (rc != RC::OK)
                    {
                        Printf(Tee::PriHigh
                              ,"LWC372_CTRL_CMD_IS_MODE_POSSIBLE failed (rc = %s)\n"
                              ,rc.Message());
                        goto Cleanup;
                    }

                    //
                    // Copy the results of the RM IMP callwlations to a local
                    // data structure.
                    //
                    rc = pLwRm->Control(m_hDisplayHandle
                                       ,LWC372_CTRL_CMD_GET_IMP_CALC_DATA
                                       ,m_pVoltaImpOutParams
                                       ,sizeof(LWC372_CTRL_GET_IMP_CALC_DATA_PARAMS));

                    if (rc != RC::OK)
                    {
                        Printf(Tee::PriHigh
                              ,"LWC372_CTRL_CMD_GET_IMP_CALC_DATA failed (rc = %s)\n"
                              ,rc.Message());
                        goto Cleanup;
                    }

                    //
                    // Call Arch IMP to callwlate their results for the same
                    // mode, for comparison.
                    //
                    CallArchImp();
                }

                // Write all of the input values to the output file.
                fprintf(m_ofile, "IMP INPUTS:\n");
                PrintImpParam(string("")
                             ,m_pVoltaImpInParams
                             ,NULL
                             ,NULL
                             ,pImpParam
                             ,m_ofile
                             ,true);

                //
                // Write all of the IMP output values (RM and Arch) to the
                // output file.
                //
                fprintf(m_ofile, "\nCALLWLATED OUTPUT VALUES:\n");
                fprintf(m_ofile, "OUTPUT                                            RM        ARCH     DIFF\n");
                PrintImpParam(string("")
                             ,m_pVoltaImpOutParams
                             ,m_pVoltaImpOutArchParams0
                             ,m_pVoltaImpOutArchParams1
                             ,m_pImpOutParams
                             ,m_ofile
                             ,true);
            }

            //
            // We have completed the old section.  Switch variables to the new
            // section that we are just starting to read from the config file.
            //
            oldSection = newSection;
            pOldDataStruct = pNewDataStruct;
            oldTestString = newTestString;
        }
        else
        {
            if (oldSection == SECTION_NONE)
            {
                Printf(Tee::PriHigh
                      ,"Line %d: No section recognized (\"Clocks\" or \"Test\" section header is required)\n"
                      ,lineNum);
                rc = RC::CANNOT_PARSE_FILE;
                goto Cleanup;
            }

            //
            // Read the clock data or the display mode data (depending on
            // whether we are in the Clocks section or the Test section) from
            // the config file into an internal data structure.
            //
            argNum = 0;
            MASSERT(pOldDataStruct != nullptr);
            while (argNum < configToks.size())
            {
                CHECK_RC_CLEANUP(ParseInputString(configToks[argNum]
                                                 ,configToks[argNum+1]
                                                 ,lineNum
                                                 ,impParams
                                                 ,pOldDataStruct));
                argNum += 2;
            }
        }
    } while (bGotInput);

    fprintf(m_ofile, "\nThe largest difference is %d%%\n",
            (UINT32) m_largestRmArchPercentDiff);

Cleanup:
    // Close the output file.
    if (m_ofile != NULL)
    {
        fclose(m_ofile);
        m_ofile = NULL;
    }

    // Close the input config file.
    if (pConfigFile != NULL)
    {
        pConfigFile->close();
        delete pConfigFile;
        pConfigFile = NULL;
    }

    return rc;
}

//! \brief ImpTest  FindConfigFileUpdatePath finds the
//! MODS_RUNSPACE directory and Updates m_ModsDirPath and opens
//! config.txt
//! \sa FindConfigFileUpdatePath
ifstream *ImpTest::FindConfigFileUpdatePath()
{
    ifstream *pInputFile = NULL;
    vector<string> searchpath;
    const char *pTestConfigFileName = "fbimp/fmodelconfigs/impin.txt";
    string modsDirPath = "";

    searchpath.push_back(m_workdir.c_str());
    searchpath.push_back (".");

    Utility::AppendElwSearchPaths(&searchpath, "MODS_RUNSPACE");
    Utility::AppendElwSearchPaths(&searchpath, "LD_LIBRARY_PATH");

    for (vector<string>::const_iterator it = searchpath.begin(); it != searchpath.end(); ++it)
    {
        const string & dirname = *it;
        string fullpath = "";

        fullpath = dirname;
        fullpath += "/";
        fullpath += *pTestConfigFileName;
        pInputFile = new ifstream (fullpath.c_str());

        if (pInputFile->is_open())
        {
            m_ModsDirPath = dirname;
            break;
        }
        delete pInputFile;
        pInputFile = NULL;
    }
    return pInputFile;
}

//
// The macros below are used to initialize the tables that represent the IMP
// data structure.  There is one macro for each type of data that needs to be
// handled.
//
#define BEGIN_STRUCT(name, path)  {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_STRUCT;\
    pImpParam->pName = name;\
    pImpParam->pData = (char *) &(path);\
    pImpParam++;\
    };

#define BEGIN_STRUCT_ARRAY(name, path, arrayFlags)  {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_STRUCT_ARRAY;\
    pImpParam->pName = name;\
    pImpParam->sizeArrayElement = sizeof(path[0]);\
    pImpParam->numElements = (UINT08) (sizeof(path)/sizeof(path[0]));\
    pImpParam->pData = (char *) &path[0];\
    pImpParam->flags = arrayFlags;\
    pImpParam++;\
    };

#define END_STRUCT()    {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_END_STRUCT;\
    pImpParam++;\
    };

#define COMMENT(text)   {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_COMMENT;\
    pImpParam->pName = text;\
    pImpParam++;\
    };

#define D_FIELD(name, path)   {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_FIELD;\
    pImpParam->pName = name;\
    pImpParam->size = sizeof(path);\
    pImpParam->pData = (char *) &path;\
    pImpParam++;\
    };

#define D_FIELD_ARRAY(name, path, arrayFlags)   {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_FIELD_ARRAY;\
    pImpParam->pName = name;\
    pImpParam->sizeArrayElement = sizeof(path[0]);\
    pImpParam->numElements = (UINT08) (sizeof(path)/sizeof(path[0]));\
    pImpParam->size = sizeof(path[0]);\
    pImpParam->pData = (char *) &path[0];\
    pImpParam->flags = arrayFlags;\
    pImpParam++;\
    };

#define D_ENUM(name, path, enumList) {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_ENUM;\
    pImpParam->pName = name;\
    pImpParam->size = sizeof(path);\
    pImpParam->numElements = (UINT08) (sizeof(enumList)/sizeof(enumList[0]));\
    pImpParam->pData = (char *) &path;\
    pImpParam->pTable = enumList;\
    pImpParam++;\
    };

#define END()   {\
    if (pImpParam >= &impParams[MAX_IMP_PARAMS]) return RC::LWRM_INSUFFICIENT_RESOURCES;\
    pImpParam->type = IMP_PARAM_TYPE_END;\
    };

//! \brief Fill in impParams array with description of force clocks input structure
//!
//! Macros are used, in order to create a relatively simple and concise
//! representation of the IMP data structure in RM.
RC ImpTest::InitForceClocksParseData(IMP_PARAM **ppImpParam)
{
  IMP_PARAM *pImpParam = *ppImpParam;

  BEGIN_STRUCT("", *m_pImpStateLoadForceClocksParams);
    D_FIELD("dispClkHz", m_pImpStateLoadForceClocksParams->dispClkHz);
    D_FIELD("ramClkHz", m_pImpStateLoadForceClocksParams->ramClkHz);
    D_FIELD("l2ClkHz", m_pImpStateLoadForceClocksParams->l2ClkHz);
    D_FIELD("xbarClkHz", m_pImpStateLoadForceClocksParams->xbarClkHz);
    D_FIELD("hubClkHz", m_pImpStateLoadForceClocksParams->hubClkHz);
    D_FIELD("sysClkHz", m_pImpStateLoadForceClocksParams->sysClkHz);
  END_STRUCT();
  END();
  *ppImpParam = pImpParam;

  return RC::OK;
}

//! \brief Fill in impParams array with description of Volta IMP input parameters
//!
//! Macros are used, in order to create a relatively simple and concise
//! representation of the IMP data structure in RM.
RC ImpTest::InitLwDisplayInParseData(IMP_PARAM **ppImpParam)
{
  IMP_PARAM *pImpParam = *ppImpParam;

  BEGIN_STRUCT("", *m_pVoltaImpInParams);
    D_FIELD("numHeads", m_pVoltaImpInParams->numHeads);
    D_FIELD("numWindows", m_pVoltaImpInParams->numWindows);
    COMMENT("# head parameters");
    BEGIN_STRUCT_ARRAY("head", m_pVoltaImpInParams->head, PARAM_FLAGS_ARRAY_HEAD);
      D_FIELD("headIndex", m_pVoltaImpInParams->head[0].headIndex);
      D_FIELD("maxPixelClkKHz", m_pVoltaImpInParams->head[0].maxPixelClkKHz);
      BEGIN_STRUCT("rasterSize", m_pVoltaImpInParams->head[0].rasterSize);
        D_FIELD("width", m_pVoltaImpInParams->head[0].rasterSize.width);
        D_FIELD("height", m_pVoltaImpInParams->head[0].rasterSize.height);
      END_STRUCT();
      BEGIN_STRUCT("rasterBlankStart", m_pVoltaImpInParams->head[0].rasterBlankStart);
        D_FIELD("X", m_pVoltaImpInParams->head[0].rasterBlankStart.X);
        D_FIELD("Y", m_pVoltaImpInParams->head[0].rasterBlankStart.Y);
      END_STRUCT();
      BEGIN_STRUCT("rasterBlankEnd", m_pVoltaImpInParams->head[0].rasterBlankEnd);
        D_FIELD("X", m_pVoltaImpInParams->head[0].rasterBlankEnd.X);
        D_FIELD("Y", m_pVoltaImpInParams->head[0].rasterBlankEnd.Y);
      END_STRUCT();
      BEGIN_STRUCT("rasterVertBlank2", m_pVoltaImpInParams->head[0].rasterVertBlank2);
        D_FIELD("yStart", m_pVoltaImpInParams->head[0].rasterVertBlank2.yStart);
        D_FIELD("yEnd", m_pVoltaImpInParams->head[0].rasterVertBlank2.yEnd);
      END_STRUCT();
      BEGIN_STRUCT("control", m_pVoltaImpInParams->head[0].control);
        D_FIELD("masterLockMode", m_pVoltaImpInParams->head[0].control.masterLockMode);
        D_FIELD("masterLockPin", m_pVoltaImpInParams->head[0].control.masterLockPin);
        D_FIELD("slaveLockMode", m_pVoltaImpInParams->head[0].control.slaveLockMode);
        D_FIELD("slaveLockPin", m_pVoltaImpInParams->head[0].control.slaveLockPin);
      END_STRUCT();
      D_FIELD("maxDownscaleFactorH", m_pVoltaImpInParams->head[0].maxDownscaleFactorH);
      D_FIELD("maxDownscaleFactorV", m_pVoltaImpInParams->head[0].maxDownscaleFactorV);
      D_FIELD("outputScalerVerticalTaps", m_pVoltaImpInParams->head[0].outputScalerVerticalTaps);
      D_FIELD("bUpscalingAllowedV", m_pVoltaImpInParams->head[0].bUpscalingAllowedV);
      BEGIN_STRUCT("minFrameIdle", m_pVoltaImpInParams->head[0].minFrameIdle);
        D_FIELD("leadingRasterLines", m_pVoltaImpInParams->head[0].minFrameIdle.leadingRasterLines);
        D_FIELD("trailingRasterLines", m_pVoltaImpInParams->head[0].minFrameIdle.trailingRasterLines);
      END_STRUCT();
      D_ENUM("lut", m_pVoltaImpInParams->head[0].lut, enumLutUsage);
      D_FIELD("lwrsorSize32p", m_pVoltaImpInParams->head[0].lwrsorSize32p);
    END_STRUCT();
    COMMENT("# window parameters");
    BEGIN_STRUCT_ARRAY("window", m_pVoltaImpInParams->window, PARAM_FLAGS_ARRAY_WINDOW);
      D_FIELD("windowIndex", m_pVoltaImpInParams->window[0].windowIndex);
      D_FIELD("owningHead", m_pVoltaImpInParams->window[0].owningHead);
      // formatUsageBound is a bit map for possible formats. If this is set as ENUM can only provide one format so just passing this as a D_FIELD for now
      D_FIELD("formatUsageBound", m_pVoltaImpInParams->window[0].formatUsageBound);
      D_FIELD("rotatedFormatUsageBound", m_pVoltaImpInParams->window[0].rotatedFormatUsageBound);
      D_FIELD("maxPixelsFetchedPerLine", m_pVoltaImpInParams->window[0].maxPixelsFetchedPerLine);
      D_FIELD("maxDownscaleFactorH", m_pVoltaImpInParams->window[0].maxDownscaleFactorH);
      D_FIELD("maxDownscaleFactorV", m_pVoltaImpInParams->window[0].maxDownscaleFactorV);
      D_FIELD("inputScalerVerticalTaps", m_pVoltaImpInParams->window[0].inputScalerVerticalTaps);
      D_FIELD("bUpscalingAllowedV", m_pVoltaImpInParams->window[0].bUpscalingAllowedV);
      D_ENUM("lut", m_pVoltaImpInParams->window[0].lut, enumLutUsage);
      D_ENUM("tmoLut", m_pVoltaImpInParams->window[0].tmoLut, enumLutUsage);
    END_STRUCT();
  END_STRUCT();
  END();
  *ppImpParam = pImpParam;

  return RC::OK;
}

//! \brief Fill in impParams array with description of Volta IMP output parameters
//!
//! Macros are used, in order to create a relatively simple and concise
//! representation of the IMP data structure in RM.
RC ImpTest::InitLwDisplayOutParseData(IMP_PARAM **ppImpParam)
{
    IMP_PARAM *pImpParam = *ppImpParam;

    BEGIN_STRUCT("", *m_pVoltaImpOutParams);
      COMMENT("# Callwlated values");
      D_ENUM("impResult", m_pVoltaImpOutParams->impResult, enumImpResult);
      D_FIELD("availableBandwidthMBPS", m_pVoltaImpOutParams->availableBandwidthMBPS);
      D_FIELD("rrrbLatencyNs", m_pVoltaImpOutParams->rrrbLatencyNs);
      D_FIELD("requiredDownstreamHubClkKHz", m_pVoltaImpOutParams->requiredDownstreamHubClkKHz);
      D_FIELD("requiredDispClkKHz", m_pVoltaImpOutParams->requiredDispClkKHz);
#ifdef TEGRA_MODS
      D_FIELD("maxDispClkKHz", m_pVoltaImpOutParams->maxDispClkKHz);
      D_FIELD("maxHubClkKHz", m_pVoltaImpOutParams->maxHubClkKHz);
      D_FIELD("requiredDramClkKHz", m_pVoltaImpOutParams->requiredDramClkKHz);
      D_FIELD("requiredMcClkKHz", m_pVoltaImpOutParams->requiredMcClkKHz);
      D_FIELD("requiredMchubClkKHz", m_pVoltaImpOutParams->requiredMchubClkKHz);
#endif  // TEGRA_MODS
      D_FIELD_ARRAY("minFBFetchRateWindowKBPS", m_pVoltaImpOutParams->minFBFetchRateWindowKBPS, PARAM_FLAGS_ARRAY_WINDOW);
      D_FIELD_ARRAY("minFBFetchRateLwrsorKBPS", m_pVoltaImpOutParams->minFBFetchRateLwrsorKBPS, PARAM_FLAGS_ARRAY_HEAD);
      D_FIELD_ARRAY("minMempoolAllocWindow", m_pVoltaImpOutParams->minMempoolAllocWindow, PARAM_FLAGS_ARRAY_WINDOW);
      D_FIELD_ARRAY("minMempoolAllocLwrsor", m_pVoltaImpOutParams->minMempoolAllocLwrsor, PARAM_FLAGS_ARRAY_HEAD);
      D_FIELD_ARRAY("mempoolAllocWindow", m_pVoltaImpOutParams->mempoolAllocWindow, PARAM_FLAGS_ARRAY_WINDOW);
      D_FIELD_ARRAY("mempoolAllocLwrsor", m_pVoltaImpOutParams->mempoolAllocLwrsor, PARAM_FLAGS_ARRAY_HEAD);
      D_FIELD_ARRAY("fetchMeterWindow", m_pVoltaImpOutParams->fetchMeterWindow, PARAM_FLAGS_ARRAY_WINDOW);
      D_FIELD_ARRAY("fetchMeterLwrsor", m_pVoltaImpOutParams->fetchMeterLwrsor, PARAM_FLAGS_ARRAY_HEAD);
      if (!IsAMPEREorBetter())
      {
          D_FIELD_ARRAY("requestLimitWindow", m_pVoltaImpOutParams->requestLimitWindow, PARAM_FLAGS_ARRAY_WINDOW);
          D_FIELD_ARRAY("requestLimitLwrsor", m_pVoltaImpOutParams->requestLimitLwrsor, PARAM_FLAGS_ARRAY_HEAD);
          D_FIELD_ARRAY("pipeMeterWindow", m_pVoltaImpOutParams->pipeMeterWindow, PARAM_FLAGS_ARRAY_WINDOW);
          D_FIELD_ARRAY("pipeMeterRatioWin", m_pVoltaImpOutParams->pipeMeterRatioWin, PARAM_FLAGS_ARRAY_WINDOW);
          D_FIELD_ARRAY("pipeMeterLwrsor", m_pVoltaImpOutParams->pipeMeterLwrsor, PARAM_FLAGS_ARRAY_HEAD);
          D_FIELD_ARRAY("pipeMeterRatioLwrsor", m_pVoltaImpOutParams->pipeMeterRatioLwrsor, PARAM_FLAGS_ARRAY_HEAD);
      }
      if (IsAMPEREorBetter())
      {
          D_FIELD_ARRAY("drainMeterWindow", m_pVoltaImpOutParams->drainMeterWindow, PARAM_FLAGS_ARRAY_WINDOW);   // Ampere+
          D_FIELD_ARRAY("drainMeterLwrsor", m_pVoltaImpOutParams->drainMeterLwrsor, PARAM_FLAGS_ARRAY_HEAD);   // Ampere+
      }
      D_FIELD_ARRAY("spoolUpTimeNs", m_pVoltaImpOutParams->spoolUpTimeNs, PARAM_FLAGS_ARRAY_HEAD);
      D_FIELD_ARRAY("elvStart", m_pVoltaImpOutParams->elvStart, PARAM_FLAGS_ARRAY_HEAD);
      D_FIELD_ARRAY("memfetchVblankDurationUs", m_pVoltaImpOutParams->memfetchVblankDurationUs, PARAM_FLAGS_ARRAY_HEAD);
      COMMENT("# head parameters");
      BEGIN_STRUCT_ARRAY("head", m_pVoltaImpOutParams->head, PARAM_FLAGS_ARRAY_HEAD);
        D_FIELD("outputScalerInRateLinesPerSec", m_pVoltaImpOutParams->head[0].outputScalerInRateLinesPerSec);
      END_STRUCT();
      COMMENT("# window parameters");
      BEGIN_STRUCT_ARRAY("window", m_pVoltaImpOutParams->window, PARAM_FLAGS_ARRAY_WINDOW);
        D_FIELD("inputScalerInRateLinesPerSec", m_pVoltaImpOutParams->window[0].inputScalerInRateLinesPerSec);
      END_STRUCT();
    END_STRUCT();
    END();
    *ppImpParam = pImpParam;

    return RC::OK;
}

//! \brief Fill in impParams array with description of Fermi IMP parameters
//!
//! Macros are used, in order to create a relatively simple and concise
//! representation of the IMP data structure in RM.
RC ImpTest::InitEvoInParseData(IMP_PARAM **ppImpParam)
{
    IMP_PARAM *pImpParam = *ppImpParam;

    BEGIN_STRUCT("", *m_pFermiImpParams);
      BEGIN_STRUCT("", m_pFermiImpParams->input); // input
        BEGIN_STRUCT("", m_pFermiImpParams->input.impFbIn);   // impFbIn
          BEGIN_STRUCT("DRAM", m_pFermiImpParams->input.impFbIn.DRAM);
            COMMENT("#DRAM Parameters");
            D_ENUM("ramType", m_pFermiImpParams->input.impFbIn.DRAM.ramType, enumRamType);
            D_FIELD("tRC", m_pFermiImpParams->input.impFbIn.DRAM.tRC);
            D_FIELD("tRAS", m_pFermiImpParams->input.impFbIn.DRAM.tRAS);
            D_FIELD("tRP", m_pFermiImpParams->input.impFbIn.DRAM.tRP);
            D_FIELD("tRCDRD", m_pFermiImpParams->input.impFbIn.DRAM.tRCDRD);
            D_FIELD("tWCK2MRS", m_pFermiImpParams->input.impFbIn.DRAM.tWCK2MRS);
            D_FIELD("tWCK2TR", m_pFermiImpParams->input.impFbIn.DRAM.tWCK2TR);
            D_FIELD("tMRD", m_pFermiImpParams->input.impFbIn.DRAM.tMRD);
            D_FIELD("EXT_BIG_TIMER", m_pFermiImpParams->input.impFbIn.DRAM.EXT_BIG_TIMER);
            D_FIELD("STEP_LN", m_pFermiImpParams->input.impFbIn.DRAM.STEP_LN);
            D_FIELD("asrClkConstD4L1", m_pFermiImpParams->input.impFbIn.DRAM.asrClkConstD4L1);
            D_FIELD("asrClkConstC1L2", m_pFermiImpParams->input.impFbIn.DRAM.asrClkConstC1L2);
            D_FIELD("asrClkConstC1L1", m_pFermiImpParams->input.impFbIn.DRAM.asrClkConstC1L1);
            D_FIELD("asrClkConstD1", m_pFermiImpParams->input.impFbIn.DRAM.asrClkConstD1);
            D_FIELD("numBanks", m_pFermiImpParams->input.impFbIn.DRAM.numBanks);
            D_FIELD("bytesPerClock", m_pFermiImpParams->input.impFbIn.DRAM.bytesPerClock);
            D_FIELD("bytesPerClockFromBusWidth", m_pFermiImpParams->input.impFbIn.DRAM.bytesPerClockFromBusWidth);
            D_FIELD("bytesPerActivate", m_pFermiImpParams->input.impFbIn.DRAM.bytesPerActivate);
            D_FIELD("timingRP", m_pFermiImpParams->input.impFbIn.DRAM.timingRP);
            D_FIELD("timingRFC", m_pFermiImpParams->input.impFbIn.DRAM.timingRFC);
            D_FIELD("tMRS2RDWCK", m_pFermiImpParams->input.impFbIn.DRAM.tMRS2RDWCK);
            D_FIELD("tQPOPWCK", m_pFermiImpParams->input.impFbIn.DRAM.tQPOPWCK);
            D_FIELD("tMRSTWCK", m_pFermiImpParams->input.impFbIn.DRAM.tMRSTWCK);
            D_FIELD("bAutoSync", m_pFermiImpParams->input.impFbIn.DRAM.bAutoSync);
            D_FIELD("bFastExit", m_pFermiImpParams->input.impFbIn.DRAM.bFastExit);
            D_FIELD("bX16", m_pFermiImpParams->input.impFbIn.DRAM.bX16);
            D_FIELD("tREFI", m_pFermiImpParams->input.impFbIn.DRAM.tREFI);
          END_STRUCT();
          BEGIN_STRUCT("FBP", m_pFermiImpParams->input.impFbIn.FBP);
            COMMENT("#FBP Parameters");
            D_FIELD("dramChipCountPerBwUnit", m_pFermiImpParams->input.impFbIn.FBP.dramChipCountPerBwUnit);
            D_FIELD("enabledDramBwUnits", m_pFermiImpParams->input.impFbIn.FBP.enabledDramBwUnits);
            D_FIELD("ltcBwUnitPipes", m_pFermiImpParams->input.impFbIn.FBP.ltcBwUnitPipes);
            D_FIELD("L2Slices", m_pFermiImpParams->input.impFbIn.FBP.L2Slices);
            D_FIELD("enabledLtcs", m_pFermiImpParams->input.impFbIn.FBP.enabledLtcs);
            D_FIELD("bytesPerClock", m_pFermiImpParams->input.impFbIn.FBP.bytesPerClock);
            D_FIELD("ltcBytesPerClockWithDcmp", m_pFermiImpParams->input.impFbIn.FBP.ltcBytesPerClockWithDcmp);
            D_FIELD("ltcBytesPerClockWithFos", m_pFermiImpParams->input.impFbIn.FBP.ltcBytesPerClockWithFos);
            D_FIELD("awpPoolEntries", m_pFermiImpParams->input.impFbIn.FBP.awpPoolEntries);
            D_FIELD("bytesPerAwpBlock", m_pFermiImpParams->input.impFbIn.FBP.bytesPerAwpBlock);
            D_FIELD("rrrbPoolEntries", m_pFermiImpParams->input.impFbIn.FBP.rrrbPoolEntries);
            D_FIELD("bytesPerRrrbBlock", m_pFermiImpParams->input.impFbIn.FBP.bytesPerRrrbBlock);
          END_STRUCT();
          BEGIN_STRUCT("XBAR", m_pFermiImpParams->input.impFbIn.XBAR);
            COMMENT("#XBAR Parameters");
            D_FIELD("bytesPerClock", m_pFermiImpParams->input.impFbIn.XBAR.bytesPerClock);
            D_FIELD("totalSlices", m_pFermiImpParams->input.impFbIn.XBAR.totalSlices);
            D_FIELD("maxFbpsPerXbarSlice", m_pFermiImpParams->input.impFbIn.XBAR.maxFbpsPerXbarSlice);
            D_FIELD("numHubPorts", m_pFermiImpParams->input.impFbIn.XBAR.numHubPorts);
          END_STRUCT();
          BEGIN_STRUCT("NISO", m_pFermiImpParams->input.impFbIn.NISO);
            COMMENT("#NISO Parameters");
            D_FIELD("bytesPerClockX1000", m_pFermiImpParams->input.impFbIn.NISO.bytesPerClockX1000);
          END_STRUCT();
          BEGIN_STRUCT("ISO", m_pFermiImpParams->input.impFbIn.ISO);
            COMMENT("#ISO Parameters");
            D_FIELD("linebufferAdditionalLines", m_pFermiImpParams->input.impFbIn.ISO.linebufferAdditionalLines);
            D_FIELD("linesFetchedForBlockLinear", m_pFermiImpParams->input.impFbIn.ISO.linesFetchedForBlockLinear);
            D_FIELD("linebufferTotalBlocks", m_pFermiImpParams->input.impFbIn.ISO.linebufferTotalBlocks);
            D_FIELD("linebufferMinBlocksPerHead", m_pFermiImpParams->input.impFbIn.ISO.linebufferMinBlocksPerHead);
            D_FIELD("bytesPerClock", m_pFermiImpParams->input.impFbIn.ISO.bytesPerClock);
          END_STRUCT();
          BEGIN_STRUCT("CLOCKS", m_pFermiImpParams->input.impFbIn.CLOCKS);
            COMMENT("#CLOCKS Parameters");
            D_FIELD("display", m_pFermiImpParams->input.impFbIn.CLOCKS.display);
            D_FIELD("ram", m_pFermiImpParams->input.impFbIn.CLOCKS.ram);
            D_FIELD("l2", m_pFermiImpParams->input.impFbIn.CLOCKS.l2);
            D_FIELD("xbar", m_pFermiImpParams->input.impFbIn.CLOCKS.xbar);
            D_FIELD("sys", m_pFermiImpParams->input.impFbIn.CLOCKS.sys);
            D_FIELD("hub", m_pFermiImpParams->input.impFbIn.CLOCKS.hub);
          END_STRUCT();
          BEGIN_STRUCT("roundtripLatency", m_pFermiImpParams->input.impFbIn.roundtripLatency);
            COMMENT("#roundtripLatency Parameters");
            D_FIELD("constant", m_pFermiImpParams->input.impFbIn.roundtripLatency.constant);
            D_FIELD("ramClks", m_pFermiImpParams->input.impFbIn.roundtripLatency.ramClks);
            D_FIELD("l2Clks", m_pFermiImpParams->input.impFbIn.roundtripLatency.l2Clks);
            D_FIELD("xbarClks", m_pFermiImpParams->input.impFbIn.roundtripLatency.xbarClks);
            D_FIELD("sysClks", m_pFermiImpParams->input.impFbIn.roundtripLatency.sysClks);
            D_FIELD("hubClks", m_pFermiImpParams->input.impFbIn.roundtripLatency.hubClks);
          END_STRUCT();
          BEGIN_STRUCT("returnLatency", m_pFermiImpParams->input.impFbIn.returnLatency);
            COMMENT("#returnLatency Parameters");
            D_FIELD("constant", m_pFermiImpParams->input.impFbIn.returnLatency.constant);
            D_FIELD("ramClks", m_pFermiImpParams->input.impFbIn.returnLatency.ramClks);
            D_FIELD("l2Clks", m_pFermiImpParams->input.impFbIn.returnLatency.l2Clks);
            D_FIELD("xbarClks", m_pFermiImpParams->input.impFbIn.returnLatency.xbarClks);
            D_FIELD("sysClks", m_pFermiImpParams->input.impFbIn.returnLatency.sysClks);
            D_FIELD("hubClks", m_pFermiImpParams->input.impFbIn.returnLatency.hubClks);
          END_STRUCT();
          BEGIN_STRUCT("CAPS", m_pFermiImpParams->input.impFbIn.CAPS);
            COMMENT("#CAPS Parameters");
            D_FIELD("bEccIsEnabled", m_pFermiImpParams->input.impFbIn.CAPS.bEccIsEnabled);
            D_FIELD("bForceMinMempool", m_pFermiImpParams->input.impFbIn.CAPS.bForceMinMempool);
          END_STRUCT();
        END_STRUCT();   // end impFbIn
        //
        // Historically, bMempoolCompression is grouped with the other CAPS
        // parameters in the config and output files.  We define it here to
        // maintain this ordering in the output file.  But since it is actually
        // part of the impIsohubIn structure, we need to temporarily close out
        // the impFbIn structure, open the impIsohubIn structure, and reopen
        // the impFbIn afterwards.
        //
        BEGIN_STRUCT("", m_pFermiImpParams->input.impIsohubIn);
          BEGIN_STRUCT("CAPS", m_pFermiImpParams->input.impIsohubIn.CAPS);
            D_FIELD("bMempoolCompression", m_pFermiImpParams->input.impIsohubIn.CAPS.bMempoolCompression);
          END_STRUCT();
        END_STRUCT();
        // Reopen impFbIn.
        BEGIN_STRUCT("", m_pFermiImpParams->input.impFbIn);
          BEGIN_STRUCT("ASR", m_pFermiImpParams->input.impFbIn.ASR);
            COMMENT("#ASR Parameters");
            D_FIELD("isAllowed", m_pFermiImpParams->input.impFbIn.ASR.isAllowed);
            D_FIELD("efficiencyThreshold", m_pFermiImpParams->input.impFbIn.ASR.efficiencyThreshold);
            D_FIELD("dllOn", m_pFermiImpParams->input.impFbIn.ASR.dllOn);
            D_FIELD("tXSR", m_pFermiImpParams->input.impFbIn.ASR.tXSR);
            D_FIELD("tXSNR", m_pFermiImpParams->input.impFbIn.ASR.tXSNR);
            D_FIELD("powerdown", m_pFermiImpParams->input.impFbIn.ASR.powerdown);
          END_STRUCT();
          BEGIN_STRUCT("MSCG", m_pFermiImpParams->input.impFbIn.MSCG);
            COMMENT("#MSCG Parameters");
            D_FIELD("bIsAllowed", m_pFermiImpParams->input.impFbIn.MSCG.bIsAllowed);
          END_STRUCT();
        END_STRUCT();   // end impFbIn
        BEGIN_STRUCT("impIsohubIn", m_pFermiImpParams->input.impIsohubIn);
          COMMENT("#Isohub Parameters");
          D_FIELD("maxLwrsorEntries", m_pFermiImpParams->input.impIsohubIn.maxLwrsorEntries);
          D_FIELD("pitchFetchQuanta", m_pFermiImpParams->input.impIsohubIn.pitchFetchQuanta);
        END_STRUCT();   // end impIsohubIn
        BEGIN_STRUCT("", m_pFermiImpParams->input.impDispIn);
          COMMENT("#DispHeadIn  Parameters");
          BEGIN_STRUCT_ARRAY("impDispHeadIn", m_pFermiImpParams->input.impDispIn.impDispHeadIn, PARAM_FLAGS_ARRAY_HEAD);
            BEGIN_STRUCT("BaseUsageBounds", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseUsageBounds);
              D_FIELD("Usable", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseUsageBounds.Usable);
              D_FIELD("BytesPerPixel", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseUsageBounds.BytesPerPixel);
              D_ENUM("SuperSample", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseUsageBounds.SuperSample, enumSuperSample);
              D_ENUM("BaseLutUsage", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseUsageBounds.BaseLutUsage, enumLutUsage);
              D_ENUM("OutputLutUsage", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseUsageBounds.OutputLutUsage, enumLutUsage);
            END_STRUCT();
            BEGIN_STRUCT("Control", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].Control);
              D_FIELD("Structure", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].Control.Structure);
            END_STRUCT();
            D_FIELD("HeadActive", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].HeadActive);
            BEGIN_STRUCT("ViewportSizeIn", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeIn);
              D_FIELD("Width", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeIn.Width);
              D_FIELD("Height", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeIn.Height);
            END_STRUCT();
            BEGIN_STRUCT("OutputScaler", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OutputScaler);
              D_ENUM("VerticalTaps", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OutputScaler.VerticalTaps, enumVerticalTaps);
              D_ENUM("Force422", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OutputScaler.Force422, enumForce422);
            END_STRUCT();
            D_ENUM("outputResourcePixelDepthBPP", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].outputResourcePixelDepthBPP, enumOutputPixelDepth);
            BEGIN_STRUCT("OverlayUsageBounds", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OverlayUsageBounds);
              D_FIELD("Usable", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OverlayUsageBounds.Usable);
              D_FIELD("BytesPerPixel", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OverlayUsageBounds.BytesPerPixel);
              D_ENUM("OverlayLutUsage", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OverlayUsageBounds.OverlayLutUsage, enumLutUsage);
            END_STRUCT();
            BEGIN_STRUCT("BaseLutLo", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseLutLo);
              D_ENUM("Enable", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseLutLo.Enable, enumLutEnable);
              D_ENUM("Mode", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].BaseLutLo.Mode, enumLutMode);
            END_STRUCT();
            BEGIN_STRUCT("OutputLutLo", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OutputLutLo);
              D_ENUM("Enable", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OutputLutLo.Enable, enumLutEnable);
              D_ENUM("Mode", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].OutputLutLo.Mode, enumLutMode);
            END_STRUCT();
            BEGIN_STRUCT("Params", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].Params);
              D_FIELD("CoreBytesPerPixel", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].Params.CoreBytesPerPixel);
              D_ENUM("SuperSample", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].Params.SuperSample, enumSuperSample);
            END_STRUCT();
            BEGIN_STRUCT("PixelClock", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].PixelClock);
              D_FIELD("Frequency", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].PixelClock.Frequency);
              D_FIELD("UseAdj1000Div1001", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].PixelClock.UseAdj1000Div1001);
            END_STRUCT();
            BEGIN_STRUCT("RasterBlankEnd", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterBlankEnd);
              D_FIELD("X", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterBlankEnd.X);
              D_FIELD("Y", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterBlankEnd.Y);
            END_STRUCT();
            BEGIN_STRUCT("RasterBlankStart", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterBlankStart);
              D_FIELD("X", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterBlankStart.X);
              D_FIELD("Y", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterBlankStart.Y);
            END_STRUCT();
            BEGIN_STRUCT("RasterSize", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterSize);
              D_FIELD("Width", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterSize.Width);
              D_FIELD("Height", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterSize.Height);
            END_STRUCT();
            BEGIN_STRUCT("RasterVertBlank2", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterVertBlank2);
              D_FIELD("YStart", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterVertBlank2.YStart);
              D_FIELD("YEnd", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].RasterVertBlank2.YEnd);
            END_STRUCT();
            BEGIN_STRUCT("ViewportSizeIn", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeIn);
              D_FIELD("Width", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeIn.Width);
              D_FIELD("Height", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeIn.Height);
            END_STRUCT();
            BEGIN_STRUCT("ViewportSizeOut", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOut);
              D_FIELD("Width", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOut.Width);
              D_FIELD("Height", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOut.Height);
            END_STRUCT();
            BEGIN_STRUCT("ViewportSizeOutMin", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOutMin);
              D_FIELD("Width", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOutMin.Width);
              D_FIELD("Height", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOutMin.Height);
            END_STRUCT();
            BEGIN_STRUCT("ViewportSizeOutMax", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOutMax);
              D_FIELD("Width", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOutMax.Width);
              D_FIELD("Height", m_pFermiImpParams->input.impDispIn.impDispHeadIn[0].ViewportSizeOutMax.Height);
            END_STRUCT();
          END_STRUCT();
          COMMENT("# DispImp Input impDacIn");
          BEGIN_STRUCT_ARRAY("impDacIn", m_pFermiImpParams->input.impDispIn.impDacIn, 0);
            D_ENUM("owner", m_pFermiImpParams->input.impDispIn.impDacIn[0].owner, enumOwner);
            D_ENUM("protocol", m_pFermiImpParams->input.impDispIn.impDacIn[0].protocol, enumDacProtocol);
          END_STRUCT();
          COMMENT("# DispImp Input impSorIn");
          BEGIN_STRUCT_ARRAY("impSorIn", m_pFermiImpParams->input.impDispIn.impSorIn, 0);
            D_ENUM("owner", m_pFermiImpParams->input.impDispIn.impSorIn[0].owner, enumOwner);
            D_ENUM("protocol", m_pFermiImpParams->input.impDispIn.impSorIn[0].protocol, enumSorProtocol);
            D_ENUM("pixelReplicateMode", m_pFermiImpParams->input.impDispIn.impSorIn[0].protocol, enumPixelReplicateMode);
          END_STRUCT();
          COMMENT("# DispImp Input impPiorIn");
          BEGIN_STRUCT_ARRAY("impPiorIn", m_pFermiImpParams->input.impDispIn.impPiorIn, 0);
            D_ENUM("owner", m_pFermiImpParams->input.impDispIn.impPiorIn[0].owner, enumOwner);
            D_ENUM("protocol", m_pFermiImpParams->input.impDispIn.impPiorIn[0].protocol, enumPiorProtocol);
          END_STRUCT(); // end impPiorIn
        END_STRUCT();   // end impDispIn
      END_STRUCT();     // end input
    END_STRUCT();
    END();
    *ppImpParam = pImpParam;

    return RC::OK;
}

//! \brief Callwlate structure and array sizes.
//!
//! This function uses relwrsion to traverse all of the structures within the
//! IMP data, and records the offset of each data element.  The offset is
//! relative to the beginning of the closest surrounding structure or array.
void ImpTest::CallwlateDataOffsets(IMP_PARAM **ppImpParam)
{
    char    *pBase;
    IMP_PARAM   *pImpParam = *ppImpParam;

    //
    // pImpParam should be pointing to an IMP_PARAM of type
    // IMP_PARAM_TYPE_STRUCT.
    //
    pBase = pImpParam->pData;
    pImpParam++;
    while (pImpParam->type != IMP_PARAM_TYPE_END_STRUCT)
    {
        pImpParam->offset = (UINT16) (pImpParam->pData - pBase);
#if DEBUG_MSGS
        Printf(Tee::PriHigh,
               "%s offset=%d\n",
               pImpParam->pName,
               pImpParam->offset);
#endif
        if (pImpParam->type == IMP_PARAM_TYPE_STRUCT ||
            pImpParam->type == IMP_PARAM_TYPE_STRUCT_ARRAY)
        {
            CallwlateDataOffsets(&pImpParam);
        }
        pImpParam++;
    }
    MASSERT(pImpParam - *ppImpParam < 0x100);
    (*ppImpParam)->size = (UINT08) (pImpParam - *ppImpParam);
#if DEBUG_MSGS
    Printf(Tee::PriHigh,
           "Old pImpParam: type = %d, addr = %p, size = %d; "
           "New pImpParam: type = %d, addr = %p, size = %d\n",
           (*ppImpParam)->type,
           *ppImpParam,
           (*ppImpParam)->size,
           pImpParam->type,
           pImpParam,
           pImpParam->size);
#endif
    *ppImpParam = pImpParam;
}

//! \brief Fill in impParams array with description of IMP parameters
RC ImpTest::InitImpDataList()
{
    IMP_PARAM   *pImpParam = impParams;
    RC          rc;

    if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        rc = InitLwDisplayInParseData(&pImpParam);
        if (rc != RC::OK)
        {
            if (RC::LWRM_INSUFFICIENT_RESOURCES == rc)
            {
                Printf(Tee::PriHigh, "Out of impParams entries while processing InitLwDisplayInParseData; MAX_IMP_PARAMS needs to be increased\n");
            }
            return rc;
        }

        m_pImpOutParams = pImpParam;
        rc = InitLwDisplayOutParseData(&pImpParam);
        if (rc != RC::OK)
        {
            if (RC::LWRM_INSUFFICIENT_RESOURCES == rc)
            {
                Printf(Tee::PriHigh, "Out of impParams entries while processing InitLwDisplayOutParseData; MAX_IMP_PARAMS needs to be increased\n");
            }
            return rc;
        }

        m_pImpForceClocksParams = pImpParam;
        pImpParam = m_pImpOutParams;
        pImpParam->offset = 0;
        CallwlateDataOffsets(&pImpParam);

        pImpParam = m_pImpForceClocksParams;
        rc = InitForceClocksParseData(&pImpParam);
        if (rc != RC::OK)
        {
            if (RC::LWRM_INSUFFICIENT_RESOURCES == rc)
            {
                Printf(Tee::PriHigh, "Out of impParams entries while processing InitForceClocksParseData; MAX_IMP_PARAMS needs to be increased\n");
            }
            return rc; 
        }
        pImpParam = m_pImpForceClocksParams;
        pImpParam->offset = 0;

        CallwlateDataOffsets(&pImpParam);
    }
    else
    {
        rc = InitEvoInParseData(&pImpParam);
        if (rc != RC::OK)
        {
            if (RC::LWRM_INSUFFICIENT_RESOURCES == rc)
            {
                Printf(Tee::PriHigh, "Out of impParams entries while processing InitEvoInParseData; MAX_IMP_PARAMS needs to be increased\n");
            }
            return rc;
        }
    }

    pImpParam = impParams;
    pImpParam->offset = 0;
    CallwlateDataOffsets(&pImpParam);

    return RC::OK;
}

//! \This function initialises the IMP structures
//! required  for ParseGenConfigFile
//! \sa InitIMPstruct
void ImpTest::InitIMPstruct()
{
    // Initialize Fermi structures.
    memset(m_pFermiImpParams, 0, sizeof(*m_pFermiImpParams));
    m_pFermiImpParams->input.bGetMargin = LW_TRUE;
    m_pFermiFbImpIn = &(m_pFermiImpParams->input.impFbIn);
    m_pFermiIsohubImpIn = &(m_pFermiImpParams->input.impIsohubIn);

    InitializeFermiDispInORParameters();

    // Initialize Volta structures.
    memset(m_pVoltaImpInParams, 0, sizeof(*m_pVoltaImpInParams));
}

// Search all paths in 'searchpath' and try to open the file 'fname'.  Upon
// success, return a newly allocated ifstream.  On failure, return NULL.
ifstream * ImpTest:: alloc_ifstream (vector<string> & searchpath, const string & fname)
{
    ifstream *pOpenfile= NULL;
    for (vector<string>::const_iterator it = searchpath.begin(); it != searchpath.end(); ++it)
    {

        const string & dirname = *it;
        string fullpath = "";

        if (m_ModsDirPath != "")
        {
            fullpath = m_ModsDirPath;
            fullpath += "/";
        }
        if (dirname != "")
        {
            fullpath += dirname;
            fullpath += "/";
        }

        fullpath += fname;

        pOpenfile = new ifstream (fullpath.c_str());

        if (pOpenfile->is_open())
        {
#ifdef LWOS_IS_WINDOWS
            string::size_type delim = fname.find_last_of("/\\");
#else
            string::size_type delim = fname.find_last_of("/");
#endif
            if (delim != string::npos)
            {

                string config_dir = fname.substr (0, delim);
                string new_path = dirname + "/" + config_dir;
                searchpath.push_back (new_path);
            }
            break;
        }
        delete pOpenfile;
        pOpenfile = NULL;
    }
    return pOpenfile;
}

FLOAT64 ImpTest::GetPercentDiff(UINT64 dataValue1, UINT64 dataValue2)
{
    UINT64 diff;
    FLOAT64 percentDiff = 0.0L;

    if (dataValue1 == 0ULL && dataValue2 == 0ULL)
    {
        return 0.0L;
    }
    else if (((dataValue2 == 0ULL) && (dataValue1 != 0ULL)) ||
             ((dataValue1 == 0ULL) && (dataValue2 != 0ULL)))
    {
        return 100.0L;
    }
    else
    {
        if (dataValue1 > dataValue2)
        {
            diff = dataValue1 - dataValue2;
        }
        else
        {
            diff = dataValue2 - dataValue1;
        }
        percentDiff = (((FLOAT64) diff / dataValue2) * 100.0L);
        return percentDiff;
    }
}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//----------------------------------------------------------------------------
RC ImpTest::Cleanup()
{
    LwRmPtr pLwRm;

    if (!bIsEvoObject)
    {
        Printf(Tee::PriHigh,"Freeing non-evo display object\n");
        pLwRm->Free(m_hDisplayHandle);
    }

    m_hDisplayHandle = 0;
    // Free the memory allocated in setup function.
    if (m_pFermiImpParams)
    {
        delete m_pFermiImpParams;
    }
    if (m_pVoltaImpInParams)
    {
        delete m_pVoltaImpInParams;
    }
    if (m_pVoltaImpOutParams)
    {
        delete m_pVoltaImpOutParams;
    }
    if (m_pImpStateLoadForceClocksParams)
    {
        delete m_pImpStateLoadForceClocksParams;
    }
    return RC::OK;
}

//! \brief StripComment function
//!
//! In this function we strip the comments ( i.e #) if it is found in the line
//! passed as input
//!
string ImpTest::StripComment (const string &line)
{
    string::size_type spos = line.find_first_of("#");
    string retval = line.substr (0, spos);
    return retval;
}

//! \brief StripWhiteSpace function
//!
//! In this function we remove all whitespace from a string
//!
string ImpTest::StripWhiteSpace (const string &line)
{
    string retval (line);
    string::size_type spos = retval.find_first_not_of (" \t");
    retval.erase (0, spos);

    string::size_type epos = retval.find_last_not_of (" \t");
    retval.erase (epos+1);

    return retval;
}

//! \brief Tokenizefunction
//!
//! In this function we divide the line in to parts depending on the
//! delimiter value, if we don't passe any delimiter value
//! it takes whitespae as default.
//!
vector<string> ImpTest::Tokenize(const string &str,
                                   const string &delimiters)
{
    vector<string> tokens;
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }

    return tokens;
}


//! \brief Tokenizefunction
//!
//! In this function we divide the line in to parts depending on the
//! delimiter value, if we don't passe any delimiter value
//! it takes whitespae as default and return the token in passed vector.
//!
void ImpTest::TokenizePassByArg(string &str,
                                string &delimiters,
                                vector <string> *tokens)
{
    //vector<string> tokens;
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens->push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

RC ImpTest::ParseInputString
(
    string          paramName
   ,string          paramValue
   ,UINT32          lineNum
   ,IMP_PARAM       *pImpParam
   ,void            *pDataStruct
)
{
    vector<string>  toks;
    UINT32  token = 0;
#if DEBUG_MSGS
    UINT32  matchLevel = 0;
#endif
    UINT32  dataLevel = 0;
    string::size_type   begin = 0;
    string::size_type   end = 0;
    UINT32  index;
    char    *pData[10];
    char    *pField;
    UINT32  dataValue;

    toks = Tokenize(paramName, ".");

#if DEBUG_MSGS
    Printf(Tee::PriHigh, "m_pFermiImpParams=%p\n", m_pFermiImpParams);
#endif

    pData[0] = (char *) pDataStruct;
#if DEBUG_MSGS
    Printf(Tee::PriHigh, "pData[0]=%p\n", pData[0]);
#endif

    while (1)
    {
#if DEBUG_MSGS
        Printf(Tee::PriHigh, "Processing %d:%s  Checking: %d\n", token, toks[token].c_str(), pImpParam->type);
#endif
        switch (pImpParam->type)
        {
            case IMP_PARAM_TYPE_COMMENT:
                break;
            case IMP_PARAM_TYPE_FIELD:
                if (pImpParam->pName == toks[token])
                {
                    dataValue = strtoul(paramValue.c_str(), NULL, 0);
                    pField = pData[dataLevel] + pImpParam->offset;
                    switch (pImpParam->size)
                    {
                    case 4:
                        *(UINT32 *) pField = dataValue;
                        break;
                    case 8:
                        *(UINT64 *) pField = dataValue;
                        break;
                    case 1:
                        *(UINT08 *) pField = dataValue;
                        break;
                    case 2:
                        *(UINT16 *) pField = dataValue;
                        break;
                    default:
                        Printf(Tee::PriHigh
                              ,"Unknown data size (%d) for RM data field %s\n"
                              ,pImpParam->size
                              ,pImpParam->pName);
                        return RC::SOFTWARE_ERROR;
                    }
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"%s: pData[%d] + %d = %p + %d = %p\n"
                          ,pImpParam->pName
                          ,dataLevel
                          ,pImpParam->offset
                          ,pData[dataLevel]
                          ,pImpParam->offset
                          ,pField);
                    Printf(Tee::PriHigh, "%s: data=%d\n"
                          ,pImpParam->pName
                          ,dataValue);
#endif
                    return RC::OK;
                }
                break;
            case IMP_PARAM_TYPE_FIELD_ARRAY:
                begin  = toks[token].find("[", 0);
                if ((begin != string::npos) &&
                    (toks[token].substr(0, begin) == pImpParam->pName))
                {
                    end = toks[token].find("]", begin);
                    if (std::string::npos == end)
                    {
                        Printf(Tee::PriHigh
                              ,"Line %d: Closing bracket not found\n"
                              ,lineNum);
                        return RC::ILWALID_TEST_INPUT;
                    }
                    index = atoi((toks[token].substr(begin + 1, end-begin - 1)).c_str());
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Array index = %d\n", index);
                    matchLevel = dataLevel;
#endif
                    token++;
                    dataValue = strtoul(paramValue.c_str(), NULL, 0);
                    pField = pData[dataLevel] +
                             pImpParam->offset +
                             index * pImpParam->sizeArrayElement;
                    switch (pImpParam->size)
                    {
                    case 4:
                        *(UINT32 *) pField = dataValue;
                        break;
                    case 8:
                        *(UINT64 *) pField = dataValue;
                        break;
                    case 1:
                        *(UINT08 *) pField = dataValue;
                        break;
                    case 2:
                        *(UINT16 *) pField = dataValue;
                        break;
                    default:
                        Printf(Tee::PriHigh
                              ,"Unknown data size (%d) for RM data array field %s\n"
                              ,pImpParam->size
                              ,pImpParam->pName);
                        return RC::SOFTWARE_ERROR;
                    }
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"FIELD_ARRAY %s: pData[%d] = %p + %d + %d * %d = %p\n"
                          ,toks[token].c_str()
                          ,dataLevel
                          ,pData[dataLevel - 1]
                          ,pImpParam->offset
                          ,index
                          ,pImpParam->sizeArrayElement
                          ,pData[dataLevel]);
#endif
                    return RC::OK;
                }
                else
                {
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Skipping array field; adding %d\n"
                          ,pImpParam->size);
                    Printf(Tee::PriHigh, "Old pImpParam = %p\n", pImpParam);
#endif
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "New pImpParam = %p, type = %d\n", pImpParam, pImpParam->type);
#endif
                }
                break;
            case IMP_PARAM_TYPE_ENUM:
                if (pImpParam->pName == toks[token])
                {
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Found enum\n");
#endif
                    // Loop through our list of expected value strings for this enum.
                    for (index = 0; index < pImpParam->numElements; index++)
                    {
                        if (paramValue == pImpParam->pTable[index].pName)
                        {
                            pField = pData[dataLevel] + pImpParam->offset;
                            switch (pImpParam->size)
                            {
                            case 4:
                                *(UINT32 *) pField = pImpParam->pTable[index].enumValue;
                                break;
                            case 8:
                                *(UINT64 *) pField = pImpParam->pTable[index].enumValue;
                                break;
                            case 1:
                                *(UINT08 *) pField = pImpParam->pTable[index].enumValue;
                                break;
                            case 2:
                                *(UINT16 *) pField = pImpParam->pTable[index].enumValue;
                                break;
                            default:
                                Printf(Tee::PriHigh
                                      ,"Unknown data size (%d) for RM data enum %s\n"
                                      ,pImpParam->size
                                      ,pImpParam->pName);
                                return RC::SOFTWARE_ERROR;
                            }
#if DEBUG_MSGS
                            Printf(Tee::PriHigh
                                  ,"%s: pData[%d] + %d = %p + %d = %p\n"
                                  ,pImpParam->pName
                                  ,dataLevel
                                  ,pImpParam->offset
                                  ,pData[dataLevel]
                                  ,pImpParam->offset
                                  ,pField);
                            Printf(Tee::PriHigh
                                  , "%s: data=%d\n"
                                  ,pImpParam->pName
                                  ,pImpParam->pTable[index].enumValue);
#endif
                            return RC::OK;
                        }
                    }
                    Printf(Tee::PriHigh
                          ,"Line %d: Unrecognized enum:  %s\n"
                          ,lineNum
                          ,paramValue.c_str());
                    return RC::ILWALID_TEST_INPUT;
                }
                break;
            case IMP_PARAM_TYPE_STRUCT:
#if DEBUG_MSGS
                Printf(Tee::PriHigh
                      ,"Ref struct = %s, offset = %d\n"
                      ,pImpParam->pName
                      ,pImpParam->offset);
#endif
                if (*pImpParam->pName == 0)
                {
                    dataLevel++;
                    pData[dataLevel] = pData[dataLevel - 1] + pImpParam->offset;
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"STRUCT %s: pData[%d]=%p\n"
                          ,toks[token].c_str()
                          ,dataLevel
                          ,pData[dataLevel]);
#endif
                }
                else if (pImpParam->pName == toks[token])
                {
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Match struct %s\n", pImpParam->pName);
                    matchLevel = dataLevel;
#endif
                    dataLevel++;
                    token++;
                    if (token > toks.size())
                    {
                        Printf(Tee::PriHigh
                              ,"Line %d: Unexpected end of line found\n"
                              ,lineNum);
                        return RC::ILWALID_TEST_INPUT;
                    }
                    pData[dataLevel] = pData[dataLevel - 1] + pImpParam->offset;
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"STRUCT %s: pData[%d]=%p\n"
                          ,toks[token].c_str()
                          ,dataLevel
                          ,pData[dataLevel]);
#endif
                }
                else
                {
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Skipping struct; adding %d\n", pImpParam->size);
                    Printf(Tee::PriHigh, "Old pImpParam = %p\n", pImpParam);
#endif
                    pImpParam += pImpParam->size;
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"New pImpParam = %p, type = %d\n"
                          ,pImpParam
                          ,pImpParam->type);
#endif
                }
                break;
            case IMP_PARAM_TYPE_STRUCT_ARRAY:
                begin  = toks[token].find("[", 0);
                if (begin != string::npos &&
                    toks[token].substr(0, begin) == pImpParam->pName)
                {
                    end = toks[token].find("]", begin);
                    if (std::string::npos == end)
                    {
                        Printf(Tee::PriHigh
                              ,"Line %d: Closing bracket not found\n"
                              ,lineNum);
                        return RC::ILWALID_TEST_INPUT;
                    }
                    index = atoi((toks[token].substr(begin + 1, end-begin - 1)).c_str());
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Array index = %d\n", index);
                    matchLevel = dataLevel;
#endif
                    dataLevel++;
                    token++;
                    if (token > toks.size())
                    {
                        Printf(Tee::PriHigh
                              ,"Line %d: Unexpected end of line found\n"
                              ,lineNum);
                        return RC::ILWALID_TEST_INPUT;
                    }
                    pData[dataLevel] = pData[dataLevel - 1] +
                                       pImpParam->offset +
                                       (index * pImpParam->sizeArrayElement);
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"ARRAY %s: pData[%d] = %p + %d + %d * %d = %p\n"
                          ,toks[token].c_str()
                          ,dataLevel
                          ,pData[dataLevel - 1]
                          ,pImpParam->offset
                          ,index
                          ,pImpParam->sizeArrayElement
                          ,pData[dataLevel]);
#endif
                }
                else
                {
#if DEBUG_MSGS
                    Printf(Tee::PriHigh, "Skipping array struct; adding %d\n",
                           pImpParam->size);
                    Printf(Tee::PriHigh, "Old pImpParam = %p\n", pImpParam);
#endif
                    pImpParam += pImpParam->size;
#if DEBUG_MSGS
                    Printf(Tee::PriHigh
                          ,"New pImpParam = %p, type = %d\n"
                          ,pImpParam
                          ,pImpParam->type);
#endif
                }
                break;
            case IMP_PARAM_TYPE_END_STRUCT:
#if DEBUG_MSGS
                if (0 == dataLevel || matchLevel == dataLevel)
                {
                    Printf(Tee::PriHigh
                          ,"Line %d: Unrecognized structure member: %s\n"
                          ,lineNum
                          ,toks[token].c_str());
                }
#endif
                dataLevel--;
                if (token)
                {
                    token--;
                }
#if DEBUG_MSGS
                Printf(Tee::PriHigh
                      ,"END_STRUCT: pData[%d] = %p\n"
                      ,dataLevel
                      ,pData[dataLevel]);
#endif
                break;
            case IMP_PARAM_TYPE_END:
                Printf(Tee::PriHigh
                      ,"Line %d: Unrecognized data: %s\n"
                      ,lineNum
                      ,toks[token].c_str());
                return RC::ILWALID_TEST_INPUT;
        }
        pImpParam++;
    }
}

///! \brief InitializeFermiDispInORParameters
//!
//! This function Initializes OR parameters to orOwnerNone
//!
void ImpTest::InitializeFermiDispInORParameters()
{
    //initialize DispIn OR parameters to orOwner_none
    //to avoid default owner as HEAD0 for all OR
    for(UINT32 head=0;head<LW5070_CTRL_CMD_MAX_DACS; head++)
    {
        m_pFermiImpParams->input.impDispIn.impDacIn[head].owner = orOwner_None;
    }
    for(UINT32 head=0;head<LW5070_CTRL_CMD_MAX_SORS; head++)
    {
        m_pFermiImpParams->input.impDispIn.impSorIn[head].owner = orOwner_None;
    }
    for(UINT32 head=0;head<LW5070_CTRL_CMD_MAX_PIORS; head++)
    {
        m_pFermiImpParams->input.impDispIn.impPiorIn[head].owner = orOwner_None;
    }
}

UINT64 GetFieldValue
(
    char *pField
   ,UINT08 size
)
{
    UINT64 dataValue;

    switch (size)
    {
    case 4:
        dataValue = *(UINT32 *) pField;
        break;
    case 8:
        dataValue = *(UINT64 *) pField;
        break;
    case 1:
        dataValue = *(UINT08 *) pField;
        break;
    case 2:
        dataValue = *(UINT16 *) pField;
        break;
    default:
        dataValue = 0;
        Printf(Tee::PriHigh, "Data size = %d\n", size);
        MASSERT(!"Unsupported data size");
        break;
    }

    return dataValue;
}

void ImpTest::PrintFieldLine
(
    FILE *ofile
   ,string *pNameStr
   ,void *pData
   ,void *pData2Copy0
   ,void *pData2Copy1
   ,UINT16 offset
   ,UINT08 size
)
{
    UINT64  dataValue;
    UINT64  dataValue2;
    FLOAT64 percentDiff;
    char    *pField;
    char    *pField2;
    const char *pBangs;

    pField = (char *) pData + offset;
    dataValue = GetFieldValue(pField, size);

    if ((pData2Copy0 != NULL) &&
        (*((char *) pData2Copy0 + offset) == *((char *) pData2Copy1 + offset)))
    {
        pField2 = (char *) pData2Copy0 + offset;
        dataValue2 = GetFieldValue(pField2, size);
        if (dataValue != dataValue2)
        {
            percentDiff = GetPercentDiff(dataValue, dataValue2);
            if (m_largestRmArchPercentDiff < percentDiff)
            {
                m_largestRmArchPercentDiff = percentDiff;
            }
            pBangs = (percentDiff >= ERROR_DELTA_AUTO) ? " !" : "";
            fprintf(ofile, "%-40s%12llu%12llu%8d%%%s\n"
                   ,pNameStr->c_str()
                   ,dataValue
                   ,dataValue2
                   ,(UINT32) percentDiff
                   ,pBangs);
        }
        else
        {
            fprintf(ofile, "%-40s%12llu%12llu\n"
                   ,pNameStr->c_str()
                   ,dataValue
                   ,dataValue2);
        }
    }
    else
    {
        fprintf(ofile, "%-40s%12llu\n", pNameStr->c_str(), dataValue);
    }
}

UINT32 ImpTest::PrintImpParam
(
    string namePrefix
   ,void *pData
   ,void *pData2Copy0
   ,void *pData2Copy1
   ,IMP_PARAM *pImpParam
   ,FILE* ofile
   ,bool bSingleLineOut
)
{
    UINT32  paramsProcessed = 0;
    UINT32  subParamsProcessed;
    string  tmpstr;
    char    numberStr[100];
    IMP_PARAM   *pSubParam;
    UINT64  dataValue;
    UINT08   arrayIndex;
    UINT08  arrayCount;
    UINT08  arrayLimit;
    char    *pField;

#if DEBUG_MSGS
    Printf(Tee::PriHigh,
           "PrintImpParam(%s%s, %p, %p)\n",
           namePrefix.c_str(),
           pImpParam->pName,
           pData,
           pImpParam);
    Printf(Tee::PriHigh,
           "  IMP_PARAM(type=%d, pName=%s, size=%d, numElements=%d, offset=%d)\n",
           pImpParam->type,
           pImpParam->pName,
           pImpParam->size,
           pImpParam->numElements,
           pImpParam->offset);
#endif

    switch (pImpParam->type)
    {
    case IMP_PARAM_TYPE_COMMENT:
        if (!bSingleLineOut)
        {
            fprintf(ofile, "\n%s\n", pImpParam->pName);
        }
        break;
    case IMP_PARAM_TYPE_FIELD:
        tmpstr = namePrefix + string(pImpParam->pName);
        PrintFieldLine(ofile
                      ,&tmpstr
                      ,pData
                      ,pData2Copy0
                      ,pData2Copy1
                      ,pImpParam->offset
                      ,pImpParam->size);
        break;
    case IMP_PARAM_TYPE_FIELD_ARRAY:
        arrayLimit = 50;
        if (pImpParam->flags & PARAM_FLAGS_ARRAY_HEAD)
        {
            arrayLimit = m_activeHeads;
        } else if (pImpParam->flags & PARAM_FLAGS_ARRAY_WINDOW)
        {
            arrayLimit = m_activeWindows;
        }
        arrayCount = LW_MIN(pImpParam->numElements, arrayLimit);
        pData = (char *) pData + pImpParam->offset;
        if (pData2Copy0 != NULL)
        {
            pData2Copy0 = (char *) pData2Copy0 + pImpParam->offset;
            pData2Copy1 = (char *) pData2Copy1 + pImpParam->offset;
        }
        for (arrayIndex = 0; arrayIndex < arrayCount; arrayIndex++)
        {
            sprintf(numberStr, "%d", arrayIndex);
            tmpstr = namePrefix + pImpParam->pName + "[" + numberStr + "]";
            PrintFieldLine(ofile
                          ,&tmpstr
                          ,pData
                          ,pData2Copy0
                          ,pData2Copy1
                          ,arrayIndex * pImpParam->sizeArrayElement
                          ,pImpParam->size);
        }
        break;
    case IMP_PARAM_TYPE_ENUM:
        tmpstr = namePrefix + string(pImpParam->pName);
        pField = (char *) pData + pImpParam->offset;
        switch (pImpParam->size)
        {
        case 4:
            dataValue = *(UINT32 *) pField;
            break;
        case 8:
            dataValue = *(UINT64 *) pField;
            break;
        case 1:
            dataValue = *(UINT08 *) pField;
            break;
        case 2:
            dataValue = *(UINT16 *) pField;
            break;
        default:
            dataValue = 0;
            Printf(Tee::PriHigh, "Data size = %d\n", pImpParam->size);
            MASSERT(!"Unsupported data size");
            break;
        }
#if DEBUG_MSGS
        Printf(Tee::PriHigh, "  data = 0x%08llX\n", dataValue);
#endif
        //
        // Often enum lists are numbered sequentially from zero (0, 1, 2,
        // etc.).  Check the array index equal to the enum value first; if it's
        // a match, then we don't have to search through the entire list.
        //
        if ((dataValue < pImpParam->numElements) &&
            (dataValue == pImpParam->pTable[dataValue].enumValue))
        {
            MASSERT(dataValue < 0x100);
            arrayIndex = (UINT08) dataValue;
        }
        else
        {
            for (arrayIndex = 0; arrayIndex < pImpParam->numElements; arrayIndex++)
            {
                if (dataValue == pImpParam->pTable[arrayIndex].enumValue)
                {
                    break;
                }
            }
        }
        fprintf(ofile,
                "%-30s%22s\n",
                tmpstr.c_str(),
                (arrayIndex < pImpParam->numElements) ? pImpParam->pTable[arrayIndex].pName : "UNKNOWN");
        break;
    case IMP_PARAM_TYPE_STRUCT:
        if (*pImpParam->pName == 0)
        {
            tmpstr = namePrefix;
        }
        else
        {
            tmpstr = namePrefix + pImpParam->pName + ".";
        }
        pData = (char *) pData + pImpParam->offset;
        if (pData2Copy0 != NULL)
        {
            pData2Copy0 = (char *) pData2Copy0 + pImpParam->offset;
            pData2Copy1 = (char *) pData2Copy1 + pImpParam->offset;
        }
        pImpParam++;
        while (pImpParam->type != IMP_PARAM_TYPE_END_STRUCT)
        {
            subParamsProcessed = PrintImpParam(tmpstr
                                              ,pData
                                              ,pData2Copy0
                                              ,pData2Copy1
                                              ,pImpParam
                                              ,ofile
                                              ,bSingleLineOut);
            if (!subParamsProcessed)
            {
                return 0;
            }
            paramsProcessed += subParamsProcessed;
            pImpParam += subParamsProcessed;
        }
        // Add one for the IMP_PARAM_TYPE_END_STRUCT parameter.
        paramsProcessed++;
        break;
    case IMP_PARAM_TYPE_STRUCT_ARRAY:
        arrayLimit = 50;
        if (pImpParam->flags & PARAM_FLAGS_ARRAY_HEAD)
        {
            arrayLimit = m_activeHeads;
        } else if (pImpParam->flags & PARAM_FLAGS_ARRAY_WINDOW)
        {
            arrayLimit = m_activeWindows;
        }
        arrayCount = LW_MIN(pImpParam->numElements, arrayLimit);
        pData = (char *) pData + pImpParam->offset;
        if (pData2Copy0 != NULL)
        {
            pData2Copy0 = (char *) pData2Copy0 + pImpParam->offset;
            pData2Copy1 = (char *) pData2Copy1 + pImpParam->offset;
        }
        pSubParam = pImpParam + 1;
        for (arrayIndex = 0; arrayIndex < arrayCount; arrayIndex++)
        {
            sprintf(numberStr, "%d", arrayIndex);
            tmpstr = namePrefix + pImpParam->pName + "[" + numberStr + "].";
            pSubParam = pImpParam + 1;
            while (pSubParam->type != IMP_PARAM_TYPE_END_STRUCT)
            {
                subParamsProcessed =
                    PrintImpParam(tmpstr
                                 ,(char *) pData +
                                  (arrayIndex * pImpParam->sizeArrayElement)
                                 ,(pData2Copy0 == NULL) ? NULL
                                                        : (char *) pData2Copy0 +
                                                          (arrayIndex *
                                                           pImpParam->sizeArrayElement)
                                 ,(pData2Copy1 == NULL) ? NULL
                                                        : (char *) pData2Copy1 +
                                                          (arrayIndex *
                                                           pImpParam->sizeArrayElement)
                                 ,pSubParam
                                 ,ofile
                                 ,bSingleLineOut);
                if (!subParamsProcessed)
                {
                    return 0;
                }
                pSubParam += subParamsProcessed;
            }
        }
        paramsProcessed += (UINT32) (pSubParam - pImpParam);
        break;
    default:
        return 0;
        break;
    }
    // Add one for the initial "type" parameter.
    paramsProcessed++;
#if DEBUG_MSGS
    Printf(Tee::PriHigh, "  %s done, paramsProcessed=%d\n",
           namePrefix.c_str(),
           paramsProcessed);
#endif
    return paramsProcessed;
}


//----------------------------------------------------------------------------
// JS Linkage
//----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ AllocObjectTest
//! object.
//!
//----------------------------------------------------------------------------
JS_CLASS_INHERIT(ImpTest, RmTest,
                 "SW DispImp verifcation Test.");

CLASS_PROP_READWRITE(ImpTest,fbConfigTestNumber,UINT32,
                     "Configurable ConfigFile no for FbImp test: 0=Default.");
CLASS_PROP_READWRITE(ImpTest,infile,string,
                     "Configurable ConfigFile inputfilename");
CLASS_PROP_READWRITE(ImpTest,outfile,string,
                     "Configurable ConfigFile output filename");
CLASS_PROP_READWRITE(ImpTest,workdir,string,
                     "Configurable imp working directory");

