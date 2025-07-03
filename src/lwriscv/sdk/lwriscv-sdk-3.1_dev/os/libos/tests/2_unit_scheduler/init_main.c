#include "kernel.h"
#include "libos.h"
#include "task.h"
#include "mm.h"
#include "diagnostics.h"
#include "scheduler.h"
#include "lwriscv-2.0/sbi.h"

LwU32 LIBOS_SECTION_IMEM_PINNED rand() {
    static LwU64 LIBOS_SECTION_DMEM_PINNED(seed) = 1;
    seed = seed * 6364136223846793005 + 1442695040888963407;
    return (LwU32)(seed >> 32);
}

__attribute__((used,aligned(32))) LwU8 LIBOS_SECTION_DMEM_PINNED(KernelStack)[LIBOS_CONFIG_KERNEL_STACK_SIZE] = {0};

// Global scheduler structures
ListHeader LIBOS_SECTION_DMEM_PINNED(KernelSchedulerRunnable);
Task LIBOS_SECTION_DMEM_PINNED(TaskInit);

LibosCriticalSectionBehavior LIBOS_SECTION_DMEM_PINNED(criticalSection) = LibosCriticalSectionNone;
LIBOS_SECTION_IMEM_PINNED void ShuttleRetireSingle(Shuttle *shuttle) {}
void  LIBOS_SECTION_IMEM_PINNED KernelPortTimeout(Task *owner) {}
LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskReturn() { KernelPanic(); }
LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelExternalInterruptReport() { KernelPanic(); }
LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskPanic() 
{ 
  KernelPrintf("Task panic\n");
  KernelSbiShutdown();
}

static LwU64 globalTime = 0;

static int selectNextTask(Task * candidates, LwU64 * readyTime, LwBool print) {
  LwU64 ready = -1, priority = -1ULL;
  LwU64 index = -1;
  if (print)
    KernelPrintf("  Queue: ");
  for (int i = 0; i < 16; i++) {
    if (candidates[i].taskState == TaskStateReady)
    {
      if (print)
        KernelPrintf("[%d p:%d t:%lld] ", i, candidates[i].priority, readyTime[i]);
      if (candidates[i].priority < priority || 
          (candidates[i].priority == priority && readyTime[i] < ready)) {
        ready = readyTime[i];
        priority = candidates[i].priority;
        index = i;
      }
    }
  }
  if (print)
    KernelPrintf(" Selected [%d]\n", index);

  return index;
}

// The test runs directly in supervisor mode
__attribute__((used)) LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelInit()  
{
  KerneMpuLoad();
  KernelPrintf("Hello world from bare metal.\n");
  
  KernelTimerInit();
  
  // Initialize a task
#if LIBOS_FEATURE_PRIORITY_SCHEDULER
  TaskInit.priority  = 3;
#endif
  TaskInit.taskState = TaskStateReady;

  KernelSchedulerRunnable.next = KernelSchedulerRunnable.prev = LIBOS_PTR32_NARROW(&KernelSchedulerRunnable);

  // Load the scheduler
  KernelAssert(!KernelSchedulerIsTaskWaiting(&TaskInit));

  LwU64 readyTime[16];

  // Create worker tasks as waiting
  static Task tasks[16] = {0};
  for (int i = 0; i < 16; i++) {
#if LIBOS_FEATURE_PRIORITY_SCHEDULER
    tasks[i].priority = rand() % 3;
#endif    
    tasks[i].id = i;
    readyTime[i] = -1ULL;
    tasks[i].taskState = TaskStateWait;
    tasks[i].mpuEnables[0] = -1UL;
  }

  tasks[0].taskState = TaskStateReady;

  // Pick a random action
  KernelSchedulerLoad(&tasks[0]);
  KernelTimerLoad();

  // Exit the scheduler
  readyTime[0] = globalTime++;
  KernelSchedulerExit();

  int preempt = 0;
  for (int i = 0; i < 4000; i++) 
  {
     LwU32 randomTask = rand() % 16;
     switch(rand() % 4) {
       case 0:         
        if (!KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()))  {
          KernelPrintf("SchedulerWait %d\n", KernelSchedulerGetTask()->id);
          KernelSchedulerWait(0);
          KernelAssert(KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()));
        }
        break;

       case 1:
        // Wake a random task
        if (KernelSchedulerIsTaskWaiting(&tasks[randomTask])) {
          KernelPrintf("SchedulerRun %d\n", randomTask);
          KernelSchedulerRun(&tasks[randomTask]);
          
          // If the task we just switched away from is now running again
          // do not reset its position in the ready queue.
          if (KernelSchedulerGetTask()->id != randomTask)
            readyTime[randomTask] = globalTime++;
          KernelAssert(tasks[randomTask].taskState == TaskStateReady);
        }
        break;

      case 2:
          KernelPrintf("SchedulerPreempt %d\n", KernelSchedulerGetTask()->id);
          KernelSchedulerPreempt();
          preempt = 1;
          break;

      case 3:
      {
        KernelPrintf("SchedulerExit\n");
        if (preempt) {
            readyTime[KernelSchedulerGetTask()->id] = globalTime++;
            preempt = 0;
        }

        LwU64 newTask = selectNextTask(tasks, &readyTime[0], LW_TRUE);
        if (newTask != -1) 
        {
          // If we've switch the task, update the last ready time of the 
          // task we're moving away from?
          if (newTask != KernelSchedulerGetTask()->id) {
            readyTime[KernelSchedulerGetTask()->id] = globalTime++;
          }

          KernelSchedulerExit();
          KernelPrintf("  Exiting to task %d\n", KernelSchedulerGetTask()->id);
          KernelAssert(newTask == KernelSchedulerGetTask()->id && "Wrong task selected");
          KernelAssert(!KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()));
        }
        break;
      }
     }
  }

  KernelPrintf("Test Passed.\n");
  KernelSbiShutdown();
}

/*
Task *      KernelSchedulerGetTask();
LwBool              KernelSchedulerIsTaskTrapped(Task * task);
LwBool              KernelSchedulerIsTaskWaitingWithTimeout(Task * task);
LwBool              KernelSchedulerIsTaskWaiting(Task * task);
void KernelSchedulerWait(Shuttle * on, LwU64 timeoutWakeup);
void KernelSchedulerRun(Task * new);
LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskReturn();
void KernelSchedulerPreempt();
LIBOS_SECTION_TEXT_COLD void KernelSchedulerDirtyPagetables();
LIBOS_NORETURN void KernelSchedulerInterrupt(void);
LIBOS_SECTION_TEXT_COLD void KernelSchedulerLoad(Task * newRunningTask);
*/

void putc(char ch) {
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
}

void TrapReturn(Task *return_to)
{
  KernelSbiShutdown();
}

