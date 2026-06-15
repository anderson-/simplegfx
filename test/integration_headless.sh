#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BIN=${1:-build/buffer-term}
OUT=build/integration-stdout.txt
SHOT=build/integration-term.ppm

rm -f "$OUT" "$SHOT"

{
  printf 'clear\n'
  printf 'echo alpha\n'
  printf '\033[A\n'
  printf 'echo ac\033[Db\n'
  printf 'echo typx\bO\n'
  printf 'echo color-\\x1b[31mred\\x1b[m\n'
  printf 'echo move\\x1b[6;10HANCHOR\n'
  printf 'termshot %s\n' "$SHOT"
} | "$BIN" > "$OUT"

need() {
  if ! grep -F "$1" "$OUT" >/dev/null; then
    printf 'missing expected stdout fragment: %s\n' "$1" >&2
    printf '--- stdout ---\n' >&2
    sed -n '1,160p' "$OUT" >&2
    exit 1
  fi
}

need 'alpha'
need 'abc'
need 'typO'
need 'ANCHOR'
need "termshot: wrote $SHOT"

red=$(printf '\033[31mred\033[m')
need "$red"

alpha_count=$(awk 'BEGIN{n=0} {s=$0; while (index(s, "alpha")) { n++; s=substr(s, index(s, "alpha")+5) }} END{print n}' "$OUT")
if [ "$alpha_count" -lt 3 ]; then
  printf 'expected history replay to produce alpha at least 3 times, got %s\n' "$alpha_count" >&2
  exit 1
fi

if [ ! -s "$SHOT" ]; then
  printf 'missing termshot output: %s\n' "$SHOT" >&2
  exit 1
fi

header=$(dd if="$SHOT" bs=2 count=1 2>/dev/null)
if [ "$header" != "P6" ]; then
  printf 'termshot header mismatch: %s\n' "$header" >&2
  exit 1
fi

printf 'integration headless: ok\n'
