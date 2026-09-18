// Where the tests find the SDK's harness and the kernel and rootfs it boots.
// `make` fetches the SDK into build/braam-sdk-<SDK_VERSION>/; BRAAM_SDK names
// another, such as one installed from a local core build.

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const APPS = resolve(dirname(fileURLToPath(import.meta.url)), "..");

// The Makefile names the version, and nothing else does.
function version() {
    const m = /^SDK_VERSION\s*:?=\s*(\S+)/m.exec(readFileSync(resolve(APPS, "Makefile"), "utf8"));
    if (!m) throw new Error("no SDK_VERSION in the Makefile");
    return m[1];
}

export const SDK = process.env.BRAAM_SDK
    ? resolve(process.env.BRAAM_SDK)
    : resolve(APPS, "build", "braam-sdk-" + version());

// share/braam holds test/ and web/ as core's tree does.
export const SHARE = join(SDK, "share/braam");
export const HARNESS = join(SHARE, "test/system/harness.mjs");
export const KERNEL = join(SHARE, "web/kernel.wasm");
export const ROOTFS = join(SHARE, "web/rootfs.zip");
