#include "queue.c"
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;// probability of a ground job (launch & assembly)

//time related variables
struct timeval start_time;
struct timeval current_time;
struct timeval current_time_ct;
long start_sc;
long end_sc;

//job queues:
Queue* launch_queue;
Queue* land_queue;
Queue* assembly_queue;
//global variable for unique IDs
int JobID = 0 ;
int NextJobID;
//locks, cond variable, mutexes etc
pthread_mutex_t id_mutex =PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ct_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t id_cv = PTHREAD_COND_INITIALIZER;
//logging file
FILE *events_log;
//functions
void* LandingJob(struct timeval current);
void* LandingJobT(void *arg);
void* LaunchJob(struct timeval current);
void* LaunchJobT(void *arg);
void* EmergencyJob(void *arg);
void* AssemblyJob(struct timeval current);
void* AssemblyJobT(void *arg);
void* ControlTower(void *arg);
/*
void* LandingJob(void *arg);
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg);
void* AssemblyJob(void *arg);
void* ControlTower(void *arg);
*/
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
    }
    //get current, start end times decide the end time in seconds
    gettimeofday(&current_time, NULL);
    gettimeofday(&start_time, NULL);
    start_sc = (long) start_time.tv_sec;
    end_sc = start_sc + simulationTime;
    srand(seed); //feed the seed
    
    //logging
    events_log = fopen("./events.log", "w");
    fprintf(events_log, "EventID\tStatus\tRequest Time\tEnd Time\tTurnaround Time\tPad\n");
    
    /* Queue usage example
        Queue *myQ = ConstructQueue(1000);
        Job j;
        j.ID = myID;
        j.type = 2;
        Enqueue(myQ, j);
        Job ret = Dequeue(myQ);
        DestructQueue(myQ);
    */
    //TODO: Initialize queues for each job type
    launch_queue = ConstructQueue(1000);
    land_queue = ConstructQueue(1000);
    assembly_queue = ConstructQueue(1000);
    //TODO: Create Control Tower Thread
    pthread_t ct_thread;
    //MAYBE we can pass current_time here to Control Tower but not it calculates its own
    //to do that just change NULL at the end as a void pointer to desired val
    pthread_create(&ct_thread, NULL, ControlTower,NULL);
    
    //MAIN LOOP
    //create a random variable and create landing/departure/assembly jobs wrt to that
    double rand_p;
    while(current_time.tv_sec < end_sc){
        //TODO: creating, serving requests
        rand_p = (double)rand() / (double) RAND_MAX;
        
        //WE CAN ADD MORE ARGUMENTS IF WE NEED ONLY THING FOR SURE IS THE CURRENT TIME NOW
        //create assembly w/ probability = p/2
        if (rand_p< p/2){
            LandingJob(current_time);
        }
        //create launch w/ probability = p/2
        else if(rand_p <p){
            LaunchJob(current_time);
        }
        //create assembly w/ probability = 1-p
        else{
            AssemblyJob(current_time);
        }

        
        //t=2 as given in instructions
        pthread_sleep(2);
        //update current time
        gettimeofday(&current_time, NULL);
    }

    // your code goes here
    fclose(events_log);
    return 0;
}

//TYPES: LAND -> 1, LAUNCH -> 2 ASSEMBLY -> 3 (could not find in doc so maybe we can decide)
//FIGURE OUT IDs
// the function that creates plane threads for landing
//padA-B
void* LandingJob(struct timeval current){
    Job landing;
    pthread_mutex_lock(&id_mutex);
    landing.ID = JobID++;
    pthread_mutex_unlock(&id_mutex);
    landing.type = 1;
    landing.request_time = current;
    Enqueue(land_queue, landing);
    //create the thread that has a LandingJobT function as main
    int id = landing.ID;
    pthread_t landing_thread;
    pthread_create(&landing_thread, NULL, LandingJobT,(void *)(&id));
  
}

void* LandingJobT(void *arg){
    int job_id = *((int *) arg);
    // wait for air traffic controller
    pthread_mutex_lock(&ct_mutex);
    while (NextJobID != job_id) {
        pthread_cond_wait(&id_cv, &ct_mutex);
    }
    pthread_mutex_unlock(&ct_mutex);
    pthread_exit(NULL);
}

// the function that creates plane threads for departure
//pad A
void* LaunchJob(struct timeval current){
    Job launching;
    pthread_mutex_lock(&id_mutex);
    launching.ID = JobID++;
    pthread_mutex_unlock(&id_mutex);
    launching.type = 2;
    launching.request_time = current;
    Enqueue(launch_queue, launching);
    int id = launching.ID;
    pthread_t launch_thread;
    pthread_create(&launch_thread, NULL, LaunchJobT,(void *)(&id));
}
void* LaunchJobT(void *arg){
    int job_id = *((int *) arg);
    // wait for air traffic controller
    pthread_mutex_lock(&ct_mutex);
    while (NextJobID != job_id) {
        pthread_cond_wait(&id_cv, &ct_mutex);
    }
    pthread_mutex_unlock(&ct_mutex);
    pthread_exit(NULL);
}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *arg){

}

// the function that creates plane threads for emergency landing (padB)
void* AssemblyJob(struct timeval current){
    Job assembly;
    pthread_mutex_lock(&id_mutex);
    assembly.ID = JobID++;
    pthread_mutex_unlock(&id_mutex);
    assembly.type = 3;
    assembly.request_time = current;
    Enqueue(assembly_queue, assembly);
    pthread_t assembly_thread;
    pthread_create(&assembly_thread, NULL, AssemblyJobT,NULL);
}
void* AssemblyJobT(void *arg){
  
}

// the function that controls the air traffic
void* ControlTower(void *arg){
    //first aircraft mutex wait
    gettimeofday(&current_time_ct, NULL);
    while(current_time_ct.tv_sec < end_sc){
    //TODO: if waiting for the land do it priorily
        
    //TODO: else if launch from pad a
    //TODO: else if assembly in pad b
    //update time
    gettimeofday(&current_time_ct, NULL);
    }
    

}
