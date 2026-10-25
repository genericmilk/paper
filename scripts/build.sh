#!/bin/bash
# Builds Paper.app. 16 GB of RAM can't feed more than ~8 compile jobs.
source "$(dirname "$0")/env.sh"
cd "$CHROMIUM_SRC"

mkdir -p "$OUT_DIR"
cp "$PAPER_ROOT/args.gn" "$OUT_DIR/args.gn"
if [[ "${1:-}" == "--release" ]]; then
  printf 'is_component_build = false\nis_official_build = true\n' >> "$OUT_DIR/args.gn"
fi

gn gen "$OUT_DIR"
autoninja -j "${JOBS:-8}" -C "$OUT_DIR" chrome
