#!/bin/sh
# Downloads the prebuilt Qt that favdir builds against into .qt/ at the repository root.
# Requires uv (https://docs.astral.sh/uv/).

set -eu

QT_VERSION=6.8.3
QT_ARCH=clang_64
QT_ARCH_DIR=macos

ROOT=$(cd "$(dirname "$0")/.." && pwd)
QT_DIR="$ROOT/.qt/$QT_VERSION/$QT_ARCH_DIR"

if [ -d "$QT_DIR/lib/cmake/Qt6" ]; then
    echo "Qt $QT_VERSION already installed at $QT_DIR"
    exit 0
fi

if ! command -v uvx >/dev/null 2>&1; then
    echo "uv is required. Install it from https://docs.astral.sh/uv/ and run this script again." >&2
    exit 1
fi

uvx --from aqtinstall aqt install-qt mac desktop "$QT_VERSION" "$QT_ARCH" -O "$ROOT/.qt" --archives qtbase

echo "Qt $QT_VERSION installed at $QT_DIR"
