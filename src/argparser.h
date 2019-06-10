#ifndef ARGPARSER_H_
#define ARGPARSER_H_

#include "pool.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prints the standard help text from option argument -? or --help
 */
void print_help(void)
{
	printf("\
Usage: threadpool [OPTIONS...]\n\
\n\
This program demonstrates the use of a thread pool by creating a TCP JSON\n\
server and dispatching worker threads to listen for and process incoming\n\
connections. It is assumed the client is sending JSON strings to the server\n\
in the form below:\n\
  TODO\n\
\n\
Options\n\
  -c, --capacity <NUM>  The maximum queue capacity. This is the max amount\n\
                        of outstanding tasks the queue can hold.\n\
                        Default=%u\n\
  -p, --port <NUM>      The port to listen on for incoming client connections.\n\
                        Default=%u\n\
  -t, --threads <NUM>   Number of threads in the pool available for\n\
                        processing. Default=%u\n\
  -v, --verbose\n\
  -?, --help\n\
\n\
Report bugs to <rory.rudolph@outlook.com>\n",
	MAX_QUEUE_CAPACITY, DEFAULT_PORT, MAX_WORKER_THREADS);
}

/**
 * Parses the command line arguments and stores results (if any) into the
 * configuration variable
 * @param argc Number of arguments in `argv`
 * @param argv The command-line arguments
 * @param cfg Pointer to program configuration object
 */
void argparser(int argc, char **argv, struct cfg *cfg)
{
	static struct option long_opts[] = {
		{ "capacity", required_argument, 0, 'c' },
		{ "port", required_argument, 0, 'p' },
		{ "threads", required_argument, 0, 't' },
		{ "verbose", no_argument, 0, 'v' },
		{ "help", no_argument, 0, '?' },
		{ 0, 0, 0, 0 }
	};

	static const char *short_opts = "c:p:t:v?";

	int opt_index;
	int c;
	unsigned long n;

	while (1) {
		c = getopt_long(argc, argv, short_opts, long_opts, &opt_index);
		if (c == -1)
			break;

		switch (c) {
		case 'c': /* capacity */
			n = strtoul(optarg, NULL, 10);
			if (n == ULONG_MAX && errno == ERANGE) {
				printf("Invalid argument: %s\n", optarg);
				exit(1);
			}
			if (cfg)
				cfg->queue_capacity = (unsigned int)n;
			break;
		case 'p': /* port */
			n = strtoul(optarg, NULL, 10);
			if (n == ULONG_MAX && errno == ERANGE) {
				printf("Invalid argument: %s\n", optarg);
				exit(1);
			}
			if (cfg)
				cfg->port = (unsigned int)n;
			break;
		case 't': { /* threads */
			unsigned long n = strtoul(optarg, NULL, 10);
			if (n == ULONG_MAX && errno == ERANGE) {
				printf("Invalid argument: %s\n", optarg);
				exit(1);
			}
			if (cfg)
				cfg->nthreads = (unsigned int)n;
			break;
		}
		case 'v': /* verbose */
			if (cfg)
				cfg->verbose = 1;
			break;
		case '?': /* help */
			print_help();
			exit(0);
			break;
		default:
			exit(1);
			break;
		}
	}
}

#ifdef __cplusplus
}
#endif

#endif /* ARGPARSER_H_ */
