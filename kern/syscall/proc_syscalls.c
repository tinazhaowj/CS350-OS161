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
#include <limits.h>
#include <synch.h>
#include <addrspace.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include "opt-A2.h"

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
  *retval = curproc->p_pid;
  return (0);
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
int sys_fork(struct trapframe *tf, pid_t *retval){
  //create new process
  struct proc *cur_proc = curproc;
  struct proc *child_proc = proc_create_runprogram(cur_proc->p_name);
  //if a process wasn't created
  if(child_proc == NULL){
    kfree(cur_proc->p_name);
    return ENOMEM;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new process created.\n");


  //assign address space
  struct addrspace * child_addr;
  int err = as_copy(curproc->p_addrspace, &child_addr);
  //if address space is not assigned
  if(err){
    kfree(cur_proc->p_name);
    proc_destroy(child_proc);
    return ENOMEM;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new addrspace created.\n");
  spinlock_acquire(&child_proc->p_lock);
  child_proc->p_addrspace = child_addr;
  spinlock_release(&child_proc->p_lock);
  

  //create trapframe
  struct trapframe *ntf = kmalloc(sizeof(struct trapframe));
  if(ntf == NULL){
    kfree(cur_proc->p_name);
    as_destroy(child_proc->p_addrspace);
  }
  memcpy(ntf, tf, sizeof(struct trapframe));
  DEBUG(DB_SYSCALL, "Sys_fork: new trapframe created.\n");


  //create thread
  int error = thread_fork(curthread->t_name, child_proc, &enter_forked_process, ntf, 1);
  if(error){
    proc_destroy(child_proc);
    kfree(ntf);
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new fork created.\n");


  *retval = child_proc->p_pid;
  return 0;
}
#endif
