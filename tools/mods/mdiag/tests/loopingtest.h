/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _LOOPINGTEST_H_
#define _LOOPINGTEST_H_

#include "test.h"

// To make your test understand '-forever':
//
// 1) Subclass from LoopingTest instead of Test.
// 2) Add FOREVER_PARAM to your ParamDecl list.
// 3) Encase the meat of your Run() method inside:
//
//      do {
//        // your code here
//      } while(KeepLooping());

class ArgReader;
class Thread;

class LoopingTest : public Test {
 public:
  // A looping test is just a test that does most of the work required for '-forever'
  //  You can either give it an 'ArgReader *', which it will query for the presence
  //  of '-forever', or just pass an integer saying if forever-looping is desired
  LoopingTest(ArgReader *params);
  LoopingTest(int forever_specified);

  // please clean up any mess you make
  virtual ~LoopingTest(void);

  // factory function : Takes a pointer to an arg database for command-line args
#ifdef DEFINE_YOUR_OWN_FACTORY_FUNCTION
  static Test *FactoryFunction(ArgDatabase *args);
#endif

  // Still no Run() method defined - you must subclass from LoopingTest
  // virtual void Run(void) = 0;

  // a test that runs in an endless loop should rely on the three methods below
  //  to know when to run and when to be quiet
  // NOTE: these methods should NOT touch the HW - they are likely to be called
  //  in a different thread from the Run() method - instead they should just set
  //  flag(s) that the Run() method will look for
  virtual void EndTest(void);

 protected:
  // to be called by the subclass' Run() method - says whether the test should
  //  continue looping, and pauses indefinitely if that's what was asked for
  int KeepLooping(void);

  int forever_specified;
  int pause_flag;
  int end_flag;
  Thread *paused_thread;
};

#endif
