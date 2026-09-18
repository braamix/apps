# cmd: a command loop over a file, help, completion and the hooks.
import cmd
import io


class Shell(cmd.Cmd):
    intro = "welcome"
    prompt = "(s) "

    def do_greet(self, arg):
        """greet NAME: say hello."""
        print("hello", arg or "nobody", file=self.stdout)

    def do_add(self, arg):
        print(sum(int(x) for x in arg.split()), file=self.stdout)

    def help_add(self):
        print("add numbers", file=self.stdout)

    def do_quit(self, arg):
        print("bye", file=self.stdout)
        return True

    def default(self, line):
        print("what:", line, file=self.stdout)

    def precmd(self, line):
        return line.strip()

    def postcmd(self, stop, line):
        if line:
            print("<" + line + ">", file=self.stdout)
        return stop

    def complete_greet(self, text, line, begidx, endidx):
        return [n for n in ("alice", "bob", "alan") if n.startswith(text)]


out = io.StringIO()
sh = Shell(stdin=io.StringIO("greet bob\nadd 1 2 3\n? add\nhelp greet\nhelp\nfoo bar\n\n!ls\nquit\nadd 9\n"),
           stdout=out)
sh.use_rawinput = False
sh.cmdloop()
print(out.getvalue())
print(sh.completenames("gr"), sh.complete_greet("al", "greet al", 6, 8), sh.lastcmd)
print(sh.parseline("add 1 2"), sh.parseline("?x"), sh.parseline("!y"), sh.parseline(""))
print(sh.onecmd("greet zed"), sh.get_names()[:3])
o2 = io.StringIO()
cmd.Cmd(stdout=o2).columnize(["one", "two", "three", "four", "five"], displaywidth=20)
print(o2.getvalue())
