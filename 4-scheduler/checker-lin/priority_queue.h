#include <stdio.h>
#include <stdlib.h>
#include "so_scheduler.h"

typedef struct thread_info {
tid_t thread_id;
unsigned int priority;
so_handler *thread_func;
int time_available;
int waiting_io;
int device_io;
} th_info;

#define MAX 700

typedef struct node {
	th_info *data;
	int priority;
} node;

typedef struct pqueue {
node nodes[MAX];
int head;
int tail;
} pqueue;

void init(pqueue *pq);
int is_empty(pqueue *pq);
int is_full(pqueue *pq);
th_info *peek(pqueue *pq);
void push(pqueue *pq, th_info *thread_info, int priority);
void pop(pqueue *pq, th_info *thread_info);
int available_threads_with_prio(pqueue *pq, int priority);
void reorder_nodes(pqueue *pq);
int wake_up_all(pqueue *pq, int io);
void destroy_queue(pqueue *pq);
