/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <bitset>
#include "gpu/fuse/encoding/iffencoder.h"
#include "gpu/fuse/fusetypes.h"

// IFF fuse block in Hopper has moved to SYSCTRL.
// Architecture has changed so separate IFF Encoder
// to take care of these changes.
class HopperIffEncoder : public IffEncoder
{
public:
    RC CallwlateCrc(const vector<UINT32>& iffRows, UINT32* pCrcVal) override;

    RC DecodeIffRecord
    (
        const shared_ptr<IffRecord>& pRecord,
        string &output,
        bool* pIsValid
    ) override;

    //! \brief Retrieves the IFF records that a SKU.IffPatches has specified
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
    RC DecodeFuseValsImpl
    (
        const vector<UINT32>& rawFuses,
        FuseValues* pFuseData
    ) override;

    RC CheckIfRecordValid(const shared_ptr<IffRecord> pRecord) override;

    UINT32 GetIlwalidatedRow() const override { return s_IlwalidatedRow; }
    bool IsIlwalidatedRow(const UINT32 row) const override
        { return (row & s_IlwalidatedRow) == s_IlwalidatedRow; }

private:
    // Hopper+, ilwalidated row => ilwalidated chainId
    // From HopperFFRecord format, chainId = [31:27]|[4:0]
    static constexpr UINT32 s_IlwalidatedRow = 0xF800001F;

    bitset<16> CrcStep
    (
        const bitset<16>& crcIn,
        const bitset<32>& dataBin
    );
};
