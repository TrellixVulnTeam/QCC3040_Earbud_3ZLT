############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Base class for analyses.

Module which describes the base class for all the analyses.
"""
import abc
import math
import time
from functools import wraps

from ACAT.Core import Arch
from ACAT.Core import CoreTypes as ct
from ACAT.Core import CoreUtils as cu
from ACAT.Core.exceptions import (
    VariableNotPointerError,
    VariableMemberMissing
)
from ACAT.Display import InteractiveFormatter



def cache_decorator(function):
    """An analysis function return value can be cached with this decorator.

    Args:
        function (obj): A callable.
    """

    @wraps(function)
    def _new_method(self, *arg, **kws):
        """Wrapping function."""
        # The dictionary type is unhashable. The input parameters of the
        # function need  to be converted to a tuple.
        arguments_tuples = tuple(arg) + tuple(kws) + tuple(kws.values())

        # check if we need to empty our cache.
        now = time.time()
        if self.cache_expire_time < now:
            self.cache = {}

        # Check if the function is already cached for the analysis.
        if function in self.cache:
            function_cache = self.cache[function]
        else:
            # Function is not cached. Create a new cache for the function
            function_cache = {}

        # check if the function cache already contains the current argument.
        if arguments_tuples in function_cache:
            retval = function_cache[arguments_tuples]
        else:
            # call the function
            retval = function(self, *arg, **kws)
            # set the cache expire time.
            self.cache_expire_time = self.cache_validity_interval + now

        # put the return value of the function to the function cache
        function_cache[arguments_tuples] = retval

        # Update the operator cache.
        self.cache[function] = function_cache
        return retval

    # return the new function created by the decorator
    return _new_method


# The abstract Analysis class is used in many other places, but pylint
# can not see them. Hence disabling the check in this package.
# pylint: disable=abstract-class-not-used
class Analysis(object):
    """Base class for all analysis plugins.

    Example:

        Individual analysis can be a subclass of the ``Analysis`` class. See
        the example below::

            class MyAnalysis(Analysis.Analysis):
                def __init__(self, **kwarg):
                    # Call the base class constructor. This call will set all
                    # the values from the dictionary as attributes. In this
                    # manner chipdata, debuginfo and formatter will be set.

                    super(MyAnalysis, self).__init__(self, **kwarg)
                    do_other_stuff()

                def run_all(self):
                    run_useful_analysis()

    Convention is for useful methods (which we might want to call from other
    analyses and in Interactive mode) to be public and they should return
    data structures rather than printing it directly. An analysis which
    prints information via the formatter should follow the naming convention
    that has the form analyse_xxx (or run_all). These functions return nothing.

    Do not be afraid to throw exceptions from Analysis functions. In fact,
    it is to be encouraged; when run in (say) Interactive mode we want as much
    direct feedback as possible.

    Analyses should not assume that chipdata is non-volatile, since they may
    be asked to operate on a live chip (e.g. by a framework other than
    CoreReport). ChipData.is_volatile() is provided so that an analysis can,
    if necessary, optimise for the non-volatile case.
    (FWIW debuginfo is inherently non-volatile so there's no need to check
    that.)

    Analyses should generally avoid doing any significant work - including
    any data or symbol lookup - in their constructors. If an error occurs
    that causes an exception, none of the rest of the analysis will run.

    An analysis can cache some of its answer. In other words the return value
    for some of their functions can be cached. Function which are used for
    inter-analyses communication should always be cached. In this manner,
    we can avoid refreshing the internal data of the called analysis and
    creating new variables when providing information to other analyses. To
    mark a function for caching it must be decorated with cache_decorator.
    This cache is cleared after each user instruction in interactive mode.
    With coredumps or in automatic mode there is no need to empty the cahce.

    Args:
        **kwargs: Arbitrary keyword arguments.

    Attributes:
        cache (dict)
        cache_expire_time (timestamp)
        cache_validity_interval (int)
        chipdata (:obj:`ChipData`)
        debuginfo (:obj:`DebugInfo`)
        interpreter (:obj:`Iterpreter`)
        formatter (:obj:`Formatter`)
    """

    def __init__(self, **kwargs):
        self.cache = {}
        # Time showing when the cache expires.
        self.cache_expire_time = time.time()
        # Cache validity in seconds.
        self.cache_validity_interval = cu.global_options.cache_validity_interval

        # Default internal values.
        self.chipdata = None
        self.debuginfo = None
        self.interpreter = None
        # Note that it is valid for formatter to be None; if we just want
        # to analyse data (and not print anything out) it is not necessary
        # to create a pointless formatter object.
        self.formatter = None

        for key in kwargs:
            setattr(self, key, kwargs[key])

        if self.formatter is not None:
            self.default_formatter = self.formatter

        try:
            if not any((self.chipdata, self.debuginfo, self.formatter)):
                raise Exception(
                    "All analyses need at least chipdata, "
                    "debuginfo and formatter."
                )
        except AttributeError:
            raise Exception(
                "All analyses need at least chipdata, "
                "debuginfo and formattter."
            )

    @abc.abstractmethod
    def run_all(self):
        """Performs all the useful analysis this module can do.

        Any useful output should be directed via the Analysis' formatter.
        """

    def set_formatter(self, formatter):
        """Sets the formatter for an analysis.

        Args:
            formatter (obj): Instance of the Formatter class.
        """
        self.formatter = formatter

    def reset_formatter(self):
        """Resets the formatter for an analysis."""
        # Signal to the formatter before swapping.
        self.formatter.flush()
        self.formatter = self.default_formatter

    def to_file(self, file_name, suppress_stdout=False):
        """Sets the output of the analysis to a file.

        Args:
            file_name (str): The output filename.
            suppress_stdout (bool): Whether to suppress the standard
                output or not.
        """
        formatter = InteractiveFormatter.InteractiveFormatter()
        formatter.change_log_file(file_name, suppress_stdout)

        self.formatter = formatter
