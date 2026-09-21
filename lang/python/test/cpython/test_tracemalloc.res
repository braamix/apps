sssssssss......s.............FFFFF....s.F.FF...F...
======================================================================
FAIL: test_snapshot_group_by_cumulative (__main__.TestSnapshot.test_snapshot_group_by_cumulative)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 613, in test_snapshot_group_by_cumulative
    self.assertEqual(stats, [
AssertionError: Lists differ: [<Sta[65 chars]size=66 count=1>, <Statistic traceback=<Traceb[317 chars]t=1>] != [<Sta[65 chars]size=98 count=5>, <Statistic traceback=<Traceb[144 chars]t=1>]

First differing element 0:
<Stat[17 chars]Traceback (<Frame filename='b.py' lineno=0>,)> size=66 count=1>
<Stat[17 chars]Traceback (<Frame filename='b.py' lineno=0>,)> size=98 count=5>

First list contains 2 additional elements.
First extra element 3:
<Statistic traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=7 count=1>

- [<Statistic traceback=<Traceback (<Frame filename='b.py' lineno=0>,)> size=66 count=1>,
?                                                                            ^^       ^

+ [<Statistic traceback=<Traceback (<Frame filename='b.py' lineno=0>,)> size=98 count=5>,
?                                                                            ^^       ^

-  <Statistic traceback=<Traceback (<Frame filename='b.py' lineno=0>,)> size=32 count=4>,
?                                                    ^

+  <Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=32 count=4>,
?                                                    ^

-  <Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=30 count=3>,
-  <Statistic traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=7 count=1>,
?                                                                                           ^

+  <Statistic traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=7 count=1>]
?                                                                                           ^

-  <Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=2 count=1>]

======================================================================
FAIL: test_snapshot_group_by_file (__main__.TestSnapshot.test_snapshot_group_by_file)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 541, in test_snapshot_group_by_file
    self.assertEqual(stats1, [
AssertionError: Lists differ: [<Sta[153 chars]ize=30 count=3>, <Statistic traceback=<Traceba[142 chars]t=1>] != [<Sta[153 chars]ize=32 count=4>, <Statistic traceback=<Traceba[56 chars]t=1>]

First differing element 1:
<Stat[17 chars]Traceback (<Frame filename='a.py' lineno=0>,)> size=30 count=3>
<Stat[17 chars]Traceback (<Frame filename='a.py' lineno=0>,)> size=32 count=4>

First list contains 1 additional elements.
First extra element 3:
<Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=2 count=1>

  [<Statistic traceback=<Traceback (<Frame filename='b.py' lineno=0>,)> size=66 count=1>,
-  <Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=30 count=3>,
?                                                                             ^       ^

+  <Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=32 count=4>,
?                                                                             ^       ^

-  <Statistic traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=7 count=1>,
?                                                                                           ^

+  <Statistic traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=7 count=1>]
?                                                                                           ^

-  <Statistic traceback=<Traceback (<Frame filename='a.py' lineno=0>,)> size=2 count=1>]

======================================================================
FAIL: test_snapshot_group_by_line (__main__.TestSnapshot.test_snapshot_group_by_line)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 524, in test_snapshot_group_by_line
    self.assertEqual(statistics, [
AssertionError: Lists differ: [<Sta[79 chars](+5002) count=2 (+2)>, <StatisticDiff tracebac[586 chars]-1)>] != [<Sta[79 chars](+5000) count=2 (+1)>, <StatisticDiff tracebac[384 chars]+0)>]

First differing element 0:
<Stat[36 chars]me filename='a.py' lineno=5>,)> size=5002 (+5002) count=2 (+2)>
<Stat[36 chars]me filename='a.py' lineno=5>,)> size=5002 (+5000) count=2 (+1)>

First list contains 2 additional elements.
First extra element 5:
<StatisticDiff traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=0 (-7) count=0 (-1)>

- [<StatisticDiff traceback=<Traceback (<Frame filename='a.py' lineno=5>,)> size=5002 (+5002) count=2 (+2)>,
?                                                                                          ^            ^

+ [<StatisticDiff traceback=<Traceback (<Frame filename='a.py' lineno=5>,)> size=5002 (+5000) count=2 (+1)>,
?                                                                                          ^            ^

   <StatisticDiff traceback=<Traceback (<Frame filename='c.py' lineno=578>,)> size=400 (+400) count=1 (+1)>,
   <StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=1>,)> size=0 (-66) count=0 (-1)>,
-  <StatisticDiff traceback=<Traceback (<Frame filename='a.py' lineno=2>,)> size=30 (+30) count=3 (+3)>,
-  <StatisticDiff traceback=<Traceback (<Frame filename='a.py' lineno=2>,)> size=0 (-30) count=0 (-3)>,
   <StatisticDiff traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=0 (-7) count=0 (-1)>,
-  <StatisticDiff traceback=<Traceback (<Frame filename='a.py' lineno=5>,)> size=0 (-2) count=0 (-1)>]
?                                                                     ^             ^^         ----

+  <StatisticDiff traceback=<Traceback (<Frame filename='a.py' lineno=2>,)> size=30 (+0) count=3 (+0)>]
?                                                                     ^          +   ^^        ++++


======================================================================
FAIL: test_snapshot_group_by_traceback (__main__.TestSnapshot.test_snapshot_group_by_traceback)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 590, in test_snapshot_group_by_traceback
    self.assertEqual(diff, [
AssertionError: Lists differ: [<Sta[112 chars](+5002) count=2 (+2)>, <StatisticDiff tracebac[685 chars]-1)>] != [<Sta[112 chars](+5000) count=2 (+1)>, <StatisticDiff tracebac[417 chars]+0)>]

First differing element 0:
<Stat[69 chars]ame filename='a.py' lineno=5>)> size=5002 (+5002) count=2 (+2)>
<Stat[69 chars]ame filename='a.py' lineno=5>)> size=5002 (+5000) count=2 (+1)>

First list contains 2 additional elements.
First extra element 5:
<StatisticDiff traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=0 (-7) count=0 (-1)>

- [<StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=4>, <Frame filename='a.py' lineno=5>)> size=5002 (+5002) count=2 (+2)>,
?                                                                                                                           ^            ^

+ [<StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=4>, <Frame filename='a.py' lineno=5>)> size=5002 (+5000) count=2 (+1)>,
?                                                                                                                           ^            ^

   <StatisticDiff traceback=<Traceback (<Frame filename='c.py' lineno=578>,)> size=400 (+400) count=1 (+1)>,
   <StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=1>,)> size=0 (-66) count=0 (-1)>,
-  <StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=4>, <Frame filename='a.py' lineno=2>)> size=30 (+30) count=3 (+3)>,
-  <StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=4>, <Frame filename='a.py' lineno=2>)> size=0 (-30) count=0 (-3)>,
   <StatisticDiff traceback=<Traceback (<Frame filename='<unknown>' lineno=0>,)> size=0 (-7) count=0 (-1)>,
-  <StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=4>, <Frame filename='a.py' lineno=5>)> size=0 (-2) count=0 (-1)>]
?                                                                                                       ^            ^^         ----

+  <StatisticDiff traceback=<Traceback (<Frame filename='b.py' lineno=4>, <Frame filename='a.py' lineno=2>)> size=30 (+0) count=3 (+0)>]
?                                                                                                       ^         +   ^^        ++++


======================================================================
FAIL: test_statistic_diff_format (__main__.TestSnapshot.test_statistic_diff_format)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 649, in test_statistic_diff_format
    self.assertEqual(str(stat),
AssertionError: 'a.py:5: size=5002 B (+5002 B), count=2 (+2), average=2501 B' != 'a.py:5: size=5002 B (+5000 B), count=2 (+1), average=2501 B'
- a.py:5: size=5002 B (+5002 B), count=2 (+2), average=2501 B
?                          ^               ^
+ a.py:5: size=5002 B (+5000 B), count=2 (+1), average=2501 B
?                          ^               ^


======================================================================
FAIL: test_get_traced_memory (__main__.TestTracemallocEnabled.test_get_traced_memory)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 251, in test_get_traced_memory
    self.assertLess(size2, size)
AssertionError: 1051872 not less than 1050496

======================================================================
FAIL: test_get_traces (__main__.TestTracemallocEnabled.test_get_traces)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 195, in test_get_traces
    trace = self.find_trace(traces, obj_traceback, obj_size)
  File "/tmp/test_tracemalloc.py", line 187, in find_trace
    self.fail("trace not found")
AssertionError: trace not found

======================================================================
FAIL: test_get_traces_intern_traceback (__main__.TestTracemallocEnabled.test_get_traces_intern_traceback)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 226, in test_get_traces_intern_traceback
    trace1 = self.find_trace(traces, obj1_traceback, obj1_size)
  File "/tmp/test_tracemalloc.py", line 187, in find_trace
    self.fail("trace not found")
AssertionError: trace not found

======================================================================
FAIL: test_reset_peak (__main__.TestTracemallocEnabled.test_reset_peak)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/tmp/test_tracemalloc.py", line 290, in test_reset_peak
    self.assertLess(peak2, peak1)
AssertionError: 1717408 not less than 1717280

----------------------------------------------------------------------
Ran 51 tests in Ns

FAILED (failures=9, skipped=11)
