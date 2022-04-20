############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.wheels import PureVirtualError, display_hex, NameSpace
from csr.dev.model.base_component import BaseComponent
from .meta.i_io_map_info import FieldValueDict, FieldRefDict, BitfieldValueDict,\
FieldArrayRefDict
from ..register_field.register_field import RegisterField, bitz_engine
from csr.dev.model import interface
from csr.wheels.bitsandbobs import unique_subclass, StaticNameSpaceDict
from csr.dev.model.interface import Group, Table
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.hw.address_space import AddressSpace, AddressSlavePort, NullAccessCache,\
AddressMap
from csr.dev.hw.core.meta.i_layout_info import XapDataInfo
from .mixin.is_in_hydra import IsInHydra
import sys
import re

class CoreAttributeNameClash(RuntimeError):
    """
    Attempt to add an additional attribute to a core instance, as defined by
    extra_firmware_layers, results in a name clash.
    """

if sys.version_info >= (3, 0):
    int_type = int
else:
    int_type = (int, long)

class MemW(object):
    """
    Facade that presents data space as word-wide, rather than the normal
    byte-wide access
    """
    def __init__(self, byte_data, layout_info):
        self._byte_data = byte_data
        self._serialise = layout_info.from_words
        self._deserialise = layout_info.to_words
        self._word_width = (layout_info.data_word_bits //
                                layout_info.addr_unit_bits)
        self._align_mask = self._word_width - 1
        
    @display_hex
    def __getitem__(self, addr):
        """
        Read either a single word or a sequence of words from memory.  The
        requested address/address slice must be word-aligned.
        """
        try:
            addr = int(addr)
        except TypeError:
            # must be a slice
            pass
        else:
            if addr & self._align_mask != 0:
                raise ValueError(
                    "Can only read %d-byte aligned addresses"
                    " (address is 0x%08x)" %
                    (self._word_width, addr))
            return self._deserialise(self._byte_data[addr:addr+self._word_width])[0]
        # slice
        if addr.start is not None and addr.start & self._align_mask != 0:
            raise ValueError(
                "Can only read %d-byte aligned addresses"
                " (slice start address is 0x%08x)" %
                (self._word_width, addr.start))
        if addr.stop is not None and addr.stop & self._align_mask != 0:
            raise ValueError(
                "Can only read %d-byte aligned addresses"
                " (slice stop address is 0x%08x)" %
                (self._word_width, addr.stop))
        if addr.step is not None:
            raise ValueError("Can't read strided address ranges")
        return self._deserialise(self._byte_data[addr])

    def __setitem__(self, addr, values):
        """
        Write the supplied word-wide value(s) to memory.  If a single address
        is supplied, "values" must be an integer.  If addr is a slice it
        must be an iterable of the appropriate size.
        """
        try:
            addr = int(addr)
        except TypeError:
            # must be a slice
            if addr.start is not None and addr.start & self._align_mask != 0:
                raise ValueError("Can only write %d-byte aligned addresses" % self._word_width)
            if addr.stop is not None and addr.stop & self._align_mask != 0:
                raise ValueError("Can only write %d-byte aligned addresses" % self._word_width)
            if addr.step is not None:
                raise ValueError("Can't write strided address ranges")
            self._byte_data[addr] = self._serialise(values)
        else:
            if addr & self._align_mask != 0:
                raise ValueError("Can only write %d-byte aligned addresses" %
                                 self._word_width)
            if not isinstance(values, int_type):
                raise TypeError("Can only write integer to single address")
            self._byte_data[addr:addr+self._word_width] = self._serialise(
                [values])

    def __len__(self):
        return len(self._byte_data)//self._word_width




class BaseCore (BaseComponent):
    """\
    CPU-centric collection of resources (hw + fw proxies) (Abstract Base)
    
    Cores are the focus of most attention - but do not represent all of
    a Chip's resources (e.g. shared memory, host blocks...)
    """
    
    def __init__(self):
        
        self._fw = None
        self._sym = None
        self._sticky_brk_pts = {}
        self._subcmpts = {"fw" : "_fw"}
        self._reset_hooks = []

    # BaseComponent compliance
    
    @property
    def title(self):
        try:
            return self.nicknames[0]
        except IndexError:
            return 'cpu core'

    def _all_subcomponents(self):
        return self._subcmpts

    # Extensions
    @property
    def nicknames(self):
        """
        Iterable of names by which this core will be installed in the global 
        namespace
        """
        return []

    @property
    def core_commands(self):
        """\
        Mapping of traditional core-relative short command names to member
        functions.

        Used by command shell when registering shorthand commands in the global
        dictionary when this core is set as 'focus'.
        
        Typically used to collect together commands that implement xap2emu like
        functions.
        
        N.B. So that they can be reused by html report generators and other
        interfaces the underlying commands now return a structured result (c.f
        print).
        """         
        return {}, []
    
            
    def _get_fw(self): 
        return self._fw
        
    def _set_fw(self, fw): 
        self._fw = fw
           
    fw = property(_get_fw, _set_fw)

    def _get_sym(self): 
        return self._sym
        
    def _set_sym(self, sym): 
        self._sym = sym
           
    sym_ = property(_get_sym, _set_sym)

    def bitz(self, reg_name, report=False, desc_width=None, value=None):
        reg = self.field_refs[reg_name]
        
        return bitz_engine(reg, report=report, desc_width=desc_width,
                           value=value)

    @property
    def info(self):
        """\
        Access Core Meta-data interface (ICoreInfo).
        """
        return self._info 

    @property
    def bad_read_reg_name(self):
        """
        Name of a register containing the value to return in lieu of a timed-out
        register read access.  By default there is no such register.
        """
        return None

    @property
    def lpc_master(self):
        """\
        LPC Master Component
        
        So far all cores have one of these...
        """
        try:
            self.__lpc_master
        except AttributeError:
            self.__lpc_master = self._create_lpc_master()
        return self.__lpc_master 

    @property
    def pc(self):
        '''
        Access to the core's program counter
        '''
        raise PureVirtualError

    @property
    def data(self):
        """\
        Data Address space (as seen by processor)
        (same as prog if non-harvard)
        """
        raise PureVirtualError()
    
    @property
    def dataw(self):
        """
        Data-word-wide access to data memory
        """
        try:
            self._dataw
        except AttributeError:
            self._dataw = MemW(self.data, self._info.layout_info)
        return self._dataw
    
    @property
    def progw(self):
        """
        Data-word-wide access to program memory
        """
        try:
            self._progw
        except AttributeError:
            self._progw = MemW(self.program_space, self._info.layout_info)
        return self._progw

    @property
    def program_space(self):
        """\
        Program Address space (as seen by processor)
        (same as data if non-harvard)
        """
        raise PureVirtualError()

    @property
    def register_space(self):
        """\
        Hardware register-containing AddressSpace for this core.
        
        By default RegisterFields will refer to this space.
        
        Although it could always be aliased to the proc data space there are
        cases there is a more efficient/more resolved space it can refer to.
        """
        raise PureVirtualError()

    @property
    def fields(self):
        """\
        Access to register field values by symbolic field name.
        
        Use for one-off accesses.
        
        Example:         
            self.fields["DBG_EMU_CMD_XAP_RUN_B"] = 1                       
        """
        try:
            self.__fields
        except AttributeError:
            self.__fields = unique_subclass(FieldValueDict)(self.field_refs,
                                                            self.field_array_refs)
        return self.__fields 

    @property
    def bitfields(self):
        try:
            self.__bitfields
        except AttributeError:
            self.__bitfields = unique_subclass(BitfieldValueDict)(self.field_refs,
                                                                  self.field_array_refs)
        return self.__bitfields

    @property
    def iodefs(self):
        """\
        Access to register enum values by symbolic name.

        Use for one-off accesses.

        Example:
            self.iodefs.FREQ_COUNT_STATE_IDLE
        """
        try:
            self.__iodefs
        except AttributeError:
            self.__iodefs = unique_subclass(StaticNameSpaceDict)(self.info.io_map_info.misc_io_values,
                                                strip_leading=r"$")
        return self.__iodefs

    @property
    def regs(self):
        """
        Alias for fields
        """
        return self.fields

    @property
    def field_refs(self):
        """\
        Dictionary of references to register fields by symbolic name.
        
        Use for repeated accesses or passing around.        
        
        Example:
         
            run_b = self.field_refs["DBG_EMU_CMD_XAP_RUN_B"]
            save = run_b.read()
            run_b.write(1)
            ...
            run_b.write(save)
        """            
        try:
            self.__field_refs
        except AttributeError:
            self.__field_refs = FieldRefDict(self.info.io_map_info, 
                                             self, self.bad_read_reg_name) 
        return self.__field_refs
    
    @property
    def field_array_refs(self):
        try:
            self._field_array_refs
        except AttributeError:
            self._field_array_refs = FieldArrayRefDict(self._info.io_map_info,
                                                       self, self.bad_read_reg_name)
        return self._field_array_refs

    @property
    def reg_refs(self):
        """
        Alias for field_refs
        """
        return self.field_refs

    @property
    def reg_array_refs(self):
        """
        Alias for field_array_refs
        """
        return self.field_array_refs

    def reg_dump(self, csv_path=None, filter=None, ignore_read_errors=None):
        """
        Alias for self.fields.dump() with csv_path handling in case if it empty
        """
        if csv_path is None:
            csv_path = "%s_reg_dump.csv" % self.title.lower()

        return self.fields.dump(csv_path=csv_path,
                                filter=filter, ignore_read_errors=ignore_read_errors)

    def reg_restore(self, csv_path=None, filter=None):
        """
        Alias for self.fields.restore() with csv_path handling in case if it empty
        """
        if csv_path is None:
            csv_path = "%s_reg_dump.csv" % self.title.lower()

        self.fields.restore(csv_path=csv_path,
                            filter=filter)

    @property
    def misc_io_values(self):
        """\
        Access dictionary of miscellaneous io map values.
        
        This should only be used to lookup symbolic io values that do not have
        anywhere better to live (e.g. field enums should be accessed via field
        object)

        Expect symbols to be removed from this set once a better home is 
        available.        
        """
        return self.info.io_map_info.misc_io_values
    
    def load_register_defaults(self):
        """
        Load default register values (normally into PassiveAccessCaches in the
        addressing model for use in "sim" mode)
        """
        from csr.dev.hw.register_field.meta.io_struct_register_field_info \
            import IoStructRegisterFieldInfo

        if self.info.io_map_info.field_records:
            self.logger.info("Loading register defaults for '%s'" % self.title)
            try:
                self.fields.set_defaults()
            except IoStructRegisterFieldInfo.ResetAttributeError:
                self.logger.warning('No register reset values are available.')
    
    def map_lpc_slave_regs_into_prog_space(self):
        """\
        Map the LPC slave register address range into this subsystem's program 
        space so they can be accessed from the chip (e.g. to check magic 
        number).
        
        Exactly how and where the Slave registers will map to is subsystem 
        dependent.
        
        N.B. This sets up the NVMEM mapping hardware but does not configure 
        the LPC link (master or slave).
        
        Assumes subsystem is halted.
        
        Potential extension:: Return RAII mapping token.
         
        ALT: This is a rather specialist operation: it could be implemented 
        by a visitor rather than burdening the main class.
        """
        raise PureVirtualError()

    def load_fw_env(self, build_dir, interface_dir=None, build_info=None,
                    lazy=True):
        """
        Load a given firmware ELF either lazily or immediately
        """
        from csr.dev.fw.firmware import Firmware
        if isinstance(self.fw, Firmware):
            # Don't load a new fw env on top of an existing one as it will 
            # create stale references and leak memory
            iprint ("Not loading firmware environment for %s %s: already loaded" 
                   % (self.subsystem.name, self.title))
            return
        
        if build_info is None:
            build_info = self.firmware_build_info_type(build_dir,
                                                       self._info.layout_info, 
                                                       interface_dir,
                                                       self.subsystem.chip.name)
    
        # Standalone environment for portable firmware abstraction
        #
        from csr.wheels.bitsandbobs import LazyProxy
        from csr.dev.env.standalone_env import StandaloneFirmwareEnvironment
        if lazy:
            fw_env = LazyProxy("%s %s firmware environment" % (self.subsystem.name, 
                                                               self.title),
                               StandaloneFirmwareEnvironment,
                               [build_info, self, self._info.layout_info],
                               {}, hook_list=[])
        else:
            fw_env = StandaloneFirmwareEnvironment(build_info, self, 
                                                   self._info.layout_info)
    
        # Portable FW abstraction
        #
        fw = self.firmware_type(fw_env, self)

        # Associate fw with this core
        self.fw = fw
        
    def extra_firmware_layers(self, plugins):
        """
        Return a dictionary mapping attribute names to classes for additional
        fw-like attributes of the core, which may (or may not) be derived from
        the provided plugins information.
        """
        return {}
    
    def add_firmware_layer(self, name, constructor, *args, **kwargs):
        """
        Add an object representing an additional firmware layer as an attribute
        of the object and as a subcomponent.
        
        The object is added as a property so that we don't trigger construction,
        which typically involves looking up the firmware environment to see 
        whether the layer appears to be present, until required.
        """
        instance_name = "_"+name
        if hasattr(self.__class__, name) or hasattr(self, instance_name):
            raise CoreAttributeNameClash("Extra firmware layer '{}' "
                                         "clashes with existing attribute".format(name))

        def getter(inst):
            try:
                getattr(inst, instance_name)
            except AttributeError:
                setattr(inst,instance_name, constructor(*args, **kwargs))
            return getattr(inst, instance_name) 
        
        setattr(self.__class__, name, property(getter))
        self._subcmpts.update({name : instance_name})

    def find_matching_symbols(self, sym_regex, types=False, vars=True, 
                              funcs=False, enum_fields=False, regs=True, 
                              bitfields=True, iodefs=False, all=False, case=True):
        """
        Search the firmware and register data for symbols of specified kinds 
        whose names match the supplied regular expression.  Returns matches,
        grouped by category in a multi-level dictionary.
        
        @param sym_regex Regular expression (compiled or raw string).  Matches
        are detected using the function re.match, which means that matching is 
        done from the beginning of the string.
        @param types Search the types (there are no separate namespaces for
        enum_fields, structs and unions)
        @param vars Search the variables (globals and statics, reported 
        separately and with compilation unit details for the latter)
        @param funcs Search the functions
        @param enum_fields Search the fields of enumerations (the names of
        enumerations themselves are covered by the search through types)
        @param regs Search the register names
        @param bitfields Search the bitfield names
        @param iodefs If True, search the iodefs
        @param all Select all the types of symbol, regardless of the individual
        selection arguments
        @param case If False the regular expression is converted to a case-
        insensitive version, otherwise it is left alone 
        """
        
        if all:
            types, vars, funcs, enum_fields, regs, bitfields, iodefs = [True] * 7
            
        if not case:
            # Convert back to a raw string (if necessary) and recompile with
            # re.IGNORECASE
            try:
                sym_regex = sym_regex.pattern
            except AttributeError:
                pass
            sym_regex = re.compile(sym_regex,re.IGNORECASE)
        else:
            # if already compiled this is harmless
            sym_regex = re.compile(sym_regex)
        res = {}
        if hasattr(self.fw, "env"):
            res.update(self.fw.env.find_matching_symbols(sym_regex, types=types, 
                                                         vars=vars, funcs=funcs, 
                                                         enum_fields=enum_fields))
        if regs:
            res["regs"] = [reg for reg in self.field_refs.keys() 
                                                      if sym_regex.match(reg)]
        if bitfields:
            res["bitfields"] = []
            for reg in self.field_refs.keys():
                res["bitfields"] += [(reg,f) for f in dir(self.field_refs[reg]) 
                                     if sym_regex.match(f) and 
                                     isinstance(getattr(self.field_refs[reg],f),
                                                RegisterField)]
        if iodefs:
            res["iodefs"] = [iodef for iodef in dir(self.iodefs)
                                 if sym_regex.match(iodef) and not iodef.startswith('__')]
        
        return res

    def symbol_search(self, sym_regex, types=False, vars=True, funcs=False, 
                      enum_fields=True, regs=True, bitfields=True, iodefs=False,
                      all=False, case=True, report=False):
        """
        Search the firmware and register data for symbols of specified kinds 
        whose names match the supplied regular expression.  Returns matches,
        grouped by category, plus relevant API calls to access those symbols
        in the Pydbg core API.
        
        Note: this method does a manual reproduction of the core's API.  If
        any of the API for looking up types, variables, functions,
        enumerations, registers or bitfields changes, this function also needs
        to change!
        
        @param sym_regex Regular expression (compiled or raw string).  Matches
        are detected using the function re.match, which means that matching is 
        done from the beginning of the string.
        @param types Search the types (there are no separate namespaces for
        enum_fields, structs and unions)
        @param vars Search the variables (globals and statics, reported 
        separately and with compilation unit details for the latter)
        @param funcs Search the functions
        @param enum_fields Search the *fields* of enumerations (the names of
        enumerations themselves are covered by the search through types)
        @param regs Search the register names
        @param bitfields Search the bitfield names (only where these are unique)
        @param iodefs If True, search the iodefs
        @param all Select all the types of symbol, regardless of the individual
        selection arguments
        @param case If False the regular expression is converted to a case-
        insensitive version, otherwise it is left alone.
        @param report If True, return a report element, else write the report
        out via a plain-text adaptor to stdout.
        """
        res = self.find_matching_symbols(sym_regex, types=types, vars=vars, 
                                         funcs=funcs, enum_fields=enum_fields, 
                                         regs=regs, bitfields=bitfields, iodefs=iodefs,
                                         all=all, case=case)
                    
        # Now format the output.  Use a Table or freestyle it?
        output = interface.Table(("Type", "Name", "API call"))
        nn = self.nicknames[0]
        if "vars" in res and res["vars"]:
            cus = res["vars"].keys()
            if cus:
                output.add_row(("Vars","",""))
                for cu in sorted(cus):
                    for v in sorted(res["vars"][cu]):
                        try:
                            self.fw.env.vars[v]
                            output.add_row(("","%s: %s" % (cu,v),
                                            ".".join([nn, "fw", "env", 'vars["%s"]' % v])))
                        except KeyError:
                            output.add_row(("","%s: %s" % (cu,v),
                                            ".".join([nn,"fw","env",'cus["%s"]'%cu,
                                                  'vars["%s"]'%v])))
        if "regs" in res and res["regs"]:
            output.add_row(("Registers","",""))
            for r in sorted(res["regs"]):
                output.add_row(("",r,".".join([nn,"regs",r])))
        if "bitfields" in res and res["bitfields"]:
            output.add_row(("Bitfields","",""))
            for reg,bf in sorted(res["bitfields"]):
                api = (".".join([nn,"bitfields",bf]) 
                       if hasattr(self.bitfields,bf) 
                       else ".".join([nn,"regs",reg,bf]))
                output.add_row(("",".".join([reg,bf]), api))
        if "enum_fields" in res and res["enum_fields"]:
            output.add_row(("Enum fields","",""))
            for enum_type, enum_field in sorted(res["enum_fields"]):
                output.add_row(("",".".join([enum_type, enum_field]),
                               ".".join([nn,"fw","env","enums"])+
                               '["%s"]'%enum_type+'["%s"]'%enum_field))
                        
        if "types" in res and res["types"]:
            output.add_row(("Types","",""))
            for t in sorted(res["types"]):
                output.add_row(("",t,
                                ".".join([nn,"fw","env",'types["%s"]' % t])))
                
        if "funcs" in res and res["funcs"]:
            output.add_row(("Functions","",""))
            for cu, cufuncs in res["funcs"].items():
                for func in sorted(cufuncs):
                    output.add_row(("",func, 
                                    ".".join([nn,"fw","env",'functions["%s"]' % func])))

        if "iodefs" in res:
            output.add_row(("Iodefs", "", ""))
            for iodef in sorted(res["iodefs"]):
                output.add_row(("", iodef, ".".join([nn, "iodefs", iodef])))

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

    @property
    def has_data_source(self):
        try:
            return self._has_data_source
        except AttributeError:
            return False
    
    @has_data_source.setter
    def has_data_source(self, has):
        self._has_data_source = has


    # Protected/Required

    @property
    def _info(self):
        """\
        Access Core Meta-data (CoreInfo).
        """
        raise PureVirtualError()
    
    # Protected/Overridable
    
    def _create_lpc_master(self):
        """\
        Create Data Address Space Proxy for this Core.
        """
        # So far all those encountered are the same...
        from ..lpc_master import LPCMaster        
        return LPCMaster(self) 

    def sym_get_value(self, symbol):
        """\
        Returns the value of the symbol passed. Looks in several different possible locations.
        """
        try:
            env = self.env
        except AttributeError:
            env = self.fw.env

        try:
            value = env.abs[symbol]
        except KeyError:
            try:
                value = env.gv[symbol].address
            except KeyError:
                value = self.info.io_map_info.misc_io_values["$"+symbol+"_ENUM"]

        return value

    def sym_get_range(self, symbol, start_tag="_START", end_tag="_END"):
        """\
        Returns the name, start address, end address and size of the symbol passed.
        """
        start_sym = symbol + start_tag
        end_sym = symbol + end_tag
        start_addr = self.sym_get_value(start_sym)
        end_addr = self.sym_get_value(end_sym)
        size = end_addr - start_addr
        range = {"name":symbol, "start":start_addr, "end":end_addr, "size":size}
        return range

    @property
    def running_from_rom(self):
        try:
            return self._is_running_from_rom
        except AddressSpace.ReadFailure as e:
            raise RuntimeError("Error checking if %s is running from ROM: %s" % (self.title, e))

    @property
    def _is_running_from_rom(self):
        """
        Concrete cores should override this to determine whether they are running
        from ROM.  It's assumed this is based on a register read so will fail
        with an AddressSpace.ReadFailure if the subsystem is off.
        """
        raise NotImplementedError("No running-from-ROM check implemented for %s" % self.title)
    
    def get_patch_id(self, env=None):
        """
        Patch ID retrieval method.  If env is supplied, use that; otherwise 
        look for the env attached to this core.
        
        This default method does nothing and returns None
        """
        return None

    @property
    def patch_type(self):
        """
        The type of firmware patch, by default none meaning, not patchable.
        Firmware classes need to derive from IsPatchableMixin and the core class
        needs to override this property to support a patch type.
        """
        return None

    def add_reset_hook(self, reset_hook):
        self._reset_hooks.append(reset_hook)

    def _on_reset(self):
        for hook in self._reset_hooks:
            hook(self)

class GenericCore(BaseCore):
    
    def __init__(self, name, data_space_size, layout_info, running_from_rom=True):

        self._name = name
        self._layout_info = layout_info
        self._running_from_rom = running_from_rom

        self._create_memory_map(data_space_size)
        
    def _create_memory_map(self, data_space_size):
        self._data_space = AddressSlavePort("%s data space" % self._name, 
                                            length=data_space_size,
                                            cache_type=NullAccessCache,
                                            layout_info=self._layout_info)
        self._data = AddressMap("%s data" % self._name,length=data_space_size,
                                            cache_type=NullAccessCache,
                                            layout_info=self._layout_info)
        self._data.add_mapping(0, data_space_size, self._data_space)

    @property
    def nicknames(self):
        return [self._name]
    
    @property
    def data(self):
        return self._data.port

    @property
    def program_space(self):
        return self.data
    
    @property
    def _info(self):
        try:
            self._info_
        except AttributeError:
            self._info_ = NameSpace()
            self._info_.layout_info = self._layout_info
        return self._info_

    def is_clocked(self):
        return True
    
    @property
    def _is_running_from_rom(self):
        return self._running_from_rom
    
    def load_register_defaults(self):
        pass
    
class SimpleHydraCore(GenericCore, IsInHydra):
    
    def __init__(self, name, data_space_size, layout_info, subsystem):
        GenericCore.__init__(self, name, data_space_size, layout_info)
        self._subsystem = subsystem

    @property
    def core_commands(self):
        return {"fw_ver" : "self.fw.slt.fw_ver"}, []
    

class SimpleHydraXAPCore(SimpleHydraCore):
    
    def __init__(self, name, subsystem, include_gw2=True):
        self._include_gw2 = include_gw2
        SimpleHydraCore.__init__(self, name, 1<<16, XapDataInfo(), subsystem)
        
    def _create_memory_map(self, data_space_size):
        self._data_space = AddressSlavePort("%s data space" % self._name, 
                                            length=data_space_size,
                                            cache_type=NullAccessCache,
                                            layout_info=self._layout_info)
        self._data = AddressMap("%s data" % self._name,length=data_space_size,
                                            cache_type=NullAccessCache,
                                            layout_info=self._layout_info)
        self._data.add_mapping(0, 0x80, self._data_space)
        self._data.add_mapping(0x8000,0x10000, self._data_space)
        
        self._gw1 = AddressSlavePort("GW1", length=0x2000,
                                     cache_type=NullAccessCache,
                                     layout_info=self._layout_info)
        self._data.add_mapping(0x80, 0x2000, self._gw1)
        if self._include_gw2:
            self._gw2 = AddressSlavePort("GW1", length=0x2000,
                                         cache_type=NullAccessCache,
                                         layout_info=self._layout_info)
            self._data.add_mapping(0x2000, 0x4000, self._gw2)


class HasCoreInfo(object):
    """
    Mixin providing the info property based on parametrised CoreInfoType when
    there is no custom digits support
    """
    @property
    def _info(self):

        try:
            self._core_info
        except AttributeError:
            self._core_info = self.CoreInfoType()
        return self._core_info
    
        
class HasCoreInfoWithCustomDigits(object):
    """
    Mixin providing the info property based on parametrised CoreInfoType when
    there is custom digits support
    """
    @property
    def _info(self):

        try:
            self._core_info
        except AttributeError:
            self._core_info = self.CoreInfoType(custom_digits=
                                                      self.emulator_build)
        return self._core_info


