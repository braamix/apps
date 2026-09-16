print("loading mods.lz_b")
thing = "the thing"
other = "the other"


def __getattr__(name):
    if name == "dynamic":
        return "made by __getattr__"
    raise AttributeError(name)
