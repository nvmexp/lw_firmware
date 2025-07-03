#include "fs_chip.h"

namespace fslib {

ChipConfig::ChipConfig(Chip whichChip, FSmode whichMode)
    : chip(whichChip), mode(whichMode) {
    switch (whichChip) {
        case Chip::GA101:
            // GA101 is a special case
            // We reuse the GA100 rules for FS with some additional checks
            // If GA100 rules gets updated, GA101 rules should also get updated
            allowedGpcMask = 0x3C;
            allowedFbpMask = 0xAFA;
            bGa100AsGa101 = true;
        // Note: the break is intentionally skipped to inherit the GA100 rules
        #ifndef _MSVC_LANG
            [[fallthrough]];
        #endif
        case Chip::GA100:
            // If GA100 rules gets updated, please update GA101 rules also
            numGpcs = 8;
            numTpcPerGpc = 8;
            numPesPerGpc = 3;

            pesToTpcMasks[0] = 0x7;   // PES 0 : TPC 0-2
            pesToTpcMasks[1] = 0x38;  // PES 1 : TPC 3-5
            pesToTpcMasks[2] = 0xC0;  // PES 2 : TPC 6-7

            numFbps = 12;
            numFbpaPerFbp = 2;
            numFbioPerFbp = 2;
            numLtcPerFbp = 2;
            numHbmSites = 6;
            numFbpPerHbmSite = numFbps / numHbmSites;
            numL2NoCs = 6;
            dramType = DramType::Hbm;
            bPesWithTpcFs = true;
            bHbmSiteFs = true;
            //FIXME Bring back the following if? Lwrrently, in fs_chip, we take of it
            //            if (!bGa100AsGa101)       //Keep L2NoCFS only for
            //            GA100, not GA101
            //                bL2NoCFs = true;

            if (whichMode == FSmode::PRODUCT) {  // Products need full Hbm BW
                bFullHbmSiteFS = true;
                bL2NoCFs = true;
            }
            if (whichMode == FSmode::FUNC_VALID) {  // Special case
                bImperfectFbpFS = true;
                bL2NoCFs = true;
                //                bL2SliceFs = true;
            }
            // GA100 list of non-AMAP supported configs, Bug 2438205
            amapDisabledLtcList = {9, 11, 13, 15, 17, 18, 19, 21, 22, 23};
            
            minReqdHbmSitesForProduct = 4;
            // Initialize stitchfbpMap according to GA100 L2NoC pairing
            // https://confluence.lwpu.com/display/AMPEREFS/5.HBM%28FF%29+Floorsweeping

            for (unsigned int l2NoC = 0; l2NoC < numL2NoCs; l2NoC++) {
                std::vector<unsigned int> fbpsPerL2NoC;
                l2NocFbpMap.push_back(fbpsPerL2NoC);
            }
            l2NocFbpMap[0].push_back(1);
            l2NocFbpMap[0].push_back(4);
            l2NocFbpMap[1].push_back(0);
            l2NocFbpMap[1].push_back(3);
            l2NocFbpMap[2].push_back(2);
            l2NocFbpMap[2].push_back(5);
            l2NocFbpMap[3].push_back(6);
            l2NocFbpMap[3].push_back(11);
            l2NocFbpMap[4].push_back(7);
            l2NocFbpMap[4].push_back(8);
            l2NocFbpMap[5].push_back(9);
            l2NocFbpMap[5].push_back(10);

            uGpu0FbpDisableMask = 0x505;
            uGpu1FbpDisableMask = 0x2A8;

            // Initialize hbmfbpMap according to GA100 HBM-FBP connectivity
            for (unsigned int hbm = 0; hbm < numHbmSites; hbm++) {
                std::vector<unsigned int> fbpsPerHbmSite;
                hbmSiteFbpMap.push_back(fbpsPerHbmSite);
            }
            hbmSiteFbpMap[0].push_back(0);
            hbmSiteFbpMap[0].push_back(2);
            hbmSiteFbpMap[1].push_back(1);
            hbmSiteFbpMap[1].push_back(4);
            hbmSiteFbpMap[2].push_back(3);
            hbmSiteFbpMap[2].push_back(5);
            hbmSiteFbpMap[3].push_back(7);
            hbmSiteFbpMap[3].push_back(9);
            hbmSiteFbpMap[4].push_back(6);
            hbmSiteFbpMap[4].push_back(11);
            hbmSiteFbpMap[5].push_back(8);
            hbmSiteFbpMap[5].push_back(10);


            break;

        case Chip::GA102:
            numGpcs = 7;
            numTpcPerGpc = 6;
            numPesPerGpc = 3;
            numRopPerGpc = 2;

            numFbps = 6;
            numFbpaPerFbp = 0;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;
            numL2SlicePerFbp = 8;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x30;  // PES2 : TPC 4-5
            numLtcPerFbpa = numLtcPerFbp / numFbioPerFbp;   // Ga10x has no fbpamask, use fbiomask instead

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bL2SliceFs = true;
            bHalfFbpaFs = true;

            bRopInGpcFs = true;
            // allowed l2slice mask patterns for product configs
            // Only allow 6 such patterns + all enabled
            numL2SlicePatternAllowed = 7;
            for (unsigned int count = 0; count < numL2SlicePatternAllowed; count++) {
                std::vector<unsigned int> l2sMaskPerFbp;
                allowedL2SlicePerFbpMasks.push_back(l2sMaskPerFbp);
            }
            allowedL2SlicePerFbpMasks[0] = {0x81, 0x42, 0x24, 0x18, 0x81, 0x42};
            allowedL2SlicePerFbpMasks[1] = {0x42, 0x24, 0x18, 0x81, 0x42, 0x24};
            allowedL2SlicePerFbpMasks[2] = {0x24, 0x18, 0x81, 0x42, 0x24, 0x18};
            allowedL2SlicePerFbpMasks[3] = {0x18, 0x81, 0x42, 0x24, 0x18, 0x81};
            allowedL2SlicePerFbpMasks[4] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3};
            allowedL2SlicePerFbpMasks[5] = {0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C};
            allowedL2SlicePerFbpMasks[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

            nextAllowedL2SlicePerFbpMasks[0x00] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x0f] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0xf0] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x81] = {0x42};
            nextAllowedL2SlicePerFbpMasks[0x42] = {0x24};
            nextAllowedL2SlicePerFbpMasks[0x24] = {0x18};
            nextAllowedL2SlicePerFbpMasks[0x18] = {0x81};
            nextAllowedL2SlicePerFbpMasks[0xC3] = {0xC3};
            nextAllowedL2SlicePerFbpMasks[0x3C] = {0x3C};

            break;

        case Chip::GA103:
            numGpcs = 6;
            numTpcPerGpc = 5;
            numPesPerGpc = 3;
            numRopPerGpc = 2;

            numFbps = 5;
            numFbpaPerFbp = 0;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;
            numL2SlicePerFbp = 8;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x10;  // PES2 : TPC 4
            numLtcPerFbpa = numLtcPerFbp / numFbioPerFbp;   // Ga10x has no fbpamask, use fbiomask instead

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bL2SliceFs = true;
            bHalfFbpaFs = true;

            bRopInGpcFs = true;
            // allowed l2slice mask patterns for product configs
            // Only allow 6 such patterns + all enabled
            numL2SlicePatternAllowed = 7;
            for (unsigned int count = 0; count < numL2SlicePatternAllowed; count++) {
                std::vector<unsigned int> l2sMaskPerFbp;
                allowedL2SlicePerFbpMasks.push_back(l2sMaskPerFbp);
            }
            allowedL2SlicePerFbpMasks[0] = {0x81, 0x42, 0x24, 0x18, 0x81};
            allowedL2SlicePerFbpMasks[1] = {0x42, 0x24, 0x18, 0x81, 0x42};
            allowedL2SlicePerFbpMasks[2] = {0x24, 0x18, 0x81, 0x42, 0x24};
            allowedL2SlicePerFbpMasks[3] = {0x18, 0x81, 0x42, 0x24, 0x18};
            allowedL2SlicePerFbpMasks[4] = {0xC3, 0xC3, 0xC3, 0xC3, 0xC3};
            allowedL2SlicePerFbpMasks[5] = {0x3C, 0x3C, 0x3C, 0x3C, 0x3C};
            allowedL2SlicePerFbpMasks[6] = {0x0, 0x0, 0x0, 0x0, 0x0};

            nextAllowedL2SlicePerFbpMasks[0x00] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x0f] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0xf0] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x81] = {0x42};
            nextAllowedL2SlicePerFbpMasks[0x42] = {0x24};
            nextAllowedL2SlicePerFbpMasks[0x24] = {0x18};
            nextAllowedL2SlicePerFbpMasks[0x18] = {0x81};
            nextAllowedL2SlicePerFbpMasks[0xC3] = {0xC3};
            nextAllowedL2SlicePerFbpMasks[0x3C] = {0x3C};

            break;

        case Chip::GA104:
            numGpcs = 6;
            numTpcPerGpc = 4;
            numPesPerGpc = 2;
            numRopPerGpc = 2;

            numFbps = 4;
            numFbpaPerFbp = 0;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;
            numL2SlicePerFbp = 8;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            numLtcPerFbpa = numLtcPerFbp / numFbioPerFbp;   // Ga10x has no fbpamask, use fbiomask instead

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bL2SliceFs = true;
            bHalfFbpaFs = true;

            bRopInGpcFs = true;
            // allowed l2slice mask patterns for product configs
            // Only allow 6 such patterns + all enabled
            numL2SlicePatternAllowed = 7;
            for (unsigned int count = 0; count < numL2SlicePatternAllowed; count++) {
                std::vector<unsigned int> l2sMaskPerFbp;
                allowedL2SlicePerFbpMasks.push_back(l2sMaskPerFbp);
            }
            allowedL2SlicePerFbpMasks[0] = {0x81, 0x42, 0x24, 0x18};
            allowedL2SlicePerFbpMasks[1] = {0x42, 0x24, 0x18, 0x81};
            allowedL2SlicePerFbpMasks[2] = {0x24, 0x18, 0x81, 0x42};
            allowedL2SlicePerFbpMasks[3] = {0x18, 0x81, 0x42, 0x24};
            allowedL2SlicePerFbpMasks[4] = {0xC3, 0xC3, 0xC3, 0xC3};
            allowedL2SlicePerFbpMasks[5] = {0x3C, 0x3C, 0x3C, 0x3C};
            allowedL2SlicePerFbpMasks[6] = {0x0, 0x0, 0x0, 0x0};

            nextAllowedL2SlicePerFbpMasks[0x00] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x0f] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0xf0] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x81] = {0x42};
            nextAllowedL2SlicePerFbpMasks[0x42] = {0x24};
            nextAllowedL2SlicePerFbpMasks[0x24] = {0x18};
            nextAllowedL2SlicePerFbpMasks[0x18] = {0x81};
            nextAllowedL2SlicePerFbpMasks[0xC3] = {0xC3};
            nextAllowedL2SlicePerFbpMasks[0x3C] = {0x3C};

            break;

        case Chip::GA106:
            numGpcs = 3;
            numTpcPerGpc = 5;
            numPesPerGpc = 3;
            numRopPerGpc = 2;

            numFbps = 3;
            numFbpaPerFbp = 0;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;
            numL2SlicePerFbp = 8;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x10;  // PES2 : TPC 4
            numLtcPerFbpa = numLtcPerFbp / numFbioPerFbp;   // Ga10x has no fbpamask, use fbiomask instead

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bL2SliceFs = true;
            bHalfFbpaFs = true;

            bRopInGpcFs = true;
            // allowed l2slice mask patterns for product configs
            // Only allow 6 such patterns + all enabled
            numL2SlicePatternAllowed = 7;
            for (unsigned int count = 0; count < numL2SlicePatternAllowed; count++) {
                std::vector<unsigned int> l2sMaskPerFbp;
                allowedL2SlicePerFbpMasks.push_back(l2sMaskPerFbp);
            }
            allowedL2SlicePerFbpMasks[0] = {0x81, 0x42, 0x24};
            allowedL2SlicePerFbpMasks[1] = {0x42, 0x24, 0x18};
            allowedL2SlicePerFbpMasks[2] = {0x24, 0x18, 0x81};
            allowedL2SlicePerFbpMasks[3] = {0x18, 0x81, 0x42};
            allowedL2SlicePerFbpMasks[4] = {0xC3, 0xC3, 0xC3};
            allowedL2SlicePerFbpMasks[5] = {0x3C, 0x3C, 0x3C};
            allowedL2SlicePerFbpMasks[6] = {0x0, 0x0, 0x0};

            nextAllowedL2SlicePerFbpMasks[0x00] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x0f] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0xf0] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x81] = {0x42};
            nextAllowedL2SlicePerFbpMasks[0x42] = {0x24};
            nextAllowedL2SlicePerFbpMasks[0x24] = {0x18};
            nextAllowedL2SlicePerFbpMasks[0x18] = {0x81};
            nextAllowedL2SlicePerFbpMasks[0xC3] = {0xC3};
            nextAllowedL2SlicePerFbpMasks[0x3C] = {0x3C};


            break;

        case Chip::GA107:
            numGpcs = 2;
            numTpcPerGpc = 5;
            numPesPerGpc = 3;
            numRopPerGpc = 2;

            numFbps = 2;
            numFbpaPerFbp = 0;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;
            numL2SlicePerFbp = 8;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x10;  // PES2 : TPC 4
            numLtcPerFbpa = numLtcPerFbp / numFbioPerFbp;   // Ga10x has no fbpamask, use fbiomask instead

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bL2SliceFs = true;
            bHalfFbpaFs = true;

            bRopInGpcFs = true;
            // allowed l2slice mask patterns for product configs
            // Only allow 6 such patterns + all enabled
            numL2SlicePatternAllowed = 7;
            for (unsigned int count = 0; count < numL2SlicePatternAllowed; count++) {
                std::vector<unsigned int> l2sMaskPerFbp;
                allowedL2SlicePerFbpMasks.push_back(l2sMaskPerFbp);
            }
            allowedL2SlicePerFbpMasks[0] = {0x81, 0x42};
            allowedL2SlicePerFbpMasks[1] = {0x42, 0x24};
            allowedL2SlicePerFbpMasks[2] = {0x24, 0x18};
            allowedL2SlicePerFbpMasks[3] = {0x18, 0x81};
            allowedL2SlicePerFbpMasks[4] = {0xC3, 0xC3};
            allowedL2SlicePerFbpMasks[5] = {0x3C, 0x3C};
            allowedL2SlicePerFbpMasks[6] = {0x0, 0x0};

            nextAllowedL2SlicePerFbpMasks[0x00] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x0f] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0xf0] = {0x0,0x0f,0xf0};
            nextAllowedL2SlicePerFbpMasks[0x81] = {0x42};
            nextAllowedL2SlicePerFbpMasks[0x42] = {0x24};
            nextAllowedL2SlicePerFbpMasks[0x24] = {0x18};
            nextAllowedL2SlicePerFbpMasks[0x18] = {0x81};
            nextAllowedL2SlicePerFbpMasks[0xC3] = {0xC3};
            nextAllowedL2SlicePerFbpMasks[0x3C] = {0x3C};


            break;

        case Chip::GA10B:
            numGpcs = 2;
            numTpcPerGpc = 4;
            numPesPerGpc = 2;
            numRopPerGpc = 2;

            numFbps = 2;
            numFbpaPerFbp = 0;
            numFbioPerFbp = 0;
            numLtcPerFbp = 1;
            numL2SlicePerFbp = 4;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            numLtcPerFbpa = 0;   // Ga10b has no fbpa/fbio

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bL2SliceFs = false;
            bHalfFbpaFs = false;

            bRopInGpcFs = true;

            break;
            
        case Chip::GH100:
            // If GH100 rules gets updated, please update GH101 rules also
            numGpcs = 8;
            numTpcPerGpc = 9;
            numPesPerGpc = 1;
            numTpcPerCpc = 3;
            numCpcPerGpc = numTpcPerGpc / numTpcPerCpc;
            pesToTpcMasks[0] = 0x7;   // PES0 : TPC 0-2
            bCpcInGpcFs=true;
            bPesInOnlyGpc0Fs=true;
            // PES present only on Graphics GPC0, so added another mode
            // bPesWithTpcFs = false;
 
            numFbps = 12;
            // FBPA mask is redundant, because it is same as FBIO mask
            numFbpaPerFbp = 0;
            numFbioPerFbp = 2;
            numLtcPerFbp = 2;
            numHbmSites = 6;
            numFbpPerHbmSite = numFbps / numHbmSites;
            numL2SlicePerFbp = 8;
            bHopperSliceFs = true;
            
            numL2NoCs = 6;
            dramType = DramType::Hbm;
            bHbmSiteFs = true;
            if (whichMode == FSmode::PRODUCT) {  // Products need full Hbm BW
            //    FIXME Check with Kun
            //    bFullHbmSiteFS = true;
                bL2NoCFs = true;
            }
            if (whichMode == FSmode::FUNC_VALID) {  // Special case
                bImperfectFbpFS = true;
                bL2NoCFs = true;
            }
            // GH100 list of non-AMAP supported configs, Bug 2715213 #15, Bug 3050516, #65
            amapDisabledLtcList = {13, 15, 17, 19, 21, 23};
            
            //minReqdHbmSitesForProduct = 4;
            for (unsigned int l2NoC = 0; l2NoC < numL2NoCs; l2NoC++) {
                std::vector<unsigned int> fbpsPerL2NoC;
                l2NocFbpMap.push_back(fbpsPerL2NoC);
            }
            l2NocFbpMap[0] = {1,4};
            l2NocFbpMap[1] = {0,3};
            l2NocFbpMap[2] = {2,5};
            l2NocFbpMap[3] = {6,11};
            l2NocFbpMap[4] = {7,8};
            l2NocFbpMap[5] = {9,10};

            uGpu0FbpDisableMask = 0x505;
            uGpu1FbpDisableMask = 0x2A8;

            // Initialize hbmfbpMap according to GA100 HBM-FBP connectivity
            for (unsigned int hbm = 0; hbm < numHbmSites; hbm++) {
                std::vector<unsigned int> fbpsPerHbmSite;
                hbmSiteFbpMap.push_back(fbpsPerHbmSite);
            }
            hbmSiteFbpMap[0] = {0,2};
            hbmSiteFbpMap[1] = {1,4};
            hbmSiteFbpMap[2] = {3,5};
            hbmSiteFbpMap[3] = {7,9};
            hbmSiteFbpMap[4] = {6,11};
            hbmSiteFbpMap[5] = {8,10};

            bAllow1uGpu = false;
            
            break;

        case Chip::GH202:
            // not supported yet
            break;

        case Chip::GB100:
            // Placeholder using GH100 values
            numGpcs = 8;
            numTpcPerGpc = 9;
            numPesPerGpc = 1;
            numTpcPerCpc = 3;
            numCpcPerGpc = numTpcPerGpc / numTpcPerCpc;
            pesToTpcMasks[0] = 0x7;   // PES0 : TPC 0-2
            bCpcInGpcFs=true;
            bPesInOnlyGpc0Fs=false;
 
            numFbps = 12;
            // FBPA mask is redundant, because it is same as FBIO mask
            numFbpaPerFbp = 0;
            numFbioPerFbp = 2;
            numLtcPerFbp = 2;
            numHbmSites = 6;
            numFbpPerHbmSite = numFbps / numHbmSites;
            numL2SlicePerFbp = 8;
            bHopperSliceFs = true;
            
            numL2NoCs = 6;
            dramType = DramType::Hbm;
            bHbmSiteFs = true;
            if (whichMode == FSmode::PRODUCT) {  // Products need full Hbm BW
            //    FIXME Check with Kun
            //    bFullHbmSiteFS = true;
                bL2NoCFs = true;
            }
            if (whichMode == FSmode::FUNC_VALID) {  // Special case
                bImperfectFbpFS = true;
                bL2NoCFs = true;
            }
            // GH100 list of non-AMAP supported configs, Bug 2715213 #15, Bug 3050516, #65
            amapDisabledLtcList = {13, 15, 17, 19, 21, 23};
            
            //minReqdHbmSitesForProduct = 4;
            for (unsigned int l2NoC = 0; l2NoC < numL2NoCs; l2NoC++) {
                std::vector<unsigned int> fbpsPerL2NoC;
                l2NocFbpMap.push_back(fbpsPerL2NoC);
            }
            l2NocFbpMap[0] = {1,4};
            l2NocFbpMap[1] = {0,3};
            l2NocFbpMap[2] = {2,5};
            l2NocFbpMap[3] = {6,11};
            l2NocFbpMap[4] = {7,8};
            l2NocFbpMap[5] = {9,10};

            uGpu0FbpDisableMask = 0x505;
            uGpu1FbpDisableMask = 0x2A8;

            // Initialize hbmfbpMap according to GA100 HBM-FBP connectivity
            for (unsigned int hbm = 0; hbm < numHbmSites; hbm++) {
                std::vector<unsigned int> fbpsPerHbmSite;
                hbmSiteFbpMap.push_back(fbpsPerHbmSite);
            }
            hbmSiteFbpMap[0] = {0,2};
            hbmSiteFbpMap[1] = {1,4};
            hbmSiteFbpMap[2] = {3,5};
            hbmSiteFbpMap[3] = {7,9};
            hbmSiteFbpMap[4] = {6,11};
            hbmSiteFbpMap[5] = {8,10};
            
            break;

        case Chip::G000:
            break;

        case Chip::GV100:
            numGpcs = 6;
            numTpcPerGpc = 7;
            numPesPerGpc = 3;

            numFbps = 8;
            numFbpaPerFbp = 2;
            numFbioPerFbp = 2;
            numLtcPerFbp = 2;
            numHbmSites = 4;

            pesToTpcMasks[0] = 0x7;   // PES 0 : TPC 0-2
            pesToTpcMasks[1] = 0x18;  // PES 1 : TPC 3-4
            pesToTpcMasks[2] = 0x60;  // PES 2 : TPC 5-6
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;
            numFbpPerHbmSite = numFbps / numHbmSites;

            dramType = DramType::Hbm;
            bHbmSiteFs = true;
            bPesWithTpcFs = true;
                
            if (whichMode == FSmode::PRODUCT) {
                bFullHbmSiteFS = true;
            }

            for (unsigned int hbm = 0; hbm < numHbmSites; hbm++) {
                std::vector<unsigned int> fbpsPerHbmSite;
                fbpsPerHbmSite.push_back(2 * hbm);
                fbpsPerHbmSite.push_back(2 * hbm + 1);
                hbmSiteFbpMap.push_back(fbpsPerHbmSite);
            }

            bUseuGpu = false;

            break;

        case Chip::GP102:
            numGpcs = 6;
            numTpcPerGpc = 5;
            numPesPerGpc = 3;

            numFbps = 6;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x10;  // PES2 : TPC 4
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::GP104:
            numGpcs = 4;
            numTpcPerGpc = 5;
            numPesPerGpc = 3;

            numFbps = 4;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x10;  // PES2 : TPC 4
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::GP106:
            numGpcs = 2;
            numTpcPerGpc = 5;
            numPesPerGpc = 3;

            numFbps = 3;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x10;  // PES2 : TPC 4
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::GP107:
            numGpcs = 2;
            numTpcPerGpc = 3;
            numPesPerGpc = 1;

            numFbps = 2;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x7;  // PES0 : TPC 0-2
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bBalancedLtcFs = true;

            bUseuGpu = false;
            break;

        case Chip::GP108:
            numGpcs = 1;
            numTpcPerGpc = 3;
            numPesPerGpc = 1;

            numFbps = 1;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x7;  // PES0 : TPC 0-2
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            // bBalancedLtcFs = true;

            bUseuGpu = false;
            break;

        case Chip::TU102:
            numGpcs = 6;
            numTpcPerGpc = 6;
            numPesPerGpc = 3;

            numFbps = 6;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x30;  // PES2 : TPC 4-5
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::TU104:
            numGpcs = 6;
            numTpcPerGpc = 4;
            numPesPerGpc = 2;

            numFbps = 4;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;  // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;  // PES1 : TPC 2-3
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::TU106:
            numGpcs = 3;
            numTpcPerGpc = 6;
            numPesPerGpc = 3;

            numFbps = 4;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;   // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;   // PES1 : TPC 2-3
            pesToTpcMasks[2] = 0x30;  // PES2 : TPC 4-5
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::TU116:
            numGpcs = 3;
            numTpcPerGpc = 4;
            numPesPerGpc = 2;

            numFbps = 3;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;  // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;  // PES1 : TPC 2-3
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            bHalfFbpaFs = true;

            bUseuGpu = false;
            break;

        case Chip::TU117:
            numGpcs = 2;
            numTpcPerGpc = 4;
            numPesPerGpc = 2;

            numFbps = 2;
            numFbpaPerFbp = 1;
            numFbioPerFbp = 1;
            numLtcPerFbp = 2;

            pesToTpcMasks[0] = 0x3;  // PES0 : TPC 0-1
            pesToTpcMasks[1] = 0xC;  // PES1 : TPC 2-3
            numLtcPerFbpa = numLtcPerFbp / numFbpaPerFbp;

            dramType = DramType::Gddr;
            bPesWithTpcFs = true;
            // bHalfFbpaFs = true;
            bBalancedLtcFs = true;

            bUseuGpu = false;
            break;
        default:
            break;  

    }  // switch chip
}  // ChipConfig

}  // namespace fslib
