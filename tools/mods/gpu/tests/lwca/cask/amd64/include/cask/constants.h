#ifndef INCLUDE_GUARD_CASK_CONSTANTS_H
#define INCLUDE_GUARD_CASK_CONSTANTS_H

#include "cask.h"

namespace cask {
namespace constants {

/// Returns 1
void const* One(ScalarType id);

/// Returns 0
void const* Zero(ScalarType id);

/// Returns infinity
void const* Infinity(ScalarType id);

/// Returns negative infinity
void const* NegativeInfinity(ScalarType id);

/// Returns maximum representable value
void const* Maximum(ScalarType id);

/// Returns minimum representable value
void const* Minimum(ScalarType id);

/// Returns nan or maximum integer for non floating point types
void const* NaN(ScalarType id);

/// Returns 2
void const* Two(ScalarType id);

/// Returns pi
void const* Pi(ScalarType id);

/// Returns two pi
void const* TwoPi(ScalarType id);

/// Returns 1/2 pi
void const* HalfPi(ScalarType id);

/// Returns square root of pi
void const* RootPi(ScalarType id);

/// Returns the square root of half of pi
void const* RootHalfPi(ScalarType id);

/// Returns the square root of two pi
void const* RootTwoPi(ScalarType id);

/// Returns the square root of natural log 4
void const* RootLnFour(ScalarType id);

/// Returns euler's number (e)
void const* E(ScalarType id);

/// Returns 1/2
void const* Half(ScalarType id);

/// Returns square root 2
void const* RootTwo(ScalarType id);

/// Returns one half of square root 2
void const* HalfRootTwo(ScalarType id);

/// Returns the natural log of 2
void const* LnTwo(ScalarType id);

/// Returns the natural log of ln(2)
void const* LnLnTwo(ScalarType id);

/// Returns 3
void const* Third(ScalarType id);

/// Returns 2/3
void const* TwoThirds(ScalarType id);

/// Returns pi minus 3
void const* PiMinusThree(ScalarType id);

/// Returns 4 - pi
void const* FourMinusPi(ScalarType id);

} //namespace constants
} //namespace cask

#endif //INCLUDE_GUARD_CASK_CONSTANTS_H 
