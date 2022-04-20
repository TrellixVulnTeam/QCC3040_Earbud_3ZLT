############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.dwarf.read_dwarf import DwarfNoSymbol
"""
This module provides an interface to a scheme for specifying additional semantics
for firmware variables.  The aim is to capture the programmer's knowledge of
what particular structures actually mean where this can't be expressed in C.
Examples of such semantics are:
 - union discrimination
 - pointers as arrays (can't be distinguished from pointers to single objects)
 - pointers as pointers to individual linked list entries rather than to entire
 lists
 - selection of which logical linked-list pointer to actually treat as a list
 when there are several in a structure
 - pointers to character arrays which are intended as null-terminated strings
 rather than lists of numbers
 
This semantics scheme provides a facility for attaching semantics information to
types or to particular variable instances.  For further flexibility, type-based 
information can either be coded into Python or assigned dynamically at run time.

The information is provided and stored as a dictionary with a particular set of
valid keys (these are listed in the docstring for the method 
StructSemanticsDict.Entry.set). These are referred to below as "semantic dicts".

In the most common case of specifying semantics for a structure, a parent 
dictionary should be provided containing separate semantic dicts for each 
field in the structure (fields with no additional semantics can be omitted).
As well as entries for immediate fields, the parent dictionary can contain
entries for fields of substructures, including those accessed through pointers.
This is particularly useful when specifying union discriminator functions, as the
function will be passed the instance of the structure in whose semantics
dictionary the discriminator appears.  This allows freedom about what structure 
instance is passed to the discrimination computation - it may be the immediate
containing structure, but it could be any outer containing structure too.

Any semantics dictionary provided for a type is stored in a dictionary-like object
available as core.fw.struct_semantics.  An effort is made to assign the same
semantics to both the provided type *and* its "base type", i.e. the type that
the provided type is a typedef for.  However Pydbg's type information is not
complete enough to enable all aliases for a type to be found automatically: for
example, if the dictionary is specified for a base type which happens to be 
typedef'd, the framework won't apply the semantics to variables declared with 
the typedef because it has no way of finding out what the relevant typedef(s) are.
Hence it is recommended to supply type-based semantics using a typedef name.

The preferred way for type-based semantics to be provided is to code them up
within Pydbg.  This preference reflects Pydbg's overall objective of capturing
as much of developers' knowedge of the true semantics of the hardware and
firmware as possible within the codebase.  The dynamic setting options are
provided primarily as quick-and-dirty stop-gap, although there will no doubt be 
some situations where they serve a purpose that hard-coding can't. 

Semantics are automatically looked up and processed by the classes in the
_Variable hierarchy.  This is a little complicated because of the need to 
merge different sources of information for each structure or pointer instance:
 - relevant type-based semantics for the immediate type
 - inherited semantics from containing structures/pointers
 - dynamically added variable semantics
In addition the _Variable classes take care of binding the supplied union 
discriminator function to the appropriate structure instance.  Finally, the
_Variable classes' display methods apply the semantics when walking the
structure/pointer and when displaying their contents.  

Note that 
 - specifying semantics doesn't prevent explicit accesses that contradict them: 
 for example, specifying a pointer depth of 0 doesn't stop you calling .deref on 
 that pointer. Effectively the semantics only apply when Pydbg is left to its 
 own devices in the interpretation of a variable - i.e. in linked list expansion
 and recursive display. 
 - the semantics don't override reality: if a pointer has been given depth 10, 
 but is NULL, Pydbg won't attempt to display an array of 10 elements starting 
 at 0!
 - instance semantics set on the list head are not propagated to all the 
 elements in a linked list
 - the semantic dicts are not checked.  Unrecognised keys, whether invalid 
 structure field names at the parent dict level, or invalid directives at the 
 semantic dict level are simply ignored.
 - specifying instance-specific semantics is possible, but care needs to be
 taken, because Pydbg may create a new Python object to represent the same
 firmware variable if it is looked up again.  E.g. 
    >>> apps.fw.var.stream is apps.fw.var.stream
    False
 This may be counter-intuitive, but it has the benefit of avoiding Pydbg keeping
 its own internal references to objects, which would in effect leak if the
 user was no longer interested in them.  Instead the onus is on the user to 
 keep their own reference by assigning a local name.  This is rarely necessary
 because the Python object is just an access point to the underlying firmware
 variable which is recreated from the same information each time, but it 
 does become necessary when changes are madeto the Python object itself, e.g.
 when specific semantics are attached.
"""

import re
import copy
from ..env.env_helpers import _Variable, var_typename



class StructSemanticsDict(object):
    """
    A dictionary-like object that allows look-up of a structure type name, 
    returning (if available) a dictionary of additional semantics for the 
    structure, including information like:
     - the current valid field of a union field
     - whether a pointer field is just a pointer, or an array, and what its
     length is
     - whether to interpret a pointer or array as a string, and if so how to 
     decode it
    """
    
    class Entry(object):
        
        def __init__(self, data):
            
            self._data = data

        def set(self, semantics_dict, update=False):
            """
            Set or update the dictionary of semantics information.
            
            :param semantics_dict Dictionary of semantics information regarding the
            fields in the structure (see below for details)
            :param update Indicates whether any existing dictionary for this type
            should be updated (True) or replaced (False) with the supplied dict
            
            The dictionary should consist of per-field subdictionaries which have any
            of the following fields:
             - depth (pointers only) : an integer value which indicates how to 
             interpret pointexrs as arrays or linked lists.  
              -- 0 means don't follow the pointer at all, just display its value 
              (this is the default behaviour for void pointers)
              -- 1 means treat the pointer as pointing to a single object. This is
              the default behvaiour for pointers that are not determined to be linked
              lists.  Pointers that *are* linked lists will not be expanded if this
              setting is active
              -- N>1 means treat the pointer as an array of N elements.  (Currently
              there's no way to say "the length is given in this other field of the
              struct" but that could be added as a future extension.)
              -- None Perform the default behaviour
             - discrim (unions only) : a callable that takes a struct variable 
             instance and the name of a union field in that structure and returns 
             the name of the union field that is currently valid, or None if no 
             field is currently valid (note: the callable takes the union name
             to allow you to write a single function that interprets multiple unions
             in a structure; often it is just ignored, but it must be there as an
             parameter)
             - is_string (pointers only) : a boolean value that indicates whether 
             to interpret a pointer to integers as null-terminated ASCII characters.  
             By default const char * fields are interpreted this way and any other
             integral type pointers are interpreted as ordinary pointers.  (If the
             need arose, in a future extension we could also allow this field to 
             take the name of a character encoding so that we could correctly 
             interpret unicode strings)
             
            Note that the fields in the main semantics dict can be either fields in
            the immediate structure, *or* fields of substructures, including pointed-to
            ones.  E.g. given
            
            struct T {
              int i;
              char * name;
              };
            struct X {
              struct T *t;
              }
            
            a semantics dict for struct X could look like:
            
            {"t"            : {"depth" : 5 }, 
             "t.deref.name" : {"is_string" : True}}
            
            meaning that whenever an X was displayed, it would interpret the field t
            as an array of 5 T's, and within each T interpret "name" as a null-terminated
            ASCII characer string
    
            Note that the usual pointer expansion rules limit the effects of custom
            semantics indicators in the obvious ways.  E.g. if a pointer is NULL,
            it won't be expanded as an array even if the semantics gives is an
            explicit non-zero depth.
            """
            
            if semantics_dict is None:
                # Clear out any explicit semantics
                self._data = {}
            else:
                
                # Map any discrim entries given as string names into callables
                for field, fsem in semantics_dict.items():
                    for key, entry in fsem.items():
                        if key == "discrim" and isinstance(entry, str):
                            fsem[key] = lambda var, uname : entry
                
                if update:
                    existing_dict = self._data
                    existing_dict.update(semantics_dict)
                    new_dict = existing_dict
                else:
                    new_dict = semantics_dict
                
                self._data = new_dict
            
        def add(self, semantics_dict):
            self.set(semantics_dict, update=True)
    
        def reset(self):
            """
            Clear out any dynamically added type semantics.  The default hard-coded
            semantics will be used instead
            """
            self.set(None)
            
        def disable(self):
            """
            Stop any custom semantics being applied to the given type, including
            the default hard-coded semantics.
            """
            self.set({})
    
        def discrim(self, union_name, union_field_or_func):
            """
            Helper method to make it easier to set a fixed union field as the 
            selected one
            """
            if isinstance(union_field_or_func, str) or union_field_or_func is None: 
                self.add({union_name : {"discrim" : 
                                                  lambda *args : union_field_or_func}})
            else:
                self.add({union_name : {"discrim" : union_field_or_func}})

        @property
        def data(self):
            return copy.deepcopy(self._data)
    
    def __init__(self, fw_env, core, custom_semantics_type):
        
        self._dicts = {}
        if custom_semantics_type is not None:
            self._custom = custom_semantics_type(fw_env, core)
        else:
            self._custom = None
        self.env = fw_env
        
    def __getitem__(self, struct_name):
        
        sem = self.lookup(struct_name)
        if sem is None:
            raise KeyError("No special semantics specified for %s" % struct_name) 
        return sem
    
    def get(self, struct_name, default=None):
        sem = self.lookup(struct_name)
        if sem is None:
            return default
        return sem
    
    def lookup(self, struct_name, alt_name=None):
        
        if struct_name not in self._dicts:
            d = {}
            
            struct_alt_name = (alt_name if alt_name is not None 
                               else self._get_alt_name(struct_name))
            if self._custom is not None:
                ret = self._custom.build_semantics_dict(struct_name)
                if ret is not None:
                    struct_alt_name, d = ret
            d = self.Entry(d) 
            self._dicts[struct_name] = d
            if struct_alt_name is not None and struct_alt_name is not False:
                self._dicts[struct_alt_name] = d
        return self._dicts[struct_name]

    def _get_alt_name(self, typedef_name):
        """
        Try to find the name of the type referred to by the given typedef
        """
        return
        
        try:
            type_dict = self.env.types[typedef_name]
        except DwarfNoSymbol:
            return 
        base_name = type_dict["base_type_name"]
        if typedef_name != base_name and "<anonymous>" not in base_name:
            return type_dict["base_type_name"]
        

class HydraStructSemantics(object):
    
    def __init__(self, fw_env, core):
        
        self._core = core
        self.env = fw_env
    
    def build_semantics_dict(self, struct_name):
        """
        Standard semantics for structures in the Hydra common code
        
        Nothing implemented yet
        """
        return None


class AppsStructSemantics(HydraStructSemantics):
    
    def build_semantics_dict(self, struct_name):
        
        streams_names = {"STREAMS", "struct STREAMS"}
        stream_csb_state_names = {"struct stream_csb_state"}
        mblk_names = {"struct MBLK_T_tag", "MBLK_T"}
        transform_names = {"TRANSFORM", "struct TRANSFORM"}
        task_names = {"TASK", "struct _TASK"} 
        
        def get_alt_name(name_set, name_used):
            unused_name_list = list(name_set - {name_used})
            if len(unused_name_list) == 1:
                return unused_name_list[0]
            return None
        
        if struct_name in streams_names:
            
            def evaluator(env, streams, union_name):
                """
                Evaluate which of struct STREAMS.u's fields is currently valid
                by looking up the identity of the function pointer table that
                the STREAMS structure refers to.
                """
                if union_name != "u":
                    raise ValueError("Only union in STREAMS is called 'u' - " 
                                                 "'%s' passed" % union_name)
                # Look up variable by address - result if None if there's no
                # known variable at that address
                func_table_var_name = env.vars[streams.functions.value]
                if func_table_var_name is None:
                    return None
                # There's a handy convention with struct STREAMS where the
                # function pointer table's name reflects the corresponding 
                # union field name in a simple way
                match = re.match(r"stream_(\w+)_functions", func_table_var_name)
                if match:
                    field_name = match.group(1)
                    if field_name in streams.u.members:
                        return field_name
                return None

            return (get_alt_name(streams_names, struct_name),
                    {"u": {"discrim" : evaluator}})
        
        elif struct_name in stream_csb_state_names:
            
            # We need to be able to map from types to names of the fields in
            # DM_UPRIM_T because we can derive the type name from the symbolic
            # name of the command ID in DM_PRIM_T, but there's no fixed
            # relationship between the types and the names of the fields in
            # DM_UPRIM_T
            
            def evaluator(env, csb_state_inst, union_name):
                """
                The ID of the prim in a DM_UPRIM_T can be obtained
                from the "type" field, and from that we can figure out the 
                name of the correct field in the union 
                """
                type_to_field_map = {field_var_obj.typename:field_name
                                     for field_name, _, field_var_obj in 
                                        env.types["DM_UPRIM_T"]["members"]}
        
                prim_type = csb_state_inst[union_name].deref["type"].value
                try:
                    # DM_PRIM_T is preserved for debugging in P1, but not P0
                    if self._core.subsystem.p1 is self._core:
                        p1_env = env
                    else:
                        # We have to assume that P1's standard environment is
                        # being used.
                        p1_env = self._core.subsystem.p1.fw.env
                    try:
                        prim_enum_name = p1_env.enums["DM_PRIM_T"][prim_type]
                    except AttributeError:
                        iprint ("Can't access P1 firmware to look up DM_PRIM_T: unable "
                               "to identify stream_csb_state union fields")
                        return None
                    prim_type_name = prim_enum_name[5:] + "_T"
                    return type_to_field_map[prim_type_name]
                except (KeyError, DwarfNoSymbol):
                    # Unknown prim type
                    return None

            return (get_alt_name(stream_csb_state_names, struct_name), 
                    {"pending_upstream.deref" : {"discrim" : evaluator},
                     "pending_downstream.deref" : {"discrim" : evaluator}})
        
        elif struct_name in mblk_names:
            
            def evaluator(env, mblk_inst, union_name):
                """
                Deduce the type of mblk from the name of the compilation unit
                that the instance's vtable pointer is found in.
                """
                vtable = mblk_inst["vtable"].value
                try:
                    _, (name, filename), _ = env.vars.get_var_at_address(vtable)
                except (ValueError, TypeError):
                    # Didn't find anything at the vtable address
                    return None
                mblk_type = filename[5:-2] # strip off mblk_ and .c
                return {"pmalloc" : "pmalloc_s",
                        "msgfrag" : "msgfrag",
                        "duplicate" : "duplicate"}.get(mblk_type, None)
            
            return (get_alt_name(mblk_names, struct_name), 
                    {"u" : {"discrim" : evaluator}})

        elif struct_name in transform_names:
            
            def evaluator(env, trfm, union_name):
                """
                Potential extension: I don't think this is a complete implementation as there
                are union fields that I can't find matching function tables for.
                Needs more investigation.
                """
                func_table_var_name = env.vars[trfm.functions.value]
                if func_table_var_name is None:
                    return None
                # There's a handy convention with struct STREAMS where the
                # function pointer table's name reflects the corresponding 
                # union field name in a simple way
                match = re.match(r"(transform_)?(\w+)_functions", func_table_var_name)
                if match:
                    field_name = match.group(2)
                    if field_name in trfm.u.members:
                        return field_name
                return None
                
            
            return (get_alt_name(transform_names, struct_name),
                    {"source" : {"depth" : 1},
                     "sink" : {"depth" : 1},
                     "helper" : {"depth" : 1},
                     "u" : {"discrim" : evaluator}})
            
        elif any(struct_name.startswith(t) for t in task_names) and struct_name.endswith("]"):
            return(get_alt_name(task_names, struct_name), {"[]" : {"next" : {"depth" : 0}}})
            
        # Fall back to the parent class's builder
        return super(AppsStructSemantics, self).build_semantics_dict(struct_name)