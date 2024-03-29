
//
//
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//
//
static buffer_t buffer[5][BUFSIZE];
static buffer_t user_input_buffer[1024];
static pthread_mutex_t  input_buffer_lock   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  data_buffer_lock    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  display_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
static int bufin = 0;
static int bufout = 0;
static int freeslots = 0;
static int filledslots = 0;
static volatile sig_atomic_t initdone = 0;
static int initerror = 0;
static pthread_once_t initonce = PTHREAD_ONCE_INIT;
static sem_t semitems;
static sem_t semslots;
static sem_t seminput;

//
//
static int bufferinit(void) { /* called exactly once by getitem and putitem  */
   int error;
   if (sem_init(&semitems, 0, 0))
      return errno;
   if (sem_init(&semslots, 0, 5)) {
      error = errno;
      sem_destroy(&semitems);
   }                                      /* free the other semaphore */
   if (sem_init(&seminput, 0, 0)) {
      error = errno;
      sem_destroy(&semslots);      return error;
      sem_destroy(&semitems);      return error;
   }
   return 0;
}

//
//
static void initialization(void) {
   initerror = bufferinit();
   if (!initerror)
      initdone = 1;
}

//
//
static int bufferinitonce(void) {          /* initialize buffer at most once */
   int error;
   if ((error = pthread_once(&initonce, initialization)) )
      return error;
   return initerror;
}

//
//
int getitem(buffer_t *item) {  /* remove item from buffer and put in *itemp */
   int error;
   if (!initdone)
      bufferinitonce();
   while (((error = sem_wait(&semitems)) == -1) && (errno == EINTR)) ;
   if (error)
      return errno;
   if ((error = pthread_mutex_lock(&data_buffer_lock)))
      return error; 
   strncpy(item,buffer[bufout],1024);  //copy string
   //*itemp = buffer[bufout];
   bufout = (bufout + 1) % 5;

   filledslots--;
   freeslots++;  

   printf("2..get item \n");
   if ((error = pthread_mutex_unlock(&data_buffer_lock)))
      return error;
   if (sem_post(&semslots) == -1)  
      return errno; 
   return 0; 
}

//
//
int putitem(buffer_t *item) {                    /* insert item in the buffer */
   int error;
   if (!initdone)
      bufferinitonce();
   while (((error = sem_wait(&semslots)) == -1) && (errno == EINTR)) ;
   if (error)
      return errno;
   if ((error = pthread_mutex_lock(&data_buffer_lock)))
      return error;    
   //buffer[bufin] = item;

   strncpy(buffer[bufin],item,1024);  //copy string
   bufin = (bufin + 1) % 5;
   
   filledslots++;    
   freeslots--; 
   printf("1..put item \n");
   if ((error = pthread_mutex_unlock(&data_buffer_lock)))
      return error; 
   if (sem_post(&semitems) == -1)  
      return errno; 
   return 0; 
}


//
//
void *user_input_thread(void *arg)
{
  
   int ret;

   char pdatabuf[1024];


   //pthread_exit(NULL); 
   while(1){

     printf("please input a maximum of 1023 character string - :)");
     fflush(stdout);
     
     ret = fgets(pdatabuf,sizeof(pdatabuf),stdin);    //produce
     if(ret == 0) exit(3); //fatal error
 
     //printf("data input is %s\n", pdatabuf);
 
     pthread_mutex_lock(&input_buffer_lock);

     strncpy(user_input_buffer,pdatabuf,sizeof(user_input_buffer));

     pthread_mutex_unlock(&input_buffer_lock);

     //printf("data input is %s\n", user_input_buffer);
     sem_post(&seminput);//V()
    
     //sched_yield();


   }//end of while loop   

pthread_exit(NULL);

}


void *producer_thread(void *arg)
{
  
   int ret;
   char pdatabuf[1024];


   while(1){

     //fgets(pdatabuf, sizeof(pdatabuf), stdin);                 //produce

     sem_wait(&seminput);    //P() 

     pthread_mutex_lock(&input_buffer_lock);

     ret = putitem(user_input_buffer);

     pthread_mutex_unlock(&input_buffer_lock);

     if(ret != 0) exit(3); //fatal error

   }   

pthread_exit(NULL);

} 

//
//
void *consumer_thread(void *arg)
{
    
   int ret;
   char cdatabuf[1024];

   while(1){

        ret = getitem(cdatabuf);                             //consume
        if(ret != 0) exit(4); //fatal error
        //printf("the received data is %s\n", cdatabuf);
   }

pthread_exit(NULL);

} 

void *display_status_thread(void *arg)
{
    
   int ret;
   char cdatabuf[1024];

   sigset_t set1;

   sigemptyset(&set1);
   sigaddset(&set1,SIGINT);

   pthread_sigmask(SIG_UNBLOCK,&set1,NULL);

   while(1){

        pthread_mutex_lock(&data_buffer_lock);

        //printf("the free slots is %d \n and filled slots is  %d\n",\
                freeslots,filledslots );
        pthread_testcancel(); //whenever this api is called, 
                              //cancellation request is checked
        pthread_mutex_unlock(&data_buffer_lock);
   }

pthread_exit(NULL);  //it is the responsibility of pthread_exit() of 
                     //a thread to return a pointer when a thread
                     //terminates - this pointer will be collected 
                     //by pthread_join(), if developer is interested !!!
                     //mostly taken care by library with some help
                     //from system space

}


//
//
//these library thread ids , in user space - 
//these are different from system space thread ids - 
//these are abstract and must not be directly used
//by developers - only via appropriate library APIs
//do not print their values - do not ask what is that ???
//library thread id may be used by the library to 
//manage an user-space TD 

//although there is good info. in the manual pages
//of threading library, it will not describe the objects
//and internals of threading library !!!

//these thread ids must be global - do not allocate them
//in stack memory !!!

void* test_thread1(void *arg)
{

    printf("this is the first thread\n"); 
    while(1); //you can add your job, in this block  
    pthread_exit(NULL); 

}


void* test_thread2(void *arg)
{

    printf("this is the second thread\n"); 
    while(1); //we can add another job, in this block
    pthread_exit(NULL); 

}


void* test_thread3(void *arg)
{

    printf("this is the third  thread\n"); 
    while(1); //we can add another job, in this block 
    pthread_exit(NULL); 

}

//these are user-space thread objects, as per 
//thread library requirements 
//pthread_t is an abstract data type, which 
//we must use - this data type is not described
//, in the pthreads library manual pages 
//man pthread_create() ???

pthread_t thid1,thid2,thid3,thid4;
int main()
{

   int ret;

   sigset_t set1,set2;
   //block all signals in the signal mask field of the main thread !!    

   sigfillset(&set1);

   sigprocmask(SIG_BLOCK,&set1,&set2);

   //install all the signal handlers here-before any thread is 
   //created
  

   //param2 is used to set the attributes - we will see later
   //if param2 is NULL, default attributes are used by the 
   //library !!!

   //param3 is the address of the method associated with 
   //this thread - this will be managed by the library
   //and also passed to system call API !!!
   //methods used with pthread_create() must follow certain
   //standard prototype - see above methods !!!
   //param4 is the pointer , which will be passed to the
   //thread's method - developer is free to pass NULL, if
   //not needed
   //this pointer must point to a global memory area in the
   //process - it can global static / global dynamic,
   //not a stack area !!! 
   //if pthread_create() has successfully created a new thread,
   //internally, td will be created and other formalities will
   //be completed - in addition, pthread_create() will return 0
   //if pthread_create() encounters error, new thread will not 
   //be created and it will return a +ve value - this +ve value
   //is the error code - to interpret this error code, you must
   //use strerror_r() - in addition, as a developer, you may 
   //also look into /usr/include/asm-generic/errno-base.h  and
   //other related header files !!! 

   //pthread_create() creates a new lwp / thread, in a 
   //process/application ??
   //if pthread_create() is successful, what happens to 
   //the newly create thread/lwp ??
   //
   //td + kernel stack + user-stack + other house-keeping 
   //is done, for the newly created thread - eventually, 
   //td's state is set to ready and added to appropriate Rq??
   //
   //when will this newly created thread/lwp/td be scheduled ???
   //it depends on the scheduling policies and parameters
   //of this thread/td and other threads/tds, in the system ???
   //
   //all threads of a process are equal - meaning, they are 
   //siblings of a process - there is no hierarchy - this 
   //is unlike parent / child relationship
   //it is common to create several threads from the main() thread-
   //you may create new threads from any thread, if needed !!! 
   ret = pthread_create(&thid4,NULL,test_thread1,NULL);
   if(ret>0) { printf("error in thread creation for producer\n"); exit(4); }   
   

   ret = pthread_create(&thid1,NULL,test_thread1,NULL);
   if(ret>0) { printf("error in thread creation for consumer\n"); exit(1); }

  ret = pthread_create(&thid2,NULL,test_thread2,NULL);
   if(ret>0) { printf("error in thread creation for consumer\n"); exit(2); } 

 ret = pthread_create(&thid3,NULL,test_thread3,NULL);
   if(ret>0) { printf("error in thread creation for producer\n"); exit(3); } 
   //pthread_exit(NULL);
 //at the end of thread creation, there will be 4 additional threads 
 //and one main thread - main thread is created implicitly 
 //by the OS, when the process is created - additional 
 //threads are created, as per application's requirements ???








   //pthread_join() may used on any joinable thread of this process
   //by this thread - first param is lib thread id - pthread_join()
   //will block the current thread until target thread whose thread
   //id is in param1, terminates - in addition, pthread_join() 
   //may also be used to collect the return pointer info. passed
   //by the target thread, when it terminated 

   //mostly taken care by library with some help from 
   //system space !!!

   //apart from the above details, pthread_join() may be typically
   //used by the main thread to synchronize with other sibling threads
   //and do certain clean-up when the process is completing and
   //terminating normally - in short, pthread_join() takes care
   //synchnronizing main thread and sibling threads to keep 
   //the multithreaded application in order !!!
   pthread_join(thid1,NULL);  //current thread will block until 
                              //thread with id thid1 terminates 

   pthread_join(thid2,NULL);

   pthread_join(thid3,NULL);
   
   pthread_join(thid4,NULL);


   //may do any clean-up needed,when the process 
   //completes 

   //pause() or sigsuspend() or any system call API that blocks,
   //will block the current thread of execution, only - not all
   //threads of the process - refer to multithreading.txt - this'
   //is one of the basic changes in system call behaviour !!!


   //pause();

   //all threads of this process are terminated and 
   //resources are freed -process enters zombie state - 
   //this is a normal termination !!!

   exit(0);

}






