#include "cfg.h"
#include "argparser.h"
#include "pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static cfg_t cfg = 
{
	.queue_capacity = MAX_QUEUE_CAPACITY,
	.num_threads = MAX_WORKER_THREADS,
	.verbose = 0,
};

/**
 * Main program entry point
 * @param argc Count of command-line arguments
 * @param argv Command-line arguments
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

	pool = pool_create(cfg.num_threads, cfg.queue_capacity);
	pool_destroy(pool);

	srand(time(NULL));

//	for (i = 0; i < n; i++)
//	{
//		printf("%d\n", rand() % 50);
//	}

	return (0);
}
