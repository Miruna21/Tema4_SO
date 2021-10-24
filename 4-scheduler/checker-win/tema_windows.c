#define DllExport __declspec(dllexport)
#define DllImport __declspec(dllimport)
#include "priority_queue.h"

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
	HANDLE *hThreads;
	DWORD *threadIds;
	CRITICAL_SECTION lock;
	CONDITION_VARIABLE ready;
	CONDITION_VARIABLE running;
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
	pqueue *aux1;
	HANDLE *aux2;
	DWORD *aux3;

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

	aux1 = malloc(sizeof(pqueue));

	if (aux1 == NULL)
		return -1;

	my_scheduler->pq = aux1;
	init(my_scheduler->pq);

	my_scheduler->num_threads = 0;

	aux2 = malloc(MAX_NUM_THREADS * sizeof(HANDLE));

	if (aux2 == NULL)
		return -1;

	my_scheduler->hThreads = aux2;

	aux3 = malloc(MAX_NUM_THREADS * sizeof(DWORD));

	if (aux3 == NULL)
		return -1;

	my_scheduler->threadIds = aux3;

	InitializeCriticalSection(&my_scheduler->lock);
	InitializeConditionVariable(&my_scheduler->ready);
	InitializeConditionVariable(&my_scheduler->running);

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
	int i;

	for (i = 0; i < my_scheduler->num_threads; i++) {
		if (my_scheduler->threadIds[i] == thread_id)
			return 1;
	}

	return 0;
}


void schedule(int block)
{
	th_info *thread_info;
	th_info *best;
	th_info *another;

	EnterCriticalSection(&my_scheduler->lock);

	if (is_empty(my_scheduler->pq) == 1) {
		// nu mai planific nimic
		return;
	}

	if (my_scheduler->turn_id == -1) {
		// prima planificare
		thread_info = peek(my_scheduler->pq);
		if (thread_info == NULL) {
			// caz imposibil
			return;
		}
		my_scheduler->turn_id = thread_info->thread_id;
		my_scheduler->crt_thread_info = thread_info;

		WakeAllConditionVariable(&my_scheduler->running);
		LeaveCriticalSection(&my_scheduler->lock);
		return;
	}

	if (my_scheduler->thread_finished == 1) {
		my_scheduler->thread_finished = 0;
		if (my_scheduler->last_time_available == 0)
			goto time_;
		else
			goto next;

	} else if (my_scheduler->crt_thread_info->waiting_io == 1) {
		// daca thread-ul curent asteapta un eveniment io
		// -> este preemptat
		my_scheduler->crt_thread_info->time_available =
					my_scheduler->time_slice;
		best = peek(my_scheduler->pq);

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
		best = peek(my_scheduler->pq);

		if (best == NULL) {
			// nu am gasit niciun thread disponibil => rulez tot eu
			my_scheduler->crt_thread_info->time_available
					= my_scheduler->time_slice;
			goto running;
		}
		// daca am gasit un thread cu prioritate mai mare -> il planific
		if (best->priority > my_scheduler->crt_thread_info->priority) {
			my_scheduler->crt_thread_info->time_available
						= my_scheduler->time_slice;
			my_scheduler->turn_id = best->thread_id;
			my_scheduler->crt_thread_info = best;
		} else {
			// caut urmatorul thread cu aceeasi prioritate,
			// daca exista
			reorder_nodes(my_scheduler->pq);
			another = peek(my_scheduler->pq);

			if (best == NULL) {
				// nu am gasit niciun thread disponibil
				// => rulez tot eu
				my_scheduler->crt_thread_info->time_available
						= my_scheduler->time_slice;
			} else {
				my_scheduler->crt_thread_info->time_available
						= my_scheduler->time_slice;
				my_scheduler->turn_id = another->thread_id;
				my_scheduler->crt_thread_info = another;
			}
		}
	} else {
		// daca nu a expirat cuanta,
		// dar avem un thread mai bun disponibil
		// => il planificam
		// altfel -> ruleaza thread-ul curent
next:;
		best = peek(my_scheduler->pq);

		if (best == NULL) {
			// nu am gasit niciun thread disponibil => rulez tot eu
			my_scheduler->crt_thread_info->time_available
					= my_scheduler->time_slice;
		} else {
			my_scheduler->turn_id = best->thread_id;
			my_scheduler->crt_thread_info = best;
		}
	}

running:
	WakeAllConditionVariable(&my_scheduler->running);
	LeaveCriticalSection(&my_scheduler->lock);

	// thread-ul curent se blocheaza daca nu e planificat in acest moment
	if (block == 1 && my_scheduler->turn_id != GetCurrentThreadId()) {
		// -> asteaptă să fie planificat
		EnterCriticalSection(&my_scheduler->lock);
		while (my_scheduler->turn_id != GetCurrentThreadId())
			SleepConditionVariableCS(&my_scheduler->running,
				&my_scheduler->lock, INFINITE);
		LeaveCriticalSection(&my_scheduler->lock);
	}
}


void *start_thread(void *p)
{
	struct params my_params = *(struct params *) p;
	unsigned int prio = my_params.priority;
	so_handler *thread_func = my_params.thread_func;

	// anunta parintele ca e gata pentru planificare
	EnterCriticalSection(&my_scheduler->lock);
	my_scheduler->ready_var = 1;
	WakeConditionVariable(&my_scheduler->ready);
	LeaveCriticalSection(&my_scheduler->lock);

	// -> asteaptă să fie planificat
	EnterCriticalSection(&my_scheduler->lock);
	while (my_scheduler->turn_id != GetCurrentThreadId())
		SleepConditionVariableCS(&my_scheduler->running,
				&my_scheduler->lock, INFINITE);
	LeaveCriticalSection(&my_scheduler->lock);

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
	th_info *new_thread;
	tid_t thread_id;
	struct params p;
	HANDLE hThread;

	if (func == NULL || priority < 0 || priority > SO_MAX_PRIO)
		return INVALID_TID;

	// -> initializare thread struct
	new_thread = malloc(sizeof(th_info));

	if (new_thread == NULL)
		return INVALID_TID;

	new_thread->priority = priority;
	new_thread->thread_func = func;
	new_thread->time_available = my_scheduler->time_slice;
	new_thread->waiting_io = 0;
	new_thread->device_io = -1;

	p.priority = priority;
	p.thread_func = func;

	// -> creare thread nou ce va executa functia start_thread
	hThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) start_thread,
				(void *) &p, 0, &thread_id);
	if (hThread == NULL)
		return INVALID_TID;

	new_thread->thread_id = thread_id;
	push(my_scheduler->pq, new_thread, priority);

	my_scheduler->hThreads[my_scheduler->num_threads] = hThread;
	my_scheduler->threadIds[my_scheduler->num_threads] = thread_id;
	my_scheduler->num_threads++;

	// -> asteaptă ca threadul să intre în starea READY/RUN
	EnterCriticalSection(&my_scheduler->lock);
	while (my_scheduler->ready_var == 0)
		SleepConditionVariableCS(&my_scheduler->ready,
				&my_scheduler->lock, INFINITE);
	my_scheduler->ready_var = 0;
	LeaveCriticalSection(&my_scheduler->lock);

	if (exists_thread(GetCurrentThreadId()) == 1)
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

	if (io < 0 || io >= (unsigned int)my_scheduler->nr_IO_devices) {
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
	int woken_up_threads;

	my_scheduler->crt_thread_info->time_available--;

	if (io < 0 || io >= (unsigned int)my_scheduler->nr_IO_devices) {
		schedule(1);
		return -1;
	}

	woken_up_threads = wake_up_all(my_scheduler->pq, io);

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
	int ret;
	int i;

	if (init_error == 1 || initialized == 0)
		return;

	// asteapta toate thread-urile
	for (i = 0; i < my_scheduler->num_threads; i++) {
		ret = WaitForSingleObject(my_scheduler->hThreads[i], INFINITE);
		if (ret == WAIT_FAILED)
			return;
		CloseHandle(my_scheduler->hThreads[i]);
	}

	destroy_queue(my_scheduler->pq);
	free(my_scheduler->pq);
	free(my_scheduler->hThreads);
	free(my_scheduler->threadIds);
	DeleteCriticalSection(&my_scheduler->lock);
	free(my_scheduler);

	initialized = 0;
	init_error = 0;
}
