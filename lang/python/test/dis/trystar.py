try:
    f()
except* ValueError as e:
    print(e)
except* (TypeError, KeyError):
    raise
