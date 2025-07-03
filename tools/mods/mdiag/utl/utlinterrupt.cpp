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
#include "utlinterrupt.h"
#include "mdiag/utl/utlgpu.h"

vector<unique_ptr<UtlInterrupt> > UtlInterrupt::s_UtlInterrupts;

void UtlInterrupt::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Interrupt.GetInterrupt", "irq", true, "the irq number for target interrupt");
    kwargs->RegisterKwarg("Interrupt.GetInterrupt", "gpu", false, "the GPU on which the interrupt oclwrs. Defaults to GPU 0.");
    kwargs->RegisterKwarg("Interrupt.Hook", "func", true, "interrupt handler function");
    kwargs->RegisterKwarg("Interrupt.Hook", "args", false, "args to be passed to interrupt handler function");
    kwargs->RegisterKwarg("Interrupt.Unhook", "func", true, "interrupt handler function");
    kwargs->RegisterKwarg("Interrupt.Unhook", "args", false, "args to be passed to interrupt handler function");

    py::class_<UtlInterrupt> interrupt(module, "Interrupt");
    interrupt.def_static("GetInterrupt", &UtlInterrupt::GetInterrupt, "This fucntion creates an interrupt object with a specific irq.", py::return_value_policy::reference);
    interrupt.def("Hook", &UtlInterrupt::HookPy, "This function hooks target interrupt.");
    interrupt.def("Unhook", &UtlInterrupt::UnhookPy, "This function unhooks target interrupt.");
    interrupt.def("Enable", &UtlInterrupt::Enable, "This function enables target interrupt.");
    interrupt.def("Disable", &UtlInterrupt::Disable, "This function disables target interrupt.");
    interrupt.def("IsHooked", &UtlInterrupt::IsHooked, "This function returns true if target interrupt is hooked.");
    interrupt.def_property_readonly("irq", &UtlInterrupt::GetIrq);
    interrupt.def_property_readonly("irqType", &UtlInterrupt::GetIrqType);
    interrupt.def_property_readonly("gpu", &UtlInterrupt::GetGpu);

    py::enum_<UtlInterrupt::IrqType>(interrupt, "IrqType", "An enum specifying IrqType.")
    .value("MSIX", MSIX)
    .value("MSI", MSI)
    .value("INT", INT)
    .value("CPU", CPU);
}

UtlInterrupt::UtlInterrupt
(
    UINT32 irq,
    UINT32 irqType,
    UtlGpu* gpu
) :
    m_Irq(irq),
    m_IrqType(irqType),
    m_Gpu(gpu)
{
}

UtlInterrupt::~UtlInterrupt()
{
}

/*static*/ void UtlInterrupt::FreeInterrupts()
{
    s_UtlInterrupts.clear();
}

/*static*/ long UtlInterrupt::InterruptHandler(void* data)
{
    UtlGil::Acquire gil;
    UtlIsrData* isrData = static_cast<UtlIsrData*>(data);
    isrData->m_Func(isrData->m_Args);
    return 1;  //equal to Xp::ISR_RESULT_SERVICED
}

/*static*/ UtlInterrupt* UtlInterrupt::GetInterrupt(py::kwargs kwargs)
{
    UINT32 irq = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "irq", "Interrupt.GetInterrupt");

    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "Interrupt.GetInterrupt"))
    {
        gpu = UtlGpu::GetGpus()[0];
    }
    
    UINT08 irqType = MDiagUtils::GetIRQType();

    for (auto & interrupt: s_UtlInterrupts)
    {
        if (interrupt->GetIrq() == irq &&
            interrupt->GetIrqType() == irqType &&
            interrupt->GetGpu() == gpu)
        {
            return interrupt.get();
        }
    }

    switch(irqType)
    {
        case MSIX: 
        case MSI:
            s_UtlInterrupts.emplace_back(make_unique<UtlNonIntaInterrupt>(irq, irqType, gpu));
            break;
        case INT:
            s_UtlInterrupts.emplace_back(make_unique<UtlIntaInterrupt>(irq, irqType, gpu));
            break;
        case CPU:
        default:
            MASSERT("The interrupt type is not supported!");
    }
    return s_UtlInterrupts.back().get();
}

void UtlInterrupt::HookPy(py::kwargs kwargs)
{
    unique_ptr<UtlIsrData> isrData = make_unique<UtlIsrData>();
    isrData->ProcessKwargs(kwargs, "Interrupt.Hook");
    for (auto & isrDataItem : m_IsrDataList)
    {
        if (isrDataItem->m_Func == isrData->m_Func &&
            isrDataItem->m_Args == isrData->m_Args)
        {
            UtlUtility::PyError("Cannot hook duplicated ISR.");
        }
    }   
    Hook(isrData.get());
    m_IsrDataList.emplace_back(move(isrData));
}

void UtlInterrupt::UnhookPy(py::kwargs kwargs)
{
    unique_ptr<UtlIsrData> isrData = make_unique<UtlIsrData>();
    isrData->ProcessKwargs(kwargs, "Interrupt.Unhook");
    for (auto isrDataItem = m_IsrDataList.begin(); isrDataItem != m_IsrDataList.end(); ++isrDataItem)
    {
        if ((*isrDataItem)->m_Func == isrData->m_Func &&
            (*isrDataItem)->m_Args == isrData->m_Args)
        {
            Unhook((*isrDataItem).get());
            m_IsrDataList.erase(isrDataItem);
            return;
        }
    }
    UtlUtility::PyError("Cannot unhook the ISR.");
}

void UtlInterrupt::Enable()
{
    Platform::EnableIRQ(m_Irq);
}

void UtlInterrupt::Disable()
{
    Platform::DisableIRQ(m_Irq);
}

bool UtlInterrupt::IsHooked()
{
    return Platform::IsIRQHooked(m_Irq);
}

UINT32 UtlInterrupt::GetIrq()
{
    return m_Irq;
}

UINT32 UtlInterrupt::GetIrqType()
{
    return m_IrqType;
}

UtlGpu* UtlInterrupt::GetGpu() const
{
    return m_Gpu;
}

UtlNonIntaInterrupt::UtlNonIntaInterrupt
(
    UINT32 irq,
    UINT32 irqType,
    UtlGpu* gpu
) :
    UtlInterrupt(irq, irqType, gpu)
{
}

void UtlNonIntaInterrupt::Hook(UtlIsrData* isrData)
{
    IrqParams irqInfo;
    GpuSubdevice* pSubDev = m_Gpu->GetGpuSubdevice();
    pSubDev->LoadIrqInfo(&irqInfo, m_IrqType, 0, 0);
    irqInfo.irqNumber = m_Irq;

    RC rc = Platform::HookInt(irqInfo, static_cast<ISR>(InterruptHandler), isrData);
    UtlUtility::PyErrorRC(rc, "Fail to hook %s interrupt, whose irq is %d.",
        m_IrqType == MSIX ? "MSIX" : "MSI", m_Irq);
}

void UtlNonIntaInterrupt::Unhook(UtlIsrData* isrData)
{
    IrqParams irqInfo;
    GpuSubdevice* pSubDev = m_Gpu->GetGpuSubdevice();
    pSubDev->LoadIrqInfo(&irqInfo, m_IrqType, 0, 0);
    irqInfo.irqNumber = m_Irq;

    RC rc = Platform::UnhookInt(irqInfo, static_cast<ISR>(InterruptHandler), isrData);
    UtlUtility::PyErrorRC(rc, "Fail to unhook %s interrupt, whose irq is %d.",
        m_IrqType == MSIX ? "MSIX" : "MSI", m_Irq);
}

UtlIntaInterrupt::UtlIntaInterrupt
(
    UINT32 irq,
    UINT32 irqType,
    UtlGpu* gpu
) :
    UtlInterrupt(irq, irqType, gpu)
{
}

void UtlIntaInterrupt::Hook(UtlIsrData* isrData)
{
    RC rc = Platform::HookIRQ(m_Irq, static_cast<ISR>(InterruptHandler), isrData);
    UtlUtility::PyErrorRC(rc, "Fail to hook INTA interrupt, whose irq is %d.", m_Irq);
}

void UtlIntaInterrupt::Unhook(UtlIsrData* isrData)
{
    RC rc = Platform::UnhookIRQ(m_Irq, static_cast<ISR>(InterruptHandler), isrData);
    UtlUtility::PyErrorRC(rc, "Fail to unhook INTA interrupt, whose irq is %d.", m_Irq);
}
