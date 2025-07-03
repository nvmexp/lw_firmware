/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RANDSTRM_H_
#define _RANDSTRM_H_

// support for multiple random number streams

#include "types.h"

class RandomStream {
 public:
  // two different kinds of constructors

  // 1. given three 16-bit seeds, start a new random stream
  RandomStream(UINT16 seed0, UINT16 seed1, UINT16 seed2);

  // 2. (assuming the stream-generating stream has been initialized)
  //    a single number creates the Verilog-sim equivalent of stream #n
  RandomStream(int stream_num);

  ~RandomStream();

  // create the stream-generating stream using three seeds
  static void CreateStreamGenerator(UINT16 seed0, UINT16 seed1, UINT16 seed2);
  static void DestroyStreamGenerator();

  // restarts the stream from the original seeds
  void RestartStream();

  // gets a SIGNED 32-bit random number
  INT32 Random();

  // gets a SIGNED 32-bit number, between min and max (inclusive)
  INT32 Random(INT32 min, INT32 max);

  // gets an UNSIGNED 32-bit random number
  UINT32 RandomUnsigned();

  // gets an UNSIGNED 32-bit number, between min and max (inclusive)
  UINT32 RandomUnsigned(UINT32 min, UINT32 max);

 protected:
  static RandomStream *stream_generator;
  UINT16 lwrr_seeds[3], orig_seeds[3];
};

#endif
