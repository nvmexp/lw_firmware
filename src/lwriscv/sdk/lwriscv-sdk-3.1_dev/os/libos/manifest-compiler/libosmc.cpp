#define MANIFEST_COMPILER
#include <unordered_map>
#include <map>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <random>
extern "C" {
#include "libos-config.h"
#include "libelf.h"
#include "libdwarf.h"
#include "libriscv.h"
#include "libos.h"
#include "../include/libos_syscalls.h"


#define LIBOS_SECTION_IMEM_PINNED
#define LIBOS_SECTION_TEXT_COLD
#include "../kernel/scheduler-tables.h"

// Include kernel scheduler tables
#include "../kernel/scheduler-tables.h"
}

std::random_device rd;     
std::mt19937 mt(0);  /* Constant seed for reproducibility */  
std::uniform_int_distribution<LwU32> dist32;
std::uniform_int_distribution<LwU64> dist64;

void help()
{
    printf("\nLIBOS Manifest Compiler\n");
    printf("    This tool processes the libosManifest objects declaraed in the ELF\n");
    printf("    to directly produce the task table, object tables, and object instances\n");
    printf("    required the the kernel.\n");
    printf("    \n\n");
    printf("usage: libosmc [input-elf] [output-elf]\n");
    printf("options: \n");
    printf("  -h      : Print this help screen.\n");
}

LibosElf64Header              * elf;
size_t                      tasks;
size_t                      freeAddress, endAddress;
size_t                      virtalAddressBase;

LIBOS_PTR32(Task)   va_task_from_id;
LIBOS_PTR32(Task)   * task_from_id;

LwU64 alloc(size_t size, LwU64 align) {
  LwU64 start = freeAddress;
  start = (start + align - 1) &~ (align -1);
  assert(start + size <= endAddress);
  freeAddress = start + size;
  return start;
}

template <class T>
struct Table {
    std::vector<std::pair<LwU64, T *> > table;
    uint32_t size = 0;

    void clear() { 
      table.clear();
      size = 0;
    }

    void prime(uint32_t effectiveSize)
    {
      // Round table up to next power of two
      while (size < effectiveSize)
        size = size ? size * 2 : 1;

      table.resize(size);
    }

    size_t probe_distance(size_t hash, size_t slot_index) const
    {
        return (slot_index - hash) & (size - 1);
    }

    void insert(std::pair<LwU64, T *> element)
    {
        LwU64 idx = element.first & (size -1);
        size_t d = 0, tries = 0;
        while (true)
        {
            if (table[idx].second == 0) {
                std::swap(table[idx], element);
                return;
            }

            size_t distance = probe_distance(table[idx].first, idx);
            if (distance < d) {
                std::swap(element, table[idx]);
                d = distance;
            }

            idx = (idx + 1) & (size - 1);
            ++d;
        }
    }

    size_t maxDisplacement() {
      size_t disp = 0;
      for (int idx = 0; idx < size; idx++) {
        if (!table[idx].second)
          continue;
        disp = std::max(disp,probe_distance(table[idx].first, idx));
      }
      return disp;
    }
};

template <class T>
struct Section
{
   T * begin_, * end_;
   LwU64  va_;
   Section(LibosElf64Header * elf, const char * name)
   {
     if (!LibosElfFindSectionByName(elf, name, (LwU8**)&begin_, (LwU8**)&end_, &va_)) {
       begin_ = end_ = 0;
     }
   }

   size_t size()                   { return end_-begin_; }
   LwU64 va_index(size_t i)        { return va_ + i * sizeof(T); }
   LwU64 va_ptr(void * i)          { return va_ + ((LwU64)i - (LwU64)begin_); }
   LwU64 index_of(void * ptr)      { return ((LwU64)ptr - va_) / sizeof(T); }
   bool contains(void * ptr)       { return (LwU64)ptr >= va_index(0) && (LwU64)ptr < va_index(size()); }
   T & operator[](size_t i)        { return begin_[i];}
   T * begin()                     { return begin_; }
   T * end()                       { return end_; }
};


int compile(const char * in, const char * out)
{
    //
    //  Read the input file
    //
    FILE * f = fopen(in, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    rewind(f);

    elf = (LibosElf64Header *)malloc(fsize);
    if (fread(elf, 1, fsize, f) != fsize) {
        fprintf(stderr, "Error reading ELF\n");
        exit(1);
    }
    fclose(f);

    Table<struct LibosManifestShuttleInstance> shuttle_table_plan;
    Table<struct LibosManifestObjectGrant>     object_table_plan;

    // Count the task handles
    Section<LwU8>                               taskNames(elf,    ".TaskNames");
    Section<LwU8>                               shuttleNames(elf, ".ShuttleNames");
    Section<LwU8>                               objectNames(elf,    ".ObjectNames");
    Section<Task>                               compiledTaskArray(elf, ".CompiledTaskArray");
    Section<struct LibosManifestTaskInstance>    manifestTasks(elf, ".ManifestTasks");
    Section<struct LibosManifestShuttleInstance> manifestShuttles(elf, ".ManifestShuttles");
    Section<Shuttle> compiledShuttleTable(elf, ".CompiledShuttleTable");
    Section<struct LibosManifestObjectGrant>    manifestObjectGrants(elf, ".ManifestObjectGrants");

    // Find the default shuttle ID's
    LibosDebugResolver resolver;
    LwU64 shuttleSyncSendId, shuttleSyncRecvId;
    LibosDebugResolverConstruct(&resolver, elf);
    assert(LibosDebugResolveSymbolToVA(&resolver, "shuttleSyncSend", &shuttleSyncSendId));
    assert(LibosDebugResolveSymbolToVA(&resolver, "shuttleSyncRecv", &shuttleSyncRecvId));

    //
    //  Locate the compiledTaskArray array
    //
    if (compiledTaskArray.size() != taskNames.size() ||
        taskNames.size() != manifestTasks.size()) {
      fprintf(stderr, ".CompiledTaskArray/.ManifestTasks/.TaskNames Section incorrectly sized.\n");
      exit(1);
    }

    // Create augmented shuttle list
    std::vector<struct LibosManifestShuttleInstance> manifestShuttlesEx(manifestShuttles.begin(), manifestShuttles.end());

    // Process tasks
    for (struct LibosManifestTaskInstance * i = manifestTasks.begin(); i!=manifestTasks.end(); ++i) {
        LibosTaskId id = (LibosTaskId)(LwU64)(i->name);
        LwU64 va = compiledTaskArray.va_index(id);
        compiledTaskArray[id].taskState   = TaskStateReady;
        compiledTaskArray[id].id = id;

#if LIBOS_FEATURE_PRIORITY_SCHEDULER
        compiledTaskArray[id].priority = i->priority;
#endif

        // Initialize the linked list of completed shuttles for this task
        compiledTaskArray[id].shuttlesCompleted.next = va + offsetof(Task, shuttlesCompleted);
        compiledTaskArray[id].shuttlesCompleted.prev = va + offsetof(Task, shuttlesCompleted);
        compiledTaskArray[id].state.registers[RISCV_SP] = (LwU64)i->sp;
        compiledTaskArray[id].state.registers[RISCV_RA] = LIBOS_XEPC_SENTINEL_NORMAL_TASK_EXIT;
        compiledTaskArray[id].state.xepc = (LwU64)i->pc;

        // Create synchronous reply port
        compiledTaskArray[id].portSynchronousReply.waitingSenders.next = va + offsetof(Task, portSynchronousReply.waitingSenders);
        compiledTaskArray[id].portSynchronousReply.waitingSenders.prev = va + offsetof(Task, portSynchronousReply.waitingSenders);
        compiledTaskArray[id].portSynchronousReply.waitingReceivers.next = va + offsetof(Task, portSynchronousReply.waitingReceivers);
        compiledTaskArray[id].portSynchronousReply.waitingReceivers.prev = va + offsetof(Task, portSynchronousReply.waitingReceivers);

#if LIBOS_FEATURE_USERSPACE_TRAPS
        // Create replayPort port
        compiledTaskArray[id].replayPort.waitingSenders.next = va + offsetof(Task, replayPort.waitingSenders);
        compiledTaskArray[id].replayPort.waitingSenders.prev = va + offsetof(Task, replayPort.waitingSenders);
        compiledTaskArray[id].replayPort.waitingReceivers.next = va + offsetof(Task, replayPort.waitingReceivers);
        compiledTaskArray[id].replayPort.waitingReceivers.prev = va + offsetof(Task, replayPort.waitingReceivers);

        // Fill out shuttles
        compiledTaskArray[id].crashSendShuttle.state = ShuttleStateReset;
        compiledTaskArray[id].crashSendShuttle.owner = id;
        compiledTaskArray[id].replayWaitShuttle.state = ShuttleStateReset;
        compiledTaskArray[id].replayWaitShuttle.owner = id;
#endif

        // Create default shuttles
        LibosManifestShuttleInstance send {i->name, (LibosShuttleName)shuttleSyncSendId};
        manifestShuttlesEx.push_back(send);
        LibosManifestShuttleInstance recv {i->name, (LibosShuttleName)shuttleSyncRecvId};
        manifestShuttlesEx.push_back(recv);

        // Pre-compute the mpu-sets [these could be changed later at runtime by init]
#if !LIBOS_FEATURE_PAGING
        for (uint32_t j = 0; j < sizeof(i->mpuMappings)/sizeof(*i->mpuMappings); j++) {
          uint32_t entry = i->mpuMappings[j];
          if (entry < 64)
            compiledTaskArray[id].mpuEnables[0] |= 1ULL << entry;
          else
            compiledTaskArray[id].mpuEnables[1] |= 1ULL << (entry-64);
        }
#endif

    }

    for (int tries = 0; tries < 1000000; tries++)
    {
      for (struct LibosManifestTaskInstance * i = manifestTasks.begin(); i!=manifestTasks.end(); ++i) {
          LibosTaskId id = (LibosTaskId)(LwU64)(i->name);
          compiledTaskArray[id].tableKey = dist32(mt);
      }
      shuttle_table_plan.clear();
      object_table_plan.clear();

      //
      //  Hash the shuttle entries
      //
      shuttle_table_plan.prime(manifestShuttlesEx.size());
      for (auto i = manifestShuttlesEx.begin(); i != manifestShuttlesEx.end();i++) {
          LwU64 hash = (((LwU64)i->name) * LIBOS_HASH_MULTIPLIER) >> 32;
          hash ^= compiledTaskArray[(LibosTaskId)(LwU64)i->task].tableKey;
          shuttle_table_plan.insert(std::pair<LwU64, LibosManifestShuttleInstance *>(
            hash,
            &*i
          ));
      }


      //
      //  Hash the port grant entries
      //
      object_table_plan.prime(manifestObjectGrants.size());
      for (auto i = manifestObjectGrants.begin() ; i != manifestObjectGrants.end() ; i++) {
          LwU64 hash = (((LwU64)i->name) * LIBOS_HASH_MULTIPLIER) >> 32;
          hash ^= compiledTaskArray[(LibosTaskId)(LwU64)i->task].tableKey ;

          object_table_plan.insert(std::pair<LwU64, LibosManifestObjectGrant *>(
            hash,
            i
          ));
      }    
      if (std::max(object_table_plan.maxDisplacement(), shuttle_table_plan.maxDisplacement()) <= LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT)
        break;
    }

    //
    //  Locate the compiledShuttleTable array
    //
    if (compiledShuttleTable.size() != shuttle_table_plan.table.size()) {
      fprintf(stderr, ".CompiledShuttleTable mis-sized %d %d.\n", compiledShuttleTable.size(), shuttle_table_plan.table.size());
      exit(1);
    }

    // Compile shuttle table
    std::map<std::pair<LwU32,LwU32>, LwU32> shuttle_by_handle;

    for (int i = 0; i < shuttle_table_plan.table.size(); i++) {
      auto && entry = shuttle_table_plan.table[i];
      if (!entry.second)
        continue;

      compiledShuttleTable[i].state = ShuttleStateReset;
      compiledShuttleTable[i].owner = (LwU64)entry.second->task;
      compiledShuttleTable[i].taskLocalHandle = (LwU64)entry.second->name;
      shuttle_by_handle[std::pair<LwU32, LwU32>(compiledShuttleTable[i].owner, compiledShuttleTable[i].taskLocalHandle)] = i;
    }

    printf("Shuttle hash-table constructed (size=%d, disp=%d).\n", shuttle_table_plan.table.size(), shuttle_table_plan.maxDisplacement());
    if (shuttle_table_plan.maxDisplacement() > LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT) {
        printf("Maximum displacement exceeded.\n");
    }

    //
    //  Locate the Port declarations
    //
    Section<struct LibosManifestObjectInstance> ManifestPorts(elf, ".ManifestPorts");
    Section<Port> compiledPortArray(elf, ".CompiledPortArray");
    if (compiledPortArray.size() < ManifestPorts.size() ) {
      fprintf(stderr, ".CompiledPortArray Section too small.\n");
      exit(1);
    }

    //
    //  Locate the Timer declarations
    //
    Section<struct LibosManifestObjectInstance> ManifestTimers(elf, ".ManifestTimers");
    Section<Timer> compiledTimerArray(elf, ".CompiledTimerArray");
    if (compiledTimerArray.size() < ManifestTimers.size() ) {
      fprintf(stderr, ".CompiledTimerArray Section too small.\n");
      exit(1);
    }    

    //
    //  Locate the Lock declarations
    //
    Section<struct LibosManifestObjectInstance> ManifestLocks(elf, ".ManifestLocks");
    Section<Lock> compiledLockArray(elf, ".CompiledLockArray");
    if (compiledLockArray.size() < ManifestLocks.size() ) {
      fprintf(stderr, ".CompiledLockArray Section too small.\n");
      exit(1);
    }    

    //
    //  Compile port array
    //
    for (int i = 0; i < ManifestPorts.size(); i++) {
      compiledPortArray[i].waitingSenders.next = compiledPortArray[i].waitingSenders.prev =
        compiledPortArray.va_ptr(&compiledPortArray[i].waitingSenders);
      compiledPortArray[i].waitingReceivers.next = compiledPortArray[i].waitingReceivers.prev =
        compiledPortArray.va_ptr(&compiledPortArray[i].waitingReceivers);
      }
    printf("Port array compiled (size = %d).\n", ManifestPorts.size());

    //
    //  Compile timer array
    //
    for (int i = 0; i < ManifestTimers.size(); i++) {
      compiledTimerArray[i].pending = LW_FALSE;
      compiledTimerArray[i].port.waitingSenders.next = compiledTimerArray[i].port.waitingSenders.prev =
        compiledTimerArray.va_ptr(&compiledTimerArray[i].port.waitingSenders);
      compiledTimerArray[i].port.waitingReceivers.next = compiledTimerArray[i].port.waitingReceivers.prev =
        compiledTimerArray.va_ptr(&compiledTimerArray[i].port.waitingReceivers);
    }
    printf("Timer array compiled (size = %d).\n", ManifestTimers.size());


    //
    //  Compile lock array
    //
    for (int i = 0; i < ManifestLocks.size(); i++) {
      // Shared lock fields
      compiledLockArray[i].holdCount = 0;
      compiledLockArray[i].holder = 0;

      // Embedded port
      auto & port = compiledLockArray[i].port; 
      port.waitingSenders.next = port.waitingSenders.prev =
        compiledLockArray.va_ptr(&port.waitingSenders);
      port.waitingReceivers.next = port.waitingReceivers.prev =
        compiledLockArray.va_ptr(&port.waitingReceivers);

      // Lock isn't aquired - so the shuttle is pending send
      auto & shuttle = compiledLockArray[i].unlockShuttle;
      shuttle.state = ShuttleStateSend;
      shuttle.errorCode = LibosOk;

      // Link the shuttle into the port as a sender
      port.waitingSenders.prev = port.waitingSenders.next = compiledLockArray.va_ptr(&shuttle.portLink);
      shuttle.portLink.next = shuttle.portLink.prev = compiledLockArray.va_ptr(&port.waitingSenders);
    }
    printf("Lock array compiled (size = %d).\n", ManifestLocks.size());

    //
    //  Locate the objectTable array
    //
    Section<ObjectTableEntry> objectTable(elf, ".CompiledObjectTable");
    if (objectTable.size() < object_table_plan.table.size()) {
      fprintf(stderr, ".CompiledObjectTable Section too small.\n");
      exit(1);
    }

    //
    //  Compile the object table
    //
    std::map<std::pair<LwU32,LwU32>, LwU32> port_by_handle;
    for (int i = 0; i < object_table_plan.table.size(); i++) {
      auto entry = object_table_plan.table[i].second;
      if (!entry)
        continue;

      assert (ManifestPorts.contains(entry->instance) || 
              ManifestTimers.contains(entry->instance) ||
              ManifestLocks.contains(entry->instance));

      objectTable[i].owner   = (LibosTaskId)(LwU64)entry->task;
      objectTable[i].id      = (LibosPortId)(LwU64)entry->name;

      // Default to not grant
      objectTable[i].grant = ObjectGrantNone;

      // All objects in the object table are relative to the portarray
      LwU64 offsetReference = compiledPortArray.va_index(0);

      if (ManifestPorts.contains(entry->instance))
      {
        uint32_t portIndex = ManifestPorts.index_of(entry->instance);
        objectTable[i].offset = compiledPortArray.va_index(portIndex) - offsetReference;

        if (entry->access == LibosManifestObjectGrant::portGrantWait)
          objectTable[i].grant = ObjectGrantWait;        
        if (entry->access == LibosManifestObjectGrant::portGrantSend)
          objectTable[i].grant = PortGrantSend;
        else if (entry->access == LibosManifestObjectGrant::portGrantAll)
          objectTable[i].grant = PortGrantAll;

        port_by_handle[std::pair<LwU32, LwU32>((LwU32)objectTable[i].owner, (LwU32)objectTable[i].id)] = 
          portIndex;
      }
      else if (ManifestTimers.contains(entry->instance))
      {
        uint32_t portIndex = ManifestTimers.index_of(entry->instance);
        objectTable[i].offset = compiledTimerArray.va_index(portIndex) - offsetReference;

        if (entry->access == LibosManifestObjectGrant::timerGrantWait)
          objectTable[i].grant = ObjectGrantWait;        
        if (entry->access == LibosManifestObjectGrant::timerGrantSet)
          objectTable[i].grant = TimerGrantSet;
        else if (entry->access == LibosManifestObjectGrant::timerGrantAll)
          objectTable[i].grant = TimerGrantAll;
      }
      else if (ManifestLocks.contains(entry->instance))
      {
        uint32_t lockIndex = ManifestLocks.index_of(entry->instance);
        objectTable[i].offset = compiledLockArray.va_index(lockIndex) - offsetReference;
        objectTable[i].grant = (ObjectGrant)(ObjectGrantWait | LockGrantAll);
      }
    }

    //
    //  Patch the tasks/ManifestPorts for waiting tasks
    //
    for (auto i = manifestTasks.begin(); i != manifestTasks.end(); i++)
    {
      LwU64 id = (LwU64)i->name;

      // Was this task waiting on a shuttle?
      if (i->waitingShuttle) {
        compiledTaskArray[id].taskState = TaskStateWait;

        // Find the shuttle
        auto si = shuttle_by_handle.find(std::pair<LwU32, LwU32>((LwU32)id, (LwU32)(LwU64)i->waitingShuttle));
        if (si == shuttle_by_handle.end()) {
          printf("Shuttle not found %d\n", i->waitingShuttle);
          exit(1);
        }
        Shuttle * shuttle = &compiledShuttleTable[si->second];

        // Find the port
        auto pi = port_by_handle.find(std::pair<LwU32, LwU32>((LwU32)id, (LwU32)(LwU64)i->waitingPort));
        if (pi == port_by_handle.end()) {
          printf("Port not found task:%d port:%d\n", id, (LwU32)(LwU64)i->waitingPort);
          exit(1);
        }
        Port * port = &compiledPortArray[pi->second];

        // Connect the shuttle to the port
        port->waitingReceivers.prev = port->waitingReceivers.next = compiledShuttleTable.va_ptr(&shuttle->portLink);
        shuttle->portLink.next = shuttle->portLink.prev = compiledPortArray.va_ptr(&port->waitingReceivers);

        // Setup the shuttle
        shuttle->state = ShuttleStateRecv;
        shuttle->waitingOnPort = (LwU32)(compiledPortArray.va_ptr(port));
        shuttle->grantedShuttle = 0;
        shuttle->payloadAddress = 0;
        shuttle->payloadSize = 0;
      }
    }

    printf("Port hash-table constructed (size=%d, manifestObjectGrants=%d, disp=%d).\n", object_table_plan.table.size(), manifestObjectGrants.size(), object_table_plan.maxDisplacement());
    if (object_table_plan.maxDisplacement() > LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT) {
        printf("Maximum displacement exceeded.\n");
    }

    f = fopen(out, "wb");
    fwrite(elf, 1, fsize, f);
    fclose(f);
    return 0;
}

int main(int argc, const char *argv[])
{
    const char * input_file = 0, *output_file = 0;
    for (uint32_t i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h")==0) {
                help();
                exit(1);
            } else if (strcmp(argv[i], "-o")==0) {
                output_file = argv[++i];
            }
            else
            {
                fprintf(stderr, "%s: Unknown option '%s'\n", argv[0], argv[i]);
                help();
                exit(1);
            }
        }
        else {
            if (input_file) {
                fprintf(stderr, "%s: Too many input files '%s'\n", argv[0], argv[i]);
                help();
                exit(1);
            }
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "%s: No input ELF\n", argv[0]);
        help();
        exit(1);
    }

    if (!output_file) {
        fprintf(stderr, "%s: No output ELF\n", argv[0]);
        help();
        exit(1);
    }

    return compile(input_file, output_file);
}
