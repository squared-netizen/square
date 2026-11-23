#!/usr/bin/env bash
# tools/vendor_sqcart.sh SQCART: copy sqcart's 3.0 library (sq.cart and
# sq.cart.write, v3/) and what it builds with into third_party/sqcart/,
# and record where from.
#
#   tools/vendor_sqcart.sh ../sqcart
#
# square reads every cartridge through sqcart, never itself (docs/spec.md
# §1): the workspace's own manifest, and the templates, kits and packages
# it lists and shows. sq.data and yyjson come with it.
# The copy is committed, so a checkout builds offline and without sqcart
# beside it. Run this again when sqcart changes, then build, test and
# commit third_party/sqcart/ with its PINNED.
set -euo pipefail
SRC=${1:?"a sqcart checkout"}
[ -f "$SRC/v3/src/sq.cart.cppm" ] || { echo "no v3/src/sq.cart.cppm under $SRC" >&2; exit 1; }
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
OUT=$ROOT/third_party/sqcart
rm -rf "$OUT"
mkdir -p "$OUT/v3/src" "$OUT/third_party"
cp "$SRC"/v3/src/*.cppm "$SRC"/v3/src/*.inc "$OUT/v3/src/"
cp "$SRC/v3/Makefile" "$OUT/v3/"
cp "$SRC/LICENSE" "$OUT/"
for d in monocypher-4.0.3 squared-data yyjson-0.12.0 zstd-1.5.7; do
  cp -R "$SRC/third_party/$d" "$OUT/third_party/"
done
commit=$(git -C "$SRC" rev-parse HEAD 2>/dev/null || echo unknown)
{
  echo "# sqcart's 3.0 library, copied by tools/vendor_sqcart.sh."
  echo "# Do not edit: change it in sqcart, then copy it again."
  echo "from: sqcart $commit"
  echo "copied: $(date -u +%Y-%m-%d)"
  (cd "$OUT" && find v3 -type f | LC_ALL=C sort | xargs sha256sum)
} > "$OUT/PINNED"
echo "sqcart copied into third_party/sqcart ($commit)"
