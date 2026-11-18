# square

The workspace manager of the [squared SDK](https://squared-netizen.github.io/squared-netizen/),
and the one tool that knows what a workspace is. sqpg creates a workspace
once; from then on square answers every question about it and makes every
change to its shape.

**Status: designed, not yet built.**

## What it will do

- **Answer questions, in JSON.** `square info`, `square where <role>` (the
  app, its layouts, its docs), `square owner <path>`, `square verify`,
  `square build-inputs` for sqmake, `square explain <shim>` for sqgui. Each
  answer is one versioned JSON envelope, through the command line or
  through `libsquare`, a C++ module the other tools link in. No daemon.
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
sqpg (creates)   sqmake (builds)   sqgui (edits the UI)   you
          \            |                 |               /
                         square
          /            |                 \
     sqcart     squared-pg's engine     dexgen
```

## Licence

MIT: see [LICENSE](LICENSE). Security issues: see [SECURITY.md](SECURITY.md).
