#!/bin/bash
# Builds "BOBA-SafeSide-<version>-Mac-Installer.pkg" from the compiled, signed bundles.
# Usage: installer/mac/build_pkg.sh <dir with .vst3 and .component> <version> <output.pkg>
set -euo pipefail

SRC="$1"; VERSION="$2"; OUT="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
NAME="SAFE-SIDE"
WORK="$(mktemp -d)"

mkdir -p "$WORK/vst3" "$WORK/au" "$WORK/res"
cp -R "$SRC/$NAME.vst3"      "$WORK/vst3/"
cp -R "$SRC/$NAME.component" "$WORK/au/"
chmod +x "$HERE/scripts/postinstall"

# Component plists: never relocate the bundle to wherever an older copy was found.
for kind in vst3 au; do
    pkgbuild --analyze --root "$WORK/$kind" "$WORK/$kind.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$kind.plist"
done

pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" \
         --identifier com.boba.safeside.vst3 --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" --scripts "$HERE/scripts" \
         "$WORK/vst3.pkg"

pkgbuild --root "$WORK/au" --component-plist "$WORK/au.plist" \
         --identifier com.boba.safeside.au --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" --scripts "$HERE/scripts" \
         "$WORK/au.pkg"

for f in welcome.html conclusion.html; do
    sed "s/@VERSION@/$VERSION/g" "$HERE/resources/$f" > "$WORK/res/$f"
done
cp "$HERE/resources/background.png" "$WORK/res/background.png"
sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"

productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" \
             --package-path "$WORK" "$OUT"
rm -rf "$WORK"
echo "built $OUT"
