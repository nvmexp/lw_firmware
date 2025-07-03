/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _LIBMODS_H_
#define _LIBMODS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <IOKit/IODataQueueClient.h>

#include "macosx/kext/LwModsDriverApi.h"

typedef enum
{
    MODS_ATTRIB_CACHED,
    MODS_ATTRIB_UNCACHED,
    MODS_ATTRIB_WRITECOMBINE,
    MODS_ATTRIB_WRITETHROUGH,
    MODS_ATTRIB_WRITEPROTECT
} MODS_ATTRIB;

typedef enum
{
    MODS_CONSOLE_INFO_DISABLE,
    MODS_CONSOLE_INFO_ENABLE
} MODS_CONSOLE_INFO;

int mods_open();
int mods_close();

int mods_get_api_version(uint32_t *version);

int mods_find_pci_device(uint32_t dev_id, uint32_t vendor_id,
            int idx, uint8_t *bus_num, uint8_t *dev_num,
            uint8_t *func_num);
int mods_find_pci_class_code(uint32_t class_code, int idx,
            uint8_t *bus_num, uint8_t *dev_num, uint8_t *func_num);
int mods_pci_read(uint8_t bus_num, uint8_t dev_num,
            uint8_t func_num, uint32_t addr, uint32_t *data,
            uint32_t data_size);
int mods_pci_write(uint8_t bus_num, uint8_t dev_num,
            uint8_t func_num, uint32_t addr, uint32_t data,
            uint32_t data_size);

int mods_alloc_pages(uint64_t num_bytes, int contiguous,
            uint32_t addr_bits, MODS_ATTRIB attr, uint64_t *mem);
int mods_free_pages(uint64_t mem);
int mods_map_pages(uint64_t mem, uint64_t offset,
            uint64_t num_bytes, void **virt_addr);
int mods_map_dev_mem(uint64_t phys_addr, uint64_t num_bytes,
            MODS_ATTRIB attr, void **virt_addr);
int mods_unmap_mem(void *virt_addr, uint64_t num_bytes);
int mods_get_phys_addr(uint64_t mem, uint64_t offset, uint64_t *phys_addr);
int mods_virt_to_phys(void *virt_addr, uint64_t *phys_addr);
int mods_phys_to_virt(uint64_t phys_addr, void **virt_addr);

int mods_pio_read(uint64_t addr, uint64_t *data, uint32_t data_size);
int mods_pio_write(uint64_t addr, uint64_t data, uint32_t data_size);
int mods_set_console_info(MODS_CONSOLE_INFO op);
int mods_register_interrupt(uint32_t bus, uint32_t device, uint32_t function, uint32_t type, uint32_t index);
int mods_unregister_interrupt(uint32_t bus, uint32_t device, uint32_t function, uint32_t type);
int mods_create_interrupt_queue(mach_port_t port, IODataQueueMemory **queue);
int mods_release_interrupt_queue();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LIBMODS_H_ */
