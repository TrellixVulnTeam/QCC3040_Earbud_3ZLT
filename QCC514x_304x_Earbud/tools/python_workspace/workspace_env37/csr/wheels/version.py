# Type to describe version information of the form 1.3.1 
# (major, minor, tertiary) and allow comparisons

import functools
 
@functools.total_ordering
class Version(object):
    def __init__(self, string_or_int_list):
        if isinstance(string_or_int_list, str):
            self._version = [int(a) for a in string_or_int_list.split('.')]
        elif isinstance(string_or_int_list, list):
            self._version = [ int(a) for a in string_or_int_list ]
        else:
            self._version = [ int(string_or_int_list) ]
    
    def __repr__(self, *args, **kwargs):
        return ".".join("%d" % a for a in self._version) 
    
    @property
    def major(self):
        return self._version[0]
    
    @property
    def minor(self):
        try:
            return self._version[1]
        except IndexError:
            pass

    @property
    def tertiary(self):
        try:
            return self._version[2]
        except IndexError:
            pass
        
    def __lt__(self, y):
        if not isinstance(y, Version):
            raise TypeError
        return self._version < y._version
    
    def __eq__(self, y):
        if not isinstance(y, Version):
            raise TypeError
        return self._version == y._version
