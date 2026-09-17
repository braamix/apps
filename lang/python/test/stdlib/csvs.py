# csv over the native _csv: dialects, quoting, the reader and the writer.
import csv
import io

rows = [["name", "note"], ["a,b", 'say "hi"'], ["multi\nline", ""], [1, 2.5, None]]
out = io.StringIO()
w = csv.writer(out)
w.writerows(rows)
w.writerow(("x",))
print(repr(out.getvalue()))
print(list(csv.reader(io.StringIO(out.getvalue()))))

for q in (csv.QUOTE_ALL, csv.QUOTE_NONNUMERIC, csv.QUOTE_MINIMAL, csv.QUOTE_STRINGS, csv.QUOTE_NOTNULL):
    s = io.StringIO()
    csv.writer(s, quoting=q).writerow(["a", 1, 2.0, None, ""])
    print(q, repr(s.getvalue()))
s = io.StringIO()
csv.writer(s, quoting=csv.QUOTE_NONE, escapechar="\\").writerow(["a,b", 'c"d'])
print(repr(s.getvalue()))
try:
    csv.writer(io.StringIO(), quoting=csv.QUOTE_NONE).writerow(["a,b"])
except csv.Error as e:
    print(e)

print(list(csv.reader(["1,'2,3',4"], quotechar="'")))
print(list(csv.reader(['a;"b;c"'], delimiter=";")))
print(list(csv.reader(["  a,  b"], skipinitialspace=True)))
print(list(csv.reader(['1,"2",3'], quoting=csv.QUOTE_NONNUMERIC)))
print(list(csv.reader(['a,"b"c'])))
try:
    list(csv.reader(['a,"b"c'], strict=True))
except csv.Error as e:
    print(e)
try:
    list(csv.reader(['"open']))
except csv.Error as e:
    print(e)
r = csv.reader(["a,b", "c,d"])
print(next(r), r.line_num, r.dialect.delimiter)

class Semi(csv.Dialect):
    delimiter = ";"
    quotechar = '"'
    lineterminator = "\n"
    quoting = csv.QUOTE_MINIMAL

csv.register_dialect("semi", Semi)
print(csv.list_dialects() and "semi" in csv.list_dialects(), csv.get_dialect("semi").delimiter)
s = io.StringIO()
csv.writer(s, "semi").writerow(["x;y", "z"])
print(repr(s.getvalue()))
csv.unregister_dialect("semi")
try:
    csv.get_dialect("semi")
except csv.Error as e:
    print(e)
for bad in (dict(delimiter=""), dict(delimiter="ab"), dict(quotechar=5), dict(lineterminator=None)):
    try:
        csv.writer(io.StringIO(), **bad)
    except (TypeError, ValueError) as e:
        print(type(e).__name__, e)

s = io.StringIO()
dw = csv.DictWriter(s, fieldnames=["a", "b"], restval="?")
dw.writeheader()
dw.writerow({"a": 1})
dw.writerows([{"a": 2, "b": 3}])
print(repr(s.getvalue()))
try:
    dw.writerow({"c": 1})
except ValueError as e:
    print(e)
dr = csv.DictReader(io.StringIO("a,b\n1,2\n3,4,5\n6\n"), restkey="more")
print(dr.fieldnames, [dict(x) for x in dr])

sn = csv.Sniffer()
d = sn.sniff("a;b;c\n1;2;3\n")
print(d.delimiter, sn.has_header("name,age\nbob,5\nann,7\n"))
print(csv.field_size_limit(), csv.field_size_limit(10), csv.field_size_limit())
try:
    list(csv.reader(["x" * 11]))
except csv.Error as e:
    print(e)
csv.field_size_limit(131072)
print(csv.excel.delimiter, csv.excel_tab.delimiter, csv.unix_dialect.lineterminator == "\n")
