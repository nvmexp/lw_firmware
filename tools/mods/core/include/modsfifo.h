/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//-----------------------------------------------------------------------------

/**
 * @file   modsfifo.h
 * @brief  A simple, non-malloc'ing fifo alternative to the C++ STL classes
 *
 * This file contains:
 *   - A template class for a simple Fifo, optimized for higher performance
 *     than the STL libraries, and can be called from ISR's without fear
 *     of hosing up non-reentrant malloc's
 *
 *   - Cons: Don't overfill it our you will lose data since it is
 *     implemented as a ring buffer. You still need to disable interrupts
 *     if you will be accessing it from an ISR. Maybe later I will rewrite it
 *     to be ISR safe.
 */

#include "massert.h"

template <class _T>
class mods_fifo
{
private:
   _T * ring;
   unsigned int get;
   unsigned int put;
   bool empty;
   unsigned int size;
   unsigned int fullity;

public:
   mods_fifo(size_t s)
   {
      // Allocate a buffer to store entries.
      ring = new _T[s];
      size = s;
      get = 0;
      put = 0;
      empty = true;
      fullity = 0;
   }

   ~mods_fifo() { delete ring; }

   // Add an entry.
   void Push(_T t)
   {
      // If we are full just drop the data.
      if ((put == get) && !empty)
      {
         MASSERT(0);
         return;
      }

      ring[put] = t;
      put++;
      if (put == size)
         put = 0;
      empty = false;
      fullity++;
   }

   // Return the first entry and delete it.
   _T Pop()
   {
      unsigned int index = get;

      // If we are empty, oh well you don't get anything
      // Better check for IsEmpty() or you get old data.
      if ((put == get) && empty)
      {
         char * nully = (char *)0xffffffff;
         nully[0] = 0xff;
         MASSERT(0);
         // Unfortunately we have to return something
         return ring[index];
      }
      get++;
      if (get == size)
         get = 0;
      if (get == put)
         empty = true;
      fullity --;
      return ring[index];
   }

   unsigned int IsEmpty() { return empty; }

   // Return the number of filled entries, not hte maximum size.
   unsigned int Size() { return fullity; }
};

