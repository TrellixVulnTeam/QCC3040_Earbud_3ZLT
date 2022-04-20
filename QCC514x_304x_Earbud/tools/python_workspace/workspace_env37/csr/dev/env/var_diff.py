############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
This module is intended for use in mirrored-device scenarios such as earbud
systems.  It provides a 
means for comparing sets of static variables within a matching pair of
firmware environments for qualified equality (meaning equality in a subset of
fields, based on a typename filter), and printing unified or context diffs of
them.

There are two convenience methods: context_diff and unified_diff.  These are
identical except in the format of the output.  They are based on the class
VarDiff, which can be instantiated with similar arguments, and provides a few
extra utilities, most notably a reset() method for triggering a fresh search for
unequal variables, and a summary() method to report the names of differing
variables (and optionally identical variables too).   See the context_diff and
unified_diff docstrings for more details of how to run them.

The diffing algorithm works in two stages:
 1. The full set of variables yielded by compilation unit and type name filters
  are examined pair by pair for qualified inequality, meaning inequality in 
  fields which are not excluded by the type filter.  The full lists of equal
  and unequal variables are stored.
 2. The unequal variable pairs have their respective standard display 
  functionality executed to produce a pair of strings representing their fully
  expanded state.  These are then passed through a generic diff engine to produce
  the familiar unified or context diff formatting.
  
This two-step approach, as opposed to simply generating display strings and
running diffs on all variable pairs, is necessary for efficiency, as fully
expanding two variables for display can be relatively time-consuming compared with
simply checking for the first sign of inequality.  However, it has some minor
side-effects.  In the case that the diff is being run on a live chip it is
*possible* that differences may appear or disappear between the two stages, which
in the latter case would lead to an empty diff being reported.  However it is
assumed that it only makes sense to run a diff when the firmware state is
stable, so this isn't an important problem.   Another possibility is that despite
the type filter excluding a certain differing field from consideration, the
presence of another non-excluded differing field in the same structure will mean
that the first, uninteresting difference is present in the strings that are diffed,
so will appear in the output.  Again this is not considered a serious problem, 
since it will only result in false positives, not false negatives.

Note that the algorithm that determines equality of variables is not clever about
convertible or equivalent types:  declared type names must match exactly. So 
two integers of different types with equal values will compare unequal, as will
structures with equal contents with the same actual type if they were declared
using different typedefs of that type.

The algorithm is also not smart about unions.  It will simply compare all the
union fields, so if the "active" field is equal but there is extra space in 
the union's memory footprint which contains unequal bytes, the two variables will
compare unequal.
"""

import sys
import re
import difflib
from collections import OrderedDict
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.env.env_helpers import var_typename, _Pointer, InvalidDereference
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.model.interface import Table, Group


def get_cu_list(env, filter_cus=None, incl_not_excl=False):
        
    if filter_cus is None:
        filter_cus = []
        incl_not_excl = False

    if not incl_not_excl:
        # Automatically remove .s and .asm files
        filter_cus += [re.compile("\.s#?\d*$"), 
                       re.compile("\.asm$")]

    cus = []
    for cu_name in env.cus.keys():
        cu_key = env.cus.get_path_from_fragment(cu_name)
        if env.cus[cu_key] is not None and any(c.search(cu_key) for c in filter_cus) == incl_not_excl:
            cus.append(env.cus.lookup_key(cu_name))
            
    return cus


def get_var_list(env, filter_types=None, incl_not_excl=False,
                 return_var_objects=False):

    if filter_types is None:
        filter_types = []
        incl_not_excl = False
        
    vars = []
    for key in env.vars.keys():
        if env.vars.key_is_garbage(key):
            continue
        if key[-1].startswith("preserve_") and key[-1].endswith("debugging") or key[-1].startswith("$"):
            continue
        var = env.vars[key]
        typename = var_typename(var)
        if any(t.match(typename) for t in filter_types) == incl_not_excl:
            vars.append(var if return_var_objects else key)
    return vars


def context_diff(envA, envB, filter_cus=None, cus_incl_not_excl=False,
                 filter_types=None, types_incl_not_excl=False, labelA=None, labelB=None,
                 context=3, display=True, quiet=False):
    """
    Generate a context diff of the variables in the given firmware environments
    which pass the given compilation unit and type filters and which differ (where
    relevant in fields whose types pass the type filter).
    
    :param envA: First firmware environment (e.g. apps1A.fw.env)
    :param envB: Second firmware environment (e.g. apps1B.fw.env)
    :param filter_cus, optional: List of regular expressions which will be used 
     as a filter on the set of *full CU paths*.  By default there is no filtering.
    :param cus_incl_not_excl, optional: If True, matches with the filter_cus 
     list are interpreted as CUs to include not exclude, and all others are
     excluded.  False by default.
    :param filter_types, optional: List of regular expressions which will be used 
     as a filter on variables' declared types, and the types of structure/union
     fields. By default there is no filtering.
    :param types_incl_not_excl, optional: If True, matches with the filter_types 
     list are interpreted as types to include not exclude, and all others are
     excluded.  False by default.
    :param labelA, optional: Label to apply in the diff output to the content
     from envA (default "A")
    :param labelB, optional: Label to apply in the diff output to the content
     from envB (default "B")
    :param context, optional: Number of lines of context for the diff text
     default=3)
    :param display, optional: Whether to display the diff output to the screen
     (the default) or return it as a list of strings.
    :param quiet, optional: Whether to suppress any progress messages  
    """

    d = VarDiff(envA, envB, filter_cus=filter_cus, cus_incl_not_excl=cus_incl_not_excl,
                 filter_types=filter_types, types_incl_not_excl=types_incl_not_excl, 
                 labelA=labelA, labelB=labelB, quiet=quiet)
    
    return d.context_diff(context=context, display=display)

def unified_diff(envA, envB, filter_cus=None, cus_incl_not_excl=False,
                 filter_types=None, types_incl_not_excl=False, labelA=None, labelB=None,
                 context=3, display=True, quiet=False):
    """
    See the help for context_diff
    """

    d = VarDiff(envA, envB, filter_cus=filter_cus, cus_incl_not_excl=cus_incl_not_excl,
                 filter_types=filter_types, types_incl_not_excl=types_incl_not_excl, 
                 labelA=labelA, labelB=labelB, quiet=quiet)
    
    return d.unified_diff(context=context, display=display)


class VarDiff(object):
    """
    This class provides the facility to output a diff on a per-CU basis of all
    static variables (possibly filtered by CU name or type) between two cores 
    running the same firmware image.
    """
    
    def __init__(self, envA, envB, filter_cus=None, cus_incl_not_excl=False,
                 filter_types=None, types_incl_not_excl=False, 
                 labelA=None, labelB=None, quiet=False):
        
        self._envA = envA
        self._envB = envB
        self._filter_cus = [re.compile(c.strip()) for c in filter_cus] if filter_cus is not None else None
        self._cus_incl_not_excl=cus_incl_not_excl
        self._filter_types = [re.compile(t.strip()) for t in filter_types] if filter_types is not None else None
        self._types_incl_not_excl=types_incl_not_excl
        self._labelA = labelA if labelA is not None else "A"
        self._labelB = labelB if labelB is not None else "B"
        self._quiet = quiet
        
        self.reset()

    def reset(self):
        """
        Perform a fresh search for differing variables
        """
        if not self._quiet:
            iprint("Searching for differing variables")
        self._differing_vars, self._nondiffering_vars = self._find_differing_vars()
        if not self._quiet:
            iprint(" -> found %d" % len(self._differing_vars))
        
    def _find_differing_vars(self):
        """
        Implements the core qualified inequality search using methods on the
        variable objecst
        """
        # choice of env is arbitrary here - they are identical wrt metadata
        cus_and_vars = OrderedDict((cu, get_var_list(self._envA.cus[cu],
                                                     filter_types=self._filter_types,
                                                     incl_not_excl=self._types_incl_not_excl)) 
                                   for cu in get_cu_list(self._envA,
                                                         filter_cus=self._filter_cus,
                                                         incl_not_excl=self._cus_incl_not_excl))
        # delete entries with no vars in
        cus_and_vars = OrderedDict((cu, vars) for (cu, vars) in cus_and_vars.items() if vars)
        
        diffs = []
        nondiffs = []
        
        for cu, vars in cus_and_vars.items():
            
            for var_name in vars:
                
                varA = self._envA.cus[cu].vars[var_name]
                varB = self._envB.cus[cu].vars[var_name]
                if not varA.equals(varB, self._filter_types, 
                                   self._types_incl_not_excl):
                    diffs.append((cu,var_name))
                else:
                    nondiffs.append((cu,var_name))
                    
        return diffs, nondiffs
        
        
    def summary(self, report_identical=False, report=False):
        """
        Loop over all the specified CUs and all the specified variables within
        them recording those that don't comapre equal under the filtering that
        has been specified.
        """
        differing = Group("Differing")
        differing_instances = Table(["CU", "var"])
        differing.append(differing_instances)
        res = Group("Result")
        res.append(differing)
        if report_identical:
            identical_instances = Table(["CU", "var"])
            ident = Group("Identical")
            ident.append(identical_instances)
            res.append(ident)

        for cu, var_name in self._differing_vars:
            differing_instances.add_row([self._envA.cus.minimal_unique_subkey(cu, join=True),
                                         "::".join(var_name)])
        if report_identical:
            for cu, var_name in self._nondiffering_vars:
                identical_instances.add_row([self._envA.cus.minimal_unique_subkey(cu, join=True),
                                         "::".join(var_name)])

        if report:
            return res
        TextAdaptor(res, gstrm.iout)
    
    def _diff(self, differ, context=3, display=False):
        """
        Generates diff text based on a supplied differ. Can either display the
        diffs on the screen or return a list of diffs, each represented as a list
        of strings representing the lines of the diff output.
        """
        diffs = self._differing_vars

        def get_display_string(var):
            if isinstance(var, _Pointer):
                try:
                    display_string = var.deref.value_string
                except InvalidDereference:
                    display_string = var.value_string
            else:
                display_string = var.value_string
            return display_string

        old_setting = _Pointer.suppress_nonzero_pointer_value_display
        _Pointer.suppress_nonzero_pointer_value_display=True
        diff_text = {}
        try:
            for cu, var in diffs:
                
                name = "%s : %s" % (self._envA.cus.minimal_unique_subkey(cu, join=True),
                                   "::".join(var))
                
                diff_text[name] = ("\n".join(differ(get_display_string(self._envA.cus[cu].vars[var]).split("\n"),
                                                  get_display_string(self._envB.cus[cu].vars[var]).split("\n"),
                                                  n=context, lineterm="", 
                                                  fromfile=name +" (%s)" % self._labelA,
                                                  tofile=name + " (%s)" % self._labelB)))
                if not diff_text[name].strip():
                    iprint("WARNING: %s compared unequal but no diff displayed" % name)
                
        finally:
            _Pointer.suppress_nonzero_pointer_value_display=old_setting
            
        if display:
            for diff in diff_text.values():
                iprint(diff)
                iprint("-"*80)
        else:
            return diff_text
            
    def context_diff(self, context=3, display=True):
        """
        Generates context diff text with the given number of lines of before and
        after context. Can either display the
        diffs on the screen or return a list of diffs.
        """
        return self._diff(difflib.context_diff, context=context, display=display)
    
    def unified_diff(self, context=3, display=True):
        """
        Generates unified diff text with the given number of lines of before and
        after context. Can either display the
        diffs on the screen or return a list of diffs.
        """
        return self._diff(difflib.unified_diff, context=context, display=display)