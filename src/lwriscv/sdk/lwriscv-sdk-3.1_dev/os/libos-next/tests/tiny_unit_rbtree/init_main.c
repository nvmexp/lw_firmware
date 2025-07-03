#include "peregrine-headers.h"
#include "libos-config.h"
#include <stdint.h>
#include "kernel/task.h"
#include "libriscv.h"
#include "kernel/sched/port.h"
#include "libos.h"
#include "librbtree.h"
int main();
int worker();

__attribute__((used,section(".init.data")))   LwU64 init_stack[64];
__attribute__((used,section(".worker.data"))) LwU64 worker_stack[4096];

// Declare your MPU indices
enum {
    LIBOS_MPU_TEST_PRGNLCL = LIBOS_MPU_USER_INDEX,
    LIBOS_MPU_DATA_KERNEL,
    LIBOS_MPU_DATA_WORKER
};

LibosPortNameExtern(taskWorkerPort);
LibosTaskNameExtern(taskInit);
LibosTaskNameExtern(taskWorker);

// Init task
LibosManifestTask(taskInit, 0, main, init_stack, 
    LIBOS_MPU_CODE, 
    LIBOS_MPU_DATA_INIT_AND_KERNEL, 
    LIBOS_MPU_MMIO_KERNEL, 
    LIBOS_MPU_TEST_PRGNLCL);

// Worker task
LibosManifestTaskWaiting(taskWorker, 3, taskWorkerPort, worker, worker_stack, 
    LIBOS_MPU_CODE, 
    LIBOS_MPU_DATA_KERNEL,  
    LIBOS_MPU_DATA_WORKER, 
    LIBOS_MPU_MMIO_KERNEL, 
    LIBOS_MPU_TEST_PRGNLCL)

// User-space daemon
LibosManifestPort(libosDaemonPort);
LibosManifestGrantWait(libosDaemonPort, taskInit);
LibosManifestGrantSend(libosDaemonPort, taskWorker);
 
// Port between init and worker tasks
LibosManifestPort(taskWorkerPort);
LibosManifestGrantSend(taskWorkerPort, taskInit);
LibosManifestGrantWait(taskWorkerPort, taskWorker);

void putc(char ch) {
    *(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO) = ch;
    while (*(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO))
        ;
}

void LibosInitHookMpu()
{
    /*
    [0] LIBOS_MPU_CODE                   - mapping of all code
    [1] LIBOS_MPU_DATA_INIT_AND_KERNEL   - kernel mapping for init task
    [2] LIBOS_MPU_MMIO_KERNEL            - kernel pri mapping
    [3] LIBOS_MPU_TEST_PRI               - test app MMIO mapping
    [4] LIBOS_MPU_DATA_KERNEL            - kernel mapping for use in not init tasks
    [5] LIBOS_MPU_DATA_WORKER            - data section for worker task
    */
 
  // Create a mapping of the kernel for the other user-space tasks
    extern char taskinit_and_kernel_data[], taskinit_and_kernel_data_size[];
    LibosBootstrapMmap(
        LIBOS_MPU_DATA_KERNEL,
        (LwU64)&taskinit_and_kernel_data,
        (LwU64)&taskinit_and_kernel_data,
        (LwU64)&taskinit_and_kernel_data_size,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));

    // Create a data mapping for the worker task
    extern char taskworker_data[], taskworker_data_size[];
    LibosBootstrapMmap(
        LIBOS_MPU_DATA_WORKER,
        (LwU64)&taskworker_data,
        (LwU64)&taskworker_data,
        (LwU64)&taskworker_data_size,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL));          

    // Create a mmio mapping
    LibosBootstrapMmap(
        LIBOS_MPU_TEST_PRGNLCL,
        LW_RISCV_AMAP_INTIO_START,
        LW_RISCV_AMAP_INTIO_START,
        LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START,
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL)
    );
}

int main()
{
    LibosInitHookMpu();

    // Wakeup the worker task
    LibosPortSyncSend(ID(taskWorkerPort), 0, 0, 0, 0, 0);
    LibosPrintf("Init task exiting.\n");
    return LibosInitDaemon();
}

typedef LibosTreeHeader IntegerMap; 

typedef struct {
    LibosTreeHeader header;
    LwU64           key;
    LwU64           value;
} IntegerMapNode;

void IntegerMapConstruct(IntegerMap * integerMap) 
{
    LibosTreeConstruct(integerMap);
}

void IntegerMapInsert(IntegerMap * integerMap, IntegerMapNode * node)
{
    LibosTreeHeader * * slot = &integerMap->parent;
    node->header.parent = node->header.left = node->header.right = integerMap;
    
    while (!(*slot)->isNil)
    {
        node->header.parent = *slot;
        if (node->key < CONTAINER_OF(*slot, IntegerMapNode, header)->key)
            slot = &(*slot)->left;
        else
            slot = &(*slot)->right;
    }
    *slot = &node->header;

    LibosTreeInsertFixup(&node->header, 0);
}

void IntegerMapUnlink(IntegerMapNode * node)
{
    LibosTreeUnlink(&node->header, 0);
}

IntegerMapNode * IntegerMapFind(IntegerMap * integerMap, LwU64 key)
{
    LibosTreeHeader * i = integerMap->parent;

    while (!i->isNil)
        if (key < CONTAINER_OF(i, IntegerMapNode, header)->key)
            i = i->left;
        else if (key > CONTAINER_OF(i, IntegerMapNode, header)->key)
            i = i->right;
        else
            return CONTAINER_OF(i, IntegerMapNode, header);

    return 0;
}

void validate(LibosTreeHeader *pNode)
{
    if (pNode->isNil)
    {
        // Header node is black and has no children (later this may be used to track min/max)
        LibosAssert(!pNode->isRed && pNode->left->isNil && pNode->right->isNil);
        return;
    }

    // Red nodes have only black children
    if (pNode->isRed)
        LibosAssert(!pNode->left->isRed && !pNode->right->isRed);

    // Has left subtree?
    if (!pNode->left->isNil) {
        LibosAssert(CONTAINER_OF(pNode->left, IntegerMapNode, header)->key <= CONTAINER_OF(pNode, IntegerMapNode, header)->key);
        LibosAssert(pNode->left->parent == pNode);
        validate(pNode->left);
    }
    if (!pNode->right->isNil) {
        LibosAssert(CONTAINER_OF(pNode->right, IntegerMapNode, header)->key >= CONTAINER_OF(pNode, IntegerMapNode, header)->key);
        LibosAssert(pNode->right->parent == pNode);
        validate(pNode->right);  
    }    
}

LwU32 LIBOS_SECTION_IMEM_PINNED rand() {
    static LwU64 seed = 1;
    seed = seed * 6364136223846793005 + 1442695040888963407;
    return (LwU32)(seed >> 32);
}

int worker() {
    LibosPrintf("Worker task started...\n");

    static IntegerMap       integerMap;
    static IntegerMapNode   nodes[32] = {0};
    static LwBool           valid[32] = {0};

    IntegerMapConstruct(&integerMap);
    LwU64 id;
    for (int i = 0; i < 32; i++)
        nodes[i].key = i;

    for (int i = 0; i < 20000; i++) 
    {
        id = rand() % 32;

        if (!valid[id])
        {
            LibosPrintf("Inserting %lld\n", id);
            IntegerMapInsert(&integerMap, &nodes[id]);
            LibosAssert(IntegerMapFind(&integerMap, id));
            valid[id] = LW_TRUE;
        }
        else 
        {
            LibosPrintf("Erasing %lld\n", id);
            IntegerMapUnlink(&nodes[id]);
            LibosAssert(!IntegerMapFind(&integerMap, id));           
            valid[id] = 0;
        }

        id = rand() % 32;
        LibosPrintf("Searching %lld\n", id);
        LibosAssert(!!valid[id] == !!IntegerMapFind(&integerMap, id));

        validate(integerMap.parent);
    }  
    LibosPrintf("Test passed.\n");
    LibosProcessorShutdown();  
    return 0;
}
