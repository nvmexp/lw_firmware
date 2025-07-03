/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/fusejsonparser.h"
#include "core/include/fuseutil.h"
#include "core/include/jsonparser.h"
#include "core/include/xp.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

using namespace FuseUtil;
using namespace JsonParser;
using namespace rapidjson;

JS_CLASS(FuseJson);

static SObject FuseJson_Object
(
    "FuseJson",
    FuseJsonClass,
    0,
    0,
    "Fuse Json class"
);

FuseJsonParser::FuseJsonParser() :
    m_pFuseDefMap(nullptr),
    m_pSkuList(nullptr),
    m_pMiscInfo(nullptr)
{
}

// Use RapidJson to parse the JSON fuse file and interpret the results
RC FuseJsonParser::ParseFileImpl(const string &filename,
                                 FuseDefMap   *pFuseDefMap,
                                 SkuList      *pSkuList,
                                 MiscInfo     *pMiscInfo,
                                 FuseDataSet  *pFuseDataSet)
{
    RC rc;

    m_FileName = filename;
    m_pFuseDefMap = pFuseDefMap;
    m_pSkuList = pSkuList;
    m_pMiscInfo = pMiscInfo;

    Document doc;
    Document porDoc;
    Document fpfDoc;

    string unredactedFile;
    CHECK_RC(FindUnredactedFilename(filename, &unredactedFile));
    m_FileName = unredactedFile;
    CHECK_RC(ParseFuseJson(m_FileName, &doc));
    CHECK_RC(InterpretFuseJson(doc, false));

    // POR data is in <chip>_POR.json files for GV100+, check if the file's there first
    string porFileName = filename.substr(0, filename.rfind("_f.json")) + "_POR.json";
    // Set the filename so errors will say _POR.json, not _f.json
    if (FindUnredactedFilename(porFileName, &m_FileName) == RC::OK)
    {
        CHECK_RC(ParseFuseJson(m_FileName, &porDoc));
        CHECK_RC(InterpretPorJson(porDoc));
    }

    // FPF data is in <chip>_fpf.json files for TU10x+, check if the file's there first
    string fpfFileName = filename.substr(0, filename.rfind("_f.json")) + "_fpf.json";
    // Set the filename so errors will say _fpf.json, not _f.json
    if (FindUnredactedFilename(fpfFileName, &m_FileName) == RC::OK)
    {
        CHECK_RC(ParseFuseJson(m_FileName, &fpfDoc));
        CHECK_RC(InterpretFuseJson(fpfDoc, true));
    }

    // Reset the filename to _f.json for the rest of the functions
    m_FileName = unredactedFile;

    // Do some post-processing on the SKU list retrieved from
    // the JSON fuse file
    CHECK_RC(InterpretSkuFuses(pSkuList));
    RemoveTraditionalIff(pSkuList); // WAR for RFE 1633076

    return rc;
}

RC FuseJsonParser::ParseFuseJson(const string& filename, Document* pDoc)
{
    // Exit early if fuse file doesn't exit.
    // Fuse files are no longer shipped with official MODS builds and are
    // only distributed via internal packages.  Some code in MODS attempts
    // to reads fuse files on MODS start, resulting in the info and error
    // messages on stdout and MODS log.  Avoid these messages.
    const auto fileStatus = Utility::FindPkgFile(filename, nullptr);
    if (fileStatus == Utility::NoSuchFile)
    {
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
        {
            Printf(Tee::PriWarn, "Fuse file %s not found\n", filename.c_str());
        }
        return RC::FILE_DOES_NOT_EXIST;
    }

    return ParseJson(filename, pDoc);
}

// RapidJson returns a Document structure which is actually a tree
// which represents the JSON data it parsed. Most functions in FuseJsonParser
// operate on "JsonTree", which is the result of getting sections from
// the RapidJson Document.
RC FuseJsonParser::InterpretFuseJson(const Document& doc, bool isFpf)
{
    RC rc;

    // Mandatory sections
    if (!isFpf)
    {
        // Mandatory in <chip>_f.json, redundant in <chip>_fpf.json
        CHECK_RC(CheckHasSection(doc, "header"));
        CHECK_RC(ParseHeader(doc["header"]));
    }

    CHECK_RC(CheckHasSection(doc, "options"));
    const JsonTree& options = doc["options"];

    CHECK_RC(CheckHasSection(options, "Chip_options"));
    CHECK_RC(ParseChipOptions(options["Chip_options"], isFpf));

    if (options.HasMember("Pseudo_fuses"))
    {
        CHECK_RC(ParsePseudoFuses(options["Pseudo_fuses"]));
    }

    CHECK_RC(CheckHasSection(options, "Map_options"));
    CHECK_RC(ParseMapOptions(options["Map_options"]));

    CHECK_RC(CheckHasSection(options, "Mkt_options"));
    CHECK_RC(ParseMktOptions(options["Mkt_options"], isFpf));

    // FPF may not have a Fuse_encoding section, but if it does
    // we want to parse it
    if (!isFpf)
    {
        CHECK_RC(CheckHasSection(options, "Fuse_encoding"));
    }
    if (options.HasMember("Fuse_encoding"))
    {
        CHECK_RC(ParseFuseEncodingOptions(options["Fuse_encoding"]));
    }

    if (!isFpf && options.HasMember("Tag_map"))
    {
        CHECK_RC(ParseFsTagMap(options["Tag_map"]));
    }

    if (options.HasMember("Floorsweeping_options") && !isFpf)
    {
        CHECK_RC(ParseFloorsweepingOptions(options["Floorsweeping_options"]));
    }

    // Optional sections
    if (options.HasMember("Bootrom_patches"))
    {
        CHECK_RC(ParseBootromPatches(options["Bootrom_patches"]));
    }
    if (options.HasMember("IFF_patches") && !isFpf)
    {
        CHECK_RC(ParseIFFPatches(options["IFF_patches"]));
    }
    if (options.HasMember("Fuse_macro"))
    {
        CHECK_RC(ParseFuseMacro(options["Fuse_macro"], isFpf));
    }
    if (options.HasMember("Sku_ids"))
    {
        CHECK_RC(ParseSkuIds(options["Sku_ids"]));
    }

    // Pre-Volta chips have their POR data as part of the
    // Fuse.JSON file, so parse it out here if it exists.
    if (options.HasMember("Slt_data"))
    {
        CHECK_RC(ParseSltData(options["Slt_data"]));
    }
    if (options.HasMember("Por_data"))
    {
        CHECK_RC(ParsePorData(options["Por_data"]));
    }

    return rc;
}

RC FuseJsonParser::ParseHeader(const JsonTree& header)
{
    RC rc;

    string buf;
    CHECK_RC(FindJsolwal(header, "Perforce file revision", buf));
    m_pMiscInfo->P4Data.P4Str = buf;

    // revision string format: '$Id: //sw/mods/chipfuse/xml_v1/gm206_f.json#2 $'
    size_t Pos = buf.find("#");
    if (Pos == string::npos)
    {
        Printf(Tee::PriWarn, "Revision number not found in fuse JSON file\n");
        m_pMiscInfo->P4Data.P4Revision = 0xBADBEEF;
    }
    else
    {
        string RevStr = buf.substr(Pos+1);

        // RevStr is now '2 $'
        // now get '2' and colwer it to UINT32
        Pos = RevStr.find(" ");
        if (Pos == string::npos)
        {
            Printf(Tee::PriWarn, "Bad revision format in fuse JSON file\n");
            m_pMiscInfo->P4Data.P4Revision = 0xBADBEEF;
        }

        RevStr = RevStr.substr(0, Pos);
        m_pMiscInfo->P4Data.P4Revision = strtoul(RevStr.c_str(), nullptr, 10);
    }

    CHECK_RC(FindJsolwal(header, "Generation time", m_pMiscInfo->P4Data.GeneratedTime));
    CHECK_RC(FindJsolwal(header, "Project Name", m_pMiscInfo->P4Data.ProjectName));
    CHECK_RC(FindJsolwal(header, "Status", m_pMiscInfo->P4Data.Validity));
    CHECK_RC(FindJsolwal(header, "Version", m_pMiscInfo->P4Data.Version));

    return rc;
}

RC FuseJsonParser::ParseChipOptions(const JsonTree& chipOptions, bool isFpf)
{
    RC rc;

    // Iterate through all the fuses in Chip_options
    for (Value::ConstMemberIterator chipOptItr = chipOptions.MemberBegin();
         chipOptItr != chipOptions.MemberEnd(); ++chipOptItr)
    {
        // Make sure this fuse is actually a JSON object and not an array or value
        if (!chipOptItr->value.IsObject())
        {
            Printf(Tee::PriError,
                   "Bad format for fuse \"%s\" in Chip_options section of %s\n",
                   chipOptItr->name.GetString(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // Make sure there is not a duplicate fuse entry
        map<string, FuseDef>::const_iterator fuseItr;
        fuseItr = m_pFuseDefMap->find(Utility::ToUpperCase(chipOptItr->name.GetString()));
        if (fuseItr != m_pFuseDefMap->end())
        {
            Printf(Tee::PriError,
                   "Duplicate definition of fuse \"%s\" in %s\n",
                   chipOptItr->name.GetString(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // Populate a new FuseDef and add it to the FuseDefMap
        FuseDef newFuse;
        newFuse.Name = Utility::ToUpperCase(chipOptItr->name.GetString());
        newFuse.isInFpfMacro = isFpf;

        CHECK_RC(FindJsolwal(chipOptItr->value, "bits", newFuse.NumBits));
        CHECK_RC(FindJsolwal(chipOptItr->value, "lwstomer_visible", newFuse.Visibility));
        CHECK_RC(FindJsolwal(chipOptItr->value, "type", newFuse.Type));

        // Parse the offset field
        // This field can either be a string or an unsigned integer
        // So we can't just use FindJsolwal()
        if (!chipOptItr->value.HasMember("offset"))
        {
            Printf(Tee::PriError,
                   "Bad \"offset\" field of fuse \"%s\" in %s\n",
                   chipOptItr->name.GetString(),
                   m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }
        if (chipOptItr->value["offset"].IsString())
        {
            ParseOffset(chipOptItr->value["offset"].GetString(), newFuse.PrimaryOffset);
        }
        else if (chipOptItr->value["offset"].IsUint())
        {
            // This logic comes from FuseParser::ParseOffset()
            newFuse.PrimaryOffset.Offset = chipOptItr->value["offset"].GetUint();
            newFuse.PrimaryOffset.Msb = 31;
            newFuse.PrimaryOffset.Lsb = 0;
        }

        // Add the new fuse to the fuse definition list
        (*m_pFuseDefMap)[newFuse.Name] = newFuse;
    }

    return rc;
}

// Parses the Map_options section
// This only grabs the fuse_num and redundant_fuse_num (if one exists)
RC FuseJsonParser::ParseMapOptions(const JsonTree& mapOptions)
{
    RC rc;

    // Iterate through all the fuses in Chip_options
    for (Value::ConstMemberIterator mapOptItr = mapOptions.MemberBegin();
         mapOptItr != mapOptions.MemberEnd(); ++mapOptItr)
    {
        // Make sure this fuse is actually a JSON object and not an array or value
        if (!mapOptItr->value.IsObject())
        {
            Printf(Tee::PriError,
                   "Bad format for fuse \"%s\" in Map_options section of %s\n",
                   mapOptItr->name.GetString(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // Check to see that we parsed a fuse with the same name in Chip_options
        map<string, FuseDef>::iterator fuseItr;
        fuseItr = m_pFuseDefMap->find(Utility::ToUpperCase(mapOptItr->name.GetString()));
        if (fuseItr == m_pFuseDefMap->end())
        {
            Printf(Tee::PriError,
                   "Fuse \"%s\" is defined in Map_options but not Chip_options in %s\n",
                   mapOptItr->name.GetString(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // Took this logic from the FuseXmlParser
        // There's another section for parsing fuseless info
        if (fuseItr->second.Type == "fuseless" ||
            fuseItr->second.Type == "pseudo")
        {
            continue;
        }

        if (!mapOptItr->value.HasMember("fuse_num") ||
            !mapOptItr->value["fuse_num"].IsArray())
        {
            Printf(Tee::PriError,
                   "fuse_num field incorrect for %s in Map_options section of %s\n",
                   mapOptItr->name.GetString(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // There can be multiple entries for fuse_num, for example OPT_X_COORDINATE
        // The entries are sorted as an array of objects, and each object
        // describes only one FuseLoc
        for (SizeType ii = 0; ii < mapOptItr->value["fuse_num"].Size(); ++ii)
        {
            const JsonTree& fuseNumObj = mapOptItr->value["fuse_num"][ii];
            Value::ConstMemberIterator fuseLoc = fuseNumObj.MemberBegin();
            fuseItr->second.FuseNum.push_back(ParseFuseLocString(fuseLoc));
        }

        // If we have redundant fuse locations, add them to the FuseDef
        // There can be multiple locations just like fuse_num
        if (mapOptItr->value.HasMember("redundant_fuse_num") &&
            mapOptItr->value["redundant_fuse_num"].IsArray())
        {
            for (SizeType ii = 0; ii < mapOptItr->value["redundant_fuse_num"].Size(); ++ii)
            {
                const JsonTree& fuseNumObj = mapOptItr->value["redundant_fuse_num"][ii];
                Value::ConstMemberIterator fuseLoc = fuseNumObj.MemberBegin();
                fuseItr->second.RedundantFuse.push_back(ParseFuseLocString(fuseLoc));
            }
        }
    }

    return rc;
}

RC FuseJsonParser::ParseMktOptions(const JsonTree& mktOptions, bool isFpf)
{
    RC rc;

    // Iterate through all the SKUs in the Mkt_options section
    for (Value::ConstMemberIterator mktOptItr = mktOptions.MemberBegin();
         mktOptItr != mktOptions.MemberEnd(); ++mktOptItr)
    {
        // Skip over the descriptions
        if (!strcmp(mktOptItr->name.GetString(), "Description"))
        {
            continue;
        }

        // Check for duplicate SKUs
        SkuList::iterator skuIter;
        skuIter = m_pSkuList->find(mktOptItr->name.GetString());
        SkuConfig* pSku = nullptr;
        if (isFpf)
        {
            if (skuIter == m_pSkuList->end())
            {
                Printf(Tee::PriError,
                       "Entry for SKU %s in %s, but not in main fuse file\n",
                       mktOptItr->name.GetString(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }
            // Add data to the existing SKU
            pSku = &skuIter->second;
        }
        else
        {
            if (skuIter != m_pSkuList->end())
            {
                Printf(Tee::PriError,
                       "Duplicate entry for SKU %s in %s\n",
                       mktOptItr->name.GetString(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }
            // New SKU: add this SKU to the SkuList
            string name = mktOptItr->name.GetString();
            pSku = &((*m_pSkuList)[name]);
            pSku->SkuName = name;
        }

        // For each fuse in this SKU
        for (Value::ConstMemberIterator fuseItr = mktOptItr->value.MemberBegin();
             fuseItr != mktOptItr->value.MemberEnd(); ++fuseItr)
        {
            string fuseName = Utility::ToUpperCase(fuseItr->name.GetString());

            FuseDefMap::iterator fuseDefItr;
            fuseDefItr = m_pFuseDefMap->find(fuseName);
            if (fuseDefItr == m_pFuseDefMap->end())
            {
                Printf(Tee::PriError,
                       "Fuse %s in SKU %s was not previously defined in %s\n",
                       fuseName.c_str(), mktOptItr->name.GetString(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            if (!fuseItr->value.IsString())
            {
                Printf(Tee::PriError,
                       "Bad value for fuse %s in SKU %s in Mkt_options\n",
                       fuseName.c_str(), mktOptItr->name.GetString());
                return RC::CANNOT_PARSE_FILE;
            }

            FuseInfo fuseVal;
            fuseVal.StrValue = fuseItr->value.GetString();
            fuseVal.pFuseDef = &(fuseDefItr->second);

            // Add this FuseInfo to the FuseList for this SKU
            pSku->FuseList.push_back(fuseVal);
        }
    }

    return rc;
}

RC FuseJsonParser::ParseFuseEncodingOptions(const JsonTree& fuseOptions)
{
    RC rc;

    string buffer;
    CHECK_RC(FindJsonUintFromString(fuseOptions, "ramRepairFuseBlockBegin",
                                    &m_pMiscInfo->FuselessStart));
    CHECK_RC(FindJsonUintFromString(fuseOptions, "ramRepairFuseBlockEnd",
                                    &m_pMiscInfo->FuselessEnd));

    // Figure out all known subfuses from the configFields section
    map<string, FuseLoc> SubFuses;

    if (!fuseOptions.HasMember("configFields") ||
        !fuseOptions["configFields"].IsArray())
    {
        Printf(Tee::PriError, "Invalid configFields section in %s\n", m_FileName.c_str());
        return RC::CANNOT_PARSE_FILE;
    }

    CHECK_RC(ParseConfigFields(fuseOptions["configFields"], SubFuses));

    if (!fuseOptions.HasMember("fuselessFuse") ||
        !fuseOptions["fuselessFuse"].IsArray())
    {
        Printf(Tee::PriError, "Invalid fuselessFuse section in %s\n", m_FileName.c_str());
        return RC::CANNOT_PARSE_FILE;
    }

    CHECK_RC(ParseFuselessFuse(fuseOptions["fuselessFuse"], SubFuses));

    return rc;
}

RC FuseJsonParser::ParseFsTagMap(const JsonTree& tagsSection)
{
    // Expecting to parse JSON of the form
    //"Tag_map": {
    //  "OPT_GPC_DISABLE": {
    //    "Floorsweep_Tag": [
    //      "tag_FS_g0gpc",
    //      "tag_FS_g1gpc",
    //      "tag_FS_g2gpc",
    //      "tag_FS_g3gpc",
    //      "tag_FS_g4gpc",
    //      "tag_FS_g5gpc",
    //      "tag_FS_g6gpc",
    //      "tag_FS_g7gpc"
    //    ]
    //  },
    //  "OPT_LWDEC_DISABLE": {
    //    "Floorsweep_Tag": [
    //      "tag_FS_s0lwdec0",
    //      "tag_FS_s0lwdec1",
    //      "tag_FS_m0lwdec2",
    //      "tag_FS_m0lwdec3",
    //      "tag_FS_m0lwdec4"
    //    ]
    //  }
    //}
    // Notes: - we *must* preserve the order of the tags within a fuse block
    //        - the tags could be duplicate (eg in OPT_FBIO_DISABLE)
    MASSERT(m_pMiscInfo);
    map<string, vector<string>>& tagMap = m_pMiscInfo->tagMap;
    for (Value::ConstMemberIterator fuseNameItr = tagsSection.MemberBegin();
        fuseNameItr != tagsSection.MemberEnd();
        ++fuseNameItr)
    {
        const string entryName = fuseNameItr->name.GetString();
        constexpr char fieldName[] = "Floorsweep_Tag";
        constexpr char mispelledFieldName[] = "Floosweep_Tag";

        if ((!fuseNameItr->value.IsObject()) ||
            (!fuseNameItr->value.HasMember(fieldName)) ||
            (!fuseNameItr->value[fieldName].IsArray()))
        {
            // Remove this if when bug 2833548 is fixed
            if ((!fuseNameItr->value.IsObject()) ||
                (!fuseNameItr->value.HasMember(mispelledFieldName)) ||
                (!fuseNameItr->value[mispelledFieldName].IsArray()))
            {
                Printf(Tee::PriError,
                       "Bad format for tags of fuse '%s' in 'Tag_map' section of %s\n",
                       entryName.c_str(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }
        }

        if (tagMap.count(entryName))
        {
            Printf(Tee::PriError,
                   "Duplicate tags for fuse '%s' in 'Tag_map' section of %s\n",
                   entryName.c_str(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        vector<string> tags;
        // Remove this if when bug 2833548 is fixed
        const JsonTree& tagsObj = fuseNameItr->value.HasMember(fieldName) ?
                                  fuseNameItr->value[fieldName] :
                                  fuseNameItr->value[mispelledFieldName];
        tags.reserve(tagsObj.Size());

        for (SizeType ii = 0; ii < tagsObj.Size(); ++ii)
        {
            const JsonTree& tag = tagsObj[ii];
            if (!tag.IsString())
            {
                Printf(Tee::PriError, "Wrong format found for tag in fuse block"
                       "'%s' in 'Tag_map' section of %s\n",
                       entryName.c_str(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            tags.push_back(tag.GetString());
        }

        // Add all the tags associated with the fuse name to the map
        tagMap[entryName] = std::move(tags);
    }

    return RC::OK;
}

// "Floorsweeping_options": {
//   "family": {
//     "perfect_gpc_count": "6",
//     "perfect_tpc_count": "36",
//     "perfect_tpcs_per_gpc_count": "6",
//     "perfect_fbp_count": "6",
//     "perfect_half_fbpa_count": "6",
//     "perfect_rop_l2_per_fbp_count": "2"
//   },
//   "skus": {
//     "TU102-310-A1":
//       "gpc_count": "4",
//       "tpc_count": "20",
//       "fbp_count": "4",
//       "l2_count": "8"
//   }
// },
RC FuseJsonParser::ParseFloorsweepingOptions(const JsonTree& fsOptions)
{
    RC rc;

    // Get perfect unit counts
    CHECK_RC(CheckHasSection(fsOptions, "family"));
    const JsonTree& perfectCounts = fsOptions["family"];
    if (perfectCounts.MemberCount() == 0)
    {
        // If there isn't any data under "family", the Floorsweeping_options
        // section hasn't been populated. Floorsweeping will default to
        // use the opt_*_disable fuses for its settings
        Printf(Tee::PriWarn, "No perfect counts for Floorsweeping_options in %s. "
                             "Ignoring sku specific floorsweeping options, if any.\n",
                              m_FileName.c_str());
        return RC::OK;
    }
    string valStr;
    FloorsweepingUnits& fsUnits = m_pMiscInfo->perfectCounts;
    fsUnits.populated = true;

    CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_gpc_count", &fsUnits.gpcCount));
    CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_tpc_count", &fsUnits.tpcCount));
    CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_tpcs_per_gpc_count",
                                    &fsUnits.tpcsPerGpc));
    CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_fbp_count", &fsUnits.fbpCount));

    if (perfectCounts.HasMember("perfect_half_fbpa_count"))
    {
        CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_half_fbpa_count",
                                        &fsUnits.fbpaCount));
    }
    else
    {
        // Renamed GA10x onwards
        CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_fbpa_count",
                                        &fsUnits.fbpaCount));
    }

    if (perfectCounts.HasMember("perfect_rop_l2_per_fbp_count"))
    {
        CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_rop_l2_per_fbp_count",
                                        &fsUnits.l2sPerFbp));
    }
    else
    {
        // Renamed GA10x onwards
        CHECK_RC(FindJsonUintFromString(perfectCounts, "perfect_l2_per_fbp_count",
                                        &fsUnits.l2sPerFbp));
    }
    fsUnits.l2Count = fsUnits.l2sPerFbp * fsUnits.fbpCount;

    // GA10x+ fields
    CHECK_RC(TryFindJsonUintFromString(perfectCounts, "perfect_rop_per_gpc_count",
                                       &fsUnits.ropsPerGpc));
    CHECK_RC(TryFindJsonUintFromString(perfectCounts, "perfect_l2_slice_per_ltc_count",
                                       &fsUnits.l2SlicesPerLtc));
    fsUnits.ropCount     = fsUnits.ropsPerGpc * fsUnits.gpcCount;
    fsUnits.l2SliceCount = fsUnits.l2SlicesPerLtc * fsUnits.l2Count;

    // Parse out per-SKU unit counts
    if (!fsOptions.HasMember("skus") ||
        !fsOptions["skus"].IsObject())
    {
        Printf(Tee::PriError, "No skus sub-tree for Floorsweeping_options in %s\n",
               m_FileName.c_str());
        return RC::CANNOT_PARSE_FILE;
    }

    const JsonTree& skus = fsOptions["skus"];

    // Lambda to colwert a CSV list of indices to a vector<UINT32>
    // e.g. string("3, 7, 20") -> vector<UINT32>({ 3, 7, 20 });
    auto idxListToVector = [](const string& list, vector<UINT32>* pRtn) -> bool
    {
        pRtn->clear();
        if (list == "")
        {
            return true;
        }

        vector<string> numbers;
        if (Utility::Tokenizer(list, ",", &numbers) != RC::OK)
        {
            return false;
        }

        for (const auto& strNum: numbers)
        {
            const auto begin = strNum.find_first_not_of(" ");
            const auto end = strNum.find_last_not_of(" ");
            if (begin == strNum.npos || end == strNum.npos)
            {
                return false;
            }
            char* foundEnd = nullptr;
            const UINT32 num = strtoul(strNum.c_str() + begin, &foundEnd, 0);
            if (foundEnd == nullptr ||
                static_cast<size_t>(foundEnd - strNum.c_str()) != end + 1)
            {
                return false;
            }
            pRtn->push_back(num);
        }
        return true;
    };

    for (auto skuIter = skus.MemberBegin(); skuIter != skus.MemberEnd(); skuIter++)
    {
        string skuName = skuIter->name.GetString();
        auto skuCfgIter = m_pSkuList->find(skuName);
        if (skuCfgIter == m_pSkuList->end())
        {
            Printf(Tee::PriError, "SKU %s in Floorsweeping_options, but not in"
                " Mkt_options in %s\n", skuName.c_str(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        map<string, FsConfig>& fsSettings = skuCfgIter->second.fsSettings;

        const vector<string> fsFuses = { "gpc", "tpc", "pes", "rop", "cpc",
                                   "fbp", "fbpa", "l2", "l2_slice", 
                                   "sys_pipe", "lwdec", "lwjpg", "ioctrl"};
        set<string> processedFsSettings = {};

        for (auto unit : fsFuses)
        {
            FsConfig& fuseCfg = fsSettings[unit];

            const string unitLabel = unit + "_count";
            CHECK_RC(TryFindJsonUintFromString(skuIter->value, unitLabel, &fuseCfg.enableCount));
            processedFsSettings.insert(unitLabel);

            if (!skuIter->value.HasMember(unitLabel.c_str()))
            {
                // To differentiate between no enable count specified and a zero enable count
                fuseCfg.enableCount = FuseUtil::UNKNOWN_ENABLE_COUNT;
            }

            string enableList;
            string disableList;
            CHECK_RC(TryFindJsolwal(skuIter->value, unit + "_enabled_list", &enableList));
            CHECK_RC(TryFindJsolwal(skuIter->value, unit + "_disable_list", &disableList));
            processedFsSettings.insert(unit + "_enabled_list");
            processedFsSettings.insert(unit + "_disable_list");

            if (!idxListToVector(enableList, &fuseCfg.mustEnableList))
            {
                Printf(Tee::PriError, "Invalid entry in %s_enabled_list for SKU %s: %s\n",
                    unit.c_str(), skuName.c_str(), enableList.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            if (!idxListToVector(disableList, &fuseCfg.mustDisableList))
            {
                Printf(Tee::PriError, "Invalid entry in %s_disable_list for SKU %s: %s\n",
                    unit.c_str(), skuName.c_str(), disableList.c_str());
                return RC::CANNOT_PARSE_FILE;
            }
        }

        // Minimum enabled unit count allowed per enabled group
        // (e.g. minimum number of enabled TPCs per enabled GPCs)
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_tpc_per_gpc_count",
                                           &fsSettings["tpc"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_pes_per_gpc_count",
                                           &fsSettings["pes"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_cpc_per_gpc_count",
                                           &fsSettings["cpc"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_rop_per_gpc_count",
                                           &fsSettings["rop"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_fbpa_per_fbp_count",
                                           &fsSettings["fbpa"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_l2_per_fbp_count",
                                           &fsSettings["l2"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_sys_pipe_count",
                                           &fsSettings["sys_pipe"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_ioctrl_count",
                                           &fsSettings["ioctrl"].minEnablePerGroup));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_links_per_ioctrl",
                                           &fsSettings["perlink"].minEnablePerGroup));
        processedFsSettings.insert("min_tpc_per_gpc_count");
        processedFsSettings.insert("min_pes_per_gpc_count");
        processedFsSettings.insert("min_cpc_per_gpc_count");
        processedFsSettings.insert("min_rop_per_gpc_count");
        processedFsSettings.insert("min_fbpa_per_fbp_count");
        processedFsSettings.insert("min_l2_per_fbp_count");
        processedFsSettings.insert("min_sys_pipe_count");
        processedFsSettings.insert("min_ioctrl_count");
        processedFsSettings.insert("min_links_per_ioctrl");

        // Exact unit count allowed per enabled group
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "l2_slice_per_ltc_count",
                                           &fsSettings["l2_slice"].enableCountPerGroup));
        processedFsSettings.insert("l2_slice_per_ltc_count");

        // TPC reconfig values
        // The "reconfig" flag isn't always present due to bugs in the
        // fuse tool; check for "reconfig_min_tpcs" as a backup check.
        UINT32 reconfig = 0;
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "reconfig", &reconfig));
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "reconfig_min_tpcs",
                                           &fsSettings["tpc"].reconfigMinCount));
        processedFsSettings.insert("reconfig");
        processedFsSettings.insert("reconfig_min_tpcs");

        fsSettings["tpc"].hasReconfig = reconfig != 0 ||
                                        fsSettings["tpcs"].reconfigMinCount != 0;
        if (fsSettings["tpc"].hasReconfig)
        {
            CHECK_RC(TryFindJsonUintFromString(skuIter->value, "reconfig_tpcs",
                                               &fsSettings["tpc"].reconfigMaxCount));
            CHECK_RC(TryFindJsonUintFromString(skuIter->value, "reconfig_lwrrent_tpcs",
                                               &fsSettings["tpc"].reconfigLwrCount));
            processedFsSettings.insert("reconfig_tpcs");
            processedFsSettings.insert("reconfig_lwrrent_tpcs");

            CHECK_RC(TryFindJsonUintFromString(skuIter->value, "repair_min_tpcs",
                                               &fsSettings["tpc"].repairMinCount));
            CHECK_RC(TryFindJsonUintFromString(skuIter->value, "repair_max_tpcs",
                                               &fsSettings["tpc"].repairMaxCount));
            processedFsSettings.insert("repair_min_tpcs");
            processedFsSettings.insert("repair_max_tpcs");
        }

        // Min number of graphics TPCs (Hopper+, some TPCs are compute only)
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "min_graphics_tpcs",
                                           &fsSettings["tpc"].minGraphicsTpcs));
        processedFsSettings.insert("min_graphics_tpcs");

        // For HBM chips, whether half sites are allowed or not
        UINT32 fullSitesOnly = 0;
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "full_sites_only", &fullSitesOnly));
        fsSettings["fbp"].fullSitesOnly = fullSitesOnly != 0;
        processedFsSettings.insert("full_sites_only");

        // Max allowed difference in the number of enabled GPC accross UGPUs (Hopper+)
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "max_gpc_ugpu_imbalance", ~0U,
                                           &fsSettings["gpc"].maxUnitUgpuImbalance));
        processedFsSettings.insert("max_gpc_ugpu_imbalance");

        // Max allowed difference in the number of enabled TPC accross UGPUs (Hopper+)
        CHECK_RC(TryFindJsonUintFromString(skuIter->value, "max_tpc_ugpu_imbalance", ~0U,
                                           &fsSettings["tpc"].maxUnitUgpuImbalance));
        processedFsSettings.insert("max_tpc_ugpu_imbalance");

        // For vgpc skyline, refer: http://lwbugs/3359659
        string vgpcSkylineList;
        CHECK_RC(TryFindJsolwal(skuIter->value, "vgpc_skyline", &vgpcSkylineList));
        vector<UINT32> unsortedTmpList;
        if (!idxListToVector(vgpcSkylineList, &unsortedTmpList))
        {
            Printf(Tee::PriError, "Invalid entry in vgpc_skyline for SKU %s: %s\n",
                skuName.c_str(), vgpcSkylineList.c_str());
            return RC::CANNOT_PARSE_FILE;
        }
        sort(unsortedTmpList.begin(), unsortedTmpList.end());
        fsSettings["tpc"].vGpcSkylineList = unsortedTmpList;
        processedFsSettings.insert("vgpc_skyline");
        // vgpc_skyline will be populated only if has_vgpc_skyline is set, so is redundant
        processedFsSettings.insert("has_vgpc_skyline");

        // walk through the fs options provided to make sure they were all processed
        for (auto fsOptions = skuIter->value.MemberBegin();
             fsOptions != skuIter->value.MemberEnd(); fsOptions++)
        {
            if (processedFsSettings.count(fsOptions->name.GetString()) == 0)
            {
                Printf(Tee::PriWarn, "Invalid floorsweeping option: %s\n",
                                      fsOptions->name.GetString());
            }
        }
    }

    return rc;
}

// Gets fuse location for each subfuse
RC FuseJsonParser::ParseConfigFields(const JsonTree& configFields,
                                     map<string, FuseLoc>& subFuses)
{
    RC rc;

    for (SizeType ii = 0; ii < configFields.Size(); ++ii)
    {
        // Unfortunately all of these numeric values are still encoded
        // as strings in the JSON file, hence the strtoul() everywhere

        FuseLoc subFuse = {0};
        string buffer;

        CHECK_RC(FindJsolwal(configFields[ii], "blockIndex", buffer));
        subFuse.ChainOffset = strtoul(buffer.c_str(), nullptr, 10);
        CHECK_RC(FindJsolwal(configFields[ii], "blockOffset", buffer));
        subFuse.DataShift = strtoul(buffer.c_str(), nullptr, 10);
        CHECK_RC(FindJsolwal(configFields[ii], "chainId", buffer));
        subFuse.ChainId = strtoul(buffer.c_str(), nullptr, 10);
        FindJsolwal(configFields[ii], "lsbIndex", buffer);
        subFuse.Lsb = strtoul(buffer.c_str(), nullptr, 10);
        CHECK_RC(FindJsolwal(configFields[ii], "msbIndex", buffer));
        subFuse.Msb = strtoul(buffer.c_str(), nullptr, 10);

        CHECK_RC(FindJsolwal(configFields[ii], "name", buffer));

        subFuses[buffer] = subFuse;
    }

    return rc;
}

RC FuseJsonParser::ParseFuselessFuse(const JsonTree& fuselessFuse,
                                     map<string, FuseUtil::FuseLoc>& subFuses)
{
    RC rc;

    for (SizeType ii = 0; ii < fuselessFuse.Size(); ++ii)
    {
        // Make sure each fuseless fuse actually exists
        string fuseName;
        CHECK_RC(FindJsolwal(fuselessFuse[ii], "fuseName", fuseName));
        FuseDefMap::iterator fuseDefItr;
        fuseDefItr = m_pFuseDefMap->find(Utility::ToUpperCase(fuseName));

        if (fuseDefItr == m_pFuseDefMap->end())
        {
            Printf(Tee::PriError, "Fuseless fuse %s was not previously defined in %s\n",
                   fuseName.c_str(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        if (!fuselessFuse[ii].HasMember("jtagConfigFields") ||
            !fuselessFuse[ii]["jtagConfigFields"].IsArray())
        {
            Printf(Tee::PriError,
                   "Invalid jtagConfigFields section for fuse %s in %s\n",
                   fuseName.c_str(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        const JsonTree& jtagConfigFields = fuselessFuse[ii]["jtagConfigFields"];
        for (SizeType jj = 0; jj < jtagConfigFields.Size(); ++jj)
        {
            string subFuseName = jtagConfigFields[jj].GetString();

            // Make sure each subfuse for this fuse was previously
            // defined in configFields
            map<string, FuseLoc>::const_iterator subFuseItr;
            subFuseItr = subFuses.find(subFuseName);
            if (subFuseItr == subFuses.end())
            {
                Printf(Tee::PriError,
                       "Subfuse %s for fuse %s was not previously defined in %s\n",
                       subFuseName.c_str(), fuseName.c_str(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            // Add this subfuse location to the list of locations for this fuse
            // This subfuse location was previously parsed in ParseConfigFields()
            fuseDefItr->second.FuseNum.push_back(subFuseItr->second);
        }
    }

    return rc;
}

RC FuseJsonParser::ParsePseudoFuses(const JsonTree& pseudoFuses)
{
    RC rc;

    for (auto pseudoFuse = pseudoFuses.MemberBegin();
         pseudoFuse != pseudoFuses.MemberEnd();
         pseudoFuse++)
    {
        auto pseudoFuseName = Utility::ToUpperCase(pseudoFuse->name.GetString());

        // pseudo fuse defs should also exist in Chip_options, so grab that FuseDef
        if (m_pFuseDefMap->find(pseudoFuseName) == m_pFuseDefMap->end())
        {
            Printf(Tee::PriError, "Missing fuse definition: %s\n", pseudoFuseName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }
        FuseDef& pseudoDef = (*m_pFuseDefMap)[pseudoFuseName];
        pseudoDef.Type = FuseUtil::TYPE_PSEUDO;

        if (!pseudoFuse->value.IsArray())
        {
            Printf(Tee::PriError,
                "%s invalid! Pseudo fuse definitions must be arrays!\n", pseudoFuseName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        for (UINT32 i = 0; i < pseudoFuse->value.Size(); i++)
        {
            // Ex: "OPT_TPC_GPC0_DISABLE[6:0]"
            string subFuseStr = Utility::ToUpperCase(pseudoFuse->value[i].GetString());
            auto index = subFuseStr.find('[');
            string subFuseName = subFuseStr;

            // If bit limits aren't specified, use the full sub-fuse
            UINT32 upperLimit = 31;
            UINT32 lowerLimit = 0;
            if (index != string::npos)
            {
                subFuseName = subFuseName.substr(0, index);
                string rangeStr = subFuseStr.substr(index + 1, subFuseStr.size() - index - 2);

                char *pEnd;
                upperLimit = strtoul(rangeStr.c_str(), &pEnd, 10);
                pEnd++; // skip over ':'
                lowerLimit = strtoul(pEnd, nullptr, 10);
            }

            auto subFuseDef = m_pFuseDefMap->find(subFuseName);
            if (subFuseDef == m_pFuseDefMap->end())
            {
                Printf(Tee::PriError, "Subfuse %s not found!\n", subFuseName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            auto subFuseTuple = make_tuple(&(subFuseDef->second), upperLimit, lowerLimit);
            pseudoDef.subFuses.push_back(subFuseTuple);
        }
    }

    return rc;
}

RC FuseJsonParser::ParseBootromPatches(const JsonTree& bootromPatches)
{
    RC rc;

    // Iterate through all the SKUs in the Bootrom_patches section
    for (Value::ConstMemberIterator skuItr = bootromPatches.MemberBegin();
         skuItr != bootromPatches.MemberEnd(); ++skuItr)
    {
        string skuName = skuItr->name.GetString();

        SkuList::iterator skuConfItr = m_pSkuList->find(skuName);
        if (skuConfItr == m_pSkuList->end())
        {
            SkuConfig newSku;
            newSku.SkuName = skuName;
            (*m_pSkuList)[newSku.SkuName] = newSku;
            skuConfItr = m_pSkuList->find(newSku.SkuName);
        }

        // Iterate through each of the patches for this SKU
        for (Value::ConstMemberIterator brPatchItr = skuItr->value.MemberBegin();
             brPatchItr != skuItr->value.MemberEnd(); ++brPatchItr)
        {
            BRRevisionInfo newBRRev;
            newBRRev.VerName = brPatchItr->name.GetString();

            // the 'checksum' name value pair is listed after all the patches and
            // gets parsed as a new BR revision, when infact its a special field.
            // stop parsing BR revisions since its the last item after all the
            // BR revisions for a specific sku
            if (newBRRev.VerName == "checksum")
            {
                skuConfItr->second.checksum = brPatchItr->value.GetString();
                break;
            }

            UINT32 versionNum;
            FindJsolwal(brPatchItr->value, "sequence", versionNum);
            newBRRev.VerNo = Utility::StrPrintf("%u", versionNum);

            if (!brPatchItr->value.HasMember("rows") ||
                !brPatchItr->value["rows"].IsArray())
            {
                Printf(Tee::PriError,
                       "Invalid rows field in the bootrom patch for SKU %s in %s\n",
                       skuName.c_str(), m_FileName.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            // Each patch consists of many rows which contain an address and value
            const JsonTree& patchRows = brPatchItr->value["rows"];
            for (SizeType ii = 0; ii < patchRows.Size(); ++ii)
            {
                BRPatchRec newBRPatchRec;
                string valueStr;
                FindJsolwal(patchRows[ii], "value", valueStr);
                newBRPatchRec.Value = strtoul(valueStr.c_str(), nullptr, 16);
                FindJsolwal(patchRows[ii], "address", newBRPatchRec.Addr);
                newBRRev.BRPatches.push_back(newBRPatchRec);
            }

            // Add this patch to the list of BR patches for this SKU
            skuConfItr->second.BRRevs.push_back(newBRRev);
        }
    }

    return rc;
}

RC FuseJsonParser::ParseIFFPatches(const JsonTree& iffPatchSect)
{
    RC rc;

    // Iterate through all the SKUs in the IFF_patches section
    for (Value::ConstMemberIterator skuItr = iffPatchSect.MemberBegin();
         skuItr != iffPatchSect.MemberEnd(); ++skuItr)
    {
        string skuName = skuItr->name.GetString();
        FuseUtil::SkuList::iterator skuListItr = m_pSkuList->find(skuName);

        // If this SKU wasn't in Mkt_options section bail out here
        if (skuListItr == m_pSkuList->end())
        {
            Printf(Tee::PriError,
                   "SKU %s in IFF_patches section not found in Mkt_options\n",
                   skuName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // Iterate over each of the IFF patches for this SKU
        for (Value::ConstMemberIterator patchItr = skuItr->value.MemberBegin();
             patchItr != skuItr->value.MemberEnd(); ++patchItr)
        {
            IFFInfo iffInfo;

            iffInfo.name = patchItr->name.GetString();
            // Skip the checksum entry
            if (iffInfo.name == "checksum")
            {
                continue;
            }

            UINT32 val;
            CHECK_RC(FindJsolwal(patchItr->value, "sequence", val));
            iffInfo.num = Utility::StrPrintf("%u", val);

            if (!patchItr->value.HasMember("rows") ||
                !patchItr->value["rows"].IsArray())
            {
                Printf(Tee::PriError,
                       "IFF patch %s for SKU %s has invalid rows field\n",
                       patchItr->name.GetString(), skuItr->name.GetString());
                return RC::CANNOT_PARSE_FILE;
            }

            const JsonTree& row = patchItr->value["rows"];

            // Iterate through each of the rows in the patch
            for (SizeType ii = 0; ii < row.Size(); ++ii)
            {
                if (!row[ii].HasMember("value"))
                {
                    Printf(Tee::PriError,
                        "IFF entry for row %d patch %s for SKU %s is invalid\n",
                         ii, patchItr->name.GetString(), skuItr->name.GetString());
                    return RC::CANNOT_PARSE_FILE;
                }
                string rowStr = row[ii]["value"].GetString();
                FuseUtil::IFFRecord newRecord;
                if (rowStr == "ATE:")
                {
                    newRecord.dontCare = true;
                }
                else
                {
                    newRecord.data = strtoul(rowStr.c_str(), nullptr, 16);
                }
                iffInfo.rows.push_back(newRecord);
            }

            // Add this patch to the SKU
            skuListItr->second.iffPatches.push_back(iffInfo);
        }
    }

    return rc;
}

RC FuseJsonParser::ParseFuseMacro(const JsonTree& fuseMacro, bool isFpf)
{
    RC rc;
    FuseMacroInfo& macroInfo = isFpf ?
                               m_pMiscInfo->fpfMacroInfo :
                               m_pMiscInfo->MacroInfo;
    CHECK_RC(FindJsolwal(fuseMacro, "num_fuse_rows", macroInfo.FuseRows));
    CHECK_RC(FindJsolwal(fuseMacro, "num_fuse_cols", macroInfo.FuseCols));
    INT32 fuseRecordStart = 0;
    CHECK_RC(FindJsolwal(fuseMacro, "fuse_record_start", fuseRecordStart));
    if (fuseRecordStart < 0)
    {
        macroInfo.FuseRecordStart = macroInfo.FuseRows;
    }
    else
    {
        macroInfo.FuseRecordStart = static_cast<UINT32>(fuseRecordStart);
    }
    return rc;
}

// "Sku_ids:" {
//   "TU102-300-A1": 1,
//   "TU102-400-A1": 2,
//   }
RC FuseJsonParser::ParseSkuIds(const JsonTree& skuIds)
{
    RC rc;

    for (auto skuIter = skuIds.MemberBegin(); skuIter != skuIds.MemberEnd(); skuIter++)
    {
        string skuName = skuIter->name.GetString();
        auto skuCfgIter = m_pSkuList->find(skuName);
        if (skuCfgIter == m_pSkuList->end())
        {
            Printf(Tee::PriError, "SKU %s in Sku_ids, but not in Mkt_options in %s\n",
                                   skuName.c_str(), m_FileName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        CHECK_RC(FindJsolwal(skuIds, skuName, skuCfgIter->second.skuId));
    }

    return rc;
}

// "Por_data": {
//   "GP100-875-14-A1": {
//     "Bin_POR::CVB": {
//       "2": {
//         "Speedo": 1995,
//         "IDDQ": "23.13"
//       }
//     }
//   },
//   "GP100-875-24-A1": {
//     "Bin_POR::CVB": {
//       "1": {
//         "Speedo": 1805,
//         "IDDQ": "43.13"
//       }
//     }
//   },
RC FuseJsonParser::ParsePorData(const JsonTree& porData)
{
    RC rc;
    // Iterate through all the SKUs in the Por_data section
    for (Value::ConstMemberIterator skuItr = porData.MemberBegin();
         skuItr != porData.MemberEnd(); ++skuItr)
    {
        string skuName = skuItr->name.GetString();
        FuseUtil::SkuList::iterator skuListItr = m_pSkuList->find(skuName);

        // If this SKU wasn't in Mkt_options section skip this SKU
        if (skuListItr == m_pSkuList->end())
        {
            Printf(Tee::PriLow,
                   "SKU %s in Por_data section not found in Mkt_options. Skipping...\n",
                   skuName.c_str());
            continue;
        }

        if (!skuItr->value.HasMember("Bin_POR::CVB"))
        {
            Printf(Tee::PriLow,
                   "Por_data for SKU %s is missing Bin_POR::CVB field\n",
                   skuItr->name.GetString());
            continue;
        }

        const JsonTree& porBins = skuItr->value["Bin_POR::CVB"];

        for (Value::ConstMemberIterator binIter = porBins.MemberBegin();
             binIter != porBins.MemberEnd(); binIter++)
        {
            FuseUtil::PorBin bin;
            string alphaIndexStr = binIter->name.GetString();
            if (alphaIndexStr == "N/A")
            {
                bin.hasAlpha = false;
            }
            else
            {
                bin.hasAlpha = true;
                UINT32 alphaIdx = strtoul(alphaIndexStr.c_str(), nullptr, 10);
                if (alphaIdx >= m_pMiscInfo->alphaLines.size())
                {
                    Printf(Tee::PriError,
                        "POR bin for SKU %s uses invalid alpha line index: %d\n",
                        skuName.c_str(), alphaIdx);
                    return RC::CANNOT_PARSE_FILE;
                }
                bin.alphaLine = m_pMiscInfo->alphaLines[alphaIdx];
            }
            CHECK_RC(FindJsolwal(binIter->value, "Speedo", bin.speedoMin));
            CHECK_RC(FindJsolwal(binIter->value, "IDDQ", bin.iddqMax));
            skuListItr->second.porBinsMap[PORBIN_GPU].push_back(bin);
        }
    }

    return rc;
}

// "Slt_data": {
//   "alpha_eqns": {
//     "4": {
//       "Beta": "3.4",
//       "c": "28.95135",
//       "b": "0.00392",
//       "Fab": "TSMC"
//     },
//     "1": {
//       "Beta": "1.6",
//       "c": "21.70661",
//       "b": "0.00392",
//       "Fab": "TSMC"
//     },
RC FuseJsonParser::ParseSltData(const JsonTree& sltData)
{
    RC rc;

    if (!sltData.HasMember("alpha_eqns"))
    {
        Printf(Tee::PriError, "Slt_data is missing alpha_eqns\n");
        return RC::CANNOT_PARSE_FILE;
    }

    const JsonTree& eqns = sltData["alpha_eqns"];
    for (Value::ConstMemberIterator equation = eqns.MemberBegin();
         equation != eqns.MemberEnd(); equation++)
    {
        string indexStr = equation->name.GetString();
        UINT32 index = strtoul(indexStr.c_str(), nullptr, 10);

        FuseUtil::AlphaLine line;
        CHECK_RC(FindJsolwal(equation->value, "b", line.bVal));
        CHECK_RC(FindJsolwal(equation->value, "c", line.cVal));

        if (m_pMiscInfo->alphaLines.size() <= index)
        {
            m_pMiscInfo->alphaLines.resize(index + 1);
        }
        m_pMiscInfo->alphaLines[index] = line;
    }

    return rc;
}

//----------old version of POR.json------------
// {
//   "id": 3,
//   "Por_data": {
//     "GV100-893-A1": {
//       "sku_version": 3,
//       "por_properties": {
//         "Bin_POR::Sub_Bins": [
//           {
//             "Bin_Size": "98.7",
//             "Alpha": 3
//             "Beta": 3,
//             "b": "0.00369",
//             "c": "45.70933",
//             "Min_Speedo": 1804,
//             "Max_Speedo": 2067,
//             "Max_IDDQ": 80,
//             "Min_SRAM_Bin": 0,
//             "Min_KappaBin": -1,
//           }
//         ]
// } } } }
//---------new verison of POR.json-------------
// {
//   "id": 52,
//   "Por_data": {
//     "TU106-400-A1": {
//       "sku_version": 6,
//       "por_properties": {
//         "Bin_POR::Sub_Bins": {
//           "Sub_Bin_0": {
//             "Beta": 3,
//             "c": 20.68927,
//             "Speedo": {
//               "Max": 2321,
//               "Min": 1981
//             },
//             "b": 0.00367,
//             "SRAM_Bin": {
//               "Min": 0
//             },
//             "Bin_Size": 98.5,
//             "IDDQ": {
//               "Max": 78.0
//             },
//             "Alpha": 2
// }}}}}
RC FuseJsonParser::InterpretPorJson(const Document& doc)
{
    RC rc;

    CHECK_RC(CheckHasSection(doc, "Por_data"));
    const auto& porData = doc["Por_data"];

    for (auto skuItr = porData.MemberBegin();
         skuItr != porData.MemberEnd(); ++skuItr)
    {
        const string skuName = skuItr->name.GetString();
        auto skuListItr = m_pSkuList->find(skuName);

        // If this SKU wasn't in Mkt_options section skip this SKU
        if (skuListItr == m_pSkuList->end())
        {
            Printf(Tee::PriLow,
                   "SKU %s in Por_data section not found in Mkt_options\n",
                   skuName.c_str());
            continue;
        }

        auto& porBinsMap = skuListItr->second.porBinsMap;

        if (!porBinsMap.empty())
        {
            Printf(Tee::PriError, "Fuse.JSON also listed SKU %s's POR data!\n", skuName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        if (!skuItr->value.HasMember("por_properties"))
        {
            Printf(Tee::PriLow,
                   "Por_data for SKU %s is missing por_properties field\n",
                   skuItr->name.GetString());
            continue;
        }

        const auto& porProps = skuItr->value["por_properties"];

        for (auto porBinItr = porProps.MemberBegin();
                porBinItr != porProps.MemberEnd(); ++porBinItr)
        {
            const string& binName = porBinItr->name.GetString();
            PorBinType porBinType;

            if (binName == "Bin_POR::SOC::Sub_Bins")
            {
                porBinType = PORBIN_SOC;
            }
            else if (binName == "Bin_POR::CPU::Sub_Bins")
            {
                porBinType = PORBIN_CPU;
            }
            else if ((binName == "Bin_POR::GPU::Sub_Bins") ||
                     (binName == "Bin_POR::Sub_Bins"))
            {
                porBinType = PORBIN_GPU;
            }
            else if (binName == "Bin_POR::CV::Sub_Bins")
            {
                porBinType = PORBIN_CV;
            }
            else if (binName == "Bin_POR::FSI::Sub_Bins")
            {
                porBinType = PORBIN_FSI;
            }
            else
            {
                Printf(Tee::PriError,
                        "Unrecognized POR Bin Type %s for %s",
                         binName.c_str(),
                         skuItr->name.GetString());
                return RC::CANNOT_PARSE_FILE;
            }

            const Value& porBin = porProps[binName.c_str()];
            //In the old version of POR.json file, the sub_bins are put in an array
            if (porBin.IsArray())
            {
                // Defined SKUs need to have at least 1 sub-bin
                if (porBin.Size() == 0)
                {
                    Printf(Tee::PriError,
                        "Por bin empty in SKU %s\n",
                        skuItr->name.GetString());
                    return RC::CANNOT_PARSE_FILE;
                }

                for (UINT32 subBin = 0; subBin < porBin.Size(); subBin++)
                {
                    FuseUtil::PorBin bin;

                    bin.hasAlpha = true;
                    CHECK_RC(FindJsolwal(porBin[subBin], "b", bin.alphaLine.bVal));
                    CHECK_RC(FindJsolwal(porBin[subBin], "c", bin.alphaLine.cVal));
                    CHECK_RC(FindJsolwal(porBin[subBin], "Min_Speedo", bin.speedoMin));
                    CHECK_RC(FindJsolwal(porBin[subBin], "Max_Speedo", bin.speedoMax));
                    CHECK_RC(FindJsolwal(porBin[subBin], "Max_IDDQ", bin.iddqMax));
                    CHECK_RC(FindJsolwal(porBin[subBin], "Min_SRAM_Bin", bin.minSram));
                    if (porBin[subBin].HasMember("Min_KappaBin"))
                    {
                        CHECK_RC(FindJsolwal(porBin[subBin], "Min_KappaBin", bin.kappaVal));
                    }
                    bin.subBinId = subBin;
                    porBinsMap[porBinType].push_back(bin);
                }
            }
            //In the new version of POR.json file, the sub_bins appear as different Value
            else
            {
                if (porBin.ObjectEmpty())
                {
                    Printf(Tee::PriError,
                        "Por bin empty in SKU %s\n",
                        skuItr->name.GetString());
                    return RC::CANNOT_PARSE_FILE;
                }

                for (auto subBinItr = porBin.MemberBegin(); subBinItr != porBin.MemberEnd(); ++subBinItr)
                {
                    FuseUtil::PorBin bin;
                    bin.hasAlpha = true;
                    const Value& subBin = subBinItr->value;
                    const string subBinName = subBinItr->name.GetString();
                    if (subBin.HasMember("Kappa_Bin"))
                    {
                        const Value& kappa = subBin["Kappa_Bin"];
                        CHECK_RC(FindJsolwal(kappa, "Min", bin.kappaVal));
                    }
                    CHECK_RC(FindJsolwal(subBin, "b", bin.alphaLine.bVal));
                    CHECK_RC(FindJsolwal(subBin, "c", bin.alphaLine.cVal));
                    vector<string> vSpeedo = {"Speedo", "SOC_SPEEDO", "CPU_SPEEDO", "GPU_SPEEDO", "CV_SPEEDO"};
                    for (const auto& strSpeedo: vSpeedo)
                    {
                        if (subBin.HasMember(strSpeedo.c_str()))
                        {
                            const Value& speedo = subBin[strSpeedo.c_str()];
                            CHECK_RC(FindJsolwal(speedo, "Min", bin.speedoMin));
                            CHECK_RC(FindJsolwal(speedo, "Max", bin.speedoMax));
                            break;
                        }
                    }
                    vector<string> vSram_bin = {"SRAM_Bin", "SRAM_SOC", "SRAM_CPU", "SRAM_GPU", "SRAM_CV"};
                    for (const auto& strSram_bin: vSram_bin)
                    {
                        if (subBin.HasMember(strSram_bin.c_str()))
                        {
                            const Value& sram_bin = subBin[strSram_bin.c_str()];
                            CHECK_RC(FindJsolwal(sram_bin, "Min", bin.minSram));
                            if (strSram_bin != "SRAM_Bin")
                            {
                                CHECK_RC(FindJsolwal(sram_bin, "Max", bin.maxSram));
                            }
                            break;
                        }
                    }
                    vector<string> vIddq = {"IDDQ", "SOC_IDDQ", "CPU_IDDQ", "GPU_IDDQ", "CV_IDDQ"};
                    for (const auto& strIddq: vIddq)
                    {
                        if (subBin.HasMember(strIddq.c_str()))
                        {
                            const Value& iddq = subBin[strIddq.c_str()];
                            if (strIddq != "IDDQ")
                            {
                                CHECK_RC(FindJsolwal(iddq, "Min", bin.iddqMin));
                            }
                            CHECK_RC(FindJsolwal(iddq, "Max", bin.iddqMax));
                            break;
                        }
                    }

                    if (subBin.HasMember("HVT_Speedo"))
                    {
                        const Value& hvtSpeedo = subBin["HVT_Speedo"];
                        // hvtSpeedoMax is an optional param while hvtSpeedoMin is
                        // compulsory
                        CHECK_RC(FindJsolwal(hvtSpeedo, "Min", bin.hvtSpeedoMin));
                        CHECK_RC(TryFindJsolwal(hvtSpeedo, "Max", &bin.hvtSpeedoMax));
                    }
                    if (subBin.HasMember("HVTSVTRatio"))
                    {
                        const Value& hvtSvtRatio = subBin["HVTSVTRatio"];
                        CHECK_RC(FindJsolwal(hvtSvtRatio, "Min", bin.hvtSvtRatioMin));
                        CHECK_RC(FindJsolwal(hvtSvtRatio, "Max", bin.hvtSvtRatioMax));
                    }
                    UINT32 subBinId;
                    //Parse "Sub_Bin_X" to get the Id
                    MASSERT(subBinName.substr(0, 8) == "Sub_Bin_");
                    CHECK_RC(Utility::StringToUINT32(subBinName.substr(8, subBinName.size() - 8), &subBinId));
                    bin.subBinId = subBinId;
                    porBinsMap[porBinType].push_back(bin);
                }
            }
        }

        // check to make sure that atleast 1 GPU subbin was parsed correctly.
        if (porBinsMap.count(PORBIN_GPU) == 0)
        {
            Printf(Tee::PriLow,
                   "por_properties for SKU %s is missing Bin_POR::Sub_Bins field\n",
                   skuItr->name.GetString());
            continue;
        }
    }

    return rc;
}

RC FuseJsonParser::GetPorFileId(const string& fuseFileName, UINT32 *pId)
{
    RC rc;
    MASSERT(pId);

    string unredactedFile;
    string porFileName = fuseFileName.substr(0, fuseFileName.rfind("_f.json")) + "_POR.json";
    CHECK_RC(FindUnredactedFilename(porFileName, &unredactedFile));

    Document porDoc;
    CHECK_RC(ParseJson(unredactedFile, &porDoc));
    CHECK_RC(CheckHasSection(porDoc, "id"));
    if (porDoc["id"].IsUint())
    {
        *pId = porDoc["id"].GetUint();
    }
    else
    {
        return RC::BAD_FORMAT;
    }

    return rc;
}

JS_SMETHOD_LWSTOM(FuseJson,
                  GetPorFileId,
                  1,
                  "Get id from the given POR file")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    string        FileName;
    JSObject     *pData;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &pData)))
    {
        JS_ReportError(pContext, "Usage: FuseJson.GetPorFileId(Filename, pData)");
        return JS_FALSE;
    }

    RC rc;
    UINT32 id;
    auto pJsonParser = make_unique<FuseJsonParser>();
    C_CHECK_RC(pJsonParser->GetPorFileId(FileName, &id));
    RETURN_RC(pJavaScript->SetProperty(pData, "id", id));
}

// Given a Json value { "191": "31:0" }
// This will return a FuseLoc with Number = 191, Msb = 31, Lsb = 0
FuseUtil::FuseLoc FuseJsonParser::ParseFuseLocString(const Value::ConstMemberIterator& fuseLocItr)
{
    FuseLoc newFuseLoc = {0};
    newFuseLoc.Number = strtoul(fuseLocItr->name.GetString(), nullptr, 10);

    string msbColonLsb = fuseLocItr->value.GetString();

    char *pEnd;
    newFuseLoc.Msb = strtoul(msbColonLsb.c_str(), &pEnd, 10);
    pEnd++; // skip over ':'
    newFuseLoc.Lsb = strtoul(pEnd, nullptr, 10);

    return newFuseLoc;
}

RC FuseJsonParser::FindUnredactedFilename(const string& name, string *pFileName)
{
    // Look for the unredacted file name first so that users can drop-in unredacted versions
    string unredactedName = Utility::RestoreString(name);
    if (unredactedName != name)
    {
        if (Xp::DoesFileExist(Utility::DefaultFindFile(unredactedName, true)) ||
            Xp::DoesFileExist(Utility::DefaultFindFile(unredactedName + "e", true)))
        {
            *pFileName = unredactedName;
            return RC::OK;
        }
    }

    if (Xp::DoesFileExist(Utility::DefaultFindFile(name, true)) ||
        Xp::DoesFileExist(Utility::DefaultFindFile(name + "e", true)))
    {
        *pFileName = name;
    }
    else
    {
        return RC::FILE_DOES_NOT_EXIST;
    }

    return RC::OK;
};
