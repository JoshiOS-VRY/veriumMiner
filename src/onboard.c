/*
 * Interactive first-run configuration for new miners.
 */
#include "onboard.h"
#include "miner.h"

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

static void trim_line(char *s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
		s[--n] = '\0';
}

bool onboard_interactive(char *out_config_path, size_t pathsz)
{
	char url[512], user[128], pass[128], threads[16], path[512];
	FILE *f;

	printf("\n=== Verium Miner Setup ===\n");
	printf("No pool configured. Enter your mining pool details.\n\n");

	printf("Pool URL (stratum+tcp://host:port): ");
	if (!fgets(url, sizeof(url), stdin))
		return false;
	trim_line(url);
	if (!url[0])
		return false;

	printf("Wallet or worker name: ");
	if (!fgets(user, sizeof(user), stdin))
		return false;
	trim_line(user);

	printf("Password (often 'x'): ");
	if (!fgets(pass, sizeof(pass), stdin))
		return false;
	trim_line(pass);
	if (!pass[0])
		strcpy(pass, "x");

	printf("Mining threads (Enter = auto): ");
	if (!fgets(threads, sizeof(threads), stdin))
		return false;
	trim_line(threads);

	cpuminer_config_dir(path, sizeof(path));
	mkdir(path, 0755);
	cpuminer_config_json_path(path, sizeof(path));

	f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "Could not write config to %s\n", path);
		return false;
	}

	fprintf(f, "{\n");
	fprintf(f, "\t\"url\": \"%s\",\n", url);
	fprintf(f, "\t\"user\": \"%s\",\n", user);
	fprintf(f, "\t\"pass\": \"%s\",\n", pass);
	fprintf(f, "\t\"threads\": %s,\n", threads[0] ? threads : "0");
	fprintf(f, "\t\"api-bind\": \"127.0.0.1:4048\",\n");
	fprintf(f, "\t\"quiet\": false,\n");
	fprintf(f, "\t\"status-interval\": 30\n");
	fprintf(f, "}\n");
	fclose(f);

	snprintf(out_config_path, pathsz, "%s", path);
	printf("\nConfiguration saved to %s\n", path);
	printf("Restart the miner or it will load this file on the next run.\n\n");
	return true;
}
