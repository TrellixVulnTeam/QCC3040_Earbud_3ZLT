############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Various high level routines that create a firmware environment.
Caller chooses appropriate routine to use.
They are all named create_xxxxx_fw_environment.
"""
import os, glob, sys
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import LazyProxy, retrieve_lazy_proxy_setup
from csr.dev.fw.meta.i_firmware_build_info import IFirmwareBuildInfo, \
IGenericHydraFirmwareBuildInfo
from csr.dev.env.standalone_env import StandaloneFirmwareEnvironment
from csr.dev.fw.firmware_component import FirmwareComponent

if sys.version_info >= (3, 0):
    str_type = (str,)
else:
    str_type = (str, unicode) # pylint: disable=undefined-variable

def create_hydra_subsystem_fw_environment(device, subsystem, build_dir_or_dict,
                                          patch_build_dir=None,
                                          xide_macros=True,
                                          install_core=True,
                                          gbl_shell=None,
                                          load_program_space_cache=False,
                                          clone_from=None,
                                          interface_dir=None,
                                          lazyproxy=True,
                                          elf_dir=None,
                                          cache_dir=None,
                                          plugins=None):
    '''
    Set up the firmware environment of the given subsystem in the given device,
    if it is valid.
    '''

    # Firmware Build info  (could in theory be common to multiple
    # cores on multiple devices).
    #
    #iprint("Creating firmware environment for subsystem '%s'" % subsystem)

    clone_device = clone_from if clone_from is not None else device

    # Find the indicated subsystem in one of the device's chips.  Assume if
    # there's more than one that the first matching one is the one that should
    # be returned
    target_subsys = None
    for chip, clone_chip in zip(device.chips, clone_device.chips):
        try:
            if subsystem == "bluetooth":
                subsystem = "bt"
            target_subsys = getattr(chip, "%s_subsystem" % subsystem)
            clone_subsys = getattr(clone_chip, "%s_subsystem" % subsystem)
            break
        except AttributeError:
            pass # Maybe the subsystem is in a different chip in the device
    if target_subsys is None:
        iprint("WARNING: %s has no subsystem '%s'" % (device.name, subsystem))
        return

    if isinstance(build_dir_or_dict, dict):
        bad_core_ids = [i for i in build_dir_or_dict.keys()
                        if i >= len(target_subsys.cores)]
        if bad_core_ids:
            tail = (" one core" if len(target_subsys.cores) == 1
                    else " %d cores" % len(target_subsys.cores))
            iprint("WARNING: bad build dir specification: %s has only %s" %
                   (subsystem, tail))

    for i, (core, clone_core) in enumerate(zip(target_subsys.cores,
                                               clone_subsys.cores)):
        
        if target_subsys.has_per_core_firmware:
            if isinstance(build_dir_or_dict, dict):
                # If we've been passed separate build dirs for each core,
                # use them
                if i not in build_dir_or_dict:
                    # If we haven't been pointed at firmware,
                    # don't try to load it
                    continue
                build_dir_or_ver = build_dir_or_dict[i]

            else:
                # Else use the same one for all cores, but try to construct the
                # appropriate variant of it for multicore cases
                build_dir_or_ver = build_dir_or_dict
                if len(target_subsys.cores) != 1:
                    if isinstance(build_dir_or_ver, str):
                        build_dir_or_ver = core.firmware_build_info_type.\
                            munge_build_dir(build_dir_or_ver)
        else:
            # Should just be one build specified, or they should be the same
            if isinstance(build_dir_or_dict, dict):
                if len(build_dir_or_dict) == 1:
                    build_dir_or_ver = build_dir_or_dict.values()[0]
                else:
                    if len(set(build_dir_or_dict.values())) > 1:
                        raise ValueError(
                            "Subsystem %s doesn't have per-core firmware, "
                            "but specified core build paths differ!" %
                            target_subsys.name)
                    build_dir_or_ver = build_dir_or_dict.values()[0]
            else:
                build_dir_or_ver = build_dir_or_dict

        multi_elf_dir = None
        if elf_dir:
            if isinstance(elf_dir, str_type):
                elf_build_dir = elf_dir
            else:
                elf_build_dir = elf_dir.get(i, None)
            if elf_build_dir:
                if os.path.isfile(elf_build_dir):
                    build_dir_or_ver = elf_build_dir
                else:
                    elf_files = glob.glob(os.path.join(elf_build_dir, "*.elf"))
                    if len(elf_files) > 1:
                        multi_elf_dir = elf_build_dir
                    else:
                        build_dir_or_ver = elf_build_dir

        load_xuv_not_elf = False
        build_info = None
        if isinstance(build_dir_or_ver, str):
            build_dir_or_ver = os.path.expanduser(build_dir_or_ver)
            build_dir_or_ver = os.path.abspath(build_dir_or_ver)
            build_dir_or_ver = os.path.normpath(build_dir_or_ver)
            if not os.path.exists(build_dir_or_ver):
                raise IOError("No such build dir %s" % build_dir_or_ver)

            if build_dir_or_ver.endswith(".xuv"):
                # If we've been passed an XUV instead of an ELF/build dir don't
                # attempt to set up the firmware objects.  Instead load the XUV
                # into the program space cache.  If this is really a live chip
                # then we'll just end up with pointless writes.
                load_xuv_not_elf = True

        if not load_xuv_not_elf:
            if core.firmware_type is core.default_firmware_type:
                # The core deliberately chooses not to expose full-fat firmware 
                # so skip this whole set-up. 
                continue
        
            try:
                if ((clone_from is None or not hasattr(clone_core.fw, "env")) and
                        (target_subsys.has_per_core_firmware or i == 0)):
                    build_info = core.firmware_build_info_type(
                        build_dir_or_ver,
                        core.info.layout_info,
                        interface_dir,
                        chip.name,
                        multi_elf_dir=multi_elf_dir,
                        cache_dir=cache_dir)
                    if patch_build_dir is not None:
                        from .multibuild_hydra_fw_build_info \
                        import HydraPatchedROMFirmwareBuildInfo
                        build_info = HydraPatchedROMFirmwareBuildInfo(
                            build_info,
                            patch_build_dir,
                            core.info.layout_info,
                            interface_dir,
                            chip.name,
                            core=core,
                            cache_dir=cache_dir)
                    else:
                        # Delay figuring out what type of build info we're creating
                        # until we actually need it, because even if we've been
                        # told what the build path is, it may be for a ROM, meaning
                        # we need to set up a combined ROM/patch build info object.
                        # Pass current value of core in the for loop lest it could
                        # change to different value by time of delayed call.
                        def firmware_build_info_creator(build_info, core):
                            from .multibuild_hydra_fw_build_info \
                            import HydraPatchedROMFirmwareBuildInfo
                            cur = core.subsystem.chip.curator_subsystem.core
                            # The idea here is that if Curator has a data source
                            # attached we can look up whether the target subsystem
                            # is running, and if Curator has no data source,
                            # we are in coredump mode so either core is dumped and
                            # we can call running_from_rom,
                            # or it isn't and this whole operation is pointless.
                            core_is_queryable = (
                                cur.has_data_source and
                                cur.is_subsystem_up(core.subsystem) or
                                not cur.has_data_source)
                            if core_is_queryable and core.running_from_rom:
                                # We're likely patched, but we'll have to detect the
                                # patch build ID ourselves later
                                return HydraPatchedROMFirmwareBuildInfo(
                                    build_info,
                                    core.get_patch_id,
                                    core.info.layout_info,
                                    interface_dir,
                                    chip.name,
                                    core=core,
                                    cache_dir=cache_dir)
                            return build_info
    
                        build_info = LazyProxy("%s build info" % subsystem,
                                               (firmware_build_info_creator,
                                                IGenericHydraFirmwareBuildInfo),
                                               [build_info, core], {}, [])
    
                else:
                    # Same firmware but it's running in different memory, so we
                    # share the build info but create separate environments from it.
                    if clone_from is None:
                        # This is the case where we're cloning P0 for P1's build
                        # as opposed to cloning a given core on device 0 for the
                        # same core on device N
                        clone_core = target_subsys.cores[0]
                    try:
                        clone_core_fw_env_setup = retrieve_lazy_proxy_setup(
                            clone_core.fw.env)
                        build_info = clone_core_fw_env_setup["cons_args"][0]
                    except TypeError:
                        # The clone core has a nonlazy environment
                        try:
                            build_info = clone_core.fw.env.build_info
                        except AttributeError:
                            build_info = None
                    except AttributeError:
                        # The clone core has no environment (e.g. there's no
                        # interal network access for looking up builds)
                        build_info = None

            except IFirmwareBuildInfo.FirmwareSetupException as exc:
                if not isinstance(exc, IFirmwareBuildInfo.LookupDisabledException):
                    iprint(exc)

        if build_info is not None:
            # Standalone environment for portable firmware abstraction
            #
            prog = core.program_space if load_program_space_cache else None

            if lazyproxy:
                fw_env = LazyProxy(
                    "%s firmware environment" % subsystem,
                    StandaloneFirmwareEnvironment,
                    [build_info, core, core.info.layout_info],
                    {"program_space":prog},
                    [AttributeError,
                     # build_info raises following
                     # if it can't find matching firmware
                     IFirmwareBuildInfo.NonBinutilsException],
                    hook_list=[])
            else:
                iprint("Creating %s firmware environment" % core.title)
                fw_env = StandaloneFirmwareEnvironment(
                    build_info, core,
                    core.info.layout_info,
                    program_space=prog)

            # Portable FW abstraction
            fw = core.firmware_type(fw_env, core, build_info=build_info)

            # Associate fw with respective core
            core.fw = fw
            
            # Now add further firmware layer abstractions as required.
            for attr_name, extra_firmware_type in core.extra_firmware_layers(plugins).items():
                core.add_firmware_layer(attr_name, FirmwareComponent.create_component_variant,
                                        (extra_firmware_type,), fw_env, core)

        elif load_xuv_not_elf:
            core.load_program_cache_from_xuv(build_dir_or_ver)


def create_simple_device_fw_environment(device, build_paths, 
                                        gbl_shell, chip_name=None):
    """
    Create firmware environments in all the cores that support it
    """
    cores = device.chip.cores
    for core in cores:
        try:
            core.firmware_build_info_type
            core.firmware_type
            core.default_firmware_type
        except AttributeError:
            pass # Core doesn't support firmware
        else:
            nickname = core.nicknames[0]
            try:
                build_path = build_paths[nickname]
            except KeyError:
                if core.default_firmware_type is not None:
                    core.fw = core.default_firmware_type(core)
                else:
                    core.fw = None
            else:
                create_simple_fw_environment(core, build_path, 
                                             gbl_shell, 
                                             chip_name=chip_name)


def create_simple_fw_environment(core, build_path, gbl_shell, chip_name=None):
    """
    Create a firmware environment from a single binary build path
    """
    try:
        build_info = core.firmware_build_info_type(build_path,
                                              core.info.layout_info,
                                              None, chip_name)
        fw_env = StandaloneFirmwareEnvironment(build_info, core,
                                               core.info.layout_info)
        core.fw = core.firmware_type(fw_env, core, build_info)
    except IFirmwareBuildInfo.FirmwareSetupException as exc:
        iprint("Creating default firmware environment: %s" % exc)
        core.fw = core.default_firmware_type(core)
