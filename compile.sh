#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
gcc -shared -fPIC "$SCRIPT_DIR/hook.c" -o "$SCRIPT_DIR/libdrm_hook.so" -ldl
echo "libdrm_hook.so compiled successfully!"
