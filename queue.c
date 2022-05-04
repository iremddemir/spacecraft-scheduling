#include <stdlib.h>
#include <stdio.h>

#define TRUE  1
#define FALSE 0

typedef struct {
    int ID;
    int type;
    // you might want to add variables here!
    struct timeval request_time;
    
} Job;

/* a link in the queue, holds the data and point to the next Node */
typedef struct Node_t {
    Job data;
    struct Node_t *prev;
} NODE;

/* the HEAD of the Queue, hold the amount of node's that are in the queue */
typedef struct Queue {
    NODE *head;
    NODE *tail;
    int size;
    int limit;
} Queue;

Queue *ConstructQueue(int limit);
void DestructQueue(Queue *queue);
int Enqueue(Queue *pQueue, Job j);
Job Dequeue(Queue *pQueue);
int isEmpty(Queue* pQueue);

Queue *ConstructQueue(int limit) {
    Queue *queue = (Queue*) malloc(sizeof (Queue));
    if (queue == NULL) {
        return NULL;
    }
    if (limit <= 0) {
        limit = 65535;
    }
    queue->limit = limit;
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;

    return queue;
}

void DestructQueue(Queue *queue) {
    NODE *pN;
    while (!isEmpty(queue)) {
        Dequeue(queue);
    }
    free(queue);
}

int Enqueue(Queue *pQueue, Job j) {
    /* Bad parameter */
    NODE* item = (NODE*) malloc(sizeof (NODE));
    item->data = j;

    if ((pQueue == NULL) || (item == NULL)) {
        return FALSE;
    }
    // if(pQueue->limit != 0)
    if (pQueue->size >= pQueue->limit) {
        return FALSE;
    }
    /*the queue is empty*/
    item->prev = NULL;
    if (pQueue->size == 0) {
        pQueue->head = item;
        pQueue->tail = item;

    } else {
        /*adding item to the end of the queue*/
        pQueue->tail->prev = item;
        pQueue->tail = item;
    }
    pQueue->size++;
    return TRUE;
}

Job Dequeue(Queue *pQueue) {
    /*the queue is empty or bad param*/
    NODE *item;
    Job ret;
    if (isEmpty(pQueue))
        return ret;
    item = pQueue->head;
    pQueue->head = (pQueue->head)->prev;
    pQueue->size--;
    ret = item->data;
    free(item);
    return ret;
}

int isEmpty(Queue* pQueue) {
    if (pQueue == NULL) {
        return FALSE;
    }
    if (pQueue->size == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

int printQueue(Queue *pQueue, long second, int type){
  //LANDING QUEUE
  if (type == 1){
    printf("At %ld landing queue :", second);
  }
  //LAUNCH QUEUE
  if (type == 2){
    printf("At %ld launch queue :", second);
  }
  //ASSEMBLY QUEUE
  if (type == 3){
    printf("At %ld assembly queue :", second);
  }
  if (pQueue== NULL){
    printf("NULL QUEUE\n");
  }
  else if (pQueue->size == 0){
    printf("Empty Queue\n");
  }
  else{
    NODE* start = pQueue->head;
    while(start != NULL){
      printf(" %d,", start->data.ID);
      start = start->prev;
    }
    printf("\n");
  }
  
}
