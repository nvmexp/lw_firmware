/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2018, 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "uctchip.h"

// Append the suffix list
void uct::ChipSuffix::append(Gpu::LwDeviceId devid)
{
    size_t pos, digit;
    rmt::String chip;

    // Colwert the chip name to lowercase and ensure it is all alphanumeric.
    // Examples herein use "gf119" as the chip.
    // diag/mods/gpu/include/gpulist.h contains a complete list of chips, but
    // documentation in that file discourages its use here.
    chip = Gpu::DeviceIdToString(devid);
    chip.keepOnlyAlnum();
    chip.lcase();
    MASSERT(!chip.empty());

    // Generate a list of suffixes based on the chip name.
    // The order is specific to general.
    // The local 'chip' is modified throughout this process.

    // Start with the most specific (e.g. ".gf119")
    push_back('.' + chip);

    // Handle cases like "gf106b" and "gf110f3" by truncating at the first
    // alphabetic that is followed by at least three numeric characters.
    // Ignore the first character, which is always alphabetic.
    digit = 0;
    for (pos = 1; pos < chip.length(); ++pos)
    {
        // Count the digits we've found
        if (isdigit(chip.at(pos)))
            ++digit;

        // First alpha after a digit, so truncate and exit the loop
        else if (digit >= 3)
        {
            chip.erase(pos);
            push_back('.' + chip);
            break;
        }
    }

    // Colwert each digit from right to left to 'x' and add to the list.
    // Always preserve the first two characters.
    // Examples: ".gf11x", ".gf1xx", ".gfxxx"
    pos = chip.length();
    while (--pos > 1 && isdigit(chip[pos]))
    {
        chip[pos] = 'x';
        push_back('.' + chip);
    }

    // Add names the family names to the list.  (e.g. ".fermi")
    // Note that, for example, ".fermi" has the same scope as ".gfxxx".
    switch (Gpu::DeviceIdToFamily(devid))
    {
        case GpuDevice::Celsius:    push_back(".celsius");  break;
        case GpuDevice::Rankine:    push_back(".rankine");  break;
        case GpuDevice::Lwrie:      push_back(".lwrie");    break;
        case GpuDevice::Tesla:      push_back(".tesla");    break;
        case GpuDevice::Fermi:      push_back(".fermi");    break;
        case GpuDevice::Kepler:     push_back(".kepler");   break;
        case GpuDevice::Maxwell:    push_back(".maxwell");  break;
        case GpuDevice::Pascal:     push_back(".pascal");   break;
        case GpuDevice::Volta:      push_back(".volta");    break;
        case GpuDevice::Turing:     push_back(".turing");   break;
        case GpuDevice::Ampere:     push_back(".ampere");   break;
        case GpuDevice::Ada:        push_back(".ada");      break;
        case GpuDevice::Hopper:     push_back(".hopper");   break;
     }

    // Finally, there is the global default file with no family spec.
    push_back("");
}

// Determine the suffix list
void uct::PlatformSuffix::initialize(GpuSubdevice *pSubdevice)
{
    iterator pSuffix;
    rmt::String platform      = NULL;
    rmt::String pstateKeyword = NULL;
    Gpu::LwDeviceId devid     = pSubdevice->DeviceId();
    size_t count, iterate;

    // Get the emulation/simulation mode text
    MASSERT(pSubdevice);
    if (pSubdevice->IsEmulation())
        platform = ".emu";
    else switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:    platform = ".hw";   break;
        case Platform::RTL:         platform = ".rtl";  break;
        case Platform::Fmodel:      platform = ".cmod"; break;
        case Platform::Amodel:      platform = ".amod"; break;
    }

    // Initialize this list to the chip-specific suffixes.
    ChipSuffix::initialize(devid);

    // Prefix the platform-specific suffix on each one.
    for (pSuffix = begin(); pSuffix != end(); ++pSuffix)
        *pSuffix = platform + *pSuffix;

    // Append another copy of the chip-specific suffixes.
    ChipSuffix::append(devid);

    for (pSuffix = begin(), count = 0; pSuffix != end(); ++pSuffix , ++count);

    if (m_PstateSupported)
    {
        pstateKeyword = ".pstate";
    }
    else
    {
        pstateKeyword = ".nonpstate";
    }

    // Prefix the pstate-specific suffix on each one.
    for (pSuffix = begin(), iterate = 0; iterate < count; ++pSuffix, iterate++)
        push_back(pstateKeyword + *pSuffix);

    // Show the prefixes we'll us in the file names.
    Printf(Tee::PriHigh, "CTP name suffixes: %s  (%s)\n", concatenate().c_str(), __FUNCTION__);
}


// Initialize the set of programmable clock domains
// 'exclude' is UniversalClockTest::m_Exclude based on the -exclude command-line parameter.
uct::ExceptionList uct::ChipContext::initialize(GpuSubdevice *pSubdevice, rmt::String excludeText)
{
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS params;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(pSubdevice);
    RC rc;
    ExceptionList status;

    // Get the set of clock domains
    params.clkDomains       = LW2080_CTRL_CLK_DOMAIN_UNDEFINED;
    params.clkDomainsType   = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;
    rc = pLwRm->Control(pRmHandle, LW2080_CTRL_CMD_CLK_GET_DOMAINS,
        &params, sizeof(LW2080_CTRL_CLK_GET_DOMAINS_PARAMS));

    // Save the domain set unless there is an error.
    supportedFreqDomainSet = (rc == OK? params.clkDomains : DomainBitmap_None);

    // Check for errors
    if (rc != OK)
        status.push(Exception(rc, "LW2080_CTRL_CMD_CLK_GET_DOMAINS",
            __PRETTY_FUNCTION__, Exception::Fatal));

    // We must have at least one clock domain.  Otherwise UCT is pointless.
    else if (!supportedFreqDomainSet)
        status.push(Exception("Unable to continue without programmable clock domains",
            __PRETTY_FUNCTION__, Exception::Fatal));

    // Parse the list of excluded domains.
    rmt::String s;
    FreqDomainBitmap excludeBitmap = 0x00000000u;
    while (!excludeText.empty())
    {
        // Get the next alphanumeric clock domain.
        s = excludeText.extractShardOf(rmt::String::alnum);
        if (!s.empty())
        {
            // Add to the bitmap (if defined).
            UINT32 b = uct::DomainNameMap::freq.bit(s);
            if (!b)
                status.push(Exception(s + ": No such clock domain exists for '-exclude_clock' parameter.",
                    __PRETTY_FUNCTION__, Exception::Fatal));
            else
                excludeBitmap |= b;
        }
    }

    // At least one domain has been excluded.
    if (excludeBitmap)
    {
        // Print the list of programmable domains before excluding any.
        Printf(Tee::PriHigh, "Programmable Frequency (Clock) Domains: %s  (%s)\n",
            DomainNameMap::freq.textList(supportedFreqDomainSet).concatenate().c_str(),
            rmt::String::function(__PRETTY_FUNCTION__).c_str());
        Printf(Tee::PriHigh, "Excluded Frequency (Clock) Domains: %s  (%s)\n",
            DomainNameMap::freq.textList(excludeBitmap).concatenate().c_str(),
            rmt::String::function(__PRETTY_FUNCTION__).c_str());

        // Remove excluded domains from the available ones.
        supportedFreqDomainSet &= ~excludeBitmap;
    }

    Printf(Tee::PriHigh, "Supported Frequency (Clock) Domains: %s  (%s)\n",
        DomainNameMap::freq.textList(supportedFreqDomainSet).concatenate().c_str(),
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Done -- return null exception
    return status;
}

