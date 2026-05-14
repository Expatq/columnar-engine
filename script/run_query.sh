#!/usr/bin/env bash
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -lt 4 ]]; then
  echo "Usage: script/run_query.sh <query_num 0..42> <columnar> <output_csv> <log_file>" >&2
  exit 2
fi

QUERY_NUM="$1"
COLUMNAR="$2"
OUTPUT="$3"
LOG="$4"
BIN="${ROOT_DIR}/build/tools/run_query"

mkdir -p "$(dirname "${OUTPUT}")" "$(dirname "${LOG}")"
"${BIN}" "${COLUMNAR}" "${QUERY_NUM}" > "${OUTPUT}" 2> "${LOG}"
