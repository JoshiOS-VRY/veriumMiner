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
#include <ctype.h>

#ifdef WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define ONBOARD_DEFAULT_POOL VERIUM_DEFAULT_POOL_URL
#define ONBOARD_DEFAULT_SOLO VERIUM_DEFAULT_SOLO_URL

static bool url_is_solo_rpc(const char *url)
{
	if (!url || !url[0])
		return false;
	return !strncasecmp(url, "http://", 7) || !strncasecmp(url, "https://", 8);
}

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

static void sanitize_mining_url(char *url)
{
	size_t n;

	if (!url || !url[0])
		return;
	trim_line(url);
	n = strlen(url);
	while (n > 0) {
		char c = url[n - 1];
		if (c == ']' || c == ')' || c == '}' || c == '"' || c == '\'' ||
		    c == ' ' || c == '\t')
			url[--n] = '\0';
		else
			break;
	}
}

static bool mining_url_valid(const char *url, char *err, size_t errsz)
{
	const char *port;
	char *end;

	if (!url || !url[0]) {
		snprintf(err, errsz, "URL cannot be empty.");
		return false;
	}
	if (url_is_solo_rpc(url))
		return true;
	if (!strncasecmp(url, "stratum+tcp://", 14) ||
	    !strncasecmp(url, "stratum+ssl://", 14)) {
		port = strrchr(url, ':');
		if (!port || port == url)
			goto bad_port;
		port++;
		if (!*port || !isdigit((unsigned char)port[0]))
			goto bad_port;
		end = NULL;
		strtol(port, &end, 10);
		if (!end || *end != '\0')
			goto bad_port;
		long p = strtol(port, NULL, 10);
		if (p < 1 || p > 65535)
			goto bad_port;
		return true;
	}
	snprintf(err, errsz,
	         "Use stratum+tcp://host:port for pools or http://127.0.0.1:33987 for solo.");
	return false;

bad_port:
	snprintf(err, errsz,
	         "Invalid port in URL (must be 1–65535). Check for stray characters like ']'.");
	return false;
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
	char saved_def[512];

	saved_def[0] = '\0';
	if (deflt && deflt[0])
		snprintf(saved_def, sizeof(saved_def), "%s", deflt);

	if (saved_def[0])
		printf("%s [%s]: ", label, saved_def);
	else
		printf("%s: ", label);
	fflush(stdout);
	if (!fgets(out, (int)outsz, stdin))
		return false;
	trim_line(out);
	if (!out[0] && saved_def[0])
		snprintf(out, outsz, "%s", saved_def);
	return true;
}

static bool prompt_mining_mode(bool *solo_out)
{
	char line[32];
	size_t i;

	printf("Mining mode — [P]ool (Vericonomy) or [S]olo (your veriumd) [P]: ");
	fflush(stdout);
	if (!fgets(line, sizeof(line), stdin))
		return false;
	trim_line(line);
	if (!line[0] || line[0] == 'p' || line[0] == 'P') {
		*solo_out = false;
		return true;
	}
	if (line[0] == 's' || line[0] == 'S') {
		*solo_out = true;
		return true;
	}
	for (i = 0; line[i]; i++) {
		if (line[i] == 's' || line[i] == 'S') {
			*solo_out = true;
			return true;
		}
		if (line[i] == 'p' || line[i] == 'P') {
			*solo_out = false;
			return true;
		}
	}
	fprintf(stderr, "Enter P for pool or S for solo.\n");
	return prompt_mining_mode(solo_out);
}

void verium_sanitize_mining_url(char *url)
{
	sanitize_mining_url(url);
}

bool onboard_interactive(char *out_config_path, size_t pathsz)
{
	char url[512] = { 0 }, url_default[512] = { 0 };
	char user[160] = { 0 }, pass[128] = { 0 };
	char coinbase[160] = { 0 }, backup[512] = { 0 }, threads[16] = "0", path[512];
	char url_e[1024], user_e[512], pass_e[512], coinbase_e[512], backup_e[1024];
	const struct topo_info *tp;
	bool solo = false, mode_chosen = false;
	int rec_threads = 0;
	FILE *f;
	json_error_t err;
	json_t *existing;

	snprintf(url_default, sizeof(url_default), "%s", ONBOARD_DEFAULT_POOL);
	snprintf(url, sizeof(url), "%s", url_default);
	snprintf(pass, sizeof(pass), "%s", "x");

	cpuminer_config_json_path(path, sizeof(path));
	existing = JSON_LOADF(path, &err);
	if (json_is_object(existing)) {
		json_t *val;
		val = json_object_get(existing, "url");
		if (json_is_string(val) && json_string_value(val)[0]) {
			snprintf(url, sizeof(url), "%s", json_string_value(val));
			snprintf(url_default, sizeof(url_default), "%s", url);
			solo = url_is_solo_rpc(url);
			mode_chosen = true;
		}
		val = json_object_get(existing, "user");
		if (json_is_string(val) && json_string_value(val)[0])
			snprintf(user, sizeof(user), "%s", json_string_value(val));
		val = json_object_get(existing, "pass");
		if (json_is_string(val) && json_string_value(val)[0])
			snprintf(pass, sizeof(pass), "%s", json_string_value(val));
		val = json_object_get(existing, "backup-url");
		if (json_is_string(val))
			snprintf(backup, sizeof(backup), "%s", json_string_value(val));
		val = json_object_get(existing, "threads");
		if (json_is_integer(val))
			snprintf(threads, sizeof(threads), "%d",
			         (int)json_integer_value(val));
		val = json_object_get(existing, "coinbase-addr");
		if (json_is_string(val) && json_string_value(val)[0])
			snprintf(coinbase, sizeof(coinbase), "%s",
			         json_string_value(val));
	}
	if (existing)
		json_decref(existing);

	printf("\n=== Verium Miner Setup ===\n");
	printf("Press Enter to accept the [default] shown for each question.\n");
	printf("Re-run this wizard anytime to change wallet, pool, or threads ");
	printf("(--setup, or \"Change Settings\" on macOS).\n\n");

	tp = topo_get();
	/* Same formula as runtime when threads=0 (CPU budget and available RAM). */
	rec_threads = topo_recommended_threads(scrypt_scratchpad_bytes(1048576));
	if (tp && rec_threads > 0) {
		printf("Detected CPU: %d logical / %d physical core(s).\n",
		       tp->logical_cpus, tp->physical_cpus);
		if (tp->performance_cpus > 0 && tp->performance_cpus < tp->logical_cpus)
			printf("  Hybrid CPU: %d performance (P) logical CPUs (P-first worker order).\n",
			       tp->performance_cpus);
		printf("  threads=0 (auto) will use ~%d worker(s) (~%.0f MB scrypt scratchpad each;\n",
		       rec_threads,
		       scrypt_scratchpad_bytes(1048576) / (1024.0 * 1024.0));
		printf("  capped by physical-core budget and available RAM).\n\n");
	}

	if (!mode_chosen) {
		if (!prompt_mining_mode(&solo))
			return false;
		snprintf(url_default, sizeof(url_default), "%s",
		         solo ? ONBOARD_DEFAULT_SOLO : ONBOARD_DEFAULT_POOL);
		snprintf(url, sizeof(url), "%s", url_default);
	}

	for (;;) {
		const char *url_label = solo
		    ? "Solo node URL (http://127.0.0.1:33987 or your veriumd RPC)"
		    : "Pool URL (stratum+tcp://host:port)";

		if (!prompt(url_label, url_default, url, sizeof(url)))
			return false;
		sanitize_mining_url(url);
		if (!url[0])
			snprintf(url, sizeof(url), "%s", url_default);
		solo = url_is_solo_rpc(url);
		{
			char verr[256];
			if (mining_url_valid(url, verr, sizeof(verr)))
				break;
			fprintf(stderr, "%s\n", verr);
		}
	}

	if (solo) {
		if (!prompt("RPC username (from verium.conf rpcuser)", user, user,
		            sizeof(user)))
			return false;
		if (!pass[0] || !strcmp(pass, "x"))
			pass[0] = '\0';
		if (!prompt("RPC password (from verium.conf rpcpassword)", pass, pass,
		            sizeof(pass)))
			return false;
		if (!prompt("Payout address for block rewards (--coinbase-addr, V…)",
		            coinbase[0] ? coinbase : NULL, coinbase, sizeof(coinbase)))
			return false;
		if (coinbase[0] && coinbase[0] != 'V')
			printf("  Note: Verium payout addresses normally start with 'V'.\n");
	} else {
		if (!prompt("Verium wallet address (starts with V), optional .worker",
		            user[0] ? user : NULL, user, sizeof(user)))
			return false;
		if (user[0] && user[0] != 'V')
			printf("  Note: Verium addresses normally start with an uppercase 'V'.\n");

		if (!prompt("Password (usually 'x')", pass, pass, sizeof(pass)))
			return false;

		if (!prompt("Backup pool URL (optional, Enter to skip)", "",
		            backup, sizeof(backup)))
			return false;
		if (backup[0]) {
			char verr[256];
			sanitize_mining_url(backup);
			if (!mining_url_valid(backup, verr, sizeof(verr))) {
				fprintf(stderr, "Backup URL ignored: %s\n", verr);
				backup[0] = '\0';
			}
		}
	}

	if (!prompt("Mining threads (0 = auto)", threads, threads, sizeof(threads)))
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

	if (!pass[0])
		snprintf(pass, sizeof(pass), "x");

	json_escape(url, url_e, sizeof(url_e));
	json_escape(user, user_e, sizeof(user_e));
	json_escape(pass, pass_e, sizeof(pass_e));
	json_escape(backup, backup_e, sizeof(backup_e));
	json_escape(coinbase, coinbase_e, sizeof(coinbase_e));

	fprintf(f, "{\n");
	fprintf(f, "\t\"url\": \"%s\",\n", url_e);
	if (!solo && backup[0])
		fprintf(f, "\t\"backup-url\": \"%s\",\n", backup_e);
	fprintf(f, "\t\"user\": \"%s\",\n", user_e);
	fprintf(f, "\t\"pass\": \"%s\",\n", pass_e);
	if (solo && coinbase[0])
		fprintf(f, "\t\"coinbase-addr\": \"%s\",\n", coinbase_e);
	fprintf(f, "\t\"threads\": %s,\n", threads);
	fprintf(f, "\t\"profile\": \"%s\",\n", solo ? "dedicated" : "background");
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
