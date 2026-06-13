#ifndef VERIUM_ONBOARD_H
#define VERIUM_ONBOARD_H

#include <stdbool.h>
#include <stddef.h>

#define VERIUM_DEFAULT_POOL_URL "stratum+tcp://mine.vericonomy.com:3333"
#define VERIUM_DEFAULT_SOLO_URL "http://127.0.0.1:33987"

/* Interactive setup wizard; writes ~/.cpuminer/cpuminer-conf.json (or %APPDATA% on Windows). */
bool onboard_interactive(char *out_config_path, size_t pathsz);

/* Trim whitespace and stray trailing brackets/quotes from pool/solo URLs. */
void verium_sanitize_mining_url(char *url);

#endif /* VERIUM_ONBOARD_H */
