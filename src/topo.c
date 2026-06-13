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

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#if defined(__linux__)
#include <sched.h>
#endif

static struct topo_info g_topo;

/* Worker priority: performance (P) first, then unknown, then efficient (E). */
#define TOPO_PREF_P        0
#define TOPO_PREF_UNKNOWN  1
#define TOPO_PREF_E        2

typedef struct {
	int cpu;
	int pref;
} topo_cpu_ent;

static int topo_cmp_cpu_ent(const void *a, const void *b)
{
	const topo_cpu_ent *x = (const topo_cpu_ent *)a;
	const topo_cpu_ent *y = (const topo_cpu_ent *)b;

	if (x->pref != y->pref)
		return x->pref - y->pref;
	return x->cpu - y->cpu;
}

#if defined(WIN32)
static int win_core_pref[TOPO_MAX_CPUS];
static int win_core_pref_valid;
#endif

static uint64_t topo_os_reserve_bytes(void)
{
	uint64_t t = g_topo.total_ram_bytes;

	if (t == 0)
		return 2ULL * 1024ULL * 1024ULL * 1024ULL;
	/* ~12.5% for OS, clamped to 1–4 GiB. */
	{
		uint64_t r = t / 8;
		if (r < 1ULL * 1024ULL * 1024ULL * 1024ULL)
			r = 1ULL * 1024ULL * 1024ULL * 1024ULL;
		if (r > 4ULL * 1024ULL * 1024ULL * 1024ULL)
			r = 4ULL * 1024ULL * 1024ULL * 1024ULL;
		return r;
	}
}

/* CPU thread budget before RAM cap (no platform-specific overrides). */
static int topo_recommended_cpu_threads(void)
{
	int logical = g_topo.logical_cpus > 0 ? g_topo.logical_cpus : 1;
	int physical = g_topo.physical_cpus > 0 ? g_topo.physical_cpus : logical;

	/* HT or hybrid (logical > physical): one worker per physical core. */
	if (logical > physical)
		return physical;

	/* Homogeneous CPUs: leave one core for the OS. */
	if (physical > 1)
		return physical - 1;
	return 1;
}

static int topo_linux_core_pref(int cpu)
{
	char path[128];
	FILE *f;
	int t = 0;

	snprintf(path, sizeof(path),
		"/sys/devices/system/cpu/cpu%d/topology/core_type", cpu);
	f = fopen(path, "r");
	if (!f)
		return TOPO_PREF_UNKNOWN;
	if (fscanf(f, "%d", &t) != 1)
		t = 0;
	fclose(f);
	/* Linux kernel: 1 = Atom/E, 2 = Core/P (Intel hybrid). */
	if (t == 2)
		return TOPO_PREF_P;
	if (t == 1)
		return TOPO_PREF_E;
	return TOPO_PREF_UNKNOWN;
}

#if defined(WIN32)
typedef struct _TOPO_CPU_SET_ENTRY {
	ULONG Size;
	ULONG Type;
	struct {
		ULONG Id;
		USHORT Group;
		UCHAR LogicalProcessorIndex;
		UCHAR CoreIndex;
		UCHAR LastLevelCacheIndex;
		USHORT NumaNodeIndex;
		UCHAR EfficiencyClass;
	} CpuSet;
} TOPO_CPU_SET_ENTRY;

typedef BOOL (WINAPI *topo_pfn_GetSystemCpuSetInformation)(
	PVOID, ULONG, PULONG, HANDLE, ULONG);

static void topo_win_load_core_prefs(void)
{
	topo_pfn_GetSystemCpuSetInformation pfn;
	ULONG len = 0;
	PVOID buf = NULL;
	TOPO_CPU_SET_ENTRY *e;
	ULONG off;
	BYTE best_class = 255;
	int logical = g_topo.logical_cpus;

	win_core_pref_valid = 0;
	if (logical > TOPO_MAX_CPUS)
		logical = TOPO_MAX_CPUS;
	for (int i = 0; i < logical; i++)
		win_core_pref[i] = TOPO_PREF_UNKNOWN;

	pfn = (topo_pfn_GetSystemCpuSetInformation)(void *)
		GetProcAddress(GetModuleHandleA("kernel32"), "GetSystemCpuSetInformation");
	if (!pfn || !pfn(NULL, 0, &len, NULL, 0) || !len)
		return;
	buf = malloc(len);
	if (!buf || !pfn(buf, len, &len, NULL, 0))
		goto out;

	for (off = 0; off < len; ) {
		e = (TOPO_CPU_SET_ENTRY *)((char *)buf + off);
		if (e->Size < sizeof(ULONG) * 2)
			break;
		if (e->Type == 0 && e->CpuSet.EfficiencyClass < best_class)
			best_class = e->CpuSet.EfficiencyClass;
		off += e->Size;
	}
	if (best_class == 255)
		goto out;

	for (off = 0; off < len; ) {
		e = (TOPO_CPU_SET_ENTRY *)((char *)buf + off);
		if (e->Size < sizeof(ULONG) * 2)
			break;
		if (e->Type == 0 && e->CpuSet.Group == 0) {
			int idx = (int)e->CpuSet.LogicalProcessorIndex;
			if (idx >= 0 && idx < logical) {
				win_core_pref[idx] = (e->CpuSet.EfficiencyClass == best_class)
					? TOPO_PREF_P : TOPO_PREF_E;
			}
		}
		off += e->Size;
	}
	win_core_pref_valid = 1;

out:
	free(buf);
}
#endif /* WIN32 */

static int topo_core_pref(int cpu)
{
#if defined(__linux__)
	return topo_linux_core_pref(cpu);
#elif defined(WIN32)
	if (cpu >= 0 && cpu < TOPO_MAX_CPUS && win_core_pref_valid)
		return win_core_pref[cpu];
	return TOPO_PREF_UNKNOWN;
#else
	(void)cpu;
	return TOPO_PREF_UNKNOWN;
#endif
}

static void topo_build_worker_schedule(void)
{
	topo_cpu_ent ents[TOPO_MAX_CPUS];
	int logical = g_topo.logical_cpus;
	int n = 0;
	int p_logical = 0;

	if (logical < 1)
		logical = 1;
	if (logical > TOPO_MAX_CPUS)
		logical = TOPO_MAX_CPUS;

	for (int cpu = 0; cpu < logical; cpu++) {
		ents[n].cpu = cpu;
		ents[n].pref = topo_core_pref(cpu);
		if (ents[n].pref == TOPO_PREF_P)
			p_logical++;
		n++;
	}

	if (n > 0)
		qsort(ents, (size_t)n, sizeof(ents[0]), topo_cmp_cpu_ent);

	g_topo.worker_count = n;
	for (int i = 0; i < n; i++)
		g_topo.worker_cpu[i] = ents[i].cpu;

	/* P-logical count for logging; 0 when OS did not expose core types. */
	g_topo.performance_cpus = p_logical > 0 ? p_logical : 0;
}

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
			int p = phys_id >= 0 ? phys_id : 0;
			/* ARM (e.g. Raspberry Pi) often omits core id; use processor index. */
			int c = core_id >= 0 ? core_id : cur;
			int i, found = 0;
			for (i = 0; i < ncores; i++) {
				if (cores[i].p == p && cores[i].c == c) {
					found = 1;
					break;
				}
			}
			if (!found && ncores < 512) {
				cores[ncores].p = p;
				cores[ncores].c = c;
				cores[ncores].cpu = cur;
				ncores++;
			}
			cur = phys_id = core_id = -1;
		}
	}
	fclose(f);

	/* Last processor block may not end with a blank line (common on ARM). */
	if (cur >= 0 && ncores < 512) {
		int p = phys_id >= 0 ? phys_id : 0;
		int c = core_id >= 0 ? core_id : cur;
		int i, found = 0;
		for (i = 0; i < ncores; i++) {
			if (cores[i].p == p && cores[i].c == c) {
				found = 1;
				break;
			}
		}
		if (!found) {
			cores[ncores].p = p;
			cores[ncores].c = c;
			cores[ncores].cpu = cur;
			ncores++;
		}
	}

	g_topo.physical_cpus = ncores > 0 ? ncores : g_topo.logical_cpus;
	if (siblings > 0 && g_topo.physical_cpus == g_topo.logical_cpus)
		g_topo.physical_cpus = g_topo.logical_cpus / siblings;
	if (g_topo.physical_cpus < 1)
		g_topo.physical_cpus = g_topo.logical_cpus;

	/* worker schedule built in topo_init() via topo_build_worker_schedule(). */

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
}
#elif defined(WIN32)
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

	{
		MEMORYSTATUSEX ms;
		ms.dwLength = sizeof(ms);
		if (GlobalMemoryStatusEx(&ms)) {
			g_topo.total_ram_bytes = ms.ullTotalPhys;
			g_topo.avail_ram_bytes = ms.ullAvailPhys;
		}
	}
	return;

fallback:
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		g_topo.logical_cpus = (int)si.dwNumberOfProcessors;
		g_topo.physical_cpus = g_topo.logical_cpus;
	}
	{
		MEMORYSTATUSEX ms;
		ms.dwLength = sizeof(ms);
		if (GlobalMemoryStatusEx(&ms)) {
			g_topo.total_ram_bytes = ms.ullTotalPhys;
			g_topo.avail_ram_bytes = ms.ullAvailPhys;
		}
	}
}
#elif defined(__APPLE__)
static int topo_darwin_sysctl_int(const char *name, int *out)
{
	size_t len = sizeof(int);

	if (sysctlbyname(name, out, &len, NULL, 0) != 0)
		return 0;
	return 1;
}

static void topo_darwin_probe(void)
{
	int val = 0;

	if (topo_darwin_sysctl_int("hw.logicalcpu", &val) && val > 0)
		g_topo.logical_cpus = val;
#if defined(_SC_NPROCESSORS_ONLN)
	else
		g_topo.logical_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
	else
		g_topo.logical_cpus = 1;
#endif
	if (g_topo.logical_cpus < 1)
		g_topo.logical_cpus = 1;

	if (topo_darwin_sysctl_int("hw.physicalcpu", &val) && val > 0)
		g_topo.physical_cpus = val;
	else
		g_topo.physical_cpus = g_topo.logical_cpus;

	{
		uint64_t mem = 0;
		size_t len = sizeof(mem);

		if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) == 0)
			g_topo.total_ram_bytes = mem;
	}

	{
		vm_statistics64_data_t vm = {0};
		mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
		vm_size_t page = 0;

		if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
				(host_info64_t)&vm, &count) == KERN_SUCCESS
				&& host_page_size(mach_host_self(), &page) == KERN_SUCCESS
				&& page > 0) {
			uint64_t avail = (uint64_t)(vm.free_count + vm.inactive_count
					+ vm.purgeable_count) * (uint64_t)page;
			g_topo.avail_ram_bytes = avail;
		}
	}
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
	topo_win_load_core_prefs();
#elif defined(__APPLE__)
	topo_darwin_probe();
#else
	topo_generic_probe();
#endif

	topo_build_worker_schedule();

	if (g_topo.worker_count < 1) {
		g_topo.worker_count = g_topo.logical_cpus > 0 ? g_topo.logical_cpus : 1;
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
	int by_cpu = topo_recommended_cpu_threads();
	int by_ram = by_cpu;
	uint64_t ram_for_miner;
	const uint64_t os_reserve = topo_os_reserve_bytes();

	ram_for_miner = g_topo.avail_ram_bytes;
	if (ram_for_miner < os_reserve && g_topo.total_ram_bytes > os_reserve)
		ram_for_miner = g_topo.total_ram_bytes - os_reserve;
	else if (ram_for_miner > os_reserve)
		ram_for_miner -= os_reserve;
	else if (g_topo.total_ram_bytes > os_reserve)
		ram_for_miner = g_topo.total_ram_bytes - os_reserve;
	else
		ram_for_miner = 0;

#if defined(WIN32)
	/* ullAvailPhys overstates commit headroom when many large VirtualAllocs start together. */
	if (ram_for_miner > 0)
		ram_for_miner = (ram_for_miner * 3) / 4;
#endif

	if (scratchpad_bytes > 0 && ram_for_miner > 0) {
		by_ram = (int)(ram_for_miner / scratchpad_bytes);
		if (by_ram < 1)
			by_ram = 1;
	}

	{
		int rec = by_cpu;
		if (by_ram < rec)
			rec = by_ram;
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
