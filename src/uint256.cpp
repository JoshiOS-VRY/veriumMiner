#include "uint256.h"

#ifdef __cplusplus
extern "C"{
#endif

#include "miner.h"

// compute the diff ratio between a found hash and the target
double hash_target_ratio(uint32_t* hash, uint32_t* target)
{
	uint256 h, t;
	double dhash;

	memcpy(&t, (void*) target, 32);
	memcpy(&h, (void*) hash, 32);

	dhash = h.getdouble();
	if (dhash > 0.)
		return t.getdouble() / dhash;
	else
		return dhash;
}

// store actual share difficulty (for pct-to-network and block detection)
void work_set_target_ratio(struct work* work, uint32_t* hash)
{
	double ratio;

	if (!work)
		return;
	ratio = hash_target_ratio(hash, work->target);
	if (ratio <= 0.)
		return;
	work->shareratio = ratio;
	work->sharediff = work->targetdiff * work->shareratio;
	if (opt_showdiff && opt_debug)
		applog(LOG_DEBUG, "share diff %.8g (%.1fx pool target)", work->sharediff, work->shareratio);
}

#ifdef __cplusplus
}
#endif
