/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TEST_STATE_H_
#define _TEST_STATE_H_

#include "mdiag/utils/crc.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utils/profile.h"
#include "core/include/cmdline.h"
#include "mdiag/utils/CIface.h"
#include "mdiag/utils/ITestStateReport.h"
#include "mdiag/utils/ICrcProfile.h"
#include <string>
#include <map>
#include <stdio.h>

// XXX This doesn't belong here.
#if !defined(MAX_FILENAME_SIZE)
#define MAX_FILENAME_SIZE 256
#endif

class TestStateReport : public CIfaceObject, public ITestStateReport
{
public:
  TestStateReport();
  ~TestStateReport();

  BEGIN_BASE_INTERFACE
  ADD_QUERY_INTERFACE_METHOD(IID_CHIP_IFACE, ITestStateReport)
  ADD_QUERY_INTERFACE_METHOD(IID_TESTREPORT_IFACE, ITestStateReport)
  END_BASE_INTERFACE

  // this violates encapsulation a bit, but
  void init(const char *testname);                          // DNR

  // during setup
  void setupFailed(const char *report);                     //      +ERR

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

  bool isStatusGold(void) { return m_lwrState == GOLD; }

  void disable() { m_enabled = false;  }
  void enable()  { m_enabled = true;  }

  string create_status_filename(const char *ext);

  static void createNooFileIfNeeded(const char *outputName);
  static void setGlobalOutputName(const char *outputName);
  static void globalError(const char *errString);

  // this is here as a null-pointer-avoidance technique if allocation failed
  // in Test... though that would be the least of the code's worries, if so:
  // it's more importantly here because code might one day be modified to use
  // this instead of (in addition to) the per-test state reporters.
  static TestStateReport s_defaultStateReport;

  friend class TestDirectory;
public:
  // enum hack; MSDEV can't handle constants being declared inside classes,
  // so I'm using an enum of the constants I want
  enum { NUM_STATES = 5, NUM_SUBSTATES = 5, EXTLEN = 5  };
protected:
  // toplevel state (mutually exclusive)
  enum State {
    // legacy hack: MIS and GOLD are 'toplevel' states even though they
    // shouldn't be.  This needs to change someday.
    UNDEF = -1, DNR = 0, NOO = 1, MIS = 2, YES = 3, GOLD = 4
  };
  static const char state_ext[NUM_STATES][EXTLEN];
  State m_lwrState;             // current state
  State m_newState;             // deferred state change
  bool m_stateLocked;
  bool m_initialized;
  bool m_enabled;
  // to be portable, we avoid doing this the obvious way with bits
  enum SubState {
    SUB_DNC = 0, SUB_ERR = 1, SUB_CRC = 2, SUB_MIS = 3, SUB_MIS_GOLD = 4
  };
  static const char substate_ext[NUM_SUBSTATES][EXTLEN];
  typedef short SubStateSet[NUM_SUBSTATES];

  SubStateSet m_lwrSubState;  // state
  SubStateSet m_newSubState;  // +1 to add, -1 to remove
  string m_error;  // subsequent error messages append to this string
  ICrcProfile *m_crcDump;
  string m_crcInfo;  // subsequent error messages append to this string
  string m_dncState;

  string m_basefile;
  int m_testCounter;

  void create(const char *ext);
  void create(const char *ext, const string &contents);
  void create(const char *ext, const char *contents) { create(ext, string(contents)); }
  void create(const char *ext, ICrcProfile *profile);
  void remove(const char *ext, bool silent = false);
  void writeStates();
  void appendToErrorString(const char *report);

  // The following two declarations are private and unimplemented;
  // it is unsupported to copy or assign a TestStateReport
  TestStateReport(const TestStateReport &rhs);
  const TestStateReport &operator =(const TestStateReport &rhs);

public:
// These private functions are called by our friend TestDirectory
  void beforeSetup();                                       // NOO  +DNC
  void afterSetup();                                        //           flush
  void beforeRun();                                         //      mDNC
  void afterRun();                                          //           flush
  void beforeCleanup();                                     //      mDNC
  void afterCleanup();                                      //      -DNC flush
  static string create_status_filename(const char *ext, const string &basefile);

protected:
  static string s_globalName;
  static map<string,int> s_namesInUse;
  static map<string,int> s_testIdNumber;
  static int s_maxCount;
  static int s_testCounter;
  // static string stripPath(const string &fullpath);
};

#endif
