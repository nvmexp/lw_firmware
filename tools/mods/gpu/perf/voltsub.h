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

#pragma once
#ifndef INCLUDED_VOLT_H
#define INCLUDED_VOLT_H

#include <map>
#include <set>
#include <vector>

#include "core/include/gpu.h"
#include "core/include/types.h"
#include "core/include/setget.h"

class GpuSubdevice;
struct JSObject;
class RC;
class SplitRailConstraintManager;

//! Class which provides APIs to get info, status or control to many
//! voltage rails, devices, and policies
//! Relies heavily on RMCTRL calls found within ctrl2080volt.h
class Volt3x
{
public:
    //! Used to set voltages on a particular domain
    struct VoltageSetting
    {
        Gpu::SplitVoltageDomain voltDomain;
        UINT32 voltuV;

        VoltageSetting() :
            voltDomain(Gpu::ILWALID_SplitVoltageDomain), voltuV(0) {}

        VoltageSetting(Gpu::SplitVoltageDomain dom, UINT32 val) :
            voltDomain(dom), voltuV(val) {}
    };

    Volt3x(GpuSubdevice *pSubdevice);
    ~Volt3x();

    //! Call all of the VOLT GET_INFO APIs. Save rail/device/policy masks and print all info
    RC Initialize();
    RC Cleanup();

    //! Does this GPU support the voltage domain?
    bool HasDomain(Gpu::SplitVoltageDomain dom) const;

    //! Print all voltage limits being applied by MODS
    RC PrintVoltLimits() const;
    vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS> GetVoltLimits()
    {
        return m_VoltLimits;
    }

    //! Get the current voltages for all rails via GetVoltRailStatus()
    RC GetLwrrentVoltagesuV
    (
        map<Gpu::SplitVoltageDomain, UINT32>& voltages
    );

    //! Get the current voltages for all rails via GetVoltRailStatus()
    RC GetSensedVoltagesuV
    (
        map<Gpu::SplitVoltageDomain, UINT32>& voltages
    );

    //! Return status info for the given voltage rails
    RC GetVoltRailStatusViaMask
    (
        UINT32 voltRailMask,
        LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS *pVoltRailParams
    );

    //! Return the status info for *all* known voltage rails
    RC GetVoltRailStatus
    (
        LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS *pVoltRailParams
    );

    //! Given a rail id, return the status for that rail
    RC GetVoltRailStatusViaId
    (
        Gpu::SplitVoltageDomain voltDom,
        LW2080_CTRL_VOLT_VOLT_RAIL_STATUS *pRailStatus
    );

    RC GetVoltRailRegulatorStepSizeuV
    (
        Gpu::SplitVoltageDomain dom,
        LwU32* pStepSizeuV
    ) const;

    enum VoltageLimit
    {
        RELIABILITY_LIMIT     = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_REL_LIM_IDX,
        ALT_RELIABILITY_LIMIT = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_ALT_REL_LIM_IDX,
        OVERVOLTAGE_LIMIT     = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_OV_LIM_IDX,
        VMIN_LIMIT            = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_VMIN_LIM_IDX,
        VCRIT_LOW_LIMIT       = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_VCRIT_LOW_IDX,
        VCRIT_HIGH_LIMIT      = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_VCRIT_HIGH_IDX,
        NUM_VOLTAGE_LIMITS    = LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES
    };

    enum RamAssistType
    {
        RAM_ASSIST_TYPE_DISABLED = LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DISABLED,
        RAM_ASSIST_TYPE_STATIC = LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_STATIC,
        RAM_ASSIST_TYPE_DYNAMIC_WITHOUT_LUT = 
            LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DYNAMIC_WITHOUT_LUT,
        RAM_ASSIST_TYPE_DYNAMIC_WITH_LUT = 
            LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DYNAMIC_WITH_LUT
    };

    //! Apply an offset to a limit on a rail such as the over-voltage limit.
    //! This does not apply an offset to the VF lwrve itself, but it can
    //! "unlock" the bottom and top portions of a VF lwrve by changing
    //! Vmin/Vmax.
    //!
    //! Example: if Vmin is decreased, lower VF points will be become available
    RC SetRailLimitOffsetuV
    (
        Gpu::SplitVoltageDomain voltDom,
        VoltageLimit voltLimit,
        INT32 offsetuV
    );

    //! Get the offsets to all voltage rail limits
    RC GetRailLimitOffsetsuV
    (
        map<Gpu::SplitVoltageDomain, vector<INT32> >* limitsuV
    ) const;

    //! Gets offset voltLimit to specified domain using cached values from
    //! previous SetRailOffsetuV calls
    RC GetCachedRailLimitOffsetsuV
    (
        Gpu::SplitVoltageDomain voltDom,
        VoltageLimit voltLimit,
        INT32 *pOffsetuV
    ) const;

    //! Clear all voltage limit offsets applied by MODS
    RC ClearRailLimitOffsets();

    //! \brief Get all of the voltage policies' statuses
    //!
    //! \param statuses - a map of policy indices to policy statuses
    RC GetVoltPoliciesStatus
    (
        map<UINT32, LW2080_CTRL_VOLT_VOLT_POLICY_STATUS>& statuses
    ) const;

    //! Get the voltage on a particular voltage rail
    RC GetVoltageMv(Gpu::SplitVoltageDomain voltDom, UINT32 *pVoltMv);

    //! Get the sensed voltage on a particular voltage rail
    RC GetCoreSensedVoltageUv(Gpu::SplitVoltageDomain voltDom, UINT32 *pVoltUv);

    //! \brief Set the voltage on one or more rails
    //!
    //! Multiple calls to SetVoltagesuV() on different rails can cause
    //! split-rail constraint violations. This function was created
    //! to allow the RM voltage arbiter to set multiple voltage domains
    //! at once and avoid violating any split-rail constraints.
    RC SetVoltagesuV(const vector<VoltageSetting>& voltages);

    //! \brief Set the voltage on one or more rails via
    //!        LW2080_CTRL_CMD_PERF_CHANGE_SEQ_SET_CONTROL
    RC InjectVoltagesuV(const vector<VoltageSetting>& voltages);

    //! Release limit over specified voltage rail
    //! This causes the voltage to go back to being arbitrated by RM
    //! Effectively undoes SetVoltageMv
    RC ClearSetVoltage(Gpu::SplitVoltageDomain voltDom);

    //! \brief Clears all voltages set by SetVoltage(s)Mv
    //!
    //! Called during shutdown to avoid split-rail constraint violations
    RC ClearAllSetVoltages();

    RC GetLwrrOverVoltLimitsuVToJs(JSObject *pJsObj);
    RC GetLwrrVoltRelLimitsuVToJs(JSObject *pJsObj);
    RC GetLwrrAltVoltRelLimitsuVToJs(JSObject *pJsObj);
    RC GetLwrrVminLimitsuVToJs(JSObject *pJsObj);
    RC GetRamAssistInfos(map<Gpu::SplitVoltageDomain, Volt3x::RamAssistType> *pRamAssistTypes);
    RC GetRamAssistPerVoltRailInfo(JSObject *pJsObj);
    static void PrintSingleVoltRailInfo
    (
        LW2080_CTRL_VOLT_VOLT_RAIL_INFO &railInfo
    );
    static void PrintSingleVoltDeviceInfo
    (
        LW2080_CTRL_VOLT_VOLT_DEVICE_INFO &devInfo
    );
    static void PrintSingleVoltPolicyInfo
    (
        LW2080_CTRL_VOLT_VOLT_POLICY_INFO &policyInfo,
        LW2080_CTRL_VOLT_VOLT_POLICY_STATUS &policyStatus
    );

    RC SetInterVoltSettleTimeUs(UINT16 timeUs);
    RC SetSplitRailConstraintOffsetuV(bool bMax, INT32 OffsetInuV);
    RC GetSplitRailConstraintuV(INT32* min, INT32* max);

    GET_PROP(SplitRailConstraintAuto, bool);
    RC SetSplitRailConstraintAuto(bool enable);
    SplitRailConstraintManager* GetSplitRailConstraintManager()
    {
        return m_pSplitRailConstraintManager;
    }

    bool IsInitialized() { return m_IsInitialized; }
    GET_PROP(AvailableDomains, set<Gpu::SplitVoltageDomain>);
    GET_PROP(VoltRailMask, UINT32);
    GET_PROP(VoltDeviceMask, UINT32);
    GET_PROP(VoltPolicyMask, UINT32);
    SETGET_PROP(PrintVoltSafetyWarnings, bool);

    UINT32 GpuSplitVoltageDomainToRailIdx(Gpu::SplitVoltageDomain dom) const;
    Gpu::SplitVoltageDomain RailIdxToGpuSplitVoltageDomain(UINT32 railIdx) const;

    RC GetRailLimitVfeEquIdx
    (
        Gpu::SplitVoltageDomain dom,
        VoltageLimit voltLim,
        UINT32* pVfeEquIdx
    ) const;

    //! Find Rail Index for a given Voltage Domain based on Rail Index
    Gpu::SplitVoltageDomain RmVoltDomToGpuSplitVoltageDomain(UINT08 voltDomain);

    bool IsPascalSplitRailSupported() const;

    bool IsRamAssistSupported();

    RC SetRailLimitsOffsetuV
    (
        Gpu::SplitVoltageDomain voltDom,
        const map<VoltageLimit, INT32> &voltOffsetuV
    );

private:
    GpuSubdevice *m_pSubdevice;
    bool m_IsInitialized;
    UINT32 m_VoltRailMask;
    UINT32 m_VoltDeviceMask;
    UINT32 m_VoltPolicyMask;
    bool m_SplitRailConstraintAuto;
    SplitRailConstraintManager* m_pSplitRailConstraintManager;

    // The voltage perf limits that we send to RM
    vector<LW2080_CTRL_PERF_LIMIT_SET_STATUS> m_VoltLimits;

    // The original voltage perf limits
    map<Gpu::SplitVoltageDomain, map<VoltageLimit, UINT32> > m_OrigVoltLimitsuV;

    // Used during shutdown to see if we need to cleanup anything
    bool m_HasActiveVoltageLimits;
    bool m_HasActiveVoltageLimitOffsets;

    set<Gpu::SplitVoltageDomain> m_AvailableDomains;

    struct VoltRailInfo
    {
        Gpu::SplitVoltageDomain voltDom;
        UINT32 railIdx;
    };
    using VoltRailInfos = vector<VoltRailInfo>;
    VoltRailInfos m_VoltRailInfos;

    LW2080_CTRL_VOLT_VOLT_RAILS_INFO_PARAMS m_CachedRailsInfo = {};
    LW2080_CTRL_VOLT_VOLT_DEVICES_INFO_PARAMS m_CachedDevicesInfo = {};

    // Only caches rail offsets applied through SetRailLimitOffsetuV()
    LW2080_CTRL_VOLT_VOLT_RAILS_CONTROL_PARAMS m_CachedRailsCtrl = {};

    bool m_PrintVoltSafetyWarnings = true;

    RC VoltageDomainToLimitId(Gpu::SplitVoltageDomain voltDom, UINT32 *pLimitId);
    RC SetVoltageLimits();
    RC GetVoltLimitByVoltDomain
    (
        Gpu::SplitVoltageDomain voltDom,
        LW2080_CTRL_PERF_LIMIT_INPUT** limit
    );
    RC ConstructVoltLimits();

    RC LazyInstantiateSRConstMgr();
    void LazyDeleteSRConstMgr();

    RC GetVoltageViaStatus
    (
        JSObject *pJsObj,
        const vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS>& statuses
    );
};

/*
 * This class manages the split rail constraint which is
 * min <= Vlogic - Vsram <= max
 *
 * When -split_rail_constraint_auto_offset is used, this
 * class will actively update the constraint in response to
 * any voltage offsets or limits that are applied.
 *
 * For now, this class only increases the max and decreases the min.
 * This is to avoid ilwalidating the current VF lwrves, but a better
 * algorithm to fit the constraints more closely is possible,
 * but more complicated because of potential quantization issues
 * as well as ordering issues (i.e. whether to update the constraint
 * first or the VF lwrves)
 */
class SplitRailConstraintManager
{
public:
    SplitRailConstraintManager(GpuSubdevice* pGpuSub);
    RC Initialize();
    RC Cleanup();

    //! Gets the current split rail constraint min/max
    RC GetConstraintuV(INT32* pMinuV, INT32* pMaxuV);

    //! Set the constraint min and/or max
    RC SetConstraintOffsetsuV
    (
        bool bMin,
        bool bMax,
        INT32 MinOffsetuV,
        INT32 MaxOffsetuV
    );

    //! Make the split rail constraint take effect after voltage
    //! offsets or limits have been applied
    RC UpdateConstraint();

    //! If we set voltages via the MODS_RULES, update the constraint
    RC ApplyVoltLimituV(Gpu::SplitVoltageDomain, INT32 voltageuV);
    void ResetVoltLimit(Gpu::SplitVoltageDomain dom);

    //! Controls how offsets to either rail affect the constraint
    RC ApplyRailOffsetuV(Gpu::SplitVoltageDomain, INT32 offsetuV);

    //! Controls how offsets to individual domain's VF lwrve affect
    //! the constraint
    RC ApplyClkDomailwoltOffsetuV(Gpu::SplitVoltageDomain, INT32 offsetuV);

    //! VF points that get offset affect the constraint too
    void ApplyVfPointOffsetuV(INT32 offsetuV);



private:
    SplitRailConstraintManager() {}

    RC UpdateConstraintFromVoltLimits();

    // A ConstraintPair represents what effect a certain type of
    // voltage offset (or limit) will have on the split rail constraint.
    // When one of the Apply*() methods is called, we privately update
    // the corresponding constraint pair
    struct ConstraintPair
    {
        INT32 minuV, maxuV;
        ConstraintPair() : minuV(0), maxuV(0) {}
        ConstraintPair(INT32 minuV, INT32 maxuV) : minuV(minuV), maxuV(maxuV) {}

        // Operators '+' and '+=' are overloaded to behave like 2D vector
        // addition, but with a twist: min can only get smaller and max
        // can only get larger.
        ConstraintPair operator+(const ConstraintPair& other);
        ConstraintPair operator+=(const ConstraintPair& other);
    };

    RC ActuallyUpdateConstraint
    (
        ConstraintPair& lwrrOffsetsCP,
        ConstraintPair& newOffsetsCP
    );

    // The voltLimits constraint pair is mutually exclusive with
    // all othe constraint pairs because when we set the voltage
    // via the MODS_RULES, no voltage offsets can take effect.
    ConstraintPair voltLimitsCP;

    // The three below constraint pairs are additive. For example,
    // we could have an offset on the SRAM rail of +50mV and another
    // offset on the gpc2clk domain of +30mV, so the constraint max
    // would have to increase by 80mV.
    ConstraintPair railOffsetCP;
    ConstraintPair railClkDomOffsetCP;
    ConstraintPair vfpOffsetCP;

    // Used to keep track of how much the split rail constraint
    // min/max have been offset. Should only be modified by
    // UpdateSplitRailConstraint() and can be used to figure out
    // the current split-rail constraint min/max by adding to origCP
    ConstraintPair lwrrOffsetsCP;

    // The constraints provided by VBIOS. Note that this pair
    // does NOT represent an offset to the constraints, but is
    // the original split rail constraint min/max
    ConstraintPair origConstraints;

    GpuSubdevice* m_pGpuSub;

    map<Gpu::SplitVoltageDomain, UINT32> lwrrVoltagesuV;
    map<Gpu::SplitVoltageDomain, UINT32> newVoltagesuV;

    // Are we locking the voltages on both rails via the MODS_RULES?
    bool UsingTwoVoltageLimits();

    //! Figure out how much a voltage offset will change the
    //! split rail constraint min or max
    static RC GetOffsetEffectuV
    (
        Gpu::SplitVoltageDomain dom,
        INT32 offsetuV,
        INT32* pMinuV,
        INT32* pMaxuV
    );
};

#endif // INCLUDED_VOLT_H
