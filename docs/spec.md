# square: specification

**Status: draft 0.1, 2026-11-20;** phase 1 is being built (§8). It carries the
design of 2026-11-17 ("square: design", decided 2026-11-18) forward and
brings it into line with the SDK's decisions since: the tool protocol and
the people/tools split (squared's `docs/developer/sdk-tools.md`), and
sqmake's independence (sqmake's spec §1). What is decided is marked; the open
questions are listed at the end.

## 1. What square is

square is the one tool that knows what a workspace is. sqpg creates a
workspace once; from then on square answers every question about it and makes
every change to its shape: adding a kit or a package, updating one, and
extracting templates, kits and packages from a workspace that has grown.

What a workspace is, square defines (§2). What a cartridge is, sqcart defines
(the Squared Cartridge Specification): square reads and writes cartridges
only through sqcart. How a project builds, the project's `build.lua` says
(§4); sqmake builds it and asks square nothing.

## 2. A workspace (decided)

A workspace is a folder that is an **exploded cartridge** of kind
`workspace`: it holds `SQ-INF/manifest.json` in sqcart's envelope (decided
2026-11-18, with cartridge spec 3.0). Packed, the same workspace is one `.sq`
file: to back it up, share it, or keep it in a drive.

### 2.1 What is in it, and who writes it

| Path | Written by | Class | What it is |
| --- | --- | --- | --- |
| `SQ-INF/manifest.json` | square (sqpg, the first time, through libsquare) | metadata | what the workspace is (§2.2) |
| `SQ-INF/files.json` | square | metadata | the provenance table: each generated path's class, origin and sha256 (§2.2) |
| `.squared/` | square | metadata | its journal, staging and lock while a change runs (§6), nothing else |
| `build.lua` | the template, once; then the developer | seeded | how the project builds (§4) |
| `sq/build/*.lua` | the template and each kit; replaced on update | generated | the build's conventions, which `build.lua` includes (§4) |
| `squared/` | the framework package | generated | squared, vendored |
| the template's layers (`sq_app/`, `sq_android/`, ...) | the template, then the developer | seeded, user | the project's own code and resources |

The **classes** are squared-pg's (§2.7.10): `generated` is rewritten by an
update when unchanged since it was written; `seeded` and `user` are the
developer's and never touched; `metadata` is square's own.

The rule of the SDK holds here: **people write Lua, tools write JSON**. The
developer edits `build.lua` and the code; they never edit the manifest, which
changes only through square's commands (§5), each one a plan they see first.

### 2.2 The manifest

sqcart's envelope (identity, `kind: "workspace"`, version), and one member
of `consumers` for each tool that gives the workspace a meaning. sqcart
knows none of it: a kind, and what a workspace is, are square's (sqcart's
spec §7).
Only that tool reads or changes its member; sqcart keeps them all without
reading them.

```json
{"format": "squared-cartridge", "format_version": 3,
 "kind": "workspace", "id": "dev.squared.sqterm", "version": "0.1.0", "title": "sqterm",
 "consumers": {
   "square": {
     "made_from": {"generator": "squared-pg@0.1.0-alpha.1", "template": "template.android.cpp@0.1.0",
                   "kits": [], "packages": ["package.squared-core@1.0.0"], "framework": "1.0.0"},
     "roles": {"app": "sq_app", "android": "sq_android", "assets": "sq_android/assets",
               "layouts": "sq_app/layouts", "build": "build", "docs": "docs"},
     "files": {"squared/data/json.cppm": {"class": "generated", "origin": "package.squared-core",
                                          "sha256": "..."}}
   },
   "squared_pg": {"parameters": {"project_name": "sqterm", "package_name": "dev.squared.sqterm"}}
 }}
```

- **`consumers.square.made_from`**: the generator, template, kits and
  packages the workspace came from, with versions. square's, not the
  envelope's: the cartridge format records no provenance (its §7).
- **`consumers.square.roles`**: a path for each role. This is how a tool
  finds the app, its layouts or its assets without guessing from folder
  names (squared-pg §2.7.3 allows any layout).
- **The provenance table** (in `SQ-INF/files.json`, beside the manifest,
  §9; shown here inside it for brevity): One entry per path
  that a template, kit or package wrote: its class, its origin, and its
  sha256 as written. A file that is not listed is the developer's.
- **`consumers.squared_pg`**: what sqpg needs to generate again, such as the
  template's parameter values. square carries it and does not read it.

### 2.3 Finding the workspace (decided)

The nearest folder above the current one whose `SQ-INF/manifest.json` has
`kind: "workspace"`, as git finds `.git`. `--workspace DIR` (or the call's
`args.workspace`) says which instead. Paths in every answer are relative to
the workspace, so an answer stays right on another machine.

## 3. Questions and changes (decided)

| Command | Answers or does | Reads |
| --- | --- | --- |
| `info` | name, id, template, kits, packages, versions, framework version | the manifest |
| `where` (`role`) | a role's path: app, android, assets, layouts, build, docs | `roles` |
| `owner` (`path`) | a path's class and origin, and whether it changed since it was written | `files`, the file's sha256 |
| `verify` | every generated or seeded path that changed, and what an update would overwrite | `files`, the tree |
| `resources` | the templates, kits and packages that can be added, with versions | installed cartridges, through sqcart |
| `show` (`id`) | one resource's manifest: description, version, dependencies, files | that cartridge, through sqcart |
| `explain` (`shim`) | a dex shim's description, bytecode, spec references, its Java equivalent | dexgen's description file |
| `add` (`id`) | adds a kit or a package (§6) | as §6 |
| `update` (`id`) | moves the template, a kit or a package to a newer version (§6) | as §6 |
| `extract` (`what`, `paths`) | turns parts of the workspace into a template, kit or package (§7) | as §7 |
| `backup` (`file`) | the workspace packed into one `.sq` file **with its `.git/`** (`sqcart pack --keep .git`); restored with `sqcart unpack --executable` so its hooks run | the tree |

A plain `sqcart pack` of a workspace (to share it) leaves `.git/` and
`.squared/` out, as every top-level dot folder (sqcart's spec §6); only
`backup` keeps `.git/`. `.squared/` is never kept.

**`build-inputs` is gone** (2026-11-20). sqmake asks square nothing (§4).

## 4. Building: conventions in Lua, in the workspace (decided)

sqmake builds what `build.lua` describes, and nothing else tells it what to
build. So the conventions a template or a kit brings are Lua files in the
workspace, not knowledge inside sqmake or square:

```lua
-- build.lua: seeded, the developer's. The template wrote it once.
include("sq/build/template.lua")   -- the template's conventions: squared, the app, the APK
include("sq/build/kits.lua")       -- one line per kit, kept by square (generated)

-- The developer's own additions, if any:
-- toolchain.cxxflags = {"-O3"}
```

- **`sq/build/template.lua`** (generated) is the template's build: for
  template.android.cpp, `toolchain.sdk("squared/build/toolchain.sh")`,
  squared's modules, the app's modules from the `app` role, the native
  library, and the APK through apkpack. An update of the template replaces
  it; the developer's `build.lua` is untouched.
- **`sq/build/kits.lua`** (generated) includes each kit's own
  `sq/build/kit_<id>.lua`; `square add` and `update` rewrite it and the kit's
  file, never `build.lua`.
- **sqmake needs one more function for this**: `include(path)`, which runs
  another Lua file of the workspace in the same sandboxed environment.
  sqmake's phase 3 adds it. With it, phase 3 needs no `project{...}`
  built into sqmake: the convention is the template's Lua file.

This keeps sqmake knowing nothing of workspaces, square knowing nothing of how
a build runs, and the developer able to read and change every rule that
builds their project.

## 5. Calls (decided)

square speaks the SDK's tool protocol (squared's
`docs/developer/sdk-tools.md`), instead of the envelope of its own that the
2026-11-17 design had:

```sh
echo '{"sq": 1, "command": "where", "args": {"role": "layouts"}}' | square call
# {"sq": 1, "ok": true, "result": {"role": "layouts", "path": "sq_app/layouts"}}
square describe
```

- **Commands** as §3, each with `args.workspace` (optional, §2.3).
- **Error codes** in squared-pg's dotted form (§2.15), with `detail` a tool
  can act on: `{"code": "workspace.role.unknown", "message": "no role named
  layouts", "detail": {"roles": ["app", "android", "assets"]}}`.
- **People** get the same commands as words: `square where layouts` prints
  `sq_app/layouts`. `call` is for programs.
- **libsquare**: the same calls in process, as `sq::data::JsonValue`, for
  sqgui and for sqpg.
- **Calls go one way.** square calls sqcart (cartridges) and squared-pg's
  engine (writing files, §6). sqpg and sqgui call square. square never calls
  sqmake, and sqmake never calls square (§9).

## 6. Changing a workspace (decided)

`add` and `update` run in three steps, and nothing is written until the third:

1. **Plan.** Through sqcart, square gets the resource's files and their
   classes. It compares them with `files`, and writes a plan: each path, what
   happens to it (add, replace, leave), and every conflict. A conflict is a
   generated file that has changed since it was written. `--plan` prints the
   plan and stops.
2. **Decide.** Each conflict needs an answer: keep mine, take theirs, or write
   theirs beside mine as `<name>.new`. Nothing is overwritten silently. A
   `--force` would still report every path.
3. **Apply, as a transaction.** New content is staged under
   `.squared/staging/<txn>/` and the plan recorded in
   `.squared/journal/<txn>.json`, with `.squared/lock` held. Then each path
   is renamed into place, and the manifest updated last. After a crash, the
   next square run finishes the journal or undoes it before doing anything
   else.

squared-pg's engine writes the files (instantiation and parameters exist once,
there); square decides. A kit that changes the build adds its
`sq/build/kit_<id>.lua` and a line in `sq/build/kits.lua`, both generated.
It never edits `build.lua`.

## 7. Extraction (decided)

`extract template` (or `kit` / `package` with paths) sorts every path:
- unchanged since generation: carried as is;
- seeded and since edited: the candidates, the developer's improvements;
- the developer's own: asked about one by one;
- build output: left out.

It also proposes turning the project's own values back into parameters.
The plan is a file the developer edits. `extract --apply` writes the result
as a cartridge through sqcart's writer: a manifest with each path's class
declared.

## 8. Phases

1. **Read**: `info`, `where`, `owner`, `verify`, `resources`, `show`, as
   `call`, `describe`, words and libsquare. (Built in 0.2.0, but for
   libsquare and the sqterm gate. `resources` searches `--resources PATH`,
   else `$SQUARE_RESOURCES`, else `$PREFIX/share/squared-pg/resources` and
   `/usr/local/share/squared-pg/resources`, each folder and its
   `templates/`, `kits/`, `packages/` and `assets/`.) Before it, cartridge spec 3.0 and
   sqterm's workspace converted to an exploded cartridge. Gate: sqterm's
   workspace answers every question correctly, and `verify` reports a file
   edited on purpose.
2. **Build conventions**: `sq/build/template.lua` for template.android.cpp,
   with sqmake's `include()`. Gate: sqterm builds and installs with
   `sqmake`, and with no make.
3. **Explain**: once dexgen's description file exists.
4. **Change**: `add`, `update`, the journal. Gate: a kit added to a copy of
   sqterm, a crash forced mid-apply, and the next run rolls it back cleanly.
5. **Extract**: templates, then kits and packages. Gate: a template
   extracted from sqterm generates a new workspace that builds and runs.

## 9. Decided 2026-11-20 (the open questions of draft 0.1)

- [x] **`sq/build/`** holds the generated build files; `sq/` is where any
  other generated tool file of a workspace goes.
- [x] **The provenance table is a file of its own**, `SQ-INF/files.json`
  (tool-written JSON, in the cartridge), not a member of the manifest: the
  manifest stays small, and it is read on every question. §2.2's
  `consumers.square.files` moves there: `{"<path>": {"class", "origin",
  "sha256"}}`.
- [x] **square never runs a build.** `verify` says what changed; building is
  sqmake's, asked by the developer or by sqpg, never by square.
