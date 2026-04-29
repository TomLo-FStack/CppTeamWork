#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
exe="$build_dir/expense_app"
if [[ ! -x "$exe" ]]; then
    echo "missing executable: $exe" >&2
    exit 1
fi

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/cppteamwork-smoke.XXXXXX")"
data="$tmp_dir/ledger.etx"
settlement="$tmp_dir/settlement.txt"

"$exe" --data "$data" add 2026-04-01 12.50 food "campus lunch"
"$exe" --data "$data" add 2026-04-02 8.25 transit "metro card"
"$exe" --data "$data" add 2026-04-15 99.99 books "systems programming text"

summary="$("$exe" --data "$data" summary 2026-04)"
printf '%s\n' "$summary"
if [[ ! "$summary" =~ total:[[:space:]]+120\.74 ]]; then
    echo "summary total mismatch" >&2
    exit 1
fi
if [[ ! "$summary" =~ Food[[:space:]]+12\.50 ]]; then
    echo "category canonicalization mismatch" >&2
    exit 1
fi

list_output="$("$exe" --data "$data" list month 2026-04)"
if [[ ! "$list_output" =~ 3[[:space:]]record ]]; then
    printf '%s\n' "$list_output"
    echo "list count mismatch" >&2
    exit 1
fi

"$exe" --data "$data" --settlement "$settlement" settle 2026-04
test -f "$settlement"
echo "smoke ok"
