#ifndef ARGPARSER_H_
#define ARGPARSER_H_

#include "cfg.h"
#include "pool.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * Prints the standard help text from option argument -? or --help
 */
void print_help(void)
{
	printf("\
Usage: threadpool [OPTIONS...]\n\
\n\
Options\n\
  -c, --capacity <NUM>  The maximum queue capacity. This is the max amount\n\
                        of outstanding tasks the queue can hold.\n\
                        Default=65536\n\
  -t, --threads <NUM>   Number of threads in the pool available for\n\
                        processing. Default=4\n\
  -v, --verbose\n\
  -?, --help\n\
\n\
Report bugs to <rory.rudolph@outlook.com>\n");
}


/**
 * Parses the command line arguments and stores results (if any) into the
 * configuration variable
 * @param argc Count of arguments in @c argv
 * @param argv Command-line arguments
 * @param cfg Pointer to program configuration variable
 */
void argparser(int argc, char **argv, cfg_t *cfg)
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
			{
				unsigned long n = strtoul(optarg, NULL, 10);
				if (n == ULONG_MAX && errno == ERANGE)
					n = MAX_QUEUE_CAPACITY;
				cfg->queue_capacity = (uint32_t)n;
				break;
			}
			case 't':
			{
				unsigned long n = strtoul(optarg, NULL, 10);
				if (n == ULONG_MAX && errno == ERANGE)
					n = MAX_WORKER_THREADS;
				cfg->num_threads = (uint8_t)n;
				break;
			}
			case 'v':
				cfg->verbose = 1;
				break;
			case '?':
				print_help();
				exit(0);
				break;
			default:
				//print_help();
				exit(1);
				break;
		}
	}
}


#ifdef __cplusplus
}
#endif

#endif // ARGPARSER_H_
