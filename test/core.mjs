// Where the tests find core: its harness, kernel and rootfs. `make core`
// fetches them into build/braam-<SDK_VERSION>/, laid out as a core tree;
// BRAAM_CORE names another, such as a local ../braam-core build.

import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "..");

// The Makefile names the version, and nothing else does.
function version() {
    const m = /^SDK_VERSION\s*:?=\s*(\S+)/m.exec(readFileSync(resolve(APPS, "Makefile"), "utf8"));
    if (!m) throw new Error("no SDK_VERSION in the Makefile");
    return m[1];
}

export const CORE = process.env.BRAAM_CORE
    ? resolve(process.env.BRAAM_CORE)
    : resolve(APPS, "build", "braam-" + version());
