# Convenience wrapper over CMake. The build system proper is CMakeLists.txt.
# Override with GENERATOR=Ninja, JOBS=1, BUILD=<dir>, SDK=<prefix>.

# The SDK this tree builds against. Move it with each Braam release, here and
# in README.md. A binary stamped for another process ABI is refused at exec.
SDK_RELEASE := v0.4
SDK_VERSION := 0.4.162-6b94bea
SDK_URL := https://github.com/braamix/core/releases/download/$(SDK_RELEASE)/braam-sdk-$(SDK_VERSION).zip

BUILD     ?= build
GENERATOR ?= Unix Makefiles
# Fetched into the build directory, unless SDK names one already unpacked.
SDK       ?= $(BUILD)/braam-sdk-$(SDK_VERSION)
TOOLCHAIN := $(SDK)/lib/cmake/braam/wasm32-unknown-unknown.cmake
# make's own -jN cannot reach the generated build: its jobserver descriptors do
# not survive the cmake process in between. Pass a count explicitly instead.
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: all clean

all: $(BUILD)/CMakeCache.txt
	@cmake --build $(BUILD) -j $(JOBS)

clean:
	@rm -rf $(BUILD)

# The zip holds one directory, braam-sdk-<version>/. Its entries carry the pack
# time rather than now, so the toolchain file is stamped after unpacking.
$(TOOLCHAIN):
	@mkdir -p $(BUILD)
	@echo "fetching braam-sdk-$(SDK_VERSION)"
	@curl -fsSL -o $(BUILD)/braam-sdk-$(SDK_VERSION).zip $(SDK_URL)
	@unzip -q -o -d $(BUILD) $(BUILD)/braam-sdk-$(SDK_VERSION).zip
	@touch $@

# The toolchain file is named on this first configure and only here: CMake
# fixes the compiler when a project is configured, and a build directory
# configured without it cannot be repaired by adding the flag.
$(BUILD)/CMakeCache.txt: $(TOOLCHAIN)
	@cmake -B $(BUILD) -G "$(GENERATOR)" \
	    -DCMAKE_TOOLCHAIN_FILE=$(abspath $(TOOLCHAIN)) $(CMAKE_ARGS)
