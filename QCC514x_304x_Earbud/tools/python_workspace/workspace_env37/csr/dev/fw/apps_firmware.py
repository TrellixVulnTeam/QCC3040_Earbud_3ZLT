############################################################################
############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.dev.fw.debug_log import DebugLogReader, \
    HydraLog, PerModuleHydraLog, PerModuleApps1Log, GlobalHydraLog, \
    ClassicHydraLog, PrimHydraLog,\
    Apps1LogDecoder, TrapHydraLog
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.fw.slt import AppsBaseSLT, RawSLT, AppsSLTNotImplemented,  AppsFakeSLT
from csr.dev.fw.firmware import BasicHydraFirmware,  DefaultFirmware, FirmwareVersionError,  FirmwareAttributesRequireEnvMeta
from csr.dev.fw.meta.i_firmware_build_info import BadElfSectionName
from csr.dev.hw.address_space import AddressSpace
from csr.dev.model import interface
from .appcmd import AppCmd
from .mib import MibApps
from .sched import SchedOxygen, SchedFreeRTOS, AppsP1SchedFreeRTOS
from csr.dev.fw.stack_unwinder import K32StackUnwinder
from csr.dev.fw.stack import AppsStack
from csr.dev.fw.ipc import IPC
from csr.dev.fw.feature_licensing import FeatureLicensing
from csr.dev.fw.psflash import Psflash
from csr.dev.fw.trap_message_queue import TrapMessageQueueSingletask, TrapMessageQueueMultitask
from csr.dev.fw.message_queue import MessageRouter
from .structs import IAppsStructHandler
from .call import Call
from csr.wheels.bitsandbobs import unique_subclass
from .trap_api.trap_utils import TrapUtils
from csr.dev.fw.trap_api.system_message import SystemMessage
from csr.dev.fw.pmalloc import AppsPmalloc
from csr.dev.model.base_component import BaseComponent
from csr.dev.env.env_helpers import var_address, var_typename, InvalidDereference
import re


class AppsDefaultFirmware(DefaultFirmware):
    """
    Default firmware class, containing anything that doesn't depend on the
    ELF
    """
    SLTtype = AppsBaseSLT

    @property
    def build_string(self):
        try:
            return self._core.dump_build_string
        except AttributeError:
            slt = self.reread_slt()
            return slt.build_id_string

    @property
    def build_number(self):
        try:
            return self._core.dump_build_id
        except AttributeError:
            slt = self.reread_slt()
            return slt.build_id_number
        
                

    def create_slt(self):

        # Current firmware uses baseline SLT.
        # Change this to specific SLT if/when SLT gets extended.
        try:
            return self.SLTtype.generate(self._core)
        except (AddressSpace.NoAccess, RawSLT.BadFingerprint) as e:
            # Create a fake SLT with only the build ID and string.
            if hasattr (self._core, "dump_build_id") and \
                hasattr(self._core, "dump_build_string"):
                return AppsFakeSLT(self._core.dump_build_id,
                    self._core.dump_build_string)
            else:
                if isinstance(e, AddressSpace.NoAccess):
                    return AppsSLTNotImplemented()
                else:
                    return None

    def reread_slt(self):
        '''
        Reread the SLT using a local copy of the SLT object.

        Re-read the Software Look-up Table taking into account that during
        reprogramming the SLT may appear to 'move' address range.

        The SLT should always be located at a fixed address. During
        reprogramming the SQIF OFFSET registers may be cleared making the SLT
        appear at a higher address. If the SLT is not found at the expected
        try an alternative location based on the software image header.
        '''
        try:
            local_slt = self.SLTtype(self._core)
        except (RawSLT.BadFingerprint, AddressSpace.ReadFailure):
            # Try reading at the offset given by the flash header in case
            # the SQIF offsets have been reset by a previous fw.load().
            local_slt = None
            if self.loader.detect_flash_boot_image():
                section_name = "apps_p1" if self._core.processor_number else "apps_p0"
                section = self.loader.flash_builder.image_header.get_section_details(section_name)
                offset = section["offset"] + self.loader.flash_builder.image_header.flash_offset
                # Assume that both cores use SQIF0 for now
                # This info will come from flash header later
                # NB On some chips this register is fixed to 0 and marked read
                # only
                if self._core.fields.APPS_SYS_SQIF1_PRESENT.is_writeable:
                    self._core.fields.APPS_SYS_SQIF1_PRESENT=0
                try:
                    local_slt = self.SLTtype(self._core, address_space_offset=offset)
                except RawSLT.BadFingerprint:
                    pass
        return local_slt


    def detect_encryption(self, encrypt):
        """
        If not specified by user, try to read the Curator eFuse information to determine whether
        the Apps SQIF should be encrypted/decrypted on this board (SECURITY_ENABLE=True)
        """

        # If the user has decided whether to encrypt, we don't need to read the eFuse
        if encrypt is not None:
            return encrypt

        # We can read the Curator eFuse to determine whether encryption is enabled.
        # We don't encrypt the SQIF if we cannot read the eFuse and report to the user.
        cur = self._core.subsystem.chip.curator_subsystem.core
        try:
            cur.fw.env.load = True
        except AttributeError:
            raise IOError("Couldn't read the eFuse to detect whether this board is encrypted. "
                          "Please re-run specifying whether encryption is enabled, or provide the "
                          "Curator firmware as a Pydbg argument.")

        # Read the eFuse to determine whether we need to encrypt the Apps SQIF.
        return cur.encryption_enabled

class AppsP1DefaultFirmware(AppsDefaultFirmware):
    
    def check_version_info(self, verbose=True):
        '''
        Check the Apps P1 elf trap version information against the loaded
        Apps P0 code.
        http://cognidox.europe.root.pri/vdocs/CS-328404-DD-LATEST.pdf
         section 7.2.1 describes the compatibility criteria:
        That major versions must match and the minor version of P1
        code must be less than or equal to the minor version of P0 code.
        '''
        apps0 = self._core.subsystem.cores[0]
        p0_slt = apps0.fw.reread_slt()
        if not p0_slt:
            raise FirmwareVersionError("Cannot read loaded Apps P0 "
               "version for compatibility check. This may be fixed by a reset")

        try:
            p0_trap_ver= p0_slt.trap_version
            p0_supported_trapsets = p0_slt.trapset_bitmap
        except RawSLT.BadFingerprint:
            raise FirmwareVersionError("Cannot read loaded Apps P0 "
               "version for compatibility check. This may be fixed by a reset")
        except KeyError:
            if verbose:
                iprint ("Loaded Processor 0 firmware does not have trap version"
                        " information")
            return
        try:
            # We are interested in the elf file we've been pointed at which
            # is unlikely to be the same as the one currently flashed on.
            elf_slt = self.build_info.elf_slt
            p1_trap_ver = elf_slt.trap_version
            p1_required_trapsets = elf_slt.trapset_bitmap
        except KeyError:
            raise FirmwareVersionError("Processor 1 firmware ELF does not "
                                       "contain trap version information so "
                                       "is incompatible with the loaded P0 "
                                       "firmware")
        if verbose:
            iprint("Loaded Processor 0 firmware has trap API v%s" % p0_trap_ver)
            iprint("Processor 1 firmware ELF has trap API    v%s" % p1_trap_ver)
        if (p1_trap_ver.major != p0_trap_ver.major or
            p1_trap_ver.minor > p0_trap_ver.minor):
            raise FirmwareVersionError("Firmware with trap API version %s "
                           "is not compatible with loaded Apps P0 "
                           "firmware with trap API version %s" %
                           (p1_trap_ver, p0_trap_ver))

# compatible with Python 2 and 3
FARE = FirmwareAttributesRequireEnvMeta('FARE', (BaseComponent,), {'__slots__': ()})

class AppsCommonFirmware (FARE):
    
    """
    Common parts of the Apps Firmware.  Used as a mixin to AppsP0Firmware and
    AppsP1Firmware
    """
    # This is defined in the firmware via the macro FILE_MEMMAPPED_ADDR
    ROFS_ADDRESS = 0x400000

    @property
    def stack(self):
        """
        Unwind the stack using the Kalimba 32 unwinder
        """
        return self._stack()

    def _stack(self, **kwargs):
        return K32StackUnwinder(self.env, self._core).bt(**kwargs)

    @property
    def stack_unused(self):
        return AppsStack(self.env, self._core).unused

    def _all_subcomponents(self):
        return {"sched" : "_sched",
                 "ipc" : "_ipc",
                "stack_model" : "_stack_model",
                "psflash" : "_psflash",
                "pmalloc"  : "_pmalloc"}

    def _generate_report_body_elements(self):
        return [self.irqs()]

    def mmu_handles(self):
        return self.appcmd.mmu_handles()

    # ------------------------------------------------------------------------
    # Extensions
    # ------------------------------------------------------------------------

    @property
    def appcmd(self):
        """\
        APPCMD interface
        """
        # Construct lazily...
        try:
            self._appcmd
        except AttributeError:
            self._appcmd = AppCmd(self.env,
                                  self._core,
                                  self._core.field_refs["INT_SW1_EVENT"])

        return self._appcmd

    @property
    def mib(self):
        """\
        MIB set, get, dump.
        """
        # Construct lazily...
        try:
            self._mib
        except AttributeError:
            self._mib = FirmwareComponent.create_component_variant((MibApps,),
                                                              self.env,
                                                              self._core,
                                                              parent = self)

        return self._mib

    @property
    def ipc(self):
        try:
            self._ipc
        except AttributeError:
            self._ipc = IPC.create(self.env, self._core)
        return self._ipc

    @property
    def sched(self):
        try:
            self._sched
        except AttributeError:
            self._sched = FirmwareComponent.create_component_variant((SchedOxygen,
                                                                      SchedFreeRTOS),
                                                                     self.env,
                                                                     self._core,
                                                                     parent = self)
        return self._sched

    @property
    def stack_model(self):
        try:
            self._stack_model
        except AttributeError:
            self._stack_model = AppsStack(self.env, self._core)
        return self._stack_model

    @property
    def call(self):
        try:
            self._call
        except AttributeError:
            self._call = unique_subclass(Call)(self.env, self._core, self.appcmd)
        return self._call

    def start_trace(self, filename=None, type="stepping"):
        '''
        Kick off a pc_trace in a sub-process.

        Potential extension: Add this as a generic part of the Firmware class interface once
        there's more than one implementation of the stepping agent.
        '''
        try:
            import csr.tools
        except ImportError:
            raise NotImplementedError("csr.tools package not available: can't "
                                      "run PC harvesting trace")
        from csr.tools.pc_trace.pc_harvesting_agent import AppsPCSteppingAgent, \
                                                       AppsPCSamplingAgent
        if type == "stepping":
            self._pc_tracer = AppsPCSteppingAgent(self._core)
        elif type == "sampling":
            self._pc_tracer = AppsPCSamplingAgent(self._core)
        else:
            raise ValueError("start_trace: type must either be 'stepping' "
                             "or 'sampling'!")
        if filename is not None:
            self._pc_tracer.reset_filename(filename)

        self._pc_tracer.start()

    def stop_trace(self):
        '''
        Stop a trace that has previously been kicked off
        '''
        try:
            self._pc_tracer
        except AttributeError:
            iprint("WARNING: you can't stop a PC trace you haven't started")
            return

        self._pc_tracer.stop()

    @property
    def elf_slt(self):
        try:
            self._elf_slt
        except AttributeError:
            self._elf_slt = AppsBaseSLT(self.env.fw.build_info.elf_code)
        return self._elf_slt

    def _create_debug_log(self):
        # Potential extension:: autodetect variants
        try:
            return FirmwareComponent.create_component_variant((GlobalHydraLog,
                                                               PerModuleHydraLog,
                                                               ClassicHydraLog),
                                                              self.env,
                                                              self._core,
                                                              parent = self)
        except FirmwareComponent.NotDetected:
            #If we don't have enough info to look at structures, we can't
            #support log levels
            return HydraLog(self.env, self._core, self)

    def _create_debug_log_decoder(self):
        # Potential extension:: Specialise to expect local timestamp
        return AppsLogDecoder(self)


    @staticmethod
    def _struct_handler_type():
        return IAppsStructHandler

    def irqs(self):
        try:
            text = ""
            last_element = self.env.globalvars["irq_counts"].num_elements - 1
            for i,x in enumerate(self.env.globalvars["irq_counts"]):
                if i == last_element:
                    #The last item gets its own enum and INT_SOURCE_LAST_POSN
                    #so we special case it
                    names = self.env.enums["int_source_enum"][i]
                    names.remove("INT_SOURCE_LAST")
                    name = names[0]
                else:
                    name = self.env.enums["int_source_enum"][i]
                count = x.value_string
                text += (count + ":" + name + "\n")
        except KeyError:
            iprint("Rebuild firmware with LOG_IRQS defined")

        grp = interface.Group("IRQ counts")
        grp.append(interface.Code(text))
        return grp

    def _generate_memory_report_component(self):
        dm_whole   = self._core.sym_get_range(self._ram_name, "_LOWER", "_UPPER")
        dm_bss     = self._core.sym_get_range("MEM_MAP_BSS")
        dm_initc   = self._core.sym_get_range("MEM_MAP_INITC")
        return [dm_whole, [dm_bss, dm_initc]]

    @property
    def prim_log(self):
        try:
            self._prim_log
        except AttributeError:
            self._prim_log = PrimHydraLog(self.env, self._core, self)
        return self._prim_log

    @property
    def psflash(self):
        try:
            self._psflash
        except AttributeError:
            self._psflash = Psflash(self.env, self._core)
        return self._psflash

    @property
    def pmalloc(self):
        # Construct lazily...
        try:
            self._pmalloc
        except AttributeError:
            self._pmalloc = AppsPmalloc(self.env, self._core)

        return self._pmalloc


# AppsCommonFirmware needs to be first in the list of parent classes because
# its implementations of certain functions need precede the pure virtual
# implementations that GenericHydraFirmware inherits
class AppsP1Firmware(AppsCommonFirmware, BasicHydraFirmware,
                     AppsP1DefaultFirmware):
    """
    Apps P1-specific extensions, such as trap API support modules
    """

    def __init__(self, fw_env, core, fw_build_id=None,
                 fw_build_str=None, build_info=None):

        BasicHydraFirmware.__init__(self, fw_env, core,
                                    build_info=build_info)

        self._ram_name = "PxD_P1_DM_RAM"
        
    @property
    def trap_utils(self):
        try:
            self._trap_utils
        except AttributeError:
            self._trap_utils = TrapUtils(self.env, self._core)
        return self._trap_utils

    def _all_subcomponents(self):
        cmps = {"trap_utils" : "_trap_utils",
                "trap_message_queue" : "_trap_message_queue",
                "msg_router" : "_msg_router"}
        cmps.update(AppsCommonFirmware._all_subcomponents(self))
        cmps.update(BasicHydraFirmware._all_subcomponents(self))
        return cmps

    @property
    def feature_licensing(self):
        try:
            self._feature_licensing
        except AttributeError:
            self._feature_licensing = FeatureLicensing(self.env, self._core)
        return self._feature_licensing

    @property
    def sched(self):
        try:
            self._sched
        except AttributeError:
            self._sched = FirmwareComponent.create_component_variant((SchedOxygen,
                                                                      AppsP1SchedFreeRTOS),
                                                                     self.env,
                                                                     self._core,
                                                                     parent = self)
        return self._sched

    @property
    def trap_log(self):
        try:
            self._trap_log
        except AttributeError:
            self._trap_log = TrapHydraLog(self.env, self._core, self)
        return self._trap_log

    @property
    def trap_message_queue(self):
        try:
            self._trap_message_queue
        except AttributeError:
            self._trap_message_queue = FirmwareComponent.create_component_variant(
                                        (TrapMessageQueueSingletask, TrapMessageQueueMultitask),
                                        self.env,
                                        self._core,
                                        parent = self)
        return self._trap_message_queue

    class MsgEnums(FirmwareComponent):
        """
        Helper class that can provide info about the mappings of message IDs,
        based on the firmware using special macros to link dummy variables of
        the message ID enum types into a special section.
        """
        def __init__(self, fw_env, core, parent=None):
            self._fw_env = fw_env
            try:
                self._fw_env.build_info.get_elf_section_bounds("msg_enums")
            except BadElfSectionName:
                raise FirmwareComponent.NotDetected
            

        def _get_enum_set(self):
            msg_enum_sec_bounds = self._fw_env.build_info.get_elf_section_bounds("msg_enums")
            self._msg_enums = {}
            self._msg_enum_value_sets = {}
            lower, upper = msg_enum_sec_bounds
            for var in self._fw_env.vars.values():
                if lower <= var_address(var) < upper:
                    msg_enum_name = var_typename(var)
                    msg_enum = self._fw_env.enums[msg_enum_name]
                    self._msg_enums[msg_enum_name] = msg_enum
                    self._msg_enum_value_sets[msg_enum_name] = set(msg_enum.values())
                    
            # Now add SystemMessage contents as plain dictionary, to mimic an Enum object
            # The name doesn't matter very much because we know that these IDs are
            # unique, so we don't need the log message to give us the typename hint
            system_msgs = {value:attr for attr, value in SystemMessage.__dict__.items()
                              if re.match(r"^[A-Z0-9_]+[A-Z0-9]$", attr)}
            self._msg_enums["system_messages"] = system_msgs
            self._msg_enum_value_sets["system_messages"] = set(system_msgs)
        
        def name_from_id(self, msg_id, enum_name=None, no_exceptions=False):
            try:
                self._msg_enums
            except AttributeError:
                self._get_enum_set()
            matching_enums = []
            if enum_name is not None:
                matching_enums = [me_name for me_name in self._msg_enums 
                                    if me_name in (enum_name, "enum "+enum_name) and 
                                        msg_id in self._msg_enum_value_sets[me_name]]
            if not matching_enums:
                # Either no enum hint was given or it was invalid
                matching_enums = [me_name for (me_name, me_value_set) in self._msg_enum_value_sets.items() 
                                  if msg_id in me_value_set]
            if len(matching_enums) == 1:
                return self._msg_enums[matching_enums[0]][msg_id]
            
            if no_exceptions:
                return None
            
            if not matching_enums:
                raise ValueError("Message ID '{}' doens't appear in any known message ID enum".format(msg_id))
            raise ValueError("Message ID '{}' appears in multiple message ID enums ({})".format(msg_id, ", ".join(matching_enums)))

    @property
    def msg_router(self):
        """
        Message Queue pointer for Free RTOS
        """
        try:
            self._msg_router
        except AttributeError:
            self._msg_router = FirmwareComponent.create_component_variant(
                                                    (MessageRouter,),
                                                    self.env, self._core, self)
        return self._msg_router

    @property
    def msg_enums(self):
        try:
            self._msg_enums
        except AttributeError:
            self._msg_enums = FirmwareComponent.create_component_variant((self.MsgEnums,),
                                                                         self.env, self._core, self)
        return self._msg_enums
                

    def _create_debug_log(self):
        try:
            log = PerModuleApps1Log(self.env, self._core, parent = self)
        except FirmwareComponent.NotDetected:
            log = AppsCommonFirmware._create_debug_log(self)
        return log

    def _create_debug_log_decoder(self):
        return Apps1LogDecoder(self)
