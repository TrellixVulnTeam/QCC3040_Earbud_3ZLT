############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides a number of alternative classes derived from a base class 
PydbgFrontEndBase that give a front end to the pydbg tool.  
A default one is stored in variable PydbgFrontEnd.
"""

import re
import sys
import functools
import os
import csr
import platform
try:
    from collections.abc import Sequence, Iterable, Mapping
except ImportError:
    from collections import Sequence, Iterable, Mapping
from csr.wheels.global_streams import dprint, iprint, wprint, eprint, \
    set_streams
from csr.dev.fw.meta.create_fw_environment import  create_hydra_subsystem_fw_environment,  create_simple_fw_environment
from csr.front_end.base_front_end import BaseFrontEnd, ReadlineCmdlineMixin, \
    NullCmdlineMixin, CodeModuleInteractionMixin, \
    PythonInspectInteractionMixin, DaemoniserInteractionMixin
from csr.wheels.bitsandbobs import NameSpace
from csr.wheels.cmdline_options import CmdlineOption, CmdlineOptions
from csr.dev.hw.chip.hydra_chip import HydraChip
from csr.dev.hw.address_space import AddressSpace
from csr.dev.hw.system.system import system_factory, DeviceSystem
from csr.wheels.plugin_manager import PluginManager

try:
    basestring
except NameError:
    # Python 3 does not have basetring builtin
    basestring = str

try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

_csr_root = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))


class PydbgFrontEndBase(BaseFrontEnd):
    """
    Front-end logic for Pydbg.  Handles attaching to the supplied debug 
    transports and controlling set-up of firmware environments.  Provides a 
    couple of levels of interaction - whole session set-up (via main()) and
    single device set-up (via attach())
    
    This class should be subclassed with suitable mixins for handling of 
    interactivity and command-line setup to give required behaviour
    """
    cmdline_options = (BaseFrontEnd.cmdline_options + 
    
            [CmdlineOption("-C", "--cfg_file",
                               help="Config file for default options; "
                               "use '-C-' to generate a template YAML file to edit.",
                               action="store", type=str,
                               dest="cfg_file",
                               default=None),
            
            CmdlineOption("-d", "--device-url",
                          help="Descriptor indicating device(s) to attach to",
                           action="store", type=str,
                           dest="device_url",
                           default="trb:scar"),
            
            CmdlineOption("-f", "--firmware-builds",
                              help="""Firmware build directory, in the format
                            <subsys1_name>:<subsys1_build_dir> or 
                            <subsys1_name>=<subsys1_build_dir>""",
                              action="append", type=str,
                              dest="firmware_builds",
                              default=None),
    
            CmdlineOption("-p", "--patch-builds",
                              help="""Patch build directories, in the format
                            <subsys1_name>:<subsys1_build_dir> or 
                            <subsys2_name>=<subsys2_build_dir>...""",
                              action="append", type=str,
                              dest="patch_builds",
                              default=None),
    
            CmdlineOption("-l", "--preload",
                          help="""Force firmware to be preloaded""",
                          action="store_true",
                          default=False),

            CmdlineOption("-e", "--emulator-build",
                              help="Path to directory containing io_struct files.  Used to find "
                              "io_struct files (these replace the built-in ones for the "
                              "chip, if present). Can be specified multiple times. "
                              "The io_struct files are searched for in slightly different "
                              "ways depending on the chip architecture.  Hydra chips "
                              "expect a emulator build release structure; other chips " 
                              "will search the given directory tree for io_struct files "
                              "with standard names",
                              dest="emulator_build",
                              default=None, action="append", type=str),
        
            CmdlineOption("-i", "--interface-dir",
                              help="Sets the interface directory. "
                                   "Default is <subsystem root>/common/interface",
                              action="store", type=str,
                              dest="interface_dir",
                              default=None),
        
            CmdlineOption("-r", "--enable-recovery-mode",
                              help=("Allow Pydbg to go into an interactive recovery "
                                    "mode in which transport-level operations are "
                                    "available even if the chip is not responsive. "
                                    "WARNING: only use this option in non-"
                                    "interactive sessions if someone will be "
                                    "present to perform manual recovery.  Otherwise "
                                    "the process will wait indefinitely in the "
                                    "interactive interpreter for recovery to be "
                                    "performed."),
                              dest="recovery", action="store_true",
                              default=False),
        
            CmdlineOption("-t", "--target",
                              help="Name of target, for non-CSR chips",
                              default=None),

            CmdlineOption("-z", "--cache-dir",
                          help="Absolute path to directory desired to be used for caching "
                               "subsystem ELF files",
                          dest="cache_dir",
                          default=None),

            CmdlineOption("-c", "--core-commands", 
                          help="Has no effect. Retained for historical reasons",
                          action="store_true",
                          dest="load_core_commands", 
                          default=False),

            CmdlineOption("-v", "--verbosity",
                          help="Set verbosity level for the pydbg interactive session."
                               "Choices are debug, info, warning, error",
                          action="store", type=str,
                          dest="verbosity",
                          default="info"),
            CmdlineOption("-g", "--plugin", dest="plugins",
                          help="Path to application plug-in module",
                          action="append",type=str,
                          default=None),
            CmdlineOption("-q", "--sequences", dest="sequences",
                         help="Paths to sequence package 'top' modules, specified per-core",
                         default=None, action="append", type=str),
            ])

    @staticmethod
    def validate_chip_ss_name(ss_name, _device):
        """
        Validates the subsystem ss occurs on a chip on _device
        """
        ss_found = False
        for _chip in _device.chips:
            try:
                _chip.__getattribute__("%s_subsystem" % ss_name)
                ss_found = True
                break
            except AttributeError:
                pass

        if not ss_found:
            iprint("ERROR no subsystem '%s' available" % ss_name)
            iprint("Valid subsystems on this device are:")
            for _chip in _device.chips:
                for name in dir(_chip):
                    match = re.match(r"([a-z]+)_subsystem", name)
                    if match:
                        iprint(" %s" % match.group(1))
                sys.exit(1)

    @staticmethod
    def core_paths_dict(firmware_builds, _device, is_hydra=True):
        """
        Convert string in firmware_builds into a dictionary of paths per
        subsystem available  on _device. If _device is None then all
        are set in dictionary.
        """
        if not firmware_builds:
            return {}

        # Convert the argument to str type
        # This is for the case when the command line
        # arguments are passed in as list
        if type(firmware_builds) is list:
            firmware_builds_str = ",".join(firmware_builds)
        else:
            firmware_builds_str = firmware_builds

        # The argument syntax is basically a dictionary.  But Windows messes
        # everything up
        fw_dict = {}
        builds = firmware_builds_str.split(",")
        for build in builds:
            match = re.match(r"[a-z_0-9#]{2,}(=|:).*", firmware_builds_str)
            if match:
                sep_char = match.group(1)
                build = build.split(sep_char, 1)
                if len(build) != 2:
                    raise ValueError("Bad syntax in fw option: "
                                     "missing ':'/'=' (%s)" % build[0])
            else:
                raise ValueError("Bad syntax in fw option: "
                                 "missing ':' or '=' '(%s)" % build[0])
            build_dir = build[1]

            if is_hydra:
                build_match = re.match(r"(\w+)(\d)", build[0])
                if build_match:
                    name = build_match.group(1)
                    num = int(build_match.group(2))
                else:
                    name = build[0]
                    num = None
                if _device:
                    PydbgFrontEndBase.validate_chip_ss_name(name, _device)

            if is_hydra and num is not None:
                # If particular builds are specified, pass the build dirs as a
                # core_num->builddir dictionary
                if name not in fw_dict:
                    fw_dict[name] = {}
                fw_dict[name][num] = build_dir
            else:
                fw_dict[build[0]] = build_dir

        return fw_dict

    @staticmethod
    def attach(device_options, interactive=True, primary_device=None,
               shell=globals(), allow_recovery=False):

        _device_or_procman, _trans = PydbgFrontEnd.multi_attach(device_options, 
                                                                interactive=interactive, 
                                                                primary_device=primary_device,
                                                                shell=shell, 
                                                                allow_recovery=allow_recovery)

        if isinstance(_device_or_procman, DeviceSystem):
            raise ValueError("Provided URLs generate multiple devices. Consider calling multi_attach")
        return _device_or_procman, _trans

    @staticmethod
    def multi_attach(device_options, interactive=True, primary_device=None,
               shell=globals(), allow_recovery=False):
        """
        Main entry point for scripted use of Pydbg: creates a single device
        model instance, representing the complete access API to a particular
        device.
        
        Create device and transport objects as specified in the device_options
        parameter.  If construction of the device is impossible because 
        communication across the specified transport doesn't work, the device
        object is returned as None.  The transport object is always returned.
        This allows manual recovery in some cases, if the transport interface
        supports a hard reset of the device-under-test.
        
        :param device_options: Dictionary of configuration options.  Valid
         keys are:
         
         * "device_url" (trb:scar) - Identifier for the transport to attach to
         * "firmware_builds" (None) - Comma-separated list of colon-separated
           firmware_name:firmware_build_path pairs, specifying build directories/
           images for some subset of the independent firmware instances on the
           device.  Typical firmware names for a Hydra chip are "apps1" (for
           the Apps P1 processor) or "audio0" (for the Audio processor).  
         * "target" (None) - Used for chips for which silicon ID cannot be
           determined automatically by transport to indicate what sort of chip
           is connected
         * "emulator_build" - Used to indicate a specific digital FPGA build
           directory corresponding to the image that is present in the target
           emulator 
         * "plugins" - Paths to additional packages to load, currently just to
           represent the P1 application on Hydra chips.
         * "sequences" - Path to sequences package directory, for loading register
           execution sequences.
        
        :param interactive, optional: ignored; retained for backwards 
         compatibilty. DO NOT USE as this parameter may be removed in future.
        :param primary_device, optional: Used to specify the "primary" device 
         when constructing further devices during multi-device set-up.  This is
         only appropriate to specify if the devices are identical (same silicon
         running same firmware).  Its purpose is to allow sharing the firmware
         metadata to save memory.
        :param shell, optional: Specifies a namespace dictionary into which the 
         objects representing processor cores can be inserted for convenient
         retrieval.  Defaults to globals(); can be set to None to disable
         name insertion.
        :param allow_recovery, optional: Indicates to the lower-level device
         creation functionality that errors obtaining the device's identity from
         the debug dongle should be caught so that interactive attempts may be
         made within the session to recover the board (e.g. by invoking a power
         cycle by transport-specific means).
        
        :param returns: (device object, transport object) tuple where 
        device_object is None if the attach was unsuccessful.  
       
        """
        from pprint import pprint
        default_device_options = {"device_url": "trb:scar",
                                  "firmware_builds": None,
                                  "target": None,
                                  "preload": False,
                                  "emulator_build": None,
                                  "cache_dir": None,
                                  "verbosity": "info",
                                  "plugins" : None,
                                  "sequences" : None}
        default_device_options.update(device_options)
        device_options = default_device_options

        from csr.transport.dongle_manager import DongleManager
        mgr = DongleManager()

        from csr.dev.hw.device.device_factory import DeviceAttacher
        _device_or_procman, _trans = DeviceAttacher.checked_attach(
            device_options["firmware_builds"],
            device_options["device_url"],
            allow_recovery,
            device_options["target"],
            device_options["emulator_build"],
            dongle_manager=mgr)
        
        if _device_or_procman is None:
            iprint("WARNING: Couldn't create device model!")
            return _device_or_procman, _trans

        device_model = True
        if isinstance(_device_or_procman, list):
            # Debug partitions may contain a number of "devices"
            # Save all the devices but make the most recent the default
            device_system = system_factory(_device_or_procman,enum_prefix="evt")
            device_obj = _device_or_procman[-1]
            _device_list = _device_or_procman
        else:
            device_obj = _device_or_procman
            _device_list = [device_obj]
            device_system = None

        is_hydra = device_model and isinstance(device_obj.chips[0], HydraChip)

        ss_fw_dict = {}
        if device_options["firmware_builds"]:
            ss_fw_dict = PydbgFrontEndBase.core_paths_dict(\
                device_options["firmware_builds"], device_obj,
                is_hydra=is_hydra)

        if device_options.get("patch_builds", None):
            ss_patch_dict = PydbgFrontEndBase.core_paths_dict(
                device_options["patch_builds"], device_obj, is_hydra=is_hydra)
        else:
            ss_patch_dict = {}

        # There is a mismatch between the subsystem name and the command
        # line convention for bluetooth
        for ss_dict in [ss_fw_dict, ss_patch_dict]:
          try:
              ss_dict["bluetooth"] = ss_dict["bt"]
          except KeyError:
              # If "bt" not present then we don't need to update name
              pass
    
        plugins = PluginManager(device_options["plugins"],
                                default_root=os.path.join(_csr_root, "dev", "fw"))
    
        if device_model:
            
            source_device = primary_device
            sequence_dict = PydbgFrontEndBase.core_paths_dict(device_options["sequences"], source_device)

            for ievt, _device in enumerate(_device_list):
                try:
                    cores = []
                    subsystems = {}
                    for chip in _device.chips:
                        for subsystem in chip.subsystems.values():
                            subsystems[subsystem.name.lower()] = subsystem
                            for core in subsystem.cores:
                                cores.append(core)
    
                except AttributeError:
                    cores = []
                    for chip in _device.chips:
                        cores += [core for core in chip.cores]

                for core in cores:
                    # Install the core into the global namespace
                    if shell is not None:
                        if core.has_data_source:
                            for name in core.nicknames:
                                if len(_device_list) > 1:
                                    name += "e%d" % ievt
                                shell[name] = core

                    # Install default firmware access if no full firmware
                    # environment is available
                    if not hasattr(core, "fw") or core.fw is None:
                        if core.default_firmware_type:
                            core.fw = core.default_firmware_type(core)
                        if core.default_firmware_type or getattr(core,"firmware_type",None):
                            from csr.dev.tools.core_command_set_manager import \
                                                                  CoreCommandSetManager
                            CoreCommandSetManager(core.__dict__).change_focus(core)

                if "interface_dir" not in device_options:
                    device_options["interface_dir"] = None

                if is_hydra:
                    # Loop over cores and set them up to attempt to load firmware by
                    # looking it up from the firmware version ID or the given directory.
                    for ssname, subsystem in subsystems.items():
                        ss_fw_core_dict = {}

                        # Doesn't make sense to build fw environment for subsystems with no cores
                        if len(subsystem.cores) == 0:
                            continue

                        for i, core in enumerate(subsystem.cores):

                            def get_fw_version(core):
                                'Invoked when environment needs to be created'
                                return (core.fw.build_string, core.fw.build_number)

                            # Bind the relevant core to allow the above function to
                            # be called later without needing a reference to the
                            # core at that stage.
                            ss_fw_core_dict[i] = functools.partial(get_fw_version,
                                                              core)

                        # If the subsystem doesn't have separate builds for its cores,
                        # replace the per-core dict with a simple per-subsystem value
                        if not subsystem.has_per_core_firmware:
                            ss_fw_core_dict_or_dir = list(ss_fw_core_dict.values())[0]
                        else:
                            ss_fw_core_dict_or_dir = ss_fw_core_dict

                        # Determine if directory has been provided for this subsystems
                        # ELF desired ELF file and handle according to number of subsystem cores.
                        build_dir = ss_fw_dict.get(ssname, None)
                        elf_dict_or_dir = {}

                        if not subsystem.has_per_core_firmware:
                            if isinstance(build_dir, dict):
                                # We have already reduced down to single dict entry in this case
                                elf_dict_or_dir = list(build_dir.values())[0]
                            else:
                                elf_dict_or_dir = build_dir
                        else:
                            if isinstance(build_dir, str):
                                    elf_dict_or_dir = build_dir
                            elif isinstance(build_dir, dict):
                                for core in ss_fw_core_dict_or_dir:
                                    if isinstance(build_dir, dict):
                                        elf_dict_or_dir[core] = build_dir.get(core, None)
                            else:
                                elf_dict_or_dir = None
    
                        # In future we may not be able to assume that firmware
                        # versions are identical for multi-event sets.
                        if source_device is None and ievt > 0:
                            source_device = _device_list[0]
                        if isinstance(source_device, DeviceSystem):
                            source_device = source_device[0]
    
                        lazy_proxy = not device_options["preload"] if build_dir else True
                        create_hydra_subsystem_fw_environment(
                                 _device, ssname,
                                 ss_fw_core_dict_or_dir,
                                 patch_build_dir=ss_patch_dict.get(ssname, None),
                                 install_core=True,
                                 gbl_shell=shell,
                                 load_program_space_cache=(_trans is None),
                                 clone_from=source_device,
                                 interface_dir=device_options["interface_dir"],
                                 lazyproxy=lazy_proxy,
                                 elf_dir=elf_dict_or_dir,
                                 cache_dir=device_options["cache_dir"],
                                 plugins=plugins)
    
                    # If we are using tctrans suppress some Curator logging to
                    # avoid corrupting the log
                    if hasattr(_trans, "tctrans"):
                        _device.chip.curator_subsystem.core.suppress_tc_logging()
                    # If we are using usbcc attempt to call a function to stop the
                    # earbud going dormant under our feet
                    try:
                        _trans.transport_type
                    except AttributeError:
                        pass
                    else:
                        if _trans.transport_type == "usbcc":
                            # Call the hook now, and register it to be called whenever the device is reset
                            apps1 = _device.chip.apps_subsystem.p1
                            def dormant_reset_hook(apps1):
                                return apps1.disallow_dormant()
                            dormant_reset_hook(apps1)
                            apps1.add_reset_hook(dormant_reset_hook)



        
                else:
                    iprint ("No firmware environment support for " 
                           "'%s' chips" % _device.chips[0].__class__.__name__)
        
        # Insert all the var_* helper functions from env_helpers
        if shell is not None:
            from csr.dev.env import env_helpers
            for v in (v for v in dir(env_helpers) if v.startswith("var_")):
                shell[v] = getattr(env_helpers, v)
            shell["set_int_wrap_policy"] = env_helpers._Integer.set_wrap_policy
            del env_helpers

            # Insert TextAdaptor and ObjectAdaptor helpers for convenience
            from csr.dev.adaptor.text_adaptor import text_adaptor
            shell["text_adaptor"] = text_adaptor
            from csr.dev.adaptor.object_adaptor import object_adaptor
            shell["object_adaptor"] = object_adaptor


        return (device_system if device_system is not None else device_obj), _trans

                
    @staticmethod
    def set_device(id, set_prompt=True):
        """
        Make the indicated device the one "in focus", i.e. that csr.dev points
        at.  The others are still available if required, but have to be 
        accessed explicitly via e.g. csr.devs[2].attached_device etc 
        """
        try:
            dev = csr.devs[id]
        except IndexError:
            if len(csr.devs) == 1:
                iprint("ERROR: Single-device session")
            else:
                iprint("ERROR: Only %d devices available" % len(csr.devs))
            return
        
        csr.shell.update(dev.globals)
        if "device" in dev.globals:
            csr.dev.attached_device = dev.globals["device"]
            if "chips" in dev.globals:
                csr.dev.attached_chips = dev.globals["chips"]
            if "chip" in dev.globals:
                csr.dev.attached_chip = dev.globals["chip"]
        else:
            csr.dev.attached_procman = dev.globals["procman"]
        if dev.globals["trans"] is not None:
            csr.dev.transport = dev.globals["trans"]
        if "trb_raw" in dev.globals:
            csr.dev.trb_raw = dev.globals["trb_raw"]

        # Never do this if we're running inside QMDE as it seems to expect a
        # particular value for the prompt.
        if set_prompt and os.getenv("PYDBG_RUNNING_IN_SUBPROCESS") is None:
            sys.ps1 = "%d>>> " % id
    
    @staticmethod
    def for_all_devices(callable_string, *args, **kw_args):
        "find the membership hierarchy"
        cmpts = callable_string.split(".")
        for i in range(len(csr.devs)):
            PydbgFrontEnd.set_device(i)
            base = globals()[cmpts[0]]
            for cmpt in cmpts[1:]:
                base = getattr(base, cmpt)
            base(*args, **kw_args)

    
    class DeviceToggler(object):
        """
        Dodgy little class to make toggling context a matter of "displaying"
        a magic variable
        """
        def __repr__(self):
            id = sys.ps1.rstrip().rstrip(">>>")
            if id == '':
                return 'No other device to which to toggle'
            id = int(id)
            new_id = (id+1) % len(csr.devs)
            PydbgFrontEnd.set_device(new_id)
            return "Switched to device %d" % new_id
    
    # ----------------------------------------------------------------------------
    # Main
    # ----------------------------------------------------------------------------
    
    @staticmethod
    def main(options, interactive=True, shell=globals()):
        '''
        Initialise the Pydbg environment
        
        Note: this interface is for Pydbg's internal use and shouldn't be
        called from other scripts unless you know exactly what you are doing.
        If you want to create an interactive session mimicking Pydbg's standard
        interactive session, call create_session instead.
        
        :param options: Namespace object containing parsed commandline options
        :param interactive: Unused.  Should be removed.
        :param shell, optional: Namespace dictionary into which useful objects
         relating to the device model and transport can be inserted.
        
        :return: Boolean indicating if everything worked or not
        '''

        set_verbosity(options.values["verbosity"])
        urls = options.values["device_url"].split(",")
        # Pass in the values without the device_url
        option_values = {k:v for (k,v) in options.values.items() if k != "device_url"}
        allow_recovery = option_values.get("recovery", None)
            

        return PydbgFrontEnd._main(urls, option_values, interactive=interactive,
                                   shell=shell, 
                                   allow_recovery=allow_recovery)

    @staticmethod
    def _main(urls, options, interactive=True, allow_recovery=None, 
              shell=globals()):
        """
        Entry point for interactive sessions
        :param urls: Iterable of device URLs, including extra flags like device
         labels (must be an iterable even if only one device is to be attached)
        :param options: Dictionary of device configuration options (not including
         the URL) or list of such dictionaies.  If a list is supplied it is 
         assumed that there are as many entries as there are URLs in the urls
         parameter.  If a single value is supplied the same options are applied
         to all devices.
        """
        urls = list(urls)
        if isinstance(options, Sequence):
            # We've got a list of options, so each device will have a completely
            # separate firmware environment
            options = list(options)
            shared_env = False
        else:
            # We're sharing a single option set, so each device will share its
            # firmware environment
            options = [options]*len(urls)
            shared_env = True

        shell["set_dev"] = PydbgFrontEnd.set_device
        shell["all_devs"] = PydbgFrontEnd.for_all_devices
        csr.shell = shell # Stash a reference to the interactive globals dict so
        # we can fiddle with it in set_device and XCD importers

        # Establish standalone (threading) environment for portable framework
        # components.
        #
        from csr.dev.framework.env import StandaloneEnv
        csr.dev.framework.runtime = StandaloneEnv()
        if len(urls) > 1 and interactive:
            shell["D"] = PydbgFrontEnd.DeviceToggler()
    
        csr.devs = [None]*len(urls)
        setup_ok = True
        system_devices = []
        labels = []
        raw_urls = []
        for i, url in enumerate(urls):

            # Are there system labels on the end of the URLs?  Make sure we 
            # handle the possibility of equals signs inside the URLs (could
            # appear in an coredump path, in theory)
            url_split=url.split("=")
            if len(url_split) > 1:
                unlabelled_url, label = "=".join(url_split[:-1]), url_split[-1]
                if re.match(r"\w+", label):
                    # Only take the label if it is something that could function
                    # as a Python name token
                    labels.append(label)
                    url = unlabelled_url
            raw_urls.append(url)
            
        if 0 < len(labels) < len(urls):
            raise ValueError("Labels must be supplied for all URLs or none")
        urls = raw_urls
        
        for i, url in enumerate(urls):
            csr.devs[i] = NameSpace()
            # Attach to Device (local or proxy according to url)
            #
            iprint("Attaching to Device @ \"%s\"..." % url)
            
            dev = csr.devs[i]
            dev.globals = {}
            
            attach_options = options[i].copy()
            attach_options["device_url"] = url
            if allow_recovery is None:
                # allow_recovery defaults to True for an interactive session
                # and False otherwise
                allow_recovery = interactive 
            
            _device, _trans = PydbgFrontEnd.multi_attach(\
                attach_options,
                interactive=interactive,
                primary_device=( \
                    None if not shared_env or i == 0 else csr.devs[0].attached_device), 
                shell=dev.globals,
                allow_recovery=allow_recovery)

            device_model = True
            if isinstance(_device, DeviceSystem):
                # Debug partitions may contain a number of "devices"
                # Save all the devices but make the most recent the default
                _representative_device = _device[0]
                system_devices.append(_device)
            else:
                _representative_device = _device
                system_devices.append(_device)
    
            # We might not get a device if there was a problem talking to the
            # chip
            if _representative_device is not None:
                # Install device objects into the modules for easy access
                
                iprint("device = %s" % (_representative_device.name))
                _chips = _device.chips
                _representative_chips = _representative_device.chips
                try:
                    for _chip in _representative_chips:
                        if not (isinstance(_chip, HydraChip) and options[i]["target"]):
                            # Only attempt to look up the name of a HydraChip
                            # if it has been autodetected
                            try:
                                iprint("chip = %s" % (_chip.full_name))
                            except AttributeError:
                                iprint("chip = %s" % (_chip.name))
                except AddressSpace.ReadFailure as e:
                    if allow_recovery:
                        iprint ("Error accessing chip: '%s'" % e)
                        iprint ("Device model deleted.")
                        _device = None
                    else:
                        raise
        
            if _device is not None:
                # Install into csr space for others to find.
                #
                
                dev.attached_device = _device 
                dev.attached_chips = _chips

            # Provide a function that lets us easily re-create an identical
            # session (assuming sys.argv hasn't been messed about with in the
            # mean time). 
            dev.globals["reattach"] = functools.partial(PydbgFrontEnd.main_wrapper,
                                                            shell=shell)
            # We won't have a debug transport if we've loaded from a coredump
            if _trans is not None:
                dev.transport = _trans
                try:
                    from csr.transport.trb_raw import TrbRaw
                    dev.trb_raw = TrbRaw(_trans)
                except (TypeError, ImportError):
                    #We're evidently not on TRB, so just pretend this never happened
                    pass
            
            # Install _device and _chip in global dictionary
            #
            
            dev.globals["device"] = _device
            
            if _device is None:
                setup_ok = False
                if len(urls) == 1:
                    iprint (
"""
    *****************************************************************
    WARNING: Unresponsive device: recovery required.  
     - Check debug transport dongle is correctly connected/enumerated
     - %s
     - Call 'reattach()' to re-attempt initialisation
    ******************************************************************
""" % _trans.hard_reset_doc("trans"))
                else:
                    iprint(
"""
    *****************************************************************
    WARNING: Unresponsive device: recovery required.  
     - Check debug transport dongle is correctly connected/enumerated
     - Call set_dev(%d) to select this connection in the session
     - %s
     - Call 'reattach()' to re-attempt initialisation
    ******************************************************************
""" % (i, _trans.hard_reset_doc("trans")))
                        
            if _trans is not None:
                try:
                    dev.globals["trb_raw"] = dev.trb_raw
                except AttributeError:
                    #We're evidently not on TRB, so just pretend this never happened
                    pass
            if _device is not None:
                if len(_chips) == 1:
                    dev.globals["chip"] = _chips[0]
                dev.globals["chips"] = _chips

            dev.globals["trans"] = _trans

            if len(urls) > 1:
                # Add all the globals in for this device with a letter suffix
                suffix = chr(ord("A")+i)
                shell.update({name+suffix:obj for (name,obj) in dev.globals.items()})

        if system_devices:
            shell["system"] = system_factory(system_devices,
                                             shared_env=shared_env, labels=labels)
                 
                
        PydbgFrontEnd.set_device(0, set_prompt=(len(urls) > 1))

        return setup_ok
    
                        
class PydbgRlPythonInspectFrontEnd(PydbgFrontEndBase, ReadlineCmdlineMixin, 
                                   PythonInspectInteractionMixin):
    """
    The classic pydbg interactive front end
    Uses the PYTHONINSPECT env var to trigger an interactive session once the 
    set-up logic has completed.
    """

class PydbgCodeInteractFrontEnd(PydbgFrontEndBase, NullCmdlineMixin,
                                CodeModuleInteractionMixin):
    """
    A pydbg front end that uses code.interact and doesn't do anything
    with readline (so doesn't support tab-completion or cross-session command
    history) 
    """
    
class PydbgRlCodeInteractFrontEnd(PydbgFrontEndBase, ReadlineCmdlineMixin,
                                  CodeModuleInteractionMixin):
    """
    An alternative front end that uses code.interact and readline.  
    code.interact is the start Python library method for running an interactive
    interpreter inside a script, but at least anecdotally, it doesn't play very
    nicely with the readline support.
    """

class PydbgRlDaemonisedInteractFrontEnd(PydbgFrontEndBase, ReadlineCmdlineMixin,
                                        DaemoniserInteractionMixin):
    """
    An alternative front end that uses code.interact and readline.  
    code.interact is the start Python library method for running an interactive
    interpreter inside a script, but at least anecdotally, it doesn't play very
    nicely with the readline support.
    """
    
try:
    __IPYTHON__
    PydbgFrontEnd = PydbgRlPythonInspectFrontEnd
except NameError:
    PydbgFrontEnd = PydbgRlCodeInteractFrontEnd


def _build_options_dict(device_url, firmware_builds=None, patch_builds=None,
                        interface_dir=None, emulator_build=None, target=None,
                        preload=False, cache_dir=None, plugins=None,
                        sequences=None):
    return {"device_url" : device_url,
            "firmware_builds" : (",".join(":".join(item)
                               for item in firmware_builds.items())
                                          if firmware_builds is not None else None),
            "patch_builds" : (",".join(":".join(item) 
                               for item in patch_builds.items()) 
                                          if patch_builds is not None else None),
            "interface_dir" : interface_dir,
            "target" : target,
            "preload" : preload,
            "emulator_build" : emulator_build,
            "cache_dir" : cache_dir,
            "plugins": plugins,
            "sequences" : sequences}


def device_attach(device_url, firmware_builds=None, patch_builds=None,
                  interface_dir=None, emulator_build=None, 
                  target=None,  preload=False, cache_dir=None,
                  plugins=None, sequences=None, primary_device=None, shell=globals()):
    """
    Simplified device attach API for creation of device objects
    
    :param device_url: string representing the device or coredump to attach to
    :param firmware_builds, optional: Dictionary mapping core names to paths to
     build directories or ELF files
    :param patch_builds, optional: Dictionary mapping core names to 
     patch build directory/patch ELF file paths
    :param interface_dir: Path to the location of the Hydra interface XML files.
     Only required for Hydra chips in the case where the build ELFs are not in a
     standard source tree. 
    :param emulator_build, optional: Path to the digital results area of an
     emulator build.  Used to pick up register definitions during the early
     stages of chip development when they are not checked into the Pylib source
     tree
    :param target, optional: Identifier for the chip to attach to (not needed for
     Hydra chips)
    :param preload, optional: Load all firmware environments at start-up (not
     normally necessary)
    :param cache_dir: custom directory for the ELF file cache.  The cache dir is
     only used on Windows (at present).  A custom cache directory is usually only
     needed to avoid clashes between separate Pydbg processes that are running
     simultaneously.
    :param plugins, optional: Paths to additional packages to load, currently
     just to represent the P1 application on Hydra chips.
    :param sequences, optional: Path to package of SCALe register access sequences
    :param primary_device, optional: Used in multi-device scenarios to allow 
     subsequent device to clone the first device's firmware environment
    :param shell, optional: namespace (i.e. dictionary) into which core objects
     are inserted for convenience.  Can be None.
    """
    
    device_options = _build_options_dict(device_url, firmware_builds, patch_builds,
                                         interface_dir, emulator_build, target, preload, 
                                         cache_dir, plugins, sequences)
    device, _ = PydbgFrontEnd.attach(device_options, primary_device=primary_device, 
                                     shell=shell)
    return device
    

def system_attach(device_urls, firmware_builds=None, patch_builds=None,
                  interface_dirs=None, emulator_builds=None, 
                  targets=None, 
                  preload=False, cache_dir=None, plugins=None, sequences=None,
                  shell=globals(),
                  shared_env=None, labels=None):
    """
    System attach API for creation of systems of device objects.  These can be
    homogeneous (identical devices running identical firmware) or heterogeneous 
    systems.  Homogeneous systems share the build information per core, which 
    saves start-up time and memory.
    
    :param device_urls: iterable of strings representing the devices or coredumps 
     to attach to
    :param firmware_builds, optional: Dictionary (for homogeneous systems) or 
     iterable of dictionaries (for heterogeneous systems) mapping core names to 
     paths to build directories or ELF files
    :param patch_builds, optional: Dictionary (for homogeneous systems) or 
     iterable of dictionaries (for heterogeneous systems) mapping core names to 
     patch build directory/patch ELF file paths
    :param interface_dirs: Path (for homogeneous systems) or iterable of paths
     (for heterogeneous systems) to the location of the Hydra interface XML files.
     Only required for Hydra chips in the case where the build ELFs are not in a
     standard source tree. 
    :param emulator_builds, optional: Path or iterable of paths to the digital 
     results area of an emulator build.  Used to pick up register definitions 
     during the early stages of chip development when they are not checked into 
     the Pylib source tree.  The iterable can yield None for devices in the
     system for which this isnh't needed.
    :param targets, optional: Identifier or iterable of identifiers for the chip 
     (or chips) that is to be attached to (not needed for Hydra chips). The
     iterable can yield None for devices in the system where this isn't needed.
    :param preload, optional: Load all firmware environments at start-up (not
     normally necessary)
    :param cache_dir: custom directory for the ELF file cache.  The cache dir is
     only used on Windows (at present).  A custom cache directory is usually only
     needed to avoid clashes between separate Pydbg processes that are running
     simultaneously.
    :param plugins, optional: Paths to additional packages to load, currently
     just to represent the P1 application on Hydra chips.
    :param sequences, optional: Path to package of SCALe register access sequences
    :param primary_device, optional: Used in multi-device scenarios to allow 
     subsequent device to clone the first device's firmware environment
    :param shell, optional: namespace (i.e. dictionary) into which core objects
     are inserted for convenience.  Can be None.
    :param shared_env, optional: Indicates whether the system is homogeneous
     (i.e. identical devices running identical firmware, the default) or 
     heterogeneous.  If heterogeneous, any of the arguments firmware_builds, 
     target or emulator_build which are supplied must yield values for every
     device_url, even if the yielded value is None.  (If they are not provided,
     it is as if an iterable yielding None every time had been provided).
    :param labels, optional: Iterable of label strings for each device in the
     system.  If not provided the labels default to A, B, C, ... 
    """
    # Attempt to deduce what shared_env should be.
    if shared_env is None:
        if firmware_builds is not None:
            if isinstance(firmware_builds, Mapping):
                # Single dict supplied: assume common environment 
                shared_env = True
            else:
                # Otherwise, not shared
                shared_env = False
        else:
            # We're being left to auto-detect the firmware.  The only safe thing
            # is to assume they have distinct envs
            shared_env = False

    if not shared_env:
        def none_filler():
            while True: yield None
        firmware_builds = firmware_builds or none_filler()
        patch_builds = patch_builds or none_filler()
        interface_dirs = interface_dirs or none_filler()
        emulator_builds = emulator_builds or none_filler()
        targets = targets or none_filler()
        plugins = plugins or none_filler()
        sequences = sequences or none_filler()
        
        # Devices have different firmware
        def dev_attach(url, fw, pt, itf, emu, tgt, pl, se):
            return device_attach(url, firmware_builds=fw, patch_builds=pt,
                                 interface_dir=itf, target=tgt,
                                 preload=preload, emulator_build=emu,
                                 cache_dir=cache_dir, plugins=pl, sequences=se, shell=shell)
        devices = (dev_attach(url, fw, pt, itf, emu, tgt, pl, se) for (url, fw, pt, itf,
                                                               emu, tgt, pl, se)
                                         in zip(device_urls, firmware_builds,
                                                patch_builds, interface_dirs,
                                                emulator_builds, targets, plugins, 
                                                sequences))
    else:
        def dev_attach(url, primary_device=None):
            return device_attach(url, firmware_builds=firmware_builds,
                                 patch_builds=patch_builds, 
                                 interface_dir=interface_dirs, 
                                 target=targets, preload=preload, 
                                 emulator_build=emulator_builds,
                                 cache_dir=cache_dir, plugins=plugins,
                                 sequences=sequences,
                                 shell=shell,
                                 primary_device=primary_device)
        urls = list(device_urls)
        primary = dev_attach(urls[0])
        devices = [primary] + [dev_attach(url, primary) for url in urls[1:]]
        
    return system_factory(devices, shared_env=shared_env, labels=labels)


def create_session(device_urls, firmware_builds=None, patch_builds=None,
                   interface_dirs=None, emulator_builds=None, 
                   targets=None, preload=False, cache_dir=None, 
                   plugins=None, sequences=None,
                   shell=globals(), shared_env=None, allow_recovery=None ):
    """
    Session creation API for creation of an interactive session with one or more
    devices.  Where there is more than one device, the session can be homogeneous 
    (identical devices running identical firmware) or heterogeneous.  Homogeneous 
    systems share the build information per core, which saves start-up time and 
    memory.
    
    :param device_urls: iterable of strings representing the devices or coredumps 
     to attach to
    :param firmware_builds, optional: Dictionary (for homogeneous systems) or 
     iterable of dictionaries (for heterogeneous systems) mapping core names to 
     build directory/ELF file paths
    :param patch_builds, optional: Dictionary (for homogeneous systems) or 
     iterable of dictionaries (for heterogeneous systems) mapping core names to 
     patch build directory/patch ELF file paths
    :param interface_dirs: Path (for homogeneous systems) or iterable of paths
     (for heterogeneous systems) to the location of the Hydra interface XML files.
     Only required for Hydra chips in the case where the build ELFs are not in a
     standard source tree. 
    :param emulator_builds, optional: Path or iterable of paths to the digital 
     results area of an emulator build.  Used to pick up register definitions 
     during the early stages of chip development when they are not checked into 
     the Pylib source tree.  The iterable can yield None for devices in the
     system for which this isnh't needed.
    :param targets, optional: Identifier or iterable of identifiers for the chip 
     (or chips) that is to be attached to (not needed for Hydra chips). The
     iterable can yield None for devices in the system where this isn't needed.
    :param preload, optional: Load all firmware environments at start-up (not
     normally necessary)
    :param cache_dir: custom directory for the ELF file cache.  The cache dir is
     only used on Windows (at present).  A custom cache directory is usually only
     needed to avoid clashes between separate Pydbg processes that are running
     simultaneously.
    :param plugins, optional: A list of paths to additional packages to load,
     currently just to represent the P1 application on Hydra chips.
    :param sequences, optional: Path to package of SCALe register access sequences
    :param shell, optional: namespace (i.e. dictionary) into which core objects
     are inserted for convenience.  Can be None.
    :param sequences, optional: Path to package of SCALe register access sequences
    :param shared_env, optional: Indicates whether the system is homogeneous
     (i.e. identical devices running identical firmware, the default) or 
     heterogeneous.  If heterogeneous, any of the arguments 'firmware_builds', 
     'patch_builds', 'interface_dirs','targets' or 'emulator_builds' which are 
     supplied must yield values for every device_url, even if the yielded value 
     is None.  (If they are not provided, it is as if an iterable yielding None 
     every time had been provided). As a convenience, if shared_env is not set 
     we attempt to deduce the appropriate value from the shape of the 
     'firmware_builds' parameter.  If this has been provided and is a single 
     dictionary-like object, we assume this is common build info for all devices 
     and we act as if shared_env had been set to True.  Otherwise, we assume 
     each device should have independent environments, and set it to False.
    :param allow_recovery, optional: Indicates how to handle failure to create
     any of the device objects for the session: if False, errors are simply
     propagated, but if True, certain errors are caught and the underlying
     attach completes, but with return device object set to None.  This allows
     operations to be performed interactively on the transport object to attempt
     to recover the situation/
    """
    
    # Attempt to deduce what shared_env should be.
    if shared_env is None:
        if firmware_builds is not None:
            if isinstance(firmware_builds, Mapping):
                # Single dict supplied: assume common environment 
                shared_env = True
            else:
                # Otherwise, not shared
                shared_env = False
        else:
            # We're being left to auto-detect the firmware.  The only safe thing
            # is to assume they have distinct envs
            shared_env = False
    
    if not shared_env:
        def none_filler(): 
            while True: yield None
        firmware_builds = firmware_builds or none_filler()
        patch_builds = patch_builds or none_filler()
        interface_dirs = interface_dirs or none_filler()
        emulator_builds = emulator_builds or none_filler()
        targets = targets or none_filler()
        plugins = plugins or none_filler()
        sequences = sequences or none_filler()
        options = [_build_options_dict("", fw, pa, itf, em, tgt,
                                       preload, cache_dir, pl, se)
                   for (_,fw,pa,itf,em,tgt,pl,se) in zip(device_urls,firmware_builds, patch_builds,
                                            interface_dirs, emulator_builds, targets, 
                                            plugins, sequences)]
    else:
        options = _build_options_dict("", firmware_builds, patch_builds,
                                      interface_dirs, emulator_builds, 
                                       targets, preload, cache_dir, 
                                       plugins, sequences)
    
    if sys.version_info < (3,0):
        string_types = (str,unicode)
    else:
        string_types = (bytes, str)
    
    if isinstance(device_urls, string_types) or not isinstance(device_urls, Iterable):
        device_urls = [device_urls]
    else:
        device_urls = list(device_urls)
    if PydbgFrontEnd._main(device_urls, options, shell=shell, 
                           allow_recovery=allow_recovery):
        PydbgFrontEnd._go_interactive(shell)


def get_attached_trb_and_usbdbg_devices(trb_driver=None):
    """
    Return a pair of lists (each possibly empty) containing the details of
    dongles attached to this host via TRB or USBDBG.
    
    The function selects an appropriate default for trb_driver based on the
    host platform (Scarlet for Windows and Murphy for Linux), but this can
    be overridden via the optional trb_driver argument.
    
    USBDBG isn't supported on Linux, but the function returns an empty list
    for this anyway for consistency.
    
    The lists contain the native TrbTrans/TcTrans device metadata objects, which
    have in common an attribute "id", which can be used as a suffix to the 
    appropriate device URL when setting up arguments to pass to the Pydbg device 
    attach / session creation API (e.g. "trb:usb2trb:<id>", "tc:usb2tc:<id>") 
    """
    
    from csr.transport import trbtrans, tctrans
    system = platform.system()
    is_32bit = sys.maxsize == (1 << 31) - 1 

    if system == "Windows":
        if trb_driver is None:
            trb_driver = "usb2trb"
        lib_path="win32" if is_32bit else "win64"
        trb = trbtrans.Trb(override_lib_path=os.path.join(lib_path, "trbtrans.dll"))
        trb_devices = trb.build_dongle_list(trb_driver)
    
        tc = tctrans.TcTrans(override_lib_path=os.path.join(lib_path,"tctrans.dll"))
        tc_devices = tc.enumerate_devices()
    
    else:
        if trb_driver is None:
            trb_driver = "murphy"
        trb = trbtrans.Trb()
        trb_devices = trb.build_dongle_list(trb_driver)
        tc_devices = []

    return trb_devices, tc_devices


def set_verbosity(verbosity_level):
    """ Set verbosity level for pydbg interactive session"""

    if verbosity_level.lower() == "debug":
        """ Nothing is redirected to a null stream """
        set_streams()
    elif verbosity_level.lower() == "info":
        """ Set logging level to info and above """
        set_streams(dstream=False)
    elif verbosity_level.lower() == "warning":
        set_streams(dstream=False, istream=False)
    elif verbosity_level.lower() == "error":
        set_streams(dstream=False, istream=False, wstream=False)
    else:
        """ 
        Throw an error if the argument 
        does not match any of the above
        """
        error_str = "-v argument did not match any of the allowed values."\
                    " Value given: " + verbosity_level + ", whereas value" \
                    " expected is debug, info, warning or error"
        raise ValueError(error_str)


def pkg_main():
    """
    A simple routine entry point that setuptools:setup.py can
    provide as an executable entry point when project is built via setup.py
    """
    try:
        from csr.version import PYDBG_VERSION
    except ImportError:
        PYDBG_VERSION = None

    if "--version" in sys.argv:
        iprint(PYDBG_VERSION if PYDBG_VERSION is not None else "<unversioned>")
        sys.exit(0)

    PydbgFrontEnd.main_wrapper(shell=globals(), version=PYDBG_VERSION)
