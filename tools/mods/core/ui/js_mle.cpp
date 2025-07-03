/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/errormap.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/mle_protobuf.h"
#ifdef INCLUDE_GPU
#include "gpu/perf/perfsub.h"
#endif
#include <exception>

JS_CLASS(Mle);

static SObject Mle_Object
(
    "Mle",
    MleClass,
    0,
    0,
    "MLE class"
);

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintTaggedString,
    3,
    "PrintTaggedString"
)
{
    MASSERT(pContext     != nullptr);
    MASSERT(pArguments   != nullptr);
    MASSERT(pReturlwalue != nullptr);

    JavaScriptPtr pJs;
    string tag;
    string str;
    INT32 gpu = -1;
    if ((NumArguments != 2 && NumArguments != 3) ||
       (OK != pJs->FromJsval(pArguments[0], &tag)) ||
       (OK != pJs->FromJsval(pArguments[1], &str)) ||
       (NumArguments == 3 && OK != pJs->FromJsval(pArguments[2], &gpu)))
    {
        pJs->Throw(pContext, RC::ILWALID_ARGUMENT,
            "Usage: Mle.PrintTaggedString(tag, string[, gpu])");
        return JS_FALSE;
    }

    ErrorMap::ScopedDevInst setDevInst(gpu);

    const auto colonPos = str.find(':');
    string key;
    if (colonPos != str.npos)
    {
        key = str.substr(0, colonPos);
        str = str.substr(colonPos + 1);
    }
    Mle::Print(Mle::Entry::tagged_str)
        .tag(tag)
        .key(key)
        .value(str);

    return JS_TRUE;
}

// This function gets called when the JS equivalent Mle.PrintRc gets triggered
// It parses the MODS error code and passes the pieces of information to Mle::RcMetadata
JS_SMETHOD_LWSTOM
(
    Mle,
    PrintRc,
    2,
    "PrintRc"
)
{
#ifdef INCLUDE_GPU
    MASSERT(pContext     != nullptr);
    MASSERT(pArguments   != nullptr);
    MASSERT(pReturlwalue != nullptr);

    JavaScriptPtr pJs;
    string modsErrorCode;
    INT32 devInst = -1;

    if ((NumArguments != 1 && NumArguments != 2) ||
       (RC::OK != pJs->FromJsval(pArguments[0], &modsErrorCode)) ||
       (NumArguments == 2 && RC::OK != pJs->FromJsval(pArguments[1], &devInst)))
    {
        pJs->Throw(pContext, RC::ILWALID_ARGUMENT,
            "Usage: Mle.PrintRc(modsErrorCode[, devInst])");
        return JS_FALSE;
    }

    UINT64 perfTestType = std::strtoul(modsErrorCode.substr(0,1).c_str(), nullptr, 10);
    UINT64 perfPointType = std::strtoul(modsErrorCode.substr(1,1).c_str(), nullptr, 10);
    UINT64 intersectRailType = std::strtoul(modsErrorCode.substr(2,1).c_str(), nullptr, 10);
    UINT64 clkDomainIntersect = std::strtoul(modsErrorCode.substr(3,1).c_str(), nullptr, 10);
    UINT32 errorCode = std::atoi(modsErrorCode.substr(9,3).c_str());

    Mle::ExitCode::Domain clkDomainIntersectEnum = Mle::ExitCode::unknown_dom;
    switch (static_cast<Perf::ClkDomainForIntersect>(clkDomainIntersect))
    {
        case Perf::ClkDomainForIntersect::Gpc2Clk:       clkDomainIntersectEnum = Mle::ExitCode::gpc2; break;
        case Perf::ClkDomainForIntersect::DispClk:       clkDomainIntersectEnum = Mle::ExitCode::disp; break;
        case Perf::ClkDomainForIntersect::DramClk:       clkDomainIntersectEnum = Mle::ExitCode::dram; break;
        case Perf::ClkDomainForIntersect::NotApplicable: clkDomainIntersectEnum = Mle::ExitCode::not_applicable; break;
        case Perf::ClkDomainForIntersect::Unknown:       clkDomainIntersectEnum = Mle::ExitCode::unknown_dom; break;
        default:
            Printf(Tee::PriLow, "Unknown clock Domain Intersect colwersion for %llu\n", clkDomainIntersect);
            break;
    }

    Mle::ExitCode::Rail intersectRailEnum = Mle::ExitCode::unknown_rail;
    switch (static_cast<Perf::IntersectRail>(intersectRailType))
    {
        case Perf::IntersectRail::Logic:   intersectRailEnum = Mle::ExitCode::logic; break;
        case Perf::IntersectRail::Sram:    intersectRailEnum = Mle::ExitCode::sram; break;
        case Perf::IntersectRail::Msvdd:   intersectRailEnum = Mle::ExitCode::msvdd; break;
        case Perf::IntersectRail::Unknown: intersectRailEnum = Mle::ExitCode::unknown_rail; break;
        default:
            Printf(Tee::PriLow, "Unknown intersectType colwersion for %llu\n", intersectRailType);
            break;
    }

    Mle::ExitCode::PerfPoint perfPointEnum = Mle::ExitCode::explicit_point;
    switch (static_cast<Perf::PerfPointType>(perfPointType))
    {
        case Perf::PerfPointType::Explicit:          perfPointEnum = Mle::ExitCode::explicit_point; break;
        case Perf::PerfPointType::Min:               perfPointEnum = Mle::ExitCode::min; break;
        case Perf::PerfPointType::Max:               perfPointEnum = Mle::ExitCode::max; break;
        case Perf::PerfPointType::Tdp:               perfPointEnum = Mle::ExitCode::tdp; break;
        case Perf::PerfPointType::Turbo:             perfPointEnum = Mle::ExitCode::turbo; break;
        case Perf::PerfPointType::IntersectVolt:     perfPointEnum = Mle::ExitCode::intersect_v; break;
        case Perf::PerfPointType::IntersectVoltFreq: perfPointEnum = Mle::ExitCode::intersect_v_f; break;
        case Perf::PerfPointType::IntersectPState:   perfPointEnum = Mle::ExitCode::intersect_pstate; break;
        case Perf::PerfPointType::IntersectVPState:  perfPointEnum = Mle::ExitCode::intersect_v_pstate; break;
        case Perf::PerfPointType::MultipleIntersect: perfPointEnum = Mle::ExitCode::multiple_intersect; break;
        default:
            Printf(Tee::PriLow, "Unknown perfType colwersion for %llu\n", perfPointType);
            break;
    }

    ErrorMap eMap(errorCode);
    ErrorMap::ScopedTestNum setTestNum(atoi(modsErrorCode.substr(6,3).c_str()));
    ErrorMap::ScopedDevInst setDevInst(devInst);
    Mle::Print(Mle::Entry::exit_code)
        .perf_test_type(static_cast<Mle::ExitCode::PerfTestType>(perfTestType))
        .perf_point_type(perfPointEnum)
        .intersect_rail(intersectRailEnum)
        .intersect_clock_domain(clkDomainIntersectEnum)
        .pstate_num(std::strtoul(modsErrorCode.substr(4,2).c_str(), nullptr, 10))
        .error_code(errorCode)
        .error_msg(eMap.Message());
#endif
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintThermalResistanceSummary,
    12,
    "PrintThermalResistanceSummary"
)
{
    STEST_HEADER(12, 12, "");

    STEST_ARG(0, string, thermalChannelName);
    STEST_ARG(1, FLOAT32, finalR);
    STEST_ARG(2, FLOAT32, maxR);
    STEST_ARG(3, FLOAT32, minR);
    STEST_ARG(4, FLOAT32, finalTemp);
    STEST_ARG(5, FLOAT32, idleTemp);
    STEST_ARG(6, FLOAT32, tempDelta);
    STEST_ARG(7, UINT32, finalPower);
    STEST_ARG(8, UINT32, idlePower);
    STEST_ARG(9, INT32, powerDelta);
    STEST_ARG(10, UINT32, checkpointTimeMs);
    STEST_ARG(11, UINT32, checkpointIndex);

    Mle::Print(Mle::Entry::thermal_resistance)
        .therm_channel(thermalChannelName)
        .final_r(finalR)
        .max_r(maxR)
        .min_r(minR)
        .final_temp(finalTemp)
        .idle_temp(idleTemp)
        .temp_delta(tempDelta)
        .final_power(finalPower)
        .idle_power(idlePower)
        .power_delta(powerDelta)
        .checkpt_time_ms(checkpointTimeMs)
        .checkpt_idx(checkpointIndex);

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintFanSanityTestSummary,
    5,
    "PrintFanSanityTestSummary"
)
{
    STEST_HEADER(5, 5, "");

    STEST_ARG(0, UINT08, fanIdx);
    STEST_ARG(1, bool, isMinFanSpeedCheck);
    STEST_ARG(2, UINT32, measuredFanRpm);
    STEST_ARG(3, UINT32, expectedFanRpm);
    STEST_ARG(4, UINT32, gpu);

    ErrorMap::ScopedDevInst setDevInst(static_cast<INT32>(gpu));
    Mle::Print(Mle::Entry::fan_info)
        .fan_idx(fanIdx)
        .min_speed_check(isMinFanSpeedCheck)
        .measured_rpm(measuredFanRpm)
        .expected_rpm(expectedFanRpm);

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintClocksTestInfo,
    6,
    "PrintClocksTestInfo"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(6, 6, "");

    STEST_ARG(0, string, pstate);
    STEST_ARG(1, UINT08, passFailPerc);
    STEST_ARG(2, UINT32, targetGpcKhz);
    STEST_ARG(3, UINT32, effectiveGpcKhz);
    STEST_ARG(4, UINT32, gpuId);
    STEST_ARG(5, UINT32, clock);

    Mle::ClocksTestInfo::Clock clockEnum = Mle::ClocksTestInfo::unknown;
    switch (static_cast<Gpu::ClkDomain>(clock))
    {
        case Gpu::ClkDomain::ClkGpc: clockEnum = Mle::ClocksTestInfo::gpc; break;
        default:
            MASSERT(!"Unknown clock type");
            break;
    }

    ErrorMap::ScopedDevInst setDevInst(static_cast<INT32>(gpuId));
    Mle::Print(Mle::Entry::clocks_test_info)
        .clock(clockEnum)
        .pstate(pstate)
        .pass_fail_percent(passFailPerc)
        .target_khz(targetGpcKhz)
        .effective_khz(effectiveGpcKhz);
#endif
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintPowerBalSummary,
    6,
    "PrintPowerBalSummary"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(6, 6, "");

    STEST_ARG(0, UINT08, powerBalPolicyIdx);
    STEST_ARG(1, FLOAT32, shiftPctDelta);
    STEST_ARG(2, FLOAT32, minShiftPctDelta);
    STEST_ARG(3, FLOAT32, linearDeviation);
    STEST_ARG(4, FLOAT32, maxLinearDeviation);
    STEST_ARG(5, UINT32, gpuId);

    ErrorMap::ScopedDevInst setDevInst(static_cast<INT32>(gpuId));
    Mle::Print(Mle::Entry::power_balancing_info)
        .index(powerBalPolicyIdx)
        .shift_percent_delta(shiftPctDelta)
        .min_shift_percent_delta(minShiftPctDelta)
        .linear_deviation(linearDeviation)
        .max_linear_deviation(maxLinearDeviation);
#endif
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintAdcISenseDeltaInfo,
    10,
    "PrintAdcISenseDeltaInfo"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(10, 10, "");

    STEST_ARG(0, FLOAT32, upperDelta);
    STEST_ARG(1, FLOAT32, upperSlope);
    STEST_ARG(2, FLOAT32, upperIntercept);
    STEST_ARG(3, FLOAT32, lowerDelta);
    STEST_ARG(4, FLOAT32, lowerSlope);
    STEST_ARG(5, FLOAT32, lowerIntercept);
    STEST_ARG(6, UINT32,  numISenseFails);
    STEST_ARG(7, UINT32,  totalCount);
    STEST_ARG(8, string,  deltaUnitLower);
    STEST_ARG(9, string,  deltaUnitUpper);

    Mle::Print(Mle::Entry::adc_isense_delta)
        .worst_upper_delta(upperDelta)
        .worst_upper_slope(upperSlope)
        .worst_upper_intercept(upperIntercept)
        .worst_lower_delta(lowerDelta)
        .worst_lower_slope(lowerSlope)
        .worst_lower_intercept(lowerIntercept)
        .num_isense_fails(numISenseFails)
        .total_count(totalCount)
        .delta_unit_lower(deltaUnitLower)
        .delta_unit_upper(deltaUnitUpper);
#endif
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintPowerBalResult,
    6,
    "PrintPowerBalResult"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(6, 6, "");

    STEST_ARG(0, UINT32,  policyIndex);
    STEST_ARG(1, FLOAT32, totalPwrLowPwm);
    STEST_ARG(2, FLOAT32, totalPwrHighPwm);
    STEST_ARG(3, FLOAT32, priRailDelta);
    STEST_ARG(4, FLOAT32, secRailDelta);
    STEST_ARG(5, string,  pstate);

    Mle::Print(Mle::Entry::power_bal_result)
        .policy_index(policyIndex)
        .total_power_lowpwm_watt(totalPwrLowPwm)
        .total_power_highpwm_watt(totalPwrHighPwm)
        .primary_rail_delta_watt(priRailDelta)
        .secondary_rail_delta_watt(secRailDelta)
        .pstate(pstate);
#endif
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintFsSetInfo,
    1,
    "PrintFsSetInfo"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(1, 1, "Mle.PrintFsSetInfo(JSObject*)");

    STEST_ARG(0, JSObject*, pJsGroups);
    JavaScriptPtr pJs;
    JsArray jsGroups;
    ByteStream bs;
    auto entry = Mle::Entry::floorsweep_set_info(&bs);
    UINT32 state;
    string set_name;
    if (RC::OK != pJs->GetProperty(pJsGroups, "state", &state))
    {
        pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for state"
                   " in Mle.PrintFsSetInfo(JSObject*)\n");
        return JS_FALSE;
    }
    if (RC::OK != pJs->GetProperty(pJsGroups, "setName", &set_name))
    {
        pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for setName"
                   " in Mle.PrintFsSetInfo(JSObject*)\n");
        return JS_FALSE;
    }
    entry.state(static_cast<Mle::FloorsweepSetInfo::State>(state))
         .set_name(set_name);
    if (RC::OK != pJs->GetProperty(pJsGroups, "groupinfo", &jsGroups))
    {
        pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for groupinfo"
                   " in Mle.PrintFsSetInfo(JSObject*)\n");
        return JS_FALSE;
    }
    
    for (const auto& jsgroup : jsGroups)
    {
        JSObject* pJsgroupInfo;
        if (RC::OK != pJs->FromJsval(jsgroup, &pJsgroupInfo))
        {
            pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for object groupinfo" 
                       " in Mle.PrintFsSetInfo(JSObject*)\n");
            return JS_FALSE;
        }
        string groupName;
        UINT32 groupMask;
        bool isProtected;
        if (RC::OK != pJs->GetProperty(pJsgroupInfo, "groupName", &groupName)) 
        {
            pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for groupName"
                       " in Mle.PrintFsSetInfo(JSObject*)\n");
            return JS_FALSE;
        }
        if (RC::OK != pJs->GetProperty(pJsgroupInfo, "groupMask", &groupMask))
        {
            pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for groupMask "
                       " in Mle.PrintFsSetInfo(JSObject*)\n");
            return JS_FALSE;
        }
        if (RC::OK != pJs->GetProperty(pJsgroupInfo, "isProtected", &isProtected))
        {
            pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for isProtected "
                       " in Mle.PrintFsSetInfo(JSObject*)\n");
            return JS_FALSE;
        }
        auto groupElement = entry.groups();
        groupElement.name(groupName)
                    .mask(groupMask)
                    .isProtected(isProtected);

        JsArray jsElements;
        if (RC::OK != pJs->GetProperty(pJsgroupInfo, "elementInfo", &jsElements))
        {
            pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for elementInfo"
                       " in Mle.PrintFsSetInfo(JSObject*)\n");
            return JS_FALSE;
        }

        for (const auto& jsElement: jsElements)
        {
            string elementName;
            UINT32 elementMask;
            bool protectedMask;
            JSObject* pJselementInfo;
            if (RC::OK != pJs->FromJsval(jsElement, &pJselementInfo))
            {
                pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value inside elementInfo"
                           " in Mle.PrintFsSetInfo(JSObject*)\n");
                return JS_FALSE;
            }

            if (RC::OK != pJs->GetProperty(pJselementInfo, "elementName", &elementName))
            {
                pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for elementName"
                           " in Mle.PrintFsSetInfo(JSObject*)\n");
                return JS_FALSE;
            }
            if (RC::OK != pJs->GetProperty(pJselementInfo, "elementMask", &elementMask))
            {
                pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for elementMask "
                           " in Mle.PrintFsSetInfo(JSObject*)\n");
                return JS_FALSE;
            }
            if (RC::OK != pJs->GetProperty(pJselementInfo, "protectedElementMask", &protectedMask))
            {
                pJs->Throw(pContext, RC::BAD_PARAMETER, "Invalid value for protectedElementMask "
                           "in Mle.PrintFsSetInfo(JSObject*)\n");
                return JS_FALSE;                  
            }
            groupElement.elements()
                        .elName(elementName)
                        .elMask(elementMask)
                        .elProtectedMask(protectedMask);
        }
    }

    entry.Finish();
    Tee::PrintMle(&bs);
#endif
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Mle,
    PrintPcieLinkSpeedMismatch,
    11,
    "PrintPcieLinkSpeedMismatch"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(11, 11, "");

    STEST_ARG(0,  UINT32, hostPortDomain);
    STEST_ARG(1,  UINT32, hostPortBus);
    STEST_ARG(2,  UINT32, hostPortDevice);
    STEST_ARG(3,  UINT32, hostPortFunction);
    STEST_ARG(4,  UINT32, localPortDomain);
    STEST_ARG(5,  UINT32, localPortBus);
    STEST_ARG(6,  UINT32, localPortDevice);
    STEST_ARG(7,  UINT32, localPortFunction);
    STEST_ARG(8,  UINT32, actualLinkSpeedMbps);
    STEST_ARG(9, UINT32, expectedMinLinkSpeedMbps);
    STEST_ARG(10, UINT32, expectedMaxLinkSpeedMbps);

    ByteStream bs;
    auto entry = Mle::Entry::pcie_link_speed_mismatch(&bs);
    entry.link()
        .host()
            .domain(hostPortDomain)
            .bus(hostPortBus)
            .device(hostPortDevice)
            .function(hostPortFunction);
    entry.link()
        .local()
            .domain(localPortDomain)
            .bus(localPortBus)
            .device(localPortDevice)
            .function(localPortFunction);
    entry.actual_link_speed_mbps(actualLinkSpeedMbps)
        .expected_min_link_speed_mbps(expectedMinLinkSpeedMbps)
        .expected_max_link_speed_mbps(expectedMaxLinkSpeedMbps);
    entry.Finish();
    Tee::PrintMle(&bs);
#endif
    return JS_TRUE;
}


JS_SMETHOD_LWSTOM
(
    Mle,
    PrintPcieLinkWidthMismatch,
    12,
    "PrintPcieLinkWidthMismatch"
)
{
#ifdef INCLUDE_GPU
    STEST_HEADER(12, 12, "");

    STEST_ARG(0,  UINT32, hostPortDomain);
    STEST_ARG(1,  UINT32, hostPortBus);
    STEST_ARG(2,  UINT32, hostPortDevice);
    STEST_ARG(3,  UINT32, hostPortFunction);
    STEST_ARG(4,  UINT32, localPortDomain);
    STEST_ARG(5,  UINT32, localPortBus);
    STEST_ARG(6,  UINT32, localPortDevice);
    STEST_ARG(7,  UINT32, localPortFunction);
    STEST_ARG(8,  bool,   isHost);
    STEST_ARG(9,  UINT32, actualLinkWidth);
    STEST_ARG(10, UINT32, expectedMinLinkWidth);
    STEST_ARG(11, UINT32, expectedMaxLinkWidth);

    ByteStream bs;
    auto entry = Mle::Entry::pcie_link_width_mismatch(&bs);
    entry.link()
        .host()
            .domain(hostPortDomain)
            .bus(hostPortBus)
            .device(hostPortDevice)
            .function(hostPortFunction);
    entry.link()
        .local()
            .domain(localPortDomain)
            .bus(localPortBus)
            .device(localPortDevice)
            .function(localPortFunction);
    entry.is_host(isHost)
        .actual_link_width(actualLinkWidth)
        .expected_min_link_width(expectedMinLinkWidth)
        .expected_max_link_width(expectedMaxLinkWidth);
    entry.Finish();
    Tee::PrintMle(&bs);
#endif
    return JS_TRUE;
}
