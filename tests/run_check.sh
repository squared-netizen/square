#!/usr/bin/env bash
# tests/run_check.sh SQUARE: square's commands on a workspace made here, as
# sqpg makes one (SQ-INF/manifest.json of kind workspace, SQ-INF/files.json):
# the words, the JSON answers, `call` and its errors, and the exit statuses.
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
printf 'edited\n' >> "$ws/mk/gen.mk"
rm "$ws/build.lua"
out=$(cd "$ws" && "$square" verify) && die "verify with changes exited 0"
grep -q 'changed  mk/gen.mk  (generated; an update would overwrite it)' <<< "$out" || die "verify: a changed generated file"
grep -q 'missing  build.lua  (seeded)' <<< "$out" || die "verify: a missing seeded file"
(cd "$ws" && "$square" owner mk/gen.mk) | grep -q '(changed since written)' || die "owner: changed"
ok "verify: changed and missing, what an update would overwrite, non-zero"

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

echo "run check passed"
