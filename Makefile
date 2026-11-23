# square's build (make, for now; sqmake builds square once a build.lua says so).
#
#   make CXX=/path/to/clang++   clang and libc++ 23 with `import std;`
#   make test                    the run check: square on a generated workspace
#
# Offline: squared's sq.data and yyjson are vendored in third_party, and the
# std module is built here from the libc++ the compiler uses (its manifest's
# source-path, read with make's own functions: no sed).

CXX      ?= clang++
BUILD    ?= build
OPT      ?= -O2 -g
STDLIB   := -stdlib=libc++
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -Wno-missing-field-initializers
CXXFLAGS := -std=c++23 $(STDLIB) $(OPT) $(WARN) -isystem third_party/yyjson-0.12.0/src \
            -fprebuilt-module-path=$(BUILD)/pcm -fmodule-file=std=$(BUILD)/pcm/std.pcm
LDFLAGS  := $(STDLIB)

STD_MANIFEST := $(shell readlink -f "$$($(CXX) $(STDLIB) -print-library-module-manifest-path 2>/dev/null)" 2>/dev/null)
comma := ,
STD_WORDS    := $(if $(wildcard $(STD_MANIFEST)),$(subst ", ,$(subst $(comma), ,$(file < $(STD_MANIFEST)))))
STD_SOURCE   := $(firstword $(filter %/std.cppm std.cppm,$(STD_WORDS)))
STD_CPPM     ?= $(if $(filter /%,$(STD_SOURCE)),$(STD_SOURCE),$(if $(STD_SOURCE),$(dir $(STD_MANIFEST))$(STD_SOURCE)))

# squared's data module (sq.data, vendored), its units in the order they
# import each other (file:module name), then its implementation units.
SQD       := third_party/squared-data
SQD_UNITS := src/json_error_code:src.json_error_code src/json_error:src.json_error \
             src/json_parse_options:src.json_parse_options json_value:json_value \
             src/json_parse_result:src.json_parse_result src/json_write_options:src.json_write_options \
             src/json_write_result:src.json_write_result json:json
sqd_name   = $(lastword $(subst :, ,$(1)))
SQD_PCMS  := $(foreach u,$(SQD_UNITS),$(BUILD)/pcm/sq.data-$(call sqd_name,$(u)).pcm)
SQD_OBJS  := $(foreach u,$(SQD_UNITS),$(BUILD)/obj/sq.data/$(call sqd_name,$(u)).o) \
             $(BUILD)/obj/sq.data/module.o $(BUILD)/obj/sq.data/json.cpp.o $(BUILD)/obj/sq.data/json_value.cpp.o

MODULES := sha256 workspace call
OBJS    := $(BUILD)/obj/std.o $(MODULES:%=$(BUILD)/obj/%.o) $(SQD_OBJS) $(BUILD)/obj/yyjson.o

.PHONY: all test clean
all: $(BUILD)/square

$(BUILD)/pcm/std.pcm:
	@test -n "$(STD_CPPM)" && test -f "$(STD_CPPM)" || { echo "no libc++ std module (set STD_CPPM)" >&2; exit 1; }
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj
	$(CXX) -std=c++23 $(STDLIB) $(OPT) -Wno-reserved-module-identifier --precompile $(STD_CPPM) -o $@
$(BUILD)/obj/std.o: $(BUILD)/pcm/std.pcm
	$(CXX) $(OPT) -c $< -o $@

define sqd_unit
$(BUILD)/pcm/sq.data-$(2).pcm $(BUILD)/obj/sq.data/$(2).o &: $(SQD)/$(1).cppm $(3)
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj/sq.data
	$$(CXX) $$(CXXFLAGS) -w -c $$< -fmodule-output=$(BUILD)/pcm/sq.data-$(2).pcm -o $(BUILD)/obj/sq.data/$(2).o
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
	$(CXX) $(CXXFLAGS) -w -c $< -fmodule-output=$(BUILD)/pcm/sq.data.pcm -o $(BUILD)/obj/sq.data/module.o
$(BUILD)/obj/sq.data/%.cpp.o: $(SQD)/src/%.cpp $(BUILD)/pcm/sq.data.pcm
	$(CXX) $(CXXFLAGS) -w -c $< -o $@
$(BUILD)/obj/yyjson.o: third_party/yyjson-0.12.0/src/yyjson.c
	@mkdir -p $(dir $@)
	$(CC) $(OPT) -w -c $< -o $@

define unit
$(BUILD)/pcm/square.$(1).pcm $(BUILD)/obj/$(1).o &: src/$(1).cppm $(BUILD)/pcm/std.pcm $(2)
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj
	$(CXX) $(CXXFLAGS) -c $$< -fmodule-output=$(BUILD)/pcm/square.$(1).pcm -o $(BUILD)/obj/$(1).o
endef
$(eval $(call unit,sha256,))
$(eval $(call unit,workspace,$(BUILD)/pcm/square.sha256.pcm $(BUILD)/pcm/sq.data.pcm))
$(eval $(call unit,call,$(BUILD)/pcm/square.workspace.pcm $(BUILD)/pcm/sq.data.pcm))

$(BUILD)/square: tools/square.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

test: all
	@bash tests/run_check.sh $(BUILD)/square

clean:
	rm -rf $(BUILD)
