# Convenience wrapper over CMake. The build system proper is CMakeLists.txt.
# Override with GENERATOR=Ninja, JOBS=1, BUILD=<dir>, SDK=<prefix>.

# The SDK this tree builds against. Move it with each Braam release, here and
# in README.md. A binary stamped for another process ABI is refused at exec.
SDK_RELEASE := v0.5
SDK_VERSION := 0.5.172-e9a2981
SDK_URL := https://github.com/braamix/core/releases/download/$(SDK_RELEASE)/braam-sdk-$(SDK_VERSION).zip

BUILD     ?= build
GENERATOR ?= Unix Makefiles

# Fetched into the build directory, unless SDK names one already unpacked.
SDK       ?= $(BUILD)/braam-sdk-$(SDK_VERSION)
TOOLCHAIN := $(SDK)/lib/cmake/braam/wasm32-unknown-unknown.cmake

# make's own -jN cannot reach the generated build: its jobserver descriptors do
# not survive the cmake process in between. Pass a count explicitly instead.
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Publishing. The repository the index is for, which must equal the client's
# /etc/repositories line byte for byte, and the index's own two numbers.
REPO_URL      ?= https://braamix.github.io

# G. A client refuses an index whose version is below the one it holds, so this
# rises at every publication. It cannot be derived: only the publisher knows
# what was last uploaded.
INDEX_VERSION ?= 5

# E, milliseconds since the epoch: 2027-08-21. A promise to re-sign by then.
INDEX_EXPIRY  ?= 1818806400000
INDEX_DESC    ?= Braam applications

# The publisher's own key, outside this tree and never copied into it. Its
# public half has to be the K:index of the anchor the client boots with.
INDEX_KEY     ?= $(HOME)/.ssh/braam/index.key

# The SDK ships it, in libexec beside mkpkg.py.
MKINDEX ?= $(firstword $(wildcard $(SDK)/libexec/braam/mkindex.py))

REPO := $(BUILD)/repo

.PHONY: all package test index clean

all: $(BUILD)/CMakeCache.txt
	@cmake --build $(BUILD) -j $(JOBS)

# The zips /bin/pkg installs, one per program. `packages` rather than
# `package`, which CPack claims.
package: all
	@cmake --build $(BUILD) -j $(JOBS) --target packages

# Headless tests, driving a built binary under ../braam-core's smoke harness.
# Needs node and a built core tree.
test: all
	@node games/adventure/test/play.mjs
	@node games/adventure/test/interrupt.mjs
	@node games/adventure/test/suspend.mjs
	@node games/adventure/test/back.mjs
	@node benchmarks/dhrystone/test/interrupt.mjs
	@node benchmarks/duremark/test/interrupt.mjs

# The repository to upload: the signed index and the zips it vouches for, in
# one directory, because a package's URL is derived from the index's own N.
index: package
	@test -n "$(MKINDEX)" || \
	    { echo "no mkindex.py in $(SDK)"; exit 1; }
	@test -r "$(INDEX_KEY)" || { echo "cannot read $(INDEX_KEY)"; exit 1; }
	@rm -rf $(REPO) && mkdir -p $(REPO)
	@# A package zip is $(BUILD)/<category>/<program>/<name>-<version>.zip —
	@# depth three, which the SDK's own zip beside it is not.
	@find $(BUILD) -mindepth 3 -maxdepth 3 -name '*-*.zip' \
	    -not -path '$(SDK)/*' -exec cp {} $(REPO)/ \;
	@python3 $(MKINDEX) --out $(REPO)/index --url $(REPO_URL) \
	    --version $(INDEX_VERSION) --expiry $(INDEX_EXPIRY) \
	    --description '$(INDEX_DESC)' --sign $(INDEX_KEY) $(REPO)/*.zip
	@echo "$(REPO): index $(INDEX_VERSION), `ls $(REPO)/*.zip | wc -l | tr -d ' '` package(s)"

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
