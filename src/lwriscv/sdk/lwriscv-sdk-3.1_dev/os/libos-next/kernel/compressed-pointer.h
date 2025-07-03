#pragma once


// @todo: Compressed pointers don't really work for full 64-bit builds. Eck

#if !LIBOS_TINY
// Pointers inside the kernel are guaranteed by medlow memory model to be below 32-bit
#define LIBOS_PTR32(x)           x *
#define LIBOS_PTR32_EXPAND(x, y) y
#define LIBOS_PTR32_NARROW(y)    y
#else
// Pointers inside the kernel are guaranteed by medlow memory model to be below 32-bit
#define LIBOS_PTR32(x)           LwU32
#define LIBOS_PTR32_EXPAND(x, y) ((x *)((LwU64)(y)))
#define LIBOS_PTR32_NARROW(y)    ((LwU32)((LwU64)(y)))
#endif