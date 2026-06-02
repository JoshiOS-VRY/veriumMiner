/*
 * Golden scrypt^2 vectors for regression testing.
 *
 * The miner's hash is consensus-bound: it must never change. These values
 * lock the output of scrypthash() for fixed inputs.
 *
 * RECORDING: the first time the test is built on real hardware (e.g. in CI),
 * run `test_hash` once; it prints the computed digests in "RECORD:" lines.
 * Paste the hex for the empty-buffer vector below to lock it. Until then the
 * test still enforces determinism and non-null output.
 *
 * Vector: scrypthash() of an 80-byte all-zero buffer with N = 1048576,
 * which is exactly what `cpuminer --cputest` prints as "scrypt:1048576".
 */
#ifndef VERIUM_GOLDEN_H
#define VERIUM_GOLDEN_H

/* 64 lowercase hex chars (32 bytes), or "" if not yet recorded. */
#define VERIUM_GOLDEN_EMPTY_N1048576 "ef9dc4d21dc073154996e0fbdebc4c014d6defa4a3791ab14bc5def68b0d76dc"

#endif /* VERIUM_GOLDEN_H */
