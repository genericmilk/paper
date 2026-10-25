#!/bin/bash
# Regenerates patches/ from edits made directly in the Chromium tree. New files
# under the overlay directories are copied back to overlay/ instead.
#
# Usage: scripts/export-patches.sh <patch-name> <file>...
#   e.g. scripts/export-patches.sh 0004-toolbar chrome/browser/ui/views/toolbar/toolbar_view.cc
source "$(dirname "$0")/env.sh"
cd "$CHROMIUM_SRC"

name="$1"; shift
git diff "tags/$CHROMIUM_VERSION" -- "$@" > "$PAPER_ROOT/patches/$name.patch"
echo "Wrote patches/$name.patch"

for dir in chrome/browser/ui/views/paper chrome/browser/ui/webui/paper \
           chrome/browser/resources/paper; do
  rsync -a --delete "$CHROMIUM_SRC/$dir/" "$PAPER_ROOT/overlay/$dir/"
done
