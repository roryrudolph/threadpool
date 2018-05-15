#include "pool.h"
#include <pthread.h>
#include <stdlib.h>

struct pool
{
	uint8_t num_threads;
	uint32_t queue_capacity;
};

/**
 * Creates a thread pool
 * @param num_threads Available number of threads for processing
 * @param queue_capacity Max number of outstanding tasks
 */
pool_t *pool_create(uint8_t num_threads, uint32_t queue_capacity)
{
	pool_t *pool = calloc(1, sizeof(pool_t));
	pool->num_threads = num_threads;
	pool->queue_capacity = queue_capacity;
	return (pool);

}

/**
 * Destroys and releases memory associated with a thread pool
 * @param pool The thread pool to destroy
 */
void pool_destroy(pool_t *pool)
{
	if (pool)
		free(pool);
}
