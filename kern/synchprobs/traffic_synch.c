#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>
/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;
static struct cv* cv_n;
static struct cv* cv_s;
static struct cv* cv_w;
static struct cv* cv_e;
static struct lock* mylock;
struct cv* cv_array[4]; 
volatile int count[4];
volatile unsigned int curDir = 5;
volatile unsigned int carCount = 0;

/*typedef struct Vehicle {
  Direction origin;
  Direction destination;
}Vehicle;
struct array* varray;*/ 

void intersection_sync_init(void);
void intersection_sync_cleanup(void);
void intersection_before_entry(Direction, Direction);
void intersection_after_exit(Direction, Direction); 
/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */

void
intersection_sync_init(void)
{
//  kprintf("start sync initialization\n");
  /* replace this default implementation with your own implementation */
  cv_n = cv_create("north");
  cv_s = cv_create("south");
  cv_w = cv_create("west");
  cv_e = cv_create("east");
  mylock = lock_create("mylock");

  cv_array[0] = cv_n;
  cv_array[1] = cv_e;
  cv_array[2] = cv_s;
  cv_array[3] = cv_w;
  
  for(int i = 0; i < 4; ++i)
    count[i] = 0;  

  //varray = array_create();
  //array_init(varray);

  if (mylock == NULL || cv_n == NULL || cv_s == NULL || cv_w == NULL || cv_e == NULL){// || varray == NULL) {
    panic("could not create intersection lock or condition variables");
  }
  //kprintf("sync successfully\n");
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */

void
intersection_sync_cleanup(void)
{
  //kprintf("start cleanup\n");
  /* replace this default implementation with your own implementation */
  KASSERT(mylock != NULL);
  KASSERT(cv_n != NULL && cv_s != NULL && cv_w != NULL && cv_e != NULL);
  //KASSERT(varray != NULL);

  lock_destroy(mylock);
  for(unsigned int i = 0; i < (sizeof(cv_array)/sizeof(cv_array[0])); ++i){
    cv_destroy(cv_array[i]);
  }

  //array_destroy(varray);
  //kprintf("finish cleaning up\n");
}

/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  // KASSERT(intersectionSem != NULL);
  //kprintf("********before entry*********\n");
  KASSERT(mylock != NULL);
  KASSERT(cv_n != NULL && cv_s != NULL && cv_w != NULL && cv_e != NULL);
  //KASSERT(varray != NULL);

  lock_acquire(mylock);
  //kprintf("acquire lock\n");
  //struct Vehicle* newcar = kmalloc(sizeof(Vehicle));
  //newcar->origin = origin;
  //newcar->destination = destination;

  //while(!intersection_canPass(origin)){}
  ++count[origin];
  //kprintf("car is from %d, to %d\n", origin, destination);
  //array_add(varray, newcar, NULL);
  if(curDir == 5) curDir = origin;

  if(origin != curDir || carCount > 10){
    //kprintf("this car should be put to waiting channel\n");
    cv_wait(cv_array[origin], mylock);
  } else {
    //kprintf("this car should leave now\n");
    ++carCount;
  }  

  //kprintf("going to release lock\n");
  lock_release(mylock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
grated thread synchtest: cpu 1 -> 2king threadMigrated thread synchtest: cpu 2 -> 0: synchtest
FMigrated thread synchtest: cpu 1 -> 0orkingMigrated thread synchtest: cpu 0 -> 1Migrated thread <boot #0>: cpu 1 -> 2 Migrated thread synchtest: cpu 2 -> 1thread: synchtest
FMigrated thread <boot #0>: cpu 2 -> 0orking threadMigrated thread synchtest: :cpu 1 -> 2 synchtest
FMigrated thread synchtest: cpu 0 -> 1orking Migratted thread synchtest: cpu 1 -> 3hread: synchtest
ForkMigrated thread <boot #0>: cpu 0 -> 1iMigrated thread synchtest: cpu 1 -> 2ng thread: synchtest
Migrated thread synchtest: cpu 1 -> 3Forking thread: synchtest
Forking thread: synchtest
ForMigrated thread synchtest: cpu 1 -> 2kiMigrated thread synchtest: cpu 2 -> 1ng thread: synchtest
FMigrated thread synchtest: cpu 1 -> 2orkiMigrated thread synchtest: cpu 2 -> 0ng thread: Migrated thread synchtest: cpu 0 -> 1Migrated thread <boot #0>: cpu 1 -> 0synchteMigrated thread synchtest: cpu 0 -> 1st
M* return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  // KASSERT(intersectionSem != NULL);
  //kprintf("after exit\n");
  KASSERT(mylock != NULL);
  KASSERT(cv_n != NULL && cv_s != NULL && cv_w != NULL && cv_e != NULL);
  //KASSERT(varray != NULL);

  lock_acquire(mylock);
  //kprintf("after exit lock acquired\n");
  //kprintf("the car leaving right now is from %d, to %d\n", origin, destination);
  --count[origin];
  unsigned int oriDir = origin;
  //bool lastloop = false;
  while(carCount > 10 || count[curDir] == 0){
    //switch to another direction and reset the car count
    //kprintf("in while loop\n");
    carCount = 0;
    if(curDir < 3)
      ++curDir;
    else
      curDir = 0;
    //if(lastloop) break;
    if(curDir == oriDir) curDir = 5;
  }
  //kprintf("new direction is %d\n", curDir);
  if(curDir != 5) cv_broadcast(cv_array[curDir], mylock);
  
  /*for(unsigned int i = 0; i < array_num(varray); ++i){
    Vehicle* car = array_get(varray,i); 
    if(car->origin == origin && car->destination == destination){
      array_remove(varray, i);
      --count[origin];
    }
  }*/
  //kprintf("going to relase after exit lock\n");
  lock_release(mylock);
}
