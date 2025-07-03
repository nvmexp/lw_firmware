/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/rmclkutil.h"

#include "uctexception.h"
#include "uctvflwrve.h"
#include "ucttestreference.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl2080/ctrl2080boardobj.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "uctdomain.h"

/*******************************************************************************
    SourceField
*******************************************************************************/

// Apply the source field to the p-state definition
uct::ExceptionList uct::TestSpec::SourceField::apply(uct::VfLwrve &vflwrve) const
{
    rmt::String argList = *this;
    rmt::String arg;
    RmtClockUtil clkUtil;
    UINT32 source;
    uct::VfLwrve::SourceSetting setting;
    DomainBitmap domainBitmap;
    size_type exceptionCount;
    ExceptionList status;
    //GpuSubdevice *m_pSubdevice;

    // Add a copy of this field for each domain.
    for (domainBitmap = DomainBitmap_Last; domainBitmap; domainBitmap >>= 1)
    {
        // Include only those domains specified in the statement.
        if (domainBitmap & domain)
        {
            setting.domain = BIT_IDX_32(domainBitmap);
        }
    }

    // Finish parsing each comma-delimited subfield
    while (!argList.empty())
    {
        exceptionCount = status.size();
        arg = argList.extractShard(',');

        source = clkUtil.GetClkSourceValue(arg.c_str());

        // Validate
        if(source == 0)
            status.push(ReaderException("Invalid clock source " + arg,
                *this, __PRETTY_FUNCTION__));

        // If this subfield had no exceptions, add it to the list.
        if (status.size() <= exceptionCount) {
            setting.source = source;
            vflwrve.settingVector.push(setting);
        }
    }

    // Done
    return status;
}

//
// Apply the name field to the vflwrve definition.
//
// Although the 'NameField' class is defined on field.h, the implementation of
// this function is here since it is specific to vflwrve definitions.
//
uct::ExceptionList uct::NameField::apply(VfLwrve &vflwrve,GpuSubdevice *m_pSubdevice) const
{
    //
    // Since 'name' starts the vflwrve, it's not possible to have a redudnancy.
    // Therefore, if this assert fires, the logic in uCT must have failed.
    //
    MASSERT(vflwrve.name.isEmpty());

    // Same the name into the trial spec.
    vflwrve.name = *this;
    return Exception();
}

uct::ExceptionList uct::FieldVector::apply(VfLwrve &vflwrve,GpuSubdevice *m_pSubdevice) const
{
    const_iterator ppField;
    ExceptionList status;

    MASSERT(blockType() == VfLwrveBlock);

    for (ppField = begin(); ppField != end(); ++ppField)
        status.push((*ppField)->apply(vflwrve,m_pSubdevice));

    if (status.isOkay() && vflwrve.name.empty())
        status.push(ReaderException("Anonymous vflwrve definition block",
            *this, __PRETTY_FUNCTION__));

    return status;
}

// Add a new setting object to this vector.
void uct::VfLwrve::Setting::Vector::push(const FreqSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Keep track of the union of all setting domains
    freqDomainSet |= BIT(setting.domain);

    // Add the setting to the list for this domain.
    freqDomainMap[setting.domain].push_back(setting);
}

// Add a new setting object to this vector.
void uct::VfLwrve::Setting::Vector::push(const VoltSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domailwolt < Domain_Count);

    // Keep track of the union of all setting domains
    voltDomainSet |= BIT(setting.domailwolt);

    // Add the setting to the list for this domain.
    voltDomainMap[setting.domailwolt].push_back(setting);
}

// Add a new setting object to this vector.
void uct::VfLwrve::Setting::Vector::push(const SourceSetting &setting)
{
    // The domain must have valid range to avoid overflopwing the array.
    MASSERT(setting.domain < Domain_Count);

    // Add the setting to the list for this domain.
    this->sourceDomainMap[setting.domain].push_back(setting);
}

// Number of elements in the cartesian product
// MSVC doesn't like "rmt::Vector<Setting>::size_type" because Setting is abstract.
unsigned long long uct::VfLwrve::Setting::Vector::cardinality() const
{
    Domain domain;
    unsigned long long cardinality = 0;
    rmt::Vector<FreqSetting>::size_type freqSize;
    rmt::Vector<VoltSetting>::size_type voltSize;

    // We can't apply settings that are not there!
    //MASSERT(!empty());

    // Just return the number of setting of the first non-empty ADC voltage domain
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        voltSize = voltDomainMap[domain].size();
        if (voltSize)
        {
            if (!cardinality)
            {
                // found the FIRST non-empty domain, set cardinality
                cardinality = voltDomainMap[domain].size();
            }
            else
            {
                // there is already a non-empty domain, assert that the sizes match
                MASSERT(cardinality == voltSize);
                freqSize = freqDomainMap[domain].size();
                if (freqSize)
                {
                    // Also if there is an associated frequency domain, then
                    // the size should match as well.
                    MASSERT(voltSize == freqSize);
                }
            }
        }
    }

    if (cardinality)
        return cardinality;

    // Just return the number of setting of the first non-empty LUT frequency domain
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        freqSize = freqDomainMap[domain].size();
        if (freqSize)
        {
            if (!cardinality)
            {
                // found the FIRST non-empty domain, set cardinality
                cardinality = freqDomainMap[domain].size();
            }
            else
            {
                // there is already a non-empty domain, assert that the sizes match
                MASSERT(cardinality == freqSize);
                voltSize = voltDomainMap[domain].size();
                if (voltSize)
                {
                    // Also if there is an associated ADC voltage domain, then
                    // the size should match as well.
                    MASSERT(voltSize == freqSize);
                }
            }
        }
    }

    return cardinality;
}

// Apply the 'i'th setting for the cardinality.
// This function is called by VfLwrve::fullPState.
uct::ExceptionList uct::VfLwrve::Setting::Vector::fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const
{
    Domain domain;
    ExceptionList status;

    // Apply a setting from each nonempty list of frequency domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        if (voltDomainMap[domain].size())
        {
            // Apply the numeric operator unless this frequency domain has been pruned.
            if (BIT(domain) & freqMask)
            {
                status.push(voltDomainMap[domain][i].fullPState(fullPState));
                if (freqDomainMap[domain].size())
                    status.push(freqDomainMap[domain][i].fullPState(fullPState));
            }
        }
        else if (freqDomainMap[domain].size())
        {
            // Apply the numeric operator unless this frequency domain has been pruned.
            if (BIT(domain) & freqMask)
            {
                status.push(freqDomainMap[domain][i].fullPState(fullPState));
                if (voltDomainMap[domain].size())
                    status.push(voltDomainMap[domain][i].fullPState(fullPState));
            }
        }
    }

    // The fully-defined p-state is invalid if we had any exceptions.
    if (!status.isOkay())
        fullPState.ilwalidate();

    // Done
    return status;
}

// Human-readable representation of this vflwrve
rmt::String uct::VfLwrve::Setting::Vector::string() const
{
    Domain domain;
    rmt::Vector<FreqSetting>::size_type freqSize, f;
    std::ostringstream oss;

    oss << " cardinality = ";

    // Get the settings from each nonempty list of frequency domain settings.
    for (domain = 0; domain < Domain_Count; ++domain)
    {
        // Ignore domains for which no settings have been specified.
        freqSize = freqDomainMap[domain].size();
        if (freqSize)
        {
            // Print the domain followed by the settings
            oss << freqDomainMap[domain].size() << " " << DomainNameMap::freq.text(BIT(domain)) << "={";
            oss << freqDomainMap[domain][0].string();
            for (f = 1; f < freqSize; ++f)
                oss << ", " << freqDomainMap[domain][f].string();
            oss << "}";
        }
    }

    // Done
    return oss.str();
}

// Virtual destructor -- nothing to do
uct::VfLwrve::Setting::~Setting()
{   }

/*******************************************************************************
    SourceSetting
*******************************************************************************/

// Apply this source setting to a fully-defined p-state.
// This function is called by VfLwrve::Setting::Vector::fullPState
uct::SolverException uct::VfLwrve::SourceSetting::fullPState(uct::FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domain < FreqDomain_Count);

    // At this point the source has already been validated
    fullPState.source[domain] = source;

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::VfLwrve::SourceSetting::string() const
{
    RmtClockUtil clkUtil;

    // Return the pertinent info as a string
    return rmt::String(clkUtil.GetClkSourceName(source));
}

/*******************************************************************************
    FreqSetting
*******************************************************************************/

// Tests as skipped for frequencies outside this range
const uct::NumericOperator::Range uct::VfLwrve::FreqSetting::range =
    { FREQ_RANGE_MIN, FREQ_RANGE_MAX };   // 0 < f <= 100GHz

// Human-readable representation of this setting
rmt::String uct::VfLwrve::FreqSetting::string() const
{
    // Return the pertinent info as a string
    return numeric->string();
}

// Frequency Text
rmt::String uct::VfLwrve::FreqSetting::dec(NumericOperator::value_t kilohertz)
{
    // Is it reasonable to colwert to gigahertz?
    if (kilohertz % 1000000 == 0)
        return rmt::String::dec(kilohertz / 1000000) + "GHz";

    // Is it reasonable to colwert to megaherta?
    if (kilohertz % 1000 == 0)
        return rmt::String::dec(kilohertz / 1000) + "MHz";

    // Kilohertz it is!
    return rmt::String::dec(kilohertz) + "KHz";
}

// Apply this frequency setting to a fully-defined p-state.
// This function is called by VfLwrve::Setting::Vector::fullPState
uct::SolverException uct::VfLwrve::FreqSetting::fullPState(uct::FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    UINT32 slaveMask        = 0;
    UINT32 clkVfRelIdxFirst = 0xFF;
    UINT32 clkVfRelIdxLast  = 0xFF;

    MASSERT(domain < VoltDomain_Count);

    // We must have a 'prior' value unless this is an absolute operator.
    if (!fullPState.freq[domain] && !numeric->isAbsolute())
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Absolute operator required because " +
            DomainNameMap::volt.text(BIT(domain)) +
            " is not specified in the base (vbios) p-state",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Make sure we won't overflow or underflow.
    if (!numeric->validRange(fullPState.freq[domain], range))
        return SolverException( fullPState.name.defaultTo("<anonymous>") +
            ": Result " + DomainNameMap::volt.text(BIT(domain)) +
            " frequency out of range (" + dec(range.min) + "," + dec(range.max) + "]",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Apply the numeric operator to the frequency.
    fullPState.freq[domain] = numeric->apply(fullPState.freq[domain]);
    fullPState.bVfLwrveBlock = true;

    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX(32, clkDomailwfLwrveIndex, &clkDomainsInfo.super.objMask.super)
    {
        if (clkDomainsInfo.domains[clkDomailwfLwrveIndex].domain & BIT(domain))
        {
            switch (clkDomainsInfo.domains[clkDomailwfLwrveIndex].super.type)
            {
                case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
                {
                    slaveMask = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v30Master.master.slaveIdxsMask;
                    break;
                }
                case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
                {
                    slaveMask = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v35Master.master.slaveIdxsMask;
                    break;
                }
                case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
                {
                    LwU8 railIdx;
                    FOR_EACH_INDEX_IN_MASK(32, railIdx, clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railMask)
                    {
                        if (clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railVfItem[railIdx].type ==
                            LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)
                        {
                            slaveMask        = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railVfItem[railIdx].data.master.slaveDomainsMask.super.pData[0];
                            clkVfRelIdxFirst = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railVfItem[railIdx].data.master.clkVfRelIdxFirst;
                            clkVfRelIdxLast  = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railVfItem[railIdx].data.master.clkVfRelIdxLast;
                        }
                    }
                    FOR_EACH_INDEX_IN_MASK_END;

                    break;
                }
                default:
                {
                    slaveMask = 0;
                    break;
                }
            }
            break;
        }
    }LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;

    // Making all the clkDomains freq to 0 and further setting only those which are specified in
    // ctp files.
    for (int j = 0; j < FreqDomain_Count; j++)
    {
        fullPState.freq[j] = 0;
    }

    fullPState.freq[domain] = numeric->apply(fullPState.freq[domain]);

    if (clkDomainsInfo.version == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40)
    {
        UINT32 clkVfRelIdxIdx;

        for(clkVfRelIdxIdx = clkVfRelIdxFirst; clkVfRelIdxIdx <= clkVfRelIdxLast; clkVfRelIdxIdx++)
        {
            UINT32 slaveIdx;
            UINT32 slaveEntryIdx;
            UINT32 freqMHz;
            UINT32 freqMaxMHz;
            UINT32 freqMasterRatioMaxMHz;
            UINT32 freqSlaveEnumEntryMaxMHz;

            switch (clkVfRelsInfo.vfRels[clkVfRelIdxIdx].type)
            {
                case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
                {
                    for (slaveEntryIdx = 0; slaveEntryIdx < LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRIES_MAX; slaveEntryIdx++)
                    {
                        FOR_EACH_INDEX_IN_MASK(32, slaveIdx, slaveMask)
                        {
                            MASSERT(clkDomainsInfo.domains[slaveIdx].super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG);

                            if(clkVfRelsInfo.vfRels[clkVfRelIdxIdx].data.ratioVolt.super.slaveEntries[slaveEntryIdx].clkDomIdx == slaveIdx)
                            {
                                freqMasterRatioMaxMHz = (clkVfRelsInfo.vfRels[clkVfRelIdxIdx].freqMaxMHz) * 
                                                         clkVfRelsInfo.vfRels[clkVfRelIdxIdx].data.ratioVolt.super.slaveEntries[slaveEntryIdx].ratio /
                                                         100;

                                //
                                // Now get the max frequency for this slave clock domain from its CLK_ENUM entry.
                                //

                                //
                                // Assumption here is that all the slaves of masters with VF rel type RATIO_VOLT
                                // would have single entries in enumeration table
                                //
                                MASSERT(clkDomainsInfo.domains[slaveIdx].data.v40Prog.clkEnumIdxFirst == clkDomainsInfo.domains[slaveIdx].data.v40Prog.clkEnumIdxLast);

                                freqSlaveEnumEntryMaxMHz = clkEnumsInfo.enums[clkDomainsInfo.domains[slaveIdx].data.v40Prog.clkEnumIdxFirst].freqMaxMHz;

                                // freqMaxMHz should be the min of freqMasterRatioMaxMHz & freqSlaveEnumEntryMaxMHz
                                freqMaxMHz = ((freqSlaveEnumEntryMaxMHz < freqMasterRatioMaxMHz) ? freqSlaveEnumEntryMaxMHz : freqMasterRatioMaxMHz);

                                freqMHz = (numeric->apply(fullPState.freq[domain]) / 1000) * 
                                          clkVfRelsInfo.vfRels[clkVfRelIdxIdx].data.ratioVolt.super.slaveEntries[slaveEntryIdx].ratio /
                                          100;
                                fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] = 
                                          ((freqMHz > freqMaxMHz) ? freqMaxMHz : freqMHz) * 1000;
                            }
                        }FOR_EACH_INDEX_IN_MASK_END;
                    }
                    break;
                }

                case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
                {
                    if ((clkVfRelsInfo.vfRels[clkVfRelIdxIdx].freqMaxMHz * 1000) != fullPState.freq[domain])
                    {
                        break;
                    }

                    for (slaveEntryIdx = 0; slaveEntryIdx < LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRIES_MAX; slaveEntryIdx++)
                    {
                        FOR_EACH_INDEX_IN_MASK(32, slaveIdx, slaveMask)
                        {
                            MASSERT(clkDomainsInfo.domains[slaveIdx].super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG);

                            if(clkVfRelsInfo.vfRels[clkVfRelIdxIdx].data.table.slaveEntries[slaveEntryIdx].clkDomIdx == slaveIdx)
                            {
                                freqMaxMHz = clkVfRelsInfo.vfRels[clkVfRelIdxIdx].data.table.slaveEntries[slaveEntryIdx].freqMHz;

                                fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] = freqMaxMHz;

                                //
                                // If clock domain is not PCIEGENCLK then store value in KHz
                                //
                                if (clkDomainsInfo.domains[slaveIdx].domain != LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)
                                {
                                    fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] *= 1000;
                                }
                            }
                        }FOR_EACH_INDEX_IN_MASK_END;
                    }
                    break;
                }

                default:
                {
                    continue;
                }
            }
        }
    }
    else
    {
        UINT32 clkProgsInfoIndex;

        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX(255, clkProgsInfoIndex, &clkProgsInfo.super.objMask.super)
        {
            UINT32 slaveIdx;
            UINT32 slaveProgIdx;
            UINT32 slaveEntryIdx;
            UINT32 freqMHz;
            UINT32 freqMaxMHz;

            switch(clkProgsInfo.progs[clkProgsInfoIndex].type)
            {
                case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO:
                    for (slaveEntryIdx = 0; slaveEntryIdx < LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES; slaveEntryIdx++)
                    {
                        FOR_EACH_INDEX_IN_MASK(32, slaveIdx, slaveMask)
                        {
                            MASSERT(clkDomainsInfo.domains[slaveIdx].super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY);
                            slaveProgIdx = clkDomainsInfo.domains[slaveIdx].data.v30Slave.super.super.clkProgIdxFirst;

                            if (slaveProgIdx != 0xFF)
                            {
                                freqMaxMHz = clkProgsInfo.progs[slaveProgIdx].data.v30.super.freqMaxMHz;
                            }
                            else
                            {
                                freqMaxMHz = UINT_MAX;
                            }

                            if(clkProgsInfo.progs[clkProgsInfoIndex].data.v30MasterRatio.ratio.slaveEntries[slaveEntryIdx].clkDomIdx == slaveIdx)
                            {
                                freqMHz = (numeric->apply(fullPState.freq[domain]) / 1000) *
                                          clkProgsInfo.progs[clkProgsInfoIndex].data.v30MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio /
                                          100;
                                fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] =
                                          ((freqMHz > freqMaxMHz) ? freqMaxMHz : freqMHz) * 1000;
                            }
                        }FOR_EACH_INDEX_IN_MASK_END;
                    }
                    break;

                case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
                    if (clkProgsInfo.progs[clkProgsInfoIndex].data.v30MasterTable.super.super.super.freqMaxMHz*1000 != fullPState.freq[domain])
                        break;
                    for (slaveEntryIdx = 0; slaveEntryIdx < LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES; slaveEntryIdx++)
                    {
                        FOR_EACH_INDEX_IN_MASK(32, slaveIdx, slaveMask)
                        {
                            MASSERT(clkDomainsInfo.domains[slaveIdx].super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY);
                            slaveProgIdx = clkDomainsInfo.domains[slaveIdx].data.v30Slave.super.super.clkProgIdxFirst;

                            if (slaveProgIdx != 0xFF)
                            {
                                freqMaxMHz = clkProgsInfo.progs[slaveProgIdx].data.v30.super.freqMaxMHz;
                            }
                            else
                            {
                                freqMaxMHz = UINT_MAX;
                            }

                            if(clkProgsInfo.progs[clkProgsInfoIndex].data.v30MasterTable.table.slaveEntries[slaveEntryIdx].clkDomIdx == slaveIdx)
                            {
                                freqMHz = clkProgsInfo.progs[clkProgsInfoIndex].data.v30MasterTable.table.slaveEntries[slaveEntryIdx].freqMHz;
                                fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] =
                                          (freqMHz > freqMaxMHz) ? freqMaxMHz : freqMHz;

                                //
                                // If clock domain is not PCIEGENCLK then store value in KHz
                                //
                                if (clkDomainsInfo.domains[slaveIdx].domain != LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)
                                {
                                    fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] *= 1000;
                                }
                            }
                        }FOR_EACH_INDEX_IN_MASK_END;
                    }
                    break;

                case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
                    for (slaveEntryIdx = 0; slaveEntryIdx < LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES; slaveEntryIdx++)
                    {
                        FOR_EACH_INDEX_IN_MASK(32, slaveIdx, slaveMask)
                        {
                            MASSERT(clkDomainsInfo.domains[slaveIdx].super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY);
                            slaveProgIdx = clkDomainsInfo.domains[slaveIdx].data.v35Slave.super.super.clkProgIdxFirst;

                            if (slaveProgIdx != 0xFF)
                            {
                                freqMaxMHz = clkProgsInfo.progs[slaveProgIdx].data.v35.super.freqMaxMHz;
                            }
                            else
                            {
                                freqMaxMHz = UINT_MAX;
                            }

                            if(clkProgsInfo.progs[clkProgsInfoIndex].data.v35MasterRatio.ratio.slaveEntries[slaveEntryIdx].clkDomIdx == slaveIdx)
                            {
                                freqMHz = (numeric->apply(fullPState.freq[domain]) / 1000) * 
                                          clkProgsInfo.progs[clkProgsInfoIndex].data.v35MasterRatio.ratio.slaveEntries[slaveEntryIdx].ratio /
                                          100;
                                fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] = 
                                          ((freqMHz > freqMaxMHz) ? freqMaxMHz : freqMHz) * 1000;
                            }
                        }FOR_EACH_INDEX_IN_MASK_END;
                    }
                    break;

                case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
                    if ((clkProgsInfo.progs[clkProgsInfoIndex].data.v35MasterTable.super.super.super.freqMaxMHz * 1000) != fullPState.freq[domain])
                        break;

                    for (slaveEntryIdx = 0; slaveEntryIdx < LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES; slaveEntryIdx++)
                    {
                        FOR_EACH_INDEX_IN_MASK(32, slaveIdx, slaveMask)
                        {
                            MASSERT(clkDomainsInfo.domains[slaveIdx].super.type == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY);
                            slaveProgIdx = clkDomainsInfo.domains[slaveIdx].data.v35Slave.super.super.clkProgIdxFirst;

                            if (slaveProgIdx != 0xFF)
                            {
                                freqMaxMHz = clkProgsInfo.progs[slaveProgIdx].data.v35.super.freqMaxMHz;
                            }
                            else
                            {
                                freqMaxMHz = UINT_MAX;
                            }

                            if(clkProgsInfo.progs[clkProgsInfoIndex].data.v35MasterTable.table.slaveEntries[slaveEntryIdx].clkDomIdx == slaveIdx)
                            {
                                freqMHz = clkProgsInfo.progs[clkProgsInfoIndex].data.v35MasterTable.table.slaveEntries[slaveEntryIdx].freqMHz;
                                fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] =
                                          (freqMHz > freqMaxMHz) ? freqMaxMHz : freqMHz;

                                //
                                // If clock domain is not PCIEGENCLK then store value in KHz
                                //
                                if (clkDomainsInfo.domains[slaveIdx].domain != LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)
                                {
                                    fullPState.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveIdx].domain)] *= 1000;
                                }
                            }
                        }FOR_EACH_INDEX_IN_MASK_END;
                    }
                    break;

                default:
                    continue;
            }
        }LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    }

    // No error
    return SolverException();
}

/*******************************************************************************
    VoltSetting
*******************************************************************************/

// Tests as skipped for voltages outside this range.
// Range TBD
const uct::NumericOperator::Range uct::VfLwrve::VoltSetting::range =
    { 0, 10000000 };        // 0 < v <= 10V

// Voltage Text
rmt::String uct::VfLwrve::VoltSetting::dec(NumericOperator::value_t microvolts)
{
    // Is it reasonable to colwert to volts?
    if (microvolts % 1000000 == 0)
        return rmt::String::dec(microvolts/ 1000000) + "V";

    // Is it reasonable to colwert to millivolts?
    if (microvolts % 1000 == 0)
        return rmt::String::dec(microvolts/ 1000) + "mV";

    // Microvolts it is!
    return rmt::String::dec(microvolts) + "uV";
}

// Apply this voltage setting to a fully-defined p-state.
// This function is called by VfLwrve::Setting::Vector::fullPState
// TBD Need to check if required
uct::SolverException uct::VfLwrve::VoltSetting::fullPState(FullPState &fullPState) const
{
    // Earlier syntax checking should ensure that the domain is valid.
    MASSERT(domailwolt < VoltDomain_Count);

    // We must have a 'prior' value unless this is an absolute operator.
    if (!fullPState.volt[domailwolt] && !numeric->isAbsolute())
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Absolute operator required because " +
            DomainNameMap::volt.text(BIT(domailwolt)) +
            " is not specified in the base p-state",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Make sure we won't overflow or underflow.
    if (!numeric->validRange(fullPState.volt[domailwolt], range))
        return SolverException(fullPState.name.defaultTo("<anonymous>") +
            ": Result " + DomainNameMap::volt.text(BIT(domailwolt)) +
            " voltage out of range (" + dec(range.min) + ", " + dec(range.max) + "]",
            fullPState.start, __PRETTY_FUNCTION__, Exception::Skipped);

    // Apply the numeric operator to the voltage.
    fullPState.volt[domailwolt] = numeric->apply(fullPState.volt[domailwolt]);

    // No error
    return SolverException();
}

// Human-readable representation of this setting
rmt::String uct::VfLwrve::VoltSetting::string() const
{
    // Return the pertinent info as a string
    return numeric->string();
}

/*******************************************************************************
    VfLwrveField
*******************************************************************************/

// Create a new field object.
uct::Field *uct::VfLwrve::VfLwrveField::newField(const CtpStatement &statement)
{
    return new VfLwrveField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::VfLwrve::VfLwrveField::clone() const
{
    return new VfLwrveField(*this);
}

uct::ExceptionList uct::VfLwrve::VfLwrveField::parse()
{
    ExceptionList status;
    if (!domain)
        status.push(ReaderException("Missing required voltage domain",
                *this, __PRETTY_FUNCTION__));

    // The argument is not optional
    if (empty())
        status.push(ReaderException("Missing required voltage specification",
                *this, __PRETTY_FUNCTION__));

    return status;
}

uct::ExceptionList uct::VfLwrve::VfLwrveField::apply(VfLwrve &vflwrve, GpuSubdevice *m_pSubdevice) const
{
    rmt::String argList = *this;
    rmt::String shard;
    NumericOperator::PointerOrException numeric;
    FreqSetting freqSetting;
    VoltSetting voltSetting;
    DomainBitmap domainBitmap;
    ExceptionList status;
    RC rc;
    long n=0;
    UINT32 maxSupportedVoltuV = UINT32_MAX;
    UINT32 voltIdx;

    bool bInitPState;
    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> vfPoints;
    ExceptionList refStatus;

    shard = argList.extractShard(',');

    // Is this a init PState?

    shard == "initpstate"? bInitPState = 1 : bInitPState = 0;

    //
    // VBIOS P-States have the form "vbiosp" <number>.  Complain if the reference
    // is neither a VBIOS P-State nor a init pstate
    //

    if (!shard.startsWith("vbiosp") && !shard.startsWith("initpstate"))
        status.push(SolverException(shard.defaultTo("[empty]") +
            ": Base of a p-state reference must be either a VBIOS PState or " +
            VbiosPState::initPStateName, __PRETTY_FUNCTION__));

    if (bInitPState)
    {
        m_pSubdevice->GetPerf()->GetVfPointsStatusForDomain(
                                     PerfUtil::ClkCtrl2080BitToGpuClkDomain(domain),&vfPoints);
    }

    // Determine the number used
    else
    {
        if (!bInitPState)
        {
            // Discard the text "vbios" and extract a (presumably decimal) number.
            rmt::String s = shard;
            s.erase(0, 6);
            n = s.extractLong();

            // P-State nunber must be and integer and must not have extraneous text
            if (errno || !s.empty())
                status.push(SolverException(shard.defaultTo("[empty]") +
                    ": Invalid VBIOS p-state index", __PRETTY_FUNCTION__));

            m_pSubdevice->GetPerf()->GetVfPointsAtPState(n,
                                     PerfUtil::ClkCtrl2080BitToGpuClkDomain(domain),&vfPoints);
        }
    }

    // Add a copy of this field for each domain.
    for (domainBitmap = DomainBitmap_Last; domainBitmap; domainBitmap >>= 1)
    {
        // Include only those domains specified in the statement.
        if (domainBitmap & domain)
        {
            // Setting::Vector::push copies
            freqSetting.domain = BIT_IDX_32(domainBitmap);
            if ((clkDomainsInfo.version == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40) &&
                (domainBitmap == LW2080_CTRL_CLK_DOMAIN_XBARCLK))
            {
                voltSetting.domailwolt = BIT_IDX_32(CLK_UCT_VOLT_DOMAIN_MSVDD);
            }
            else
            {
                voltSetting.domailwolt = BIT_IDX_32(CLK_UCT_VOLT_DOMAIN_LOGIC);
            }
        }
    }

    //
    // Check the max supported voltage on the rail we're concerned for this domain.
    // For now disable this check for DISPCLK as the initial GA10x VBIOSes seem to have
    // no supported frequencies needing voltage below maxLimit.
    //
    if (domain != LW2080_CTRL_CLK_DOMAIN_DISPCLK)
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(voltIdx, &voltRailsStatus.super.objMask.super)
        {
            if (voltRailsStatus.rails[voltIdx].super.type == ClkVoltDomainUctTo2080(BIT(voltSetting.domailwolt)))
            {
                maxSupportedVoltuV = voltRailsStatus.rails[voltIdx].maxLimituV;
                break;
            }
        } LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    }

    AbsoluteNumericOperator *pOperatorFreq = new AbsoluteNumericOperator;
    AbsoluteNumericOperator *pOperatorVolt = new AbsoluteNumericOperator;

    for (map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS>::const_iterator pointsItr = vfPoints.begin();
            pointsItr != vfPoints.end(); ++pointsItr)
    {
        pOperatorFreq->value = (AbsoluteNumericOperator::value_t)((pointsItr->second.freqMHz)*1000);
        pOperatorVolt->value = (AbsoluteNumericOperator::value_t)((pointsItr->second.voltageuV));
        freqSetting.numeric = pOperatorFreq;
        voltSetting.numeric = pOperatorVolt;

        // Add this VF tuple to our target list only if the needed voltage can be supported
        if (pOperatorVolt->value <= maxSupportedVoltuV)
        {
            vflwrve.settingVector.push(freqSetting);
            vflwrve.settingVector.push(voltSetting);
        }

    }
    return uct::ExceptionList();
}

// Simple human-readable representation (without file location)
rmt::String uct::VfLwrve::VfLwrveField::string() const
{
    if (isEmpty())
        return NULL;

    // This is a bit odd since we are not showing the parsed data.
    return "vbiosp0,vbiosp100";
}

bool uct::VfLwrve::supportPStateMode(const VbiosPState &vbios) const
{
    //
    // If this object contains any frequency (clock) domains or voltage domains
    // that are not defined in the VBIOS, then the fully-defined p-state that
    // results will also.  Therefore, it would not be possible to use the result
    // p-state to modify the VBIOS p-state for use in the p-state (perf) RMAPI.
    //
    return TRUE;
}

//
// Compute the 'i'th fully-defined p-state in the result set (cartesian
// product) using the specified p-state as the basis.
// This function is called by PStateReference::fullPState.
//
uct::ExceptionList uct::VfLwrve::fullPState(
    uct::FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    ExceptionList status = settingVector.fullPState(fullPState, i, freqMask);
    fullPState.name += rmt::String(":") + this->name;
    return status;
}

// Human-readable representation of this VfLwrve spec
rmt::String uct::VfLwrve::debugString(const char *info) const
{
    std::ostringstream oss;

    // File name and line number (location) if applicable
    oss << name.locationString();

    // Additional info if any
    if (info && *info)
        oss << info << ": ";

    // Trial name if available
    //if (!name.empty())
        oss << name << ": ";

    // Setting info
    //if (settingVector.empty())
    //    oss << "empty";
    //else
        oss << settingVector.string();

    // Done
    return oss.str();
}

