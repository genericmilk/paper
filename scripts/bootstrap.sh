#!/bin/bash
# One-time setup: depot_tools + a history-less Chromium checkout at the pinned
# tag. Needs ~110-130 GB free once built.
source "$(dirname "$0")/env.sh"

free_gb=$(df -g "$(dirname "$CHROMIUM_DIR")" | awk 'NR==2 {print $4}')
if (( free_gb < 110 )); then
  echo "Only ${free_gb} GB free under $(dirname "$CHROMIUM_DIR")."
  echo "Chromium needs ~110-130 GB. Free space or set CHROMIUM_DIR to another disk."
  exit 1
fi

if [[ ! -d "$DEPOT_TOOLS" ]]; then
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS"
fi

mkdir -p "$CHROMIUM_DIR"
cd "$CHROMIUM_DIR"
if [[ ! -d src ]]; then
  fetch --nohooks --no-history chromium
fi

cd src
git fetch --depth=1 origin "refs/tags/$CHROMIUM_VERSION:refs/tags/$CHROMIUM_VERSION"
git checkout -B paper "tags/$CHROMIUM_VERSION"
gclient sync -D --with_branch_heads --with_tags --no-history
gclient runhooks

echo "Chromium $CHROMIUM_VERSION ready at $CHROMIUM_SRC. Next: scripts/apply.sh"
