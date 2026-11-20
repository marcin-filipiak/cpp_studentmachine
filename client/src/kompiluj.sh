#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

make

mkdir -p build ../server/src/files
cp build/studentmachine ../server/src/files/studentmachine
chmod +x build/studentmachine ../server/src/files/studentmachine

echo "Build complete: build/studentmachine"
echo "Copied to:      ../server/src/files/studentmachine"

if [[ "${1:-}" == "--run" ]]; then
    shift
    exec build/studentmachine "$@"
fi
