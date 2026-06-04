#!/bin/bash
# Re-run the setup wizard (wallet, pool, thread count, etc.).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
xattr -dr com.apple.quarantine "$DIR" 2>/dev/null || true
exec "$DIR/Verium Miner.app/Contents/MacOS/cpuminer" --setup
