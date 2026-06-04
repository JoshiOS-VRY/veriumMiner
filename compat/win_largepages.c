/*
 * Enable Windows large-page privilege for scrypt scratchpads (MEM_LARGE_PAGES).
 */
#include <cpuminer-config.h>

#ifdef WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "miner.h"
#include "compat/win_largepages.h"

static bool enable_lock_pages_privilege(void)
{
	HANDLE tok = NULL;
	TOKEN_PRIVILEGES tp;
	LUID luid;
	BOOL ok;

	if (!OpenProcessToken(GetCurrentProcess(),
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
		return false;

	if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid)) {
		CloseHandle(tok);
		return false;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = luid;

	ok = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), NULL, NULL);
	CloseHandle(tok);

	if (!ok || GetLastError() == ERROR_NOT_ALL_ASSIGNED)
		return false;
	return true;
}

void verium_win_largepages_init(void)
{
	SIZE_T min_large = GetLargePageMinimum();

	if (enable_lock_pages_privilege()) {
		if (min_large > 0)
			applog(LOG_NOTICE,
				"Large pages: privilege enabled (minimum allocation %zu KB)",
				(size_t)(min_large / 1024));
		else
			applog(LOG_NOTICE, "Large pages: privilege enabled");
	} else {
		applog(LOG_NOTICE,
			"Large pages: unavailable (assign \"Lock pages in memory\" to this user — using 4 KB pages)");
	}
}

#else

void verium_win_largepages_init(void)
{
}

#endif /* WIN32 */
