#ifndef VERIUM_WIN_LARGE_PAGES_H
#define VERIUM_WIN_LARGE_PAGES_H

/* Try to enable SeLockMemoryPrivilege for this process (Windows large pages).
 * Safe no-op on non-Windows. */
void verium_win_largepages_init(void);

#endif /* VERIUM_WIN_LARGE_PAGES_H */
