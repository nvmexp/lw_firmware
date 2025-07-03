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
#ifndef INCLUDED_COVERAGE_H
#define INCLUDED_COVERAGE_H
/*
 * This header file describes the classes needed for reporting test coverage from within MODS.
 * The CoverageDatabase contains a list of CoverageObjects, each CoverageObject contains a list of
 * CoverageRecords. Each CoverageRecord provides details about how many times the specific item
 * was covered for each test.
 * To implement new coverage reporting you need to do the following:
 * 1. Create a new CoverageObject class (see mglcoverage.cpp for examples)
 * 2. Add that CoverageObject to the CoverageDatabase
 * 3. Report each time a coverage component has been tested from within your test.
*/
#include "rc.h"
#include <map>
#include <set>

class JsonItem;

namespace TestCoverage
{

enum eCoverageReportFlags
{
     ReportBasicCoverage = 0x1       // report on coverage percent
    ,ReportDetailedCoverage = 0x2   // report details about each subComponent
    ,ReportVerboseCoverage = 0xff   // report everything we can.
    ,ReportCoverageFlagsMask = 0xff00  // special reporting flags.
    ,ReportEndLoopFlag = (1 << 14)      // report coverage everytime Gputest::EndLoop() is called
    ,ReportInlineFlag = (1 << 15)       // report in mods.log using Printf(Tee::PriNormal)
};

enum eCoverageType
{
    Unknown,
    Hardware,       //GPU hardware features
    Software,       //OpenGL, Lwca, etc. software features not tied to specific hardware.
    System,         //PEX bandwidth, MSI, INTA, etc
    Thermal,        //Temp sensors, Fans, etc.
    Power           //GCx, ELPG, MSCG, etc
};

// Specific GPU hardware components covered. This list will get expanded in the future.
// prefix these with 'hw'
enum eHardwareComponent
{
    hwUnknown = 0,
    hwIsa = 1,
    hwTexture
};

// Specific software components covered, prefix all of these with 'sw'
enum eSoftwareComponent
{
    swUnknown = 0,
    swOpenGLExt = 1
};

// Specifi system components covered, prefix this with 'sys'.
enum eSystemComponent
{
    sysUnknown = 0
};

// Specific thermal components covered, prefix these with 'therm'
enum eThermalComponent
{
    thermUnknown = 0
};

// Specific power components covered, prefix these with 'pwr'.
enum ePowerComponent
{
    pwrUnknown = 0
};

//------------------------------------------------------------------------------------------------
// The CoverageRecord maintains a map of TestNames to coverage counts for a specific coverage
// component. The coverage component type is actually stored in the CoverageObject not the
// CoverageRecord.
class CoverageRecord
{
public:
    UINT32 m_TotalCount;            //total # of times this component was covered in all tests.
    map<string, UINT32> m_Tests;    //key=testName, value = count
    CoverageRecord(): m_TotalCount(0) { }
};

//------------------------------------------------------------------------------------------------
// The CoverageObject maintains a map of records that have a common coverage type and component.
// m_Type:      The type of coverage object you are creating. Current valid types are:
//              Unknown (default), Hardware, Software, System, Thermal, VBIOS and Power.
// m_Component: The specific component for this type of coverage managed by this CoverageObject.
//              for example:
//              If m_Type = Hardware, then m_Component could be: hwIsa, hwTexture, etc.
//              If m_Type = Software, then m_Component could be swOpenGLExt, etc.
//
class CoverageObject
{
public:
    virtual RC      AddCoverage(UINT32 subComponent, string testName);
    virtual RC      AddCoverage(const char* subComponent, string testName);
    virtual RC      AllocateResources();
    virtual string  ComponentEnumToString(UINT32 component) = 0;
                    CoverageObject();
                    CoverageObject(UINT32 type, UINT32 comp, const char * szName);
    virtual         ~CoverageObject();
    static void     FreeResources();
    FLOAT64         GetCoverage(const char* testName);
    FLOAT64         GetCoverage(string& testName);
    string          GetName() const { return m_Name; }
    virtual RC      ReportCoverage(FILE *fp,
                                   UINT32 coverageFlags,
                                   string &testName,
                                   JsonItem *pJsItem);

protected:
    bool            m_Initialized;  // If true all empty records have been created
    UINT32          m_Type;
    UINT32          m_Component;
    string          m_Name;

    // map of unique coverage records. Key= ((m_Component << 32) | subComponent)
    map<UINT64, CoverageRecord>     m_Records;

private:
    virtual RC      Initialize() = 0;

    //! Global mutex to serialize calls by multiple threads
    static void*        s_Mutex;
};

//------------------------------------------------------------------------------------------------
// Implement  singleton design pattern.
// Class to maintain a database of the coverage objects. Each CoverageObject must register itself
// with the CoverageDatabase for usage.

class CoverageDatabase
{
public:
    // Register a specific type of coverage object with the database
    void                        FreeCoverageObjects();
    static void                 FreeResources();
    UINT32                      GetCoverageLevel();
    bool                        GetReportCoverageInline();
    RC                          GetCoverageObject(UINT32 type,
                                                  UINT32 component,
                                                  CoverageObject ** ppCov);
    RC                          GetCoverageObject(string &objectName,
                                                  CoverageObject **ppCov);
    string                      GetFilename() const { return m_Filename; }
    static CoverageDatabase *   Instance();
    RC                          ReportCoverage(string &testName);
    RC                          RegisterObject(UINT32 type, UINT32 component,
                                               CoverageObject * pCov);
    RC                          SetCoverageLevel(UINT32 level);
    RC                          SetFilename(const char * filename);
    RC                          SetReportCoverageInline(bool bInline);
private:
    // map of unique coverage records.
    // Key= ((Type << 32) | Component)
    map<UINT64, CoverageObject *> m_Objects;
    string                      m_Filename;
    UINT32                      m_CoverageLevel;
    static CoverageDatabase *   s_Instance;

    // Keep constructor private to prevent instantiation by other code.
                                CoverageDatabase();
                                ~CoverageDatabase();
    // Non-copyable
                                CoverageDatabase(const CoverageDatabase&) { }
                                CoverageDatabase & operator=(const CoverageDatabase*)
                                { return *this; }
   // JsonItem to log global coverage to the mods json log.
   JsonItem *                   m_pJsCoverage = nullptr;
};

}// namespace TestCoverage

#endif //INCLUDED_COVERAGE_H

