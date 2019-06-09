#ifndef CFG_H_
#define CFG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cfg {
	uint32_t num_threads;
	uint32_t queue_capacity;
	int verbose;
};

#ifdef __cplusplus
}
#endif

#endif // CFG_H_
