#include "priority_queue.h"
#include <pthread.h>

#define MAX_NUM_THREADS 700

struct params {
	unsigned int priority;
	so_handler *thread_func;
};


struct scheduler {
	int time_slice;
	int nr_IO_devices;
	pqueue *pq;
	int num_threads;
	tid_t *threads;
	pthread_mutex_t lock;
	pthread_cond_t ready;
	pthread_cond_t running;
	int ready_var;
	tid_t turn_id;
	th_info *crt_thread_info;
	int thread_finished;
	int last_time_available;
} *my_scheduler;


int init_error;
int initialized;


/*
 * creates and initializes scheduler
 * + time quantum for each thread
 * + number of IO devices supported
 * returns: 0 on success or negative on error
 */
int so_init(unsigned int time_quantum, unsigned int io)
{
	int ret;

	if (initialized == 1)
		return -1;

	if (time_quantum <= 0 || io < 0 || io > SO_MAX_NUM_EVENTS) {
		init_error = 1;
		return -1;
	}

	my_scheduler = malloc(sizeof(struct scheduler));
	if (my_scheduler == NULL)
		return -1;


	my_scheduler->time_slice = time_quantum;
	my_scheduler->nr_IO_devices = io;

	pqueue *aux1 = malloc(sizeof(pqueue));

	if (aux1 == NULL)
		return -1;

	my_scheduler->pq = aux1;
	init(my_scheduler->pq);

	my_scheduler->num_threads = 0;

	tid_t *aux2 = malloc(MAX_NUM_THREADS * sizeof(tid_t));

	if (aux2 == NULL)
		return -1;

	my_scheduler->threads = aux2;

	ret = pthread_mutex_init(&my_scheduler->lock, NULL);
	if (ret != 0)
		return -1;

	ret = pthread_cond_init(&my_scheduler->ready, NULL);
	if (ret != 0)
		return -1;

	ret = pthread_cond_init(&my_scheduler->running, NULL);
	if (ret != 0)
		return -1;

	my_scheduler->ready_var = 0;
	my_scheduler->turn_id = -1;
	my_scheduler->crt_thread_info = NULL;
	my_scheduler->thread_finished = 0;
	my_scheduler->last_time_available = -1;

	initialized = 1;

	return 0;
}


int exists_thread(tid_t thread_id)
{
	for (int i = 0; i < my_scheduler->num_threads; i++) {
		if (my_scheduler->threads[i] == thread_id)
			return 1;
	}

	return 0;
}


void schedule(int block)
{
	pthread_mutex_lock(&my_scheduler->lock);

	if (is_empty(my_scheduler->pq) == 1) {
		// nu mai planific nimic
		return;
	}

	th_info *thread_info;

	if (my_scheduler->turn_id == -1) {
		// prima planificare
		thread_info = peek(my_scheduler->pq);
		if (thread_info == NULL) {
			// caz imposibil
			return;
		}
		my_scheduler->turn_id = thread_info->thread_id;
		my_scheduler->crt_thread_info = thread_info;

		pthread_cond_broadcast(&my_scheduler->running);
		pthread_mutex_unlock(&my_scheduler->lock);
		return;
	}

	if (my_scheduler->thread_finished == 1) {
		my_scheduler->thread_finished = 0;
		if (my_scheduler->last_time_available == 0)
			goto time_;
		else
			goto next;

	} else if (my_scheduler->crt_thread_info->waiting_io == 1) {
		// daca thread-ul curent asteapta un eveniment io -> este preemptat
		my_scheduler->crt_thread_info->time_available = my_scheduler->time_slice;
		th_info *best = peek(my_scheduler->pq);

		if (best == NULL) {
			// nu am gasit niciun thread disponibil
			return;
		}
		my_scheduler->turn_id = best->thread_id;
		my_scheduler->crt_thread_info = best;
	} else if (my_scheduler->crt_thread_info->time_available == 0) {
		// cuanta thread-ului curent a expirat
		// daca gasesc un thread mai bun, il planific
		// daca nu, continui tot eu + resetez cuanta
time_:;
		th_info *best = peek(my_scheduler->pq);

		if (best == NULL) {
			// nu am gasit niciun thread disponibil => rulez tot eu
			my_scheduler->crt_thread_info->time_available = my_scheduler->time_slice;
			goto running;
		}
		// daca am gasit un thread cu prioritate mai mare -> il planific
		if (best->priority > my_scheduler->crt_thread_info->priority) {
			my_scheduler->crt_thread_info->time_available = my_scheduler->time_slice;
			my_scheduler->turn_id = best->thread_id;
			my_scheduler->crt_thread_info = best;
		} else {
			// caut urmatorul thread cu aceeasi prioritate, daca exista
			reorder_nodes(my_scheduler->pq);
			th_info *another = peek(my_scheduler->pq);

			if (best == NULL) {
				// nu am gasit niciun thread disponibil => rulez tot eu
				my_scheduler->crt_thread_info->time_available = my_scheduler->time_slice;
			} else {
				my_scheduler->crt_thread_info->time_available = my_scheduler->time_slice;
				my_scheduler->turn_id = another->thread_id;
				my_scheduler->crt_thread_info = another;
			}
		}
	} else {
		// daca nu a expirat cuanta, dar avem un thread mai bun disponibil
		// => il planificam
		// altfel -> ruleaza thread-ul curent
next:;
		th_info *best = peek(my_scheduler->pq);

		if (best == NULL) {
			// nu am gasit niciun thread disponibil => rulez tot eu
			my_scheduler->crt_thread_info->time_available = my_scheduler->time_slice;
		} else {
			my_scheduler->turn_id = best->thread_id;
			my_scheduler->crt_thread_info = best;
		}
	}

running:
	pthread_cond_broadcast(&my_scheduler->running);
	pthread_mutex_unlock(&my_scheduler->lock);

	// thread-ul curent se blocheaza daca nu e planificat in acest moment
	if (block == 1 && my_scheduler->turn_id != pthread_self()) {
		// -> asteaptă să fie planificat
		pthread_mutex_lock(&my_scheduler->lock);
		while (my_scheduler->turn_id != pthread_self())
			pthread_cond_wait(&my_scheduler->running, &my_scheduler->lock);
		pthread_mutex_unlock(&my_scheduler->lock);
	}
}


void *start_thread(void *p)
{
	struct params my_params = *(struct params *) p;
	unsigned int prio = my_params.priority;
	so_handler *thread_func = my_params.thread_func;

	// anunta parintele ca e gata pentru planificare
	pthread_mutex_lock(&my_scheduler->lock);
	my_scheduler->ready_var = 1;
	pthread_cond_signal(&my_scheduler->ready);
	pthread_mutex_unlock(&my_scheduler->lock);

	// -> asteaptă să fie planificat
	pthread_mutex_lock(&my_scheduler->lock);
	while (my_scheduler->turn_id != pthread_self())
		pthread_cond_wait(&my_scheduler->running, &my_scheduler->lock);
	pthread_mutex_unlock(&my_scheduler->lock);

	// -> apeleaza handler(prio)
	thread_func(prio);

	// thread-ul a terminat de executat handler-ul
	my_scheduler->thread_finished = 1;
	pop(my_scheduler->pq, my_scheduler->crt_thread_info);
	schedule(0);

	// -> iesire thread
	return NULL;
}


/*
 * creates a new so_task_t and runs it according to the scheduler
 * + handler function
 * + priority
 * returns: tid of the new task if successful or INVALID_TID
 */
tid_t so_fork(so_handler *func, unsigned int priority)
{
	if (func == NULL || priority < 0 || priority > SO_MAX_PRIO)
		return INVALID_TID;

	// -> initializare thread struct
	th_info *new_thread = malloc(sizeof(th_info));

	if (new_thread == NULL)
		return INVALID_TID;

	new_thread->priority = priority;
	new_thread->thread_func = func;
	new_thread->time_available = my_scheduler->time_slice;
	new_thread->waiting_io = 0;
	new_thread->device_io = -1;

	tid_t thread_id;
	struct params p;

	p.priority = priority;
	p.thread_func = func;

	// -> creare thread nou ce va executa functia start_thread
	int ret = pthread_create(&thread_id, NULL, &start_thread, (void *) &p);

	if (ret != 0)
		return INVALID_TID;

	new_thread->thread_id = thread_id;
	push(my_scheduler->pq, new_thread, priority);

	my_scheduler->threads[my_scheduler->num_threads] = thread_id;
	my_scheduler->num_threads++;

	// -> asteaptă ca threadul să intre în starea READY/RUN
	pthread_mutex_lock(&my_scheduler->lock);
	while (my_scheduler->ready_var == 0)
		pthread_cond_wait(&my_scheduler->ready, &my_scheduler->lock);
	my_scheduler->ready_var = 0;
	pthread_mutex_unlock(&my_scheduler->lock);

	if (exists_thread(pthread_self()) == 1)
		my_scheduler->crt_thread_info->time_available--;

	schedule(1);
	// -> returnează id-ul noului thread
	return thread_id;
}


/*
 * waits for an IO device
 * + device index
 * returns: -1 if the device does not exist or 0 on success
 */
int so_wait(unsigned int io)
{
	my_scheduler->crt_thread_info->time_available--;

	if (io < 0 || io >= my_scheduler->nr_IO_devices) {
		schedule(1);
		return -1;
	}

	my_scheduler->crt_thread_info->waiting_io = 1;
	my_scheduler->crt_thread_info->device_io = io;

	schedule(1);

	return 0;
}


/*
 * signals an IO device
 * + device index
 * return the number of tasks woke or -1 on error
 */
int so_signal(unsigned int io)
{
	my_scheduler->crt_thread_info->time_available--;

	if (io < 0 || io >= my_scheduler->nr_IO_devices) {
		schedule(1);
		return -1;
	}

	int woken_up_threads = wake_up_all(my_scheduler->pq, io);

	schedule(1);
	return woken_up_threads;
}


/*
 * does whatever operation
 */
void so_exec(void)
{
	my_scheduler->crt_thread_info->time_available--;

	schedule(1);
}


/*
 * destroys a scheduler
 */
void so_end(void)
{
	if (init_error == 1 || initialized == 0)
		return;

	// asteapta toate thread-urile
	for (int i = 0; i < my_scheduler->num_threads; i++)
		pthread_join(my_scheduler->threads[i], NULL);

	destroy_queue(my_scheduler->pq);
	free(my_scheduler->pq);
	free(my_scheduler->threads);
	pthread_mutex_destroy(&my_scheduler->lock);
	pthread_cond_destroy(&my_scheduler->ready);
	pthread_cond_destroy(&my_scheduler->running);
	free(my_scheduler);

	initialized = 0;
	init_error = 0;
}
