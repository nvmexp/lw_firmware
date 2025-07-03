/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlutil.h"

#include "core/include/chiplibtracecapture.h"
#include "core/include/tasker.h"
#include "device/interface/pcie.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/crc.h"
#include "utl.h"
#include "utlgpu.h"
#include "utlvftest.h"

#include <boost/algorithm/string.hpp>
#include <boost/lambda/lambda.hpp>

Tasker::ThreadID UtlGil::s_ThreadWithGil = Tasker::NULL_THREAD;

void UtlSequence::Register(py::module module)
{
    py::class_<UtlSequence> seq(module, "Sequence",
        "This class represents a seqeunce of bytes which can be manipulated via [] operators.");

    seq.def("__getitem__", [](const UtlSequence &s, size_t i) {
            if (i >= s.m_Size) throw py::index_error();
            return s.m_Data[i];
        });
    seq.def("__setitem__", [](UtlSequence &s, size_t i, UINT32 v) {
            if (i >= s.m_Size) throw py::index_error();
            s.m_Data[i] = v;
        });
    seq.def("__len__", [](UtlSequence &s) {
        return s.m_Size;
        });
}

UtlSequence::UtlSequence(void* data, size_t size)
{
    m_Size = size;
    m_Data = static_cast<UINT08*> (data);
}

UtlGil::Acquire::Acquire()
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();

    // It is legal for a thread to instantiate multiple conselwtive Acquire
    // objects without any Release objects in between.  This is necessary
    // because a C++ function might need the GIL in order to call into Python,
    // but that function might not know if it's caller has already acquired the
    // GIL (e.g., UtlUserTest::Setup).
    //
    // If a thread does instantiate multiple conselwtive Acquire objects with
    // no Release objects in between, only the first Acquire object actually
    // matters, as the GIL will already be acquired when the subsequent Acquire
    // constructors are called, and the GIL will only be released when the
    // destructor of the first Acquire object is called.  So the
    // gil_scoped_acquire object only needs to be allocated in the first
    // conselwtive Acquire object, which will be the one that happens when
    // no thread lwrrently owns the GIL.

    if (s_ThreadWithGil == Tasker::NULL_THREAD)
    {
        s_ThreadWithGil = threadId;
        m_Gil = make_unique<py::gil_scoped_acquire>();
    }
    else if (threadId != s_ThreadWithGil)
    {
        ErrPrintf("Thread %d attempted to acquire the GIL, but thread %d still owns it.\n",
            threadId, s_ThreadWithGil);
        MASSERT(!"UTL GIL error - asserting to prevent potential hang.");
    }
}

UtlGil::Acquire::~Acquire()
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();
    if (threadId != s_ThreadWithGil)
    {
        ErrPrintf("Thread %d does not own the GIL at the end of acquire (owned by %d).\n",
            threadId, s_ThreadWithGil);
        MASSERT(!"UTL GIL error - asserting to prevent potential hang.");
    }

    if (m_Gil.get() != nullptr)
    {
        s_ThreadWithGil = Tasker::NULL_THREAD;
    }
}

UtlGil::Release::Release()
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();

    // The GIL should only be released if this thread has already acquired it.
    // Also, it is not legal to have multiple conselwtive gil_scoped_release
    // objects, unlike the gil_scoped_acquire object.
    if (threadId != s_ThreadWithGil)
    {
        ErrPrintf("Thread %d attempted to release the GIL, but thread %d still owns it.\n",
            threadId, s_ThreadWithGil);
        MASSERT(!"UTL GIL error - asserting to prevent potential hang.");
    }

    s_ThreadWithGil = Tasker::NULL_THREAD;
    m_Gil = make_unique<py::gil_scoped_release>();
}

UtlGil::Release::~Release()
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();

    // If any thread (including this one) acquires the GIL while the current
    // thread has temporarily released it, that thread needs to release the GIL
    // before this object goes out of scope.
    if (s_ThreadWithGil != Tasker::NULL_THREAD)
    {
        ErrPrintf("Thread %d attempted to re-acquire the GIL but thread %d still owns it.\n",
            threadId, s_ThreadWithGil);
        MASSERT(!"UTL GIL error - asserting to prevent potential hang.");
    }

    s_ThreadWithGil = threadId;
}

UtlGil::NoYield::NoYield()
{
    // This flag is set to prevent yielding the thread. In case, the function that uses this
    // object accidentally yields the thread, it will be caught by Tasker::Yield() and will
    // result in a failed assertion.
    Tasker::SetCanThreadYield(false);
}

UtlGil::NoYield::~NoYield()
{
    Tasker::SetCanThreadYield(true);
}

void UtlUtility::Register(py::module module)
{
    UtlSequence::Register(module);

    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("utl.LogInfo", "text", true, "a string representing the message to be printed");
    kwargs->RegisterKwarg("utl.LogError", "text", true, "a string representing the message to be printed");
    kwargs->RegisterKwarg("utl.LogError", "abort", false, "set to True to abort exelwtion; default false");
    kwargs->RegisterKwarg("utl.LogDebug", "text", true, "a string representing the message to be printed");
    kwargs->RegisterKwarg("utl.EscapeRead", "path", true, "path string to read from");
    kwargs->RegisterKwarg("utl.EscapeRead", "index", true, "index to read from");
    kwargs->RegisterKwarg("utl.EscapeRead", "size", true, "size of data (in bytes) to read which is up to 4");
    kwargs->RegisterKwarg("utl.EscapeRead", "gpu", false, "target gpu");
    kwargs->RegisterKwarg("utl.EscapeWrite", "path", true, "path string to write");
    kwargs->RegisterKwarg("utl.EscapeWrite", "index", true, "index to write");
    kwargs->RegisterKwarg("utl.EscapeWrite", "size", true, "size of data (in bytes) to write which is up to 4");
    kwargs->RegisterKwarg("utl.EscapeWrite", "value", true, "value to write");
    kwargs->RegisterKwarg("utl.EscapeWrite", "gpu", false, "target gpu");
    kwargs->RegisterKwarg("utl.EscapeReadBuffer", "path", true, "path string to read from");
    kwargs->RegisterKwarg("utl.EscapeReadBuffer", "index", true, "index to read from");
    kwargs->RegisterKwarg("utl.EscapeReadBuffer", "size", true, "size of data (in bytes) to read");
    kwargs->RegisterKwarg("utl.EscapeReadBuffer", "gpu", true, "target gpu");
    kwargs->RegisterKwarg("utl.EscapeWriteBuffer", "path", true, "path string to write");
    kwargs->RegisterKwarg("utl.EscapeWriteBuffer", "index", true, "index to write");
    kwargs->RegisterKwarg("utl.EscapeWriteBuffer", "size", true, "size of data to write");
    kwargs->RegisterKwarg("utl.EscapeWriteBuffer", "buffer", true, "values to write");
    kwargs->RegisterKwarg("utl.EscapeWriteBuffer", "gpu", true, "target gpu");
    kwargs->RegisterKwarg("utl.PciCfgRead", "address", true, "source address");
    kwargs->RegisterKwarg("utl.PciCfgRead", "size", false, "size of data (in bytes) to read; must be one of: 1,2,4 (default 4)");
    kwargs->RegisterKwarg("utl.PciCfgRead", "gpu", true, "target gpu");
    kwargs->RegisterKwarg("utl.PciCfgWrite", "address", true, "destination address");
    kwargs->RegisterKwarg("utl.PciCfgWrite", "data", true, "data to write");
    kwargs->RegisterKwarg("utl.PciCfgWrite", "gpu", true, "target gpu");
    kwargs->RegisterKwarg("utl.PciCfgWrite", "size", false, "size of data (in bytes) to write; must be one of: 1,2,4 (default 4)");
    kwargs->RegisterKwarg("utl.PciIoRead", "port", true, "source port");
    kwargs->RegisterKwarg("utl.PciIoRead", "size", false, "size of data (in bytes) to read; must be one of: 1,2,4 (default 4)");
    kwargs->RegisterKwarg("utl.PciIoWrite", "port", true, "destination port");
    kwargs->RegisterKwarg("utl.PciIoWrite", "data", true, "data to write");
    kwargs->RegisterKwarg("utl.PciIoWrite", "size", false, "size of data (in bytes) to write; must be one of: 1,2,4 (default 4)");
    kwargs->RegisterKwarg("utl.FcraLogging", "prefix", true, "a string representing visualization hierarchy");
    kwargs->RegisterKwarg("utl.FcraLogging", "text", true, "a string representing the messages for FCRA");
    kwargs->RegisterKwarg("utl.SetDumpPhysicalAccess", "enable", true, "a boolean representing whether SW accesses should be dumped or not.");
    kwargs->RegisterKwarg("utl.ClockSimulator", "cycles", true, "number of clock simulator cycles");
    kwargs->RegisterKwarg("utl.DirectPhysicalWrite", "address", true, "physical address to be written");
    kwargs->RegisterKwarg("utl.DirectPhysicalWrite", "data", true, "list of data bytes to be written");
    kwargs->RegisterKwarg("utl.DirectPhysicalRead", "address", true, "physical address to be read");
    kwargs->RegisterKwarg("utl.DirectPhysicalRead", "size", true, "number of bytes to be read");
    kwargs->RegisterKwarg("utl.CallwlateBufferCrc", "data", true, "an array of bytes for which a CRC value will be callwlated");
    kwargs->RegisterKwarg("utl.TriggerUserEvent", "data", true, "a dictionary of strings to be passed to the UserEvent callback function");
    
    module.def("LogInfo", &LogInfo,
        UTL_DOCSTRING("utl.LogInfo", "This function prints an info message to mods.log."));
    module.def("LogError", &LogError,
        UTL_DOCSTRING("utl.LogError", "This function prints an error message to mods.log.  This function will not fail the test or halt MODS exelwtion unless the optional 'abort' keyword argument is set to True."));
    module.def("LogDebug", &LogDebug,
        UTL_DOCSTRING("utl.LogDebug", "This function prints a debug message to mods.log.  The message will only be printed if MODS is run with the -d argument."));
    module.def("GetTimeUs", &GetTimeUs,
        UTL_DOCSTRING("GetTimeUs", "This function returns the current Simulator time in usecs."));
    module.def("GetLwrrentSriovFunction", &GetLwrrentSriovFunction,
        UTL_DOCSTRING("GetLwrrentSriovFunction", "This function prints whether it is under PF or VF function."),
        py::return_value_policy::take_ownership);
    module.def("ListAllParams", &ListAllParams,
        UTL_DOCSTRING("ListAllParams", "This function returns parameters passed to mdiag."),
        py::return_value_policy::reference);
    module.def("GetScriptArgs", &GetScriptArgs,
        UTL_DOCSTRING("GetScriptArgs", "This function returns a string containing all of the arguments passed to the current script."),
        py::return_value_policy::reference);
    module.def("GetSimulationMode", &GetSimulationMode,
        UTL_DOCSTRING("GetSimulationMode", "This function return the current simulation mode."));
    module.def("EscapeRead", &EscapeRead,
        UTL_DOCSTRING("utl.EscapeRead", "This function return the excapeRead value."));
    module.def("EscapeReadBuffer", &EscapeReadBuffer,
        UTL_DOCSTRING("utl.EscapeReadBuffer", "This function returns the escapeRead buffer."));
    module.def("EscapeWrite", &EscapeWrite,
        UTL_DOCSTRING("utl.EscapeWrite", "This function write a value to escapeWrite path string."));
    module.def("EscapeWriteBuffer", &EscapeWriteBuffer,
        UTL_DOCSTRING("utl.EscapeWriteBuffer", "This function write a file to escapeWrite path string."));
    module.def("PciCfgRead", &PciCfgRead,
        UTL_DOCSTRING("utl.PciCfgRead", "This function reads 1, 2, or 4 bytes from a PCI Cfg address"));
    module.def("PciCfgWrite", &PciCfgWrite,
        UTL_DOCSTRING("utl.PciCfgWrite", "This function writes 1, 2, or 4 bytes to a PCI Cfg address"));
    module.def("PciIoRead", &PciIoRead,
        UTL_DOCSTRING("utl.PciIoRead", "This function reads 1, 2, or 4 bytes from a PCI IO port"));
    module.def("PciIoWrite", &PciIoWrite,
        UTL_DOCSTRING("utl.PciIoWrite", "This function writes 1, 2, or 4 bytes to a PCI IO port"));
    module.def("CheckScriptsPresent" , &UtlUtility::IsCheckUtlScriptsUtil, 
        UTL_DOCSTRING("utl.IsCheckUtlScripts", "This function exposes the presence of -check_utl_scripts argument."));
    module.def("EnableInterrupts", &UtlUtility::EnableInterrupts,
        UTL_DOCSTRING("utl.EnableInterrupts", "This function enables external hardware interrupts."));
    module.def("DisableInterrupts", &UtlUtility::DisableInterrupts,
        UTL_DOCSTRING("utl.DisableInterrupts", "This function disables external hardware interrupts."));
    module.def("IsVirtualFunction", &UtlUtility::IsVirtualFunction,
        UTL_DOCSTRING("utl.IsVirtualFunction", "This function returns if the current mods is in VF mode."));
    module.def("FlushCpuWriteCombineBuffer", &UtlUtility::FlushCpuWriteCombineBuffer,
        UTL_DOCSTRING("utl.FlushCpuWriteCombineBuffer", "Flush the CPU's write combine buffer."));
    module.def("ClockSimulator", &ClockSimulator,
        UTL_DOCSTRING("utl.ClockSimulator", "This function clocks the simulator for specified number of cycles. ClockSimulator function can not be used with emulation or silicon"));
    module.def("DirectPhysicalWrite", &DirectPhysicalWritePy,
        UTL_DOCSTRING("utl.DirectPhysicalWrite", "This function writes data to a physical address. It is to be used for special cases only- like ROM BAR, non-GPU CheetAh registers."));
    module.def("DirectPhysicalRead", &DirectPhysicalReadPy,
        UTL_DOCSTRING("utl.DirectPhysicalRead", "This function reads data from a physical address. It is to be used for special cases only- like ROM BAR, non-GPU CheetAh registers."));
    module.def("CallwlateBufferCrc", &CallwlateBufferCrc,
        UTL_DOCSTRING("utl.CallwlateBufferCrc", "This function callwlates a CRC value based on the specified array of bytes.  The CRC callwlation algorithm used is the same one MODS uses to CRC check surfaces at the end of a test."));

    py::enum_<SimulationMode>(module, "SimulatorMode")
        .value("HARDWARE", SimulationMode::HARDWARE)
        .value("RTL", SimulationMode::RTL)
        .value("FMODEL", SimulationMode::FMODEL)
        .value("AMODEL", SimulationMode::AMODEL);

    module.def("FcraLogging", &FcraLoggingPy,
        UTL_DOCSTRING("utl.FcraLoggingPy", "This function prints an info message to fcra.log (aka. vector_raw.log for now)."));

    module.def("SetDumpPhysicalAccess", &UtlUtility::SetDumpPhysicalAccess,
        UTL_DOCSTRING("utl.SetDumpPhysicalAccess", "This function sets the flag to determine whether all SW accesses should be dumped or not."));
    
    module.def("TriggerUserEvent", &UtlUtility::TriggerUserEventPy,
        UTL_DOCSTRING("utl.TriggerUserEvent", "This function enable user to create lwstomized utl user event at the utl script."));
    module.def("IsPolicyFileLaunchingVFs", &UtlUtility::IsPolicyFileLaunchingVFs,
        UTL_DOCSTRING("utl.IsPolicyFileLaunchingVFs", "This function returns true if VFs are started by policy manager."));
}

// This function should be called whenever a py::error_already_set exception
// is caught.
//
void UtlUtility::HandlePythonException(const char* errorText)
{
    DebugPrintf("UTL: HandlePythonException on thread id = %d\n", Tasker::LwrrentThread());

    // The first exception handled by this function is guaranteed to be a
    // genuine error, but all exceptions after the first are likely to be
    // exceptions that are thrown simply to abort a different thread.
    // Unfortunately there is no way to determine if the later exceptions are
    // real errors that should be reported to the user, because all exceptions
    // that come from Python are colwerted by pybind11 into
    // py::error_already_set exceptions, whether they originated in C++
    // extension code or in the Python interpreter.  Therefore, only print
    // an error for the first exception and ignore the rest.
    if (!Utl::Instance()->IsAborting())
    {
        Utl::Instance()->Abort();
        string errorString(errorText);
        vector<string> errorLines;

        // The error string might have multiple lines separated by
        // carriage returns, so split them into a list and
        // print each one individually.

        boost::split(errorLines, errorString, boost::lambda::_1 == '\n');

        for (const auto& line : errorLines)
        {
            // Only print non-blank lines.
            if (line.length() > 0)
            {
                ErrPrintf("%s\n", line.c_str());
            }
        }
    }
}

void UtlUtility::RequirePhase
(
    Utl::UtlPhase phaseFlags,
    const char* pythonFunc
)
{
    if ((Utl::Instance()->Phase() & phaseFlags) == 0)
    {
        switch (Utl::Instance()->Phase())
        {
            case Utl::ImportScriptsPhase:
                UtlUtility::PyError(
                    "can't call %s while UTL scripts are being imported.",
                    pythonFunc);
                break;

            case Utl::InitEventPhase:
                UtlUtility::PyError("can't call %s during the InitEvent.",
                    pythonFunc);
                break;

            case Utl::RunPhase:
                UtlUtility::PyError("can't call %s while tests are running.",
                    pythonFunc);
                break;

            case Utl::EndEventPhase:
                UtlUtility::PyError("can't call %s during the EndEvent.",
                    pythonFunc);
                break;

            default:
                UtlUtility::PyError("can't call %s at this time.",
                    pythonFunc);
                break;
        }
    }
}

// This function can be called from a UTL script to print an info message.
//
void UtlUtility::LogInfo(py::kwargs kwargs)
{
    string text = UtlUtility::GetRequiredKwarg<string>(kwargs, "text",
        "utl.LogInfo");

    InfoPrintf("%s\n", text.c_str());
}

// This function can be called from a UTL script to print an error message.
// The user may optionally specify that UTL should abort exelwtion.
//
void UtlUtility::LogError(py::kwargs kwargs)
{
    string text = UtlUtility::GetRequiredKwarg<string>(kwargs, "text",
        "utl.LogError");

    bool abort = false;
    if (UtlUtility::GetOptionalKwarg<bool>(&abort, kwargs, "abort", "utl.LogError") &&
        abort)
    {
        UtlUtility::PyError(text.c_str());
    }
    else
    {
        ErrPrintf("%s\n", text.c_str());
    }
}

// This function can be called from a UTL script to print a debug message.
//
void UtlUtility::LogDebug(py::kwargs kwargs)
{
    string text = UtlUtility::GetRequiredKwarg<string>(kwargs, "text",
        "utl.LogDebug");

    DebugPrintf("%s\n", text.c_str());
}

UtlLwrrentSriovFunction * UtlUtility::GetLwrrentSriovFunction()
{
    if (Platform::IsPhysFunMode())
        return new UtlPFLwrrentFunction();

    if (Platform::IsVirtFunMode())
        return new UtlVFLwrrentFunction();

    return nullptr;
}

char * UtlUtility::ListAllParams()
{
    Utl * pUtl = Utl::Instance();
    return pUtl->ListAllParams();
}

// This function can be called from a UTL script to get any user arguments
// that were passed to that script.
//
vector<string>* UtlUtility::GetScriptArgs()
{
    return Utl::Instance()->GetScriptArgs();
}

UtlUtility::SimulationMode UtlUtility::GetSimulationMode()
{
    return static_cast<UtlUtility::SimulationMode>(Platform::GetSimulationMode());
}

UINT32 UtlUtility::EscapeRead(py::kwargs kwargs)
{
    UtlGpu * pGpu;
    string path = UtlUtility::GetRequiredKwarg<string>(kwargs, "path",
            "utl.EscapeRead");
    UINT32 index = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "index",
            "utl.EscapeRead");
    UINT32 size = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "size",
            "utl.EscapeRead");

    if (size > 4)
    {
        UtlUtility::PyError(
            "EscapeRead: unsupported size %u, should not be greater than 4", 
            size);
    }

    UINT32 value;
    string escapeGildStr;

    if(UtlUtility::GetOptionalKwarg<UtlGpu *>(&pGpu, kwargs, "gpu", "utl.EscapeRead"))
    {
        UtlGil::Release gil;

        Platform::GpuEscapeRead(pGpu->GetGpuId(), path.c_str(),
                index, size, &value);
        escapeGildStr = Utility::StrPrintf("GPU_ESCAPE_READ %d:%d %s 0x%08x",
                pGpu->GetDeviceInst(),
                pGpu->GetSubdeviceInst(),
                path.c_str(), value);
    }
    else
    {
        UtlGil::Release gil;

        Platform::EscapeRead(path.c_str(), index, size, &value);
        escapeGildStr = Utility::StrPrintf("ESCAPE_READ %s 0x%08x",
                path.c_str(), value);
    }

    // ToDo: suppor Gild file in UTL?
    DebugPrintf("UTL: %s.\n", escapeGildStr.c_str());
    return value;
}

void UtlUtility::EscapeWrite(py::kwargs kwargs)
{
    UtlGpu * pGpu;
    string path = UtlUtility::GetRequiredKwarg<string>(kwargs, "path",
            "utl.EscapeWrite");
    UINT32 index = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "index",
            "utl.EscapeWrite");
    UINT32 size = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "size",
            "utl.EscapeWrite");
    UINT32 value = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "value",
            "utl.EscapeWrite");

    if (size > 4)
    {
        UtlUtility::PyError(
            "EscapeWrite: unsupported size %u, should not be greater than 4", 
            size);
    }

    string escapeGildStr;

    if(UtlUtility::GetOptionalKwarg<UtlGpu *>(&pGpu, kwargs, "gpu", "utl.EscapeWrite"))
    {
        UtlGil::Release gil;

        Platform::GpuEscapeWrite(pGpu->GetGpuId(), path.c_str(),
                index, size, value);
        escapeGildStr = Utility::StrPrintf("GPU_ESCAPE_WRITE %d:%d %s 0x%08x",
                pGpu->GetDeviceInst(),
                pGpu->GetSubdeviceInst(),
                path.c_str(), value);
    }
    else
    {
        UtlGil::Release gil;

        Platform::EscapeWrite(path.c_str(), index, size, value);
        escapeGildStr = Utility::StrPrintf("ESCAPE_WRITE %s 0x%08x",
                path.c_str(), value);
    }

    // ToDo: suppor Gild file in UTL?
    DebugPrintf("UTL: %s.\n", escapeGildStr.c_str());
}
bool UtlUtility::IsCheckUtlScriptsUtil()
{
    return Utl::Instance()->IsCheckUtlScripts();
}

py::bytes UtlUtility::EscapeReadBuffer(py::kwargs kwargs)
{
    string path = UtlUtility::GetRequiredKwarg<string>(kwargs, "path",
            "utl.EscapeReadBuffer");
    UINT32 index = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "index",
            "utl.EscapeReadBuffer");
    UINT32 size = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "size",
            "utl.EscapeReadBuffer");
    UtlGpu *pGpu = UtlUtility::GetRequiredKwarg<UtlGpu *>(kwargs, "gpu",
            "utl.EscapeReadBuffer");

    std::string data;
    data.resize(size);

    {
        UtlGil::Release gil;
        Platform::GpuEscapeReadBuffer(pGpu->GetGpuId(), path.c_str(),
            index, size, static_cast<void *>(&data[0]));
    }

    return py::bytes(std::move(data));
}

void UtlUtility::EscapeWriteBuffer(py::kwargs kwargs)
{
    string path = UtlUtility::GetRequiredKwarg<string>(kwargs, "path",
            "utl.EscapeWriteBuffer");
    UINT32 index = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "index",
            "utl.EscapeWriteBuffer");
    UINT32 size = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "size",
            "utl.EscapeWriteBuffer");
    vector<UINT08> data = UtlUtility::GetRequiredKwarg<vector<UINT08> >(kwargs, "buffer",
            "utl.EscapeWriteBuffer");
    UtlGpu *pGpu = UtlUtility::GetRequiredKwarg<UtlGpu *>(kwargs, "gpu",
            "utl.EscapeWriteBuffer");

    UtlGil::Release gil;

    Platform::GpuEscapeWriteBuffer(pGpu->GetGpuId(), path.c_str(),
            index, size, (void *)(&data[0]));
}

UINT32 UtlUtility::PciCfgRead(py::kwargs kwargs)
{
    UINT32 addr = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "address",
            "utl.PciCfgRead");
    UINT08 size = 4;
    UtlUtility::GetOptionalKwarg<UINT08>(&size, kwargs, "size", "utl.PciCfgRead");
    UtlGpu *pGpu = UtlUtility::GetRequiredKwarg<UtlGpu *>(kwargs, "gpu",
            "utl.PciCfgRead");

    UtlGil::Release gil;

    UINT32 data = 0;
    auto pPcie = pGpu->GetGpuSubdevice()->GetInterface<Pcie>();
    switch (size)
    {
        case 1:
            {
                UINT08 value = 0;
                Platform::PciRead08(pPcie->DomainNumber(),
                        pPcie->BusNumber(), pPcie->DeviceNumber(),
                        pPcie->FunctionNumber(), addr, &value);
                data = value;
            }
            break;
        case 2:
            {
                UINT16 value = 0;
                Platform::PciRead16(pPcie->DomainNumber(),
                        pPcie->BusNumber(), pPcie->DeviceNumber(),
                        pPcie->FunctionNumber(), addr, &value);
                data = value;
            }
            break;
        case 4:
            Platform::PciRead32(pPcie->DomainNumber(),
                    pPcie->BusNumber(), pPcie->DeviceNumber(),
                    pPcie->FunctionNumber(), addr, &data);
            break;
        default:
            UtlUtility::PyError("PciCfgRead: invalid size %u", size);
            break;
    }

    return data;
}

void UtlUtility::PciCfgWrite(py::kwargs kwargs)
{
    UINT32 addr = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "address",
            "utl.PciCfgWrite");
    UINT08 size = 4;
    UtlUtility::GetOptionalKwarg<UINT08>(&size, kwargs, "size", "utl.PciCfgWrite");
    UtlGpu *pGpu = UtlUtility::GetRequiredKwarg<UtlGpu *>(kwargs, "gpu",
            "utl.PciCfgWrite");
    UINT32 data = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "data",
            "utl.PciCfgWrite");

    UtlGil::Release gil;

    auto pPcie = pGpu->GetGpuSubdevice()->GetInterface<Pcie>();
    switch (size)
    {
        case 1:
            if (data & 0xff)
            {
                WarnPrintf("PciCfgWrite: data larger than %u bytes", size);
            }
            Platform::PciWrite08(pPcie->DomainNumber(),
                    pPcie->BusNumber(), pPcie->DeviceNumber(),
                    pPcie->FunctionNumber(), addr, data);
            break;
        case 2:
            if (data & 0xffff)
            {
                WarnPrintf("PciCfgWrite: data larger than %u bytes", size);
            }
            Platform::PciWrite16(pPcie->DomainNumber(),
                    pPcie->BusNumber(), pPcie->DeviceNumber(),
                    pPcie->FunctionNumber(), addr, data);
            break;
        case 4:
            Platform::PciWrite32(pPcie->DomainNumber(),
                    pPcie->BusNumber(), pPcie->DeviceNumber(),
                    pPcie->FunctionNumber(), addr, data);
            break;
        default:
            UtlUtility::PyError("PciCfgWrite: invalid size %u", size);
            break;
    }
}

UINT32 UtlUtility::PciIoRead(py::kwargs kwargs)
{
    UINT16 port = UtlUtility::GetRequiredKwarg<UINT16>(kwargs, "port",
            "utl.PciIoRead");
    UINT08 size = 4;
    UtlUtility::GetOptionalKwarg<UINT08>(&size, kwargs, "size", "utl.PciIoRead");

    UtlGil::Release gil;

    UINT32 data = 0;
    switch (size)
    {
        case 1:
            {
                UINT08 value = 0;
                Platform::PioRead08(port, &value);
                data = value;
            }
            break;
        case 2:
            {
                UINT16 value = 0;
                Platform::PioRead16(port, &value);
                data = value;
            }
            break;
        case 4:
            Platform::PioRead32(port, &data);
            break;
        default:
            UtlUtility::PyError("PciIoRead: invalid size %u", size);
            break;
    }

    return data;
}

void UtlUtility::PciIoWrite(py::kwargs kwargs)
{
    UINT32 port = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "port",
            "utl.PciIoWrite");
    UINT08 size = 4;
    UtlUtility::GetOptionalKwarg<UINT08>(&size, kwargs, "size", "utl.PciIoWrite");
    UINT32 data = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "data",
            "utl.PciIoWrite");

    UtlGil::Release gil;

    switch (size)
    {
        case 1:
            if (data & 0xff)
            {
                WarnPrintf("PciIoWrite: data larger than %u bytes", size);
            }
            Platform::PioWrite08(port, data);
            break;
        case 2:
            if (data & 0xffff)
            {
                WarnPrintf("PciIoWrite: data larger than %u bytes", size);
            }
            Platform::PioWrite16(port, data);
            break;
        case 4:
            Platform::PioWrite32(port, data);
            break;
        default:
            UtlUtility::PyError("PciIoWrite: invalid size %u", size);
            break;
    }
}

void UtlUtility::EnableInterrupts()
{
    Platform::EnableInterrupts();
}

void UtlUtility::DisableInterrupts()
{
    Platform::DisableInterrupts();
}

UINT64 UtlUtility::GetTimeUs()
{
    return Platform::GetTimeUS();
}

// This function can be called from a UTL script to print an info message.
//
void UtlUtility::FcraLoggingPy(py::kwargs kwargs)
{
    if (!ChiplibTraceDumper::GetPtr()->RawDumpChiplibTrace())
    {
        return;
    }

    string prefix = UtlUtility::GetRequiredKwarg<string>(kwargs, "prefix",
        "utl.FcraLogging");
    string text = UtlUtility::GetRequiredKwarg<string>(kwargs, "text",
        "utl.FcraLogging");

    FcraLogging(prefix, text);
}

// This function can be called from a UTL c++ codes to print an info message.
//
void UtlUtility::FcraLogging(const string& prefix, const string& text)
{
    if (!ChiplibTraceDumper::GetPtr()->RawDumpChiplibTrace())
    {
        return;
    }

    ChiplibOpBlock *lwrBlock = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
    lwrBlock->AddAnnotOp(prefix.c_str(), (text + "\n").c_str());
}

bool UtlUtility::IsVirtualFunction()
{
    return Platform::IsVirtFunMode();
}

void UtlUtility::FlushCpuWriteCombineBuffer()
{
    UtlGil::Release gil;
    RC rc = Platform::FlushCpuWriteCombineBuffer();
    UtlUtility::PyErrorRC(rc, "utl.FlushCpuWriteCombineBuffer failed");
}

void UtlUtility::SetDumpPhysicalAccess(py::kwargs kwargs)
{
    bool enable = UtlUtility::GetRequiredKwarg<bool>(kwargs, "enable",
        "utl.SetDumpPhysicalAccess");

    UtlGil::Release gil;
    
    Platform::SetDumpPhysicalAccess(enable);
}

// This function can be called from a UTL script to clock the simulator for specified number of cycles

void UtlUtility::ClockSimulator(py::kwargs kwargs)
{
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        UtlUtility::PyError("ClockSimulator function can not be used with emulation or silicon");
    }
    UINT32 cycles = GetRequiredKwarg<UINT32>(kwargs, "cycles", "utl.ClockSimulator");
    UtlGil::Release gil;
    Platform::ClockSimulator(cycles);
}

void UtlUtility::DirectPhysicalWritePy(py::kwargs kwargs)
{
    UINT64 address = GetRequiredKwarg<UINT64>(kwargs, "address", "utl.DirectPhysicalWrite");
    vector<UINT08> data = GetRequiredKwarg<vector<UINT08>>(kwargs, "data", "utl.DirectPhysicalWrite");

    UtlGil::Release gil;

    void* pCpuVirtualAddress = 0;
    RC rc;

    // For Emu/Si map the address to CPU VASpace
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        rc = Platform::MapDeviceMemory(&pCpuVirtualAddress, address, 
            data.size(), Memory::UC, Memory::ReadWrite);

        UtlUtility::PyErrorRC(rc, "utl.DirectPhysicalWrite: Failed to map memory.");
    }

    rc = Platform::PhysWr(address, &data[0], data.size());
    UtlUtility::PyErrorRC(rc, "utl.DirectPhysicalWrite: Failed to write data.");

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        Platform::UnMapDeviceMemory(pCpuVirtualAddress);
    }
}

vector<UINT08> UtlUtility::DirectPhysicalReadPy(py::kwargs kwargs)
{
    UINT64 address = GetRequiredKwarg<UINT64>(kwargs, "address", "utl.DirectPhysicalRead");
    UINT64 size = GetRequiredKwarg<UINT64>(kwargs, "size", "utl.DirectPhysicalRead");

    UtlGil::Release gil;

    vector<UINT08> data(size);

    void* pCpuVirtualAddress = 0;
    RC rc;

    // For Emu/Si map the address to CPU VASpace
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        rc = Platform::MapDeviceMemory(&pCpuVirtualAddress, address, 
            data.size(), Memory::UC, Memory::ReadWrite);

        UtlUtility::PyErrorRC(rc, "utl.DirectPhysicalWrite: Failed to map memory.");
    }

    rc = Platform::PhysRd(address, &data[0], size);
    UtlUtility::PyErrorRC(rc, "utl.DirectPhysicalRead: Failed to read data.");

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        Platform::UnMapDeviceMemory(pCpuVirtualAddress);
    }

    return data;
}

UINT32 UtlUtility::CallwlateBufferCrc(py::kwargs kwargs)
{
    vector<UINT08> data = UtlUtility::GetRequiredKwarg<vector<UINT08>>(kwargs,
        "data", "utl.CallwlateBufferCrc");

    return BufCRC32(reinterpret_cast<uintptr_t>(&data[0]), data.size());
}

void UtlUtility::TriggerUserEventPy(py::kwargs kwargs)
{
    map<string, string> data = UtlUtility::GetRequiredKwarg<map<string, string> >(kwargs,
                    "data", "utl.TriggerUserEvent");

    Utl::Instance()->TriggerUserEvent(move(data));
}

bool UtlUtility::IsPolicyFileLaunchingVFs()
{
    PolicyManager * pPolicyMgr = PolicyManager::Instance();

    if (!pPolicyMgr)
        return false;
    
    return pPolicyMgr->GetStartVfTestInPolicyManager();
}
