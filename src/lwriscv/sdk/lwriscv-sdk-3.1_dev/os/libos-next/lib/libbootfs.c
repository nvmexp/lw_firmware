#include "libbootfs.h"
#include <lwtypes.h>
#include "libelf.h"

__attribute__((used)) LibosBootfsRecord * LibosBootFileFirst(LibosBootFsArchive * archive)
{
    if (archive->magic != LIBOS_BOOT_FS_MAGIC)
        return 0;
    return (LibosBootfsRecord *)((LwU64)archive + archive->headerSize);
}

__attribute__((used)) LibosBootfsRecord * LibosBootFileNext(LibosBootFsArchive * archive, LibosBootfsRecord * file)
{
    if (!file->next)
        return 0;

    return (LibosBootfsRecord *)((LwU64)archive + file->next);
}

__attribute__((used)) LibosBootfsExelwtableMapping * LibosBootMappingFirst(LibosBootFsArchive * archive, LibosBootfsRecord * file)
{
    if (!file->offsetFirstMapping)
        return 0;

    return (LibosBootfsExelwtableMapping *)((LwU64)archive + file->offsetFirstMapping);
}

__attribute__((used)) LibosBootfsExelwtableMapping * LibosBootMappingNext(LibosBootFsArchive * archive, LibosBootfsExelwtableMapping * mapping)
{
    if (!mapping->next)
        return 0;
    return (LibosBootfsExelwtableMapping *)((LwU64)archive + mapping->next);
}

__attribute__((used)) LibosBootfsExelwtableMapping * LibosBootFindMappingWithAttributes(LibosBootFsArchive * archive, LibosBootfsRecord * file, LwU64 attributes)
{
    LibosBootfsExelwtableMapping * i = LibosBootMappingFirst(archive, file);
    while (i) {
        if (i->attributes == attributes)
            return i;
        i = LibosBootMappingNext(archive, i);
    }
    return 0;
}

__attribute__((used)) LibosBootfsExelwtableMapping * LibosBootFindMappingText(LibosBootFsArchive * archive, LibosBootfsRecord * file)
{
    return LibosBootFindMappingWithAttributes(archive, file, LIBOS_BOOTFS_MAPPING_X|LIBOS_BOOTFS_MAPPING_R);
}

__attribute__((used)) LibosBootfsExelwtableMapping * LibosBootFindMappingData(LibosBootFsArchive * archive, LibosBootfsRecord * file)
{
    return LibosBootFindMappingWithAttributes(archive, file, LIBOS_BOOTFS_MAPPING_R|LIBOS_BOOTFS_MAPPING_W);
}

__attribute__((used)) LibosElf64Header  * LibosBootFindElfHeader(LibosBootFsArchive * archive, LibosBootfsRecord * file)
{
    // Find text section by attributes
    LibosBootfsExelwtableMapping * text = LibosBootFindMappingText(archive, file);
    if (!text)
        return 0;

    // Find start of section in archive
    LibosElf64Header  * elf = (LibosElf64Header  *)((LwU64)archive + text->offset);

    // Validate the ELF header
    if (elf->ident[0] != 0x7F || elf->ident[1] != 'E' ||
        elf->ident[2] != 'L'  || elf->ident[3] != 'F')
        return 0;

    // containts entire ELF header
    if (text->length < sizeof(LibosElf64Header))
        return 0;

#ifdef FUTURE
    // contains PHDRs 
    LwU64 phdrsEnd = elf->phoff + elf->phentsize * elf->phnum;
    if (text->length < phdrsEnd)
        return 0;

    // contains sections
    LwU64 sectionsEnd = elf->shoff + elf->shentsize * elf->shnum;
    if (text->length < sectionsEnd)
        return 0;
#endif
    return elf;
}

const char * LibosBootFileName(LibosBootFsArchive * archive, LibosBootfsRecord * file)
{
    return (const char *)((LwU64)archive + file->filename);
}

void * LibosBootMappingBuffer(LibosBootFsArchive * archive, LibosBootfsExelwtableMapping * file)
{
    return (void *)((LwU64)archive + file->offset);
}