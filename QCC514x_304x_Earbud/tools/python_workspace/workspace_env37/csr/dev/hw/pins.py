############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.hw.address_space import AddressSpace
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.model.base_component import BaseComponent
import collections
import sys
import operator
from itertools import chain
import re

class Pin(object):
    def __init__(self, curator, pio_no, pad_name, pio_domain="CORE"):
        self._cur = curator
        self.pio_no = pio_no
        self.pad_name = pad_name

        pad_control_class = None
        pio_mux_class = None
        pad_mux_class = None

        if pio_domain == "KA/POR":
            pad_control_class = KAPORPad
            pio_mux_class = ChipPioMux
            pad_mux_class = PorPadMux
        elif pio_domain == "POR":
            pad_control_class = PORPad
            pio_mux_class = ChipPioMux
            pad_mux_class = PorPadMux
        elif pio_domain == "KA":
            pad_control_class = KAPad
            pio_mux_class = ChipPioMux
            pad_mux_class = CorePadMux
        else: # CORE pios
            if ("SQIF" in self.pad_name or
                "SQIO" in self.pad_name or
                "PIO" in self.pad_name):
                pad_control_class = StandardPad
                pio_mux_class = ChipPioMux
                pad_mux_class = CorePadMux
            if ("PWM" in self.pad_name):
                pad_control_class = PwmPad
                pio_mux_class = ChipPioMux
                pad_mux_class = CorePadMux
            if ("XTAL" in self.pad_name):
                pad_control_class = XtalPad
            if ("SYS_CTRL" in self.pad_name):
                pad_control_class = SysCtrlPad
            if ("RESETB" in self.pad_name):
                pass
            if ("LED" in self.pad_name):
                pad_control_class = LedPad
                pio_mux_class = ChipPioMux
                pad_mux_class = LedPadMux
            if ("XIO" in self.pad_name):
                pad_control_class = XioPad
                pio_mux_class = ChipPioMux
                pad_mux_class = CorePadMux
            if ("UNIMPLEMENTED" in self.pad_name):
                pass
            if ("USB" in self.pad_name):
                if ("DP" in self.pad_name or
                    "DN" in self.pad_name):
                    pad_control_class = UsbPad
                    pio_mux_class = ChipPioMux
                    pad_mux_class = CorePadMux

        # Assign pin attributes
        self.core_pad_mux = pad_mux_class(self._cur, self.pio_no, self.pad_name) if pad_mux_class is not None else None
        self.core_pad_control = pad_control_class(self._cur, self.pio_no, self.pad_name) if pad_control_class is not None else None
        self.chip_pio_mux = pio_mux_class(self._cur, self.pio_no) if pio_mux_class is not None else None

    def report(self, report=False):
        """
        Reports the index, pad name, core pad control, core pad mux and
        chip pio mux settings of a pin.

        The index is the actual PIO number.

        Read is the value of the PIO read by the Curator. We don't care if it's
        an input or output, this should return the state of the PIO.

        The pad name is taken from the register definitions and hints at the
        type of PIO pad implemented in hardware.

        The core pad control report is a summarised version of
        core_pad_control.report() present in most PIOs (others don't have these
        controls). This contains the type of pad, drive strength, slew enable
        and pull enable, direction, strength and sticky settings. Different
        pads have different settings here. Some differ in drive strengths,
        others in slew rates, or they may be open drain or even analogue
        capable etc. Please see core_pad_control.report() for more details.

        The core pad mux report is a copy of core_pad_mux.report() present in
        most PIOs (others don't go through this mux and instead are connected
        straight to a hardware module). This contains the actual muxed signal
        name.

        The chip pio mux report is a summary of chip_pio_mux.report() present
        in most PIOs. This contains the invert enable, operational mode
        (functional or debug) and based on this either the subsystem mux
        setting or debug mode(parallel or serial), bus and bit.
        """
        def status(pio):
            bank16 = (pio//16) + 1 # Starts at 1
            if bank16 == 1:
                reg_name = "PROC_PIO_STATUS"
            else:
                reg_name = "PROC_PIO_STATUS%d" % bank16
            return (self._cur.fields[reg_name] >> (pio % 16)) & 0x01

        headings = ["PIO"]
        row = ["%02d" % self.pio_no]

        headings += ["Read"]
        row += ["%d" % status(self.pio_no)]

        headings += ["Pad Name"]
        row += ["%s" % self.pad_name]

        headings += ["Core Pad Control"]
        try:
            core_pad_control = self.core_pad_control.report(report=True).summary if self.core_pad_control is not None else "-"
        except AddressSpace.ReadFailure:
            core_pad_control = "not powered"
        row += [core_pad_control]

        headings += ["Core Pad Mux"]
        core_pad_mux = self.core_pad_mux.report(report=True).text if self.core_pad_mux is not None else "-"
        row += [core_pad_mux]

        headings += ["Chip Pio Mux"]
        chip_pio_mux = self.chip_pio_mux.report(report=True).summary if self.chip_pio_mux is not None else "-"
        row += [chip_pio_mux]

        output = interface.Table(headings)
        output.add_row(row)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class Pins(BaseComponent):
    def __init__(self, curator):
        self._cur = curator
        self._pins = []
        self._ka_pad_dict = {}
        self._por_pad_dict = {}
        _pio_nos = set()
        _ka_pio_nos = set()
        _por_pio_nos = set()

        self._pad_dict = self._cur.core.info.io_map_info.\
                                virtual_field_records["PAD_INDICES"].__dict__
        _core_pio_nos = set([x.value for x in self._pad_dict.values()])

        try:
            self._ka_pad_dict = self._cur.core.info.io_map_info.\
                                    virtual_field_records["KA_PAD_INDICES"].__dict__
            _ka_pio_nos = set([x.value for x in self._ka_pad_dict.values()])
        except KeyError:
            # No KA pins
            pass

        try:
            self._por_pad_dict = self._cur.core.info.io_map_info.\
                                    virtual_field_records["POR_PAD_INDICES"].__dict__
            _por_pio_nos = set([x.value for x in self._por_pad_dict.values()])
        except KeyError:
            # No POR pins
            pass

        self._full_pad_dict = dict(chain(self._pad_dict.items(),
                                         self._ka_pad_dict.items(),
                                         self._por_pad_dict.items()))

        # Generate a list of all pins by pin number
        # Key is the PIO number
        # Value is the name of the PIO from PAD indicies array with the
        # suffix removed
        full_pin_list_by_num = {}
        for k, v in self._full_pad_dict.items():
            n = k.replace("_PAD_IDX", "")
            try:
                # Pin already in the dictionary, add the new name to the list
                full_pin_list_by_num[v.value].append(n)
            except KeyError:
                # Pin is not in the dictionary yet, add as a new item
                full_pin_list_by_num[v.value] = [ n ]

        # Order names by PIO then alt names
        # Name will be "PIOxx, altname1, altname2..."
        for k, v in full_pin_list_by_num.items():
            # Sort entries in names list
            v.sort(key=lambda n: "" if "PIO" in n else str.lower(n))
            # Make one string for the names, separated by a comma
            full_pin_list_by_num[k] = ', '.join(v)

        for k in sorted(full_pin_list_by_num):
            v = full_pin_list_by_num[k]
            # Add this pin to the list
            # Determine which domain this pin is in:
            if k in _ka_pio_nos and k in _por_pio_nos:
                # Special case where the pin exists as a POR pin and a KA pin
                pio_domain = "KA/POR"
            elif k in _ka_pio_nos:
                pio_domain = "KA"
            elif k in _por_pio_nos:
                pio_domain = "POR"
            else:
                pio_domain = "CORE"

            self._pins.append(Pin(self._cur, k, v, pio_domain))
            _pio_nos.add(k)
        # Sort pins by pio_no
        self._pins = sorted(self._pins, key=lambda pin: pin.pio_no)
        for i in range(len(self._pins)):
            self.__setattr__("pio%02d" % i, self._pins[i])

    def __len__(self):
        return len(self._pins)

    def __getitem__(self, slice):
        return self._pins[slice]

    def test(self):
        """
        Sets up the PIOs in some "interesting" way so that we can easily test
        the report manually.
        """
        def get_bank(pio):
            return pio // 32
        def get_mask(pio):
            return 1 << (pio - (get_bank(pio) * 32))
        def drive_en(pio, val=None):
            bank16 = (pio//16) + 1 # Starts at 1
            if bank16 == 1:
                reg_name = "PROC_PIO_DRIVE_ENABLE"
            else:
                reg_name = "PROC_PIO_DRIVE_ENABLE%d" % bank16
            if val == None:
                return self._cur.fields[reg_name]
            else:
                self._cur.fields[reg_name] = val
        def drive(pio, val=None):
            bank16 = (pio//16) + 1 # Starts at 1
            if bank16 == 1:
                reg_name = "PROC_PIO_DRIVE"
            else:
                reg_name = "PROC_PIO_DRIVE%d" % bank16
            if val == None:
                return self._cur.fields[reg_name]
            else:
                self._cur.fields[reg_name] = val
        for (pio, drive_str, slew_en, pull_en,
             pull_dir, pull_str, sticky_en) in [(20, 2, 0, 0, "down", "weak", 1),
                                                (21, 4, 0, 1, "up", "strong", 0),
                                                (22, 8, 1, 0, "up", "weak", 1),
                                                (23, 12, 1, 1, "down", "strong", 0)]:
            pin = self._pins[pio]
            pin.chip_pio_mux.set_pio_settings_debug_enable(False)
            pin.chip_pio_mux.set_pio_settings_func_mode_subsys_sel(0)
            pin.core_pad_control.set_drive_strength(drive_str)
            pin.core_pad_control.set_slew_enable(slew_en)
            pin.core_pad_control.set_pull_enable(pull_en)
            pin.core_pad_control.set_pull_direction(pull_dir)
            pin.core_pad_control.set_pull_strength(pull_str)
            pin.core_pad_control.set_sticky_enable(sticky_en)
        for pio, inv, dbus, dbus_bit in [(24, 0, "A", 0),
                                         (25, 1, "A", 13),
                                         (26, 0, "B", 24),
                                         (27, 1, "B", 24),]:
            pin = self._pins[pio]
            pin.chip_pio_mux.set_pio_settings_debug_enable(True)
            pin.chip_pio_mux.set_pio_settings_parallel_sel(True)
            pin.chip_pio_mux.set_pio_chip_output_invert(inv)
            pin.chip_pio_mux.set_pio_settings_debug_mode_a_sel(dbus)
            pin.chip_pio_mux.set_pio_settings_debug_bus_bit_sel(dbus_bit)
        for pio, inv, dbus, ser_bit in [(28, 0, "A", "BIT2"),
                                        (29, 1, "B", "SYNC4")]:
            pin = self._pins[pio]
            pin.chip_pio_mux.set_pio_settings_debug_enable(True)
            pin.chip_pio_mux.set_pio_settings_parallel_sel(False)
            pin.chip_pio_mux.set_pio_chip_output_invert(inv)
            pin.chip_pio_mux.set_pio_settings_debug_mode_a_sel(dbus)
            pin.chip_pio_mux.set_pio_settings_debug_mode_serial_bit_sel(ser_bit)
        for (pio, slew_en, slew_rate, pull_en,
             pull_dir, pull_str, sticky_en) in [(59, 0, 0, 0, "down", "weak", 0),
                                                (60, 1, 1, 1, "up", "weak", 0),
                                                (61, 0, 2, 1, "down", "strong", 1),
                                                (62, 1, 3, 0, "up", "strong", 1)]:
            pin = self._pins[pio]
            pin.chip_pio_mux.set_pio_settings_debug_enable(False)
            pin.chip_pio_mux.set_pio_settings_func_mode_subsys_sel(0)
            pin.core_pad_control.set_slew_enable(slew_en)
            pin.core_pad_control.set_slew_rate(slew_rate)
            pin.core_pad_control.set_pull_enable(pull_en)
            pin.core_pad_control.set_pull_direction(pull_dir)
            pin.core_pad_control.set_pull_strength(pull_str)
            pin.core_pad_control.set_sticky_enable(sticky_en)
        for pio, pull_en, amux_en, invi, invc in [(65, 0, 0, 0, 0),
                                                  (66, 0, 0, 0, 1),
                                                  (67, 0, 0, 1, 0),
                                                  (68, 0, 0, 1, 1),
                                                  (69, 0, 1, 0, 0),
                                                  (70, 1, 0, 0, 0),
                                                  (72, 1, 1, 0, 0),
                                                  (72, 1, 1, 1, 1)]:
            pin = self._pins[pio]
            pin.chip_pio_mux.set_pio_settings_debug_enable(False)
            pin.chip_pio_mux.set_pio_settings_func_mode_subsys_sel(0)
            pin.core_pad_control.set_pull_enable(pull_en)
            pin.core_pad_control.set_amux_enable(amux_en)
            pin.core_pad_control.set_invi(invi)
            pin.core_pad_control.set_invc(invc)
        for pio, dr_en, extra, dr, dr_str in [(74, 0, 0, 0, 0.5),
                                              (75, 0, 1, 0, 0.5),
                                              (76, 0, 1, 0, 4),
                                              (77, 0, 1, 1, 0.5),
                                              (78, 0, 1, 1, 4),
                                              (79, 1, 0, 0, 0.5),
                                              (80, 1, 0, 1, 2),
                                              (81, 1, 0, 0, 4),
                                              (82, 1, 0, 1, 8),
                                              (83, 1, 1, 0, 0.5),
                                              (84, 1, 1, 1, 0.5)]:
            pin = self._pins[pio]
            pin.chip_pio_mux.set_pio_settings_debug_enable(False)
            pin.chip_pio_mux.set_pio_settings_func_mode_subsys_sel(0)
            drive_en(pio, drive_en(pio) & (~(1<<(pio%16))) | (dr_en<<(pio%16)) )
            drive(pio, drive(pio) & (~(1<<(pio%16))) | (dr<<(pio%16)) )
            pin.core_pad_control.set_drive_strength(dr_str)
            pin.core_pad_control.set_extra_func(extra)
        for pio, dr_en, dr, pull_en, d_pull_en in [(88, 0, 0, 0, 0),
                                                   (89, 1, 1, 0, 0),
                                                   (92, 0, 0, 1, 0),
                                                   (93, 0, 0, 1, 1)]:
            pin = self._pins[pio]
            pin.core_pad_mux.set_mux_sel("CORE_PIO")
            pin.chip_pio_mux.set_pio_settings_debug_enable(False)
            pin.chip_pio_mux.set_pio_settings_func_mode_subsys_sel(0)
            drive_en(pio, drive_en(pio) & (~(1<<(pio%16))) | (dr_en<<(pio%16)) )
            drive(pio, drive(pio) & (~(1<<(pio%16))) | (dr<<(pio%16)) )
            if pin.core_pad_control.host:
                pin.core_pad_control.set_pad_enable(False)
                pin.core_pad_control.set_pull_enable(pull_en)
                if pin.core_pad_control.dm:
                    pin.core_pad_control.set_d_pullup_enable(d_pull_en)
            else:
                pin.core_pad_control.set_pad_enable(True)
                if pin.core_pad_control.dp:
                    pin.core_pad_control.set_d_pullup_enable(d_pull_en)

    def report(self, report=False):
        """
        This function grabs the report for each pin and inserts it into a table.
        Please see the documentation for the report function of the pin you are
        interested in.
        """
        output = self._pins[0].report(report=True)
        for pin in self._pins[1:]:
            output.append_table_data(pin.report(report=True))

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

    @property
    def title(self):
        return "GPIO interface configuration"

    def _generate_report_body_elements(self):
        return [self.report(report=True)]

class ChipPioMux(object):
    """
    Pio Mux abstraction class.
    """
    def __init__(self, curator, pio_no):
        self._cur = curator
        self.pio_no = pio_no
        self.pio_settings_vfr = self._cur.core.info.io_map_info.virtual_field_records["PIO_SETTINGS"]
        self._serial_dict = {
            "BIT0":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_BIT0.value,
            "BIT1":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_BIT1.value,
            "BIT2":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_BIT2.value,
            "BIT3":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_BIT3.value,
            "DATA":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_DATA.value,
            "SYNC":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_SYNC.value,
            "SYNC4":self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_SYNC4.value}
        self._serial_dict_reverse = {v:k for k,v in self._serial_dict.items()}

    def _get_pio_reg(self):
        if self.pio_no % 2:
            reg_name = "PIO_%02d%02d_SUBSYS_DEBUG_SELECT" % (self.pio_no - 1, self.pio_no)
        else:
            reg_name = "PIO_%02d%02d_SUBSYS_DEBUG_SELECT" % (self.pio_no, self.pio_no + 1)
        reg = getattr(self._cur.fields, reg_name)
        return getattr(reg, "PIO%02d_SUBSYS_DEBUG_SELECT" % self.pio_no)

    def _get_bits(self, reg, posn, length=1):
        return (reg.read() >> posn) & ((1 << length) - 1)

    def _set_bits(self, reg, posn, v, length=1):
        mask = v << posn
        reg.write(reg.read() & (~(((1 << length) - 1) << posn)) | mask)

    def get_pio_settings_debug_enable(self):
        """
        Returns True if PIO is in debug mode or False if it is in functional mode.
        """
        return (self._get_bits(self._get_pio_reg(),
                               self.pio_settings_vfr.PIO_SETTINGS_DEBUG_EN.value) ==
                self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE.value)

    def set_pio_settings_debug_enable(self, value):
        """
        Sets the PIO in debug mode (True) or functional mode (False).
        """
        if value:
            v = self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE.value
        else:
            v = self.pio_settings_vfr.PIO_SETTINGS_FUNC_MODE.value
        self._set_bits(self._get_pio_reg(),
                       self.pio_settings_vfr.PIO_SETTINGS_DEBUG_EN.value,
                       v)

    def get_pio_settings_parallel_sel(self):
        """
        Returns True if PIO is in parallel mode or False if it is in serial mode.
        """
        return (self._get_bits(self._get_pio_reg(),
                               self.pio_settings_vfr.PIO_SETTINGS_PARALLEL_SERIAL_SEL.value) ==
                self.pio_settings_vfr.PIO_SETTINGS_PARALLEL_SEL.value)

    def set_pio_settings_parallel_sel(self, value):
        """
        Sets the PIO in parallel mode (True) or serial mode (False).
        """
        if value:
            v = self.pio_settings_vfr.PIO_SETTINGS_PARALLEL_SEL.value
        else:
            v = self.pio_settings_vfr.PIO_SETTINGS_SERIAL_SEL.value
        self._set_bits(self._get_pio_reg(),
                       self.pio_settings_vfr.PIO_SETTINGS_PARALLEL_SERIAL_SEL.value,
                       v)

    def get_pio_settings_debug_mode_a_sel(self):
        """
        Returns "A" if PIO is muxed to debug bus A or "B" if PIO is muxed to
        debug bus B.
        """
        if (self._get_bits(self._get_pio_reg(),
                               self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_A_B_SEL.value) ==
            self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SEL_A.value):
            return "A"
        else:
            return "B"

    def set_pio_settings_debug_mode_a_sel(self, value):
        """
        Muxes the PIO to debug bus A ("A") or debug bus B ("B").
        """
        if value == "A":
            v = self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SEL_A.value
        elif value == "B":
            v = self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SEL_B.value
        else:
            raise ValueError("Invalid value %s for bus select" % value)
        self._set_bits(self._get_pio_reg(),
                       self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_A_B_SEL.value,
                       v)

    def get_pio_settings_debug_mode_serial_bit_sel(self):
        """
        Returns the active setting of the serialiser output assuming the PIO is
        in serial mode. Expect "BIT0", "BIT1", "BIT2", "BIT3" for selecting
        which bit of the 4 bit serialiser output, "DATA" for the single data
        bit of the 1 bit serialiser, "SYNC" for selecting the 1 bit sync signal
        and finally "SYNC4" for the 4 bit serialiser sync signal.
        """
        v = self._get_bits(self._get_pio_reg(),
                           self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_BIT_SEL.value,
                           length=3)
        return self._serial_dict_reverse[v]

    def set_pio_settings_debug_mode_serial_bit_sel(self, value):
        """
        Sets the active setting of the serialiser output assuming the PIO is in
        serial mode. Please supply "BIT0", "BIT1", "BIT2", "BIT3" for selecting
        which bit of the 4 bit serialiser output, "DATA" for the single data
        bit of the 1 bit serialiser, "SYNC" for selecting the 1 bit sync signal
        and finally "SYNC4" for the 4 bit serialiser sync signal.
        """
        if value not in self._serial_dict:
            raise ValueError("Invalid value %s for serial bit select" % value)
        v = self._serial_dict[value]
        self._set_bits(self._get_pio_reg(),
                       self.pio_settings_vfr.PIO_SETTINGS_DEBUG_MODE_SERIAL_BIT_SEL.value,
                       v,
                       length=3)

    def get_pio_settings_func_mode_subsys_sel(self):
        """
        Returns the SSID of the subsystem the PIO is muxed to assuming
        functional mode is selected.
        """
        ssid = self._get_bits(self._get_pio_reg(),
                              self.pio_settings_vfr.PIO_SETTINGS_FUNC_MODE_SUBSYS_SEL.value,
                              length=4)
        return ssid

    def set_pio_settings_func_mode_subsys_sel(self, ssid):
        """
        Muxes the PIO to the subsystem indicated by the given SSID assuming
        functional mode is selected.
        """
        self._set_bits(self._get_pio_reg(),
                       self.pio_settings_vfr.PIO_SETTINGS_FUNC_MODE_SUBSYS_SEL.value,
                       ssid,
                       length=4)

    def get_pio_settings_debug_bus_bit_sel(self):
        """
        Returns the bit index of the debug bus the PIO is muxed to assuming
        debug mode is selected.
        """
        bit = self._get_bits(self._get_pio_reg(),
                             self.pio_settings_vfr.PIO_SETTINGS_DEBUG_BUS_BIT_SEL.value,
                             length=5)
        return bit

    def set_pio_settings_debug_bus_bit_sel(self, bit):
        """
        Muxes the PIO to the given bit index on the debug bus assuming debug
        mode is selected.
        """
        self._set_bits(self._get_pio_reg(),
                       self.pio_settings_vfr.PIO_SETTINGS_DEBUG_BUS_BIT_SEL.value,
                       bit,
                       length=5)

    def get_pio_chip_output_invert(self):
        """
        Returns True if the PIO is inverted or False if not.
        Please note that this is a PIO level invert, not a debug bus invert.
        """
        reg = self._cur.fields.PIO_CHIP_OUTPUT_INVERT
        return reg.read() & (1 << self.pio_no) > 0

    def set_pio_chip_output_invert(self, value):
        """
        Enables (True) or disables (False) the inversion of a PIO level.
        Please note that this is a PIO level invert, not a debug bus invert.
        """
        reg = self._cur.fields.PIO_CHIP_OUTPUT_INVERT
        pio_mask = 1 << self.pio_no
        if value == True:
            v_mask = pio_mask
        elif value == False:
            v_mask = 0
        else:
            raise ValueError("Invalid value %s for output invert." % value)
        reg.write(reg.read() & (~pio_mask) | v_mask)

    def parallel_debug_line(self):
        """
        If this PIO is connected to the debug bus in parallel mode then report
        the bus and debug line this PIO is connected to. Otherwise, return None.
        """
        if self.get_pio_settings_debug_enable() and \
                self.get_pio_settings_parallel_sel():
            return (self.get_pio_settings_debug_mode_a_sel(),
                    self.get_pio_settings_debug_bus_bit_sel())
        else:
            return None, None

    def report(self, report=False):
        """
        Reports the chip PIO mux settings of the current pin.
        This contains the invert enable and mode settings. Based on mode the
        pin can be either functional, in which case a further SSID mux setting
        will be reported, or debug. In debug mode the pin will be muxed to the
        debug bus and further bus index (A or B) and debug mode settings will
        be reported. Debug mode can be parallel, in which case the bus bit
        would also be reported, or serial, in which case the bit select setting
        will be reported.
        """
        output = interface.ListLine()

        if self.get_pio_chip_output_invert():
            output.add("Inverted", "yes", "", "inv")
        else:
            output.add("Inverted", "no", "", "ninv")

        if self.get_pio_settings_debug_enable():
            bus = self.get_pio_settings_debug_mode_a_sel()
            output.add_table("Mode", "debug")
            output.add_table("Bus", bus)
            output.add_summary(None, "deb %s" % bus)
            if self.get_pio_settings_parallel_sel():
                bus_bit = self.get_pio_settings_debug_bus_bit_sel()
                output.add_table("Debug Mode", "parallel")
                output.add_table("Bus Bit", bus_bit)
                output.add_summary(None, "par %d" % bus_bit)
            else:
                bit_sel = self.get_pio_settings_debug_mode_serial_bit_sel()
                output.add_table("Debug Mode", "serial")
                output.add_table("Bit Sel", bit_sel)
                output.add_summary(None, "ser %s" % bit_sel)
        else:
            output.add("Mode", "functional", "", "func")
            ssid_mux = self.get_pio_settings_func_mode_subsys_sel()
            output.add("SSID Mux", ssid_mux, "SS")

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class CorePadMux(object):
    """
    Core Pad Mux abstraction class.
    """
    def __init__(self, curator, pio_no, pad_name, func_sel_enum=None, func_sel_prefix=None):
        self._cur = curator
        self.pio_no = pio_no
        self._func_sel_dict={}
        self._field_name = None
        
        for n in pad_name.split(","):
            field_name = n.strip() + "_MUX_SEL"
            if hasattr(self._cur.bitfields, field_name):
                self._field_name = field_name
                break

        if self._field_name is not None:
            vfr = self._cur.core.info.io_map_info.virtual_field_records
            if func_sel_enum is None or func_sel_prefix is None:
                func_sel_enum = "IO_STANDARD_FUNCTION_SELECT_ENUM"
                func_sel_prefix = "IO_FUNC_SEL_"
            for k,v in vfr[func_sel_enum].__dict__.items():
                k = k.split(func_sel_prefix)[1]
                self._func_sel_dict[k] = v.value
            self._func_sel_dict_reverse = {v:k for k,v in self._func_sel_dict.items()}

    def get_mux_sel(self):
        """
        Returns the active setting of the core pad mux.
        Expect "CORE_PIO", "JANITOR_PIO", "T_BRIDGE", "DEBUG_SPI",
        "SDIO_DEVICE", "ULPI", "AUDIO_PCM", "AUDIO_PWM", "EFUSE", "CLK32K",
        "SDIO_HOST", "SQIF" or "USB".
        """
        if self._field_name is not None:
            v = int(getattr(self._cur.bitfields, self._field_name))
            return self._func_sel_dict_reverse[v]
        else:
            return "-"

    def set_mux_sel(self, value):
        """
        Sets the active setting of the core pad mux.
        Please provide "CORE_PIO", "JANITOR_PIO", "T_BRIDGE", "DEBUG_SPI",
        "SDIO_DEVICE", "ULPI", "AUDIO_PCM", "AUDIO_PWM", "EFUSE", "CLK32K",
        "SDIO_HOST", "SQIF" or "USB".
        """
        if self._field_name is not None:
            if value not in self._func_sel_dict:
                raise ValueError("Value %s not allowed for mux select." % value)
            v = self._func_sel_dict[value]
            setattr(self._cur.bitfields, self._field_name, v)
        else:
            raise AttributeError("Pin %u does not have a core MUX selector" % self.pio_no)

    def report(self, report=False):
        """
        Reports the core pad mux settings of the current pin.
        """
        output = interface.Code(self.get_mux_sel())
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class KAPadMux(CorePadMux):
    """
    KA domain pad Mux abstraction class.
    """
    def __init__(self, curator, pio_no, name):
        super(KAPadMux, self).__init__(curator, pio_no, name,
            func_sel_enum="IO_KA_PAD_FUNCTION_SELECT_ENUM", func_sel_prefix="IO_KA_SEL_")

class PorPadMux(CorePadMux):
    """
    POR pad Mux abstraction class.
    """
    def __init__(self, curator, pio_no, name):
        super(PorPadMux, self).__init__(curator, pio_no, name,
            func_sel_enum="IO_POR_PAD_FUNCTION_SELECT_ENUM", func_sel_prefix="IO_POR_SEL_")

class LedPadMux(CorePadMux):
    """
    LED pad Mux abstraction class.

    """
    def __init__(self, curator, pio_no, name):
        self._led_config_field_name = None
        self._led_config_func_sel_dict = {}
        super(LedPadMux, self).__init__(curator, pio_no, name)

        for n in name.split(","):
            led = re.search("(LED[0-9]*)", n).group(1)
            field_name = "PMU_" + led + "_CONFIG"
            if hasattr(self._cur.bitfields, field_name):
                self._led_config_field_name = field_name
                break;

        vfr = self._cur.core.info.io_map_info.virtual_field_records
        func_sel_enum = "PMU_LED_CONFIG_ENUM"
        func_sel_prefix = "PMU_LED_CONFIG_"
        for k,v in vfr[func_sel_enum].__dict__.items():
            k = k.split(func_sel_prefix)[1]
            self._led_config_func_sel_dict[k] = v.value
        self._led_config_func_sel_dict_reverse = {v:k for k,v in self._led_config_func_sel_dict.items()}

    def get_led_owner(self):
        v = int(getattr(self._cur.bitfields, self._led_config_field_name))
        return self._led_config_func_sel_dict_reverse[v]

    def set_led_owner(self, value):
        if value not in self._led_config_func_sel_dict:
            raise ValueError("Value %s not allowed for LED config select." % value)
        v = self._led_config_func_sel_dict[value]
        setattr(self._cur.bitfields, self._led_config_field_name, v)

    def report(self, report=False):
        """
        Reports the core pad mux settings of the current pin.
        """
        t = [self.get_led_owner()]
        if self._field_name is not None:
            t.append(super(LedPadMux, self).get_mux_sel())
        output = interface.Code("/".join(t))
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class CorePadControl(object):
    def __init__(self, curator, pio_no, pad_name):
        self._cur = curator
        self.pio_no = pio_no

        self.pad_name = ""

        s = pad_name.split(",")
        # If there are multiple names for the pin, work out which one to use
        # First search for a PAD CONTROL register
        for n in s:
            reg_name = "PAD_CONTROL_" + n.strip()
            if hasattr(self._cur.fields, reg_name):
                self.pad_name = n.strip()
                break
        # The pad may not have a control register
        # Try other names
        if self.pad_name == "":
            if len(s) > 1:
                # Use alt name
                self.pad_name = s[1].strip()
            else:
                # Use PIOxx or only name
                self.pad_name = s[0].strip()

    def _get_pad_control_reg_name(self):
        reg_name = "PAD_CONTROL_%s" % self.pad_name
        return reg_name

    def _get_pad_control_field(self, field_name):
        reg_name = self._get_pad_control_reg_name()
        pad_control_reg = getattr(self._cur.fields, reg_name)
        field = getattr(pad_control_reg, "%s_%s" % (reg_name, field_name))
        return field

    def _get_bits(self, reg, posn, length=1):
        return (reg.read() >> posn) & ((1 << length) - 1)

    def _set_bits(self, reg, posn, v, length=1):
        mask = v << posn
        reg.write(reg.read() & (~(((1 << length) - 1) << posn)) | mask)

    def get_drive_strength(self):
        """
        Returns drive strength settings.
        Expect 2, 4, 8 or 12 as a result. These represent milliamps.
        """
        v = self._get_pad_control_field("DRIVE_STRENGTH").read()
        if v == 0:
            value = 2
        elif v == 1:
            value = 4
        elif v == 2:
            value = 8
        elif v == 3:
            value = 12
        return value

    def set_drive_strength(self, value):
        """
        Sets drive strength settings.
        Please provide 2, 4, 8 or 12 as a value. These represent milliamps.
        """
        if value == 2:
            v = 0
        elif value == 4:
            v = 1
        elif value == 8:
            v = 2
        elif value == 12:
            v = 3
        else:
            raise ValueError("Invalid value %s for drive strength." % value)
        self._get_pad_control_field("DRIVE_STRENGTH").write(v)

    def get_slew_enable(self):
        """
        Returns the slew enable settings.
        Expect True or False as a result.
        """
        v = self._get_pad_control_field("SLEW_ENB").read()
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_slew_enable(self, value):
        """
        Sets the slew enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for slew enable." % value)
        self._get_pad_control_field("SLEW_ENB").write(v)

    def get_pull_enable(self):
        """
        Returns the pull enable settings.
        Expect True or False as a result.
        """
        v = self._get_pad_control_field("PULL_ENABLE").read()
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_pull_enable(self, value):
        """
        Sets the pull enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for pull enable." % value)
        self._get_pad_control_field("PULL_ENABLE").write(v)

    def get_pull_strength(self):
        """
        Returns the pull strength settings.
        Expect weak or strong as a result.
        """
        v = self._get_pad_control_field("PULL_STRENGTH").read()
        if v == 0:
            value = "weak"
        else:
            value = "strong"
        return value

    def set_pull_strength(self, value):
        """
        Sets the pull strength settings.
        Please provide weak or strong as a value.
        """
        if value == "weak":
            v = 0
        elif value == "strong":
            v = 1
        else:
            raise ValueError("Invalid value %s for pull strength." % value)
        self._get_pad_control_field("PULL_STRENGTH").write(v)

    def get_pull_direction(self):
        """
        Returns the pull direction settings.
        Expect down or up as a result.
        """
        v = self._get_pad_control_field("PULL_DIRECTION").read()
        if v == 0:
            value = "down"
        else:
            value = "up"
        return value

    def set_pull_direction(self, value):
        """
        Sets the pull direction settings.
        Please provide down or up as a value.
        """
        if value == "down":
            v = 0
        elif value == "up":
            v = 1
        else:
            raise ValueError("Invalid value %s for pull direction." % value)
        self._get_pad_control_field("PULL_DIRECTION").write(v)

    def get_sticky_enable(self):
        """
        Returns the sticky enable settings.
        Expect True or False as a result.
        """
        v = self._get_pad_control_field("STICKY_ENABLE").read()
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_sticky_enable(self, value):
        """
        Sets the sticky enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for sticky enable." % value)
        self._get_pad_control_field("STICKY_ENABLE").write(v)

    @property
    def _pad_type_full(self):
        """
        Return the full type of this pad including any extra human-readable
        decorations. Unlike pad_type() this may contain spaces.
        """
        return self.pad_type

    def _slew_report_text(self):
        """
        The piece of text used to report the slew settings for this pad.
        Unlike get_slew_enable() a pad that doesn't support slew will
        return the string "N/A" rather than throwing an error.

        The result of this call is human readable text. Do not parse it.
        If you want the underlying values then go to get_slew_enable().
        """
        try:
            if self.get_slew_enable():
                return "on"
            else:
                return "off"
        except AttributeError:
            return "N/A"

    def report(self, report=False):
        """
        Reports the core pad control settings of the current pin.

        This contains the type of pad, drive strength, slew enable and
        pull enable, direction, strength and sticky settings.
        """
        output = interface.ListLine()

        output.add("Type", self._pad_type_full, "")

        try:
            output.add("Drive str", "%smA" % self.get_drive_strength(),
                       "drv str")
        except AttributeError:
            try:
                # Reasons are self-describing
                dsa_reason = self._drive_strength_absence_reason()
                output.add("Drive str", "N/A(%s)" % dsa_reason, "", dsa_reason)
            except AttributeError:
                output.add("Drive str", "N/A", "drv str")

        output.add("Slew", self._slew_report_text(), "slew(")

        pull_report = ""
        try:
            if self.get_pull_enable():
                pull_report += "enable"
            else:
                pull_report += "disable"
        except AttributeError:
            pass
        try:
            pull_report += ", %s" % self.get_pull_direction()
        except AttributeError:
            pass
        try:
            pull_report += ", %s" % self.get_pull_strength()
        except AttributeError:
            pass
        try:
            if self.get_sticky_enable():
                pull_report += ", sticky"
            else:
                pull_report += ", fixed"
        except AttributeError:
            pass
        if pull_report == "":
            pull_report = "N/A"
        output.add("Pull", pull_report, "pull(")

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class StandardPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "std"
        super(StandardPad, self).__init__(curator, pio_no, pad_name)

class PwmPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "pwm"
        channel = re.search("PWM_[NP]([0-9]*)", pad_name).group(1)
        self.channel = int(channel, 10)
        super(PwmPad, self).__init__(curator, pio_no, pad_name)

    def get_slew_rate(self):
        """
        Returns the slew rate settings.
        Expect 0, 1, 2 or 3 as a result.
        """
        length = 2
        posn = self.channel * length
        value = self._get_bits(self._cur.fields.AUDIO_PWM_SLEW_RATES, posn, length=length)
        return value

    def set_slew_rate(self, value):
        """
        Sets the slew rate settings.
        Please provide 0, 1, 2 or 3 as a value.
        """
        length = 2
        posn = self.channel * length
        if value not in [0, 1, 2, 3]:
            raise ValueError("Invalid value %s for slew rate." % value)
        self._set_bits(self._cur.fields.AUDIO_PWM_SLEW_RATES, posn, value, length=length)

    def _slew_report_text(self):
        """
        The piece of text used to report the slew settings for this pad.
        This combines the information from several calls including handling
        expected exceptions and returning "N/A".

        The result of this call is human readable text. Do not parse it.
        If you want the underlying values then go to the underlying calls.
        """
        return "%s, rate %d" % (super(PwmPad, self)._slew_report_text(),
                                self.get_slew_rate())

class XioPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "xio"
        xio_num = re.search("XIO([0-9]*)", pad_name).group(1)
        self.xio_no = int(xio_num, 10)
        super(XioPad, self).__init__(curator, pio_no, pad_name)

    @property
    def _drive_strength(self):
        bank = self.xio_no // 8
        reg_name = "XIO_DRIVE_STRENGTH_%02d_TO_%02d" % (bank * 8, (bank * 8) + 7)
        reg = getattr(self._cur.fields, reg_name)
        field_name = "%s_DRIVE_STRENGTH" % self.pad_name
        field = getattr(reg, field_name)
        return field

    def _get_md_sel_md_sel_pull(self):
        saved_xio_no = self._cur.fields.XIO_BOOT_CONTROL.XIO_BOOT_CONTROL_XIO_STATUS_SELECT.read()
        self._cur.fields.XIO_BOOT_CONTROL.XIO_BOOT_CONTROL_XIO_STATUS_SELECT.write(self.xio_no)
        md_sel = self._cur.fields.XIO_ALL_STATUS.XIO_ALL_STATUS_MD_SEL_FB.read()
        md_sel_pull = self._cur.fields.XIO_ALL_STATUS.XIO_ALL_STATUS_MD_SEL_PULL_FB.read()
        self._cur.fields.XIO_BOOT_CONTROL.XIO_BOOT_CONTROL_XIO_STATUS_SELECT.write(saved_xio_no)
        return md_sel, md_sel_pull

    def get_drive_strength(self):
        """
        Returns drive strength settings.
        Expect 0.5, 2, 4, or 8 as a result. These represent milliamps.
        Note: bits[1:0] on MD_SEL
        """

        v = self._drive_strength.read()
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_10 = md_sel & 0b11
        if v != md_sel_10:
            iprint ("Discrepancy between XIO %d drive strength "
                   "register (%d) and MD_SEL[1:0] (%d)." % (self.xio_no, v, md_sel_10))
        if v == 0:
            value = 0.5
        elif v == 1:
            value = 2
        elif v == 2:
            value = 4
        elif v == 3:
            value = 8
        return value

    def set_drive_strength(self, value):
        """
        Sets drive strength settings.
        Provide 0.5, 2, 4, or 8 as value. These represent milliamps. Use 4 for
        capacitive sensor pins.
        Note: bits[1:0] on MD_SEL
        """
        if value == 0.5:
            v = 0
        elif value == 2:
            v = 1
        elif value == 4:
            v = 2
        elif value == 8:
            v = 3
        else:
            raise ValueError("Invalid value %s for XIO drive strength." % value)
        self._drive_strength.write(v)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_10 = md_sel & 0b11
        if v != md_sel_10:
            iprint ("Discrepancy between XIO %d drive strength "
                   "register (%d) and MD_SEL[1:0] (%d)." % (self.xio_no, v, md_sel_10))

    def get_pull_enable(self):
        """
        Returns the pull enable settings.
        Expect True or False as a result.
        """
        v = self._get_bits(self._cur.fields.XIO_PULL_ENABLE, self.xio_no)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_pull_2 = (md_sel_pull & 0b100) >> 2
        if v != md_sel_pull_2:
            iprint ("Discrepancy between XIO %d pull enable "
                   "register (%d) and MD_SEL_PULL[2] (%d)." % (self.xio_no, v, md_sel_pull_2))
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_pull_enable(self, value):
        """
        Sets the pull enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for pull enable." % value)
        self._set_bits(self._cur.fields.XIO_PULL_ENABLE, self.xio_no, v)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_pull_2 = (md_sel_pull & 0b100) >> 2
        if v != md_sel_pull_2:
            iprint ("Discrepancy between XIO %d pull enable "
                   "register (%d) and MD_SEL_PULL[2] (%d)." % (self.xio_no, v, md_sel_pull_2))

    def get_pull_strength(self):
        """
        Returns the pull strength settings.
        Expect weak or strong as a result.
        """
        v = self._get_bits(self._cur.fields.XIO_PULL_STRONG, self.xio_no)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_pull_1 = (md_sel_pull & 0b10) >> 1
        if v != md_sel_pull_1:
            iprint ("Discrepancy between XIO %d pull strength "
                   "register (%d) and MD_SEL_PULL[1] (%d)." % (self.xio_no, v, md_sel_pull_1))
        if v == 0:
            value = "weak"
        else:
            value = "strong"
        return value

    def set_pull_strength(self, value):
        """
        Sets the pull strength settings.
        Please provide weak or strong as a value.
        """
        if value == "weak":
            v = 0
        elif value == "strong":
            v = 1
        else:
            raise ValueError("Invalid value %s for pull strength." % value)
        self._set_bits(self._cur.fields.XIO_PULL_STRONG, self.xio_no, v)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_pull_1 = (md_sel_pull & 0b10) >> 1
        if v != md_sel_pull_1:
            iprint ("Discrepancy between XIO %d pull strength "
                   "register (%d) and MD_SEL_PULL[1] (%d)." % (self.xio_no, v, md_sel_pull_1))

    def get_pull_direction(self):
        """
        Returns the pull direction settings.
        Expect down or up as a result.
        """
        v = self._get_bits(self._cur.fields.XIO_PULL_UP_NOT_DOWN, self.xio_no)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_pull_0 = (md_sel_pull & 0b1)
        if v != md_sel_pull_0:
            iprint ("Discrepancy between XIO %d pull direction "
                   "register (%d) and MD_SEL_PULL[0] (%d)." % (self.xio_no, v, md_sel_pull_0))
        if v == 0:
            value = "down"
        else:
            value = "up"
        return value

    def set_pull_direction(self, value):
        """
        Sets the pull direction settings.
        Please provide down or up as a value.
        """
        if value == "down":
            v = 0
        elif value == "up":
            v = 1
        else:
            raise ValueError("Invalid value %s for pull direction." % value)
        self._set_bits(self._cur.fields.XIO_PULL_UP_NOT_DOWN, self.xio_no, v)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_pull_0 = (md_sel_pull & 0b1)
        if v != md_sel_pull_0:
            iprint ("Discrepancy between XIO %d pull direction "
                   "register (%d) and MD_SEL_PULL[0] (%d)." % (self.xio_no, v, md_sel_pull_0))

    def get_extra_func(self):
        """
        Returns the XIO extra function settings.
        Expect True or False as a result.
        """
        v = self._get_bits(self._cur.fields.XIO_EXTRA_FUNCTION_SELECT, self.xio_no)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_3 = (md_sel & 0b1000) >> 3
        if v != md_sel_3:
            iprint ("Discrepancy between XIO %d extra function (%d) and "
                   "MD_SEL[3] (%d)." % (self.xio_no, v, md_sel_3))
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_extra_func(self, value):
        """
        Sets the XIO extra function settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for extra function." % value)
        self._set_bits(self._cur.fields.XIO_EXTRA_FUNCTION_SELECT, self.xio_no, v)
        md_sel, md_sel_pull = self._get_md_sel_md_sel_pull()
        md_sel_3 = (md_sel & 0b1000) >> 3
        if v != md_sel_3:
            iprint ("Discrepancy between XIO %d extra function (%d) and "
                   "MD_SEL[3] (%d)." % (self.xio_no, v, md_sel_3))

    def get_slew_enable(self):
        raise AttributeError("XIO pads do not have slew capabilities")

    def set_slew_enable(self, value):
        raise AttributeError("XIO pads do not have slew capabilities")

    def get_sticky_enable(self):
        raise AttributeError("XIO pads do not have sticky pulls")

    def set_sticky_enable(self, value):
        raise AttributeError("XIO pads do not have sticky pulls")

    def report(self, report=False):
        """
        Reports the core pad control settings of the current pin.

        This contains the type of pad, drive strength, pull enable, direction
        and strength, mode select value and decoding. The summary contains just
        a few wordsin the mode select decoding, if you need more info please
        see core_pad_control.report().
        """
        def bitlist2int(l):
            ret = 0
            for i in range(len(l)):
                ret += l[i]<<i
            return ret

        output = super(XioPad, self).report(report=True)

        md_sel_dec, md_sel_pull = self._get_md_sel_md_sel_pull()
        output.add("MD_SEL", format(md_sel_dec,"#07b"))

        out_driver_en = "no"
        drive_level = "z"
        drive_strength = "z"
        pad_ai_lcd_m1 = "no"
        pad_ai_lcd_m2 = "no"
        single_ended = "no"
        schmitt_trigger = "no"
        sel_ana_in_enable = "no"
        md_sel = [(md_sel_dec >> bit) & 0x01 for bit in range(5)]
        if md_sel[4]:
            if md_sel[3]:
                if md_sel[2]:
                    pad_ai_lcd_m1 = "yes"
                else:
                    pad_ai_lcd_m2 = "yes"
            else:
                out_driver_en = "yes"
                if md_sel[2]:
                    drive_level = "VDD"
                else:
                    drive_level = "VSS"
                drive_strength = {0:"0.5", 1:"2", 2:"4", 3:"8"}[(md_sel[1] << 1) | md_sel[0]]
        else:
            if md_sel[3]:
                if md_sel[2]:
                    if md_sel[1]:
                        single_ended = "yes"
                    else:
                        schmitt_trigger = "yes"
                else:
                    if md_sel[1]:
                        if md_sel[0]:
                            pass
                        else:
                            sel_ana_in_enable = "yes"
                    else:
                        pass
            else:
                schmitt_trigger = "yes"

        output.add_table("Out drv en", out_driver_en)
        output.add_table("N/PMOS drv lvl", drive_level)
        output.add_table("Drv str (mA)", drive_strength)
        output.add_table("PAD_AI_LCD_M1", pad_ai_lcd_m1,)
        output.add_table("PAD_AI_LCD_M2", pad_ai_lcd_m2)
        output.add_table("Single ended", single_ended)
        output.add_table("Schmitt trig", schmitt_trigger)
        output.add_table("SEL_ANA_IN_ENABLE", sel_ana_in_enable)

        if (bitlist2int(md_sel[3:4+1]) == 0b00 or
            bitlist2int(md_sel[1:4+1]) == 0b0110):
            #BOE = Boot On Edge
            output.add_summary_text("(dig in | BOE)")
        elif bitlist2int(md_sel[1:4+1]) == 0b0111:
            output.add_summary_text("(sg end: boot > 800mV)")
        elif bitlist2int(md_sel[0:4+1]) == 0b01010:
            output.add_summary_text("(amux)")
        elif bitlist2int(md_sel[0:4+1]) in [0b01011, 0b01001, 0b01000]:
            output.add_summary_text("(NC hi-Z)")
        elif bitlist2int(md_sel[2:4+1]) == 0b100:
            output.add_summary_text("(dig out lo %smA)" % drive_strength)
        elif bitlist2int(md_sel[2:4+1]) == 0b101:
            output.add_summary_text("(dig out hi %smA)" % drive_strength)
        elif bitlist2int(md_sel[2:4+1]) == 0b110:
            output.add_summary_text("(M2 LCD)")
        elif bitlist2int(md_sel[2:4+1]) == 0b111:
            output.add_summary_text("(M1 LCD)")
        else:
            output.add_summary_text("(invalid config)")

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class LedPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "led"
        self.led_no = int(pad_name[6:], 10)
        super(LedPad, self).__init__(curator, pio_no, pad_name)

    def get_pull_enable(self):
        """
        Returns the pull enable settings.
        Expect True or False as a result.
        """
        v = self._get_bits(self._cur.fields.PMU_CTRL7.PMU_DA_KA_LED_ENPULL,
                           self.led_no)
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_pull_enable(self, value):
        """
        Sets the pull enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for pull enable." % value)
        self._set_bits(self._cur.fields.PMU_CTRL7.PMU_DA_KA_LED_ENPULL,
                       self.led_no,
                       v)

    def get_pull_direction(self):
        """
        Returns the pull direction settings.
        Always expect down as a result since LED pads only have pull down
        resistors.
        """
        return "down"

    def set_pull_direction(self, value):
        """
        Sets the pull direction settings.
        Please only provide down as a value since LED pads only have pull down
        resistors.
        """
        if value != "down":
            raise ValueError("Invalid value %s for pull direction." % value)

    def get_pull_strength(self):
        """
        Returns the pull strength settings.
        Always expect strong as a result since LED pads only have strong pulls.
        """
        return "strong"

    def set_pull_strength(self, value):
        """
        Sets the pull strength settings.
        Please provide weak or strong as a value.
        """
        if value != "strong":
            raise AttributeError("Invalid value %s for pull strength." % value)

    def get_drive_strength(self):
        raise AttributeError("LED pads do not have drive strength controls")

    def set_drive_strength(self, value):
        raise AttributeError("LED pads do not have drive strength controls")

    def get_slew_enable(self):
        raise AttributeError("LED pads do not have slew controls")

    def set_slew_enable(self, value):
        raise AttributeError("LED pads do not have slew controls")

    def get_sticky_enable(self):
        raise AttributeError("LED pads do not have sticky pulls")

    def set_sticky_enable(self, value):
        raise AttributeError("LED pads do not have sticky pulls")

    def get_amux_enable(self):
        """
        Returns the amux enable settings.
        Expect True or False as a result.
        """
        v = self._get_bits(self._cur.fields.PMU_CTRL6.PMU_DA_KA_LED_AMUX_EN,
                           self.led_no)
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_amux_enable(self, value):
        """
        Sets the amux enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for amux enable." % value)
        self._set_bits(self._cur.fields.PMU_CTRL6.PMU_DA_KA_LED_AMUX_EN,
                       self.led_no,
                       v)

    def get_invi(self):
        """
        Returns the invi settings.
        Expect True or False as a result.
        """
        v = self._get_bits(self._cur.fields.PMU_CTRL6.PMU_DA_KA_LED_INVI,
                           self.led_no)
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_invi(self, value):
        """
        Sets the invi settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for invi enable." % value)
        self._set_bits(self._cur.fields.PMU_CTRL6.PMU_DA_KA_LED_INVI,
                       self.led_no,
                       v)

    def get_invc(self):
        """
        Returns the invc settings.
        Expect True or False as a result.
        """
        v = self._get_bits(self._cur.fields.PMU_CTRL7.PMU_DA_KA_LED_INVC,
                           self.led_no)
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_invc(self, value):
        """
        Sets the invc settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for invc enable." % value)
        self._set_bits(self._cur.fields.PMU_CTRL7.PMU_DA_KA_LED_INVC,
                       self.led_no,
                       v)

    def _drive_strength_absence_reason(self):
        """
        A piece of text to give the reason why we can't report a drive
        strength.

        In this case we return "OD" for Open Drain.

        The result of this call is human readable text. Do not parse it.
        Extend this class hierarchy with the information you want.
        """
        return "OD"

    def report(self, report=False):
        """
        Reports the core pad control settings of the current pin.

        This contains the type of pad, drive strength (in this case we have an
        open drain output), pull enable, direction and strength, analogue mux
        enable (this connects the pin to an analogue bus line), I and C
        controls invert enable.

        I and C controls do not affect the output value of the pin.
        C control inverts(1) or not(0) the input value just before it exits the
        pad.
        I control allows the input value to be read when drive enable is
        False(0) or True(1). Otherwise it reads 0.
        In other words:
        in = (((NOT drv_en) XOR invI) AND pin) XOR invC
        where pin is the logic level of the pin outside the chip.
        """
        output = super(LedPad, self).report(report=True)

        headings = ["Amux"]
        if self.get_amux_enable():
            output.add("Amux", "enable", "amux", "ena")
        else:
            output.add("Amux", "disable", "amux", "dis")

        headings += ["Controls"]
        if self.get_invi():
            controls = "invi"
        else:
            controls = "ninvi"
        if self.get_invc():
            controls += ", invc"
        else:
            controls += ", ninvc"

        output.add("Controls", controls, "")

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class XtalPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "xtal"
        super(XtalPad, self).__init__(curator, pio_no, pad_name)

    def get_drive_strength(self):
        """
        Returns drive strength settings.
        Expect 2, 4, 8 or 12 as a result. These represent milliamps.
        """
        v = self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_DRIVE_STRENGTH.read()
        if v == 0:
            value = 2
        elif v == 1:
            value = 4
        elif v == 2:
            value = 8
        elif v == 3:
            value = 12
        return value

    def set_drive_strength(self, value):
        """
        Sets drive strength settings.
        Please provide 2, 4, 8 or 12 as a value. These represent milliamps.
        """
        if value == 2:
            v = 0
        elif value == 4:
            v = 1
        elif value == 8:
            v = 2
        elif value == 12:
            v = 3
        else:
            raise ValueError("Invalid value %s for drive strength." % value)
        self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_DRIVE_STRENGTH.write(v)

    def get_slew_enable(self):
        """
        Returns the slew enable settings.
        Expect True or False as a result.
        """
        v = self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_SLEW_EN_B.read()
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_slew_enable(self, value):
        """
        Sets the slew enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for slew enable." % value)
        self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_SLEW_EN_B.write(v)

    def get_pull_enable(self):
        """
        Returns the pull enable settings.
        Expect True or False as a result.
        """
        v = self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_EN_PULL.read()
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_pull_enable(self, value):
        """
        Sets the pull enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for pull enable." % value)
        self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_EN_PULL.write(v)

    def get_pull_strength(self):
        """
        Returns the pull strength settings.
        Expect weak or strong as a result.
        """
        v = self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_PULL_STRENGTH.read()
        if v == 0:
            value = "weak"
        else:
            value = "strong"
        return value

    def set_pull_strength(self, value):
        """
        Sets the pull strength settings.
        Please provide weak or strong as a value.
        """
        if value == "weak":
            v = 0
        elif value == "strong":
            v = 1
        else:
            raise ValueError("Invalid value %s for pull strength." % value)
        self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_PULL_STRENGTH.write(v)

    def get_pull_direction(self):
        """
        Returns the pull direction settings.
        Expect down or up as a result.
        """
        v = self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_PULL_UP.read()
        if v == 0:
            value = "down"
        else:
            value = "up"
        return value

    def set_pull_direction(self, value):
        """
        Sets the pull direction settings.
        Please provide down or up as a value.
        """
        if value == "down":
            v = 0
        elif value == "up":
            v = 1
        else:
            raise ValueError("Invalid value %s for pull direction." % value)
        self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_PULL_UP.write(v)

    def get_sticky_enable(self):
        """
        Returns the sticky enable settings.
        Expect True or False as a result.
        """
        v = self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_EN_STICKY.read()
        if v == 0:
            value = False
        else:
            value = True
        return value

    def set_sticky_enable(self, value):
        """
        Sets the sticky enable settings.
        Please provide True or False as a value.
        """
        if value == False:
            v = 0
        elif value == True:
            v = 1
        else:
            raise ValueError("Invalid value %s for sticky enable." % value)
        self._cur.fields.AUX_ANA_XTAL_CTRL0.AUX_ANA_XTAL_DRV_EN_STICKY.write(v)

class SysCtrlPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "sys_ctrl"
        super(SysCtrlPad, self).__init__(curator, pio_no, pad_name)

    def get_drive_strength(self):
        raise AttributeError("SYS_CTRL pads do not have drive strength controls")

    def set_drive_strength(self, value):
        raise AttributeError("SYS_CTRL pads do not have drive strength controls")

    def get_pull_direction(self):
        raise AttributeError("SYS_CTRL pads do not have pull direction controls")

    def set_pull_direction(self, value):
        raise AttributeError("SYS_CTRL pads do not have pull direction controls")

    def get_pull_enable(self):
        raise AttributeError("SYS_CTRL pads do not have pull enable controls")

    def set_pull_enable(self, value):
        raise AttributeError("SYS_CTRL pads do not have pull enable controls")

    def get_pull_strength(self):
        raise AttributeError("SYS_CTRL pads do not have pull strength controls")

    def set_pull_strength(self, value):
        raise AttributeError("SYS_CTRL pads do not have pull strength controls")

    def get_slew_enable(self):
        raise AttributeError("SYS_CTRL pads do not have slew capabilities")

    def set_slew_enable(self, value):
        raise AttributeError("SYS_CTRL pads do not have slew capabilities")

    def get_sticky_enable(self):
        raise AttributeError("SYS_CTRL pads do not have sticky pulls")

    def set_sticky_enable(self, value):
        raise AttributeError("SYS_CTRL pads do not have sticky pulls")

class UsbPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "usb"
        if "HST" in pad_name:
            self.host = True
            self.device = False
        else:
            self.host = False
            self.device = True
        if "DP" in pad_name:
            self.dp = True
            self.dm = False
        else:
            self.dp = False
            self.dm = True
        super(UsbPad, self).__init__(curator, pio_no, pad_name)

    @property
    def _reg(self):
        try:
            self.__reg
        except AttributeError:
            if self.host:
                self.__reg = self._cur.fields.USB_CHARGING
            else:
                self.__reg = self._cur.fields.USB2_FS_PAD_CTRL
        return self.__reg

    def _get_pad_control_field(self, field):
        if self.host:
            field = "USB_HOST_%s" % field
        else:
            field = "USB2_FS_%s" % field

        return getattr(self._reg, field)

    def get_pad_enable(self):
        """
        Returns the power down settings.
        Expect True or False as a result.
        """
        v = self._get_pad_control_field("PAD_CTRL_PWR_DOWN_EN").read()
        if v == 0:
            value = True
        else:
            value = False
        return value

    def set_pad_enable(self, value):
        """
        Sets the power down settings.
        Please provide True or False as a result.
        """
        if value == True:
            v = 0
        elif value == False:
            v = 1
        else:
            raise ValueError("Invalid value %s for pad enable." % value)
        self._get_pad_control_field("PAD_CTRL_PWR_DOWN_EN").write(v)

    def get_pull_direction(self):
        """
        Returns the pull direction settings.
        Expect down or up as a result.
        """
        if self.host:
            return "down"
        else:
            raise AttributeError("USB DEV pads do not have pull direction controls")

    def set_pull_direction(self, value):
        """
        Sets the pull direction settings.
        Please provide down or up as a value.
        """
        if self.host:
            if value != "down":
                raise ValueError("Invalid value %s for pull direction." % value)
        else:
            raise AttributeError("USB DEV pads do not have pull direction controls")

    def get_pull_enable(self):
        """
        Returns the pull enable settings.
        Expect True or False as a result.
        """
        if self.host:
            v = self._get_pad_control_field("PULLDOWN_DISABLE").read()
            if v == 0:
                value = True
            else:
                value = False
            return value
        else:
            raise AttributeError("USB DEV pads do not have pull enable controls")

    def set_pull_enable(self, value):
        """
        Sets the pull enable settings.
        Please provide True or False as a value.
        """
        if self.host:
            if value == True:
                v = 0
            elif value == False:
                v = 1
            else:
                raise ValueError("Invalid value %s for pull enable." % value)
            self._get_pad_control_field("PULLDOWN_DISABLE").write(v)

    def get_d_pullup_enable(self):
        """
        Returns the pull enable settings for the very strong D+/- pull up.
        Expect True or False as a result.
        """
        if (self.dp and self.device or
            self.dm and self.host):
            v = self._get_pad_control_field("PAD_CTRL_PULLUP_EN").read()
            if v == 1:
                value = True
            else:
                value = False
            return value
        else:
            raise AttributeError("USB pads only have pullups on D+ for device and D- for host")

    def set_d_pullup_enable(self, value):
        """
        Sets the pull enable settings for the very strong D+/- pull up.
        Please provide True or False as a value.
        """
        if (self.dp and self.device or
            self.dm and self.host):
            if value == True:
                v = 1
            elif value == False:
                v = 0
            else:
                raise ValueError("Invalid value %s for D+ pullup enable." % value)
            self._get_pad_control_field("PAD_CTRL_PULLUP_EN").write(v)
        else:
            raise AttributeError("USB pads only have pullups on D+ for device and D- for host")

    def get_drive_strength(self):
        raise AttributeError("USB pads do not have drive strength controls")

    def set_drive_strength(self, value):
        raise AttributeError("USB pads do not have drive strength controls")

    def get_slew_enable(self):
        raise AttributeError("USB pads do not have slew capabilities")

    def set_slew_enable(self, value):
        raise AttributeError("USB pads do not have slew capabilities")

    def get_pull_strength(self):
        raise AttributeError("USB pads do not have pull strength controls")

    def set_pull_strength(self, value):
        raise AttributeError("USB pads do not have pull strength controls")

    def get_sticky_enable(self):
        raise AttributeError("USB pads do not have sticky pulls")

    def set_sticky_enable(self, value):
        raise AttributeError("USB pads do not have sticky pulls")

    @property
    def _pad_type_full(self):
        """
        Return the full type of this pad including any extra human-readable
        decorations. Unlike pad_type() this may contain spaces.
        """
        type = super(UsbPad, self)._pad_type_full + " "
        if self.host:
            type += "hst"
        else:
            type += "dev"
        return type

    def report(self, report=False):
        """
        Reports the core pad control settings of the current pin.

        This contains the type of pad, drive strength, pull enable and
        direction, pad enable/disable and D+/- pullup enable/disable.

        The D+/- pullup is a very strong 1K5 resistor.
        """
        output = super(UsbPad, self).report(report=True)

        if self.get_pad_enable():
            pad = "enable"
        else:
            pad = "disable"
        output.add("Pad", pad, "pad")

        try:
            if self.get_d_pullup_enable():
                pullup = "enable"
            else:
                pullup = "disable"
        except AttributeError:
            pullup = "N/A"
        output.add("D+/- pullup", pullup)

        if report:
            return output
        TextAdaptor(output, gstrm.iout)

class KAPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "KA"
        super(KAPad, self).__init__(curator, pio_no, pad_name)

class PORPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "POR"
        super(PORPad, self).__init__(curator, pio_no, pad_name)

class KAPORPad(CorePadControl):
    def __init__(self, curator, pio_no, pad_name):
        self.pad_type = "KA/POR"
        super(KAPORPad, self).__init__(curator, pio_no, pad_name)
