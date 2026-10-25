#!/bin/bash
# Builds a release Paper.app (the WebKit prototype) into prototype/dist/,
# with its app icon, ad-hoc signed for running on this Mac.
# Usage: prototype/scripts/build-app.sh [version]
set -euo pipefail
cd "$(dirname "$0")/.."

version="${1:-0.1.0}"
build_number="$(date +%Y%m%d%H%M)"
app="dist/Paper.app"

swift build -c release

rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp .build/release/Paper "$app/Contents/MacOS/Paper"

# App icon.
iconset="$(mktemp -d)/Paper.iconset"
swift scripts/make-icon.swift "$iconset"
iconutil -c icns "$iconset" -o "$app/Contents/Resources/Paper.icns"

cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Paper</string>
  <key>CFBundleDisplayName</key><string>Paper</string>
  <key>CFBundleIdentifier</key><string>app.paper.prototype</string>
  <key>CFBundleExecutable</key><string>Paper</string>
  <key>CFBundleIconFile</key><string>Paper</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>${version}</string>
  <key>CFBundleVersion</key><string>${build_number}</string>
  <key>LSMinimumSystemVersion</key><string>26.0</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.productivity</string>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

codesign --force --deep --sign - "$app"
echo "Built $app (v${version}, build ${build_number})"
