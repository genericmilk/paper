#!/bin/bash
# Shared paths for the Paper scripts. Override CHROMIUM_DIR to put the
# ~110 GB checkout on another disk, e.g. CHROMIUM_DIR=/Volumes/SSD/chromium.
set -euo pipefail

PAPER_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CHROMIUM_DIR="${CHROMIUM_DIR:-$HOME/chromium}"
CHROMIUM_SRC="$CHROMIUM_DIR/src"
DEPOT_TOOLS="${DEPOT_TOOLS:-$HOME/depot_tools}"
CHROMIUM_VERSION="$(cat "$PAPER_ROOT/CHROMIUM_VERSION")"
OUT_DIR="$CHROMIUM_SRC/out/Paper"

export PATH="$DEPOT_TOOLS:$PATH"
