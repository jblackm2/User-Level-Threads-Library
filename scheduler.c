#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic_ops.h>
#include "scheduler.h"

#define STACK_SIZE (1024*1024)

struct queue ready_list;
AO_TS_t ready_list_lock;

void block(AO_TS_t * spinlock){
  spinlock_lock(&ready_list_lock);
  spinlock_unlock(spinlock);

  if(!is_empty(&ready_list)){
  
    struct thread * temp = current_thread;
    //Get next thread from ready list                                         
    struct thread* next_thread = thread_dequeue(&ready_list);

    //Switch current thread with next thread                                  
    set_current_thread(next_thread);
    next_thread->state = RUNNING;

    thread_switch(temp, current_thread);
    //Unlock spinlock upon returning from switch             
  }
  spinlock_unlock(&ready_list_lock);
}

void spinlock_lock(AO_TS_t* lock){
  while (AO_test_and_set_acquire(lock) == AO_TS_SET) {
  }
}

void spinlock_unlock(AO_TS_t* lock){
  AO_CLEAR(lock);
}

int kernel_thread_begin(void* fake){
  
  struct thread* kernel_thread;
  kernel_thread = (struct thread*) malloc(sizeof(struct thread));
  kernel_thread->state = RUNNING;
  set_current_thread(kernel_thread);
  
  while(1){
    yield();
  }
}

void scheduler_begin(int num_threads){
  //Set up ready list and current thread
  set_current_thread(malloc(sizeof(struct thread)));
  current_thread->state = RUNNING;
  ready_list.head = NULL;
  ready_list.tail = NULL;
  ready_list_lock = AO_TS_INITIALIZER;
  int i, fake;
  unsigned char* child_stack;
  fake = 1;

  for(i = 0; i< num_threads-1; i+=1){
    child_stack = malloc(STACK_SIZE) + STACK_SIZE;
    clone(kernel_thread_begin,child_stack,CLONE_THREAD | CLONE_VM | CLONE_SIGHAND | CLONE_FILES | CLONE_FS | CLONE_IO, &fake);
  }
}

void yield() {

  //Get the ready list lock
  spinlock_lock(&ready_list_lock);

  if(!is_empty(&ready_list)){
      //Add threads to ready list if thread is not finished or BLOCKED
      if(current_thread->state != DONE && current_thread->state != BLOCKED){
	current_thread->state = READY;
	thread_enqueue(&ready_list, current_thread); 
      }
   
      struct thread * temp = current_thread;
      //Get next thread from ready list
      struct thread* next_thread = thread_dequeue(&ready_list);
      next_thread->state = RUNNING;
      //Switch current thread with next thread
      set_current_thread(next_thread);
  
      thread_switch(temp, current_thread);      
  }   
  //Unlock spinlock upon returning from switch
  spinlock_unlock(&ready_list_lock);
}

void thread_wrap() {
  spinlock_unlock(&ready_list_lock);
  //Run initial function, set state to DONE, yield to other threads
  current_thread->initial_function(current_thread->initial_argument);
  current_thread->state = DONE;
  //This is the only point when we know the thread is DONE, so broadcast
  condition_broadcast(&current_thread->cond);
  yield();
}

struct thread* thread_fork(void(*target)(void*), void * arg){
  //Create new thread
  struct thread * new_thread;
  new_thread = malloc(sizeof(struct thread));
  new_thread->stack_pointer = malloc(STACK_SIZE) + STACK_SIZE;
  new_thread->initial_argument = arg;  
  new_thread->initial_function = target;
  mutex_init(&new_thread->mut);
  condition_init(&new_thread->cond);

  //Prepare new thread for running                                             
  new_thread->state = RUNNING;
 
  //Enqueue current thread
  current_thread->state = READY;

  //Get lock for ready list
  spinlock_lock(&ready_list_lock);
  thread_enqueue(&ready_list, current_thread);

  struct thread * temp = current_thread;
  set_current_thread(new_thread);

  thread_start(temp, current_thread);
  spinlock_unlock(&ready_list_lock);

  return new_thread;
}

void scheduler_end(){ //Now protected by spinlock ready_list_lock
  spinlock_lock(&ready_list_lock);
  //While threads are still on the ready list, yield to other threads
  while(!is_empty(&ready_list)){
    spinlock_unlock(&ready_list_lock);
    yield();
    spinlock_lock(&ready_list_lock);
  }
  spinlock_unlock(&ready_list_lock);
}

void mutex_init(struct mutex* m){
  m->held = 0;
  m->waiting_threads.head = NULL;
  m->waiting_threads.tail = NULL;
  m->spinlock = AO_TS_INITIALIZER;
}                             

void mutex_lock(struct mutex* m){
  spinlock_lock(&m->spinlock); 
  if(m->held == 0){ //If not locked, lock mutex
    m->held = 1;
    spinlock_unlock(&m->spinlock);
  }
  else{ //If locked, block and wait
    current_thread->state = BLOCKED; 
    thread_enqueue(&m->waiting_threads, current_thread);
    block(&m->spinlock);
  }
}

void mutex_unlock(struct mutex* m){
  spinlock_lock(&m->spinlock);

  if(is_empty(&m->waiting_threads)){//If there are no waiting threads, unlock
    m->held = 0;
  }
  else{ //Wake up a thread on waiting list
    struct thread* woken_thread = thread_dequeue(&m->waiting_threads);
    spinlock_lock(&ready_list_lock);
    woken_thread->state = READY;

    thread_enqueue(&ready_list, woken_thread); //Put on ready list
        
    spinlock_unlock(&ready_list_lock);
  }
  spinlock_unlock(&m->spinlock);
}

void thread_join(struct thread* t){
  
  if(t->state != DONE){ // Wait on condition if thread is not DONE
    condition_wait(&t->cond, &t->mut);
  }
}

void condition_init(struct condition* c){
  c->waiting_threads.head = NULL;
  c->waiting_threads.tail = NULL;
  c->spinlock = AO_TS_INITIALIZER;
}

void condition_wait(struct condition* c, struct mutex* m){
  mutex_unlock(m); 
  spinlock_lock(&c->spinlock);
  current_thread->state = BLOCKED;
  thread_enqueue(&c->waiting_threads, current_thread);
  block(&c->spinlock);
  mutex_lock(m); //Lock mutex upon waking from sleep
}

void condition_signal(struct condition* c){
  spinlock_lock(&c->spinlock);
  struct thread* signaled_thread = thread_dequeue(&c->waiting_threads);
  signaled_thread->state = READY;
  spinlock_lock(&ready_list_lock);
  thread_enqueue(&ready_list, signaled_thread);
  spinlock_unlock(&ready_list_lock);
  spinlock_unlock(&c->spinlock);
}

void condition_broadcast(struct condition* c){
  spinlock_lock(&c->spinlock);

  while(!is_empty(&c->waiting_threads)){ //signal all threads waiting on cond
    spinlock_unlock(&c->spinlock);
    condition_signal(c);
    spinlock_lock(&c->spinlock);
  }
  spinlock_unlock(&c->spinlock);
}

//For the safe memory operations
#undef malloc
#undef free
void * safe_mem(int op, void * arg) {
  static AO_TS_t spinlock = AO_TS_INITIALIZER;
  void * result = 0;

  spinlock_lock(&spinlock);
  if(op == 0) {
    result = malloc((size_t)arg);
  } else {
    free(arg);
  }
  spinlock_unlock(&spinlock);
  return result;
}
#define malloc(arg) safe_mem(0, ((void*)(arg)))
#define free(arg) safe_mem(1, arg)
