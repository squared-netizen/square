// SPDX-License-Identifier: MIT
//
// square: libsquare's one module (docs/spec.md §5). The questions square
// answers, in process, as sq::data::JsonValue: what sqpg and sqgui import
// rather than running the program.
//
//   import square;
//   const sq::data::JsonValue response = square::call(request, folder);
//
// The request and response are the SDK's tool protocol, the same as
// `square call` reads and writes; run(), describe() and the questions
// themselves (info, where, owner, verify, roles, find, pack, explode,
// resources, show) are
// here too.
// Link build/libsquare.a with sqcart's writing libraries (and -lpthread: the Drive).

export module square;

export import square.workspace;
export import square.resources;
export import square.survey;
export import square.cartridge;
export import square.call;
