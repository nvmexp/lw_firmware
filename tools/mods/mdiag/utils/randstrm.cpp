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
#include "randstrm.h"

// two different kinds of constructors

// 1. given three 16-bit seeds, start a new random stream
RandomStream::RandomStream(UINT16 seed0, UINT16 seed1, UINT16 seed2)
{
  orig_seeds[0] = seed0;
  orig_seeds[1] = seed1;
  orig_seeds[2] = seed2;

  RestartStream();
}

// 2. (assuming the stream-generating stream has been initialized)
//    a single number creates the Verilog-sim equivalent of stream #n
RandomStream::RandomStream(int stream_num)
{
  stream_generator->RestartStream();

  do {
    orig_seeds[0] = stream_generator->Random(0, 0xffff);
    orig_seeds[1] = stream_generator->Random(0, 0xffff);
    orig_seeds[2] = stream_generator->Random(0, 0xffff);
  } while(--stream_num);

  RestartStream();
}

RandomStream::~RandomStream()
{
  // nothing to clean up
}

RandomStream *RandomStream::stream_generator = 0;

// create the stream-generating stream using three seeds
void RandomStream::CreateStreamGenerator(UINT16 seed0, UINT16 seed1, UINT16 seed2)
{
  if ( stream_generator )  delete stream_generator;
  stream_generator = new RandomStream(seed0, seed1, seed2);
}

void RandomStream::DestroyStreamGenerator()
{
  if ( stream_generator )
    delete stream_generator;
  stream_generator = 0L;
}

// restarts the stream from the original seeds
void RandomStream::RestartStream()
{
  lwrr_seeds[0] = orig_seeds[0];
  lwrr_seeds[1] = orig_seeds[1];
  lwrr_seeds[2] = orig_seeds[2];
}

// gets a SIGNED 32-bit random number
INT32 RandomStream::Random()
{
  return((INT32) RandomUnsigned());
}

// gets a SIGNED 32-bit number, between min and max (inclusive)
INT32 RandomStream::Random(INT32 min, INT32 max)
{
  UINT32 rval;
  UINT32 range;

  rval = RandomUnsigned();
  range = (max - min + 1);

  if(range)
    return((rval % range) + min); else
    return((INT32) rval);
}

// gets an UNSIGNED 32-bit random number
UINT32 RandomStream::RandomUnsigned()
{
  UINT32 new_seed[3];

  /* from jrand48() man page:
     All the routines work by generating  a  sequence  of  48-bit
     integer  values,  X[i], according to the linear congruential
     formula

          X sub (n+1) = (aX sub n + c) sub (mod m)       n>=0.

     The parameter m=2**48; hence 48-bit  integer  arithmetic  is
     performed.   Unless  lcong48()  has been ilwoked, the multi-
     plier value a and the addend value c are given by

           a = 5DEECE66D base 16 = 273673163155 base 8
           c = B base 16 = 13 base 8.

     The value  returned  by  any  of  the  functions  drand48(),
     erand48(),  lrand48(), nrand48(), mrand48(), or jrand48() is
     computed by first generating the next  48-bit  X[i]  in  the
     sequence.  Then the appropriate number of bits, according to
     the type of data item to be returned, are  copied  from  the
     high-order  (leftmost) bits of X[i] and transformed into the
     returned value.
   */

  new_seed[0] = (lwrr_seeds[0] * 0xE66D) + 0xB;
  new_seed[1] = ((lwrr_seeds[0] * 0xDEEC) + (lwrr_seeds[1] * 0xE66D) +
                 (new_seed[0] >> 16));
  new_seed[2] = ((lwrr_seeds[0] * 0x0005) +
                 (lwrr_seeds[1] * 0xDEEC) +
                 (lwrr_seeds[2] * 0xE66D) +
                 (new_seed[1] >> 16));

  lwrr_seeds[0] = new_seed[0] & 0x0000FFFF;
  lwrr_seeds[1] = new_seed[1] & 0x0000FFFF;
  lwrr_seeds[2] = new_seed[2] & 0x0000FFFF;

  return((new_seed[2] << 16) | lwrr_seeds[1]);
}

// gets an UNSIGNED 32-bit number, between min and max (inclusive)
UINT32 RandomStream::RandomUnsigned(UINT32 min, UINT32 max)
{
  UINT32 rval;
  UINT32 range;

  rval = RandomUnsigned();
  range = (max - min + 1);

  if(range)
    return((rval % range) + min); else
    return(rval);
}
