#!/bin/bash
# Double-click this file to start mining (clears macOS download quarantine first).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
xattr -dr com.apple.quarantine "$DIR" 2>/dev/null || true
exec "$DIR/Verium Miner.app/Contents/MacOS/cpuminer"
