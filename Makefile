# square's build (make, for now; sqmake builds square once a build.lua says so).
#
#   make CXX=/path/to/clang++   clang and libc++ 23 with `import std;`
#   make test                    the run check: square on a generated workspace
#
# Offline: sq.cart (the cartridge reader, with squared's sq.data and yyjson
# inside it) is vendored in third_party/sqcart (tools/vendor_sqcart.sh) and
# built there; square takes its std module and sq.data from that build. The
# std module's source is found from the compiler's manifest with make's own
# functions (no sed), and handed to sqcart's build as STD_CPPM.

CXX      ?= clang++
BUILD    ?= build
OPT      ?= -O2 -g
STDLIB   := -stdlib=libc++
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -Wno-missing-field-initializers
CART     := third_party/sqcart/v3
CART_B   := $(abspath $(BUILD))/sqcart
CXXFLAGS := -std=c++23 $(STDLIB) $(OPT) $(WARN) \
            -fprebuilt-module-path=$(CART_B)/pcm -fprebuilt-module-path=$(BUILD)/pcm \
            -fmodule-file=std=$(CART_B)/pcm/std.pcm
LDFLAGS  := $(STDLIB)

STD_MANIFEST := $(shell readlink -f "$$($(CXX) $(STDLIB) -print-library-module-manifest-path 2>/dev/null)" 2>/dev/null)
comma := ,
STD_WORDS    := $(if $(wildcard $(STD_MANIFEST)),$(subst ", ,$(subst $(comma), ,$(file < $(STD_MANIFEST)))))
STD_SOURCE   := $(firstword $(filter %/std.cppm std.cppm,$(STD_WORDS)))
STD_CPPM     ?= $(if $(filter /%,$(STD_SOURCE)),$(STD_SOURCE),$(if $(STD_SOURCE),$(dir $(STD_MANIFEST))$(STD_SOURCE)))

# Reading only: sq.cart (with std, sq.data and yyjson) and zstd's decoder.
CART_LIBS := $(CART_B)/libsqcart.a $(CART_B)/libsqcart-unzstd.a
MODULES   := workspace resources call
OBJS      := $(MODULES:%=$(BUILD)/obj/%.o)

.PHONY: all test clean sqcart
all: $(BUILD)/square

# sqcart's own build, told the std module's source and not to look for it.
sqcart:
	$(MAKE) -C $(CART) BUILD=$(CART_B) CXX='$(CXX)' OPT='$(OPT)' STD_MANIFEST= STD_CPPM='$(STD_CPPM)' \
	    $(CART_LIBS) $(CART_B)/pcm/sq.cart.pcm
$(CART_LIBS) $(CART_B)/pcm/sq.cart.pcm: sqcart

define unit
$(BUILD)/pcm/square.$(1).pcm $(BUILD)/obj/$(1).o &: src/$(1).cppm $(CART_B)/pcm/sq.cart.pcm $(2)
	@mkdir -p $(BUILD)/pcm $(BUILD)/obj
	$(CXX) $(CXXFLAGS) -c $$< -fmodule-output=$(BUILD)/pcm/square.$(1).pcm -o $(BUILD)/obj/$(1).o
endef
$(eval $(call unit,workspace,))
$(eval $(call unit,resources,$(BUILD)/pcm/square.workspace.pcm))
$(eval $(call unit,call,$(BUILD)/pcm/square.workspace.pcm $(BUILD)/pcm/square.resources.pcm))

$(BUILD)/square: tools/square.cpp $(OBJS) $(CART_LIBS)
	$(CXX) $(CXXFLAGS) $< $(OBJS) $(CART_LIBS) $(LDFLAGS) -o $@

test: all
	@bash tests/run_check.sh $(BUILD)/square

clean:
	rm -rf $(BUILD)
