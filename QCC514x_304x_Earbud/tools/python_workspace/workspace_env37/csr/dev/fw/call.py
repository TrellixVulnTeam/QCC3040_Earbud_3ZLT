############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import timeout_clock
from csr.dev.env.env_helpers import _Variable, _Structure, _Array, _Pointer, \
_Integer, var_address, var_typename, var_size
from csr.dwarf.read_dwarf import DW_TAG, DwarfNoSymbol
from csr.interface.lib_util import twos_complement
from copy import copy

from csr.dev.hw.core.kal_core import KalCore
from csr.dev.hw.core.mixin.is_xap import IsXAP

try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = (int,)


class CallTypeError(TypeError):
    """
    Special exception for local use in place of generic TypeError
    """

class CallFwMallocFailed(RuntimeError):
    """
    Indicates that a call to xpmalloc failed, indicating a lack of pmalloc RAM
    in the firmware
    """

class CallbackTimeout(RuntimeError):
    """
    Indicates that it took the firware longer than the given timeout to arrive
    at the breakpoint at the callback site
    """


def get_value(var, variable_type, python_type=int_type):
    """
    Get the value of the supplied object, whether it's a _Variable-type object
    or a built-in Python type.  If python_type is supplied, it must be an
    iterable.
    """
    if isinstance(var, variable_type):
        return var.value
    elif isinstance(var, python_type):
        return var
    else:
        # Does the type have an implicit conversion method for any of the
        # value in python_type?
        for typ in python_type:
            try:
                return typ(var)
            except TypeError:
                pass
        # If none of them work, we fail
        raise CallTypeError("Python variable with type '%s' supplied for C "
                            "variable when '%s' or '%s' expected" % 
                            (type(var), variable_type, python_type))

def implicit_conversion(arg_struct, param_struct):
    """
    Checks whether the give typename is a valid thing to pass for the given
    param 
    """
    try:
        base_arg_type = arg_struct["base_type_name"]
    except KeyError:
        base_arg_type = arg_struct["type_name"]
    try:
        base_param_type = param_struct["base_type_name"]
    except KeyError:
        base_param_type = param_struct["type_name"]
                                                    
    return base_arg_type == base_param_type or base_param_type == "void"


class CallLog(object):
    
    CALL=0
    COMMENT=1
    
    def __init__(self, fw_env, core):
        self._core = core
        self._fw_env = fw_env
        self.reset()
        
    def reset(self, log=None):
        if log is None:
            self._log = []
        else:
            self._log = log
        self._value_names = {}
        
    def __copy__(self):
        cpy = CallLog(self)
        cpy.reset(self._log[:])
        return cpy
        
    def log_call(self, f, cu, args=None):
        """
        Register a firmware function call in the log.  Arguments whose values
        have been registered as named (via log_variable()) will have those
        names included in the log entry.
        """
        if isinstance(f, int_type):
            fname = self._fw_env.functions[f]
        elif isinstance(f, str):
            fname = f
        else:
            raise ValueError("Expecting function name or address when logging "
                             "a function call")
        cu_path = None if cu is None else self._fw_env.cus.full_path(cu)
        f_addr = self._fw_env.functions[(fname, cu_path)]
        _, _, f_sym = self._fw_env.functions.get_function_of_pc(f_addr)
        f_param_names = tuple(name for name,value in f_sym.params)
        f_param_types = tuple(value.typename for name,value in f_sym.params)
        
        if f_param_names and args is None or len(f_param_names) != len(args):
            iprint("WARNING: passed wrong number of args for function!")
        
        # Gather the list of arguments and their values (and the names
        # associated with their values if available)
        args_log = []
        for name, typename, value in zip(f_param_names, f_param_types, args):
            
            arg_log_string = "%s=0x%x" % (name, value)
            try:
                # If the variable has had an explicit name registered, use it
                arg_log_string += "(%s)" % self._value_names[value]
            except KeyError:
                # If not...
                try:
                    # If the type is an enum, get the symbolic name
                    symbolic_value = self._fw_env.enums[typename][value]
                    if not isinstance(symbolic_value, str):
                        # You get a list back if there are multiple symbolic names
                        # for the given value, in which case just abandon the idea
                        raise ValueError
                    arg_log_string += "(%s)" % symbolic_value
                except (KeyError, ValueError):
                    # If that doesn't work either, just print the value
                    pass
            args_log.append(arg_log_string)
        # Then turn them into a comma-separated string
        args_log = ", ".join(args_log)
        
        if cu is None:
            call_log = "%s(%s)" % (fname, args_log)
        else:
            cu_short_name = "/".join(self._fw_env.cus.normalise_path(cu))
            call_log = "%s:%s(%s)" % (fname, cu_short_name, args_log)
        self._log.append((self.CALL, call_log))
        
    def log_return(self, ret=None):
        """
        Log a return value from the most recently called function
        """
        if ret != None:
            self._log[-1] = (self._log[-1][0], 
                             self._log[-1][1]+" = 0x%x" % ret)
        
    def log_comment(self, comment):
        """
        Add a comment to the log, e.g. to demarcate a particular block of
        calls
        """
        self._log.append((self.COMMENT, comment))
        
    def log_variable(self, value, name):
        """
        Register a name for a particular value that might appear in the log.
        Any existing name for the given value is lost.
        """
        self._value_names[value] = name
        
    def format(self, as_string=False):
        """
        Process the raw log as either a list of strings, or as a 
        newline-separated string-of-strings
        """
        formatted = []
        for entry_type, entry in self._log:
            if entry_type == self.CALL:
                formatted.append(entry)
            elif entry_type == self.COMMENT:
                formatted.append("/*")
                formatted.append(" * %s" % entry)
                formatted.append("*/")
        if as_string:
            return "\n".join(formatted)
        return formatted
        
        
class GarbageCollectedFirmwareFunction(ValueError):
    """
    The function call that has been requested is impossible because that function
    has been garbage-collected by the linker
    """

class Call(object):
    """
    Interface for writing firmware-like Python via the magic arbitrary function
    call interface in appcmd/cucmd
    """
    
    # a (big) easily identifiable number that shouldn't clash with anything in
    # the firmware and works nicely for the pmalloc trace code in pmalloc.py
    MALLOC_TRACE_OWNER = 0xFFFF << 3

    MACRO = 0
    FUNCTION = 1
    
    def __init__(self, fw_env, core, fw_cmd, hal_macros=True, include=None, blocking=True):
        
        self._fw_env = fw_env
        self._core = core
        self._fw_cmd = fw_cmd
        self._layout = core.info.layout_info
        self._trace_alloc = False
        self._int_bit_width = self._fw_env.types["int"]["byte_size"]*8
        self._call_timeout = 0
        self._hal_macros = hal_macros
        self._log = CallLog(self._fw_env, core)
        self._unrealised = {}
        self.blocking = blocking

        # Index after setting up all the object's attributes so that we can
        # do a reliable check for attributes we mustn't overwrite
        self.index(include=include)

    def clone(self, include=None):
        return unique_subclass(Call)(self._fw_env, self._core, self._fw_cmd, 
                                     hal_macros=self._hal_macros, include=include,
                                     blocking=self.blocking)

    def get_signature(self, name_or_func, cu=None):
        if isinstance(name_or_func, str):
            name = name_or_func
        else:
            name = name_or_func.__name__
            if name_or_func._fw_type == self.MACRO:
                if name.startswith("hal_get_reg"):
                    return "unsigned %s(void)" % name
                elif name.startswith("hal_set_reg"):
                    return "void %s(unsigned value)" % name
                else:
                    raise ValueError("Unknown macro emulation method '%s'" % name)

        cu_path = None if cu is None else self._fw_env.cus.full_path(cu)
        func_addr = self._fw_env.functions[(name, cu_path)]
        _,_,func_sym = self._fw_env.functions.get_function_of_pc(func_addr)
        return func_sym.signature_string % name
  
    @property
    def log(self):
        return self._log
            
    def _safe_setattr(self, name, attr):
        """
        Add the attribute object under the given name if there is no attribute
        of that name already present.
        """
        if not hasattr(self.__class__, name):
            setattr(self, name, attr)

    def _make_func(self, name, cuname):
        def func(*args, **kwargs):
            return self((name, cuname), *args, **kwargs)
        try:
            sig = self.get_signature(name, cuname)
        except Exception as exc:
            sig = ("""Couldn't get function signature: {}
Call get_signature("{}") on the Call object for a full backtrace.""".
                    format(str(exc), name))
        func.__name__ = name
        func.__doc__ = """
Call firmware function '%s'.\n\n%s
""" % (name, sig)
        func._fw_type = self.FUNCTION
        return func

    def _add_callable(self, name, cuname):
        """
        Make the named function an attribute of the Call class so that you can
        type, e.g.
            apps.fw.call.my_function(1,2,3)
        instead of
            apps.fw.call("my_function", 1, 2,3)
        """
        if name in ("pfree", "pmalloc_trace", "sfree", "smalloc_trace"):
            self._unrealised["_%s_" % name] = (name, cuname)
        else:
            self._unrealised[name] = (name, cuname)
        
    def index(self, prefix_filter=None, include=None):
        """
        Add all the functions that start with a string in the prefix filter list
        as attributes
        """
        if prefix_filter is not None:
            if isinstance(prefix_filter, str):
                prefix_filter = [prefix_filter]
            include = lambda fname : any(fname.startswith(pfx) for pfx in prefix_filter)

        for name, cuname, _ in self._fw_env.dwarf.function_list:
            if include is not None and include(name):
                self._add_callable(name, cuname)
            else:
                self._add_callable(name, cuname)
                
        if self._hal_macros:
            if (prefix_filter is None or 
                        "hal" in prefix_filter or "hal_" in prefix_filter):
                self.add_hal_macros()

        
    def add_hal_macros(self, add_help=True):
        """
        Loop through all the registers adding functions called 
        "hal_get_reg_<name>" and "hal_set_reg_<name>" where name is the 
        lower-case version of the register
        """
        for reg in self._core.field_refs.keys():
            getter = "hal_get_reg_%s" % reg.lower()
            setter = "hal_set_reg_%s" % reg.lower()
            # We use an intermediate func_getter so that there is a dedicated
            # closure for each register's getfunc/setfunc pair from which 
            # the register name variable 'reg' can be retrieved.  Otherwise
            # every getfunc/setfunc would inherit its closure from
            # add_hal_macros, in which the value of 'reg' after the call
            # to all_hal_macros completes will be whatever is last in
            # self._core.field_refs.keys().
            def func_getter(reg):
                def getfunc():
                    return self._core.field_refs[reg].read()
                def setfunc(value):
                    return self._core.field_refs[reg].write(value)
                if add_help:
                    getfunc.__name__ = getter
                    getfunc.__doc__ = """Reads register '%s'""" % reg
                    getfunc._fw_type = self.MACRO
                    setfunc.__name__ = setter
                    setfunc.__doc__ = """Writes register '%s'""" % reg
                    setfunc._fw_type = self.MACRO
                return getfunc, setfunc

            gfunc,sfunc = func_getter(reg)
            setattr(self, getter, gfunc)        
            setattr(self, setter, sfunc)
        
    def __call__(self, func_name_cu_name, *args_in, **kwargs):
        """
        Call the given function with the given arguments.
        
        Does conversion of arguments before forwarding the call to the 
        underlying firmware interface module (appcmd or cucmd).  In particular,
        the conversion allows:
         - _Integer objects (as well as plain Python ints and longs) to be 
         passed by value for any parameter with DWARF tag "base_type"
         - Enumerator names to be passed as strings (as well as _Integer objects
         and plain Python ints and longs) for any parameter with DWARF tag
         "enumeration_type"
         - _Pointer objects (as well as plain Python ints and longs) to be
         passed by value for any parameter with DWARF tag "pointer_type", so
         long as the underlying types match
         - _Structure objects to be passed for any parameter with DWARF tag
         "pointer_type" so long as the Structure's type matches the pointed-to
         type
         Passing arbitrary pointers or structures to void * parameters is
         supported.
        """
        
        # We would just have "blocking" as an explicit keyword argument, but Python 2
        # doesn't allow explicit keywords after implicit positionals.
        blocking = kwargs.pop("blocking", True)
        if kwargs:
            raise TypeError("Unexpected keyword argument(s) {} in call to "
                            "Call.__call__".format(", ".join("'{}'".format(k) 
                                                             for k in kwargs.keys())))

        try:
            func_name, cu_name = func_name_cu_name.split(":")
            if cu_name == "None":
                cu_name = None
        except AttributeError:
            # It's a tuple
            func_name, cu_name = func_name_cu_name
        except ValueError:
            # It's a string but it doesn't split into two parts separated by a 
            # colon: it's just a function name
            func_name, cu_name = func_name_cu_name, None
        
        args = list(args_in)
        free_args = [] # Store indices of arguments that are copied into locally 
        # allocated pointers so the pointers can be freed after the call
        capture_args = [] # Store references to arguments that need to be captured
        # before any freeing is done because they are implicitly memory-managed by
        # this function
        cu_path = None if cu_name is None else self._fw_env.cus.full_path(cu_name)
        func_addr = self._fw_env.functions[(func_name, cu_path)]
        if self._fw_env.functions.address_is_garbage(func_addr):
            
            cu_str = " from {}".format(cu_path) if cu_path is not None else ""
            raise GarbageCollectedFirmwareFunction("Cannot call '{}'{}: "
               "this function was garbage-collected by the linker".format(func_name, cu_str))
        _,_,func_sym = self._fw_env.functions.get_function_of_pc(func_addr)
            
        params = list(func_sym.params)
        if len(params) != len(args):
            raise TypeError("%s takes %d arguments, but %d supplied" % 
                                            (func_name, len(params), len(args)))
        layout = self._fw_env.layout_info
        bytes_in_size_unit = layout.addr_unit_bits//8
        total_arg_size = 0
        for _,param in params:
            size = param.struct_dict["byte_size"]*bytes_in_size_unit
            if size is None or isinstance(size, str):
                # void pointer
                size = self._fw_env.layout_info.data_word_bits//self._fw_env.layout_info.adddr_unit_bits
            total_arg_size += size
        self._fw_cmd.call_total_arg_size(total_arg_size)

        try:
            for i, ((pname, psym),a) in enumerate(zip(params, args)):
                type_tag = psym.type_tag
                
                if type_tag == DW_TAG["base_type"]:
                    if not isinstance(a, _Pointer): # _Pointer is-an _Integer, but
                        # passing a _Pointer to a function that expects an integer
                        # is not legal
                        args[i] = get_value(a, _Integer)
                        if psym.issigned:
                            args[i] = twos_complement(args[i], 
                                                      self._int_bit_width,
                                                      to_unsigned=True)
                elif type_tag == DW_TAG["enumeration_type"]:
                    if isinstance(a, str):
                        # Map enum strings to values
                        try:
                            args[i] = self._fw_env.enums[psym.typename][a]
                        except KeyError:
                            args[i] = self._fw_env.enum_consts[a]
                    else:
                        args[i] = get_value(a, _Integer)
                elif type_tag == DW_TAG["pointer_type"]:
                    if isinstance(a, _Pointer):
                        if (implicit_conversion(a._info.struct["pointed_to"], 
                                                psym.struct_dict["pointed_to"])):
                            args[i] = a.value
                        else:
                            raise CallTypeError("Parameter has type %s but "
                                                "_Pointer to %s passed" %
                                                (psym.typename,
                                                 a.deref._info.datatype))
                    elif isinstance(a, _Array):
                        if (implicit_conversion(a._info.struct["element_type"],
                                                psym.struct_dict["pointed_to"])):
                            args[i] = var_address(a)
                        else:
                            raise CallTypeError("Parameter has type %s but "
                                                "_Array of type %s passed" %
                                                (psym.typename,
                                                 a._info.datatype))
                    elif isinstance(a, _Variable):
                        if implicit_conversion(a._info.struct, 
                                               psym.struct_dict["pointed_to"]):
                            args[i] = var_address(a)
                        else:
                            raise CallTypeError("Parameter has type %s but "
                                                "object of type %s passed" % 
                                                (psym.typename,
                                                 a._info.datatype))
                    elif isinstance(a, int_type):
                        args[i] = a
                        
                        
                    elif isinstance(a, Callback):
                        # Is the function expecting a function pointer argument?
                        # Let's just assume so for now
                        args[i] = var_address(a)
                        # Set the breakpoint that will allow the Python to run 
                        # as the callback code
                        a.prepare()
                        
                    # We allow a Python string to be passed for a const char * argument.
                    elif isinstance(a, str):
                        if psym.typename != "const char *":
                            raise CallTypeError("Parameter has type '%s' but Python "
                                                "string passed (only valid for const "
                                                "char * params)" % param.typename)
                        char_array = self.new("char", len(a)+1)
                        free_args.append(i)
                        # Write each character into RAM
                        for j,c in enumerate(a):
                            char_array[j].value = ord(c)
                        # Followed by the null terminator
                        char_array[len(a)].value = 0
                        args[i] = var_address(char_array)

                    elif isinstance(a, self._TmpPtr):
                        # We need to allocate some memory for the pointer
                        if implicit_conversion(a._TmpPtr_type_dict,
                                               psym.struct_dict["pointed_to"]):
                            size = var_size(a._TmpPtr_referent)
                            scratch_ptr = self._fw_cmd.call_request_scratch_space(size)
                            if scratch_ptr is None:
                                scratch_ptr = self._malloc(size)
                                free_args.append(i)
                            a.__TmpPtr_assign__(scratch_ptr)
                            args[i] = scratch_ptr
                            capture_args.append(a)
                        else:
                            raise CallTypeError("Parameter has type {} but ptr to {} "
                                "passed".format(param.typename, var_typename(a._TmpPtr_referent)))

                    else:
                        raise CallTypeError("Parameter has type %s but Python object "
                                            "of type %s passed" % (psym.typename,
                                                                   type(a)))
                else:
                    raise CallTypeError("Functions with parameters of non-"
                                        "integral types not supported")
                    

            if self._log is not None:
                self._log.log_call(func_name, cu_path, args)
            func_ret = self._fw_cmd.call_function(func_name_cu_name, args, 
                                                  timeout=self._call_timeout,
                                                  blocking=blocking)
            if not blocking:
                return
            for a in capture_args:
                a.__TmpPtr_capture__()
        finally:
            # Free arguments that have been locally allocated (e.g. for passing
            # strings)
            for index in free_args:
                self._free(args[index])
        

        # Do something useful with the returned value
        ret_type = func_sym.return_type
        if ret_type is None:
            func_ret = None
        else:
            ret_dict = ret_type.struct_dict
            if ret_dict["type_name"] == "bool":
                # Convert C bool to Python boolean
                func_ret = (func_ret != 0)
            elif (ret_dict["type_tag"]== DW_TAG["base_type"] and 
                  ret_type.issigned):
                # Note that regardless of the width of the returned type, we
                # need an int-width two's complement
                func_ret = twos_complement(func_ret, self._int_bit_width)
            if self._log is not None:
                self._log.log_return(func_ret)
        
        return func_ret
        
    def _malloc_helper(self, typename, arrsize, malloc_fn, malloc_trace_arg):
    
        type = self._fw_env.types[typename]
        if arrsize is not None:
            type = {"num_elements" : arrsize,
                    "type_tag" : DW_TAG["array_type"],
                    "element_type" : type, 
                    "byte_size" : type["byte_size"] * arrsize,
                    "type_name" : "%s[%d]" % (type["type_name"], arrsize)
                   }
        size_bytes = type["byte_size"]
        size = size_bytes * 8 / self._layout.addr_unit_bits
        
        malloc_args = [size]
        if malloc_trace_arg is not None:
            malloc_args.append(malloc_trace_arg)
        pointer = malloc_fn(*malloc_args)
        
        if pointer == 0:
            raise CallFwMallocFailed
        if self._trace_alloc:
            iprint("Allocated 0x%x" % pointer)
        
        return _Variable.create_from_type(type, pointer, self._core.data, 
                                          self._layout, env=self._fw_env)


        
    def new(self, typename, arrsize=None):
        """
        Convenience function resembling the macro of the same name in the 
        Hydra firmware which does a pmalloc of the right size and creates a _Variable
        object of the appropriate type pointing at the space
        """

        return(self._malloc_helper(typename, arrsize, self._malloc, self._malloc_trace_arg))

    # Alternative name for non-Hydra users
    def delete(self, addr):
        return self._free(addr)


    def snew(self, typename, arrsize=None):
        """
        Convenience function resembling the macro of the same name in the 
        firmware which does a smalloc of the right size and creates a _Variable
        object of the appropriate type pointing at the space
        """
        
        return(self._malloc_helper(typename, arrsize, self._smalloc, self._smalloc_trace_arg))

    # A name Hydra folks are familiar with
    pnew = new

    def pmalloc_trace(self, size, owner):

        addr = self._pmalloc_trace_(size, owner)
        if self._trace_alloc:
            iprint("Allocated 0x%x" % addr)
        return addr

    def pfree(self, addr):
        
        if self._trace_alloc:
            iprint("Freeing 0x%x" % addr)
        self._pfree_(addr)


    def sfree(self, addr):
        if self._trace_alloc:
            iprint("Freeing 0x%x" % addr)
        self._sfree_(addr)

    def set_timeout(self, timeout):
        """
        Set the timeout for all function calls through this object
        """
        ret = self._call_timeout
        self._call_timeout = timeout
        return ret

    def _get_malloc_and_free(self):
        """
        Return the canonical malloc and free functions for this particular 
        firmware build.
        """
        for malloc_name, free_name, trace_arg in [
                          ("xzpmalloc","pfree", None),
                          ("xzpmalloc_trace","pfree", self.MALLOC_TRACE_OWNER),
                          ("xzpmalloc_trace_pc","pfree", None),
                          ("malloc","free", None),
                          # Zeagle BT
                          ("rom_PF_OS_eMalloc","rom_PF_OS_eFree", None),
                          # QAPI
                          ("qapi_Malloc", "qapi_Free", None)]:
            try:
                return getattr(self,malloc_name), getattr(self, free_name), trace_arg 
            except AttributeError:
                pass
        raise AttributeError("No known candidates for malloc and free!")


    def _get_smalloc_and_sfree(self):
        """
        Return the canonical smalloc and sfree functions for this particular 
        firmware build.
        """
        for malloc_name, free_name, trace_arg in [
                          ("xsmalloc","sfree", None),
                          ("xsmalloc_trace","sfree", self.MALLOC_TRACE_OWNER),
                          ("xsmalloc_trace_pc","sfree", None)]:
            try:
                return getattr(self,malloc_name), getattr(self, free_name), trace_arg 
            except AttributeError:
                pass
        raise AttributeError("No known candidates for smalloc and sfree!")

    @property
    def _smalloc(self):
        try:
            self.__smalloc
        except AttributeError:
            self.__smalloc, self.__sfree, self.__smalloc_trace_arg = self._get_smalloc_and_sfree()
        return self.__smalloc
    
    @property
    def _sfree(self):
        try:
            self.__sfree
        except AttributeError:
            self.__smalloc, self.__sfree, self.__smalloc_trace_arg = self._get_smalloc_and_sfree()
        return self.__sfree
    
    @property
    def _smalloc_trace_arg(self):
        try:
            self.__smalloc_trace_arg
        except AttributeError:
            self.__smalloc, self.__sfree, self.__smalloc_trace_arg = self._get_smalloc_and_sfree()
        return self.__smalloc_trace_arg

    @property
    def _malloc(self):
        try:
            self.__malloc
        except AttributeError:
            self.__malloc, self.__free, self.__malloc_trace_arg = self._get_malloc_and_free()
        return self.__malloc

    @property
    def _free(self):
        try:
            self.__free
        except AttributeError:
            self.__malloc, self.__free, self.__malloc_trace_arg = self._get_malloc_and_free()
        return self.__free

    @property
    def _malloc_trace_arg(self):
        try:
            self.__malloc_trace_arg
        except AttributeError:
            self.__malloc, self.__free, self.__malloc_trace_arg = self._get_malloc_and_free()
        return self.__malloc_trace_arg


    class _PnewContextGuard(object):
        """
        Support an allocation context for firmware variables which causese them
        to be magically freed 
        """
        def __init__(self, call, typename, arrsize=None):
            
            self._call = call
            self._obj = call.new(typename, arrsize=arrsize)
    
        def __enter__(self):
            return self._obj
    
        def __exit__(self,  type, value, traceback):
            self._call._free(self._obj)
            return False
        
    def create_local(self, typename, arrsize=None):
        """
        Return a ContextGuard that handles allocation and freeing of objects.
        This lets you do:
        
        with create_local("uint16", 4) as my_array:
           do things with my_array
           
        my_array is now magically freed, even if the with block raises an
        exception
        
        """
        return self._PnewContextGuard(self, typename, arrsize)

    def __getattr__(self, attr):
        """
        Handle failure to get an attribute in a slightly more graceful way than
        the interpreter does
        """
        try:
            name, cuname = self._unrealised[attr]
        except KeyError:
            if self._core.nicknames:
                name = self._core.nicknames[0]
            else:
                name = "this core"
            raise AttributeError("No firmware function in %s called '%s'" % (name, 
                                                                            attr))
        else:
            func = self._make_func(name, cuname)
            self._safe_setattr(attr, func)
            return func

    def __dir__(self):
        attrs = set(dir(super(Call, self)))
        attrs = attrs.union(self._unrealised.keys())
        return list(attrs)
    

    def local(self, typename, value=None, array_len=None):
        """
        Returns an object representing a pointer to the given type.  This pointer
        is suitable for passing into a function call if and only if ownership 
        is retained by the caller.  
        
        Pydbg manages the referent memory during the call, ensuring it is freed
        as appropriate.  For pointers to small types Pydbg will use unused
        call parameter space for the referent memory, avoiding an additional
        pair of malloc/free calls alongside the intended function call. 
        However, when the referent memory block is too large for the available
        space a malloc/free is used instead.
        """
        return self._TmpPtr(typename, self._fw_env, value=value, array_len=array_len)


    class _TmpPtr(object):
        """
        Class that manages memory for use with pointers passed into function
        call where the memory remains in the caller's ownership
        """
        def __init__(self, typename, env, value=None, array_len=None):
            self._TmpPtr_env = env
            self._TmpPtr_type_dict = env.types[typename]
            self._TmpPtr_array_len = array_len
            size = (self._TmpPtr_type_dict["byte_size"] // (env.layout_info.addr_unit_bits//8) * 
                            (1 if array_len is None else array_len))
            self._TmpPtr_referent = env.cast(0, typename, data_mem=[0]*size, array_len=array_len)
            self._TmpPtr_add_referent_members()

            if value is not None:
                self._TmpPtr_referent.value = value

        def __repr__(self):
            try:
                self._TmpPtr_referent
            except AttributeError:
                return object.__repr__(self)
            else:
                return repr(self._TmpPtr_referent)

        def _TmpPtr_add_referent_members(self):
            try:
                self._TmpPtr_referent.members
            except AttributeError:
                pass
            else:
                self._TmpPtr_allow_member_attribute_setting = True
                for mbr_name, mbr in self._TmpPtr_referent.members.items():
                    setattr(self, mbr_name, mbr)
                self._TmpPtr_allow_member_attribute_setting = False

        def __TmpPtr_assign__(self, raw_ptr):
            """
            Populate the object, if it has a value supplied
            """
            # If there is already a local referent, pull out its value
            try:
                value = self._TmpPtr_referent.value
            except AttributeError:
                value = None
            # Now recreate the same type against RAM
            self._TmpPtr_referent = self._TmpPtr_env.cast(raw_ptr, 
                                            var_typename(self._TmpPtr_referent))
            # We need to re-add the referent members or else they will
            # still be pointing to local memory.
            self._TmpPtr_add_referent_members()
            if value is not None:
                self._TmpPtr_referent.value = value

        def __TmpPtr_capture__(self):
            """
            Capture the referent into local memory
            """
            self._TmpPtr_referent = self._TmpPtr_referent.capture()
            # We need to re-add the referent members or else they will
            # still be pointing to on-chip memory.
            self._TmpPtr_add_referent_members()

        def __getattr__(self, attr):
            """
            We want the TmpPtr to act like the referent for its public interface
            """
            return getattr(self._TmpPtr_referent, attr)

        def __setattr__(self, attr, value):
            # We don't want users to accidentally set attributes on the 
            # TmpPtr when they were trying to set them on the referent.
            if attr.startswith("_TmpPtr_") or self._TmpPtr_allow_member_attribute_setting:
                super(self.__class__, self).__setattr__(attr, value)
            else:
                setattr(self._TmpPtr_referent, attr, value)

        @property
        def value(self):
            return self._TmpPtr_referent.value

        @value.setter
        def value(self, value):
            self._TmpPtr_referent.value = value

        # Array interface
        def __len__(self):
            return len(self._TmpPtr_referent)

        def __getitem__(self, index):
            return self._TmpPtr_referent[index]


class Callback(object):
    """
    Replaces a firmware callback so that Python code can be executed while the
    processor waits.  Uses a breakpoint to pause the processor.
    
    The Python code should take positional arguments only, of equal number to 
    the arguments
    """
    def __init__(self, core, cb_type_name_or_dict, callback,
                 env=None, autoset=False):
        """
        Set up the callback diversion.
        
        :param core: The object modelling the processor that will executed the 
        callback
        :param cb_type_name_or_dict: String or dictionary: if string, the name of
        the (function pointer) type of the callback; if a dictionary, a standard 
        read_dwarf-style type dictionary for the relevant function pointer type
        (the reason for both options is that function pointer types are often
        anonymous)
        :param env: The firmware environment representing the firmware running
        on-chip.  Defaults to the core's built-in environment
        :param autoset: Activate the callback diversion immediately and keep it
        activated at all times 
        """
        
        self._core = core
        self.env = env if env is not None else core.fw.env
        # Look up the "pointed_to" field because callback types are actually
        # function *pointer* types
        if isinstance(cb_type_name_or_dict, str):
            self._type_dict = self.env.types[cb_type_name_or_dict]["pointed_to"]
        else:
            self._type_dict = cb_type_name_or_dict["pointed_to"]

        # Find the address that needs to be called 
        self._fw_callback_addr = self.env.functions.get_call_address("appcmd_test_cb")

        # Store the callable to invoke when the firmware hits the callback
        # breakpoint
        self._cb = callback
        
        self._reset = autoset
        if autoset:
            self.prepare()
        

    @property
    def address(self):
        """
        Address which the firmware should store in the function pointer 
        """
        return self._fw_callback_addr
    
    def prepare(self):
        """
        Set the callback up to be diverted, using a breakpoint 
        """
        self._brk_id = self._core.brk_set(self._fw_callback_addr)
        
    def await_and_invoke(self, timeout=None):
        """
        Blocking wait for the callback to be called.
        """
        if timeout is not None:
            start = timeout_clock()
        while self._core.pc != self._fw_callback_addr:
            if timeout is not None and start - timeout_clock() > timeout:
                raise CallbackTimeout
        # We're at the breakpoint.
        
        # 1. Gather the arguments
        args = self._get_args()
        
        # 2. Invoke the Python callback
        ret = self._cb(*args)
        
        # 3. Set the return value if necessary
        self._set_ret(ret)
        
        # 4. Clear the breakpoint if necessary and run the processor
        if not self._reset:
            self._core.brk_clear(self._brk_id)
            self._brk_id = None
        self._core.run()
        
    def _get_args(self):
        """
        Retrieve the arguments that the firmware placed in registers/stack as
        part of the call prologue.
        """
        args = []
        if isinstance(self._core, KalCore):
            # Kalimba calling convention: first four args in r[0:3], remainder
            # on stack in reverse order
            for i,(_,param) in enumerate(self._type_dict["params"]):
                if i < 4:
                    # Arg is in r[i]
                    args.append(self._core.r[i])
                else:
                    # Arg is on the stack, in R-to-L order
                    var_addr = self._core.sp - 4*(i-3)
                    args.append(bytes_to_dwords(self._core.dm[var_addr:var_addr+4])[0])
        # XAP calling convention - requires more work because args wider than 16
        # bits are split across registers so it gets a bit messier
        elif isinstance(self._core, IsXAP):
            raise NotImplementedError("Haven't implemented the reverse XAP "
                                      "calling convention to enable Python callbacks")

        # Now cast pointers to _Variables of the appropriate type, using the
        # param type information.  But leave void pointers as simple values.
        for i, (_, param) in enumerate(self._type_dict["params"]):
            if param.type_tag == DW_TAG["pointer_type"] and "void" not in param.typename:
                args[i] = _Variable.create_from_type(param.struct_dict["pointed_to"],
                                                     args[i],
                                                     self._core.data,
                                                     self._core.info.layout_info,
                                                     env=self.env)
                
        return args
            
    def _set_ret(self, ret):
        """
        Check what type of return value we're expecting, if any, and set it in
        the calling-convention-appropriate way.
        """
        if (ret is not None) != ("type_name" in self._type_dict and 
                                 not isinstance(self._type_dict["type_name"],str)):
            # There is a return value when there shouldn't be or vice versa
            raise CallTypeError("Unexpected presence/absence of return value")
        
        if ret is not None:
            if isinstance(self._core, KalCore):
                self._core.r[0] = ret
            elif isintance(self._core, IsXAP):
                self._core.xap_al = ret
        
        
