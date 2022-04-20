############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from collections import OrderedDict
from .bitsandbobs import PureVirtualError

try:
    long
except NameError:
    long = int

class JavascriptValue(object):
    """
    Base class for wrappers around Python objects that know how to format them
    appropriately as Javascript literals
    """
    @staticmethod
    def create(py_value):
        """
        Create an appropriate JavascriptValue object for the supplied Python
        variable
        """
        if isinstance(py_value, (list, tuple)):
            return JavascriptArray(py_value)
        if isinstance(py_value, dict):
            return JavascriptObject(py_value)
        if isinstance(py_value, (int, long, float)):
            return JavascriptScalar(py_value)
        if isinstance(py_value, (str, unicode)):
            return JavascriptString(py_value)
        
        raise TypeError("Don't know how to convert '%s' to a Javascript "
                        "literal"  % type(py_value))
        
    def __init__(self, py_value):
        """
        By default, simply store the Python value as-is
        """
        self._value = py_value
        
    @property
    def literal(self):
        """
        Return a string containing the value as a Javascript literal
        """
        raise PureVirtualError
    
class JavascriptArray(JavascriptValue):
    
    def __init__(self, py_value):
        
        self._value = [JavascriptValue.create(v) for v in py_value]
    
    @property
    def literal(self):
        """
        Loop through the Python list writing out the elements surrounded by
        square brackets
        """
        lit = "["
        for v in self._value:
            lit += "%s," % v.literal
        lit = lit.rstrip(",") + "]"
        return lit
    
class JavascriptObject(JavascriptValue):

    def __init__(self, py_value):
        
        # We assume that the dictionary keys will remain hashable when converted
        # to Javascript objects
        self._value = OrderedDict((JavascriptValue.create(key), 
                            JavascriptValue.create(v))  
                                            for key, v in py_value.items())

    @property
    def literal(self):
        """
        Loop over the entries doing things with curly braces
        """
        lit = "{"
        for k, v in self._value.items():
            lit += "%s: %s," % (k.literal, v.literal)
        lit = lit.rstrip(",") + "}"
        return lit
    
class JavascriptString(JavascriptValue):
    
    def __init__(self, py_value):
        
        self._value = py_value.encode("ascii")
        
    @property
    def literal(self):
        """
        """
        return '''"%s"''' % self._value
    
class JavascriptScalar(JavascriptValue):
    
    def __init__(self, py_value):
        self._value = py_value
        
    @property
    def literal(self):
        """
        Return the same thing as Python's literal representation
        """
        return str(self._value)
    

class JavascriptVariable(object):
    """
    Class for creating a variable to be written to a Javascript file.
    """
    def __init__(self, name, py_value):
        
        self._name = name
        self._value = JavascriptValue.create(py_value)
        
    def write(self, out):
        """
        Write this variable to a stream
        """
        out.write("var %s = %s;\n" % (self._name, self._value.literal))
        
