#if 0 == 0
good
#else
bad
#endif

#if 0 == zero
good
#else
bad
#endif

#if 10 * zero == 0
good
#else
bad
#endif

#if ' ' == 0x20
good
#else
bad
#endif

#if 2 * 3 + 4 == 10
good
#else
bad
#endif

#if 2 + 3 * 4 == 14
good
#else
bad
#endif

#if 1 + 2 * 3 - 4 * 5 == - 13
good
#else
bad
#endif

#if 1 && 1 && 1
good
#else
bad
#endif

#if 1 && 0 && 1
bad
#else
good
#endif

#if 0 || 0 || 0
bad
#else
good
#endif

#if 0 || 1 || 0
good
#else
bad
#endif

#if zero && zero || 1
good
#else
bad
#endif

#if zero && 1 || 1
good
#else
bad
#endif

#if 1 + 2 == 3 \
 && -2 - 3 == -5
good
#else
bad
#endif

#if (1 + 2) * (3 - 1) == 6
good
#else
bad
#endif

#if (1+-~1*3+1)/2==18%7
good
#else
bad
#endif

#if (1+1<<2|2^15&7) == 13
good
#else
bad
#endif

#if (1 + 2 == 3 ? 2 == 1 + 1 : 0) == 1
good
#else
bad
#endif

#if (1 ? 2 : 3 ? 0 : 0) == 2
good
#else
bad
#endif

#if (0 ? 1 : 2 ? 3 : 4) == 3
good
#else
bad
#endif

#if (1 ? 2 ? 3 : 4 : 5) == 3
good
#else
bad
#endif

#if (0 ? 0 ? 1 : 2 : 3) == 3
good
#else
bad
#endif

#define SOME1

#if 1+defined SOME1+zero==defined(UNDEFINED)+2
good
#else
bad
#endif
