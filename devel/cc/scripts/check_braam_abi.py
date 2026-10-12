#!/usr/bin/env python3
"""Compare the Braam ABI numbers of vcc's headers with braam-core's.

    check_braam_abi.py braam.h sysabi.h result.h errno.h

braam.h's BRAAM_PROC_* and BRAAM_SYS_* against sysabi.h's constants and its enum Sys
(implicit values counted), BRAAM_O_* against SYS_O_*, BRAAM_KIND_*, BRAAM_SIG_* and BRAAM_POLL_* against
SYS_KIND_*, SIG_* and SYS_POLL_*, BRAAM_CHUNK and
BRAAM_STAT_NOFOLLOW against theirs;
errno.h's Braam errors (Error + 32) against result.h's enum Error.  Prints each
disagreement and exits 1 if there is one.
"""
import re
import sys


def defines(text):
    return {m.group(1): int(m.group(2), 0)
            for m in re.finditer(r"#define\s+(\w+)\s+(0x[0-9a-fA-F]+|\d+)\b", text)}


def enum(text, name):
    body = re.search(r"enum class " + name + r"\s*:\s*\w+\s*\{(.*?)\};", text, re.S).group(1)
    body = re.sub(r"//[^\n]*", "", body)
    values, n = {}, -1
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        m = re.match(r"(\w+)\s*(?:=\s*(\d+))?$", item)
        n = int(m.group(2)) if m.group(2) else n + 1
        values[m.group(1)] = n
    return values


def main(braam_h, sysabi_h, result_h, errno_h):
    ours = defines(open(braam_h).read())
    sysabi = open(sysabi_h).read()
    theirs = {m.group(1): int(m.group(2), 0)
              for m in re.finditer(r"constexpr (?:u32|usize) (\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+);",
                                   sysabi)}
    theirs.update({m.group(1): int(m.group(2), 0)
                   for m in re.finditer(r"\b(SYS_POLL_\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+),", sysabi)})
    bad = []

    def check(what, ours_v, theirs_v):
        if ours_v != theirs_v:
            bad.append(f"{what}: ours {ours_v}, braam-core {theirs_v}")

    for name in ("MAGIC", "ABI", "PAGE", "MAX_PAGES"):
        check("BRAAM_PROC_" + name, ours.get("BRAAM_PROC_" + name), theirs.get("PROC_" + name))
    sys_ops = enum(sysabi, "Sys")
    for name, value in ours.items():
        if name.startswith("BRAAM_SYS_") and name != "BRAAM_SYS_OP":
            want = {k.upper(): v for k, v in sys_ops.items()}.get(name[len("BRAAM_SYS_"):])
            check(name, value, want)
        if name.startswith("BRAAM_O_"):
            check(name, value, theirs.get("SYS_O_" + name[len("BRAAM_O_"):]))
        if name.startswith("BRAAM_SIG_"):
            check(name, value, theirs.get("SIG_" + name[len("BRAAM_SIG_"):]))
        if name.startswith("BRAAM_POLL_"):
            check(name, value, theirs.get("SYS_POLL_" + name[len("BRAAM_POLL_"):]))
        if name.startswith("BRAAM_KIND_"):
            check(name, value, theirs.get("SYS_KIND_" + name[len("BRAAM_KIND_"):]))
    check("BRAAM_CHUNK", ours.get("BRAAM_CHUNK"), theirs.get("SYS_CHUNK"))
    check("BRAAM_STAT_NOFOLLOW", ours.get("BRAAM_STAT_NOFOLLOW"), theirs.get("SYS_STAT_NOFOLLOW"))

    errors = enum(open(result_h).read(), "Error")
    errno = open(errno_h).read()
    for m in re.finditer(r"#define\s+(E\w+)\s+(\d+)\s*/\*\s*(\w+)\s*\*/", errno):
        if int(m.group(2)) >= 32:
            check(m.group(1), int(m.group(2)) - 32, errors.get(m.group(3)))

    for b in bad:
        print(b)
    print(f"{len(bad)} disagreement(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:5]))
