#!/usr/bin/env bash
# banner.sh - rebuild docs/images/banner.svg: VLMF's main screen from the Linux
# stub with made-up licenses (tools/banner_frame.c), drawn by tools/banner.py.
set -euo pipefail
top=$(cd "$(dirname "$0")/.." && pwd)
cd "$top"
mkdir -p build docs/images
cc -std=c99 -pedantic -Wall -Wextra -Werror -Isrc/core -Isrc/app -Isrc/plat -Isrc/plat/stub \
   -o build/banner_frame tools/banner_frame.c src/core/vl_util.c src/core/vl_edit.c \
   src/core/vl_screen.c src/core/vl_ansi.c src/core/vl_lic.c src/app/vl_app.c src/app/vl_draw.c \
   src/plat/stub/vl_plat_stub.c
./build/banner_frame > build/banner.ans
python3 tools/banner.py build/banner.ans docs/images/banner.svg
