#ifndef POOL_H_
#define POOL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define MAX_QUEUE_CAPACITY  65536
#define MAX_WORKER_THREADS  4

typedef struct pool pool_t;

pool_t *pool_create(uint8_t num_threads, uint32_t capacity);
void pool_destroy(pool_t *pool);


#ifdef __cplusplus
}
#endif

#endif // POOL_H_
