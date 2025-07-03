/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/fuse/fuseencoder.h"

#include "gpu/fuse/encoding/columnfuseencoder.h"
#include "gpu/fuse/encoding/subencoder.h"
#include "gpu/fuse/encoding/iffencoder.h"
#include "gpu/fuse/encoding/rirencoder.h"
#include "gpu/include/testdevice.h"
// Fuse encoder where FUSE, FPF, and RIR are all on separate macros
// Used until Ampere (Ampere combines the different macros into one)
class SplitMacroEncoder : public FuseEncoder
{
public:
    SplitMacroEncoder
    (
        TestDevice *pDev,
        const map<FuseMacroType, shared_ptr<FuseHwAccessor>>& hwAccessors,
        unique_ptr<FuselessFuseEncoder>&& pFFEncoder,
        unique_ptr<IffEncoder>&& pIffEncoder,
        unique_ptr<ColumnFuseEncoder>&& pFpfColEncoder,
        unique_ptr<RirEncoder>&& pRirEncoder
    );
    ~SplitMacroEncoder() = default;

    SplitMacroEncoder() = delete;
    SplitMacroEncoder(const SplitMacroEncoder&) = delete;

    // Remove fuses that are not available on the given chip
    RC RemoveUnsupportedFuses
    (
        FuseValues *pFuseRequest
    ) override;

    // Encode fuse values into binary array
    RC EncodeFuseValues
    (
        const FuseValues& fuseRequest,
        FuseBinaryMap* pFuseBinary
    ) override;

    // Encode fuse values into a set of register offset:value
    // pairs for the FuseHwAccessors to write
    RC EncodeSwFuseValues
    (
        const FuseValues& fuseRequest,
        SwFuseMap* pSwFuseWrites
    ) override;

    // Encode values for a fuse rework
    RC EncodeReworkFuseValues
    (
        const FuseValues &srcFuses,
        const FuseValues &destFuses,
        ReworkRequest *pRework
    ) override;

    // Encode iff rows for FuseHwAccessors to write
    RC EncodeSwIffValues
    (
        const FuseValues& fuseRequest,
        vector<UINT32>* pIffRows
    ) override;

    // Callwlate the CRC value for Iff fusing
    RC CallwlateIffCrc(const vector<UINT32>& iffRows, UINT32 * pCrcVal) override;

    // Get the decoded value for a single fuse
    RC GetFuseValue(const string& fuseName, FuseFilter filter, UINT32* pVal) override;

    // Get the decoded values for multiple fuses
    RC GetFuseValues(FuseValues* pFilteredVals, FuseFilter filter) override;

    // Checks whether or not the given fuse can be changed to the give value
    bool CanBurnFuse(const string& fuseName, FuseFilter filter, UINT32 newValue) override;

    // Sets whether or not we can use the undo bit of an undo fuse
    // to flip the value of the fuse from 1 to 0
    void SetUseUndoFuse(bool useUndo) override;

    bool GetUseUndoFuse() override;

    // Force ordering of FF records
    void ForceOrderedFFRecords(bool ordered) override;

    // Request use of RIR on fuse if needed
    RC SetUseRirOnFuse(const string& fuseName) override;

    // Ignore current HW fusing on the chip
    void IgnoreLwrrFuseVals(bool ignore) override;

    // Ignore mismatches between HW fuses and OPT register values
    void SetIgnoreRawOptMismatch(bool ignore) override;

    void PostSltIgnoreFuse(const string& fuseName) override;
    void TempIgnoreFuse(const string& fuseName) override;
    void ClearTempIgnoreList() override;

    RC AddReworkFuseInfo
    (
        const string &name,
        UINT32 newVal,
        vector<ReworkFFRecord> &fuseRecords
    ) const override;
    bool IsFsFuse(const string &fsFuseName) const override;
    void AppendFsFuseList(const string& fsFuseName) override;
    void SetNumSpareFFRows(const UINT32 numSpareFFRows) override;
    
    RC DecodeIffRecord
    (
        const shared_ptr<IffRecord>& pRecord, 
        string &output,
        bool* pIsValid
    ) override;

    RC GetIffRecords
    (
        const list<FuseUtil::IFFInfo>& iffPatches, 
        vector<shared_ptr<IffRecord>>* pRecords
    ) override;

    RC ValidateIffRecords
    (
        const vector<shared_ptr<IffRecord>>& chipRecords,
        const vector<shared_ptr<IffRecord>>& skuRecords,
        bool* pDoesMatch
    ) override;

protected:
    // Decode the fuse binaries into the cached fuse values
    RC DecodeRawFuses(FuseValues* pDecodedVals) override;
    // Read the OPT registers into the cached fuse values
    RC ReadOptFuses(FuseValues* pDecodedVals) override;

private:
    bool m_HasIff = false;
    bool m_HasFpf = false;
    bool m_HasRir = false;

    bool m_IsInitialized = false;

    bool m_UseUndo = false;

    bool m_IgnoreLwrrFuseVals = false;

    bool m_IgnoreRawOptMismatch = false;

    Device::LwDeviceId m_DevId = Device::LwDeviceId::ILWALID_DEVICE;
    TestDevice* m_pDev = nullptr; // Added for Ampere+ constructors
    map<FuseMacroType, shared_ptr<FuseHwAccessor>> m_HwAccessors;
    map<FuseMacroType, vector<SubFuseEncoder*>> m_SubEncoders;

    map<string, FuseUtil::FuseDef> m_FuseDefs;

    // Sub-encoders
    ColumnFuseEncoder m_ColumnFuseEncoder;
    unique_ptr<FuselessFuseEncoder> m_pFuselessFuseEncoder;
    unique_ptr<IffEncoder> m_pIffEncoder;

    unique_ptr<ColumnFuseEncoder> m_pFpfColEncoder;

    unique_ptr<RirEncoder> m_pRirEncoder;

    set<string> m_PostSltIgnoredFuses;
    set<string> m_TempIgnoredFuses;

    // Initialize the sub-encoders from the Fuse.JSON
    RC Initialize();

    bool IsFuseSupported(const FuseUtil::FuseDef& fuseDef, FuseFilter filter) override;
};
