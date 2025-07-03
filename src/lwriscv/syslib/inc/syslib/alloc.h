/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef MMAN_H
#define MMAN_H

/*!
 * @file    mman.h
 * @brief   Posix memory management functions (for dlmalloc).
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <stddef.h>
// off_t is only defined as an integer, most implementations use long int.
typedef long int off_t;

/* ------------------------ Defines ---------------------------------------- */

#define PROT_NONE       0
#define PROT_READ       1
#define PROT_WRITE      2

#define MAP_SHARED      0
#define MAP_PRIVATE     1
#define MAP_ANONYMOUS   2

#define MMAP_FAIL ((void *)-1)

// MMAP will work for now only to allocate memory.
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
#endif // MMAN_H
