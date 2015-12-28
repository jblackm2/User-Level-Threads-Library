#include "queue.h"
#include <atomic_ops.h>
#define current_thread (get_current_thread())

//For malloc and free
extern void * safe_mem(int, void*);
#define malloc(arg) safe_mem(0, ((void*)(arg)))
#define free(arg) safe_mem(1, arg)

extern struct thread * get_current_thread();
extern void set_current_thread(struct thread*);

typedef enum {
  RUNNING, // The thread is currently running.
  READY,   // The thread is not running, but is runnable.
  BLOCKED, // The thread is not running, and not runnable.
  DONE     // The thread has finished. 
} state_t;

struct mutex{
  int held;
  struct queue waiting_threads;
  AO_TS_t spinlock;
};

struct condition{
  struct queue waiting_threads;
  AO_TS_t spinlock;
};

//Struct that represents the TCB
struct thread{
  unsigned char* stack_pointer;
  void (*initial_function)(void*);
  void* initial_argument;
  state_t state;
  struct mutex mut;
  struct condition cond;
};

void scheduler_begin();
struct thread* thread_fork(void(*target)(void*), void * arg);
void yield();
void scheduler_end();
void thread_join(struct thread*);

void mutex_init(struct mutex *);
void mutex_lock(struct mutex *);
void mutex_unlock(struct mutex *);

void condition_init(struct condition *);
void condition_wait(struct condition *, struct mutex *);
void condition_signal(struct condition *);
void condition_broadcast(struct condition *);

void spinlock_lock(AO_TS_t *);
void spinlock_unlock(AO_TS_t *);
void block(AO_TS_t *);
int kernel_thread_begin(void * arg);
