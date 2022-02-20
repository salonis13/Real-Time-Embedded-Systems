/**********************************************************/
/* Code to demonstrate working of pthreads and	          */ 
/*  perform summation of numbers as task for each thread  */
/**********************************************************/

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#define NUM_THREADS 64	//number of threads to be executed


typedef struct		//required thread parameters
{
    int threadIdx;	//threadidx denotes thread number
} threadParams_t;


// POSIX thread declarations and scheduling attributes
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];


/**************************************************************************/
/* Function to perform summation as the task assigned to each thread      */
/* Parameters: *threadp - taken from argument passed in pthread_create    */
/* 			  (here thread number)                                            */
/* Return type:void                                                       */
/**************************************************************************/
void *counterThread(void *threadp)
{
    int sum=0, i;
    threadParams_t *threadParams = (threadParams_t *)threadp;    //set the parameters of the thread as parameters passed in function argument

    //printf("Executing thread number %d\n",threadParams->threadIdx);

    for(i=1; i < (threadParams->threadIdx)+1; i++)	//compute the summation of all numbers from 1 to threadIdx(currently running thread)
        sum=sum+i;
 
    printf("Thread idx=%d, sum[1...%d]=%d\n", 	//print the output of the summation
           threadParams->threadIdx,
           threadParams->threadIdx, sum);

    //printf("Terminated thread number %d\n",threadParams->threadIdx);
}


int main (int argc, char *argv[])
{
   int rc;
   int i;

   for(i=1; i <= NUM_THREADS; i++)	//create NUM_THREADS number of threads to be executed
   {
       threadParams[i].threadIdx=i;	//set thread parameter threadIdx as the thread number

       pthread_create(&threads[i],   // pointer to thread descriptor
                      (void *)0,     // use default attributes
                      counterThread, // thread function entry point(function to be invoked after the thread is created)
                      (void *)&(threadParams[i]) // parameters to pass in
                     );
       //printf("thread number %d created\n",i);
       //pthread_join(threads[i], NULL);
       //printf("thread number %d executed\n",i);
   }

  for(i=1;i<=NUM_THREADS;i++) 		//join all threads
  {
       pthread_join(threads[i], NULL);
       //printf("thread number %d executed\n",i);
  }

   printf("TEST COMPLETE\n");
}

