############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels import gstrm
from csr.wheels import FrozenNamespace, LazyAttribEvaluator, display_hex,\
NameSpace
from csr.wheels.scoped_symbol_dict import ScopedSymbolDict
from csr.dev.framework.meta.elf_firmware_info import BadPCLow, BadPCHigh, BadPC,\
AmbiguousCUName
from csr.dev.hw.address_space import AddressMultiRange, AddressRange
from csr.dev.model.interface import Group, Text, Table
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dwarf.read_dwarf import DwarfFunctionAddressRangeNotPresent, DwarfNoSymbol
from csr.wheels.bitsandbobs import unique_subclass

from bisect import bisect
from collections import namedtuple
import sys
import os
import re

try:
    # Python 2
    int_type = (int, long)
    str_type = (str, unicode)
except NameError:
    # Python 3
    int_type = int
    str_type = (str)


class FwReport(object):
    "Convenience type for use in reporting functions and static variables"
        
    elt_index = ["name", "cu", "address", "size"]
    
    def __init__(self, name, cu, addr, size):
        self.name = name
        self.cu = cu
        self.size = size
        self.address = addr
    def __len__(self):
        return len(self.elt_index)
    def __getitem__(self, i):
        return getattr(self, self.elt_index[i])

class SimpleSymbolDict(object):
    """
    ELF-orientated wrapper around ScopedSymbolDict with a particular fixed filter
    and an optional auxiliary dict (itself a SimpleSymbolDict, but one that
    guarantees not to throw an UnknownName exception and instead just return 
    None).  Conventional __getitem__-based name look-up is by *symbol_name" 
    (basename plus inner scopes) not *scoped_name* (outer scopes) 
    """
    def __init__(self, symbol_dict, aux_dict=None, filter=None, 
                 factory=None, cacheable=False):
        """
        :p symbol_dict Primary ScopedSymbolDict containing symbols
        :p aux_dict Optional ScopedSymbolDict containing extra symbols
        :p filter Predicate callable taking a key and a symbol reference, used
        to filter out elements of the underlying
        :p factory Callable taking a key and a symbol.  Note: this factory
        is distinct from the symbol_ref->symbol factory applied by the underlying
        symbol_dict.
        """
        self._symbol_dict = symbol_dict
        self._aux_dict = aux_dict
        self._sep = symbol_dict.sep
        self._filter = filter
        if factory is None:
            self._factory = lambda k, x : x
        else:
            self._factory = factory
        try:
            self._ambig_name_exceps = (self._symbol_dict.AmbiguousNameExcep,
                                       self._aux_dict.AmbiguousNameExcep)
        except AttributeError:
            self._ambig_name_exceps = self._symbol_dict.AmbiguousNameExcep
        
        self._cache = {} if cacheable else None

    def __getitem__(self, symbol_name):
        """
        Look up the given symbol_name in the symbol_dict or, if that fails, the
        aux_dict.
        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        """
        try:
            full_key = self._symbol_dict.lookup_key(symbol_name, filter=self._filter)
            if full_key is not None:
                if self._cache is not None:
                    try:
                        return self._cache[full_key]
                    except KeyError:
                        pass
                obj = self._factory(full_key, self._symbol_dict[full_key])
                if self._cache is not None:
                    self._cache[full_key] = obj
                return obj
        except self._symbol_dict.UnknownNameExcep:
            full_key = None
            
        if full_key is None:
            if self._aux_dict:
                # aux_dicts return None on an unsuccessful name retrieval
                lookup = self._aux_dict.lookup_symbol(symbol_name)
                if lookup:
                    return lookup
                else:
                    if self._aux_dict.lookup_key(symbol_name) is not None:
                        return None
            raise KeyError(symbol_name)
    
    def get(self, symbol_name, default=None):
        """
        Identical to __getitem__ except that failure of symbol_name look-up 
        results in the default being returned.

        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        :p default Value to return if the scoped_name is not present
        """
        try:
            return self[symbol_name]
        except KeyError:
            return default
    
    def __contains__(self, symbol_name):
        """
        Does the supplied symbol_name lie in the dictionary?
        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        """
        try:
            self[symbol_name]
            return True
        except (KeyError,) + self._ambig_name_exceps:
            return False 
    
    def __iter__(self):
        return (k for k in self.keys())
    
    def keys(self):
        """
        All the symbol keys for the entire nested scope set from both the main
        dictionary and the aux dict, if present
        """
        return  (self._symbol_dict.keys(filter=self._filter)
                  +  (list(self._aux_dict.keys()) if self._aux_dict else []))

    def local_symbols(self):
        """
        Keys corresponding to actual entities directly within the top-level
        scope
        """
        return (self._symbol_dict.symbol_keys(filter=self._filter) + 
                (self._aux_dict.symbol_keys() if self._aux_dict else []))
    
    def nested_scopes(self):
        """
        A dictionary mapping subscope names to sub-clones of this object, for 
        use by recursive iteration schemes.
        """
        if self._aux_dict:
            aux_scopes = {k:v for (k,v) in self._aux_dict.scope_items()}
        else:
            aux_scopes = {}
        
        main_scopes = {k:self._clone(v, aux_dict=aux_scopes.get(k)) 
                            for (k,v) in self._symbol_dict.scope_items()
                                            if v.any_keys(filter=self._filter)}
        # add in any scopes that are only in the aux_dict
        extra_scope_names = set(aux_scopes) - set(main_scopes)
        for extra in extra_scope_names:
            main_scopes[extra] = self._clone(aux_scopes[extra])
        return main_scopes
    
    def _clone(self, symbol_dict, aux_dict=None):
        return SimpleSymbolDict(symbol_dict, filter=self._filter,
                              factory=self._factory,
                              aux_dict=aux_dict)
    
    def lookup_key(self, symbol_name):
        """
        Attempt to look up a particular symbol_name returning the full key of a 
        unique matching entry
        :p symbol_name Partially scoped symbol_name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        """
        try:
            key = self._symbol_dict.lookup_key(symbol_name)
        except self._symbol_dict.UnknownNameExcep:
            key = None
        if key is None and self._aux_dict:
            key = self._aux_dict.lookup_key(symbol_name)
        return key
    
    def minimal_unique_subkey(self, scoped_name, join=True):
        """
        Turn a scoped_name into the shortest equivalent unique symbol_name (i.e.
        the shortest name that could be passed to lookup_key to find the symbol)
        :p scoped_name Full name of a symbol or scope, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        :p join If True return the symbol_name as a separator-joined string,
        otherwise return as a tuple
        """
        minimal_main, minimal_aux = None, None
        if scoped_name in self._symbol_dict:
            minimal_main = self._symbol_dict.minimal_unique_subkey(scoped_name, join=join)
            # if this is a valid scoped_name in the aux dict then it's too minimal
            if self._aux_dict:
                aux_matches = self._aux_dict.scoped_name_matches(minimal_main)
                if aux_matches:
                    # Annoying case: a name clash between the main and aux dicts
                    raise NotImplementedError("Can't (yet) handle name clash "
                                              "between main and aux name dicts")
                
            return minimal_main
            
        if self._aux_dict and scoped_name in self._aux_dict:
            return self._aux_dict.minimal_unique_subkey(scoped_name, join=join)
            
        # It's not in either: let the main symbol dict raise an exception for us
        raise ValueError("'%s' is not a valid scoped_name" % str(scoped_name))
    
    def lookup_scope(self, scope_name):
        """
        Return the ScopedSymbolDict corresponding to the given scope name from
        either the main symbol_dict or the aux_dict
        :p scope_name Full name of a scope, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        try:
            return self._symbol_dict[scope_name]
        except self._symbol_dict.UnknownNameExcep:
            if self._aux_dict:
                return self._aux_dict[scope_name]
            raise
        

class SymbolDict(SimpleSymbolDict):
    """
    Extends SimpleSymbolDict by adding the notion that the top-level scope may 
    be a compilation unit, which has unique scoping semantics - primarily 
    because entities declared within it may in fact be outside its scope 
    (because they are global).  Supplies generic functionality for recursively
    applying scope-level operations based on the contents of the dict.  This
    is intended for creation of attribute-based containers. 
    """
    def __init__(self, symbol_dict, cu_dict, aux_dict=None, filter=None, 
                 factory=None, elide_top_level_scope=False, is_cu=False):
        SimpleSymbolDict.__init__(self, symbol_dict, aux_dict=aux_dict,
                                  filter=filter, factory=factory)
        """
        :p symbol_dict Primary ScopedSymbolDict containing symbols
        :p cu_dict ScopedSymbolDict containing the CU paths as "nested scopes"
        (used for CU name lookup/disambiguation)
        :p aux_dict Optional ScopedSymbolDict containing extra symbols
        :p filter Predicate callable taking a key and a symbol reference, used
        to filter out elements of the underlying
        :p factory Callable taking a key and a symbol.  Note: this factory
        is distinct from the symbol_ref->symbol factory applied by the underlying
        symbol_dict.
        :p elide_top_level_scope Drop the top-level scope when returning keys
        and applying scope-level operations 
        :p is_cu Indicates that this SymbolDict represents a single CU, rather 
        than a collection of CUs.  If this is True, elide_top_level_scope should
        be False.
        """
        if is_cu and elide_top_level_scope:
            raise ValueError("SymbolDict construction error: Setting "
                             "'elide_top_level_scope' when 'is_cu' is set doesn't "
                             "make sense")
        self._is_cu = is_cu
        self._cu_dict = cu_dict # needed to look up partial CU references
        self._elide_top_level_scope = elide_top_level_scope

    def _clone(self, symbol_dict, aux_dict=None,
               elide_top_level_scope=False, is_cu=True):
        """
        Create an equivalent object with a modified symbol dict.  The CU-related
        defaults reflect the fact that this function is normally used to create
        SymbolDicts corresponding to sub-scopes.
        :p symbol_dict Primary ScopedSymbolDict containing symbols
        :p aux_dict Optional ScopedSymbolDict containing extra symbols
        :p elide_top_level_scope Drop the top-level scope when returning keys
        and applying scope-level operations 
        :p is_cu Indicates that this SymbolDict represents a single CU, rather 
        than a collection of CUs.  If this is True, elide_top_level_scope should
        be False
        """
        return SymbolDict(symbol_dict, self._cu_dict, filter=self._filter,
                          factory=self._factory,
                          aux_dict=aux_dict,
                          elide_top_level_scope=elide_top_level_scope,
                          is_cu=is_cu)

    def name_cu_from_key(self, key):
        """
        Split the given scoped_name (key) into a compilation unit part and the
        rest (the "name", joined using the standard separator), returning a
        (name, cu) pair.  If this SymbolDict represents a CU, return the cu part
        as None
        """
        if self._is_cu:
            return self._sep.join(key), None
        cu = key[0]
        name = self._sep.join(key[1:])
        return name, cu

    def _normalise_cu_in_key(self, key):
        """
        Handle the variant forms of a scoped_name (key) that includes a 
        specification of a CU that may be supplied to a method like __getitem__.  
        
        The key is allowed to take the following forms:
         - "::"-separated string in outer-to-inner scoping order with CU as 
         outermost substring
        - Equivalent tuple
        - (legacy) ":"-separated string of the form name:cu, or name:None for
        globals
        - (legacy) Equivalent tuple 
        
        Where there is a CU component this is allowed to be supplied as a 
        partial path (from basename outwards); this method normalises that to
        a full path
        
        """
        if self._is_cu:
            return key # Nothing to do - there should be no CU component
        
        if self._sep in key:
            key_cmpts = key.split(self._sep)
            if "." in key_cmpts[0]:
                # Looks like a CU name
                cu = self._cu_dict.get_path_from_fragment(key_cmpts[0])
                return self._sep.join([cu]+key_cmpts[1:])
            else:
                return key # Nothing to do - apparently no CU component
        elif ":" in key:
            # name:cu format
            name, cu = key.split(":")
            cu = self._cu_dict.get_path_from_fragment(cu)
            if cu is None:
                raise KeyError("%s doesn't contain a valid CU component" % key)
            # Convert to canonical flat-string format
            return self._sep.join((cu, name))
        elif isinstance(key, str):
            return key # Nothing to do - there should be no CU component
        elif isinstance(key, tuple):
            if len(key) == 2 and key[1] is None: # legacy way of specifying a global
                return key[0]
            elif "." in key[0]:
                cu, name = key[0], key[1:]
                cu = self._cu_dict.get_path_from_fragment(cu)
                if cu is None:
                    raise KeyError("%s doesn't contain a valid CU component" % str(key))
                return (cu,) + name
            elif len(key) == 2 and "." in key[1]:
                # legacy (name, cu) tuple format
                cu, name = key[1], key[0]
                cu = self._cu_dict.get_path_from_fragment(cu)
                if cu is None:
                    raise KeyError("%s doesn't contain a valid CU component" % str(key))
                # Convert to the canonical tuple format
                return (cu,name)
            else:
                return key # Nothing to do - apparently no CU component
        else:
            raise TypeError("Expected string or tuple for symbol lookup, not "
                            "%s" % type(key).__name__)

    def keys(self):
        """
        Return all the symbol keys for the entire nested scope set as a list
        """
        if self._elide_top_level_scope:
            return [k[1:] for k in super(SymbolDict, self).keys()]

        return list(super(SymbolDict, self).keys())

    def values(self):
        return (self[k] for k in self.keys())
    
    def items(self):
        return ((k, self[k]) for k in self.keys())
    
    def __iter__(self):
        return iter(self.keys())
    

    def _apply_scope_action(self, scope, scope_create, scope_populate,
                            scope_finalise=None,
                            elide_top_level_scope=False,
                            name_remapping=None):
        """
        Recursively apply supplied creation, population and (optionally) 
        finalisation functions to subscopes of the given scope object by looping
        over the subscopes in the dictionary
        :p scope An arbitrary object that to be populated with subscopes 
        :p scope_create Callable that takes a sub-scope name and the parent
        scope object and returns a new or existing subscope of that name
        :p scope_populate Callable that takes a scope object and the
        SymbolDict for it and inserts all symbols at that scope level into the
        scope object
        :p scope_finalise Optional callable that marks the end of construction
        of the scope and all its nested members
        :p elide_top_level_scope Whether to create separate subscopes for the
        top-level scope or not (set to False to drop CUs as scopes, for instance)
        :p name_remapping Dictionary of replacement names, intended for 
        disambiguating clashing scope/symbol names when CU scopes have been
        dropped
        """
        
        if name_remapping is None:
            name_remapping = {}
        
        def get_subscope_rename_and_remapping(name_remapping,
                                              subscope_name):
            subscope_name_or_remapping = name_remapping.get(subscope_name,
                                                                      subscope_name)
            if isinstance(subscope_name_or_remapping, ScopedSymbolDict):
                # This scope isn't renamed, but there are renamings below
                subscope_remapping = subscope_name_or_remapping
                #subscope_name is unchanged
                return subscope_name, subscope_remapping 
            else:
                # This scope is renamed (possibly trivially).  This means 
                # there are no subscope renamings below it
                subscope_name = subscope_name_or_remapping
                return subscope_name, None
        
        if not elide_top_level_scope:
            # Create 
            for subscope_name, subscope_dict in self.nested_scopes().items():
                subscope_name, subscope_remapping = get_subscope_rename_and_remapping(
                                        name_remapping, subscope_name)
                subscope = scope_create(subscope_name, scope)
                scope_populate(subscope, subscope_dict,
                               name_remapping=subscope_remapping)
                subscope_dict._apply_scope_action(subscope, scope_create,
                                                 scope_populate,
                                                 scope_finalise=scope_finalise,
                                                 elide_top_level_scope=False,
                                                 name_remapping=subscope_remapping)
                if scope_finalise:
                    scope_finalise(subscope)
        else:
            for subscope_name, subscope_dict in self.nested_scopes().items():
                if subscope_name == "<globals>":
                    continue
                subscope_name, subscope_remapping = get_subscope_rename_and_remapping(
                                        name_remapping, subscope_name)
                scope_populate(scope, subscope_dict, 
                               name_remapping=subscope_remapping)
                subscope_dict._apply_scope_action(scope, scope_create,
                                                 scope_populate,
                                                 scope_finalise=None,
                                                 elide_top_level_scope=False,
                                                 name_remapping=subscope_remapping)

    def build_scope_tree(self, scope_create, scope_populate, scope_finalise=None,
                         name_remapping=None, elide_top_level_scope=False):
        """
        Build a top-level scope and then recurse on the nested scopes
        :p scope_create Callable that takes a sub-scope name and the parent
        scope object and returns a new or existing subscope of that name
        :p scope_populate Callable that takes a scope object and the
        SymbolDict for it and inserts all symbols at that scope level into the
        scope object
        :p scope_finalise Optional callable that marks the end of construction
        of the scope and all its nested members
        :p elide_top_level_scope Whether to create separate subscopes for the
        top-level scope or not (set to False to drop CUs as scopes, for instance)
        :p name_remapping Dictionary of replacement names, intended for 
        disambiguating clashing scope/symbol names when CU scopes have been
        dropped
        """
        
        if name_remapping:
            # Turn a key-oriented normal dictionary into a scoped dictionary
            name_remapping = ScopedSymbolDict(name_remapping, lambda k,x:x,
                                              unflatten_dict=True, scope_sep=None)
        
        scope = scope_create(None, None) # default create
        scope_populate(scope, self)
        self._apply_scope_action(scope, 
                                 scope_create,
                                 scope_populate,
                                 scope_finalise=scope_finalise,
                                 name_remapping=name_remapping,
                                 elide_top_level_scope=elide_top_level_scope)
        if scope_finalise:
            scope_finalise(scope)
        return scope
    
    def ambiguous_name_remapping(self, cu_dict, scope_remappings=None):
        """
        Returns a dictionary mapping those keys which are identical to another
        key modulo the CU name to an alternative key built by appending a
        minimal but sufficient part of the CU name to the highest-level non-global
        key element (i.e. which will be the innermost name, except for the case
        of identically-named static functions which contain identically-named
        static variables)
        :p cu_dict SimpleSymbolDict of CU paths
        :p scope_remappings Pre-compupted remappings of scopes that may be
        present in this dictionary
        """
        
        if self._is_cu:
            return None
        
        remappings = {}
        for name in self._symbol_dict.index:
            non_cu_subindex = {}
            for entry in self._symbol_dict.index[name]:
                non_cu_subindex.setdefault(entry[1:], []).append(entry)
                
            for entry_list in non_cu_subindex.values():
                if len(entry_list) > 1:
                    # Need to create a disambiguated version of the name
                    
                    # First see if the immediately enclosing scope is in the
                    # supplied disambiguations (if any) - this happens if the
                    # ambiguity is in the name of a static function. We can
                    # ignore this if so.
                    for entry in entry_list:
                        if not scope_remappings or entry[:-1] not in scope_remappings:
                            # if not, add a suitable disambiguating suffix to the name
                            minimal_cu_string = "_".join(cu_dict.minimal_unique_subkey(
                                                         entry[0], join=False)).split(".")[0]
                            remappings[entry] = entry[-1]+"__"+minimal_cu_string

        # merge name remappings with scope remappings to create an overall
        # set of remappings
        if scope_remappings:
            remappings.update(scope_remappings)
                
        return remappings
    
    def search(self, sym_regex):
        """
        Search for all symbols matching the regular expression, returning the
        results in a CU-based dictionary, with the CU names minimalised
        :p sym_regex Regular expression to match against symbol names
        """
        key_matches = self._symbol_dict.search(sym_regex)
        if self._aux_dict:
            aux_dict_matches = self._aux_dict.search(sym_regex)
            key_matches += aux_dict_matches

        matches = {}
        for key in key_matches:
            name, cu = self.name_cu_from_key(key)
            try:
                minimal_cu = self._cu_dict.minimal_unique_subkey(cu)
            except ValueError:
                minimal_cu = cu
            matches.setdefault(minimal_cu, []).append(name)
        return matches
        

class AddressableSymbolDict(SymbolDict):
    """
    Subclass of SymbolDict that adds generic logic for performing 
    address-based look-up.  This is used for making the DWARF's vars and funcs
    dicts searchable by address, to provide methods like get_function_of_pc and
    get_var_at_address.  Also provides an info method that prints size information
    that can be filtered in various ways.
    """
    
    def __init__(self, symbol_dict, factory, cu_dict, toolchain,
                 aux_dict=None,
                 address_underflow_excep=ValueError,
                 address_overflow_excep=ValueError,
                 is_cu=False):
        """
        :param symbol_dict: Primary ScopedSymbolDict containing symbols
        :param factory: Callable taking a key and a symbol.  Note: this factory
         is distinct from the symbol_ref->symbol factory applied by the underlying
         symbol_dict.
        :param cu_dict: ScopedSymbolDict containing the CU paths as "nested scopes"
         (used for CU name lookup/disambiguation)
        :param toolchain: Misc metadata about the compiler toolchain
        :param aux_dict: Optional ScopedSymbolDict containing extra symbols
        :param address_underflow_excep: Exception to raise if a search-by-address
         fails because the address is too low to be inside any named symbol.
         Set to False to disable exceptions being raised for this scenario. 
        :param address_overflow_excep: Exception to raise if a search-by-address
         fails because the address is too high to be inside any named symbol.  
         This is also raised if the address lies beyond the end of a symbol but
         before the start of any higher-addressed one.  Set to False to disable
         exceptions being raised for these scenarios. 
        :param is_cu: Indicates that this represents a single CU, rather 
         than a collection of CUs. 
        """
        
        SymbolDict.__init__(self, symbol_dict, cu_dict, filter=None,
                            aux_dict=aux_dict,
                            factory=factory,
                            elide_top_level_scope=False,
                            is_cu=is_cu)

        self._garbage_address = (self._adjust_address(toolchain.GARBAGE_ADDRESS) 
                                     if toolchain.GARBAGE_ADDRESS is not None 
                                     else None)
        self._toolchain = toolchain
        self.AddressUnderflowExcep = address_underflow_excep
        self.AddressOverflowExcep = address_overflow_excep
    
    @display_hex
    def __getitem__(self, symbol_or_address):
        """
        Look up by name or address: name look-up may return an API object, in the
        case of VarDict or simply the object's address, in the case of 
        FunctionDict (this is controlled by a class attribute).
        
        symbol_or_address can take the following forms:
         - an integer: this is interpreted as a program address, and the name of
         the entity in which this falls, fully scope-qualified, is returned 
         - a string, containing either
          -- an unambiguous name, with or without scope qualification ("::"-separated)
          -- a name qualified by a /-separated CU path fragment, with the CU
          either supplied as an outer scope (i.e. prepended and "::"-separated,
          e.g. "path/to/my_cu.c::my_namespace::my_func")
          or as a suffix (i.e. appended and ":"-separated,  e.g. 
          "my_func:path/to/my_cu.c". Note that the second form is only supported
          for C, not C++.
         - a tuple, containing the same elements as for the qualified
         function name.  In the tuple form of the "CU suffix" notation the CU
         entry (the second) can be None, indicating a global.
         
         Note: in the case where this dictionary refers to a single CU,
         explicit CU qualification of string and tuple keys is not supported,
         naturally: names must simply be unambiguous in the target CU (i.e.
         contain sufficient scoping qualification).
        """
        if isinstance(symbol_or_address, int_type):
            try:
                key = self.find_by_address(symbol_or_address)
            except (self.AddressUnderflowExcep, self.AddressOverflowExcep):
                raise KeyError("Address 0x%x lies outside any addressable "
                               "entity" % symbol_or_address)
            if key is None:
                return None
            name, cu = self.name_cu_from_key(key)
            return name
        
        else:
            symbol_or_address = self._normalise_cu_in_key(symbol_or_address)
        
            # We're trying to do fundamentally different things with 
            # __getitem__[<string>] between VarDict (return the variable
            # object itself) and FunctionDict (return the address)
            try:
                key = self._symbol_dict.lookup_key(symbol_or_address)
                if self._name_lookup_returns_address:
                    return self._get_address(key)
                else:
                    return super(AddressableSymbolDict, self).__getitem__(key)
                    
            except self._symbol_dict.UnknownNameExcep as e:
                if self._aux_dict:
                    aux_key = self._aux_dict.lookup_key(symbol_or_address)
                    if aux_key:
                        if self._name_lookup_returns_address:
                            return self._get_address(aux_key)
                        else:
                            return self._aux_dict[aux_key]
                raise KeyError(symbol_or_address)
                
            else:
                raise TypeError("Lookup is by name or address. "
                                 "Supplied type is '%s' "
                                 "(supplied value is '%s')" % (
                                                   type(symbol_or_address),
                                                   str(symbol_or_address)))

    def _clone(self, symbol_dict, aux_dict=None, is_cu=True):
        """
        Create an equivalent object with a modified symbol dict.  The CU-related
        default reflects the fact that this function is normally used to create
        AddressableSymbolDicts corresponding to sub-scopes.
        :p symbol_dict Primary ScopedSymbolDict containing symbols
        :p aux_dict Optional ScopedSymbolDict containing extra symbols
        :p is_cu Indicates that this SymbolDict represents a single CU, rather 
        than a collection of CUs.  If this is True, elide_top_level_scope should
        be False
        """
        return AddressableSymbolDict(symbol_dict, self._factory, self._cu_dict,
                                     self._toolchain,
                                     aux_dict=aux_dict,
                                     address_underflow_excep=self.AddressUnderflowExcep,
                                     address_overflow_excep=self.AddressOverflowExcep,
                                     is_cu=is_cu)

    @property
    def _addr_index(self):
        """
        Mapping from start addresses to dictionary keys to enable look-up-by-
        address
        """
        try:
            self._addr_index_
        except AttributeError:
            self._addr_list_, self._addr_index_ = self._build_index()
        return self._addr_index_

    @property
    def _addr_list(self):
        """
        Ordered list of start addresses to enable efficient search-by-address
        """
        try:
            self._addr_list_
        except AttributeError:
            self._addr_list_, self._addr_index_ = self._build_index()
        return self._addr_list_
    
    def _build_index(self):
        """
        Put all the entries into a list sorted by address (which is defined by
        the virtual function self._get_address(key) 
        """
        addr_to_key_map = {self._get_address(k):k for k in self._symbol_dict.keys()}
        # If the address evaluates to None, we can't handle it
        try:
            del addr_to_key_map[None]
        except KeyError:
            pass
        if self._aux_dict:
            aux_addr_to_key_map = {self._adjust_address(v.address):k 
                                           for (k,v) in self._aux_dict.items()}
            # Overwrite any aux addresses with the main ones
            # for flashheart tends to overwrite munged/secret symbols with real information
            aux_addr_to_key_map.update(addr_to_key_map)
            addr_to_key_map =  aux_addr_to_key_map
        return sorted(addr_to_key_map.keys()), addr_to_key_map
                 
    
    def find_by_address(self, addr, result_factory=None):
        """
        Return the key corresponding to the entity which covers the given address
        if any.  Before returning this will be passed to the supplied 
        result_factory callable if supplied.  If no entity covers the given 
        address a self.AddressOverflowExcep or self.AddressUnderflowExcep is 
        raised if the object was constructed with these; otherwise None is
        returned.
        :p addr The address to look up
        :p result_factory An optional callable to pass any found key to
        """
        next_index = bisect(self._addr_list, addr)
        if next_index == 0:
            # this address is before the start of the list - whoops
            if self.AddressUnderflowExcep:
                raise self.AddressUnderflowExcep("0x%x is before the lowest-addressed entity" % addr)
            return None
        matching_index = next_index -1
        # Otherwise check that the address doesn't fall into the gap between
        # two successive entries
        matching_addr =  self._addr_list[matching_index]
        matching_key = self._addr_index[matching_addr]
        size = self._get_size(matching_key)
        if size is not None:
            entry_end_addr = matching_addr + size 
        else:
            try:
                # Just use the start of the next entity, if we can
                entry_end_addr = self._addr_list[matching_index+1]
            except IndexError:
                entry_end_addr = 0xffffffff # infinity
        if addr >= entry_end_addr:
            # not sure if "overflow" is the right idea if we're in the gap between
            # two entities.
            if self.AddressOverflowExcep:
                raise self.AddressOverflowExcep("0x%x is after the end of entry "
                                                "%d, 0x%x" % (addr, matching_index,
                                                              entry_end_addr))
            return None
        
        return (matching_key if result_factory is None else 
                                              result_factory(matching_key))
        
    def find_in_container(self, addr_range, result_factory=None):
        """
        Find all objects whose address is in the given container
        :p addr_range An object implementing the "in" operator for address values
        :p result_factory An optional callable to pass any found keys to
        """
        fact = (lambda x : x) if result_factory is None else result_factory
        elements = [fact(self._addr_index[addr]) for addr in self._addr_list 
                                             if  addr in addr_range]
        for e in elements:
            if e.size is None:
                ## We'd like to do something like this, but we can't rely on
                ## the ordered list of functions to be contiguous
                #try:
                #    e.size = self._addr_list[i+1]-self._addr_list[i]
                #except IndexError:
                #    e.size = 0
                ## So we just do this
                e.size = 0
        return set(elements)
    
    def symbol_is_garbage(self, symbol):
        """
        Return True if the toolchain emits garbage-collected symbols and this
        symbol is marked as such.
        """
        if self._garbage_address is None:
            return False
        return self.key_is_garbage(self._symbol_dict.lookup_key(symbol))
    
    def key_is_garbage(self, key):
        """
        Return True if the toolchain emits garbage-collected symbols and the
        symbol corresponding to this scope key is marked as such.
        """
        if self._garbage_address is None:
            return False
        return self.address_is_garbage(self._get_address(key))
        
    def address_is_garbage(self, address):
        """
        Return True if the toolchain emits garbage-collected symbols and the
        given address is the magic address given to garbage-collected symbols.
        """
        return address == self._garbage_address
    
    def _adjust_address(self, address):
        """
        Apply any required adjustments to any address obtained from the ELF file
        (e.g. clear flag bits)
        
        The base implementation does nothing.
        
        :param address: A raw address obtained from the ELF
        :returns: A corrected address which is more meaningful to users
        """
        return address
        
    def _get_address(self, scoped_name):
        """
        Do a context-specific address lookup given the name of the symbol
        :p scoped_name Full name of a symbol, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        raise NotImplementedError
    
    def _get_size(self, scoped_name):
        """
        Do a context-specific size lookup given the name of the symbol
        :p scoped_name Full name of a symbol, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        raise NotImplementedError
    
    
    def _get_info(self, address_range, report_name,
                  cu_filter=None, size_filter=None, output_limit=None,
                  ordering=None, report=False, known_size_bytes=None,
                  remainder_name="REMAINDER"):
        """
        Return tabulated information about the arbitrary entities in the firmware.  
    
        :param address_range The range of addresses in which to search.  This is
        expected to be a type supporting the "in" operator for addresses 
        Should ideally be convertible to a string describing the range in some suitable
        way. (One way to construct arbitrary ranges is to use the class
        AddressRange from csr.dev.hw.address_space).
        :param report_name Title for the report group
        
        The entities can be filtered on
         - compilation unit name
         - size (bytes)
         - address range
        The respective arguments are
        :param cu_filter Regular expression to find within the trimmed CU path
        name, or arbitrary callable taking a path and returning boolean (the 
        callable must have a __name__ attribute, which is used to describe the 
        output)
        :param size_filter Fixed minimum size or arbitrary callable taking a byte
        size and returning a boolean (the callable must have a __name__ attribute, 
        which is used to describe the output)
        :param known_size_bytes Optional indication of the total size of the
        symbols that should be found (after filtering).  If this is supplied, and 
        the calculated size is less than this, an
        entry <remainder_name> will be added representing the remainder of the space.
        This might correspond for example to assembly functions whose sizes aren't
        available.
        :param remainder_name Optional friendly name to give the "remainder" entry,
        if there is one
        
        There are also options to control formatting: 
        :param output_limit Max number of variables to display. Unlimited by default
        :param ordering Callable taking an iterable of _VarDict.VarReport objects
        which should return the same items in a list.  By default, variables are 
        ordered largest first
        """
            
        if size_filter is None:
            size_filter = lambda size : True # No filter on size
            size_filter.__name__ = "no limit"
        elif isinstance(size_filter, int_type):
            # Interpret a constant value as being a lower bound
            lower_limit = size_filter
            size_filter = lambda size : (size >= lower_limit)
            size_filter.__name__ = "at least %d" % lower_limit
            
        if ordering is None:
            # Default to ordering on size, largest first
            ordering = lambda the_set : sorted(the_set, key=lambda s : (-s.size, s.name, s.cu))
        
        if cu_filter is None:
            # default to no filtering on compilation unit
            cu_filter = lambda name : True
            cu_filter.__name__ = "none"
        elif isinstance(cu_filter, str):
            name_match = cu_filter
            cu_filter = lambda cu : cu is not None and re.search(name_match,cu)
            cu_filter.__name__ = "regexp match '%s'" % name_match
                    
        def build_fw_report(key):
            name, cuname = self.name_cu_from_key(key)
            return FwReport(name, cuname, self._get_address(key), self._get_size(key))
        # Now get the list the caller asked for
        range_set = self.find_in_container(address_range, result_factory=build_fw_report)
    
        total_size_range = 0
        for e in range_set:
            total_size_range += e.size
    
        # Calculate the common prefix here even though most of these will 
        # probably be thrown away to avoid stripping off too much - if we do it 
        # later, we might lose the entire path if we end up filtering down to a 
        # single file
        common_prefix = os.path.commonprefix([e.cu for e in range_set 
                                                        if e.cu is not None and
                                                        os.path.basename(e.cu) != e.cu])
        # Limit the common prefix to the last separator
        last_slash = common_prefix[::-1].find("/")
        if last_slash == -1:
            last_slash = common_prefix[::-1].find("\\")
        if last_slash > 0: # don't trim if no slash or last slash already at the
                           # end
            common_prefix = common_prefix[:-1*last_slash]
        
        # First trim off uninteresting path fragments from the CUs
        len_common_prefix = len(common_prefix)
        def trim(pth):
            if pth is None:
                return "-"
            if pth.startswith(common_prefix):
                return pth[len_common_prefix:]
            return pth
    
        # Also scale word sizes to byte sizes
        scale_to_bytes = self._layout_info.addr_unit_bits // 8
        range_set = {FwReport(e.name, trim(e.cu), hex(e.address).rstrip('L'), 
                                    e.size*scale_to_bytes) 
                                    for e in range_set}
        total_size_range *= scale_to_bytes
        
        filtered_set = {entry for entry in range_set 
                         if cu_filter(entry.cu) and 
                            size_filter(entry.size) and entry.size > 0}
        
        total_size_filtered = 0
        for e in filtered_set:
            total_size_filtered += e.size
    
        if known_size_bytes is not None and known_size_bytes > total_size_filtered:
            remainder_size = known_size_bytes - total_size_filtered
            filtered_set.add(FwReport(remainder_name,"(n/a)","(n/a)", 
                                      remainder_size))
            total_size_filtered += remainder_size
            total_size_range += remainder_size
    
        ordered_list = ordering(filtered_set)
        final_list = ordered_list if output_limit is None else ordered_list[:output_limit]
        # Now format the list as a table
        
        group = Group(report_name)
        region_info = Text()
        descr = Text("Address range(s): %s. CU filter: %s. Size filter: %s.  "
                     "Output limit: %s" % (address_range, cu_filter.__name__,
                                           size_filter.__name__,
                                           output_limit))
        if scale_to_bytes == 1:
            tab = Table(("Name", "CU", "address", "size (bytes)"))
        else:
            tab = Table(("Name", "CU", "word address", "size (bytes)"))
        for row in final_list:
            tab.add_row(row)
        group.append(descr)
        group.append(tab)
        group.append(Text("Total size in range: %d bytes" % total_size_range))
        group.append(Text("Size of filtered set: %d bytes" % total_size_filtered))
        if report:
            return [group]
        TextAdaptor(group, gstrm.iout)


class ScopeContainer(FrozenNamespace, LazyAttribEvaluator):
    """
    Plain container class that supports insertion of attributes into nested
    scopes
    """
    _type_int = 0
    
    def __init__(self, scope_dict=None):
        class _Adapter(object):
            def __init__(self, d):
                self._d = d
            def keys(self):
                return self._d.local_symbols()
            def __getattr__(self, attr):
                return getattr(self._d, attr)
            def __getitem__(self, key):
                return self._d[key]
        
        LazyAttribEvaluator.__init__(self, _Adapter(scope_dict) if scope_dict else None)
        FrozenNamespace({}, delay_finalisation=True)

    @classmethod
    def create(cls, scope_name, parent_scope):
        """
        Creae a new uniquely-typed ScopeContainer as an attribute of the parent 
        scope if no such attribute exists already
        :p scope_name Name for the attribute
        :p parent_scope Scope in which to search for/insert the attribute; may
        be None
        Returns the created subscope after inserting it into the parent scope if
        supplied.
        """
        if parent_scope:
            try:
                # Has this subscope already been created?
                return getattr(parent_scope, scope_name)
            except AttributeError:
                # Needs to be created - carry on
                pass
        subscope_type, cls._type_int = unique_subclass(ScopeContainer, id_hint=cls._type_int)
        subscope = subscope_type()
        if parent_scope:
            setattr(parent_scope, scope_name, subscope)
        return subscope

class _ScopeDictKeysWrapper(object):
    """
    Make a SimpleSymbolDict look like a dictionary for ScopeContainer building
    """
    def __init__(self, scope_dict):
        self._s = scope_dict
    def keys(self):
        return self._s.local_symbols()
    
    def __getitem__(self, k):
        return self._s[k]

def scope_container_populate(scope, scope_dict, name_remapping=None):
    scope.populate(_ScopeDictKeysWrapper(scope_dict), name_remapping=name_remapping)

def scope_container_finalise(scope):
    scope.freeze()


def build_scope_container(scope_dict, **kwargs):
    """
    Create an attribute-based API container from  the supplied SymbolDict
    """
    return scope_dict.build_scope_tree(ScopeContainer.create,
                                       scope_container_populate,
                                       scope_container_finalise, **kwargs)
        


class FunctionDict(AddressableSymbolDict):
    """
    Provide the legacy function API using the unified base class that supplies
    scope-based look-up and ordering/searching logic.
    
    Subtleties:
        1. Function naming involves resolving a set of possible aliases.  This
        means that any public method that wants to return names must pass the
        underlying dictionary key through a resolution call.  This will be hidden
        inside an override of name_cu_from_key.
        2. Resolving aliases, and other things, comes by combining information
        from the symbol table, so we need to pass in ELF symbol info as well as
        the DWARF if we want to do that.  But we need to accommodate that not
        being present (as will be the case with C++, and hopefully all future
        distinguishable instances)
        3. The patch lookup adjustment function needs to be supported, primarily
        by having a private method that knows how to construct offset versions 
        of the raw DWARF function symbols so that get_function_of_pc can return
        the latter (and __getitem__ can get its addresses from them too)
    
    """    
    _name_lookup_returns_address = True

    def __init__(self, dwarf_funcs, 
                 func_syms, extra_func_syms, minim_ranges,
                 cu_dict,
                 layout_info, 
                 toolchain,
                 sym_tab_dwarf_funcs=None,
                 is_cu=False):
        """
        :p dwarf_funcs Primary ScopedSymbolDict containing DWARF function symbols
        :p func_syms ScopedSymbolDict containing extra function symbols from the
        ELF
        :p extra_func_syms: Some extra function names that may be useful for alias
        resolution
        :p minim_ranges: Ranges of addresses for which functions are compiled to
        run in minim mode (Kalimba only)
        :p cu_dict ScopedSymbolDict containing the CU paths as "nested scopes"
        (used for CU name lookup/disambiguation)
        :p layout_info Layout metadata for the processor architecture, of type
        ILayoutInfo
        :p toolchain Miscellaneous metadata for the compiler toolchain (assumed
        unique) 
        :p sym_tab_dwarf_Funcs: alternative container of DWARF symbols for the
        patch lookup adjustment case, where the DWARF symbols exist in two forms: one with
        inaccurate function address data but otherwise complete information and
        the other (this one) with accurate function addresses but not much else
        :p is_cu Indicates that this represents a single CU, rather 
        than a collection of CUs. 
        """
        # Set before instantiating AddressableSymbolDict part because it needs to
        # call adjust_address.
        self._addr_flag = toolchain.FUNC_ADDR_FLAG
        self._mode_flag = toolchain.FUNC_MODE_FLAG

        AddressableSymbolDict.__init__(self, dwarf_funcs, self._factory, cu_dict,
                                       toolchain=toolchain,
                                       aux_dict=func_syms,
                                       address_underflow_excep=BadPCLow,
                                       address_overflow_excep=BadPCHigh,
                                       is_cu=is_cu)
        
        self._sym_tab_dwarf_funcs = sym_tab_dwarf_funcs if sym_tab_dwarf_funcs else dwarf_funcs
        self._minim_ranges = minim_ranges
        self._layout_info = layout_info
        self._extra_func_syms = extra_func_syms
        self._toolchain = toolchain

    def _clone(self, symbol_dict, aux_dict=None, is_cu=True):
        """
        Create an equivalent object with a modified symbol dict.  The CU-related
        default reflects the fact that this function is normally used to create
        AddressableSymbolDicts corresponding to sub-scopes.
        :p symbol_dict Primary ScopedSymbolDict containing symbols
        :p aux_dict Optional ScopedSymbolDict containing extra symbols
        :p is_cu Indicates that this SymbolDict represents a single CU, rather 
        than a collection of CUs.  If this is True, elide_top_level_scope should
        be False
        """
        return FunctionDict(symbol_dict, aux_dict, 
                            self._extra_func_syms, self._minim_ranges,
                            self._cu_dict,
                            self._layout_info, 
                            self._toolchain,
                            sym_tab_dwarf_funcs=self._sym_tab_dwarf_funcs,
                            is_cu=is_cu)

    def _factory(self, key, raw_dwarf_sym):
        """
        On patch lookup adjustment cases we have to map the raw DWARF API objects to offset versions
        because the symbols with correct addresses are different from the symbols
        with full info.  Sigh.
        """
        if self._sym_tab_dwarf_funcs is self._symbol_dict:
            # non-patch lookup adjustment case
            return raw_dwarf_sym
        
        # Look up the symbol in the sym tab DWARF and construct a wrapper
        # API object around the raw symbol from the _patched_syms ELF that adjusts
        # the address and size values appropriately
        try:
            # On patch lookup adjustment case, if we've loaded the symbol table from the
            # "real" ELF but the debug from the _patched_syms ELF, we'll
            # find that most functions start 8 bytes earlier than they
            # are listed as doing in the _patched_syms ELF
            shifted_sym = self._sym_tab_dwarf_funcs[key]
        except self._sym_tab_dwarf_funcs.UnknownNameExcep:
            # That DWARF doesn't know about this function, so we can't
            # account for a possible preamble
            return raw_dwarf_sym
        else:
            return raw_dwarf_sym.get_offset_api(shifted_sym.address, size)

    @property
    def _aliases(self):
        """
        Build the dictionary of aliases on demand.  This maps the actual names
        in keys (with full scope qualification) into the designated "public"
        name
        """
        try:
            self._aliases_
        except AttributeError:
            self._aliases_ = self._build_aliases()
        return self._aliases_
    
    def _build_aliases(self):
        """
        Build a dictionary which for every key in the main symbol dictionary 
        returns its "public name"
        """
        alias_lists = {}
        
        # Build a dictionary mapping function addresses to the list of names
        # that share that address (i.e. aliases for the same function)
        function_names=set()
        #for name, cuname, func in self._sym_tab_dwarf.function_list:
        for key, func in self._sym_tab_dwarf_funcs.items():
            key_name = key if self._is_cu else key[1:]
            name = self._sym_tab_dwarf_funcs.sep.join(key_name)
            try:
                if func.size > 0:
                    alias_lists.setdefault(func.address,[]).append(name)
            except ValueError:
                pass
            function_names.add(name)
        if self._aux_dict:
            for key in self._aux_dict.keys():
                name = key[-1]
                if name not in function_names:
                    addr = self._aux_dict[key].address
                    alias_lists.setdefault(addr,[]).append(name)
                
        # Also add the more dubious-looking function symbol names from the
        # original parsing effort in case they're better than the DWARF
        # name (case in point - QCC512X_QCC302X Audio ELF has "$M.interrupt.handler"
        # in the DWARF but "interrupt.handler" in the ELF.  Even though we
        # generally ignore ELF symbols with dots, in this case that's the
        # best name we have.
        for name, addr in self._extra_func_syms.items():
            if name not in function_names:
                alias_lists.setdefault(addr,[]).append(name)

        def select_best_alias(alias_list):
            # Fewest special characters (., $, double-_), then fewest 
            # capital letters
            for char_class in (r"(\.|\$|__)", r"[A-Z]"):
                num_matches = [len(re.findall(char_class, a)) 
                                                       for a in alias_list]
                lowest = min(num_matches)
                alias_list = [a for (a,n) in zip(alias_list, num_matches) 
                                                            if n == lowest]
                if len(alias_list) == 0:
                    return alias_list[0]
            # If there are still ambiguous names, just pick one
            return alias_list[0]
            
        # Now turn the address->names mapping into a names->chosen alias
        # mapping
        aliases = {}
        for funcs in alias_lists.values():
            if len(funcs) > 1:
                alias = select_best_alias(funcs)
                for f in funcs:
                    aliases[f] = alias
        return aliases

    # Required private virtual functions for AddressableSymbolDict
    
    def _adjust_address(self, address):
        return address & ~(self._addr_flag | self._mode_flag)
    
    def _get_address(self, scoped_name):
        """
        Return the address of a given scoped_name (i.e. a key for the underlying
        ScopedSymbolDicts)
        :p scoped_name Full name of a symbol, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        try:
            return self._adjust_address(self._symbol_dict[scoped_name].address)
        except KeyError:
            if self._aux_dict:
                symbol = self._aux_dict.get(scoped_name)
                if symbol:
                    return self._adjust_address(symbol.address)
            raise
        except DwarfFunctionAddressRangeNotPresent:
            if self._aux_dict:
                symbol = self._aux_dict.get(scoped_name)
                if symbol:
                    return self._adjust_address(symbol.address)
                symbol = self._aux_dict.lookup_symbol(scoped_name[-1])
                if symbol:
                    return self._adjust_address(symbol.address)
            raise
        
    def _get_size(self, scoped_name):
        """
        Return the address of a given scoped_name (i.e. a scoped_name for the underlying
        ScopedSymbolDicts)
        :p scoped_name Full name of a symbol, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        try:
            return self._symbol_dict[scoped_name].size
        except KeyError:
            if self._aux_dict:
                symbol = self._aux_dict.get(scoped_name)
                if symbol:
                    return symbol.size
            raise
    
    # overrides of inherited functionality
    def name_cu_from_key(self, key):
        """
        Split the given scoped_name (key) into a compilation unit part and the
        rest (the "name", joined using the standard separator), returning a
        (name, cu) pair.  If this SymbolDict represents a CU, return the cu part
        as None.  The raw name is converted to the canonical alias before
        being returned.
        """
        raw_name, cu = AddressableSymbolDict.name_cu_from_key(self, key)
        try:
            return self._aliases[raw_name], cu
        except KeyError:
            # no aliasing
            return raw_name, cu
        
    

    def __contains__(self, symbol_or_address):
        try:
            self.__getitem__(symbol_or_address)
        except KeyError:
            return False
        return True

    def get(self, symbol_or_address, default = None):
        """
        Identical to __getitem__ except that failure to look up symbol_or_address
         results in the default being returned.

        :p symbol_or_address Name or addres of a symbol
        :p default Value to return if the symbol reference is not present
        """
        try:
            return self.__getitem__(symbol_or_address)
        except KeyError:
            return default

    def keys(self):
        """
        Returns a list of the names of all functions in the firmware, with
        globals listed via just their names and static listed with the full
        compilation unit name in a tuple.  Naturally the names in this
        form are good for indexing with __getitem__.
        """
        if not self._is_cu:
            # Global function keys are presented in a legacy format
            legacy_keys = []
            for key in super(FunctionDict, self).keys():
                name, cu = self.name_cu_from_key(key)
                try:
                    is_global = self._symbol_dict[key].is_global
                except KeyError:
                    # We ought to catch the potential absence of aux_dict here,
                    # except we should be able to trust that keys are valid since
                    # we're calling the superclass's keys() to get them
                    is_global = self._aux_dict[key].is_global
                if is_global:
                    legacy_keys.append(name)
                else:
                    legacy_keys.append((name, cu))
            return legacy_keys
        
        return list(super(FunctionDict, self).keys())
        
    def get_function_of_pc(self, pc):
        """
        Return a triple of objects describing the function that contains
        the given program address.  The triple contains:
         - true function start address (with flags removed; this may not be
         equal to the call address due to mode flags)
         - function name, consisting of just the name if a global, else a
         (function name, cu name) tuple, where the cu_name is a "/"-separated
         minimal unique path fragment
         - a DWARF function API object, or None if there is no DWARF info
         for this function.
        :p pc The program address to look up
        """
        
        key = self.find_by_address(pc)
        addr = self[key]
        # get the raw symbol
        try:
            sym = self._symbol_dict[key]
        except KeyError:
            # Key must be valid so KeyError implies aux_dict is present and
            # contains the key
            sym = self._aux_dict[key]
        public_name, cu = self.name_cu_from_key(key)
        display_name = public_name if sym.is_global else (public_name, cu)
        return addr, display_name, sym
        

    def get_call_address(self, function):
        """
        Returns the address by which the given function is called, i.e. with
        any special mode bit set.
        
        :p function Symbol name (partially scoped) of the function to be looked up
        
        Note (for Kalimba): If you want a function address to set a function 
        pointer field to when writing a firmware structure, *you must use this 
        function to get the address* or else you risk running a MINIM function 
        in MAXIM mode, which isn't a good idea.
        """
        addr = self[function]
        if self.address_is_garbage(addr):
            return None
        return self.convert_to_call_address(addr)

    def convert_to_call_address(self, addr):
        """
        Convert the given address to one that can be called by the processor by
        applying suitable mode flags
        :p addr address to convert
        """
        # MINIM is specific to Kalimba, but this is harmless in other
        # cases because minim_ranges will be empty
        addr |= self._addr_flag
        addr |= any(addr >= region[0] and addr < region[1] for region in self._minim_ranges)
        return addr & ~self._addr_flag

    def get_offset_of_function_srcline(self, function, line_no):
        """
        Find the first instruction that maps to a source line that is
        at least the specified line 

        :param function: Symbol name of the function
        :param line_no: Target line number
        """
        func = self._symbol_dict.lookup_symbol(function)
        src_file, line = func.get_srcfile_and_lineno(0)
        if line_no < line:
            raise ValueError("line_no %d before start of %s in %s" % 
                             (line_no, function, 
                              os.path.basename(src_file)))
        offs = 0
        while line < line_no:
            try:
                _, line = func.get_srcfile_and_lineno(offs)
                offs += 1
            except RuntimeError:
                # The offset has probably got too large
                # Dodgy to catch a generic exception for a specific
                # purpose but this is coming from the extension module
                # and I didn't go as far as defining my own exception
                # classes in C...
                raise ValueError("Check if line %d is really inside "
                                 "%s" % (line_no, function) )
        return offs
        
    def get_size(self, func):
        """
        Return the length of the named function as the difference between
        its start address and the start address of the next function in the
        program
        """
        if self.symbol_is_garbage(func):
            return None
        return self._get_size(func)
    
    
    def info(self, address_range=None, **kwargs):
        """
        Return tabulated information about the functions in the firmware.  
        
        :param address_range Description of the range of addresses in which to 
        look for variables.  This can come in a few different forms:
         - an ordered iterable pair of integers - (low addr, high addr) 
         - an ordered iterable of such iterable pairs
            ** See csr.framework.meta.elf_firmware_info.fw_info_helper docs 
            for details of the arguments **
        - None, meaning all known functions.  THis is the default.
        """
        # It needs to be a pair of integers or a list of pairs of integers
        if address_range is None:
            class UniversalContainer(object):
                def __contains__(self, address):
                    return True
                def __str__(self):
                    return "None" # i.e. no address range supplied
            address_range = UniversalContainer()
        try:
            address_range = list(address_range)
        except TypeError:
            pass # Assume user passed something precooked
        else:
            if (len(address_range) == 2 and 
                all(isinstance(a, int_type) for a in address_range)):
                address_range = (address_range,)
            address_range = AddressMultiRange(AddressRange(*pair) for pair in address_range) 
        
        ret = self._get_info(address_range, "Functions", **kwargs)
        if ret is not None:
            return ret


class VarDict(AddressableSymbolDict):
    """
    Present access to all variables in the firmware, including lookup-by-address
    support
    """
    _name_lookup_returns_address = False
    
    def __init__(self, vars, layout_info, cu_dict, toolchain,
                 factory, aux_dict=None, is_cu=False):
        """
        :param vars: Primary ScopedSymbolDict containing DWARF variable symbols
        :param factory: Callable taking a key and a symbol.  Note: this factory
         is distinct from the symbol_ref->symbol factory applied by the underlying
         symbol_dict.
        :param cu_dict: ScopedSymbolDict containing the CU paths as "nested scopes"
         (used for CU name lookup/disambiguation)
        :param toolchain: Miscellaneous metadata for the compiler toolchain (assumed
         unique) 
        :param aux_dict: Optional ScopedSymbolDict containing extra symbols
        :param is_cu: Indicates that this represents a single CU, rather 
         than a collection of CUs. 
        """
        
        AddressableSymbolDict.__init__(self, vars, factory, cu_dict,
                                       toolchain,
                                       aux_dict=aux_dict,
                                       address_underflow_excep=None,
                                       address_overflow_excep=None,
                                       is_cu=is_cu)
        self._bytes_to_addr_units = layout_info.addr_unit_bits // 8
        self._layout_info = layout_info
        try:
            # factory can be a callable that also exposes a data space
            # object for convenience
            self._dspace = factory.data_space
        except AttributeError:
            # Or not
            self._dspace = None
            
    def get_var_at_address(self, addr):
        """
        Implements the traditional variable address-based look-up API  that
        returns a triple representing the variable (if any) containing the given
        address.  The triple is:
        
        (var_address, var_name, var_symbol)
        
        where var_address is the (start) address of the variable, var_name is
        a name string for globals and a name, cu pair for file-scoped statics,
        and var_symbol is the DWARF symbol, if any
        
        Returns None if the address is not inside a static variable.
        
        :p addr Address to look up.
        """
        key = self.find_by_address(addr)
        if key is None:
            return None
        address = self._get_address(key)
        try:
            sym = self._symbol_dict[key]
        except KeyError:
            # Key must be valid so KeyError implies aux_dict is present and
            # contains the key
            sym = self._aux_dict[key]
        name, cu = self.name_cu_from_key(key)
        display_name = name if sym.is_global else (name, self._cu_dict.minimal_unique_subkey(cu))
        return addr, display_name, sym
            
    def _clone(self, symbol_dict, aux_dict=None, is_cu=True):
        """
        Create an equivalent object with a modified symbol dict.  The CU-related
        default reflects the fact that this function is normally used to create
        AddressableSymbolDicts corresponding to sub-scopes.
        :p symbol_dict Primary ScopedSymbolDict containing symbols
        :p aux_dict Optional ScopedSymbolDict containing extra symbols
        :p is_cu Indicates that this SymbolDict represents a single CU, rather 
        than a collection of CUs.  If this is True, elide_top_level_scope should
        be False
        """
        return VarDict(symbol_dict, self._layout_info, self._cu_dict,
                       self._toolchain, self._factory, aux_dict=aux_dict, 
                       is_cu=is_cu)
        
    def _get_address(self, scoped_name):
        """
        Return the address of a given scoped_name (i.e. a key for the underlying
        ScopedSymbolDicts)
        :p scoped_name Full name of a symbol, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        try:
            return self._symbol_dict[scoped_name].static_location
        except KeyError:
            if self._aux_dict:
                symbol = self._aux_dict.get(scoped_name)
                if symbol:
                    return symbol.address
            raise
    
    def _get_size(self, scoped_name):
        """
        Return the size of a given scoped_name (i.e. a key for the underlying
        ScopedSymbolDicts)
        :p scoped_name Full name of a symbol, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        try:
            return self._symbol_dict[scoped_name].byte_size // self._bytes_to_addr_units
        except KeyError:
            if self._aux_dict:
                symbol = self._aux_dict.get(scoped_name)
                if symbol:
                    # Guess that non-DWARF variables are one word long
                    return self._layout_info.data_word_bits // self._layout_info.addr_unit_bits
            raise
        
    def __getitem__(self, varname_or_addr):
        """
        Look up by name or address: name look-up will return a Variable object
        
        varname_or_addr can take the following forms:
         - an integer: this is interpreted as a program address, and the name of
         the entity in which this falls, fully scope-qualified, is returned 
         - a string, containing either
          -- an unambiguous name, with or without scope qualification ("::"-separated)
          -- a name qualified by a /-separated CU path fragment, with the CU
          either supplied as an outer scope (i.e. prepended and "::"-separated,
          e.g. "path/to/my_cu.c::my_namespace::my_func")
          or as a suffix (i.e. appended and ":"-separated,  e.g. 
          "my_func:path/to/my_cu.c". Note that the second form is only supported
          for C, not C++.
         - a tuple, containing the same elements as for the qualified
         function name.  In the tuple form of the "CU suffix" notation the CU
         entry (the second) can be None, indicating a global.
         
         Note: in the case where this dictionary refers to a single CU,
         explicit CU qualification of string and tuple keys is not supported,
         naturally: names must simply be unambiguous in the target CU (i.e.
         contain sufficient scoping qualification).
        """
        try:
            return super(VarDict, self).__getitem__(varname_or_addr)
        except self._ambig_name_exceps:
            # Traditionally var dicts raised KeyError when a look-up was ambiguous
            raise KeyError(varname_or_addr)
        

    def info(self, address_range=None, **kwargs):
        """
        Return tabulated information about the statically allocated variables
        in the firmware.
        
        :param address_range Description of the range of addresses in which to 
        look for variables.  This can have any of several types:
         - an ordered iterable pair of integers - (low addr, high addr) 
         - an ordered iterable of such iterable pairs
         - the name of an address map group or slave
         - a list of such names (but they can't be a mixture of groups and slaves)
         - any type supporting "in" for addresses
        
        By default the address map group "local ram" is used.  This is usually
        going to be what you want.
        
        ** See csr.framework.meta.elf_firmware_info.fw_info_helper docs for 
        details of the filter and output arguments **
        """
        data = self._dspace
        # Sort out defaults
        if address_range is None:
            address_range = ("local ram",)
        elif isinstance(address_range, str):
            address_range = (address_range,)
        try:
            address_range[0]
        except TypeError:
            0 in address_range # throwaway expression just to check that the
            # thing the user passed supports "in"
        else:
            if isinstance(address_range[0], str) and data is not None:
                # we want the address range sorted for consistent output
                subranges = data.map.select_subranges(
                    *address_range, must_exist=True)
                subranges.sort(key=lambda x: x.start)
                address_range = AddressMultiRange(subranges)
            else:
                # It needs to be a pair of integers or a list of pairs of integers
                try:
                    address_range = list(address_range)
                except TypeError:
                    raise TypeError("Unsupported address range type '%s'" % 
                                    type(address_range))
                if (len(address_range) == 2 and 
                    all(isinstance(a, int_type) for a in address_range)):
                    address_range = (address_range,)
                address_range = AddressMultiRange(AddressRange(*pair) 
                                                  for pair in address_range) 
                
        def build_fw_report(key):
            name, cuname = self.name_cu_from_key(key)
            return FwReport(name, cuname, self._get_address(key), self._get_size(key))

        ret = self._get_info(address_range, "Statically allocated variables", 
                             **kwargs)
        if ret is not None:
            return ret

    def const_info(self, **kwargs):
        """
        Convenience function to select just variables in memory map regions
        labelled as const space
        """
        return self.info(address_range="const space", **kwargs)





# Note: Theoretically CUDict could be an AddressableSymbolDict because CUs have
# an address and a size.  However SymbolDict represents containers where the
# keys are of the form (CU, name), so it wouldn't quite work
class CUDict(SimpleSymbolDict):
    """
    Implements the classic CUDict interface against an SimpleSymbolDict
    """
    AmbiguousNameExcep = AmbiguousCUName
    
    def __init__(self, symbol_dict, factory, filter=None, aux_dict=None):
        SimpleSymbolDict.__init__(self, symbol_dict, aux_dict=aux_dict, 
                                  factory=factory,
                                  filter=filter, cacheable=True)
        self._ambig_name_exceps += (self.AmbiguousNameExcep,)
    
    def get_path(self, path_tuple):
        """
        Given a inner path fragment tuple return the full path as a flat string
        :p path_tuple Tuple representing innermost part of a CU path
        """
        return self._symbol_dict.sep.join(self.lookup_key(path_tuple))
    full_path = get_path # backwards compatilibity
    
    def get_path_from_fragment(self, path_fragment):
        """
        Given a inner path fragment string return the full path as a flat string
        (Equivalent to get_path really)
        :p path_fragment Innermost part of a CU path
        """
        matching_key = self.lookup_key(path_fragment)
        if matching_key:
            return self._symbol_dict.sep.join(matching_key)
    def get_basename_tuple(self, full_path):
        """
        Turn the supplied full path into a minimal unique subkey, returned as a
        tuple
        :p full_path Full CU path
        """
        return self._symbol_dict.minimal_unique_subkey(full_path)
    def normalise_path(self, path_fragment):
        """
        Turn the supplied path fragment into a minimal unique subkey
        :p path_fragment Innermost part of a a CU path
        """
        matching_key = self.lookup_key(path_fragment)
        if matching_key:
            return self.minimal_unique_subkey(matching_key, join=False)
    
    def key_munging_wrapper(self, sep="_", basename_sep="__", basename_first=True, 
                            strip_suffices=True):
        """
        Return a wrapper around this object that converts the keys (CU 
        identifiers) from tuples to strings with the given separators and
        ordering.
        """
        return CUPathJoinWrapper(self, sep, basename_first=basename_first,
                                 strip_suffices=strip_suffices,
                                 basename_sep=basename_sep)

    def build_scope_tree(self, scope_create, scope_populate, scope_finalise=None,
                         env=None):
        """
        Build a scope tree containing the CU objects.  This isn't recursive:
        we simply provide a property that looks up env.cus with a suitable
        key.
        :p scope_create Callable that takes a sub-scope name and the parent
        scope object and returns a new or existing subscope of that name
        :p scope_populate Callable that takes a scope object and the
        SymbolDict for it and inserts all symbols at that scope level into the
        scope object
        :p scope_finalise Optional callable that marks the end of construction
        of the scope and all its nested members
        :p env a GlobalFirmwarePresenter (need to access env.cus).  Not optional.
        """
        scope = scope_create(None, None) # default create
        cus = env.cus
        for cu_key in self.keys():
            subscope_name = "_".join(self.minimal_unique_subkey(cu_key, 
                                                                join=False)).split(".")[0]
            def cu_build_factory(cu_key):
                def _factory(obj):
                    return cus[cu_key]
                return _factory
            setattr(type(scope), subscope_name, property(cu_build_factory(cu_key)))
        scope._components = NameSpace()
        if scope_finalise:
            scope_finalise(scope)
        return scope

    def __getitem__(self, key):
        """
        Looks up the given partial CU name
        """
        try:
            return super(CUDict, self).__getitem__(key)
        except self._ambig_name_exceps as e:
            raise self.AmbiguousNameExcep(e.matches)
        
        
    def basenames(self):
        """
        Return a list of the basename part of the all the CU keys 
        """
        # The keys are fragmented path tuples so we just need the last entry
        return [k[-1] for k in self.keys()]

        
class EnumDict(object):
    """
    Wrap the underlying DWARF's CU type dictionary, allowing access to
    types specifically as enums (a DwarfNoSymbol will be raised in the read_dwarf
    layer if the named type isn't actually an enumeration)
    """        
    def __init__(self, dwarf):
        """
        :p dwarf A Dwarf_Reader or similar object providing an "enums" 
        container and a "get_enum" method 
        """
        self._dwarf = dwarf
        self._enum_cache = {}
        self._unique_class_id = 0
        
    def __getitem__(self, enum):
        """
        Look up an enum of the given name, returning a unique self._EnumLookup-based
        type
        """
        try:
            self._enum_cache[enum]
        except KeyError:
            cls, self._unique_class_id = unique_subclass(self._EnumLookup,
                                                         id_hint=self._unique_class_id)
            self._enum_cache[enum] = cls(self._dwarf.get_enum(enum))
        return self._enum_cache[enum]

    class _EnumLookup(object):
        """
        Dictionary-plus class representing a single enum definition via a 
        name-or-value lookup function for enum fields and attribute-based access
        to values of fields, plus field name regex search and a pretty self-print 
        """
        def __init__(self, dwarf_enum_sym):
            """
            :p dwarf_enum_sym A Dwarf_Enum_Symbol object
            """
            self._sym = dwarf_enum_sym
            for enum in self.keys():
            
                def _getter_factory(name):
                    def _getter(obj):
                        return obj[name]
                    return _getter
                setattr(type(self),enum,property(fget=_getter_factory(enum)))
            
        @display_hex
        def __getitem__(self, value_or_symbol):
            """
            Look up the given field name or value.  If the former,
            returns the value; if the latter, returns the corresponding field or
            if more than one share the value, a list of field names
            :p value_or_symbol Either one of the integral values of the enum or
            one of its field names
            """
            if isinstance(value_or_symbol, str_type):
                return self._sym[value_or_symbol]

            elif isinstance(value_or_symbol, int_type):
                try:
                    self._val_dict
                except AttributeError:
                    self._val_dict = {}
                    for name, val in self._sym.items():
                        self._val_dict.setdefault(val,[]).append(name)

                val_list = self._val_dict[value_or_symbol]
                if len(val_list) == 1:
                    return val_list[0]
                return val_list 
            
            raise TypeError("Must look up an enum using a string or an integer")
                
        def items(self):
            """
            Iterable of field name, field value pairs
            """
            return iter(self._sym.items())

        def values(self):
            return iter(self._sym.values())

        def __iter__(self):
            """
            Iterator over field names
            """
            return iter(self._sym.keys())
        
        def keys(self):
            "List of field names"
            return list(self._sym.keys())
        
        def __repr__(self):
            return self._sym.__repr__()
        
    def keys(self):
        """
        Return a list of all the fully-scoped enum names within the supplied
        DWARF object as flat strings
        """
        return [self._dwarf.enums.sep.join(k) for k in self._dwarf.enums.keys()]

    def search_fields(self, sym_regex):
        """
        Return a list of all the field names within all the enums in this container
        which match the given regular expression
        """
        if isinstance(sym_regex, str):
            sym_regex = re.compile(sym_regex)
        matches = []
        for enum_name in self.keys():
            enum = self[enum_name]
            enum_matches = [k for k in enum.keys() if sym_regex.search(k)]
            for m in enum_matches:
                matches.append((enum_name, m))
        return matches

class EnumConstDict(object):
    """
    Transparent container for a simple dictionary of all the unique enumeration 
    fields in a given set of enums
    """
    def __init__(self, dwarf):
        """
        :p dwarf A Dwarf_Reader or similar object providing an "enums" 
        container and a "get_enum" method 
        """
        self._dwarf = dwarf
        self._econst_dict_ = None
        
    @property
    def _econst_dict(self):
        """
        Get a flat dictionary of all the unique enumeration constants
        """
        if self._econst_dict_ is None:
            self._econst_dict_ = self._dwarf.econsts
        return self._econst_dict_
        
    def __getattr__(self, name):
        """
        Forward attribute accesses to the underlying dictionary, by default
        """
        return getattr(self._econst_dict, name)
    
    def __getitem__(self, key):
        """
        Provide look-up of unique enum fields by name
        """
        return self._econst_dict[key]
    

class TypeDict(object):
    """
    Facade wrapping Dwarf_Reader's type API in a dictionary-like form
    """
    
    def __init__(self, dwarf):
        """
        :p dwarf A Dwarf_Reader or similar object providing a "types" 
        container and a "get_type" method 
        """
        self._dwarf = dwarf
        
    def __getitem__(self, name):
        """
        Uses Dwarf_Reader's "look-up type plus-plus" method to find a specific
        type name, returning a dictionary of type details (name, byte size, etc)
        """
        # read_dwarf does the caching so we can just do a raw look-up
        return self._dwarf.get_type(name).struct_dict
    
    def __contains__(self, name):
        """
        Is there a type of this name available?
        """
        try:
            self._dwarf.get_type(name)
            return True
        except DwarfNoSymbol:
            return False
        
    def search(self, sym_regex):
        """
        Return a list of type names matching the given regex
        """
        return ["::".join(k) for k in self._dwarf.types.search(sym_regex)]

class Enums(FrozenNamespace, LazyAttribEvaluator):
    """
    Attribute-based container of enums
    """    
    def __init__(self, fw_env):
        LazyAttribEvaluator.__init__(self, fw_env.enums, strip_prefix="enum ")
        FrozenNamespace.__init__(self, {})

class EnumConsts(FrozenNamespace, LazyAttribEvaluator):
    """
    Attribute-based container of enum fields
    """    
    def __init__(self, fw_env):
        LazyAttribEvaluator.__init__(self, fw_env.enum_consts)
        FrozenNamespace.__init__(self, {})
