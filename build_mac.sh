#!/bin/bash
# Сборка SAFE-SIDE на Mac одной командой:
#   cd путь/к/SAFE-SIDE && bash build_mac.sh
# Плагины сами попадут в ~/Library/Audio/Plug-Ins/VST3 и ~/Library/Audio/Plug-Ins/Components
set -e
cd "$(dirname "$0")"

echo "==> Проверяю инструменты"
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Нужны Xcode Command Line Tools. Сейчас откроется установщик —"
    echo "после установки запусти этот скрипт ещё раз."
    xcode-select --install
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
        echo "==> Ставлю cmake через Homebrew"
        brew install cmake
    else
        echo "==> Ставлю cmake через pip"
        python3 -m pip install --user --quiet cmake
        export PATH="$(python3 -m site --user-base)/bin:$PATH"
    fi
fi

echo "==> Настраиваю проект (первый раз скачается JUCE, ~1-2 мин)"
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release

echo "==> Собираю (несколько минут)"
cmake --build build-mac --config Release -j"$(sysctl -n hw.ncpu)"

VST3="$HOME/Library/Audio/Plug-Ins/VST3/SAFE-SIDE.vst3"
AU="$HOME/Library/Audio/Plug-Ins/Components/SAFE-SIDE.component"

echo "==> Подписываю (ad-hoc), чтобы macOS разрешила загрузку"
for p in "$VST3" "$AU"; do
    if [ -d "$p" ]; then
        xattr -cr "$p" || true
        codesign --force --deep --sign - "$p"
    fi
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Готово!"
echo "  VST3: $VST3"
echo "  AU:   $AU"
echo "Открой Ableton -> Settings -> Plug-ins -> включи 'Use VST3 Plug-in System Folders' -> Rescan."
