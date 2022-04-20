from collections import namedtuple
from collections import deque


Action = namedtuple("Action", ['description', 'callable'])


class ActionsList(deque):
    pass
