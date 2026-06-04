/*
 * Interactive first-run configuration for new miners.
 *
 * Writes a starter cpuminer-conf.json pointed at the official Vericonomy pool
 * by default, validating input and JSON-escaping every value so a stray quote
 * in a worker name can never corrupt the generated config.
 */
#include "onboard.h"
#include "miner.h"
#include "topo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define ONBOARD_DEFAULT_POOL "stratum+tcp://mine.vericonomy.com:3333"

static void trim_line(char *s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
	                 s[n - 1] == ' '  || s[n - 1] == '\t'))
		s[--n] = '\0';
	/* trim leading whitespace */
	size_t lead = 0;
	while (s[lead] == ' ' || s[lead] == '\t')
		lead++;
	if (lead)
		memmove(s, s + lead, strlen(s + lead) + 1);
}

/* Escape a string for safe embedding inside a JSON string literal. */
static void json_escape(const char *src, char *dst, size_t dstsz)
{
	size_t di = 0;

	if (!dstsz)
		return;
	if (src) {
		for (; *src && di + 1 < dstsz; src++) {
			unsigned char c = (unsigned char)*src;
			if (c == '"' || c == '\\') {
				if (di + 2 >= dstsz)
					break;
				dst[di++] = '\\';
				dst[di++] = (char)c;
			} else if (c < 0x20) {
				if (di + 6 >= dstsz)
					break;
				di += (size_t)snprintf(dst + di, dstsz - di, "\\u%04x", c);
			} else {
				dst[di++] = (char)c;
			}
		}
	}
	dst[di] = '\0';
}

static bool prompt(const char *label, const char *deflt, char *out, size_t outsz)
{
	if (deflt && deflt[0])
		printf("%s [%s]: ", label, deflt);
	else
		printf("%s: ", label);
	fflush(stdout);
	if (!fgets(out, (int)outsz, stdin))
		return false;
	trim_line(out);
	if (!out[0] && deflt)
		snprintf(out, outsz, "%s", deflt);
	return true;
}

bool onboard_interactive(char *out_config_path, size_t pathsz)
{
	char url[512], user[160], pass[128], backup[512], threads[16], path[512];
	char url_e[1024], user_e[512], pass_e[512], backup_e[1024];
	const struct topo_info *tp;
	int rec_threads = 0;
	FILE *f;

	printf("\n=== Verium Miner Setup ===\n");
	printf("Press Enter to accept the [default] shown for each question.\n\n");

	tp = topo_get();
	/* Same formula as runtime when threads=0 (RAM + L3 + bandwidth, not core count). */
	rec_threads = topo_recommended_threads(scrypt_scratchpad_bytes(1048576));
	if (tp && rec_threads > 0) {
		printf("Detected CPU: %d logical / %d physical core(s).\n",
		       tp->logical_cpus, tp->physical_cpus);
		if (tp->performance_cpus > 0 && tp->performance_cpus != tp->physical_cpus)
			printf("  Hybrid CPU: %d performance (P) logical CPUs detected.\n",
			       tp->performance_cpus);
		printf("  threads=0 (auto) will use ~%d worker(s) (~%.0f MB scrypt scratchpad each;\n",
		       rec_threads,
		       scrypt_scratchpad_bytes(1048576) / (1024.0 * 1024.0));
		printf("  auto may be below core count due to RAM, L3 cache, or memory bandwidth).\n\n");
	}

	if (!prompt("Pool URL (stratum+tcp://host:port)", ONBOARD_DEFAULT_POOL,
	            url, sizeof(url)))
		return false;
	if (strncmp(url, "stratum+tcp://", 14) != 0 &&
	    strncmp(url, "stratum+ssl://", 14) != 0)
		printf("  Note: pool URLs normally start with stratum+tcp://\n");

	if (!prompt("Verium wallet address (starts with V), optional .worker",
	            NULL, user, sizeof(user)))
		return false;
	if (user[0] && user[0] != 'V')
		printf("  Note: Verium addresses normally start with an uppercase 'V'.\n");

	if (!prompt("Password (usually 'x')", "x", pass, sizeof(pass)))
		return false;

	if (!prompt("Backup pool URL (optional, Enter to skip)", "",
	            backup, sizeof(backup)))
		return false;

	if (!prompt("Mining threads (0 = auto)", "0", threads, sizeof(threads)))
		return false;
	/* keep only a leading integer; fall back to auto on garbage */
	{
		char *p = threads;
		while (*p == '+' )
			p++;
		long v = strtol(p, NULL, 10);
		if (v < 0)
			v = 0;
		snprintf(threads, sizeof(threads), "%ld", v);
	}

	cpuminer_config_dir(path, sizeof(path));
	mkdir(path, 0755);
	cpuminer_config_json_path(path, sizeof(path));

	f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "Could not write config to %s\n", path);
		return false;
	}

	json_escape(url, url_e, sizeof(url_e));
	json_escape(user, user_e, sizeof(user_e));
	json_escape(pass, pass_e, sizeof(pass_e));
	json_escape(backup, backup_e, sizeof(backup_e));

	fprintf(f, "{\n");
	fprintf(f, "\t\"url\": \"%s\",\n", url_e);
	if (backup[0])
		fprintf(f, "\t\"backup-url\": \"%s\",\n", backup_e);
	fprintf(f, "\t\"user\": \"%s\",\n", user_e);
	fprintf(f, "\t\"pass\": \"%s\",\n", pass_e);
	fprintf(f, "\t\"threads\": %s,\n", threads);
	fprintf(f, "\t\"profile\": \"background\",\n");
	fprintf(f, "\t\"status-interval\": 30,\n");
	fprintf(f, "\t\"api-bind\": \"127.0.0.1:4048\",\n");
	fprintf(f, "\t\"quiet\": false\n");
	fprintf(f, "}\n");
	fclose(f);

	snprintf(out_config_path, pathsz, "%s", path);
	printf("\nConfiguration saved to %s\n", path);
	printf("Starting mining now...\n\n");
	return true;
}
