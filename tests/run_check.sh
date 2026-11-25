#!/usr/bin/env bash
# tests/run_check.sh SQUARE [TEST_LIBSQUARE]: square's commands on a workspace made here, as
# sqpg makes one (SQ-INF/manifest.json of kind workspace, SQ-INF/files.json),
# and on resources (cartridges on a search path): the words, the JSON
# answers, `call` and its errors, and the exit statuses.
set -euo pipefail

square=$(realpath "$1")
work=$(mktemp -d "${TMPDIR:-/tmp}/square-check.XXXXXX")
trap 'rm -rf "$work"' EXIT
ok() { printf '  ok   %s\n' "$1"; }
die() { printf '  FAIL %s\n' "$1" >&2; exit 1; }

ws=$work/demo
mkdir -p "$ws/SQ-INF" "$ws/sq_app/src" "$ws/mk"
printf 'generated\n' > "$ws/mk/gen.mk"
printf 'seeded\n' > "$ws/build.lua"
printf 'mine\n' > "$ws/sq_app/src/main.cpp"
sum() { sha256sum "$1" | cut -d ' ' -f 1; }
cat > "$ws/SQ-INF/manifest.json" <<EOF
{"format": "squared-cartridge", "format_version": 3, "kind": "workspace",
 "id": "dev.me.demo", "version": "0.1.0", "title": "demo",
 "consumers": {"square": {
   "made_from": {"generator": "squared-pg@0.1.0-alpha.2", "template": "template.android.cpp@0.1.0",
                 "kits": [], "packages": [], "framework": "1.0.0"},
   "roles": {"app": "sq_app", "layouts": "sq_app/layouts", "build": "build"}},
  "squared_pg": {"parameters": {"project_name": "demo"}}}}
EOF
cat > "$ws/SQ-INF/files.json" <<EOF
{"mk/gen.mk": {"class": "generated", "origin": "template.android.cpp", "sha256": "$(sum "$ws/mk/gen.mk")"},
 "build.lua": {"class": "seeded", "origin": "template.android.cpp", "sha256": "$(sum "$ws/build.lua")"}}
EOF

# Found from inside, as git finds .git; and not found outside one.
(cd "$ws/sq_app/src" && "$square" info) | grep -q 'demo  dev.me.demo 0.1.0' || die "info from a subfolder"
(cd "$work" && ! "$square" info 2> err.txt) && grep -q 'no workspace here or above' "$work/err.txt" ||
    die "info outside a workspace did not say so"
"$square" --workspace "$ws" info | grep -q 'template   template.android.cpp@0.1.0' || die "info --workspace"
ok "finding the workspace: here or above, --workspace, and none"

[ "$(cd "$ws" && "$square" where layouts)" = sq_app/layouts ] || die "where layouts"
(cd "$ws" && ! "$square" where nothing 2> "$work/err.txt") && grep -q 'no role named nothing' "$work/err.txt" ||
    die "where an unknown role"
(cd "$ws" && "$square" --json where app) | grep -q '"path": "sq_app"' || die "where --json"
ok "where: a role's path, an unknown role refused"

(cd "$ws/sq_app" && "$square" owner ../build.lua) | grep -q 'build.lua  seeded  from template.android.cpp' ||
    die "owner of a seeded file, by a relative path"
(cd "$ws" && "$square" owner sq_app/src/main.cpp) | grep -q 'sq_app/src/main.cpp  user' || die "owner of a user file"
(cd "$ws" && ! "$square" owner /etc/hostname 2>/dev/null) || die "owner outside the workspace"
ok "owner: class and origin from the table; the developer's own; outside refused"

(cd "$ws" && "$square" verify) | grep -q 'nothing changed' || die "verify, unchanged"
# A seeded file the developer edits is theirs: reported, and no failure.
printf 'mine\n' >> "$ws/build.lua"
out=$(cd "$ws" && "$square" verify) || die "verify failed on a seeded file edited"
grep -q 'edited   build.lua  (seeded: yours)' <<< "$out" || die "verify: a seeded edit: $out"
printf 'edited\n' >> "$ws/mk/gen.mk"
rm "$ws/build.lua"
out=$(cd "$ws" && "$square" verify) && die "verify with changes exited 0"
grep -q 'changed  mk/gen.mk  (generated; an update would overwrite it)' <<< "$out" || die "verify: a changed generated file"
grep -q 'missing  build.lua  (seeded: yours)' <<< "$out" || die "verify: a missing seeded file"
(cd "$ws" && "$square" owner mk/gen.mk) | grep -q '(changed since written)' || die "owner: changed"
ok "verify: a seeded edit is yours (exit 0); a generated change fails, with what an update would overwrite"

resp=$(cd "$ws" && printf '{"sq": 1, "command": "where", "args": {"role": "app"}}' | "$square" call)
[ "$resp" = '{"ok":true,"result":{"path":"sq_app","role":"app"},"sq":1}' ] || die "call where: $resp"
resp=$(cd "$ws" && printf '{"sq": 1, "command": "where", "args": {"role": "x"}}' | "$square" call) && die "call error exited 0"
grep -q '"code":"workspace.role.unknown"' <<< "$resp" && grep -q '"roles":\["app","build","layouts"\]' <<< "$resp" ||
    die "call where, unknown: $resp"
resp=$(printf '{"sq": 2, "command": "info"}' | "$square" call) || true
grep -q '"code":"request.protocol"' <<< "$resp" || die "call, not sq 1: $resp"
resp=$(printf 'not json' | "$square" call) || true
grep -q '"code":"request.json"' <<< "$resp" || die "call, not JSON: $resp"
resp=$(cd "$ws" && printf '{"sq": 1, "command": "frob"}' | "$square" call) || true
grep -q '"code":"request.command.unknown"' <<< "$resp" || die "call, unknown command: $resp"
"$square" describe | grep -q '"tool": "square"' || die "describe"
ok "call: the SDK's protocol, its answers and its error codes; describe"

# roles: each role's path, whether it is there, its files and their class.
mkdir -p "$ws/sq_app/layouts"
printf '{}\n' > "$ws/sq_app/layouts/main.json"
out=$(cd "$ws" && "$square" roles)
grep -q '^app  *sq_app  *2 files$' <<< "$out" && grep -q '^layouts  *sq_app/layouts  *1 file$' <<< "$out" &&
    grep -q '^build  *build  *(not there)$' <<< "$out" || die "roles: $out"
out=$(cd "$ws" && "$square" --json roles)
grep -q '"path": "sq_app/src/main.cpp"' <<< "$out" && grep -q '"class": "user"' <<< "$out" &&
    grep -q '"form": "missing"' <<< "$out" || die "roles --json: $out"
ok "roles: each role's files and their class; a role not there says so"

# find: the workspaces under a folder, newest first; not inside one; a .sq
# that is not a cartridge reported, not fatal.
mkdir -p "$work/all/older" "$work/all/.hidden/ws" "$work/all/deep/a/b/c/d"
cp -r "$ws" "$work/all/older/one"
cp -r "$ws" "$work/all/two"
cp -r "$ws" "$work/all/.hidden/ws/three"
cp -r "$ws" "$work/all/deep/a/b/c/d/four"
cp -r "$ws" "$work/all/two/sq_app/nested"   # inside a workspace: the workspace's
touch -d '2020-01-01' "$work/all/older/one/SQ-INF/manifest.json"
printf 'not a cartridge' > "$work/all/broken.sq"
out=$("$square" find "$work/all" 2> "$work/err.txt")
[ "$(grep -c '^exploded' <<< "$out")" = 2 ] || die "find: two workspaces (not hidden, not too deep, not nested): $out"
[ "$(head -n 1 <<< "$out" | grep -c '/all/two$')" = 1 ] || die "find: newest first: $out"
grep -q 'could not read .*broken.sq' "$work/err.txt" || die "find: a broken .sq not reported"
[ "$(cd "$ws" && "$square" find | grep -c '^exploded')" = 1 ] || die "find in a workspace: itself"
"$square" --json find "$work/all" 2> /dev/null | grep -q '"template": "template.android.cpp@0.1.0"' || die "find --json"
! "$square" find "$work/none" 2> /dev/null || die "find: not a folder"
ok "find: newest first; hidden, too deep and nested left out; a broken .sq reported"

# pack and explode, through sqdrive: build/ and .git/ left out; a .sq found
# by find; exploded back into a folder of its own, the same files.
mkdir -p "$ws/build" "$ws/.git"
printf 'made\n' > "$ws/build/app.o"
printf 'ref\n' > "$ws/.git/HEAD"
mkdir -p "$work/shelf"
out=$(cd "$ws" && "$square" pack "$work/shelf/demo.sq")
grep -q "shelf/demo.sq" <<< "$out" || die "pack: $out"
(cd "$ws" && ! "$square" pack "$work/shelf/demo.sq" 2> "$work/err.txt") && grep -q 'there already' "$work/err.txt" ||
    die "pack: refuses a file that is there"
out=$("$square" find "$work/shelf")
grep -q '^compressed  *dev.me.demo  *0.1.0 .*shelf/demo.sq$' <<< "$out" || die "find: the .sq: $out"
out=$("$square" explode "$work/shelf/demo.sq" "$work/back")
grep -q 'back/demo  (' <<< "$out" || die "explode: $out"
[ -f "$work/back/demo/sq_app/src/main.cpp" ] && [ -f "$work/back/demo/SQ-INF/files.json" ] || die "explode: the files"
[ ! -e "$work/back/demo/build" ] && [ ! -e "$work/back/demo/.git" ] || die "explode: build/ or .git/ came back"
cmp -s "$ws/sq_app/src/main.cpp" "$work/back/demo/sq_app/src/main.cpp" || die "explode: not the same bytes"
(cd "$work/back/demo" && "$square" info) | grep -q 'demo  dev.me.demo 0.1.0' || die "explode: not a workspace again"
! "$square" explode "$work/shelf/demo.sq" "$work/back" 2> /dev/null || die "explode: over a folder that is there"
mkdir -p "$work/notws/SQ-INF"
printf '{"format":"squared-cartridge","format_version":3,"kind":"kit","id":"kit.x","version":"1.0.0"}' > "$work/notws/SQ-INF/manifest.json"
! "$square" --workspace "$work/notws" pack "$work/shelf/kit.sq" 2> /dev/null || die "pack: a kit is not a workspace"
resp=$(printf '{"sq": 1, "command": "explode", "args": {"cartridge": "%s", "into": "%s"}}' "$work/shelf/demo.sq" "$work/again" | "$square" call)
grep -q '"files":' <<< "$resp" || die "call explode: $resp"
ok "pack and explode (sqdrive): build/ and .git/ left out; find sees the .sq; back the same; refusals"

# Resources: cartridges on a search path, in it or in templates/, kits/ ...
res=$work/resources
cart() {  # cart DIR KIND ID VERSION TITLE [EXTRA-JSON]
    mkdir -p "$1/SQ-INF" "$1/tree"
    printf 'x\n' > "$1/tree/a.txt"
    printf '{"format": "squared-cartridge", "format_version": 3, "kind": "%s", "id": "%s", "version": "%s", "title": "%s"%s}\n' \
        "$2" "$3" "$4" "$5" "${6:-}" > "$1/SQ-INF/manifest.json"
}
cart "$res/templates/template.demo" template template.demo 0.1.0 "A demo" \
    ', "description": "Made for the check.", "requires": {"packages": ["package.core"]}'
cart "$res/kits/kit.demo" kit kit.demo 0.2.0 "Demo kit"
mkdir -p "$res/kits/kit.broken/SQ-INF"
printf '{"format": "squared-cartridge", "format_version": 2}\n' > "$res/kits/kit.broken/SQ-INF/manifest.json"
out=$("$square" --resources "$res" resources 2> "$work/err.txt")
grep -q '^template  template.demo .* 0.1.0 .* A demo$' <<< "$out" || die "resources: the template: $out"
grep -q '^kit .* kit.demo .* 0.2.0' <<< "$out" || die "resources: the kit"
grep -q 'refused .*kit.broken' "$work/err.txt" || die "resources: a refused cartridge not reported"
SQUARE_RESOURCES=$res "$square" resources kit 2>/dev/null | grep -q template && die "resources KIND did not filter"
SQUARE_RESOURCES=$res "$square" --json resources 2>/dev/null | grep -q '"id": "kit.demo"' || die "resources from \$SQUARE_RESOURCES"
ok "resources: found on the search path, by kind; one sq.cart refuses is reported, not fatal"

out=$("$square" --resources "$res" show template.demo)
grep -q 'Made for the check.' <<< "$out" && grep -q 'package.core' <<< "$out" && grep -q 'files     2' <<< "$out" ||
    die "show: $out"
"$square" --resources "$res" show kit.demo@0.2.0 > /dev/null || die "show id@version"
! "$square" --resources "$res" show kit.demo@9 2> "$work/err.txt" && grep -q 'no resource kit.demo@9' "$work/err.txt" ||
    die "show an absent version"
resp=$(printf '{"sq": 1, "command": "show", "args": {"id": "kit.demo", "search": "%s"}}' "$res" | "$square" call)
grep -q '"version":"0.2.0"' <<< "$resp" || die "call show: $resp"
ok "show: description, requires, files; id@version; absent refused; as a call"

# libsquare: the same calls in process (tests/test_libsquare.cpp), when built.
if [ -n "${2:-}" ]; then
    "$2" "$ws" "$res" > "$work/lib.txt" || { cat "$work/lib.txt"; die "libsquare"; }
    ok "libsquare: square::call in process, and a question called directly"
fi

echo "run check passed"
