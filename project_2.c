#include "queue.c"
#include <sys/time.h>
int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;// probability of a ground job (launch & assembly)

//time related variables
struct timeval start_time;
strutct timeval current_time;
long start_sc;
long end_sc;
//logging file
FILE *events_log;
void* LandingJob(void *arg); 
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg); 
void* AssemblyJob(void *arg); 
void* ControlTower(void *arg); 

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
    srand(seed); // feed the seed
    
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
    //TODO: Initialize locks, threads etx
    
    //MAIN LOOP
    while(current_time.tv_sec < end_sc){
        //TODO: creating, serving requests
        
        //update current time
        gettimeofday(&current_time, NULL);
    }

    // your code goes here

    return 0;
}

// the function that creates plane threads for landing
void* LandingJob(void *arg){

}

// the function that creates plane threads for departure
void* LaunchJob(void *arg){

}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *arg){

}

// the function that creates plane threads for emergency landing
void* AssemblyJob(void *arg){

}

// the function that controls the air traffic
void* ControlTower(void *arg){

}
