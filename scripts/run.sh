#!/bin/bash
# Launches the built Paper.app with a throwaway dev profile.
source "$(dirname "$0")/env.sh"

exec "$OUT_DIR/Paper.app/Contents/MacOS/Paper" \
  --user-data-dir="${PAPER_PROFILE:-/tmp/paper-dev}" \
  --no-first-run --no-default-browser-check "$@"
