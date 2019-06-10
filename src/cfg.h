#ifndef CFG_H_
#define CFG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_PORT  42000

struct cfg {
	int verbose;
	uint32_t nthreads;
	uint32_t queue_capacity;
	uint16_t port;
};

#ifdef __cplusplus
}
#endif

#endif /* CFG_H_ */
