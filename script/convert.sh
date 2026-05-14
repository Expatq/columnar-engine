#!/usr/bin/env bash
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -lt 2 ]]; then
  echo "Usage: script/convert.sh <input_csv> <output_columnar>" >&2
  exit 2
fi

INPUT_CSV="$1"
COLUMNAR="$2"
SCHEMA="${ROOT_DIR}/script/hits.schema"
BIN="${ROOT_DIR}/build/tools/csv2iyx"

mkdir -p "$(dirname "${COLUMNAR}")"
"${BIN}" "${SCHEMA}" "${INPUT_CSV}" "${COLUMNAR}"
