# subprocess and the os calls under it: a child's output taken through one
# pipe, its status, its environment and its errors. Never two pipes at once,
# which this interpreter cannot wait on. Every write is flushed before a child
# that shares stdout runs, so the order is the same in a file as on a screen.
import os
import subprocess
import sys


def say(*args, end="\n"):
    print(*args, end=end, flush=True)


say(subprocess.run(["echo", "hello"]).returncode)
say(subprocess.run(["echo", "x"], stdout=subprocess.PIPE))
say(subprocess.check_output(["echo", "out"]))
say(subprocess.check_output(["sh", "-c", "echo two >&2"], stderr=subprocess.STDOUT))
say(subprocess.check_output("echo shell; exit 0", shell=True, text=True), end="")
say(subprocess.run(["cat"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE).stdout)
say(subprocess.run(["cat"], input=b"through cat\n").returncode)
say(subprocess.call(["false"]), subprocess.call(["sh", "-c", "exit 7"]))

try:
    subprocess.check_call(["false"])
except subprocess.CalledProcessError as e:
    say(e)
try:
    subprocess.run(["no-such-program"])
except FileNotFoundError as e:
    say(type(e).__name__, e.errno, e.filename)

say(subprocess.check_output("echo $FOO", shell=True, env={"FOO": "bar"}))
os.environ["BAZ"] = "qux"
say(subprocess.check_output("echo $BAZ", shell=True))
say(subprocess.getoutput("echo got"))
say(subprocess.getstatusoutput("echo st; exit 2"))
say(repr(os.popen("echo popen").read()))
say(subprocess.check_output([sys.executable, "-c", "print(6 * 7)"]))

with subprocess.Popen(["sh", "-c", "echo a; echo b"], stdout=subprocess.PIPE, text=True) as p:
    say([line for line in p.stdout])
say(p.returncode)

p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
say("poll", p.poll())
p.stdin.close()
say("wait", p.wait(timeout=60), p.poll())

# os
say(os.system("echo from system"), os.system("exit 3"))
st = os.system("exit 5")
say(os.WIFEXITED(st), os.WEXITSTATUS(st), os.WIFSIGNALED(st), os.waitstatus_to_exitcode(st))

r, w = os.pipe()
pid = os.posix_spawn("/bin/echo", ["echo", "spawned"], os.environ,
                     file_actions=[(os.POSIX_SPAWN_DUP2, w, 1), (os.POSIX_SPAWN_CLOSE, r)])
os.close(w)
say(os.read(r, 100))
os.close(r)
got, st = os.waitpid(pid, 0)
say(got == pid, st)
try:
    os.waitpid(pid, 0)
except ChildProcessError as e:
    say(type(e).__name__, e.errno)

pid = os.posix_spawnp("sh", ["sh", "-c", "exit 4"], {"PATH": "/bin"})
say(os.waitstatus_to_exitcode(os.waitpid(pid, 0)[1]))
