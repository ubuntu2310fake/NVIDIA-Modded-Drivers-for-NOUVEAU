#!/bin/bash
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

VK_ICD_FILENAMES="$SCRIPT_DIR/mesa-23.3/build/src/nouveau/vulkan/nouveau_devenv_icd.x86_64.json" \
LD_PRELOAD="$SCRIPT_DIR/libdrm_hook.so" \
"$@"
