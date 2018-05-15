#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#define MAX_QUEUE_CAPACITY  1024
#define MAX_WORKER_THREADS  4

typedef struct cfg
{
	uint8_t num_threads;
	uint32_t queue_capacity;
	int verbose;
} cfg_t;


void parse_args(int argc, char **argv, cfg_t *cfg)
{
	static struct option long_opts[] =
	{
		{ "capacity", required_argument, 0, 'c' },
		{ "help", no_argument, 0, '?' },
		{ "threads", required_argument, 0, 't' },
		{ "verbose", no_argument, 0, 'v' },
		{ 0, 0, 0, 0 }
	};

	static const char *short_opts = "c:t:v?";

	int opt_index;
	int c;

	while (1)
	{
		c = getopt_long(argc, argv, short_opts, long_opts, &opt_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 'c':
				printf("TODO capacity argument\n");
				break;
			case 't':
				printf("TODO threads argument\n");
				break;
			case 'v':
				cfg->verbose = 1;
				break;
			case '?':
				printf("TODO help argument\n");
				break;
			default:
				printf("TODO default/unknown argument\n");
				break;
		}
	}
}

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
	int i, n;
	time_t t;

	parse_args(argc, argv, &cfg);

	n = 5;

	srand((unsigned)time(&t));

	for (i = 0; i < n; i++)
	{
		printf("%d\n", rand() % 50);
	}

	return (0);
}
