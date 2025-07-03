/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_SorSplit.cpp
//! \brief a sor split test which tests all SOR combination of GPU
//! for all detected displays
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "ctrl/ctrl0073.h"
#include <algorithm>
#include <vector>
using namespace std;

#define FINDMIN(a, b)          ((a) < (b) ? (a): (b))
typedef vector<UINT32> TestCombi;

class SorSplit : public RmTest
{
public:
    SorSplit();
    virtual ~SorSplit();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    enum SORTYPE
    {
        TMDS_A = 0,
        TMDS_B,
        DUAL_TMDS,
        DP_A,
        DP_B,
        LVDS_LWSTOM,
    };

    struct SorSplitDisplayCtx
    {
        DisplayID displayId;
        UINT32 SorNum;
        SORTYPE SorType;
        bool FakeDevice;
        string OverrrideEdidFile;
    };

    static bool CompareFunc(const SorSplitDisplayCtx& it1 , const SorSplitDisplayCtx& it2)
    {
        return (it1.SorNum < it2.SorNum);
    }
    SETGET_PROP(rastersize, string); //Config raster size through cmdline
    Surface2D m_DisplayImage;
    UINT32 m_BaseWidth;
    UINT32 m_BaseHeight;
    RC AttachDisplay(DisplayID displayid);
    RC DetachDisplay(DisplayID displayid);
    bool FindChangeIndex(TestCombi& , TestCombi& , vector<UINT32>& );

private:
    string      m_rastersize;
    Display *m_pDisplay;

};

//! \brief SorSplit constructor
//!
//! Sets the name of the test
//!
//------------------------------------------------------------------------------
SorSplit::SorSplit()
{
    SetName("SorSplit");
}

//! \brief SorSplit destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SorSplit::~SorSplit()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//! The test has to run of GF11X + chips but can run on all elwironments
//------------------------------------------------------------------------------
string SorSplit::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC SorSplit::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//! Lists all SORs and find the combinations of SOR displayids and do modeset on it.
//! Test has to be run with SplitMode and Non Split Mode Vbios to test the modeset.
//------------------------------------------------------------------------------
RC SorSplit::Run()
{
    RC rc;
    m_pDisplay = GetDisplay();

    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    DisplayIDs Detected;
    DisplayIDs Supported;

    bool useSmallRaster = false;

    // get all the Detected displays
    CHECK_RC(m_pDisplay->GetDetected(&Detected));
    CHECK_RC(m_pDisplay->GetConnectors(&Supported));

    vector<SorSplitDisplayCtx>SorDispTestVector;

    DisplayIDs DpdisplayIdsToFake;
    DisplayIDs DpDisplayidsCnctd;
    for(UINT32 index = 0; index < Supported.size(); index++)
    {
        UINT32 orIndex    = 0;
        UINT32 orType     = 0;
        UINT32 orProtocol = 0;
        CHECK_RC(m_pDisplay->GetORProtocol(Supported[index],
                    &orIndex, &orType, &orProtocol));
        if (orType == LW0073_CTRL_SPECIFIC_OR_TYPE_SOR)
        {
            SorSplitDisplayCtx sorDispTestItem;
            sorDispTestItem.displayId = Supported[index];
            sorDispTestItem.SorNum = orIndex;
            switch (orProtocol)
            {
                case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM:
                    sorDispTestItem.SorType = LVDS_LWSTOM;
                    break;
                case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A:
                    sorDispTestItem.SorType = TMDS_A;
                    break;
                case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B:
                    sorDispTestItem.SorType = TMDS_B;
                    break;
                case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS:
                    sorDispTestItem.SorType = DUAL_TMDS;
                    break;
                case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A:
                    sorDispTestItem.SorType = DP_A;
                    break;
                case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B:
                    sorDispTestItem.SorType = DP_B;
                    break;
            }
            sorDispTestItem.OverrrideEdidFile = "edids/edid_DP_HP.txt";
            if (!m_pDisplay->IsDispAvailInList(Supported[index], Detected))
            {
                sorDispTestItem.FakeDevice = true;
                CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, Supported[index], false, sorDispTestItem.OverrrideEdidFile));
            }
            else
            {
                sorDispTestItem.FakeDevice = false;
                if (!(DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, Supported[index]) &&
                    DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, Supported[index]))
                    )
                {
                    CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, Supported[index], true, sorDispTestItem.OverrrideEdidFile));
                }
            }

            SorDispTestVector.push_back(sorDispTestItem);
        }
    }

    sort (SorDispTestVector.begin(), SorDispTestVector.end(), SorSplit::CompareFunc);

    // Do modeset

    for ( UINT32 i = 0; i < SorDispTestVector.size(); i++)
    {

        Printf(Tee::PriHigh, "SorDispTestVector[%d].DisplayId:0x%x , SorDispTestVector[%d].SorNum :%d \n",
            i, (UINT32)SorDispTestVector[i].displayId, i, SorDispTestVector[i].SorNum);

        if( SorDispTestVector[i].SorType == DP_A || SorDispTestVector[i].SorType == DP_B)
            Printf(Tee::PriHigh, " Protocol DP \n" );
    }

    if (m_rastersize.compare("") == 0)
    {
        useSmallRaster = (Platform::GetSimulationMode() != Platform::Hardware);
    }
    else
    {
        useSmallRaster = (m_rastersize.compare("small") == 0);
        if (!useSmallRaster && m_rastersize.compare("large") != 0)
        {
            // Raster size param must be either "small" or "large".
            Printf(Tee::PriHigh, "%s: Bad raster size argument: \"%s\". Treating as \"large\"\n", __FUNCTION__, m_rastersize.c_str());
        }
    }

    if (useSmallRaster)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        m_BaseWidth    = 160;
        m_BaseHeight   = 120;
    }
    else
    {
        m_BaseWidth    = 640;
        m_BaseWidth   = 480;
    }

    // here we have to check the register for the value 3 .. if SOR3 then SorIndexSize is 4.
    //In split mode bios, LW_PDISP_CLK_REM_SOR_CTRL(3)_BACKEND is set to SOR0.
    //In non-split mode, LW_PDISP_CLK_REM_SOR_CTRL(3)_BACKEND is set to SOR3.
    // for now this check is not required, since with RM32 capabilities matrix DisplayIds will get Retuned ..

    UINT32 SorTestVectorSize =  static_cast<UINT32>(SorDispTestVector.size());
    vector<vector<UINT32> >TestMatrix;
    vector<UINT32>SorIndex;

    SorIndex.push_back(0);

    // Now Just List Out Combinations
    for (UINT32 i =0; i < (SorTestVectorSize - 1); i++)
    {
        if (SorDispTestVector[i].SorNum != SorDispTestVector[i+1].SorNum)
            SorIndex.push_back(i+1);
    }

    Printf( Tee::PriHigh, "SOR indexs\n");
    for (UINT32 i =  0; i < SorIndex.size(); i++)
        Printf( Tee::PriHigh, " %d ", SorIndex[i]);
    Printf( Tee::PriHigh, "\n");
    //
    // Now generate Combinations I will optimize the combinations later .. By Finding out Duplicates
    // TBD : chnage the for loop implemenation also
    //
        UINT32 Limit =0;
        for (UINT32 i = 0; i < SorIndex[1]; i ++)
        {
            for (UINT32 j = SorIndex[1]; j < SorIndex[2]; j++)
            {
                UINT32 k = SorIndex[2];
                do
                {
                    if (SorIndex.size() == 3)
                    {
                        TestCombi item;
                        item.push_back(i);
                        item.push_back(j);
                        item.push_back(k);
                        TestMatrix.push_back(item);
                        Limit = SorTestVectorSize;
                    }
                    else if (SorIndex.size() == 4)
                    {
                        Limit = SorIndex[3];
                        for (UINT32 l = SorIndex[3]; l < SorTestVectorSize ; l++)
                        {
                            TestCombi item;
                            item.push_back(i);
                            item.push_back(j);
                            item.push_back(k);
                            item.push_back(l);
                            TestMatrix.push_back(item);
                        }
                    }
                    k ++;
                } while (k < Limit);
            }
        }

        //
        // Print the Test combinations here ..
        //
        vector< vector<UINT32> >::iterator iter_outer;
        vector<UINT32>::iterator iter_inner;

        for (UINT32 i = 0; i < SorIndex.size(); i++)
            Printf(Tee::PriHigh, "DisplayID       SOR     ");

        Printf(Tee::PriHigh, "\n\n");

        for (iter_outer=TestMatrix.begin(); iter_outer!=TestMatrix.end(); iter_outer++)
        {
            for(iter_inner=(*iter_outer).begin(); iter_inner!=(*iter_outer).end(); iter_inner++)
            {
                UINT32 testIndex = *iter_inner;
                DisplayID displayid = SorDispTestVector[testIndex].displayId;
                Printf(Tee::PriHigh, "0x%x       %d     ", (UINT32)displayid, SorDispTestVector[testIndex].SorNum);
            }
            Printf(Tee::PriHigh,"\n");
        }

     // test is for kepler only so considering 4 heads ..
    //  now here the actual logic begins
    // take the iterator here and get the DisplayID for that testvector its and index right then Do Modeset for tat combination
    // after done with modeset just discconet all and proceed with next modeset.

        vector< vector<UINT32> >::iterator iter_ii;
        vector<UINT32>::iterator iter_jj;

        CHECK_RC(m_pDisplay->SetupChannelImage(SorDispTestVector[0].displayId,
            FINDMIN(m_BaseWidth, 640),
            FINDMIN(m_BaseHeight, 480),
            32,
            Display::CORE_CHANNEL_ID,
            &m_DisplayImage,
            "dispinfradata/images/baseimage640x480.PNG"));

        vector<UINT32> changeIndexes;
        for (iter_ii  = TestMatrix.begin(); iter_ii != TestMatrix.end() ; iter_ii++)
        {
            if (iter_ii != TestMatrix.begin())
            {
                TestCombi oldItem = *(iter_ii -1);
                TestCombi newItem = *(iter_ii);
                changeIndexes.clear();
                if (FindChangeIndex(oldItem , newItem, changeIndexes))
                {
                    for (UINT32 i = 0; i < changeIndexes.size(); i++)
                    {
                        rc  = DetachDisplay(SorDispTestVector[oldItem[changeIndexes[i]]].displayId);
                        if ( rc != OK)
                        {
                            Printf(Tee::PriHigh, " DetachDisplay ERROR \n");
                            return rc;
                        }
                    }
                    Printf (Tee::PriHigh, "\n \n #### Display Combination #### \n");
                    for (UINT32 i=0; i < newItem.size(); i++)
                    {
                        Printf(Tee::PriHigh, "DisplayID : 0x%x    ",(UINT32)SorDispTestVector[newItem[i]].displayId);
                    }
                    Printf (Tee::PriHigh, "\n \n");
                    for (UINT32 i=0; i < changeIndexes.size(); i++)
                    {
                        rc  = AttachDisplay(SorDispTestVector[newItem[changeIndexes[i]]].displayId);
                        if ( rc != OK)
                        {
                            Printf(Tee::PriHigh, " AttachDisplay ERROR \n");
                            return rc;
                        }
                    }
                }
                else
                {
                    Printf(Tee::PriHigh, " ERROR Combination is wrong \n");
                    rc = RC::SOFTWARE_ERROR;
                    break;

                }
            }
            else
            {
                for (iter_jj=(*iter_ii).begin(); iter_jj!=(*iter_ii).end(); iter_jj++)
                {
                    rc =  AttachDisplay(SorDispTestVector[*iter_jj].displayId);
                    if ( rc != OK)
                    {
                        Printf(Tee::PriHigh, " AttachDisplay ERROR");
                        return rc;
                    }
                }
            }

        }

    if (rc !=OK)
    {
        Printf(Tee::PriHigh , " Test Failed rc = %d " , (UINT32) rc);
    }

   return rc;
}

//!
//! @brief Cleanup(): does nothing for this simple test.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK
//-----------------------------------------------------------------------------
RC SorSplit::Cleanup()
{
    return OK;
}

bool SorSplit::FindChangeIndex(TestCombi& oldItem, TestCombi& newItem, vector<UINT32>& chngeIndexes)
{
    UINT32 j = 0;
    for (UINT32 i =0; i < oldItem.size(); i ++)
    {
        if(oldItem[i] != newItem[i])
        {
            chngeIndexes.push_back(i);
            j++;
        }
    }

    if ( j == 0)
    {
        Printf(Tee::PriHigh, "FindChangeIndex Combination invalid Error \n");
    }

    return true;
}

RC SorSplit::AttachDisplay(DisplayID displayid)
{
    RC rc;

    CHECK_RC(m_pDisplay->SetupChannelImage(displayid,
        Display::CORE_CHANNEL_ID,
        &m_DisplayImage));

    Printf(Tee::PriHigh, "Doing Modeset on DisplayID 0x%x",(UINT32)displayid);

    // do modeset on the current displayId
    rc = m_pDisplay->SetMode(displayid,
        FINDMIN(m_BaseWidth, 640),
        FINDMIN(m_BaseHeight, 480),
        32,
        60);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "This Combination is not possible \n");
    }
    return rc;
}

RC SorSplit::DetachDisplay(DisplayID displayid)
{
    RC rc;
    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, displayid)));
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SorSplit, RmTest, "simple cycle resolution test \n");
CLASS_PROP_READWRITE(SorSplit, rastersize, string,
                     "Desired raster size (small/large)");

