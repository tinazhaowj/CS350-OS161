#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
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
#include <vfs.h>
#include "opt-A2.h"

static volatile pid_t pid_counter = 2;

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  //parent dead -> burn myself down
  //parent not dead -> tell parent that i'm dying (leave a message before i die)
  //child dead -> burn child down
  //child not dead -> tell them that i'm dying and that they are going to be orphans
  //main idea: if there is someone related to you who are still alive, tell them you are dying before you die
  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2
  lock_acquire(waitLock);
  struct Proc * child = findChildProc(curproc->p_pid);
  if(child != NULL){
    child->exit_code = _MKWAIT_EXIT(exitcode);
    child->dead = true;
  }
  cv_broadcast(waitCV, waitLock);

  struct Proc * parent = findParentProc(curproc->p_pid);
  while(parent != NULL){
    pid_t temp = parent->child_pid;
    removeFromProcTable(temp);
    parent = findParentProc(curproc->p_pid);
  }
  lock_release(waitLock);

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
  int result = 0;
  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2
  lock_acquire(waitLock);
  //if(pid != curproc->p_pid) result2 = ECHILD;
  struct Proc *children = findChildProc(pid);
  if(children == NULL) {
    result = ESRCH;
  }
  if(children->parent_pid != curproc->p_pid){ 
    result = ECHILD;
  }
  if (result > 0) {
    lock_release(waitLock);
    return(result);
  }

  while(!children->dead)
    cv_wait(waitCV, waitLock);

  //zombie
  exitstatus = children->exit_code;

  lock_release(waitLock);
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result > 0) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval){
  KASSERT(curproc != NULL);
  //Create process structure for child process
  struct proc *cur_proc = curproc;
  struct proc *child_proc = proc_create_runprogram(cur_proc->p_name);
  //if a process wasn't created
  if(child_proc == NULL){
    kfree(cur_proc->p_name);
    return ENPROC;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new process created.\n");


  //Create and copy address space (and data) from parent to child
  int err = as_copy(curproc_getas(), &(child_proc->p_addrspace));
  //if address space is not assigned
  if(err){
    kfree(cur_proc->p_name);
    proc_destroy(child_proc);
    return ENOMEM;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new addrspace created.\n");
  
  //Assign PID to child process 
  lock_acquire(pidLock);
  child_proc->p_pid = pid_counter++;
  lock_release(pidLock);

  //create the parent/child relationship
  struct Proc *newproc = kmalloc(sizeof(struct Proc));
  if(newproc == NULL){
    kfree(cur_proc->p_name);
    as_destroy(child_proc->p_addrspace);
  }
  lock_acquire(pidLock);
  newproc->parent_pid = curproc->p_pid;
  newproc->child_pid = child_proc->p_pid;
  newproc->dead = false;
  newproc->exit_code = 0;
  lock_release(pidLock);
  addToProcTable(newproc);

  //create trapframe
  struct trapframe *ntf = kmalloc(sizeof(struct trapframe));
  if(ntf == NULL){
    kfree(cur_proc->p_name);
    as_destroy(child_proc->p_addrspace);
    kfree(newproc);
    return ENOMEM;
  }
  memcpy(ntf, tf, sizeof(struct trapframe));
  DEBUG(DB_SYSCALL, "Sys_fork: new trapframe created.\n");


  //Create thread for child process
  //int error = thread_fork(curthread->t_name, child_proc, &enter_forked_process, ntf, 1);
  int error = thread_fork(curthread->t_name, child_proc, &enter_forked_process, ntf, 1);
  if(error){
    proc_destroy(child_proc);
    kfree(ntf);
    return error;
  }
  DEBUG(DB_SYSCALL, "Sys_fork: new fork created.\n");


  *retval = child_proc->p_pid;
  return 0;
}

//int execv(const char *program, char **args){
  //find file path
  //find the file
  //open the file
  //read the file
  //execute the file

//}

#endif
