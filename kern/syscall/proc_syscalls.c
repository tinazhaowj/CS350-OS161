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
#include <mips/trapframe.h>
#include "opt-A2.h"

static volatile pid_t pid_counter = 1;

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

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
  /* note: curproc cannot be used after this call */
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
  *retval = 1;
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
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int sys__fork(struct trapframe *tf, int *retval){
  (void) retval;
  //create new process
  struct proc *cur_proc = curproc;
  struct proc *child_proc = proc_create_runprogram(cur_proc->p_name);
  
  //if a process wasn't created
  if(child_proc == NULL){
    DEBUG(DB_SYSCALL, "Sys_fork: Couldn't create new process\n");
    return ENOMEM;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: New process created.\n");

  //assign address space
  as_copy(curproc_getas(), &(child_proc->p_addrspace));
  //if address space is not assigned
  if(child_proc->p_addrspace == NULL){
    DEBUG(DB_SYSCALL, "Sys_fork: Couldn't create new addrspace\n");
    proc_destroy(child_proc);
    return ENOMEM;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new addrspace created.\n");

  //assign pid?
/*
  lock_acquire(mylock);
  child_proc->pid = counter++;
  lock_release(mylock);
*/
  
  //create trapframe
  struct trapframe *ntf = kmalloc(sizeof(struct trapframe));
  if(ntf == NULL){
    DEBUG(DB_SYSCALL, "Sys_fork: Couldn't create new trapframe\n");
    return ENOMEM;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new trapframe created.\n");
  memcpy(ntf, tf, sizeof(struct trapframe));

  //create thread
  int error = thread_fork(curthread->t_name, child_proc, &enter_forked_process, ntf, 1);
  if(error){
    DEBUG(DB_SYSCALL, "Sys_fork: Couldn't create new fork\n");
    proc_destroy(child_proc);
    kfree(ntf);
    return error;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new fork created.\n");
  //*retval = child_proc->pid;
  return 0;
}
#endif
