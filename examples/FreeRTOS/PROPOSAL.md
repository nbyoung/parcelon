# Parcelon Wrapper for FreeRTOS on x86

## Motivation

FreeRTOS exposes a flat C API of handle-based functions. Parcelon can give it an object-based interface where each RTOS primitive is an object type with a concrete state struct holding its handle, and a method table wrapping its lifecycle operations. Targeting x86 via the FreeRTOS POSIX simulator enables development and testing on a desktop before deployment to embedded targets.

## Target Platform

FreeRTOS ships a POSIX/Linux simulator port (`FreeRTOS/Source/portable/ThirdParty/GCC/Posix`) that maps the scheduler onto `pthreads`. This port runs unmodified on x86 Linux — the same platform used to develop Parcelon itself — making it the natural first target.

## Object Types

Six object types cover the core FreeRTOS primitives:

### 1. Scheduler — `scheduler.c`

```c
typedef struct {
    int (*init )(void *self);
    int (*start)(void *self);
    int (*fini )(void *self);
} Scheduler;
```

`start` calls `vTaskStartScheduler`; `fini` calls `vTaskEndScheduler`. The state struct holds configuration (tick rate, heap size).

### 2. Task — `task.c`

```c
typedef struct {
    int (*init)   (void *self, char *name, void (*fn)(void *), void *arg,
                   int priority, int stack_depth);
    int (*suspend)(void *self);
    int (*resume) (void *self);
    int (*fini)   (void *self);
} Task;
```

State holds a `TaskHandle_t`. `init` calls `xTaskCreate`; `fini` calls `vTaskDelete`.

### 3. Queue — `queue.c`

```c
typedef struct {
    int (*init)   (void *self, int length, int item_size);
    int (*send)   (void *self, void *item,   int ticks);
    int (*receive)(void *self, void *buffer, int ticks);
    int (*fini)   (void *self);
} Queue;
```

State holds a `QueueHandle_t`. Wraps `xQueueCreate`, `xQueueSend`, `xQueueReceive`, `vQueueDelete`.

### 4. Semaphore — `semaphore.c`

```c
typedef struct {
    int (*init)(void *self, int initial, int max);
    int (*take)(void *self, int ticks);
    int (*give)(void *self);
    int (*fini)(void *self);
} Semaphore;
```

`max == 1` selects `xSemaphoreCreateBinary`; `max > 1` selects `xSemaphoreCreateCounting`.

### 5. Mutex — `mutex.c`

```c
typedef struct {
    int (*init)  (void *self);
    int (*lock)  (void *self, int ticks);
    int (*unlock)(void *self);
    int (*fini)  (void *self);
} Mutex;
```

Wraps `xSemaphoreCreateMutex` / `xSemaphoreCreateRecursiveMutex`.

### 6. Timer — `timer.c`

```c
typedef struct {
    int (*init) (void *self, char *name, int period_ticks,
                 int auto_reload, void (*fn)(void *));
    int (*start)(void *self, int ticks);
    int (*stop) (void *self, int ticks);
    int (*reset)(void *self, int ticks);
    int (*fini) (void *self);
} Timer;
```

Wraps the `xTimer*` family. State holds a `TimerHandle_t`.

## Implementations

Each object type gets two implementations, enabling compile-time polymorphism:

| Object | `freertos` implementation | `posix` implementation |
|--------|--------------------------|----------------------|
| Task | `xTaskCreate` / `vTaskDelete` | `pthread_create` / `pthread_join` |
| Queue | `xQueueCreate` / `xQueueSend` | POSIX message queue (`mq_open`) |
| Semaphore | `xSemaphoreCreate*` | `sem_init` / `sem_wait` / `sem_post` |
| Mutex | `xSemaphoreCreateMutex` | `pthread_mutex_t` |
| Timer | `xTimerCreate` | `timer_create` (POSIX interval timer) |
| Scheduler | `vTaskStartScheduler` | no-op (native thread scheduling) |

The `posix` implementation allows the same caller code to run natively on x86 without FreeRTOS, useful for unit testing RTOS-dependent logic.

## Directory Structure

```
freertos_parcelon/
  scheduler.c             # Scheduler object file
  task.c                  # Task object file
  queue.c                 # Queue object file
  semaphore.c             # Semaphore object file
  mutex.c                 # Mutex object file
  timer.c                 # Timer object file
  scheduler/
    freertos.c            # vTaskStartScheduler wrapper
    posix.c               # no-op
  task/
    freertos.c            # xTaskCreate wrapper
    posix.c               # pthread_create wrapper
  queue/
    freertos.c
    posix.c
  semaphore/
    freertos.c
    posix.c
  mutex/
    freertos.c
    posix.c
  timer/
    freertos.c
    posix.c
  scheduler.h             # Selector: -DSCHEDULER_POSIX
  task.h                  # Selector: -DTASK_POSIX
  queue.h
  semaphore.h
  mutex.h
  timer.h
  examples/
    producer_consumer/
      main.c
```

## Selector Headers

Each selector header follows the `output.h` pattern. For example, `task.h`:

```c
#include "import/task/_.tk"

#if defined(TASK_POSIX)
#include "import/task/posix.px"
#define TASK       px
#define TASK_STATE px_Posix
#else
#include "import/task/freertos.fr"
#define TASK       fr
#define TASK_STATE fr_Freertos
#endif
```

The complete implementation is selected by a single compiler flag; `main.c` is unchanged between targets.

## Example: Producer–Consumer

`examples/producer_consumer/main.c` demonstrates a queue shared between two tasks:

```c
#include "scheduler.h"
#include "task.h"
#include "queue.h"

QUEUE_STATE q;
TASK_STATE  producer_task, consumer_task;

static void producer(void *arg) { /* QUEUE->queue->send(&q, ..., portMAX_DELAY) */ }
static void consumer(void *arg) { /* QUEUE->queue->receive(&q, ..., portMAX_DELAY) */ }

int main(void) {
    SCHEDULER_STATE sched;
    SCHEDULER->scheduler->init(&sched);

    QUEUE->queue->init(&q, 10, sizeof(int));
    TASK->task->init(&producer_task, "producer", producer, &q, 1, 256);
    TASK->task->init(&consumer_task, "consumer", consumer, &q, 1, 256);

    SCHEDULER->scheduler->start(&sched);
    /* does not return under FreeRTOS */

    TASK->task->fini(&producer_task);
    TASK->task->fini(&consumer_task);
    QUEUE->queue->fini(&q);
    SCHEDULER->scheduler->fini(&sched);
    return 0;
}
```

Compiling without flags uses the `freertos` implementation via the POSIX simulator. Adding `-DTASK_POSIX -DQUEUE_POSIX -DSCHEDULER_POSIX` switches all three to native `pthreads` with no source changes.

## Open Questions

1. **`portMAX_DELAY` abstraction** — the `ticks` parameter leaks FreeRTOS terminology into the interface. A sentinel value (e.g. `-1` for "wait forever") could replace it, but requires a translation layer in the implement file.
2. **ISR variants** — FreeRTOS has `FromISR` variants of queue and semaphore operations. These could be a separate object type (`IsrQueue`) or additional methods on the same interface.
3. **Error reporting** — returning `int` (success/failure) discards FreeRTOS error codes (`errQUEUE_FULL`, etc.). A richer return scheme or an error-query method on the state could be considered.
4. **Heap and memory model** — FreeRTOS heap schemes (heap_1 through heap_5) affect whether `fini` can reclaim memory. A `Memory` object type could abstract `pvPortMalloc`/`vPortFree` in the same pattern.
