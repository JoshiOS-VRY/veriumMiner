#ifndef VERIUM_ONBOARD_H
#define VERIUM_ONBOARD_H

#include <stdbool.h>
#include <stddef.h>

/* Interactive setup wizard; writes ~/.cpuminer/cpuminer-conf.json (or %APPDATA% on Windows). */
bool onboard_interactive(char *out_config_path, size_t pathsz);

#endif /* VERIUM_ONBOARD_H */
