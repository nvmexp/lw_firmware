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

#include <vector>

class IffRecord
{
public:
    // IffType values come from dev_bus.h
    // bits  OP        | Description
    // ----------------+--------------------------------------------------------------
    // 00000 NULL      | Unprogrammed entry; will be skipped
    // 11100 MODIFY_DW | Read-modify-write using the next two records as AND/OR masks
    // 11101 WRITE_DW  | Write dword using the next record as the write data
    // 11110 SET_FIELD | Write up to 6 bits of data to one of a small set of registers
    // 11111 INVALID   | Ilwalidated record; will be skipped
    // other abort     | Unrecognized entry; flags IFR to release holds and IFF aborts
    // ----------------+--------------------------------------------------------------
    enum class IffType : UINT32
    {
        NULL_RECORD  = 0x0,
        POLL_DW      = 0x1B,
        MODIFY_DW    = 0x1C,
        WRITE_DW     = 0x1D,
        SET_FIELD    = 0x1E,
        ILWALIDATED  = 0x1F,
        UNKNOWN
    };

    IffRecord() {}
    explicit IffRecord(UINT32 firstRecord);
    virtual ~IffRecord() {}

    virtual bool operator==(const IffRecord& other) const
        { return m_IffType == other.m_IffType && m_Data == other.m_Data; }

    virtual bool operator!=(const IffRecord& other) const
        { return !(*this == other); }

    IffType GetIffType() const { return m_IffType; }

    virtual vector<UINT32> GetDataList() const { return m_Data; }
    virtual UINT32 GetData(const UINT32 recordIdx) const { return m_Data[recordIdx]; }
    virtual void AddData(const UINT32 fuseData) { m_Data.push_back(fuseData); }

    virtual UINT32 GetOpCode(const UINT32 lwrRecord) const { return lwrRecord & s_OpCodeMask; }
    virtual UINT32 GetIffNumRows() const;
    virtual UINT32 GetIffNumRows(const UINT32 opCode) const;

    virtual bool IsIlwalidatedRecord() const { return m_IffType == IffType::ILWALIDATED; }
    virtual bool IsNullRecord() const { return m_IffType == IffType::NULL_RECORD; }
    virtual bool IsUnknownRecord() const { return m_IffType == IffType::UNKNOWN; }

private:
    static constexpr UINT08 s_OpCodeMask = 0x1f;
    static constexpr UINT08 s_TypeMask = 0x20;

    IffType m_IffType = IffType::UNKNOWN;
    vector<UINT32> m_Data;

};
