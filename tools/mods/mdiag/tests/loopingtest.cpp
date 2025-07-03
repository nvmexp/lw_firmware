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
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "stdtest.h"

#include "loopingtest.h"
#include "mdiag/thread.h"

// A looping test is just a test that does most of the work required for '-forever'
//  You can either give it an 'ArgReader *', which it will query for the presence
//  of '-forever', or just pass an integer saying if forever-looping is desired
LoopingTest::LoopingTest(ArgReader *params) :
  Test()
{
  // our subclass might not have even defined '-forever' - if it doesn't,
  //  it's an error to ask if it's present - it might not even have parsed
  //  any args at all
  if(params && params->ParamDefined("-forever"))
    this->forever_specified = params->ParamPresent("-forever"); else
    this->forever_specified = 0;

  this->pause_flag = 0;
  this->end_flag = 0;
  this->paused_thread = 0;
}

LoopingTest::LoopingTest(int forever_specified) :
  Test()
{
  this->forever_specified = forever_specified;
  this->pause_flag = 0;
  this->end_flag = 0;
  this->paused_thread = 0;
}

// please clean up any mess you make
LoopingTest::~LoopingTest(void)
{
  // nothing to clean up
}

// a test that runs in an endless loop should rely on the three methods below
//  to know when to run and when to be quiet
// NOTE: these methods should NOT touch the HW - they are likely to be called
//  in a different thread from the Run() method - instead they should just set
//  flag(s) that the Run() method will look for
void LoopingTest::EndTest(void)
{
  end_flag = 1;

  // if the thread was asleep, wake it up so it can quit
  if(paused_thread) paused_thread->Awaken();
}

// to be called by the subclass' Run() method - says whether the test should
//  continue looping, and pauses indefinitely if that's what was asked for
int LoopingTest::KeepLooping(void)
{
  // if forever wasn't specified, the answer is easy as well
  if(!forever_specified) return(0);

  // if end_flag is set, kick out immediately
  if(end_flag) return(0);

  // if the pause_flag is set, go to sleep for a while
  if(pause_flag) {
    while(pause_flag) {
      paused_thread = Thread::LwrrentThread();
      pause_flag = 0;
      Thread::Sleep();
      paused_thread = 0;
    }

    // check end_flag once more
    if(end_flag) return(0);
  }

  // got through all the hurdles - ok to loop again
  return(1);
}
