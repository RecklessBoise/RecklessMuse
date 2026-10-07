#!/usr/bin/env bash
# Installs RecklessMuse from the downloaded release folder.
# Run from Terminal:  cd ~/Downloads/RecklessMuse && ./install.sh
set -euo pipefail
cd "$(dirname "$0")"

VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DEST" "$AU_DEST"

rm -rf "$VST3_DEST/RecklessMuse.vst3" "$AU_DEST/RecklessMuse.component"
cp -R RecklessMuse.vst3 "$VST3_DEST/"
cp -R RecklessMuse.component "$AU_DEST/"

# The builds are not notarised: remove the download quarantine and re-sign ad hoc.
xattr -dr com.apple.quarantine "$VST3_DEST/RecklessMuse.vst3" "$AU_DEST/RecklessMuse.component" RecklessMuse.app 2>/dev/null || true
codesign --force --deep -s - "$VST3_DEST/RecklessMuse.vst3" "$AU_DEST/RecklessMuse.component" >/dev/null 2>&1 || true

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "RecklessMuse installed (VST3 + AU). Rescan plug-ins in your DAW."
