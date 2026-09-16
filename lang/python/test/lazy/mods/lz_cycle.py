lazy from mods.lz_cycle import me
print("loading mods.lz_cycle")
try:
    me
except ImportError as e:
    print(type(e).__name__, str(e).split(" (/")[0])
