# sq.cart: the Squared Cartridge 3.0 reader and writer, as C++23 modules.
#
#   make              the libraries, sqcart3 and the tests
#   make test         build and run the tests
#   make clean
#
# Needs clang and libc++ new enough for `import std;` (19 or later; 23 is
# what sqterm and CI use): the std module is built once, from the libc++ the
# compiler names (clang -print-library-module-manifest-path).
#
#   make CXX=/path/to/clang++
#
# Four archives, so a program that only reads carries no compressor:
#
#   reading:  libsqcart.a libsqcart-unzstd.a          (sq.cart, zstd's decoder)
#   writing:  libsqcart-write.a libsqcart.a libsqcart-zstd.a
#                                                     (sq.cart.write, all of zstd)
#
# libsqcart-unzstd.a and libsqcart-zstd.a both hold zstd's decoder: link one.
#
# Offline: nothing here downloads. zstd's decoder, yyjson, Monocypher and
# squared's data module (sq.data, the JSON facade sq.cart reads JSON
# through: ../tools/vendor_squared_data.sh) are vendored in ../third_party.

CXX      ?= clang++
CC       := $(patsubst %clang++,%clang,$(CXX))
BUILD    ?= build
OPT      ?= -O2 -g
STDLIB   := -stdlib=libc++
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -Wno-missing-designated-field-initializers
TP       := ../third_party
CFLAGS   := $(OPT) -w -I$(TP)/yyjson-0.12.0/src -I$(TP)/zstd-1.5.7 -I$(TP)/monocypher-4.0.3 \
            -I$(TP)/monocypher-4.0.3/optional
CXXFLAGS := -std=c++23 $(STDLIB) $(OPT) $(WARN) -fprebuilt-module-path=$(BUILD)/pcm \
            -fmodule-file=std=$(BUILD)/pcm/std.pcm \
            -isystem $(TP)/yyjson-0.12.0/src -isystem $(TP)/zstd-1.5.7 \
            -isystem $(TP)/monocypher-4.0.3 -isystem $(TP)/monocypher-4.0.3/optional -Isrc
LDFLAGS  := $(STDLIB) -lpthread

# libc++'s std module sources, as the compiler names them.
# Its paths are relative to the manifest itself, so follow any link to it
# first (Debian's /lib/x86_64-linux-gnu/libc++.modules.json is one), and take
# an absolute path as it is.
STD_MANIFEST := $(shell readlink -f "$$($(CXX) $(STDLIB) -print-library-module-manifest-path 2>/dev/null)" 2>/dev/null)
STD_SOURCE   := $(if $(STD_MANIFEST),$(shell sed -n 's/.*"source-path": *"\([^"]*std\.cppm\)".*/\1/p' $(STD_MANIFEST) 2>/dev/null | head -n 1))
STD_CPPM     := $(if $(filter /%,$(STD_SOURCE)),$(STD_SOURCE),$(if $(STD_SOURCE),$(dir $(STD_MANIFEST))$(STD_SOURCE)))

# squared's data module: its interface units in the order they import each
# other (file:module name), then its implementation units.
SQD       := $(TP)/squared-data
SQD_UNITS := src/json_error_code:src.json_error_code src/json_error:src.json_error \
             src/json_parse_options:src.json_parse_options json_value:json_value \
             src/json_parse_result:src.json_parse_result src/json_write_options:src.json_write_options \
             src/json_write_result:src.json_write_result json:json
sqd_file   = $(firstword $(subst :, ,$(1)))
sqd_name   = $(lastword $(subst :, ,$(1)))
SQD_PCMS  := $(foreach u,$(SQD_UNITS),$(BUILD)/pcm/sq.data-$(call sqd_name,$(u)).pcm)
SQD_OBJS  := $(foreach u,$(SQD_UNITS),$(BUILD)/obj/sq.data/$(call sqd_name,$(u)).o) \
             $(BUILD)/obj/sq.data/module.o $(BUILD)/obj/sq.data/json.cpp.o $(BUILD)/obj/sq.data/json_value.cpp.o

# The partitions, in the order they import each other.
PARTS    := error bytes sha256 json fold zstd ed25519 archive exploded
OBJS     := $(BUILD)/obj/std.o $(SQD_OBJS) $(foreach p,$(PARTS),$(BUILD)/obj/$(p).o) $(BUILD)/obj/sq.cart.o
C_OBJS   := $(BUILD)/obj/yyjson.o $(BUILD)/obj/monocypher.o $(BUILD)/obj/monocypher-ed25519.o
READ_LIBS  := $(BUILD)/libsqcart.a $(BUILD)/libsqcart-unzstd.a
WRITE_LIBS := $(BUILD)/libsqcart-write.a $(BUILD)/libsqcart.a $(BUILD)/libsqcart-zstd.a
TESTS    := $(patsubst tests/%.cpp,$(BUILD)/%,$(wildcard tests/test_*.cpp))

.PHONY: all test clean
all: $(READ_LIBS) $(WRITE_LIBS) $(BUILD)/sqcart3 $(BUILD)/sqcart3-read $(TESTS)

$(BUILD)/pcm/std.pcm:
	@test -n "$(STD_CPPM)" && test -f "$(STD_CPPM)" || { \
	  echo "no libc++ std module: $(CXX) $(STDLIB) -print-library-module-manifest-path says '$(STD_MANIFEST)'" >&2; exit 1; }
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj
	$(CXX) -std=c++23 $(STDLIB) $(OPT) -Wno-reserved-module-identifier --precompile $(STD_CPPM) -o $@
$(BUILD)/obj/std.o: $(BUILD)/pcm/std.pcm
	$(CXX) $(OPT) -c $< -o $@

# sq.data, one unit after another (each may import those before it).
define sqd_unit
$(BUILD)/pcm/sq.data-$(2).pcm $(BUILD)/obj/sq.data/$(2).o &: $(SQD)/$(1).cppm $(3)
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj/sq.data
	$$(CXX) $$(CXXFLAGS) -c $$< -fmodule-output=$(BUILD)/pcm/sq.data-$(2).pcm -o $(BUILD)/obj/sq.data/$(2).o
endef
SQDP := $(BUILD)/pcm/sq.data-
$(eval $(call sqd_unit,src/json_error_code,src.json_error_code,$(BUILD)/pcm/std.pcm))
$(eval $(call sqd_unit,src/json_error,src.json_error,$(SQDP)src.json_error_code.pcm))
$(eval $(call sqd_unit,src/json_parse_options,src.json_parse_options,$(SQDP)src.json_error.pcm))
$(eval $(call sqd_unit,json_value,json_value,$(SQDP)src.json_parse_options.pcm))
$(eval $(call sqd_unit,src/json_parse_result,src.json_parse_result,$(SQDP)json_value.pcm))
$(eval $(call sqd_unit,src/json_write_options,src.json_write_options,$(SQDP)src.json_parse_result.pcm))
$(eval $(call sqd_unit,src/json_write_result,src.json_write_result,$(SQDP)src.json_write_options.pcm))
$(eval $(call sqd_unit,json,json,$(SQDP)src.json_write_result.pcm))
$(BUILD)/pcm/sq.data.pcm $(BUILD)/obj/sq.data/module.o &: $(SQD)/module.cppm $(SQD_PCMS)
	$(CXX) $(CXXFLAGS) -c $< -fmodule-output=$(BUILD)/pcm/sq.data.pcm -o $(BUILD)/obj/sq.data/module.o
$(BUILD)/obj/sq.data/%.cpp.o: $(SQD)/src/%.cpp $(BUILD)/pcm/sq.data.pcm
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Each partition: its BMI and its object in one compile, after what it imports.
define part
$(BUILD)/pcm/sq.cart-$(1).pcm $(BUILD)/obj/$(1).o &: src/$(1).cppm $(BUILD)/pcm/std.pcm $(2)
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj
	$$(CXX) $$(CXXFLAGS) -c $$< -fmodule-output=$(BUILD)/pcm/sq.cart-$(1).pcm -o $(BUILD)/obj/$(1).o
endef
$(eval $(call part,error,))
$(eval $(call part,bytes,$(BUILD)/pcm/sq.cart-error.pcm))
$(eval $(call part,sha256,))
$(eval $(call part,json,$(BUILD)/pcm/sq.cart-error.pcm $(BUILD)/pcm/sq.data.pcm))
$(eval $(call part,fold,$(BUILD)/pcm/sq.cart-json.pcm src/casefold.inc src/nfc.inc))
$(eval $(call part,zstd,$(BUILD)/pcm/sq.cart-error.pcm))
$(eval $(call part,ed25519,))
$(eval $(call part,archive,$(foreach p,error bytes sha256 json fold zstd ed25519,$(BUILD)/pcm/sq.cart-$(p).pcm) $(BUILD)/pcm/sq.data.pcm))
$(eval $(call part,exploded,$(BUILD)/pcm/sq.cart-archive.pcm))

$(BUILD)/pcm/sq.cart.pcm $(BUILD)/obj/sq.cart.o &: src/sq.cart.cppm $(foreach p,$(PARTS),$(BUILD)/pcm/sq.cart-$(p).pcm)
	$(CXX) $(CXXFLAGS) -c $< -fmodule-output=$(BUILD)/pcm/sq.cart.pcm -o $(BUILD)/obj/sq.cart.o

$(BUILD)/obj/yyjson.o: $(TP)/yyjson-0.12.0/src/yyjson.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/obj/zstddeclib.o: $(TP)/zstd-1.5.7/zstddeclib.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/obj/zstdlib.o: $(TP)/zstd-1.5.7/zstd.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DZSTD_MULTITHREAD=0 -c $< -o $@
$(BUILD)/obj/monocypher.o: $(TP)/monocypher-4.0.3/monocypher.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/obj/monocypher-ed25519.o: $(TP)/monocypher-4.0.3/optional/monocypher-ed25519.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# The writer: its own module, sq.cart.write, which imports sq.cart.
$(BUILD)/pcm/sq.cart.write.pcm $(BUILD)/obj/write.o &: src/write.cppm $(BUILD)/pcm/sq.cart.pcm
	$(CXX) $(CXXFLAGS) -c $< -fmodule-output=$(BUILD)/pcm/sq.cart.write.pcm -o $(BUILD)/obj/write.o

$(BUILD)/libsqcart.a: $(OBJS) $(C_OBJS)
	rm -f $@
	ar rcs $@ $^
$(BUILD)/libsqcart-unzstd.a: $(BUILD)/obj/zstddeclib.o
	rm -f $@
	ar rcs $@ $^
$(BUILD)/libsqcart-write.a: $(BUILD)/obj/write.o
	rm -f $@
	ar rcs $@ $^
$(BUILD)/libsqcart-zstd.a: $(BUILD)/obj/zstdlib.o
	rm -f $@
	ar rcs $@ $^

$(BUILD)/sqcart3: tools/sqcart3.cpp $(BUILD)/pcm/sq.cart.write.pcm $(WRITE_LIBS)
	$(CXX) $(CXXFLAGS) $< $(WRITE_LIBS) $(LDFLAGS) -o $@

# The tests share a module of helpers (tests/testing.cppm: it writes
# cartridges, byte by byte, for the reader to open).
$(BUILD)/pcm/sq.cart.testing.pcm $(BUILD)/obj/testing.o &: tests/testing.cppm $(BUILD)/pcm/sq.cart.pcm
	$(CXX) $(CXXFLAGS) -c $< -fmodule-output=$(BUILD)/pcm/sq.cart.testing.pcm -o $(BUILD)/obj/testing.o

# The tests link the writer's archives (a reader-only test links them too:
# it is the reader's code either way).
$(BUILD)/test_%: tests/test_%.cpp $(BUILD)/pcm/sq.cart.testing.pcm $(BUILD)/pcm/sq.cart.write.pcm $(WRITE_LIBS)
	$(CXX) $(CXXFLAGS) $< $(BUILD)/obj/testing.o $(WRITE_LIBS) $(LDFLAGS) -o $@

# A reader alone links without the compressor: proved by linking sqcart3's
# read-only twin against the reading archives.
$(BUILD)/sqcart3-read: tools/sqcart3.cpp $(BUILD)/pcm/sq.cart.pcm $(READ_LIBS)
	$(CXX) $(CXXFLAGS) -DSQCART3_READ_ONLY $< $(READ_LIBS) $(LDFLAGS) -o $@

test: all
	@fail=0; for t in $(TESTS); do ./$$t || fail=1; done; \
	if [ $$fail -ne 0 ]; then echo "FAILED"; exit 1; fi; echo "all tests passed"

clean:
	rm -rf $(BUILD)
