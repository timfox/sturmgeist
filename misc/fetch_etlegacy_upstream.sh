#!/usr/bin/env bash
# Fetch upstream ET: Legacy without merging (safe for agents and quick checks).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
URL="${ETL_UPSTREAM:-https://github.com/etlegacy/etlegacy.git}"
BRANCH="${ETL_UPSTREAM_BRANCH:-master}"
git fetch "$URL" "$BRANCH"
printf 'Fetched %s from %s as FETCH_HEAD.\n' "$BRANCH" "$URL"
printf 'Inspect: git log --oneline HEAD..FETCH_HEAD\n'
printf 'Merge:   git merge FETCH_HEAD\n'
