# veriumMiner - multi-stage CMake build
#
# Build:  docker build -t veriumminer .
# Run:    docker run --rm veriumminer -o stratum+tcp://pool:port -u WALLET.WORKER -p x -t 1
#
# Requires .dockerignore (excludes host build/) so CMake does not ingest a stale
# CMakeCache.txt from a local compile. See docs/RASPBERRY_PI.md for Pi / Compose.
#
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git \
        libcurl4-openssl-dev libjansson-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j "$(nproc)" \
    && ctest --test-dir build --output-on-failure

# ---- runtime image ----
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        libcurl4 libjansson4 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/cpuminer /usr/local/bin/cpuminer

ENTRYPOINT ["cpuminer"]
