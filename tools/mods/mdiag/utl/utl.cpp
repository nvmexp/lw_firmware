/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "core/include/argread.h"
#include "core/include/cmdline.h"
#include "core/include/mgrmgr.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/utility/trep.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/include/gpumgr.h"
#include "lwdiagutils.h"

#include "mdiag/mdiag.h"
#include "mdiag/sysspec.h"
#include "mdiag/lwgpu.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utils/utils.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/utils/crc.h"
#include "utlgpuverif.h"

#include "utl.h"

#include "utlchannel.h"
#include "utlevent.h"
#include "utlvectorevent.h"
#include "utlgpu.h"
#include "utlhelp.h"
#include "utllwswitch.h"
#include "utlsurface.h"
#include "utlvidmemaccessbitbuffer.h"
#include "utlperfmonbuffer.h"
#include "utltest.h"
#include "utlthread.h"
#include "utlutil.h"
#include "utlgpupartition.h"
#include "utlsmcengine.h"
#include "utlvftest.h"
#include "utlengine.h"
#include "utlgpuregctrl.h"
#include "utlusertest.h"
#include "utlmutex.h"
#include "utldebug.h"
#include "utlvaspace.h"
#include "utlmemctrl.h"
#include "utlfbinfo.h"
#include "utlrawmemory.h"
#include "utlhwpm.h"
#include "utlinterrupt.h"
#include "utldpc.h"

// This header file defines the mappings of C++ classes and functions
// to the Python equivalents that can be used in a UTL script.  It must be
// included in only one source file, and that source file must be the one
// that calls the Python Interpreter.
#include "utlmodules.h"

extern void CleanupMMUFaultBuffers();

unique_ptr<Utl> Utl::s_Instance;
vector<char> Utl::m_ArgVec;

namespace
{
    const char* DefaultPythonHome = "/home/utils/Python/builds/3.6.9-20190726";
    const char s_ScriptExtractPrefix[]  = "extract_script_dir";
}

// This class is used to save the script Name with corresponding scope information.
// With the help of this class, user can easily share scope with the same script name
//

class ScriptScopeManager
{
public:
    // <utl test, scope name>
    using ScopeKey = tuple<UtlTest *, string>;

    void RegisterScriptNameToScope(ScopeKey &&key, const py::dict &scope)
    {
        if (s_NameToScope.find(key) == s_NameToScope.end())
        {
            InfoPrintf("Add scope %s in test name is %s\n", std::get<1>(key).c_str(), 
                    std::get<0>(key)->GetName().c_str());
            s_NameToScope.emplace(move(key),scope);
        }
    }

    py::dict GetScopeFromName(const ScopeKey &key)
    {
        if (s_NameToScope.find(key) != s_NameToScope.end())
        {
            return s_NameToScope[key];
        }
        return py::dict();
    }

    static ScriptScopeManager * Instance()
    {
        if (s_Instance == nullptr)
        {
            s_Instance.reset(new ScriptScopeManager());
        }

        return s_Instance.get();
    }

    static bool HasInstance() { return (s_Instance.get() != nullptr); }

    static void Free()
    {
        if (s_Instance)
        {
            UtlGil::Acquire gil;
            s_NameToScope.clear();
        }
        s_Instance.reset(nullptr);
    }

    static py::dict CreateScopeGlobal(const string& scriptPath)
    {        
        py::dict scriptGlobals;
        // For normal case which scopeName is empty
        // For dynamic trace case which is first caller 
        for (const auto &item : py::globals())
        {
            scriptGlobals[item.first] = item.second;
        }
        if (!scriptPath.empty())
        {
            // Set the __file__ attribute to more closely match normal Python behavior.
            scriptGlobals["__file__"] = py::cast(scriptPath);
        }
        return scriptGlobals;
    }
    
private:
    static map<ScopeKey, py::dict> s_NameToScope;
    static unique_ptr<ScriptScopeManager> s_Instance;
};

unique_ptr<ScriptScopeManager> ScriptScopeManager::s_Instance;
map<ScriptScopeManager::ScopeKey, py::dict> ScriptScopeManager::s_NameToScope;

// This constructor should only be called by Utl::CreateInstance.
//
Utl::Utl
(
    ArgDatabase* argDatabase,
    const ArgReader* mdiagArgs,
    Trep* trep,
    bool skipDevCreation
)
{
    m_ArgDatabase = argDatabase;
    m_MdiagArgs = mdiagArgs;
    m_Trep = trep;

    if (mdiagArgs != nullptr)
    {
        const vector<string>* testCmdLines = m_MdiagArgs->ParamStrList("-e", NULL);
        if (testCmdLines && !testCmdLines->empty())
        {
            UtlTest::s_CfgLines = *testCmdLines;
        }
    }

    if (!skipDevCreation)
    {
        UtlGpu::CreateGpus();
#if defined(INCLUDE_LWSWITCH)
        UtlLwSwitch::CreateLwSwitches();
#endif
    }

    ChannelWrapperMgr::Instance()->UseUtlWrapper();
}

Utl::~Utl()
{
    if (m_PythonRunning)
    {
        // Reacquire the Python global interpreter lock (GIL) if it was
        // released at the end of Utl::InitPython.  This code is copied
        // from the py::gil_scoped_release destructor.
        if (m_ThreadState != nullptr)
        {
            PyEval_RestoreThread((PyThreadState*) m_ThreadState);
        }

        UtlThread::FreeThreads(); 
        BaseUtlEvent::FreeCallbacks();
        UtlPmaChannel::FreePmaChannels();
        UtlSelwreProfiler::FreeProfilers();
        UtlSurface::FreeSurfaces();
        UtlUserTest::FreePythonObjects();
        UtlChannel::FreeChannels();
        UtlTest::FreeTests();
        UtlTsg::FreeTsgs();
#if defined(INCLUDE_LWSWITCH)
        UtlLwSwitch::FreeLwSwitches();
#endif
        UtlEngine::FreeEngines();
        UtlMemCtrl::FreeMemCtrls();
        UtlInterrupt::FreeInterrupts();
        UtlGpu::FreeGpus();
        UtlVfTest::FreeVfTests();
        UtlSmcEngine::FreeSmcEngines();
        UtlGpuPartition::FreeGpuPartitions();
        UtlMutex::FreeMutexes();
        UtlDpc::FreeAll();

        // Stop the Python interpreter.  This should be done after all UTL
        // objects are freed as some of them use Python memory.
        py::finalize_interpreter();

        // If MODS set the Python home variable, release the memory
        // allocated to do so.  (This memory needs to exist until the
        // interpreter finishes.)
        if (m_NewPythonHome != nullptr)
        {
            PyMem_RawFree(m_NewPythonHome);
        }
    }
}

ArgDatabase* Utl::GetArgDatabase()
{
    return m_ArgDatabase;
}

Trep* Utl::GetTrep()
{
    return m_Trep;
}

bool Utl::HasInstance()
{
    return (s_Instance.get() != nullptr);
}

bool Utl::IsAborting() const
{
    return m_IsAborting;
}

Utl::UtlPhase Utl::Phase() const
{
    return m_Phase;
}

void Utl::SetPhase(UtlPhase phase)
{
    m_Phase = phase;
}

const ArgReader* Utl::GetArgReader()
{
    return m_MdiagArgs;
}

bool Utl::IsCheckUtlScripts()
{
    return m_IsCheckUtlScripts;
}

void Utl::SetUpdateSurfaceInMmu(const string & name)
{
    m_OwnedSurfacesMmu.push_back(name);
}

bool Utl::HasUpdateSurfaceInMmu(const string & name)
{
    return find(m_OwnedSurfacesMmu.begin(), m_OwnedSurfacesMmu.end(), name) != m_OwnedSurfacesMmu.end();
}

void Utl::AddGpuPartition(LWGpuResource *pLWGpuResource, GpuPartition* pGpuPartition)
{
    for (auto & gpu : UtlGpu::GetGpus())
    {
        if (pLWGpuResource == gpu->GetGpuResource())
        {
            gpu->CreateGpuPartition(pGpuPartition);
        }
    }
}

void Utl::AddSmcEngine(GpuPartition* pGpuPartition, SmcEngine* pSmcEngine)
{
    for (auto & gpu : UtlGpu::GetGpus())
    {
        auto pUtlGpuPartition = gpu->GetGpuPartition(pGpuPartition);
        MASSERT(pUtlGpuPartition);
        pUtlGpuPartition->CreateSmcEngine(pSmcEngine);
    }
}

RC Utl::SetupVfTests()
{
    RC rc;

    VmiopMdiagElwManager* pMdiagElwMgr = VmiopMdiagElwManager::Instance();
    if (Platform::IsPhysFunMode())
    {
        if (pMdiagElwMgr != nullptr)
        {
            CHECK_RC(pMdiagElwMgr->SetupAllVfTests(m_ArgDatabase));
        }
    }

    return rc;
}

// This is the external API for creating a Utl instance.  This function
// Should only be called once.  Utl::Shutdown should be called when
// the instance is no longer needed.
//
void Utl::CreateInstance
(
    ArgDatabase* argDatabase,
    const ArgReader* mdiagArgs,
    Trep* trep,
    bool skipDevCreation
)
{
    MASSERT(!s_Instance.get());
    s_Instance.reset(new Utl(argDatabase, mdiagArgs, trep, skipDevCreation));
}

Utl* Utl::Instance()
{
    if (!s_Instance.get())
    {
        MASSERT(!"Attempted to get Utl instance before it was created.");
    }
    return s_Instance.get();
}

void Utl::Shutdown()
{
    m_ArgVec.clear();
    ScriptScopeManager::Free();
    s_Instance.reset(nullptr);
}

// Initialize the UTL resources, start the Python interpreter,
// and import the UTL scripts.
//
RC Utl::Init()
{
    RC rc;

    try
    {
        InitPython();
        CHECK_RC(ImportScripts());
        InitEvent();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    if ((rc == OK) && m_IsAborting)
    {
        rc = RC::SOFTWARE_ERROR;
    }

    m_Phase = RunPhase;

    return rc;
}

// This function sets up and launches the Python interpreter.
//
void Utl::InitPython()
{
    // Prevent Python from creating the __pycache__ directory.
    Py_DontWriteBytecodeFlag = 1;

    // By default the Python interpreter imports the site module when it is
    // initialized.  Prevent the automatic import so that we can do the import
    // manually later in this function.
    Py_NoSiteFlag = 1;

    // The embedded Python interpreter has C++ code for most of the standard
    // Python libraries, but there are some libraries that need to be loaded
    // from Python files.  If the user has set the PYTHONHOME environment
    // variable, don't do anything.  When the Python interpreter is
    // initialized, it will use that variable to find the Python library
    // files.  Otherwise, set the Python interpreter's internal Python home
    // variable to a Python installation on the farm.

    char* elwPythonHome = getelw("PYTHONHOME");

    if (elwPythonHome != nullptr)
    {
        InfoPrintf("UTL will use existing PYTHONHOME environment variable value '%s'\n",
            elwPythonHome);
    }
    else
    {
        InfoPrintf("setting PYTHONHOME environment variable to '%s' for UTL\n",
            DefaultPythonHome);

        m_NewPythonHome = Py_DecodeLocale(DefaultPythonHome, nullptr);
        MASSERT(m_NewPythonHome != nullptr);
        Py_SetPythonHome(m_NewPythonHome);
    }

    // Launch the python interpreter.
    py::initialize_interpreter();
    m_PythonRunning = true;

    // This allows the Python interpreter to work
    // in a multi-threaded environment.
    PyEval_InitThreads();

    // Set sys.exelwtable to an empty string so that when we import the
    // site module, it won't read any config files in the user's path.
    // (See bug 3293334.)
    string cmd = "import sys\n";
    cmd += "sys.exelwtable = \"\"\n";
    cmd += "import site\n";

    // Because we set the Py_NoSiteFlag to 1 earlier, site.main() needs
    // to be called manually to add the site-packages to sys.path, as some
    // UTL scripts rely on libraries in these site-packages.  For example,
    // utllib.py imports texttable and docs_gen.py imports sphinx.
    cmd += "site.main()\n";
    py::exec(cmd.c_str());

    // Add any user-specified library paths to Python's sys.path.
    UINT32 libraryPathCount = m_MdiagArgs->ParamPresent("-utl_library_path");
    for (UINT32 pathIndex = 0; pathIndex < libraryPathCount; ++pathIndex)
    {
        string cmd = "import sys\n";
        cmd += Utility::StrPrintf("sys.path[%u:%u] = [ \"%s\" ]\n",
            pathIndex,
            pathIndex,
            m_MdiagArgs->ParamNStr("-utl_library_path", pathIndex, 0));
        py::exec(cmd.c_str());
    }

    // The Python interpreter automatically grabs the global interpreter lock
    // (GIL) when it is launched.  The GIL needs to be released here in case
    // another thread wants to access Python memory.  The py::gil_scoped_release
    // object can't be used here because it will reacquire the GIL as soon
    // as the function ends.  The two lines below are copied from the
    // py::gil_scoped_release constructor.
    py::detail::get_internals();
    m_ThreadState = PyEval_SaveThread();
}

// This function imports all of the UTL scripts.
//
RC Utl::ImportScripts()
{
    RC rc;
    m_Phase = ImportScriptsPhase;
    UtlGil::Acquire gil;

    {
        // Save the sys.path list so that it can be restored after the
        // table printing library is imported.
        UtlUtility::SysPathCheckpoint checkPoint;

        // Load utl_table_printing so that UtlTablePrintEvent can use it
        // to print data in a tabular format
        string libDir = LwDiagUtils::DefaultFindFile("utllib.py", true);
        libDir = LwDiagUtils::StripFilename(libDir.c_str());
        string cmd = "import sys\n";
        cmd += Utility::StrPrintf("sys.path.append('%s')", libDir.c_str());
        py::exec(cmd.c_str());
        py::module::import("utllib").attr("utl_table_printing")();
    }

    // Execute each UTL script specified on the command-line.

    UINT32 paramCount = m_MdiagArgs->ParamPresent("-utl_script");
    for (UINT32 i = 0; i < paramCount; ++i)
    {
        string scriptParam = m_MdiagArgs->ParamNStr("-utl_script", i, 0);
        UINT32 scriptId = m_NextScriptId++;
        CHECK_RC(ImportScript(scriptParam, scriptId, "", nullptr));
    }

    py::module::import("sys").attr("argv") = nullptr;

    return rc;
}

RC Utl::ImportScript
(
    string scriptParam, 
    UINT32 id,
    const string & scopeName,
    UtlTest * pTest
)
{
    RC rc;

    // Remove leading and trailing whitespace, otherwise the list of
    // user arguments will have unwanted empty tokens.
    boost::trim(scriptParam);
    DebugPrintf("UTL: scriptParam = %s\n", scriptParam.c_str());

    // Get the script name and user arguments.  The tokenizer uses space
    // and tab characters to separate, though single and double quote
    // characters can be used to specify a single string argument
    // that has spaces in it.

    boost::escaped_list_separator<char> sep("\\", " \t", "\'\"");
    boost::tokenizer<boost::escaped_list_separator<char>> tokenInfo(
        scriptParam, sep);

    m_LwrrentScriptArgs.clear();

    // Save the tokens as the current script arguments, which will be
    // used by Utl::GetScriptArgs.
    for (const auto& token : tokenInfo)
    {
        if (!token.empty())
        {
            m_LwrrentScriptArgs.push_back(token);
        }
    }

    MASSERT(m_LwrrentScriptArgs.size() > 0);

    // Users are encouraged to use utl.GetScriptArgs, but sys.argv
    // will still be set at import time for backwards compatibility.
    py::module::import("sys").attr("argv") = m_LwrrentScriptArgs;

    vector<string> scriptProperties;
    // Format: option1=value1:option2=value2:path/scripts/b.py
    Utility::Tokenizer(m_LwrrentScriptArgs[0], ":", &scriptProperties);
    string scriptPath = scriptProperties.back();

    // Process script options if any
    for (auto scriptOption = scriptProperties.begin(); 
        scriptOption != scriptProperties.end() - 1; 
        scriptOption++)
    {
        size_t optionKeyDelimiter = scriptOption->find("=");
        // If no value than whole scriptOption is the optionKey
        string optionKey = scriptOption->substr(0, optionKeyDelimiter);

        if (optionKey == "archive_path")
        {
            if (optionKeyDelimiter == string::npos)
            {
                ErrPrintf("Please check: %s. The correct format of archive_path is archive_path=/your/path/to/hdr.zip.tgz.\n", 
                    scriptOption->c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            string packagePath = scriptOption->substr(optionKeyDelimiter + 1, scriptOption->size());
            if (packagePath.empty())
            {
                ErrPrintf("No package path after \":\" in %s\n", scriptOption->c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            packagePath = UtilStrTranslate(packagePath, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);
            string lwrrentPath = boost::filesystem::lwrrent_path().string();
            string absPackagePath = packagePath;
            // Check if it's not a absolute path
            if (packagePath.rfind(DIR_SEPARATOR_CHAR, 0) == string::npos)
            {
                absPackagePath = Utility::CombinePaths(lwrrentPath.c_str(), packagePath.c_str());
            }
            string absExtractPath;
            const auto found = m_ExtractPathMap.find(absPackagePath);
            if (found == end(m_ExtractPathMap))
            {
                string testName = "_" + MDiagUtils::GetTestNameFromTraceFile(packagePath);
                if (testName == "_")
                {
                    testName.clear();
                }
                absExtractPath = Utility::CombinePaths(lwrrentPath.c_str(),
                    (s_ScriptExtractPrefix + to_string(m_TestExtractCount++) + testName).c_str());             
                py::object ret = py::module::import("utllib")
                    .attr("extract_packaged_script")(packagePath, absExtractPath);
                RC rc = ret.cast<UINT32>();
                UtlUtility::PyErrorRC(rc, "Error oclwred in extracting file %s!\n", scriptProperties);
                InfoPrintf("Extract scripts in archive %s to path %s\n", absPackagePath.c_str(), absExtractPath.c_str());
                m_ExtractPathMap.emplace(absPackagePath, absExtractPath);
            }
            else
            {
                absExtractPath = found->second;
            }
            scriptPath = Utility::CombinePaths(absExtractPath.c_str(), scriptPath.c_str());
        }
        else
        {
            ErrPrintf("Unknown utl_script parsing option %s!\n", scriptOption->c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    // Previous versions of UTL could import a script even if the user
    // omitted the .py extension in the -utl_script argument.
    // The py::eval_file function requires the exact script path,
    // so add the .py extension to the script path if there isn't one.
    if (scriptPath.substr(scriptPath.size() - 3) != ".py")
    {
        scriptPath += ".py";
    }
    if (!LwDiagXp::DoesFileExist(scriptPath))
    {
        ErrPrintf("Missing/invalid UTL script path: %s\n", scriptPath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    // Save the sys.path list so that it can be restored after the script
    // directory is temporarily added to it.
    UtlUtility::SysPathCheckpoint checkPoint;

    string scriptDir = LwDiagUtils::StripFilename(scriptPath.c_str());

    // If the script path has a directory component, add the directory
    // to sys.path so that the script can be imported by module name.
    // Also add script path plus "../lib" to reduce the need for users
    // to specify -utl_library_path for common library locations.
    if (!scriptDir.empty())
    {
        string libDir = Utility::StrPrintf("%s/../lib", scriptDir.c_str());
        string cmd = "import sys\n";
        cmd += Utility::StrPrintf("sys.path[0:0] = [ \"%s\", \"%s\" ]\n",
            scriptDir.c_str(),
            libDir.c_str());
        py::exec(cmd.c_str());
    }

    ScriptScopeManager * pScriptScopeManager = ScriptScopeManager::Instance();
    // Make a copy of py::globals because there can be multiple
    // UTL scripts, and each one should have its own unique version
    // of the __main__ module.
    ScriptScopeManager::ScopeKey key(pTest, scopeName);
    py::dict scriptGlobals = pScriptScopeManager->GetScopeFromName(key);
    // Current pybind11 py::dict don't support the empty()
    // copy the empty() implement here
    if (scriptGlobals.size() == 0)
    {
        scriptGlobals = ScriptScopeManager::CreateScopeGlobal(scriptPath);
    }

    if (!scopeName.empty())
    {
        // Diff from the normal utl import script. Only Utl dynamic tracing
        // will register scipt name for sharing
        pScriptScopeManager->RegisterScriptNameToScope(move(key), scriptGlobals);
    }

    DebugPrintf("UTL: scriptPath %s gets id %u\n", scriptPath.c_str(), id);

    MASSERT(m_ScriptArgs.find(id) == m_ScriptArgs.end());
    m_ScriptArgs[id] = m_LwrrentScriptArgs;

    DebugPrintf("UTL: Calling py::eval_file on UTL script %s\n",
        scriptPath.c_str());

    RegisterScriptIdToThread(id);

    // Tell the Python interpreter to run the script.
    py::eval_file(scriptPath.c_str(), scriptGlobals);

    UnregisterScriptIdFromThread();

    return rc;
}

// This function triggers the InitEvent.  This event can be used instead of
// StartEvent if a user wants to run UTL code after all UTL scripts are
// imported, but does not wish to manually run tests.
//
void Utl::InitEvent()
{
    m_Phase = InitEventPhase;
    UtlGil::Acquire gil;

    UtlInitEvent initEvent(nullptr);
    initEvent.Trigger();
}

// This function triggers the UTL StartEvent, then checks the test results.
//
RC Utl::Run()
{
    RC rc;

    CHECK_RC(SetupVfTests());

    UINT32 maxStartEventCallbackCount = 
        m_MdiagArgs->ParamUnsigned("-utl_start_event_callback_count", 1);

    if (UtlStartEvent::CallbackCount() > maxStartEventCallbackCount)
    {
        ErrPrintf("StartEvent callback count %u exceeds its max permissible count of %u. "
            "Please use -utl_start_event_callback_count to override the max count.",
            UtlStartEvent::CallbackCount(),
            maxStartEventCallbackCount);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    try
    {
        // This is the main UTL phase.  All StartEvent callbacks are triggered
        // here, which is where the tests should be run.
        UtlStartEvent startEvent;
        startEvent.Trigger();

        // Wait for all child threads to finish.
        Thread::JoinAll();

        rc = ReportTestResults();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());

        // Wait for all child threads to finish.
        Thread::JoinAll();
        rc = RC::SOFTWARE_ERROR;
    }

    if ((rc == OK) && m_IsAborting)
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

// This function determines whether or not the entire test run passes or
// fails, based on the test status of each individual test and the result
// of any user specific EndEvent status.
//
RC Utl::ReportTestResults()
{
    RC rc;

    // Update trep status of every test.
    for (const auto& test : UtlTest::GetTests())
    {
        test->UpdateTrepStatus();
    }

    CHECK_RC(TriggerEndEvent());

    bool testsPassed = true;
    for (const auto& test : UtlTest::GetTests())
    {
        if (test->GetStatus() != Test::TEST_SUCCEEDED)
        {
            testsPassed = false;
            break;
        }
    }

    if (testsPassed)
    {
        InfoPrintf("Mdiag: all tests passed\n");
        CHECK_RC(m_Trep->AppendTrepString("summary = all tests passed\n"));
    }
    else
    {
        ErrPrintf("Mdiag: some tests failed\n");
        CHECK_RC(m_Trep->AppendTrepString("summary = some tests failed\n"));
    }

    return rc;
}

RC Utl::TriggerEndEvent()
{
    StickyRC rc;
    try
    {
        UtlEndEvent event;
        event.m_Status = Test::TEST_SUCCEEDED;

        // Get the first failing test status (or TEST_SUCCEEDED if all pass).
        for (const auto& test : UtlTest::GetTests())
        {
            if (test->GetStatus() != Test::TEST_SUCCEEDED)
            {
                event.m_Status = test->GetStatus();
                break;
            }
        }

        // Pass the test status to the UTL script(s) via the check event.
        // A UTL script might override the final status by modifying m_Status.
        Test::TestStatus previousStatus = event.m_Status;
        m_Phase = EndEventPhase;
        event.Trigger();

        // Check to see if any UTL script changed the test status.
        if (previousStatus != event.m_Status)
        {
            if (previousStatus == Test::TEST_SUCCEEDED)
            {
                ErrPrintf("UTL script changed test status to failing.\n");
                rc = RC::SOFTWARE_ERROR;
            }
            else
            {
                // Lwrrently the user is only allowed to override the status
                // to a failing value.  UtlEndEvent::SetStatus already checks
                // for this, so assert here instead of giving an error.
                MASSERT(event.m_Status != Test::TEST_SUCCEEDED);
            }
        }
        CHECK_RC(m_Trep->AppendTrepString(
            Utility::StrPrintf("expected results = %lu\n",
                UtlTest::GetTests().size()).c_str()));
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    if ((rc == OK) && m_IsAborting)
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

/*static*/void Utl::CreateUtlVfTests(const vector<VmiopMdiagElwManager::VFSetting> & vfSettings)
{
    UtlVfTest::CreateUtlVfTests(vfSettings);
}

char * Utl::ListAllParams()
{
    if (m_ArgVec.size() == 0)
    {
        const UINT32 vecSize = m_ArgDatabase->ListAllArgs(&m_ArgVec[0], 0);
        m_ArgVec.resize(vecSize, 0);
        m_ArgDatabase->ListAllArgs(&m_ArgVec[0], vecSize);
    }
    return &m_ArgVec[0];
}

// This function determines if UTL is responsible for running the tests.
// If the user registered at least one callback function for utl.StartEvent,
// then the user is responsible for creating tests in that callback function
// and UTL will assume test control.
//
bool Utl::ControlsTests()
{
    return HasInstance() && (UtlStartEvent::CallbackCount() > 0);
}

// This function is called from TestDirectory to register Test objects
// that are not created by UTL script.
//
void Utl::AddTest(Test* test)
{
    UtlTest::CreateFromTestPtr(test);
}

// This function is called from a Test object to register a channel
// to a UtlTest object.
//
void Utl::AddTestChannel(Test* test, LWGpuChannel* channel, const string& name, vector<UINT32>* pSubChCEInsts)
{
    UtlTest *utlTest = UtlTest::GetFromTestPtr(test);
    MASSERT(utlTest != nullptr);

    // Check to see if there's already a UtlChannel object related to the
    // specified LWGpuChannel pointer.  If not, create one.
    UtlChannel *utlChannel = UtlChannel::GetFromChannelPtr(channel);
    if (utlChannel == nullptr)
    {
        utlChannel = UtlChannel::Create(channel, name, pSubChCEInsts);

        if (channel->GetLWGpuTsg())
        {
            UtlTsg * utlTsg =  UtlTsg::GetFromTsgPtr(channel->GetLWGpuTsg());
            utlTsg->AddChannel(utlChannel);
            utlChannel->SetTsg(utlTsg);
        }

        UtlSubCtx::Create(channel->GetSubCtx(), utlTest);
        utlChannel->SetSubCtx(UtlSubCtx::GetFromSubCtxPtr(channel->GetSubCtx(), utlTest));
    }

    utlTest->AddChannel(utlChannel);
}

// This function is called from the destructor of LWGpuChannel to free the 
// corresponding UtlChannel object
//
void Utl::RemoveChannel(LWGpuChannel* channel)
{
    UtlChannel::FreeChannel(channel);
}

// This function is called from a Test object to register a tsg
// to a UtlTest object.
//
void Utl::AddTestTsg(Test* test, LWGpuTsg* tsg)
{
    UtlTest *utlTest = UtlTest::GetFromTestPtr(test);
    MASSERT(utlTest != nullptr);

    UtlTsg *utlTsg = UtlTsg::GetFromTsgPtr(tsg);
    MASSERT(utlTsg != nullptr);

    utlTest->AddTsg(utlTsg);
}

// This function is called from the destructor of LWGpuTsg to free the 
// corresponding UtlTsg object
//
void Utl::RemoveTsg(LWGpuTsg* lwgpuTsg)
{
    UtlTsg::FreeTsg(lwgpuTsg);
}

// This function is called from a Test object to register a surface
// to a UtlTest object.
//
void Utl::AddTestSurface(Test* pTest, MdiagSurf* pSurface)
{
    UtlTest *pUtlTest = UtlTest::GetFromTestPtr(pTest);
    MASSERT(pUtlTest != nullptr);

    UtlSurface *pUtlSurface = UtlSurface::GetFromMdiagSurf(pSurface);
    MASSERT(pUtlSurface != nullptr);

    pUtlTest->AddSurface(pUtlSurface);
}

// This function is called from the destructor of MdiagSurf to free the 
// corresponding UtlSurface object
//
void Utl::RemoveSurface(MdiagSurf* mdiagSurface)
{
    UtlSurface::FreeSurface(mdiagSurface);
}

// This function is called from a Test object to remove a surface from
// a UtlTest object.
//
void Utl::RemoveTestSurface(Test* pTest, MdiagSurf* pSurface)
{
    UtlTest *pUtlTest = UtlTest::GetFromTestPtr(pTest);
    MASSERT(pUtlTest != nullptr);

    UtlSurface *pUtlSurface = UtlSurface::GetFromMdiagSurf(pSurface);
    MASSERT(pUtlSurface != nullptr);

    pUtlTest->RemoveSurface(pUtlSurface);
}

// This function is called from a Gpu object to register a surface
// to a UtlGpu object.
//
void Utl::AddGpuSurface(GpuSubdevice* pGpuSubdevice, MdiagSurf* pSurface)
{
    UtlGpu *pUtlGpu = UtlGpu::GetFromGpuSubdevicePtr(pGpuSubdevice);
    MASSERT(pUtlGpu != nullptr);

    UtlSurface *pUtlSurface = UtlSurface::GetFromMdiagSurf(pSurface);
    MASSERT(pUtlSurface != nullptr);

    pUtlGpu->AddSurface(pUtlSurface);
}

// This function obtains the list of user specified arguments that were
// passed to a script.  The script is identified by the script path.
//
vector<string>* Utl::GetScriptArgs()
{
    switch (m_Phase)
    {
        case ImportScriptsPhase:
            return &m_LwrrentScriptArgs;
            break;

        case InitEventPhase:
        case RunPhase:
        case EndEventPhase:
            {
                UINT32 id = GetScriptId();
                if (m_ScriptArgs.count(id) > 0)
                {
                    return &m_ScriptArgs[id];
                }
            }
            break;

        default:
            MASSERT(!"Utl::GetScriptArgs called in unexpected phase");
            break;
    }

    return nullptr;
}

void Utl::TriggerSmcPartitionCreatedEvent(GpuPartition* pPartition)
{
    UtlSmcPartitionCreatedEvent smcPartitionCreatedEvent(
        UtlGpuPartition::GetFromGpuPartitionPtr(pPartition));
    smcPartitionCreatedEvent.Trigger();
}

void Utl::TriggerTestMethodsDoneEvent(Test* pTest)
{
    UtlTestMethodsDoneEvent event(UtlTest::GetFromTestPtr(pTest));
    event.Trigger();
}

void Utl::TriggerBeforeSendPushBufHeaderEvent
(
    Test* pTest, 
    const string& pbName,
    UINT32 pbOffset
)
{
    try
    {
        UtlBeforeSendPushBufHeaderEvent event(UtlTest::GetFromTestPtr(pTest));
        event.m_PushBufName = pbName;
        event.m_PushBufOffset = pbOffset;
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

RC Utl::TriggerTrace3dEvent(Test* pTest, const string& eventName)
{
    StickyRC rc;
    try
    {
        UtlTrace3dEvent event(UtlTest::GetFromTestPtr(pTest), eventName);
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    if ((rc == OK) && m_IsAborting)
    {
        rc = RC::SOFTWARE_ERROR;
    }
    
    return rc;
}

void Utl::TriggerTraceFileReadEvent(Test* pTest, const string& fileName, void* data, size_t size)
{
    try
    {
        UtlTraceFileReadEvent event(UtlTest::GetFromTestPtr(pTest), fileName, data, size);
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerTablePrintEvent
(
    string&& tableHeader, 
    vector<vector<string>>&& tableData,
    bool isDebug
)
{
    try
    {
        UtlTablePrintEvent event(move(tableHeader), move(tableData), isDebug);
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerUserEvent(map<string, string>&& userData)
{
    try
    {
        UtlUserEvent event(move(userData));
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerTraceOpEvent(Test* pTest, TraceOp* pOp)
{
    try
    {
        UtlTraceOpEvent event(UtlTest::GetFromTestPtr(pTest), pOp);
        event.Trigger();
    }
    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerSurfaceAllocationEvent(MdiagSurf* surface, Test* test)
{
    try
    {
        UtlSurface* utlSurface = UtlSurface::GetFromMdiagSurf(surface);
        UtlTest* utlTest = UtlTest::GetFromTestPtr(test);
        UtlSurfaceAllocationEvent event(utlSurface, utlTest);
        event.Trigger();
    }
    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerChannelResetEvent(LwRm::Handle hObject, LwRm* pLwRm)
{
    try
    {
        LwRm::Handle hClient = pLwRm->GetClientHandle();
        Tsg *pTsg = Tsg::GetTsgFromHandle(hClient, hObject);
        if (pTsg)
        {
            Tsg::Channels tsgChannels = pTsg->GetChannels();
            for (auto pTsgChannel : tsgChannels)
            {
                UtlChannel* pUtlChannel = UtlChannel::GetChannelFromHandle(pTsgChannel->GetHandle());
                if (pUtlChannel)
                {
                    UtlChannelResetEvent event(pUtlChannel);
                    event.Trigger();
                }
            }
        }
        else
        {
            UtlChannel* pUtlChannel = UtlChannel::GetChannelFromHandle(hObject);
            if (pUtlChannel)
            {
                UtlChannelResetEvent event(pUtlChannel);
                event.Trigger();
            }
        }
    }
    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerTestCleanupEvent(Test* test)
{
    try
    {
        UtlTestCleanupEvent event(UtlTest::GetFromTestPtr(test));
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::Abort()
{
    m_IsAborting = true;

    for (const auto& test : UtlTest::GetTests())
    {
        test->AbortTest();
    }
}

// Import each UTL script to check for syntax errors, but don't trigger
// any events.
//
RC Utl::CheckScripts()
{
    m_IsCheckUtlScripts = true;
    RC rc;
    try
    {
        InitPython();
        CHECK_RC(ImportScripts());
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

// Load the UTL module and print the help strings for all UTL module types.
//
RC Utl::PrintHelp()
{
    RC rc;

    try
    {
        InitPython();

        UtlGil::Acquire gil;
        UtlHelp help;

        help.Print();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

// This function do some inits related to vaspace since it need
// smc related information so it need to be called after smc initialization.
// It will be called after vaspace is initialized.
//
void Utl::InitVaSpace()
{
    for (auto & gpu : UtlGpu::GetGpus())
    {
        if (m_MdiagArgs->ParamPresent("-smc_partitioning") ||
            m_MdiagArgs->ParamPresent("-smc_partitioning_sys0_only") ||
            gpu->GetGpuSubdevice()->IsSMCEnabled() ||
            m_MdiagArgs->ParamPresent("-smc_mem"))
        {
            LWGpuResource* pGpuResource = gpu->GetGpuResource();

            for (auto it = gpu->GetGpuPartitionList().begin(); it !=
                    gpu->GetGpuPartitionList().end(); ++it)
            {
                for (auto const pUtlSmcEngine : (*it)->GetActiveSmcEngines())
                {
                    LwRm * pLwRm = pGpuResource->GetLwRmPtr(pUtlSmcEngine->GetSmcEngineRawPtr());
                    AddVaSpace(pLwRm, gpu);
                }
            }
        }
        else
        {
            for (auto test : UtlTest::GetTests())
            {
                LwRm * pLwRm = test->GetRawPtr()->GetLwRmPtr();
                AddVaSpace(pLwRm, gpu);
            }
            
        }
    }
}

void Utl::AddVaSpace
(
    LwRm * pLwRm,
    UtlGpu * pGpu
)
{
    LWGpuResource* pGpuResource = pGpu->GetGpuResource();

    LWGpuResource::VaSpaceManager * pVasManager = pGpuResource->GetVaSpaceManager(pLwRm);
    for (Trace3DResourceContainer<VaSpace>::iterator it = pVasManager->begin();
            it != pVasManager->end(); ++it)
    {
        UtlTest * pTest = nullptr;
        // just registe the vaspace which is in this trace3d or it is global
        if (it->first != LWGpuResource::TEST_ID_GLOBAL)
        {                
            pTest = UtlTest::GetFromTestId(it->first);
        }
        UtlVaSpace::Create(it->second.get(), pTest, pGpu);
    }
}

// For the serial run, per trace vaspace will be free at the end of test,
// Utl side also need to remove that reference
//
void Utl::FreeVaSpace
(
    UINT32 testId,
    LwRm * pLwRm
)
{
    UtlTest * pTest = nullptr;
    if (testId == LWGpuResource::TEST_ID_GLOBAL)
    {
        pTest = nullptr;
    }
    else
    {
        pTest = UtlTest::GetFromTestId(testId);
        MASSERT(pTest);
    }

    UtlVaSpace::FreeVaSpace(pTest, pLwRm);
}

void Utl::FreeSubCtx
(
    UINT32 testId
)
{
    UtlTest * pTest = nullptr;
    if (testId == LWGpuResource::TEST_ID_GLOBAL)
    {
        pTest = nullptr;
    }
    else
    {
        pTest = UtlTest::GetFromTestId(testId);
        MASSERT(pTest);
    }

    UtlSubCtx::FreeSubCtx(pTest);
}


RC Utl::ReadTestSpecificScript
(
    Test* test, 
    string scriptParam,
    const string &scopeName
)
{
    RC rc;

    if (!Utl::IsSupported())
    {
        ErrPrintf("UTL is not supported on this platform.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    try
    {
        UtlGil::Acquire gil;

        // Save the sys.path list so that it can be restored after each import.
        py::list sysPathList;
        for (auto sysPath : py::module::import("sys").attr("path"))
        {
            sysPathList.append(sysPath);
        }

        // Load utl_table_printing so that UtlTablePrintEvent can use it
        // to print data in a tabular format
        string libDir = LwDiagUtils::DefaultFindFile("utllib.py", true);
        libDir = LwDiagUtils::StripFilename(libDir.c_str());
        string cmd = "import sys\n";
        cmd += Utility::StrPrintf("sys.path.append('%s')", libDir.c_str());
        py::exec(cmd.c_str());
        py::module::import("utllib").attr("utl_table_printing")();
        // Restore sys.path so each UTL script can be imported with a clean list.
        py::module::import("sys").attr("path") = sysPathList;

        UINT32 scriptId = m_NextScriptId++;
        m_IdTestMap[scriptId] = test;
        ImportScript(scriptParam, scriptId, scopeName, UtlTest::GetFromTestPtr(test));

        // Restore sys.path in case the previous script modified it
        // so that the next UTL script can be imported with a clean list.
        py::module::import("sys").attr("path") = sysPathList;
        py::module::import("sys").attr("argv") = nullptr;
    }
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

void Utl::TriggerTestInitEvent(Test* test)
{
    try
    {
        UtlInitEvent event(UtlTest::GetFromTestPtr(test));
        event.Trigger();
    }
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

void Utl::TriggerVfLaunchedEvent(UINT32 gfid)
{
    try
    {
        UtlVfLaunchedEvent event(UtlVfTest::GetFromGfid(gfid));
        event.Trigger();
    }
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }
}

UINT32 Utl::GetScriptId()
{
    UINT32 scriptId = 0;
    Tasker::ThreadID threadId = Tasker::LwrrentThread();
    auto iter = m_ThreadIdToScriptId.find(threadId);

    if (iter != m_ThreadIdToScriptId.end())
    {
        scriptId = (*iter).second.top();
    }

    return scriptId;
}

UtlTest* Utl::GetTestFromScriptId()
{
    UINT32 scriptId = Utl::Instance()->GetScriptId();

    if (m_IdTestMap.find(scriptId) != m_IdTestMap.end())
    {
        Test* test = m_IdTestMap[scriptId];
        MASSERT(test != nullptr);
        UtlTest* utlTest = UtlTest::GetFromTestPtr(test);
        MASSERT(utlTest != nullptr);
        return utlTest;
    }

    return nullptr;
}

void Utl::RegisterScriptIdToThread(UINT32 scriptId)
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();
    auto iter = m_ThreadIdToScriptId.find(threadId);
    if (iter == m_ThreadIdToScriptId.end())
    {
        m_ThreadIdToScriptId[threadId].push(scriptId);
    }
    else
    {
        (*iter).second.push(scriptId);
    }
}

void Utl::UnregisterScriptIdFromThread()
{
    Tasker::ThreadID threadId = Tasker::LwrrentThread();
    auto iter = m_ThreadIdToScriptId.find(threadId);
    MASSERT(iter != m_ThreadIdToScriptId.end());

    (*iter).second.pop();
    if ((*iter).second.size() == 0)
    {
        m_ThreadIdToScriptId.erase(iter);
    }
}

void Utl::FreeSurfaceMmuLevels(MdiagSurf* pSurface)
{
    if (UtlSurface::GetFromMdiagSurf(pSurface))
    {
        UtlSurface::GetFromMdiagSurf(pSurface)->FreeMmuLevels();
    }
}

// Determine if UTL is supported on the current platform.
//
bool Utl::IsSupported()
{
    // If the user specifies a PYTHONHOME environment variable, assume they are
    // pointing to a compatible Python library, and therefore UTL can work.
    // Otherwise, check for the existence of the default Python library
    // that UTL uses if PYTHONHOME is not specified.
    return (getelw("PYTHONHOME") != nullptr) ||
        LwDiagXp::DoesFileExist(DefaultPythonHome);
}

RC Utl::Eval
(
    const string & expression, 
    const string & scopeName, 
    Test * pTest
)
{
    RC rc;

    try
    {
        UtlGil::Acquire gil;

        ScriptScopeManager * pScriptScopeManager = ScriptScopeManager::Instance();
        UtlTest *utlTest = UtlTest::GetFromTestPtr(pTest);
        ScriptScopeManager::ScopeKey key(utlTest, scopeName);

        py::dict scope = pScriptScopeManager->GetScopeFromName(key);
        if (scope.size() == 0)
        {
            ErrPrintf("Can't find corresponding scope %s in Test %s.\n",
                    scopeName.c_str(), pTest->GetTestName());
            return RC::SOFTWARE_ERROR;
        }

        py::eval(expression, scope);
    }
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Utl::Exec
(
    const string & expression, 
    const string & scopeName, 
    Test * pTest
)
{
    RC rc;

    try
    {
        UtlGil::Acquire gil;
        
        ScriptScopeManager * pScriptScopeManager = ScriptScopeManager::Instance();
        UtlTest *utlTest = UtlTest::GetFromTestPtr(pTest);
        ScriptScopeManager::ScopeKey key(utlTest, scopeName);

        py::dict scope = pScriptScopeManager->GetScopeFromName(key);
        if (scope.size() == 0)
        {
            // Use "" as scriptName since it's an inline script
            scope = ScriptScopeManager::CreateScopeGlobal("");
        }

        if (!scopeName.empty())
        {
            // Diff from the normal utl import script. Only Utl dynamic tracing
            // will register scipt name for sharing
            pScriptScopeManager->RegisterScriptNameToScope(move(key), scope);
        }
        py::exec(expression, scope);
    }
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

void Utl::TriggerTestParseEvent
(
    Test* pTest,
    vector<char> *buf
)
{
    UtlTestParseEvent event(UtlTest::GetFromTestPtr(pTest), buf);
    event.Trigger();
}

void Utl::FreePreResetRecoveryObjects()
{
    RmEventData::FreeAllEventData();
    CleanupMMUFaultBuffers();
}
