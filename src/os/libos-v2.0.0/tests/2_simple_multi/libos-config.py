import sys
sys.path.append('../../config')
import libos
from libos import *

# 1MB heap for pagetales and image BSS
libos.init_heap_size = 1024*1024

# Declare ELF images to create
image(name="init.elf", sources=['task.c', '$(LIBOS_SOURCE)/lib/init_daemon.c'], preboot_init_mapped=True)
image(name="task1A.elf",sources=['task1A.c'])
image(name="task1B.elf", sources=['task1B.c'])
image(name="task2A.elf", sources=['task2A.c'])
image(name="task2B.elf", sources=['task2B.c'])

# Setup isolation groups
address_space(name='PASID_INIT', images=["init.elf"])
address_space(name='DOMAIN1', images=["task1A.elf", "task1B.elf"])
address_space(name='DOMAIN2', images=["task2A.elf", "task2B.elf"])

# Interrupt port
port(name='PORT_EXTINTR', receiver='TASK_INIT', senders=[])

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
task(name='TASK1A', pasid='DOMAIN1', entry='task1A_entry', stack=[8192, memory_region.CACHE], waiting_on='TASK1A_PORT', randomize_initial_registers=True)
port(name='TASK1A_PORT', receiver='TASK1A', senders=['TASK_INIT'])

task(name='TASK1B',  pasid='DOMAIN1', entry='task1B_entry', stack=[8192, memory_region.CACHE], waiting_on='TASK1B_PORT', randomize_initial_registers=True)
port(name='TASK1B_PORT', receiver='TASK1B', senders=['TASK_INIT'])

task(name='TASK2A',  pasid='DOMAIN2',entry='task2A_entry', stack=[8192, memory_region.CACHE], waiting_on='TASK2A_PORT', randomize_initial_registers=True)
port(name='TASK2A_PORT', receiver='TASK2A', senders=['TASK_INIT'])

task(name='TASK2B',  pasid='DOMAIN2',entry='task2B_entry', stack=[8192, memory_region.CACHE], waiting_on='TASK2B_PORT', randomize_initial_registers=True)
port(name='TASK2B_PORT', receiver='TASK2B', senders=['TASK_INIT'])

log(name='LOG_INIT', task='TASK_INIT', id = 'LOGINIT')
log(name='LOG_TASK1A', task='TASK1A', id = 'LOG1A')
log(name='LOG_TASK1B', task='TASK1B', id = 'LOG1B')
log(name='LOG_TASK2A', task='TASK2A', id = 'LOG2A')
log(name='LOG_TASK2B', task='TASK2B', id = 'LOG2B')

