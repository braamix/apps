class Empty:
    pass

class Point(object):
    kind = 'point'

    def __init__(self, x, y):
        self.x = x
        self.y = y

    def norm(self):
        return self.x * self.x + self.y * self.y

class Meta(Point, metaclass=type):
    pass
