############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import re

class ScopedSymbolDict(object):
    """
    Generic interface for retrieving named symbols within a recursively-scoped
    namespace.
    
    This class supports the following sub-interfaces:
    1. Scope-oriented lookup.  This is implemented via the standard __getitem__
    method; iterables of nested scope names up to and including the symbol name
    itself can be used to retrieve scopes or symbols.  If scopes are retrieved
    they are wrapped as ScopedSymbolDict objects in their own right.  If symbols
    are retrieved, they are passed through the dictionary's built-in factory
    function which is intended to provide client code with the opportunity to
    wrap raw symbol references as types.  When sub-scopes are retrieved they
    are populated with a specially wrapped version of the original factory
    function which adds the scope's fully-qualified name to the scope-relative
    name of the symbol so that the original factory function is always called 
    with the symbol's fully qualified name (and the symbol references), even when
    invoked from a sub-scope dictionary.
    
    2. Global iteration.  This is implemented via the standard dictionary 
    interface keys, values, items.  The keys method returns a list of all the
    valid scope and symbol tuples, and the values and items methods are based on
    this set.
    
    3. Name-oriented look-up.  This is aimed at retrieving incompletely-qualified
    names, so as to provide a more convenient look-up for users who don't want
    to worry about exactly which compilation unit or namespace a variable is in.
    Clearly this has limited validity: anbiguity causes a suitable exception to
    be raised.  The contents of the underlying dictionary are indexed on demand
    and this index is used to efficiently look up matches to the name.  This set
    can be refined if additional scoping information is supplied.  
    
    In addition filtering can be applied to any lookup.  Filtering is always
    performed after applying the factory function to create the full instance of
    the symbol.  A filter is not a property of the ScopedSymbolDict, it is a 
    property of the particular iteration call that has been made, i.e. it is
    passed in as an optional parameter.
    
    """
    class SymbolLookupError(KeyError):
        """
        Base class for errors calling the lookup methods
        """
        
    class UnknownName(SymbolLookupError):
        """
        No symbol found with the supplied name that passes the filter
        """
        def __init__(self, scoped_name):
            ScopedSymbolDict.SymbolLookupError.__init__(self, 
                       "No symbol found named '%s'" % str(scoped_name))
    
    class AmbiguousName(SymbolLookupError):
        """
        Multiple symbols found with the supplied name that pass the filter
        """
        def __init__(self, scoped_name, matches):
            ScopedSymbolDict.SymbolLookupError.__init__(self, 
                       "Multiple symbols found named '%s' pass" % scoped_name)
            self.matches = matches
    
    class DummyNotRaisedException(SymbolLookupError):
        pass
    
    def __init__(self, the_dict, symbol_factory, ignore_globals=True,
                 scope_sep="::", UnknownNameExcep=True, AmbiguousNameExcep=True,
                 unflatten_dict=False, alt_sep=None, combine_outermost=False,
                 supplement=None):
        """
        :p the_dict: Nested scope-orientated dictionary mapping names to symbol
        references (but see unflatten_dict below)
        :p symbol_factory: Callable that turns a symbol reference into a symbol.
        Arguments are the symbol reference's key and the symbol reference
        :p ignore_globals: If True, ignore any top-level dictionary in the_dict
        called '<globals>'
        :p scope_sep: String that should be used as the separator when converting
        a scope (key) tuple into a flat string
        :p UnknownNameExcep Exception type to be raised if a name is supplied
        that has no matches.  Defaults to self.UnknownName.  If False is passed,
        exceptions are not raised in this case, and None is returned instead. 
        :p AmbiguousNameExcep Exception type to be raised if a name is supplied
        that has multiple matches.  Defaults to self.AmbiguousName.  If False is
        passed, exceptions are not raised in this case and None is returned
        instead.
        :p unflatten_dict Indicates that the dictionary is not nested, but 
        instead (equivalently) is a flat dictionary with tuple (or flat-string-
        with-separators) keys that reflect an implicit scoping structure
        :p alt_sep An alternative separator string in the case unflatten_dict
        is True (motivating use case is where the_dict's keys are file paths
        containing a mixture of forward and backslashes.
        :p combine_outermost If True, indicates that the supplied dictionary's 
        outermost scopes are all equivalent in the sense that duplicated entries 
        at that level should be considered identical.  This is useful where the
        outermost scopes are compilation units and the symbols are types, as the
        simplest course of action is typically to consider every compilation 
        unit's definition of a particular type name as being identical.
        """
        self._factory = symbol_factory
        self._ignore_globals = ignore_globals
        self._sep = scope_sep
        alt_sep = alt_sep or scope_sep
        self._combine_outermost = combine_outermost
        self._d = the_dict if not unflatten_dict else self._unflatten_keys(the_dict,
                                                                           alt_sep)

        if UnknownNameExcep is True:
            self.UnknownNameExcep = self.UnknownName
        elif UnknownNameExcep is None:
            self.UnknownNameExcep = self.DummyNotRaisedException
        else:
            self.UnknownNameExcep = UnknownNameExcep
        if AmbiguousNameExcep is True:
            self.AmbiguousNameExcep = self.AmbiguousName
        elif AmbiguousNameExcep is None:
            self.AmbiguousNameExcep = self.DummyNotRaisedException
        else:
            self.AmbiguousNameExcep = AmbiguousNameExcep 

        self._supplement = supplement
    
    @property
    def supplemented(self):
        return self._supplement is not None
    
    def _unflatten_keys(self, flat_keys_dict, alt_sep):
        """
        Insert scoped dictionaries corresponding to the given iterable of keys
        :p flat_keys_dict: iterable of keys of tuple or separated-string type
        :p alt_sep: Alternative separator of the separated strings (e.g. "\" if
        the primary separator is "/")
        """
        
        def add_level(current_level_dict, level_name):
            return current_level_dict.setdefault(level_name, {})
        
        # An obvious optimisation would be to find the common prefix and only
        # add that once.
        
        scoped_dict = {}
        
        for key in flat_keys_dict:
            key_cmpts = (key.replace(alt_sep,self._sep).split(self._sep) 
                                            if self._sep is not None else key)
            current_level_dict = scoped_dict
            for k in key_cmpts[:-1]:
                current_level_dict = add_level(current_level_dict, k)
            current_level_dict[key_cmpts[-1]] = flat_keys_dict[key]
            
        # flush the index
        try:
            del self._index
        except AttributeError:
            pass
        
        return scoped_dict
        
    @property
    def sep(self):
        return self._sep # expose this so that clients know which separator the
                         # object understands when retrieving flat string symbol
                         # names
    
    def _clone(self, new_dict, factory):
        
        return self.__class__(new_dict, factory, 
                              ignore_globals=self._ignore_globals,
                              scope_sep=self._sep,
                              UnknownNameExcep=self.UnknownNameExcep,
                              AmbiguousNameExcep=self.AmbiguousNameExcep,
                              supplement=self._supplement)
        
    def _wrap_value(self, k, v):
        if isinstance(v, dict):
            def filtered_factory(k):
                # A variant on the factory where this scope's key gets silently added
                # to the factory call so it looks as though the factory recognises
                # the scope
                def _inner_factory(inner_key_list, v):
                    return self._factory(k+inner_key_list, v)
                return _inner_factory
            return self._clone(v, filtered_factory(k))
        return v if not self._factory else self._factory(k, v)
    
    # ------------------------------------------------------------------------
    # Dictionary-like interface
    # ------------------------------------------------------------------------
    
    def __getitem__(self, scoped_name):
        """
        Return an element of the underlying dictionary by defining an ordered 
        set of names.  This is applied from the top down; it may return a
        symbol or it may return an interior scope's SymbolDict
        
        :p scoped_name Full name of a symbol or scope, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        
        """
        if self.supplemented:
            self._apply_supplement()

        if isinstance(scoped_name, str):
            scope_tuple = tuple(scoped_name.split(self._sep))
        else:
            scope_tuple = scoped_name
        try:
            scope_tuple = self._keys_mapping[scope_tuple]
        except (AttributeError, KeyError):
            # Either self._combine_outermost is False, in which case there is no
            # _keys_mapping attribute, or the supplied key is a full one, in 
            # which case it won't have a mapping, but doesn't need one either.
            pass
            
        lookup = self._d
        for i, k in enumerate(scope_tuple):
            try:
                lookup = lookup[k]
            except KeyError:
                raise KeyError("Nested index %s failed at entry %d" % 
                               (scope_tuple, i))
        return self._wrap_value(scope_tuple, lookup)

    def get(self, scoped_name, default=None):
        """
        Identical to __getitem__ except that absence of scoped_name from the
        dictionary results in the default being returned.

        :p scoped_name Full name of a symbol or scope, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        :p default Value to return if the scoped_name is not present
        """
        try:
            return self[scoped_name]
        except KeyError:
            return default

    def __contains__(self, scoped_name):
        """
        Does the supplied scoped_name lie in the dictionary?
        :p scoped_name Full name of a symbol or scope, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        """
        try:
            self[scoped_name]
            return True
        except KeyError:
            return False

    def keys(self, filter=None):
        """
        Return all the logical dictionary keys for values that pass the filter 
        as tuples.
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        if self.supplemented:
            self._apply_supplement()
        return self._keys(filter=filter)

    def _keys(self, filter=None):
        if not self._combine_outermost:
            def get_inner_keys(inner_dict, key_list, filter):
                inner_keys = []
                for k, v in inner_dict.items():
                    new_key_list = key_list + (k,)
                    if isinstance(v, dict):
                        if not self._ignore_globals or k != "<globals>":
                            inner_keys += get_inner_keys(v, new_key_list, filter)
                    else:
                        if filter is None or filter(new_key_list,
                                                    self._factory(new_key_list,v)):
                            inner_keys.append(new_key_list)
                return inner_keys
                        
            return get_inner_keys(self._d, tuple(), filter)
        else:
            unique_keys = set()
            self._keys_mapping = {}
            for key_list in self.index.values():
                for k in key_list:
                    if k[1:] not in unique_keys and (
                        filter is None or filter(k, self._factory(k, self[k]))):
                        unique_keys.add(k[1:])
                        self._keys_mapping[k[1:]] = k
            return list(unique_keys)
    
    def any_keys(self, filter=None):
        """
        Return True if there are any keys at any level in this scoped dict that
        pass the filter (this is used for efficiently filtering out empty scope 
        trees when iterating)
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        if self.supplemented:
            self._apply_supplement()

        if not self._combine_outermost:
            def any_inner_keys(inner_dict, key_list, filter):
                for k, v in inner_dict.items():
                    new_key_list = key_list + (k,)
                    if isinstance(v, dict):
                        if not self._ignore_globals or k != "<globals>":
                            if any_inner_keys(v, new_key_list, filter):
                                return True
                    else:
                        if filter is None or filter(new_key_list,
                                                    self._factory(new_key_list,v)):
                            return True
                return False
            return any_inner_keys(self._d, tuple(), filter)
        else:
            for key_list in self.index.values():
                for k in key_list:
                    if not self._ignore_globals or k[0] != "<globals>":
                        if filter is None or filter(k, self._factory(k, self[k])):
                            return True
            return False
                                                                        
    
    def values(self, filter=None):
        """
        Return the full set of symbol references passing the filter as a list
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        return [self[k] for k in self.keys(filter=filter)]

    def items(self, filter=None):
        """
        Return the full set of (key, symbol reference) pairs passing the filter 
        as a list
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        return [(k, self[k]) for k in self.keys(filter=filter)]
    

    # Flat iteration over either symbols or scopes in the immediate scope
    
    # Note that because this is flat iteration the keys are not tuples but 
    # individual strings.  This means that they must be converted internally to
    # tuples before being passed to the factory methods.
    
    def symbol_keys(self, filter=None):
        """
        Return all the *symbol names* at the top level passing the filter, 
        as a list (note that the list elements are simple name strings, not 
        (single-element) tuples)
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        if self.supplemented:
            self._apply_supplement()

        return [k for (k,v) in self._d.items() 
                    if not isinstance(v, dict) and 
                        (filter is None or filter((k,), self._factory((k,), v)))]
        
    def scope_keys(self):
        """
        Return all the *scope names* at the top level passing the filter 
        as a list (note that the list elements are simple name strings, not 
        (single-element) tuples)
        """
        if self.supplemented:
            self._apply_supplement()

        return [k for (k,v) in self._d.items() if isinstance(v, dict)]
    
    def symbol_values(self, filter=None):
        """
        Return all the symbols at the top level passing the filter, 
        as a list 
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        if self.supplemented:
            self._apply_supplement()

        values = []
        for (k, v) in self._d.items():
            if not isinstance(v, dict):
                v_instance = self._factory(k, v)
                if not filter or filter(k, v_instance):
                    values.append(v_instance)
        return values
    
    def scope_values(self):
        """
        Return all the scopes at the top level passing the filter 
        as clones of this dictionary.  In particular an equivalent factory is
        passed into the new object
        """
        if self.supplemented:
            self._apply_supplement()

        def filtered_factory(k):
            # A variant on the factory where this scope's key gets silently added
            # to the factory call so it looks as though the factory recognises
            # the scope
            def _inner_factory(inner_key_list, v):
                return self._factory((k,)+inner_key_list, v)
            return _inner_factory
        return [self.__class__(v, filtered_factory(k)) 
                     for (k,v) in self._d.items() if isinstance(v, dict)]

    def symbol_items(self, filter=None):
        """
        Return all the symbols at the top level passing the filter, 
        as a list of (name, symbol) pairs
        :p filter Callable predicate taking fully scoped name (as a tuple) and 
        symbol reference.  Defaults to a predicate that is always True.
        """
        if self.supplemented:
            self._apply_supplement()

        items = []
        for (k, v) in self._d.items():
            if not isinstance(v, dict):
                v_instance = self._factory(k, v)
                if not filter or filter(k, v_instance):
                    items.append(k, v_instance)
        return items
    
    def scope_items(self):
        """
        Return all the scopes at the top level passing the filter as a list of
        (name, scope) pairs where the "scopes" are clones of this dictionary. 
        """
        if self.supplemented:
            self._apply_supplement()

        def filtered_factory(k):
            # A variant on the factory where this scope's key gets silently added
            # to the factory call so it looks as though the factory recognises
            # the scope
            def _inner_factory(inner_key_list, v):
                return self._factory((k,)+inner_key_list, v)
            return _inner_factory
        return [(k, self.__class__(v, filtered_factory(k))) 
                     for (k, v) in self._d.items() if isinstance(v, dict)]
    
    # ------------------------------------------------------------------------
    # Name look-up interface
    # ------------------------------------------------------------------------
    # Supplement is applied in index so none of these index-based functions need
    # to do it explicitly.

    def _build_index(self):
        self._index = {}
        restore_combine_outermost = self._combine_outermost
        self._combine_outermost = False
        for k in self._keys():
            # Every key tuple starts with some level of scoping (perhaps none); the 
            # indexable name is the last (or only) tuple element
            self._index.setdefault(k[-1], []).append(k)
        self._combine_outermost = restore_combine_outermost

    @property
    def index(self):
        """
        Dictionary mapping base names (names with scopes stripped off) to all 
        scoped names with that basename.  Constructed on demand. 
        """
        try:
            self._index
        except AttributeError:
            self._build_index()

        if self.supplemented:
            self._apply_supplement()

        return self._index

    def _apply_supplement(self):
        """
        Adds the supplementary dictionary to self such that keys are only added
        if the supplenemtary dicts key is does not share a name with an existing key as
        the point is to add missing entried rather than introduce ambiguity in existing ones.
        """

        supplementary_dict = self._supplement() # actually generate the supplementary dict here
        self._supplement = None
        if supplementary_dict is None:
            return

        # Provoke index building to ensure self._index exists and is initialised, 
        # but don't run the full index property implementation here because it 
        # calls _apply_supplement itself.
        try:
            self._index
        except AttributeError:
            self._build_index()


        # Build a set of known addresses and ensure no supplementary symbols at known addresses
        # are added because we don't want to replace genuine symbols with supplementary ones.
        known_addresses = {sym.address:sym for sym in self.values()}
        # Remove anything from supplementary_dict that matches a known address
        filtered_supp_dict = {}
        for scope_name, scope in supplementary_dict.items():
            filtered_scope = {}
            for sym_name, sym in scope.items():
                if sym.address not in known_addresses:
                    filtered_scope[sym_name] = sym
                else:
                    # We already have a symbol here, but it may not have a size, so update if
                    # not.
                    existing_symbol = known_addresses[sym.address]
                    if existing_symbol.size is None:
                        existing_symbol.size = sym.size
            if filtered_scope:
                filtered_supp_dict[scope_name] = filtered_scope


        # The supplementaty dict should always be constructed to define unique scopes so
        # this should not clash with anything in existing dictionary
        self._d.update(filtered_supp_dict)
        # We then rebuild the index so that we don't clash names
        restore_combine_outermost = self._combine_outermost
        self._combine_outermost = False
        for k in self._keys():
            name = k[-1]
            if name not in self._index:
                self._index.setdefault(name, []).append(k)
        self._combine_outermost = restore_combine_outermost

    def _prepare_name_lookup(self, symbol_name):
        if isinstance(symbol_name, str):
            symbol_name = tuple(symbol_name.split(self._sep))
        return symbol_name

    def name_matches(self, symbol_name):
        """
        Return everything in the index with a name that matches the basename of
        the given partially scoped name
        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        """
        symbol_name = self._prepare_name_lookup(symbol_name)
        try:
            return self.index[symbol_name[-1]]
        except KeyError:
            return []

    def scoped_name_matches(self, symbol_name):
        """
        Return everything in the index with a name that matches the full extent
        of the scoping in the partially scoped name
        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        """
        symbol_name = self._prepare_name_lookup(symbol_name)
        name_matches = self.name_matches(symbol_name)
        # Filter the raw_matches down to those that also match any supplied scope
        symbol_scope = symbol_name[:-1] 
        if symbol_scope:
            scope_depth = len(symbol_scope)
            scoped_name_matches = [n for n in name_matches 
                                   if n[-1*scope_depth-1:-1] == symbol_scope]
        else:
            # No scope supplied
            scoped_name_matches = name_matches
        return scoped_name_matches

    def _report_unknown_name(self, report_name):

        if self.UnknownNameExcep is not self.DummyNotRaisedException:
            raise self.UnknownNameExcep(report_name)

    def _report_ambiguous_name(self, report_name, filtered_matches):
        
        if self.AmbiguousNameExcep is not self.DummyNotRaisedException:
            raise self.AmbiguousNameExcep(report_name, filtered_matches)

    def lookup_key(self, symbol_name, require_unique=True, filter=None,
                      report_name=None):

        """
        Attempt to look up a particular symbol_name returning the full key of a 
        unique matching entry
        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        :p require_unique If True, raise an error if multiple matches are
        found; if False return an arbitrary one of the set of matches if
        multiple matches are found
        :p Filter: limit the results to keys whose symbols pass the given filter,
        a predicate callable taking a full key and symbol reference as arguments
        """
        if report_name is None:
            report_name = symbol_name
        
        scoped_name_matches = self.scoped_name_matches(symbol_name)
        if not scoped_name_matches:
            if not isinstance(symbol_name, str):
                symbol_name = self._sep.join(symbol_name)
            self._report_unknown_name(report_name)
         
        # Filter the scoped name matches down to those that also match the
        # supplied filter if any

        if not require_unique:
            # Pick one arbitrarily
            filtered_matches = []
            for n in scoped_name_matches:
                if not filter or filter(n, self[n]):
                    filtered_matches.append(n)
                    break
        else:
            if filter:
                filtered_matches = [n for n in scoped_name_matches if filter(n, self[n])]
            else:
                filtered_matches = scoped_name_matches
        
        if len(filtered_matches) == 1:
            return filtered_matches[0]
        elif filtered_matches:
            if self._combine_outermost:
                s = set(k[1:] for k in filtered_matches)
                if len(s) == 1:
                    return filtered_matches[0]
            if not isinstance(report_name, str):
                report_name = self._sep.join(report_name)
            self._report_ambiguous_name(report_name, filtered_matches)
        else:
            if not isinstance(report_name, str):
                report_name = self._sep.join(report_name)
            self._report_unknown_name(report_name)


    def lookup_symbol(self, symbol_name, require_unique=True, filter=None,
                      report_name=None):
        """
        Attempt to look up a particular name, or partially scoped name list,
        returning the symbol of the unique matching entry
        :p symbol_name Partially scoped name (i.e. basename with zero or more 
        inner scopes) supplied either as a tuple or a separated string 
        :p require_unique If True, raise an error if multiple matches are
        found; if False return an arbitrary one of the set of matches if
        multiple matches are found
        :p Filter: limit the results to keys whose symbols pass the given filter,
        a predicate callable taking a full key and symbol reference as arguments
        :p report_name: Alternative name to report in any error message
        """
        key = self.lookup_key(symbol_name, require_unique=require_unique,
                              filter=filter, report_name=report_name)
        if key is not None:
            return self[key]

    def minimal_unique_subkey(self, scoped_name, join=True):
        """
        Turn a scoped_name into the shortest equivalent unique symbol_name (i.e.
        the shortest name that could be passed to lookup_key to find the symbol)
        :p scoped_name Full name of a symbol or scope, supplied either as a tuple
        or as a string containing the object's defined separator (e.g. "::")
        :p join If True return the symbol_name as a separator-joined string,
        otherwise return as a tuple
        """
        key_cmpts = self._prepare_name_lookup(scoped_name)
        num_cmpts = 0
        matches = [None,None] # dummy "we're not done" value
        while len(matches) > 1:
            num_cmpts += 1
            if num_cmpts > len(key_cmpts):
                raise RuntimeError("Key '%s' has no unique representation" % scoped_name)
            matches = self.scoped_name_matches(key_cmpts[0-num_cmpts:])
            if not matches:
                raise ValueError("Key '%s' doesn't exist in dictionary!" % scoped_name)
        return self._sep.join(key_cmpts[0-num_cmpts:]) if join else key_cmpts[0-num_cmpts:]
    
    def search(self, sym_regex):
        """
        Return a list of all keys for names that match the given regular
        expression
        :p sym_regex Regular expression to match against symbol names
        """
        if isinstance(sym_regex, str):
            sym_regex = re.compile(sym_regex)
        
        def map_keys(keys):
            if self._combine_outermost:
                return list(set(k[1:] for k in keys))
            return keys
        
        return sum((map_keys(v) for k,v in self.index.items() if sym_regex.match(k)), [])
