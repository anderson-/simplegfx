#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BIN=${1:-build/buffer-term}
OUT=build/headless-stdio.out

rm -f "$OUT"

{
  printf 'echo alpha\n'
  printf 'echo ac\033[Db\n'
  printf 'echo typx\bO\n'
  printf '\033[A\n'
  printf '\004'
} | "$BIN" > "$OUT"

need() {
  if ! grep -F "$1" "$OUT" >/dev/null; then
    echo "missing expected stdout fragment: $1" >&2
    sed -n '1,120p' "$OUT" >&2
    exit 1
  fi
}

need 'simplegfx'
need 'alpha'
need 'abc'
need 'typO'

typ_count=$(awk 'BEGIN{n=0} {s=$0; while (index(s, "typO")) { n++; s=substr(s, index(s, "typO")+4) }} END{print n}' "$OUT")
if [ "$typ_count" -lt 2 ]; then
  echo "expected history replay to execute typO, got count $typ_count" >&2
  sed -n '1,120p' "$OUT" >&2
  exit 1
fi

echo "headless stdio: ok"
