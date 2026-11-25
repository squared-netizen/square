# square

The workspace manager of the [squared SDK](https://squared-netizen.github.io/squared-netizen/),
and the one tool that knows what a workspace is. sqpg creates a workspace
once; from then on square answers every question about it and makes every
change to its shape.

**Status: phases 1 and 2 built** (0.3.0, 2026-11-24): `info`, `where`,
`owner`, `verify`, `resources` and `show`, as words and as `square call`, on
the workspaces sqpg writes and the cartridges it installs; and every
template and kit builds with sqmake from Lua in the workspace
(`sq/build/template.lua`, `sq/build/kit_<name>.lua`: spec §4).
Manifests are read through sqcart (vendored in `third_party/sqcart`).
libsquare (`make lib`: `build/libsquare.a` and the module `square`) gives
sqpg and sqgui the same calls in process. Next: changing a workspace
(`add`, `update`, phase 4). [docs/spec.md](docs/spec.md),
draft 0.1 (2026-11-20).

```sh
make CXX=clang++ && make test       # clang and libc++ 23, `import std`
cd my-workspace/sq_app
square info                          # what it is, what it was made from, its roles
square where layouts                 # sq_app/layouts
square owner ../build.lua            # build.lua  seeded  from template.android.cpp
square verify                        # what changed since it was written (non-zero if anything)
square roles                         # each role: its path, its files and their class
square find ~                        # the workspaces under a folder, exploded or .sq, newest first
square pack                          # this workspace into one .sq to share (build/ left out)
square explode demo.sq ~/work        # a .sq workspace out as ~/work/demo
square resources kit                 # the kits installed (--resources PATH, $SQUARE_RESOURCES)
square show kit.terminal             # one of them: what it is, what it needs
echo '{"sq": 1, "command": "where", "args": {"role": "app"}}' | square call
```

## What it will do

- **Answer questions, in JSON.** `square info`, `square where <role>` (the
  app, its layouts, its docs), `square owner <path>`, `square verify`,
  `square explain <shim>` for sqgui: the SDK's tool protocol (`square call`,
  `square describe`), and the same in process through `libsquare`. No daemon.
- **Keep a workspace's record.** A workspace is an exploded cartridge
  (`SQ-INF/manifest.json`, kind `workspace`): its template, kits and packages,
  each path's role and ownership. People write Lua (`build.lua`); square
  writes the JSON.
- **Leave the build to the workspace.** A template's and a kit's build
  conventions are Lua files in the workspace (`sq/build/*.lua`) that
  `build.lua` includes, so sqmake builds it asking square nothing.
- **Ask sqcart, never read manifests itself.** Every manifest is defined by
  the Squared Cartridge Specification; square reads the sections addressed
  to it through sqcart, so a change in the format touches sqcart and square
  alone.
- **Change a workspace safely.** Adding a kit or a package and updating a
  resource run as plan, decide, apply: a plan the developer reads, an
  answer for every conflict, and a journalled transaction that a crash
  cannot leave half done. Ownership classes decide the rest: a `seeded` or
  `user` file is never touched.
- **Extract.** Turn a workspace that has grown to suit its developer into a
  template, kits or packages for the next project, through a plan file the
  developer edits.

## Where it sits

```text
sqpg (creates)        sqgui (edits the UI)        you
          \                   |                  /
                         square
          /                   |                  \
     sqcart          squared-pg's engine         dexgen

sqmake builds the workspace from its build.lua, and asks square nothing.
```

## Licence

MIT: see [LICENSE](LICENSE). Security issues: see [SECURITY.md](SECURITY.md).
