# argparse: options, positionals, subcommands, groups, help and errors.
import argparse
import io
import sys

p = argparse.ArgumentParser(prog="demo", description="A demo.", epilog="That is all.")
p.add_argument("files", nargs="*", help="input files")
p.add_argument("-v", "--verbose", action="count", default=0, help="say more")
p.add_argument("--mode", choices=["a", "b"], default="a", help="the mode (default: %(default)s)")
p.add_argument("-n", type=int, metavar="N", help="how many")
p.add_argument("--ratio", type=float, default=0.5)
p.add_argument("--tag", action="append", dest="tags")
p.add_argument("--no-color", action="store_false", dest="color")
p.add_argument("--version", action="version", version="%(prog)s 1.0")
print(p.parse_args(["-vv", "x", "y", "--mode", "b", "-n", "3", "--tag", "t1", "--tag=t2"]))
print(p.parse_args([]))
print(p.parse_args(["--no-color", "--ratio", "2.5", "--", "-notanoption"]))
print(p.format_usage(), end="")
print(p.format_help())

out = io.StringIO()
sys.stderr, saved = out, sys.stderr
for argv in (["--mode", "c"], ["-n", "x"], ["--bogus"], ["-n"]):
    try:
        p.parse_args(argv)
    except SystemExit as e:
        print("exit", e.code)
sys.stderr = saved
print(out.getvalue(), end="")

out = io.StringIO()
sys.stdout, saved = out, sys.stdout
try:
    p.parse_args(["--version"])
except SystemExit as e:
    code = e.code
sys.stdout = saved
print(repr(out.getvalue()), code)

q = argparse.ArgumentParser(prog="tool", exit_on_error=False)
q.add_argument("--level", type=int, default=1)
sub = q.add_subparsers(dest="cmd", required=True, title="commands")
run = sub.add_parser("run", help="run it", aliases=["r"])
run.add_argument("--fast", action="store_true")
run.add_argument("target")
show = sub.add_parser("show", help="show it")
show.add_argument("what", choices=["all", "some"], nargs="?", default="all")
show.set_defaults(func="shower")
g = q.add_mutually_exclusive_group()
g.add_argument("--on", action="store_true")
g.add_argument("--off", action="store_true")
print(q.parse_args(["--on", "run", "--fast", "here"]))
print(q.parse_args(["r", "there"]))
print(q.parse_args(["--level", "4", "show"]))
for argv in (["--on", "--off", "run", "x"], ["--level", "z", "show"]):
    try:
        q.parse_args(argv)
    except argparse.ArgumentError as e:
        print("error:", e, "|", e.argument_name)
print(q.format_help())
print(run.format_help())
ns, rest = q.parse_known_args(["show", "some", "--zzz", "tail"])
print(ns, rest)
m = argparse.ArgumentParser(prog="m")
m.add_argument("--foo")
m.add_argument("cmd")
m.add_argument("rest", nargs="*", type=int)
print(m.parse_known_intermixed_args(["doit", "1", "--foo", "bar", "2", "3", "--zzz"]))

r = argparse.ArgumentParser(prog="r", fromfile_prefix_chars="@", allow_abbrev=True,
                            formatter_class=argparse.RawDescriptionHelpFormatter,
                            description="  keep\n    this layout")
r.add_argument("--verbosity", type=int)
r.add_argument("pair", nargs=2, type=str.upper)
r.add_argument("rest", nargs=argparse.REMAINDER)
print(r.parse_args(["--verb", "2", "a", "b", "c", "--d"]))
print(r.format_help())
opt = r.add_argument_group("extra", "extra options")
opt.add_argument("--flag", action=argparse.BooleanOptionalAction, default=False)
print(r.parse_args(["x", "y", "--no-flag"]).flag, r.parse_args(["--flag", "x", "y"]).flag)
print(vars(argparse.Namespace(a=1, b="two")), argparse.Namespace(a=1) == argparse.Namespace(a=1))
print("a" in argparse.Namespace(a=1))

d = argparse.ArgumentParser(prog="d", argument_default=argparse.SUPPRESS, add_help=False)
d.add_argument("--x")
d.add_argument("-y", required=True)
print(d.parse_args(["-y", "1"]))
try:
    argparse.ArgumentParser(prog="e", exit_on_error=False).add_argument("--x", "--x")
except argparse.ArgumentError as e:
    print(e)
print(argparse.ArgumentDefaultsHelpFormatter("p").__class__.__name__)
f = argparse.ArgumentParser(prog="f", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
f.add_argument("--size", type=int, default=10, help="the size")
f.add_argument("inputs", nargs="+", metavar="IN", help="some inputs")
print(f.format_help())
