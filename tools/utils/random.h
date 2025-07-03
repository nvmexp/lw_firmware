/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   random.h
 * @brief  Contains the Random class.
 *
 * The Random class contains a random number generator with local context and a
 * variety of random number utilities.
 */

#ifndef INCLUDED_RANDOM_H
#define INCLUDED_RANDOM_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#include <cstddef>

//! The Random class.
class Random
{
public:

   //! The constructor leaves the RNG uninitialized.
   //! Note: The old RNG actually initialized itself to some sane
   //! state and some MODS tests relied on this behavior,
   //! but we don't support this anymore.
   Random() = default;

   //! Set the random seed.
   void SeedRandom(UINT32 Seed);
   void SeedRandom(UINT32* pSeedArray, size_t arraySize);
   void SeedRandom(UINT64 seedA, UINT64 seedB, UINT64 seedC, UINT64 seedD);

   //! Get random UINT32 between 0 and 0xFFFFFFFF, inclusive.  Very fast.
   UINT32 GetRandom();

   //! Get random UINT32 between Min and Max, inclusive.  Slower.
   UINT32 GetRandom(UINT32 Min, UINT32 Max);

   //! Get random UINT64
   UINT64 GetRandom64();

   //! Get random UINT64 between Min and Max, inclusive.
   UINT64 GetRandom64(UINT64 Min, UINT64 Max);

   //! Get random single-precision float between Min and Max, inclusive.
   //! Returns one of 2*32 float values evenly spaced across the range.
   //! About as fast as GetRandom(UINT32,UINT32).
   float GetRandomFloat(double Min, double Max);

   //! Get random double between Min and Max, inclusive.
   //! Returns one of 2*64 double values evenly spaced across the range.
   //! Twice as slow as GetRandomFloat().
   double GetRandomDouble(double Min, double Max);

   //! Get float with a random mantissa and sign, with the binary exponent
   //! clamped to a range.  Useful to prevent overflow/underflow.
   //! If maxPow2 is 128 or greater, NAN and INF values may be returned.
   float GetRandomFloatExp(INT32 minPow2, INT32 maxPow2);

   //! Get double with a random mantissa and sign, with the binary exponent
   //! clamped to a range.  Useful to prevent overflow/underflow.
   //! If maxPow2 is 1024 or greater, NAN and INF values may be returned.
   double GetRandomDoubleExp(INT32 minPow2, INT32 maxPow2);

   //! Shuffle the deck in place.
   //! If numSwaps == 0, goes through the deck exactly once (decksize-1 swaps).
   //! If (0 < numSwaps < deckSize-1), only the first numSwaps items are guaranteed
   //! to be random.
   void Shuffle(UINT32 deckSize, UINT32* deck, UINT32 numSwaps = 0);
   void Shuffle(UINT32 deckSize, UINT64* deck, UINT32 numSwaps = 0);
   void Shuffle(UINT32 deckSize, void** deck, UINT32 numSwaps = 0);

   //! Get a normally-distrubuted random float.
   //! Mean and mode will be 0.
   //! Standard deviation will be 1.0, so 68% likely between -1 and 1.
   float GetNormalFloat();

   //! Holds one of N items with weighted likelyhood to be picked.
   struct PickItem
   {
      UINT32   RelProb;          //!< probability, relative to other items
      UINT32   Min;              //!< Min value
      UINT32   Max;              //!< Max value

      UINT32   ScaledProb;       //!< probability, aclwmulative, scaled to 2^32
                                 //!< Filled in by PreparePickItems()...don't call
                                 //!< Pick() until this has been filled in!

      double   ScaledRange;      //!< Select within range: (max-min)/(2^32)
                                 //!< Filled in by PreparePickItems()...don't call
                                 //!< Pick() until this has been filled in!
   };

   //! Get a random int with specified range and weighted probability.
   /**
    * Example:
     @verbatim

       static Random::PickItem SickDayPicker[] =
          {
             PI_ONE( 3, MONDAY ),             // 30% likely monday
             PI_ONE( 2, FRIDAY ),             // 20% likely friday
             PI_RANGE(3, TUESDAY, THURSDAY)   // 10% each other weekday
          };
       STATIC_PI_PREP(SickDayPicker);

       UINT32 DayToBeSick = DefaultRandom::Pick( &SickDayPicker[0] );

     @endverbatim
    */
   UINT32   Pick(const PickItem * Items);

   //! Callwlate and fill in the ScaledProb and ScaledRange members,
   //! so that the Pick() function can run faster.
   static void PreparePickItems(INT32 NumItems, PickItem * pItems);

   class PickPrepper
   {
   public:
      PickPrepper(UINT32 numI, PickItem * pI)
      {
         Random::PreparePickItems(numI, pI);
      }
   };

   //
   // Here are some handy macros for declaring static PickItem arrays.
   // These MUST BE STATIC -- not local function variables or class members.
   //
   #define PI_ONE(prob, val)         {(prob), (val), (val), 0, 0.0}
   #define PI_RANGE(prob, min, max)  {(prob), (min), (max), 0, 0.0}

   #define STATIC_PI_PREP(name) static Random::PickPrepper p##name(sizeof(name)/sizeof(Random::PickItem), &name[0])

   void Enable(bool bEnable) {m_Enabled = bEnable;}

   struct PCG32State
   {
       UINT64 state;
       UINT64 stream;
   };

private:

   // Random number generator's state.
   // The RNG must always be seeded with SeedRandom().
   // Unfortunately many places in MODS leave it unseeded,
   // so we have to initialzie the state to some sane value.
   // The initial state is arbitrary, I read it from /dev/urandom.
   PCG32State m_State[2] =
   {
       { 0x5974'BC49'6545'D865ULL, 0x5E5E'38A6'A2BD'8AEBULL },
       { 0x2D67'6DD9'A68A'91C0ULL, 0x7D01'3C5F'901E'116BULL }
   };

   bool   m_Enabled         = true;
   bool   m_Initialized     = false;
   bool   m_HaveSavedNormal = false;
   float  m_SavedNormal     = 0;

////////////////////////////////////////////////////////////////////////////////
// TODO remove all stuff below (old RNG implementation)
//
// Note: At the moment of this writing, the new RNG exposes pre-existing bugs
// in some tests, causing CRC errors or crashes, so we can't easily remove
// the old RNG code yet.  Once these tests are fixed, we can use the new RNG
// for them, but fixing them will be a matter of commitment, since some
// failing tests include legacy display or video engine tests, which are
// not actively maintained.
////////////////////////////////////////////////////////////////////////////////

   bool   m_UseOld = false;
   static constexpr int    RND_DEG = 31;
   static constexpr UINT32 RND_SEP = 3;
   UINT32 m_LegacyState[RND_DEG] =
   {
      0x9a319039U, 0x32d9c024U, 0x9b663182U, 0x5da1f342U,
      0xde3b81e0U, 0xdf0a6fb5U, 0xf103bc02U, 0x48f340fbU,
      0x7449e56bU, 0xbeb1dbb0U, 0xab5c5918U, 0x946554fdU,
      0x8c2e680fU, 0xeb3d799fU, 0xb11ee0b7U, 0x2d436b86U,
      0xda672e2aU, 0x1588ca88U, 0xe369735dU, 0x904f35f7U,
      0xd7158fd6U, 0x6fa6f051U, 0x616e6b96U, 0xac94efdlw,
      0x36413f93U, 0xc622c298U, 0xf5a42ab8U, 0x8a88d77bU,
      0xf5ad9d0eU, 0x8999220bU, 0x27fb47b9U
   };
   UINT32 m_Front = RND_SEP;
   UINT32 m_Rear  = 0;

   struct Old;
   friend struct Old;

public:
   //! Force legacy behavior
   void UseOld() { m_UseOld = true; }

////////////////////////////////////////////////////////////////////////////////
// TODO remove up to here
////////////////////////////////////////////////////////////////////////////////
};

//! The DefaultRandom wrapper class.
//! Some legacy tests still use it, but it is not thread-safe and must be avoided.
class DefaultRandom
{
public:
   typedef Random::PickItem PickItem;

   inline static void SeedRandom(UINT32 Seed)
   { s_Instance.SeedRandom(Seed); };

   inline static UINT32 GetRandom()
   { return s_Instance.GetRandom(); }

   inline static UINT32 GetRandom(UINT32 Min, UINT32 Max)
   { return s_Instance.GetRandom(Min, Max); }

   inline static float GetRandomFloat(double Min, double Max)
   { return s_Instance.GetRandomFloat(Min, Max); }

   inline static double GetRandomDouble(double Min, double Max)
   { return s_Instance.GetRandomDouble(Min, Max); }

   inline static void Shuffle(UINT32 deckSize, UINT32* deck, UINT32 numSwaps = 0)
   { s_Instance.Shuffle(deckSize, deck, numSwaps); };

   inline static UINT32 Pick(const Random::PickItem* pItems)
   { return s_Instance.Pick(pItems); };

private:
   static Random s_Instance;
};

#endif // INCLUDED_RANDOM_H

