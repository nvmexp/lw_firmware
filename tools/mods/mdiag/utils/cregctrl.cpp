/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011,2013,2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <ctype.h>
#include "core/include/gpu.h"
#include "core/include/utility.h"
#include "mdiag/lwgpu.h"
#include "core/include/massert.h"
#include "mdiag/sysspec.h"
#include "cregctrl.h"
#include "utils.h"

const int   CREG_MAX_STR_SIZE = 8192;
const int   CREG_MAX_BUF_SIZE = 1024;
const char* CREG_DELIMITERS   = ":\n\r";

vector<unique_ptr<IRegisterClass>> CRegCtrlFactory::s_Registers;

//-----------------------------------------------------------------------
CRegRef::CRegRef()
{
}

//-----------------------------------------------------------------------
CRegRef::~CRegRef()
{
}

//-----------------------------------------------------------------------
void CRegRef::AddRegRef(unique_ptr<IRegisterClass> reg, UINT32 address)
{
    IRegRef_t ireg;
    ireg.m_reg = move(reg);
    ireg.m_address = address;
    m_regrefvec.push_back(move(ireg));
}

//-----------------------------------------------------------------------
IRegRefVec_t::iterator CRegRef::Begin()
{
    return m_regrefvec.begin();
}

//-----------------------------------------------------------------------
IRegRefVec_t::iterator CRegRef::End()
{
    return m_regrefvec.end();
}

//-----------------------------------------------------------------------
CRegisterControl::CRegisterControl()
{
    m_action = CRegisterControl::EUNKNOWN;
    m_address = 0x0;
    m_mask = 0xFFFFFFFF;
    m_data = 0x0;
    m_rdiSelect = 0;
    m_rdiAddress = 0;
    m_dataAddress = 0;
    m_arrayIdx1 = 0;
    m_arrayIdx2 = 0;
    m_arrayIdxNum = 0;
    m_regPtr = nullptr;
    m_ctxIdx = 0;
    m_isCtxReg = false;
    m_domain = RegHalDomain::RAW;
}

string CRegisterControl::GetFieldAndValueName
(
    IRegisterClass* reg
) const
{
    string fieldAndValue;

    if (NULL == reg)
    {
        return fieldAndValue;
    }

    unique_ptr<IRegisterField> field = reg->GetFieldHead();
    UINT32 mask = m_mask;
    while ((NULL != field) && (0 != mask))
    {
        if (field->GetMask() & mask)
        {
            string fieldValName = GetValuename(field.get());

            if (fieldValName.empty())
            {
                // field name:valnum
                fieldAndValue += Utility::StrPrintf("%s:%d ",
                    field->GetName(),
                    (m_data & mask) >> field->GetStartBit());
            }
            else
            {
                // value name
                fieldAndValue += Utility::StrPrintf("%s ",
                    fieldValName.c_str());
            }

            // clear bits processed to speed up the search
            mask &= ~field->GetMask();
        }

        //
        // according the implementation in regmap.cpp
        // delete is needed to avoid memory leak
        // but it should be the one allocating memory
        // who frees the memory
        //
        // Fix me:
        // delete field;
        field = reg->GetFieldNext();
    }

    return fieldAndValue;
}

string CRegisterControl::GetValuename
(
    IRegisterField* field
) const
{
    string valname;

    if (NULL == field)
    {
        return valname;
    }

    unique_ptr<IRegisterValue> regVal = field->GetValueHead();
    UINT32 valSearched = field->GetMask() & m_data;
    while (NULL != regVal)
    {
        UINT32 fieldValue = regVal->GetValue() << field->GetStartBit();
        if (valSearched == fieldValue)
        {
            valname = regVal->GetName();
            break;
        }

        //
        // according the implementation in regmap.cpp
        // delete is needed to avoid memory leak
        // but it should be the one allocating memory
        // who frees the memory
        //
        // Fix me:
        // delete regVal;
        regVal = field->GetValueNext();
    }

    return valname;
}

void CRegisterControl::PrintRegInfo() const
{
    // sanity check
    if (!m_regPtr || (EUNKNOWN == m_action))
    {
        return;
    }

    // collect info
    string actionName;
    switch(m_action)
    {
    case EWRITE:
    case EARRAY1WRITE:
    case EARRAY2WRITE:
        actionName = "WRITE";
        break;

    case EMODIFY:
    case EARRAY1MODIFY:
    case EARRAY2MODIFY:
        actionName = "MODIFY";
        break;

    case EREAD:
    case EARRAY1READ:
    case EARRAY2READ:
        actionName = "READ";
        break;

    case ECOMPARE:
        actionName = "COMPARE";
        break;

    case EUNKNOWN:
    default:
        MASSERT(!"This should not happen after sanity check.");
    }

    string regName = m_regPtr->GetName();
    switch(m_arrayIdxNum)
    {
    case 1:
        regName += Utility::StrPrintf("(%d)", m_arrayIdx1);
        break;

    case 2:
        regName += Utility::StrPrintf("(%d,%d)", m_arrayIdx1, m_arrayIdx2);
        break;

    default:
        // do nothing
        break;
    }

    string fieldValueName = GetFieldAndValueName(m_regPtr);

    // print info collected
    InfoPrintf("Register controls:\n"
               "       action=%s\n"
               "       regdomain=%s\n"
               "       regspace=%s\n"
               "       regname=%s\n"
               "       regfieldval=%s\n"
               "       regaddr=0x%08x data=0x%08x mask=0x%08x\n",
               actionName.c_str(),
               RegHalTable::ColwertToString(m_domain).c_str(),
               m_regSpace.c_str(),
               regName.c_str(),
               fieldValueName.c_str(),
               m_address, m_data, m_mask);
}

//-----------------------------------------------------------------------
CRegCtrlFactory::CRegCtrlFactory(LWGpuResource* cb) : m_cb(cb)
{
    if (cb == NULL)
    {
        MASSERT(!"Cannot create a CRegCtrlFactory obj w/ a NULL callback ptr\n");
    }
}

//-----------------------------------------------------------------------
CRegCtrlFactory::~CRegCtrlFactory()
{
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::ParseLine(RegCtrlVec_t& regvec, const char* str)
{
    //
    // first determine the action type, and handle it if it is a file action
    bool flag = false;
    char regstr[CREG_MAX_BUF_SIZE];
    char idx1str[CREG_MAX_BUF_SIZE];
    char idx2str[CREG_MAX_BUF_SIZE];
    char datastr[CREG_MAX_BUF_SIZE];
    char maskstr[CREG_MAX_BUF_SIZE];
    switch(str[0])
    {
    case 'F':
        flag = ParseRegFile(regvec, str);
        break;
    case 'M':
        // single reg modify
        flag = ParseSingleReg(str, regstr, datastr, maskstr,
            CRegisterControl::EMODIFY);
        if (!flag)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }
        flag = CreateSingleReg(regvec, regstr, datastr, maskstr,
            CRegisterControl::EMODIFY);
        break;
    case 'W':
        // single reg write
        flag = ParseSingleReg(str, regstr, datastr, maskstr,
            CRegisterControl::EWRITE);
        if (!flag)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }
        flag = CreateSingleReg(regvec, regstr, datastr, maskstr,
            CRegisterControl::EWRITE);
        break;
    case 'C':
        // single reg compare
        flag = ParseSingleReg(str, regstr, datastr, maskstr,
            CRegisterControl::ECOMPARE);
        if (!flag)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }
        flag = CreateSingleReg(regvec, regstr, datastr, maskstr,
            CRegisterControl::ECOMPARE);
        break;
    case 'R':
        // single reg read
        flag = ParseSingleReg(str, regstr, NULL, maskstr,
            CRegisterControl::EREAD);
        if (!flag)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }
        flag = CreateSingleReg(regvec, regstr, NULL, maskstr,
            CRegisterControl::EREAD);
        break;
    case 'A':
        if (strncmp(str, "AW", 2) == 0)
        {
            // array1 write
            flag = ParseArray1Reg(str, regstr, idx1str, datastr, maskstr,
                CRegisterControl::EARRAY1WRITE);
            if (!flag)
            {
                ErrPrintf("Invalid register control string:%s\n", str);
                return false;
            }
            flag = CreateArray1Reg(regvec, regstr, idx1str, datastr,
                maskstr, CRegisterControl::EARRAY1WRITE);
        }
        else if (strncmp(str, "A2W", 3) == 0)
        {
            // array2 write
            flag = ParseArray2Reg(str, regstr, idx1str, idx2str, datastr,
                maskstr, CRegisterControl::EARRAY2WRITE);
            if (!flag)
            {
                ErrPrintf("Invalid register control string:%s\n", str);
                return false;
            }
            flag = CreateArray2Reg(regvec, regstr, idx1str, idx2str,
                datastr, maskstr, CRegisterControl::EARRAY2WRITE);
        }
        else if (strncmp(str, "AM", 2) == 0)
        {
            // array1 modify
            flag = ParseArray1Reg(str, regstr, idx1str, datastr, maskstr,
                CRegisterControl::EARRAY1MODIFY);
            if (!flag)
            {
                ErrPrintf("Invalid register control string:%s\n", str);
                return false;
            }
            flag = CreateArray1Reg(regvec, regstr, idx1str, datastr,
                maskstr, CRegisterControl::EARRAY1MODIFY);
        }
        else if (strncmp(str, "A2M", 3) == 0)
        {
            // array2 modify
            flag = ParseArray2Reg(str, regstr, idx1str, idx2str, datastr,
                maskstr, CRegisterControl::EARRAY2MODIFY);
            if (!flag)
            {
                ErrPrintf("Invalid register control string:%s\n", str);
                return false;
            }
            flag = CreateArray2Reg(regvec, regstr, idx1str, idx2str,
                datastr, maskstr, CRegisterControl::EARRAY2MODIFY);
        }
        else if (strncmp(str, "AR", 2) == 0)
        {
            // array1 read
            flag = ParseArray1Reg(str, regstr, idx1str, datastr, maskstr,
                CRegisterControl::EARRAY1READ);
            if (!flag)
            {
                ErrPrintf("Invalid register control string:%s\n", str);
                return false;
            }
            flag = CreateArray1Reg(regvec, regstr, idx1str, NULL,
                maskstr, CRegisterControl::EARRAY1READ);
        }
        else if (strncmp(str, "A2R", 3) == 0)
        {
            // array2 read
            flag = ParseArray2Reg(str, regstr, idx1str, idx2str, datastr,
                maskstr, CRegisterControl::EARRAY2READ);
            if (!flag)
            {
                ErrPrintf("Invalid register control string:%s\n", str);
                return false;
            }
            flag = CreateArray2Reg(regvec, regstr, idx1str, idx2str, NULL,
                maskstr, CRegisterControl::EARRAY2READ);
        }
        else
        {
            ErrPrintf("Unknown register control action : %s\n", str);
            return false;
        }
        break;
    default:
        ErrPrintf("Unknown register control action : %s\n", str);
        break;
    }

    return flag;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::ParseRegFile(RegCtrlVec_t& regvec, const char* str)
{
    FileIO *file;
    char line[CREG_MAX_STR_SIZE + 1];
    bool errorInFile = false;
    vector<string> badLines;

    char* fname    = NULL;
    char* overflow = NULL;
    if (strncpy(line, str, CREG_MAX_STR_SIZE) == NULL)
    {
        ErrPrintf("User supplied -reg string exceeds max buffer size: %d\n", CREG_MAX_STR_SIZE);
        return false;
    }

    strtok(line, CREG_DELIMITERS);
    fname    = strtok(NULL, CREG_DELIMITERS);
    overflow = strtok(NULL, CREG_DELIMITERS);
    if (overflow != NULL )
    {
        ErrPrintf("Extra argument supplied for -reg F:<filename> command: %s\n", str);
        return false;
    }

    file = Sys::GetFileIO(fname, "r");
    if (file)
    {
        while (file->FileGetStr(line, sizeof(line)-1))
        {
            if ((line[0] == '#') || (line[0] == '\n'))
                continue; // skip comments and blank lines

            if (ParseLine(regvec, line) == false)
            {
                badLines.push_back(line);
                errorInFile = true;
            }
        }
    }
    else
    {
        ErrPrintf("ProcessRegisterFile: can't open file %s, failing test\n", fname);
        return false;
    }

    if (errorInFile)
    {
        ErrPrintf("Error(s) found in file: %s\n", fname);
        vector<string>::const_iterator itend(badLines.end());
        for (vector<string>::const_iterator it(badLines.begin()); it != itend; ++it)
            ErrPrintf("\t%s", it->c_str());
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::IsFloorSweptReg(const CRegisterControl& regc)
{
    // pGpu->TpcMask() has PGRAPH register access from SW mods.
    // In SMC mode, it will cause crash since there's no SMC info.
    // This is a workaround for SMC mode.
    // Will fix this after RM provides a new RM ctrl call. See bug2090133

    if (m_cb->IsSMCEnabled() || !regc.m_regPtr)
        return false;

    GpuSubdevice* pGpuSubdev = m_cb->GetGpuSubdevice();
    UINT32 smMask  = pGpuSubdev->GetFsImpl()->SmMask();
    // The GPC/TPC number in the register is logical ones
    UINT32 logicalGpcNum = ~0U;
    UINT32 logicalTpcNum = ~0U;
    UINT32 hwGpcNum = ~0U;
    UINT32 hwTpcNum = ~0U;
    UINT32 smNum = ~0U;

    const char* regname = regc.m_regPtr->GetName();
    const char* gpcStr = strstr(regname, "GPC");

    if (gpcStr)
    {
        if (isdigit(gpcStr[3]))
        {
            logicalGpcNum = ParseFromStr((const char*)(&(gpcStr[3])));
            // If there is no HW GPC for this logical GPC then this function will return rc != OK
            // This means this logical GPC is floorswept
            if (pGpuSubdev->VirtualGpcToHwGpc(logicalGpcNum, &hwGpcNum) != OK)
            {
                return true;
            }
            
            // GPC is not floorswept- check for TPC FS
            const char* tpcStr = strstr(regname, "TPC");
            if (tpcStr)
            {
                if (isdigit(tpcStr[3]))
                {
                    logicalTpcNum = ParseFromStr((const char*)(&(tpcStr[3])));
                    // If there is no HW TPC for this logical TPC then this function will return rc != OK
                    // This means this logical TPC within the GPC is floorswept
                    if (pGpuSubdev->VirtualTpcToHwTpc(hwGpcNum, logicalTpcNum, &hwTpcNum) != OK)
                    {
                        return true;
                    }
                }
            
                if (strstr(regname, "SM" ) && regc.m_arrayIdxNum)
                {
                    smNum = regc.m_arrayIdx1;
                    if (!(BIT(smNum) & smMask))
                    {
                        return true;
                    }
                }
            }
       }
    }

    return false;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::CreateSingleReg
(
    RegCtrlVec_t& regvec,
    const char* regstr,
    const char* datastr,
    const char* maskstr,
    CRegisterControl::RCActionType action
)
{
    // sanity check args
    if (regstr == NULL)
    {
        MASSERT(!"Required register string ptr cannot be null\n");
    }

    // parse registers
    CRegRef regptrvec;
    if (MatchRegisters(regstr, regptrvec) == false)
    {
        ErrPrintf("Cannot match registers: %s\n", regstr);
        return false;
    }

    RegHalDomain regDomain = MDiagUtils::GetDomain(regstr);

    IRegRefVec_t::iterator it = regptrvec.Begin();
    IRegRefVec_t::iterator itend = regptrvec.End();
    for(; it != itend; ++it)
    {
        CRegisterControl regc;
        regc.m_action  = action;
        regc.m_domain = regDomain;
        s_Registers.push_back(move(it->m_reg));
        regc.m_regPtr  = s_Registers.back().get();
        regc.m_address = it->m_address;
        if (!MatchRegFields(datastr, maskstr, regc.m_regPtr,
                           &regc.m_data, &regc.m_mask, action))
        {
            return false;
        }

        if (IsFloorSweptReg(regc))
        {
            ErrPrintf("Ignoring attempted access to a floorswept register %s\n", regc.m_regPtr->GetName());
            continue;
        }

        regvec.push_back(regc);
    }

    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::CreateArray1Reg
(
    RegCtrlVec_t& regvec,
    const char* regstr,
    const char* idxstr,
    const char* datastr,
    const char* maskstr,
    CRegisterControl::RCActionType action
)
{
    // sanity check args
    if (regstr == NULL || idxstr == NULL)
    {
        MASSERT(!"Required register and index strings cannot be NULL\n");
    }

    // parse registers
    CRegRef regptrvec;
    if (MatchRegisters(regstr, regptrvec) == false)
    {
        ErrPrintf("Cannot match registers: %s\n", regstr);
        return false;
    }

    RegHalDomain regDomain = MDiagUtils::GetDomain(regstr);

    // check if index is a number, or a "*"
    bool idxIsNum = IsNumerical(idxstr);
    bool idxIsStar = HasStar(idxstr);
    IRegRefVec_t::iterator it = regptrvec.Begin();
    IRegRefVec_t::iterator itend = regptrvec.End();
    if (idxIsNum)
    {
        UINT32 index = ParseFromStr(idxstr);
        for(; it != itend; ++it)
        {
            // check array index
            UINT32 limit = 0x0;
            UINT32 stride = 0x0;
            s_Registers.push_back(move(it->m_reg));
            IRegisterClass* const reg = s_Registers.back().get();

            // register ptr cannot be null for array registers
            MASSERT(reg);

            reg->GetFormula1(limit, stride);
            if (reg->GetArrayDimensions() != 1)
            {
                // that's OK to access a non-indexed register as an array element 0 (bug 248973)
                if (reg->GetArrayDimensions() != 0 || index != 0)
                {
                    ErrPrintf("Reg type:%d name:%s, but this is not an array w/ dimension=1\n",
                        action, reg->GetName());
                    continue;
                }
            }
            if (index >= limit)
            {
                ErrPrintf("Register of type %d specified with invalid array index1 (0<=%d<%d)\n",
                    action, index, limit);
                continue;
            }

            // create new reg control obj, insert into vector
            CRegisterControl regc;
            regc.m_action      = action;
            regc.m_domain      = regDomain;
            regc.m_regPtr      = reg;
            regc.m_address     = reg->GetAddress() + (index*stride);
            regc.m_arrayIdx1   = index;
            regc.m_arrayIdxNum = 1;
            if (!MatchRegFields(datastr, maskstr, reg,
                               &regc.m_data, &regc.m_mask, action))
            {
                return false;
            }

            if (IsFloorSweptReg(regc))
            {
                ErrPrintf("Ignoring attempted access to a floorswept register %s\n", reg->GetName());
                continue;
            }

            regvec.push_back(regc);
        }
    }
    else if (idxIsStar)
    {
        for(; it != itend; ++it)
        {
            // check array index
            UINT32 limit = 0x0;
            UINT32 stride = 0x0;
            s_Registers.push_back(move(it->m_reg));
            IRegisterClass* const reg = s_Registers.back().get();

            // register ptr cannot be null for register arrays
            MASSERT(reg);

            reg->GetFormula1(limit, stride);

            for(UINT32 i = 0; i < limit; ++i)
            {
                if (reg->GetArrayDimensions() != 1)
                {
                    // that's OK to access a non-indexed register as an array element 0 (bug 248973)
                    if (reg->GetArrayDimensions() != 0 || i != 0)
                    {
                        ErrPrintf("Reg type:%d name:%s, but this is not an array w/ dimension=1\n",
                            action, reg->GetName());
                        continue;
                    }
                }
                if (i >= limit)
                {
                    continue;
                }

                // create new reg control obj, insert into vector
                CRegisterControl regc;
                regc.m_regPtr      = reg;
                regc.m_action      = action;
                regc.m_address     = reg->GetAddress() + (i*stride);
                regc.m_arrayIdx1   = i;
                regc.m_arrayIdxNum = 1;
                if (!MatchRegFields(datastr, maskstr, reg,
                                   &regc.m_data, &regc.m_mask, action))
                {
                    return false;
                }

                if (IsFloorSweptReg(regc))
                {
                    ErrPrintf("Ignoring attempted access to a floorswept register %s\n", reg->GetName());
                    continue;
                }

                regvec.push_back(regc);
            }
        }
    }
    else
    {
        ErrPrintf("Invalid index string specified: %s\n", idxstr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::CreateArray2Reg
(
    RegCtrlVec_t& regvec,
    const char* regstr,
    const char* idx1str,
    const char* idx2str,
    const char* datastr,
    const char* maskstr,
    CRegisterControl::RCActionType action
)
{
    // sanity check args
    if (regstr == NULL || idx1str == NULL || idx2str == NULL)
    {
        MASSERT(!"Required register and index strings cannot be NULL\n");
    }

    // parse registers
    CRegRef regptrvec;
    if (MatchRegisters(regstr, regptrvec) == false)
    {
        ErrPrintf("Cannot match registers: %s\n", regstr);
        return false;
    }

    // check if indicies is a number, or a "*"
    bool idx1IsNum = IsNumerical(idx1str);
    bool idx2IsNum = IsNumerical(idx2str);
    bool idx1IsStar = HasStar(idx1str);
    bool idx2IsStar = HasStar(idx2str);

    IRegRefVec_t::iterator it = regptrvec.Begin();
    IRegRefVec_t::iterator itend = regptrvec.End();
    if (idx1IsNum && idx2IsNum)
    {
        // both index values are integers
        UINT32 idx1 = ParseFromStr(idx1str);
        UINT32 idx2 = ParseFromStr(idx2str);
        UINT32 limit1 = 0x0;
        UINT32 limit2 = 0x0;
        UINT32 stride1 = 0x0;
        UINT32 stride2 = 0x0;

        for(; it != itend; ++it)
        {
            s_Registers.push_back(move(it->m_reg));
            IRegisterClass* const reg = s_Registers.back().get();

            // register ptr cannot be null for register arrays
            MASSERT(reg);
            reg->GetFormula2(limit1, limit2, stride1, stride2);

            // sanity check index values
            if (reg->GetArrayDimensions() != 2)
            {
                ErrPrintf("Register of type %d specified, but this register is not an Array w/ dimension=2\n",
                    action);
                continue;
            }
            if (idx1 >= limit1)
            {
                ErrPrintf("Register of type %d specified with invalid array index1 (0<=%d<%d)\n",
                    action, idx1, limit1);
                continue;
            }
            if (idx2 >= limit2)
            {
                ErrPrintf("Register of type %d specified with invalid array index2 (0<=%d<%d)\n",
                    action, idx2, limit2);
                continue;
            }

            // create new reg control obj, insert into vector
            CRegisterControl regc;
            regc.m_regPtr      = reg;
            regc.m_action      = action;
            regc.m_address     = reg->GetAddress() + (idx1 * stride1) +(idx2 * stride2);
            regc.m_arrayIdx1   = idx1;
            regc.m_arrayIdx2   = idx2;
            regc.m_arrayIdxNum = 2;
            if (!MatchRegFields(datastr, maskstr, reg,
                               &regc.m_data, &regc.m_mask, action))
            {
                return false;
            }

            regvec.push_back(regc);
        }
    }
    else if (idx1IsNum && idx2IsStar)
    {
        // index1 is an integer, index2 is a star
        UINT32 idx1 = ParseFromStr(idx1str);
        UINT32 limit1 = 0x0;
        UINT32 limit2 = 0x0;
        UINT32 stride1 = 0x0;
        UINT32 stride2 = 0x0;

        for(; it != itend; ++it)
        {
            s_Registers.push_back(move(it->m_reg));
            IRegisterClass* const reg = s_Registers.back().get();

            // register ptr cannot be null for register arrays
            MASSERT(reg);
            reg->GetFormula2(limit1, limit2, stride1, stride2);

            // sanity check index values
            if (reg->GetArrayDimensions() != 2)
            {
                ErrPrintf("Register of type %d specified, but this register is not an Array w/ dimension=2\n",
                    action);
                continue;
            }
            if (idx1 >= limit1)
            {
                ErrPrintf("Register of type %d specified with invalid array index1 (0<=%d<%d)\n",
                    action, idx1, limit1);
                continue;
            }

            UINT32 idx2 = 0x0;
            for(; idx2 < limit2; ++idx2)
            {
                // create new reg control obj, insert into vector
                CRegisterControl regc;
                regc.m_regPtr      = reg;
                regc.m_action      = action;
                regc.m_address     = reg->GetAddress() + (idx1 * stride1) +(idx2 * stride2);
                regc.m_arrayIdx1   = idx1;
                regc.m_arrayIdx2   = idx2;
                regc.m_arrayIdxNum = 2;
                if (!MatchRegFields(datastr, maskstr, reg,
                                   &regc.m_data, &regc.m_mask, action))
                {
                    return false;
                }

                regvec.push_back(regc);
            }
        }

    }
    else if (idx1IsStar && idx2IsNum)
    {
        // index1 is star, index2 is an integer
        UINT32 idx2 = ParseFromStr(idx2str);
        UINT32 limit1 = 0x0;
        UINT32 limit2 = 0x0;
        UINT32 stride1 = 0x0;
        UINT32 stride2 = 0x0;

        for(; it != itend; ++it)
        {
            s_Registers.push_back(move(it->m_reg));
            IRegisterClass* const reg = s_Registers.back().get();

            // register ptr cannot be null for register arrays
            MASSERT(reg);
            reg->GetFormula2(limit1, limit2, stride1, stride2);

            // sanity check index values
            if (reg->GetArrayDimensions() != 2)
            {
                ErrPrintf("Register of type %d specified, but this register is not an Array w/ dimension=2\n",
                    action);
                continue;
            }
            if (idx2 >= limit2)
            {
                ErrPrintf("Register of type %d specified with invalid array index1 (0<=%d<%d)\n",
                    action, idx2, limit2);
                continue;
            }

            UINT32 idx1 = 0x0;
            for(; idx1 < limit1; ++idx1)
            {
                // create new reg control obj, insert into vector
                CRegisterControl regc;
                regc.m_regPtr      = reg;
                regc.m_action      = action;
                regc.m_address     = reg->GetAddress() + (idx1 * stride1) +(idx2 * stride2);
                regc.m_arrayIdx1   = idx1;
                regc.m_arrayIdx2   = idx2;
                regc.m_arrayIdxNum = 2;
                if (!MatchRegFields(datastr, maskstr, reg,
                                   &regc.m_data, &regc.m_mask, action))
                {
                    return false;
                }
                regvec.push_back(regc);
            }
        }
    }
    else if (idx1IsStar && idx2IsStar)
    {
        // both index values are stars
        UINT32 limit1 = 0x0;
        UINT32 limit2 = 0x0;
        UINT32 stride1 = 0x0;
        UINT32 stride2 = 0x0;

        for(; it != itend; ++it)
        {
            s_Registers.push_back(move(it->m_reg));
            IRegisterClass* const reg = s_Registers.back().get();

            // register ptr cannot be null for register arrays
            MASSERT(reg);
            reg->GetFormula2(limit1, limit2, stride1, stride2);

            // sanity check index values
            if (reg->GetArrayDimensions() != 2)
            {
                ErrPrintf("Register of type %d specified, but this register is not an Array w/ dimension=2\n",
                    action);
                continue;
            }

            // iterate through all reasonable index values
            UINT32 idx1 = 0x0;
            UINT32 idx2 = 0x0;
            for(; idx1 < limit1; ++idx1)
            {
                for(; idx2 < limit2; ++idx2)
                {
                    // create new reg control obj, insert into vector
                    CRegisterControl regc;
                    regc.m_regPtr      = reg;
                    regc.m_action      = action;
                    regc.m_address     = reg->GetAddress() + (idx1 * stride1) +(idx2 * stride2);
                    regc.m_arrayIdx1   = idx1;
                    regc.m_arrayIdx2   = idx2;
                    regc.m_arrayIdxNum = 2;
                    if (!MatchRegFields(datastr, maskstr, reg,
                                       &regc.m_data, &regc.m_mask, action))
                    {
                        return false;
                    }

                    regvec.push_back(regc);
                }
            }
        }
    }
    else
    {
        ErrPrintf("Invalid index strings specified: %s %s\n", idx1str, idx2str);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::IsNumerical(const char* str)
{
    if (str != 0 && *str != '\0')
    {
        for ( ; *str != '\0'; ++str)
        {
            if (!isdigit(*str))
            {
                return false;
            }
        }
        return true;
    }

    return false;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::HasStar(const char* str)
{
    if (strchr(str, '*') == NULL)
        return false;
    return true;
}

bool CRegCtrlFactory::MatchRegFields(const char* datastr,
                                     const char* maskstr,
                                     IRegisterClass* const reg,
                                     UINT32* data, UINT32* mask,
                                     CRegisterControl::RCActionType action)
{
    // For action R
    if ((CRegisterControl::EREAD == action) ||
        (CRegisterControl::EARRAY1READ == action) ||
        (CRegisterControl::EARRAY2READ == action))
    {
        *data = 0; // ignore datastr

        if (0 == strlen(maskstr))
        {
            *mask = 0xFFFFFFFF;
        }
        else if (maskstr[0] == '_')
        {
            MASSERT(reg);
            string regName = reg->GetName();

            regName += maskstr;
            unique_ptr<IRegisterField> field = reg->FindField(regName.c_str());
            if (!field)
            {
                ErrPrintf("Cannot match register field: %s\n", regName.c_str());
                return false;
            }
            *mask = field->GetMask();
        }
        else
            *mask = ParseFromStr(maskstr);

        return true;
    }

    // For action W/M
    MASSERT(datastr != 0);
    if (datastr[0] == '_')
    {
        // name format: start with '_'
        // regname(regstr):fieldname(datastr):fieldvalname(maskstr)
        MASSERT(reg);
        string regName = reg->GetName();

        regName += datastr;
        unique_ptr<IRegisterField> field = reg->FindField(regName.c_str());
        if (!field)
        {
            ErrPrintf("Cannot match register field: %s\n", regName.c_str());
            return false;
        }
        *mask = field->GetMask();

         if (0 == strlen(maskstr))
         {
             ErrPrintf("Register field %s does not have value.\n",
                 regName.c_str());
             return false;
         }
         else if (maskstr[0] == '_')
         {
             regName += maskstr;
             unique_ptr<IRegisterValue> fieldval = field->FindValue(regName.c_str());
             if (!fieldval)
             {
                 ErrPrintf("Cannot match register field value: %s\n",
                     regName.c_str());
                 return false;
             }
             *data = fieldval->GetValue();
         }
         else
            *data = ParseFromStr(maskstr); // It's a number

         *data <<= field->GetStartBit();

         if ((*data & ~(*mask)) != 0)
         {
             ErrPrintf("The value does not fit bit field."
                 "Name: %s - Val:0x%X - Mask:0x%X\n",
                 regName.c_str(), *data, *mask);
             return false;
         }
    }
    else
    {
        // number format
        // regname(regstr):dataval(datastr):maskval(maskstr)
        *data    = ParseFromStr(datastr);
        if (0 == strlen(maskstr)) // Null mask string
        {
            *mask = 0xFFFFFFFF;
        }
        else if (maskstr[0] == '_')
        {
            ErrPrintf("Invalid format: %s is not a number but %s is.\n",
                maskstr,datastr);
            return false;
        }
        else
            *mask    = ParseFromStr(maskstr);
    }

    if (((CRegisterControl::EWRITE == action) ||
       (CRegisterControl::EARRAY1WRITE == action) ||
       (CRegisterControl::EARRAY2WRITE == action)) &&
       (*mask != 0xFFFFFFFF))
    {
        WarnPrintf("Mask field for action\'W\' will be ignored! "
            "0xFFFFFFFF will be used.\n");
    }

    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::MatchRegisters(const char* regstr, CRegRef& regvec)
{
    unique_ptr<IRegisterClass> reg;
    if (HasStar(regstr) == true)
    {
        IRegisterMap* regmap = m_cb->GetRegisterMap();
        if (regmap == NULL)
        {
            MASSERT(!"Fatal error, can't get RegisterMap from LwGpuResource\n");
        }

        char buffer[CREG_MAX_STR_SIZE];
        for(reg = regmap->GetRegisterHead(); reg;
            reg = regmap->GetRegisterNext())
        {
            const char* regname = reg->GetName();
            if (Utility::MatchWildCard(regname, regstr) == true)
            {
                DebugPrintf("Matched %s\n", reg->GetName());
                regvec.AddRegRef(move(reg), reg->GetAddress());
                continue;
            }
            sprintf(buffer, "%d", reg->GetAddress());
            if (Utility::MatchWildCard(buffer, regstr) == true)
            {
                DebugPrintf("Matched %s\n", reg->GetName());
                regvec.AddRegRef(move(reg), reg->GetAddress());
            }
        }
    }
    else
    {
        // lookup single register info
        if (strncmp(regstr, "LW", 2) == 0)
        {
            reg = m_cb->GetRegister(regstr);
            if (!reg)
            {
                ErrPrintf("Can't find address for register name : %s\n", regstr);
                return false;
            }
            regvec.AddRegRef(move(reg), reg->GetAddress());
        }
        else
        {
            UINT32 address = ParseFromStr(regstr);
            reg = m_cb->GetRegister(address);
            if (!reg)
            {
                WarnPrintf("Address %08x does not have a corresponding register!\n", address);
            }
            regvec.AddRegRef(move(reg), address);
        }
    }

    return true;
}

//-----------------------------------------------------------------------
UINT32 CRegCtrlFactory::ParseFromStr(const char* str)
{
    char* dummy;
    UINT32 value = 0x0;
    if (str != NULL)
    {
        value = strtoul(str, &dummy, 0);
        if (str == dummy)
        {
            ErrPrintf("%s: %s is not a valid number\n", __FUNCTION__, str);
            MASSERT(!"Wrong register value used");
        }
    }

    return value;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::ParseSingleReg
(
    const char* str,
    char* regstr,
    char* datastr,
    char* maskstr,
    CRegisterControl::RCActionType action
)
{
    char buffer[CREG_MAX_STR_SIZE + 1];
    strncpy(buffer, str, CREG_MAX_STR_SIZE);

    char* t1 = NULL;
    char* t2 = NULL;
    char* t3 = NULL;
    char* t4 = NULL;
    char* t5 = NULL;
    switch(action)
    {
    case CRegisterControl::EMODIFY:
    case CRegisterControl::EWRITE:
    case CRegisterControl::ECOMPARE:
        // format should be:
        // <action>:<reg>:<data>:[optional_mask]
        t1 = strtok(buffer, CREG_DELIMITERS);
        t2 = strtok(NULL, CREG_DELIMITERS);
        t3 = strtok(NULL, CREG_DELIMITERS);
        t4 = strtok(NULL, CREG_DELIMITERS);
        t5 = strtok(NULL, CREG_DELIMITERS);

        if (t1 == NULL || t2 == NULL || t3 == NULL || t5 != NULL)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }

        // copy int argument strings, assumes that memory allocated by caller
        strncpy(regstr, t2, CREG_MAX_BUF_SIZE);
        strncpy(datastr, t3, CREG_MAX_BUF_SIZE);

        // check if mask present
        if (t4 != NULL)
            strncpy(maskstr, t4, CREG_MAX_BUF_SIZE);
        else
            maskstr[0] = '\0'; // add end tag

        break;
    case CRegisterControl::EREAD:
        // format shoud be:
        // <action>:<reg>:[optional_mask]
        t1 = strtok(buffer, CREG_DELIMITERS);
        t2 = strtok(NULL, CREG_DELIMITERS);
        t3 = strtok(NULL, CREG_DELIMITERS);
        t4 = strtok(NULL, CREG_DELIMITERS);
        if (t1 == NULL || t2 == NULL || t4 != NULL)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }

        // copy int argument strings, assumes that memory allocated by caller
        strncpy(regstr, t2, CREG_MAX_BUF_SIZE);

        // check if mask present
        if (t3 != NULL)
            strncpy(maskstr, t3, CREG_MAX_BUF_SIZE);
        else
            maskstr[0] = '\0'; // add end tag

        break;
    default:
        ErrPrintf("Invalid register control action: %d\n", action);
        return false;
    }

    Utility::RemoveHeadTailWhitespace(regstr);
    Utility::RemoveHeadTailWhitespace(datastr);
    Utility::RemoveHeadTailWhitespace(maskstr);

    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::ParseArray1Reg
(
    const char* str,
    char* regstr,
    char* idxstr,
    char* datastr,
    char* maskstr,
    CRegisterControl::RCActionType action
)
{
    char buffer[CREG_MAX_STR_SIZE + 1];
    strncpy(buffer, str, CREG_MAX_STR_SIZE);

    char* t1 = NULL;
    char* t2 = NULL;
    char* t3 = NULL;
    char* t4 = NULL;
    char* t5 = NULL;
    char* t6 = NULL;
    switch(action)
    {
    case CRegisterControl::EARRAY1MODIFY:
    case CRegisterControl::EARRAY1WRITE:
        // format should be:
        // <action>:<reg>:<index>:<data>:[optional_mask]
        t1 = strtok(buffer, CREG_DELIMITERS);
        t2 = strtok(NULL, CREG_DELIMITERS);
        t3 = strtok(NULL, CREG_DELIMITERS);
        t4 = strtok(NULL, CREG_DELIMITERS);
        t5 = strtok(NULL, CREG_DELIMITERS);
        t6 = strtok(NULL, CREG_DELIMITERS);

        if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL || t6 != NULL)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }

        // copy int argument strings, assumes that memory allocated by caller
        strncpy(regstr, t2, CREG_MAX_BUF_SIZE);
        strncpy(idxstr, t3, CREG_MAX_BUF_SIZE);
        strncpy(datastr, t4, CREG_MAX_BUF_SIZE);

        // check if mask present
        if (t5 != NULL)
            strncpy(maskstr, t5, CREG_MAX_BUF_SIZE);
        else
            maskstr[0] = '\0'; // add end tag

        break;
    case CRegisterControl::EARRAY1READ:
        // format should be:
        // <action>:<reg>:<index>:[optional_mask]
        t1 = strtok(buffer, CREG_DELIMITERS);
        t2 = strtok(NULL, CREG_DELIMITERS);
        t3 = strtok(NULL, CREG_DELIMITERS);
        t4 = strtok(NULL, CREG_DELIMITERS);
        t5 = strtok(NULL, CREG_DELIMITERS);

        if (t1 == NULL || t2 == NULL || t3 == NULL || t5 != NULL)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }

        // copy int argument strings, assumes that memory allocated by caller
        strncpy(regstr, t2, CREG_MAX_BUF_SIZE);
        strncpy(idxstr, t3, CREG_MAX_BUF_SIZE);

        // check if mask present
        if (t4 != NULL)
            strncpy(maskstr, t4, CREG_MAX_BUF_SIZE);
        else
            maskstr[0] = '\0'; // add end tag

        break;
    default:
        ErrPrintf("Invalid register control action: %d\n", action);
        return false;
    }

    Utility::RemoveHeadTailWhitespace(regstr);
    Utility::RemoveHeadTailWhitespace(idxstr);
    Utility::RemoveHeadTailWhitespace(datastr);
    Utility::RemoveHeadTailWhitespace(maskstr);
    return true;
}

//-----------------------------------------------------------------------
bool CRegCtrlFactory::ParseArray2Reg
(
    const char* str,
    char* regstr,
    char* idx1str,
    char* idx2str,
    char* datastr,
    char* maskstr,
    CRegisterControl::RCActionType action
)
{
    char buffer[CREG_MAX_STR_SIZE + 1];
    strncpy(buffer, str, CREG_MAX_STR_SIZE);

    char* t1 = NULL;
    char* t2 = NULL;
    char* t3 = NULL;
    char* t4 = NULL;
    char* t5 = NULL;
    char* t6 = NULL;
    char* t7 = NULL;
    switch(action)
    {
    case CRegisterControl::EARRAY2MODIFY:
    case CRegisterControl::EARRAY2WRITE:
        // format should be:
        // <action>:<reg>:<idx1>:<idx2>:<data>:[optional_mask]
        t1 = strtok(buffer, CREG_DELIMITERS);
        t2 = strtok(NULL, CREG_DELIMITERS);
        t3 = strtok(NULL, CREG_DELIMITERS);
        t4 = strtok(NULL, CREG_DELIMITERS);
        t5 = strtok(NULL, CREG_DELIMITERS);
        t6 = strtok(NULL, CREG_DELIMITERS);
        t7 = strtok(NULL, CREG_DELIMITERS);

        if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL ||
            t5 == NULL || t7 != NULL)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }

        // copy int argument strings, assumes that memory allocated by caller
        strncpy(regstr, t2, CREG_MAX_BUF_SIZE);
        strncpy(idx1str, t3, CREG_MAX_BUF_SIZE);
        strncpy(idx2str, t4, CREG_MAX_BUF_SIZE);
        strncpy(datastr, t5, CREG_MAX_BUF_SIZE);

        // check if mask present
        if (t6 != NULL)
            strncpy(maskstr, t6, CREG_MAX_BUF_SIZE);
        else
            maskstr[0] = '\0'; // add end tag

        break;
    case CRegisterControl::EARRAY2READ:
        // format should be:
        // <action>:<reg>:<idx1>:<idx2>:[optional_mask]
        t1 = strtok(buffer, CREG_DELIMITERS);
        t2 = strtok(NULL, CREG_DELIMITERS);
        t3 = strtok(NULL, CREG_DELIMITERS);
        t4 = strtok(NULL, CREG_DELIMITERS);
        t5 = strtok(NULL, CREG_DELIMITERS);
        t5 = strtok(NULL, CREG_DELIMITERS);

        if (t1 == NULL || t2 == NULL || t3 == NULL || t4 == NULL || t6 != NULL)
        {
            ErrPrintf("Invalid register control string:%s\n", str);
            return false;
        }

        // copy int argument strings, assumes that memory allocated by caller
        strncpy(regstr, t2, CREG_MAX_BUF_SIZE);
        strncpy(idx1str, t3, CREG_MAX_BUF_SIZE);
        strncpy(idx2str, t4, CREG_MAX_BUF_SIZE);

        // check if mask present
        if (t5 != NULL)
            strncpy(maskstr, t5, CREG_MAX_BUF_SIZE);
        else
            maskstr[0] = '\0'; // add end tag

        break;
    default:
        ErrPrintf("Invalid register control action: %d\n", action);
        return false;
    }

    Utility::RemoveHeadTailWhitespace(regstr);
    Utility::RemoveHeadTailWhitespace(idx1str);
    Utility::RemoveHeadTailWhitespace(idx2str);
    Utility::RemoveHeadTailWhitespace(datastr);
    Utility::RemoveHeadTailWhitespace(maskstr);

    return true;
}
