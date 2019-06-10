#ifndef POOL_H_
#define POOL_H_

#include <stdlib.h> /* size_t */
#include <limits.h> /* INT_MIN, INT_MAX */

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKER_THREADS   16
#define MAX_QUEUE_CAPACITY   65536

/**
 * Global error value set by the pool functions, very much like the
 * normal `errno`
 * @todo Is this thread-safe? Should it be?
 * @todo Where is this defined?
 */
extern int poolerrno;

/** These are custom error defintions used by the pool functions. */
typedef enum {
	POOLERRNO_OK = 0,
	POOLERRNO_QUEUE_FULL = 1000,
} poolerrno_t;

/**
 * Forward declaration of the pool_t type. The actual struct is defined in
 * the source file, which is intended to obfuscate the use of this type
 * except through the API calls below
 */
typedef struct pool pool_t;

/*-----------------------*
 * THREAD POOL API CALLS *
 *-----------------------*/

pool_t *pool_init(size_t nthreads, size_t capacity);
void pool_free(pool_t *pool);
int pool_enqueue(pool_t *pool, void (*func)(void *), void *arg);
int pool_get_queue_count(pool_t *pool, size_t *count);
int pool_get_queue_capacity(pool_t *pool, size_t *capacity);

const char *poolerrno_str(int poolerrno);

#ifdef __cplusplus
}
#endif

#endif /* POOL_H_ */
