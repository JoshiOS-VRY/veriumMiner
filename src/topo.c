/*
 * CPU topology: physical vs logical cores, L3 budget, affinity binding.
 */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "topo.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#if defined(__linux__)
#include <sched.h>
#endif

static struct topo_info g_topo;

#if defined(__linux__)
static int parse_u64_suffix(const char *s, uint64_t *out)
{
	char *end = NULL;
	unsigned long long v = strtoull(s, &end, 10);
	if (end == s)
		return 0;
	while (*end == ' ' || *end == '\t')
		end++;
	if (*end == 'K' || *end == 'k') {
		v *= 1024ULL;
		end++;
	} else if (*end == 'M' || *end == 'm') {
		v *= 1024ULL * 1024ULL;
		end++;
	} else if (*end == 'G' || *end == 'g') {
		v *= 1024ULL * 1024ULL * 1024ULL;
		end++;
	}
	*out = (uint64_t)v;
	return 1;
}

#endif /* __linux__ */

#if defined(__linux__)
static void topo_linux_probe(void)
{
	FILE *f;
	char line[512];
	int siblings = 0;
	int max_cpu = -1;
	int cur = -1;
	int phys_id = -1, core_id = -1;
	/* Map (phys,core) -> first logical cpu */
	struct { int p, c, cpu; } cores[512];
	int ncores = 0;

	memset(cores, 0xff, sizeof(cores));

	g_topo.logical_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (g_topo.logical_cpus < 1)
		g_topo.logical_cpus = 1;

	f = fopen("/proc/cpuinfo", "r");
	if (!f)
		return;

	while (fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "processor", 9)) {
			char *p = strchr(line, ':');
			if (p)
				cur = atoi(p + 1);
			if (cur > max_cpu)
				max_cpu = cur;
		} else if (!strncmp(line, "physical id", 11)) {
			char *p = strchr(line, ':');
			if (p)
				phys_id = atoi(p + 1);
		} else if (!strncmp(line, "core id", 7)) {
			char *p = strchr(line, ':');
			if (p)
				core_id = atoi(p + 1);
		} else if (!strncmp(line, "cpu cores", 9)) {
			char *p = strchr(line, ':');
			if (p && !siblings)
				siblings = atoi(p + 1);
		} else if (line[0] == '\n' && cur >= 0) {
			int i, found = 0;
			for (i = 0; i < ncores; i++) {
				if (cores[i].p == phys_id && cores[i].c == core_id) {
					found = 1;
					break;
				}
			}
			if (!found && ncores < 512) {
				cores[ncores].p = phys_id;
				cores[ncores].c = core_id;
				cores[ncores].cpu = cur;
				ncores++;
			}
			cur = phys_id = core_id = -1;
		}
	}
	fclose(f);

	g_topo.physical_cpus = ncores > 0 ? ncores : g_topo.logical_cpus;
	if (siblings > 0 && g_topo.physical_cpus == g_topo.logical_cpus)
		g_topo.physical_cpus = g_topo.logical_cpus / siblings;
	if (g_topo.physical_cpus < 1)
		g_topo.physical_cpus = g_topo.logical_cpus;

	for (int i = 0; i < ncores && i < TOPO_MAX_CPUS; i++)
		g_topo.worker_cpu[i] = cores[i].cpu;
	g_topo.worker_count = ncores < TOPO_MAX_CPUS ? ncores : TOPO_MAX_CPUS;

	/* L3: pick largest cache level with type Unified or level 3 */
	for (int cpu = 0; cpu <= max_cpu && cpu < 4; cpu++) {
		char path[128];
		for (int idx = 0; idx < 8; idx++) {
			uint64_t sz = 0;
			snprintf(path, sizeof(path),
				"/sys/devices/system/cpu/cpu%d/cache/index%d/size",
				cpu, idx);
			f = fopen(path, "r");
			if (!f)
				continue;
			if (fgets(line, sizeof(line), f) && parse_u64_suffix(line, &sz)) {
				if (sz > g_topo.l3_bytes)
					g_topo.l3_bytes = sz;
			}
			fclose(f);
		}
	}

	f = fopen("/proc/meminfo", "r");
	if (f) {
		uint64_t avail_kb = 0;
		while (fgets(line, sizeof(line), f)) {
			if (!strncmp(line, "MemTotal:", 9)) {
				uint64_t kb = 0;
				sscanf(line + 9, "%llu", (unsigned long long *)&kb);
				g_topo.total_ram_bytes = kb * 1024ULL;
			} else if (!strncmp(line, "MemAvailable:", 13)) {
				sscanf(line + 13, "%llu", (unsigned long long *)&avail_kb);
			}
		}
		fclose(f);
		if (avail_kb)
			g_topo.avail_ram_bytes = avail_kb * 1024ULL;
	}
	g_topo.performance_cpus = g_topo.physical_cpus;
}
#elif defined(WIN32)
/* CpuSet API (Win10+); resolved at runtime for older SDKs. */
typedef struct _TOPO_CPU_SET_ENTRY {
	ULONG Size;
	ULONG Type;
	struct {
		BYTE Id;
		WORD Group;
		BYTE LogicalProcessorIndex;
		BYTE CoreIndex;
		BYTE LastLevelCacheIndex;
		WORD NumaNodeIndex;
		BYTE EfficiencyClass;
	} CpuSet;
} TOPO_CPU_SET_ENTRY;

typedef BOOL (WINAPI *topo_pfn_GetSystemCpuSetInformation)(
	PVOID, ULONG, PULONG, HANDLE, ULONG);

static int topo_win_cmp_cpu(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static void topo_win_fill_perf_cpus(void)
{
	topo_pfn_GetSystemCpuSetInformation pfn;
	ULONG len = 0;
	PVOID buf = NULL;
	TOPO_CPU_SET_ENTRY *e;
	ULONG off;
	int cpus[TOPO_MAX_CPUS];
	int n = 0;
	BYTE best_class = 255;

	pfn = (topo_pfn_GetSystemCpuSetInformation)(void *)
		GetProcAddress(GetModuleHandleA("kernel32"), "GetSystemCpuSetInformation");
	if (!pfn)
		goto hybrid_fb;

	if (!pfn(NULL, 0, &len, NULL, 0) || !len)
		goto hybrid_fb;
	buf = malloc(len);
	if (!buf || !pfn(buf, len, &len, NULL, 0))
		goto hybrid_fb;

	for (off = 0; off < len; ) {
		e = (TOPO_CPU_SET_ENTRY *)((char *)buf + off);
		if (e->Size < sizeof(ULONG) * 2)
			break;
		if (e->Type == 0 && e->CpuSet.EfficiencyClass < best_class)
			best_class = e->CpuSet.EfficiencyClass;
		off += e->Size;
	}
	if (best_class == 255) {
		free(buf);
		goto hybrid_fb;
	}

	for (off = 0; off < len; ) {
		e = (TOPO_CPU_SET_ENTRY *)((char *)buf + off);
		if (e->Size < sizeof(ULONG) * 2)
			break;
		if (e->Type == 0 && e->CpuSet.EfficiencyClass == best_class
				&& n < TOPO_MAX_CPUS) {
			/* Group 0: affinity mask bit == LogicalProcessorIndex. */
			if (e->CpuSet.Group == 0)
				cpus[n++] = (int)e->CpuSet.LogicalProcessorIndex;
		}
		off += e->Size;
	}
	free(buf);

	if (n < 1)
		goto hybrid_fb; /* hybrid_fb: no buffer held */

	qsort(cpus, (size_t)n, sizeof(cpus[0]), topo_win_cmp_cpu);
	g_topo.performance_cpus = n;
	g_topo.worker_count = n;
	for (int i = 0; i < n; i++)
		g_topo.worker_cpu[i] = cpus[i];
	return;

hybrid_fb:
	/* Alder Lake+: first N logical CPUs are usually P-cores. */
	if (g_topo.logical_cpus == 24 && g_topo.physical_cpus == 16) {
		g_topo.performance_cpus = 16;
		g_topo.worker_count = 16;
		for (int i = 0; i < 16; i++)
			g_topo.worker_cpu[i] = i;
	}
}

static void topo_win_probe(void)
{
	DWORD len = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buf = NULL, p;
	int logical = 0, physical = 0;

	GetLogicalProcessorInformationEx(RelationAll, NULL, &len);
	if (!len)
		goto fallback;
	buf = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(len);
	if (!buf || !GetLogicalProcessorInformationEx(RelationAll, buf, &len))
		goto fallback;

	p = buf;
	while ((char *)p - (char *)buf < (ptrdiff_t)len) {
		if (p->Relationship == RelationProcessorCore) {
			int n = 0;
			for (int g = 0; g < p->Processor.GroupCount; g++) {
				KAFFINITY m = p->Processor.GroupMask[g].Mask;
				while (m) {
					if (m & 1)
						n++;
					m >>= 1;
				}
			}
			if (n > 0) {
				physical++;
				logical += n;
			}
		}
		p = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)
			((char *)p + p->Size);
	}
	free(buf);

	g_topo.logical_cpus = logical > 0 ? logical : 1;
	g_topo.physical_cpus = physical > 0 ? physical : g_topo.logical_cpus;
	g_topo.worker_count = g_topo.physical_cpus;
	for (int i = 0; i < g_topo.worker_count && i < TOPO_MAX_CPUS; i++)
		g_topo.worker_cpu[i] = i;

	{
		MEMORYSTATUSEX ms;
		ms.dwLength = sizeof(ms);
		if (GlobalMemoryStatusEx(&ms)) {
			g_topo.total_ram_bytes = ms.ullTotalPhys;
			g_topo.avail_ram_bytes = ms.ullAvailPhys;
		}
	}
	topo_win_fill_perf_cpus();
	return;

fallback:
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		g_topo.logical_cpus = (int)si.dwNumberOfProcessors;
		g_topo.physical_cpus = g_topo.logical_cpus;
		for (int i = 0; i < g_topo.logical_cpus && i < TOPO_MAX_CPUS; i++)
			g_topo.worker_cpu[i] = i;
		g_topo.worker_count = g_topo.logical_cpus;
	}
	{
		MEMORYSTATUSEX ms;
		ms.dwLength = sizeof(ms);
		if (GlobalMemoryStatusEx(&ms)) {
			g_topo.total_ram_bytes = ms.ullTotalPhys;
			g_topo.avail_ram_bytes = ms.ullAvailPhys;
		}
	}
	topo_win_fill_perf_cpus();
}
#else
static void topo_generic_probe(void)
{
#if defined(_SC_NPROCESSORS_ONLN)
	g_topo.logical_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
	g_topo.logical_cpus = 1;
#endif
	if (g_topo.logical_cpus < 1)
		g_topo.logical_cpus = 1;
	g_topo.physical_cpus = g_topo.logical_cpus;
	g_topo.worker_count = g_topo.logical_cpus;
	for (int i = 0; i < g_topo.logical_cpus && i < TOPO_MAX_CPUS; i++)
		g_topo.worker_cpu[i] = i;
}
#endif

void topo_init(void)
{
	memset(&g_topo, 0, sizeof(g_topo));

#if defined(__linux__)
	topo_linux_probe();
#elif defined(WIN32)
	topo_win_probe();
#else
	topo_generic_probe();
#endif

	if (g_topo.worker_count < 1) {
		g_topo.worker_count = g_topo.physical_cpus;
		for (int i = 0; i < g_topo.worker_count && i < TOPO_MAX_CPUS; i++)
			g_topo.worker_cpu[i] = i;
	}
}

const struct topo_info *topo_get(void)
{
	return &g_topo;
}

int topo_recommended_threads(size_t scratchpad_bytes)
{
	int phys = g_topo.physical_cpus > 0 ? g_topo.physical_cpus : 1;
	int perf = g_topo.performance_cpus > 0 ? g_topo.performance_cpus : phys;
	int perf_phys = perf;
	int by_l3 = phys;
	int by_ram = phys;
	int by_bw = phys;
	uint64_t ram_for_miner;
	const uint64_t os_reserve = 4ULL * 1024ULL * 1024ULL * 1024ULL;

	/* Hybrid Intel: performance_cpus counts P-logical CPUs; use P-core count. */
	if (g_topo.performance_cpus > 0 && g_topo.logical_cpus > g_topo.physical_cpus)
		perf_phys = g_topo.performance_cpus / 2;
	else if (perf > phys && phys > 0)
		perf_phys = phys;
	else if (perf > 1 && perf == phys * 2)
		perf_phys = phys;

	if (scratchpad_bytes > 0 && g_topo.l3_bytes > 0) {
		by_l3 = (int)(g_topo.l3_bytes / scratchpad_bytes);
		if (by_l3 < 1)
			by_l3 = 1;
	}

	ram_for_miner = g_topo.avail_ram_bytes;
	if (ram_for_miner < os_reserve && g_topo.total_ram_bytes > os_reserve)
		ram_for_miner = g_topo.total_ram_bytes - os_reserve;
	else if (ram_for_miner > os_reserve)
		ram_for_miner -= os_reserve;
	else if (g_topo.total_ram_bytes > os_reserve)
		ram_for_miner = g_topo.total_ram_bytes - os_reserve;
	else
		ram_for_miner = 0;

	if (scratchpad_bytes > 0 && ram_for_miner > 0) {
		by_ram = (int)(ram_for_miner / scratchpad_bytes);
		if (by_ram < 1)
			by_ram = 1;
	}

	/* Scrypt is memory-bandwidth bound; cap heuristically on desktop CPUs. */
	by_bw = perf_phys;
	if (by_bw > 12)
		by_bw = 12;

	{
		int rec = by_bw;
		if (by_l3 < rec)
			rec = by_l3;
		if (by_ram < rec)
			rec = by_ram;
		if (perf_phys < rec)
			rec = perf_phys;
		if (rec < 1)
			rec = 1;
		return rec;
	}
}

#if defined(__linux__)
static void topo_bind_linux_cpu(int cpu)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	if (cpu >= 0 && cpu < CPU_SETSIZE)
		CPU_SET(cpu, &set);
	sched_setaffinity(0, sizeof(set), &set);
}
#elif defined(WIN32)
static void topo_bind_win_cpu(int cpu)
{
	DWORD_PTR mask = (cpu >= 0 && cpu < 64) ? (1ULL << cpu) : 1;
	SetThreadAffinityMask(GetCurrentThread(), mask);
}
#endif

void topo_bind_worker(int thr_id)
{
	int cpu;

	if (g_topo.worker_count <= 0)
		return;
	cpu = g_topo.worker_cpu[thr_id % g_topo.worker_count];

#if defined(__linux__)
	topo_bind_linux_cpu(cpu);
#elif defined(WIN32)
	topo_bind_win_cpu(cpu);
#else
	(void)cpu;
#endif
}

void topo_bind_process_mask(uint64_t mask)
{
#if defined(__linux__)
	cpu_set_t set;
	int i;
	CPU_ZERO(&set);
	for (i = 0; i < g_topo.logical_cpus && i < CPU_SETSIZE; i++) {
		if (mask & (1ULL << i))
			CPU_SET(i, &set);
	}
	sched_setaffinity(0, sizeof(set), &set);
#elif defined(WIN32)
	SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)mask);
#else
	(void)mask;
#endif
}
