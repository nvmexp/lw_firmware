#define DEFINED

 /**/ # \
     /**/ ifdef DEFINED
good
 /**/ # /**/ else
#bad bad
#endif

#ifndef DEFINED
#bad bad
#else
good
#endif

#if defined DEFINED
good
#else
#bad bad
#endif

#if defined(DEFINED)
good
#else
#bad bad
#endif

#if 1
good
#else
#bad bad
#endif

#ifdef UNDEFINED
#bad bad
#else
good
#endif

#ifndef UNDEFINED
good
#else
#bad bad
#endif

#if defined UNDEFINED
#bad bad
#else
good
#endif

#if defined(UNDEFINED)
#bad bad
#else
good
#endif

#if 0
#bad bad
#else
good
#endif

#if 1
good
#if 0
#bad bad
#include "non-existent"
#else
good
#endif
#else
#bad bad
#include "non-existent"
#endif

#pragma once
