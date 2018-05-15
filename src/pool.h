#ifndef POOL_H_
#define POOL_H_

#include <stdlib.h> // size_t

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_WORKER_THREADS   16
#define MAX_QUEUE_CAPACITY   65536

typedef struct pool pool_t;

pool_t *pool_init(size_t nthreads, size_t capacity);
int pool_enqueue(pool_t *pool, void (*func)(void *), void *arg);
int pool_get_queue_count(pool_t *pool, size_t *count);
int pool_get_queue_capacity(pool_t *pool, size_t *capacity);
void pool_free(pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif // POOL_H_
