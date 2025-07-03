/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/fileholder.h"
#include "core/include/jscript.h"
#include "core/include/gpu.h"       // For Gpu::Shutdown
#include "core/include/platform.h"  // For Platform::FlushCpuWriteCombineBuffer
#include "device/interface/pcie.h"  // To get the BDF of a sub-device, for the pre-IST init script
#include "gpu/include/gpudev.h"     // For GpuDevice and GpuFamily
#include "gpu/fuse/fuse.h"          // To read FS tags
#include "gpu/reghal/reghal.h"      // For registers
#include "gpu/perf/zpi.h"           // To perform ZPI reboot

#include "ist.h"
#include "ist_init.h"               // For gpu initialization utils
#include "ist_smbus.h"              // For smbus related function
#include "ist_tagmap.h"             // GPU fuse tag maps

#include "basic/basic_store.h"
#include "DUT_Manager/ControllerStatusTableV1.h"
#include "DUT_Manager/FrequencyTable.h"
#include "DUT_Manager/VoltageTable.h"
#include "DUT_Manager/LWPEX_Config.h"
#include "image/image_load.h"                  // Our entry point to MAL
#include "image/image_global.h"
#include "CFG_filter/config_filter.h"
#include "MEM_Manager/mem_manager_impl.h"      // The memory manager
#include "ProcBlock/MATHS_ProcBlock_Impl.h"    // The results manager
#include "Serialization/SerStoreUtility.h"
#include "Serialization/LightStore.h"
#include "ExecBlock/controllerStatus.h"
#include "ExecBlock/testResult.h"
#include "ist_mem_device.h"                    // Our replacement for MAL's MemDevice

#include <memory>                   // For MAL shared pointers
#include <fstream>                  // Remove once WAR for GetTotalMemRequirements is removed
#include <set>
#include <regex>
#include <unistd.h>
#include <libgen.h>
#include <ctime>


BOOST_CLASS_EXPORT(MATHSerialization::TestProgram_light)
BOOST_CLASS_EXPORT(MATHSerialization::TestFlow_light)
BOOST_CLASS_EXPORT(MATHSerialization::TestSequence_light)
BOOST_CLASS_EXPORT(MATHS_store_ns::TestExelwtionTimes_impl)
BOOST_CLASS_EXPORT(MATHS_store_ns::TestExelwtionResults_impl)
BOOST_CLASS_EXPORT(MATHS_store_ns::ProcessedControllerResults_impl)
BOOST_CLASS_EXPORT(MATHS_store_ns::ProcessedResults_impl)
BOOST_CLASS_EXPORT(MATHS_store_ns::PassFailResults_impl)
BOOST_CLASS_EXPORT(MATHS_store_ns::FSResults_impl)
BOOST_CLASS_EXPORT(MATHS_execblock_ns::testResult)
BOOST_CLASS_EXPORT(MATHS_execblock_ns::controllerStatus)


using std::string;
namespace MemManager = MATHS_memory_manager_ns;

namespace
{
    /**
     * \brief Helper function that checks whether the filename ends with a
     *        particular extexsion
     */
    bool fileEndsWith(const std::string& path, const std::string& extension)
    {
        return (path.size() >= extension.size() &&
                path.compare(path.size() - extension.size(), extension.size(), extension) == 0);
    }
    

    /**
     * \brief Compute the system memory size to be allocated to load the test
     *        images. Through some sensitivity analysis, the size of the
     *        test image(s) was found to be a good proxy. The image size are
     *        labeled IMG2. Hence MODS scans through the configuration files
     *        adding the size of the IMG2 files. As an added bonus of this
     *        process, we check whether an IMG1 was part of the configuration
     *        files.
     */
    RC GetTotalMemRequirements
    (
        MemManager::MemDevice::MemRequirements* pMemRequirements,
        const std::string path,
        std::vector<std::string> &configFiles,
        std::map<std::string, std::vector<std::string>> &burstConfig,
        std::vector<std::string> &testProgramsNames
    )
    {
        Printf(Tee::PriDebug, "In GetTotalMemRequirements()\n");
        using Utility::ArraySize;
        MASSERT(pMemRequirements);
        MemManager::MemDevice::MemRequirements& memRequirements = *pMemRequirements;

        // Get the proper file extension
        bool isImageBin = fileEndsWith(path, ".img.bin");
        bool isConfig = fileEndsWith(path, ".cfg");

        // The blocks size if always 32MB
        const UINT32 blockSizeBytes = 33554432;
        size_t totalNumBlocks = 0;
        if (isImageBin)
        {   
            // A single binary file, let's get the size of the image file
            // We've noticed image size is a good proxy of the memory requirement.
            totalNumBlocks = boost::filesystem::file_size(path);
        }
        else if (isConfig)
        {   
            // The config file is a yaml file of config files which are also yaml files
            // we relwrsively go through all the IMG2 files and get their total size
            YAML::Node config = YAML::LoadFile(path);
            std::string dirName = dirname(const_cast<char*>(path.c_str()));
            YAML::Node root = config["config_files"];
            if (!root || !root.IsSequence())
            {   
                Printf(Tee::PriError, "The input config file %s is malformed\n", path.c_str());
                return RC::BAD_PARAMETER;
            }
            for (UINT32 i = 0; i < root.size(); i++)
            {   
                std::string subConfigPath = dirName + "/" + root[i].as<std::string>();
                FILE_EXISTS(subConfigPath);
                YAML::Node subConfig = YAML::LoadFile(subConfigPath);
                if (subConfig["Files"] && subConfig["Files"]["IMG1"])
                {
                    // IMG1 config file, just store name
                    configFiles.push_back(root[i].as<std::string>());
                }
                if (subConfig["Files"] && subConfig["Files"]["IMG2"])
                {   
                    std::string img2file = subConfig["Files"]["IMG2"].as<std::string>();
                    img2file = dirName + "/" + img2file; // Actually the path of IMG2
                    FILE_EXISTS(img2file);
                    totalNumBlocks += boost::filesystem::file_size(img2file);

                    // The config files are loaded by MAS
                    // the test program names are for MODS internal purposes, 
                    // to run them inorder listed in the config files
                    configFiles.push_back(root[i].as<std::string>());
                    std::string tpName = root[i].as<std::string>();
                    tpName.erase(tpName.end() - 4, tpName.end());
                    testProgramsNames.push_back(tpName);
                }
                else if (subConfig["Burst_Content"] && subConfig["Burst_Label"])
                {
                    // Config file to build a burst image
                    std::string burstLabel = subConfig["Burst_Label"].as<std::string>();
                    testProgramsNames.push_back(burstLabel);
                    for (UINT32 index = 0; index < subConfig["Burst_Content"].size(); index++)
                    {
                         std::string contentLabel =
                             subConfig["Burst_Content"][index].as<std::string>();
                         std::string burstConfigPath = dirName + "/" + contentLabel + ".cfg";
                         FILE_EXISTS(burstConfigPath);
                         configFiles.push_back(contentLabel + ".cfg");
                         YAML::Node burstNode = YAML::LoadFile(burstConfigPath);
                         if (burstNode["Files"] && burstNode["Files"]["IMG2"])
                         {
                             std::string img2file =
                                  burstNode["Files"]["IMG2"].as<std::string>();
                             totalNumBlocks += boost::filesystem::file_size(dirName + "/" +
                                      img2file);
                         }
                         // Populate the burstLabel content map.
                         burstConfig[burstLabel].push_back(contentLabel);
                    }
                }
            }
        }

        // The memmory manager is MAS is not quite efficient 
        // Hence we add 20% extra blocks for good measure.
        totalNumBlocks = totalNumBlocks >> 25;
        const UINT32 minBlocks = 10;
        if (totalNumBlocks < minBlocks)
        {
            totalNumBlocks = minBlocks;
        }
        totalNumBlocks *= 1.2;

        memRequirements.blockSizeBytes = blockSizeBytes;
        memRequirements.numBlocks = totalNumBlocks;

        Printf(Tee::PriNormal, "Number of Blocks %u\n", memRequirements.numBlocks);

        return RC::OK;
    }

    /**
     * \brief Extract the fuses tags from the finalFSask string
     * \note 
     *       The finalFsMask from MATHS is a ; separated string of FSTags. It can be a 
     *       combination of codec and non-codec FS e.g 
     *       CAT1_atpgFS_g6gpcpes0tpc1__Codec47;g6gpcpes0tpc1;g2gpcpes1tpc5;
     *       The codec and non-codec are the only two formats returned from MAS.
     *       Hence this function is pretty simple, it parses the string using the ";"
     *       delimiter then looks for a "__" in each token. If found
     *       the format is a codec otherwise it's a non-codec format
     **/

    std::set<string> ExtractFuseTags(const std::string &finalFsMask)
    {
        Printf(Tee::PriDebug, "In ExtractFuseTags()\n");
        vector<string> tokens;
        set<string>fusesTags;
        Utility::Tokenizer(finalFsMask, "; ", &tokens);

        for (UINT32 i = 0; i < tokens.size(); i++)
        {   
            // Do additional processing in case of GA10x
            size_t found = tokens[i].find("__");
            if (found == std::string::npos)
            {   
                // a non-codec FS format
                fusesTags.insert(tokens[i]);
            }
            else
            {   
                // codec FS format 
                std::string tmpStr = tokens[i].substr(0, found);
                found = tmpStr.find_last_of("_");
                fusesTags.insert(tmpStr.substr(found + 1));
            }
        }
 
        return fusesTags;
    }

    /**
     * \brief Compare a collection of "FS region" tags to the current FS config
     *
     *        This function returns three non-overlapping sets describing:
     *          - which tags match with floorswept regions (pTagsInFs)
     *          - which tags are on non-floorswept regions (pTagsOutOfFs)
     *          - which floorswept regions have not been tagged (pMissingTags)
     */
    RC MaskTags
    (
        const set<string>& tags,
        const Ist::IstFlow::FsTags& fsTagMap,
        const Ist::IstFlow::Fuses& fuses,
        string* pTagsInFs,
        string* pTagsOutOfFs,
        string* pMissingTags
    )
    {
        Printf(Tee::PriDebug, "In MaskTags()\n");
        if (fuses.empty() || fsTagMap.empty())
        {
            Printf(Tee::PriError,
                   "Failed to read fuse file at initialization, "
                   "so skipping FS masking of FS tags.\n"
                   "Test will only PASS if no tags were found\n");
            return RC::FILE_READ_ERROR;
        }

        MASSERT(pTagsInFs && pTagsOutOfFs && pMissingTags);
        string& tagsInFs = *pTagsInFs;
        string& tagsOutOfFs = *pTagsOutOfFs;
        string& missingTags = *pMissingTags;

        for (const Ist::IstFlow::FuseInfo& fuse : fuses)
        {
            // Sanity checks for the reference data
            if (fsTagMap.count(fuse.name) == 0)
            {
                Printf(Tee::PriError,
                       "Tag '%s' not found in fuse file under 'Tag_map'\n",
                       fuse.name.c_str());
                return RC::BAD_FORMAT;
            }
            if (fsTagMap.at(fuse.name).size() != fuse.size)
            {
                Printf(Tee::PriError, "Mismatch: Tag '%s' has %zu entries while"
                                      " the corresponding fuse has %u bits\n",
                       fuse.name.c_str(), fsTagMap.at(fuse.name).size(), fuse.size);
                return RC::BAD_FORMAT;
            }

            // Compare the state of each fuse bit to the incoming tags
            MASSERT(fuse.size <= sizeof(UINT32) * CHAR_BIT &&
                    "Fuses have grown too big for this algorithm to remain valid");
            for (size_t bitIndex = 0; bitIndex < fuse.size; ++bitIndex)
            {
                // Check the fuse mask to see if the fuse bit is lwrrently en/disabled
                const bool bitIsEnabled = ((fuse.mask & (UINT32(1) << bitIndex)) != 0);
                // The corresponding tag for this fuse bit.
                const string bitTag = fsTagMap.at(fuse.name)[bitIndex];
                // Is this tag in the input tags?
                const bool bitIsTagged = (tags.find(bitTag) != tags.end());
                // Compare the two and add to correct set
                if (bitIsEnabled && bitIsTagged)
                {
                    tagsOutOfFs += bitTag + ", ";
                }
                else if (!bitIsEnabled && bitIsTagged)
                {
                    tagsInFs += bitTag + ", ";
                }
                else if (!bitIsEnabled && !bitIsTagged)
                {
                    missingTags += bitTag + ", ";
                }
            }
        }

        // If "Default" or "Defaultfffxx" appear in the tags, need to report an error
        for (set<string>::iterator it = tags.begin(); it != tags.end(); it ++)
        {
            string tag = *it;
            if (regex_search(tag, regex("Default")))
            {
                tagsOutOfFs += tag + ", ";
            }
        }

        // Remove trailing commas
        if (!tagsInFs.empty())
        {
            tagsInFs.pop_back();       // space
            tagsInFs.pop_back();       // comma
        }
        if (!tagsOutOfFs.empty())
        {
            tagsOutOfFs.pop_back();    // space
            tagsOutOfFs.pop_back();    // comma
        }
        if (!missingTags.empty())
        {
            missingTags.pop_back();    // space
            missingTags.pop_back();    // comma
        }

        return RC::OK;
    }
}

namespace Ist
{

//--------------------------------------------------------------------
IstFlow::IstFlow(const std::vector<TestArgs>& testArgs)
    : m_TestArgs(testArgs)
{}

/**
 * \brief Gather GA10x fuse information needed after result processing
 * \note This must be called before the GPU enters IST mode because
 *       fuses are no longer available after that
 **/
RC IstFlow::GetGH100FuseInfo
( 
    const GpuDevice& gpu, 
    Ist::IstFlow::FsTags* pFsTagMap,
    Ist::IstFlow::Fuses* pFuses
)
{
    Printf(Tee::PriDebug, "In GetGH100FuseInfo()\n");
    const auto &subdevice = *(gpu.GetSubdevice(0));
     RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();

    const UINT32 numLTCPerFbp = regHal.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    // Unfortunately, the LTC count register returns the number of active slices
    // not the max count per LTC (http://lwbugs/2606117).
    // The decoder ring confirms that GH100 have a max of 4 slices per LTC
    const UINT32 maxL2SlicesPerLTC = 4;
    const UINT32 maxTotalL2Slice = maxL2SlicesPerLTC * numLTCPerFbp; // 8 total
    
    UINT32 maxCPCCount = regHal.Read32(MODS_SCAL_LITTER_NUM_CPC_PER_GPC);
    const UINT32 maxC2CCount = 1;
    const UINT32 maxL2perFBP = subdevice.GetMaxL2Count();
    const UINT32 maxLTCCount = maxL2perFBP * subdevice.GetMaxFbpCount();

    UINT32 totalL2Mask = 0;
    for (UINT32 lwrFbp = 0; lwrFbp < subdevice.GetMaxFbpCount(); lwrFbp++)
    {
        totalL2Mask |= subdevice.GetFsImpl()->L2Mask(lwrFbp) << (maxL2perFBP * lwrFbp);
    }
    UINT32 totalCpcMask = 0;
    // There are 8 GPC and each CPC fora GPC has 3 bit masks
    UINT32 CpcBitCount = Utility::CountBits(subdevice.GetFsImpl()->CpcMask(0)); //3
    for (UINT32 gpcNum = 0; gpcNum < subdevice.GetMaxGpcCount(); gpcNum++)
    {
        totalCpcMask |= subdevice.GetFsImpl()->CpcMask(gpcNum) << (CpcBitCount *gpcNum);
    }

    const UINT32 maxPerlinkCount = regHal.LookupAddress(MODS_SCAL_LITTER_NUM_LWLINK);
    vector<Ist::IstFlow::FuseInfo> fuses =
    {
        {"OPT_C2C_DISABLE", subdevice.GetFsImpl()->C2CMask(), maxC2CCount},
        {"OPT_CPC_DISABLE", totalCpcMask, maxCPCCount},
        {"OPT_FBIO_DISABLE", subdevice.GetFB()->GetFbioMask(),
            subdevice.GetFB()->GetMaxFbioCount()},
        {"OPT_FBP_DISABLE", subdevice.GetFsImpl()->FbpMask(), subdevice.GetMaxFbpCount()},
        {"OPT_GPC_DISABLE", subdevice.GetFsImpl()->GpcMask(), subdevice.GetMaxGpcCount()},
        {"OPT_L2SLICE_FBP0_DISABLE", subdevice.GetFsImpl()->L2SliceMask(0), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP1_DISABLE", subdevice.GetFsImpl()->L2SliceMask(1), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP2_DISABLE", subdevice.GetFsImpl()->L2SliceMask(2), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP3_DISABLE", subdevice.GetFsImpl()->L2SliceMask(3), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP4_DISABLE", subdevice.GetFsImpl()->L2SliceMask(4), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP5_DISABLE", subdevice.GetFsImpl()->L2SliceMask(5), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP6_DISABLE", subdevice.GetFsImpl()->L2SliceMask(6), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP7_DISABLE", subdevice.GetFsImpl()->L2SliceMask(7), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP8_DISABLE", subdevice.GetFsImpl()->L2SliceMask(8), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP9_DISABLE", subdevice.GetFsImpl()->L2SliceMask(9), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP10_DISABLE", subdevice.GetFsImpl()->L2SliceMask(10), maxTotalL2Slice},
        {"OPT_L2SLICE_FBP11_DISABLE", subdevice.GetFsImpl()->L2SliceMask(11), maxTotalL2Slice},
        {"OPT_LTC_DISABLE", totalL2Mask, maxLTCCount},
        {"OPT_LWDEC_DISABLE", subdevice.GetFsImpl()->LwdecMask(), subdevice.GetMaxLwdecCount()},
        {"OPT_LWJPG_DISABLE", subdevice.GetFsImpl()->LwjpgMask(), subdevice.GetMaxLwjpgCount()},
        //{"OPT_LWLINK_DISABLE", subdevice.GetFsImpl()->LwLinkLinkMask(),
        //    subdevice.GetMaxLwlinkCount()},
        {"OPT_OFA_DISABLE", subdevice.GetFsImpl()->OfaMask(), subdevice.GetMaxOfaCount()},
        {"OPT_PERLINK_DISABLE", subdevice.GetFsImpl()->LwLinkLinkMask(),maxPerlinkCount},
        {"OPT_SYS_PIPE_DISABLE", subdevice.GetFsImpl()->SyspipeMask(),
            subdevice.GetMaxSyspipeCount()},
        {"OPT_TPC_GPC0_DISABLE", subdevice.GetFsImpl()->TpcMask(0), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC1_DISABLE", subdevice.GetFsImpl()->TpcMask(1), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC2_DISABLE", subdevice.GetFsImpl()->TpcMask(2), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC3_DISABLE", subdevice.GetFsImpl()->TpcMask(3), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC4_DISABLE", subdevice.GetFsImpl()->TpcMask(4), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC5_DISABLE", subdevice.GetFsImpl()->TpcMask(5), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC6_DISABLE", subdevice.GetFsImpl()->TpcMask(6), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC7_DISABLE", subdevice.GetFsImpl()->TpcMask(7), subdevice.GetMaxTpcCount()}
    };
    (*pFuses).swap(fuses);
    *pFsTagMap = gh100_tagMap;

    return RC::OK;
}

/**
 * \brief Gather GA10x fuse information needed after result processing
 * \note This must be called before the GPU enters IST mode because
 *       fuses are no longer available after that
 **/
RC IstFlow::GetGA10xFuseInfo
(   
    const GpuDevice& gpu, 
    Ist::IstFlow::FsTags* pFsTagMap,
    Ist::IstFlow::Fuses* pFuses
)
{
    Printf(Tee::PriDebug, "In GetGA10xFuseInfo()\n");
    const auto& subdevice = *(gpu.GetSubdevice(0));
    RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();

    const UINT32 maxPesPerGpc = subdevice.GetMaxPesCount();
    const UINT32 maxTotalPesCount = maxPesPerGpc * subdevice.GetMaxGpcCount();
    UINT32 pesMask = 0;
    for (UINT32 lwrGpc = 0; lwrGpc < subdevice.GetMaxGpcCount(); lwrGpc++)
    {   
        pesMask |= subdevice.GetFsImpl()->PesMask(lwrGpc) <<  (maxPesPerGpc * lwrGpc);
    }
    const UINT32 maxRopPerGpc = regHal.Read32(MODS_PTOP_SCAL_NUM_ROP_PER_GPC);
    const UINT32 maxTotalRopCount = maxRopPerGpc * subdevice.GetMaxGpcCount();
    UINT32 ropMask = 0;
    for (UINT32 lwrGpc = 0; lwrGpc < subdevice.GetMaxGpcCount(); lwrGpc++)
    {   
        ropMask |=  subdevice.GetFsImpl()->RopMask(lwrGpc) << (maxRopPerGpc * lwrGpc);
    }
    const UINT32 numLTCPerFbp = regHal.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);

    // Unfortunately, the LTC count register returns the number of active slices
    // not the max count per LTC (http://lwbugs/2606117). After consulting the GA10x
    // decoder ring, it's confirmed that GA10x have a max of 4 slices per LTC
    const UINT32 maxSlicesPerLTC = 4;

    const UINT32 maxTotalSlice = maxSlicesPerLTC * numLTCPerFbp; // 8, there are 4 slices per LTC
    //const UINT32 maxLTCCount = numLTCPerFbp * subdevice.GetMaxFbpCount();


    // Populated fuses with the minimum info (i.e a la GA107)
    vector<Ist::IstFlow::FuseInfo> fuses =
    {   
        {"OPT_FBP_DISABLE", subdevice.GetFsImpl()->FbpMask(), subdevice.GetMaxFbpCount()},
        {"OPT_GPC_DISABLE", subdevice.GetFsImpl()->GpcMask(), subdevice.GetMaxGpcCount()},
        {"OPT_L2SLICE_FBP0_DISABLE", subdevice.GetFsImpl()->L2SliceMask(0), maxTotalSlice},
        {"OPT_L2SLICE_FBP1_DISABLE", subdevice.GetFsImpl()->L2SliceMask(1), maxTotalSlice},
        //{"OPT_LTC_DISABLE", , maxLTCCount},
        {"OPT_LWDEC_DISABLE", subdevice.GetFsImpl()->LwdecMask(), subdevice.GetMaxLwdecCount()},
        //{"OPT_LWENC_DISABLE", subdevice.GetFsImpl()->LwjpgMask(), subdevice.GetMaxLwjpgCount()},
        {"OPT_OFA_DISABLE", subdevice.GetFsImpl()->OfaMask(), subdevice.GetMaxOfaCount()},
        {"OPT_PES_DISABLE", pesMask, maxTotalPesCount},
        {"OPT_ROP_DISABLE", ropMask, maxTotalRopCount},
        {"OPT_TPC_GPC0_DISABLE", subdevice.GetFsImpl()->TpcMask(0), subdevice.GetMaxTpcCount()},
        {"OPT_TPC_GPC1_DISABLE", subdevice.GetFsImpl()->TpcMask(1), subdevice.GetMaxTpcCount()},
    };

    // The fuses above is for the minimum set of components, i.e for GA107
    // Below we add the additional components for each GPU class
    switch(subdevice.DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA106)
        case Gpu::GA106:
            fuses.push_back({"OPT_L2SLICE_FBP2_DISABLE", subdevice.GetFsImpl()->L2SliceMask(2),
                    maxTotalSlice});
            fuses.push_back({"OPT_TPC_GPC2_DISABLE", subdevice.GetFsImpl()->TpcMask(2),
                    subdevice.GetMaxTpcCount()});
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA104)
        case Gpu::GA104:
            fuses.push_back({"OPT_L2SLICE_FBP2_DISABLE", subdevice.GetFsImpl()->L2SliceMask(2),
                    maxTotalSlice});
            fuses.push_back({"OPT_L2SLICE_FBP3_DISABLE", subdevice.GetFsImpl()->L2SliceMask(3),
                    maxTotalSlice});
            fuses.push_back({"OPT_TPC_GPC2_DISABLE", subdevice.GetFsImpl()->TpcMask(2),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC3_DISABLE", subdevice.GetFsImpl()->TpcMask(3),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC4_DISABLE", subdevice.GetFsImpl()->TpcMask(4),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC5_DISABLE", subdevice.GetFsImpl()->TpcMask(5),
                    subdevice.GetMaxTpcCount()});
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA102)
        case Gpu::GA102:
            fuses.push_back({"OPT_L2SLICE_FBP2_DISABLE", subdevice.GetFsImpl()->L2SliceMask(2),
                    maxTotalSlice});
            fuses.push_back({"OPT_L2SLICE_FBP3_DISABLE", subdevice.GetFsImpl()->L2SliceMask(3),
                    maxTotalSlice});
            fuses.push_back({"OPT_L2SLICE_FBP4_DISABLE", subdevice.GetFsImpl()->L2SliceMask(4),
                    maxTotalSlice});
            fuses.push_back({"OPT_L2SLICE_FBP5_DISABLE", subdevice.GetFsImpl()->L2SliceMask(5),
                    maxTotalSlice});
            fuses.push_back({"OPT_TPC_GPC2_DISABLE", subdevice.GetFsImpl()->TpcMask(2),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC3_DISABLE", subdevice.GetFsImpl()->TpcMask(3),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC4_DISABLE", subdevice.GetFsImpl()->TpcMask(4),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC5_DISABLE", subdevice.GetFsImpl()->TpcMask(5),
                    subdevice.GetMaxTpcCount()});
            fuses.push_back({"OPT_TPC_GPC6_DISABLE", subdevice.GetFsImpl()->TpcMask(6),
                    subdevice.GetMaxTpcCount()});
            break;  
#endif
        default: // Do nothing
            break;
    }
    (*pFuses).swap(fuses);
    switch(subdevice.DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA102)
        case  Gpu::GA102:
            *pFsTagMap = ga102_tagMap;
            Printf(Tee::PriNormal, "GLOBAL_GPU_IMPL_GA102\n");
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA104)
        case  Gpu::GA104:
            *pFsTagMap = ga104_tagMap;
            Printf(Tee::PriNormal, "GLOBAL_GPU_IMPL_GA104\n");
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA106)
        case  Gpu::GA106:
            *pFsTagMap = ga106_tagMap;
            Printf(Tee::PriNormal, "GLOBAL_GPU_IMPL_GA106\n");
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA107)
        case  Gpu::GA107:
            *pFsTagMap = ga107_tagMap;
            Printf(Tee::PriNormal, "GLOBAL_GPU_IMPL_GA107\n");
            break;
#endif
        default:
            Printf(Tee::PriNormal, "Could not retrieve GA10x fuse tag maps\n");
            break;
    }

    return RC::OK;
}

/**
 * \brief Gather fuse information needed after result processing
 * \note This must be called before the GPU enters IST mode because
 *       fuses are no longer available after that
 **/
RC IstFlow::IstFlow::GetGA100FuseInfo
(
    const GpuDevice& gpu,
    Ist::IstFlow::FsTags* pFsTagMap,
    Ist::IstFlow::Fuses* pFuses
)
{
    Printf(Tee::PriDebug, "In GetGA100FuseInfo()\n");
    const auto& subdevice = *(gpu.GetSubdevice(0));
    RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();

    const UINT32 maxPesCount = subdevice.GetMaxPesCount();
    const UINT32 maxTotalPesCount = maxPesCount * subdevice.GetMaxGpcCount();
    UINT32 pesMask = 0;
    for (UINT32 lwrGpc = 0; lwrGpc < subdevice.GetMaxGpcCount(); lwrGpc++)
    {
        pesMask |= subdevice.GetFsImpl()->PesMask(lwrGpc) <<  (maxPesCount * lwrGpc);
    }

    const UINT32 maxL2Count = subdevice.GetMaxL2Count();
    const UINT32 maxTotalL2Count = subdevice.GetMaxFbpCount() * maxL2Count;
    UINT32 l2Mask = 0;
    for (UINT32 lwrFbp = 0; lwrFbp < subdevice.GetMaxFbpCount(); lwrFbp++)
    {
        l2Mask |= subdevice.GetFsImpl()->L2Mask(lwrFbp) << (maxL2Count * lwrFbp);
    }

    // Check the errors reported by MAL against the DEFECTIVE fuses when m_CheckDefectiveFuse is set
    // The following masks are all enable mask which means that if a bit is 1, this component is enabled
    const UINT32 fbioDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_FBIO_DEFECTIVE) & ((1 << subdevice.GetFB()->GetMaxFbioCount()) - 1);
    const UINT32 fbpDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_FBP_DEFECTIVE) & ((1 << subdevice.GetMaxFbpCount()) - 1);
    const UINT32 gpcDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_GPC_DEFECTIVE) & ((1 << subdevice.GetMaxGpcCount()) - 1);
    const UINT32 lwdecDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_LWDEC_DEFECTIVE) & ((1 << subdevice.GetMaxLwdecCount()) - 1);
    const UINT32 lwjpgDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_LWJPG_DEFECTIVE) & ((1 << subdevice.GetMaxLwjpgCount()) - 1);
    const UINT32 ofaDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_OFA_DEFECTIVE) & ((1 << subdevice.GetMaxOfaCount()) - 1);
    const UINT32 pesDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_PES_DEFECTIVE) & ((1 << maxTotalPesCount) - 1);
    const UINT32 ropL2DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_ROP_L2_DEFECTIVE) & ((1 << maxTotalL2Count) - 1);
    const UINT32 sysPipeDefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_SYS_PIPE_DEFECTIVE) & ((1 << subdevice.GetMaxSyspipeCount()) - 1);
    const UINT32 tpcGpc0DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 0) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc1DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 1) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc2DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 2) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc3DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 3) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc4DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 4) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc5DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 5) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc6DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 6) & ((1 << subdevice.GetMaxTpcCount()) - 1);
    const UINT32 tpcGpc7DefectiveMask = ~regHal.Read32(MODS_FUSE_OPT_TPC_GPCx_DEFECTIVE, 7) & ((1 << subdevice.GetMaxTpcCount()) - 1);


    vector<Ist::IstFlow::FuseInfo> fuses =
    {
        //TODO: Figure out how to handle LWLink IOCTRL mask and MaxCount
        // Note that MODS <Fs>Mask functions return Enable masks. Eg. FbMask(), GpcMask()
        {
            "OPT_FBIO_DISABLE", 
            m_CheckDefectiveFuse ? fbioDefectiveMask : subdevice.GetFB()->GetFbioMask(),
            subdevice.GetFB()->GetMaxFbioCount()
        },
        {
            "OPT_FBP_DISABLE", 
            m_CheckDefectiveFuse ? fbpDefectiveMask : subdevice.GetFsImpl()->FbpMask(),
            subdevice.GetMaxFbpCount()
        },
        {
            "OPT_GPC_DISABLE",
            m_CheckDefectiveFuse ? gpcDefectiveMask : subdevice.GetFsImpl()->GpcMask(),
            subdevice.GetMaxGpcCount()
        },
        {
            "OPT_LWDEC_DISABLE",
            m_CheckDefectiveFuse ? lwdecDefectiveMask : subdevice.GetFsImpl()->LwdecMask(),
            subdevice.GetMaxLwdecCount()
        },
        {
            "OPT_LWJPG_DISABLE",
            m_CheckDefectiveFuse ? lwjpgDefectiveMask : subdevice.GetFsImpl()->LwjpgMask(),
            subdevice.GetMaxLwjpgCount()
        },
        //{"OPT_LWLINK_DISABLE", linkMask, maxLinks},
        {
            "OPT_OFA_DISABLE",
            m_CheckDefectiveFuse ? ofaDefectiveMask : subdevice.GetFsImpl()->OfaMask(),
            subdevice.GetMaxOfaCount()
        },
        {
            "OPT_PES_DISABLE",
            m_CheckDefectiveFuse ? pesDefectiveMask : pesMask,
            maxTotalPesCount
        },
        {
            "OPT_ROP_L2_DISABLE",
            m_CheckDefectiveFuse ? ropL2DefectiveMask : l2Mask,
            maxTotalL2Count
        },
        {
            "OPT_SYS_PIPE_DISABLE",
            m_CheckDefectiveFuse ? sysPipeDefectiveMask : subdevice.GetFsImpl()->SyspipeMask(),
            subdevice.GetMaxSyspipeCount()
        },
        {
            "OPT_TPC_GPC0_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc0DefectiveMask : subdevice.GetFsImpl()->TpcMask(0),
            subdevice.GetMaxTpcCount()
        },
        {
            "OPT_TPC_GPC1_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc1DefectiveMask : subdevice.GetFsImpl()->TpcMask(1),
            subdevice.GetMaxTpcCount()
        },
        {
            "OPT_TPC_GPC2_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc2DefectiveMask : subdevice.GetFsImpl()->TpcMask(2),
            subdevice.GetMaxTpcCount()
        },
        {
            "OPT_TPC_GPC3_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc3DefectiveMask : subdevice.GetFsImpl()->TpcMask(3),
            subdevice.GetMaxTpcCount()},
        {
            "OPT_TPC_GPC4_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc4DefectiveMask : subdevice.GetFsImpl()->TpcMask(4),
            subdevice.GetMaxTpcCount()
        },
        {
            "OPT_TPC_GPC5_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc5DefectiveMask : subdevice.GetFsImpl()->TpcMask(5),
            subdevice.GetMaxTpcCount()
        },
        {
            "OPT_TPC_GPC6_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc6DefectiveMask : subdevice.GetFsImpl()->TpcMask(6),
            subdevice.GetMaxTpcCount()
        },
        {
            "OPT_TPC_GPC7_DISABLE",
            m_CheckDefectiveFuse ? tpcGpc7DefectiveMask : subdevice.GetFsImpl()->TpcMask(7),
            subdevice.GetMaxTpcCount()
        },
    };

    (*pFuses).swap(fuses);
    *pFsTagMap = ga100_tagMap;

    return RC::OK;
}
//--------------------------------------------------------------------
RC IstFlow::CheckHwCompatibility()
{
    Printf(Tee::PriDebug, "In CheckHwCompatibility()\n");

    const State initialState = m_State;
    m_State = State::NOT_SUPPORTED;   // In case any of the tests fail

    // The GPU must be initialized in "Normal mode" and not in IST mode in order
    // for us to read fuses and registers. If this is not the case, there is a programming error
    SW_ERROR((initialState != State::NORMAL_MODE) || !m_pGpu,
             "CheckHwCompatibility() must come after Initialize()\n");

    // IST HW present (family >= Ampere)
    if (!m_pGpu->GetSubdevice(0)->HasFeature(Device::Feature::GPUSUB_HAS_IST))
    {
        Printf(Tee::PriError, "IST/MATHS-Link not present on this chip\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // IST_DISABLE fuse not blown
    {
        RC rc;
        RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();
        if (regHal.Test32(MODS_FUSE_OPT_IST_DISABLE_DATA_YES))
        {
            Printf(Tee::PriError,
                   "IST is not available on this device (fuse opt_ist_disable is blown)\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
    }

    // All tests passed. Set state variable back to what it was when entering the function
    m_State = initialState;
    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::Initialize()
{
    RC rc;
    Printf(Tee::PriDebug, "In Initialize()\n");

    SW_ERROR((m_State != State::UNDEFINED),
             "Initialize() should only be called if uninitialized\n");

    JavaScriptPtr pJs;
    JsArray pllProgInfoList;

    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                "g_PvsIgnoreError233", &m_PvsIgnoreError233));
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                "g_DelayAfterPollingMs", &m_DelayAfterPollingMs));
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                "g_DelayAfterBmcCmdMs", &m_DelayAfterBmcCmdMs));
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(), "g_Img1Path", &m_Img1Path));
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                "g_StepIncrementOffsetMv", &m_StepIncrementOffsetMv));
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(), "g_IsPG199Board", &m_IsPG199Board));

    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(), "g_PllNameParams", &pllProgInfoList));
    // Retrieve the PLL programming infos
    for (UINT32 i = 0; i < pllProgInfoList.size(); i++)
    {
        JSObject *pllProgInfoObj;
        PllProgArgs pllProgInfo;
        pJs->FromJsval(pllProgInfoList[i], &pllProgInfoObj);
        CHECK_RC(pJs->GetProperty(pllProgInfoObj, "pllname", &pllProgInfo.pllName));
        CHECK_RC(pJs->GetProperty(pllProgInfoObj, "pllfreq", &pllProgInfo.pllFrequencyMhz));
        m_PllProgInfos.push_back(pllProgInfo);
    }


    m_CoreMalHandler.set_Info("bypass_rpc_constraints", m_SkipJTAGCommonInit ? "1" : "0");

    // MODS-IST is to support two types of images:
    // SLT type: .img.bin image
    // ATE type .cfg config file.
    std::string path = Utility::RestoreString(m_TestArgs[0].path);
    m_IsATETypeImage = fileEndsWith(path, ".cfg");

    // Enumerate devices, map BAR0, etc
    const Init::InitGpuArgs initGpuArgs =
    {   
        false,
        m_FbBroken,
        !m_PreIstScript.empty() || !m_UserScript.empty() || m_IsATETypeImage,
        static_cast<UINT64>(m_TestTimeoutMs)
    };

    rc = Init::InitGpu(&m_pGpu, initGpuArgs, m_FsArgs);
    if (rc != RC::OK)
    {
        // No need to capture rc here because this line will only be run
        // if something has already failed, so we want to retain the rc
        // of the original failure.
        (void)RebootIntoNormalMode();
        return rc;
    }

    // Retrieve all the fuse information we might need for later
    // (can't be retrieved once we're in IST mode)
    switch(m_pGpu->GetSubdevice(0)->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
        case Gpu::GA100:
            rc = GetGA100FuseInfo(*m_pGpu, &m_FsTagMap, &m_Fuses);
            if (rc != RC::OK)
            {
                Printf(Tee::PriWarn,
                       "Failed to retrieve information from fuse file.\n");
                // Don't error out in this case.
                // We can still run the tests and report results; we just won't
                // be able to mask out the errors which are in floorswept regions
                rc.Clear();
            }
            m_ChipID = "ga100";
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA102) || \
    LWCFG(GLOBAL_GPU_IMPL_GA104) || \
    LWCFG(GLOBAL_GPU_IMPL_GA106) || \
    LWCFG(GLOBAL_GPU_IMPL_GA107)
        case Gpu::GA102:
            m_ChipID = m_ChipID.empty() ? "ga102" : m_ChipID;
        case Gpu::GA104:
            m_ChipID = m_ChipID.empty() ? "ga104" : m_ChipID;
        case Gpu::GA106:
            m_ChipID = m_ChipID.empty() ? "ga106" : m_ChipID;
        case Gpu::GA107:
            m_ChipID = m_ChipID.empty() ? "ga107" : m_ChipID;
            rc = GetGA10xFuseInfo(*m_pGpu, &m_FsTagMap, &m_Fuses);
            if (rc != RC::OK)
            {
                Printf(Tee::PriWarn, "Failed to retrieve information from fuse file.\n");
                rc.Clear();
            }
            m_IsGa10x = true;
            // TODO: Handshake with VBIOS to know the supported voltage increment. Lwrrently,
            // MODS supports voltage increment of 25mv or 12.5mV. Up to the user to make sure that
            // the proper VBIOS is loaded. If not, erroneous voltage measurements to be reported.
            Printf(Tee::PriWarn, "Check that the loaded VBIOS does support %.1fmv increment.\n\n",
                    m_StepIncrementOffsetMv);
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
       case Gpu::GH100:
            m_ChipID = "gh100";
            rc = GetGH100FuseInfo(*m_pGpu, &m_FsTagMap, &m_Fuses);
            if (rc != RC::OK)
            {
                Printf(Tee::PriWarn, "Failed to retrieve information from fuse file.\n");
                rc.Clear();
            }
            if (!m_PreIstScript.empty())
            {
                // On GH100, the lwpex script is translated into 
                // MAS compatible transaction
                CHECK_RC(SetMALProgramFlowToLWPEX());
            }
            break;
#endif            
        default:
            Printf(Tee::PriWarn, "Could not retrieve GPU fuse tag maps\n");
    }

    /*
     *  Set the proper RunLoop function to be exelwted.
     *  IST exelwtion implemention is different between GPU family,
     *  mostly due to HW diff. Until we find a way to reconciliate
     *  both designs, we'd use a fork.
     */
    switch(m_pGpu->GetFamily())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA100) || \
    LWCFG(GLOBAL_GPU_IMPL_GA102) || \
    LWCFG(GLOBAL_GPU_IMPL_GA104) || \
    LWCFG(GLOBAL_GPU_IMPL_GA106) || \
    LWCFG(GLOBAL_GPU_IMPL_GA107)
        case GpuDevice::Ampere:
            RunLoop = &IstFlow::RunLoopAmpere;
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        case GpuDevice::Hopper:
            RunLoop = &IstFlow::RunLoopHopper;
            break;
#endif            
        default:
            Printf(Tee::PriError, "Unsupported GPU family device\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (m_IsGa10x && m_PllProgInfos.size() && m_Img1Path.empty())
    {
        Printf(Tee::PriError, "Pll frequency or name set but no Image1 path specified\n");
        return RC::BAD_PARAMETER;
    }

    /*
     * The RunMode is used by MAL to manage the exelwtion block i.e load specific
     * preist code etc. A user provided script or VBIOS takes care of the preist
     * sequence. Hence, we set the RunMode to instruct MAS exelwtion block
     * to skip performing its preist sequence, Only to fill internal 
     * MAS bookeeping structures.
     */

    if ((!m_PreIstScript.empty() || !m_UserScript.empty()) && m_ChipID != "gh100")
    {
        m_CoreMalHandler.set_Info("RunMode", "PreIstScript");
    }
    else if (!m_IsATETypeImage)
    {
        m_CoreMalHandler.set_Info("RunMode", "VBIOS");
    }

    if (m_RebootWithZpi && m_ChipID != "gh100")
    {
        CHECK_RC(m_ZpiLock.CreateLockFile(".modsIstZpi.lock"));
        if (m_ZpiScript.empty())
        {
            // External ZPI script not used, initialize MODS ZPI interface
            if (m_ZpiImpl.get() == nullptr)
            {
                m_ZpiImpl = make_unique<ZeroPowerIdle>();
            }

            m_ZpiImpl->SetIsSeleneSystem(m_SeleneSystem);
            m_ZpiImpl->SetIstMode(true);
            m_ZpiImpl->SetDelayBeforeOOBEntryMs(m_DelayBeforeOOBEntryMs);
            m_ZpiImpl->SetDelayAfterOOBExitMs(m_DelayAfterOOBExitMs);
            m_ZpiImpl->SetLinkPollTimeoutMs(m_LinkPollTimeoutMs);
            m_ZpiImpl->SetDelayBeforeRescanMs(m_DelayBeforeRescanMs);
            m_ZpiImpl->SetRescanRetryDelayMs(m_RescanRetryDelayMs);
            m_ZpiImpl->SetDelayAfterPexRstAssertMs(m_DelayAfterPexRstAssertMs);
            m_ZpiImpl->SetDelayBeforePexDeassertMs(m_DelayBeforePexDeassertMs);
            m_ZpiImpl->SetDelayAfterExitZpiMs(m_DelayAfterExitZpiMs);
            m_ZpiImpl->SetDelayAfterPollingMs(m_DelayAfterPollingMs);
            m_ZpiImpl->SetDelayAfterBmcCmdMs(m_DelayAfterBmcCmdMs);

            CHECK_RC(m_ZpiLock.AcquireLockFile());
            {
                DEFER {m_ZpiLock.ReleaseLockFile();};
                CHECK_RC(m_ZpiImpl->Setup(m_pGpu->GetSubdevice(0)));
            }
        }
    }
    CHECK_RC(m_SysMemLock.CreateLockFile(".modsIstSysMem.lock"));

    if (!m_SeleneSystem)
    {
        // Luna systems use the IPMI device, no need to Initialize SMBus
        m_SmbusImpl = make_unique<IstSmbus>();
        rc = m_SmbusImpl->InitializeIstSmbusDevice(m_pGpu->GetSubdevice(0));
        if (rc != RC::OK)
        {
            // Initialisation failed, not a problem. We just won't be able to
            // report the temperature.
            Printf(Tee::PriWarn, "SMBus not initialized on this device.\n");
            rc.Clear();
        }
        else
        {
            m_SmbusInitialized = true;
        }
    }

    const auto& pPcie = m_pGpu->GetSubdevice(0)->GetInterface<Pcie>();
    m_DevUnderTestBdf = Utility::StrPrintf("%04x:%02x:%02x.%02x", pPcie->DomainNumber(),
                        pPcie->BusNumber(), pPcie->DeviceNumber(), pPcie->FunctionNumber());
    m_UserScriptArgsMap["$dut_bdf"] = m_DevUnderTestBdf;

    const size_t dutNumber = 0;
    m_CoreMalHandler.get_DUTManager()->addPort(dutNumber, "PORT0", m_DevUnderTestBdf);
    m_CoreMalHandler.get_DUTManager()->getPortByIndex(dutNumber)->setChipID(m_ChipID);
    m_CoreMalHandler.set_Info("proc-time-out-start-seq", std::to_string(m_InitTestSeqRCSTimeoutMs));
    m_CoreMalHandler.set_Info("proc-time-out", std::to_string(m_TestSeqRCSTimeoutMs));

    // Files onto which the result are saved. The Files are located in the
    // current working directory. A time stamp is appended to the file name.
    std::time_t lwrrentTime = std::time(nullptr);
    char timeBuff[50];
    std::strftime(timeBuff, sizeof(timeBuff), "%T", std::localtime(&lwrrentTime));

    if (!m_TestRstDumpFile.empty())
    {
        m_TestRstFileName = m_TestRstDumpFile + "_" + m_IstType + "_result_" + 
            std::string(timeBuff) + ".log";
        if (Xp::DoesFileExist(m_TestRstFileName))
        {
            Xp::EraseFile(m_TestRstFileName);
        }
    }

    if (!m_CtrlStsDumpFile.empty())
    {
        m_CtrlStsRawFileName = m_CtrlStsDumpFile + "_" + m_IstType + "_CtrlSts_Raw_" +
            std::string(timeBuff) + ".log";
        m_CtrlStsFileName = m_CtrlStsDumpFile + "_" + m_IstType + "_CtrlSts_" +
            std::string(timeBuff) + ".log";
        if (Xp::DoesFileExist(m_CtrlStsRawFileName))
        {
             Xp::EraseFile(m_CtrlStsRawFileName);
        }
        if (Xp::DoesFileExist(m_CtrlStsFileName))
        {
            Xp::EraseFile(m_CtrlStsFileName);
        }
    }


    m_State = State::NORMAL_MODE;

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::Setup()
{
    Printf(Tee::PriDebug, "In Setup()\n");

    if (m_TestArgs.empty())
    {
        return RC::SPECIFIC_TEST_NOT_RUN;
    }

    if (m_TestArgs.size() > 1)
    {
        Printf(Tee::PriError,
               "This test lwrrently only supports one test image (%zu provided)\n",
                m_TestArgs.size());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return RC::OK;
}

template<>
RC IstFlow::DumpImageToFile<MemManager::MemDevice::MemAddresses>
(
    const MemManager::MemDevice::MemAddresses &allocatedMemory,
    Dump order
)
{
    Printf(Tee::PriDebug, "In DumpImageToFile()\n");
    RC rc;
    const std::string filename = (order == Dump::PRE_TEST) ? 
                                 m_ImageDumpFile + "_pre.bin" :
                                 m_ImageDumpFile + "_post.bin";

    // TODO: Should we check that the file already exists and warn the user
    FILE *outfile;
    outfile = fopen(filename.c_str(), "wb");
    if (!outfile)
    {
        Printf(Tee::PriError, "Failed to open Image Dump file %s\n",
               filename.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    // Write image into file
    for (unsigned int i = 0; i < allocatedMemory.size(); i++)
    {
        MemDevice memDevice("", allocatedMemory[i].physicalAddress,
                            allocatedMemory[i].sizeBytes, allocatedMemory[i].id);
        const size_t sizeWritten = memDevice.DumpToFile(outfile,
                                   allocatedMemory[i].sizeBytes, 0);
        if (sizeWritten != allocatedMemory[i].sizeBytes)
        {
            Printf(Tee::PriError, "Image file dump error\n");
            return RC::FILE_FAULT;
        }
    }

    fclose(outfile);

    return RC::OK;
}

template<>
RC IstFlow::LoadTestToSysMem<MemManager::MemDevice::MemAddresses>
(
    const std::string& path,
    MemManager::MemDevice::MemAddresses* pMemory
)
{
    Printf(Tee::PriDebug, "In LoadTestToSysMem()\n");
    MASSERT(pMemory);

    MemManager::MemDevice::MemAddresses& memory = *pMemory;
    MemManager::MemDevice::MemRequirements memRequirements = {};
    std::vector<std::string> configFiles;

    {
        RC rc;
        CHECK_RC(GetTotalMemRequirements(
                    &memRequirements,
                    path,
                    configFiles, m_BurstConfig,
                    m_TestProgramsNames));
    }

    if (memRequirements.numBlocks == 0)
    {
        Printf(Tee::PriError, "Failed to get memory requirements for test image.\n");
        return RC::ON_ENTRY_FAILED;
    }
    memRequirements.numBlocks += m_ExtraMemoryBlocks;
    Printf(Tee::PriNormal, "Total number of blocks allocated %u.\n", memRequirements.numBlocks);

    MemManager::Parameters memMgrParams = {};       // Use defaults
    // Tell the memory manager to only use the pre-allocated memory
    memMgrParams._type = MemManager::mgrType::LIMITED;
    // Size of each block of memory
    memMgrParams._mockMemSize = memRequirements.blockSizeBytes;
    // TODO: MAL should set these, not me (values lwrrently taken from their example)
    memMgrParams._mockGap = 0;
    memMgrParams._mockNumDev = 1;

    switch (m_pGpu->GetFamily())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA102) || \
    LWCFG(GLOBAL_GPU_IMPL_GA104) || \
    LWCFG(GLOBAL_GPU_IMPL_GA106) || \
    LWCFG(GLOBAL_GPU_IMPL_GA107)
        case GpuDevice::Ampere:
            memMgrParams._alignment = 32;
            memMgrParams._mlpcMin = 32;
            memMgrParams._mlpcMax = 128;
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        case GpuDevice::Hopper:
            memMgrParams._alignment = 256;
            memMgrParams._mlpcMin = 0;
            memMgrParams._mlpcMax = 0;
            break;
#endif
        default:
            Printf(Tee::PriError, "Unsupported GPU family device\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }


    const UINT64 allocateMemStartTime = Xp::GetWallTimeMS();

    // Pre-allocate all the memory needed by the test image
    // Only reallocate if we don't yet have enough allocated memory
    if (memory.size() < memRequirements.numBlocks ||
        memory[0].sizeBytes < memRequirements.blockSizeBytes)
    {
        // If the chunk sizes are wrong, free all of this memory before
        // we reallocate the correct chunks
        if (!memory.empty() &&
            memory[0].sizeBytes != memRequirements.blockSizeBytes)
        {
            MemManager::MemDevice::Free(&memory);
        }
        // If all we need is more chunks, add to existing ones
        memRequirements.numBlocks -= memory.size();
        const auto& newMem = MemManager::MemDevice::AllocateMemory(memRequirements,
                m_pGpu->GetSubdevice(0),
                m_IOMMUEnabled);
        memory.insert(memory.end(), newMem.begin(), newMem.end());
        // TODO: we might want to sort by address but that doesn't seem to be a requirement for now
    }

    if (memory.empty())
    {
        Printf(Tee::PriError,
               "Failed to allocate memory for test. [blockSize:%zu bytes, numBlocks:%u]\n",
               memRequirements.blockSizeBytes, memRequirements.numBlocks);
        return RC::ON_ENTRY_FAILED;
    }
    memMgrParams._initialMem = memory;

    const UINT64 allocateMemEndTime = Xp::GetWallTimeMS();
    m_TimeStampMap.at("AllocateMemory") = allocateMemEndTime - allocateMemStartTime;

    // MAL uses exceptions for errors so we must surround the code by a try-catch
    std::shared_ptr<MemManager::mem_manager_impl> memoryManager;
    try
    {
        memoryManager = std::make_shared<MemManager::mem_manager_impl>(memMgrParams);
    }
    catch (const std::exception& e)
    {
        Printf(Tee::PriError, "Failed to create a memory manager for MAL. [%s]\n", e.what());
        return RC::ON_ENTRY_FAILED;
    }

    m_CoreMalHandler.set_MemManager(memoryManager);

    const UINT64 imgLoadStartTime = Xp::GetWallTimeMS();
    try
    {
        if (!m_IsATETypeImage)
        {
            auto chunkManager = std::make_shared<image::PacketManager>(memoryManager);
            std::vector<std::shared_ptr<TestProgram_ifc>>pTpVec;

            // See documentation @ https://gitlab-master.lwpu.com/maths/maths/issues/307
            constexpr UINT32 load_mode = 0;
            constexpr UINT32 end_op = 1;
            constexpr UINT32 dump_mode = 0;

            m_TestImageManager =
                image::ImageMgr::image_load(chunkManager,
                    nullptr,    // sram manager: not needed
                    path,
                    "",         // output dir: N/A
                    "",         // log file: no logging
                    -1,         // debug level: no print
                    load_mode,
                    end_op,
                    dump_mode);

            // Add the test program to the STORE interface
            pTpVec.push_back(m_TestImageManager);
            m_CoreMalHandler.get_STORE()->add_TestPrograms(pTpVec);
        }
        else
        {
            MATHS_iofilter::sptr imgLoadFilter =
                std::make_shared<MATHS_iofilter>(image::load_image);
            std::map<std::string, std::map<int,MATHS_iofilter::sptr>> filterPriorities =
            {
                {"CFG/IMG2", {{80, imgLoadFilter}}},
                {"CFG/IMG4", {{80, imgLoadFilter}}}
            };

            // Load all the config files
            for (UINT32 i = 0; i < configFiles.size(); i++)
            {
                std::string localPath = path;
                std::string dirName = dirname(const_cast<char*>(localPath.c_str()));

                std::map<std::string,std::string> paramsIn =
                {
                    {"imagepath", dirName + "/" + configFiles[i]},   // image path
                    {"outdir", dirName},                             // Output directory
                    {"logile", ""},
                    {"debug", "-1"},
                    {"load_mode", "0"},
                    {"EndOperation", "0"},
                    {"production", "0"},
                    {"dumpMode", "0"},
                    {"memory_comments", "0"},
                    {"useSRAM", "NO"}
                };

                std::map<std::string,std::string> returnsOut;
                MATHS_DUT_Manager_ns::DUT_Config_ifc::sptr dutCfg =  nullptr;

                CFG_Filter_ns::load_image(&m_CoreMalHandler, dutCfg, filterPriorities,
                        paramsIn, returnsOut);
            }
        }
    }
    catch (const std::exception& e)
    {
        Printf(Tee::PriError, "Failed loading image. [%s]\n", e.what());
        return RC::ON_ENTRY_FAILED;
    }
    const UINT64 imgLoadEndTime = Xp::GetWallTimeMS();
    m_TimeStampMap.at("ImageLoad") = imgLoadEndTime - imgLoadStartTime;
    
    Platform::FlushCpuWriteCombineBuffer();

    return RC::OK;
}

/**
 * \brief When using a user provided LWPEX script, this function 
 *        allows setting the proper program flow within the MAL.
 *        Ultimately, the script is transcoded into transactions
 *        exelwted by MAL exelwtion. This transactions are exelwted
 *        using MODS pcie config space functions (i.e ModsDrvPciRd32, etc.)
 */
RC IstFlow::SetMALProgramFlowToLWPEX()
{
    Printf(Tee::PriDebug, "In SetMALProgramFlowToLWPEX\n");
    MATHS_STORE_ifc::sptr storeHandler;
    const int revision = 0;
    const std::string program = "user_script";
    const std::string flowEntry = "mods_user";
    m_CoreMalHandler.get_STORE(storeHandler);
    std::shared_ptr<MATHS_store_ns::ChipInfoManager> chipInfoMgr =
        storeHandler->createChipInfoMgr(m_ChipID);
    if (chipInfoMgr == NULL)
    {
        Printf(Tee::PriError, "Creating ChipManager failed\n");
        return RC::SOFTWARE_ERROR;
    }
    // This function expects the user provided arg to be of the forms
    // -pre_ist_script_war "sudo /path_to_script" or
    // -pre_ist_script_war "/path_to_script"
    vector<string> tokens;
    Utility::Tokenizer(m_PreIstScript, " ", &tokens);
    std::ifstream ifs;
    switch(tokens.size())
    {
        case 1:
            ifs.open(tokens[0], std::ios::in);
            break;
        case 2:
            ifs.open(tokens[1], std::ios::in);
            break;
        default:
            Printf(Tee::PriError, "Can't set LWPEX program flow in MAL due to"
                    " invalid user script argument\n");
            return RC::BAD_PARAMETER;
    }

    MATHS_LWPEX2_ns::MFN_COMMANDS commands;
    MFN_Programs::scriptToMFN_COMMANDS(commands, ifs);
    if (commands.empty())
    {
        Printf(Tee::PriError, "Failed to decode LWPEX script\n");
        return RC::SOFTWARE_ERROR;
    }
    chipInfoMgr->programs().addProgram(m_ChipID, revision, program, commands);
    std::vector<std::string> flow = {program};
    chipInfoMgr->programs().setProgramFlow(m_ChipID, revision, flowEntry, flow);
    m_CoreMalHandler.set_Info("RunMode", flowEntry);
    m_MALProgramFlowToLWPEX = true;

    return RC::OK;
}

RC IstFlow::DecryptImage1Yaml()
{
    Printf(Tee::PriDebug, "In DecryptImage1Yaml()\n");
    if (m_Img1RootNode["chip_info"])
    {
        // Image1 already decrypted;
        return RC::OK;
    }
    const string& img1Path = Utility::RestoreString(m_Img1Path);
    if (!Xp::DoesFileExist(img1Path))
    {
        Printf(Tee::PriError, "Image1 '%s' not found\n", img1Path.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    std::fstream ifs(img1Path.c_str(), std::ios::in | std::ios::binary);
    const bool isDecrypted = decryptYaml(m_Img1RootNode, ifs);

    if (!isDecrypted || !(m_Img1RootNode["chip_info"]))
    {
        Printf(Tee::PriError, "Decryption of Image1 failed\n");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC IstFlow::ProgramJtags
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    bool isCvbMode
)
{
    Printf(Tee::PriDebug, "In ProgramJtags()\n");
    const size_t dutNumber = 0;
    const unsigned int testNumber = 0;
    std::string msg;
    ReqStatus ioBlockStatus;
    std::shared_ptr<MATHS_IOBlock_ifc> ioHandler;
    const std::string labelStr = testProgram->get_Name(dutNumber);
    const std::string flowStr = testProgram->get_TestFlow(dutNumber)[testNumber]->get_Name();
    m_CoreMalHandler.get_IOBlock(ioHandler);

    std::vector<JtagProgArgs> jtagProgInfos(m_JtagProgInfos);
    if (isCvbMode)
    {
        jtagProgInfos.clear();
        // Program the gpc pll to 2 * fmax calc
        jtagProgInfos = m_CvbMnpTagProgInfos;
    }
    for (const auto &jtagProgInfo: jtagProgInfos)
    {
        MATHSerialization::ImageJtagTag jtagTag;
        const std::map<std::string, std::string> mm =
        {
            {"label", labelStr},
            {"test_flow", flowStr},
            {"JtagTag", "Write"},
            {"dut_id", std::to_string(dutNumber)}
        };
        // Retrieve the JTAG for the specfic testprogram and testflow
        ioHandler->REQ_GetImageJtagTags(mm, jtagTag, ioBlockStatus, msg);
        if (ioBlockStatus == ReqStatus::Failed || ioBlockStatus == ReqStatus::Refused)
        {
            Printf(Tee::PriError, "Failed to fetch the image tags\n");
            return RC::SOFTWARE_ERROR;
        }

        std::map<std::string, std::string> umm;
        umm["tag"] = jtagProgInfo.jtagName;  // Get the TAG string
        umm["value"] = MATHSerialization::toBinaryString(jtagProgInfo.value, jtagProgInfo.width);
        MATHSerialization::ImageJtagTag targetTag;
        if (jtagProgInfo.jtagType == "img")
        {
            // An image tag
            targetTag = jtagTag.searchTagByName(umm);
        }
        else if (jtagProgInfo.jtagType == "pll")
        {
            // A Pll tag
            targetTag = jtagTag.getEditedPllTag(umm);
        }
        else
        {
            Printf(Tee::PriError, "Incorrect Jtag type specified %s\n"
                    "The correct types are pll or img\n", jtagProgInfo.jtagType.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        if (targetTag.getSeqTags().size() == 0)
        {
            Printf(Tee::PriError, "Incorrect tag, could not be retrieved %s\n",
                        jtagProgInfo.jtagName.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        ReqStatus uIoBlockStatus;
        ioHandler->REQ_UpdateImageJtagTags(mm, targetTag, uIoBlockStatus, msg);
        if (uIoBlockStatus == ReqStatus::Failed || uIoBlockStatus == ReqStatus::Refused)
        {
            Printf(Tee::PriError, "Failed to update the image tags\n");
            return RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriNormal,
                "Programming Jtag, Type: %s, Name: %s, Width: %u, Value: %u\n", 
                jtagProgInfo.jtagType.c_str(), jtagProgInfo.jtagName.c_str(),
                jtagProgInfo.width, jtagProgInfo.value);
    }

    return RC::OK;
}

RC IstFlow::ProgramPllJtags
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    bool isCvbMode,
    bool isShmooOp
)
{
    Printf(Tee::PriDebug, "In ProgramPllJtags()\n");
    RC rc;
    const size_t dutNumber = 0;
    const unsigned int testNumber = 0;
    std::string msg;
    ReqStatus ioBlockStatus;
    std::shared_ptr<MATHS_IOBlock_ifc> ioHandler;
    const std::string labelStr = testProgram->get_Name(dutNumber);
    const std::string flowStr = testProgram->get_TestFlow(dutNumber)[testNumber]->get_Name();
    m_CoreMalHandler.get_IOBlock(ioHandler);
    std::shared_ptr<FrequencyTable> freqTable;
    auto numConf = testProgram->get_DUTConfigNum();
    auto dutCfg = m_CoreMalHandler.get_DUTManager()->getDUTConfig(numConf, m_ChipID);

    if (m_IsATETypeImage)
    {
        freqTable = make_shared<FrequencyTable>(dutCfg->getFrequencyTable());
    }
    else
    {
        CHECK_RC(DecryptImage1Yaml());
        freqTable = make_shared<FrequencyTable>(m_Img1RootNode);
    }
    std::vector<PllProgArgs> pllProgInfos(m_PllProgInfos);
    if (isCvbMode && m_CvbTargetFreqMhz != UINT_MAX)
    {
        pllProgInfos.clear();
        // Program the gpc pll to 2 * fmax calc
        pllProgInfos.push_back({"gpcpll", 2 * m_CvbTargetFreqMhz});
    }

    // When a shmoo operation, resetting the pllProgInfos.
    // Static pll programming is done before shmooing.
    if (isShmooOp)
    {
        pllProgInfos.clear();
        pllProgInfos.push_back({m_PllTargetName, m_PllTargetFrequencyMhz});
    }
    for (const auto &pllProgInfo: pllProgInfos)
    {
        MATHSerialization::ImageJtagTag jtagTag;
        // Retreive the map of TAG and value given the pllName and frequency
        const std::map<std::string, int> tagInfo = 
            freqTable->getPllProgramming(pllProgInfo.pllName, pllProgInfo.pllFrequencyMhz);
        if (tagInfo.size() == 0)
        {
            Printf(Tee::PriError, "Invalid Pll name: %s or Frequency: %u specified\n",
                    pllProgInfo.pllName.c_str(), pllProgInfo.pllFrequencyMhz);
            return RC::BAD_PARAMETER;
        }
        const std::map<std::string, std::string> mm =
        {
            {"label", labelStr},
            {"test_flow", flowStr},
            {"JtagTag", "Write"},
            {"dut_id", std::to_string(dutNumber)}
        };
        // Retrieve the JTAG for the specfic testprogram and testflow
        ioHandler->REQ_GetImageJtagTags(mm, jtagTag, ioBlockStatus, msg);
        if (ioBlockStatus == ReqStatus::Failed || ioBlockStatus == ReqStatus::Refused)
        {
            Printf(Tee::PriError, "Failed to fetch the pll tags\n");
            return RC::SOFTWARE_ERROR;
        }

        for (const auto &pp: tagInfo)
        {
            std::map<std::string, std::string> umm;
            umm["tag"] = pp.first;  // Get the TAG string
            umm["value"] = MATHSerialization::toBinaryString(pp.second,
                freqTable->getRegWidth(pllProgInfo.pllName, pp.first)); // Register Value
            MATHSerialization::ImageJtagTag targetTag;
            targetTag = jtagTag.getEditedPllTag(umm);
            if (targetTag.getSeqTags().size() == 0)
            {
                Printf(Tee::PriError, "Could not retrieve tag %s info for pll %s\n",
                        pp.first.c_str(), pllProgInfo.pllName.c_str());
                return RC::SOFTWARE_ERROR;
            }
            ReqStatus uIoBlockStatus;
            ioHandler->REQ_UpdateImageJtagTags(mm, targetTag, uIoBlockStatus, msg);
            if (uIoBlockStatus == ReqStatus::Failed || uIoBlockStatus == ReqStatus::Refused)
            {
                Printf(Tee::PriError, "Failed to update the pll tags\n");
                return RC::SOFTWARE_ERROR;
            }
        }
        Printf(Tee::PriNormal, "Programming pll, Name: %s, Frequency: %u Mhz\n",
                pllProgInfo.pllName.c_str(), pllProgInfo.pllFrequencyMhz);
    }

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::ProgramVoltageJtags
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    bool isSmhooOp
)
{
    // This function is only application on GH100.
#if LWCFG(GLOBAL_GPU_IMPL_GH100)    
    Printf(Tee::PriDebug, "In ProgramVoltageJtags()\n");
    RC rc;
    const size_t dutNumber = 0;
    const unsigned int testNumber = 0;
    std::string msg;
    ReqStatus ioBlockStatus;
    std::shared_ptr<MATHS_IOBlock_ifc> ioHandler;
    const std::string labelStr = testProgram->get_Name(dutNumber);
    const std::string flowStr = testProgram->get_TestFlow(dutNumber)[testNumber]->get_Name();
    m_CoreMalHandler.get_IOBlock(ioHandler);
    auto numConf = testProgram->get_DUTConfigNum();
    auto dutCfg = m_CoreMalHandler.get_DUTManager()->getDUTConfig(numConf, m_ChipID);
    std::shared_ptr<VoltageTable> voltTable;

    // When using the MAL config filter, the voltage table is constructed directly
    // from IMG1. Else, the user is to supply the path to Image
    if (m_IsATETypeImage)
    {
        voltTable = make_shared<VoltageTable>(dutCfg->getVoltageTable());
    }
    else
    {
        CHECK_RC(DecryptImage1Yaml());
        voltTable = make_shared<VoltageTable>(m_Img1RootNode);
    }
    
    if (isSmhooOp)
    {
        m_VoltageProgInfos.clear();
        m_VoltageProgInfos.push_back({m_VoltageDomain, m_VoltageTargetMv});
    }

    for (const auto &voltageProgInfo : m_VoltageProgInfos)
    {
        MATHSerialization::ImageJtagTag jtagTag;

        std::map<std::string, std::string> mm = {
            {"label", labelStr},
            {"test_flow", flowStr},
            {"JtagTag", "Write"},
            {"dut_id", std::to_string(dutNumber)}
        };
        ioHandler->REQ_GetImageJtagTags(mm, jtagTag, ioBlockStatus, msg);
        if (ioBlockStatus == ReqStatus::Failed || ioBlockStatus == ReqStatus::Refused)
        {
            Printf(Tee::PriError, "Failed to fetch the pll tags\n");
            return RC::SOFTWARE_ERROR;
        }

         // Retrieve the Jtag and corresponding values
         // CAT3_t00ghapad0gpiob0_98jtag_val_dft_sci_vid_cfg1_0_hi, 50
         // CAT3_t00ghapad0gpiob0_98jtag_val_dft_sci_vid_cfg0_0_period, 90

         std::map<std::string, uint64_t> voltProgram = 
            voltTable->getProgrammingInfo(voltageProgInfo.domain, voltageProgInfo.valueMv);
         
         // Iterate through the tags and program them
         for (auto const &pp: voltProgram)
         {
             std::map<std::string, std::string> umm;
             umm["tag"] = pp.first;
             umm["value"] = MATHSerialization::toBinaryString(pp.second,
                    voltTable->getRegWidth(voltageProgInfo.domain, pp.first));
             MATHSerialization::ImageJtagTag targetTag;
             targetTag = jtagTag.getEditedVoltageTag(umm);
             ReqStatus uIoBlockStatus;
             ioHandler->REQ_UpdateImageJtagTags(mm, targetTag, uIoBlockStatus, msg);
             if (uIoBlockStatus == ReqStatus::Failed || uIoBlockStatus == ReqStatus::Refused)
             {
                  Printf(Tee::PriError, "Failed to update the Voltage pll tags\n");
                  return RC::SOFTWARE_ERROR;
             }
         }
    }
#endif
     return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::ProgramAllTags(const shared_ptr<TestProgram_ifc> &testProgram)
{
    RC rc;
    // Programming of the tags is done on GA10x and Hopper
    if (m_IsGa10x || m_ChipID == "gh100")
    {
        // If applicable, program the specified plls.
        if (m_PllProgInfos.size())
        {
            CHECK_RC(ProgramPllJtags(testProgram));
        }
        if (m_JtagProgInfos.size())
        {
            CHECK_RC(ProgramJtags(testProgram));
        }

        if (m_VoltageProgInfos.size() && m_ChipID == "gh100")
        {
            CHECK_RC(ProgramVoltageJtags(testProgram));
        }

        if (m_CvbInfo.testPattern.size())
        {
            if (m_CvbTargetFreqMhz > 2100)
            {
                CHECK_RC(ProgramJtags(testProgram, true));
            }
            else
            {
                CHECK_RC(ProgramPllJtags(testProgram, true));
            }
        }
    }

    return RC::OK;
}

//----------------------------------------------------------------------
RC IstFlow::InitGpu(bool bIstMode)
{
    Printf(Tee::PriDebug, "In InitGpu()\n");
    RC rc;
    const Init::InitGpuArgs initGpuArgs =
    {
        bIstMode,
        m_FbBroken,
        !m_PreIstScript.empty() || !m_UserScript.empty() || m_IsATETypeImage,
        static_cast<UINT64>(m_TestTimeoutMs)
    };

    CHECK_RC(Init::InitGpu(&m_pGpu, initGpuArgs, m_FsArgs));
    if (m_RebootWithZpi && m_ZpiScript.empty() && m_ChipID != "gh100")
    {
        CHECK_RC(m_ZpiImpl->ReInitializePciInterface(m_pGpu->GetSubdevice(0)));
    }

    return rc;
}

/*static*/ RC IstFlow::ReleaseGpu()
{
    Printf(Tee::PriDebug, "In ReleaseGpu()\n");
    // GpuPtr is the access point to the Gpu singleton
    return GpuPtr()->ShutDown(Gpu::ShutDownMode::Normal);
}

//--------------------------------------------------------------------
RC IstFlow::ResetIntoIstMode()
{
    RC rc;
    Printf(Tee::PriDebug, "In ResetIntoIstMode()\n");

    if (m_State == State::IST_MODE)
    {
        return rc;
    }

    SW_ERROR((m_State != State::NORMAL_MODE),
             "Cannot reset into IST mode if not in normal mode\n");

    m_State = State::UNDEFINED;   // In case something fails

    SW_ERROR((!m_pGpu),
             "If m_State is NORMAL_MODE, m_pGpu must be set");

    // Set register to tell VBIOS to enter IST mode after reset and assert hot reset
    // We do so when the pre_ist_war argument is not used.
    if (m_PreIstScript.empty() && m_UserScript.empty() && !m_IsATETypeImage)
    {
        // Write bit 30 of LW_PGC6_SCI_LW_SCRATCH
        RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();
        regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_ENABLE_TRUE);
        const UINT32 voltRef = 400;

        if (m_IstType == "stuck-at")
        {
            Printf(Tee::PriDebug, "\n---------------------\n");
            Printf(Tee::PriDebug, "Stuck-at IST - MOODE 0 \n");
            Printf(Tee::PriDebug, "-----------------------\n");

            regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_IST_TYPE_STUCK_AT);
   
            if (m_PreVoltageOverride)
            {
                // MODE2: Logical Voltage Override
                // MODS specifies a logical voltage value, then VBIOS
                // translate it into PWM settings, and set the voltage
                const UINT32 voltageToVbios =
                            (int) ((m_VoltageOverrideMv - voltRef)/m_StepIncrementOffsetMv);

                Printf(Tee::PriDebug, "\n---------------------\n");
                Printf(Tee::PriDebug, "Stuck-at IST - MODS 2:\n"
                        "Setting GPU to %.1f mV\n", m_VoltageOverrideMv);
                Printf(Tee::PriDebug, "-----------------------\n");

                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_VOLTAGE_MODE_MODE2);
                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_MODE2_VOLTAGE_OFFSET,
                        voltageToVbios);
            }
            else
            {
                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_VOLTAGE_MODE_MODE0);
            }
        }
        else if (m_IstType == "predictive")
        {   
            regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_IST_TYPE_PREDICTIVE);

            if (m_PreVoltageOverride)
            {   
                // MODE2: Logical Voltage Override
                // MODS specifies a logical voltage value, then VBIOS
                // translate it into PWM settings, and set the voltage
                const UINT32 voltageToVbios =
                          (int)((m_VoltageOverrideMv - voltRef)/m_StepIncrementOffsetMv);
                SW_ERROR(m_PreVoltageOffset,
                        "Voltage Offset should not be set in override mode\n");

                Printf(Tee::PriDebug, "\n---------------------\n");
                Printf(Tee::PriDebug, "Predictive IST - MODS 2:\n"
                        "Setting GPU to %.1f mV\n", m_VoltageOverrideMv);
                Printf(Tee::PriDebug, "-----------------------\n");

                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_VOLTAGE_MODE_MODE2);
                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_MODE2_VOLTAGE_OFFSET,
                        voltageToVbios);
            }
            else if (m_PreVoltageOffset)
            {
                INT32 offsetToVbios = int(m_VoltageOffsetMv/m_StepIncrementOffsetMv);
                SW_ERROR(m_PreVoltageOverride,
                        "Voltage Override should not be set in offset mode\n");

                Printf(Tee::PriDebug, "\n---------------------\n");
                Printf(Tee::PriDebug, "Predictive IST - MODS 3:\n"
                        "Adding an Offset of %.1f mV\n", m_VoltageOffsetMv);
                Printf(Tee::PriDebug, "-----------------------\n");

                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_MODE3_STEP_SIZE_SIGN,
                        offsetToVbios >= 0 ? 0x00: 0x01);
                // Get the offsetToVbios absolute value 
                offsetToVbios = offsetToVbios >= 0 ? offsetToVbios : -offsetToVbios;
                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_VOLTAGE_MODE_MODE3);
                regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_MODE3_VOLTAGE_STEP_SIZE,
                        offsetToVbios);
            }
            else
            {   
                // MODE not supported
                return RC::BAD_PARAMETER;
            }
        }
    }

    m_pGpu->GetSubdevice(0)->HotReset();
    CHECK_RC(ReleaseGpu());

    // Re-enumerate devices, map BAR0, etc
    constexpr bool isIstMode = true;
    // If InitGpu fails, then the target voltage can't be updated. Just use -1 to indicate
    // the invalid voltage
    rc = InitGpu(isIstMode);
    if (rc != RC::OK)
    {
        m_TargetVoltage = -1;
        return rc;
    }

    // TODO: Disable Link Low Power State

    // TODO: Mask Interrupts
    if (m_PreIstScript.empty() && m_UserScript.empty() && !m_IsATETypeImage)
    {
        /**
         * Get GPU voltage is set at after IST devinit. MODS_PGC6_SCI_VID_CFG1_0 is valid when
         * the PreIstScript is not used. Formula from
         * https://confluence.lwpu.com/pages/viewpage.action?pageId=191959890#T194/Tu104ISTMODSandScriptSetup-ConfigureTu104ISTVoltage
         *
         * Vtarget = (Vmax - Vmin) * PWM_HI/PWM_Period + Vmin
         *         = (1300mV-300mV) * PWM_HI/40 + 300mV
         *         =  PWM_HI * 25 + 300mV
         */

        UINT32 pwmHigh = m_pGpu->GetSubdevice(0)->Regs().Read32(MODS_PGC6_SCI_VID_CFG1_0);
        pwmHigh = pwmHigh & 0xFFF;
        UINT32 voltageMin = (int)m_StepIncrementOffsetMv == 25 ? 300: 400;
        // Add 100mv to Vmin if step size is 25mv and it's pg199 board
        voltageMin = (m_IsPG199Board && (int)m_StepIncrementOffsetMv == 25) ? voltageMin + 100 : voltageMin;

        m_TargetVoltage = m_StepIncrementOffsetMv * pwmHigh + voltageMin;
        Printf(Tee::PriDebug, "PWM Register: %u -- Target Voltage: %.1f mV\n",
                pwmHigh, m_TargetVoltage);

        if (m_PreVoltageOverride)
        {
            // Fail if the gap between the VBIOS reported voltage and the user's target voltage
            // is greater than a step size.
            if (fabs(m_TargetVoltage - m_VoltageOverrideMv) > m_StepIncrementOffsetMv)
            {
                Printf(Tee::PriError, "Target Voltage %.1fmv and Programmed Voltage %.1fmv "
                        "gap is greater than %.1fmv\n\n",
                        m_VoltageOverrideMv, m_TargetVoltage, m_StepIncrementOffsetMv);
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    if (m_SeleneSystem)
    {
        if (m_RebootWithZpi)
        {
            CHECK_RC(m_ZpiLock.AcquireLockFile());
            {
                DEFER
                {
                    m_ZpiLock.ReleaseLockFile();
                };
                // Get temp from BMC, can only get positive temperature
                // The range is 0 - 255 C degree (UINT08)
                UINT08 tempViaBMC = 0;
                m_ZpiImpl->GetGPUTemperatureThroughBMC(&tempViaBMC);
                m_GpuTemp = tempViaBMC;
            }
        }
    }
    else
    {
        // Get the gpu temperature from SMBus
        if (m_SmbusInitialized)
        {
            SW_ERROR((!m_SmbusImpl.get()), "IstFlow: SmBus not exist\n");
            rc = m_SmbusImpl->ReadTemp(&m_GpuTemp, m_NumRetryReadTemp);
            if (rc != RC::OK)
            {
                Printf(Tee::PriWarn, "Can not read temperature.\n");
                // reset the temperature to an invalid value
                m_GpuTemp = INT_MIN;
                rc.Clear();
            }
        }
    }
    m_State = State::IST_MODE;
    return rc;
}

//--------------------------------------------------------------------
RC IstFlow::RebootIntoNormalMode()
{
    Printf(Tee::PriNormal, "In RebootIntoNormalMode()\n");
    //if (!m_RebootWithZpi && m_ChipID != "gh100")
    if (!m_RebootWithZpi)
    {
        Printf(Tee::PriDebug, "Skipping RebootIntoNormalMode()\n");
        return RC::OK;
    }

    RC rc;
    Printf(Tee::PriDebug, "In RebootIntoNormalMode()\n");
    if (m_ChipID != "gh100")
    {
        CHECK_RC(m_ZpiLock.AcquireLockFile());
        {
            DEFER {m_ZpiLock.ReleaseLockFile();};
            if (m_ZpiScript.empty())
            {
                // Use MODS ZPI interface to reset GPU
                CHECK_RC(ZpiPowerCycle());
            }
            else
            {
                // External script to reset the GPU. Used for PG506 boards.
                // Op team uses an external script with its own arguments.
                const string& zpiCommand = Utility::RestoreString(m_ZpiScript);
                string cmd = Utility::StrPrintf("%s", zpiCommand.c_str());
                Printf(Tee::PriNormal, "Running ZPI script %s\n", cmd.c_str());
                const int res = std::system(cmd.c_str());
                if (res != 0)
                {
                    Printf(Tee::PriError, "ZPI script '%s' failed [return value: %d]\n",
                            zpiCommand.c_str(), res);
                    return RC::SCRIPT_FAILED_TO_EXELWTE;
                }
            }
        }
    }
    else
    {
        m_pGpu->GetSubdevice(0)->HotReset();
    }

    CHECK_RC(ReleaseGpu());
    // Re-enumerate devices, map BAR0, etc
    constexpr bool isIstMode = false;
    CHECK_RC(InitGpu(isIstMode));
    m_State = State::NORMAL_MODE;
    return rc;
}


//--------------------------------------------------------------------
RC IstFlow::ZpiPowerCycle()
{
    RC rc;

    SW_ERROR((!m_ZpiImpl.get()), "IstFlow: Zpi not initialized\n");

    Printf(Tee::PriDebug, "In ZpiPowerCycle()\n");

    bool gpuOnTheBus = false;
    CHECK_RC(m_ZpiImpl->IsGpuOnTheBus(m_pGpu->GetSubdevice(0), &gpuOnTheBus));
    if (!gpuOnTheBus)
    {
        Printf(Tee::PriError, "GPU already Dropped off PCI Bus. Can not enter ZPI\n");
        return RC::DEVICE_NOT_FOUND;
    }
    CHECK_RC(m_ZpiImpl->EnterZPI(m_DelayAfterPwrDisableMs * 1000));
    CHECK_RC(m_ZpiImpl->ExitZPI());

    return rc;
}

/**
 * \brief Main IST loop for Hopper
 *        TODO: find a way to unify both RunLoop
 */
RC IstFlow::RunLoopHopper
(
    const uint16_t numLoops,
    const string& path,
    bool* pTestWasRun,
    const std::vector<TestNumber>& testNumbers
)
{
    Printf(Tee::PriDebug, "In RunLoopHopper()\n");
    RC rc;
    INT32 voltIterations = 1;
    INT32 freqIterations = 1;
    UINT32 voltStart = 0;
    UINT32 freqStart = 0;
    UINT32 voltStep = 0;
    UINT32 freqStep = 0;
    bool voltUpSchmoo = true;   // By default, we schmoo up
    bool freqUpSchmoo = true;

    SW_ERROR(testNumbers.size() != 1, "Only a single test sequence is to run\n");
    const UINT32 testNumber = testNumbers[0];
    const size_t dutNumber = 0;

    // NOTE: At this time there is a hard requirement to support only programming
    // of 1 voltage and pll domain. However the paramaters are stored in arrays
    // to account for expension.
    // TODO: Add support for voltage override.
    if (m_VoltageSchmooInfos.size())
    {
        SW_ERROR(m_VoltageSchmooInfos.size() != 1,
                "Only one voltage domain programming is supperted\n");
        voltIterations = (m_VoltageSchmooInfos[0].endValue - 
                m_VoltageSchmooInfos[0].startValue) / m_VoltageSchmooInfos[0].step;
        voltUpSchmoo = (voltIterations >= 0);
        m_VoltageDomain = m_VoltageSchmooInfos[0].domain;
        voltStart = m_VoltageSchmooInfos[0].startValue;
        voltIterations = (voltIterations >= 0) ? voltIterations : -voltIterations;
        SW_ERROR(voltIterations < 0, "Voltage iterations has to be greater than 0\n");
    }

    if (m_FrequencySchmooInfos.size())
    {
        SW_ERROR(m_FrequencySchmooInfos.size() != 1,
                "Only one frequency domain programming is supported\n");
        freqIterations =  (m_FrequencySchmooInfos[0].endValue -
                m_FrequencySchmooInfos[0].startValue) / m_FrequencySchmooInfos[0].step;
        freqUpSchmoo = (freqIterations >= 0);
        m_PllTargetName = m_FrequencySchmooInfos[0].pllName;
        freqStart = m_FrequencySchmooInfos[0].startValue;
        freqIterations = (freqIterations >= 0) ? freqIterations : -freqIterations;
        SW_ERROR(freqIterations < 0, "Frequency iterations has to be greater than 0\n");
    }
    // On GH100, we only need to reset in IST mode once.
    CHECK_RC(ResetIntoIstMode());

    CHECK_RC(RunJTAGCommonProgram());
    CHECK_RC(CreateBurstImage());
    CHECK_RC(GetExelwtableTestProg());

    for (UINT32 i = 0; i < m_ExecTestProgs.size(); i++)
    {
        auto testProgram = m_ExecTestProgs[i];
        std::string testProgramName = testProgram->get_Name(dutNumber);

        CHECK_RC(FilterMbistResult(testProgram, path));
        CHECK_RC(ProgramAllTags(testProgram));

        for (INT32 voltIdx = 0; voltIdx <  voltIterations; voltIdx++)
        {
            INT32 voltageOffset = voltIdx * voltStep;
            voltageOffset = voltUpSchmoo ? voltageOffset : -voltageOffset;
            m_VoltageTargetMv = voltStart + voltageOffset;
            if (m_VoltageTargetMv != 0)
            {
                ProgramVoltageJtags(testProgram, true);
            }

            for (INT32 freqIdx = 0; freqIdx < freqIterations; freqIdx++)
            {
                INT32 freqOffset = freqIdx * freqStep;
                freqOffset = freqUpSchmoo ? freqOffset : -freqOffset;
                m_PllTargetFrequencyMhz = freqStart + freqOffset;
                if (m_PllTargetFrequencyMhz != 0)
                {
                    ProgramPllJtags(testProgram, false, true);
                }

                for (UINT32 repIndex = 1; repIndex < m_Repeat + 1; repIndex++)
                {
                    boost::posix_time::ptime exeTime;
                    CHECK_RC(ResetTestProgram(testProgram));
                    const UINT64 testStartTime = Xp::GetWallTimeMS();
                    RC rc1 = RunTest(testProgram, testNumber, exeTime);
                    const UINT64 testEndTime = Xp::GetWallTimeMS();
                    m_TimeStampMap.at("ImageTestRun") = testEndTime - testStartTime;
                    *pTestWasRun = true;
                    CHECK_RC(CheckControllerStatus(testProgram, testNumber, m_VoltageTargetMv,
                                repIndex));
                    CHECK_RC(DumpResultToFile(testProgram, testNumber, m_VoltageTargetMv,
                                repIndex));
                    if (rc1 != RC::OK)
                    {
                        m_PerTestProgramResults[testProgramName].emplace_back(rc1,
                                repIndex, m_VoltageTargetMv, m_PllTargetFrequencyMhz, m_GpuTemp);
                        Printf(Tee::PriNormal,
                                "Voltage: %d mV -- Frequency: %d Mhz -- Iteration: %d"
                                " -- Temperature: %d deg C\n",
                                m_VoltageTargetMv, m_PllTargetFrequencyMhz, repIndex, m_GpuTemp);
                        Printf(Tee::PriNormal, "Error Code = %08u (%s)\n", rc1.Get(), rc1.Message());
                        rc1.Clear();
                        // TODO: look into random failure to reboot into normal
                        // when RunTest() fails. Seems VBIOS related
                        CHECK_RC(RebootIntoNormalMode());
                        continue;
                    }
                    rc = ProcessTestResults(testProgram, testNumber, exeTime);
                    m_PerTestProgramResults[testProgramName].emplace_back(rc,
                            repIndex, m_VoltageTargetMv, m_PllTargetFrequencyMhz, m_GpuTemp);
                    Printf(Tee::PriNormal,
                            "Voltage: %d mV -- Frequency: %d Mhz -- Iteration: %d"
                            " -- Temperature: %d deg C\n",
                            m_VoltageTargetMv, m_PllTargetFrequencyMhz, repIndex, m_GpuTemp);
                    Printf(Tee::PriNormal, "Error Code = %08u (%s)\n", rc.Get(), rc.Message());
                    rc.Clear();
                    CHECK_RC(PrintTimeStampTable());
                    CHECK_RC(SaveTestProgramResult(testProgram, exeTime, repIndex));
                }
            }

        }
     }

    CHECK_RC(RunJTAGShutdownProgram());
    CHECK_RC(PollOnShutdownRegister(testNumber, m_TestTimeoutMs));
    // TODO: Should we issue a reboot in normal mode here. Most likely
    CHECK_RC(RebootIntoNormalMode());

    return RC::OK;
}


/**
 * \brief Main IST loop for Ampere family 
 */
RC IstFlow::RunLoopAmpere
(
    const uint16_t numLoops,
    const string& path,
    bool* pTestWasRun,
    const std::vector<TestNumber>& testNumbers
)
{
    Printf(Tee::PriDebug, "In RunLoopAmpere()\n");
    RC rc;
    const size_t testProgramNum = 0;
    auto testPrograms = m_CoreMalHandler.get_STORE()->get_TestPrograms();
    const size_t dutNumber = 0;

    SW_ERROR(testPrograms.size() != 1, "Only one test program is supported in Ampere\n");
    auto testProgram = testPrograms[testProgramNum];
    std::string testProgramName = testProgram->get_Name(dutNumber);

    CHECK_RC(FilterMbistResult(testProgram, path));
    CHECK_RC(ProgramAllTags(testProgram));

    for (UINT32 loop = 0; loop < numLoops; ++loop)
    {
        SW_ERROR(testNumbers.size() != 1, "Only a single test sequence is to run\n");

        for (const TestNumber& testNumber : testNumbers)
        {
            // Iterate from -m_VoltageOffsetMv to +m_VoltageOffsetMv in step of 25mv
            // when  max_offset_mv is set. Each test with m_VoltageOffsetMv setting is
            // repeated a number of times (set by user in command line)
            // When max_offset_mv not set the test is run once
            // i.e voltIterations = m_Repeat = 1
            // When -m_OffsetShmooDown is set, we want to start from high voltage
            // and go to the lowest
            UINT32 voltIterations = 1;
            SW_ERROR(m_PreVoltageOffset && m_PreVoltageOverride, 
                    "Voltage Offset and Voltage override can't both be set.");
            if (m_PreVoltageOffset)
            {
                voltIterations = 2 * (m_VoltageOffsetMv / m_StepIncrementOffsetMv) + 1;
                m_VoltageOffsetMv = (m_OffsetShmooDown) ? m_VoltageOffsetMv : -m_VoltageOffsetMv;
            }
            
            // If VoltageOverrideMv is set, iterate through the multiple voltage values
            else if (m_PreVoltageOverride)
            {
                voltIterations = m_VoltageOverrideMvAry.size();
            }

            for (UINT32 voltIndex = 0; voltIndex < voltIterations; voltIndex++)
            {   
                if (m_PreVoltageOverride)
                {
                    m_VoltageOverrideMv = m_VoltageOverrideMvAry[voltIndex];
                }

                for (UINT32 repIndex = 1; repIndex < m_Repeat + 1; repIndex++)
                {   
                    boost::posix_time::ptime exeTime;
                    rc = PreRunTest(*pTestWasRun);
                    if (rc != RC::OK)
                    {
                        // PreRuntest fails, mostly due to a failure to properly reset
                        // in IST mode. Record test result then reset GPU in normal mode
                        // and try the next iteration.
                        m_PerTestProgramResults[testProgramName].emplace_back(rc,
                                repIndex, m_TargetVoltage, 0, m_GpuTemp);
                        if (m_PreIstScript.empty() && m_UserScript.empty() && !m_IsATETypeImage)
                        {
                            // Print info when IST devinit done by VBIOS
                            Printf(Tee::PriNormal, "Voltage: %.1f mV -- Iteration: %d"
                                    " -- Temperature: %d deg C\n",
                                    m_TargetVoltage, repIndex, m_GpuTemp);
                            Printf(Tee::PriNormal, "Error Code = %08u (%s)\n", rc.Get(),
                                    rc.Message());
                        }
                        rc.Clear();
                        CHECK_RC(RebootIntoNormalMode());
                        continue;
                    }
                    Printf(Tee::PriNormal, "Entering test '%s', seq %d, loop %d.\n",
                                            path.c_str(), testNumber, loop);
                    RC rc1 = RunTest(testProgram, testNumber, exeTime);
                    *pTestWasRun = true;
                    CHECK_RC(CheckControllerStatus( testProgram, testNumber,
                                m_TargetVoltage, repIndex));
                    CHECK_RC(DumpResultToFile( testProgram, testNumber, m_TargetVoltage, repIndex));
                    if (rc1 != RC::OK)
                    {   
                        // The test failed due to a linkcheck error or hardware failure.
                        // Record test result, reset GPU and try the next iteration.
                        m_PerTestProgramResults[testProgramName].emplace_back(rc1,
                                repIndex, m_TargetVoltage, 0, m_GpuTemp);
                        if (m_PreIstScript.empty() && m_UserScript.empty() && !m_IsATETypeImage)
                        {
                            // Print info when IST devinit done by VBIOS
                            Printf(Tee::PriNormal, "Voltage: %.1f mV -- Iteration: %d"
                                    " -- Temperature: %d deg C\n",
                                    m_TargetVoltage, repIndex, m_GpuTemp);
                            Printf(Tee::PriNormal, "Error Code = %08u (%s)\n", rc1.Get(),
                                    rc1.Message());
                        }
                        rc1.Clear();
                        // Remove this at later
                        CHECK_RC(RebootIntoNormalMode());
                        continue;
                    }

                    rc = ProcessTestResults(testProgram, testNumber, exeTime);
                    m_PerTestProgramResults[testProgramName].emplace_back(rc,
                            repIndex, m_TargetVoltage, 0, m_GpuTemp);
                    if (m_PreIstScript.empty() && m_UserScript.empty() && !m_IsATETypeImage)
                    {
                        Printf(Tee::PriNormal, "Voltage: %.1f mV -- Iteration: %d"
                                " -- Temperature: %d deg C\n",
                                m_TargetVoltage, repIndex, m_GpuTemp);
                        Printf(Tee::PriNormal, "Error Code = %08u (%s)\n", rc.Get(),
                                rc.Message());
                    }
                    // The error code is saved. ok to clear it.
                    rc.Clear();

                    CHECK_RC(PrintTimeStampTable());
                    CHECK_RC(SaveTestProgramResult(testProgram, exeTime, repIndex));

                    if (RebootIntoNormalMode() != RC::OK)
                    {   
                        Printf(Tee::PriError, "Unable to reboot device using ZPI "
                                                "after a successful MATHS Link test.\n"
                                                "Does this board have the modified ZPI?"
                                                " (see Confluence documentation).\n"
                                                "You can also use the -skip_reboot flag"
                                                " if running only one test sequence.\n");
                        return RC::UNSUPPORTED_HARDWARE_FEATURE;
                    }
                }
                if (m_PreVoltageOffset)
                {

                    // Add the increment step to voltage Offset
                    m_VoltageOffsetMv +=  (m_OffsetShmooDown) ? 
                        -m_StepIncrementOffsetMv : m_StepIncrementOffsetMv;
                }
            }
            SW_ERROR((m_PerTestProgramResults[testProgramName].size() !=
                        (voltIterations * m_Repeat)), "Test Records are of unexpected size\n");
        }
    }
    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::Run()
{
    Printf(Tee::PriDebug, "In Run()\n");

    // Save allocated chunks to reuse them between runs and free once done
    MemManager::MemDevice::MemAddresses allocatedChunks;

    // Go back to a usable state if something goes wrong
    DEFER
    {
        MemManager::MemDevice::Free(&allocatedChunks);
        if (m_State != State::NORMAL_MODE)
        {
            // No need to capture rc here because this line will only be run
            // if something has already failed, so we want to retain the rc
            // of the original failure.
            // (if run at the end of the function, m_State will be NORMAL_MODE
            // so this will not run)
            (void)RebootIntoNormalMode();
        }
    };

    bool testWasRun = false;
    RC rc;

    m_PreVoltageOverride = (m_VoltageOverrideMvAry.size() != 0);
    m_PreVoltageOffset = (m_VoltageOffsetMv > 0);

    string path;

    SW_ERROR(m_TestArgs.size() != 1, "One Test Image allowed\n");

    for (const TestArgs& testArgs: m_TestArgs)
    {
        const uint16_t numLoops = testArgs.numLoops;
        std::vector<TestNumber> testNums = testArgs.testNumbers;
        // TODO: if testNumber is empty, should we run all tests?
        // For now, by default run only test 0, START_MASTER Sequence_Index 0
        if (testNums.empty())
        {
            testNums.push_back(0);
        }

        SW_ERROR(numLoops != 1, "Only one loop allowed at this time\n");

        MASSERT(!testArgs.path.empty());
        if (path == Utility::RestoreString(testArgs.path))
        {
            Printf(Tee::PriDebug,
                   "Reusing test image '%s' (ie. skipping image load)\n",
                   path.c_str());
        }
        else
        {
            // RestoreString() is used to de-obfuscate the input argument
            path = Utility::RestoreString(testArgs.path);

            // Make sure the image file exists and is readable before we do anything else
            if (!Xp::DoesFileExist(path))
            {
                Printf(Tee::PriError, "Image '%s' not found\n", path.c_str());
                return RC::FILE_DOES_NOT_EXIST;
            }

            Printf(Tee::PriDebug, "Loading test image '%s'\n", path.c_str());
            m_TestImage = basename(const_cast<char*>(path.c_str()));
            CHECK_RC(m_SysMemLock.AcquireLockFile());
            rc = LoadTestToSysMem(path, &allocatedChunks);
            m_SysMemLock.ReleaseLockFile();
            CHECK_RC(rc);
            CHECK_RC(GetAteSpeedo());
            CHECK_RC(ExtractCVBInfoFromFile());
            // At this time, dump all the memory content at once.
            // TODO: Add support for multiple test program dump
            if (!m_ImageDumpFile.empty())
            {
                CHECK_RC(IstFlow::DumpImageToFile(allocatedChunks, Dump::PRE_TEST));
            }
        }

        RC rcLoop = (this->*RunLoop)(numLoops, path, &testWasRun, testNums);
        if (!m_ImageDumpFile.empty())
        {
            CHECK_RC(IstFlow::DumpImageToFile(allocatedChunks, Dump::POST_TEST));
        }
        CHECK_RC(ProcessTestRecords());
        if (rcLoop != RC::OK) return rcLoop;
    }

    rc = GetFirstError();

    // For the purpose of sanity check in PVS we want the option to ignore error
    // RC::BAD_LWIDIA_CHIP. The sanity check fails when the test returns a BAD_LWIDIA_CHIP.
    // However, it's is not a fatal error. RC::BAD_LWIDIA_CHIP means the test
    // ran successfully and found faulty components on the GPU; It is not a test failure.
    // We override rc when instructed to ignore the error
    if ((GetFirstError() == RC::BAD_LWIDIA_CHIP) && m_PvsIgnoreError233)
    {
        rc.Clear();
        rc = RC::OK;
    }
    return testWasRun ? rc : RC::SPECIFIC_TEST_NOT_RUN;
}

//------------------------------------------------------------------
RC IstFlow::PreRunTest(bool testWasRun)
{
    Printf(Tee::PriDebug, "In PreRunTest()\n");
    // See if there is a function in JS that we should call for manual tampering/debug
    // Note: Danger - Danger - security hole
    // Requested by SSG in bug 2555859
    constexpr char jsCallbackFunc[] = "PreIstRunActions";
    JavaScriptPtr pJs;
    JSObject* pGlobObj = pJs->GetGlobalObject();
    JsArray args;   // empty args
    JSFunction* pFunction = 0;
    // If the function is not defined, just skip it (and don't crash!)
    const bool runUserFunction = (pJs->GetProperty(pGlobObj, jsCallbackFunc, &pFunction) == RC::OK);
    RC rc;

    CHECK_RC(ResetIntoIstMode());

    if (testWasRun)
    {   
        CHECK_RC(ClearTestResults());
    }

    // Call into an optional JS function for potential extra code
    // (eg RW registers)
    if (runUserFunction)
    {   
        jsval retValJs = JSVAL_NULL;
        UINT32 jsRetVal = static_cast<UINT32>(RC::SOFTWARE_ERROR);
        CHECK_RC(pJs->CallMethod(pGlobObj, jsCallbackFunc, args, &retValJs));
        CHECK_RC(pJs->FromJsval(retValJs, &jsRetVal));
        CHECK_RC(jsRetVal);
    }

    return RC::OK;
}

/**
 * \brief Not all the test programs loaded contains test vectors.
 *        Some contains are special test program and only contain JTAG.
 *        Hence, we iterate through them  and check that they are not 
 *        JTAG images.
 */
RC IstFlow::GetExelwtableTestProg()
{
    Printf(Tee::PriDebug, "In GetExelwtableTestProg()\n");
    const size_t dutNumber = 0;
    std::set<std::string> testNamesSet;

    for (const auto &name : m_TestProgramsNames)
    {
        if (testNamesSet.count(name))
        {
            // MAS doesn't allow running the same multiple times.
            // Hence, our way to prevent that in case the user
            // enters a test multiple times in the config file
            // if a testProgram with name has been previously
            // processed, continue
            continue;
        }
        auto testProgram = m_CoreMalHandler.get_STORE()->findTpFromName(name, dutNumber);
        if (testProgram)
        {
            std::map<std::string, std::string> info;
            testProgram->get_Info(info);
            if (info.find("RESTORE_JTAG_IMAGE") == info.end() &&
                    info.find("SHUTDOWN_IMAGE") == info.end() &&
                        info.find("COMMON_IMAGE") == info.end())
            {
                m_ExecTestProgs.push_back(testProgram);
                testNamesSet.insert(name);
            }
        }
        else
        {
            // No test program found, warn the user and move on
            Printf(Tee::PriWarn, "Test program %s not found\n", name.c_str());
        }
    }

    return RC::OK;
}

/**
 *  \brief Online burst Image creation
 *
 */
RC IstFlow::CreateBurstImage()
{

    Printf(Tee::PriDebug, "In CreateBurstImage()\n");
    RC rc;
    std::shared_ptr<MATHS_IOBlock_ifc> ioHandler;
    MATHSerialization::ImageChaining ChainedTestProgram;
    IoBlockStatus IOStatus;
    ReqStatus ioBlockStatus;
    std::string msg;
    const size_t dutNumber = 0;
    m_CoreMalHandler.get_IOBlock(ioHandler);

    if (m_BurstConfig.size() == 0)
    {
        // No burst image to be created, return
        return RC::OK;
    }

    for (const auto& burstConfig : m_BurstConfig)
    {
        for (const auto& label : burstConfig.second)
        {
            ChainedTestProgram.addTpTf(label, label);
        }
        ChainedTestProgram.setOutTpName(burstConfig.first);
        ioBlockStatus = ioHandler->REQ_ImageChaining(dutNumber, ChainedTestProgram, IOStatus, msg);
        if (IOStatus != IoBlockStatus::Pass || ioBlockStatus != ReqStatus::Accepted)
        {
            Printf(Tee::PriError, "Online burst creation failed\n");
            return RC::SOFTWARE_ERROR;
        }

    }

    return RC::OK;
}

/**
 *\brief Execute the Jtag commnon image. Needed before exelwting a test image on GH100
 */
RC IstFlow::RunJTAGCommonProgram()
{
    Printf(Tee::PriDebug, "In RunJTAGCommonProgram()\n");
    RC rc;
    std::shared_ptr<MATHS_ExecBlock_ifc> exeHandler;
    m_CoreMalHandler.get_ExecBlock(exeHandler);
    ReqStatus  reqStatus;
    ExecStatus execStatus;
    std::string execMsg;
    const size_t dutNumber = 0;
    boost::posix_time::ptime exeTime;
    auto portCfg = m_CoreMalHandler.get_DUTManager()->getPortByIndex(dutNumber);
    auto chipInfoMgr = m_CoreMalHandler.get_STORE()->getChipInfoMgr(m_ChipID);
    auto jtagCommonTestProgram = chipInfoMgr->findTestProgram(ImageType::JTAG_COMMON,
            dutNumber, false);

    // The user decided no to run JTAG common. His/her responsability to ensure
    // that the proper test image is loaded.
    if (m_SkipJTAGCommonInit)
    {
        Printf(Tee::PriWarn, "Skipping the exelwtion of JTAG common init\n");
        return RC::OK;
    }

    if (!jtagCommonTestProgram)
    {
        // The user might have loaded a monolithic image. So no need to error out.
        // Just return gracefully
        Printf(Tee::PriWarn, "No JTAG common image found\n");
        return RC::OK;
    }
    std::string testProgramName = jtagCommonTestProgram->get_Name(dutNumber);

    exeHandler->REQ_ExelwteCommonTestProgram(m_ChipID, dutNumber, reqStatus,
            execStatus, execMsg, exeTime);
    if (reqStatus == ReqStatus::Failed || reqStatus == ReqStatus::Refused)
    {
        Printf(Tee::PriError, "Exelwtion of common process failed\n");
        return RC::SOFTWARE_ERROR;
    }
    m_JtagCommonInitRan = true;
    CHECK_RC(PollOnLinkCheck(jtagCommonTestProgram, 0, exeTime));

    // For the purpose of dumping the controller status and result correctly
    // we fiddle the setLastTf then set back correctly
    portCfg->setLastTf(nullptr);
    CHECK_RC(CheckControllerStatus(jtagCommonTestProgram, 0, 0, 0, true));
    CHECK_RC(DumpResultToFile(jtagCommonTestProgram, 0, 0, 0, true));
    portCfg->setLastTf(jtagCommonTestProgram->get_TestFlow(dutNumber)[0]);
    rc = ProcessTestResults(jtagCommonTestProgram, 0, exeTime, true);
    m_PerTestProgramResults[testProgramName].emplace_back(rc, 1, 0, 0, m_GpuTemp);
    rc.Clear();

    return RC::OK;
}

/**
 *\brief Execute the Jtag shutdown image. Needed on GH100 for a graceful shutdown
 */
RC IstFlow::RunJTAGShutdownProgram()
{
    Printf(Tee::PriDebug, "In RunJTAGShutdownProgram()\n");
    RC rc;
    std::shared_ptr<MATHS_ExecBlock_ifc> exeHandler;
    m_CoreMalHandler.get_ExecBlock(exeHandler);
    ExecStatus execStatus;
    std::string execMsg;
    const size_t dutNumber = 0;
    auto portCfg = m_CoreMalHandler.get_DUTManager()->getPortByIndex(dutNumber);
    auto chipInfoMgr = m_CoreMalHandler.get_STORE()->getChipInfoMgr(m_ChipID);
    auto shutdownTestProgram = chipInfoMgr->findTestProgram(ImageType::SHUT_DOWN, dutNumber, false);
    auto resetTestProgram = chipInfoMgr->findTestProgram(ImageType::JTAG_RESET, dutNumber, false);

    // The user decided no to run JTAG shutdown. His/her responsability to ensure
    // that the proper test image is loaded.
    if (m_SkipJTAGCommonInit)
    {
        Printf(Tee::PriWarn, "Skipping the exelwtion of JTAG shutdown\n");
        return RC::OK;
    }

    if (!shutdownTestProgram || !resetTestProgram)
    {   
        // Just return gracefully
        Printf(Tee::PriWarn, "No JTAG shutdown nor reset image found\n");
        return RC::OK;
    }
    std::string testProgramName = shutdownTestProgram->get_Name(dutNumber);

    exeHandler->ExecRunShutdownTP(m_ChipID, dutNumber, execStatus);
    if (execStatus != ExecStatus::PASS)
    {
        Printf(Tee::PriError, "Exelwtion of shutdown process failed\n");
        return RC::SOFTWARE_ERROR;
    }

    // For the purpose of dumping the controller status and result correctly
    // we fiddle the setLastTf then set back correctly
    portCfg->setLastTf(shutdownTestProgram->get_TestFlow(dutNumber)[0]);
    CHECK_RC(CheckControllerStatus(resetTestProgram, 0, 0, 0, true));
    CHECK_RC(DumpResultToFile(resetTestProgram, 0, 0, 0, true));

    CHECK_RC(CheckControllerStatus(shutdownTestProgram, 0, 0, 0, true));
    CHECK_RC(DumpResultToFile(shutdownTestProgram, 0, 0, 0, true));
    portCfg->setLastTf(nullptr);

    // TODO: Ask MAS  to allow a time exelwtion to be passed when doing shutdown
    // rc = ProcessTestResults(shutdownTestProgram, 0, exeTime, true);
    // m_PerTestProgramResults[testProgramName].emplace_back(rc, 0, 0, 0, m_GpuTemp);
    // rc.Clear();

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::RunTest
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    const TestNumber& testNumber,
    boost::posix_time::ptime &exeTime
)
{
    RC rc;
    Printf(Tee::PriDebug, "In RunTest(%u)\n", testNumber);
    const size_t dutNumber = 0;
    std::shared_ptr<MATHS_ExecBlock_ifc> exeHandler;
    m_CoreMalHandler.get_ExecBlock(exeHandler);
    ReqStatus  reqStatus;
    ExecStatus execStatus;
    std::string execMsg;

    SW_ERROR((!testProgram), "IstFlow: Cannot run a test if image loading failed\n");

    // The GPU must be initialized and in IST mode in order to run a test.
    SW_ERROR((m_State != State::IST_MODE && m_State != State::IST_PAUSE_MODE),
             "IstFlow: Cannot run a test sequence if the GPU is not in IST mode\n");
    SW_ERROR(!m_pGpu, "Cannot RunTest() without Initialize()\n");
    auto testFlows = testProgram->get_TestFlow(dutNumber);

    // By design, a test program contains only one test flow. Add this check 
    // in case things change in the future
    SW_ERROR(testFlows.size() != 1, "Test program contains more than 1 test flow\n");

    std::shared_ptr<TestFlow_ifc> testFlow = testFlows[testNumber];
    std::shared_ptr<TestSequence_ifc> lwrSeq = testFlow->get_FirstSequence();
    std::shared_ptr<mem_manager_ifc> memoryManager = m_CoreMalHandler.get_MemManager();
    std::pair<size_t, size_t>cseqBlock = lwrSeq->get_MemManagerBlock();

    const auto numTests = testFlow->get_AllLinkedSequences().size();
    if (testNumber >= numTests)
    {
        Printf(Tee::PriError,
               "There are only %lu test sequences in this image (no test %u)\n",
                numTests, testNumber);
        return RC::BAD_PARAMETER;
    }

    // The test flow address is effectively the address of the first sequence
    const UINT64 address = (uint64_t)((char*)memoryManager->get_block_address(cseqBlock.first) +
            cseqBlock.second);
    if (address == 0)
    {
        Printf(Tee::PriError,
               "Test sequence %u does not exist in this test image\n", testNumber);
        return RC::BAD_PARAMETER;
    }

    m_UserScriptArgsMap["$img_addr_msb"] = Utility::StrPrintf("%x", LwU64_HI32(address));
    m_UserScriptArgsMap["$img_addr_lsb"] = Utility::StrPrintf("%x", LwU64_LO32(address));

    // No VBIOS, the user provided script is responsible for IST devinit
    // and starting the test.
    // On GH100, not need to run the script through the lwpex2 binary
    // as that dependency has been lifted 
    if (!m_UserScript.empty() && m_ChipID != "gh100")
    {
        string userScriptArgsStr = Utility::RestoreString(m_UserScriptArgs);
        if (!m_UserScriptArgs.empty())
        {
            vector<string> userPreIstArgs;
            Utility::CreateArgv(userScriptArgsStr, &userPreIstArgs);
            m_UserScriptArgs.clear();
            for (UINT32 i = 0; i < userPreIstArgs.size(); i++)
            {
                // when the argument is someone in m_UserScriptArgsMap, override it with
                // callwlated value stored in the map
                if (m_UserScriptArgsMap.count(userPreIstArgs[i]) > 0)
                {
                    // If there's no callwlated value stored in the map, report an error
                    if (m_UserScriptArgsMap[userPreIstArgs[i]].empty())
                    {
                        Printf(Tee::PriWarn, "MODS doesn't know how to get the value of %s. "
                                "Not overriding it\n", userPreIstArgs[i].c_str());
                    }
                    else
                    {
                        Printf(Tee::PriNormal, "Override -user_script_args[%u] from %s to %s\n",
                                i, userPreIstArgs[i].c_str(),
                                m_UserScriptArgsMap[userPreIstArgs[i]].c_str());
                        userPreIstArgs[i] = m_UserScriptArgsMap[userPreIstArgs[i]];
                    }
                }
                m_UserScriptArgs += userPreIstArgs[i];
                m_UserScriptArgs += " ";
            }
            // remove the last space
            m_UserScriptArgs.pop_back();
        }

        Printf(Tee::PriNormal, "Ate SVT Speedo: %d \n", m_AteSpeedoMhz);
        Printf(Tee::PriNormal, "Ate HVT Speedo: %d \n", m_AteHvtSpeedoMhz);
        Printf(Tee::PriNormal, "IST TYPE: %s\n", m_IstType.c_str());
        Printf(Tee::PriNormal, "Device Under Test: %s\n", m_DevUnderTestBdf.c_str());
        Printf(Tee::PriNormal, "Temperature: %d deg C\n", m_GpuTemp);
        // Tell the HW to expect absolute addresses
        RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_ABSOLUTE_ADDRESS_0, UINT32(1U));
        string cmd = Utility::StrPrintf("%s %s", m_UserScript.c_str(), m_UserScriptArgs.c_str());
        Printf(Tee::PriNormal, "Running command: '%s'\n", cmd.c_str());
        
        const UINT64 scriptStartTime = Xp::GetWallTimeMS();

        const int result = std::system(cmd.c_str());

        const UINT64 scriptEndTime = Xp::GetWallTimeMS();
        m_TimeStampMap.at("PreIstScript") = scriptEndTime - scriptStartTime;

        if (result != 0)
        {
            Printf(Tee::PriError,
                   "User script '%s' failed [return value: %d]\n",
                   cmd.c_str(), result);
            return RC::SCRIPT_FAILED_TO_EXELWTE;
        }
    }

    // If we're running this test without a compatible VBIOS, we call a
    // script to do the pre-IST initialization.
    else if (!m_PreIstScript.empty() && m_ChipID != "gh100")
    {
        UINT32 vfeVoltage = UINT_MAX;
        // vfeVoltage is only needed for GA100
        if (!m_IsGa10x)
        {
            CHECK_RC(GetVfeVoltage(m_IstType, m_AteSpeedoMhz, m_FrequencyMhz,
                        m_StepIncrementOffsetMv, &vfeVoltage));
        }
        Printf(Tee::PriNormal, "Ate Speedo: %d \n", m_AteSpeedoMhz);
        Printf(Tee::PriNormal, "IST TYPE: %s - Frequency: %d\n", m_IstType.c_str(), m_FrequencyMhz);
        Printf(Tee::PriNormal, "Device Under Test: %s\n", m_DevUnderTestBdf.c_str());
        if (vfeVoltage != UINT_MAX)
        {
            Printf(Tee::PriNormal, "Voltage: %d mV\n", vfeVoltage);
        }
        Printf(Tee::PriNormal, "Temperature: %d deg C\n", m_GpuTemp);

        const string& preIstCommand = Utility::RestoreString(m_PreIstScript);
        // Tell the HW to expect absolute addresses
        RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_ABSOLUTE_ADDRESS_0, UINT32(1U));

        string cmd;
        if (m_IsGa10x)
        {
            cmd = Utility::StrPrintf("%s %s %x %x", preIstCommand.c_str(),
                            m_DevUnderTestBdf.c_str(), LwU64_HI32(address), LwU64_LO32(address));
        }
        // For GA100, also need to append vfeVoltage
        else
        {
            cmd = Utility::StrPrintf("%s %s %x %x %d", preIstCommand.c_str(),
                            m_DevUnderTestBdf.c_str(), LwU64_HI32(address), LwU64_LO32(address),
                            vfeVoltage);
        }
        Printf(Tee::PriNormal, "Running command: '%s'\n", cmd.c_str());

        const UINT64 scriptStartTime = Xp::GetWallTimeMS();
        const int result = std::system(cmd.c_str());

        const UINT64 scriptEndTime = Xp::GetWallTimeMS();
        m_TimeStampMap.at("PreIstScript") = scriptEndTime - scriptStartTime;
        if (result != 0)
        {
            Printf(Tee::PriError,
                   "Pre-IST script '%s' failed [return value: %d]\n",
                   preIstCommand.c_str(), result);
            return RC::SCRIPT_FAILED_TO_EXELWTE;
        }
    }


    // When running an IST supported VBIOS i.e no user script nor script from ATE config,
    // we program the address of the test sequence and trigger the test.
    else if (!m_IsATETypeImage)
    {
        RegHal& regHal = m_pGpu->GetSubdevice(0)->Regs();
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_ABSOLUTE_ADDRESS_0, UINT32(1U));
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_EMMC_START_ADDRESS_0_START_ADDRESS,
            LwU64_LO32(address));
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_EMMC_START_ADDRESS_MSB_0_START_ADDRESS,
            LwU64_HI32(address));

        // Launch IST on HW
        regHal.Write32Sync(MODS_MATHSLINK_PCIE_CNTRL_PLM0_MUX_DEMUX_CONFIG_1_MUX_DEMUX_EN,
            UINT32(1U));
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_IST_TRIGGER_0, 0x01);
        regHal.Write32Sync(MODS_IST_SEQ_SLV_IST_FGC6_CLAMP_0, 0x03);
    }
    // Call into MAL exelwtion block
    exeHandler->REQ_ExelwteTestProgram(
            testProgram,
            dutNumber,
            reqStatus,
            execStatus,
            execMsg,
            exeTime);
    if (reqStatus == ReqStatus::Failed || reqStatus == ReqStatus::Refused)
    {
        Printf(Tee::PriError, "TestProgram exelwtion failed\n");
        return RC::SOFTWARE_ERROR;
    }

    // If JTAG common was not run, this is a ga10x style of image.
    // Hence, the START sequence is part of the monolithic image.
    // LinkCheck can be performed
    if (!m_JtagCommonInitRan)
    {
        CHECK_RC(PollOnLinkCheck(testProgram, testNumber, exeTime));
    }

    // On GA10x, pulling on the shutdown register is part of the test run
    if (m_ChipID != "gh100")
    {
        const UINT64 testStartTime = Xp::GetWallTimeMS();
        CHECK_RC(PollOnShutdownRegister(testNumber, m_TestTimeoutMs));
        const UINT64 testEndTime = Xp::GetWallTimeMS();
        m_TimeStampMap.at("ImageTestRun") = testEndTime - testStartTime;
    }

    // On GH100, the GPU device remains in a pause state after an IST run
    m_State = (m_ChipID == "gh100") ? State::IST_PAUSE_MODE : State::UNDEFINED;
    return RC::OK;
}


RC IstFlow::PollOnLinkCheck
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    const TestNumber& testNumber,
    boost::posix_time::ptime &exeTime
)
{
    Printf(Tee::PriDebug, "In PollOnLinkCheck()\n");
    std::shared_ptr<MATHS_ProcBlock_ifc> pProcHandler;
    m_CoreMalHandler.get_ProcessingBlock(pProcHandler);

    // To make sure the HW has switched to IST mode, we need to
    // poll on the link-check memory.
    {
        // 100ms should be plenty since we expect it to take < 1ms
        constexpr std::chrono::milliseconds timeout_ms(100);
        using namespace std::chrono;
        auto start = high_resolution_clock::now();

        UINT64 count = 0;
        constexpr size_t dutNumber = 0; // We only have one GPU
        auto testFlow = testProgram->get_TestFlow(dutNumber)[testNumber];
        TestFlow_ifc::sptr pTestFlow(testFlow);
        Printf(Tee::PriLow, "Polling on Link Check\n");

        while (timeout_ms >
               duration_cast<std::chrono::milliseconds>(high_resolution_clock::now() - start))
        {
            ++count;
            if (pProcHandler->REQ_compareLinkCheckKernelSP(pTestFlow, dutNumber, &exeTime))
            {
                Printf(Tee::PriLow, " -> Success\n");
                return RC::OK;
            }
        }

        Printf(Tee::PriError, "-> Link Check failed even after %lims (%llu checks)\n",
                              timeout_ms.count(), count);
        return RC::ON_ENTRY_FAILED;
    }

    return RC::OK;
}

RC IstFlow::PollOnShutdownRegister(const TestNumber& testNumber, const FLOAT64 timeoutMs)
{
    Printf(Tee::PriDebug, "In PollOnShutdownRegister()\n");
    RC rc;
    // Poll on status register.
    // We don't use reghal for this because it is only available once
    // the HW has entered IST mode. It is a BAR-addressable register
    // inside MLPC with a hard-coded address of 0x1c1000
    // doc: https://confluence.lwpu.com/display/DFT/MATHS+Link+PCIE+Controller#MATHSLinkPCIEController-DownstreamRead/WriteRegister //$
    constexpr UINT32 IST_STATUS_REGISTER = 0x001c1000;
    constexpr UINT32 INITIAL_VALUE = 0;

    // The only communication channel from the GPU to the CPU is through these
    // 3 bits:
    // bit0 is for graceful shutdown
    // bit1 is for shutdown because of Subid mismatch
    // bit2 is for shutdown because of FATAL Error
    enum HwState
    {
        GRACEFUL_SHUTDOWN   = 1 << 0,
        SUBID_MISMATCH      = 1 << 1,
        FATAL_ERROR         = 1 << 2,
    };

    {
        Printf(Tee::PriLow, "Polling on Shutdown register\n");
        ShutdownPollingArgs args =
        {
            m_pGpu,
            IST_STATUS_REGISTER,
            INITIAL_VALUE
        };
        RC rc;
        CHECK_RC(POLLWRAP_HW(&IstFlow::PollNotValue, &args, timeoutMs));
    }

    // Check and act on the register value
    SW_ERROR((!m_pGpu->GetSubdevice(0)),
             "How can there be no subdevice at this point in the code ?\n");
    const UINT32 hwStatus = m_pGpu->GetSubdevice(0)->RegRd32(IST_STATUS_REGISTER);

    // Disable interrupt legacy interrrupt generated by Maths Controller
    // and initiate DeepL1 state entry
    m_pGpu->GetSubdevice(0)->RegWr32(IST_STATUS_REGISTER, 0x80000000);
    if (m_DeepL1State && m_PreIstScript.empty() && m_UserScript.empty() && !m_IsATETypeImage)
    {
        const auto& pPcie = m_pGpu->GetSubdevice(0)->GetInterface<Pcie>();
        INT32 domain = pPcie->DomainNumber();
        INT32 bus = pPcie->BusNumber();
        INT32 device = pPcie->DeviceNumber();
        INT32 function = pPcie->FunctionNumber();
        UINT08 pwrmCapPtr = 0;
        // Find the Capability header for Power Management
        if (Pci::FindCapBase(domain, bus, device, function, PCI_CAP_ID_POWER_MGT, &pwrmCapPtr) == RC::OK)
        {
            // Extract Power Management Control and Status register from
            // Power Management Capability header
            UINT16 pwrMgtCtrlSts = 0;
            CHECK_RC(Platform::PciRead16(domain, bus, device, function,
                        pwrmCapPtr + LW_PCI_CAP_POWER_MGT_CTRL_STS, &pwrMgtCtrlSts));
            Printf(Tee::PriDebug,
                    "NOTE: The value read from LW_XVE_PWR_MGMT_1 before writing is 0x%x\n",
                    pwrMgtCtrlSts);
            // When deep_l1 flag is used, MODS is writing to a PCIe configuration space address
            // Actually MODS is writing this: LW_XVE_PWR_MGMT_1_D3_HOT (0x64)
            pwrMgtCtrlSts =
                FLD_SET_DRF(_PCI_CAP, _POWER_MGT_CTRL_STS, _POWER_STATE, _PS_D3, pwrMgtCtrlSts);
            CHECK_RC(Platform::PciWrite16(domain, bus, device, function,
                                        pwrmCapPtr + LW_PCI_CAP_POWER_MGT_CTRL_STS,
                                        pwrMgtCtrlSts));
            Printf(Tee::PriDebug, "NOTE: The value read from LW_XVE_PWR_MGMT_1 after writing "
                    "is 0x%x, since deepl1 state arg is used\n", pwrMgtCtrlSts);
        }
    }

    if (hwStatus & HwState::FATAL_ERROR)
    {
        Printf(Tee::PriError, "HW failed while running test sequence %u [shutdown register:%x]\n",
               testNumber, hwStatus);
        return RC::HW_ERROR;
    }
    else if (hwStatus & HwState::SUBID_MISMATCH)
    {
        Printf(Tee::PriError,
               "Sub-id mismatch while running test sequence %u [shutdown register:%x]\n",
               testNumber, hwStatus);
        return RC::HW_ERROR;
    }
    else if (hwStatus & HwState::GRACEFUL_SHUTDOWN)
    {
        //Printf(Tee::PriLow, "Test sequence %u passed [shutdown register:%x]\n",
        Printf(Tee::PriNormal, "Test sequence %u passed [shutdown register:%x]\n",
               testNumber, hwStatus);
    }
    else
    {
        Printf(Tee::PriError, "Unknown test sequence failure [test:%u, shutdown register:%x]\n",
               testNumber, hwStatus);
        return RC::HW_ERROR;
    }

    return RC::OK;
}

/*static*/ bool IstFlow::PollNotValue(void* pArgs)
{
    Printf(Tee::PriDebug, "In PollNotValue()\n");
    MASSERT(pArgs);
    const ShutdownPollingArgs& args = *static_cast<const ShutdownPollingArgs*>(pArgs);
    MASSERT(args.pGpu);
    MASSERT(args.pGpu->GetSubdevice(0) &&
            "How can there be no subdevice at this point in the code?");
    const UINT32 readValue = args.pGpu->GetSubdevice(0)->RegRd32(args.address);
    return readValue != args.notValue;
}

//--------------------------------------------------------------------
RC IstFlow::GetAteSpeedo()
{
    RC rc;
    UINT32 speedoVer = UINT_MAX;

    // Only when -extract_cvb_info or -user_script or -pre_ist_script argument is used
    if (m_CvbInfo.testPattern.size() || !m_UserScript.empty() || !m_PreIstScript.empty())
    {
        // opt_speedo0 contains the SVT speedo of the chip.
        CHECK_RC(m_pGpu->GetSubdevice(0)->GetAteSpeedo(0, &m_AteSpeedoMhz, &speedoVer));
        // opt_speedo2 contains the HVT speedo of the chip.
        CHECK_RC(m_pGpu->GetSubdevice(0)->GetAteSpeedo(2, &m_AteHvtSpeedoMhz, &speedoVer));
    }

    return RC::OK;
}


//--------------------------------------------------------------------
RC IstFlow::ExtractCVBInfoFromFile()
{
    RC rc;
    FLOAT32 lwvddVoltMv = FLT_MAX;
    FLOAT32 msvddVoltMv = FLT_MAX;
    FLOAT32 voltLimitHighMv = FLT_MAX;
    FLOAT32 voltLimitLowMv = FLT_MAX;

    // when -extract_cvb_info argument is specified, extract the information from YAML file
    // otherwise just return 
    if (m_CvbInfo.testPattern.size())
    {
        CHECK_RC(CvbGetVoltSettingMv(&msvddVoltMv, &voltLimitHighMv, &voltLimitLowMv));
        // For at-speed, MODS needs to extract info from yaml file and program gpc pll
        // clock to the target freq
        // Also need to set the LWVDD voltage according to user's input value
        if (m_IstType == "at-speed")
        {
            lwvddVoltMv = m_CvbInfo.voltageV * 1000;
            CHECK_RC(CvbGetFreqMhz(m_AteSpeedoMhz, m_AteHvtSpeedoMhz, m_GpuTemp,
                        &m_CvbTargetFreqMhz));
            Printf(Tee::PriNormal, "-extract_cvb_info is used for at-speed, override lwvddVoltMv "
                    "= %.1f mV, msvddVoltMv = %.1f mV.\n"
                    "NOTE: This two voltage value will only be overriden when user has $hexlwvdd "
                    "and $hexmsvdd in -user_script_args.\n"
                    "Fmax from callwlation is %u mHz. Override gpc pll to %u mHz\n",
                    lwvddVoltMv, msvddVoltMv, m_CvbTargetFreqMhz, m_CvbTargetFreqMhz * 2);
            if (m_CvbTargetFreqMhz > 2100)
            {
                FLOAT32 mnpTagProgParamM = 15.0;
                FLOAT32 mnpTagProgParamP = 1.0;
                FLOAT32 mnpTagProgParamN = (FLOAT32)m_CvbTargetFreqMhz * 2 / (1.8 *
                        mnpTagProgParamM * mnpTagProgParamP);
                m_CvbMnpTagProgInfos.push_back({"pll", "CAT3_s0_0_gpcpll_MDIV0", 8,
                        (UINT32)mnpTagProgParamM});
                m_CvbMnpTagProgInfos.push_back({"pll", "CAT3_s0_0_gpcpll_PDIV0", 6,
                        (UINT32)mnpTagProgParamP});
                // round up when decimal part >= 0.5, and round down when < 0.5
                m_CvbMnpTagProgInfos.push_back({"pll", "CAT3_s0_0_gpcpll_NDIV_INT0", 8,
                        (UINT32)(mnpTagProgParamN + 0.5)});
            }
        }
        else if (m_IstType == "stuck-at")
        {
            CHECK_RC(CvbGetLwvddVoltMv(m_AteSpeedoMhz, m_StepIncrementOffsetMv, &lwvddVoltMv));
            Printf(Tee::PriNormal, "-extract_cvb_info is used for stuck-at, override lwvddVoltMv"
                    "= %.1f mV, msvddVoltMv = %.1f mV.\n"
                    "NOTE: This two voltage value will only be overriden when user has $hexlwvdd "
                    "and $hexmsvdd in -user_script_args.\n", lwvddVoltMv, msvddVoltMv);
        }
        if (lwvddVoltMv > voltLimitHighMv || lwvddVoltMv < voltLimitLowMv)
        {
            Printf(Tee::PriError,
                    "LWVDD Voltage: %.1f mV is not in the allowed range: %.1f mV - %.1f mV.\n",
                    lwvddVoltMv, voltLimitLowMv, voltLimitHighMv);
            return RC::SOFTWARE_ERROR;
        }

        // For 25mv step size, Vmin = 300mv. For 12.5mv step size, Vmin = 400mv.
        FLOAT32 vminMv = (abs(m_StepIncrementOffsetMv - 25) < 1e-6) ? 300 : 400;
        UINT32 lwvddNumSteps = UINT32((lwvddVoltMv - vminMv) / m_StepIncrementOffsetMv);
        UINT32 msvddNumSteps = UINT32((msvddVoltMv - vminMv) / m_StepIncrementOffsetMv);
        m_UserScriptArgsMap["$hexlwvdd"] = Utility::StrPrintf("%x", (1 << 31) + lwvddNumSteps);
        m_UserScriptArgsMap["$hexmsvdd"] = Utility::StrPrintf("%x", (1 << 31) + msvddNumSteps);
    }
 
    return RC::OK;
}


//--------------------------------------------------------------------
RC IstFlow::ProcessTestResults
(
    shared_ptr<TestProgram_ifc> &testProgram,
    const TestNumber& testNumber,
    boost::posix_time::ptime &exeTime,
    bool isCommonOrShutdown
)
{
    Printf(Tee::PriDebug, "In ProcessTestResults()\n");
    RC rc;
    SW_ERROR((m_State == State::ERROR),
             "IstFlow: Cannot process test results if there has been an error\n");

    size_t errorCount = 0;
    size_t nonFsErrCount = 0;
    std::string errorMessage;
    std::string finalFsMask;
    ReqStatus finalTestStatus;
    ExecStatus exStatus;
    constexpr size_t dutNumber = 0;  // device under test is always 0 since ther is only 1 GPU
    std::shared_ptr<MATHS_ProcBlock_ifc> pProcHandler;
    m_CoreMalHandler.get_ProcessingBlock(pProcHandler);
    auto testFlow = testProgram->get_TestFlow(dutNumber)[testNumber];

    try
    {
        pProcHandler->REQ_GetTestProgramFS(
            testProgram,
            dutNumber,
            finalFsMask,
            finalTestStatus,
            exStatus,
            errorMessage,
            errorCount,
            m_ChipID == "ga100" ? nullptr : &exeTime);
    }
    catch (const std::exception& e)
    {
        Printf(Tee::PriError,
               "Failed while processing results from test %d. [%s]\n",
               testNumber, e.what());
        return RC::STATUS_INCOMPLETE;
    }

    // If asked to filter MBIST test sequence result, we discard the finalMask
    // returned from MAS processing block and reconstruct a new one removing
    // the MBIST related FS tags. It seems like a waste, however we first need
    // to call into MAS processing block into order to be able to access
    // the test sequence results.
    if (m_FilterMbist)
    {
        finalFsMask.clear();
        const size_t secondErrorCount = basic::queryErrCount(testFlow, nullptr);
        // A sanity check for our internal peace.
        SW_ERROR(secondErrorCount != errorCount, "Error count discrepancies\n");
        auto fsCountAsSeqMap = basic::queryFScountAsSeqMap(testFlow, 
                m_ChipID == "ga100" ? nullptr : &exeTime);
        auto sequences = testFlow->get_AllLinkedSequences();
        // Helper lambda function, a wrapper around MAS API to
        // obtain test sequence index
        auto GetIndex = [] (std::string name, INT32 &index)
        {
            size_t found = name.find_last_of("_");
            std::map<std::string, std::string> mapInfo;
            index = std::stoi(name.substr(found + 1));
        };

        for (const auto &mapIter0 : fsCountAsSeqMap)
        {
            INT32 tsIndex = -1;
            string tsName = mapIter0.first;
            GetIndex(tsName, tsIndex);
            if (m_MbistSeqIndexes.count(tsIndex) == 0)
            {
                // Sequence not filtered, retreive his FS tag
                for (const auto &fsmap : mapIter0.second)
                {
                    finalFsMask += fsmap.first;
                    finalFsMask += ";";
                }
            }
        }
    }


    const set<string>& tags = ExtractFuseTags(finalFsMask);

    string tagsStr;
    for (const string& tag : tags)
    {
        tagsStr += tag + ", ";
    }
    if (!tagsStr.empty())
    {
        tagsStr.erase(tagsStr.size() - 2);
    }

    nonFsErrCount = QueryMalErrorCount(testProgram, testNumber, errorCount, exeTime);
    Printf(Tee::PriNormal, "\nTEST RESULT:\n");
    Printf(Tee::PriNormal, "Test Program: %s\n", testProgram->get_Name(dutNumber).c_str());
    Printf(Tee::PriNormal, "Total Error %lu -- NonFS Error %lu\n", errorCount, nonFsErrCount);
    SW_ERROR(nonFsErrCount > errorCount, "Non-FS error bits greater than total error bits\n");

    if (errorCount == 0)
    {
        Printf(Tee::PriNormal,
               "No error bits found.\n"
               "   Final test status: %d\n",
               static_cast<UINT32>(finalTestStatus));
    }
    else if (tags.empty())
    {
        Printf(Tee::PriNormal,
               "No error bits found outside of floorswept regions.\n"
               "   Total (FS tags + Non-FS tags) number of error bits: %zu\n"
               "   FS tags error bits: %zu\n"
               "   Non-FS tags error bits: %zu\n"
               "   FS tags: '%s'\n"
               "   Final test status: %d (0-fail, 1-pass)\n",
               errorCount, (errorCount - nonFsErrCount), nonFsErrCount,
               tagsStr.c_str(), static_cast<UINT32>(finalTestStatus));
    }
    else
    {
        Printf(Tee::PriWarn, "\n"
               "   Total (FS tags + Non-FS tags) number of error bits: %zu\n"
               "   FS tags error bits: %zu\n"
               "   Non-FS tags error bits: %zu\n"
               "   FS tags: '%s'\n"
               "   Final test status: %d (0-fail, 1-pass)\n",
               errorCount, (errorCount - nonFsErrCount), nonFsErrCount,
               tagsStr.c_str(), static_cast<UINT32>(finalTestStatus));
    }
    Printf(Tee::PriDebug,
           "   FS mask returned by MATHS-ACCESS: '%s'\n",
           finalFsMask.c_str());

    string tagsInFs;
    string tagsOutOfFs;
    string missingTags;
    {
        const RC rc = MaskTags(tags, m_FsTagMap, m_Fuses,
                               &tagsInFs, &tagsOutOfFs, &missingTags);
        if (rc != RC::OK)
        {
            Printf(Tee::PriWarn, "Failed to get FS information for tag processing");
            if (errorCount == 0)
            {
                Printf(Tee::PriWarn,
                       "...but there were no errors found by IST, so ignoring this issue.");
                return RC::OK;
            }
            else
            {
                SW_ERROR(tags.empty(), "How can there be errors but no tags?\n");
                Printf(Tee::PriWarn,
                       "...assuming the GPU is completely unfused");
                tagsInFs.clear();
                missingTags.clear();
                tagsOutOfFs.clear();
                for (const string& tag : tags)
                {
                    tagsOutOfFs += tag + ", ";
                }
                if (!tagsOutOfFs.empty())
                {
                    tagsOutOfFs.pop_back(); // space
                    tagsOutOfFs.pop_back(); // comma
                }
            }
        }
    }

    string fsInfo = "\n---------------\n";
    fsInfo += "Identified FS Tags for mismatched bits from FS regions which are not floorswept: ";
    fsInfo += tagsOutOfFs.empty() ? ("None") : (tagsOutOfFs);
    fsInfo += '\n';

    fsInfo += "Identified FS Tags for mismatched bits from FS regions which are ";
    fsInfo += "already disabled by floorsweeping: ";
    fsInfo += tagsInFs.empty() ? ("None") : (tagsInFs);
    fsInfo += '\n';

    fsInfo += "FYI: FS SKU Floorswept regions not tagged by the IST test ";
    fsInfo += "(ignored for result determination): ";
    fsInfo += missingTags.empty() ? ("None") : (missingTags);
    fsInfo += '\n';
    fsInfo += "\n---------------\n";
    // Jtag common init and jtag shutdon don't have any FS tags associated with them
    // so we skip printing those.
    if (!isCommonOrShutdown)
    {
        Printf(Tee::PriNormal, "%s", fsInfo.c_str());
    }
    Printf(Tee::PriNormal, "Number of faulty controller status bits: %d\n", m_CtrlStsError);
    Printf(Tee::PriNormal, "\n---------------\n");


    if (!tagsOutOfFs.empty() || m_CtrlStsError || nonFsErrCount)
    {
        if (nonFsErrCount)
        {
            Printf(Tee::PriError, "IST test detects non-FS failure.\n");
        }
        if (!tagsOutOfFs.empty())
        {
            Printf(Tee::PriError, "IST test uncovered defective component(s) not floorswept.\n");
        }
        if (m_CtrlStsError)
        {
            Printf(Tee::PriError, "IST test has failed/mismatched controller status bits.\n");
        }
        Printf(Tee::PriNormal, "\n---------------\n");

        return RC::BAD_LWIDIA_CHIP;
    }

    return RC::OK;
}

RC IstFlow::CheckControllerStatus
(
    const shared_ptr<TestProgram_ifc>& testProgram,
    const TestNumber &testNumber,
    UINT32 voltageOffset,
    UINT32 iterations,
    bool isCommonOrShutdown
)
{
    Printf(Tee::PriDebug, "In CheckControllerStatus()\n");
    const size_t dutNumber = 0;
    RC rc;
    std::shared_ptr<TestFlow_ifc> testFlow = testProgram->get_TestFlow(dutNumber)[testNumber];
    FileHolder ctrlStsRawFile;
    FileHolder ctrlStsFile;
    auto portCfg = m_CoreMalHandler.get_DUTManager()->getPortByIndex(dutNumber);
    auto testSequences = basic::get_AllToBeExeSequences(testFlow, portCfg);
    auto chipSeries = basic::chipName2chipSeries(m_ChipID);

    if (!m_CtrlStsDumpFile.empty())
    {
        CHECK_RC(ctrlStsRawFile.Open(m_CtrlStsRawFileName.c_str(), "a+"));
        CHECK_RC(ctrlStsFile.Open(m_CtrlStsFileName.c_str(), "a+"));

        fprintf(ctrlStsRawFile.GetFile(), "\nTest Program: %s\n",
                testProgram->get_Name(dutNumber).c_str());
        fprintf(ctrlStsFile.GetFile(),
                "\nTest Program: %s\n", testProgram->get_Name(dutNumber).c_str());
        if (!isCommonOrShutdown)
        {
            fprintf(ctrlStsRawFile.GetFile(), "Voltage: %d mV -- Iterations: %d\n",
                    voltageOffset, iterations);
            fprintf(ctrlStsFile.GetFile(),
                    "Voltage: %d mV -- Iterations: %d\n", voltageOffset, iterations);
        }
    }

    m_CtrlStsError = 0;
    for (const auto &testSequence : testSequences)
    {
        auto ctrlStatus  = testSequence->get_ControllerStatus();
        auto goldenStatus = testSequence->get_TestInfo()->get_GoldenControllerStatus();
        auto maskstatus  = testSequence->get_TestInfo()->get_ControllerStatusMask();
        std::vector<unsigned char> compareData;

        controllerStatus ctrlSts(ctrlStatus, &m_CoreMalHandler, chipSeries);
        ctrlSts.compareWith(dutNumber, goldenStatus, maskstatus, compareData);
        for (const auto &data : compareData)
        {
            m_CtrlStsError += Utility::CountBits(data);
        }
        if (!m_CtrlStsDumpFile.empty())
        {
            std::vector<unsigned char> crtStsRaw = ctrlSts.getRawData();
            SW_ERROR(crtStsRaw.size() != compareData.size(),
                    "Controller Status and Golden Status have different sizes");

            fprintf(ctrlStsRawFile.GetFile(), "%s:\n", testSequence->get_Name().c_str());
            fprintf(ctrlStsFile.GetFile(), "%s:\n", testSequence->get_Name().c_str());
            for (INT32 i = crtStsRaw.size() - 1; i >= 0; i--)
            {
                fprintf(ctrlStsRawFile.GetFile(), "%02x ", crtStsRaw[i]);
                fprintf(ctrlStsFile.GetFile(), "%02x ", compareData[i]);
            }
            fprintf(ctrlStsRawFile.GetFile(), "\n");
            fprintf(ctrlStsFile.GetFile(), "\n");
        }
    }

    return RC::OK;
}

/**
 * \brief As per MODS convention, the final test result is the first non RC::OK
 *        This function scans all the results records for all the test programs
 *        that were exelwted and returns the first error code.
 *
 */
RC IstFlow::GetFirstError()
{
    Printf(Tee::PriDebug, "In GetFirstError()\n");
    SW_ERROR(m_PerTestProgramResults.empty(), "Test Results record is empty\n");

    for (const auto &programResult: m_PerTestProgramResults)
    {
        std::string testProgramName = programResult.first;
        for (const auto &testRecord : programResult.second)
        {
            if (testRecord.rc != RC::OK)
            {
                return testRecord.rc;
            }
        }
    }

    return RC::OK;
}



RC IstFlow::DumpResultToFile
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    const TestNumber &testNumber,
    UINT32 voltageOffset,
    UINT32 iterations,
    bool isCommonOrShutdown
)
{
    Printf(Tee::PriDebug, "In DumpResultToFile()\n");
    if (m_TestRstDumpFile.empty())
    {
        return RC::OK;
    }

    RC rc;
    const size_t dutNumber = 0;
    std::shared_ptr<TestFlow_ifc> testFlow = testProgram->get_TestFlow(dutNumber)[testNumber];
    FileHolder testRstFile;
    auto portCfg = m_CoreMalHandler.get_DUTManager()->getPortByIndex(dutNumber);
    auto testSequences = basic::get_AllToBeExeSequences(testFlow, portCfg);

    CHECK_RC(testRstFile.Open(m_TestRstFileName.c_str(), "a+"));
    fprintf(testRstFile.GetFile(), "\nTest Program: %s\n",
            testProgram->get_Name(dutNumber).c_str());
    if (!isCommonOrShutdown)
    {
        fprintf(testRstFile.GetFile(),
                "Voltage: %d mV -- Iterations: %d\n", voltageOffset, iterations);
    }

    for (const auto &testSequence : testSequences)
    {   
        // Retrieve test result metadata
        std::shared_ptr<TestResponse_ifc> testRstp = testSequence->get_TestResult();
        if (testRstp)
        {   
            auto goldenTestRs = testSequence->get_TestInfo()->get_GoldenTestResult();
            testResult testRes(testRstp, &m_CoreMalHandler);
            testResult goldenRes(goldenTestRs, &m_CoreMalHandler);
            std::vector<unsigned char> testRstData = testRes.getRawData();
            std::vector<unsigned char> goldenData = goldenRes.getRawData();
            fprintf(testRstFile.GetFile(), "%s:\n", testSequence->get_Name().c_str());
            // Little Endian format
            for (INT32 i = goldenTestRs->get_MemSize() - 1; i >= 0 ; i--)
            {
                fprintf(testRstFile.GetFile(), "%02x ", testRstData[i]);
            }
            fprintf(testRstFile.GetFile(), "\n");
        }
    }
    Printf(Tee::PriDebug, "Test Result Dump completed\n");

    return RC::OK;
}

//-------------------------------------------------------------------
RC IstFlow::ProcessTestRecords()
{
    Printf(Tee::PriDebug, "In ProcessTestRecords()\n");
    // on Ampere, the test recored is displayed differently so no need for this
    if ((!m_PreIstScript.empty() || !m_UserScript.empty()) && m_ChipID != "gh100")
    {
        return RC::OK;
    }

    for (const auto & programResult: m_PerTestProgramResults)
    {
        std::string testProgramName = programResult.first;
        UINT32 numPasses = 0;
        UINT32 numRuns = programResult.second.size();
        string testRecord = "| Voltage(mV) | Iterations | Temperature(deg C) | RC Code | RC Message       \n";
        testRecord += "|-------------|------------|--------------------|---------|------------------\n";
        for (const auto& tr : programResult.second)
        {   
            testRecord += tr.TestRecordLine();
            if (tr.rc.Get() == 0) numPasses++;
        }

        MASSERT(numRuns >= numPasses);
        Printf(Tee::PriNormal, "\nTEST SUMMARY:\n");
        Printf(Tee::PriNormal, "Test program: %s\n", testProgramName.c_str());
        // TODO: Figure out to get the proper IST type
        // Printf(Tee::PriNormal, "IST Type: %s\n", m_IstType.c_str());
        Printf(Tee::PriNormal, "Device Under Test: %s\n", m_DevUnderTestBdf.c_str());
        Printf(Tee::PriNormal, "Total Iterations: %u, Passing: %u Failing: %u\n",
                               numRuns, numPasses, numRuns - numPasses);
        Printf(Tee::PriNormal, "%s\n", testRecord.c_str());
    }

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::ClearTestResults()
{
    Printf(Tee::PriDebug, "In ClearTestResults()\n");

    SW_ERROR((m_State == State::ERROR),
             "IstFlow: Should not call this function if there has been an error\n");

    for (const auto &flow_ptr: m_TestImageManager->TestFlows)
    {
        assert(flow_ptr); // This should pass
        flow_ptr->Scramble_Status_Results("Set");
    }

    return RC::OK;
}

//---------------------------------------------------------------------
RC IstFlow::ResetTestProgram(const shared_ptr<TestProgram_ifc> &testProgram)
{
    Printf(Tee::PriDebug, "In ResetTestProgram()\n");
    const size_t dutNumber = 0;
    std::shared_ptr<MATHS_ExecBlock_ifc> exeHandler;
    auto testFlows = testProgram->get_TestFlow(dutNumber);
    auto port = m_CoreMalHandler.get_DUTManager()->getPortByIndex(dutNumber);
    m_CoreMalHandler.get_ExecBlock(exeHandler);
    auto chipSeries = basic::chipName2chipSeries(m_ChipID);

    for (const auto & testFlow: testFlows)
    {   
        auto testSequences = basic::get_AllToBeExeSequences(testFlow, port);
        for (auto const &testSequence : testSequences)
        {   
            exeHandler->resetTestSequence(testSequence, chipSeries);
        }
    }

    return RC::OK;
}

//--------------------------------------------------------------------
size_t IstFlow::QueryMalErrorCount
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    const TestNumber &testNumber,
    size_t totalErrorCount,
    const boost::posix_time::ptime &exeTime
)
{    
    Printf(Tee::PriDebug, "In QueryMalErrorCount()\n");
    const size_t dutNumber = 0; 
    size_t fsErrorCount = 0;
    std::shared_ptr<TestFlow_ifc> testFlow = testProgram->get_TestFlow(dutNumber)[testNumber];
    std::vector<std::shared_ptr<TestSequence_ifc>> sequences = testFlow->get_AllLinkedSequences();

    // This API returns a map of map<sequence_name, map<fstag, error_count>>
    std::map<std::string, std::map<std::string, size_t>> fsCountAsSeqMap =
        basic::queryFScountAsSeqMap(testFlow, m_ChipID == "ga100" ? nullptr : &exeTime);

    // Helper lambda function, a wrapper around MAS API to
    // obtain test sequence index and packet label
    auto GetIndex = [&sequences] (std::string name, INT32 &index, std::string &label)
    {
        size_t found = name.find_last_of("_");
        std::map<std::string, std::string> mapInfo;
        index = std::stoi(name.substr(found + 1));
        sequences[index]->query_Info("Packet_Label", mapInfo);
        label = mapInfo["Packet_Label"];
    };

    string outputTable = "Table of the Error Count per FS Tag per Sequence:\n\n";
    outputTable += "| Sequence Index |    ";
    outputTable += "                          Sequence Name                                      |";
    outputTable +=" Sequence Filtered |    FS Tag      | Error Count |\n";
    outputTable += "|----------------|----";
    outputTable += "-----------------------------------------------------------------------------|";
    outputTable += "-------------------|----------------|-------------|\n";

    for (auto & mapIter0 : fsCountAsSeqMap)
    {
        string tsName = mapIter0.first;
        INT32 tsIndex = -1;
        std::string tsPacketLabel;
        GetIndex(tsName, tsIndex, tsPacketLabel);
        const bool isFiltered = (m_MbistSeqIndexes.count(tsIndex) == 1);
        outputTable += Utility::StrPrintf("|%15d |", tsIndex);
        outputTable += Utility::StrPrintf("%80s |", tsPacketLabel.c_str());
        outputTable += isFiltered ? "        Yes        |" : "        No         |";
        if (mapIter0.second.size())
        {
            // Print the first tag on the same line as the Sequence and subsequent
            // tags on the next line but at the same indentation as the first tag.
            std::map<std::string, size_t >::iterator it;
            it = mapIter0.second.begin();
            outputTable += Utility::StrPrintf("%15s | %-12lu|\n", it->first.c_str(), it->second);
            fsErrorCount += it->second;
            while (++it != mapIter0.second.end())
            {
                outputTable += Utility::StrPrintf("|%15s | %79s | %17s | %14s | %-12lu|\n", " ",
                        " ", " ", it->first.c_str(), it->second);
                fsErrorCount += it->second;
            }
        }
        // If there's no FS related error count of this sequence, just go to the next sequence
        else
        {
            outputTable += Utility::StrPrintf("       N/A      |     N/A     |\n");
        }
        outputTable += "|----------------|----";
        outputTable += "-------------------------------------";
        outputTable += "----------------------------------------|";
        outputTable += "-------------------|----------------|-------------|\n";
    }

    if (m_PrintPerSeqFsTagErrCnt)
    {
        Printf(Tee::PriNormal, "%s\n", outputTable.c_str());
    }

    return (totalErrorCount - fsErrorCount);
}

//--------------------------------------------------------------------
RC IstFlow::FilterMbistResult
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    const string& path
)
{
    Printf(Tee::PriDebug, "In FilterMbistResult()\n");

    if (!m_IgnoreMbist && !m_FilterMbist)
    {
        // No need to ingore nor filter out any MBIST sequence let's return
        return RC::OK;
    }
    const size_t dutNumber = 0; 
    const size_t testNumber = 0; 
    // The input argument path is the path of the xxx.img.bin file
    // Need to find the xxx.result.yml file in the same directory
    string binFileName = std::string(basename(const_cast<char*>(path.c_str())));
    string dirName= std::string(dirname(const_cast<char*>(path.c_str())));
    string binFileExtension = ".img.bin";
    string ymlFileExtension = ".result.yml";
    string ymlFileName = binFileName.substr(0,
            binFileName.length() - binFileExtension.length()) + ymlFileExtension;
    string ymlFilePath = dirName + "/" + ymlFileName;
    if (!Xp::DoesFileExist(ymlFilePath))
    {
        Printf(Tee::PriError, "'%s' not found, needed to identify MBIST sequences\n",
                ymlFilePath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    YAML::Node yamlRoot = YAML::LoadFile(ymlFilePath);
    if (!yamlRoot)
    {
        Printf(Tee::PriError, "Can not Load the result.yml file\n");
        return RC::FILE_READ_ERROR;
    }

    // The gathering of MBist sequence is done once per test program
    // Hence we clear the sequence once we get here since it might contain
    // info from the previour run test program
    m_MbistSeqIndexes.clear();

    // The yml file format is like this:
    //ISTResultsInfo:
    // Results_Group0_KeyOff_0_HdrRelAddr@@@ADDR_0_HEADER22@@@:
    //  TestType: mbist
    //  Test_Sequence_Index: 22
    if (yamlRoot["ISTResultsInfo"])
    {
        YAML::Node istRstInfo = yamlRoot["ISTResultsInfo"];
        for (YAML::const_iterator it = istRstInfo.begin(); it != istRstInfo.end(); ++it)
        {
            string seqName = it->first.as<std::string>();
            if (istRstInfo[seqName]["TestType"])
            {
                string testType = istRstInfo[seqName]["TestType"].as<std::string>();
                if (testType == "mbist")
                {
                    if (istRstInfo[seqName]["Test_Sequence_Index"])
                    {
                        m_MbistSeqIndexes.insert(
                                istRstInfo[seqName]["Test_Sequence_Index"].as<UINT32>()
                                );
                    }
                }
            }
        }
    }

    // If applicable, mask the mbist test sequences. MAS will not returned any test
    // result for masked sequences
    if (m_IgnoreMbist)
    {
        const size_t testProgramNum = 0;
        auto tp = m_CoreMalHandler.get_STORE()->get_TestPrograms()[testProgramNum];
        std::shared_ptr<TestFlow_ifc> testFlow = tp->get_TestFlow(dutNumber)[testNumber];
        // Get an vector of all the sequences of the current test flow
        std::vector<std::shared_ptr<TestSequence_ifc>> sequences =
            testFlow->get_AllLinkedSequences();
        string mbistSeqStr = "ignore_mbist_sequences is used, these MBIST sequences are ignored: ";
        for (const auto & mbistSeqIndex : m_MbistSeqIndexes)
        {
            mbistSeqStr += Utility::StrPrintf("%d ", mbistSeqIndex);
            sequences[mbistSeqIndex]->setMasked(true);
        }
        Printf(Tee::PriWarn, "%s\n", mbistSeqStr.c_str());
    }

    // Add a text to alert the user about MBIST being filtered.
    // Filtered sequence are not used in evaluating the overall
    // test results
    if (m_FilterMbist)
    {
        string mbistSeqStr = "filter_mbist_sequences is used, these MBIST sequences are filtered";
        mbistSeqStr+= " (not used in evaluating the test result): ";
        for (const auto & mbistSeqIndex : m_MbistSeqIndexes)
        {
            mbistSeqStr += Utility::StrPrintf("%d ", mbistSeqIndex);
        }
        Printf(Tee::PriWarn, "%s\n", mbistSeqStr.c_str());
    }

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::CvbGetFreqMhz
(
    const UINT32 ateSpeedoMhz,
    const UINT32 ateHvtSpeedoMhz,
    INT32 gpuTemp,
    UINT32* targetFreqMhz
)
{
    Printf(Tee::PriDebug, "In CvbGetFreqMhz()\n");
    if (!Xp::DoesFileExist(m_CbvEquationYmlPath))
    {
        Printf(Tee::PriError, "'%s' not found\n", m_CbvEquationYmlPath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    YAML::Node yamlRoot = YAML::LoadFile(m_CbvEquationYmlPath);
    if (!yamlRoot)
    {
        Printf(Tee::PriError, "Can not Load the cvb_equations.yml file\n");
        return RC::FILE_READ_ERROR;
    }
    // The yml file format is like this:
    // ftm2clk:
    //   voltage_limit: 0.8
    //     below_volt_limit:
    //       C0: 1
    //       C1: 1
    //       ......
    //       C8: 1
    //       C9: 1
    //     above_volt_limit:
    //       C0: 1
    //       C1: 1
    //       ......
    //       C8: 1
    //       C9: 1
    if (!yamlRoot[m_CvbInfo.testPattern])
    {
        Printf(Tee::PriError, "Can not find test pattern %s in cvb_equations.yml.\n",
                m_CvbInfo.testPattern.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    YAML::Node testPatternNode = yamlRoot[m_CvbInfo.testPattern];
    if (!testPatternNode["below_volt_limit"] || !testPatternNode["above_volt_limit"] || !testPatternNode["voltage_limit"])
    {
        Printf(Tee::PriError, "Can not find below_volt_limit/above_volt_limit/voltage_limit for test pattern %s in cvb_equations.yml.\n", m_CvbInfo.testPattern.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    FLOAT32 voltLimit = testPatternNode["voltage_limit"].as<FLOAT32>();
    UINT32 speedoMhz = 0;
    // There should be 10 coefficients in the equation, got from the yaml file
    vector<FLOAT32> coefficientsAry;
    if (m_CvbInfo.voltageV < voltLimit && abs(m_CvbInfo.voltageV - voltLimit) > 1e-6)
    {
        for (YAML::const_iterator it = testPatternNode["below_volt_limit"].begin();
                it != testPatternNode["below_volt_limit"].end(); ++it)
        {
            coefficientsAry.push_back(it->second.as<FLOAT32>());
        }
        // using HVT Speedo for the low voltage range
        speedoMhz = ateHvtSpeedoMhz;
        Printf(Tee::PriNormal, "Using HVT Speedo: %d in callwlation for low voltage range\n",
                ateHvtSpeedoMhz);
    }
    if ((m_CvbInfo.voltageV > voltLimit && abs(m_CvbInfo.voltageV - voltLimit) > 1e-6) || 
            abs(m_CvbInfo.voltageV - voltLimit) < 1e-6)
    {
        for (YAML::const_iterator it = testPatternNode["above_volt_limit"].begin();
                it != testPatternNode["above_volt_limit"].end(); ++it)
        {
            coefficientsAry.push_back(it->second.as<FLOAT32>());
        }
        // using SVT Speedo for the high voltage range
        speedoMhz = ateSpeedoMhz;
        Printf(Tee::PriNormal, "Using SVT Speedo: %d in callwlation for high voltage range\n",
                ateSpeedoMhz);
    }
    if (coefficientsAry.size() != 10)
    {
        Printf(Tee::PriError,
                "The number of coefficients is not 10 for test pattern %s in cvb_equations.yml.\n",
                m_CvbInfo.testPattern.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    if (gpuTemp == INT_MIN)
    {
        Printf(Tee::PriWarn, "MODS can't read temperature, use 35C to callwlate CVB Frequency\n");
        gpuTemp = 35;
    }
    // Freq(mHz) = C0*P^2 + C1*V^2 + C2*T^2 + C3*PV + C4*VT+ C5*PT + C6*P + C7*V + C8*T + C9
    // P is speedo (mHz), T is temperature (C), V is User's input Voltage (V)
    *targetFreqMhz = UINT32(coefficientsAry[0] * (speedoMhz * speedoMhz) + 
            coefficientsAry[1] * (m_CvbInfo.voltageV * m_CvbInfo.voltageV) +
            coefficientsAry[2] * gpuTemp * gpuTemp +
            coefficientsAry[3] * speedoMhz * m_CvbInfo.voltageV +
            coefficientsAry[4] * m_CvbInfo.voltageV * gpuTemp +
            coefficientsAry[5] * speedoMhz * gpuTemp +
            coefficientsAry[6] * speedoMhz + coefficientsAry[7] * m_CvbInfo.voltageV +
            coefficientsAry[8] * gpuTemp + coefficientsAry[9]);

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::CvbGetLwvddVoltMv
(
    const UINT32 ateSpeedoMhz,
    const FLOAT32 voltStep,
    FLOAT32* targetVoltMv
)
{
    Printf(Tee::PriDebug, "In CvbGetLwvddVoltMv()\n");
    if (!Xp::DoesFileExist(m_CbvEquationYmlPath))
    {
        Printf(Tee::PriError, "'%s' not found\n", m_CbvEquationYmlPath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    YAML::Node yamlRoot = YAML::LoadFile(m_CbvEquationYmlPath);
    if (!yamlRoot)
    {
        Printf(Tee::PriError, "Can not Load the cvb_equations.yml file\n");
        return RC::FILE_READ_ERROR;
    }
    // The yml file format is like this:
    // atpg:
    //   C0: -0.2642
    //   C1: 1043.8
    if (!yamlRoot[m_CvbInfo.testPattern])
    {
        Printf(Tee::PriError, "Can not find test pattern %s in cvb_equations.yml.\n",
                m_CvbInfo.testPattern.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    YAML::Node testPatternNode = yamlRoot[m_CvbInfo.testPattern];
    if (!testPatternNode["C0"] || !testPatternNode["C1"])
    {
        Printf(Tee::PriError,
                "Can not find C0 or C1 for callwlating voltage in cvb_equations.yml.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    FLOAT32 coefficient0 = testPatternNode["C0"].as<FLOAT32>();
    FLOAT32 coefficient1 = testPatternNode["C1"].as<FLOAT32>();
    const FLOAT32 rdThres = voltStep / 2;
    // V(in V) = C0 * Speedo + C1
    *targetVoltMv = (coefficient0 * ateSpeedoMhz + coefficient1) * 1000;
    // Round up the final voltage to the nearest high multiple of 25mV / 12.5mV
    *targetVoltMv += fmod(*targetVoltMv, voltStep) > rdThres ?
                    (voltStep - fmod(*targetVoltMv, voltStep)) : - fmod(*targetVoltMv, voltStep);

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::CvbGetVoltSettingMv
(
    FLOAT32* msvddVoltMv,
    FLOAT32* voltLimitHighMv,
    FLOAT32* voltLimitLowMv
)
{
    Printf(Tee::PriDebug, "In CvbGetVoltSettingMv()\n");
    if (!Xp::DoesFileExist(m_CbvEquationYmlPath))
    {
        Printf(Tee::PriError, "'%s' not found\n", m_CbvEquationYmlPath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    YAML::Node yamlRoot = YAML::LoadFile(m_CbvEquationYmlPath);
    if (!yamlRoot)
    {
        Printf(Tee::PriError, "Can not Load the cvb_equations.yml file\n");
        return RC::FILE_READ_ERROR;
    }
    // The yml file format is like this:
    // voltage_setting:
    //   msvdd_stuck_at_v: 0.8
    //   msvdd_at_speed_v: 1.1
    //   volt_limit_high_v: 1.1
    //   volt_limit_low_v: 0.4
    if (!yamlRoot["voltage_setting"])
    {
        Printf(Tee::PriError, "Can not find voltage_setting section in cvb_equations.yml.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    YAML::Node voltSettingNode = yamlRoot["voltage_setting"];
    if (!voltSettingNode["msvdd_stuck_at_v"] || !voltSettingNode["msvdd_at_speed_v"] ||
        !voltSettingNode["volt_limit_high_v"] || !voltSettingNode["volt_limit_low_v"])
    {
        Printf(Tee::PriError,
                "Can not find msvdd_stuck_at_v/msvdd_at_speed_v/volt_limit_high_v/volt_limit_low_v"
                " in voltage_setting section in cvb_equations.yml.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    if (m_IstType == "stuck-at")
    {
        *msvddVoltMv = voltSettingNode["msvdd_stuck_at_v"].as<FLOAT32>() * 1000;
    }
    else if (m_IstType == "at-speed")
    {
        *msvddVoltMv = voltSettingNode["msvdd_at_speed_v"].as<FLOAT32>() * 1000;
    }
    *voltLimitHighMv = voltSettingNode["volt_limit_high_v"].as<FLOAT32>() * 1000;
    *voltLimitLowMv = voltSettingNode["volt_limit_low_v"].as<FLOAT32>() * 1000;

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::GetVfeVoltage
(
    const string &istType,
    const UINT32 &ateSpeedoMhz,
    const UINT32 &frequency,
    const UINT32 &voltStep,
    UINT32* vfeVoltage
)
{  
    Printf(Tee::PriDebug, "In GetVfeVoltage()\n");
    const UINT32 predictiveVolt = 575;    // contanst voltage (575mV) for predictive IST
    const UINT32 stuckAtVolt = 600;       // constant voltage (600mV) for stuck-at IST
    const UINT32 rdThres = 5;
    if (istType == "at-speed")
    {   
        switch (frequency)
        {   
            // The coefficients for the VFE equations are provided by SSG
            // doc: https://wiki.lwpu.com/gpuchipsolutions/index.php/Chip_Productization/GPU_Productization/GA100/POR/PerfTable8840
            case 915:
                *vfeVoltage = UINT32((-0.29905 * ateSpeedoMhz) + 1236.84);
                break;
            case 1605:
                *vfeVoltage = UINT32((-0.28339 * ateSpeedoMhz) + 1358.834);
                break;
            case 1755:
                *vfeVoltage = UINT32((-0.29157 * ateSpeedoMhz) + 1452.078);
                break;
            default:
                Printf(Tee::PriError,
                        "Wrong Freq for at-speed, correct values are 915, 1605 or 1755.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        // Round up the final voltage to the nearest high multiple of 25mV if the computed
        // voltage is greater than 5mV from the nearest lower multiple of 25mV
        // e.g if vfeVoltage > 505mV, vfeVoltage is round up to 525mV else round down to 500mV
        *vfeVoltage += (*vfeVoltage % voltStep) > rdThres ?
            (voltStep - (*vfeVoltage % voltStep)) : - (*vfeVoltage % voltStep);
    }
    else if (istType == "predictive")
    {
        *vfeVoltage = predictiveVolt;
    }
    else //stuck-at
    {
        *vfeVoltage = stuckAtVolt;
    }
    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::PrintTimeStampTable()
{
    string outputTable = "Table of the elapsed time:\n\n";
    outputTable += "|      Function        |     Time (ms)  |\n";
    outputTable += "|----------------------|----------------|\n";
    for (auto &mapIter : m_TimeStampMap)
    {
        if (mapIter.second != 0)
        {
            outputTable += Utility::StrPrintf("|%21s |", mapIter.first.c_str());
            outputTable += Utility::StrPrintf("%15llu |\n", mapIter.second);
        }
    }
    outputTable += "|----------------------|----------------|\n";
    if (m_PrintTimes)
    {
        Printf(Tee::PriNormal, "%s\n", outputTable.c_str());
    }
    return RC::OK;
}

RC IstFlow::SaveTestProgramResult
(
    const shared_ptr<TestProgram_ifc> &testProgram,
    const boost::posix_time::ptime &exeTime,
    UINT32 iteration
)
{
    if (!m_SaveTestProgramResult)
    {   
        return RC::OK;
    }
    char timeBuff[50];
    const size_t dutNumber = 0;
    std::time_t lwrrentTime = std::time(nullptr);
    std::strftime(timeBuff, sizeof(timeBuff), "%T", std::localtime(&lwrrentTime));
    std::string fileName = testProgram->get_Name(dutNumber) + "_iter_" + std::to_string(iteration);
    fileName += "_" + std::string(timeBuff) + ".txt";
    saveResultFromTestProgram(testProgram, dutNumber, fileName, 
            &m_CoreMalHandler, MATHSerialization::serType::txt, &exeTime);

    return RC::OK;
}

//--------------------------------------------------------------------
RC IstFlow::Cleanup()
{
    StickyRC rc;
    Printf(Tee::PriDebug, "In Cleanup()\n");

    if ((m_State != State::NORMAL_MODE) && m_RebootWithZpi)
    {
        Printf(Tee::PriWarn, "Something went wrong and we were unable "
                             "to leave the GPU in a usable state.\n");
    }

    if (!m_SeleneSystem && m_SmbusInitialized)
    {
        CHECK_RC(m_SmbusImpl->UninitializeIstSmbusDevice());
    }

    if (m_RebootWithZpi && m_ChipID != "gh100")
    {
        m_ZpiLock.CloseLockFile();
    }
    m_SysMemLock.CloseLockFile();

    // IMPLEMENTME
    // Deinitialize MATHS-ACCESS library
    // Free any allocated memory
    m_TestImageManager.reset(); // safe even if nullptr

    return rc;
}

IstFlow::~IstFlow()
{
    Printf(Tee::PriDebug, "In ~IstFlow()\n");

    // Undo anything done in Initialize()
    ReleaseGpu();   // Can be run even if nothing has happened
    m_pGpu = nullptr;
}

} // namespace Ist
