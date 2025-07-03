#pragma once

#include <lwtypes.h>
#include "libelf.h"

typedef struct {
    LwU64 virtualAddress;
    LwU64 originalElfOffset;    // effective offset in original elf of this VA
    LwU64 alignment;
    LwU64 offset;               
    LwU64 length;           
    LwU64 attributes;  
    LwU64 next;   
} LibosBootfsExelwtableMapping;

#define LIBOS_BOOTFS_MAPPING_X 1
#define LIBOS_BOOTFS_MAPPING_W 2
#define LIBOS_BOOTFS_MAPPING_R 4

typedef struct {
    LwU64 filename;     
    LwU64 offsetFirstMapping;
    LwU64 next;
} LibosBootfsRecord;

typedef struct {
    LwU64 magic;
    LwU64 headerSize;   // size of this structure
    LwU64 totalSize;    // size of entire file with mapping
} LibosBootFsArchive;

#define LIBOS_BOOT_FS_MAGIC 0x81B26A705C10E14D

// Enumerate files in an archive
LibosBootfsRecord * LibosBootFileFirst(LibosBootFsArchive * archive);
LibosBootfsRecord * LibosBootFileNext(LibosBootFsArchive * archive, LibosBootfsRecord * file);
const char * LibosBootFileName(LibosBootFsArchive * archive, LibosBootfsRecord * file);
void * LibosBootMappingBuffer(LibosBootFsArchive * archive, LibosBootfsExelwtableMapping * file);


// Enumerate mappings in an mapping file
LibosBootfsExelwtableMapping * LibosBootMappingFirst(LibosBootFsArchive * archive, LibosBootfsRecord * file);
LibosBootfsExelwtableMapping * LibosBootMappingNext(LibosBootFsArchive * archive, LibosBootfsExelwtableMapping * mapping);

// Find special sections in an mapped ELF
LibosBootfsExelwtableMapping * LibosBootFindMappingText(LibosBootFsArchive * archive, LibosBootfsRecord * file);
LibosBootfsExelwtableMapping * LibosBootFindMappingData(LibosBootFsArchive * archive, LibosBootfsRecord * file);
LibosElf64Header  * LibosBootFindElfHeader(LibosBootFsArchive * archive, LibosBootfsRecord * file);