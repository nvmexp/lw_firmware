import sys
sys.path.append('../../config')
import libos
from libos import *

# 1MB heap for pagetales and image BSS
libos.init_heap_size = 1024*1024

# Declare ELF images to create
image(name="task_init.elf", sources=['task_init.c', '$(LIBOS_SOURCE)/lib/init_daemon.c'], preboot_init_mapped=True)
image(name="task_dmem.elf",sources=['task_dmem.c'])
image(name="task_imem.elf",sources=['task_imem.c', '$(OUT_DIR)/pageable_imem.c'])

# Setup isolation groups
address_space(name='PASID_INIT', images=["task_init.elf"])
address_space(name='PASID_IMEM', images=["task_imem.elf" ])
address_space(name='PASID_DMEM', images=["task_dmem.elf" ])

# Interrupt port
port(name='PORT_EXTINTR', receiver='TASK_INIT', senders=[])

# Setup the init task
task(name='TASK_INIT', pasid='PASID_INIT', entry='task_init_entry', stack=[8192, memory_region.CACHE | memory_region.PREBOOT_INIT_MAPPED])
port(name='TASK_INIT_PANIC_PORT', receiver='TASK_INIT', senders=[])

# DMEM mapping for copy acceleration
memory_region(
    mapping=memory_region.PAGED_TCM | memory_region.PREBOOT_INIT_MAPPED,   
    size=0x10000,
    name="TASK_INIT_DMEM_64KB", 
    reader_list=['TASK_INIT'], 
    writer_list=['TASK_INIT'])

# Init task requires aperture for MMIO
memory_region(
    mapping=memory_region.APERTURE_MMIO | memory_region.PREBOOT_INIT_MAPPED,   
    virtual_address=0x2000000000000000, 
    aperture_offset=0,
    size=0x1000000000000,
    name="TASK_INIT_PRIV", 
    reader_list=['TASK_INIT'], 
    writer_list=['TASK_INIT'])

# Init task requires aperture for FB (init message)
memory_region(
    mapping=memory_region.APERTURE_FB | memory_region.PREBOOT_INIT_MAPPED,   
    virtual_address=0x6180000000000000, 
    aperture_offset=0,
    size=0x1000000000000, 
    name="TASK_INIT_FB", 
    reader_list=['TASK_INIT'], 
    writer_list=['TASK_INIT'])

# Setup the goal tasks
task(name='TASK_IMEM', pasid='PASID_IMEM', entry='task_imem_entry', stack=[8192, memory_region.CACHE], waiting_on='TASK_IMEM_PORT')
port(name='TASK_IMEM_PORT', receiver='TASK_IMEM', senders=['TASK_INIT'])

task(name='TASK_DMEM',  pasid='PASID_DMEM', entry='task_dmem_entry', stack=[8192, memory_region.CACHE], waiting_on='TASK_DMEM_PORT')
port(name='TASK_DMEM_PORT', receiver='TASK_DMEM', senders=['TASK_INIT'])

log(name='LOG_INIT', task='TASK_INIT', id = 'LOGINIT')
log(name='LOG_IMEM', task='TASK_IMEM', id = 'LOGIMEM')
log(name='LOG_DMEM', task='TASK_DMEM', id = 'LOGDMEM')
