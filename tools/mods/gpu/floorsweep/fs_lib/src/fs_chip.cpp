#include "fs_chip.h"
#include <stdio.h>
#include <cassert>
#include <iostream>
#include <string>
#include "fs_utils.h"
namespace fslib {

FsChip::FsChip(Chip chip, FSmode mode) : conf(ChipConfig(chip, mode)) {
}

// Check for Non-existent GPCs
bool FsChip::CheckBounds(const GpcMasks& gpcSettings) {
    // Only existing GPCs can be disabled
    if (gpcSettings.gpcMask >= (1ull << conf.numGpcs)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GPC mask: %d. Disabling non-existent GPC(s)\n",
               gpcSettings.gpcMask);
        conf.errmsg = conf.buff;
        return false;
    }

    // At least one GPC must be present in the chip
    if (gpcSettings.gpcMask == NBitsSet(conf.numGpcs)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GPC mask: %d. At least one GPC must be present.\n",
               gpcSettings.gpcMask);
        conf.errmsg = conf.buff;
        return false;
    }

    // Non-existent elements in GPCs
    for (unsigned int gpc = 0; gpc < conf.numGpcs; gpc++) {
        if ((gpcSettings.pesPerGpcMasks[gpc] > NBitsSet(conf.numPesPerGpc)) && conf.bPesWithTpcFs) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent PES in GPC%d; can only have %d per "
                   "GPC\n",
                   gpc, conf.numPesPerGpc);
            conf.errmsg = conf.buff;
            return false;
        }
        if (gpcSettings.tpcPerGpcMasks[gpc] > NBitsSet(conf.numTpcPerGpc)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent TPC in GPC%d; can only have %d per "
                   "GPC\n",
                   gpc, conf.numTpcPerGpc);
            conf.errmsg = conf.buff;
            return false;
        }
        if ((gpcSettings.cpcPerGpcMasks[gpc] > NBitsSet(conf.numCpcPerGpc)) && conf.bCpcInGpcFs) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent CPC in GPC%d; can only have %d per "
                   "GPC\n",
                   gpc, conf.numCpcPerGpc);
            conf.errmsg = conf.buff;
            return false;
        }
    }

    // Non-existent GPCs
    for (unsigned int gpc = conf.numGpcs; gpc < MAX_FS_PARTS; gpc++) {
        if ((gpcSettings.pesPerGpcMasks[gpc] != 0)&& conf.bPesWithTpcFs) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling PESs in non-existent GPC%d\n", gpc);
            conf.errmsg = conf.buff;
            return false;
        }
        if (gpcSettings.tpcPerGpcMasks[gpc] != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling TPCs in non-existent GPC%d\n", gpc);
            conf.errmsg = conf.buff;
            return false;
        }
        if ((gpcSettings.cpcPerGpcMasks[gpc] != 0)&& conf.bCpcInGpcFs) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling CPCs in non-existent GPC%d\n", gpc);
            conf.errmsg = conf.buff;
            return false;
        }
    }
    return true;
}

bool FsChip::CheckDisabledGpc(unsigned int gpc, const GpcMasks& gpcSettings) {
    // If this GPC is floorswept, it has to be completely disabled
    if ((gpcSettings.pesPerGpcMasks[gpc] != NBitsSet(conf.numPesPerGpc))&& conf.bPesWithTpcFs) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "PES masks are inconsistent for GPC%d which is disabled\n", gpc);
        DEBUGMSG("For GPC %d, pesPerGpcMask=%x, compared number=%x\n", gpc,
                 gpcSettings.pesPerGpcMasks[gpc], NBitsSet(conf.numPesPerGpc));
        conf.errmsg = conf.buff;
        return false;
    }
    if (gpcSettings.tpcPerGpcMasks[gpc] != NBitsSet(conf.numTpcPerGpc)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "TPC masks are inconsistent for GPC%d which is disabled\n", gpc);
        DEBUGMSG("For GPC %d, tpcPerGpcMask=%x, numTpcPerGpc=%d, compared number=%x\n", gpc,
                 gpcSettings.tpcPerGpcMasks[gpc], conf.numTpcPerGpc, NBitsSet(conf.numTpcPerGpc));
        conf.errmsg = conf.buff;
        return false;
    }
    if ((gpcSettings.cpcPerGpcMasks[gpc] != NBitsSet(conf.numCpcPerGpc))&& conf.bCpcInGpcFs) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "CPC masks are inconsistent for GPC%d which is disabled\n", gpc);
        DEBUGMSG("For GPC %d, cpcPerGpcMask=%x, compared number=%x\n", gpc,
                 gpcSettings.cpcPerGpcMasks[gpc], NBitsSet(conf.numCpcPerGpc));
        conf.errmsg = conf.buff;
        return false;
    }
    return true;
}

bool FsChip::CheckDisabledTpcWithPes(unsigned int gpc,
                                     const GpcMasks& gpcSettings) {
    // PES floorswept => all connected TPCs floorswept
    for (unsigned int pes = 0; pes < conf.numPesPerGpc; pes++) {
        bool pesDisabled = isDisabled(pes, gpcSettings.pesPerGpcMasks[gpc]);
        DEBUGMSG("For GPC %d, pes %d disabled=%d\n", gpc, pes, pesDisabled);
        unsigned int tpcsInPesMask =
            gpcSettings.tpcPerGpcMasks[gpc] & conf.pesToTpcMasks[pes];
        unsigned int allTpcsDisabledInPes = conf.pesToTpcMasks[pes];

        if (pesDisabled) {
            // All connected TPCs must be disabled.
            if (tpcsInPesMask != allTpcsDisabledInPes) {
                PRINTF(
                    conf.buff, sizeof(conf.buff),
                    "Inconsistent TPC masks for GPC%d. PES%d is disabled when "
                    "connected TPCs are present.\n",
                    gpc, pes);
                conf.errmsg = conf.buff;
                return false;
            }
        }
        // FIXME following check is dubious, but Chris wants it
        // There is a comment in http://lwbugs/200414345/21
        // PES can be still alive with all TPC connected to it are FS
        // But enforcing All connected TPCs floorswept => PES floorswept
        if (tpcsInPesMask == allTpcsDisabledInPes) {
            if (!pesDisabled) {
                PRINTF(conf.buff, sizeof(conf.buff),
                    "Inconsistent TPC masks for GPC%d. PES%d is enabled but "
                    "all its TPCs are disabled.\n",
                    gpc, pes);
                conf.errmsg = conf.buff;
                return false;
            }
        }

    }  // for each pes
    return true;
}

bool FsChip::CheckBounds(const FbpMasks& fbpSettings) {
    // Only existing FBPs can be disabled
    if (fbpSettings.fbpMask >= (1ull << conf.numFbps)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid FBP mask: %d. Disabling non-existent FBP(s)\n",
               fbpSettings.fbpMask);
        conf.errmsg = conf.buff;
        return false;
    }

    // Check if at least one FBP is present
    if (fbpSettings.fbpMask == NBitsSet(conf.numFbps)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid FBP mask: %d. At least one FBP must be present.\n",
               fbpSettings.fbpMask);
        conf.errmsg = conf.buff;
        return false;
    }

    // Non-existent elements in FBPs
    for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
        if (fbpSettings.fbioPerFbpMasks[fbp] > NBitsSet(conf.numFbioPerFbp)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent FBIO in FBP%d; can have only %d per "
                   "FBP\n",
                   fbp, conf.numFbioPerFbp);
            conf.errmsg = conf.buff;
            return false;
        }
        if ((conf.numFbpaPerFbp > 0) && (fbpSettings.fbpaPerFbpMasks[fbp] > NBitsSet(conf.numFbpaPerFbp))) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent FBPA in FBP%d; can have only %d per "
                   "FBP\n",
                   fbp, conf.numFbpaPerFbp);
            conf.errmsg = conf.buff;
            return false;
        }
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
        // skip for ga10x
        if ((conf.numFbpaPerFbp > 0) && (fbpSettings.halfFbpaPerFbpMasks[fbp] > NBitsSet(conf.numFbpaPerFbp))) {
            PRINTF(
                conf.buff, sizeof(conf.buff),
                "Half-FBPA mode set for non-existent FBPA in FBP%d; can have "
                "only %d PAs per FBP\n",
                fbp, conf.numFbpaPerFbp);
            conf.errmsg = conf.buff;
            return false;
        }
#endif
        if (fbpSettings.ltcPerFbpMasks[fbp] > NBitsSet(conf.numLtcPerFbp)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent LTC in FBP%d; can have only %d per "
                   "FBP\n",
                   fbp, conf.numLtcPerFbp);
            conf.errmsg = conf.buff;
            return false;
        }
        if (fbpSettings.l2SlicePerFbpMasks[fbp] > NBitsSet(conf.numL2SlicePerFbp)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling non-existent L2 slice in FBP%d; can have only %d per "
                   "FBP\n",
                   fbp, conf.numL2SlicePerFbp);
            conf.errmsg = conf.buff;
            return false;
        }
    }

    // Non-existent FBPs
    for (unsigned int fbp = conf.numFbps; fbp < MAX_FS_PARTS; fbp++) {
        if (fbpSettings.fbioPerFbpMasks[fbp] != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling FBIOs in non-existent FBP%d\n", fbp);
            conf.errmsg = conf.buff;
            return false;
        }
        if ((conf.numFbpaPerFbp > 0) && (fbpSettings.fbpaPerFbpMasks[fbp] != 0)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling FBPAs in non-existent FBP%d\n", fbp);
            conf.errmsg = conf.buff;
            return false;
        }
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
        if (fbpSettings.halfFbpaPerFbpMasks[fbp] != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling Half-FBPAs in non-existent FBP%d\n", fbp);
            conf.errmsg = conf.buff;
            return false;
        }
#endif
        if (fbpSettings.ltcPerFbpMasks[fbp] != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling LTC (ROP+L2) in non-existent FBP%d\n", fbp);
            conf.errmsg = conf.buff;
            return false;
        }
        if (fbpSettings.l2SlicePerFbpMasks[fbp] != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Disabling L2 Slices in non-existent FBP%d\n", fbp);
            conf.errmsg = conf.buff;
            return false;
        }
    }  // for each FBP

    unsigned int numActiveLtcs = 0;
    for (unsigned int fbp=0; fbp < conf.numFbps; fbp++) {
        unsigned int ltcmask = fbpSettings.ltcPerFbpMasks[fbp];
        unsigned int l2slicemask = fbpSettings.l2SlicePerFbpMasks[fbp];
        unsigned int numL2SlicePerLtc = (conf.numL2SlicePerFbp)/(conf.numLtcPerFbp);

        // Add check for l2slicemask corresponding to ltcmask, fs0_rand_snap bug
        if (conf.bL2SliceFs) {
            if ((ltcmask != 0) && (l2slicemask == 0)) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "Some LTCs are disabled but No L2 slices are disabled for FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }

            for (unsigned int ltc=0; ltc < conf.numLtcPerFbp; ltc++) {
                bool ltcDisabled = isDisabled(ltc, ltcmask);
                unsigned int shift = ltc*numL2SlicePerLtc;
                unsigned int shiftmask = NBitsSet(numL2SlicePerLtc);
                unsigned int l2sMaskPerLtc = (l2slicemask & (shiftmask << shift)) >> shift;
                if (ltcDisabled && l2sMaskPerLtc != NBitsSet(numL2SlicePerLtc)) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                           "LtcMask and L2SliceMask are not mutually compatible for FBP%d - LTC%d\n", fbp,ltc);
                    conf.errmsg = conf.buff;
                    return false;
                }

                // FIXME Kun confirmed min 2 slice per ltc is required for func_valid
                // num active slices in an active ltc
                if (!ltcDisabled) {
                    if (countDisabled(0,numL2SlicePerLtc-1,l2sMaskPerLtc) > numL2SlicePerLtc/2) {
                        PRINTF(conf.buff, sizeof(conf.buff),
                               "More than half the l2slices are disabled in an active ltc FBP%d - LTC%d\n", fbp,ltc);
                        conf.errmsg = conf.buff;
                        return false;
                    }
                }
            }
        }
        // AMAP disabled LTC list callwlation
        if (ltcmask == 0) {
            numActiveLtcs += conf.numLtcPerFbp;
        }
        else {
            for (unsigned int i=0; i<conf.numLtcPerFbp; i++) {
                if (((ltcmask>>i) & 1) == 0) {
                    numActiveLtcs++;
                }
            }
        }
    }
    for (auto ltcs = conf.amapDisabledLtcList.begin(); ltcs != conf.amapDisabledLtcList.end(); ltcs++) {
        if (*ltcs == numActiveLtcs) {
            PRINTF(conf.buff, sizeof(conf.buff),
                    "Number of total active LTCs: %d not supported by AMAP\n", numActiveLtcs);
            conf.errmsg = conf.buff;
            return false;
        }
    }


    return true;
}

bool FsChip::CheckUnsupportedModes(const GpcMasks& gpcSettings) {
    return true;
}

bool FsChip::CheckUnsupportedModes(const FbpMasks& fbpSettings) {
    for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
        if (!conf.bHalfFbpaFs && (fbpSettings.halfFbpaPerFbpMasks[fbp] != 0)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Half-FBPA masks are not supported for this chip but set in "
                   "FBP%d\n",
                   fbp);
            conf.errmsg = conf.buff;
            return false;
        }
#endif
        // TODO: Is this expected behavior?
        if (!(conf.bL2SliceFs || conf.bHopperSliceFs) && (fbpSettings.l2SlicePerFbpMasks[fbp] != 0)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "L2 Slice masks are not supported for this chip but set in "
                   "FBP%d\n",
                   fbp);
            conf.errmsg = conf.buff;
            return false;
        }
    }
    return true;
}

bool FsChip::CheckDisabledFbp(unsigned int fbp, const FbpMasks& fbpSettings) {
    // Enable the check back on as fbio masks must be provided
    if (fbpSettings.fbioPerFbpMasks[fbp] != NBitsSet(conf.numFbioPerFbp)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "FBIO masks are inconsistent for FBP%d which is disabled\n",
               fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    if ((conf.numFbpaPerFbp > 0) && (fbpSettings.fbpaPerFbpMasks[fbp] != NBitsSet(conf.numFbpaPerFbp))) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "FBPA masks are inconsistent for FBP%d which is disabled\n",
               fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    if (fbpSettings.ltcPerFbpMasks[fbp] != NBitsSet(conf.numLtcPerFbp)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "LTC masks are inconsistent for FBP%d which is disabled\n", fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    if ((conf.bL2SliceFs || conf.bHopperSliceFs) && (fbpSettings.l2SlicePerFbpMasks[fbp] != NBitsSet(conf.numL2SlicePerFbp))) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "L2Slice masks are inconsistent for FBP%d which is disabled\n", fbp);
        conf.errmsg = conf.buff;
        return false;
    }

    if (fbpSettings.halfFbpaPerFbpMasks[fbp] != 0) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Half-FBPA masks are inconsistent for FBP%d which is disabled\n",
               fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    // TODO: What should l2SlicePerFbpMasks be when FBP is disabled?
    return true;
}

// FIXME Why CheckPerfectFbp is not being called in GddrFS?
bool FsChip::CheckPerfectFbp(unsigned int fbp, const FbpMasks& fbpSettings) {
    if (fbpSettings.fbioPerFbpMasks[fbp] != 0) {
        PRINTF(
            conf.buff, sizeof(conf.buff),
            "Enabled FFs need to be perfect but FBIOs are disabled in FBP%d\n",
            fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    if ((conf.numFbpaPerFbp > 0) && (fbpSettings.fbpaPerFbpMasks[fbp] != 0)) {
        PRINTF(
            conf.buff, sizeof(conf.buff),
            "Enabled FFs need to be perfect but FBPAs are disabled in FBP%d\n",
            fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    if (fbpSettings.ltcPerFbpMasks[fbp] != 0) {
        PRINTF(
            conf.buff, sizeof(conf.buff),
            "Enabled FFs need to be perfect but LTCs are disabled in FBP%d\n",
            fbp);
        conf.errmsg = conf.buff;
        return false;
    }
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
    if (fbpSettings.halfFbpaPerFbpMasks[fbp] != 0) {
        PRINTF(
            conf.buff, sizeof(conf.buff),
            "Enabled FFs need to be perfect but half-FBPA mode is enabled in "
            "FBP%d\n",
            fbp);
        conf.errmsg = conf.buff;
        return false;
    }
#endif
    if (fbpSettings.l2SlicePerFbpMasks[fbp] != 0) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Enabled FFs need to be perfect but L2 Slices are disabled in "
               "FBP%d\n",
               fbp);
        conf.errmsg = conf.buff;
        return false;
    }
    return true;
}

unsigned int FsChip::FindSisterFbp(unsigned int searchFbp,
                                   const FbpMasks& fbpSettings) {
    DEBUGMSG("FindSisterFbp <> called with searchFbp = %d\n", searchFbp);
    for (unsigned int l2NoC = 0; l2NoC < conf.l2NocFbpMap.size(); l2NoC++) {
        std::vector<unsigned int> fbpsPerL2NoC = conf.l2NocFbpMap[l2NoC];
        DEBUGMSG("Going over Map: l2noc=%d\n", l2NoC);

        for (std::vector<unsigned int>::iterator it = fbpsPerL2NoC.begin();
             it != fbpsPerL2NoC.end(); it++) {
            unsigned int fbp = *it;
            DEBUGMSG("Going over iter: it=%d and it+1=%d\n", fbp, *(it + 1));
            if (fbp == searchFbp) {
                if ((it + 1) != fbpsPerL2NoC.end()) {
                    DEBUGMSG("case0: Returning searchFbp %d's sisterFbp %d\n",
                             searchFbp, *(it + 1));
                    return *(it + 1);
                } else {
                    DEBUGMSG("case1: Returning searchFbp %d's sisterFbp %d\n",
                             searchFbp, *(it - 1));
                    return *(it - 1);
                }
            }
        }
    }
    // Find should be able to track sisterFbp by now
    assert(0);
    return 0;
}

bool FsChip::CheckImperfectFbp(unsigned int fbp, const FbpMasks& fbpSettings,
                               const GpcMasks& gpcSettings) {
    if (fbpSettings.ltcPerFbpMasks[fbp] == NBitsSet(conf.numLtcPerFbp)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "All LTCs are disabled in an active FBP%d\n", fbp);
        conf.errmsg = conf.buff;
        return false;
    } else {
        if (CheckBothUgpuActive(fbpSettings, gpcSettings) && GpcInOppositeUgpu(fbp,fbpSettings,gpcSettings)) {
            unsigned int ltcMask = fbpSettings.ltcPerFbpMasks[fbp];
            unsigned int sisterFbp = FindSisterFbp(fbp, fbpSettings);
            unsigned int sisterLtcMask = fbpSettings.ltcPerFbpMasks[sisterFbp];
            DEBUGMSG("ImperfectFbp: ltcmask = 0x%x & sisterltcmask = 0x%x\n",
                     ltcMask, sisterLtcMask);
            if ((ltcMask != (sisterLtcMask << 1) &&
                 sisterLtcMask != (ltcMask << 1)) &&
                !(isDisabled(fbp, fbpSettings.fbpMask) ||
                  isDisabled(sisterFbp, fbpSettings.fbpMask))) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "Complementary LTCs are not enabled across an L2NoC "
                       "between fbps: %d and %d\n",
                       fbp, sisterFbp);
                conf.errmsg = conf.buff;
                return false;
            }
            if (conf.bHopperSliceFs) {
                unsigned int l2sMask = fbpSettings.l2SlicePerFbpMasks[fbp];
                unsigned int numL2sDisabled = countDisabled(0,conf.numL2SlicePerFbp,l2sMask);
                unsigned int numL2SlicePerLtc = (conf.numL2SlicePerFbp)/(conf.numLtcPerFbp);
                if (numL2sDisabled > 1 && (numL2sDisabled % numL2SlicePerLtc != 0)) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                           "Hopper slice FS only allows single slice FS or full LTC FS per FBP. "
                           "Invalid l2slicemask: 0x%x for fbp %d\n",
                           l2sMask, fbp);
                    conf.errmsg = conf.buff;
                    return false;
                }

                unsigned int sisterL2sMask = fbpSettings.l2SlicePerFbpMasks[sisterFbp];
                unsigned int swapL2sMask = swapL2SliceMask(conf.numL2SlicePerFbp,sisterL2sMask);
                DEBUGMSG("ImperfectFbp: l2sMask = 0x%x & sisterL2smask = 0x%x & swapL2smask = 0x%x\n",l2sMask, sisterL2sMask, swapL2sMask);
                 if (l2sMask != swapL2sMask) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                           "Corresponding L2 slices are not enabled across an L2NoC "
                           "between fbps: %d and %d\n",
                           fbp, sisterFbp);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
        }
    }
    if (fbpSettings.fbioPerFbpMasks[fbp] == NBitsSet(conf.numFbioPerFbp)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "All FBIOs are disabled in an active FBP%d\n", fbp);
        conf.errmsg = conf.buff;
        return false;
    } else {
        if (conf.chip == Chip::GH100) {
            uint32_t expected_fbio_mask = isDisabled(fbp, fbpSettings.fbpMask) ? 0x3 : 0x0;
            if (fbpSettings.fbioPerFbpMasks[fbp] != expected_fbio_mask) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "FBIOs can't be disabled unless FBP is disabled. FBP%d, FBIOMASK: %d, expected fbio_mask: %d, fbp_disabled: %d\n", fbp, fbpSettings.fbioPerFbpMasks[fbp], expected_fbio_mask, isDisabled(fbp, fbpSettings.fbpMask));
                conf.errmsg = conf.buff;
                return false;
            }
        } else {
            if (fbpSettings.fbioPerFbpMasks[fbp] !=
                fbpSettings.ltcPerFbpMasks[fbp]) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "FBIO mask is not same as LTCmask for active FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }
        }
    }
    // Following check only valid if fbpa_mask exists, it is missing in ga10x
    if (conf.numFbpaPerFbp > 0) {
        if (fbpSettings.fbpaPerFbpMasks[fbp] == NBitsSet(conf.numFbpaPerFbp)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "All FBPAs are disabled in an active FBP%d\n", fbp);
            conf.errmsg = conf.buff;
            return false;
        } else {
            if (fbpSettings.fbpaPerFbpMasks[fbp] !=
                fbpSettings.ltcPerFbpMasks[fbp]) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "FBPA mask is not same as LTCmask for active FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }
        }
    }
    // TODO: Include l2sliceFS later
    /*
        if (fbpSettings.l2SlicePerFbpMasks[fbp] != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Enabled FFs need to be perfect but L2 Slices are disabled in
       "
                   "FBP%d\n",
                   fbp);
            conf.errmsg = conf.buff;
            return false;
        }
        if (fbpSettings.halfFbpaPerFbpMasks[fbp] != 0) {
            PRINTF(
                conf.buff, sizeof(conf.buff),
                "Enabled FFs need to be perfect but half-FBPA mode is enabled in
       "
                "FBP%d\n",
                fbp);
            conf.errmsg = conf.buff;
            return false;
        }
    */
    return true;
}

bool FsChip::CheckHalfFbpaMode(unsigned int fbp, const FbpMasks& fbpSettings) {
    // Floorsweeping LTC <=> Half-FBPA mode
    unsigned int ltcMask = fbpSettings.ltcPerFbpMasks[fbp];
    unsigned int halfPaMask = fbpSettings.halfFbpaPerFbpMasks[fbp];

#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
    if ((ltcMask == 0) && (halfPaMask != 0)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Half-FBPA mode is enabled in FBP%d when all LTCs are present\n",
               fbp);
        conf.errmsg = conf.buff;
        return false;
    }
#endif
    // For configs with only 1 FBP and 1 LTC, ignore half_fbpa mask
    if ((ltcMask != 0) && 
        (countDisabled(0,conf.numFbps-1,fbpSettings.fbpMask) < conf.numFbps-1)) {
        // Some LTCs are floorswept
        unsigned int expectedHalfPaMask = 0;

        for (unsigned int ltc = 0; ltc < conf.numLtcPerFbp; ltc++) {
            unsigned int pa = ltc / conf.numLtcPerFbpa;
            bool ltcDisabled = isDisabled(ltc, ltcMask);
            expectedHalfPaMask = ltcDisabled
                                     ? SetBits(pa, pa, expectedHalfPaMask)
                                     : expectedHalfPaMask;
        }  // for each LTC

        // FIXME this actually works because the CheckHalfFbpaMode is called only
        // when balanced ltc config check failed
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
        if (halfPaMask != expectedHalfPaMask) {
            PRINTF(
                conf.buff, sizeof(conf.buff),
                "Half-FBPA masks are inconsistent in FBP%d which has disabled "
                "LTCs\n",
                fbp);
            conf.errmsg = conf.buff;
            return false;
        }
#endif
    }  // are LTCs floorswept

    return true;
}
// The following function should be deprecated
// Ga10x onward, we will use CheckBalancedLtc
bool FsChip::CheckLtcBalanced(const FbpMasks& fbpSettings) {
    // Balanced LTC mode is only supported for 2FBPs and 2LTCs/FBP so far
    // Do not call this function for other cases.
    assert(conf.numFbps == 2);
    assert(conf.numLtcPerFbp == 2);

    bool fbpsEnabled = !isDisabled(0, fbpSettings.fbpMask) &&
                       !isDisabled(1, fbpSettings.fbpMask);

    if (fbpsEnabled) {
        if (isDisabled(0, fbpSettings.ltcPerFbpMasks[0]) !=
            isDisabled(1, fbpSettings.ltcPerFbpMasks[1])) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Inconsistent LTC masks for FBP0, expected balanced LTC "
                   "(even-odd) floorsweeping\n");
            conf.errmsg = conf.buff;
            return false;
        }
        if (isDisabled(1, fbpSettings.ltcPerFbpMasks[0]) !=
            isDisabled(0, fbpSettings.ltcPerFbpMasks[1])) {
            PRINTF(conf.buff, sizeof(conf.buff),
                   "Inconsistent LTC masks for FBP1, expected balanced LTC "
                   "(even-odd) floorsweeping\n");
            conf.errmsg = conf.buff;
            return false;
        }
    }
    return true;
}

bool FsChip::CheckHbmSiteFs(const FbpMasks& fbpSettings,
                            const GpcMasks& gpcSettings) {
    int numActiveHbms = conf.numHbmSites;
    for (unsigned int hbm = 0; hbm < conf.numHbmSites; hbm++) {
        unsigned int numFbpDisabled = 0;
        bool hbm_disabled = false;
        std::vector<unsigned int> fbpsPerHbmSite = conf.hbmSiteFbpMap[hbm];

        for (std::vector<unsigned int>::iterator it = fbpsPerHbmSite.begin();
             it != fbpsPerHbmSite.end(); it++) {
            unsigned int fbp = *it;
            if (isDisabled(fbp, fbpSettings.fbpMask)) numFbpDisabled++;
        }
        // Special case for PRODUCT, any fbp disabled => Hbm disabled
        if (conf.mode == FSmode::PRODUCT) {
            if (numFbpDisabled > 0) {
                numActiveHbms--;
                hbm_disabled = true;
            }
        }
        // For FUNC_VALID, Hbm still valid with any valid fbp
        else {
            if (numFbpDisabled == fbpsPerHbmSite.size()) {
                numActiveHbms--;
                hbm_disabled = true;
            }
        }
        DEBUGMSG("hbmSite=%d numActiveHbms=%d hbm_disabled=%d\n", hbm,
                 numActiveHbms, hbm_disabled);

        assert(numActiveHbms >= 0);  // Impossible to have -ve HbmSites

        // HbmSite disabled <=> all its FBPs must be completely disabled.
        // HbmSite enabled  <=> all its FBPs must be enabled and perfect.
        for (std::vector<unsigned int>::iterator it = fbpsPerHbmSite.begin();
             it != fbpsPerHbmSite.end(); it++) {
            unsigned int fbp = *it;
            bool fbp_disabled = isDisabled(fbp, fbpSettings.fbpMask);
            if (conf.bFullHbmSiteFS) {
                if (hbm_disabled && !fbp_disabled) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                           "FBP%d is not disabled when HbmSite%d is disabled: "
                           "There are other disabled FBP(s) in this HbmSite \n",
                           fbp, hbm);
                    conf.errmsg = conf.buff;
                    return false;
                }
                if (!hbm_disabled && fbp_disabled) {
                    PRINTF(
                        conf.buff, sizeof(conf.buff),
                        "Not all FBPs in HbmSite%d are disabled when FBP%d is "
                        "disabled: There are other enabled FBP(s) in this "
                        "HbmSite \n",
                        hbm, fbp);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
            if (fbp_disabled && !CheckDisabledFbp(fbp, fbpSettings)) {
                return false;
            }
            if (conf.bImperfectFbpFS) {
                if (!fbp_disabled &&
                    !CheckImperfectFbp(fbp, fbpSettings, gpcSettings)) {
                    return false;
                }
            } 
            else {
                if (!fbp_disabled && !CheckPerfectFbp(fbp, fbpSettings)) {
                    return false;
                }
            }
        }  // for each FBP in HbmSite
    }      // for each HbmSite
    //Ravi's table relaxes the following condition
    if ((numActiveHbms < int(conf.minReqdHbmSitesForProduct)) && (conf.mode == FSmode::PRODUCT) && (!(conf.bGa100AsGa101))) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "%d HbmSites active, Less than %u HbmSites not allowed in this "
               "product config\n",
               numActiveHbms,(conf.minReqdHbmSitesForProduct));
        conf.errmsg = conf.buff;
        return false;
    }

    return true;
}
// This function checks whether each FBP has equal number of LTCs enabled, aka the balancedltc mode
// e.g. in ga10x, in this mode, 1 FBP will have 1 LTC and that LTC will drive two sub-PAs.
bool FsChip::CheckAllFbpHasHalfLtcEnabled(const FbpMasks& fbpSettings) {
    unsigned int lwrrentDisabledLtcPerActiveFbp = 0;
    unsigned int prevDisabledLtcPerActiveFbp = 0;
    bool isFirstFbp = true;

    // If only 1 FBP is active, return false, so that we do check half_fbpa mode
    // Note that this is not an error, so it doesn't have an errormsg
    if (countDisabled(0, conf.numFbps - 1, fbpSettings.fbpMask) == conf.numFbps - 1)
        return false;

    for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
        bool fbp_disabled = isDisabled(fbp, fbpSettings.fbpMask);
        if (!fbp_disabled) {
            unsigned int ltcmask = fbpSettings.ltcPerFbpMasks[fbp];
            lwrrentDisabledLtcPerActiveFbp = countDisabled(0,conf.numLtcPerFbp-1,ltcmask);
            // This is not an error condition, just checking if every fbp has equal number of ltcs enabled
            // Hence not adding any error msg
            if (!isFirstFbp && (lwrrentDisabledLtcPerActiveFbp != prevDisabledLtcPerActiveFbp)) {
                return false;
            }
            prevDisabledLtcPerActiveFbp = lwrrentDisabledLtcPerActiveFbp;
            isFirstFbp = false;
        }
    }
    return true;
}

bool FsChip::CheckGddrFs(const FbpMasks& fbpSettings) {
    bool isEveryFbpHasEqualLtc = CheckAllFbpHasHalfLtcEnabled(fbpSettings);
    for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
        bool fbp_disabled = isDisabled(fbp, fbpSettings.fbpMask);
        if (fbp_disabled && !CheckDisabledFbp(fbp, fbpSettings)) {
            return false;
        }
        if (!fbp_disabled) {
            // FBPA cannot be floorswept
            if ((conf.numFbpaPerFbp > 0) && (fbpSettings.fbpaPerFbpMasks[fbp] != 0)) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "FBPA masks are inconsisent for FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }
            // FBIO cannot be floorswept
            if (fbpSettings.fbioPerFbpMasks[fbp] != 0) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "FBIO masks are inconsistent for FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }
            // For GA102, we add L2 Slice floorsweeping
            if ((!conf.bL2SliceFs) && (fbpSettings.l2SlicePerFbpMasks[fbp] != 0)) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "L2 Slice masks are inconsistent for FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }

            // At least one LTC must be present, aka LTC FS
            if (fbpSettings.ltcPerFbpMasks[fbp] ==
                NBitsSet(conf.numLtcPerFbp)) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "Cannot disable all LTCs in active FBP%d\n", fbp);
                conf.errmsg = conf.buff;
                return false;
            }
            // Not adding any error msg since CheckHalfFbpaMode will add that
            if (!isEveryFbpHasEqualLtc) {
                if (conf.bHalfFbpaFs && !CheckHalfFbpaMode(fbp, fbpSettings)) {
                    return false;
                }
            }
        }  // fbp enabled
    }      // for each fbp

    if (conf.bBalancedLtcFs && !CheckLtcBalanced(fbpSettings)) {
        return false;
    }

    return true;
}

bool FsChip::CheckBothUgpuActive(const FbpMasks& fbpSettings,
                                 const GpcMasks& gpcSettings) {
    //    bool isUgpu0Present = ((fbpSettings.fbpMask &
    //    conf.uGpu0FbpDisableMask) != conf.uGpu0FbpDisableMask);
    //    bool isUgpu1Present = ((fbpSettings.fbpMask &
    //    conf.uGpu1FbpDisableMask) != conf.uGpu1FbpDisableMask);
    unsigned int fmask = fbpSettings.fbpMask;
    unsigned int gmask = gpcSettings.gpcMask;

    bool isUgpu0Present = ((fmask & 0x555) < 0x555) || ((gmask & 0xC3) < 0xC3);
    bool isUgpu1Present = ((fmask & 0xAAA) < 0xAAA) || ((gmask & 0x3C) < 0x3C);

    if (conf.bUseuGpu && !(conf.bAllow1uGpu || (isUgpu0Present && isUgpu1Present))) {
        PRINTF(conf.buff, sizeof(conf.buff),
            "Invalid config: chip does not allow only 1 uGPU\n");
        conf.errmsg = conf.buff;
    }

    return (isUgpu0Present && isUgpu1Present);
}

bool FsChip::GpcInOppositeUgpu(unsigned int fbp, const FbpMasks& fbpSettings,
                               const GpcMasks& gpcSettings) {
    unsigned int gmask = gpcSettings.gpcMask;
    unsigned int enabled_fbpmask = NBitsSet(conf.numFbps) & (~(1 << fbp));
    //std::cout << "For fbp = " << fbp << " : enabled_fbpmask = 0x" << std::hex << enabled_fbpmask << std::dec << std::endl;
    bool fbpInUgpu0 = ((enabled_fbpmask & 0x555) < 0x555);
    bool fbpInUgpu1 = ((enabled_fbpmask & 0xAAA) < 0xAAA);
    if ((fbpInUgpu0 && ((gmask & 0x3C) < 0x3C)) || (fbpInUgpu1 && ((gmask & 0xC3) < 0xC3))) {
        return true;
    }
    else {
        return false;
    }
}

bool FsChip::CheckHbmSiteDisabled(unsigned int searchFbp,
                                  const FbpMasks& fbpSettings) {
    //std::cout << "Entered CheckHbmSiteDisabled function" << std::endl;
    int index = -1;
    for (unsigned int hbm = 0; hbm < conf.numHbmSites; hbm++) {
        std::vector<unsigned int> fbpsPerHbmSite = conf.hbmSiteFbpMap[hbm];

        for (std::vector<unsigned int>::iterator it = fbpsPerHbmSite.begin();
             it != fbpsPerHbmSite.end(); it++) {
            unsigned int fbp = *it;
            if (fbp == searchFbp) { //found the searchFbp
                index = hbm;
                break;
            }
        }
    }
    //std::cout << "Found searchFbp" << searchFbp << " corresponding hbmsite: " << index << std::endl;
    bool hbm_disabled = true;
    if (index >= 0) {
        std::vector<unsigned int> fbpsPerHbmSite = conf.hbmSiteFbpMap[index];   //pick the indexed one
        for (std::vector<unsigned int>::iterator it = fbpsPerHbmSite.begin();
            it != fbpsPerHbmSite.end(); it++) {
            unsigned int fbp = *it;
            hbm_disabled = hbm_disabled && (isDisabled(fbp, fbpSettings.fbpMask));
        }
    }

    return hbm_disabled;
}

bool FsChip::CheckL2NoCFs(const FbpMasks& fbpSettings,
                          const GpcMasks& gpcSettings) {
    if (!CheckBothUgpuActive(fbpSettings, gpcSettings))  // L2NoC pairing irrelevant if any ugpu is inactive
        return true;

    for (unsigned int l2NoC = 0; l2NoC < conf.l2NocFbpMap.size(); l2NoC++) {
        bool l2NoC_disabled = false;
        std::vector<unsigned int> fbpsPerL2NoC = conf.l2NocFbpMap[l2NoC];

        for (std::vector<unsigned int>::iterator it = fbpsPerL2NoC.begin();
             it != fbpsPerL2NoC.end(); it++) {
            unsigned int fbp = *it;
            l2NoC_disabled |= isDisabled(fbp, fbpSettings.fbpMask);
        }

        // L2NoC disabled <=> All FBPs connected to L2NoC must be completely
        // disabled.
        // L2NoC enabled  <=> All FBPs connected to L2NoC must be enabled and
        // perfect.
        for (std::vector<unsigned int>::iterator it = fbpsPerL2NoC.begin();
             it != fbpsPerL2NoC.end(); it++) {
            unsigned int fbp = *it;
            bool fbp_disabled = isDisabled(fbp, fbpSettings.fbpMask);
            if (l2NoC_disabled && !fbp_disabled) {
                if ((conf.mode == FSmode::PRODUCT) || (GpcInOppositeUgpu(fbp,fbpSettings,gpcSettings) || !CheckHbmSiteDisabled(FindSisterFbp(fbp,fbpSettings),fbpSettings))) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                           "FBP%d is not disabled when L2NoC%d is disabled\n", fbp,
                           l2NoC);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
            if (!l2NoC_disabled && fbp_disabled) {
                PRINTF(conf.buff, sizeof(conf.buff),
                       "Not all FBPs in L2NoC%d are disabled when FBP%d is "
                       "disabled\n",
                       l2NoC, fbp);
                conf.errmsg = conf.buff;
                return false;
            }
            if (fbp_disabled && !CheckDisabledFbp(fbp, fbpSettings)) {
                return false;
            }
            //if (!fbp_disabled && !CheckPerfectFbp(fbp, fbpSettings)) {
            //    return false;
            // }
        }  // for each FBP in L2NoC
    }      // for each L2NoC
    return true;
}

bool FsChip::CheckValidGa101AsGa100(const FbpMasks& fbpSettings) {
    // make sure to set the allowedFbpMask
    assert(conf.allowedFbpMask != 0);
    if ((fbpSettings.fbpMask & conf.allowedFbpMask) != conf.allowedFbpMask) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GA101 config: input fbpMask 0x%X does not follow "
               "allowed fbpMask 0x%X\n",
               fbpSettings.fbpMask, conf.allowedFbpMask);
        conf.errmsg = conf.buff;
        return false;
    }
    return true;
}

bool FsChip::CheckValidGa101AsGa100(const GpcMasks& gpcSettings) {
    // make sure to set the allowedFbpMask
    assert(conf.allowedGpcMask != 0);
    if ((gpcSettings.gpcMask & conf.allowedGpcMask) != conf.allowedGpcMask) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GA101 config: input gpcMask 0x%X does not follow "
               "allowed gpcMask 0x%X\n",
               gpcSettings.gpcMask, conf.allowedGpcMask);
        conf.errmsg = conf.buff;
        return false;
    }
    return true;
}

bool FsChip::CheckValidUgpuConnection(const FbpMasks& fbpSettings,
                                      const GpcMasks& gpcSettings) {
    unsigned int fmask = fbpSettings.fbpMask;
    unsigned int gmask = gpcSettings.gpcMask;

    bool ugpu0Fbp = ((fmask & 0x555) < 0x555);
    bool ugpu0Gpc = ((gmask & 0xC3) < 0xC3);
    bool ugpu1Fbp = ((fmask & 0xAAA) < 0xAAA);
    bool ugpu1Gpc = ((gmask & 0x3C) < 0x3C);

    if (ugpu0Gpc && !ugpu0Fbp) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GA100 config: gpcs active in ugpu0 gpcMask 0x%X "
               "fbps inactive in ugpu0 fbpMask 0x%X\n",
               gpcSettings.gpcMask, fbpSettings.fbpMask);
        conf.errmsg = conf.buff;
        return false;
    }
    else if (ugpu1Gpc && !ugpu1Fbp) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GA100 config: gpcs active in ugpu1 gpcMask 0x%X "
               "fbps inactive in ugpu1 fbpMask 0x%X\n",
               gpcSettings.gpcMask, fbpSettings.fbpMask);
        conf.errmsg = conf.buff;
        return false;
    }
    else if (!ugpu0Gpc && ugpu0Fbp && (conf.mode == FSmode::PRODUCT)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GA100 Product config: no gpcs active in ugpu0 gpcMask 0x%X "
               "but fbps active in ugpu0 fbpMask 0x%X\n",
               gpcSettings.gpcMask, fbpSettings.fbpMask);
        conf.errmsg = conf.buff;
        return false;
    }
    else if (!ugpu1Gpc && ugpu1Fbp && (conf.mode == FSmode::PRODUCT)) {
        PRINTF(conf.buff, sizeof(conf.buff),
               "Invalid GA100 Product config: no gpcs active in ugpu1 gpcMask 0x%X "
               "but fbps active in ugpu1 fbpMask 0x%X\n",
               gpcSettings.gpcMask, fbpSettings.fbpMask);
        conf.errmsg = conf.buff;
        return false;
    }
    return true;
}

bool FsChip::CheckLtcFs(const FbpMasks& fbpSettings) {

    unsigned int prevL2SlicePerFbpDisabled,lwrrentL2SlicePerFbpDisabled;
    unsigned int prevL2SlicePerLtcMaskDisabled,lwrrentL2SlicePerLtcMaskDisabled;
    unsigned int prevLtcPerFbpDisabled,lwrrentLtcPerFbpDisabled;
    prevL2SlicePerFbpDisabled = lwrrentL2SlicePerFbpDisabled = prevL2SlicePerLtcMaskDisabled = lwrrentL2SlicePerLtcMaskDisabled = 0;
    prevLtcPerFbpDisabled = lwrrentLtcPerFbpDisabled = 0;
    bool isFirstActiveFbp = true;
    
    for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
        bool fbp_disabled = isDisabled(fbp, fbpSettings.fbpMask);
        if (!fbp_disabled) {
            unsigned int ltcMask = fbpSettings.ltcPerFbpMasks[fbp];
            unsigned int l2SliceMask = fbpSettings.l2SlicePerFbpMasks[fbp];
            lwrrentL2SlicePerFbpDisabled = countDisabled(0,conf.numL2SlicePerFbp-1,l2SliceMask);
            unsigned int numL2SlicePerLtc = (conf.numL2SlicePerFbp)/(conf.numLtcPerFbp);
            lwrrentLtcPerFbpDisabled =  countDisabled(0,conf.numLtcPerFbp-1,ltcMask);

            // Following check ensures equal number of l2slices per ltc across all active fbps
            unsigned int lwrrentL2SlicePerFbpMask = l2SliceMask;
            for (unsigned int ltc = 0; ltc < conf.numLtcPerFbp; ltc++) {
                lwrrentL2SlicePerLtcMaskDisabled = lwrrentL2SlicePerFbpMask & NBitsSet(numL2SlicePerLtc);
                // Skip the check for disabled LTC
                if (lwrrentL2SlicePerLtcMaskDisabled != NBitsSet(numL2SlicePerLtc)) {
                    // If not first active fbp, compare the number of disabled l2slices with that of a previous ltc
                    if (!isFirstActiveFbp && (countDisabled(0,conf.numL2SlicePerFbp-1,lwrrentL2SlicePerLtcMaskDisabled) != countDisabled(0,conf.numL2SlicePerFbp-1,prevL2SlicePerLtcMaskDisabled))) {
                        PRINTF(conf.buff, sizeof(conf.buff),
                            "Unequal number of L2slices disabled in LTCs across fbps: lwrrentL2SlicePerLtcMaskDisabled=0x%X for FBP%d and previous active prevL2SlicePerLtcMaskDisabled=0x%X. Please check the fbp_l2slice_enable chiparg\n", lwrrentL2SlicePerLtcMaskDisabled,fbp,prevL2SlicePerLtcMaskDisabled);
                        conf.errmsg = conf.buff;
                        return false;
                    }
                    // Set the previousl2slicemask if the current mask is not fully disabled
                    prevL2SlicePerLtcMaskDisabled = lwrrentL2SlicePerLtcMaskDisabled;
                }
                // Reduce the current mask, drop the last ltc
                lwrrentL2SlicePerFbpMask = lwrrentL2SlicePerFbpMask >> numL2SlicePerLtc;
            }
            
            // Following if checks if the numl2slices active in the previous active fbp matches the current one
            // Check skipped if the fbps have unequal number of ltcs
            if (!isFirstActiveFbp && (prevLtcPerFbpDisabled == lwrrentLtcPerFbpDisabled)) {
                if (!isFirstActiveFbp && (lwrrentL2SlicePerFbpDisabled != prevL2SlicePerFbpDisabled)) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Unequal number of L2slices disabled: lwrrentL2SlicePerFbpDisabled=%d for FBP%d and previous active FBP prevL2SlicePerFbpDisabled=%d. Please check the fbp_l2slice_enable chiparg \n", lwrrentL2SlicePerFbpDisabled,fbp,prevL2SlicePerFbpDisabled);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }

            // Set the variables for next iteration, i.e. active fbp
            prevL2SlicePerFbpDisabled = lwrrentL2SlicePerFbpDisabled;
            prevLtcPerFbpDisabled = lwrrentLtcPerFbpDisabled;
            isFirstActiveFbp = false;

            // Following if catches the case where numslice disabled in an active fbp > 6 in ga10x
            if ((countDisabled(0,conf.numL2SlicePerFbp-1,l2SliceMask) > (conf.numL2SlicePerFbp + (conf.numL2SlicePerFbp)/2))) {
                PRINTF(conf.buff, sizeof(conf.buff),
                    "Inconsistent l2SlicePerFbpMask 0x%X for FBP%d : %d L2 slices disabled \n", l2SliceMask, fbp, countDisabled(0,conf.numL2SlicePerFbp-1,l2SliceMask));
                conf.errmsg = conf.buff;
                return false;       
            }
        }
    }
    return true;
}
// Returns the next active fbp mask
unsigned int FsChip::GetNextL2SliceMask(const FbpMasks& fbpSettings, unsigned int init_fbp) {
    unsigned int l2sMask = 0;
    for (unsigned int fbp = init_fbp+1; fbp < (conf.numFbps); fbp++) {
        l2sMask = fbpSettings.l2SlicePerFbpMasks[fbp];
        if ((l2sMask != NBitsSet(conf.numL2SlicePerFbp)) || (!isDisabled(fbp,fbpSettings.fbpMask))) {
            break;
        }
    }
    return l2sMask;
}
bool FsChip::CheckProductL2SlicePerFbpPattern(const FbpMasks& fbpSettings) {
/*
    bool anyFbpDisabled = false;
    bool anyLtcDisabled = false;
    bool anyL2SliceDisabled = false;
    for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
        unsigned int fbpmask = fbpSettings.fbpMask;
        unsigned int ltcmask = fbpSettings.ltcPerFbpMasks[fbp];
        unsigned int l2smask = fbpSettings.l2SlicePerFbpMasks[fbp];
        unsigned int numL2SlicePerLtc = conf.numL2SlicePerFbp/conf.numLtcPerFbp;
        if (!isDisabled(fbp, fbpmask)) {
            for (unsigned int ltc=0; ltc < conf.numLtcPerFbp; ltc++) {
                if (!isDisabled(ltc, ltcmask)) {
                    unsigned int shift = ltc*(numL2SlicePerLtc);
                    unsigned int shiftmask = NBitsSet(numL2SlicePerLtc);
                    unsigned int l2sMaskPerLtc = (l2smask & (shiftmask << shift)) >> shift;
                    if (countDisabled(0,numL2SlicePerLtc-1,l2sMaskPerLtc) > 0) {
                        anyL2SliceDisabled = true;
                    }
                }
                else {
                    anyLtcDisabled = true;
                }
            }
        }
        else {
            anyFbpDisabled = true;
        }
    }
    // TODO Chris has AI to provide document. This is a big restriction for product_valid mode
    if (anyL2SliceDisabled && (anyFbpDisabled || anyLtcDisabled)) {
        PRINTF(conf.buff, sizeof(conf.buff),
            "L2slice FS and FBP/LTC FS together are not supported in product_valid mode, you might want to try func_valid mode");
        conf.errmsg = conf.buff;
        return false;
    }
*/
    // Checking at an fbp granularity
    for (unsigned int fbp = 0; fbp < (conf.numFbps) - 1; fbp++) {
        
        if (isDisabled(fbp, fbpSettings.fbpMask)) {
            continue;
        }

        unsigned int l2sMask = fbpSettings.l2SlicePerFbpMasks[fbp];
        unsigned int nextL2sMask = GetNextL2SliceMask (fbpSettings, fbp);
        // We make sure if it is not the last active fbp
        if (nextL2sMask != NBitsSet(conf.numL2SlicePerFbp)) { 
            if (conf.nextAllowedL2SlicePerFbpMasks.find(l2sMask) != conf.nextAllowedL2SlicePerFbpMasks.end()) {
                std::vector<unsigned int> allowedL2s = conf.nextAllowedL2SlicePerFbpMasks[l2sMask];
                // FIXME Following code is GA10x specific
                // assumes 2 ltc per fbp and 2 max entry per allowedL2Slice
                bool valid = false;
                for (auto l2s : allowedL2s) {
                    if((nextL2sMask == l2s) || (nextL2sMask == (l2s & 0x0f)) || (nextL2sMask == (l2s & 0xf0))) {
                        valid = true;
                    }
                }
                if (!valid) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Two conselwtive active fbp%d l2slicemask=0x%x and nextl2slicemask=0x%x, expected=0x%x -- not product_valid, Try func_valid mode\n",fbp,l2sMask,nextL2sMask,allowedL2s[0]);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
            // Just skip the check if no match found
            else {
                PRINTF(conf.buff, sizeof(conf.buff),
                    "fbp%d l2slicemask=0x%x is not product_valid l2slicemask, Try func_valid mode\n",fbp,l2sMask);
                conf.errmsg = conf.buff;
                return false;
            }
        }
    }

/*
    unsigned int errCnt = 0;
    for (unsigned int pattern = 0; pattern < conf.numL2SlicePatternAllowed; pattern++) {
        for (unsigned int fbp = 0; fbp < conf.numFbps; fbp++) {
            unsigned int l2s = fbpSettings.l2SlicePerFbpMasks[fbp];
            unsigned int l2sAllowed = conf.allowedL2SlicePerFbpMasks[pattern][fbp];
            // FIXME assuming 4 l2slices in a ltc, 2 ltcs in a fbp
            // make parameterized if needed later
            if (!((l2s == l2sAllowed) || (l2s == 0xff) || (l2s == (l2sAllowed | 0xf0)) || (l2s == (l2sAllowed | 0x0f)))) {
                errCnt++;
                break;
            }
        }
    }
    if (errCnt >= conf.numL2SlicePatternAllowed) {
        PRINTF(conf.buff, sizeof(conf.buff),
            "Given total l2slice mask for all FBPs do not follow any of the product_valid patterns, you might want to try func_valid mode");
        conf.errmsg = conf.buff;
        return false;

    }
*/
    return true;
}

bool FsChip::CheckRopInGpcFs(const GpcMasks& gpcSettings) {
    unsigned int prevRopDisCnt, lwrrRopDisCnt;
    prevRopDisCnt = lwrrRopDisCnt = 0;
    bool isFirstActiveGpc = true;
    for (unsigned int gpc = 0; gpc < conf.numGpcs; gpc++) {
        unsigned int ropMask = gpcSettings.ropPerGpcMasks[gpc];
        bool gpcDisabled = isDisabled(gpc, gpcSettings.gpcMask);
        if (!gpcDisabled) {
            if (ropMask == NBitsSet(conf.numRopPerGpc)) {
                PRINTF(conf.buff, sizeof(conf.buff),
                    "All ROPs disabled in active GPC%d \n",gpc);
                conf.errmsg = conf.buff;
                return false;
            }
            lwrrRopDisCnt = countDisabled(0,conf.numRopPerGpc-1,ropMask);

            if (conf.mode == FSmode::PRODUCT) {
                if (!isFirstActiveGpc && (lwrrRopDisCnt != prevRopDisCnt)) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Unequal number of ROPs disabled across active GPCs. GPC%d has %d ROPs disabled whereas the previous active GPC has %d ROPs disabled \n", gpc,lwrrRopDisCnt,prevRopDisCnt);
                    conf.errmsg = conf.buff;
                    return false;            
                }   
            }
            isFirstActiveGpc = false;
            prevRopDisCnt = lwrrRopDisCnt;
        }
    }
    return true;
}

bool FsChip::CheckCpcInGpcFs(unsigned int gpc, const GpcMasks& gpcSettings) {
    unsigned int cpcMask = gpcSettings.cpcPerGpcMasks[gpc];
    unsigned int tpcMask = gpcSettings.tpcPerGpcMasks[gpc];
    if (gpc == 0) {
        if (isDisabled(0,cpcMask)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                "CPC0 of GPC0 can't be disabled; cpcmask provided for gpc0 = 0x%x \n",cpcMask);
            conf.errmsg = conf.buff;
            return false;                
        }
    }
    for (unsigned int cpc = 0; cpc < conf.numCpcPerGpc; cpc++) {
        unsigned int all_tpc_disabled_cpc = NBitsSet(conf.numTpcPerCpc);
        if ((isDisabled(cpc,cpcMask)) && 
                (((tpcMask >> cpc*conf.numTpcPerCpc) & all_tpc_disabled_cpc) != all_tpc_disabled_cpc)) {

            PRINTF(conf.buff, sizeof(conf.buff),
                "Corresponding TPCs of floorswept [CPC%d,GPC%d] aren't disabled; tpcPerGpcMask[%d] =0x%x cpcPerGpcMask[%d] =0x%x\n",cpc,gpc,gpc,tpcMask,gpc,cpcMask);
            conf.errmsg = conf.buff;
            return false;                
        }
        if ((((tpcMask >> cpc*conf.numTpcPerCpc) & all_tpc_disabled_cpc) == all_tpc_disabled_cpc) 
                && !isDisabled(cpc,cpcMask)) {
            PRINTF(conf.buff, sizeof(conf.buff),
                "Corresponding CPC of floorswept TPCs [GPC%d] isn't disabled; tpcPerGpcMask[%d] =0x%x cpcPerGpcMask[%d] =0x%x\n",gpc,gpc,tpcMask,gpc,cpcMask);
            conf.errmsg = conf.buff;
            return false;             
        }
    }
    return true;
}

bool FsChip::CheckPesInOnlyGpc0Fs(const GpcMasks& gpcSettings) { 
    unsigned int pesMask = gpcSettings.pesPerGpcMasks[0];
    unsigned int tpcMask = gpcSettings.tpcPerGpcMasks[0];
    bool pesDisabled = isDisabled(0,pesMask);
    DEBUGMSG("For GPC %d, pes %d disabled=%d\n", gpc, pes, pesDisabled);
    unsigned int tpcsInPesMask = tpcMask & conf.pesToTpcMasks[0];
    unsigned int allTpcsDisabledInPes = conf.pesToTpcMasks[0];
    
    if (pesDisabled) {
        PRINTF(conf.buff, sizeof(conf.buff),
            "PES0 of GPC0 can't be disabled; pesmask provided for gpc0 = 0x%x \n",pesMask);
        conf.errmsg = conf.buff;
        return false;                
    }
    if (tpcsInPesMask==allTpcsDisabledInPes) {
        PRINTF(conf.buff, sizeof(conf.buff),
            "All TPCs for PES0,GPC0 can't be disabled; tpcmask provided for gpc0 = 0x%x \n",tpcMask);
        conf.errmsg = conf.buff;
        return false;
    }
    return true;
}

bool FsChip::CheckHopperSliceFs(const FbpMasks& fbpSettings, const GpcMasks& gpcSettings) {
    unsigned int numActiveLtcs = 0;
    unsigned int numActiveFbps = 0;
    unsigned int numActiveL2s  = 0;
    for (unsigned int fbp=0; fbp < conf.numFbps; fbp++) {
        unsigned int ltcmask = fbpSettings.ltcPerFbpMasks[fbp];
        unsigned int l2slicemask = fbpSettings.l2SlicePerFbpMasks[fbp];

        if ((((fbpSettings.fbpMask)>>fbp) & 1) == 0) {
            numActiveFbps++ ;
        }

        if (ltcmask == 0) {
            numActiveLtcs += conf.numLtcPerFbp;
        }
        else {
            for (unsigned int i=0; i<conf.numLtcPerFbp; i++) {
                if (((ltcmask>>i) & 1) == 0) {
                    numActiveLtcs++;
                }
            }
        }        
        DEBUGMSG("Checkbounds [fbp %d] l2slicemask = 0x%x\n", fbp,l2slicemask);
        if (l2slicemask == 0) {
            numActiveL2s += conf.numL2SlicePerFbp;
        }
        else {
            for (unsigned int i=0; i<conf.numL2SlicePerFbp; i++) {
                if (((l2slicemask>>i) & 1) == 0) {
                    numActiveL2s++;
                }
            }
        }
    }
    
    DEBUGMSG("Checkbounds numActiveFbps = %d, numActiveLtcs= %d, numActiveL2s = %d\n", numActiveFbps,numActiveLtcs,numActiveL2s);
    
    if (CheckBothUgpuActive(fbpSettings,gpcSettings)) { //uGPU mode
        //This should take care of 2slice per ltc FS
        if (numActiveFbps != conf.numFbps) {    //1 or more FBPs FS
            if (numActiveLtcs == conf.numLtcPerFbp*numActiveFbps) { //only FBP FS
                if (numActiveL2s % (2*conf.numL2SlicePerFbp) != 0) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Number of total active L2 slices: %d, active FBPs: %d. Can't support L2 slice FS and FBP FS together.\n", numActiveL2s, numActiveFbps);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
            else {  // LTC FS
                if (numActiveL2s % conf.numL2SlicePerFbp != 0) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Number of total active L2 slices: %d, active LTCs: %d. Can't support L2 slice FS and LTC+FBP FS together.\n", numActiveL2s, numActiveLtcs);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
        }
        else {  //no FBP FS
            if (numActiveLtcs == conf.numLtcPerFbp*numActiveFbps) { //only L2slice FS possible
                //FIXME capping 92 slices for Hopper slice FS
                if (conf.numL2SlicePerFbp*numActiveFbps - numActiveL2s > 4) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Number of total active L2 slices: %d, Not supported.\n", numActiveL2s);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
            else { //LTC FS
                if (numActiveL2s % conf.numL2SlicePerFbp != 0) {
                    PRINTF(conf.buff, sizeof(conf.buff),
                        "Number of total active L2 slices: %d, active LTCs: %d. Can't support L2 slice FS and LTC FS together.\n", numActiveL2s, numActiveLtcs);
                    conf.errmsg = conf.buff;
                    return false;
                }
            }
        }
    }
    else {  // non-uGPU mode
        unsigned int numL2SlicePerLtc = (conf.numL2SlicePerFbp)/(conf.numLtcPerFbp);
        if (numActiveL2s % numL2SlicePerLtc != 0) {
            PRINTF(conf.buff, sizeof(conf.buff),
                "Single uGPU mode: Number of total active L2 slices: %d, active LTCs: %d. Can't support L2 slice FS in single uGPU mode.\n", numActiveL2s, numActiveLtcs);
            conf.errmsg = conf.buff;
            return false;
        }        
    }
    return true;
}
/*
bool FsChip::IsFsValid(GpcMasks gpcSettings, std::string& msg) {
    // For this mode, ignore the FSLIB
    if (conf.mode == FSmode::IGNORE_FSLIB)
        return true;

    // First check for unsupported settings
    if (!CheckBounds(gpcSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }
    if (!CheckUnsupportedModes(gpcSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }

    if (conf.bGa100AsGa101) {
        if (!CheckValidGa101AsGa100(gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    for (unsigned int gpc = 0; gpc < conf.numGpcs; gpc++) {
        bool gpcDisabled = isDisabled(gpc, gpcSettings.gpcMask);
        if (gpcDisabled && !CheckDisabledGpc(gpc, gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
        else if (!gpcDisabled) {
            if (conf.bPesWithTpcFs &&
                !CheckDisabledTpcWithPes(gpc, gpcSettings)) {
                assert(!(conf.errmsg.empty()));
                msg = conf.errmsg;
                return false;
            }
        }  // GPC disabled
    }      // for each GPC
    return true;
}

bool FsChip::IsFsValid(FbpMasks fbpSettings, std::string& msg) {
    // For this mode, ignore the FSLIB
    if (conf.mode == FSmode::IGNORE_FSLIB)
        return true;

    // First check for unsupported settings
    if (!CheckBounds(fbpSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }

    if (!CheckUnsupportedModes(fbpSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }

    if (conf.bGa100AsGa101) {
        if (!CheckValidGa101AsGa100(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    if (conf.bHbmSiteFs) {
        if (!CheckHbmSiteFs(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    } else {
        if (!CheckGddrFs(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    // Skip L2NoCFS rules for GA101
    if (conf.bL2NoCFs & !conf.bGa100AsGa101) {
        if (!CheckL2NoCFs(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    return true;
}
*/
bool FsChip::IsFsValid(const GpcMasks& gpcSettings,
                       const FbpMasks& fbpSettings, std::string& msg) {
    // For this mode, ignore the FSLIB
    if (conf.mode == FSmode::IGNORE_FSLIB)
        return true;

    // Except these chips, ignore the FSLIB, lwrrently supports GA100 and GA10X
    // Exception: GA104 because MODS does not have support for that
    if (!(     conf.chip == Chip::GA100 
            || conf.chip == Chip::GA102
            || conf.chip == Chip::GA103
#ifdef LW_MODS_SUPPORTS_GA104_MASKS
            || conf.chip == Chip::GA104
#endif
            || conf.chip == Chip::GA106
            || conf.chip == Chip::GA107
            || conf.chip == Chip::GA10B
            || conf.chip == Chip::GH100
        )) {
        return true;
    }

    if (conf.bUseuGpu && !(conf.bAllow1uGpu || CheckBothUgpuActive(fbpSettings, gpcSettings))) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }
    // GPC floorsweeping checks
    // First check for unsupported settings
    if (!CheckBounds(gpcSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }
    if (!CheckUnsupportedModes(gpcSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }

    if (conf.bGa100AsGa101) {
        if (!CheckValidGa101AsGa100(gpcSettings)) {
            // assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    for (unsigned int gpc = 0; gpc < conf.numGpcs; gpc++) {
        bool gpcDisabled = isDisabled(gpc, gpcSettings.gpcMask);
        DEBUGMSG("Main: GPC%d disabled? %d\n", gpc, gpcDisabled);
        if (gpcDisabled && !CheckDisabledGpc(gpc, gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        } else if (!gpcDisabled) {
            if (conf.bPesWithTpcFs &&
                !CheckDisabledTpcWithPes(gpc, gpcSettings)) {
                assert(!(conf.errmsg.empty()));
                msg = conf.errmsg;
                return false;
            }
            
            if (conf.bCpcInGpcFs &&
                !CheckCpcInGpcFs(gpc, gpcSettings)) {
                assert(!(conf.errmsg.empty()));
                msg = conf.errmsg;
                return false;
            }
        }  // GPC disabled
    }      // for each GPC
    // FBP floorsweeping checks
    // First check for unsupported settings
    if (!CheckBounds(fbpSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }

    if (!CheckUnsupportedModes(fbpSettings)) {
        assert(!(conf.errmsg.empty()));
        msg = conf.errmsg;
        return false;
    }

    if (conf.bGa100AsGa101) {
        if (!CheckValidGa101AsGa100(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    if (conf.bHbmSiteFs) {
        if (!CheckHbmSiteFs(fbpSettings, gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    } 
    else {
        if (!CheckGddrFs(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
        if (conf.bL2SliceFs && !CheckLtcFs(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
        if (conf.bL2SliceFs && (conf.mode == FSmode::PRODUCT) && !CheckProductL2SlicePerFbpPattern(fbpSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;            
        }
    }

    // Skip L2NoCFS rules for GA101
    if (conf.bL2NoCFs & !conf.bGa100AsGa101) {
        if (!CheckL2NoCFs(fbpSettings, gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
        if (!CheckValidUgpuConnection(fbpSettings,gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }
    
    if (conf.bRopInGpcFs) {
        if (!CheckRopInGpcFs(gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    if (conf.bPesInOnlyGpc0Fs) {
        if (!CheckPesInOnlyGpc0Fs(gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    if (conf.bHopperSliceFs) {
        if (!CheckHopperSliceFs(fbpSettings,gpcSettings)) {
            assert(!(conf.errmsg.empty()));
            msg = conf.errmsg;
            return false;
        }
    }

    return true;
}

}  // namespace fslib
