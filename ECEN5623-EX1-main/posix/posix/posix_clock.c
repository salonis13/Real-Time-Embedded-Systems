/****************************************************************************/
/* Function: nanosleep and POSIX 1003.1b RT clock demonstration             */
/*                                                                          */
/* Sam Siewert - 02/05/2011                                                 */
/*                                                                          */                                   
/* Edited By: Saloni Shah (06/10/2021)                                      */                                   
/****************************************************************************/

#include <pthread.h>  //include library file for POSIX thread
#include <unistd.h>   //get access to POSIX OS API
#include <stdio.h>   
#include <stdlib.h>
#include <time.h>     //get and manipulate date and time information
#include <errno.h>    //to define macros to report error conditions

//define macros 
#define NSEC_PER_SEC (1000000000)   //1e9ns in 1sec
#define NSEC_PER_MSEC (1000000)     //1e6ns in 1ms
#define NSEC_PER_USEC (1000)        //1e3ns in 1us
#define ERROR (-1)		    //return -1 in case of error
#define OK (0)			    //return 0 in case of no error
#define TEST_SECONDS (0)	    
#define TEST_NANOSECONDS (NSEC_PER_MSEC * 10)	//test time is 10ms

void end_delay_test(void);

//struct timespec {time_t tv_sec in seconds, long tv_nsec in nanoseconds}
static struct timespec sleep_time = {0, 0};
static struct timespec sleep_requested = {0, 0};
static struct timespec remaining_time = {0, 0};

static unsigned int sleep_count = 0;

pthread_t main_thread;                   //pthread_t  is a type to store unique thread identifier of each thread. main_thread of type pthread_t
pthread_attr_t main_sched_attr;	  //pthread_attr_t structure contents are used at the time of creating thread to determine attributes for                                               the thread. main_sched_attr is of type pthread_attr_t
int rt_max_prio, rt_min_prio, min;
struct sched_param main_param;	  //struct sched_param { int sched_priority }


/******************************************************/
/* Function to print the current scheduling policy    */
/* Parameters: none				                        */
/* Return type: void				                        */
/******************************************************/
void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());    //returns the  current  scheduling  policy  of  the thread identified by getpid(getpid()  returns  							the process ID (PID) of the calling process)

   switch(schedType)		//schedtype is an integer whose value we get from schedular info
   {
     case SCHED_FIFO:		//first in first out policy
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
     case SCHED_OTHER:		//standard round-robin time-sharing policy
           printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:		//round-robin policy
           printf("Pthread Policy is SCHED_RR\n");
           break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }
}


/******************************************************************/
/* Function to get difference between start time and stop time    */
/* Parameters: start time *fstart                                 */
/* 	       stop time *fstop			                              */
/* Return type: double type time difference in seconds            */
/******************************************************************/
double d_ftime(struct timespec *fstart, struct timespec *fstop)
{
	//get start time in seconds
  double dfstart = ((double)(fstart->tv_sec) + ((double)(fstart->tv_nsec) / 1000000000.0));
  	//get stop time in seconds
  double dfstop = ((double)(fstop->tv_sec) + ((double)(fstop->tv_nsec) / 1000000000.0));

  return(dfstop - dfstart); 
}


/******************************************************************/
/* Function to get difference between start time and stop time    */
/* Time difference is stored in delta_t		                     */
/* Parameters: start time *start                                  */
/*             stop time *stop                                    */
/*	       time difference delta_t			                        */
/* Return type: int 0 if function executed without error          */
/******************************************************************/
int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  //calculate difference of time in sec and nanosec
  int dt_sec=stop->tv_sec - start->tv_sec;
  int dt_nsec=stop->tv_nsec - start->tv_nsec;

  //printf("\ndt calculation\n");

  // case 1 - less than a second of change
  if(dt_sec == 0)	//if the time difference in seconds is 0
  {
	  //printf("dt less than 1 second\n");

	  if(dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)	//if dt_nsec is greater than or equal to 0 AND less than 1e9
	  {
	          //printf("nanosec greater at stop than start\n");
		  delta_t->tv_sec = 0;
		  delta_t->tv_nsec = dt_nsec;		//time difference in nanosec is same
	  }

	  else if(dt_nsec > NSEC_PER_SEC)    //if dt_nsec is greater than 1e9 then increment sec and update time diff in nsec to handle rollover
	  {
	          //printf("nanosec overflow\n");
		  delta_t->tv_sec = 1;
		  delta_t->tv_nsec = dt_nsec-NSEC_PER_SEC;
	  }

	  else // dt_nsec < 0 means stop is earlier than start
	  {
	         printf("stop is earlier than start\n");	//show error messsage for invalid time difference
		 return(ERROR);  
	  }
  }

  // case 2 - more than a second of change, check for roll-over
  else if(dt_sec > 0)		//now check if dt_sec is greater than 0
  {
	  //printf("dt more than 1 second\n");

	  if(dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)	//if dt_nsec is greater than or equal to 0 AND less than 1e9
	  {
	          //printf("nanosec greater at stop than start\n");
		  delta_t->tv_sec = dt_sec;		//time diff in sec is unchanged
		  delta_t->tv_nsec = dt_nsec;		//time diff in nanosec is unchanged
	  }

	  else if(dt_nsec > NSEC_PER_SEC)	//if dt_nsec is greater than 1e9, increment time diff in sec and update time diff in nanosec
	  {
	          //printf("nanosec overflow\n");
		  delta_t->tv_sec = delta_t->tv_sec + 1;
		  delta_t->tv_nsec = dt_nsec-NSEC_PER_SEC;
	  }

	  else // dt_nsec < 0 means roll over
	  {
	          //printf("nanosec roll over\n");
		  delta_t->tv_sec = dt_sec-1;			//decrement time diff in nanosec
		  delta_t->tv_nsec = NSEC_PER_SEC + dt_nsec;	//handle nanosecond rollover
	  }
  }

  return(OK);
}


//define real time start time, stop time, time difference and timing error for realtime clock
static struct timespec rtclk_dt = {0, 0};
static struct timespec rtclk_start_time = {0, 0};
static struct timespec rtclk_stop_time = {0, 0};
static struct timespec delay_error = {0,0};
static struct timespec my_rtclk_diff = {0, 0};
static struct timespec my_rtclk_start_time = {0, 0};
static struct timespec my_rtclk_stop_time = {0, 0};


//define clock type
//#define MY_CLOCK CLOCK_REALTIME
//#define MY_CLOCK CLOCK_MONOTONIC
//#define MY_CLOCK CLOCK_MONOTONIC_RAW
//#define MY_CLOCK CLOCK_REALTIME_COARSE
#define MY_CLOCK CLOCK_MONOTONIC_COARSE

//run the realtime clock for 100 tests
#define TEST_ITERATIONS (100)


/****************************************************************/
/* Function to calculate delay in realtime clock calculation    */
/* Parameters: pointer to threadid                              */
/* Return type: void                                            */
/****************************************************************/
void *delay_test(void *threadID)
{
  int idx, rc;
  unsigned int max_sleep_calls=3;
  int flags = 0;
  struct timespec rtclk_resolution;	//rtclk_resolution is a struct of time in sec and nsec

  sleep_count = 0;

  //finds resolution of MY_CLOCK and store it in struct timespec rtclk_resolution and checks for an error
  if(clock_getres(MY_CLOCK, &rtclk_resolution) == ERROR)
  {
      perror("clock_getres");
      exit(-1);
  }
  else
  {  	//prints the resolution of MY_CLOCK
      printf("\n\nPOSIX Clock demo using system RT clock with resolution:\n\t%ld secs, %ld microsecs, %ld nanosecs\n", rtclk_resolution.tv_sec, (rtclk_resolution.tv_nsec/1000), rtclk_resolution.tv_nsec);
  }

  //runs realtime clock test 100 times
  for(idx=0; idx < TEST_ITERATIONS; idx++)
  {
      printf("test %d\n", idx);

      /* run test for defined seconds */
      sleep_time.tv_sec=TEST_SECONDS;		//TEST_SECONDS=0
      sleep_time.tv_nsec=TEST_NANOSECONDS;	//TEST_NANOSECONDS=10ms
      sleep_requested.tv_sec=sleep_time.tv_sec;		//0
      sleep_requested.tv_nsec=sleep_time.tv_nsec;	//10ms

      /* start time stamp */ 
      clock_gettime(MY_CLOCK, &rtclk_start_time);	//clock_gettime retrieves time of the specified clock

      /* request sleep time and repeat if time remains */
      do
      {
          if(rc=nanosleep(&sleep_time, &remaining_time) == 0) break;	//suspends execution of thread until sleep_time has elapsed 				//here sleep_time = 10ms. if nanosleep is interrupted before sleep time passes, the remaining time will be updated
          sleep_time.tv_sec = remaining_time.tv_sec;
          sleep_time.tv_nsec = remaining_time.tv_nsec;
          sleep_count++;				//initially sleep count is 0
	  printf("nanosleep is getting interrupted, sleep count is incremented\n");
      } 
      while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0))	//executes until remaining time is greater than 0 or
		      && (sleep_count < max_sleep_calls));			//sleep_count<3
	//printf("Sleep count=%d\n",sleep_count);
	//printf("remaining time=%ld\n",remaining_time.tv_nsec);

      //gets current stop time after completing each sleep time
      clock_gettime(MY_CLOCK, &rtclk_stop_time);

      //calculates time diff between start and stop time and stores it in rtclk_dt
      delta_t(&rtclk_stop_time, &rtclk_start_time, &rtclk_dt);

      //calculates diff between requested sleep time(10ms) and the real time diff and considers it as delay
      delta_t(&rtclk_dt, &sleep_requested, &delay_error);

      end_delay_test();	//print time difference and delay error after every iteration
  }

}


/******************************************************/
/* Function to print the time difference calculated   */
/* Parameters: none                                   */
/* Return type: void                                  */
/******************************************************/
void end_delay_test(void)
{
    double real_dt;
#if 1 
  printf("MY_CLOCK start seconds = %ld, nanoseconds = %ld\n", 
         rtclk_start_time.tv_sec, rtclk_start_time.tv_nsec);
  
  printf("MY_CLOCK clock stop seconds = %ld, nanoseconds = %ld\n", 
         rtclk_stop_time.tv_sec, rtclk_stop_time.tv_nsec);
#endif

  real_dt=d_ftime(&rtclk_start_time, &rtclk_stop_time);
  printf("MY_CLOCK clock DT seconds = %ld, msec=%ld, usec=%ld, nsec=%ld, sec=%6.9lf\n", 
         rtclk_dt.tv_sec, rtclk_dt.tv_nsec/1000000, rtclk_dt.tv_nsec/1000, rtclk_dt.tv_nsec, real_dt);	//print time difference

#if 1
  printf("Requested sleep seconds = %ld, nanoseconds = %ld\n", 
         sleep_requested.tv_sec, sleep_requested.tv_nsec);

  printf("Sleep loop count = %d\n", sleep_count);
#endif
  printf("MY_CLOCK delay error = %ld, nanoseconds = %ld\n\n", 
         delay_error.tv_sec, delay_error.tv_nsec);		//print delay error
}

#define RUN_RT_THREAD

void main(void)
{
   int rc, scope;

   //get code start time
   clock_gettime(MY_CLOCK, &my_rtclk_start_time);
   printf("Before adjustments to scheduling policy:\n");
   print_scheduler();		//prints the current scheduling policy

#ifdef RUN_RT_THREAD
   pthread_attr_init(&main_sched_attr);       //initialize thread attributes object of main_sched_attr 
   pthread_attr_setinheritsched(&main_sched_attr, PTHREAD_EXPLICIT_SCHED);	//specifies from where main_sched_attr will inherit its attrib												utes
   pthread_attr_setschedpolicy(&main_sched_attr, SCHED_FIFO);	//set scheduling policy of main_thread_attr to first in first out

   rt_max_prio = sched_get_priority_max(SCHED_FIFO);	//returns maximum allowable priority to FIFO
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);	//returns minimum allowable priority to FIFO

   main_param.sched_priority = rt_max_prio;		//stores maximum allowable priority
   rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);	//sets SCHED_FIFO and main_param both for thread specified by getpid()


   if (rc)		//if rc is nonzero show error message
   {
       printf("ERROR; sched_setscheduler rc is %d\n", rc);
       perror("sched_setscheduler"); exit(-1);			//produce error message
   }

   printf("After adjustments to scheduling policy:\n");
   print_scheduler();			//print updated scheduling policy

   main_param.sched_priority = rt_max_prio;
   pthread_attr_setschedparam(&main_sched_attr, &main_param);	//sets main_sched_attr attributes to parameters specified by main_param

   rc = pthread_create(&main_thread, &main_sched_attr, delay_test, (void *)0);	//creates a thread main_thread with attributes 										main_sched_attr.Invokes delay_test with threadid=0

   if (rc)	//shows error message if there is an error creating the thread
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);
       perror("pthread_create");
       exit(-1);
   }

   pthread_join(main_thread, NULL);	//executes main_thread until it is terminated

   if(pthread_attr_destroy(&main_sched_attr) != 0)	//destroys main_sched_attr attributes so that it can be reinitialized
     perror("attr destroy");
#else
   delay_test((void *)0);
   printf("it is in else delay_test\n");
#endif

   printf("TEST COMPLETE\n\n");

   //get code end time
   clock_gettime(MY_CLOCK, &my_rtclk_stop_time);
   delta_t(&my_rtclk_stop_time, &my_rtclk_start_time, &my_rtclk_diff);	//calculate difference between code start and end time
   printf("Code start time - %ld sec, %ld nsec\n",my_rtclk_start_time.tv_sec,my_rtclk_start_time.tv_nsec);
   printf("Code stop time - %ld sec, %ld nsec\n", my_rtclk_stop_time.tv_sec, my_rtclk_stop_time.tv_nsec);
   printf("Time taken to execute the code is %ld sec, %ld msec, %ld usec, %ld nsec\n",
   my_rtclk_diff.tv_sec, my_rtclk_diff.tv_nsec/1000000, my_rtclk_diff.tv_nsec/1000, my_rtclk_diff.tv_nsec);
}


