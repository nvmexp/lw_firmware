import sys
import atexit
import argparse
import os

libos_path = os.path.realpath(os.path.dirname(__file__) + "/..")

##
##  Root nametable
##

nametable = {}
tasks = []
pasid_list = []
regions = []
init_heap_size = 64*1024


def declare_symbol(name, value):
    if name in nametable:
        sys.stderr.write("Duplicate definition %s.\n" % (name))
        sys.exit(1)
    nametable[name] = value

def resolve_symbol(name):
    if not name in nametable:
        sys.stderr.write("Missing declaration of %s.\n" % (name))
        sys.exit(1)
    return nametable[name]

##
##  LIBOS Objects
##
class image:
    def __init__(self, name, objects=[], sources=[], cflags=None, preboot_init_mapped=False):
        declare_symbol(name, self)
        self.name = name
        self.instance_name = name.replace(".","_").replace("-","_").lower()
        self.objects = objects
        self.sources = sources
        self.cflags = cflags
        self.pasid_list = []
        self.preboot_init_mapped = preboot_init_mapped

class address_space:
    def __init__(self, name, images, grant_debug = []):
        declare_symbol(name, self)
        self.name = name
        self.image_names = images
        self.image_objects = []
        self.grant_debug_pasids = grant_debug
        self.pasid = len(pasid_list)
        if self.pasid == 4096:
            sys.stderr.write("%s exceeded limit of 4096 pasids.\n" % (name))
            sys.exit(1)
        self.mask = 1 << self.pasid

        pasid_list.append(self)

class memory_region:
    PAGED_TCM           = 1
    APERTURE_MMIO       = 2
    DMA_ONLY            = 4
    APERTURE_SYSCOH     = 8
    APERTURE_FB         = 16
    CACHE               = 32
    EXECUTE             = 128

    # Internal use
    LINKER_REGION       = 256    # Corresponds to text/data section
    INIT_ARG_REGION     = 512
    FIXED_VA            = 1024
    PREBOOT_INIT_MAPPED = 2048

    def __init__(self, name, writer_list, reader_list, size, mapping, aperture_offset = None, instance_name = None, init_arg_id8 = None, virtual_address = None):
        declare_symbol(name, self)
        self.name = name
        self.writer_names = writer_list
        self.reader_names = reader_list

        # Encode size
        self.size = (size + 4095) & ~4095
        self.mapping = mapping

        # Do we have a fixed VA?
        if virtual_address != None:
            self.mapping = self.mapping | memory_region.FIXED_VA
            self.virtual_address = virtual_address
            if self.virtual_address >= 0x100000000:
                # Addresses above 4B must be 48-bit aligned
                assert((self.virtual_address & 0xFFFFFFFFFFFF) == 0)
        else:
            assert(self.size < (1<<28)) # This region requires FIXED_VA due to RISC-V mcmodel=medany limitation

        if self.mapping & memory_region.EXECUTE:
            self.phdr_flags = "PF_X"
        else:
            self.phdr_flags = "0"

        if self.mapping & memory_region.PREBOOT_INIT_MAPPED:
            self.phdr_flags = self.phdr_flags + " | LIBOS_PHDR_FLAG_PREMAP_AT_INIT"

        # Callwlate the LIBOS aperture kind and validate all
        if self.mapping & memory_region.APERTURE_MMIO:
            assert(self.mapping & (memory_region.APERTURE_MMIO | memory_region.EXECUTE | memory_region.FIXED_VA) == (self.mapping &~ memory_region.PREBOOT_INIT_MAPPED))
            self.phdr_flags = self.phdr_flags + " | LIBOS_MEMORY_KIND_MMIO"
        elif self.mapping & memory_region.PAGED_TCM:
            assert(self.mapping & (memory_region.APERTURE_FB | memory_region.APERTURE_SYSCOH | memory_region.INIT_ARG_REGION | memory_region.LINKER_REGION |memory_region.PAGED_TCM | memory_region.FIXED_VA | memory_region.EXECUTE) == (self.mapping &~ memory_region.PREBOOT_INIT_MAPPED))
            self.phdr_flags = self.phdr_flags + " | LIBOS_MEMORY_KIND_PAGED_TCM"
        elif self.mapping & memory_region.DMA_ONLY:
            assert(self.mapping & (memory_region.APERTURE_FB | memory_region.APERTURE_SYSCOH | memory_region.INIT_ARG_REGION | memory_region.DMA_ONLY | memory_region.FIXED_VA) == (self.mapping &~ memory_region.PREBOOT_INIT_MAPPED))
            self.phdr_flags = self.phdr_flags + " | LIBOS_MEMORY_KIND_DMA"
        elif self.mapping & memory_region.CACHE:
            assert(self.mapping & (memory_region.APERTURE_FB | memory_region.APERTURE_SYSCOH | memory_region.INIT_ARG_REGION | memory_region.LINKER_REGION | memory_region.CACHE | memory_region.FIXED_VA | memory_region.EXECUTE) == (self.mapping &~ memory_region.PREBOOT_INIT_MAPPED))
            self.phdr_flags = self.phdr_flags + " | LIBOS_MEMORY_KIND_CACHED"
        else:
            assert(self.mapping & (memory_region.APERTURE_FB | memory_region.APERTURE_SYSCOH | memory_region.INIT_ARG_REGION | memory_region.FIXED_VA) == (self.mapping &~ memory_region.PREBOOT_INIT_MAPPED))
            self.phdr_flags = self.phdr_flags + " | LIBOS_MEMORY_KIND_UNCACHED"

        # Compute
        self.physical_address = "LIBOS_MEMORY_PHYSICAL_ILWALID"
        if self.mapping & memory_region.APERTURE_MMIO:
            if aperture_offset != None:
                self.physical_address = "0x%016x" % (aperture_offset | 0x2000000000000000)
        elif mapping & memory_region.APERTURE_SYSCOH:
            if aperture_offset != None:
                self.physical_address = "0x%016x" % (aperture_offset | 0x8180000000000000)
        elif mapping & memory_region.APERTURE_FB:
            if aperture_offset != None:
                self.physical_address = "0x%016x" % (aperture_offset | 0x6180000000000000)
        else:
            assert(not aperture_offset)

        self.reader_pasids = []
        self.writer_pasids = []
        self.instance_name = name.lower() + "_instance"
        if mapping & memory_region.LINKER_REGION:
            self.size = name.lower() + "_size"
        if mapping & memory_region.INIT_ARG_REGION:
            id8 = hex(reduce(lambda s, x: (s<<8) + x, bytearray(init_arg_id8)))

            # the version of python used in unix-build elw adds an 'L' to end of the hex colwersion; this hack removes it
            self.id8 = id8.replace("L", "")
        regions.append(self)

class shuttle:
    def __init__(self, name, owner_name, initial_wait_port = None, hidden = False):
        declare_symbol(name, self)
        self.name = name
        self.initial_wait_port = initial_wait_port
        self.owner_name = owner_name
        self.instance_name = name.lower() + "_instance"
        self.hidden = hidden

class task:
    def __init__(self, name, pasid, entry, stack, waiting_on = None, randomize_initial_registers = False):
        declare_symbol(name, self)
        self.task_id = len(tasks)
        tasks.append(self)
        self.name = name
        self.address_space_name = pasid
        self.stack = stack
        self.entry = entry
        self.port_recv = waiting_on
        self.logs = []
        self.id_for_resource = {}
        self.randomize_initial_registers = randomize_initial_registers
        self.instance_name = name.lower() + "_instance"
        self.default_shuttles = [
            shuttle(name.upper() + '_SHUTTLE_DEFAULT', self.name, None),
            shuttle(name.upper() + '_SHUTTLE_DEFAULT_ALT', self.name, None),
            shuttle(name.upper() + '_INTERNAL_TRAP', self.name, None, hidden=True),
            shuttle(name.upper() + '_INTERNAL_REPLAY', self.name, None, hidden=True)
        ]
        self.default_ports = [
            port(name.upper() + "_INTERNAL_REPLAY_PORT", self.name, [])
        ]


class port:
    def __init__(self, name, receiver, senders):
        declare_symbol(name, self)
        self.name = name
        self.initial_waiter = None
        self.receiver_name = receiver
        self.sender_names = senders
        self.instance_name = name.lower() + "_instance"

class log:
    def __init__(self, name, id, task, maximum_size=0x1000):
        declare_symbol(name, self)
        assert(len(id)<=8)
        self.name = name
        self.id = id
        self.task = task
        self.region = memory_region(name = name+"_buffer", writer_list=[task], reader_list=[task], size=maximum_size, mapping=memory_region.INIT_ARG_REGION, init_arg_id8 = id)

def initarg(name, task, size, id):
    assert(len(id)<=8)
    return memory_region(name=name+"_buffer",
                         writer_list=[task],
                         reader_list=[task],
                         size=size,
                         mapping=memory_region.INIT_ARG_REGION,
                         init_arg_id8=id)

def resolve_name(resource):
    if isinstance(resource, log):
        resource.task_object = resolve_symbol(resource.task)
        resource.task_object.logs.append(resource)
    elif isinstance(resource, address_space):
        resource.image_objects = [resolve_symbol(image) for image in resource.image_names]
        debug_mask = 0
        for i in resource.grant_debug_pasids:
            pasid = resolve_symbol(i)
            debug_mask = debug_mask | (1 << pasid.pasid)
        resource.debug_mask = debug_mask
    elif isinstance(resource, shuttle):
        resource.owner = resolve_symbol(resource.owner_name)
    elif isinstance(resource, port):
        resource.receiver_task = resolve_symbol(resource.receiver_name)
        resource.sender_tasks = [resolve_symbol(sender) for sender in resource.sender_names]
    elif isinstance(resource, memory_region):
        resource.reader_pasids = sorted(set([resolve_symbol(reader).pasid_object.pasid for reader in resource.reader_names]))
        resource.writer_pasids = sorted(set([resolve_symbol(writer).pasid_object.pasid for writer in resource.writer_names]))
    elif isinstance(resource, task):
        resource.pasid_object = resolve_symbol(resource.address_space_name)
        if resource.port_recv:
            resource.waiting_object = resolve_symbol(resource.port_recv)
        else:
            resource.waiting_object = None
        if isinstance(resource.stack, str):
            resource.stack_object = resolve_symbol(resource.stack)
        else:
            if not isinstance(resource.stack, list) or len(resource.stack)!=2 or not isinstance(resource.stack[0], int) or not isinstance(resource.stack[1], int):
                sys.stderr.write("Expected stack name or [size, CACHE] for task %s.\n" % (resource.name))
                sys.exit(1)
            resource.stack_object = memory_region(resource.name + "_STACK", [resource.name], [resource.name], resource.stack[0], resource.stack[1])
            resolve_name(resource.stack_object)

def resolve_names():
    for name,resource in nametable.items():
        if isinstance(resource, task):
            resolve_name(resource)

    for name,resource in nametable.items():
        if not isinstance(resource, task):
            resolve_name(resource)

def assign_local_ids():
    # Assign "fixed" per-task handles like default_shuttles
    # @todo: Cross reference values.
    for t in tasks:
        t.id_for_resource[t.default_shuttles[0]] = 1
        t.id_for_resource[t.default_shuttles[1]] = 2
        t.id_for_resource[t.default_shuttles[2]] = 3
        t.id_for_resource[t.default_shuttles[3]] = 4
        t.id_for_resource[t.default_ports[0]]    = 5

    for name, resource in nametable.items():
        if isinstance(resource, address_space):
            pass
        elif isinstance(resource, shuttle):
            if not resource in resource.owner.id_for_resource:
                resource.owner.id_for_resource[resource] = 1+len(resource.owner.id_for_resource)
        elif isinstance(resource, port):
            for task_resource in resource.sender_tasks:
                if not resource in task_resource.id_for_resource:
                    task_resource.id_for_resource[resource] = 1+len(task_resource.id_for_resource)
            if not resource in resource.receiver_task.id_for_resource:
                resource.receiver_task.id_for_resource[resource] = 1+len(resource.receiver_task.id_for_resource)
        elif isinstance(resource, memory_region):
            pass
        elif isinstance(resource, task):
            pass

def wire():
    for name,resource in nametable.items():
        # Build the initial waiter list linked list
        if isinstance(resource, task):
            if resource.port_recv:
                resource.waiting_object.initial_waiter = resource

                if resource.waiting_object.receiver_task != resource:
                    sys.stderr.write("%s is marked as waiting on %s but is not the receiver.\n" % (resource.name, resource.port_recv))
                    sys.exit(1)

        # Compute pasid mask for images *and* create memory regions
        elif isinstance(resource, address_space):
            for i in resource.image_objects:
                i.pasid_list.append(resource.pasid)

    # Create memory descriptors for images (second pass as the pasid data is pushed down above)
    for name,resource in nametable.items():
        if isinstance(resource, image):
            # Non-pageable memory can be mapped at init
            # (This is used to allow the init task to bootstrap the page tables)
            if resource.preboot_init_mapped:
                premapped_flags = memory_region.PREBOOT_INIT_MAPPED
            else:
                premapped_flags = 0

            # Exelwtable
            text = memory_region(resource.instance_name+'_text', [], [], 0, premapped_flags | memory_region.EXECUTE | memory_region.CACHE | memory_region.LINKER_REGION)
            text.reader_pasids = resource.pasid_list

            # Read-only
            data = memory_region(resource.instance_name+'_rodata', [], [], 0, premapped_flags | memory_region.CACHE | memory_region.LINKER_REGION)
            data.reader_pasids = resource.pasid_list

            # Writable data
            data = memory_region(resource.instance_name+'_data', [], [], 0, premapped_flags | memory_region.CACHE | memory_region.LINKER_REGION)
            data.reader_pasids = resource.pasid_list
            data.writer_pasids = resource.pasid_list

            # Paged exelwtable
            paged_text = memory_region(resource.instance_name+'_paged_text', [], [], 0, premapped_flags | memory_region.EXECUTE | memory_region.PAGED_TCM | memory_region.LINKER_REGION)
            paged_text.reader_pasids = resource.pasid_list

            # Paged data
            paged_data = memory_region(resource.instance_name+'_paged_data', [], [], 0, premapped_flags | memory_region.PAGED_TCM | memory_region.LINKER_REGION)
            paged_data.reader_pasids = resource.pasid_list
            paged_data.writer_pasids = resource.pasid_list

def compile_task_headers():
    for name,task_resource in nametable.items():
        if isinstance(task_resource, task):
            task_header = open(task_resource.name.lower()+".h", "w")
            task_header.write("#include <sdk/lwpu/inc/lwtypes.h>\n\n")

            for local_resource, local_resource_id in task_resource.id_for_resource.items():
                task_header.write("#define %-20s     0x%08X\n" % (local_resource.name, local_resource_id))

            for t in tasks:
                task_header.write("#define %-20s     0x%08X\n" % (t.name, t.task_id))
                for addr_space in pasid_list:
                    if t.address_space_name == addr_space.name:
                        task_header.write("#define %-20s     0x%08X\n" % (t.address_space_name, addr_space.pasid))

            for region in regions:
                if len(region.reader_pasids) != 0:
                    if region.mapping & memory_region.FIXED_VA:
                        task_header.write("#define %-20s     0x%016XULL\n" % (region.name.upper(), region.virtual_address))
                    else:
                        task_header.write("extern LwU64 %s[];\n" % (region.instance_name))
                        task_header.write("#define %-20s     ((LwU64)%s)\n" % (region.name.upper(), region.instance_name))

                    if region.mapping & memory_region.INIT_ARG_REGION:
                        task_header.write("#define %s_ID8     %0sULL\n" % (region.name.upper(), region.id8))



            for log_resource in task_resource.logs:
                task_header.write("#define %s_ALLOW_LARGE_BUFFER  %d\n" % (log_resource.name, log_resource.region.size > 0x1000))
                task_header.write("#define %s_MAX_PTE             %d\n" % (log_resource.name, (log_resource.region.size + 0xfff) / 0x1000))
                body = ""
                template = """#define DEFINE_PREFIX()
    LwU64 prefix_buffer_local_put = 1;
    LwU64 prefix_buffer_size = 0;
    LwU64 prefix_pte_count = 0;

    LwU64 * prefix_buffer_page0 = (LwU64 *)&%s_buffer_instance;
    LwU64 * prefix_buffer_lwr_page;
    LwU64 prefix_buffer_pagetable[PREFIX_MAX_PTE];

    void prefix_log_entry(const LwU64 n_args, const LwU64 * args)
    {
        LwU64 total_args = n_args + 1;

        if (prefix_buffer_size == 0)
            return;

        for (LwU32 i = 0; i < total_args; i++)
        {
            if (i < n_args)
                prefix_buffer_lwr_page[prefix_buffer_local_put & 0x1FF] = args[i];
            else
                prefix_buffer_lwr_page[prefix_buffer_local_put & 0x1FF] = libosGetTimeNs();
            prefix_buffer_local_put++;
            if (prefix_buffer_local_put >= prefix_buffer_size)
                prefix_buffer_local_put = 1;
            if (PREFIX_ALLOW_LARGE_BUFFER && (prefix_buffer_size > 512) &&
                ((prefix_buffer_local_put & 0x1FF) < 2))
            {
                prefix_buffer_lwr_page = (LwU64 *)
                    prefix_buffer_pagetable[prefix_buffer_local_put >> 9];
            }
        }
        prefix_buffer_page0[0 /* streaming put */] += total_args;
    }

    void construct_prefix()
    {
        LwU32  access_perms;
        LwU64  size;

        prefix_buffer_page0 = (LwU64 *)&prefix_buffer_instance;

        /* How big was the buffer? */
        if (libosMemoryQuery(LIBOS_TASK_SELF, prefix_buffer_page0, &access_perms, 0, &size, 0, 0) == LIBOS_OK)
        {
            prefix_buffer_size = size / sizeof(LwU64);
            prefix_buffer_lwr_page = prefix_buffer_page0;

            /* Create a page table when needed. */
            if (PREFIX_ALLOW_LARGE_BUFFER && (size > 0x1000))
            {
                LwU64 offset = (LwU64)prefix_buffer_page0 - prefix_buffer_page0[1];
                LwU64 i;
                prefix_pte_count = (size + 0xFFF)/ 0x1000;
                for (i = 0; i < prefix_pte_count; i++)
                    prefix_buffer_pagetable[i] = prefix_buffer_page0[i + 1] + offset;
            }
        }
    }
""" % (log_resource.name.lower())
                template = template.replace('prefix', log_resource.name.lower())
                template = template.replace('PREFIX', log_resource.name)

                for line in template.splitlines():
                    body += "%-120s\\\n" % line

                body += "\n"
                body += "void %s_log_entry(LwU64 n_args, const LwU64 * args);\n" % (log_resource.name.lower())
                body += "#define %s(...) LIBOS_LOG_INTERNAL(%s_log_entry, __VA_ARGS__)\n" % (log_resource.name, log_resource.name.lower())
                task_header.write(body)
            task_header.close()


def source_path(str):
    if "/" in str:
        return str
    else:
        return "../../" + str

def object_path(str):
    return str

def source_to_object(str):
    return os.path.splitext(os.path.basename(str))[0]+".o"

def compile_makefile():
    images = filter(lambda r: isinstance(r, image), nametable.values())
    makefile = open("Makefile", "w")
    template = open(libos_path + '/config/Makefile.template', 'r').read()

    task_elfs = "\n".join(["	%s	\\" % referenced_image.name for referenced_image in images])
    template = template.replace("<%TASK_ELFS%>", task_elfs)

    task_elf_rules = ""
    for referenced_image in images:
        sources = " ".join(map(lambda path: source_path(path), referenced_image.sources))
        objects = " ".join(map(lambda path: object_path(path), referenced_image.objects))
        sources_paths = " ".join(map(lambda path: os.path.realpath(path), referenced_image.sources))
        dependencies = sources + " " + objects
        task_elf_rules += "%s: %s\n" % ( referenced_image.name, dependencies)
        for source in sources.split():
            source_object = source_to_object(source)
            objects += " " + source_object
            task_elf_rules += "\t@printf  \" [ %%-28s ] $E[92m$E[1m%%-14s$E[39m$E[0m  %%s\\n\" \"$(PREFIX)\" \" CC\" $(subst $(LW_SOURCE)/,,%s)\n" % (source)
            if referenced_image.cflags:
                task_elf_rules += "\t@$(CC) $(TASK_CFLAGS) %s %s -o %s -Wl,-r\n" % ( referenced_image.cflags, source, source_object)
            else:
                task_elf_rules += "\t@$(CC) $(TASK_CFLAGS) %s -o %s -Wl,-r \n" % ( source, source_object)
        task_elf_rules += "\t@printf  \" [ %%-28s ] $E[92m$E[1m%%-14s$E[39m$E[0m  %%s\\n\" \"$(PREFIX)\" \" LD\" %s\n" % (referenced_image.name)
        # OOM uses SIGKILL which results in error 137
        task_elf_rules += "\t@$(LD) %s -o %s -r -T $(LIBOS_SOURCE)/kernel/task.elf.ld || { [ $$? = 137 ] && { echo \"$$OOM_BANNER\"; rm -f %s; exit 1; }; }\n" \
            % ( objects, (referenced_image.name), (referenced_image.name))
    template = template.replace("<%TASK_ELF_RULES%>", task_elf_rules)
    template = template.replace("$(LIBOS_SOURCE)", libos_path)

    makefile.write(template)

def compile_acls(ldscript):
    # Emit memory descriptor array
    ldscript.write("""
    . = ALIGN(4);
    .acl : {
      LONG(acl_null - ADDR(.acl));    /* phdr_boot: readers */
      LONG(acl_null - ADDR(.acl));    /* phdr_boot: writers */

    acl_entry_manifest_acl = . ;
      LONG(acl_null - acl_entry_manifest_acl);    /* manifest_acl: readers */
      LONG(acl_null - acl_entry_manifest_acl);    /* manifest_acl: writers */

    acl_entry_manifest_init_arg_memory_region = . ;
      LONG(acl_null - acl_entry_manifest_init_arg_memory_region);    /* manifest_external_buffers: readers */
      LONG(acl_null - acl_entry_manifest_init_arg_memory_region);    /* manifest_external_buffers: writers */

""")

    for name, memory_resource in nametable.items():
        if isinstance(memory_resource, memory_region):
            ldscript.write("      acl_entry_%s = . ;                                 \n" % (memory_resource.name))
            ldscript.write("      LONG(acl_%s_readers - acl_entry_%s); // readers \n" % (memory_resource.name,memory_resource.name))
            ldscript.write("      LONG(acl_%s_writers - acl_entry_%s); // writers \n" % (memory_resource.name,memory_resource.name))

    ldscript.write("""
    acl_entry_logging = .;
      LONG(acl_null - acl_entry_logging);    /* phdr_logging: readers */
      LONG(acl_null - acl_entry_logging);    /* phdr_logging: writers */

    acl_null = . ;
      BYTE(0);

""")

    phdr_index = 3
    for name, memory_resource in nametable.items():
        if isinstance(memory_resource, memory_region):
            memory_resource.index = phdr_index

            ldscript.write("    acl_%s_readers = . ; \n" % (memory_resource.name))
            readers = sorted(set(memory_resource.reader_pasids) - set(memory_resource.writer_pasids))
            ldscript.write("      BYTE(%d); // count\n" % (len(readers)))
            for t in readers:
                ldscript.write("      BYTE(%d); // pasid\n" % (t))
            ldscript.write("\n")

            ldscript.write("    acl_%s_writers = . ; \n" % (memory_resource.name))
            ldscript.write("      BYTE(%d); // count\n" % (len(memory_resource.writer_pasids)))
            for t in sorted(set(memory_resource.writer_pasids)):
                ldscript.write("      BYTE(%d); // pasid\n" % (t))
            ldscript.write("\n")

            phdr_index = phdr_index + 1

    ldscript.write("""
    } :phdr_manifest_acls

""")

def compile_ldscript():
    # bring in random number support if requested
    if any(t for t in tasks if t.randomize_initial_registers):
        import random

    ldscript = open("assemble.ld", "w")

    ldscript.write( """
#define U(x) x
#define ULL(x) x
#define PF_X 1
#include "libos_defines.h"
OUTPUT_ARCH( "riscv" )
ENTRY(libos_start)

PHDRS
{
  /* All memory descriptors indices are valid PHDR indicies */
phdr_boot                                 PT_LOAD FILEHDR PHDRS;
phdr_manifest_acls                        LIBOS_PT_MANIFEST_ACLS;
phdr_manifest_init_arg_memory_region      LIBOS_PT_MANIFEST_INIT_ARG_MEMORY_REGION;
""")

    # PHDR entries and memory descriptor table entries correspond 1 to 1
    for name, memory_resource in nametable.items():
        if isinstance(memory_resource, memory_region):
            if  (memory_resource.mapping & memory_region.APERTURE_MMIO)   or         \
                (memory_resource.mapping & memory_region.APERTURE_FB)  or         \
                (memory_resource.mapping & memory_region.APERTURE_SYSCOH):
                ldscript.write("  phdr_%-30s       LIBOS_PT_PHYSICAL AT(%s) FLAGS(%s);" % (memory_resource.instance_name, memory_resource.physical_address, memory_resource.phdr_flags))
            elif memory_resource.mapping & memory_region.INIT_ARG_REGION:
                ldscript.write("  phdr_%-30s       LIBOS_PT_INIT_ARG_REGION FLAGS(%s);" % (memory_resource.instance_name, memory_resource.phdr_flags))
            else:
                ldscript.write("  phdr_%-30s       PT_LOAD FLAGS(%s);" % (memory_resource.instance_name, memory_resource.phdr_flags))

            if memory_resource.mapping & memory_region.FIXED_VA:
                ldscript.write("/* runtime: phdr_patch_va */\n")
            else:
                ldscript.write("\n")


    ldscript.write( """
  /*  Start of kernel / debug sections (must have corresponding access_control_list entry) */
  phdr_logging                      LIBOS_PT_LOGGING_STRINGS;
}

SECTIONS
{
  . = LIBOS_PHDR_BOOT_VA + SIZEOF_HEADERS;

  /* Data section starts at LIBOS_PHDR_BOOT_VA (the base of phdr_boot) as this contains the ELF headers */
  kernel_data_cached_start = LIBOS_PHDR_BOOT_VA;
  .gnext_data ALIGN(4096): { KEEP(**kernel-gh100.o(.data)) } : phdr_boot
  .ga10x_data ALIGN(4096): { KEEP(**kernel-ga10x.o(.data)) } : phdr_boot
  .ga100_data ALIGN(4096): { KEEP(**kernel-ga100.o(.data)) } : phdr_boot
  .tu10x_data ALIGN(4096): { KEEP(**kernel-tu10x.o(.data)) } : phdr_boot

  . = ALIGN(4096);
  code_cached_start = . ;
  .gnext_text ALIGN(4096): { KEEP(**kernel-gh100.o(.boot)) KEEP(**kernel-gh100.o(.text)) } : phdr_boot
  .ga10x_text ALIGN(4096): { KEEP(**kernel-ga10x.o(.boot)) KEEP(**kernel-ga10x.o(.text)) } : phdr_boot
  .ga100_text ALIGN(4096): { KEEP(**kernel-ga100.o(.boot)) KEEP(**kernel-ga100.o(.text)) } : phdr_boot
  .tu10x_text ALIGN(4096): { KEEP(**kernel-tu10x.o(.boot)) KEEP(**kernel-tu10x.o(.text)) } : phdr_boot
  . = ALIGN(4096);
  code_cached_size = ABSOLUTE(. - code_cached_start);

  .bootstrap :  {
    ASSERT(libos_start == . , "Entry point must be here");
    KEEP(*(.text.startup))
  } : phdr_boot

  .gnext_resident_text ALIGN(4096) : { KEEP(**kernel-gh100.o(.paged_text)) ; . = ALIGN(4096); KEEP(**kernel-gh100.o(.paged_text_endstone)) } : phdr_boot
  .gnext_resident_data ALIGN(4096) : { KEEP(**kernel-gh100.o(.paged_data)) } : phdr_boot
  .ga10x_resident_text ALIGN(4096) : { KEEP(**kernel-ga10x.o(.paged_text)) ; . = ALIGN(4096); KEEP(**kernel-ga10x.o(.paged_text_endstone)) } : phdr_boot
  .ga10x_resident_data ALIGN(4096) : { KEEP(**kernel-ga10x.o(.paged_data)) } : phdr_boot
  .ga100_resident_text ALIGN(4096) : { KEEP(**kernel-ga100.o(.paged_text)) ; . = ALIGN(4096); KEEP(**kernel-ga100.o(.paged_text_endstone)) } : phdr_boot
  .ga100_resident_data ALIGN(4096) : { KEEP(**kernel-ga100.o(.paged_data)) } : phdr_boot
  .tu10x_resident_text ALIGN(4096) : { KEEP(**kernel-tu10x.o(.paged_text)) ; . = ALIGN(4096); KEEP(**kernel-tu10x.o(.paged_text_endstone))} : phdr_boot
  .tu10x_resident_data ALIGN(4096) : { KEEP(**kernel-tu10x.o(.paged_data)) } : phdr_boot
""")

    # Compile the ACL database
    compile_acls(ldscript)

    # Generate task id's
    # Emit memory descriptor array
    ldscript.write("""
    . = ALIGN(8);
  .manifest : {
    phdr_patch_va_begin = . ;""")

    index = 3    # skip LIBOS_MANIFEST_HEADER / LIBOS_MANIFEST_PHDR_ACLS
    count = 0
    for name, memory_resource in nametable.items():
        if isinstance(memory_resource, memory_region):
            if (memory_resource.mapping & memory_region.FIXED_VA):
                ldscript.write("  QUAD(%d); QUAD(0x%08X); QUAD(0x%08X); \n" % (index, memory_resource.virtual_address, memory_resource.size))
                count = count + 1
            index = index + 1
    ldscript.write("notaddr_phdr_patch_va_count = ABSOLUTE(%d) ;\n" % (count))

    # Generate task id's
    # Emit memory descriptor array
    ldscript.write("""
    . = ALIGN(8);
    task_by_id = . ;""")
    for task_resource in tasks:
        ldscript.write("""
        QUAD(%s);""" % (task_resource.instance_name))
    ldscript.write("""
    notaddr_task_by_id_count = ABSOLUTE(%d);
    """ % (len(tasks)))

    # Generate debug security mask for each task
    ldscript.write("""
    . = ALIGN(8);
    pasid_debug_mask_by_id = . ;""")
    for pasid_resource in pasid_list:
        ldscript.write("""
        LONG(0x%08X);""" % (pasid_resource.debug_mask))

    # Emit ports and shuttles
    for name, resource in nametable.items():
        if isinstance(resource, shuttle):
            shuttle_resource = resource
            ldscript.write( """
    . = ALIGN(8);
    %s = . ;
""" % shuttle_resource.instance_name)
            if shuttle_resource.initial_wait_port:
                ldscript.write("""        LONG(%s); /* waiting_on_port */\n""" % (shuttle_resource.initial_wait_port.instance_name))
            else:
                ldscript.write("""        LONG(0); /* port_sender_link.next / waiting_on_port */\n""")
            ldscript.write("""        LONG(0); /* port_sender_link.prev */\n""")
            ldscript.write("""        LONG(0); /* reply_to_shuttle */\n""")
            ldscript.write("""        BYTE(0); /* state */\n""")
            ldscript.write("""        BYTE(0); /* origin_task */\n""")
            ldscript.write("""        BYTE(0); /* rsvd */\n""")
            ldscript.write("""        BYTE(%d); /* owner */\n""" % (shuttle_resource.owner.task_id))
            ldscript.write("""        QUAD(0); /* payload */\n""")
            ldscript.write("""        LONG(0); /* payload-size */\n\n""")
            ldscript.write("""        LONG(%d); /* task-local handle */\n\n""" % (shuttle_resource.owner.id_for_resource[shuttle_resource]))
        elif isinstance(resource, port):
            port_resource = resource
            ldscript.write( """
    . = ALIGN(8);
    %s = . ;
        LONG(%s);  /* waiting_senders.next */
        LONG(%s);  /* waiting_senders.prev */
            """ % (port_resource.instance_name, port_resource.instance_name, port_resource.instance_name))
            if port_resource.receiver_task.waiting_object == port_resource:
                ldscript.write("""LONG(%s_shuttle_default_instance);  /* waiting_future */""" % port_resource.receiver_task.name.lower())
            else:
                ldscript.write("""LONG(0);  /* waiting_future */""")




    # Generate tasks
    for name, task_resource in nametable.items():
        if isinstance(task_resource, task):
            task_state = "LIBOS_TASK_STATE_NORMAL"

            port_recv = 0
            if task_resource.port_recv != None:
                task_state = "LIBOS_TASK_STATE_PORT_WAIT"
                port_recv = task_resource.id_for_resource[task_resource.waiting_object]
            entry = "0"
            if task_resource.entry:
                entry = task_resource.entry

            ldscript.write("""
    . = ALIGN(8);
    %s = . ;
        QUAD(0);     /* timestamp */
        LONG(0);     /* task_header_scheduler::next */
        LONG(0);     /* task_header_scheduler::prev */
        LONG(0);     /* task_header_timer::next */
        LONG(0);     /* task_header_timer::prev */
    %s_task_completed_shuttles = . ;
        LONG(%s_task_completed_shuttles);     /* task_completed_shuttles::next */
        LONG(%s_task_completed_shuttles);     /* task_completed_shuttles::prev */
        BYTE(%s);    /* state */
        BYTE(0);     /* are we in the kernel? */
        BYTE(%d);    /* task id */
        BYTE(%d);    /* port_recv */
        BYTE(0x%02X);/* pasid */
        BYTE(LIBOS_CONTINUATION_NONE);     /* continuation */
        BYTE(0); BYTE(0); /* reserved[2]; */
    """ % (task_resource.instance_name, task_resource.instance_name, task_resource.instance_name, task_resource.instance_name,
           task_state, task_resource.task_id, port_recv, task_resource.pasid_object.pasid))

            # Write core registers
            for register in range(0, 32):
                if register==17 and task_resource.port_recv != None:
                    ldscript.write("""        QUAD(0); /* LIBOS_REG_IOCTL_SENDRECV_INCOMING_SIZE_A7 = 0 */\n""")
                elif register==2: # SP
                    ldscript.write("""        QUAD(ADDR(.section_%s) + 0x%08X); /* SP */\n""" % (task_resource.stack_object.instance_name, task_resource.stack_object.size))
                # RA must be zero to ensure that task return traps cleanly
                elif register==1:
                    ldscript.write("""        QUAD(0); /* R%d */\n""" % register)
                else:
                    if task_resource.randomize_initial_registers:
                        # TESTING MODE!
                        ldscript.write("""        QUAD(0x%016x); /* R%d */\n""" % (random.getrandbits(64),register))
                    else:
                        ldscript.write("""        QUAD(0); /* R%d */\n""" % register)

            # write control registers and context
            ldscript.write("""        QUAD(%s); /* xepc */\n""" % entry)
            ldscript.write("""        QUAD(0); /* xcause */\n""")
            ldscript.write("""        QUAD(0); /* xbadaddr */\n""")
            ldscript.write("""        LONG(0); /* priv_err_info */\n""")
            ldscript.write("""        LONG(0); /* reserved */\n""")
            ldscript.write("""        SHORT(0); /* kernel_read_pasid */\n""")
            ldscript.write("""        SHORT(0); /* kernel_write_pasid */\n""")
            ldscript.write("""        LONG(%d); /* resource_count */\n""" % (len(task_resource.id_for_resource)))
            ldscript.write("""        LONG(0); /* dma::tcm::memory */\n""")
            ldscript.write("""        LONG(0); /* dma::tcm::size */\n""")
            ldscript.write("""        QUAD(0); /* dma::tcm::va */\n""")

            ldscript.write("""        LONG(0); /* sleeping_on_shuttle */\n""")
            ldscript.write("""        LONG(0); /* sending_shuttle */\n""")
            ldscript.write("""        LONG(0); /* receiving_shuttle */\n""")
            ldscript.write("""        LONG(0); /* receiving_port */\n""")

            ldscript.write("""
    %s_resources = . ;""" % (task_resource.instance_name))
            for (resource,id) in sorted(task_resource.id_for_resource.items(),key=lambda kv: kv[1]):
                if isinstance(resource, shuttle):
                    ldscript.write("""
        ASSERT((%s & LIBOS_RESOURCE_PTR_MASK) == %s, "libos invalid memory layout");
        LONG(%s""" % (resource.instance_name,resource.instance_name,resource.instance_name))
                    if resource.hidden:
                        ldscript.write(")")
                    else:
                        ldscript.write(" | LIBOS_RESOURCE_SHUTTLE_GRANT)")
                elif isinstance(resource, port):
                    ldscript.write("""
        ASSERT((%s & LIBOS_RESOURCE_PTR_MASK) == %s, "libos invalid memory layout");
        LONG(%s""" % (resource.instance_name,resource.instance_name,resource.instance_name))

                    if task_resource == resource.receiver_task:
                        ldscript.write(" | LIBOS_RESOURCE_PORT_OWNER_GRANT")
                    if task_resource in resource.sender_tasks:
                        ldscript.write(" | LIBOS_RESOURCE_PORT_SEND_GRANT")
                    ldscript.write(")")
                else:
                    print("Unknown resource")
    ldscript.write("""
    . = ALIGN(4096);
    kernel_data_cached_end = . ;
    kernel_data_cached_size = ABSOLUTE(kernel_data_cached_end - kernel_data_cached_start) ;
  } :phdr_boot

  /* LIBOS header forms the end of the phdr_boot */
  .header :  {
    /* Magic */
    LONG(0x51CB7F1D);
    LONG(0);
    /* Additional memory required after end of ELF image */
    QUAD(%d);""" % init_heap_size)

    ldscript.write("    } : phdr_boot\n")

    # @todo place this in section inside PHDR phdr_manifest_init_arg_memory_region
    ldscript.write("    .memory_region_init_arguments : {\n")

    for name, memory_resource in nametable.items():
        if isinstance(memory_resource, memory_region):
            if memory_resource.mapping & memory_region.INIT_ARG_REGION:
                ldscript.write("""
        QUAD(%d);      /* index into memory_region_list  */
        QUAD(%s);      /* id8      */
    """ % (memory_resource.index, memory_resource.id8))

    ldscript.write("} : phdr_manifest_init_arg_memory_region\n")


    # Generate text/data sections for images
    # TODO: Could merge memory descriptors for images that always appear in pairs?
    for name, image_resource in nametable.items():
        if isinstance(image_resource, image):
            # Read-only data sections must have memsz == filesz, hence the fill pattern
            # This is required since a copy-on-write copy isn't made during load
            ldscript.write("    .section_%s_rodata_instance ALIGN(4096):  {\n" % (image_resource.instance_name))
            ldscript.write("        FILL(0);\n")            # Init mapped regions must have memsz == filesz
            ldscript.write("        KEEP(%s(.srodata*))\n" % (image_resource.name))
            ldscript.write("        KEEP(%s(.rodata*))\n" % (image_resource.name))
            if image_resource.preboot_init_mapped:
                ldscript.write("        .padding_%s = . ;\n" % (image_resource.instance_name))
                ldscript.write("        . = ((.padding_%s + 4096) &~4095) - 1;\n" % (image_resource.instance_name))
                ldscript.write("        BYTE(0); \n")
            else:
                ldscript.write("        . = ALIGN(4096); \n")
            ldscript.write("    } : phdr_%s_rodata_instance\n" % (image_resource.instance_name))

            ldscript.write("    .section_%s_data_instance ALIGN(4096):  {\n" % (image_resource.instance_name))
            ldscript.write("        KEEP(%s(.sdata*))\n" % (image_resource.name))
            ldscript.write("        KEEP(%s(.data*))\n" % (image_resource.name))
            ldscript.write("        KEEP(%s(.sbss*))\n" % (image_resource.name))
            ldscript.write("        KEEP(%s(.bss*))\n" % (image_resource.name))
            if image_resource.preboot_init_mapped:
                ldscript.write("        .padding_%s = . ;\n" % (image_resource.instance_name))
                ldscript.write("        . = ((.padding_%s + 4096) &~4095) - 1;\n" % (image_resource.instance_name))
                ldscript.write("        BYTE(0); \n")
            else:
                ldscript.write("        . = ALIGN(4096); \n")
            ldscript.write("    } : phdr_%s_data_instance\n" % (image_resource.instance_name))

            ldscript.write("    .section_%s_paged_data_instance ALIGN(4096) :  {\n" % (image_resource.instance_name))
            ldscript.write("        KEEP(%s(.paged_data*))\n" % (image_resource.name))
            if image_resource.preboot_init_mapped:
                ldscript.write("        .padding_%s = . ;\n" % (image_resource.instance_name))
                ldscript.write("        . = ((.padding_%s + 4096) &~4095) - 1;\n" % (image_resource.instance_name))
                ldscript.write("        BYTE(0); \n")
            else:
                ldscript.write("        . = ALIGN(4096); \n")
            ldscript.write("    } : phdr_%s_paged_data_instance\n" % (image_resource.instance_name))

            ldscript.write("    .section_%s_text_instance ALIGN(4096) :  {\n" % (image_resource.instance_name))
            ldscript.write("        KEEP(%s(.text*))\n" % (image_resource.name))
            if image_resource.preboot_init_mapped:
                ldscript.write("        .padding_%s = . ;\n" % (image_resource.instance_name))
                ldscript.write("        . = ((.padding_%s + 4096) &~4095) - 1;\n" % (image_resource.instance_name))
                ldscript.write("        BYTE(0); \n")
            else:
                ldscript.write("        . = ALIGN(4096); \n")
            ldscript.write("    } : phdr_%s_text_instance\n" % (image_resource.instance_name))

            ldscript.write("    .section_%s_paged_text_instance ALIGN(4096):  {\n" % (image_resource.instance_name))
            ldscript.write("        KEEP(%s(.paged_text*))\n" % (image_resource.name))
            if image_resource.preboot_init_mapped:
                ldscript.write("        .padding_%s = . ;\n" % (image_resource.instance_name))
                ldscript.write("        . = ((.padding_%s + 4096) &~4095) - 1;\n" % (image_resource.instance_name))
                ldscript.write("        BYTE(0); \n")
            else:
                ldscript.write("        . = ALIGN(4096); \n")
            ldscript.write("    } : phdr_%s_paged_text_instance\n" % (image_resource.instance_name))

    # Generate memory for memory_regions
    ldscript.write("\n")
    for name, memory_resource in nametable.items():
        if isinstance(memory_resource, memory_region):
            if memory_resource.mapping & memory_region.LINKER_REGION:
                pass
            elif memory_resource.mapping & memory_region.FIXED_VA:
                pass
            else:
                ldscript.write("    /* donor memory to initialize memory descriptor */\n")
                ldscript.write("    .section_%s ALIGN(4096) : { \n" % (memory_resource.instance_name))
                ldscript.write("        FILL(0); \n")
                ldscript.write("        %s = . ; \n" % (memory_resource.instance_name))
                if memory_resource.mapping & memory_region.PREBOOT_INIT_MAPPED:
                    ldscript.write("        . = . + 0x%08X; \n" % (memory_resource.size - 8))
                    ldscript.write("        QUAD(0); \n")
                else:
                    ldscript.write("        . = . + 0x%08X; \n" % (memory_resource.size))
                ldscript.write("    } : phdr_%s\n" % (memory_resource.instance_name))

    # Dump logging section into logging PHDR
    ldscript.write("    .logging ALIGN(4096) :  {\n")
    ldscript.write("        KEEP(.logging*)\n")
    ldscript.write("    } : phdr_logging\n")

    ldscript.write("}\n")

def compile():
    parser = argparse.ArgumentParser(description='LIBOS configure tool')
    parser.add_argument('out', help='Output directory')

    args = parser.parse_args()

    resolve_symbol('TASK_INIT')

    # Perform name resolution
    resolve_names()

    # Assign task-local ids
    assign_local_ids()

    # Wire ports
    wire()

    # Switch to final output directory for code generation
    os.chdir(args.out)

    # Generate per task headers
    compile_task_headers()

    # Generate Makefile
    compile_makefile()

    # Generate ldscript
    compile_ldscript()

address_space(name='PASID_DEFAULT', images=[])

atexit.register(compile)
