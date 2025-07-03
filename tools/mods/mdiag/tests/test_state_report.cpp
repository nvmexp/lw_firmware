/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2010,2015,2019,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _WIN32
#include <unistd.h>
#endif

#include <errno.h>
#include "test_state_report.h"
#include "mdiag/utils/utils.h"
#include "mdiag/sysspec.h"

// This is much less clean than it could have been, because the
// multiple-use nature of the files means we have some very odd
// states possible.  A cleanup of this file in conjunction with
// changes to analyze.pl is encouraged.

// If no CRC file is found, the result is supposed to be
// MIS and CRC files, but /no/ NOO file; this is the only case
// where there is not a DNR, NOO, YES, or GOLD output.

// For the future, the MIS and GOLD toplevel states really should
// go away -- for that matter, there's little reason to have so many
// files around anymore, since this cleanly writes all state at the
// end of each test.

const char TestStateReport::state_ext[NUM_STATES][EXTLEN] = {
  "dnr", "noo", "mis", "yes", "gold"
};

const char TestStateReport::substate_ext[NUM_SUBSTATES][EXTLEN] = {
  "dnc", "err", "crc", "mis", "msg"
};

// extern int test_id_being_setup;

string TestStateReport::s_globalName = "";
map<string,int> TestStateReport::s_namesInUse;
map<string,int> TestStateReport::s_testIdNumber;
int TestStateReport::s_testCounter = 0;
int TestStateReport::s_maxCount    = 0;
TestStateReport TestStateReport::s_defaultStateReport;

void TestStateReport::setGlobalOutputName(const char *outputName)
{
    s_globalName = UtilStrTranslate(outputName, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);
}

void TestStateReport::globalError(const char *errString) {
  if (s_globalName == "") {
    s_globalName = "test";
  }
  string file = create_status_filename("err", s_globalName);
  unlink(file.c_str());
  DebugPrintf("TestStateReport::Opening file '%s' in mode 'w'\n", file.c_str());
  if (FILE *f = fopen(file.c_str(), "a")) {
    fputs(errString, f);
    fclose(f);
  } else {
    InfoPrintf("can't open file: %s\n", file.c_str());
  }
}

TestStateReport::TestStateReport() {
  m_enabled = false;
  m_initialized = false;
  m_newState = m_lwrState = NOO;
  for (int j=0; j<NUM_SUBSTATES; j++)
  {
    m_lwrSubState[j] = m_newSubState[j] = 0;
  }
  // initialize rest of variables
  m_error = "";
  m_crcDump = 0;
  m_crcInfo = "";
  m_dncState = "";
  m_basefile = "test";
  m_testCounter = s_testCounter++;
}

//this is called after all tests were set up (and finished), so we can
//clean anything at once
TestStateReport::~TestStateReport()
{
    s_namesInUse.clear();
    s_testIdNumber.clear();
}

void TestStateReport::writeStates() {
  if (!m_enabled)  return;
  if (m_lwrState != m_newState)
  {
    create(state_ext[m_newState]);
    remove(state_ext[m_lwrState]);
    m_lwrState = m_newState;
  }

  if (m_newSubState[SUB_DNC] == 1) {
    create(substate_ext[SUB_DNC], m_dncState);
    m_lwrSubState[SUB_DNC] = 1;
    m_dncState = "";
  }
  if (m_newSubState[SUB_ERR] == 1) {
    create(substate_ext[SUB_ERR], m_error);
    m_lwrSubState[SUB_ERR] = 1;
    // leave m_error alone; we only append to this string
  }
  if (m_newSubState[SUB_CRC] == 1) {
    if (m_crcDump)
    {
      create(substate_ext[SUB_CRC], m_crcDump);
    }
    else
    {
      create(substate_ext[SUB_CRC], m_crcInfo);
    }
    m_lwrSubState[SUB_CRC] = 1;
    m_crcDump = 0;
    m_crcInfo = "";
  }
  if (m_newSubState[SUB_MIS] == 1) {
    create(substate_ext[SUB_MIS]);
    m_lwrSubState[SUB_MIS] = 1;
  }
  if ( m_newSubState[SUB_MIS_GOLD] == 1 && m_lwrSubState[SUB_CRC] == 1) {
    create(substate_ext[SUB_MIS_GOLD]);
    m_lwrSubState[SUB_MIS_GOLD] = 1;
  }

  for (int i=0; i<NUM_SUBSTATES; i++)
  {
    if (m_newSubState[i] == -1) {
      remove(substate_ext[i]);
      m_lwrSubState[i] = 0;
    }
    m_newSubState[i] = 0;
  }
}

// This can be called twice. This is not an error,
// though it could be cleaner.

// It is called first as soon as we know the name of the test (e.g. trace_cels);
// it is called again with the -o parameter once that's parsed.

void TestStateReport::init(const char *testname)
{
  m_stateLocked = false;
  map<string,int>::iterator i;

  DebugPrintf("TestStateReport: initializing to name %s\n", testname);
  DebugPrintf("TestStateReport: m_basefile %s\n", m_basefile.c_str());
  DebugPrintf("TestStateReport: m_testCounter=%d\n",m_testCounter);
  DebugPrintf("TestStateReport: s_maxCount=%d\n",s_maxCount);

  string newname = "test";
  if (testname && *testname)
  {
    newname = testname;
  }
  else if (s_globalName != "")
  {
    newname = s_globalName;
  }
  else if (m_basefile != "")
  {
    newname = m_basefile;
  }

  //This function was never intended to work with multiple traces, so it
  //gets really confusing. Best to explain using example:
  //For a test running multiple traces say (trace_3d, v2d, trace_3d, v2d)
  //this function will be called 10 times (no kidding) with
  //m_testCount = 1,2,3,4,1,1,2,3,3,4
  //First four call will assign names of traces, we do not want to change them
  //so we do nothing since (s_maxCount < m_testCounter) is true.
  //Next six calls assign output names. We keep track of output names and
  //their frequency using s_namesInUse map and match output names with
  //corresponding test numbers using s_testIdNumber map.
  //If output name in shell command is used more than once, it is changed by
  //adding _2, _3, etc. at the end and modified name is added to map.
  //
  //An additional complication is that some tests, like trace_3d, call this
  //function three times, once to set trace name and twice to get output name.
  //An extra call for the same test number should not change output name,
  //thus s_testIdNumber map is used to check if output name was already assigned.

    if(s_maxCount < m_testCounter)
    {
        s_maxCount=m_testCounter;
    }
    else
    {
        if(s_testIdNumber[m_basefile] !=m_testCounter ) {s_namesInUse[newname]++;}

        DebugPrintf("namesInUse map:    name    count\n");
        for (i=s_namesInUse.begin();i != s_namesInUse.end();i++)
            DebugPrintf("      %s % d\n",(i->first).c_str(),i->second);

        if ( s_namesInUse[newname] > 1)
        {
            if(s_testIdNumber[m_basefile] ==m_testCounter )
            {
                newname=m_basefile;
            }
            else
            {
                char buf[8];
                sprintf(buf,"_%d", s_namesInUse[newname]);
                newname+=buf;
                s_namesInUse[newname]++;
                s_testIdNumber[newname]=m_testCounter;
            }
            WarnPrintf("TestStateReport::name collision!!! Renamed test output files to %s\n", newname.c_str());
        }
        s_testIdNumber[newname]=m_testCounter;
    }
  if (m_enabled) {
    remove(state_ext[m_lwrState]);
  }
  m_basefile = newname;

  if (m_enabled) {
    // remove any evidence of a successful run
    remove(state_ext[MIS]);
    remove(state_ext[YES]);
    remove(state_ext[GOLD]);
    if (m_lwrState != DNR)
    {
      create(state_ext[m_lwrState]);
    }
  }
  m_initialized = true;
}

// Some tests only init() *during*
// Setup, and therefore would write initial files with the wrong name.

void TestStateReport::beforeSetup()
{
  m_newState = NOO;
}

void TestStateReport::afterSetup()
{
  m_newSubState[SUB_DNC] = 1;
  writeStates();
}

void TestStateReport::beforeRun()
{
  m_dncState = "run";
  m_newSubState[SUB_DNC] = 1;
  writeStates();
}

void TestStateReport::afterRun()
{
  writeStates();
}

void TestStateReport::beforeCleanup()
{
  m_dncState = "cleanup";
  m_newSubState[SUB_DNC] = 1;
  writeStates();
}

void TestStateReport::afterCleanup()
{
  m_newSubState[SUB_DNC] = -1;
  writeStates();
}

// all these functions are called within Test and must not do any file IO
void TestStateReport::setupFailed(const char *report)
{
  if (report == 0)  report = "";
  DebugPrintf("TestStateReport::setupFailed(%s)\n", report);
  m_newSubState[SUB_ERR] = 1;
  appendToErrorString(report);
}

void TestStateReport::runFailed(const char *report)
{
  if (report == 0)  report = "";
  DebugPrintf("TestStateReport::runFailed(%s)\n", report);
  m_newState = NOO;  // this should be redundant
  m_newSubState[SUB_ERR] = 1;
  m_stateLocked = true;   // can't later claim success without an override
  appendToErrorString(report);
}

void TestStateReport::runInterrupted()
{
  DebugPrintf("TestStateReport::runInterrupted()\n");
  m_newState = NOO;
}

void TestStateReport::runSucceeded(bool override)
{
  DebugPrintf("TestStateReport::runSucceeded(%d)\n", (int)override);
  if (m_stateLocked && !override)
  {
    WarnPrintf("Tried to claim success when a failure was already set!\n");
    error("Tried to claim success when a failure was already set!\n");
  }
  else
  {
    m_newState = YES;
    m_stateLocked = false;
  }
}

void TestStateReport::crcCheckPassed()
{
  DebugPrintf("TestStateReport::crcCheckPassed()\n");
  if (m_stateLocked)
  {
    WarnPrintf("Tried to claim success when a failure was already set!\n");
    error("Tried to claim success when a failure was already set!\n");
  }
  else
  {
      // YES state only can override SUB_MIS_GOLD and GOLD state
      if (m_newState == GOLD)
      {
          m_newState = YES;
      }
      else if (m_newState == NOO)
      {
          for (int i = 0; i < NUM_SUBSTATES; i++)
          {
              // Substate except SUB_MIS_GOLD can't be overriden
              if ((i != SUB_MIS_GOLD) && (m_newSubState[i] != 0))
              {
                  return;
              }
          }

          m_newState = YES;
      }
  }
}

void TestStateReport::crcCheckPassedGold()
{
  DebugPrintf("TestStateReport::crcCheckPassedGold()\n");
  if (m_stateLocked)
  {
    WarnPrintf("Tried to claim success when a failure was already set!\n");
    error("Tried to claim success when a failure was already set!\n");
  }
  else
  {
      // GOLD state only can override empty state
      if (m_newState == NOO)
      {
          for (int i = 0; i < NUM_SUBSTATES; i++)
          {
              if (m_newSubState[i] != 0)
              {
                  return;
              }
          }

          m_newState = GOLD;
      }
  }
}

void TestStateReport::crlwsingOldVersion()
{
  DebugPrintf("TestStateReport::crlwsingOldVersion()\n");
  if (m_newState != MIS)
  {
    // the MIS subfile indicates that the crc is using an old version.
    m_newSubState[SUB_MIS] = 1;
  }
}

void TestStateReport::crcNoFileFound()
{
  DebugPrintf("TestStateReport::crcNoFileFound()\n");
  // error("No CRC file found!");
  // legacy: this is CRC + MIS, and /not/ NOO
  // one day MIS will not be a legal toplevel state
  m_newState = MIS;
  // hack: MIS can be both a state and a substate, so we make sure
  // there's no attempt to change it here
  m_newSubState[SUB_MIS] = 0;
  m_newSubState[SUB_CRC] = 1;
  m_stateLocked = true;
}

void TestStateReport::goldCrcMissing()
{
  DebugPrintf("TestStateReport::goldCrcMissing()\n");
  m_newSubState[SUB_MIS_GOLD] = 1;
}

void TestStateReport::crcCheckFailed(ICrcProfile *crc)
{
  DebugPrintf("TestStateReport::crcCheckFailed(CRC)\n");
  m_newSubState[SUB_CRC] = 1;
  m_crcDump = crc;
  if (m_newState != MIS)
  {
    m_newState = NOO;
  }
  m_stateLocked = true;
}

void TestStateReport::crcCheckFailed(const char *report)
{
  if (report == 0)  report = "";
  DebugPrintf("TestStateReport::crcCheckFailed(%s)\n", report);
  m_newSubState[SUB_CRC] = 1;
  m_crcDump = 0;
  m_crcInfo += string(report);  // multiple calls can append to this string
  if (m_newState != MIS)
  {
    m_newState = NOO;
  }
  m_stateLocked = true;
}

void TestStateReport::error(const char *report)
{
  if (report == 0)  report = "";
  DebugPrintf("TestStateReport::error(%s)\n", report);
  m_newSubState[SUB_ERR] = 1;
//  if (m_newState != MIS)
//  {
//    m_newState = NOO;  // remove YES or GOLD if previously set
//  }
//  m_stateLocked = true;
  appendToErrorString(report);
}

void TestStateReport::appendToErrorString(const char *report)
{
  string sreport(report);
  if ((sreport.length() == 0) || (m_error.length() > 100000))  return;
  if (sreport.length() > 100000)
  {
    m_error += sreport.substr(0, 100000);
  }
  else
  {
    m_error += sreport;
  }
  if (m_error.length() > 100000)
  {
    m_error = m_error.substr(0, 100000) + "\n\nError report truncated for length.\n";
  }
}

// stolen directly from existing code in TraceFile
void TestStateReport::create(const char *ext)
{
  string file = create_status_filename(ext);
  DebugPrintf("TestStateReport::Opening file '%s' in mode 'w'\n", file.c_str());
  if (FILE *f = fopen(file.c_str(), "w")) {
    fclose(f);
  } else {
    InfoPrintf("can't open file: %s\n", file.c_str());
  }
}

void TestStateReport::create(const char *ext, const string &contents)
{
  string file = create_status_filename(ext);
  DebugPrintf("TestStateReport::Writing data to file '%s' in mode 'w'\n", file.c_str());
  if (FILE *f = fopen(file.c_str(), "w")) {
    if (1 != fwrite(contents.c_str(), contents.length(), 1, f))
    {
        InfoPrintf("can't write file: %s\n", file.c_str());
    }
    fclose(f);
  } else {
    InfoPrintf("can't open file: %s\n", file.c_str());
  }
}

void TestStateReport::create(const char *ext, ICrcProfile *profile)
{
  string file = create_status_filename(ext);
  DebugPrintf("TestStateReport::Writing crc profile to file '%s' in mode 'w'\n", file.c_str());
  profile->SaveToFile(file.c_str());
}

void TestStateReport::remove(const char *ext, bool silent)
{
  string file = create_status_filename(ext);
  if (!silent)  DebugPrintf("TestStateReport::Deleting file '%s'\n", file.c_str());
  if ((unlink(file.c_str()) == -1) && (errno != ENOENT)) {
    ErrPrintf( "Unable to unlink file %s!\n", file.c_str());
  }
}

string TestStateReport::create_status_filename(const char *ext) {
  return create_status_filename(ext, m_basefile);
}

// private static functions, implementations that don't depend on any member data

string TestStateReport::create_status_filename(const char *ext, const string &basefile)
{
  string tempstr = basefile;

  // Look for an extension that matches any of the ones in the state_ext or
  // substate_ext string arrays, and remove it from the base name.
  int i;
  for (i = 0; i < NUM_STATES + NUM_SUBSTATES; i++)
  {
    string tmpExt = ".";
    if (i < NUM_STATES)
      tmpExt = tmpExt + state_ext[i];
    else
      tmpExt = tmpExt + substate_ext[i-NUM_STATES];

    if (string::npos != tempstr.find(tmpExt, tempstr.size() - tmpExt.size()))
    {
      tempstr.erase(tempstr.size() - tmpExt.size(), tmpExt.size());
      break;
    }
  }

  // Add a "." only if the extension hasn't already got one.
  string dot = (*ext == '.') ? "" : ".";

  return tempstr + dot + ext;
}

/*
// This may turn out to be necessary.  It was inserted during testing to try to
// accomodate WINE submissions of simultaneous tests, but testgen/WINE has other
// problems, so this doesn't at this moment solve anything.  testgen will have to
// be modified, so it will likely remove the path-naming inconsistency that led to
// this routine being necessary.  Placed in source, commented out, in case it turns
// out to be needed later, once testgen is fixed.

string TestStateReport::stripPath(const string &fullpath) {
  // we could use an algorithm for this but it's just as easy to do it this way...
  const char *buf = fullpath.c_str();
  const char *start = buf;
  while (*buf) {
    if ((*buf == DIR_SEPARATOR_CHAR) || (*buf == DIR_SEPARATOR_CHAR2)) {
      start = buf+1;
    }
    ++buf;
  }
  return string(start);
}
*/

// This performs creation of the .noo file only if a
// test has not already been set up with this name.  Not an elegant solution but
// a working one.

void TestStateReport::createNooFileIfNeeded(const char *outputName) {
  string oname(outputName);
  if (s_namesInUse.find(oname) == s_namesInUse.end()) {
    if (FILE *f = fopen(create_status_filename(state_ext[NOO], oname).c_str(), "w")) {
      fclose(f);
    }
  }
}

