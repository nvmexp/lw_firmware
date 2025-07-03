/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MMESIM_H
#define INCLUDED_MMESIM_H

#ifndef INCLUDED_GPUSBDEV_H
#include "gpu/include/gpusbdev.h"
#endif

#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

#ifndef INCLUDED_MMEIGEN_H
#include "mmeigen.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

//--------------------------------------------------------------------
//! \brief Provides an interface simulating MME shadow RAM accesses
//!
class MMEShadowSimulator
{
public:
    MMEShadowSimulator(const GpuSubdevice::MmeMethods mmeMethods);
    ~MMEShadowSimulator();

    void SetValue(const GpuSubdevice::MmeMethod method, const UINT32 Data);
    UINT32 GetValue(const GpuSubdevice::MmeMethod method);
    bool IsValueWritten(const GpuSubdevice::MmeMethod method);
    UINT32 GetScratchSize() { return 0x80; }
    INT32 SetRollbackPoint();
    RC    Rollback(INT32 RollbackToken);
    RC    ForgetRollbackPoint(INT32 RollbackToken);
    enum { ROLLBACK_NONE = -1 };
private:
    //! This structure describes the current state of an entry in MME
    //! Shadow RAM
    struct ShadowEntry
    {
        bool   bWritten;  //!< true if the shadowed value has been written
        UINT32 Value;     //!< The value of the shadowed data
    };

    //! Map of non-shared shadowed methods to their respective data.  The
    //! values for the methods here are different for each class even if
    //! the method is the same
    map<GpuSubdevice::MmeMethod, ShadowEntry> m_MethodData;

    //! Map of shared shadowed methods.  The values for shared shadow
    //! methods are the same for every class.  This map contains the
    //! index in the shared data array for the method being shadowed
    map<GpuSubdevice::MmeMethod, UINT32> m_SharedMethodIndexes;

    //! Data for the shared methods
    vector<ShadowEntry> m_SharedData;

    //! Data for rolling back the shadow RAM
    vector < map<GpuSubdevice::MmeMethod, ShadowEntry> > m_RollbackData;
    vector < map<UINT32, ShadowEntry> > m_RollbackSharedData;
    set<INT32> m_ValidRollbackTokens;
};

//--------------------------------------------------------------------
//! \brief Provides a simulator for the MME.  This simulator was adapted
//!        from <branch>/drivers/common/lwmme/playback/mme.cpp with
//!        adjustments to aclwrately track the state of hardware shadow
//!        RAM
//!
//! NOTE : This simulates Shadow RAM in Track mode only, not Passthrough,
//! Track and Filter, or Playback modes
//!
class MMESimulator
{
public:
    MMESimulator(MmeInstGenerator * pMmeIGen, UINT32 version);
    ~MMESimulator() { }

    RC RunProgram(const UINT32 subCh,
                  const UINT32 subChClass,
                  const vector<UINT32> &Instructions,
                  const vector<UINT32> &InputData,
                  MMEShadowSimulator * const pStateInput,
                  const UINT32 BasePC,
                  vector<UINT32> *pOutputData);
    UINT32 GetReleasedMethodValue(UINT32 SubCh,
                                  UINT32 Method,
                                  UINT32 ProgramCounter);

    void   SetMethodShift(UINT32 shift) { m_MethodShift = shift; }

    void   SetTracePriority(Tee::Priority pri) { m_TracePri = pri; }
    Tee::Priority GetTracePriority() const { return m_TracePri; }

    string GetError() const {return m_Error;}
    string GetWarningCategory() const {return m_WarningCategory;}
    string GetWarningMessage() const {return m_WarningMessage;}

    //--------------------------------------------------------------------
    //! \brief RAII class for changing the trace priority of the simulator
    //!
    class ChangeTracePriority
    {
    public:
        ChangeTracePriority(MMESimulator *pMmeSim, Tee::Priority pri) :
            m_pMmeSim(pMmeSim)
        {
            MASSERT(m_pMmeSim);
            m_OldPri = m_pMmeSim->GetTracePriority();
            m_pMmeSim->SetTracePriority(pri);
        }
        ~ChangeTracePriority()
        {
            m_pMmeSim->SetTracePriority(m_OldPri);
        }
    private:
        MMESimulator  *m_pMmeSim;  //!< Pointer to the simulator
        Tee::Priority  m_OldPri;   //!< Old trace level
    };

private:
    //! This structure describes the state of a MME register
    struct MMERegister
    {
        bool   bWritten;   //!< True if the register has been written
        INT32  CycleReady; //!< The cycle that the register will be ready
        UINT32 Value;      //!< The current value for the register
    };
    enum
    {
        END_PC = -1,  //!< PC is at the end
        SKIP_PC = -2  //!< PC is in "skip" mode...i.e. a branch was taken
    };

    //! MME Instruction Generator to use to determine information about
    //! instructions
    MmeInstGenerator * m_pMmeIGen;

    //! The current shadow RAM state
    MMEShadowSimulator * m_pStateInput;

    Tee::Priority m_TracePri;   //!< Trace output is printed at this priority
    INT32         m_Cycles;     //!< Number of cycles that macro exelwtion took
    UINT32        m_Method;     //!< Next method that will be released
    UINT32        m_MethodInc;  //!< Method increment that will be applied
                                //!< after the next release
    UINT32        m_Carry;      //!< Carry value of arithmetic operations
    UINT32        m_LwrData;    //!< Current index into the input data
    UINT32        m_SubCh;      //!< Subchannel where Macro will be run
    UINT32        m_Class;      //!< Class of subchannel for macro

    //! Method values used when accessing shadow RAM are 12-bit method values
    //! (i.e. they increment by 1) the method values used in header files are
    //! the full 14-bit values (i.e. they increment by 4) this shift is used
    //! to translate between what the MME shadow indexing requires and the
    //! values from header files.
    UINT32        m_MethodShift;

    //! The MME register file
    vector<MMERegister> m_RegFile;

    string m_Error;           //!< Error string if an error oclwrs during sim
    string m_WarningCategory; //!< Warning category if a warning oclwrs
    string m_WarningMessage;  //!< Warning message if a warning oclwrs
    bool   m_bIsError;        //!< true if an error oclwrred
    UINT32 m_Version;

    UINT32 Reg(UINT32 RegNum, INT32 Latency);
    UINT32 Reg0(UINT32 Inst, INT32 Latency = 0);
    UINT32 Reg1(UINT32 Inst, INT32 Latency = 0);
    UINT32 Insert(UINT32 Base, UINT32 Src1,
                  UINT32 SourceBit, UINT32 DestBit,
                  UINT32 Width);
    UINT32 Add(UINT32 a, UINT32 b, bool bUseCarry = true);
    UINT32 Load(const vector<UINT32> &Data);
    UINT32 GetState(UINT32 Value);
    void Release(vector<UINT32> *pOutputData,
                 UINT32 ProgramCounter,
                 UINT32 Method,
                 UINT32 Data);

    void SetError(string ErrorString);
    void SetWarning(string Category, string Message);
    void PrintMux(MmeInstGenerator::MuxOp muxOp);
    RC HandleBinaryOp(UINT32 Inst, UINT32 *pResult);
};

#endif
