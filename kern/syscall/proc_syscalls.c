#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <limits.h>
#include <synch.h>
#include <addrspace.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>

static volatile pid_t PID_counter = 1;

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2

  lock_acquire(wait_lock);
  struct procT * table = get_procT_c(procTable, curproc->p_pid);
  if(table != NULL){
    table->exitcode = _MKWAIT_EXIT(exitcode);
    table->running = false;
  }
  cv_broadcast(wait_CV, wait_lock);
  lock_release(wait_lock);
 
  lock_acquire(wait_lock); 
  struct procT * ptable = get_procT_p(procTable, curproc->p_pid);
  while(ptable != NULL){
    pid_t c_pid = ptable->child_pid;
    remove_procT(procTable, c_pid);
    ptable = get_procT_p(procTable, curproc->p_pid);
  }
  lock_release(wait_lock);

#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
#endif
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call  */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  KASSERT(curproc != NULL);
  *retval = curproc->p_pid;
#else
  *retval = 1;
#endif
  return(0);

}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
      userptr_t status,
      int options,
      pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2
  struct procT *table;
  int ret = 0;
  lock_acquire(wait_lock);
  table = get_procT_c(procTable, pid);
  if(table == NULL){
    ret = ESRCH;
  }
  if(table->parent_pid != curproc->p_pid){
    ret = ECHILD;
  }
  
  if(ret){
    return ret;
  }
  while(table->running){
    cv_wait(wait_CV, wait_lock);
  }
  exitstatus = table->exitcode;
  
  remove_procT(procTable, pid);
  lock_release(wait_lock);

  *retval = pid;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  return(0);

#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif
}


#if OPT_A2
int
sys_fork(struct trapframe *tf, pid_t *retval){
  KASSERT(curproc != NULL);
  
// Create process structure for child process
     // need to change the name of the child
  char *child_name = kmalloc(sizeof(char) * NAME_MAX);
    strcpy(child_name, curproc->p_name); 
    strcat(child_name, "_child"); 

  struct proc * child_proc = proc_create_runprogram(child_name);
  if(child_proc == NULL){
    kfree(child_name);
    return ENOMEM;
  }

//Create and copy address space
  struct addrspace * child_addr;
  int err = as_copy(curproc->p_addrspace, &child_addr);
  if(err){
    kfree(child_name);
    proc_destroy(child_proc);
    return ENOMEM; 
  }
  
  spinlock_acquire(&child_proc->p_lock);
  child_proc->p_addrspace = child_addr;
  spinlock_release(&child_proc->p_lock);

//Assign PID to child process and create the parent/child relationship
  struct procT *table = kmalloc(sizeof(struct procT));
  if(table == NULL){
    kfree(child_name);
    as_destroy(child_addr);
    proc_destroy(child_proc);
  }

  lock_acquire(PID_lock);
  PID_counter++;
  child_proc->p_pid = PID_counter; 
  lock_release(PID_lock);

  table->child_pid = child_proc->p_pid;
  table->parent_pid = curproc->p_pid;
  table->exitcode = 0;
  table->running = true;
  add_procT(procTable, table);


  struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  if(child_tf == NULL){
    kfree(child_name);
    kfree(table);
    as_destroy(child_addr);
    proc_destroy(child_proc);
  }
  memcpy(child_tf, tf, sizeof(struct trapframe));

//Create thread for child process
  int error = thread_fork(child_name, child_proc, enter_forked_process, child_tf, 1);  
  if(error){
    kfree(table);
    kfree(child_name);
    as_destroy(child_addr);
    kfree(child_tf);
    proc_destroy(child_proc);
  }
  
  *retval = child_proc->p_pid;
  return 0;

}
#endif
