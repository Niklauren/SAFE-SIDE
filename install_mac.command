#!/bin/bash
# Установка готовой сборки (из GitHub Actions). Двойной клик или: bash install_mac.command
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"SAFE-SIDE.vst3" ~/Library/Audio/Plug-Ins/Components/"SAFE-SIDE.component"
cp -R "SAFE-SIDE.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "SAFE-SIDE.component" ~/Library/Audio/Plug-Ins/Components/
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"SAFE-SIDE.vst3" ~/Library/Audio/Plug-Ins/Components/"SAFE-SIDE.component"
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/"SAFE-SIDE.vst3"
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/"SAFE-SIDE.component"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
echo "Установлено. Перезапусти Ableton и сделай Rescan плагинов. SAFE-SIDE будет у производителя BOBA."
