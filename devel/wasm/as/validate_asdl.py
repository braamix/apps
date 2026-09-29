#!/usr/bin/env python3
#
# Validate ASDL file.
#
# Every field's type must be defined in the file, be one of ASDL's own
# (identifier, string, bytes, int, bool, object, constant, singleton), or
# be named with -p. No type or constructor may be defined twice, and every
# type must be reachable from the first. Exits 1 on the first file that
# fails. With -j, a valid file is written out as JSON instead: each type
# with its fields, or its constructors and theirs, and its attributes.
#
# Needs the Python package pyasdl.
#
import argparse
import json
import sys

import pyasdl

BUILTINS = {"identifier", "string", "bytes", "int", "bool", "object", "constant", "singleton"}


def strip_asdl_comments(text):
    lines = []
    for line in text.splitlines():
        idx = line.find('--')
        if idx >= 0:
            line = line[:idx]
        lines.append(line)
    return '\n'.join(lines)


def fields_of(type_def):
    """Yield (where, field) for every field and attribute of a type."""
    value = type_def.value
    for field in value.attributes:
        yield type_def.name, field
    if isinstance(value, pyasdl.Sum):
        for cons in value.types:
            for field in cons.fields:
                yield f"{type_def.name}.{cons.name}", field
    else:
        for field in value.fields:
            yield type_def.name, field


def as_json(schema):
    """The schema as plain data: {type: {fields} or {constructors}, attributes}."""
    def fields(fs):
        quals = {None: "", pyasdl.FieldQualifier.OPTIONAL: "?",
                 pyasdl.FieldQualifier.SEQUENCE: "*"}
        return [{"name": f.name, "type": f.kind, "qualifier": quals[f.qualifier]} for f in fs]
    types = {}
    for t in schema.body:
        v = t.value
        if isinstance(v, pyasdl.Sum):
            types[t.name] = {"constructors": {c.name: fields(c.fields) for c in v.types},
                             "attributes": fields(v.attributes)}
        else:
            types[t.name] = {"fields": fields(v.fields), "attributes": fields(v.attributes)}
    return {"module": schema.name, "types": types}


def validate_asdl_file(file_path, primitives, dump=False):
    try:
        with open(file_path, 'r') as f:
            schema = pyasdl.parse(strip_asdl_comments(f.read()))
    except SyntaxError as e:
        return [f"syntax error: {e}"]
    except OSError as e:
        return [str(e)]

    errors = []
    type_defs = schema.body
    if not type_defs:
        return ["no type definitions"]

    # Each type and each constructor defined once.
    seen = set()
    for type_def in type_defs:
        if type_def.name in seen:
            errors.append(f"type '{type_def.name}' defined twice")
        seen.add(type_def.name)
    owner = {}
    for type_def in type_defs:
        if isinstance(type_def.value, pyasdl.Sum):
            for cons in type_def.value.types:
                if cons.name in owner:
                    errors.append(f"constructor '{cons.name}' in both "
                                  f"'{owner[cons.name]}' and '{type_def.name}'")
                owner[cons.name] = type_def.name

    # Every field's type defined.
    known = seen | BUILTINS | primitives
    for type_def in type_defs:
        for where, field in fields_of(type_def):
            if field.kind not in known:
                errors.append(f"'{where}' field '{field.name}' has undefined "
                              f"type '{field.kind}'")

    # Every type reachable from the first.
    by_name = {t.name: t for t in type_defs}
    reached, todo = set(), [type_defs[0].name]
    while todo:
        name = todo.pop()
        if name in reached or name not in by_name:
            continue
        reached.add(name)
        todo.extend(field.kind for _, field in fields_of(by_name[name]))
    for type_def in type_defs:
        if type_def.name not in reached:
            errors.append(f"type '{type_def.name}' is not reachable "
                          f"from '{type_defs[0].name}'")

    if not errors and dump:
        print(json.dumps(as_json(schema), indent=1))
    elif not errors:
        print(f"{file_path}: module {schema.name}, {len(type_defs)} types, "
              f"{len(owner)} constructors: valid")
    return errors


def main():
    parser = argparse.ArgumentParser(description="Validate ASDL files.")
    parser.add_argument("-p", "--primitives", default="",
                        help="comma-separated primitive types beyond ASDL's own")
    parser.add_argument("-j", "--json", action="store_true",
                        help="write a valid file out as JSON")
    parser.add_argument("files", nargs="+", metavar="file.asdl")
    args = parser.parse_args()
    primitives = {p for p in args.primitives.split(",") if p}

    status = 0
    for file_path in args.files:
        for error in validate_asdl_file(file_path, primitives, args.json):
            print(f"{file_path}: {error}", file=sys.stderr)
            status = 1
    sys.exit(status)


main()
