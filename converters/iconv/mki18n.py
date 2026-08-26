#!/usr/bin/env python3
"""Build the whole i18n tree from data/.

    mki18n.py <datadir> <outdir> [<manifest>]

Produces <outdir>/csmapper and <outdir>/esdb: a .mps per mapping source, an
.esdb per encoding, the .646 tables copied, the four index texts copied and
their containers built beside them. This is what the package ships.

With a third argument it also writes the list of what it made, one path a line
relative to <outdir> — the package needs that list before the build runs.
"""

import sys
from pathlib import Path

import mkcsmapper
import mkdb
import mkesdb

# mapper.dir.<CODE>.src and charset.pivot.<CODE>.src are index fragments, not
# mapping tables; upstream's Makefiles concatenate them and mkcsmapper never
# sees one.
def is_mapping(path):
    return not (path.name.startswith("mapper.dir")
                or path.name.startswith("charset.pivot"))


def build_csmapper(data, out):
    n = 0
    for src in sorted(data.rglob("*.src")):
        if not is_mapping(src):
            continue
        rel = src.relative_to(data)
        dst = out / rel.parent / (src.stem + ".mps")
        dst.parent.mkdir(parents=True, exist_ok=True)
        try:
            dst.write_bytes(mkcsmapper.compile_src(src.read_text()))
        except mkcsmapper.Error as e:
            sys.exit(f"mki18n.py: {rel}: {e}")
        n += 1
    for f in sorted(data.rglob("*.646")):
        dst = out / f.relative_to(data)
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(f.read_bytes())
    return n


def build_esdb(data, out):
    n = 0
    for d in sorted(data.iterdir()):
        if not d.is_dir():
            continue
        spec = mkesdb.DIRS.get(d.name)
        if spec is None:
            sys.exit(f"mki18n.py: {d.name}: no expansion rule")
        sub = out / spec["subdir"]
        sub.mkdir(parents=True, exist_ok=True)
        try:
            items = mkesdb.expand(d.name, spec, d)
        except (mkesdb.Error, KeyError) as e:
            sys.exit(f"mki18n.py: {d.name}: {e}")
        for name, text in items:
            try:
                blob = mkesdb.compile_src(text)
            except mkesdb.Error as e:
                sys.exit(f"mki18n.py: {d.name}/{name}: {e}")
            (sub / (name.replace(":", "@") + ".esdb")).write_bytes(blob)
            n += 1
    return n


def indexes(data, out):
    # The four are checked in as text: upstream builds them from 33 BSD make
    # recipes, and they ship verbatim either way. Only the containers are made.
    for rel, kind in (("csmapper/mapper.dir", "lookup"),
                      ("csmapper/charset.pivot", "pivot"),
                      ("esdb/esdb.dir", "lookup"),
                      ("esdb/esdb.alias", "lookup")):
        src = data / rel
        dst = out / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(src.read_text())
        text = src.read_text()
        if kind == "lookup":
            dst.with_suffix(dst.suffix + ".db").write_bytes(mkdb.lookup(text))
        else:
            dst.with_name(dst.name + ".pvdb").write_bytes(mkdb.pivot(text))


def main(argv):
    if len(argv) not in (3, 4):
        sys.exit(__doc__)
    data, out = Path(argv[1]), Path(argv[2])
    out.mkdir(parents=True, exist_ok=True)
    m = build_csmapper(data / "csmapper", out / "csmapper")
    e = build_esdb(data / "esdb", out / "esdb")
    indexes(data, out)

    if len(argv) == 4:
        made = sorted(str(f.relative_to(out)) for f in out.rglob("*") if f.is_file())
        Path(argv[3]).write_text("\n".join(made) + "\n")

    print(f"i18n: {m} mappers, {e} encodings")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
