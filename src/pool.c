#include "pool.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h> /* strerror() */
#include <errno.h> /* ESRCH, EINVAL, etc */
#include <signal.h>

int poolerrno = POOL_ERRNO_OK;

/**
 * A queue element that will be handled by a worker thread. The worker thread
 * will pop one of these elements off the queue, then call the @c func
 * method with the @c arg parameter.
 */
typedef struct {
	void (*func)(void *arg); /** Function pointer */
	void *arg; /** Argument passed to the @c func function pointer */
} queue_element_t;

/**
 * The runtime status of the pool. Typically, the state should always
 * be @c POOL_STATUS_NORMAL until @c pool_free() is called.
 */
typedef enum {
	POOL_STATUS_NORMAL = 0,
	POOL_STATUS_SHUTDOWN,
} pool_status_t;

/**
 * The thread pool struct
 */
struct pool {
	queue_element_t *queue; /** Pointer to the beginning of the queue space */
	queue_element_t *head; /** Points to the queue 'push' point */
	queue_element_t *tail; /** Points to the queue 'pop' point */
	pthread_t *threads; /** Pointer to the beginning of the threads space */
	pthread_mutex_t mtx; /** The mutex used to lock critical sections */
	pthread_cond_t cnd; /** The condtion used for thread synchronization */
	pool_status_t status; /** The runtime status of the pool */
	size_t nthreads; /** Number of threads allocated */
	size_t nalive; /** Number of threads alive */
	size_t capacity; /** Maximum queue depth */
	size_t count; /** Current queue depth */
};

static void *worker(void *arg);

/**
 * Initializes a thread pool used to perform various asynchronous work
 * @param nthreads The number of worker threads to use
 * @param capacity The depth of the work queue, i.e. how many possible tasks
 * @return Returns a @c pool_t object on success. On error, NULL is returned
 * and poolerrno is set to an error number.
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
	if (pool = NULL) {
		poolerrno = ENOMEM;
		return NULL;
	}

	do {
		/* Allocate the pool's queue */
		pool->queue = (queue_element_t *) calloc(capacity, sizeof(*pool->queue));
		if (pool->queue == NULL) {
			poolerrno = ENOMEM;
			break;
		}
		pool->head = pool->queue;
		pool->tail = pool->queue;
	
		/* Allocate the pool's worker threads */
		pool->threads = (pthread_t *) calloc(nthreads, sizeof(*pool->threads));
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

	} while (0);

	/* If there is an error, back out the memory allocations, then exit */
	if (poolerrno != POOLERRNO_OK) {
		if (pool) {
			if (pool->threads) {
				for (size_t i = 0; i < pool->nalive; i++) {
					pthread_cancel(pool->threads[i]);
					pthread_join(pool->threads[i], NULL);
					pool->nalive--;
				}
				free(pool->threads);
			}
			pool->threads = NULL;
			if (pool->queue)
				free(pool->queue);
			pool->queue = NULL;
			free(pool);
			pool = NULL;
		}
		return NULL;
	}

	return pool;
}

/**
 * TODO Document
 * @param pool TODO Document
 */
void pool_free(pool_t *pool)
{
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
	for (size_t i = 0; i < nalive; i++) {
		if ((rc = pthread_join(pool->threads[i], NULL)) != 0) {
			printf("ERROR: Could not join thread %lu: %s\n", i, strerror(rc));
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
 * TODO Document
 * @param pool TODO Document
 * @param func TODO Document
 * @param arg TODO Document
 * @return Returns 0 on success. Less than 0 is returned on error and
 * poolerrno is set.
 */
int pool_enqueue(pool_t *pool, void (*func)(void *), void *arg)
{
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

	pool->tail->func = func;
	pool->tail->arg = arg;

	if (++pool->tail == pool->queue + pool->capacity)
		pool->tail = pool->queue;

	pool->count++;

	pthread_cond_signal(&pool->cnd);

	if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
		poolerrno = rc;
		return -1;
	}

	return 0;
}

/**
 * TODO Document
 * @param pool TODO Document
 * @param count TODO Document
 * @return Returns 0 on success and @c count is set, less than 0 on error
 * and @c count is undefined
 */
int pool_get_queue_count(pool_t *pool, size_t *count)
{
	if (pool == NULL || count == NULL)
		return EINVAL;

	if (pthread_mutex_lock(&pool->mtx) != 0)
		return -1;

	*count = pool->count;

	if (pthread_mutex_unlock(&pool->mtx) != 0)
		return -1;

	return 0;
}

/**
 * TODO Document
 * @param pool TODO Document
 * @param capacity TODO Document
 * @return Returns 0 on success, less than 0 on error.
 */
int pool_get_queue_capacity(pool_t *pool, size_t *capacity)
{
	if (pool == NULL || capacity == NULL)
		return EINVAL;

	if (pthread_mutex_lock(&pool->mtx) != 0)
		return -1;

	*capacity = pool->capacity;

	if (pthread_mutex_unlock(&pool->mtx) != 0)
		return -1;

	return 0;
}

/**
 * This is a worker thread that acts on the queue. There can be multiple
 * workers, which is the reason for the mutex locks
 * @param arg This must be the pool_t object allocated from 
 * @return TODO Document
 */
void *worker(void *arg)
{
	int rc;
	pool_t *pool;
	queue_element_t task;

	if (arg == NULL) {
		poolerrno = EINVAL;
		pthread_exit(NULL);
	}
	pool = (pool_t *)arg;

	for (;;) {
		if ((rc = pthread_mutex_lock(&pool->mtx)) != 0) {
			printf("ERROR: Could not lock mutex: %s\n", strerror(rc));
			pthread_exit(NULL);
		}

		while (pool->count == 0 && pool->status != POOL_STATUS_SHUTDOWN) {
			if ((rc = pthread_cond_wait(&pool->cnd, &pool->mtx)) != 0)
				printf("ERROR: Could not wait on signal: %s\n", strerror(rc));
		}

		if (pool->status == POOL_STATUS_SHUTDOWN)
			break;

		task.func = pool->head->func;
		task.arg = pool->head->arg;

		if (++pool->head >= pool->queue + pool->capacity)
			pool->head = pool->queue;

		pool->count--;

		if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
			printf("ERROR: Could not unlock mutex: %s\n", strerror(rc));
			pthread_exit(NULL);
		}

		(*task.func)(task.arg);
	}

	if ((rc = pthread_mutex_unlock(&pool->mtx)) != 0) {
		printf("ERROR: Could not unlock mutex: %s\n", strerror(rc));
		pthread_exit(NULL);
	}

	pthread_exit(NULL);
}
