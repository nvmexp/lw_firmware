/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/coverage.h"
#include "core/include/utility.h"
#include "opengl/mglcoverage.h"
#include "core/include/jscript.h"
#include "core/include/fileholder.h"
#include "core/include/tasker.h"
#include "core/include/script.h"
#include "core/include/jsonlog.h"
#include <cstdio>
#include <map>

using namespace TestCoverage;
CoverageDatabase *CoverageDatabase::s_Instance = nullptr;
void* CoverageObject::s_Mutex = nullptr;

//------------------------------------------------------------------------------------------------
CoverageObject::CoverageObject()
    :m_Initialized(false), m_Type(Unknown), m_Component(0), m_Name("Unknown")
{
    AllocateResources();
}

//------------------------------------------------------------------------------------------------
CoverageObject::CoverageObject(UINT32 type, UINT32 comp, const char * szName)
    :m_Initialized(false), m_Type(type), m_Component(comp), m_Name(szName)
{
    AllocateResources();
}

//------------------------------------------------------------------------------------------------
CoverageObject::~CoverageObject()
{
}

//------------------------------------------------------------------------------------------------
/*static*/ RC CoverageObject::AllocateResources()
{
    if (0 == s_Mutex)
    {
        s_Mutex = Tasker::AllocMutex("CoverageObject::s_Mutex",
                                     Tasker::mtxUnchecked);
    }
    return OK;
}

//------------------------------------------------------------------------------------------------
/*static*/ void CoverageObject::FreeResources()
{
    if (0 != s_Mutex)
    {
        Tasker::FreeMutex(s_Mutex);
        s_Mutex = 0;
    }
}

//------------------------------------------------------------------------------------------------
// Add a colwerage count for this test.
RC CoverageObject::AddCoverage(UINT32 subComponent, string testName)
{
    Tasker::MutexHolder mh(s_Mutex);
    UINT64 key = (static_cast<UINT64>(m_Component) << 32) | subComponent;
    CoverageRecord &rRec = m_Records[key];
    UINT32 count = rRec.m_Tests[testName];
    count++;
    rRec.m_TotalCount++;
    rRec.m_Tests[testName] = count;
    return OK;
}

//------------------------------------------------------------------------------------------------
RC CoverageObject::AddCoverage(const char* subComponent, string testName)
{
    Tasker::MutexHolder mh(s_Mutex);
    std::map<UINT64, CoverageRecord>::iterator it;
    string s(subComponent);
    // Use the ComponentEnumToString() function to do a reverse lookup of the stored subComponent.
    for (it = m_Records.begin(); it != m_Records.end(); it++)
    {
        if (ComponentEnumToString(it->first & 0xFFFFFFFF) == s)
        {
            AddCoverage(it->first & 0xFFFFFFFF, testName);
            break;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------------------------
// Return the coverage percent for this coverage object.
FLOAT64 CoverageObject::GetCoverage(string& testName)
{
    return GetCoverage(testName.c_str());
}

//------------------------------------------------------------------------------------------------
FLOAT64 CoverageObject::GetCoverage(const char * testName)
{
    UINT32 covered = 0;
    UINT32 totRecs = 0;
    FLOAT64 coverage = 0.0;
    // If no testName is supplied the caller wants the overall coverage
    bool bTotalCoverage = (testName == nullptr || *testName == 0);

    std::map<UINT64, CoverageRecord>::iterator it;

    for (it = m_Records.begin(); it != m_Records.end(); it++)
    {
        if (bTotalCoverage)
        {
            if (it->second.m_TotalCount > 0)
            {
                covered++;
            }
        }
        else
        {
            std::map<string, UINT32>::iterator search = it->second.m_Tests.find(testName);
            if (search != it->second.m_Tests.end() )
            {
                covered++;
            }
        }
        totRecs++;
    }
    if (covered)
    {
        coverage = static_cast<double>(covered) / static_cast<double>(totRecs);
    }
    return coverage;
}

//------------------------------------------------------------------------------------------------
// Iterate through all of the coverage objects and report a specific test coverage or the global
// coverage value. If testName is provided than the test specific coverage is reported, otherwise
// the global coverage is reported.
RC CoverageObject::ReportCoverage
(
    FILE *fp,
    UINT32 coverageFlags,
    string &testName,
    JsonItem *pJsItem
)
{
    std::map<UINT64, CoverageRecord>::iterator it;
    Tee::Priority pri = Tee::PriNormal;
    UINT32 globalRecs = 0;
    UINT32 testRecs = 0;
    UINT32 totRecs = 0;
    float coverage = 0.0;

    // rough callwlation of coverage percent.
    if (coverageFlags & ReportBasicCoverage)
    {
        for (it = m_Records.begin(); it != m_Records.end(); it++)
        {
            if (it->second.m_TotalCount > 0)
            {
                globalRecs++;
            }
            // Check for test specific coverage.
            if (!testName.empty())
            {
                std::map<string, UINT32>::iterator search = it->second.m_Tests.find(testName);
                if (search != it->second.m_Tests.end() )
                {
                    testRecs++;
                }
            }
            totRecs++;
        }
        if (!testName.empty())
        {
            coverage = static_cast<float>(testRecs) / static_cast<float>(totRecs) * 100;
            fprintf(fp, "%s: %s is at %3.2f%% with %d out of %d components covered. ",
                    testName.c_str(), GetName().c_str(), coverage, testRecs, totRecs);
        }
        coverage = static_cast<float>(globalRecs) / static_cast<float>(totRecs) * 100;
        fprintf(fp, "Global: %s is at %3.2f%% with %d out of %d components covered. \n",
                GetName().c_str(), coverage, globalRecs, totRecs);
        if (pJsItem)
        {
            double dCoverage = coverage / 100.0;
            string pfname = "pf_" + GetName();
            pJsItem->SetField(pfname.c_str(), dCoverage);
        }
    }

    if (coverageFlags & ReportDetailedCoverage)
    {
        for (it = m_Records.begin(); it != m_Records.end(); it++)
        {
            fprintf(fp, "%.1d, 0x%llx:%-53s, %.4d ",
                    m_Type,
                    it->first,  // bits 31:16 component, 15:0 subComponent
                    ComponentEnumToString(it->first & 0xFFFF).c_str(),
                    it->second.m_TotalCount);

            for (std::map<string, UINT32>::iterator ii = it->second.m_Tests.begin();
                  ii != it->second.m_Tests.end();
                  ii++)
            {
                //first = testName, second=count
                fprintf(fp, ", %-12s: %.3d", ii->first.c_str(), ii->second);
            }
            fprintf(fp, "\n");
        }
    }

    // Report coverage data in the mods.log
    if (coverageFlags & ReportInlineFlag)
    {
        if (coverageFlags & ReportBasicCoverage)
        {
            if (!testName.empty())
            {
                coverage = static_cast<float>(testRecs) / static_cast<float>(totRecs) * 100;
                Printf(pri, "%s: %s is at %3.2f%% with %d out of %d components covered. ",
                        testName.c_str(), GetName().c_str(), coverage, testRecs, totRecs);
            }
            coverage = static_cast<float>(globalRecs) / static_cast<float>(totRecs) * 100;
            Printf(pri, "Global: %s is at %3.2f%% with %d out of %d components covered. \n",
                    GetName().c_str(), coverage, globalRecs, totRecs);

        }
        if (coverageFlags & ReportDetailedCoverage)
        {
            for (it = m_Records.begin(); it != m_Records.end(); it++)
            {
                Printf(pri, "%.1d, 0x%llx:%-53s, %.4d ",
                        m_Type,
                        it->first,  // bits 31:16 component, 15:0 subComponent
                        ComponentEnumToString(it->first & 0xFFFF).c_str(),
                        it->second.m_TotalCount);

                for (std::map<string, UINT32>::iterator ii = it->second.m_Tests.begin();
                      ii != it->second.m_Tests.end();
                      ii++)
                {
                    //first = testName, second=count
                    Printf(pri, ", %-12s: %.3d", ii->first.c_str(), ii->second);
                }
                Printf(pri, "\n");
            }
        }
    }
    return (OK);
}

//------------------------------------------------------------------------------------------------
// Specify the filename for the coverage database
RC CoverageDatabase::SetFilename(const char * filename)
{
    m_Filename = filename;
    return OK;
}

//------------------------------------------------------------------------------------------------
RC CoverageDatabase::SetCoverageLevel(UINT32 level)
{
    m_CoverageLevel = level;
    return OK;
}

//------------------------------------------------------------------------------------------------
RC CoverageDatabase::SetReportCoverageInline(bool bInline)
{
    if (bInline)
    {
        m_CoverageLevel |= (ReportInlineFlag);
    }
    else
    {
        m_CoverageLevel &= ~(ReportInlineFlag);
    }
    return OK;
}

//------------------------------------------------------------------------------------------------
bool CoverageDatabase::GetReportCoverageInline()
{
    return ((m_CoverageLevel & (ReportInlineFlag)) != 0);
}

//------------------------------------------------------------------------------------------------
// Report the current test coverage base on the coverage level.
// If testName is nullptr then the global test coverage will be reported otherwise the test
// coverage associated with testName will be reported.
RC CoverageDatabase::ReportCoverage(string &testName)
{
    if (!m_CoverageLevel)
    {
        return OK;
    }
    
    if (!m_pJsCoverage)
    {   
        JavaScriptPtr pJs;
        JSContext *pContext = nullptr;
        pJs->GetContext(&pContext);
        m_pJsCoverage = new JsonItem(pContext, pJs->GetGlobalObject(), "GlobalCoverage");
        m_pJsCoverage->SetTag("GlobalCoverage");
    }
    RC rc;
    FileHolder fileHolder;
    // Get coverage level from JS object.
    GetCoverageLevel();
    CHECK_RC(fileHolder.Open(m_Filename.c_str(), "w+"));

    FILE *fp = fileHolder.GetFile();
    std::map<UINT64, CoverageObject *>::iterator it = m_Objects.begin();
    bool bPrintInline = (m_CoverageLevel & ReportInlineFlag) != 0;
    if (m_CoverageLevel & ReportBasicCoverage)
    {
        UINT32 printMask = m_CoverageLevel & (ReportBasicCoverage | ReportInlineFlag);
        fprintf(fp, "BASIC COVERAGE:\n");
        if (bPrintInline)
        {
            Printf(Tee::PriNormal, "BASIC COVERAGE:\n");
        }
        while (it != m_Objects.end())
        {
            it->second->ReportCoverage(fp, printMask, testName, m_pJsCoverage);
            it++;
        }
        m_pJsCoverage->WriteToLogfile();
    }

    if (m_CoverageLevel & ReportDetailedCoverage)
    {
        UINT32 printMask = m_CoverageLevel & (ReportDetailedCoverage | ReportInlineFlag);
        it = m_Objects.begin();
        fprintf(fp, "DETAILED COVERAGE:\n");
        if (bPrintInline)
        {
            Printf(Tee::PriNormal, "DETAILED COVERAGE:\n");
        }
        while (it != m_Objects.end())
        {
            it->second->ReportCoverage(fp, printMask, testName, nullptr);
            it++;
        }
    }

    return OK;
}
//------------------------------------------------------------------------------------------------
// Returns a pointer to the coverage object given the object name.
RC CoverageDatabase::GetCoverageObject(string &objectName, CoverageObject **ppCov)
{
    *ppCov = nullptr;
    std::map<UINT64, CoverageObject *>::iterator it;
    for (it = m_Objects.begin(); it != m_Objects.end(); it++)
    {
        if (it->second->GetName() == objectName)
        {
            *ppCov = it->second;
            return OK;
        }
    }
    return RC::COVERAGE_OBJECT_NOT_FOUND;
}

//------------------------------------------------------------------------------------------------
// Returns a pointer to coverage object given the coverage object and component type.
RC CoverageDatabase::GetCoverageObject(UINT32 type, UINT32 component, CoverageObject ** ppCov)
{
    UINT64 key = static_cast<UINT64>(type) << 32 | static_cast<UINT64>(component);
    std::map<UINT64, CoverageObject *>::iterator it = m_Objects.find(key);
    if (it == m_Objects.end())
    {
        *ppCov = nullptr;
        return RC::COVERAGE_OBJECT_NOT_FOUND;
    }
    else
        *ppCov = it->second;

    return OK;
}

//------------------------------------------------------------------------------------------------
RC CoverageDatabase::RegisterObject(UINT32 type, UINT32 component, CoverageObject * pCov)
{
    UINT64 key = static_cast<UINT64>(type) << 32 | static_cast<UINT64>(component);
    std::map<UINT64, CoverageObject *>::iterator it = m_Objects.find(key);
    if (it == m_Objects.end() )
    {
        m_Objects[key] = pCov;
        return OK;
    }
    else
    {
        if (it->second != pCov)
        {
            Printf(Tee::PriError,
                   "Error a CoverageObject of type:0x%x with component:0x%x"
                   " has already been registered\n", type, component);
            return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------------------------
// Return a pointer to the single instance of this database.
CoverageDatabase * CoverageDatabase::Instance()
{
    if (s_Instance == nullptr)
    {
        s_Instance = new CoverageDatabase;
    }
    return s_Instance;
}

//------------------------------------------------------------------------------------------------
// Free up the memory resources consumed by this database.
void CoverageDatabase::FreeCoverageObjects()
{
    std::map<UINT64, CoverageObject *>::iterator it = m_Objects.begin();
    while (it != m_Objects.end())
    {
        delete it->second;
        it++;
    }
    m_Objects.clear();
    if (m_pJsCoverage)
    {
        delete m_pJsCoverage;
        m_pJsCoverage = nullptr;
    }
    CoverageObject::FreeResources();
}

//------------------------------------------------------------------------------------------------
/*static*/
void CoverageDatabase::FreeResources()
{
    if (s_Instance != nullptr)
    {
        s_Instance->FreeCoverageObjects();
        delete s_Instance;
        s_Instance = nullptr;
    }
}

//------------------------------------------------------------------------------------------------
CoverageDatabase::~CoverageDatabase()
{
}

//------------------------------------------------------------------------------------------------
CoverageDatabase::CoverageDatabase()
{
    m_Filename = "mods.cov";
    m_CoverageLevel = 0;
}

//------------------------------------------------------------------------------------------------
// Get the coverage level from the JS global object properties.
UINT32 CoverageDatabase::GetCoverageLevel()
{
    return m_CoverageLevel & ReportVerboseCoverage;
}

//------------------------------------------------------------------------------------------------
// JS Linkage for the global JS object.
//------------------------------------------------------------------------------------------------
C_(Global_GetCoverage);
static SMethod Global_GetCoverage
(
    "GetCoverage",
    C_Global_GetCoverage,
    2,
    "Get global/test coverage for objectName, test."
);
// SMethod
C_(Global_GetCoverage)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    StickyRC rc;
    JavaScriptPtr pJavaScript;
    string testName = "";
    string objName = "";

    // Check the argument.
    switch (NumArguments)
    {
        default:
            JS_ReportError(pContext, "Usage: GetCoverage(objName[, testname])");
            return JS_FALSE;
        case 2:
            rc = pJavaScript->FromJsval(pArguments[1], &testName); // fall through
        case 1:
            rc = pJavaScript->FromJsval(pArguments[0], &objName);
            if (OK != rc)
            {
                JS_ReportError(pContext, "Error oclwrred in GetCoverage().");
                *pReturlwalue = JSVAL_NULL;
                return JS_FALSE;
            }
            break;
    }

    CoverageDatabase * pDB = CoverageDatabase::Instance();
    if (pDB)
    {
        CoverageObject * pCov = nullptr;
        rc = pDB->GetCoverageObject(objName, &pCov);
        if (OK == rc)
        {
            if (pJavaScript->ToJsval(pCov->GetCoverage(testName.c_str()), pReturlwalue) == OK)
            {
                return JS_TRUE;
            }
        }
        else
        {
            JS_ReportError(pContext, "Error oclwrred getting coverage object %s.",
                           objName.c_str());
        }
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred getting coverage database.");
    }

    return JS_FALSE;
}

//------------------------------------------------------------------------------------------------
// Enable test coverage using specified level of detail.
C_(Global_EnableCoverage);
static SMethod Global_EnableCoverage
(
    "EnableCoverage",
    C_Global_EnableCoverage,
    1,
    "Enable test coverage at with specified level of detail."
);
// SMethod
C_(Global_EnableCoverage)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    JavaScriptPtr pJavaScript;
    UINT32 coverageLevel = 0;

    // Check the argument
    if (NumArguments != 1 ||
       (pJavaScript->FromJsval(pArguments[0], &coverageLevel) != OK))
    {
        JS_ReportError(pContext, "Usage: EnabledCoverage(coverageLevel)");
        return JS_FALSE;
    }

    CoverageDatabase * pDB = CoverageDatabase::Instance();
    if (pDB)
    {
        pDB->SetCoverageLevel(coverageLevel);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred getting coverage database.");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------------------------
// Report the test coverage to the specified filename.
//------------------------------------------------------------------------------------------------
C_(Global_ReportCoverage);
static STest Global_ReportCoverage
(
    "ReportCoverage",
    C_Global_ReportCoverage,
    2,
    "Report test/global coverage output to filename. "
);
// STest
C_(Global_ReportCoverage)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    StickyRC rc;
    JavaScriptPtr pJavaScript;
    string filename = "";
    string testName = "";
    // Check the argument.
    switch (NumArguments)
    {
        default:
            JS_ReportError(pContext, "Usage: ReportCoverage(filename[, testname])");
            return JS_FALSE;
        case 2:
            rc = pJavaScript->FromJsval(pArguments[1], &testName); // fall through
        case 1:
            rc = pJavaScript->FromJsval(pArguments[0], &filename);
            if (OK != rc)
            {
                JS_ReportError(pContext, "Error oclwrred in ReportCoverage().");
                *pReturlwalue = JSVAL_NULL;
                return JS_FALSE;
            }
            break;
    }

    CoverageDatabase * pDB = CoverageDatabase::Instance();
    if (pDB)
    {
        pDB->SetFilename(filename.c_str());
        rc = pDB->ReportCoverage(testName);
    }
    else
    {
        JS_ReportError(pContext, "Error oclwrred getting coverage database.");
        return JS_FALSE;
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------------------------
// Enable test coverage using specified level of detail.
C_(Global_FreeCoverageResources);
static SMethod Global_FreeCoverageResources
(
    "FreeCoverageResources",
    C_Global_FreeCoverageResources,
    0,
    "Free up all of the test coverage resources."
);
// SMethod
C_(Global_FreeCoverageResources)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    CoverageDatabase::FreeResources();

    return JS_TRUE;
}

//------------------------------------------------------------------------------------------------
// Const properties for coverage flags.
GLOBAL_JS_PROP_CONST(ReportCoverageInline, ReportInlineFlag,
                     "Report test coverage in mods.log");
GLOBAL_JS_PROP_CONST(ReportCoverageEndLoop, TestCoverage::ReportEndLoopFlag,
                     "Report test coverage when EndLoop is called");
GLOBAL_JS_PROP_CONST(ReportCoverageFlagsMask, TestCoverage::ReportCoverageFlagsMask,
                     "Report test coverage after each loop");

GLOBAL_JS_PROP_CONST(ReportDetailedCoverage, TestCoverage::ReportDetailedCoverage,
                     "Report detailed test coverage.");
GLOBAL_JS_PROP_CONST(ReportBasicCoverage, TestCoverage::ReportBasicCoverage,
                     "Report basic test coverage.");
GLOBAL_JS_PROP_CONST(ReportVerboseCoverage, TestCoverage::ReportVerboseCoverage,
                     "Report very verbose test coverage.");

