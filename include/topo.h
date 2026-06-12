#ifndef VERIUM_TOPO_H
#define VERIUM_TOPO_H

#include <stddef.h>
#include <stdint.h>

#define TOPO_MAX_CPUS 512

struct topo_info {
	int logical_cpus;
	int physical_cpus;
	/* Count of performance (P) logical CPUs when OS exposes core type; else 0. */
	int performance_cpus;
	uint64_t l3_bytes;
	uint64_t total_ram_bytes;
	uint64_t avail_ram_bytes;
	/* Logical CPU indices assigned to mining workers (distinct physical where possible). */
	int worker_cpu[TOPO_MAX_CPUS];
	int worker_count;
	char cpu_name[128];
};

void topo_init(void);
const struct topo_info *topo_get(void);

/* Recommended worker count from cache/RAM vs per-thread scratchpad. */
int topo_recommended_threads(size_t scratchpad_bytes);

/* Bind mining worker thread `thr_id` (0 .. n_threads-1). */
void topo_bind_worker(int thr_id);

/* Bind entire process to mask (legacy --cpu-affinity); mask is logical bitmask for low CPUs. */
void topo_bind_process_mask(uint64_t mask);

#endif /* VERIUM_TOPO_H */
