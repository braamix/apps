# getopt: short and long options, GNU order, and the errors.
import getopt

print(getopt.getopt(["-a", "-b", "val", "-cfoo", "rest", "-d"], "ab:c:d"))
print(getopt.getopt(["--alpha", "--beta=2", "--gam", "x", "a1"], "", ["alpha", "beta=", "gamma="]))
print(getopt.gnu_getopt(["a1", "-a", "a2", "--beta", "3", "a3"], "a", ["beta="]))
print(getopt.gnu_getopt(["a1", "--", "-a"], "a"))
print(getopt.getopt(["-a", "--", "-b"], "ab"))
print(getopt.getopt(["--opt"], "", ["opt=?"]), getopt.getopt(["--opt=v"], "", ["opt=?"]))
for args, short, long in [(["-x"], "a", []), (["-b"], "b:", []), (["--nope"], "", ["yes"]),
                          (["--al"], "", ["alpha", "alps"]), (["--yes=1"], "", ["yes"])]:
    try:
        getopt.getopt(args, short, long)
    except getopt.GetoptError as e:
        print(type(e).__name__, e.msg, e.opt, str(e))
print(getopt.error is getopt.GetoptError)
