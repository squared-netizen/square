-- build.lua: square's own build, for sqmake.
--
--   sqmake          build/square, libsquare (build/libsquare.a: the module
--                   square, for sqpg and sqgui) and build/test_libsquare
--   sqmake test     the run check: square on a generated workspace, and
--                   libsquare called in process
--   sqmake --debug  -O0 (build/debug)
--
-- The compiler is $CXX (and $CC): clang and libc++ 23 with `import std;`.
-- Offline: sq.drive (sqdrive's library: how square moves cartridges, pack
-- and explode) is vendored in third_party/sqdrive, and the sq.cart it
-- builds on (with squared's sq.data and yyjson) in third_party/sqcart, both
-- by tools/vendor_sqdrive.sh; sq.cart is built by its own v3/build.lua.

local opt = mode.debug and {"-O0", "-g"} or {"-O2", "-g"}
toolchain.cxxflags = {"-std=c++23", "-stdlib=libc++", opt}
toolchain.cflags = {opt}
toolchain.ldflags = {"-stdlib=libc++", "-pthread"}  -- -pthread: Android has no libpthread

local warn = {"-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wno-sign-conversion",
              "-Wno-missing-field-initializers"}

sqcart_settings = {root = "third_party/sqcart/v3"}
local cart = include("third_party/sqcart/v3/build.lua")

-- sq.drive as sqdrive builds it (its warnings are its own).
local drive = library{name = "sqdrive", srcs = {"third_party/sqdrive/src/drive.cppm"}, deps = {cart.write},
                      flags = {warn, "-Wno-conversion"}}

-- libsquare: square's modules, and square, which re-exports them.
local lib = library{name = "square", srcs = glob("src/*.cppm"), deps = {drive, cart.write}, flags = warn}

local bin = exe{name = "square", srcs = {"tools/square.cpp"}, deps = {lib}, flags = warn}
local libtest = exe{name = "test_libsquare", srcs = {"tests/test_libsquare.cpp"}, deps = {lib}, flags = warn}

verb("test", {needs = {bin, libtest}, call = function()
  return exec{"bash", "tests/run_check.sh", bin, libtest}
end})
