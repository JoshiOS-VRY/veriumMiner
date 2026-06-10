# Contributing to Verium Miner

Thanks for helping improve the official Vericonomy CPU miner. This guide covers
the build/test workflow and the conventions we follow.

## Golden rule: hashing is consensus-bound

The scrypt² ("VeriHash") output **must never change**. Any change that touches
the hashing path (`src/algo/`, `src/asm/`, scratchpad sizing, endianness) must:

1. keep `ctest` (the `hash_vectors` test) passing against the locked golden
   digest in [`tests/golden.h`](tests/golden.h), and
2. produce the same `cpuminer --cputest` output:
   `ef9dc4d2 1dc07315 4996e0fb debc4c01 4d6defa4 a3791ab1 4bc5def6 8b0d76dc`.

New SIMD/NEON cores must be validated against the portable C core on golden
vectors **before** being enabled by default. The runtime gate is
`cpuminer --selftest`, which hashes the locked vector with the *active* compiled
core and exits non-zero on any mismatch; CI and the release pipeline run it on
every build. On aarch64, `ctest` also runs `scrypt_3way_equiv` to verify each
3-way lane matches the 1-way core.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Platform-specific dependencies are listed in the [README](README.md#dependencies).

## Testing before you open a PR

```sh
# Hash regression (allocates ~3 GB; needs free RAM)
ctest --test-dir build --output-on-failure

# Warnings-as-errors must stay clean (this is a CI gate)
cmake -B build-werror -DCMAKE_BUILD_TYPE=Release -DWERROR=ON
cmake --build build-werror -j

# Optional: sanitizers (catches leaks/UB)
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_LTO=OFF \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j
./build-asan/cpuminer --cputest
```

CI runs the same gates (build matrix, `-Werror`, ASan/UBSan, cppcheck) on every
push and pull request.

## Coding conventions

- C11 / C++11. Match the surrounding style (tabs for indentation in C sources).
- Prefer bounded string APIs (`snprintf`) over `sprintf`/`strcat`.
- JSON-escape any externally influenced string before emitting JSON.
- Keep comments focused on intent/trade-offs, not narration.
- Don't introduce new compiler warnings; `-DWERROR=ON` must stay clean.

## Commit / PR guidance

- Keep changes focused; describe the "why" in the PR.
- Note any impact on hashing, the wire protocol, or the config format.
- Update the README/docs when you change user-facing behavior or options.

## Releases

Releases are tag-driven: pushing a `vX.Y.Z` tag runs
[`.github/workflows/release.yml`](.github/workflows/release.yml), which builds,
tests, packages, checksums, and publishes binaries for all platforms. Bump the
version in [`CMakeLists.txt`](CMakeLists.txt) (`project(... VERSION ...)`) before
tagging.

**macOS Intel:** x86_64 `.S` assembly requires `contrib/nomacro.pl` (run
automatically by CMake on `APPLE` + x86_64) because Clang does not expand GAS
`.macro` blocks the same way as Linux GAS.
