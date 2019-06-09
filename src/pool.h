#ifndef POOL_H_
#define POOL_H_

#include <stdlib.h> /* size_t */
#include <limits.h> /* INT_MIN */

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKER_THREADS   16
#define MAX_QUEUE_CAPACITY   65536

extern int poolerrno;

typedef enum {
	POOLERRNO_QUEUE_FULL = INT_MIN,
	POOLERRNO_OK = 0
} poolerrno_t;

typedef struct pool pool_t;

pool_t *pool_init(size_t nthreads, size_t capacity);
void pool_free(pool_t *pool);
int pool_enqueue(pool_t *pool, void (*func)(void *), void *arg);
int pool_get_queue_count(pool_t *pool, size_t *count);
int pool_get_queue_capacity(pool_t *pool, size_t *capacity);

#ifdef __cplusplus
}
#endif

#endif /* POOL_H_ */
