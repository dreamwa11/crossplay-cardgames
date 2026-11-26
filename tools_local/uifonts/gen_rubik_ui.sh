#!/bin/bash
# Regenerate the system UI cuts (rubik_{10,12}_{regular,bold}.h) that replace
# upstream's Ubuntu ones in this fork.
#
#   ./tools_local/uifonts/gen_rubik_ui.sh
#
# Rubik (SIL OFL, Google Fonts) at weight 400 for regular and 700 for bold:
# 500, the games' weight, reads as bold at 10-12px and flattens the hierarchy. The static instances are committed in
# lib/EpdFont/builtinFonts/source/Rubik/; this only converts. The fallback stack
# is upstream's: Noto Sans for what Rubik lacks, then Hebrew and Arabic.
set -euo pipefail
cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../../lib/EpdFont/scripts"
ARABIC=(0x060C,0x060C 0x061B,0x061B 0x061F,0x061F 0x0621,0x0621 0x0640,0x0640 0x0654,0x0654 0x0660,0x0669
  0x06BA,0x06BA 0x06D4,0x06D4 0x06D5,0x06D5 0x06F0,0x06F9 0xFB56,0xFB59 0xFB66,0xFB69 0xFB7A,0xFB7D 0xFB88,0xFB95
  0xFB9E,0xFB9F 0xFBA6,0xFBB1 0xFBFC,0xFBFF 0xFE80,0xFEFC)
ARGS=(--additional-intervals 0x05D0,0x05EA)
for a in "${ARABIC[@]}"; do ARGS+=(--additional-intervals "$a"); done
for size in 10 12; do
  for style in Regular Bold; do
    cut=$style
    name="rubik_${size}_$(echo "$style" | tr '[:upper:]' '[:lower:]')"
    uv run --quiet --with freetype-py --with fonttools --with zopfli python fontconvert.py "$name" "$size" \
      ../builtinFonts/source/Rubik/Rubik-$cut.ttf \
      ../builtinFonts/source/NotoSans/NotoSans-$style.ttf \
      ../builtinFonts/source/NotoSansHebrew/NotoSansHebrew-$style.ttf \
      ../builtinFonts/source/NotoSansArabic/NotoSansArabic-$style.ttf \
      "${ARGS[@]}" > "../builtinFonts/$name.h"
    echo "wrote $name.h"
  done
done
