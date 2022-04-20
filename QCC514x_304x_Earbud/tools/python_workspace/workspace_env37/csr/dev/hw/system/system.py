############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from collections import OrderedDict
import itertools
import os, functools
from abc import ABCMeta, abstractproperty, abstractmethod

from csr.wheels.global_streams import iprint
from csr.dev.env.var_diff import VarDiff
from csr.dev.fw.debug_log import log_livener
from csr.dev.tools.core_command_set_manager import command_wrapper
from csr.dev.model.interface import OrderedSet, Code
from datetime import datetime, timedelta
from csr.wheels.bitsandbobs import NameSpace
import bisect
import threading
from itertools import cycle

# compatible with Python 2 and 3
ABC = ABCMeta('ABC', (object,), {'__slots__': ()})


class BaseSystem(object):
    """
    A generic system (set of device tree objects),
    """
    def __init__(self, objs, parent_devices, shared_env=True, enum_prefix=None):
        
        self._parent_devices = parent_devices
        self._shared_env = shared_env
        
        self._protect_label_objs = False
        
        if not isinstance(objs, OrderedDict):
            # A plain list of objs: turn it into a default label->obj mapping
            if enum_prefix is None:
                self._objs = OrderedDict((chr(ord("A")+i), obj) for (i,obj) in enumerate(objs))
            else:
                self._objs = OrderedDict((enum_prefix+str(i), obj) for (i,obj) in enumerate(objs))
        else:
            # An ordered mapping from labels to objs
            self._objs = objs
            
        # Convenience for random access by index
        self.__obj_list = list(self._objs.values())

        for label, obj in self._objs.items():
            
            setattr(self, label, obj)

        self._protect_label_objs = True


    def __setattr__(self, attr, value):
        try:
            if self._protect_label_objs and attr in self._objs:
                raise AttributeError("Can't set attribute '%s'" % attr)
        except AttributeError:
            # _protect_label_objs or self._objs haven't been created yet:
            # just carry on and set the attribute.
            pass
        object.__setattr__(self, attr, value)


    def __getitem__(self, i):
        return self.__obj_list[i]
    
    def __len__(self):
        return len(self.__obj_list)
    
    def __contains__(self, obj):
        return obj in self.__obj_list

    # Protected

    def _expand_tree(self, namespace):
        """
        With heterogeneous hardware we can't do any tree expansion at all
        """
        pass
    
    def _threadsafe_method_call_no_return(self, method_name, *args, **kwargs):
        """
        Call a method on all objects in the system.  This is for cases 
        where calling the method on one object is threadsafe wrt all the other 
        system objects, and where the method has no return value.  In practice,
        thread-safety wrt other system objects requires a) that this be a device
        system, to ensure that there is a separate transport connection for
        each device, and b) that there is no shared environment between the
        devices, unless the method to be called doesn't touch firmware environment
        or build info.
        """
        threads = set()
        for obj in self:
            t = threading.Thread(target=getattr(obj, method_name), 
                                 args=args, kwargs=kwargs)
            threads.add(t)
            t.start()
        for t in threads:
            t.join()
        

def system_factory(devices, shared_env=True, labels=None, enum_prefix=None):
    """
    Create a system object from an iterable of devices.  The devices may or may
    not be able to shared firmware environments at the core level.  Labels may
    be supplied to name each element in the system.
    
    If every device supplied has the exact same type, then a CommonDeviceSystem
    is created, which recursively creates systems at lower levels within the
    device trees - a CommonChipSystem for each corresponding set of Chips within
    the device (usually there's just one of these), a CommonSubsystemSystem for
    each corresponding set of subsystems, and a CommonCoreSystem for each
    corresponding set of cores.  Otherwise a simpler DeviceSystem is created
    which provides much less functionality: its main purpose is to allow for
    the construction of ad hoc systems of cores, for example, to allow
    interleaved logging from an arbitrary set of cores within one or more chips. 
    """
    devices = list(devices) # we have to iterate over this more than once.
    
    if not labels:
        # This is an infinite generator, but we only use it in the zip call
        # below which will stop when the devices iterator ends.
        if not enum_prefix:
            labels = (chr(ord("A")+i) for i in itertools.count())
        else:
            labels =(enum_prefix+str(i) for i in itertools.count())
        
    devices = OrderedDict(zip(labels,devices))

    if len(set(type(dev) for dev in devices)) == 1:
        # All devices have same type
        system = CommonDeviceSystem(devices, devices, shared_env=shared_env)
        system._expand_tree(system)
        return system

    # Otherwise just a generic system
    return DeviceSystem(devices, devices)
    

class HasCommonHardware(ABC):
    """
    A system of device model entities all with the same hardware. In fact the
    hardware only has to be "the same" to the extent that the tree of chips,
    subsystems and cores has the same shape and naming in each device, so that 
    it is possible to create systems from corresponding sets of those entities. 
    """
    @abstractmethod
    def _expand_tree(self, namespace):
        raise PureVirtualError
        
    def _expand_tree_list(self, child_list_name, child_factory, namespace):
        """
        Recursively build systems of child entities from corresponding entries
        in each parent system entity's list of children, inserting them into a
        list attribute in the parent system with the same name as the child
        list in the parent system's entries. 
        
        For example in a system of three devices, where each device has a 
        list "chips" containing two chips:
        
        Parent system:    devA    devB    devC  
                        |       |       |      |
        Chips list:     | chip1 | chip1 | chip1|
                        | chip2 | chip2 | chip2|
        
        this function causes two child chip systems to be created like this
        
        Parent system:   devA   devB   devC
                         --------------------
        Chips list:      chip1  chip1  chip1   <- child system 1 (CommonChipSystem)
                         --------------------
                         chip2  chip2  chip2   <- child system 2 (CommonChipSystem)
                         --------------------
        
        when called as self._expand_tree_list("chips", CommonChipSystem, namespace)
        
        The list of child chip systems is then inserted into an attribute 
        "chips" in the parent system. 
        """
        # In the following, "child_set" ends up containing a list of (say) chip
        # objects, one for each (say) device in the system (note that zip is
        # effectively a matrix transposition).  zip(self._objs, child_set) then
        # creates an iterable of (label, chip) pairs (because iterating over
        # self._objs is the same as iterating over self._objs.keys()).
        child_systems = [child_factory(OrderedDict(zip(self._objs, child_set)), 
                                       self._parent_devices, shared_env=self._shared_env) 
                                 for child_set in zip(*(getattr(obj, child_list_name) 
                                                         for obj in self))]
        
        setattr(self, child_list_name, child_systems)
        
        if len(child_systems) == 1 and child_list_name.endswith("s"):
            setattr(self, child_list_name[:-1], child_systems[0])
        
        for child_system in child_systems:
            
            child_system._expand_tree(namespace)
         
        
    def _expand_tree_dict(self, child_dict_name, child_factory, namespace):
        """
        Very similar to _expand_tree_list except that in this case the child
        entities are given by name in a dictionary and are inserted into the
        parent system as a dictionary.
        """
        child_systems = {child_id : child_factory(OrderedDict(zip(self._objs, child_set)), 
                                                  self._parent_devices, 
                                                  shared_env=self._shared_env) 
                                 for (child_id, child_set) in 
                                     zip(getattr(self[0], child_dict_name).keys(), 
                                         zip(*(getattr(obj, child_dict_name).values() 
                                                               for obj in self)))}
        
        setattr(self, child_dict_name, child_systems)
        
        for child_id, child_system in child_systems.items():
            try:
                setattr(self, child_id, child_system)
            except TypeError:
                # child_id doesn't have the right type to act as an attribute name
                pass
            child_system._expand_tree(namespace)


class DeviceSystem(BaseSystem):
    """
    Generic system of devices, which need not have any particular relationship to
    each other.
    """
    def get_cores(self, *cores):
        all_cores = self._get_cores_per_device()
        def get_core_label(c):
            if len(self) == 1:
                return ""
            labels = [lbl for (lbl, core_set) in all_cores.items() if c in core_set]
                
            return labels[0]
            
        return [(c, get_core_label(c)) for c in cores]

    def _get_cores_per_device(self):
        """
        Manually walk through the device tree to find all the cores it contains
        that match the given list, returning the core object along with its
        system label
        This handles the possibility of a chip with no subsystems.
        """
        cores_found = {}
        for label, dev in self._objs.items():
            for chip in dev.chips:
                try:
                    chip.subsystems
                except AttributeError:
                    cores_found.setdefault(label, []).extend(chip.cores)
                else:
                    for ss in chip.subsystems.values():
                        cores_found.setdefault(label, []).extend(ss.cores)
        return cores_found
    
    def core_system(self, *cores):
        """
        Create a CoreSystem based on the supplied list of cores
        """
        expanded_cores = sum((list(c) if isinstance(c, CoreSystem) else [c] 
                              for c in cores), [])
        
        cores_and_labels = self.get_cores(*expanded_cores)

        if set(expanded_cores) - set(c for (c,_) in cores_and_labels):
            raise ValueError("Didn't find all the core objects!") 
        
        core_dict = OrderedDict(((c.nicknames[0]+("_"+lbl if lbl else ""),
                                  c) for (c,lbl) in cores_and_labels))
        
        return CoreSystem(core_dict, self._objs, shared_env=False)
    
    def trb_live_log(self, *cores, **kwargs):
        """
        Run TRB-based logging of the given set of core objects (note - objects,
        not names), interleaving the messages using the built-in timestamp.
        The cores can be from any of the system's devices.  Alternatively 
        system core containers can be specified, which is equivalent to specifying
        the cores they contain separately.
        
        E.g. 
         >>> system.trb_live_log(apps0A, apps1A, apps0B, apps1B)
        or
         >>> system.trb_live_log(system.cur, system.apps, system.apps1)
        """
        if not cores:
            raise TypeError("system.trb_live_log must be called with an explicit "
                            "set of core objects")
        
        return self.core_system(*cores).trb_live_log(**kwargs)

    def live_log(self, *cores, **kwargs):
        """
        Run polling-based logging of the given set of core objects (note - objects,
        not names), interleaving the messages using the built-in timestamp.
        The cores can be from any of the system's devices.  Alternatively 
        system core containers can be specified, which is equivalent to specifying
        the cores they contain separately.
        
        E.g. 
         >>> system.live_log(apps0A, apps1A, apps0B, apps1B)
        or
         >>> system.live_log(system.cur, system.apps, system.apps1)
        """
        if not cores:
            raise TypeError("system.live_log must be called with an explicit "
                            "set of core objects")
        
        return self.core_system(*cores).live_log(**kwargs)

    def reset(self, reset_type=None, **kwargs):
        """
        Reset all the devices, using a multi-threaded approach to avoid delays
        being serialised. 
        """
        self._threadsafe_method_call_no_return("reset", reset_type, **kwargs)
            

class CommonDeviceSystem(DeviceSystem, HasCommonHardware):
    """
    Set of devices of identically-shaped hardware
    """
    def _expand_tree(self, namespace):
        """
        Create a CommonChipSystem for each corresponding chip in the set of devices
        (usually there is just one chip per device). 
        """
        return self._expand_tree_list("chips", CommonChipSystem, namespace)


class CommonChipSystem(BaseSystem, HasCommonHardware):
    """
    System of chip instances which have identically-shaped subsystem structure
    """
    def _expand_tree(self, namespace):
        try:
            self[0].subsystems_dict
        except AttributeError:
            return self._expand_tree_list("cores", CommonCoreSystem, namespace)
        else:
            return self._expand_tree_dict("subsystems_dict", CommonSubsystemSystem,
                                          namespace)


class CommonSubsystemSystem(BaseSystem, HasCommonHardware):
    """
    Set of subsystem instances which have identically-shaped core structure
    """
    def _expand_tree(self, namespace):
        
        return self._expand_tree_list("cores", CommonCoreSystem, namespace)


class SimpleComponentSystem(BaseSystem):
    """
    System class that takes a given component object and reproduces its public
    API by implementing each method to call the real method on each object in the 
    system with identical arguments, and then returning the set of return
    values in an OrderedDict.
    
    Note: will not work well with callables that mutate their arguments. 
    Callables in the API thta shouldn't be included can be filtered out by adding
    their names to the "unwanted" iterable.
    """
    def __init__(self, objs, devices, shared_env=True, unwanted=None):
        BaseSystem.__init__(self, objs, devices, shared_env=shared_env)
        
        first_obj = self[0]
        # Loop over every attribute of an arbitrary one of the objects and
        # insert proxies for the public callables which don't clash with an
        # existing attribute name.
        unwanted = set(unwanted) if unwanted is not None else set()
        for attr in dir(first_obj):
            if not attr.startswith("_"):
                attr_value = getattr(first_obj, attr)
                if hasattr(attr_value, "__call__") and attr not in unwanted:
                    if hasattr(self, attr):
                        iprint("WARNING: Can't add system wrapper for '%s' - name already in use" % attr)
                    else:
                        # We need multiple layers here so that closures work.
                        # Specifically, we can't get attr from the closure 
                        # because it takes multiple values within the outer context.
                        def make_attr_fn(attr):
                            @functools.wraps(attr_value)
                            def _fn(*args, **kwargs):
                                # Call the set of real functions with fixed
                                # args.  WARNING: this could have funny behaviour
                                # if the method mutates any of the argument
                                # objects, but we will trust that doesn't happen
                                # in practice. 
                                ret = tuple(getattr(obj, attr)(*args, **kwargs) 
                                            for obj in self)
                                if all(r is None for r in ret):
                                    return None
                                # Return values ordered by label
                                return OrderedDict(zip(self._objs, ret))
                            return _fn
                                      
                        setattr(self, attr, make_attr_fn(attr))

    
class CoreSystem(BaseSystem):
    """
    System consisting of an arbitrary set of core objects  (these are assumed to
    have the normal core interface for a device model, e.g. a Firmware instance in
    attribute fw, etc.)
    
    No assumption is made about the equivalence of the cores.
    """
    
    def __init__(self, cores_dict, parent_devices, shared_env=False):
        BaseSystem.__init__(self, cores_dict, parent_devices, shared_env=shared_env)
        
        self.live_log = command_wrapper(self._live_log, 
                                        self[0].nicknames[0], "live_log")
        self.trb_live_log = command_wrapper(self._trb_live_log, 
                                            self[0].nicknames[0], "trb_live_log")
        
        # Keep track of the cores that have their log formatting set automatically.
        # These should be cleared out when the logging stops.
        self._clear_formatting_in = set()
        
        
    @property
    def decoder(self):
        """
        Proxy for the fw log decoder which simply passes any call through to
        all the system objects in system order, using the same set of arguments
        each time (so mutations will be propagated).
        """
        try:
            self._decoder
        except AttributeError:
            self._decoder = self._set_up_component_system("fw.debug_log.decoder")
        return self._decoder
    
    @property
    def reader(self):
        """
        Proxy for the fw log reader which simply passes any call through to
        all the system objects in system order, using the same set of arguments
        each time (so mutations will be propagated).
        """
        try:
            self._reader
        except AttributeError:
            self._reader = self._set_up_component_system("fw.debug_log.reader")
        return self._reader
    
    def _set_up_component_system(self, attr_string):
        """
        Create a SimpleComponentSystem based on the object obtained by applying
        the given "attr_string", which is a string represented a potentially
        nested attribute access (e.g. "attr.subattr").
        """
        attr_tree = attr_string.split(".")
        def get_attr(owner, attr_tree):
            obj = owner
            for attr in attr_tree:
                obj = getattr(obj, attr)
            return obj
        
        return SimpleComponentSystem(OrderedDict((label, get_attr(obj, attr_tree)) 
                                          for (label, obj) in self._objs.items()),
                                     self._parent_devices)
    
    
    def _order_by_device(self, cores_and_labels):
        """
        Return the given iterable of cores in an OrderedDictionary based on the
        self._parent_devices OrderedDictionary, preserving the order of cores
        within the same device
        """
        cores_and_labels_by_device = OrderedDict(((lbl, []) for lbl in self._parent_devices))
        for core, label in cores_and_labels:
            device = core.subsystem.chip.device
            matching_device_labels = [lbl for lbl, dev in self._parent_devices.items() if dev is device]
            if len(matching_device_labels) != 1:
                raise RuntimeError("Unexpected number of devices (%d) match "
                                   "this core" % len(matching_device_labels))
            device_label = matching_device_labels[0]
            cores_and_labels_by_device[device_label].append((core, label))
        return cores_and_labels_by_device
        
    
    def _set_default_decoder_formatting(self, indent=4, log_colours=None, 
                                        grouping=None, custom_core_set=None):
        """
        Sets default prefixes and colours in the underlying log decoders if
        these haven't already been set
        """
        base_colours = ["green", "cyan", "blue", "red", "magenta","yellow"]
        bright_colours = ["bright"+col for col in base_colours]
        
        if custom_core_set is None:
            custom_core_set = ["cur","bt","audio0","apps0","apps1"]
        
        if grouping is None:
            prefix_indent = 0
            for label, core in self._objs.items():
                prefix= "".join([" " * prefix_indent, label])
                prefix_indent += indent
                if not core.fw.debug_log.decoder.prefix_set:
                    core.fw.debug_log.decoder.set_prefix("%s: " % prefix, exact=True)
                    self._clear_formatting_in.add(core)
    
            if log_colours is None:
                if 2 <= len(self):
                    log_colours = bright_colours
                else:
                    log_colours = [None]
                
            for core, colour in zip(self._objs.values(), cycle(log_colours)):
                if colour and not core.fw.debug_log.decoder.default_colour_set:
                    core.fw.debug_log.decoder.set_default_colour(colour) 
        elif grouping == "core":
            # Pair up the cores where possible, giving each core in a pair the
            # same indentation and a related colour
            indent_order = custom_core_set
            
            # First group cores by their nicknames
            core_groups = OrderedDict()
            for lbl, c in self._objs.items():
                core_groups.setdefault(c.nicknames[0],[]).append((c, lbl))
            
            # Now set up the indents by ordering the groups in the order given
            # and increasing the offset for each one
            indent_groups = {}
            prefix_indent = 0
            for grp in indent_order:
                if grp in core_groups:
                    indent_groups[grp] = " "*prefix_indent
                    prefix_indent += indent
            
            # Then, break those groups up by device (this will leave lists of
            # length one since there is one core of each group (type) in each device
            ordered_core_groups = OrderedDict()
            for nickname, grp in core_groups.items():
                ordered_grp = self._order_by_device(grp)
                ordered_core_groups[nickname] = ordered_grp
            
            device_colours = bright_colours
            # Now for each group, ("cur", "apps0", etc), give each core in the
            # group the same indent, and colour according to the device
            for group, device_cores in ordered_core_groups.items():
                colours = device_colours
                prefix_indent = indent_groups[group]
                for i, cores in enumerate(device_cores.values()):
                    # we only have two colours for each core type, so if we ahve
                    # more than two devices, we'll just have to go round the set
                    # multiple times
                    colour = colours[i%len(colours)]
                    # There can only be one core in each device of a given type,
                    # but _order_by_device returns a list anyway, so we just do
                    # a one-step iteration here.
                    for (core, label) in cores:
                        if not core.fw.debug_log.decoder.default_colour_set:
                            core.fw.debug_log.decoder.set_default_colour(colour)
                            self._clear_formatting_in.add(core)
                        if not core.fw.debug_log.decoder.prefix_set:
                            core.fw.debug_log.decoder.set_prefix(prefix_indent + label + ": ",
                                                                 exact=True)
                            self._clear_formatting_in.add(core)
                
            
            
        elif grouping == "device":
            # Group cores by the device they are in, giving each device group
            # the same indentation, and each core pair across groups the same 
            # colour
            cores_by_device = self._order_by_device((core,lbl) for (lbl,core) in self._objs.items())
            prefix_indent = 0
            for idev, cores in enumerate(cores_by_device.values()):
                colour_groups = dict(zip(custom_core_set, bright_colours))
                for core, label in cores:
                    colour = colour_groups[core.nicknames[0]] 
                    if not core.fw.debug_log.decoder.default_colour_set:
                        core.fw.debug_log.decoder.set_default_colour(colour)
                        self._clear_formatting_in.add(core)
                    if not core.fw.debug_log.decoder.prefix_set:
                        core.fw.debug_log.decoder.set_prefix(" "*prefix_indent + label + ": ",
                                                             exact=True)
                        self._clear_formatting_in.add(core)
                prefix_indent += indent
                

    def _clear_default_decoder_formatting(self):
        for core in self._clear_formatting_in:
            core.fw.debug_log.decoder.clear_prefix()
            core.fw.debug_log.decoder.clear_default_colour()


    def _live_log(self, indent=4, sleep_time=0, exit_check=None, persistent=True,
                 log_colours=None, time_fmt=None, grouping=None):
        """
        Start live logs for all cores interleaving the output. 
        :param indent, optional: 
        :param persistent: Don't check if the processor is still running when 
         there is no new data in the buffer; with this False the logger will 
         exit on spotting that the processor isn't running, but note that it 
         will only check this if there is no new data available to avoid a 
         costly and pointless check that would increase the risk of buffer 
         wrapping by slowing the scraping down.
        :param log_colours, optional: A list of Ansi colour codes to apply to 
         each device's log output. Supported colours are - 'black', 'blue', 
         'cyan', 'green', 'magenta', 'red', 'white', 'yellow', optionally prefixed
         with 'bright' or 'dark' i.e. 'brightyellow'.  These are only applied
         if no default colour has been set explicitly in the underlying log
         decoder objects
        :param time_fmt, optional: A datetime style format to apply to log entries
        """
        self._set_default_decoder_formatting(indent, log_colours, grouping=grouping)

        def query_logs(real_time, **kwargs):
            logs = []
            exceps = []
            for core in self:
                try:
                    logs.append(core.fw.debug_log.generate_decoded_event_report(real_time=real_time,
                                                                                check_running=not persistent,
                                                                                time_fmt=time_fmt))
                except Exception as exc:
                    exceps.append(exc)
                    prefix = core.fw.debug_log.decoder._prefix
                    logs.append("%sLog read failed: %s" % (prefix, str(exc)))
                else:
                    exceps.append(None)
            if all(exc is not None for exc in exceps): # only give up if all the cores are failing to read the logs 
                raise exceps[0] # let log_livener deal with things
            else:
                # Reset the readers that are failing, to try to get back again
                for exc, core in zip(exceps, self):
                    if exc is not None:
                        core.fw.debug_log.reader.apply_reset()
                
            return OrderedSet(logs, item_colours=False)

        try:
            for block in log_livener(query_logs,
                                     sleep_time=sleep_time,
                                     exit_check=exit_check,
                                     reader=self.reader):
                yield block
        finally:
            self._clear_default_decoder_formatting()

    def _trb_live_log(self, indent=4, sleep_time=0, exit_check=None, persistent=True,
                      log_colours=None, time_fmt=None, grouping=None):
        """
        Start live logs for all cores using TRB logging, interleaving the output.
        :param indent, optional: Defines the indent applied between the two devices log entries.
        :param sleep_time, optional: Defines any sleep time used between log polls.
        :param exit_check, optional: Callback that can be used to exit live log.
        :param persistent, optional: Not currently implemented
        :param log_colours, optional: Not currently implemented
        :param time_fmt, optional: Not currently implemented
        """
        log_streams = []

        # First figure out the underlying set of chips, because each one has its
        # own TrbLogger instance.
        chips = {}
        chip_cores = {}
        for core in self:
            # a BaseChip itself isn't hashable, so we use the object id as the
            # key mapping each chip to a list of the target cores it contains 
            chip_cores.setdefault(id(core.subsystem.chip), set()).add(core)
            # also create a simple lookup to get from the ID of a chip object
            # to the object itself
            chips[id(core.subsystem.chip)] = core.subsystem.chip

        for chip in chips.values():
            log_streams.append(NameSpace())
            log_streams[-1].chip = chip

        self._set_default_decoder_formatting(indent, log_colours, grouping=grouping)

        for log_stream in log_streams:
            log_stream.last_timestamp = 0
            log_stream.last_timestamp_high = 0

            log_stream.trb_log = log_stream.chip.trb_log
            # For each stream keep a list of the set of cores that are being
            # logged on that chip.
            log_stream.core_nicknames = [core.nicknames[0] for core in chip_cores[id(log_stream.chip)]]
            log_stream.trb_log.start(log_stream.core_nicknames)

        # Read trb timestamp so we can adjust timestamps between multiple trbs
        for log_stream in log_streams:
            log_stream.trb_datetime = datetime.now()
            log_stream.trb_start_time = log_stream.trb_log.stream.read_dongle_register(0, 0x1200)
            log_stream.trb_base_timestamp = log_stream.trb_log.stream.read_dongle_register(0, 0x1204) * (2 ** 32)

            # Read lower 32 bits again and check if it has wrapped
            low_time = log_stream.trb_log.stream.read_dongle_register(0, 0x1200)
            if low_time < log_stream.trb_start_time:
                log_stream.trb_datetime = datetime.now()
                log_stream.trb_start_time = low_time
                # Re-read upper 32 bits again in case it was read before wrap
                log_stream.trb_base_timestamp = log_stream.trb_log.stream.read_dongle_register(0, 0x1204) * (2 ** 32)
            log_stream.trb_start_time += log_stream.trb_base_timestamp

        if time_fmt == True:
            time_fmt = "%H:%M:%S:%f " 


        def query_logs(real_time, **kwargs):
            """
            Helper function to give to log livener that pulls from each devices trb log, interleaves the results
            based on timestamp and returns a formatted chunk of log as a Code interface object. real_time argument is 
            not consumed just present to match the expected signature for log_livener.
            """
            _timestamp_list = []
            _log_list = []
            top_insert_pos = -1
            for log_stream in log_streams:
                log_stream.trb_log.sort(app_decode=True)
                wrap_count = 0
                log_stream.trb_base_timestamp = (log_stream.chip.trb_log.
                                                 stream.read_dongle_register(0, 
                                                                             0x1204) * 
                                                  (2 ** 32))

                for log_entry in log_stream.trb_log.tr_log:

                    line = log_entry[1]

                    # Merge line into global list
                    time = (log_entry[0] + (2 ** 32 * wrap_count) + 
                                        log_stream.trb_base_timestamp) - log_stream.trb_start_time
                    pos = bisect.bisect(_timestamp_list, time)
                    _timestamp_list.insert(pos, time)
                    _log_list.insert(pos, (log_stream, line))
                    if pos > top_insert_pos:
                        top_insert_pos = pos

                    # Check if timestamp has wrapped
                    if log_entry[0] < log_stream.last_timestamp:
                        wrap_count += 1
                    log_stream.last_timestamp = log_entry[0]

                # Empty log after processing
                log_stream.trb_log.clear()
                log_stream.trb_log.tr_log = []
            
            # Constr
            combined_list = []
            for timestamp, (log_stream, line) in zip(_timestamp_list, _log_list):
                trb_dt = log_stream.trb_datetime + timedelta(microseconds=timestamp / 10.0)
                if time_fmt is not None:
                    timestr = trb_dt.strftime(time_fmt)
                else:
                    timestr = "{}".format(trb_dt)
                combined_list.append("{} {}".format(timestr, line))
            
            if combined_list:
                return Code("\n".join(combined_list))

        def retrigger_trb_logging():
            """
            Callback for log_livener which loops over the log streams and
            attempts to re-trigger them.  Returns True if all the re-trigger
            attempts worked.
            """
            success = True
            for log_stream in log_streams:
                success = log_stream.trb_log.attempt_restart(log_stream.core_nicknames) and success
            return success

        try:
            for block in log_livener(query_logs,
                                     sleep_time=sleep_time,
                                     exit_check=exit_check,
                                     resume_cb=retrigger_trb_logging):
                yield block
        finally:
            for log_stream in log_streams:
                log_stream.trb_log.stop(log_stream.core_nicknames)
            self._clear_default_decoder_formatting()

class CommonCoreSystem(CoreSystem, HasCommonHardware):
    """
    A CommonCoreSystem is a CoreSystem with the constraint that the cores
    contained in it are equivalent but on separate devices (e.g. all the Curator
    cores, etc).
    """
    def var_diff(self, filter_cus=None, cus_incl_not_excl=False,
                filter_types=None, types_incl_not_excl=False,
                context=3, display=True, unified=False, quiet=False):
        """
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
        :param context, optional: Number of lines of context for the diff text
         default=3)
        :param display, optional: Whether to display the diff output to the screen
         (the default) or return it as a list of strings.
        :param unified, optional: If True, returns the diff in "unified diff"
         format, rather than the default "context diff" format
        :param quiet, optional: If True, suppresses a small amout of progress
         output
        """

        if len(self) != 2:
            raise ValueError("Can't execute var_diff with a system of %d elements" % len(self))
        
        labels = list(self._objs)
        diff = VarDiff(self[0].fw.env, self[1].fw.env,
                       filter_cus=filter_cus, cus_incl_not_excl=cus_incl_not_excl,
                       filter_types=filter_types, types_incl_not_excl=types_incl_not_excl, 
                       labelA=labels[0], labelB=labels[1], quiet=quiet)
        if unified:
            return diff.unified_diff(context=context, display=display)
        else:
            return diff.context_diff(context=context, display=display)

    def _expand_tree(self, namespace):
        # We don't create system-wide subcomponents of Cores, although in principle
        # we could go on to core.fw, core.fw.env
        for nickname in self[0].nicknames:
            setattr(namespace, nickname, self)
    
    @property
    def nicknames(self):
        return self[0].nicknames
