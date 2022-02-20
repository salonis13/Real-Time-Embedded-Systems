// Sam Siewert, September 2016
//
// Check to ensure all your CPU cores on in an online state.
//
// Check /sys/devices/system/cpu or do lscpu.
//
// Tegra is normally configured to hot-plug CPU cores, so to make all available,
// as root do:
//
// echo 0 > /sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable
// echo 1 > /sys/devices/system/cpu/cpu1/online
// echo 1 > /sys/devices/system/cpu/cpu2/online
// echo 1 > /sys/devices/system/cpu/cpu3/online
//
// Check for precision time resolution and support with cat /proc/timer_list
//
// Ideally all printf calls should be eliminated as they can interfere with
// timing.  They should be replaced with an in-memory event logger or at least
// calls to syslog.

// This is necessary for CPU affinity macros in Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <sys/sysinfo.h>

#define USEC_PER_MSEC (1000)
#define NUM_CPU_CORES (1)
#define FIB_TEST_CYCLES (100)
#define NUM_THREADS (3)     // service threads + sequencer
sem_t semF10, semF20;

#define FIB_LIMIT_FOR_32_BIT (47)
#define FIB_LIMIT (10)

//variable declaration 
int abortTest = 0;
double start_time;

unsigned int seqIterations = FIB_LIMIT;
unsigned int idx = 0, jdx = 1;
unsigned int fib = 0, fib0 = 0, fib1 = 1;

double getTimeMsec(void);

//fibonacci sequence calculation
#define FIB_TEST(seqCnt, iterCnt)      \
   for(idx=0; idx < iterCnt; idx++)    \
   {                                   \
      fib0=0; fib1=1; jdx=1;           \
      fib = fib0 + fib1;               \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \

//thread parameters: thread number and time period
typedef struct
{
    int threadIdx;
    int MajorPeriods;
} threadParams_t;


/* Iterations, 2nd arg must be tuned for any given target type
   using timestamps
   
   Be careful of WCET overloading CPU during first period of LCM.
   
 */
 
/*********************************************************************/
/* Function to execute fibonacci sequence for 10ms                   */
/* Parameters: threadp - parameters of the thread                    */
/* Return type: void                                                 */
/*********************************************************************/
void *fib10(void *threadp)
{
   double event_time, run_time=0.0;
   int limit=0, release=0, cpucore, i;
   threadParams_t *threadParams = (threadParams_t *)threadp;
   unsigned int required_test_cycles;

   // Assume FIB_TEST short enough that preemption risk is minimal
   //calibrate number of cycles that fibonacci sequence can run in 10ms
   FIB_TEST(seqIterations, FIB_TEST_CYCLES); //warm cache
   event_time=getTimeMsec();
   FIB_TEST(seqIterations, FIB_TEST_CYCLES);
   run_time=getTimeMsec() - event_time;

   required_test_cycles = (int)(10.0/run_time);
   printf("F10 runtime calibration %lf msec per %d test cycles, so %u required\n", run_time, FIB_TEST_CYCLES, required_test_cycles);

   //terminate the thread by locking the semaphore
   while(!abortTest)
   {
     sem_wait(&semF10); //decrements semaphore pointed by semF10 by 1
     //printf("--------fib10 decremented\n"); 
       if(abortTest)
           break; 
       else 
           release++;

       cpucore=sched_getcpu();	//returns number of CPU on which the calling thread is currently executing
       printf("F10 start %d @ %lf on core %d\n", release, (event_time=getTimeMsec() - start_time), cpucore);
       			//calculate start time of the thread

       do
       {
           FIB_TEST(seqIterations, FIB_TEST_CYCLES);
           limit++;
       }
       while(limit < required_test_cycles);

       printf("F10 complete %d @ %lf, %d loops\n", release, (event_time=getTimeMsec() - start_time), limit);
       			//calculate completion time of the thread
       limit=0;
   }

   pthread_exit((void *)0);	//terminates the calling thread
}


/********************************************************/
/* Function to calculate fibonacci sequence for 20ms    */
/* Parameters: threadp - parameters of the thread       */
/* Return type: void                                    */
/********************************************************/
void *fib20(void *threadp)
{
   double event_time, run_time=0.0;
   int limit=0, release=0, cpucore, i;
   threadParams_t *threadParams = (threadParams_t *)threadp;
   int required_test_cycles;

   // Assume FIB_TEST short enough that preemption risk is minimal
   //calibrate the number of cycles that fibonacci sequence can run in 20ms
   FIB_TEST(seqIterations, FIB_TEST_CYCLES); //warm cache
   event_time=getTimeMsec();
   FIB_TEST(seqIterations, FIB_TEST_CYCLES);
   run_time=getTimeMsec() - event_time;

   required_test_cycles = (int)(20.0/run_time);
   printf("F20 runtime calibration %lf msec per %d test cycles, so %d required\n", run_time, FIB_TEST_CYCLES, required_test_cycles);

   //terminate the thread by locking the semaphore
   while(!abortTest)
   {
        sem_wait(&semF20);
	//printf("--------fib20 decremented\n");
        if(abortTest)
           break; 
        else 
           release++;

        cpucore=sched_getcpu();
        printf("F20 start %d @ %lf on core %d\n", release, (event_time=getTimeMsec() - start_time), cpucore);
			//calculate start time of the thread
        do
        {
            FIB_TEST(seqIterations, FIB_TEST_CYCLES);
            limit++;
        }
        while(limit < required_test_cycles);

        printf("F20 complete %d @ %lf, %d loops\n", release, (event_time=getTimeMsec() - start_time), limit);
			//calculate completion time of the thread
        limit=0;
   }

   pthread_exit((void *)0);
}


/*******************************************************/
/* Function to get current time of the machine in msec */
/* Parameters: void                                    */
/* Return type: returns resultant time in msec         */
/*******************************************************/
double getTimeMsec(void)
{
  struct timespec event_ts = {0, 0};

  clock_gettime(CLOCK_MONOTONIC, &event_ts);
  return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);
}


/*******************************************************************/
/* Function to print the current scheduling policy of the thread   */
/* Parameters : void                                               */
/* Return type: void                                               */
/*******************************************************************/
void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
     case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
       break;
     case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
     default:
       printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }

}


/********************************************************************************************/
/* Function to execute the threads in sequence based on specified time period and run time  */
/* Parameters : threadp - parameters of the thread                                          */
/* Return type : void                                                                       */
/********************************************************************************************/
void *Sequencer(void *threadp)
{
  int i;
  int MajorPeriodCnt=0;
  double event_time;
  threadParams_t *threadParams = (threadParams_t *)threadp;

  printf("Starting Sequencer: [S1, T1=20, C1=10], [S2, T2=50, C2=20], U=0.9, LCM=100\n");
  start_time=getTimeMsec();


  // Sequencing loop for LCM phasing of S1, S2
  do
  {

      // Basic sequence of releases after CI for 90% load
      //
      // S1: T1= 20, C1=10 msec 
      // S2: T2= 50, C2=20 msec
      //
      // This is equivalent to a Cyclic Executive Loop where the major cycle is
      // 100 milliseconds with a minor cycle of 20 milliseconds, but here we use
      // pre-emption rather than a fixed schedule.
      //
      // Add to see what happens on edge of overload
      // T3=100, C3=10 msec -- add for 100% utility
      //
      // Use of usleep is not ideal, but is sufficient for predictable response.
      // 
      // To improve, use a real-time clock (programmable interval time with an
      // ISR) which in turn raises a signal (software interrupt) to a handler
      // that performs the release.
      //
      // Better yet, code a driver that direction interfaces to a hardware PIT
      // and sequences between kernel space and user space.
      //
      // Final option is to write all real-time code as kernel tasks, more like
      // an RTOS such as VxWorks.
      //

      // Simulate the C.I. for S1 and S2 and timestamp in log
      printf("\n**** CI t=%lf\n", event_time=getTimeMsec() - start_time);
      sem_post(&semF10); 
      //printf("--------fib10 incremented\n"); 
      sem_post(&semF20);	//increments both semaphores by 1
      //printf("--------fib20 incremented\n");
	
      usleep(20*USEC_PER_MSEC);
      sem_post(&semF10);    //usleep suspends execution of the thread for define microseconds
      //printf("--------fib10 incremented\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      usleep(20*USEC_PER_MSEC);
      sem_post(&semF10);
      //printf("--------fib10 incremented\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      usleep(10*USEC_PER_MSEC);
      sem_post(&semF20);
      //printf("--------fib20 incremented\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      usleep(10*USEC_PER_MSEC);
      sem_post(&semF10);
      //printf("--------fib10 incremented\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      usleep(20*USEC_PER_MSEC);
      sem_post(&semF10);
      //printf("--------fib10 incremented\n");
      printf("t=%lf\n", event_time=getTimeMsec() - start_time);

      usleep(20*USEC_PER_MSEC);

      MajorPeriodCnt++;
   } 
   while (MajorPeriodCnt < threadParams->MajorPeriods);
 
   abortTest=1;
   sem_post(&semF10);
   //printf("--------fib10 incremented\n");
   sem_post(&semF20);
   //printf("--------fib20 incremented\n");
}


void main(void)
{
    int i, rc, scope;
    cpu_set_t threadcpu;	//cpu_set_t structure represents a set of CPUs
    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;
    pthread_attr_t main_attr;
    pid_t mainpid;
    cpu_set_t allcpuset;

    abortTest=0;

   printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());
   	//get_nprocs_conf returns number of processors configured by the OS
	//get_nprocs returns number of processors currently available in the system

   CPU_ZERO(&allcpuset);	//clears allcpuset

   for(i=0; i < NUM_CPU_CORES; i++)
       CPU_SET(i, &allcpuset);	//adds CPU i to allcpuset

   printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));    //returns number of CPUs in allcpuset


    // initialize the sequencer semaphores
    //
    if (sem_init (&semF10, 0, 0)) { printf ("Failed to initialize semF10 semaphore\n"); exit (-1); }
    if (sem_init (&semF20, 0, 0)) { printf ("Failed to initialize semF20 semaphore\n"); exit (-1); }
	//sem_init initializes semaphore at address pointed by semF10. 0 is the initial value of the semaphore

    mainpid=getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    //set thread attributes based on the specified parameters
    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if(rc < 0) perror("main_param");
    print_scheduler();

    //get the contention scope of a set of threads. It defines the set of threads against which a thread competes for resources
    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
      printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
      printf("PTHREAD SCOPE PROCESS\n");
    else
      printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    //set CPU processors for the threads and define thread attributes
    for(i=0; i < NUM_THREADS; i++)
    {

      CPU_ZERO(&threadcpu);
      CPU_SET(3, &threadcpu);

      rc=pthread_attr_init(&rt_sched_attr[i]);
      rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
      rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
      rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu); //sets cpu affinitiy mask of the thread

      rt_param[i].sched_priority=rt_max_prio-i;
      pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

      threadParams[i].threadIdx=i;
    }
   
    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Create Service threads which will block awaiting release for:
    //
    // serviceF10
    rc=pthread_create(&threads[1],               // pointer to thread descriptor
                      &rt_sched_attr[1],         // use specific attributes
                      //(void *)0,                 // default attributes
                      fib10,                     // thread function entry point
                      (void *)&(threadParams[1]) // parameters to pass in
                     );
    // serviceF20
    rc=pthread_create(&threads[2],               // pointer to thread descriptor
                      &rt_sched_attr[2],         // use specific attributes
                      //(void *)0,                 // default attributes
                      fib20,                     // thread function entry point
                      (void *)&(threadParams[2]) // parameters to pass in
                     );


    // Wait for service threads to calibrate and await relese by sequencer
    usleep(300000);
 
    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    threadParams[0].MajorPeriods=3;

    rc=pthread_create(&threads[0],               // pointer to thread descriptor
                      &rt_sched_attr[0],         // use specific attributes
                      //(void *)0,                 // default attributes
                      Sequencer,                 // thread function entry point
                      (void *)&(threadParams[0]) // parameters to pass in
                     );


   for(i=0;i<NUM_THREADS;i++)
       pthread_join(threads[i], NULL);


   printf("\nTEST COMPLETE\n");

}
             
