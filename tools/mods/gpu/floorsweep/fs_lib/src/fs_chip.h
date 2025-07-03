#ifndef FS_CHIP_H
#define FS_CHIP_H

#include <string>
#include <vector>
#include "fs_lib.h"
#include <map>
#include <algorithm>

namespace fslib {

// For per-GPC, per-FBP array sizes, etc
static const unsigned int MAX_FS_PARTS = 32;
static const unsigned int ERRMSG_SIZE = 400;
// Chip-specific properties
struct ChipConfig {
    ChipConfig(Chip chip, FSmode mode);

    Chip chip;
    FSmode mode;

    // DRAM type
    enum class DramType { Hbm, Gddr, None };
    DramType dramType = DramType::None;

    unsigned int numGpcs = 0;
    unsigned int numTpcPerGpc = 0;
    unsigned int numPesPerGpc = 0;
    unsigned int numRopPerGpc = 0;
    unsigned int numTpcPerCpc = 0;
    unsigned int numCpcPerGpc = 0;

    unsigned int numFbps = 0;
    unsigned int numFbpaPerFbp = 0;
    unsigned int numFbioPerFbp = 0;
    unsigned int numLtcPerFbp = 0;

    unsigned int pesToTpcMasks[MAX_FS_PARTS] = {0};
    unsigned int numLtcPerFbpa = 0;
    unsigned int numHbmSites = 0;
    unsigned int numFbpPerHbmSite = 0;
    unsigned int numL2NoCs = 0;
    unsigned int minReqdHbmSitesForProduct = 0;
    unsigned int numL2SlicePerFbp = 0;
    unsigned int numL2SlicePatternAllowed = 0;

    // Floorsweeping Modes
    bool bPesWithTpcFs = false;
    bool bHalfFbpaFs = false;
    bool bBalancedLtcFs = false;
    bool bL2SliceFs = false;
    bool bHbmSiteFs = false;
    bool bL2NoCFs = false;
    bool bGa100AsGa101 = false;
    bool bFullHbmSiteFS = false;
    bool bImperfectFbpFS = false;
    bool bHalfLtcFs = false;
    bool bRopInGpcFs = false; // Future
    bool bHopperSliceFs = false;
    bool bCpcInGpcFs = false; // Future
    bool bPesInOnlyGpc0Fs = false; // Future
    bool bUseuGpu = true;
    bool bAllow1uGpu = true;

    std::vector<std::vector<unsigned int>> l2NocFbpMap;
    std::vector<std::vector<unsigned int>> hbmSiteFbpMap;

    std::vector<unsigned int> amapDisabledLtcList;
    
    unsigned int uGpu0FbpDisableMask = 0;
    unsigned int uGpu1FbpDisableMask = 0;

    // GA101 specific masks
    unsigned int allowedGpcMask = 0;
    unsigned int allowedFbpMask = 0;

    // GA10x specific masks
    std::vector<std::vector<unsigned int>> allowedL2SlicePerFbpMasks;
    std::map<unsigned int, std::vector<unsigned int>> nextAllowedL2SlicePerFbpMasks;

    // Error msg related stuff
    std::string errmsg;
    char buff[ERRMSG_SIZE];

};  // ChipConfig

struct FsChip {
   public:
    FsChip(Chip chip, FSmode mode);
    /*
        bool IsFsValid(GpcMasks gpcSettings, std::string& msg);
        bool IsFsValid(FbpMasks fbpSettings, std::string& msg);
    */
    bool IsFsValid(const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                   std::string& msg);
    bool IsFsValid(const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                   std::string& msg, bool printUgpu);
    bool CheckBothUgpuActive(const FbpMasks& fbpSettings,
                             const GpcMasks& gpcSettings);
    
   private:
    ChipConfig conf;
    bool CheckBounds(const GpcMasks& gpcSettings);
    bool CheckUnsupportedModes(const GpcMasks& gpcSettings);
    bool CheckDisabledGpc(unsigned int gpc, const GpcMasks &gpcSettings);
    bool CheckDisabledTpcWithPes(unsigned int gpc, const GpcMasks& gpcSettings);
    bool CheckBounds(const FbpMasks& fbpSettings);
    bool CheckUnsupportedModes(const FbpMasks& fbpSettings);
    bool CheckDisabledFbp(unsigned int fbp, const FbpMasks& fbpSettings);
    bool CheckPerfectFbp(unsigned int fbp, const FbpMasks& fbpSettings);
    bool CheckImperfectFbp(unsigned int fbp, const FbpMasks& fbpSettings,
                           const GpcMasks& gpcSettings);
    bool CheckHalfFbpaMode(unsigned int fbp, const FbpMasks& fbpSettings);
    bool CheckLtcBalanced(const FbpMasks& fbpSettings);
    bool CheckHbmSiteFs(const FbpMasks& fbpSettings,
                        const GpcMasks& gpcSettings);
    bool CheckGddrFs(const FbpMasks& fbpSettings);
    bool CheckL2NoCFs(const FbpMasks& fbpSettings, const GpcMasks& gpcSettings);
    bool CheckValidGa101AsGa100(const GpcMasks& gpcSettings);
    bool CheckValidGa101AsGa100(const FbpMasks& fbpSettings);
    unsigned int FindSisterFbp(unsigned int searchFbp,
                               const FbpMasks& fbpSettings);
    bool CheckValidUgpuConnection(const FbpMasks& fbpSettings,
                                  const GpcMasks& gpcSettings);
    bool CheckProductL2SlicePerFbpPattern(const FbpMasks& fbpSettings);
    bool CheckLtcFs(const FbpMasks& fbpSettings);
    bool GpcInOppositeUgpu(unsigned int fbp, const FbpMasks& fbpSettings,
                           const GpcMasks& gpcSettings);
    bool CheckHbmSiteDisabled(unsigned int searchFbp,
                              const FbpMasks& fbpSettings);
    bool CheckAllFbpHasHalfLtcEnabled(const FbpMasks& fbpSettings);
    bool CheckRopInGpcFs(const GpcMasks& gpcSettings);
    unsigned int GetNextL2SliceMask(const FbpMasks& fbpSettings, unsigned int fbp);
    bool CheckHopperSliceFs(const FbpMasks& fbpSettings,
                              const GpcMasks& gpcSettings);
    bool CheckCpcInGpcFs(unsigned int gpc, const GpcMasks& gpcSettings);
    bool CheckPesInOnlyGpc0Fs(const GpcMasks& gpcSettings);

};  // FsChip

}  // namespace fslib
#endif
