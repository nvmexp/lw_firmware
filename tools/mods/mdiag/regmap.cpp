/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "mdiag/utils/types.h"
#include "IRegisterMap.h"
#include "core/include/massert.h"
#include "core/include/refparse.h"
#include "sysspec.h"

// XXX Current implementation leaks memory like a sieve.

class RegisterValue: public IRegisterValue
{
private:
   const RefManualFieldValue *m_Val;

public:
   RegisterValue(const RefManualFieldValue *Val)
   {
      m_Val = Val;
   }

   const char* GetName()
   {
      return m_Val->Name.c_str();
   }

   const UINT32 GetValue()
   {
      return m_Val->Value;
   }

   const UINT32 IsHWInit()
   {
      return (m_Val->initType != InitNone);
   }

   const UINT32 IsHWInitCold()
   {
      return (m_Val->initType == InitCold);
   }

   const UINT32 IsHWInitWarm()
   {
      return (m_Val->initType == InitWarm);
   }

   const bool IsConstant()
   {
      return m_Val->IsConstant;
   }

   const bool IsTask()
   {
      return m_Val->IsTask;
   }
};

class RegisterField: public IRegisterField
{
private:
   const RefManualRegisterField *m_Field;
   int m_Index;

public:
   RegisterField(const RefManualRegisterField *Field)
   {
      m_Field = Field;
      m_Index = 0;
   }

   const char* GetName()
   {
      return m_Field->GetName().c_str();
   }

   const char* GetAccess()
   {
      return m_Field->GetAccess().c_str();
   }

   const UINT32 GetStartBit()
   {
      return m_Field->GetLowBit();
   }

   const UINT32 GetEndBit()
   {
      return m_Field->GetHighBit();
   }

   const UINT32 GetMask()
   {
      return m_Field->GetMask();
   }

   const UINT32 GetReadMask()
   {
      return m_Field->IsReadable() ? m_Field->GetMask() : 0;
   }

   const UINT32 GetWriteMask()
   {
      return m_Field->IsWriteable() ? m_Field->GetMask() : 0;
   }

   const UINT32 GetUnwriteableMask()
   {
      return m_Field->IsUnWriteable() ? m_Field->GetMask() : 0;
   }

   const UINT32 GetConstMask()
   {
      return m_Field->IsConstant() ? m_Field->GetMask() : 0;
   }

   bool isReadable()
   {
      return m_Field->IsReadable();
   }

   bool isConstant()
   {
      return m_Field->IsConstant();
   }

   bool isWriteable()
   {
      return m_Field->IsWriteable();
   }

   bool isReadWrite()
   {
      return m_Field->IsReadable() && m_Field->IsWriteable();
   }

   bool isUnknown()
   {
      return m_Field->IsUnknown() ;
   }

   bool isTask()
   {
      return m_Field->IsTask() ;
   }

   unique_ptr<IRegisterValue> FindValue(const char *value_name)
   {
      const RefManualFieldValue *val = m_Field->FindValue(value_name);
      if (val)
      {
         return make_unique<RegisterValue>(val);
      } 
      else
      {
         return nullptr;
      }
   }

   unique_ptr<IRegisterValue> GetValueHead()
   {
      m_Index = 0;
      return GetValueNext();
   }

   unique_ptr<IRegisterValue> GetValueNext()
   {
      if (m_Index >= m_Field->GetNumValues())
      {
         return nullptr;
      }
      return make_unique<RegisterValue>(m_Field->GetValue(m_Index++));
   }

   UINT32 GetHWInitValue(UINT32 *oval)
   {
      // this is ambiguous, as conceivably a field has distinct warm- and cold-reset values
      unique_ptr<IRegisterValue> lwr = GetValueHead();
      while (lwr)
      {
        if (lwr->IsHWInit())
        {
           *oval = lwr->GetValue();
           return 0;
        }
        lwr = GetValueNext();
      }
      return 1;
   }

   UINT32 GetHWInitColdValue(UINT32 *oval)
   {
      unique_ptr<IRegisterValue> lwr = GetValueHead();
      while (lwr)
      {
        if (lwr->IsHWInitCold())
        {
           *oval = lwr->GetValue();
           return 0;
        }
        lwr = GetValueNext();
      }
      return 1;
   }

   UINT32 GetHWInitWarmValue(UINT32 *oval)
   {
      unique_ptr<IRegisterValue> lwr = GetValueHead();
      while (lwr)
      {
        if (lwr->IsHWInitWarm())
        {
           *oval = lwr->GetValue();
           return 0;
        }
        lwr = GetValueNext();
      }
      return 1;
   }

   const UINT32 GetMultiple()
   {
      MASSERT(0);
      return 0;
   }
};

class RegisterClass: public IRegisterClass
{
private:
   const RefManualRegister *m_Reg;
   int m_Index;

public:
   RegisterClass(const RefManualRegister *Reg)
   {
      m_Reg = Reg;
      m_Index = 0;
   }

   const char* GetName()
   {
      return m_Reg->GetName().c_str();
   }

   const char* GetAccess()
   {
      return m_Reg->GetAccess().c_str();
   }

   const UINT32 GetAddress()
   {
      return m_Reg->GetOffset();
   }

   const UINT32 GetAddress(UINT32 i)
   {
      return m_Reg->GetOffset(i);
   }

   const UINT32 GetAddress(UINT32 i, UINT32 j)
   {
      return m_Reg->GetOffset(i, j);
   }

   const UINT32 GetClassId()
   {
      return m_Reg->GetClassID();
   }

   const UINT32 GetPrefix()
   {
      MASSERT(0);
      return 0;
   }

   const UINT32 GetReadMask()
   {
      return m_Reg->GetReadableFieldMask();
   }

   const UINT32 GetWriteMask()
   {
      return m_Reg->GetWriteableFieldMask();
   }

   const UINT32 GetTaskMask()
   {
      return m_Reg->GetTaskFieldMask();
   }

   const UINT32 GetUnwriteableMask()
   {
      return m_Reg->GetUnWriteableFieldMask();
   }

   const UINT32 GetConstMask()
   {
      return m_Reg->GetConstantFieldMask();
   }

   const UINT32 GetConstValue()
   {
      return m_Reg->GetConstantFieldValue();
   }

   const UINT32 GetW1ClrMask()
   {
      return m_Reg->GetClearOnOneFieldMask();
   }

   const UINT32 GetUndefMask()
   {
      UINT32 rmask = m_Reg->GetReadableFieldMask();
      UINT32 wmask = m_Reg->GetWriteableFieldMask();
      UINT32 tmask = m_Reg->GetTaskFieldMask();
      UINT32 cmask = m_Reg->GetUnWriteableFieldMask() | m_Reg->GetConstantFieldMask();
      UINT32 w1clr = m_Reg->GetClearOnOneFieldMask();

      // anything not in these is undefined
      UINT32 def = rmask | wmask | tmask | cmask | w1clr;
      return ~def;
   }

   const UINT32 GetArrayDimensions()
   {
      return m_Reg->GetDimensionality();
   }

   bool isReadable()
   {
      return m_Reg->IsReadable();
   }

   bool isWriteable()
   {
      return m_Reg->IsWriteable();
   }

   bool isReadWrite()
   {
      return m_Reg->IsReadable() && m_Reg->IsWriteable();
   }

   // XXX These arguments are very poorly named!
   bool GetFormula1(UINT32& limit, UINT32& vsize)
   {
      limit = m_Reg->GetArraySizeI();
      vsize = m_Reg->GetStrideI();
      return (m_Reg->GetDimensionality() >= 1);
   }

   // XXX These arguments are very poorly named!
   bool GetFormula2(UINT32& limit1, UINT32& limit2, UINT32& vsize1, UINT32& vsize2)
   {
      limit1 = m_Reg->GetArraySizeI();
      limit2 = m_Reg->GetArraySizeJ();
      vsize1 = m_Reg->GetStrideI();
      vsize2 = m_Reg->GetStrideJ();
      return (m_Reg->GetDimensionality() >= 2);
   }

   unique_ptr<IRegisterField> FindField(const char *field_name)
   {
        const RefManualRegisterField *temp_reg = m_Reg->FindField(field_name);
        if (temp_reg == nullptr)
        { 
            return nullptr;
        }
        else
        { 
            return make_unique<RegisterField>(temp_reg);
        }
   }

   unique_ptr<IRegisterField> GetFieldHead()
   {
      m_Index = 0;
      return GetFieldNext();
   }

   unique_ptr<IRegisterField> GetFieldNext()
   {
      if (m_Index >= m_Reg->GetNumFields())
      {
         return nullptr;
      }
      return make_unique<RegisterField>(m_Reg->GetField(m_Index++));
   }

   UINT32 GetHWInitValue(UINT32 *oval)
   {
      unique_ptr<IRegisterField> lwr = GetFieldHead();
      UINT32 v = 0x0;
      UINT32 hasInitVal = 0;
      while (lwr)
      {
        UINT32 f;
        if ( lwr->GetHWInitValue(&f) == 0 )
        {
           // presumption is that fields are distinct, so don't have to mask
           v |= (f << lwr->GetStartBit());
           hasInitVal = 1;
        }
        lwr = GetFieldNext();
      }
      *oval = v;
      return hasInitVal;
   }

   UINT32 GetHWInitColdValue(UINT32 *oval)
   {
      unique_ptr<IRegisterField> lwr = GetFieldHead();
      UINT32 v = 0x0;
      UINT32 hasInitVal = 0;
      while (lwr)
      {
        UINT32 f;
        if ( lwr->GetHWInitColdValue(&f) == 0 )
        {
           // presumption is that fields are distinct, so don't have to mask
           v |= (f << lwr->GetStartBit());
           hasInitVal = 1;
        }
        lwr = GetFieldNext();
      }
      *oval = v;
      return hasInitVal;
   }

   UINT32 GetHWInitWarmValue(UINT32 *oval)
   {
      unique_ptr<IRegisterField> lwr = GetFieldHead();
      UINT32 v = 0x0;
      UINT32 hasInitVal = 0;
      while (lwr)
      {
        UINT32 f;
        if ( lwr->GetHWInitWarmValue(&f) == 0 )
        {
           // presumption is that fields are distinct, so don't have to mask
           v |= (f << lwr->GetStartBit());
           hasInitVal = 1;
        }
        lwr = GetFieldNext();
      }
      *oval = v;
      return hasInitVal;
   }
};

class RegisterMap: public IRegisterMap
{
private:
    int m_RefCount;
    const RefManual *m_Manual;
    int m_Index;

public:
    RegisterMap(const RefManual *Manual)
    {
        m_Manual = Manual;
        m_Index = 0;
    }

    unique_ptr<IRegisterClass> FindRegister
    (
        const char *reg_name,
        UINT32 *pIdxI = nullptr,
        UINT32 *pIdxJ = nullptr
    ) const
    {
        RefManual::ParsedRegExpr parsedResult;
        if (OK == m_Manual->ParseRegExpr(reg_name, &parsedResult))
        {
            if (pIdxI)
                *pIdxI = parsedResult.IdxI;

            if (pIdxJ)
                *pIdxJ = parsedResult.IdxJ;

            return make_unique<RegisterClass>(parsedResult.pRefReg);
        }
        else
        {
            return nullptr;
        }
    }

    unique_ptr<IRegisterClass> FindRegister(UINT32 RegisterOffset) const
    {
        const RefManualRegister *Reg =
            m_Manual->FindRegister(RegisterOffset);
        if (Reg)
        {
            return make_unique<RegisterClass>(Reg);
        }
        else
        {
            return nullptr;
        }
    }

    unique_ptr<IRegisterClass> GetRegisterHead()
    {
        m_Index = 0;
        return GetRegisterNext();
    }

    unique_ptr<IRegisterClass> GetRegisterNext()
    {
        if (m_Index >= m_Manual->GetNumRegisters())
        {
            return nullptr;
        }
        return make_unique<RegisterClass>(m_Manual->GetRegister(m_Index++));
    }

    void MatchRegisters(IRegVec_t& regvec, const char* name) const
    {
        const RefManualRegister* reg = NULL;

        // check if we need to do a wildcard match
        if(strchr(name, '*') == NULL && strchr(name, '?') == NULL)
        {
            // no wildcards, do a single reg match
            reg = m_Manual->FindRegister(name);
            if(reg != NULL)
            {
                DebugPrintf("Matched register: %s\n", reg->GetName().c_str());
                regvec.push_back(make_unique<RegisterClass>(reg));
            }
        }
        else
        {
            int length = m_Manual->GetNumRegisters();
            for(int i = 0; i < length; i++)
            {
                reg = m_Manual->GetRegister(i);
                if(reg == NULL)
                    continue;
                bool match = Utility::MatchWildCard(reg->GetName().c_str(), name);
                if(match)
                {
                    DebugPrintf("Matched register: %s\n", reg->GetName().c_str());
                    regvec.push_back(make_unique<RegisterClass>(reg));
                }
            }
        }
    }
};

unique_ptr<IRegisterMap> CreateRegisterMap(const RefManual *Manual)
{
   return make_unique<RegisterMap>(Manual);
}
