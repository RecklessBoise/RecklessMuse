#!/usr/bin/env bash
# Installs the built RecklessMuse plug-ins for the current user.
# Usage: scripts/install.sh [build-dir]   (default: build)
set -euo pipefail

BUILD_DIR="${1:-build}"
ART="$BUILD_DIR/RecklessMuse_artefacts/Release"
[ -d "$ART" ] || ART="$BUILD_DIR/RecklessMuse_artefacts"

VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DEST" "$AU_DEST"

rm -rf "$VST3_DEST/RecklessMuse.vst3" "$AU_DEST/RecklessMuse.component"
cp -R "$ART/VST3/RecklessMuse.vst3" "$VST3_DEST/"
cp -R "$ART/AU/RecklessMuse.component" "$AU_DEST/"

# Downloaded builds are quarantined by macOS; local builds just get re-signed ad hoc.
xattr -dr com.apple.quarantine "$VST3_DEST/RecklessMuse.vst3" "$AU_DEST/RecklessMuse.component" 2>/dev/null || true
codesign --force --deep -s - "$VST3_DEST/RecklessMuse.vst3" "$AU_DEST/RecklessMuse.component" >/dev/null 2>&1 || true

# Make the Audio Unit visible without a reboot
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed:"
echo "  $VST3_DEST/RecklessMuse.vst3"
echo "  $AU_DEST/RecklessMuse.component"
