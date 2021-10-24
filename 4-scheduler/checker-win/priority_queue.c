#include "priority_queue.h"

void init(pqueue *pq)
{
	pq->head = -1;
	pq->tail = -1;
}

int is_empty(pqueue *pq)
{
	if (pq->tail == -1)
		return 1;
	return 0;
}

int is_full(pqueue *pq)
{
	if ((pq->tail + 1) % MAX == pq->head)
		return 1;

	return 0;
}

th_info *peek(pqueue *pq)
{
	int i;
	th_info *best = NULL;
	th_info *it;
	int found = 0;

	if (is_empty(pq))
		return NULL;

	i = pq->head;

	if (i == pq->tail) {
		// queue with 1 element
		return pq->nodes->data;
	}

	while (i != pq->tail) {
		it = pq->nodes[i].data;
		if (it->waiting_io == 0) {
			best = it;
			found = 1;
			break;
		}

		i = (i + 1) % MAX;
	}

	if (found == 0) {
		// check last element
		if (pq->head != pq->tail)  {
			it = pq->nodes[i].data;
			if (it->waiting_io == 0)
				best = it;
		}
	}

	return best;
}

void push(pqueue *pq, th_info *thread_info, int priority)
{
	int i;

	if (is_full(pq))
		printf("Overflow\n");
	else {
		if (is_empty(pq)) {
			pq->head = 0;
			pq->tail = 0;
			pq->nodes[0].data = thread_info;
			pq->nodes[0].priority = priority;
		} else {
			i = pq->tail;

			while (priority > pq->nodes[i].priority) {
				pq->nodes[(i + 1) % MAX] = pq->nodes[i];

				i = (i - 1 + MAX) % MAX;
				if ((i + 1) % MAX == pq->head)
					break;
			}

			// insert elem
			i = (i + 1) % MAX;
			pq->nodes[i].data = thread_info;
			pq->nodes[i].priority = priority;
 
			// modify tail
			pq->tail = (pq->tail + 1) % MAX;
		}
	}
}

void pop(pqueue *pq, th_info *thread_info)
{
	int i;
	th_info *it;
	int deleted = 0;

	if (is_empty(pq)) {
		printf("Underflow\n");
	} else {
		i = pq->head;

		while (i != pq->tail) {
			it = pq->nodes[i].data;
			if (it == thread_info) {
				free(it);

				// shift all elements
				while (i != pq->tail) {
					pq->nodes[i] = pq->nodes[i + 1];

					if (i + 1 == pq->tail)
						break;

					i = (i + 1) % MAX;
				}
				deleted = 1;
				break;
			}
			i = (i + 1) % MAX;
		}

		if (deleted == 0) {
			it = pq->nodes[i].data;
			if (it == thread_info)
				free(it);
		}

		// modify tail
		pq->tail = (pq->tail - 1) % MAX;
	}
}

void pop_without_free(pqueue *pq, th_info *thread_info)
{
	int i;
	th_info *it;
	int deleted = 0;

	if (is_empty(pq)) {
		printf("Underflow\n");
	} else {
		i = pq->head;

		while (i != pq->tail) {
			it = pq->nodes[i].data;
			if (it == thread_info) {
				// shift all elements
				while (i != pq->tail) {
					pq->nodes[i] = pq->nodes[i + 1];

					if (i + 1 == pq->tail)
						break;

					i = (i + 1) % MAX;
				}
				deleted = 1;
				break;
			}
			i = (i + 1) % MAX;
		}
		if (deleted == 0)
			it = pq->nodes[i].data;

		// modify tail
		pq->tail = (pq->tail - 1) % MAX;
	}
}

int available_threads_with_prio(pqueue *pq, int priority)
{
	int nr = 0;
	int i;

	if (is_empty(pq))
		return 0;

	i = pq->head;

	if (i == pq->tail) {
		// queue with 1 element
		return pq->nodes[i].data->priority == priority
			&& pq->nodes[i].data->waiting_io == 0;
	}

	while (pq->nodes[i].data->priority == priority) {
		if (pq->nodes[i].data->waiting_io == 0)
			nr++;

		if (i == pq->tail)
			break;

		i = (i + 1) % MAX;
	}

	return nr;
}

void reorder_nodes(pqueue *pq)
{
	int i;
	int end = 0;
	th_info *ref_thread;

	if (is_empty(pq))
		return;

	i = pq->head;

	if (i == pq->tail) {
		// queue with 1 element (do nothing)
		return;
	}

	while (pq->nodes[i].data->waiting_io == 1) {
		i = (i + 1) % MAX;
		if (i == pq->tail) {
			end = 1;
			break;
		}
	}

	if (end == 1)
		return;

	ref_thread = pq->nodes[i].data;

	i = (i + 1) % MAX;

	if (available_threads_with_prio(pq, ref_thread->priority) > 1) {
		pop_without_free(pq, ref_thread);
		push(pq, ref_thread, ref_thread->priority);

		while (pq->nodes[i].data->priority == ref_thread->priority) {
			if (pq->nodes[i].data->waiting_io == 0)
				break;

			pop_without_free(pq, pq->nodes[i].data);
			push(pq, pq->nodes[i].data,
				pq->nodes[i].data->priority);

			i = (i + 1) % MAX;
			if (i == pq->tail)
				break;
		}
	}
}

int wake_up_all(pqueue *pq, int io)
{
	int nr = 0;
	int i;
	th_info *it;

	if (is_empty(pq))
		return 0;

	i = pq->head;

	while (i != pq->tail) {
		it = pq->nodes[i].data;
		if (it->waiting_io == 1 && it->device_io == io) {
			nr++;
			it->waiting_io = 0;
		}
		i = (i + 1) % MAX;
	}

	// wake up the last element
	if (pq->head != pq->tail) {
		it = pq->nodes[i].data;
		if (it->waiting_io == 1 && it->device_io == io) {
			nr++;
			it->waiting_io = 0;
		}
	}

	return nr;
}

void destroy_queue(pqueue *pq)
{
	int i;
	th_info *it;

	if (is_empty(pq)) {
		// queue is empty
		return;
	}

	i = pq->head;

	while (i != pq->tail) {
		it = pq->nodes[i].data;
		free(it);
		i = (i + 1) % MAX;
	}

	// free the last element
	if (pq->head != pq->tail)  {
		it = pq->nodes[i].data;
		free(it);
	}
}
