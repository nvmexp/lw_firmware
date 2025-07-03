/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*! \brief
 * Define compile time symbols for CPU type and operating system type.
 * This file should only contain preprocessor commands so that
 * there are no dependencies on other files.
 *
 * cpuopsys.h
 *
 * Copyright (c) 2001, Lwpu Corporation.  All rights reserved.
 */

/*!
 * Uniform names are defined for compile time options to distinguish
 * CPU types and Operating systems.
 * Distinctions between CPU and OpSys should be orthogonal.
 *
 * These uniform names have initially been defined by keying off the
 * makefile/build names defined for builds in the OpenGL group.
 * Getting the uniform names defined for other builds may require
 * different qualifications.
 *
 * The file is placed here to allow for the possibility of all driver
 * components using the same naming convention for conditional compilation.
 */

#ifndef CPUOPSYS_H
#define CPUOPSYS_H

/*****************************************************************************/
/* Define all OS/CPU-Chip related symbols */

/* ***** WINDOWS variations */
#if defined(_WIN32) || defined(_WIN16)
#   define LW_WINDOWS

#   if defined(_WIN32_WINNT)
#      define LW_WINDOWS_NT
#   elif defined(_WIN32_WCE)
#      define LW_WINDOWS_CE
#   elif !defined(LW_MODS)
#      define LW_WINDOWS_9X
#   endif
#endif  /* _WIN32 || defined(_WIN16) */

/* ***** Unix variations */
#if defined(__linux__) && !defined(LW_LINUX) && !defined(LW_VMWARE)
#   define LW_LINUX
#endif  /* defined(__linux__) */

#if defined(__VMWARE__) && !defined(LW_VMWARE)
#   define LW_VMWARE
#endif /* defined(__VMWARE__) */

/* SunOS + gcc */
#if defined(__sun__) && defined(__svr4__) && !defined(LW_SUNOS)
#   define LW_SUNOS
#endif /* defined(__sun__) && defined(__svr4__) */

/* SunOS + Sun Compiler (named SunPro, Studio or Forte) */
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#   define LW_SUNPRO_C
#   define LW_SUNOS
#endif /* defined(_SUNPRO_C) || defined(__SUNPRO_CC) */

#if defined(__FreeBSD__) && !defined(LW_BSD)
#   define LW_BSD
#endif /* defined(__FreeBSD__) */

/* XXXar don't define LW_UNIX on MacOSX or vxworks or QNX */
#if (defined(__unix__) || defined(__unix) || defined(__INTEGRITY) ) && !defined(lwmacosx) && !defined(vxworks) && !defined(LW_UNIX) && !defined(__QNX__) && !defined(__QNXNTO__)/* XXX until removed from Makefiles */
#   define LW_UNIX
#endif /* defined(__unix__) */

#if (defined(__QNX__) || defined(__QNXNTO__)) && !defined(LW_QNX)
#   define LW_QNX
#endif

#if (defined(__ANDROID__) || defined(ANDROID)) && !defined(LW_ANDROID)
#   define LW_ANDROID
#endif

#if defined(__hos__) && !defined(LW_HOS)
#    define LW_HOS
#endif

/* ***** Apple variations */
#if defined(macintosh) || defined(__APPLE__)
#   define LW_MACINTOSH
#   if defined(__MACH__)
#      define LW_MACINTOSH_OSX
#   else
#      define LW_MACINTOSH_OS9
#   endif
#   if defined(__LP64__)
#      define LW_MACINTOSH_64
#   endif
#endif  /* defined(macintosh) */

/* ***** VxWorks */
/* Tornado 2.21 is gcc 2.96 and #defines __vxworks. */
/* Tornado 2.02 is gcc 2.7.2 and doesn't define any OS symbol, so we rely on */
/* the build system #defining vxworks. */
#if defined(__vxworks) || defined(vxworks)
#   define LW_VXWORKS
#endif

/* ***** Integrity OS */
#if defined(__INTEGRITY)
#  if !defined(LW_INTEGRITY)
#    define LW_INTEGRITY
#  endif
#endif

/* ***** Processor type variations */
/* Note: The prefix LW_CPU_* is taken by \\sw\main\sdk\lwpu\inc\Lwcm.h */

#if ((defined(_M_IX86) || defined(__i386__) || defined(__i386)) && !defined(LWCPU_X86)) /* XXX until removed from Makefiles */
/* _M_IX86 for windows, __i386__ for Linux (or any x86 using gcc) */
/* __i386 for Studio compiler on Solaris x86 */
#   define LWCPU_X86               /* any IA32 machine (not x86-64) */
#   define LWCPU_MIN_PAGE_SHIFT 12
#endif

#if defined(_WIN32) && defined(_M_IA64)
#   define LWCPU_IA64_WINDOWS      /* any IA64 for Windows opsys */
#endif
#if defined(LW_LINUX) && defined(__ia64__)
#   define LWCPU_IA64_LINUX        /* any IA64 for Linux opsys */
#endif
#if defined(LWCPU_IA64_WINDOWS) || defined(LWCPU_IA64_LINUX) || defined(IA64)
#   define LWCPU_IA64              /* any IA64 for any opsys */
#endif

#if (defined(LW_MACINTOSH) && !(defined(__i386__) || defined(__x86_64__)))  || defined(__PPC__) || defined(__ppc)
#    if defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
#       ifndef LWCPU_PPC64LE
#           define LWCPU_PPC64LE           /* PPC 64-bit little endian */
#       endif
#    else
#       ifndef LWCPU_PPC
#           define LWCPU_PPC               /* any non-PPC64LE PowerPC architecture */
#       endif
#       ifndef LW_BIG_ENDIAN
#           define LW_BIG_ENDIAN
#       endif
#    endif
#    define LWCPU_FAMILY_PPC
#endif

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
#    define LWCPU_X86_64           /* any x86-64 for any opsys */
#endif

#if defined(LWCPU_X86) || defined(LWCPU_X86_64)
#    define LWCPU_FAMILY_X86
#endif

#if defined(__riscv) && (__riscv_xlen==64)
#    define LWCPU_RISCV64
#    if defined(__lwriscv)
#       define LWCPU_LWRISCV64
#    endif
#endif

#if defined(__arm__) || defined(_M_ARM)
/*
 * 32-bit instruction set on, e.g., ARMv7 or AArch32 exelwtion state
 * on ARMv8
 */
#   define LWCPU_ARM
#   define LWCPU_MIN_PAGE_SHIFT 12
#endif

#if defined(__aarch64__) || defined(__ARM64__)
#   define LWCPU_AARCH64           /* 64-bit A64 instruction set on ARMv8 */
#   define LWCPU_MIN_PAGE_SHIFT 12
#endif

#if defined(LWCPU_ARM) || defined(LWCPU_AARCH64)
#   define LWCPU_FAMILY_ARM
#endif

#if defined(__SH4__)
#   ifndef LWCPU_SH4
#   define LWCPU_SH4               /* Renesas (formerly Hitachi) SH4 */
#   endif
#   if   defined LW_WINDOWS_CE
#       define LWCPU_MIN_PAGE_SHIFT 12
#   endif
#endif

/* For Xtensa processors */
#if defined(__XTENSA__)
# define LWCPU_XTENSA
# if defined(__XTENSA_EB__)
#  define LW_BIG_ENDIAN
# endif
#endif


/*
 * Other flavors of CPU type should be determined at run-time.
 * For example, an x86 architecture with/without SSE.
 * If it can compile, then there's no need for a compile time option.
 * For some current GCC limitations, these may be fixed by using the Intel
 * compiler for certain files in a Linux build.
 */

/* The minimum page size can be determined from the minimum page shift */
#if defined(LWCPU_MIN_PAGE_SHIFT)
#define LWCPU_MIN_PAGE_SIZE (1 << LWCPU_MIN_PAGE_SHIFT)
#endif

#if defined(LWCPU_IA64) || defined(LWCPU_X86_64) || \
    defined(LW_MACINTOSH_64) || defined(LWCPU_AARCH64) || \
    defined(LWCPU_PPC64LE) || defined(LWCPU_RISCV64)
#   define LW_64_BITS          /* all architectures where pointers are 64 bits */
#else
/* we assume 32 bits. I don't see a need for LW_16_BITS. */
#endif

/*
 * NOTE: LW_INT64_OK is not needed in the OpenGL driver for any platform
 * we care about these days. The only consideration is that Linux does not
 * have a 64-bit divide on the server. To get around this, we colwert the
 * expression to (double) for the division.
 */
#if (!(defined(macintosh) || defined(vxworks) || defined(__INTEL_COMPILER)) || defined(LW_LINUX)) && !defined(LW_INT64_OK)
#define LW_INT64_OK
#endif

/* For verification-only features not intended to be included in normal drivers */
#if defined(LW_MODS) && defined(DEBUG) && !defined(DISABLE_VERIF_FEATURES)
#define LW_VERIF_FEATURES
#endif

/*
 * New, safer family of #define's -- these ones use 0 vs. 1 rather than
 * defined/!defined.  This is advantageous because if you make a typo,
 * say misspelled ENDIAN:
 *
 *   #if LWCPU_IS_BIG_ENDAIN
 *
 * ...some compilers can give you a warning telling you that you screwed up.
 * The compiler can also give you a warning if you forget to #include
 * "cpuopsys.h" in your code before the point where you try to use these
 * conditionals.
 *
 * Also, the names have been prefixed in more cases with "CPU" or "OS" for
 * increased clarity.  You can tell the names apart from the old ones because
 * they all use "_IS_" in the name.
 *
 * Finally, these can be used in "if" statements and not just in #if's.  For
 * example:
 *
 *   if (LWCPU_IS_BIG_ENDIAN) x = Swap32(x);
 *
 * Maybe some day in the far-off future these can replace the old #define's.
 */
#if defined(LW_WINDOWS)
#define LWOS_IS_WINDOWS 1
#else
#define LWOS_IS_WINDOWS 0
#endif
#if defined(LW_WINDOWS_CE)
#define LWOS_IS_WINDOWS_CE 1
#else
#define LWOS_IS_WINDOWS_CE 0
#endif
#if defined(LW_LINUX)
#define LWOS_IS_LINUX 1
#else
#define LWOS_IS_LINUX 0
#endif
#if defined(LW_UNIX)
#define LWOS_IS_UNIX 1
#else
#define LWOS_IS_UNIX 0
#endif
#if defined(LW_BSD)
#define LWOS_IS_FREEBSD 1
#else
#define LWOS_IS_FREEBSD 0
#endif
#if defined(LW_SUNOS)
#define LWOS_IS_SOLARIS 1
#else
#define LWOS_IS_SOLARIS 0
#endif
#if defined(LW_VMWARE)
#define LWOS_IS_VMWARE 1
#else
#define LWOS_IS_VMWARE 0
#endif
#if defined(LW_QNX)
#define LWOS_IS_QNX 1
#else
#define LWOS_IS_QNX 0
#endif
#if defined(LW_ANDROID)
#define LWOS_IS_ANDROID 1
#else
#define LWOS_IS_ANDROID 0
#endif
#if defined(LW_MACINTOSH)
#define LWOS_IS_MACINTOSH 1
#else
#define LWOS_IS_MACINTOSH 0
#endif
#if defined(LW_VXWORKS)
#define LWOS_IS_VXWORKS 1
#else
#define LWOS_IS_VXWORKS 0
#endif
#if defined(LW_INTEGRITY)
#define LWOS_IS_INTEGRITY 1
#else
#define LWOS_IS_INTEGRITY 0
#endif
#if defined(LW_HOS)
#define LWOS_IS_HOS 1
#else
#define LWOS_IS_HOS 0
#endif
#if defined(LWCPU_X86)
#define LWCPU_IS_X86 1
#else
#define LWCPU_IS_X86 0
#endif
#if defined(LWCPU_RISCV64)
#define LWCPU_IS_RISCV64 1
#else
#define LWCPU_IS_RISCV64 0
#endif
#if defined(LWCPU_LWRISCV64)
#define LWCPU_IS_LWRISCV64 1
#else
#define LWCPU_IS_LWRISCV64 0
#endif
#if defined(LWCPU_IA64)
#define LWCPU_IS_IA64 1
#else
#define LWCPU_IS_IA64 0
#endif
#if defined(LWCPU_X86_64)
#define LWCPU_IS_X86_64 1
#else
#define LWCPU_IS_X86_64 0
#endif
#if defined(LWCPU_FAMILY_X86)
#define LWCPU_IS_FAMILY_X86 1
#else
#define LWCPU_IS_FAMILY_X86 0
#endif
#if defined(LWCPU_PPC)
#define LWCPU_IS_PPC 1
#else
#define LWCPU_IS_PPC 0
#endif
#if defined(LWCPU_PPC64LE)
#define LWCPU_IS_PPC64LE 1
#else
#define LWCPU_IS_PPC64LE 0
#endif
#if defined(LWCPU_FAMILY_PPC)
#define LWCPU_IS_FAMILY_PPC 1
#else
#define LWCPU_IS_FAMILY_PPC 0
#endif
#if defined(LWCPU_ARM)
#define LWCPU_IS_ARM 1
#else
#define LWCPU_IS_ARM 0
#endif
#if defined(LWCPU_AARCH64)
#define LWCPU_IS_AARCH64 1
#else
#define LWCPU_IS_AARCH64 0
#endif
#if defined(LWCPU_FAMILY_ARM)
#define LWCPU_IS_FAMILY_ARM 1
#else
#define LWCPU_IS_FAMILY_ARM 0
#endif
#if defined(LWCPU_SH4)
#define LWCPU_IS_SH4 1
#else
#define LWCPU_IS_SH4 0
#endif
#if defined(LWCPU_XTENSA)
#define LWCPU_IS_XTENSA 1
#else
#define LWCPU_IS_XTENSA 0
#endif
#if defined(LW_BIG_ENDIAN)
#define LWCPU_IS_BIG_ENDIAN 1
#else
#define LWCPU_IS_BIG_ENDIAN 0
#endif
#if defined(LW_64_BITS)
#define LWCPU_IS_64_BITS 1
#else
#define LWCPU_IS_64_BITS 0
#endif
#if defined(LWCPU_FAMILY_ARM)
#define LWCPU_IS_PCIE_CACHE_COHERENT 0
#else
#define LWCPU_IS_PCIE_CACHE_COHERENT 1
#endif
/*****************************************************************************/

#endif /* CPUOPSYS_H */
