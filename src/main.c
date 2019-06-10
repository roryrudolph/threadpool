#include "cfg.h"
#include "argparser.h"
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

void process_msg(void *arg);
int jsoneq(const char *json, jsmntok_t *tok, const char *s);
int parse_json(const char *buf, size_t n);

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

	static struct cfg cfg = {
		.port = DEFAULT_PORT,
		.queue_capacity = MAX_QUEUE_CAPACITY,
		.nthreads = MAX_WORKER_THREADS,
		.verbose = 0,
	};

	argparser(argc, argv, &cfg);

	if (cfg.verbose) {
		int pad = -18;
		printf("%*s: %u\n", pad, "Port", cfg.port);
		printf("%*s: %u\n", pad, "Number of threads", cfg.nthreads);
		printf("%*s: %u\n", pad, "Queue capacity", cfg.queue_capacity);
		printf("%*s: %s\n", pad, "Verbose", cfg.verbose ? "yes" : "no");
	}

	pool = pool_init(cfg.nthreads, cfg.queue_capacity);
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
	sa.sin_port = htons(cfg.port);
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

	while (1) {
		calen = sizeof(ca);

		if (cfg.verbose)
			printf("Listening on port %u\n", cfg.port);

		if ((cfd = accept(sfd, (struct sockaddr *) &ca, &calen)) < 0) {
			printf("ERROR: accept() failed: %s\n", strerror(errno));
			break;
		}

		if (cfg.verbose) {
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
	n = read(fd, buf, sizeof(buf));
	printf("%zd\n", n);

	if (parse_json(buf, sizeof(buf)) < 0) {

	}

	close(fd);
}

/**
 * @todo Document
 * @param json @todo Document
 * @param tok @todo Document
 * @param s @todo Document
 * @return @todo Document
 */
int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

/**
 * @todo Document
 * @param buf @todo Document
 * @param n @todo Document
 * @return @todo Document
 */
int parse_json(const char *buf, size_t n)
{
	int i;
	int rc;
	jsmn_parser p;
	jsmntok_t t[128]; /* Expect no more than 128 tokens */

	jsmn_init(&p);
	rc = jsmn_parse(&p, buf, n, t, sizeof(t)/sizeof(t[0]));
	if (rc < 0) {
		printf("ERROR: Failed to parse buffer. Is it formatted correctly?\n");
		return -1;
	}

	if (rc < 1 || t[0].type != JSMN_OBJECT) {
		printf("ERROR: Expected top-level to be a JSON object\n");
		return -1;
	}

	for (i = 1; i < rc; i++) {
		if (jsoneq(buf, &t[i], "cmd") == 0) {
			printf("cmd: %.*s\n", t[i+1].end-t[i+1].start, buf+t[i+1].start);
			i++;
		} else {
			printf("unexpected key: %.*s\n", t[i].end-t[i].start, buf+t[i].start);
		}
	}

	return 0;
}
