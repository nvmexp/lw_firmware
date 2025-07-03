import sys
sys.path.append('../../config')
import libos
from libos import *

# 1MB heap for pagetales and image BSS
libos.init_heap_size = 1024*1024

# Declare ELF images to create
image(name="init.elf", sources=['init.c', '$(LIBOS_SOURCE)/lib/init_daemon.c'], preboot_init_mapped=True)
image(name="task.elf",sources=['test-app.c'])

# Setup isolation groups
address_space(name='PASID_INIT', images=["init.elf"])
address_space(name='TASK_DOMAIN', images=["task.elf"])

# Setup the init task
task(name='TASK_INIT', pasid='PASID_INIT', entry='task_init_entry', stack=[8192, memory_region.CACHE | memory_region.PREBOOT_INIT_MAPPED], randomize_initial_registers=True)
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
    reader_list=['TASK_INIT', 'TEST_APP'], 
    writer_list=['TASK_INIT', 'TEST_APP'])

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
task(name='TEST_APP', pasid='TASK_DOMAIN', entry='task_entry', stack=[8192, memory_region.CACHE], waiting_on='TEST_APP_PORT', randomize_initial_registers=True)
port(name='TEST_APP_PORT', receiver='TEST_APP', senders=['TASK_INIT'])

# Interrupt port
port(name='PORT_EXTINTR', receiver='TEST_APP', senders=[])

log(name='LOG_INIT', task='TASK_INIT', id = 'LOGINIT')
log(name='LOG_TASK', task='TEST_APP', id = 'LOG')
