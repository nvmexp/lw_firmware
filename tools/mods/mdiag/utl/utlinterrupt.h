/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLINTERRUPT_H
#define INCLUDED_UTLINTERRUPT_H

#include "utlpython.h"
#include "mdiag/utl/utlutil.h"

class UtlGpu;

// This class represents the IsrData which contains the function and args from UTL script
//
struct UtlIsrData
{
    void ProcessKwargs(py::kwargs kwargs, const string & sourceFuncName)
    {
        m_Func = UtlUtility::GetRequiredKwarg<py::function>(kwargs, "func", sourceFuncName.c_str());
        if (m_Func.is_none())
        {
            UtlUtility::PyError("func parameter is not a valid Python function");
        }

        UtlUtility::GetOptionalKwarg<py::object>(&m_Args, kwargs, "args", sourceFuncName.c_str());
    }

    py::function m_Func;
    py::object m_Args = py::none();
};

// This class represents the abstract concept of interrupt object.
//
class UtlInterrupt
{
public:
    static void Register(py::module module);

    enum IrqType
    {
        MSIX = MODS_XP_IRQ_TYPE_MSIX & MODS_XP_IRQ_TYPE_MASK,
        MSI = MODS_XP_IRQ_TYPE_MSI & MODS_XP_IRQ_TYPE_MASK,
        INT = MODS_XP_IRQ_TYPE_INT & MODS_XP_IRQ_TYPE_MASK,
        CPU = MODS_XP_IRQ_TYPE_CPU & MODS_XP_IRQ_TYPE_MASK //CheetAh
    };

    UtlInterrupt(UINT32 irq, UINT32 irqType, UtlGpu* gpu);
    virtual ~UtlInterrupt();
    virtual void Hook(UtlIsrData* isrData) = 0;
    virtual void Unhook(UtlIsrData* isrData) = 0;
    static void FreeInterrupts();
    static long InterruptHandler(void* data);

    static UtlInterrupt* GetInterrupt(py::kwargs kwargs);
    void HookPy(py::kwargs kwargs);
    void UnhookPy(py::kwargs kwargs);
    void Enable();
    void Disable();
    bool IsHooked();
    UINT32 GetIrq(); 
    UINT32 GetIrqType();
    UtlGpu* GetGpu() const;

    UtlInterrupt(UtlInterrupt&) = delete;
    UtlInterrupt& operator=(UtlInterrupt&) = delete;

protected:
    UINT32 m_Irq;
    UINT08 m_IrqType;
    UtlGpu* m_Gpu;
    vector<unique_ptr<UtlIsrData>> m_IsrDataList;

    // All interrupt object created in UTL script
    static vector<unique_ptr<UtlInterrupt>> s_UtlInterrupts;
};

// This class represents MSIX or MSI interrupt. Two kinds of interrupts share common code 
// When hooking/unhooking the interrupt now.
class UtlNonIntaInterrupt : public UtlInterrupt
{
public:
    UtlNonIntaInterrupt(UINT32 irq, UINT32 irqType, UtlGpu* gpu);
    virtual void Hook(UtlIsrData* isrData);
    virtual void Unhook(UtlIsrData* isrData);
};

// This class represents INTA interrupt.
//
class UtlIntaInterrupt : public UtlInterrupt
{
public:
    UtlIntaInterrupt(UINT32 irq, UINT32 irqType, UtlGpu* gpu);
    virtual void Hook(UtlIsrData* isrData);
    virtual void Unhook(UtlIsrData* isrData);
};

#endif
