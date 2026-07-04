#!/bin/bash
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

__GLX_VENDOR_LIBRARY_NAME=mesa \
MESA_LOADER_DRIVER_OVERRIDE=nouveau \
LIBGL_DRIVERS_PATH="$SCRIPT_DIR/mesa-23.3/build/src/gallium/targets/dri" \
LD_PRELOAD="$SCRIPT_DIR/libdrm_hook.so" \
"$@"
