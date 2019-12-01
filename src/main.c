#include "pool.h"
#include "jsmn.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h> /* close() */
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h> /* sockaddr_in */
#include <arpa/inet.h> /* inet_ntop */
#include <getopt.h>
#include <signal.h>

#define VERSION       "0.1"
#define DEFAULT_PORT  30303

static int port = DEFAULT_PORT;
static int capacity = MAX_QUEUE_CAPACITY;
static int nthreads = MAX_WORKER_THREADS;
static int verbose = 0;
static int keep_going = 0;

/**
 * @param argv0 @todo TODO Document
 */
void print_help(const char *argv0)
{
	printf("\
Usage: %s [OPTIONS]\n\
\n\
Options:\n\
  -c, --capacity  \n\
  -p, --port      \n\
  -t, --threads   \n\
  -v, --verbose   \n\
  -V, --version   \n\
\n",
	argv0);
}

/**
 * @todo TODO Document
 * @param argc The count of command-line arguments
 * @param argv Command-line arguments
 */
void argparser(int argc, char **argv)
{
	int c, optind;

	static struct option lopts[] = {
		{ "capacity", no_argument, 0, 'c' },
		{ "port", required_argument, 0, 'p' },
		{ "threads", required_argument, 0, 't' },
		{ "verbose", no_argument, 0, 'v' },
		{ "version", no_argument, 0, 'V' },
		{ "help", no_argument, 0, '?' },
		{ 0, 0, 0, 0 }
	};

	while ((c = getopt_long(argc, argv, "c:p:t:vV?", lopts, &optind)) != -1) {
		switch (c) {
		case 'c': /* capacity */
			capacity = strtoul(optarg, 0, 0);
			break;
		case 'p': /* port */
			port = strtoul(optarg, 0, 0);
			break;
		case 't': /* nthreads */
			nthreads = strtoul(optarg, 0, 0);
			break;
		case 'v': /*verbose */
			verbose = 1;
			break;
		case 'V': /* version */
			printf("%s\n", VERSION);
			exit(0);
			break;
		case '?': /* help */
			print_help(argv[0]);
			exit(0);
			break;
		default:
			printf("Unknown argument '%c'\n", c);
			exit(1);
			break;
		}
	}
}

/**
 * Processes a received socket message
 * @param arg The buffer containing the received socket message.
 *   This should cast to a `const char *` type.
 */
void process_msg(void *arg)
{
	int fd;
	char buf[4096];
	ssize_t n;

	if (arg == NULL)
		return;

	fd = *(int *)arg;

	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf)-1);

	/* Process buffer contents here */
	printf("Read %zd bytes: %s\n", n, buf);

	close(fd);
}

/**
 * @todo TODO Document
 * @param signo @todo TODO Document
 */
void sigint_handler(int signo)
{
	if (signo != SIGINT)
		return;

	if (verbose)
		printf("Caught SIGINT\n");

	keep_going = 0;
}

/**
 * Main program entry point
 * @param argc The count of command-line arguments
 * @param argv Command-line arguments
 * @return Returns 0 on success, else error
 */
int main(int argc, char **argv)
{
	int sfd;
	int cfd;
	pool_t *pool;
	struct sockaddr_in sa;
	struct sockaddr_in ca;
	socklen_t salen;
	socklen_t calen;
	struct sigaction action_new;
	struct sigaction action_old;

	argparser(argc, argv);

	if (verbose) {
		int pad = -1 * (int)strlen("Number of threads");
		printf("%*s: %u\n", pad, "Port", port);
		printf("%*s: %u\n", pad, "Number of threads", nthreads);
		printf("%*s: %u\n", pad, "Queue capacity", capacity);
		printf("%*s: %s\n", pad, "Verbose", verbose ? "yes" : "no");
	}

	pool = pool_init(nthreads, capacity);
	if (pool == NULL) {
		printf("ERROR: %s\n", poolerrno_str(poolerrno));
		return 1;
	}

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("ERROR: socket() failed: %s\n", strerror(errno));
		pool_free(pool);
		return 1;
	}

	/* Configure the socket for binding */
	salen = sizeof(sa);
	memset(&sa, 0, salen);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = INADDR_ANY;

	/* Bind the socket to a port and address */
	if (bind(sfd, (struct sockaddr *) &sa, salen) < 0) {
		printf("ERROR: bind() failed: %s\n", strerror(errno));
		close(sfd);
		pool_free(pool);
		return 1;
	}

	/* Mark socket for listening. See man listen(2) for details */
	if (listen(sfd, 128) < 0) {
		printf("ERROR: listen() failed: %s\n", strerror(errno));
		close(sfd);
		pool_free(pool);
		return 1;
	}

	action_new.sa_handler = sigint_handler;
	sigemptyset(&action_new.sa_mask);
	action_new.sa_flags = 0;

	sigaction(SIGINT, NULL, &action_old);
	if (action_old.sa_handler != SIG_IGN)
		sigaction(SIGINT, &action_new, NULL);
	sigaction(SIGHUP, NULL, &action_old);
	if (action_old.sa_handler != SIG_IGN)
		sigaction(SIGHUP, &action_new, NULL);
	sigaction(SIGTERM, NULL, &action_old);
	if (action_old.sa_handler != SIG_IGN)
		sigaction (SIGTERM, &action_new, NULL);	

	keep_going = 1;

	while (keep_going) {
		calen = sizeof(ca);

		if (verbose)
			printf("Listening on port %u\n", port);

		if ((cfd = accept(sfd, (struct sockaddr *) &ca, &calen)) < 0) {
			if (errno != EINTR)
				printf("ERROR: accept() failed: %s\n", strerror(errno));
			break;
		}

		if (verbose) {
			char buf[INET_ADDRSTRLEN];
			memset(buf, 0, sizeof(buf));
			inet_ntop(ca.sin_family, &ca.sin_addr, buf, sizeof(buf));
			printf("Received connection from %s (cfd=%d)\n", buf, cfd);
		}

		if (pool_enqueue(pool, process_msg, &cfd) < 0) {
			printf("WARN: pool_enqueue() failed: %s\n",
				poolerrno_str(poolerrno));
			close(cfd);
		}
	}

	close(sfd);

	pool_free(pool);

	return 0;
}

