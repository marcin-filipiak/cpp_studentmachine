#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
binary="$root/build/studentmachine"
temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT

export HOME="$temporary"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

"$binary" --version | grep -q "StudentMachine" || fail "version output"
"$binary" --help | grep -q "Usage" || fail "help output"
"$binary" --no-color --help | grep -q $'\033' && fail "colors were not disabled"
"$binary" status >/dev/null || fail "status command"

if ! "$binary" doctor --offline >/dev/null; then
    code=$?
    [[ "$code" -eq 3 ]] || fail "doctor returned $code"
fi

set +e
"$binary" exercise >/dev/null 2>&1
code=$?
set -e
[[ "$code" -eq 2 ]] || fail "exercise without a name returned $code"

set +e
"$binary" savework >/dev/null 2>&1
code=$?
set -e
[[ "$code" -eq 1 ]] || fail "savework without config returned $code"

set +e
"$binary" systemup --yes >/dev/null 2>&1
code=$?
set -e
[[ "$code" -eq 2 ]] || fail "systemup --yes without mode returned $code"

echo "Smoke tests passed."
