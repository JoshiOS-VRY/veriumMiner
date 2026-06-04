#!/usr/bin/env bash
#
# Assemble a release archive for one platform.
#
#   package.sh <platform> <path-to-binary>
#
# Produces dist/veriumminer-<version>-<platform>.{tar.gz|zip} containing the
# binary, license, example config, docs, and the headless service helpers.
# Version is derived from the git tag (or `git describe`).
set -euo pipefail

PLATFORM="${1:?usage: package.sh <platform> <binary>}"
BINARY="${2:?usage: package.sh <platform> <binary>}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

# Version: prefer the pushed tag, fall back to git describe.
VERSION="${GITHUB_REF_NAME:-}"
case "$VERSION" in
  v*) VERSION="${VERSION#v}" ;;
  "") VERSION="$(git describe --tags --always 2>/dev/null | sed 's/^v//')" ;;
  *)  : ;;
esac
[ -n "$VERSION" ] || VERSION="0.0.0"

NAME="veriumminer-${VERSION}-${PLATFORM}"
STAGE="dist/${NAME}"
rm -rf "$STAGE"
mkdir -p "$STAGE/contrib"

cp "$BINARY" "$STAGE/"
cp README.md "$STAGE/" 2>/dev/null || true
cp COPYING "$STAGE/LICENSE.txt" 2>/dev/null || cp LICENSE "$STAGE/LICENSE.txt" 2>/dev/null || true
# Example config only — must not be named cpuminer-conf.json or the miner will
# auto-load placeholders and skip the first-run wizard.
if [ -f cpuminer-conf.json ]; then
  cp cpuminer-conf.json "$STAGE/cpuminer-conf.example.json"
fi
cat > "$STAGE/START.txt" <<'EOF'
Verium Miner — first run
========================

1. Open PowerShell or cmd in this folder.
2. Run the setup wizard (creates your wallet/pool config):

   cpuminer.exe --setup

3. Start mining:

   cpuminer.exe

Optional: copy cpuminer-conf.example.json and edit by hand instead of --setup.
EOF

# Headless / service helpers and docs.
for d in systemd launchd windows; do
  [ -d "contrib/$d" ] && cp -r "contrib/$d" "$STAGE/contrib/"
done
[ -d docs ] && cp -r docs "$STAGE/docs"

mkdir -p dist
case "$PLATFORM" in
  windows*)
    ( cd dist && zip -r -q "${NAME}.zip" "$NAME" )
    echo "Created dist/${NAME}.zip"
    ;;
  *)
    tar -C dist -czf "dist/${NAME}.tar.gz" "$NAME"
    echo "Created dist/${NAME}.tar.gz"
    ;;
esac

rm -rf "$STAGE"
