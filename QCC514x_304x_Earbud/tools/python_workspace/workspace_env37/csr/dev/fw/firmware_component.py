############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides class FirmwareComponent.
"""
from csr.wheels.bitsandbobs import TypeCheck
from csr.dev.model.base_component import BaseComponent

class FirmwareComponent(BaseComponent):
    """\
    Firmware Component (Abstract Base)

    Provides shorthand access to firmware environment (meta data and current
    execution state).

    NB If you add new methods to this class which Firmware-derived class would
    implement differently depending on whether the firmware environment is
    available or not, be sure to make a suitable change to the Firmware
    metaclass (FirmwareAttributesRequireEnvMeta).
    """

    class NotDetected(RuntimeError):
        """\
        Should be raised by FirmwareComponent constructors if they do not
        recognise themselves in the fw environment.

        Also raised by create_component_variant() if no variants detected
        in the firmware.
        """

    class Ambiguous(RuntimeError):
        """\
        Exception raised by create_component_variant() if more
        than one variant detected in the firmware.
        """

    @staticmethod
    def create_component_variant(variants, fw_env, core, parent=None):
        """\
        Detect and Create appropriate FirmwareComponent given a set of variant
        sub-types.

        Example:
            fw_variants = (CuratorFirmware, WlanFirmware)
            fw = FirmwareComponent.static_create_variant(fw_variants, fw_env)

        This static method is needed for construction of root
        FirmwareComponents. Subcomponents can be constructed more
        conveniently using the _create_subcomponent_variant() instance method.

        Params:
        -- variants: Set of variant subtypes that may be present in the fw.
        The subtype constructors must check and raise NotDetected
        as early as possible if they cannot recognise themselves
        in the fw environment.
        -- parent: The containing component (or None if top level f/w)

        Raises:
        -- FirmwareComponent.NotDetected: if no variant can be detected.
        -- FirmwareComponent.Ambiguous: if variant detection is ambiguous.

        Limitations:
        -- There is no check that all variants have a common base class but
        passing a set of unrelated "variants" is unlikely to be sensible.
        -- This function is (only) useful when reliable, unambiguous
        variant self-detection is possible. If global resolution is required
        then it is not going to help - that will have to be done by heuristics
        in the containing FirmwareComponent.
        """

        # March through all the variants trying to construct each in turn.
        components = []
        for variant in variants:
            try:
                components.append(variant(fw_env, core, parent))
            except FirmwareComponent.NotDetected:
                continue

        # Hopefully exactly one match?
        if len(components) == 1:
            component = components[0]
            TypeCheck(component, FirmwareComponent)
            return component

        if not components:
            # If there's just one variant, we presume that it's legal for it
            # not to be present, so we return None rather than raising an
            # exception
            if len(variants) == 1:
                return None
            raise FirmwareComponent.NotDetected(
                "No matches in: %s" % str(variants))
        raise FirmwareComponent.Ambiguous(
            "Multiple matches: %s" % str(components))

    def __init__(self, fw_env, core, parent=None):
        """\
        Construct FirmwareComponent

        Future:
        -- parent property may be pointless.
        """
        TypeCheck(parent, (FirmwareComponent, type(None)))

        self.__env = fw_env
        self.__core = core
        self.__parent = parent

    # Extensions

    @property
    def parent(self):
        """\
        The Firmware Component containing this one (or None if root)
        """
        return self.__parent

    # Protected/Provided

    @property
    def _core(self):
        """\
        The CPU core .
        """
        return self.__core

    @property
    def env(self):
        """\
        This components firmware environment.
        """
        return self.__env

    def _create_subcomponent_variant(self, variants):
        """\
        Create subcomponent from set of variant types.

        Example:
            log_variants = (ClassicHydraLog, PerModuleHydraLog)
            self._log = self._create_subcomponent_variant(log_variants)

        Params:
        -- variants: Set of variant subtypes that may be present in the fw.
        The subtype constructors must check and raise NotDetected
        as early as possible if they do not recognise themselves
        in the fw environment.
        -- parent: The containing component (or None if top level f/w)

        Raises:
        -- FirmwareComponent.NotDetected: if no variant can be detected.
        -- FirmwareComponent.Ambiguous: if variant detection is ambiguous.

        Limitations:
        -- There is no check that all variants have a common base class but
        passing a set of unrelated "variants" is unlikely to be sensible.
        -- This function is (only) useful when reliable, unambiguous
        variant self-detection is possible. If global resolution is required
        then it is not going to help - that will have to be done by heuristics
        in the containing FirmwareComponent.
        """
        # Re-use the static method.
        return self.create_component_variant(variants, self.__env, self)


    def _nest_report(self, component):
        return not isinstance(component, str) or component == "slt"

    def _report_interesting_structs(self, structs):
        """
        Report interesting structs, which should be an iterable of structs.
        """
        from csr.dev.model import interface
        elements = []
        for interesting_struct in structs:

            report = interface.Group(interesting_struct)
            try:
                struct = self.env.globalvars[interesting_struct]
                item = interface.Code(
                    "\n".join(struct.display(interesting_struct, "",
                                             [interesting_struct], [])))
            except Exception as exc: #pylint: disable=broad-except
                item = self.report_error(
                    "Error generating report for {}".format(
                        interesting_struct), exc)
            report.append(item)

            elements.append([report, False])
        return elements
