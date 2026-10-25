#!/bin/bash
# Builds the WebKit prototype and launches it as a real .app bundle (Dock
# icon, keyboard focus, WebKit's networking/GPU processes all behave better
# bundled). Usage: prototype/run.sh [--release]
set -euo pipefail
cd "$(dirname "$0")"

config=debug
[[ "${1:-}" == "--release" ]] && config=release
swift build -c "$config"

app=".build/Paper.app"
rm -rf "$app"
mkdir -p "$app/Contents/MacOS"
cp ".build/$config/Paper" "$app/Contents/MacOS/Paper"
cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Paper</string>
  <key>CFBundleDisplayName</key><string>Paper</string>
  <key>CFBundleIdentifier</key><string>app.paper.prototype</string>
  <key>CFBundleExecutable</key><string>Paper</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>0.1</string>
  <key>LSMinimumSystemVersion</key><string>26.0</string>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST
codesign --force --sign - "$app" >/dev/null 2>&1 || true

# Wait for any running copy to quit, or `open` just re-activates it.
pkill -x Paper 2>/dev/null || true
while pgrep -x Paper >/dev/null; do sleep 0.1; done
sleep 0.5  # Launch Services needs a beat after a quit, or open fails with -600.
open "$app" || { sleep 1; open "$app"; }
echo "Launched $app"
