/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2008, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CREGISTERCONTROL_H
#define INCLUDED_CREGISTERCONTROL_H

#include "types.h"
#include "gpu/reghal/reghaltable.h"
#include <vector>

// fwd declare
class LWGpuResource;
class IRegisterClass;
class IRegisterField;
class CRegisterControl;

// IRegisterClass container class and vector typedef
typedef struct IRegRef_t
{
    IRegRef_t() {};
    ~IRegRef_t() {};
    IRegRef_t(IRegRef_t&& t) : 
        m_reg(move(t.m_reg)),
        m_address(t.m_address) 
    {}

    unique_ptr<IRegisterClass> m_reg;
    UINT32 m_address;
} CRegRef_t;
typedef vector<IRegRef_t> IRegRefVec_t;

// container class for IRegRefVec_t objects
class CRegRef
{
public:
    CRegRef();
    ~CRegRef();

    void AddRegRef(unique_ptr<IRegisterClass> reg, UINT32 address);
    IRegRefVec_t::iterator Begin();
    IRegRefVec_t::iterator End();

private:
    IRegRefVec_t m_regrefvec;
};

// register control class and vector typedef
class CRegisterControl
{
public:
    enum RCActionType
    {
        EUNKNOWN,
        EWRITE,
        EMODIFY,
        EREAD,
        ECOMPARE,
        EARRAY1WRITE,
        EARRAY1MODIFY,
        EARRAY1READ,
        EARRAY2WRITE,
        EARRAY2MODIFY,
        EARRAY2READ
    };

    CRegisterControl();
    void PrintRegInfo() const;
    bool operator==(const CRegisterControl& rhs)
    {
        return  m_action == rhs.m_action &&
                m_address == rhs.m_address &&
                m_mask == rhs.m_mask &&
                m_data == rhs.m_data &&
                m_rdiSelect == rhs.m_rdiSelect &&
                m_rdiAddress == rhs.m_rdiAddress &&
                m_arrayIdx1 == rhs.m_arrayIdx1 &&
                m_arrayIdx2 == rhs.m_arrayIdx2 &&
                m_arrayIdxNum == rhs.m_arrayIdxNum &&
                m_ctxIdx == rhs.m_ctxIdx &&
                m_domain == rhs.m_domain &&
                m_regSpace == rhs.m_regSpace;
    }

    RCActionType        m_action;
    UINT32              m_address;
    UINT32              m_mask;
    UINT32              m_data;
    UINT32              m_rdiSelect;
    UINT32              m_rdiAddress;
    UINT32              m_dataAddress;
    UINT32              m_arrayIdx1;
    UINT32              m_arrayIdx2;
    UINT32              m_arrayIdxNum;
    UINT32              m_ctxIdx;
    IRegisterClass*     m_regPtr;
    RegHalDomain        m_domain;
    string              m_regSpace;
    bool                m_isCtxReg;
    bool                m_isConfigSpace = false;

private:
    string GetFieldAndValueName(IRegisterClass* reg) const;
    string GetValuename(IRegisterField* field) const;
};
typedef vector<CRegisterControl> RegCtrlVec_t;

// register control object factory class
class CRegCtrlFactory
{
public:
    /// c'tor
    CRegCtrlFactory(LWGpuResource* cb);

    /// d'tor
    ~CRegCtrlFactory();

    /// parses an input line, inserts CRegisterControl objs
    /// into the regvec arg array
    /// \param regvec regcrtl objects
    /// \param str reg ctrl line
    /// \return true if line was successfully parsed
    bool ParseLine(RegCtrlVec_t& regvec, const char* str);
    
    static vector<unique_ptr<IRegisterClass>> s_Registers;

private:
    /// parses a reg ctrl file
    /// \param regvec regcrtl objects
    /// \param filename file to be parsed
    /// \return true if line was successfully parsed
    bool ParseRegFile(RegCtrlVec_t& regvec, const char* filename);

    bool IsFloorSweptReg(const CRegisterControl& regc);

    ///
    bool CreateSingleReg
    (
        RegCtrlVec_t& regvec,
        const char* regstr,
        const char* datastr,
        const char* maskstr,
        CRegisterControl::RCActionType action
    );

    ///
    bool CreateArray1Reg
    (
        RegCtrlVec_t& regvec,
        const char* regstr,
        const char* idxstr,
        const char* datastr,
        const char* maskstr,
        CRegisterControl::RCActionType action
    );

    ///
    bool CreateArray2Reg
    (
        RegCtrlVec_t& regvec,
        const char* regstr,
        const char* idx1str,
        const char* idx2str,
        const char* datastr,
        const char* maskstr,
        CRegisterControl::RCActionType action
    );

    ///
    bool IsNumerical(const char* str);

    ///
    bool HasStar(const char* str);

    ///
    bool MatchRegisters(const char* regstr, CRegRef& regvec);

    ///
    bool MatchRegFields(const char* datastr,
                        const char* maskstr,
                        IRegisterClass* const reg,
                        UINT32* data, UINT32* mask,
                        CRegisterControl::RCActionType action
                       );
    ///
    UINT32 ParseFromStr(const char* str);

    ///
    bool ParseSingleReg
    (
        const char* str,
        char* regstr,
        char* datastr,
        char* maskstr,
        CRegisterControl::RCActionType action
    );

    ///
    bool ParseArray1Reg
    (
        const char* str,
        char* regstr,
        char* idxstr,
        char* datastr,
        char* maskstr,
        CRegisterControl::RCActionType action
    );

    ///
    bool ParseArray2Reg
    (
        const char* str,
        char* regstr,
        char* idx1str,
        char* idx2str,
        char* datastr,
        char* maskstr,
        CRegisterControl::RCActionType action
    );

private:
    LWGpuResource* m_cb;
};

#endif // INCLUDED_CREGISTERCONTROL_H
