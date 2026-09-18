# queue: the four queues, one thread, and what waiting answers there.
import queue

q = queue.Queue(maxsize=2)
q.put(1)
q.put_nowait(2)
print(q.qsize(), q.full(), q.empty())
try:
    q.put_nowait(3)
except queue.Full:
    print("Full")
try:
    q.put(3, timeout=0)
except queue.Full:
    print("Full after timeout")
print(q.get(), q.get_nowait(), q.empty())
try:
    q.get_nowait()
except queue.Empty:
    print("Empty")
try:
    q.get(timeout=0)
except queue.Empty:
    print("Empty after timeout")
q.put("x")
q.get()
for _ in range(3):
    q.task_done()
q.join()
try:
    q.task_done()
except ValueError as e:
    print("ValueError", e)
lq = queue.LifoQueue()
for i in range(3):
    lq.put(i)
print([lq.get() for _ in range(3)])
pq = queue.PriorityQueue()
for p in [(3, "c"), (1, "a"), (2, "b")]:
    pq.put(p)
print([pq.get()[1] for _ in range(3)])
sq = queue.SimpleQueue()
sq.put(1)
sq.put(2)
print(sq.qsize(), sq.get(), sq.get(block=False), sq.empty())
try:
    sq.get(timeout=0)
except queue.Empty:
    print("SimpleQueue Empty")
q.shutdown()
try:
    q.put(1)
except queue.ShutDown:
    print("ShutDown")
print(type(queue.Queue[int]), issubclass(queue.Full, Exception))
