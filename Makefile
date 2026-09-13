# Convenience wrapper over CMake. The build system proper is CMakeLists.txt.
# Override with GENERATOR=Ninja, JOBS=1, BUILD=<dir>, SDK=<prefix>.

# The SDK this tree builds against. Move it with each Braam release, here and
# in README.md. A binary stamped for another process ABI is refused at exec.
SDK_RELEASE := v0.9
SDK_VERSION := 0.9.260-f1000cf
SDK_URL := https://github.com/braamix/core/releases/download/$(SDK_RELEASE)/braam-sdk-$(SDK_VERSION).zip

BUILD     ?= build
GENERATOR ?= Unix Makefiles

# Where `make test` keeps what it printed.
TEST_LOG  ?= test.log

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
INDEX_VERSION ?= 48

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

# Headless tests, driving a built binary under ../braam-core's system harness.
# Needs node and a built core tree.
TESTS := \
    archivers/zip/test/roundtrip.mjs \
    archivers/zip/test/tree.mjs \
    archivers/zip/test/interrupt.mjs \
    archivers/zip/test/update.mjs \
    archivers/zip/test/tools.mjs \
    editors/eh/test/ehcases.mjs \
    editors/eh/test/ehreplace.mjs \
    editors/eh/test/ehtab.mjs \
    games/adventure/test/play.mjs \
    games/adventure/test/interrupt.mjs \
    games/adventure/test/suspend.mjs \
    games/adventure/test/back.mjs \
    games/asciifluid/test/frames.mjs \
    games/asciifluid/test/colour.mjs \
    games/asciifluid/test/interrupt.mjs \
    games/asciiquarium/test/frames.mjs \
    games/asciiquarium/test/colour.mjs \
    games/asciiquarium/test/keys.mjs \
    games/asciiquarium/test/resize.mjs \
    games/asciiquarium/test/interrupt.mjs \
    editors/uemacs/test/emkeys.mjs \
    editors/uemacs/test/emedit.mjs \
    editors/uemacs/test/emfiles.mjs \
    editors/uemacs/test/emsearch.mjs \
    editors/uemacs/test/emmacro.mjs \
    editors/uemacs/test/emwindow.mjs \
    editors/uemacs/test/embang.mjs \
    editors/le/test/leedit.mjs \
    editors/le/test/leblock.mjs \
    editors/le/test/lesearch.mjs \
    editors/le/test/lesigint.mjs \
    editors/le/test/lescreen.mjs \
    editors/le/test/leresize.mjs \
    editors/le/test/lecolor.mjs \
    editors/le/test/leescape.mjs \
    editors/le/test/lesyntax.mjs \
    editors/le/test/lespawn.mjs \
    editors/le/test/lesession.mjs \
    editors/le/test/ledata.mjs \
    editors/vi/test/exscript.mjs \
    editors/vi/test/exerrors.mjs \
    editors/vi/test/exregex.mjs \
    editors/vi/test/exfiles.mjs \
    editors/vi/test/exbang.mjs \
    editors/vi/test/vikeys.mjs \
    editors/vi/test/viinsert.mjs \
    editors/vi/test/vikeypad.mjs \
    editors/vi/test/viresize.mjs \
    editors/vi/test/viutf8.mjs \
    editors/vi/test/vibang.mjs \
    converters/iconv/test/smoke.mjs \
    converters/iconv/test/convert.mjs \
    converters/iconv/test/errors.mjs \
    benchmarks/dhrystone/test/interrupt.mjs \
    benchmarks/duremark/test/interrupt.mjs \
    emulators/simbesm/test/boot.mjs \
    lang/mbasic/test/repl.mjs \
    lang/mbasic/test/numbers.mjs \
    lang/mbasic/test/errors.mjs \
    lang/mbasic/test/files.mjs \
    lang/mbasic/test/renum.mjs \
    lang/mbasic/test/examples.mjs \
    lang/mbasic/test/case.mjs \
    lang/mbasic/test/words.mjs \
    lang/mbasic/test/utf8.mjs \
    lang/mbasic/test/script.mjs \
    lang/mbasic/test/interrupt.mjs

# Every run is teed into $(TEST_LOG) as well as the terminal, so the output can
# be read again -- or read a second way -- without running the suite twice. The
# status travels through a file because a pipeline's is tee's, not node's, and
# `set -o pipefail` is not in every /bin/sh.
test: all
	@: > $(TEST_LOG)
	@for t in $(TESTS); do \
	    { node $$t 2>&1; echo $$? > $(BUILD)/.teststatus; } | tee -a $(TEST_LOG); \
	    read st < $(BUILD)/.teststatus; \
	    [ "$$st" = 0 ] || exit "$$st"; \
	done
	@echo "$(TEST_LOG): `wc -l < $(TEST_LOG) | tr -d ' '` lines"

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
	@rm -rf $(BUILD) $(TEST_LOG)

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
