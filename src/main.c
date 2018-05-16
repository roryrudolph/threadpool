#include "cfg.h"
#include "argparser.h"
#include "pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/**
 * Program configuration
 */
static cfg_t cfg = 
{
	.queue_capacity = MAX_QUEUE_CAPACITY,
	.num_threads = MAX_WORKER_THREADS,
	.verbose = 0,
};

/**
 * @param arg TODO Document
 */
void func(void *arg)
{
	printf("%s %d\n", __func__, *(int *)arg);
}


/**
 * Main program entry point
 * @param argc Count of command-line arguments
 * @param argv Command-line arguments
 * @return Returns 0 on success, else error
 */
int main(int argc, char **argv)
{
	pool_t *pool;

	argparser(argc, argv, &cfg);

	if (cfg.verbose)
	{
		int pad = -18;
		printf("%*s: %u\n", pad, "Number of threads", cfg.num_threads);
		printf("%*s: %u\n", pad, "Queue capacity", cfg.queue_capacity);
		printf("%*s: %s\n", pad, "Verbose output", cfg.verbose ? "yes" : "no");
	}

	pool = pool_init(cfg.num_threads, cfg.queue_capacity);

	srand(time(NULL));

	for (size_t i = 0; i < cfg.queue_capacity; i++)
		pool_enqueue(pool, func, &i);

	pool_free(pool);

	return (0);
}

