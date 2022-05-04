#include "queue.c"
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;               // probability of a ground job (launch & assembly)
int n = 30; //set n if it does not given in arguments
//time related variables
struct timeval start_time;
struct timeval current_time;
struct timeval current_time_ct;
long start_sc;
long end_sc;

// Used for round robin for pads a and b. If one type has priority but the queue is
// empty, then the other can use it
pthread_mutex_t a_priority_mutex = PTHREAD_MUTEX_INITIALIZER;
int pad_a_priority = 1;

pthread_mutex_t b_priority_mutex = PTHREAD_MUTEX_INITIALIZER;
int pad_b_priority = 1;

pthread_mutex_t max_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
int maxWaitTime = 0;

//job queues:
Queue* launch_queue;
Queue* land_queue;
Queue* assembly_queue;
Queue* padA_queue;
Queue* padB_queue;
//global variable for unique IDs
int JobID = 0 ;
//mutex for unique id creation
pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;
//mutexes for job queues
pthread_mutex_t land_queue_mutex =PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t launch_queue_mutex =PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t assembly_queue_mutex =PTHREAD_MUTEX_INITIALIZER;
//mutexes for pad queues
pthread_mutex_t padA_queue_mutex =PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t padB_queue_mutex =PTHREAD_MUTEX_INITIALIZER;
//queue log mutex
//pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
//logging file
FILE *events_log;
void* LandingJob(void *arg);
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg);
void* AssemblyJob(void *arg);
void* ControlTower(void *arg);
void* PadA(void *arg);
void* PadB(void *arg);
void* SnapShotPrint(void *arg);
// pthread sleeper function
int pthread_sleep (int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if(pthread_mutex_init(&mutex,NULL))
    {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL))
    {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
    
    pthread_mutex_lock (&mutex);
    int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock (&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    
    //Upon successful completion, a value of zero shall be returned
    return res;
}

int main(int argc,char **argv){
    // -p (float) => sets p
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-p")) {p = atof(argv[++i]);}
        else if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-n"))  {n = atoi(argv[++i]);}
    }
    //get current, start end times decide the end time in   seconds
    gettimeofday(&current_time, NULL);
    gettimeofday(&start_time, NULL);
    start_sc = (long) start_time.tv_sec;
    end_sc = start_sc + simulationTime;
    srand(seed); // feed the seed
    //logging
    events_log = fopen("./events.log", "w");
    fprintf(events_log,"EventID\tStatus\tRequest Time\tEnd Time\tTurnaround Time\tPad\n");
    //Initialize queues for each job type and for pad A and B
    launch_queue = ConstructQueue(1000);
    land_queue = ConstructQueue(1000);
    assembly_queue = ConstructQueue(1000);
    padA_queue = ConstructQueue(1000);
    padB_queue = ConstructQueue(1000);
    fprintf(events_log,"queues created\n");
    //Create Control Tower and one thread that is responsible for one job type for each job type
    pthread_t ct_thread, landing_thread, launch_thread, assembly_thread, padA_thread, padB_thread, snapshot_thread;
    pthread_create(&landing_thread, NULL, LandingJob,NULL);
    pthread_create(&launch_thread, NULL, LaunchJob,NULL);
    pthread_create(&assembly_thread, NULL, AssemblyJob,NULL);
    pthread_create(&ct_thread, NULL, ControlTower,NULL);
    pthread_create(&padA_thread, NULL, PadA,NULL);
    pthread_create(&padB_thread, NULL, PadB,NULL);
    pthread_create(&snapshot_thread, NULL, SnapShotPrint, NULL);
    fprintf(events_log,"threads created\n");
    
    //join threads for synch (given in documentation)
    pthread_join(landing_thread, NULL);
    pthread_join(launch_thread, NULL);
    pthread_join(assembly_thread, NULL);
    pthread_join(ct_thread, NULL);
    pthread_join(padA_thread, NULL);
    pthread_join(padB_thread, NULL);
    pthread_join(snapshot_thread, NULL);
    //fprintf(events_log,"joins created\n");
    // your code goes here
    //fprintf(events_log,"close created\n");
    fclose(events_log);
    DestructQueue(land_queue);
    DestructQueue(launch_queue);
    DestructQueue(assembly_queue);
    DestructQueue(padA_queue);
    DestructQueue(padB_queue);
    return 0;
}

//SnapShotPrint is a thread that  in every n seconds prints queues
void* SnapShotPrint(void *arg){
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    long sim_time = current_time.tv_sec - start_sc;
    while (current_time.tv_sec < end_sc){
      sim_time = current_time.tv_sec - start_sc;
      //printf("%ld", sim_time % n);
      if(n <= sim_time && ((int) sim_time %(int)n == 0)){
        printQueue(land_queue, sim_time, 1);
        printQueue(launch_queue, sim_time, 2);
        printQueue(assembly_queue, sim_time, 3);
      }
      pthread_sleep(n);
      gettimeofday(&current_time, NULL);
      }
}

// Landing Job Thread that controls all land jobs
//assume this is type 1
void* LandingJob(void *arg){
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    //create landing job until time is out with probability 1-p
    double rand_p;
    while (current_time.tv_sec < end_sc){
      //fprintf(events_log,"inside L while\n");
      rand_p = (double)rand() / (double) RAND_MAX;
      if(rand_p < 1-p){
        Job landing;
        pthread_mutex_lock(&id_mutex);
        landing.ID = JobID++;
        pthread_mutex_unlock(&id_mutex);
        landing.type = 1;
        landing.request_time = current_time;
        pthread_mutex_lock(&land_queue_mutex);
        Enqueue(land_queue, landing);
        pthread_mutex_unlock(&land_queue_mutex);
        fprintf(events_log,"Landing created\n");
      }
      //t=2 as given in instructions
      pthread_sleep(2);
      //update current time
      gettimeofday(&current_time, NULL);
    }
  pthread_exit(NULL);
}

// Launch Job Thread that controls all launch jobs
void* LaunchJob(void *arg){
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    //create launch job until time is out with probability p/2
    double rand_p;
    while (current_time.tv_sec < end_sc){
      rand_p = (double)rand() / (double) RAND_MAX;
      if(rand_p < p/2){
        Job launch;
        pthread_mutex_lock(&id_mutex);
        launch.ID = JobID++;
        pthread_mutex_unlock(&id_mutex);
        launch.type = 2;
        launch.request_time = current_time;
        pthread_mutex_lock(&launch_queue_mutex);
        Enqueue(launch_queue, launch);
        pthread_mutex_unlock(&launch_queue_mutex);
        fprintf(events_log,"Launch created\n");
      }
      //t=2 as given in instructions
      pthread_sleep(2);
      //update current time
      gettimeofday(&current_time, NULL);
    }
  pthread_exit(NULL);
}

// Landing Job Thread that controls all emergency jobs
void* EmergencyJob(void *arg){

}

// Assembly Job Thread that controls all assembly jobs
void* AssemblyJob(void *arg){
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    //create assembly job until time is out with probability p/2
    double rand_p;
    while (current_time.tv_sec < end_sc){
      rand_p = (double)rand() / (double) RAND_MAX;
      if(rand_p< p/2){
        Job assembly;
        pthread_mutex_lock(&id_mutex);
        assembly.ID = JobID++;
        pthread_mutex_unlock(&id_mutex);
        assembly.type = 3;
        assembly.request_time = current_time;
        pthread_mutex_lock(&assembly_queue_mutex);
        Enqueue(assembly_queue, assembly);
        pthread_mutex_unlock(&assembly_queue_mutex);
        fprintf(events_log,"Assembly created\n");
      }
      //t=2 as given in instructions
      pthread_sleep(2);
      //update current time
      gettimeofday(&current_time, NULL);
    }
  pthread_exit(NULL);
}
//Pad A thread that controls the actions on pad A
void* PadA(void *arg){
  //fprintf(events_log,"pad a thread is created\n");
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    while (current_time.tv_sec < end_sc){
     // fprintf(events_log,"inside pad a\n");
      pthread_mutex_lock(&padA_queue_mutex);
      //if there is an job is in pad A than do not need to control until it is done
      if(!isEmpty(padA_queue)){
        //pthread_mutex_unlock(&padA_queue_mutex);
        int job_type = padA_queue->head->data.type;
        pthread_mutex_unlock(&padA_queue_mutex);
        if(job_type == 2) {
          struct timeval finishedWaiting;
          gettimeofday(&finishedWaiting, NULL);
          int waitingTime = finishedWaiting.tv_sec - padA_queue->head->data.request_time.tv_sec;
          pthread_mutex_lock(&max_wait_mutex);
          if(waitingTime > maxWaitTime) {
            maxWaitTime = waitingTime;
          }
          pthread_mutex_unlock(&max_wait_mutex);
        }
        //if landing then 1 if launch 2
        int wait_time = (job_type == 1)? 2:4;
        pthread_sleep(wait_time);
        //once job is done:
        gettimeofday(&current_time, NULL);
        pthread_mutex_lock(&padA_queue_mutex);
        Job done = Dequeue(padA_queue);
        char status = (job_type==1) ? 'L': 'D';
        pthread_mutex_unlock(&padA_queue_mutex);
        /*
        if (job_type == 2) {
          pthread_mutex_lock(&land_counter_mutex);
          landCount--;
          pthread_mutex_unlock(&land_counter_mutex);
        }*/
        
        fprintf(events_log,  "%d\t\t%c\t\t%ld\t\t%ld\t\t%ld\t\t%c\n",
              done.ID,
              status,
              done.request_time.tv_sec - start_sc,
              current_time.tv_sec - start_sc,
              current_time.tv_sec - done.request_time.tv_sec,
              'A');
      }
      else {
          pthread_mutex_unlock(&padA_queue_mutex);
      }
      gettimeofday(&current_time, NULL);
  }
  pthread_exit(NULL);
}
//Pad B thread that controls the actions on pad B
void* PadB(void *arg){
    //fprintf(events_log,"pad b thread created\n");
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    while (current_time.tv_sec < end_sc){
      //fprintf(events_log,"inside pad b\n");
      pthread_mutex_lock(&padB_queue_mutex);
      //if there is an job is in pad B than do not need to control until it is done
      if(!isEmpty(padB_queue)){
        //pthread_mutex_unlock(&padB_queue_mutex);
        int job_type = padB_queue->head->data.type;
        pthread_mutex_unlock(&padB_queue_mutex);
        if(job_type == 3) {
          struct timeval finishedWaiting;
          gettimeofday(&finishedWaiting, NULL);
          int waitingTime = finishedWaiting.tv_sec - padB_queue->head->data.request_time.tv_sec;
          printf("Waiting time: %d\n", waitingTime);
          pthread_mutex_lock(&max_wait_mutex);
          if(waitingTime > maxWaitTime) {
            maxWaitTime = waitingTime;
          }
          pthread_mutex_unlock(&max_wait_mutex);
        }
        //if landing then 1 if assembly then 3
        int wait_time = (job_type == 1)? 2:12;
        pthread_sleep(wait_time);
        gettimeofday(&current_time, NULL);
        //once job is done:
        pthread_mutex_lock(&padB_queue_mutex);
        Job done = Dequeue(padB_queue);
        char status = (job_type==1) ? 'L': 'A';
        pthread_mutex_unlock(&padB_queue_mutex);
        /*
        if (job_type ==3) {
          pthread_mutex_lock(&land_counter_mutex);
          landCount--;
          pthread_mutex_unlock(&land_counter_mutex);
        }*/
        fprintf(events_log,  "%d\t\t%c\t\t%ld\t\t%ld\t\t%ld\t\t%c\n",
              done.ID,
              status,
              done.request_time.tv_sec - start_time.tv_sec,
              current_time.tv_sec - start_time.tv_sec,
              current_time.tv_sec - done.request_time.tv_sec,
              'B');
      }
      else {
          pthread_mutex_unlock(&padB_queue_mutex);
          //sleep for 2 second since t=2 given there might be a new job generated
         
      }
      gettimeofday(&current_time, NULL);
  }
  pthread_exit(NULL);
}
// the function that controls the air traffic
void* ControlTower(void *arg){
  //fprintf(events_log,"inside CT\n");
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    //until time is done
    while(current_time.tv_sec < end_sc){
      //fprintf(events_log,"inside CT while\n");
      // Check if pad A is busy or not
      pthread_mutex_lock(&padA_queue_mutex);
      if(isEmpty(padA_queue)) {
        // If pad A is not busy, determine which task to assign
        pthread_mutex_lock(&a_priority_mutex);
        int priority = pad_a_priority;
        if(priority == 1) {
          // If landing has priority, we should determine if there are any landing jobs available
          pthread_mutex_lock(&land_queue_mutex);
          if(!isEmpty(land_queue)) {
            Job currentJob = Dequeue(land_queue);
            pthread_mutex_unlock(&land_queue_mutex);
            Enqueue(padA_queue, currentJob);
            pad_a_priority = 2;
            pthread_mutex_unlock(&a_priority_mutex);
          }
          else {
            pthread_mutex_unlock(&land_queue_mutex);
            pthread_mutex_unlock(&a_priority_mutex);
            pthread_mutex_lock(&launch_queue_mutex);
            if(!isEmpty(launch_queue)) {
              Job currentJob = Dequeue(launch_queue);
              pthread_mutex_unlock(&launch_queue_mutex);
              Enqueue(padA_queue, currentJob);
            }else
              pthread_mutex_unlock(&launch_queue_mutex);
          }
        }
        else if(priority == 2) {
          pthread_mutex_lock(&launch_queue_mutex);
          if(!isEmpty(launch_queue)) {
            Job currentJob = Dequeue(launch_queue);
            pthread_mutex_unlock(&launch_queue_mutex);
            Enqueue(padA_queue, currentJob);
            pad_a_priority = 1;
            pthread_mutex_unlock(&a_priority_mutex);
          }
          else {
            pthread_mutex_unlock(&launch_queue_mutex);
            pthread_mutex_unlock(&a_priority_mutex);
            pthread_mutex_lock(&land_queue_mutex);
            if(!isEmpty(land_queue)) {
              Job currentJob = Dequeue(land_queue);
              pthread_mutex_unlock(&land_queue_mutex);
              Enqueue(padA_queue, currentJob);
            }else
              pthread_mutex_unlock(&land_queue_mutex);
          }
        }
      }
      pthread_mutex_unlock(&padA_queue_mutex);
      pthread_mutex_lock(&padB_queue_mutex);
      if(isEmpty(padB_queue)) {
        // If pad A is not busy, determine which task to assign
        pthread_mutex_lock(&b_priority_mutex);
        int priority = pad_b_priority;
        if(priority == 1) {
          // If landing has priority, we should determine if there are any landing jobs available
          pthread_mutex_lock(&land_queue_mutex);
          if(!isEmpty(land_queue)) {
            Job currentJob = Dequeue(land_queue);
            pthread_mutex_unlock(&land_queue_mutex);
            Enqueue(padB_queue, currentJob);
            pad_b_priority = 3;
            pthread_mutex_unlock(&b_priority_mutex);
          }
          else {
            pthread_mutex_unlock(&land_queue_mutex);
            pthread_mutex_unlock(&b_priority_mutex);
            pthread_mutex_lock(&assembly_queue_mutex);
            if(!isEmpty(assembly_queue)) {
              Job currentJob = Dequeue(assembly_queue);
              pthread_mutex_unlock(&assembly_queue_mutex);
              Enqueue(padB_queue, currentJob);
            }else
              pthread_mutex_unlock(&assembly_queue_mutex);
          }
        }
        else if(priority == 3) {
          pthread_mutex_lock(&assembly_queue_mutex);
          if(!isEmpty(assembly_queue)) {
            Job currentJob = Dequeue(assembly_queue);
            pthread_mutex_unlock(&assembly_queue_mutex);
            Enqueue(padB_queue, currentJob);
            pad_a_priority = 1;
            pthread_mutex_unlock(&b_priority_mutex);
          }
          else {
            pthread_mutex_unlock(&assembly_queue_mutex);
            pthread_mutex_unlock(&b_priority_mutex);
            pthread_mutex_lock(&land_queue_mutex);
            if(!isEmpty(land_queue)) {
              Job currentJob = Dequeue(land_queue);
              pthread_mutex_unlock(&land_queue_mutex);
              Enqueue(padA_queue, currentJob);
            }else
              pthread_mutex_unlock(&land_queue_mutex);
          }
        }
      }
      pthread_mutex_unlock(&padB_queue_mutex);
      gettimeofday(&current_time, NULL);
    }
  fprintf(events_log, "Maximum waiting time: %d\n", maxWaitTime);
  pthread_exit(NULL);
}
