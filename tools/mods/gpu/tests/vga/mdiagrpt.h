/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef __MDIAG_REPORT_H__
#define __MDIAG_REPORT_H__

#include "mdiag/tests/test.h"
#include <string>

class TestStateReport;
class Trep;
class TestReport : public TestStateReport, public Trep {
    string s_TestName;

    Test::TestStatus status;

    int genTrepReport ();

    static TestReport* test_report;

public:
    TestReport();
    ~TestReport();

    void init(const char* testname);

    Test::TestStatus GetStatus() {return status; };
    void SetStatus(Test::TestStatus new_status) { status = new_status; }

    int genReport();

    // during a run, or potentially during cleanup
    void runFailed(const char *report = 0);                   // NOO
    void runInterrupted();                                    // NOO  +DNC
    void runSucceeded(bool override=false);                   // YES
    void crcCheckPassed();                                    // YES
    void crcCheckPassedGold();                                // GOLD
    void crlwsingOldVersion();                                //      +MIS
    void crcNoFileFound();                                    // MIS  +CRC
    void goldCrcMissing();                                    //      +NOG (if also CRC)
    void crcCheckFailed(ICrcProfile *crc);                    // NOO  +CRC
    void crcCheckFailed(const char *report = 0);              // NOO  +CRC
    void error(const char *report = 0);                       // NOO  +ERR

    static TestReport*& GetTestReport() {return test_report;};

};

#endif
