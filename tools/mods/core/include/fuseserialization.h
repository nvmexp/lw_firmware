/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <string>
#include <tuple>
#include <vector>

#include <boost/archive/archive_exception.hpp>
#include <boost/archive/basic_archive.hpp>
#include <boost/archive/detail/common_iarchive.hpp>
#include <boost/archive/detail/interface_iarchive.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/serialization/collection_size_type.hpp>
#include <boost/serialization/extended_type_info.hpp>
#include <boost/serialization/item_version_type.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/throw_exception.hpp>

#include "core/include/fuseutil.h"
#include "core/include/types.h"

class FuseInfoClosure
{
    friend class boost::serialization::access;
public:
    FuseInfoClosure(FuseUtil::FuseInfo &fuseInfo, FuseUtil::FuseDefMap &fuseDefMap)
      : m_fuseDefMap(fuseDefMap)
      , m_fuseInfo(fuseInfo)
    {}

private:
    template <class Archive>
    void load(Archive & ar, const unsigned int version)
    {
        ar >> m_fuseInfo.StrValue;
        ar >> m_fuseInfo.Value;
        ar >> m_fuseInfo.ValRanges;
        ar >> m_fuseInfo.Vals;
        string fuseName;
        ar >> fuseName;
        m_fuseInfo.pFuseDef = &m_fuseDefMap[fuseName];
        ar >> m_fuseInfo.Attribute;
    }

    template <class Archive>
    void save(Archive & ar, const unsigned int version) const
    {
        ar << m_fuseInfo.StrValue;
        ar << m_fuseInfo.Value;
        ar << m_fuseInfo.ValRanges;
        ar << m_fuseInfo.Vals;
        const string& fuseName = m_fuseInfo.pFuseDef->Name;
        ar << fuseName;
        ar << m_fuseInfo.Attribute;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    FuseUtil::FuseDefMap &m_fuseDefMap;
    FuseUtil::FuseInfo &m_fuseInfo;
};

// closure creation operator
FuseInfoClosure operator * (FuseUtil::FuseInfo &fuseInfo, FuseUtil::FuseDefMap &fuseDefMap)
{
    return FuseInfoClosure(fuseInfo, fuseDefMap);
}

class SkuConfigClosure
{
    friend class boost::serialization::access;
public:
    SkuConfigClosure(FuseUtil::SkuConfig &skuCfg, FuseUtil::FuseDefMap &fuseDefMap)
      : m_fuseDefMap(fuseDefMap)
      , m_skuConfig(skuCfg)
    {}

private:
    template <class Archive>
    void load(Archive & ar, const unsigned int version)
    {
        ar >> m_skuConfig.SkuName;
        size_t n;
        ar >> n;
        FuseUtil::FuseInfo fi;
        m_skuConfig.FuseList.clear();
        for (size_t i = 0; n > i; ++i)
        {
            auto closure = fi * m_fuseDefMap;
            ar >> closure;
            m_skuConfig.FuseList.push_back(fi);
        }
        ar >> m_skuConfig.BRRevs;
        ar >> m_skuConfig.iffPatches;

        ar >> m_skuConfig.porBinsMap;
    }

    template <class Archive>
    void save(Archive & ar, const unsigned int version) const
    {
        ar << m_skuConfig.SkuName;
        size_t n = m_skuConfig.FuseList.size();
        ar << n;
        for (auto &fi : m_skuConfig.FuseList)
        {
            ar << fi * m_fuseDefMap;
        }
        ar << m_skuConfig.BRRevs;
        ar << m_skuConfig.iffPatches;
        ar << m_skuConfig.porBinsMap;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    FuseUtil::FuseDefMap &m_fuseDefMap;
    FuseUtil::SkuConfig  &m_skuConfig;
};

// closure creation operator
SkuConfigClosure operator * (FuseUtil::SkuConfig &skuCfg, FuseUtil::FuseDefMap &fuseDefMap)
{
    return SkuConfigClosure(skuCfg, fuseDefMap);
}

class SubFuse
{
    friend class boost::serialization::access;
    friend class FusesGroup;
public:
    SubFuse() = default;

    SubFuse(const string &subFuseName, UINT32 upperLimit, UINT32 lowerLimit)
      : m_subFuseName(subFuseName)
      , m_upperLimit(upperLimit)
      , m_lowerLimit(lowerLimit)
    {}

    SubFuse(const tuple<FuseUtil::FuseDef*, UINT32, UINT32> &t)
        : m_subFuseName(get<0>(t)->Name)
        , m_upperLimit(get<1>(t))
        , m_lowerLimit(get<2>(t))
    {}

private:
    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_subFuseName;
        ar & m_upperLimit;
        ar & m_lowerLimit;
    }

    string m_subFuseName;
    UINT32 m_upperLimit;
    UINT32 m_lowerLimit;
};

struct PseudoFuse
{
    friend class boost::serialization::access;
    friend class FusesGroup;
public:
    typedef vector<SubFuse> Container;
    typedef Container::iterator iterator;
    typedef Container::const_iterator const_iterator;
    typedef Container::value_type value_type;

    iterator begin() { return m_subfuses.begin(); }
    const_iterator begin() const { return m_subfuses.begin(); }
    iterator end() { return m_subfuses.end(); }
    const_iterator end() const { return m_subfuses.end(); }

    const string& GetFuseName() const { return m_fuseName; }
    void SetFuseName(const string &val) { m_fuseName = val; }

    void push_back(const SubFuse &subFuse) { m_subfuses.push_back(subFuse); }

private:
    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_fuseName;
        ar & m_subfuses;
    }

    string          m_fuseName;
    vector<SubFuse> m_subfuses;
};

class FusesGroup
{
    friend class boost::serialization::access;
    template <class ChipSkus> friend class FusesGroupClosure;
public:
    FusesGroup(const string &gpuCodeName, FuseUtil::FuseDefMap &fuseDevMap,
               FuseUtil::SkuList &skuList, FuseUtil::MiscInfo &miscInfo)
      : m_GpuCodeName(gpuCodeName)
      , m_fuseDevMap(fuseDevMap)
      , m_skuList(skuList)
      , m_miscInfo(miscInfo)
    {}

    const string& GetGpuCodeName() const { return m_GpuCodeName; }

private:
    template <class Archive>
    void load(Archive & ar, const unsigned int version)
    {
        size_t n;
        string stringKey;
        FuseUtil::SkuConfig skuCfg;
        FuseUtil::FuseDef   fuseDef;

        ar >> m_GpuCodeName;

        m_fuseDevMap.clear();
        ar >> n;
        for (size_t i = 0; n > i; ++i)
        {
            ar >> stringKey;
            ar >> fuseDef;
            m_fuseDevMap[stringKey] = fuseDef;
        }

        m_skuList.clear();
        ar >> n;
        for (size_t i = 0; n > i; ++i)
        {
            ar >> stringKey;
            auto closure = skuCfg * m_fuseDevMap;
            ar >> closure;
            m_skuList[stringKey] = skuCfg;
        }

        ar >> m_miscInfo;

        vector<PseudoFuse> pseudoFuses;
        ar >> pseudoFuses;

        for (const auto &pf : pseudoFuses)
        {
            auto &fuseDef = m_fuseDevMap[pf.m_fuseName];
            for (const auto &sf : pf.m_subfuses)
            {
                auto &subFuseDef = m_fuseDevMap[sf.m_subFuseName];
                fuseDef.subFuses.push_back(make_tuple(&subFuseDef, sf.m_upperLimit, sf.m_lowerLimit));
            }
        }
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()

    string                m_GpuCodeName;
    FuseUtil::FuseDefMap &m_fuseDevMap;
    FuseUtil::SkuList    &m_skuList;
    FuseUtil::MiscInfo   &m_miscInfo;
};

template <class ChipSkus>
class FusesGroupClosure
{
    friend class boost::serialization::access;
public:
    FusesGroupClosure(FusesGroup &fusesGroup, ChipSkus &chipSkus)
      : m_FusesGroup(fusesGroup)
      , m_ChipSkus(chipSkus)
    {}

private:
    template <class Archive>
    void save(Archive & ar, const unsigned int version) const
    {
        size_t nSkus = 0;
        vector<string> usedDefs;
        for (const auto &skuCfg : m_FusesGroup.m_skuList)
        {
            if (m_ChipSkus.end() == m_ChipSkus.find(skuCfg.second.SkuName)) continue;
            ++nSkus;
            boost::transform(
                skuCfg.second.FuseList,
                std::back_inserter(usedDefs),
                [](const auto &fi) { return fi.pFuseDef->Name; });
        }

        vector<PseudoFuse> pseudoFuses;
        for (const auto &fuseName : usedDefs)
        {
            const auto &fuseDef = m_FusesGroup.m_fuseDevMap.find(fuseName)->second;
            if (!fuseDef.subFuses.empty())
            {
                PseudoFuse pseudoFuse;
                pseudoFuse.SetFuseName(fuseDef.Name);
                boost::copy(fuseDef.subFuses, back_inserter(pseudoFuse));
            }
        }

        ar << m_FusesGroup.m_GpuCodeName;

        size_t n = usedDefs.size();
        ar << n;
        for (const auto &fdName : usedDefs)
        {
            const auto &fuseDefPair = *m_FusesGroup.m_fuseDevMap.find(fdName);
            ar << fuseDefPair.first;
            ar << fuseDefPair.second;
        }

        ar << nSkus;
        for (auto &skuCfg : m_FusesGroup.m_skuList)
        {
            if (m_ChipSkus.end() == m_ChipSkus.find(skuCfg.second.SkuName)) continue;
            ar << skuCfg.first;
            ar << skuCfg.second * m_FusesGroup.m_fuseDevMap;
        }

        ar << m_FusesGroup.m_miscInfo;
        ar << pseudoFuses;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    FusesGroup &m_FusesGroup;
    ChipSkus   &m_ChipSkus;
};

// closure creation operator
template <class ChipSkus>
FusesGroupClosure<ChipSkus> operator * (FusesGroup &fg, ChipSkus &chipSkus)
{
    return FusesGroupClosure<ChipSkus>(fg, chipSkus);
}

namespace boost {
    namespace serialization {

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::OffsetInfo &oi, const unsigned int version)
        {
            ar & oi.Offset;
            ar & oi.Lsb;
            ar & oi.Msb;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::FuseLoc &fl, const unsigned int version)
        {
            ar & fl.Number;
            ar & fl.Lsb;
            ar & fl.Msb;
            ar & fl.ChainId;
            ar & fl.ChainOffset;
            ar & fl.DataShift;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::FuseDef &fd, const unsigned int version)
        {
            ar & fd.Name;
            ar & fd.Type;
            ar & fd.Visibility;
            ar & fd.NumBits;
            ar & fd.PrimaryOffset;
            ar & fd.FuseNum;
            ar & fd.RedundantFuse;
            // ignore subFuses, since it has pointers we can't restore here
            ar & fd.HammingECCType;
            ar & fd.Reload;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::FuseInfo::Range &range, const unsigned int version)
        {
            ar & range.Min;
            ar & range.Max;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::BRPatchRec &brPatchRec, const unsigned int version)
        {
            ar & brPatchRec.Addr;
            ar & brPatchRec.Value;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::BRRevisionInfo &brRevInfo, const unsigned int version)
        {
            ar & brRevInfo.VerName;
            ar & brRevInfo.VerNo;
            ar & brRevInfo.BRPatches;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::IFFRecord &iffRecord, const unsigned int version)
        {
            ar & iffRecord.data;
            ar & iffRecord.dontCare;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::IFFInfo &iffInfo, const unsigned int version)
        {
            ar & iffInfo.name;
            ar & iffInfo.num;
            ar & iffInfo.rows;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::PorBin &porBin, const unsigned int version)
        {
            ar & porBin.hasAlpha;
            ar & porBin.alphaLine;
            ar & porBin.speedoMin;
            ar & porBin.iddqMax;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::EccInfo &eccInfo, const unsigned int version)
        {
            ar & eccInfo.EccType;
            ar & eccInfo.FirstWord;
            ar & eccInfo.LastWord;
            ar & eccInfo.FirstBit;
            ar & eccInfo.LastBit;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::FuseMacroInfo &fmi, const unsigned int version)
        {
            ar & fmi.FuseRows;
            ar & fmi.FuseCols;
            ar & fmi.FuseRecordStart;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::AlphaLine &al, const unsigned int version)
        {
            ar & al.bVal;
            ar & al.cVal;
        }

        template <class Archive>
        void serialize(Archive & ar, FuseUtil::MiscInfo &mi, const unsigned int version)
        {
            ar & mi.FuselessStart;
            ar & mi.FuselessEnd;
            ar & mi.McpInfo;
            ar & mi.EccList;
            ar & mi.MacroInfo;
            ar & mi.alphaLines;
        }

    } // namespace serialization
} // namespace boost
