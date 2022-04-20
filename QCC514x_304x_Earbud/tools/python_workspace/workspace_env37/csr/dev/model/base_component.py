############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Base class for device components.

Components explicitly declare subcomponents for purpose of navigation and
report generation.
"""

import logging
from collections import OrderedDict
try:
    from collections.abc import Mapping
except ImportError:
    # for python 2
    from collections import Mapping

from csr.wheels import gstrm
from csr.wheels.global_streams import iprint, \
    configure_logger_for_global_streams
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.model.interface import Text
from csr.wheels.bitsandbobs import autolazy

def _valid_component(component):
    return component is not None and getattr(component, "has_data_source", True)

class Reportable(object):
    """
    This object can generate structured reports recursing into subcomponents.
    """

    @property
    def title(self):
        """
        The default title for the report for this component.
        """
        return self.__class__.__name__.lower()

    @property
    def subcomponents(self):
        """
        Returns either a dictionary or a list of subcomponents for this
        component.

        If returning a dictionary then the keys and values are both strings
        giving the names of attributes of this component that will give access
        to the subcomponent. There is a difference for subcomponents that are
        created when needed (using lazy evaluation). The keys of the dictionary
        are normal attributes that create the subcomponents when needed and
        are the ones normally used at the command line. The values of the
        dictionary are the attributes that won't create subcomponents if they
        don't already exist.
        """
        return self._all_subcomponents()

    def _all_subcomponents(self): #pylint: disable=no-self-use
        """
        Optional private method to return the dictionary or list that is
        exposed publicly by subcomponents.  This method exists to allow the
        public subcomponents property to be decorated (e.g. to filter the
        values returned) while still giving methods access to the original
        list
        """
        return None

    def subcmpt_presentation_order(self): # pylint: disable=no-self-use
        """
        Must be overridden in a derived class to provide a list of
        subcomponents in the order in which it is desired that they be
        reported.
        """

        return NotImplemented

    def _unordered_reportable_components(self, propagate_exc=False): # pylint: disable=invalid-name
        """
        Generate a list of all components where each entry in the list is
        a (name, object, error) tuple. "name" is the name of the attribute
        holding the component or None if the component is not held in an
        attribute. "object" is the Python object for the component or None if
        the component couldn't be built. "error" is a report of any errors
        found when building the object (as returned by self.report_error()) or
        None if there were no errors.

        If propagate_exc is False, then exceptions when building components
        are caught and the error item of the tuple is filled in. If
        propagate_exc is True, then exceptions are raised as normal, the
        object item in the tuple is guaranteed to be not None and the error
        item is guaranteed to be None.

        KeyboardInterrupt exceptions are always raised as normal (so you can
        break out of this operation).

        You probably don't want to call this, you probably want to call
        _reportable_components which sorts the output into the preferred
        sorting order. Even if you don't care about the order, still use the
        _reportable_components method as it will help ensure a consistent
        reporting order which makes it easier to compare output files when
        looking for regressions.
        """
        # As noted at the end of the docstring, the only place this should be
        # called from is _reportable_components. It exists as a separate
        # method because PyLint complained that as a single function it was
        # getting too complex. PyLint was right.

        try:
            # Most classes return a static dictionary but we want to cope
            # with dynamic subcomponents list.
            subcomponents = self.subcomponents or {} # Treat None as empty
        except Exception as exc:# pylint: disable=broad-except
            # KeyboardInterrupt is a BaseException, not Exception
            if propagate_exc:
                raise
            error = self.report_error(
                "Error generating subcomponents of '%s'" %
                (self.__class__.__name__), exc)
            return [(None, None, error)]

        if hasattr(subcomponents, 'items'):
            components = []

            for name in subcomponents:
                error = None

                try:
                    component = getattr(self, name)
                    if not _valid_component(component):
                        continue # Successfully found nothing to do
                except Exception as exc:# pylint: disable=broad-except
                    # KeyboardInterrupt is a BaseException, not Exception
                    if propagate_exc:
                        raise
                    error = self.report_error(
                        "Error generating component '%s' of '%s'" % (
                            name, self.__class__.__name__),
                        exc)
                    component = None

                components.append((name, component, error))
        else:
            components = [(None, component, None)
                          for component in subcomponents
                          if _valid_component(component)]

        return components


    def _reportable_components(self, propagate_exc=False):
        """
        A list of all components to be used for reporting returned in the
        preferred reporting order. Each entry in the list is a (name, object,
        error) tuple. "name" is the name of the attribute holding the
        component or None if the component is not held in an attribute.
        "object" is the Python object for the component or None if the
        component couldn't be built. "error" is a report of any errors found
        when build the object (as returned by self.report_error()) or None if
        there were no errors.

        If propagate_exc is False, then exceptions when building components
        are caught and the error item of the tuple is filled in. If
        propagate_exc is True, then exceptions are raised as normal, the
        object item in the tuple is guaranteed to be not None and the error
        item is guaranteed to be None.

        KeyboardInterrupt exceptions are always raised as normal (so you can
        break out of this operation).
        """
        components = []
        named_components = {}

        for name, component, error in self._unordered_reportable_components(
                propagate_exc=propagate_exc):
            if name is None:
                components.append((name, component, error))
            else:
                named_components[name] = (component, error)

        if named_components:
            ordering = self.subcmpt_presentation_order()
            if ordering is NotImplemented:
                ordering = sorted(named_components.keys())

            for name in ordering:
                try:
                    comp_err = named_components[name]
                    components.append((name,) + comp_err)
                    named_components[name] = None
                except KeyError:
                    # The ordering may list optional components
                    pass

            missed_order = []
            for name, comp_err in named_components.items():
                if comp_err:
                    missed_order.append(name)
                    components.append((name,) + comp_err)
            if missed_order:
                iprint("WARNING: the following subcomponents were not "
                       "included in the ordering for %s: %s - appending"
                       % (self.title, missed_order))

        return components

    def report(self, trace=False, propagate_exc=False):
        """
        Push the report straight to stdout
        """
        TextAdaptor(self.generate_report(trace=trace,
                                         propagate_exc=propagate_exc),
                    gstrm.iout)

    def report_error(self, moan, exc):
        """
        Either return a succint interface.Code object reporting the error
        exception in exc, as described by string moan, or
        otherwise, for debugging purposes, when self.trace is True,
        return a full traceback of exc.
        Note: self.trace is set transiently by passing trace=True into
        generate_report(trace=True, ...).

        It is very helpful for regression testing if the moan starts with the
        words "Error generating".

        The supplied moan should not contain the type of exception and the
        exception object itself, because these are added by this routine.
        However it is advisable for it to contain something that identifies
        the object which has caused the error.

        A component that wants to catch an error but continue with some
        processing should use this routine to report the error,
        but the caller must add the returned object to the report.

        See generate_report for some example uses.
        """
        from traceback import format_exc
        if not getattr(self, '__trace', False):
            return interface.Code(moan + " (%s: '%s')" % (type(exc), exc))
        return interface.Code(format_exc() + moan)

    def generate_report(self, depth_limit=100, allow_multiple=False,
                        trace=False, propagate_exc=False):
        """
        Generates a report recursively for all subcomponents and this component.
        :param depth_limit: limits the recursion.
        :param allow_multiple: if allow_multiple is False then components are
         represented by a logical group element. However, if allow_multiple is
         True then this method can return a list of elements.
        :param trace: When trace is true, if an exception is raised, then the
         full python traceback is provided.
        :param propagate_exc: if True, exceptions are not caught. In this
         case the value of "trace" is irrelevant.
        """

        # TODO this routine could still do with refactoring, hence:
        # pylint: disable=too-many-locals,too-many-branches,too-many-statements,too-many-nested-blocks
        if propagate_exc:
            class _CatchallException(Exception):
                pass
        else:
            _CatchallException = Exception # pylint: disable=invalid-name
            # KeyboardInterrupt is a BaseException, not Exception
        if trace:
            setattr(self, '__trace', True)
        output = []

        group = interface.Group(self.title)

        if allow_multiple:
            output.append(group)

        # Append report body elements (if any)
        #
        try:
            body_elements = self._generate_report_body_elements()
            if body_elements:
                try:
                    body_elements = list(body_elements)
                except TypeError: # not iterable
                    body_elements = [body_elements]
                for element in body_elements:
                    # Each returned item is either a renderable element or a
                    # (renderable, nesting_flag) pair.  Nested output is the
                    # default unless specified explicitly in this way.  Even
                    # then, non-nested output is suppressed unless
                    # allow_multiple is True.
                    try:
                        element, nest = element
                    except (TypeError, ValueError):
                        nest = True
                    if allow_multiple and not nest:
                        output.append(element)
                    else:
                        group.append(element)
        except _CatchallException as exc:# pylint: disable=broad-except
            group.append(self.report_error(
                "Error generating report body for '%s'" % (self.title), exc))

        # Append any subcomponent reports recursively subject to depth limit
        lifted_components = []
        if depth_limit:
            for name, component, error in self._reportable_components(
                    propagate_exc=propagate_exc):
                nest_report = self._nest_report(
                    component if name is None else name)
                if component:
                    component_title = component.title

                    try:
                        reports = component.generate_report(
                            depth_limit - 1,
                            allow_multiple=True,
                            trace=trace,
                            propagate_exc=propagate_exc)
                    except _CatchallException as exc:# pylint: disable=broad-except
                        reports = [
                            self.report_error(
                                "Error generating report for '%s'" %
                                component_title, exc)
                            ]
                elif error:
                    component_title = name or "<Unknown>"
                    reports = [error]
                else:
                    reports = []

                if allow_multiple and not nest_report:
                    if reports and component is not None:
                        title = None
                        if len(reports) == 1:
                            try:
                                title = reports[0].title
                            except AttributeError:
                                pass
                        if not title:
                            title = component_title
                        lifted_components.append(title)
                    output.extend(reports)
                else:
                    for rep in reports:
                        group.append(rep)

        if not group.members and lifted_components:
            group.append(Text("Reports for the following subcomponents are "
                              "at the same level as this component: " +
                              ", ".join(lifted_components)))

        #trace: if now set then unset it
        if trace:
            delattr(self, '__trace')
        if allow_multiple:
            return output

        return group

    # Protected / required

    def _generate_report_body_elements(self): #pylint: disable=no-self-use
        """\
        Generate report on this component (possibly a lengthy task).

        Report body should be a list of Model elements. The elements can-be
        part of the standard interface or generated on the fly.

        Potentially a much more extensive analysis than a simple view of
        properties.

        Reports should cover any inter-component relationships.

        Reports should not replicate, or include subcomponent-specific reports
        - thats done by wrapper function.

        Reports can assume that the device is halted (as far as possible).
        """
        return []

    def _nest_report(self, component):#pylint: disable=unused-argument,no-self-use
        """
        A function that indicates whether the report for the given subcomponent
        should be presented as a child of this report rather than a sibling.
        If this component is being listed in isolation then all subcomponents
        will remain as children.

        For example, Foo has subcomponent Bar and Bar has subcomponents
        Fred and Wilma then the default heirarchy for a report of Foo would
        be:

          Foo
          |
          +-->Bar
              |
              +-->Fred
              |
              +-->Wilma

        However, if this method on Bar returns False for Fred then the report
        structure will be:

          Foo
          |
          +-->Bar
          |   |
          |   +-->Wilma
          |
          +-->Fred

        In contrast, if you request a report just for Bar then the report
        structure would remain as:

          Bar
          |
          +-->Fred
          |
          +-->Wilma

        This feature can be used to prevent excessive nesting of reports if a
        given entity can occur only once. For example, a processor core
        contains firmware which contains a scheduler. There's no need to have
        the scheduler reported so deeply. It can be reported under the
        processor core as only one version of firmware can be running on a
        given core at a given time so it's unambiguous.

        If you want all reports to be listed up a level then just return the
        constant False here.

        If you want to do the same trick to groups reported from the function
        _generate_report_body_elements then return a list of
        [element, should_nest] pairs instead of just elements.
        """
        return True

    class __Subcomponent(property):
        pass

    @staticmethod
    def has_subcomponents(decorated_cls):
        """
        Class decorator to help setting-up Reportable classes

        It automatically creates the subcomponents list from methods decorated with:
            @report.subcomponent

        E.g.:
            @has_subcomponents
            class MyApp(FirmwareComponent):
                @report.subcomponent
                def subcmptA(self):
                    <extra logic or code as needed>
                    return subA()

                @report.subcomponent
                def subcmptB(self):
                    <extra logic or code as needed>
                    return subB()

        NOTE: This decorator is only compatible with report classes where it and
        each parent class' subcomponents property returns a dictionary.
        It doesnt' support the "list" variant of the subcomponents property.
        """

        @property
        @autolazy
        def subcomponents(self):
            subcmpts = {}

            for cls in list(decorated_cls.__bases__):
                if issubclass(cls, BaseComponent):
                    base_subcomponents = cls.subcomponents.fget(self)

                    if isinstance(base_subcomponents, Mapping):
                        subcmpts.update(base_subcomponents)
                    elif base_subcomponents is not None:
                        raise TypeError("subcomponent property isn't of type Mapping, can't be merged")

            for k, v in decorated_cls.__dict__.items():
                # Exclude dunder attributes like __init__, __name__ etc from the search
                if not (k.startswith("__") and k.endswith("__")) and isinstance(v, Reportable.__Subcomponent):
                    subcmpts[k] = "_" + k

            return subcmpts

        decorated_cls.subcomponents = subcomponents
        return decorated_cls

    @staticmethod
    def subcomponent(func):
        """
        Marks a property as a subcomponent.
        """
        return Reportable.__Subcomponent(func)


class BaseComponent(Reportable):
    """
    NB If you add new methods to this class which Firmware-derived class would
    implement differently depending on whether the firmware environment is
    available or not, be sure to make a suitable change to the Firmware
    metaclass (FirmwareAttributesRequireEnvMeta).
    """

    @property
    def live_subcomponents(self):
        """
        Dictionary mapping property name to instance attribute name for the
        subset of the subcomponents that are currently instantiated.
        """
        if isinstance(self.subcomponents, dict):
            return {prop:attr for (prop, attr) in
                    list(self.subcomponents.items()) 
                        if attr is not None and hasattr(self, attr)}
        return self.subcomponents

    def apply_reset(self):
        """
        Implement DUT reset by cascading through subcomponents calling their
        _on_reset() methods, so long as they have already been constructed.
        """
        if self.live_subcomponents:
            for component in self.live_subcomponents:
                if component is not None:
                    if isinstance(component, str):
                        inst_attr_name = self.live_subcomponents[component]
                        component = getattr(self, inst_attr_name)
                        if component:
                            component.apply_reset()
                    else:
                        component.apply_reset()
        self._on_reset()

    def clock_report(self, report=False):
        """
        Create a clock report
        """
        clock_info_collection = self.clock_info()
        # Create the table
        output_table = interface.Table(["Clock Related Information", "State"])

        if clock_info_collection is not None:
            for key, value in list(clock_info_collection.items()):
                output_table.add_row([key, value])

        if report:
            return output_table
        TextAdaptor(output_table, gstrm.iout)
        return None

    def clock_info(self):
        """
        Gather clock information and return as an OrderedDict
        """
        def update_clock_info_collection():
            """
            updates the clock_info_collection dictionary
            """
            if clock_information is not None:
                if (set(clock_info_collection.keys()) &
                        set(clock_information.keys())):
                    clock_collection_set = set(clock_info_collection)
                    clock_information_set = set(clock_information)
                    for key in clock_collection_set.intersection(
                            clock_information_set):
                        iprint("Duplicate Register field %s appearing in "
                               "new clock information" % (key))
                    raise KeyError("Duplicate Register field(s).")
                clock_info_collection.update(clock_information)

        clock_info_collection = OrderedDict()

        # Note added during reportable refactoring: The looping in here looks
        # wrong. This looping looks only at subcomponents that are already
        # instantiated. This is because it says:
        #
        #   component = getattr(self, self.subcomponents[component])
        #
        # instead of:
        #
        #   component = getattr(self, component)
        #
        # Since this appears to be a reporting method, it would seem to be
        # more appropriate for it to instantiate subcomponents as it went
        # along.  Investigating and either changing or documenting the
        # behaviour was out of scope of the refactoring. If a subsequent
        # investigation decides this should loop over all subcomponents then
        # alter the code to call _reportable_components() using the code in
        # gather_mmu_handles() as a template. Otherwise, replace this comment
        # with one giving the reason for the implementation.

        if self.subcomponents:
            for component in self.subcomponents:
                if _valid_component(component) and component != 'fw':
                    try:
                        if isinstance(component, str):
                            inst_attr_name = self.subcomponents[component]
                            component = getattr(self, inst_attr_name)
                            if _valid_component(component):
                                clock_information = component.clock_info()
                                update_clock_info_collection()
                        else:
                            clock_information = component.clock_info()
                            update_clock_info_collection()
                    except AttributeError:
                        #Several attributes reported by subcomponents have
                        #not been instantiated
                        pass

        clock_information = self._get_clock_info()
        update_clock_info_collection()
        return clock_info_collection

    def _get_clock_info(self):
        """
        Any class which has clock related information should fill this out.
        It should return an OrderedDict with clock and value pairs
        """


    def gather_mmu_handles(self, propagate_exc=False):
        """
        Return a tuple of two items describing owners of handles and any
        problems encountered when tracing owners.

        The first element in the tuple is a list of the MMU handles owned by
        all the subcomponents of this object

        The second element is a list of interface objects describing problems
        encountered (typically, exceptions thrown) while building the first
        list. In particular, this can indicate the first list has items
        missing because something went wrong.

        :param propagate_exc: if True, exceptions are not caught.
        """
        if propagate_exc:
            class _CatchallException(Exception):
                pass
        else:
            _CatchallException = Exception # pylint: disable=invalid-name
        missing = []
        try:
            mmu_handles = self.mmu_handles()
        except _CatchallException as exc: #pylint: disable=broad-except
            mmu_handles = []
            missing.append(
                self.report_error("Can't identify MMU handles for '%s'" %
                                  (self.title), exc))

        for _, component, error in self._reportable_components(
                propagate_exc=propagate_exc):
            if component:
                hndls, miss = component.gather_mmu_handles(
                    propagate_exc=propagate_exc)
                mmu_handles += hndls
                missing += miss
            elif error:
                missing.append(error)

        return mmu_handles, missing

    def _gather_memory_report(self):
        """
        Returns a list containing memory reports of all the subcomponents of
        this object
        """
        # Note added during reportable refactoring: This method already didn't
        # handle exceptions raised during component construction or when
        # gathering reports. There's no point in fixing this until the
        # caller can do something with it. See gather_mmu_handles() for an
        # example of how to upgrade this function to deal with exceptions.

        memory_report = self._generate_memory_report_component()

        for _, component, _ in self._reportable_components(
                propagate_exc=True):
            # pylint: disable=protected-access
            rep = component._gather_memory_report()
            if rep != []:
                memory_report.append(rep)

        return memory_report

    def _on_reset(self):#pylint: disable=no-self-use
        """
        Perform actions within the component's internal state to bring it into
        consistency with a freshly-reset chip.  Note: this function is called
        *after* the chip has physically reset.
        """


    def mmu_handles(self):#pylint: disable=no-self-use
        """
        In general, a component doesn't own any MMU handles
        """
        return []

    def _generate_memory_report_component(self):#pylint: disable=no-self-use
        """
        Any class which owns/uses memory should fill this out.
        It should return a Group containing a table holding the memory
        usage information. The most common shape for the table is
        | total | used | unused | percent_used | other_1 | other_N |
        """
        return []

    @property
    def logger(self):
        """
        Instance-specific logger for this object. It is created on demand with
        a minimal default configuration, which can be changed by calling
        setup_logger, or using the logging module's functionality directly.

        NOTE: the logger's name is meaningless (it's the object's id() as a
        string)
        """
        try:
            self._logger
        except AttributeError:
            self._logger = logging.getLogger(str(id(self)))
            # Give the logger a default set-up to start with - i.e. level INFO,
            # no tag, no extra formatting, writing to stdout only
            self.setup_logger()
        return self._logger

    def setup_logger(self, tag=False, fmt=None,
                     level=logging.INFO, stream=True, filen=False):
        """
        Configure a logger for this object

        @param tag A prefix for the output to the logger.  By default (False)
        doesn't use one; if True uses the BaseComponent's title property;
        otherwise should be a string, or convertible to string.
        @param fmt A logging-style format string (see Python logging
        documentation).  Defaults to printing the message itself with no
        further adornment.  Note that this parameter doesn't affect whether a
        tag is set.  I.e. to get exactly the format specified, you must set
        tag=False.  Note that the name of the logger is not interesting so don't
        use formats that include %(name).  Use the tag facility instead.
        @param level The logging level to set the logger at.  This can be
        changed either by a subsequent call to this function or (if you don't
        want to reconstruct the formatting or output options) by calling the
        logger's own setLevel method
        @param stream Log to an output stream.  Can be set to a particular
        stream; by default (True) gstrm.iout is used.  Set to False to disable.
        @param filen Log to the specified file.  Refer to the logging
        documentation for how this file is handled (in terms of appending vs
        truncating etc)
        """

        self.logger.handlers = []

        if tag is True:
            tag = self.title
        if fmt is None:
            fmt = "%(msg)s"
        if tag:
            fmt = "%s: %s" % (tag, fmt)
        formatter = logging.Formatter(fmt)


        if stream:
            configure_logger_for_global_streams(
                self.logger, formatter=formatter)
        if filen:
            hdlr = logging.FileHandler(filen)
            hdlr.setFormatter(formatter)
            self.logger.addHandler(hdlr)

        self.logger.setLevel(level)
