# Copyright (c) 2016-2019 Qualcomm Technologies International, Ltd.
#   %%version
"""
Provides classes DefaultFirmware, FirmwareAttributesRequireEnvMeta,
Firmware, BasicHydraFirmware, GenericHydraFirmware,
FirmwareVersionError, LinuxFirmware
"""

import inspect
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import PureVirtualError, discard_lazy_proxy
from csr.dev.fw.firmware_component import FirmwareComponent, BaseComponent
from csr.dev.fw.structs import IHydraStructHandler
from csr.dev.fw.mib import Mib
from csr.dev.fw.pmalloc import Pmalloc
from csr.dev.fw.mixins.is_loadable import SupportsVVTest
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.tools.top_cpu_consumers import TopCpuConsumers
from csr.wheels.bitsandbobs import construct_lazy_proxy


class NoEnvAttributeError(AttributeError):
    """
    Special variant of AttributeError that lets CoreCommandSetManager detect
    command failures due to attempts to access a Firmware attribute that is
    rendered unavailable at runtime by failure to find the required ELF.
    """

class DefaultFirmware(BaseComponent):
    """
    Simple base class that is independent of Firmware which allows limited
    firmware modelling when no ELF-based environment is available.  This is
    mainly based around the SLT.
    """
    def __init__(self, core):
        self._core = core

    def create_slt(self):
        """
        Most DefaultFirmware instances will need to implement creating a SLT
        """

        raise PureVirtualError(self)

    @property
    def slt(self):
        """The Symbol Lookup Table"""

        # We recreate this everytime because the underlying information
        # may have changed.  For example, patch_ver() relies on this behaviour
        # because it forces ELF load first to ensure that the ELF SLT can be
        # looked up, rather than the FakeSLT, which has no patch version info
        # in it (though perhaps one day it will).
        return self.create_slt()

    @property
    def is_hydra_generic_build(self):
        ''' Returns whether this firmware is a generic hydra build
        or a real subsystem build.
        '''

        return self.slt.is_hydra_generic_build

    def fw_ver(self):
        ''' Redirection for core command fw_ver so that we don't construct
        a slt (and go poking at the chip) until we really need it.
        '''

        build_rpt = None
        try:
            build_str = self._core.dump_build_string
        except AttributeError:
            build_rpt = self.slt.fw_ver()
            build_str = False

        if build_str is None:
            build_number = self.build_number
            if build_number is not None:
                build_str = ("Build number %d (actual build string unavailable)"
                             % build_number)
            else:
                build_str = "Build information unavailable"
        if build_rpt is None:
            if self._core.running_from_rom:
                # We only have strings from the coredump.  We need to be clear
                # that we don't have any information about the patch level
                build_str += (
                    "\nPatch version not directly available. Call "
                    "{}.patch_ver() (loads ELF if necessary).".format(
                        self._core.nicknames[0]))
            build_rpt = interface.Text(build_str)
        return build_rpt

    @property
    def build_string(self):
        """returns a string representing the version of the firmware build"""

        try:
            return self._core.dump_build_string
        except AttributeError:
            return self.slt.build_id_string

    @property
    def build_number(self):
        """
        returns an integer for the build number taken from a coredump
        if in use and available, otherwise taken from the SLT
        """

        try:
            return self._core.dump_build_id
        except AttributeError:
            return self.slt.build_id_number

class FirmwareAttributesRequireEnvMeta(type):
    """
    Metaclass for Firmware-based classes which modifies the class as declared to
    cleanly handle the possibility that a firmware environment is not actually
    available, probably because we ended up being unable to auto-locate firmware
    from the id string.

    The metaclass acts by tweaking the class's methods at the point the class
    is constructed so that everything that relies on the presence of a firmware
    environment is suppressed.  In practice this means:
     - each property and method declared in the class, except for those that
     override BaseComponent or FirmwareComponent methods, is wrapped to raise an
     AttributeError if the fw env can't be constructed
     - the subcomponents property is wrapped with a function that filters out
     any fw env-dependent components if the fw env can't be constructed.  This
     occurs even if subcomponents is only inherited, because the default
     implementation in BaseComponent needs wrapping too.
     - a live_subcomponents override is inserted based on calling
     _all_subcomponents, rather than subcomponents, which the default impl. in
     BaseComponent does.
     - _generate_report_body_elements and related methods, if explicitly
     overridden in this class, are wrapped so that if fw env can't be
     constructed a default implementation is used.  This is the most specialised
     implementation of _generate_report_body_elements that can be found in the
     subset of the class's bases that aren't derived from Firmware, or if none
     is found a null method that just returns an empty list
    """
    def __init__(cls, name, bases, attrs, **kwargs):

        def requires_env(creater):
            """
            Decorator that raises an AttributeError if the firmware environment
            can't be set up.  It also sets an attribute on the returned
            decorating function so that it's clear that it behaves in this
            way.
            """
            def call_if_env_exists(self, *args, **kwargs):
                """Decorator"""
                if construct_lazy_proxy(self.env) is None:
                    raise NoEnvAttributeError(
                        "No firmware environment: can't use "
                        "attribute '%s'" % creater.__name__)
                return creater(self, *args, **kwargs)

            call_if_env_exists.requires_env = True
            return call_if_env_exists

        def filter_subcomponents(subcomponents_property):
            """
            If env can't be set up, filter the list of subcomponents to just
            include those that don't need the fw environment
            """
            def prop_needs_env(cls, prop_name):
                """
                Check whether the property's getter has been wrapped by the
                requires_env decorator
                """
                prop = getattr(cls, prop_name)
                try:
                    return prop.fget.requires_env
                except AttributeError:
                    return False
            def filtered_subcomponents(self):
                """
                Filter the subcomponents list if required
                """
                subcomps = subcomponents_property.fget(self)
                if construct_lazy_proxy(self.env) is None:
                    # Return just the subcomponents that haven't been wrapped
                    # with requires_env
                    return {prop:inst for (prop, inst) in subcomps.items()
                            if not prop_needs_env(self.__class__, prop)}
                # Otherwise return the dictionary untouched
                return subcomps
            # subcomponents is a read-only property so replace it with another
            return property(filtered_subcomponents)

        def requires_env_or_default(env_based_attr, default_attr):
            """
            Replace an env-based method with a function that decides whether to
            call that method or a supplied default based on whether env is
            available.
            """
            def env_based_or_default(self, *args, **kwargs):
                """Returns default of env-based attribute"""
                if construct_lazy_proxy(self.env) is None:
                    return default_attr(self, *args, **kwargs)
                return env_based_attr(self, *args, **kwargs)

            return env_based_or_default

        def get_default_method(meth_name):
            """
            Find the most specialised base class that is a DefaultFirmware but
            not a Firmware which has a _generate_report_body_elements method. If
            this exists we'll use that method as the default. If not we'll just
            pick up BaseComponent's implementation, which does nothing.
            """
            default_method = getattr(BaseComponent, meth_name)
            for baseclass in cls.mro():
                if (issubclass(baseclass, DefaultFirmware) and
                        not issubclass(baseclass, Firmware)):
                    try:
                        method = getattr(baseclass, meth_name)
                    except AttributeError:
                        pass
                    else:
                        if inspect.ismethod(method):
                            return method
            return default_method

        # Call type.__init__ in case that does anything
        super(FirmwareAttributesRequireEnvMeta, cls).__init__(name, bases,
                                                              attrs, **kwargs)

        # Attributes of BaseComponent and FirmwareComponent, including anything
        # they derives from their parents, are not mostly not overridden,
        # make a list:
        comp_attrs = set(dir(BaseComponent) + dir(FirmwareComponent))
        # However, the following need special handling
        comp_wrapped = set(
            ("_generate_report_body_elements",
             "_generate_memory_report_component", "mmu_handles"))

        # Now loop over the attributes of the class wrapping them as appropriate
        for attr in attrs:

            attr_obj = getattr(cls, attr)

            if attr.startswith("__") and attr.endswith("__"):
                # Don't mess with magic methods
                pass
            elif attr in ["build_info"]:
                # Exclude because availability of the build_info object
                # doesn't require the env to be constructable.
                pass
            elif hasattr(attr_obj, "__custom_env_check_set"):
                pass
            elif attr in comp_attrs:
                # Special handling for the some XxxComponent interfaces
                if attr in comp_wrapped:
                    # Wrap this to use the default method if necessary
                    setattr(cls, attr,
                            requires_env_or_default(attr_obj,
                                                    get_default_method(attr)))
                # Otherwise leave well alone.  (We deal with subcomponents and
                # live_subcomponents separately as they may or may not be
                # declared explicitly in this class, as opposed to a base class)

            # Now properties and methods that aren't overrding the BaseComponent
            # or FirmwareComponent interfaces...

            elif isinstance(attr_obj, property):
                # With properties, replace the getter with a decorated
                # version that raises AttributeError if self.env can't be made
                # and otherwise calls the function as normal.  Most likely
                # fset and fdel are unused but set them just in case.
                setattr(cls, attr, property(fget=requires_env(attr_obj.fget),
                                            fset=attr_obj.fset,
                                            fdel=attr_obj.fdel,
                                            doc=attr_obj.__doc__))
            elif inspect.ismethod(attr_obj):
                # Wrap plain methods directly with the same decorator as
                # for property getters
                setattr(cls, attr, requires_env(attr_obj))

        # We need to mess with subcomponents and live_subcomponents even if
        # they're inherited, i.e. not directly attributes of this class
        try:
            # Wrap subcomponents in a decorator that decides at
            # invocation time which entries to return by checking if the
            # firmware environment is or can be made available.
            setattr(cls, "subcomponents",
                    filter_subcomponents(cls.subcomponents))
        except AttributeError:
            pass

        try:
            # Override live_subcomponents to avoid unnecessarily
            # triggering construction of the firmware environment when
            # we're just trying to find out which subcomponents are
            # currently instantiated.
            def live_subcmpts(obj):
                """see above comment"""
                #pylint: disable=protected-access
                subcmpts = obj._all_subcomponents()
                if subcmpts is None:
                    return {}
                return {prop:attr for (prop, attr) in subcmpts.items()
                        if attr is not None and hasattr(obj, attr)}

            setattr(cls, "live_subcomponents", property(live_subcmpts))
        except AttributeError:
            pass


FirmwareAttributesRequireEnv = FirmwareAttributesRequireEnvMeta(
    'FirmwareAttributesRequireEnv', (object,), {'__slots__': ()})


def custom_env_check(meth):
    """
    Simple decorator that tells FirmwareAttributesRequireEnvMeta not to wrap the
    method with requires_env because the method handles the firmware env loading
    itself.  E.g. useful for patch_ver which needs an env if running against
    a coredump but not if running against a live chip.
    """

    # pylint: disable=protected-access
    meth.__custom_env_check_set = True
    return meth


class Firmware(FirmwareComponent, FirmwareAttributesRequireEnv):
    """
    Executable Firmware instance. (Abstract Base)

    Features common to all known firmware, excluding those available in
    DefaultFirmware, which should be mixed in by concrete Firmware classes.
    """

    def __init__(self, fw_env, core, build_info=None):

        FirmwareComponent.__init__(self, fw_env, core)
        self._build_info = build_info
        self.struct_handlers = {}

    # FirmwareComponent compliance

    @property
    def title(self):
        return "Firmware"

    # Extensions
    @property
    def build_info(self):
        """The firmware build information"""
        return self._build_info

    @property
    def gbl(self):
        """The firmware global variables debug info"""
        return self.env.gbl
    @property
    def cu(self): #pylint: disable=invalid-name
        """The firmware compilation unit debug info"""
        return self.env.cu
    @property
    def var(self):
        """The firmware statically allocated variable debug info"""
        return self.env.var
    @property
    def enum(self):
        """The firmware enum type debug info"""
        return self.env.enum
    @property
    def econst(self):
        """The firmware enumeration fields debug info"""
        return self.env.econst

    @custom_env_check
    def patch_ver(self):
        """returns an integer for the patch version or None"""

        if not hasattr(self.slt, "patch_id_number"):
            # If we force the env to be constructed then self.slt returns a
            # real SLT afterwards, which should have a patch_id_number attribute
            if construct_lazy_proxy(self.env) is None:
                raise NoEnvAttributeError("No firmware environment: can't use "
                                          "attribute 'patch_ver'")
        return self.slt.patch_id_number

    @staticmethod
    def _struct_handler_type():
        raise PureVirtualError

    def set_struct_handler(self, struct_name):
        """
        Register a type derived from IStructHandler to use for _Variables of
        type 'struct_name' in place of _Structure.  This is to allow custom
        behaviour such as pretty-printing.

        If the IStructHandler advertises a dependence on other IStructHandlers,
        these are recursively inserted too
        """

        handler_class = self._struct_handler_type().handler_factory(struct_name)
        self.struct_handlers[struct_name] = handler_class
        for name in handler_class.requires():
            self.set_struct_handler(name)

    def clear_struct_handler(self, struct_name):
        """
        Remove custom handler for the named structure.  Note: any other handlers
        added automatically as dependencies are left in place - they must be
        explicitly removed if required.  If the named struct type has no handler
        this function silently does nothing.
        """
        if struct_name in self.struct_handlers:
            del self.struct_handlers[struct_name]

    @property
    def custom_struct_semantics_type(self):
        """
        Allow extended variable semantics to be applied in the decoding of
        structure variables. This allows the user significantly greater control
        over how complex structures are displayed in interactive sessions.
        For more information see csr.dev.fw.struct_semantics.__doc__.
        """
        return None



    def get_union_discrims(self, struct_name):
        """
        Look up the discriminator evaluation function(s) for a given structure
        containing unions.

        This function can be overridden in subsystem-specific Firmware
        subclasses to provide relevant information.  The return value should be
        a dictionary {union_name : evaluator},
        where evaluator is a function that takes the _Structure object and the
        name of the target union and returns the name of the union field that
        is currently valid.

        For example, given the following struct containing unions:

        struct S {
            int representation_type; # 0 => numerical, 1 => textual
            union {
                   int numerical_repr;
                   const char *textual_repr;
                  } u_repr;
        }

        the evaluator would look like:

        def evaluator(struct_s_var, field_name):
            if field_name != "u":
                raise ValueError("Only union in struct S is called 'u' -
                                                     '%s' passed" % field_name)
            if struct_s_var.representation_type.value == 0:
                return "numerical_repr"
            elif struct_s_var.representation_type.value == 1:
                return "textual_repr"
            return None # The discriminator isn't valid so none of the union
                        # fields are meaningful


        """
        #pylint: disable=unused-argument,no-self-use
        return {}

class BasicFirmware(Firmware, SupportsVVTest):
    """
    Provides basic features, for any architecture, not just hydra.
    """

    @property
    def elf(self):
        """
        Publish ELF object for special needs.
        """
        return self.env.elf

    @property
    def dbg(self):
        """
        interface to debugging support for this core
        """
        try:
            self._dbg
        except AttributeError:
            self._dbg = Dbg(self.env, self._core)
        return self._dbg

    def version(self):
        """returns an interface object containing the basic firmware version"""
        #pylint: disable=no-member
        if self.slt:
            return self.slt.fw_ver()
        try:
            return interface.Code(
                "".join([chr(a.value)
                         for a in self.env.globalvars["build_id_string"]]))
        except AttributeError:
            return None

    @property
    def stack(self):
        """
        Specific subsystems' firmware objects should return a stack backtrace
        using a class appropriate to the processor
        """
        raise PureVirtualError

    @property
    def _stack(self, **kwargs):
        """
        Specific subsystems' firmware objects should return a stack backtrace
        using a class appropriate to the processor
        """
        raise PureVirtualError

    def stack_report(self, **kwargs):
        '''
        Turn the backtrace into a reportable object by exploiting the
        StackBacktrace class's __repr__ method
        '''
        grp = interface.Group("Stack")
        grp.append(interface.Code(str(self._stack(**kwargs))))
        return grp

    def top_cpu_consumers(self, max_functions=10, report=False, time_s=1):
        '''
        Returns a table of the functions the processor spends most time in
        '''
        try:
            self._top_cpu_consumers
        except AttributeError:
            self._top_cpu_consumers = TopCpuConsumers(self.env, self._core)
        self._top_cpu_consumers.poll_pc(sample_ms=time_s*1000)
        output = self._top_cpu_consumers.top(max_functions, report=True)

        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)
        return None

class BasicHydraFirmware(BasicFirmware):
    #pylint: disable=too-many-instance-attributes
    """
    Executable Firmware Instance. (Abstract Base)

    Features common to all firmware based on Hydra code base without the
    submsg-based protocols
    """

    def __init__(self, fw_env, core, build_info=None):

        BasicFirmware.__init__(self, fw_env, core,
                               build_info=build_info)

    # Firmware compliance

    def _all_subcomponents(self):
        return {
            "debug_log" : "_debug_log",
            "mib" : "_mib",
            "pmalloc" : "_pmalloc",
            "sched" : "_sched",
            "slt" : "_slt",
            }

    def _generate_report_body_elements(self):
        """
        Output useful firmware information
        """
        elements = []
        try:
            elements.append([self.stack_report(), False])
        except KeyError:
            elements.append(interface.Code("Stack trace failed"))
        return elements

    # ------------------------------------------------------------------------
    # Extensions
    # ------------------------------------------------------------------------

    @property
    def debug_log(self):
        """
        The DebugLog
        """
        # Construct lazily...
        try:
            self._debug_log
        except AttributeError:
            # pylint: disable=assignment-from-no-return
            self._debug_log = self._create_debug_log()

        return self._debug_log

    @property
    def debug_log_decoder(self):
        """
        The DebugLogDecoder.
        """
        # Construct lazily...
        try:
            self._debug_log_decoder
        except AttributeError:
            # pylint: disable=assignment-from-no-return
            self._debug_log_decoder = self._create_debug_log_decoder()

        return self._debug_log_decoder

    @property
    def mib(self):
        """
        MIB set, get, dump.
        """
        # Construct lazily...
        try:
            self._mib
        except AttributeError:
            self._mib = Mib(self.env, self._core)

        return self._mib

    @property
    def pmalloc(self):
        """
        The pool memory allocator.
        """
        # Construct lazily...
        try:
            self._pmalloc
        except AttributeError:
            self._pmalloc = Pmalloc(self.env, self._core)

        return self._pmalloc

#    @property
#    def host_interface_info(self):
#        """
#        Interface to host interface meta data.
#        """
#        # alias to fw meta data
#        return self.info.host_interface_info

    # Subclasses can implement a sched component if they like
    sched = None

    # ------------------------------------------------------------------------
    # Protected / Required
    # ------------------------------------------------------------------------

    def _create_debug_log(self):
        """
        Create DebugLog of suitable type.
        """
        raise PureVirtualError(self)

    def _create_debug_log_decoder(self):
        """
        Create DebugLogDecoder of suitable type.
        """
        raise PureVirtualError(self)

    @staticmethod
    def _struct_handler_type():
        return IHydraStructHandler

class GenericHydraFirmware(BasicHydraFirmware): # pylint: disable=too-many-ancestors
    """
    Extends the BasicHydraFirmware class to cover the submsg-based protocol
    stuff
    """

    def _all_subcomponents(self):
        cmps = BasicHydraFirmware._all_subcomponents(self)
        cmps.update({ })
        return cmps

    def _generate_report_body_elements(self):
        """
        Output useful firmware information
        """
        elements = []

        elements.append(self.version())

        # Output some interesting structures
        self._report_interesting_structs(["subserv_data"])

        elements += BasicHydraFirmware._generate_report_body_elements(self)

        return elements

class FirmwareVersionError(RuntimeError):
    '''Firmware version error'''


class HasDbgCall(object):
    """
    Simple mixin adding the dbgcall and associated call classes
    """
    @property
    def dbgcall(self):
        """
        Provides a pydbg component object representing a debug call (DbgCall)
        """
        try:
            self._dbgcall
        except AttributeError:
            self._dbgcall = FirmwareComponent.create_component_variant(
                (DbgCall,),
                self.env,
                self._core,
                parent=self)
        return self._dbgcall

    @property
    def call(self):
        """
        Perform a debug call, using the property dbgcall.
        This is not __call__.
        """
        try:
            self._call
        except AttributeError:
            if self.dbgcall is None:
                iprint("Function calling not supported in the firmware")
                self._call = None
            else:
                self._call = Call(self.env, self._core, self.dbgcall, 
                                    hal_macros=False)

        return self._call

