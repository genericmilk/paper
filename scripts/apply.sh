#!/bin/bash
# Copies the Paper overlay into the Chromium tree and applies patches/ in
# order. Safe to re-run: the tree is reset to the pinned tag first.
source "$(dirname "$0")/env.sh"
cd "$CHROMIUM_SRC"

if [[ "${1:-}" != "--no-reset" ]]; then
  git reset -q --hard "tags/$CHROMIUM_VERSION"
  git clean -qfd -- chrome/browser/ui/views/paper chrome/browser/ui/webui/paper \
    chrome/browser/resources/paper
fi

rsync -a "$PAPER_ROOT/overlay/" "$CHROMIUM_SRC/"

for patch in "$PAPER_ROOT"/patches/*.patch; do
  echo "Applying $(basename "$patch")"
  git apply --3way "$patch"
done

echo "Paper applied on Chromium $CHROMIUM_VERSION."
