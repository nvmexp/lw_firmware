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

#ifndef INCLUDED_UTLUTIL_H
#define INCLUDED_UTLUTIL_H

#include "core/include/tasker.h"
#include "core/include/utility.h"

#include "utl.h"
#include "utlpython.h"
#include "core/include/platform.h"
#include "utlkwargsmgr.h"

class UtlLwrrentSriovFunction;

// UtlSequence class is a special data structure which allows a data array to 
// be accessed in bytes on the python side. 
// It can support [] operator and thus allow the python script to modify the 
// data in-place. It also works with len(seqData)
class UtlSequence 
{
public:
    UtlSequence(void* data, size_t size);
    static void Register(py::module module);
    ~UtlSequence() {};

    UINT08 operator[](size_t index) const { return m_Data[index]; }
    UINT08 &operator[](size_t index) { return m_Data[index]; }

    UINT08* GetData() { return m_Data; }

    UtlSequence() = delete;
    UtlSequence(UtlSequence&) = delete;
    UtlSequence& operator=(UtlSequence&) = delete;

private:
    size_t m_Size;
    UINT08 *m_Data;
};

// The purpose of this class is to catch hangs caused by a thread trying to
// acquire the Python Global Interpreter Lock (GIL) when a different thread has
// not released the GIL.
//
class UtlGil
{
public:
    // This class is a wrapper around pybind11's GIL acquire object.
    // The wrapper updates global information stored in the UtlGil class
    // in order to check for potential hangs.
    //
    class Acquire
    {
    public:
        Acquire();
        ~Acquire();

    private:
        unique_ptr<py::gil_scoped_acquire> m_Gil;
    };

    // This class is a wrapper around pybind11's GIL release object.
    // The wrapper updates global information stored in the UtlGil class
    // in order to check for potential hangs.
    //
    class Release
    {
    public:
        Release();
        ~Release();

    private:
        unique_ptr<py::gil_scoped_release> m_Gil;
    };

    // This class should be used in python exported methods in which the
    // thread should NOT yield. If there is chance for thread to yield,
    // UtlRelease should be used instead of this class.
    class NoYield
    {
    public:
        NoYield();
        ~NoYield();
    };

    UtlGil() = delete;
    UtlGil(UtlGil&) = delete;
    UtlGil& operator=(UtlGil&) = delete;

private:
    // This member stores the thread ID of the current thread that owns the GIL.
    // It should be set to Tasker::NULL_THREAD when no thread owns the GIL.
    static Tasker::ThreadID s_ThreadWithGil;
};

// This namespace has miscellaneous utility functions that are used by
// the UTL classes.  Also, any Python functions that belong to the top-level
// utl module are defined here.
//
namespace UtlUtility
{
    bool IsCheckUtlScriptsUtil(); 
    void Register(py::module module);

    // Functions for reporting error messages from C++ code that is called
    // by the Python interpreter.
    template<typename ...Args>
    void PyError(const char* format, Args... args)
    {
        string message = Utility::StrPrintf(format, args...);
        throw std::runtime_error(message.c_str());
    }

    template<typename ...Args>
    void PyErrorRC(RC rc, const char* format, Args... args)
    {
        if (OK != rc)
        {
            string message = Utility::StrPrintf(format, args...);
            string fullMessage = Utility::StrPrintf("%s : %s",
                message.c_str(), rc.Message());

            throw std::runtime_error(fullMessage.c_str());
        }
    }

    // All UTL script functions which call into C++ pass their parameters
    // with keyword-arguments.  (For information about keyword-arguments, see
    // https://docs.python.org/3/tutorial/controlflow.html#keyword-arguments).
    // These keyword arguments are passed to C++ with a pybind11 class
    // called py::kwargs.  The py::kwargs class is similar to a STL map and
    // each argument is represented as a key-value pair in that map.
    // The following two functions are used to retrieve argument values
    // from a py::kwargs object.
    
    // This function is used to check for keyword-arguments that are always
    // required to be passed to the UTL function.  If the required argument
    // is present in py::kwargs, the argument value is returned.  Otherwise,
    // an error is reported (which will throw an exception).
    template <class ValueType>
    ValueType GetRequiredKwarg
    (
        const py::kwargs& kwargs,
        const char* keyword,
        const char* functionName
    )
    {
        UtlKwargsMgr* kwargsmgr = UtlKwargsMgr::Instance();
        if (!kwargsmgr->VerifyKwarg(functionName, keyword, true))
        {
            PyError(Utility::StrPrintf(
                "function %s unexpected required keyword argument '%s'.",
                functionName, keyword).c_str());   
        }
        if (!kwargs.contains(keyword))
        {
            PyError(Utility::StrPrintf(
                "function %s expected keyword argument '%s'.",
                functionName, keyword).c_str());
        }
        return kwargs[keyword].cast<ValueType>();
    }

    // This function is used to check for keyword-arguments that are not
    // required to be passed to the UTL function.  If the optional argument
    // is present in py::kwargs, its value will be stored in argValue and
    // this function will return true.  Otherwise, this function returns false
    // to indicate that the argument wasn't passed by the user.
    template <class ValueType>
    bool GetOptionalKwarg
    (
        ValueType* argValue,
        const py::kwargs& kwargs,
        const char* keyword,
        const char* functionName
    )
    {
        UtlKwargsMgr* kwargsmgr = UtlKwargsMgr::Instance();
        if (!kwargsmgr->VerifyKwarg(functionName, keyword, false))
        {
            PyError(Utility::StrPrintf(
                "function %s unexpected optional keyword argument '%s'.",
                functionName, keyword).c_str());   
        }
        if (kwargs.contains(keyword))
        {
            *argValue = kwargs[keyword].cast<ValueType>();
            return true;
        }
        return false;
    }

    // This function will be used to cast a py::object (which can be a list or 
    // a single element) to a vector or a single element. It will return true 
    // if the object is a single element.
    template <class ValueType>
    bool CastPyObjectToListOrSingleElement
    (
        const py::object& argPyObject,
        vector<ValueType>* dataList
    )
    {
        bool hasSingleElement = false;

        try
        {
            *dataList = argPyObject.cast<vector<ValueType>>();
        }
        catch (const py::cast_error& e)
        {
            dataList->push_back(argPyObject.cast<ValueType>());
            hasSingleElement = true;
        }
        return hasSingleElement;
    }

    // This function is used to colwert a vector of pointers into a py::list
    // object.  This colwersion is needed to prevent Python from taking
    // ownership of the pointer members of a vector when that vector is
    // returned by a UTL exported function.
    template<class ListMemberType>
    py::list ColwertPtrVectorToPyList(vector<ListMemberType*> memberList)
    {
        UtlGil::Acquire gil;
        py::list pyList;
        for (const auto& listMember : memberList)
        {
            pyList.append(py::cast(listMember,
                py::return_value_policy::reference));
        }
        return pyList;
    }

    void HandlePythonException(const char* errorText);
    void RequirePhase(Utl::UtlPhase phaseFlags, const char* pythonFunc);

    // The following Functions can be called from a UTL script and will
    // exist in the top-level utl module.
    void LogInfo(py::kwargs kwargs);
    void LogError(py::kwargs kwargs);
    void LogDebug(py::kwargs kwargs);

    UtlLwrrentSriovFunction * GetLwrrentSriovFunction();
    char * ListAllParams();
    vector<string>* GetScriptArgs();

    enum class SimulationMode
    {
        HARDWARE = Platform::Hardware,
        RTL = Platform::RTL,
        FMODEL = Platform::Fmodel,
        AMODEL = Platform::Amodel
    };


    SimulationMode GetSimulationMode();
    UINT32 EscapeRead(py::kwargs kwargs);
    py::bytes EscapeReadBuffer(py::kwargs kwargs);
    void EscapeWrite(py::kwargs kwargs);
    void EscapeWriteBuffer(py::kwargs kwargs);
    UINT32 PciCfgRead(py::kwargs kwargs);
    void PciCfgWrite(py::kwargs kwargs);
    UINT32 PciIoRead(py::kwargs kwargs);
    void PciIoWrite(py::kwargs kwargs);
    void EnableInterrupts();
    void DisableInterrupts();
    UINT64 GetTimeUs();
    bool IsVirtualFunction();
    void FlushCpuWriteCombineBuffer();

    void FcraLoggingPy(py::kwargs kwargs);
    void FcraLogging(const string& prefix, const string& text);

    void SetDumpPhysicalAccess(py::kwargs kwrags);
    void ClockSimulator(py::kwargs kwargs);
    void DirectPhysicalWritePy(py::kwargs kwargs);
    vector<UINT08> DirectPhysicalReadPy(py::kwargs kwargs);
    UINT32 CallwlateBufferCrc(py::kwargs kwargs);
    void TriggerUserEventPy(py::kwargs kwargs);
    bool IsPolicyFileLaunchingVFs();
    // This class is used to save the Python sys.path list so that it can
    // be temporarily modified and then restored.  Instantiating a variable
    // of this class type saves the sys.path list. The sys.path list is then
    // restored to the saved value when the variable goes out of scope.
    //
    class SysPathCheckpoint
    {
    public:
        SysPathCheckpoint()
        {
            // The py::list object is a reference to a list, so assigning
            // m_SavedSysPath to sys.path would mean that any modification to
            // sys.path would also affect m_SavedSysPath.  Because of this,
            // each list element must be copied individually.
            for (auto sysPath : py::module::import("sys").attr("path"))
            {
                m_SavedSysPath.append(sysPath);
            }
        }

        ~SysPathCheckpoint()
        {
            // Unlike in the constructor, an assignment to a list reference
            // is sufficient here because m_SavedSysPath will go out of scope
            // shortly after this function exits and will not be reused.
            py::module::import("sys").attr("path") = m_SavedSysPath;
        }

    private:
        py::list m_SavedSysPath;
    };
};

#endif
