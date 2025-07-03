#pragma once

#include "../debug/elf.h"
#include "lwtypes.h"
/**
 *
 * @brief Returns the size of the heap required at the end of the ELF
 *
 *        Once the kernel starts, additional heap
 *        space is required by the kernel to unfurl any copy
 *        on write, or BSS sections.  This memory is expected
 *        to live immediately after the end of the image.
 *
 *        The total size of this region is precise and callwlated
 *        by the linker through the ld-script.
 *
 * @param[in] elf
 * @returns false if not a recognized LIBOS ELF.
 */
LwBool libosElfGetStaticHeapSize(elf64_header *elf, LwU64 *additionalHeapSize);

/**
 *
 * @brief Returns the offset from the start of the ELF corresponding
 *        to the entry point.
 *
 *        The ELF should be copied directly into memory without modification.
 *        The packaged LIBOS elf contains a self bootstrapping ELF loader.
 *
 *        During boot:
 *            - PHDRs are copied into memory descriptors
 *            - Copy on write sections are duplicated (into additional heap above)
 *
 * @param[in] elf
 * @returns false if not a recognized LIBOS ELF.
 */

LwBool libosElfGetBootEntry(elf64_header *elf, LwU64 *entry);