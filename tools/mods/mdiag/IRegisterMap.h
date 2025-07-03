/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2015, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <vector>
#include <memory>

#ifndef INCLUDED_IREGISTERMAP_H
#define INCLUDED_IREGISTERMAP_H

class IRegisterValue
{
public:
// IRegisterValue Interface
enum HwInitType { NONE= 0 , WARM= 1 , COLD= 2  };
    virtual ~IRegisterValue() { }
    virtual const char* GetName() = 0;
    virtual const UINT32 GetValue() = 0;
    virtual const UINT32 IsHWInit() = 0;
    virtual const UINT32 IsHWInitCold() = 0;
    virtual const UINT32 IsHWInitWarm() = 0;
    virtual const bool IsTask() = 0;
    virtual const bool IsConstant() = 0;
};

class IRegisterField
{
public:
// IRegisterField Interface
    virtual ~IRegisterField() { }
    virtual const char* GetName() = 0;
    virtual const char* GetAccess() = 0;
    virtual const UINT32 GetStartBit() = 0;
    virtual const UINT32 GetEndBit() = 0;
    virtual const UINT32 GetMask() = 0;
    virtual const UINT32 GetReadMask() = 0;
    virtual const UINT32 GetWriteMask() = 0;
    virtual const UINT32 GetUnwriteableMask() = 0;
    virtual const UINT32 GetConstMask() = 0;

    virtual bool isReadable() = 0;
    virtual bool isConstant() = 0;
    virtual bool isWriteable() = 0;
    virtual bool isReadWrite() = 0;
    virtual bool isUnknown() = 0;
    virtual bool isTask() = 0;

    virtual unique_ptr<IRegisterValue> FindValue(const char *value_name) = 0;
    virtual unique_ptr<IRegisterValue> GetValueHead() = 0;
    virtual unique_ptr<IRegisterValue> GetValueNext() = 0;
    virtual UINT32 GetHWInitValue(UINT32 *oval) = 0;
    virtual UINT32 GetHWInitColdValue(UINT32 *oval) = 0;
    virtual UINT32 GetHWInitWarmValue(UINT32 *oval) = 0;
    virtual const UINT32 GetMultiple() = 0;
};

class IRegisterClass
{
public:
// IRegisterClass Interface
    virtual ~IRegisterClass() { }
    virtual const char* GetName() = 0;
    virtual const char* GetAccess() = 0;
    virtual const UINT32 GetAddress() = 0;
    virtual const UINT32 GetAddress(UINT32 i) = 0;
    virtual const UINT32 GetAddress(UINT32 i, UINT32 j) = 0;
    virtual const UINT32 GetClassId() = 0;
    virtual const UINT32 GetPrefix() = 0;
    virtual const UINT32 GetReadMask() = 0;
    virtual const UINT32 GetWriteMask() = 0;
    virtual const UINT32 GetTaskMask() = 0;
    virtual const UINT32 GetUnwriteableMask() = 0;
    virtual const UINT32 GetConstMask() = 0;
    virtual const UINT32 GetConstValue() = 0;
    virtual const UINT32 GetW1ClrMask() = 0;
    virtual const UINT32 GetUndefMask() = 0;
    virtual const UINT32 GetArrayDimensions() = 0;

    virtual bool isReadable() = 0;
    virtual bool isWriteable() = 0;
    virtual bool isReadWrite() = 0;

    virtual bool GetFormula1(UINT32& limit, UINT32& vsize) = 0;
    virtual bool GetFormula2(UINT32& limit1, UINT32& limit2, UINT32& vsize1, UINT32& vsize2) = 0;

    virtual unique_ptr<IRegisterField> FindField(const char *field_name) = 0;
    virtual unique_ptr<IRegisterField> GetFieldHead() = 0;
    virtual unique_ptr<IRegisterField> GetFieldNext() = 0;
    virtual UINT32 GetHWInitValue(UINT32 *oval) = 0;
    virtual UINT32 GetHWInitColdValue(UINT32 *oval) = 0;
    virtual UINT32 GetHWInitWarmValue(UINT32 *oval) = 0;
};

// typedef for register vector
typedef vector<unique_ptr<IRegisterClass>> IRegVec_t;

class IRegisterMap
{
public:
// IRegisterMap Interface
    virtual ~IRegisterMap() { }
    virtual void MatchRegisters(IRegVec_t& regvec, const char* name) const = 0;
    virtual unique_ptr<IRegisterClass> FindRegister(const char *reg_name,
                                         UINT32 *pRegIdxI = nullptr,
                                         UINT32 *pRegIdxJ = nullptr) const = 0;
    virtual unique_ptr<IRegisterClass> FindRegister(UINT32 RegisterOffset) const = 0;
    virtual unique_ptr<IRegisterClass> GetRegisterHead() = 0;
    virtual unique_ptr<IRegisterClass> GetRegisterNext() = 0;
};

#endif // INCLUDED_IREGISTERMAP_H
