# sched: events run in time order, then priority, on a clock of our own.
import sched


class Clock:
    def __init__(self):
        self.now = 0.0

    def time(self):
        return self.now

    def sleep(self, d):
        print("sleep", d)
        self.now += d


c = Clock()
s = sched.scheduler(c.time, c.sleep)
log = []
s.enter(5, 1, log.append, ("five",))
e = s.enter(2, 1, log.append, ("two",))
s.enterabs(3, 2, log.append, argument=("three-lo",))
s.enterabs(3, 1, lambda what: log.append(what), kwargs={"what": "three-hi"})
s.enter(9, 1, log.append, ("nine",))
print(len(s.queue), s.empty(), [ev.time for ev in s.queue])
s.cancel(e)
print(s.run(blocking=False), log)
s.run()
print(log, c.now, s.empty())
s.enter(1, 1, print, ("late",))
print(s.run(blocking=False))
try:
    s.cancel(e)
except ValueError as err:
    print("ValueError", err)
