/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Parser for chip reference manuals (e.g. lw_ref.h, mcp_ref.h)

#ifndef INCLUDED_REFPARSE_H
#define INCLUDED_REFPARSE_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#include <boost/serialization/vector.hpp>

enum RefManual_HwInitType { InitNone,     // not initialized
                            InitWarm,     // Initialized on hw reset or after PCI state transitions: D0->D3->D0
                            InitCold      // Initialized only on hw reset
                          };

struct RefManualFieldValue
{
    string Name;
    UINT32 Value;
    bool   IsTask;
    bool   IsConstant;
    RefManual_HwInitType initType;

    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & Name;
        ar & Value;
        ar & IsTask;
        ar & IsConstant;
        ar & initType;
    }
};

class RefManualRegisterField
{
public:
    RefManualRegisterField() = default;
    RefManualRegisterField
    (
        const char* name,
        const char* access,
        bool isAlias = false,
        int lowBit = 0,
        int highBit = 0,
        bool readable = true,
        bool writeable = true
    ) :
        m_Name(name),
        m_Access(access),
        m_IsAlias(isAlias),
        m_LowBit(lowBit),
        m_HighBit(highBit),
        m_IsReadable(readable),
        m_IsWriteable(writeable)
    {
    }

    const string& GetName() const { return m_Name; }
    const string& GetAccess() const { return m_Access; }
    int GetLowBit() const { return m_LowBit; }
    int GetHighBit() const { return m_HighBit; }
    bool IsReadable() const { return m_IsReadable; }
    bool IsWriteable() const { return m_IsWriteable; }
    bool IsUnWriteable() const { return m_IsUnWriteable; }
    bool IsConstant() const { return m_IsConstant; }
    bool IsTask() const { return m_IsTask; }
    bool IsClearOnOne() const { return m_IsClearOnOne; }
    bool IsUnknown() const { return m_IsUnknown; }
    bool IsAlias() const { return m_IsAlias; }

    UINT32 GetMask() const;
    UINT32 GetConstantValue() const;

    int GetNumValues() const;
    const RefManualFieldValue *GetValue(int Index) const;
    const RefManualFieldValue *GetInitValue() const;
    const RefManualFieldValue *FindValue(const char *Name) const;
    void InitAttrFromAccess();

private:
    string m_Name;
    string m_Access;
    bool m_IsAlias = false;
    int m_LowBit = 0;
    int m_HighBit = 0;
    bool m_IsReadable = false;
    bool m_IsWriteable = false;
    bool m_IsUnWriteable = false;
    bool m_IsConstant = false;
    bool m_IsTask = false;
    bool m_IsClearOnOne = false;
    bool m_IsUnknown = false;

    vector<RefManualFieldValue> m_Values;

    friend class RefManual;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_Name;
        ar & m_Access;
        ar & m_IsAlias;
        ar & m_LowBit;
        ar & m_HighBit;
        ar & m_IsReadable;
        ar & m_IsWriteable;
        ar & m_IsUnWriteable;
        ar & m_IsConstant;
        ar & m_IsTask;
        ar & m_IsClearOnOne;
        ar & m_IsUnknown;
        ar & m_Values;
    }
};

class RefManualRegister
{
public:
    const string& GetName() const { return m_Name; }
    const string& GetAccess() const { return m_Access; }
    UINT32 GetOffset() const { return m_Offset; }
    UINT32 GetOffset(int i) const;
    UINT32 GetOffset(int i, int j) const;
    int GetDimensionality() const;
    int GetArraySizeI() const { return m_ArraySizeI; }
    int GetArraySizeJ() const { return m_ArraySizeJ; }
    UINT32 GetStrideI() const { return m_StrideI; }
    UINT32 GetStrideJ() const { return m_StrideJ; }

    //! Some "registers" in the manual are actually methods in a class.
    bool IsMethod() const { return (m_ClassID != 0); }
    UINT32 GetClassID() const { return m_ClassID; }

    bool IsReadable() const { return m_IsReadable; }
    bool IsWriteable() const { return m_IsWriteable; }
    bool IsUnWriteable() const { return m_IsUnWriteable; }

    UINT32 GetFieldMask() const;
    UINT32 GetReadableFieldMask() const;
    UINT32 GetWriteableFieldMask() const;
    UINT32 GetUnWriteableFieldMask() const;
    UINT32 GetConstantFieldMask() const;
    UINT32 GetConstantFieldValue() const;
    UINT32 GetTaskFieldMask() const;
    UINT32 GetClearOnOneFieldMask() const;

    bool GetInitValue(UINT32 *Value) const;

    int GetNumFields() const;
    const RefManualRegisterField *GetField(int Index) const;
    const RefManualRegisterField *FindField(const char *Name) const;

private:
    string m_Name;
    string m_Access;
    UINT32 m_Offset;
    int m_ArraySizeI;
    int m_ArraySizeJ;
    UINT32 m_StrideI;
    UINT32 m_StrideJ;
    UINT32 m_ClassID;
    bool m_IsReadable;
    bool m_IsWriteable;
    bool m_IsUnWriteable;

    vector<RefManualRegisterField> m_Fields;

    friend class RefManual;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_Name;
        ar & m_Access;
        ar & m_Offset;
        ar & m_ArraySizeI;
        ar & m_ArraySizeJ;
        ar & m_StrideI;
        ar & m_StrideJ;
        ar & m_ClassID;
        ar & m_IsReadable;
        ar & m_IsWriteable;
        ar & m_IsUnWriteable;
        ar & m_Fields;
    }
};

class RefManualRegisterGroup
{
public:
    RefManualRegisterGroup(string name, UINT32 rangeLo, UINT32 rangeHi)  
        :m_Name(move(name))
        ,m_RangeLo(rangeLo)
        ,m_RangeHi(rangeHi) { }

    const string& GetName() const { return m_Name; }
    UINT32 GetRangeLo() const { return m_RangeLo; }
    UINT32 GetRangeHi() const { return m_RangeHi; }

    void AddRegister(RefManualRegister *) ;
    int GetNumRegisters() const;
    const RefManualRegister *GetRegister(int Index) const;
    const RefManualRegister *FindRegister(const char *Name) const;

private:
    string m_Name;
    UINT32 m_RangeLo;
    UINT32 m_RangeHi;
    vector<RefManualRegister *> m_Registers;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_Registers;
    }
};
// Overriding functions described here to support deletion of default constructor in RefManualRegisterGroup: 
// https://www.boost.org/doc/libs/1_48_0/libs/serialization/doc/serialization.html#constructors
namespace boost
{
    namespace serialization
    {
        template<class Archive>
        inline void save_construct_data(
            Archive& ar, const RefManualRegisterGroup* t, const unsigned int file_version
        )
        {
            // save data required to construct instance
            ar << t->GetName();
            ar << t->GetRangeLo();
            ar << t->GetRangeHi();
        }

        template<class Archive>
        inline void load_construct_data(
            Archive& ar, RefManualRegisterGroup* t, const unsigned int file_version
        )
        {
            // retrieve data from archive required to construct new instance
            string name;
            UINT32 rangeLo;
            UINT32 rangeHi;
            ar >> name;
            ar >> rangeLo;
            ar >> rangeHi;
            // ilwoke inplace constructor to initialize instance of RefManualRegisterGroup
            ::new(t)RefManualRegisterGroup(name, rangeLo, rangeHi);
        }
    }
}

class RefManual
{
public:
    RC Parse
    (
        const char *FileName,
        bool UseSerial = false,   // Set to true to use cached manuals when available
        const char *UnitName = NULL,
        const char *Start = NULL,
        const char *End = NULL
    );

    RC Serialize(const char *FileName) const;
    RC Deserialize(const char *FileName);

    bool ParseTried() { return m_bParseTried; }

    int GetNumRegisters() const;
    const RefManualRegister *GetRegister(int Index) const;
    const RefManualRegister *FindRegister(const char *Name) const;
    const RefManualRegister *FindRegister(UINT32 RegisterOffset) const;

    struct ParsedRegExpr
    {
        const RefManualRegister *pRefReg;
        UINT32                   IdxI;
        UINT32                   IdxJ;
    };
    RC ParseRegExpr(const char *Name, ParsedRegExpr *pParsed) const;

    int GetNumGroups() const;
    const RefManualRegisterGroup *GetGroup(int Index) const;
    const RefManualRegisterGroup *FindGroup(const char *Name) const;

private:
    void ParseAliasFieldValue
    (
        const char* Name,
        const char* Definition,
        const char* Access
    );
    void ParseAliasField
    (
        const char* Name,
        const char* Definition,
        const char* Access
    );

    vector<RefManualRegister> m_Registers;
    vector<RefManualRegisterGroup> m_RegisterGroups;
    bool m_bParseTried = false;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_Registers;
        ar & m_RegisterGroups;
        ar & m_bParseTried;
    }
};

#endif // !INCLUDED_REFPARSE_H
