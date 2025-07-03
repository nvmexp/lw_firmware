import sys
sys.path.append('../../config')
import libos
from libos import *

# 1MB heap for pagetales and image BSS
libos.init_heap_size = 1024*1024

image(name="init.elf", sources=['init.c', '$(LIBOS_SOURCE)/lib/init_daemon.c'], preboot_init_mapped=True)
image(name="test.elf", sources=['test.c'])
address_space(name='PASID_INIT', images=["init.elf"])
address_space(name='PASID_TEST', images=["test.elf"])

# Interrupt port
port(name='PORT_EXTINTR', receiver='TASK_INIT', senders=[])

task(name='TASK_INIT', pasid='PASID_INIT', entry='task_init_entry', stack=[8192, memory_region.CACHE | memory_region.PREBOOT_INIT_MAPPED], randomize_initial_registers=True)
port(name='TASK_INIT_PANIC_PORT', receiver='TASK_INIT', senders=[])
port(name='TASK_INIT_PORT', receiver='TASK_INIT', senders=['TASK_TEST'])
port(name='TASK_INIT_PORT_PRIVATE', receiver='TASK_INIT', senders=[])
log(name='LOG_INIT', task='TASK_INIT', id = 'LOGINIT')

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

task(name='TASK_TEST', pasid='PASID_TEST', entry='task_test_entry', stack=[128*1024+4096, memory_region.PAGED_TCM], waiting_on='TASK_TEST_PORT', randomize_initial_registers=True)
port(name='TASK_TEST_PORT', receiver='TASK_TEST', senders=['TASK_INIT'])
log(name='LOG_TEST', task='TASK_TEST', id = 'LOGTEST')

# Task init will send it's first message with a receive buffer
# This will cause a trap in the internal port copy
memory_region('TASK_INIT_INITIAL_RECV_BUFFER', ['TASK_INIT'], ['TASK_INIT'], 4096, memory_region.PAGED_TCM)
