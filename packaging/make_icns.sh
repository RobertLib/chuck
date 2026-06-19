#!/bin/bash
#
# Build the .icns from packaging/draw_icon.py. The icon is drawn rather than
# stored for the same reason the levels' art is: the repository holds no binary
# assets, and an icon checked in as a blob is one more thing that can drift
# from the palette everything else is drawn in.
set -euo pipefail

out=${1:?usage: make_icns.sh <out.icns>}
here=$(cd "$(dirname "$0")" && pwd)

work=$(mktemp -d "${TMPDIR:-/tmp}/chuck-icon.XXXXXX")
trap 'rm -rf "$work"' EXIT

python3 "$here/draw_icon.py" "$work/master.png"

set_dir="$work/icon.iconset"
mkdir -p "$set_dir"
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$work/master.png" \
        --out "$set_dir/icon_${size}x${size}.png" >/dev/null
    sips -z "$((size * 2))" "$((size * 2))" "$work/master.png" \
        --out "$set_dir/icon_${size}x${size}@2x.png" >/dev/null
done

mkdir -p "$(dirname "$out")"
iconutil --convert icns "$set_dir" --output "$out"
