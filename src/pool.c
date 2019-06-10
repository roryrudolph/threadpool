#include "pool.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h> /* strerror() */
#include <errno.h> /* ESRCH, EINVAL, etc */
#include <signal.h>

int poolerrno = POOLERRNO_OK;

/**
 * A queue item that will be handled by a worker thread. The worker thread
 * will pop one of these items off the queue, then call the `func`
 * method with the `arg` parameter.
 */
typedef struct {
	void (*func)(void *arg); /** Function pointer */
	void *arg; /** Argument passed to the `func` function pointer */
} queue_item_t;

/**
 * The runtime status of the pool. Typically, the state should always
 * be `POOL_STATUS_NORMAL` until `pool_free()` is called.
 */
typedef enum {
	POOL_STATUS_NORMAL = 0,
	POOL_STATUS_SHUTDOWN,
} pool_status_t;

/**
 * The threadpool struct
 */
struct pool {
	queue_item_t *queue; /** Pointer to the beginning of the queue space */
	queue_item_t *head; /** Points to the queue 'push' point */
	queue_item_t *tail; /** Points to the queue 'pop' point */
	pthread_t *threads; /** Pointer to the beginning of the threads space */
	pthread_mutex_t mtx; /** The mutex used to lock critical sections */
	pthread_cond_t cnd; /** The condtion used for thread synchronization */
	pool_status_t status; /** The runtime status of the pool */
	size_t nthreads; /** Number of threads allocated */
	size_t nalive; /** Number of threads alive */
	size_t capacity; /** Maximum queue depth */
	size_t count; /** Current queue depth */
};

/* Definition here, more details at implementation */
static void *worker(void *arg);

/**
 * Initializes a thread pool used to perform various asynchronous work
 * @param nthreads The number of worker threads to use
 * @param capacity The depth of the work queue, i.e. how many possible tasks
 * @return Returns a `pool_t` object on success. On error, NULL is returned
 * and `poolerrno` is set to an error number.
 */
pool_t *pool_init(size_t nthreads, size_t capacity)
{
	int rc;
	pool_t *pool;

	/* Verify function arguments */
	if (nthreads > MAX_WORKER_THREADS) {
		poolerrno = EINVAL;
		return NULL;
	}
	if (capacity > MAX_QUEUE_CAPACITY) {
		poolerrno = EINVAL;
		return NULL;
	}

	/* Allocate a pool object */
	pool = (pool_t *) calloc(1, sizeof(pool_t));
	if (pool == NULL) {
		poolerrno = ENOMEM;
		return NULL;
	}

	do {
		/* Allocate the pool's queue */
		pool->queue = (queue_item_t *)calloc(capacity, sizeof(*pool->queue));
		if (pool->queue == NULL) {
			poolerrno = ENOMEM;
			break;
		}
		pool->head = pool->queue;
		pool->tail = pool->queue;
	
		/* Allocate the pool's worker threads */
		pool->threads = (pthread_t *)calloc(nthreads, sizeof(*pool->threads));
		if (pool->threads == NULL) {
			poolerrno = ENOMEM;
			break;
		}

		/* Initialize mutex */
		if ((rc = pthread_mutex_init(&pool->mtx, NULL)) != 0) {
			poolerrno = rc;
			break;
		}
	
		/* Initialize condition */
		if ((rc = pthread_cond_init(&pool->cnd, NULL)) != 0) {
			poolerrno = rc;
			break;
		}

	} while (0);

	/* If there is an error, back out the memory allocations, then exit */
	if (poolerrno != POOLERRNO_OK) {
		if (pool->threads)
			free(pool->threads);
		pool->threads = NULL;
		if (pool->queue)
			free(pool->queue);
		pool->queue = NULL;
		free(pool);
		pool = NULL;
		return NULL;
	}
	
	/* To get here a pool and the mutexes/conds were successfully
	 * allocated. Now configure the pool and launch the worker threads.
	 */

	pool->status = POOL_STATUS_NORMAL;
	pool->nthreads = nthreads;
	pool->nalive = 0;
	pool->capacity = capacity;
	pool->count = 0;

	for (size_t i = 0; i < nthreads; i++) {
		rc = pthread_create(&pool->threads[i], NULL, worker, (void *)pool);
		if (rc != 0) {
			poolerrno = rc;
			break;
		}
		pool->nalive++;
	}

	return pool;
}

/**
 * @todo Document
 * @param pool @todo Document
 */
void pool_free(pool_t *pool)
{
	int i;
	int rc;
	size_t nalive;

	if (pool == NULL)
		return;

	/* First things first... get the mutex */
	pthread_mutex_lock(&pool->mtx);

	/* This can *probably* be done outside the mutex */
	nalive = pool->nalive;

	/* We are protected here, so set the status to SHUTDOWN and
	 * broadcast a signal out to all waiting threads to wake them up
	 */
	pool->status = POOL_STATUS_SHUTDOWN;
	pthread_cond_broadcast(&pool->cnd);

	/* However, some of the threads could be doing work and thus won't receive
	 * the broadcast. No worries. We set the status so that when they finish
	 * their current work they will shutdown
	 */
	pthread_mutex_unlock(&pool->mtx);

	/* Wait for threads to shutdown themselves. Waiting on them (joining)
	 * is the only way to be sure they are done
	 */
	for (i = 0; (size_t)i < nalive; ++i) {
		if ((rc = pthread_join(pool->threads[i], NULL)) != 0) {
			printf("WARN: Could not join thread %d: %s\n", i, strerror(rc));
		}
		pool->nalive--;
	}

	/* To make it here, all the threads are done working and exited
	 * Destroy the mutex
	 */
	if ((rc = pthread_mutex_destroy(&pool->mtx)) != 0)
		printf("ERROR: Could not destroy mutex: %s\n", strerror(rc));

	/* Destroy the signal condition */
	if ((rc = pthread_cond_destroy(&pool->cnd)) != 0)
		printf("ERROR: Could not destroy condition: %s\n", strerror(rc));

	if (pool->queue)
		free(pool->queue);
	pool->queue = NULL;

	if (pool->threads)
		free(pool->threads);
	pool->threads = NULL;

	if (pool)
		free(pool);
	pool = NULL;

	return;
}

/**
 * Puts a work item into the tail of the queue if it is not full.
 * @param pool The pool to use
 * @param func The function used for the work item
 * @param arg The argument to the function used for the work item
 * @return Returns 0 on success. On error, less than 0 is returned and 
 *   `poolerrno` is set.
 */
int pool_enqueue(pool_t *pool, void (*func)(void *), void *arg)
{
	int rc;

	if (pool == NULL) {
		poolerrno = EINVAL;
		return -1;
	}

	if ((rc = pthread_mutex_lock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	if (pool->count == pool->capacity) {
		poolerrno = POOLERRNO_QUEUE_FULL;
		return -1;
	}

	/* Tail points to new work item */
	pool->tail->func = func;
	pool->tail->arg = arg;

	if (++pool->tail == pool->queue + pool->capacity)
		pool->tail = pool->queue;

	pool->count++;

	/* Tell waiting threads there's something to work on */
	pthread_cond_signal(&pool->cnd);

	if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	return 0;
}

/**
 * Gets the current number of elements in the pool's queue
 * @param pool The pool to use
 * @param count This variable is filled with the current queue count
 * @return Returns 0 on success and `count` is set. On error, less than 0
 * is returned, `count` is undefined, and `poolerrno` is set.
 */
int pool_get_queue_count(pool_t *pool, size_t *count)
{
	int rc;

	if (pool == NULL || count == NULL) {
		poolerrno = EINVAL;
		return -1;
	}

	if ((rc = pthread_mutex_lock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	*count = pool->count;

	if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	return 0;
}

/**
 * @todo Document
 * @param pool The pool to use
 * @param capacity This variable is filled with the queue capacity
 * @return Returns 0 on success and `capacity` is set. On error, less than 0
 * is returned, `capacity` is undefined, and `poolerrno` is set.
 */
int pool_get_queue_capacity(pool_t *pool, size_t *capacity)
{
	int rc;

	if (pool == NULL || capacity == NULL) {
		poolerrno = EINVAL;
		return -1;
	}

	if ((rc = pthread_mutex_lock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	*capacity = pool->capacity;

	if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	return 0;
}

/**
 * Converts a `poolerrno` error number into a human-readable string
 * @param poolerrno The error number to convert to a string
 * @return Returns a human-readable string of `poolerrno`
 */
const char *poolerrno_str(int poolerrno)
{
	switch (poolerrno) {
	case POOLERRNO_OK:
		return "ok";
	case POOLERRNO_QUEUE_FULL:
		return "queue is full";
	default:
		return strerror(poolerrno);
	}
}

/**
 * This is a worker thread that acts on the queue. There can be multiple
 * workers, which is the reason for the mutex locks
 * @param arg This must be the pool_t object allocated from 
 * @return @todo Document
 */
void *worker(void *arg)
{
	int rc;
	pool_t *pool;
	queue_item_t item;

	if (arg == NULL) {
		poolerrno = EINVAL;
		return NULL;
	}

	pool = (pool_t *)arg;

	for (;;) {
		if ((rc = pthread_mutex_lock(&pool->mtx)) != 0) {
			poolerrno = rc;
			return NULL;
		}

		while (pool->count == 0 && pool->status != POOL_STATUS_SHUTDOWN) {
			if ((rc = pthread_cond_wait(&pool->cnd, &pool->mtx)) != 0) {
				poolerrno = rc;
			}
		}

		if (pool->status == POOL_STATUS_SHUTDOWN)
			break;

		item.func = pool->head->func;
		item.arg = pool->head->arg;

		if (++pool->head >= pool->queue + pool->capacity)
			pool->head = pool->queue;

		pool->count--;

		if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
			poolerrno = rc;
			return NULL;
		}

		(*item.func)(item.arg);
	}

	if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return NULL;
	}

	return NULL;
}
