############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Adaptive ANC ACAT Analysis Module.

This module provides an ACAT analysis class and python object representation
for AANC operators.

Examples:
    Display information about the currently running AANC operator(s).

    >>> aanc.run_all()

    Get a list of currently running AANC operators.

    >>> aanc.find_operators()

"""
import argparse
import math
import struct

from ACAT.Analysis import Analysis
from ACAT.Analysis import Opmgr
from ACAT.Analysis import Buffers
from ACAT.Core import CoreUtils as cu
from ACAT.Core.Arch import addr_per_word
from ACAT.Core.exceptions import VariableNotPointerError

# Provide Q format conversion functions for different formats used in AANC
Q31 = cu.qformat_factory(1, 31)
Q30 = cu.qformat_factory(2, 30)
Q29 = cu.qformat_factory(3, 29)
Q27 = cu.qformat_factory(5, 27)
Q20 = cu.qformat_factory(12, 20)

AANC_V0102 = 0x00010002

class AANCFindOperatorError(Exception):
    """Failed to find an AANC operator in the graph."""


class AANC(Analysis.Analysis):
    """Encapsulates an analysis for adaptive ANC (AANC).

    This class provides methods to summarize the current AANC operators or find
    and return AANC objects associated with each of the operators currently
    running in kymera.

    Args:
        **kwargs: Arbitrary keyword arguments.

    """

    def __init__(self, **kwargs):
        """AANC Analysis __init__ method."""
        Analysis.Analysis.__init__(self, **kwargs)
        self._opmgr = Opmgr.Opmgr(**kwargs)
        self._buffers = Buffers.Buffers(**kwargs)

    def run_all(self):
        """Standard analysis function."""
        ops = self.find_operators()
        self.formatter.section_start("AANC")
        self.formatter.output("%d AANC operators found" % len(ops))

        for aanc in ops:
            aanc.display()

        self.formatter.section_end()

    def find_operators(self, opname='AANC'):
        """Identify AANC operators in the graph.

        Args:
            opname (str, optional): Operator name to filter for. Defaults to
                `AANC`.

        Returns:
            list(ACAT.AANC.AANCOperator): List of AANC operators.

        """
        ids = [int(name.split(' ')[-1], 16) for name in
               self._opmgr.get_oplist('name') if opname in name]

        return [AANCOperator(self._opmgr.get_operator(idv), self._buffers,
                             self.formatter) for idv in ids]


def rms_db_buffer(data):
    """Calculate the average of a buffer and return the RMS in dB.

    Args:
        data (list(int, float)): List of input data.

    Returns:
        float: decibel value of the input list.

    """
    sqr = [math.pow(value, 2) for value in data if value is not None]
    if not sqr:
        raise ValueError("RMS dB buffer error: no data")
    mean = sum(sqr) / float(len(sqr))
    if mean < 0:
        raise ValueError("RMS dB buffer error: mean < 0")

    nxt = math.sqrt(mean)
    if nxt <= 0:
        raise ValueError("RMS dB buffer error: sqrt <= 0")

    return 20 * math.log10(nxt)


class AANCOperator(object):
    """Encapsulates an adaptive ANC operator.

    Args:
        operator (ACAT.Opmgr.Operator): AANC Operator object.
        buffers (ACAT.Buffers.Buffers): ACAT Buffer Analysis object.
        formatter (ACAT.Display.Formatter, optional): ACAT Formatter.
            Defaults to `None`.
    """
    def __init__(self, operator, buffers, formatter=None):
        """AANC Operator __init__ method."""
        self._operator = operator
        self._buffers_handle = buffers
        self._formatter = formatter

        self._helper = operator.helper
        self.opid = self._operator.op_entry.id.value
        """int: Operator ID associated with this AANC instance."""

        self.version = self._operator.cap_data.version
        """int: Capability version (major, minor)."""

        self.obj = operator.extra_op_data
        """ACAT.Core.CoreTypes.ChipdataVariable: Extra operator data object."""
        self.agstruct = self.obj.ag.deref
        """ACAT.Core.CoreTypes.ChipdataVariable: Adaptive gain object."""

        if self.version < AANC_V0102:
            self.fxlms = FxLMS(self._helper,
                               self.agstruct.p_fxlms,
                               self.agstruct.p_fxlms_stats,
                               self.agstruct.p_fxlms_params,
                               self._operator.cap_data.elf_id,
                               formatter=self._formatter)
            """AANC.FxLMS: FxLMS object."""
        else:
            self.fxlms = FxLMSv0102(self._helper,
                                    self.agstruct.p_fxlms.deref,
                                    self._operator.cap_data.elf_id,
                                    formatter=self._formatter)
            """AANC.FxLMSv0102: FxLMS object."""

        self.eds = argparse.Namespace()
        """argparse.Namespace: Namespace for ED objects."""
        if self.version < AANC_V0102:
            self.eds.int = ED(self._helper,
                              self.agstruct.p_ed_int,
                              self.agstruct.p_ed_int_stats,
                              self.agstruct.p_ed_int_params,
                              self._operator.cap_data.elf_id,
                              formatter=self._formatter)
            """AANC.ED: Internal microphone ED object."""
            self.eds.ext = ED(self._helper,
                              self.agstruct.p_ed_ext,
                              self.agstruct.p_ed_ext_stats,
                              self.agstruct.p_ed_ext_params,
                              self._operator.cap_data.elf_id,
                              formatter=self._formatter)
            """AANC.ED: External microphone ED object."""
            self.eds.pb = ED(self._helper,
                             self.agstruct.p_ed_pb,
                             self.agstruct.p_ed_pb_stats,
                             self.agstruct.p_ed_pb_params,
                             self._operator.cap_data.elf_id,
                             formatter=self._formatter)
            """AANC.ED: Playback stream ED object."""
        else:
            self.eds.int = ED(self._helper,
                              self.agstruct.p_ed_int.deref,
                              None,
                              None,
                              self._operator.cap_data.elf_id,
                              formatter=self._formatter)
            """AANC.ED: Internal microphone ED object."""
            self.eds.ext = ED(self._helper,
                              self.agstruct.p_ed_ext.deref,
                              None,
                              None,
                              self._operator.cap_data.elf_id,
                              formatter=self._formatter)
            """AANC.ED: External microphone ED object."""
            self.eds.pb = ED(self._helper,
                             self.agstruct.p_ed_pb.deref,
                             None,
                             None,
                             self._operator.cap_data.elf_id,
                             formatter=self._formatter)
            """AANC.ED: Playback stream ED object."""

        self.buffers = argparse.Namespace()
        """argparse.Namespace: Namespace for buffer objects."""
        self.buffers.int_ip = AANCBuffer(self._buffers_handle,
                                         self.agstruct.p_tmp_int_ip,
                                         formatter=self._formatter)
        """AANC.AANCBuffer: Internal microphone input buffer object."""
        self.buffers.int_op = AANCBuffer(self._buffers_handle,
                                         self.agstruct.p_tmp_int_op,
                                         formatter=self._formatter)
        """AANC.AANCBuffer: Internal microphone output buffer object."""
        self.buffers.ext_ip = AANCBuffer(self._buffers_handle,
                                         self.agstruct.p_tmp_ext_ip,
                                         formatter=self._formatter)
        """AANC.AANCBuffer: External microphone input buffer object."""
        self.buffers.ext_op = AANCBuffer(self._buffers_handle,
                                         self.agstruct.p_tmp_ext_op,
                                         formatter=self._formatter)
        """AANC.AANCBuffer: External microphone output buffer object."""

        self.flags = AANCFlags(self.obj.flags.address,
                               self._helper.chipdata.get_data,
                               formatter=self._formatter)
        """AANC.AANCFlags: Flags associated with the AANC operator."""

        self._valid = False
        self.refresh()

    def refresh(self):
        """Refresh the cached data read in ACAT.

        Returns:
            True if successful, False otherwise

        """
        self._valid = False

        # Make sure the cached operator ID is still in the graph. If the graph
        # has changed then try to find the new operator ID & refresh the class.
        try:
            if self.opid not in self._helper.get_oplist():
                try:
                    opr = self.find_operator()
                    self.__init__(opr, self._buffers_handle)
                except AANCFindOperatorError:
                    return False

        # Try to deal with instances where the chip has been reset.
        # TODO: would be neater to be able to catch kal_python_tools exceptions.
        # B-301161 should allow the package to be imported and exception caught
        # properly.
        except Exception as exception: # pylint: disable=broad-except
            if 'ka_exceptions' in str(type(exception)):
                self._helper.chipdata.reconnect()
                self._helper.debuginfo.reload()
                return True
            raise exception

        # Refresh the different handles for the chip
        self._operator.cast_extra_data(self._operator.extra_op_data_type)
        self.obj = self._operator.extra_op_data
        self.fxlms.refresh()
        self.eds.int.refresh()
        self.eds.ext.refresh()
        self.eds.pb.refresh()
        self._valid = True
        return True

    def find_operator(self, op_name='AANC'):
        """Find an adaptive ANC operator in the graph.

        This can be used if the AANC object was initialized but the graph has
        changed. It follows the same approach as the analysis
        ``aanc.find_operator``, so looks for operators with a particular name.
        It will also try to match the channel that this AANC object was
        originally instantiated with to avoid ambiguity.

        Args:
            op_name (str, optional): Name of the operator to look for. Defaults
                to `AANC`.

        Raises:
            AANCFindOperatorError: if no operator was found

        """
        ids = [int(name.split(' ')[-1], 16) for name in
               self._helper.get_oplist('name') if op_name in name]

        channel = -1

        for idv in ids:
            opr = self._helper.get_operator(idv)
            channel = opr.extra_op_data.anc_channel.value - 1
            if channel == self.channel:
                return opr

        if channel >= 0:
            raise AANCFindOperatorError(
                "Can't find an operator matching %s with channel %d" % (
                    op_name, channel))

        raise AANCFindOperatorError(
            "Can't find an operator matching %s" % (op_name))


    @property
    def valid(self):
        """bool: Validity of the data in this object."""
        return self._valid

    @property
    def channel(self):
        """int: AANC channel for this object."""
        return self.obj.anc_channel.value - 1

    @property
    def disable_gain_calc(self):
        """bool: Operator gain calculation disable flag."""
        return self.obj.aanc_cap_params.OFFSET_DISABLE_AG_CALC.value

    @disable_gain_calc.setter
    def disable_gain_calc(self, value):
        self._helper.chipdata.set_data(
            self.obj.aanc_cap_params.OFFSET_DISABLE_AG_CALC.address, [value])

    @property
    def ff_fine_gain(self):
        """int: FF fine gain value written to ANC HW by the operator."""
        return self.obj.ff_gain.fine.value

    @property
    def ff_coarse_gain(self):
        """int: FF coarse gain value written to ANC HW by the operator."""
        value = self.obj.ff_gain.coarse.value
        return struct.unpack('h', struct.pack('H', value))[0]

    @property
    def fb_fine_gain(self):
        """int: FB fine gain value written to ANC HW by the operator."""
        return self.obj.fb_gain.fine.value

    @property
    def fb_coarse_gain(self):
        """int: FB coarse gain value written to ANC HW by the operator."""
        value = self.obj.fb_gain.coarse.value
        return struct.unpack('h', struct.pack('H', value))[0]

    @property
    def ec_fine_gain(self):
        """int: EC fine gain value written to ANC HW by the operator."""
        return self.obj.ec_gain.fine.value

    @property
    def ec_coarse_gain(self):
        """int: EC coarse gain value written to ANC HW by the operator."""
        value = self.obj.ec_gain.coarse.value
        return struct.unpack('h', struct.pack('H', value))[0]

    @property
    def gain_event_max(self):
        """int: Gain event counter reset value."""
        return self.obj.gain_event.set_frames.value

    @property
    def gain_event_count(self):
        """int: Gain event detection count value."""
        return self.obj.gain_event.frame_counter.value

    @property
    def ed_event_max(self):
        """int: ED event counter reset value."""
        return self.obj.ed_event.set_frames.value

    @property
    def ed_event_count(self):
        """int: ED event detection count value."""
        return self.obj.ed_event.frame_counter.value

    @property
    def quiet_event_detect_max(self):
        """int: Quiet event detection counter reset value."""
        return self.obj.quiet_event_detect.set_frames.value

    @property
    def quiet_event_detect_count(self):
        """int: Quiet event detection count value."""
        return self.obj.quiet_event_detect.frame_counter.value

    @property
    def quiet_event_clear_max(self):
        """int: Quiet event clear counter reset value."""
        return self.obj.quiet_event_clear.set_frames.value

    @property
    def quiet_event_clear_count(self):
        """int: Quiet event clear count value."""
        return self.obj.quiet_event_clear.frame_counter.value

    @property
    def clip_event_max(self):
        """int: Clip event counter reset value."""
        return self.obj.clip_event.set_frames.value

    @property
    def clip_event_count(self):
        """int: Clip event detection count value."""
        return self.obj.clip_event.frame_counter.value

    @property
    def sat_event_max(self):
        """int: Saturation event counter reset value."""
        return self.obj.sat_event.set_frames.value

    @property
    def sat_event_count(self):
        """int: Saturation event detection count value."""
        return self.obj.sat_event.frame_counter.value

    @property
    def self_talk_event_max(self):
        """int: Self-Talk event counter reset value."""
        return self.obj.self_talk_event.set_frames.value

    @property
    def self_talk_event_count(self):
        """int: Self-Talk event detection count value."""
        return self.obj.self_talk_event.frame_counter.value

    @property
    def spl_event_max(self):
        """int: SPL event counter reset value."""
        return self.obj.spl_event.set_frames.value

    @property
    def spl_event_count(self):
        """int: SPL event detection count value."""
        return self.obj.spl_event.frame_counter.value

    def display(self):
        """Display AANC capability data using the ACAT formatter."""
        if self._formatter is None:
            raise Warning("No formatter found.")

        self._formatter.section_start("AANC (Channel %d, 0x%08X)" % (
            self.channel, self.obj.address))

        self._formatter.section_start("ANC Gains")
        try:

            format_tbl = "|  Path  | Coarse | Fine |\n"
            format_tbl += "|========|========|======|\n"
            format_tbl += "|   FF   |   %02d   |  %03d |\n" % (
                self.ff_coarse_gain, self.ff_fine_gain)
            format_tbl += "|   FB   |   %02d   |  %03d |\n" % (
                self.fb_coarse_gain, self.fb_fine_gain)
            format_tbl += "|   EC   |   %02d   |  %03d |\n" % (
                self.ec_coarse_gain, self.ec_fine_gain)
            self._formatter.output_raw(format_tbl)
        except (ValueError, AttributeError) as err:
            self._formatter.output("ANC Gain analysis failed: %s" % err)
        self._formatter.section_end()

        self.flags.display()

        self.fxlms.display()

        self._formatter.section_start("EDs")
        license_state = (self.eds.int.licensed |
                         self.eds.ext.licensed |
                         self.eds.pb.licensed)
        self._formatter.output_raw("Licensed: %u\n" % license_state)
        format_tbl = "|   ED     |  Address   | Detected |              "
        format_tbl += "SPL (dB)             |\n"
        format_tbl += "|          |            |          |  Min   |  Mid"
        format_tbl += "   |  Max   |  Delta |\n"
        format_tbl += "|==========|============|==========|========|====="
        format_tbl += "===|========|========|\n"
        format_tbl += "| Internal | %s\n" % self.eds.int.display(True)
        format_tbl += "| External | %s\n" % self.eds.ext.display(True)
        format_tbl += "| Playback | %s\n" % self.eds.pb.display(True)
        self._formatter.output_raw(format_tbl)
        self._formatter.section_end()

        self._formatter.section_start("Buffers")
        format_tbl = "|     Buffer     |  Address  | Avg (dB) | Avg Samples |"
        format_tbl += " Moving Avg (dB) |\n"
        format_tbl += "|================|===========|==========|=============|"
        format_tbl += "=================|\n"
        format_tbl += "|Internal Input  |%s\n" % (
            self.buffers.int_ip.display(True))
        format_tbl += "|Internal Output |%s\n" % (
            self.buffers.int_op.display(True))
        format_tbl += "|External Input  |%s\n" % (
            self.buffers.ext_ip.display(True))
        format_tbl += "|External Output |%s\n" % (
            self.buffers.ext_op.display(True))
        self._formatter.output_raw(format_tbl)
        self._formatter.section_end()

        self._formatter.section_start("Events")
        format_tbl = "|   Event      | Count Value | Max Value |\n"
        format_tbl += "|==============|=========================|\n"
        format_tbl += "|    Gain      |    %04d     |   %04d    |\n" % (
            self.gain_event_count, self.gain_event_max)
        format_tbl += "|     ED       |    %04d     |   %04d    |\n" % (
            self.ed_event_count, self.ed_event_max)
        format_tbl += "| Quiet Detect |    %04d     |   %04d    |\n" % (
            self.quiet_event_detect_count, self.quiet_event_detect_max)
        format_tbl += "| Quiet Clear  |    %04d     |   %04d    |\n" % (
            self.quiet_event_clear_count, self.quiet_event_clear_max)
        format_tbl += "|  Clipping    |    %04d     |   %04d    |\n" % (
            self.clip_event_count, self.clip_event_max)
        format_tbl += "|  Saturation  |    %04d     |   %04d    |\n" % (
            self.sat_event_count, self.sat_event_max)
        format_tbl += "|  Self Talk   |    %04d     |   %04d    |\n" % (
            self.self_talk_event_count, self.self_talk_event_max)
        format_tbl += "|    SPL       |    %04d     |   %04d    |\n" % (
            self.spl_event_count, self.spl_event_max)
        self._formatter.output_raw(format_tbl)
        self._formatter.section_end()

        self._formatter.section_start("Capability Data")
        self._formatter.output(str(self.obj))
        self._formatter.output(str(self.obj.ag.deref))
        self._formatter.section_end()

        self._formatter.section_end()

    def __repr__(self):
        """str: String representation of the AANC operator."""
        return str(self)

    def __str__(self):
        """str: String representation of the AANC operator."""
        rstr = "AANC on channel %d (extra_op_data at 0x%08X)\n" % (
            self.channel, self.obj.address)
        rstr += "ANC Gains\n"
        rstr += "FF Gain (coarse, fine): %+02d, %03d\n" % (
            self.ff_coarse_gain, self.ff_fine_gain)
        rstr += "FB Gain (coarse, fine): %+02d, %03d\n" % (
            self.fb_coarse_gain, self.fb_fine_gain)
        rstr += "EC Gain (coarse, fine): %+02d, %03d\n" % (
            self.ec_coarse_gain, self.ec_fine_gain)
        return rstr


class AANCFlags(object):
    """Represent AANC flags.

    Args:
        addr (int): Address of the flags variable.
        read_func (function): Function to read the flags variable.
        formatter (ACAT.Display.Formatter, optional): ACAT formatter.
            Defaults to `None`.
    """

    CLIP_INT_MASK = 0X00000100
    """int: Mask for internal microphone clipping flag."""
    CLIP_EXT_MASK = 0X00000200
    """int: Mask for external microphone clipping flag."""
    CLIP_PB_MASK = 0x00000400
    """int: Mask for playback stream clipping flag."""

    ED_INT_MASK = 0X00000010
    """int: Mask for internal microphone ED detection flag."""
    ED_EXT_MASK = 0X00000020
    """int: Mask for external microphone ED detection flag."""
    ED_PB_MASK = 0X000000040
    """int: Mask for playback stream ED detection flag."""
    SELF_SPEECH_MASK = 0x00000080
    """int: Mask for self-speech detection flag."""

    SAT_INT_MASK = 0X00001000
    """int: Mask for internal microphone saturation flag."""
    SAT_EXT_MASK = 0X00002000
    """int: Mask for external microphone saturation flag."""
    SAT_MDL_MASK = 0X00008000
    """int: Mask for model output saturation flag."""

    QUIET_MODE_MASK = 0X00100000
    """int: Mask for quiet mode condition detection flag."""

    def __init__(self, addr, read_func, formatter=None):
        """AANC Flags __init__ method."""
        self._read = read_func
        self.addr = addr
        self._formatter = formatter

    @property
    def raw(self):
        """int: Raw flags reported by the operator."""
        return self._read(self.addr)

    @property
    def clip_int(self):
        """bool: Clipping detected on the internal microphone."""
        return (self.raw & self.CLIP_INT_MASK) > 0

    @property
    def clip_ext(self):
        """bool: Clipping detection on the external microphone."""
        return (self.raw & self.CLIP_EXT_MASK) > 0

    @property
    def clip_pb(self):
        """bool: Clipping detection on the playback stream."""
        return (self.raw & self.CLIP_PB_MASK) > 0

    @property
    def ed_int_event(self):
        """bool: ED detection on the internal microphone."""
        return (self.raw & self.ED_INT_MASK) > 0

    @property
    def ed_ext_event(self):
        """bool: ED detection on the external microphone."""
        return (self.raw & self.ED_EXT_MASK) > 0

    @property
    def ed_pb_event(self):
        """bool: ED detection on the playback stream."""
        return (self.raw & self.ED_PB_MASK) > 0

    @property
    def self_speech_event(self):
        """bool: self speech detection."""
        return (self.raw & self.SELF_SPEECH_MASK) > 0

    @property
    def sat_int(self):
        """bool: Saturation detection on the internal microphone."""
        return (self.raw & self.SAT_INT_MASK) > 0

    @property
    def sat_ext(self):
        """bool: Saturation detection on the external microphone."""
        return (self.raw & self.SAT_EXT_MASK) > 0

    @property
    def sat_model(self):
        """bool: Saturation detection on the control-plant model output."""
        return (self.raw & self.SAT_MDL_MASK) > 0

    @property
    def quiet_mode(self):
        """bool: Quiet mode condition detected flag."""
        return (self.raw & self.QUIET_MODE_MASK) > 0

    def display(self):
        """Display AANC flags using the ACAT formatter."""
        if self._formatter is None:
            raise Warning("No formatter found.")

        self._formatter.section_start("AANC Flags")
        self._formatter.output_raw("AANC Flags: 0x%08X" % self.raw)
        format_tbl = "|         Flags          | State |\n"
        format_tbl += "|========================|=======|\n"
        format_tbl += "| Internal Clipping      |   %d   |\n" % self.clip_int
        format_tbl += "| External Clipping      |   %d   |\n" % self.clip_ext
        format_tbl += "| Internal Saturation    |   %d   |\n" % self.sat_int
        format_tbl += "| External Saturation    |   %d   |\n" % self.sat_ext
        format_tbl += "| Model Saturation       |   %d   |\n" % self.sat_model
        format_tbl += "| Internal ED Detection  |   %d   |\n" % (
            self.ed_int_event)
        format_tbl += "| External ED Detection  |   %d   |\n" % (
            self.ed_ext_event)
        format_tbl += "| Playback ED Detection  |   %d   |\n" % (
            self.ed_pb_event)
        format_tbl += "| Self Speech Detection  |   %d   |\n" % (
            self.self_speech_event)
        format_tbl += "| Quiet Mode Condition   |   %d   |\n" % (
            self.quiet_mode)

        self._formatter.output_raw(format_tbl)
        self._formatter.section_end()

    def __repr__(self):
        """str: String representation of the AANC flags."""
        return str(self)

    def __str__(self):
        """str: String representation of the AANC flags."""
        rstr = '\n  '.join([
            "AANC Flags: 0x%08X" % self.raw,
            "Internal Clipping:     %d" % self.clip_int,
            "External Clipping:     %d" % self.clip_ext,
            "Internal Saturation:   %d" % self.sat_int,
            "External Saturation:   %d" % self.sat_ext,
            "Model Saturation:      %d" % self.sat_model,
            "Internal ED Detection: %d" % self.ed_int_event,
            "External ED Detection: %d" % self.ed_ext_event,
            "Playback ED Detection: %d" % self.ed_pb_event,
            "Self Speech Detection: %d" % self.self_speech_event,
            "Quiet Mode Condition:  %d" % self.quiet_mode])
        rstr += "\n"
        return rstr


class AANCBuffer(object):
    """Encapsulates an AANC buffer.

    Args:
        buffers (ACAT.Analysis.Buffers.Buffers): ACAT Buffers Analysis
            object.
        obj (ACAT.Core.CoreTypes.ChipdataVariable): Buffer object.
        filt_nsam (int, optional): Number of samples to average over for
            filtering. Defaults to `16`.
        formatter (ACAT.Display.Formatter, optional): ACAT formatter.
            Defaults to `None`.
    """

    def __init__(self, buffers, obj, filt_nsam=16, formatter=None):
        """AANC Buffer __init__ method."""
        self._buffers = buffers
        self.obj = obj.deref
        """ACAT.Core.CoreTypes.ChipdataVariable: Buffer object reference."""
        self.filt_nsam = filt_nsam
        """int: Number of samples to average over for filtering."""
        self._filt = [0] * int(self.filt_nsam)
        self._formatter = formatter

    @property
    def data(self):
        """list(int): Data in this buffer."""
        return self._buffers.get_content(self.obj.address,
                                         only_available_data=True,
                                         conversion_function=cu.u32_to_frac32)

    @property
    def avg(self):
        """float: Average value of the buffer in dB."""
        return rms_db_buffer(self.data)

    @property
    def filt(self):
        """float: Moving average filtered value of the buffer data."""
        # Preceed operating on the internal list so that exceptions are
        # escalated before the pop().
        value = self.avg

        self._filt.pop()
        self._filt.insert(0, value)

        filt = [elem for elem in self._filt if elem is not None]
        if filt:
            return sum(filt) / float(len(filt))

        raise Warning("Moving average filter operated on 0 elements")

    def display(self, line_only=False):
        """Display AANC buffer information using the ACAT formatter.

        Args:
            line_only (bool, optional): Return a single line table entry instead
                of using the formatter. Defaults to `False`.
        """
        if self._formatter is None:
            raise Warning("No formatter found.")

        if line_only:
            try:
                lsr = "0x%08X |  % 6.2f  |     %03d     |     % 6.2f      |" % (
                    self.obj.base_addr.value, self.avg, self.filt_nsam,
                    self.filt)
            except (ValueError, AttributeError) as err:
                lsr = "Buffer Analysis failed: %s\n" % err
            return lsr

        self._formatter.section_start("AANC Buffer at 0x%08X" %
                                      self.obj.base_addr.value)
        try:
            self._formatter.output("Avg: %.2f (dB)" % self.avg)
            self._formatter.output("Moving Avg (%d samples): %.2f (dB)" %
                                   (self.filt_nsam, self.filt))
        except (ValueError, AttributeError) as err:
            self._formatter.output("Buffer Analysis failed: %s" % err)
        self._formatter.section_end()
        return None

    def __repr__(self):
        """str: String representation of key buffer variables."""
        return str(self)

    def __str__(self):
        """str: String representation of key buffer variables."""
        rstr = "Buffer object at 0x%08X:\n" % self.obj.base_addr.value
        rstr += "  Avg: %.2f (dB)\n" % self.avg
        rstr += "  Moving Avg (%d samples): %.2f (dB)\n" % (self.filt_nsam,
                                                            self.filt)
        return rstr


class FxLMSFilter:
    """Encapsulates an FxLMS filter.

    Args:
        helper (ACAT.Analysis.Analysis): ACAT Analysis object.
        obj (ACAT.Core.CoreTypes.ChipdataVariable): FxLMS filter object.
        name (str): Name of the filter
        elf_id (int): ELF ID for AANC.
        conv (function, optional): Conversion function when displaying the
            filter coefficients. Defaults to `None`.
        formatter (ACAT.Display.Formatter, optional): ACAT formatter.
            Defaults to `None`.
    """
    def __init__(self, helper, obj, name, elf_id, conv=None, formatter=None):
        """AANC FxLMSv0102 __init__ method."""
        self._helper = helper
        self.obj = obj
        """ACAT.Core.CoreTypes.ChipdataVariable: FxLMS filter object."""
        self.name = name
        """str: Name of the filter."""
        self.conv = conv
        """function: Conversion function for displaying coefficients."""
        self.elf_id = elf_id
        """int: ELF ID for AANC."""
        self._formatter = formatter
        self.address = self.obj.address
        """int: Address of the FxLMS filter object on the chip."""

    def refresh(self):
        """Refresh the data in the filter."""
        self.obj = self._helper.chipdata.cast(
            self.address, 'FXLMS100_FILTER', elf_id=self.elf_id)

    @property
    def num_coeffs(self):
        """int: Number of active coefficients in the filter."""
        return self.obj.num_coeffs.value

    @property
    def full_num_coeffs(self):
        """int: Complete number of coefficients in the filter."""
        return self.obj.full_num_coeffs.value

    @property
    def nbytes(self):
        """int: Number of bytes to read for each list in the filter."""
        return self.full_num_coeffs * addr_per_word

    @property
    def numerator(self):
        """list(float): Numerator coefficients."""
        addr = self.obj.coeffs.p_num.value
        coeffs = self._helper.chipdata.get_data(addr, self.nbytes)
        if self.conv is not None:
            coeffs = [self.conv(coeff) for coeff in coeffs]
        return coeffs

    @property
    def denominator(self):
        """list(float): Numerator coefficients."""
        addr = self.obj.coeffs.p_den.value
        coeffs = self._helper.chipdata.get_data(addr, self.nbytes)
        if self.conv is not None:
            coeffs = [self.conv(coeff) for coeff in coeffs]
        return coeffs

    def display(self):
        """Display the filter."""
        self._formatter.section_start("Filter: %s" % self.name)
        self._formatter.output("%d Coefficients (%d used)" % (
            self.full_num_coeffs, self.num_coeffs))
        format_list = "Numerator:\n"
        for value in self.numerator:
            format_list += "% 01.12f\n" % value
        format_list += "Denominator:\n"
        for value in self.denominator:
            format_list += "% 01.12f\n" % value

        self._formatter.output_raw(format_list)
        self._formatter.section_end()

class FxLMSv0102:
    """Encapsulates the FxLMS on capability versions 1.02+.

    Args:
        helper (ACAT.Analysis.Analysis): ACAT Analysis object.
        obj (ACAT.Core.CoreTypes.ChipdataVariable): FxLMS object.
        elf_id (int): ELF ID for AANC.
        formatter (ACAT.Display.Formatter, optional): ACAT formatter.
            Defaults to `None`.
    """
    def __init__(self, helper, obj, elf_id, formatter=None):
        """AANC FxLMSv0102 __init__ method."""
        self._helper = helper
        self.obj = obj
        """ACAT.Core.CoreTypes.ChipdataVariable: FxLMS object."""
        self.elf_id = elf_id
        """int: ELF ID for AANC."""
        self._formatter = formatter
        self.address = self.obj.address
        """int: Address of the FxLMS data object on the chip."""

        self.plant = FxLMSFilter(self._helper,
                                 self.obj.p_plant,
                                 "Plant (Q3.N)",
                                 self.elf_id,
                                 Q29,
                                 self._formatter)
        self.control0 = FxLMSFilter(self._helper,
                                    self.obj.p_control_0,
                                    "Control 0 (Q3.N)",
                                    self.elf_id,
                                    Q29,
                                    self._formatter)
        self.control1 = FxLMSFilter(self._helper,
                                    self.obj.p_control_1,
                                    "Control 1 (Q3.N)",
                                    self.elf_id,
                                    Q29,
                                    self._formatter)
        self.bpint = FxLMSFilter(self._helper,
                                 self.obj.p_bp_int,
                                 "Bandpass Int (Q1.N)",
                                 self.elf_id,
                                 Q31,
                                 self._formatter)
        self.bpext = FxLMSFilter(self._helper,
                                 self.obj.p_bp_ext,
                                 "Bandpass Ext (Q1.N)",
                                 self.elf_id,
                                 Q31,
                                 self._formatter)

        self.refresh()

    def refresh(self):
        """Refresh FxLMS data."""
        self.obj = self._helper.chipdata.cast(
            self.address, 'FXLMS100_DMX', elf_id=self.elf_id)
        self.plant.refresh()
        self.control0.refresh()
        self.control1.refresh()
        self.bpint.refresh()
        self.bpext.refresh()

    @property
    def gain(self):
        """int: Calculated gain value from the algorithm."""
        return self.obj.adaptive_gain.value

    @property
    def mu(self):  # pylint: disable=invalid-name
        """float: Mu value used by the algorithm."""
        return Q31(self.obj.mu.value)

    @property
    def gamma(self):  # pylint: disable=invalid-name
        """float: Gamma value used by the algorithm."""
        return Q31(self.obj.gamma.value)

    @property
    def lambda_param(self):  # pylint: disable=invalid-name
        """float: Lambda value used by the algorithm."""
        return Q30(self.obj['lambda'].value)

    @property
    def operating_config(self):
        """int: Operating configuration for the FxLMS algorithm."""
        return self.obj.configuration.value

    @property
    def licensed(self):
        """bool: License status."""
        return self.obj.licensed.value

    def display(self):
        """Display AANC flags using the ACAT formatter."""
        if self._formatter is None:
            raise Warning("No formatter found.")

        self._formatter.section_start("FxLMS at 0x%08X" % self.obj.address)
        try:
            self._formatter.output_raw("Gain:   %d" % self.gain)
            self._formatter.output_raw("Mu:     %.6f" % self.mu)
            self._formatter.output_raw("Gamma:  %.6f" % self.gamma)
            self._formatter.output_raw("Lambda: %.6f" % self.lambda_param)
            self._formatter.output_raw("Configuration: 0x%08X" %
                                       self.operating_config)
            self._formatter.output_raw("Licensed: %u" % self.licensed)
            self.plant.display()
            self.control0.display()
            self.control1.display()
            self.bpint.display()
            self.bpext.display()

        except (ValueError, AttributeError) as err:
            self._formatter.output("FxLMS Analysis failed: %s" % err)
        self._formatter.section_end()

    def __repr__(self):
        """str: String representation of key FxLMS variables."""
        return str(self)

    def __str__(self):
        """str: String representation of key FxLMS variables."""
        rstr = "FxLMS object at 0x%08X:\n" % self.obj.address
        rstr += "  Gain:  %d\n" % self.gain
        rstr += "  Mu:    %.6f\n" % self.mu
        rstr += "  Gamma: %.6f\n" % self.gamma
        rstr += "  Lamba: %.6f\n" % self.lambda_param
        rstr += "  Configuration: 0x%08X\n" % self.operating_config
        return rstr

class FxLMS(object):
    """Encapsulates the FxLMS.

    Args:
        helper (ACAT.Analysis.Analysis): ACAT Analysis object.
        obj (ACAT.Core.CoreTypes.ChipdataVariable): FxLMS object.
        stats (ACAT.Core.CoreTypes.ChipdataVariable): FxLMS statistics
            object.
        params (ACAT.Core.CoreTypes.ChipdataVariable): FxLMS parameters
            object.
        elf_id (int): ELF ID for AANC.
        formatter (ACAT.Display.Formatter, optional): ACAT formatter.
            Defaults to `None`.
    """

    def __init__(self, helper, obj, stats, params, elf_id, formatter=None):
        """AANC FxLMS __init__ method."""
        self._helper = helper
        self.obj = obj
        """ACAT.Core.CoreTypes.ChipdataVariable: FxLMS object."""
        self.stats = stats.deref
        """ACAT.Core.CoreTypes.ChipdataVariable: FxLMS statistics object."""
        self.params = params.deref
        """ACAT.Core.CoreTypes.ChipdataVariable: FxLMS parameters object."""
        self.elf_id = elf_id
        """int: ELF ID for AANC."""
        self.address = self.obj.value
        """int: Address of the FxLMS data object on the chip."""
        self._formatter = formatter

        self.refresh()

    def refresh(self):
        """Re-read values from the chip to refresh the cached data."""
        self.stats = self._helper.chipdata.cast(
            self.stats.address, 'FXLMS100_STATISTICS', elf_id=self.elf_id)

        self.params = self._helper.chipdata.cast(
            self.params.address, 'FXLMS100_PARAMETERS', elf_id=self.elf_id)

    @property
    def gain(self):
        """int: Calculated gain value from the algorithm."""
        return self.stats.adaptive_gain.value

    @property
    def mu(self):  # pylint: disable=invalid-name
        """float: Mu value used by the algorithm."""
        return Q31(self.params.mu.value)

    @property
    def gamma(self):  # pylint: disable=invalid-name
        """float: Gamma value used by the algorithm."""
        return Q31(self.params.gamma.value)

    @property
    def lambda_param(self):  # pylint: disable=invalid-name
        """float: Lambda value used by the algorithm."""
        return Q30(self.params['lambda'].value)

    @property
    def operating_config(self):
        """int: Operating configuration for the FxLMS algorithm."""
        if hasattr(self.params, 'configuration'):
            return self.params.configuration.value

        return None

    @property
    def licensed(self):
        """bool: License status."""
        return self.stats.licensed.value

    def read_coeffs(self, stat_name, stat_length_name, conv=None):
        """Read coefficients from the FxLMS statistics.

        Args:
            stat_name (str): Name of pointer to coefficients.
            stat_length_name (str): Name of the pointer to number of
                coefficients.
            conv (function): Conversion function for the final value e.g. Q27

        Returns:
            list(float): List of coefficients if the statistic is valid
                otherwise `None`.
        """
        if (hasattr(self.stats, stat_name) and
                hasattr(self.stats, stat_length_name)):
            addr = self.stats[stat_name].value
            # Check whether the number of coeffs is a pointer or just a value
            try:
                num = self.stats[stat_length_name].deref.value
            except VariableNotPointerError:
                num = self.stats[stat_length_name].value
            coeffs = self._helper.chipdata.get_data(addr, num * addr_per_word)
            if conv is None:
                return coeffs

            return [conv(value) for value in coeffs]

        return None

    @property
    def cp_numerator(self):
        """list(float): Control-plant numerator coefficients.

        These are the convolved plant-coefficients used in the algorithm.
        """
        return self.read_coeffs('p_cp_num_coeffs', 'num_cp_coeffs', Q27)

    @property
    def cp_denominator(self):
        """list(float): Control-plant denominator coefficients.

        These are the convolved plant-coefficients used in the algorithm.
        """
        return self.read_coeffs('p_cp_den_coeffs', 'num_cp_coeffs', Q27)

    @property
    def plant_numerator(self):
        """list(float): Plant numerator coefficients.

        In eANC the plant and control filters are separated in the algorithm.
        """
        return self.read_coeffs('p_plant_num_coeffs', 'p_num_plant_coeffs', Q29)

    @property
    def plant_denominator(self):
        """list(float): Plant denominator coefficients.

        In eANC the plant and control filters are separated in the algorithm.
        """
        return self.read_coeffs('p_plant_den_coeffs', 'p_num_plant_coeffs', Q29)

    @property
    def control0_numerator(self):
        """list(float): Control 0 numerator coefficients.

        In eANC there are up to 2x control filters used in the algorithm.
        """
        return self.read_coeffs('p_control_0_num_coeffs',
                                'p_num_control_0_coeffs',
                                Q29)

    @property
    def control0_denominator(self):
        """list(float): Control 0 denominator coefficients.

        In eANC there are up to 2x control filters used in the algorithm.
        """
        return self.read_coeffs('p_control_0_den_coeffs',
                                'p_num_control_0_coeffs',
                                Q29)

    @property
    def control1_numerator(self):
        """list(float): Control 1 numerator coefficients.

        In eANC there are up to 2x control filters used in the algorithm.
        """
        return self.read_coeffs('p_control_1_num_coeffs',
                                'p_num_control_1_coeffs',
                                Q29)

    @property
    def control1_denominator(self):
        """list(float): Control 1 denominator coefficients.

        In eANC there are up to 2x control filters used in the algorithm.
        """
        return self.read_coeffs('p_control_1_den_coeffs',
                                'p_num_control_1_coeffs',
                                Q29)

    def display_coeffs(self, coeff, name):
        """Display the selected coefficient using the formatter.

        Args:
            coeff (str): Coefficient to display.
            name (str): Title to give for the coefficients.
        """
        if getattr(self, coeff) is not None:
            self._formatter.section_start(name)
            format_list = ""
            for value in self.__getattribute__(coeff):
                format_list += "% 01.12f\n" % value
            self._formatter.output_raw(format_list)
            self._formatter.section_end()

    def display(self):
        """Display AANC flags using the ACAT formatter."""
        if self._formatter is None:
            raise Warning("No formatter found.")

        self._formatter.section_start("FxLMS at 0x%08X" % self.obj.address)
        try:
            self._formatter.output_raw("Gain:     %u" % self.gain)
            self._formatter.output_raw("Mu:       %.6f" % self.mu)
            self._formatter.output_raw("Gamma:    %.6f" % self.gamma)
            self._formatter.output_raw("Lambda:   %.6f" % self.lambda_param)
            self._formatter.output_raw("Licensed: %u" % self.licensed)

            if self.operating_config is not None:
                self._formatter.output_raw("Configuration: 0x%08X" %
                                           self.operating_config)

            self.display_coeffs('cp_numerator', "Control-Plant Numerator")
            self.display_coeffs('cp_denominator', "Control-Plant Denominator")
            self.display_coeffs('plant_numerator', "Plant Numerator")
            self.display_coeffs('plant_denominator', "Plant Denominator")
            self.display_coeffs('control0_numerator', "Control 0 Numerator")
            self.display_coeffs('control0_denominator', "Control 0 Denominator")
            self.display_coeffs('control1_numerator', "Control 1 Numerator")
            self.display_coeffs('control1_denominator', "Control 1 Denominator")

        except (ValueError, AttributeError) as err:
            self._formatter.output("FxLMS Analysis failed: %s" % err)
        self._formatter.section_end()

    def __repr__(self):
        """str: String representation of key FxLMS variables."""
        return str(self)

    def __str__(self):
        """str: String representation of key FxLMS variables."""
        rstr = "FxLMS object at 0x%08X:\n" % self.obj.address
        rstr += "  Gain:  %d\n" % self.gain
        rstr += "  Mu:    %.6f\n" % self.mu
        rstr += "  Gamma: %.6f\n" % self.gamma
        rstr += "  Lamba: %.6f\n" % self.lambda_param
        rstr += "  Control-Plant:\n"
        rstr += "    Numerator: %s\n" % self.cp_numerator
        rstr += "    Denominator: %s\n" % self.cp_denominator
        return rstr


class ED(object):
    """Encapsulates the Energy Detector.

    Args:
            helper (ACAT.Analysis.Analysis): ACAT Analysis object.
            obj (ACAT.Core.CoreTypes.ChipdataVariable): ED object.
            stats (ACAT.Core.CoreTypes.ChipdataVariable): ED statistics
                object.
            params (ACAT.Core.CoreTypes.ChipdataVariable): ED parameters
                object.
            elf_id (int): ELF ID for AANC.
            formatter (ACAT.Display.Formatter, optional): ACAT formatter.
                Defaults to `None`.
    """

    def __init__(self, helper, obj, stats=None, params=None, elf_id=None,
                 formatter=None):
        """ED __init__ method."""
        self._helper = helper
        self.obj = obj
        """ACAT.Core.CoreTypes.ChipdataVariable: ED object."""

        self.stats = stats
        """ACAT.Core.CoreTypes.ChipdataVariable: ED statistics object."""
        if self.stats is not None:
            self.stats = stats.deref

        self.params = params
        """ACAT.Core.CoreTypes.ChipdataVariable: ED parameters object."""
        if self.params is not None:
            self.params = params.deref

        self.elf_id = elf_id
        """int: ELF ID for AANC."""
        self.address = self.obj.address
        """int: Address of the ED data object in the chip."""
        self._formatter = formatter

        self.refresh()

    def refresh(self):
        """Re-read values from the chip to refresh the cached data."""
        if self.stats is None and self.params is None:
            self.obj = self._helper.chipdata.cast(
                self.address, 'ED100_DMX', elf_id=self.elf_id)
            return

        self.stats = self._helper.chipdata.cast(
            self.stats.address, 'ED100_STATISTICS', elf_id=self.elf_id)

        self.params = self._helper.chipdata.cast(
            self.params.address, 'ED100_PARAMETERS', elf_id=self.elf_id)

    @property
    def speech_detected(self):
        """bool: Speech detected state."""
        handle = self.stats
        if self.stats is None:
            handle = self.obj
        return handle.detection.value

    @property
    def spl(self):
        """float: SPL value in dB."""
        handle = self.stats
        if self.stats is None:
            handle = self.obj
        return Q20(handle.spl.value)

    @property
    def spl_mid(self):
        """float: SPL mid value in dB."""
        handle = self.stats
        if self.stats is None:
            handle = self.obj
        return Q20(handle.spl_mid.value)

    @property
    def spl_max(self):
        """float: SPL max value in dB."""
        handle = self.stats
        if self.stats is None:
            handle = self.obj
        return Q20(handle.spl_max.value)

    @property
    def delta_spl(self):
        """float: Delta SPL value in dB."""
        return self.spl_max - self.spl

    @property
    def licensed(self):
        """bool: License state."""
        handle = self.stats
        if self.stats is None:
            handle = self.obj
        return handle.licensed.value

    def display(self, line_only=False):
        """Display AANC flags using the ACAT formatter.

        This function assumes that a table has been defined with columns for
        each value in the formatter.
        """
        if self._formatter is None:
            raise Warning("No formatter found.")

        if line_only:
            lstr = "".join([
                "0x%08X |",
                "     %d    |",
                " % 6.2f |",
                " % 6.2f |",
                " % 6.2f |",
                " % 6.2f |"])
            try:
                return lstr % (self.obj.address,
                               self.speech_detected,
                               self.spl,
                               self.spl_mid,
                               self.spl_max,
                               self.delta_spl)
            except (ValueError, AttributeError) as err:
                return "ED Analysis failed: %s\n" % err

        self._formatter.section_start("ED at 0x%08X" % self.obj.address)
        try:
            self._formatter.output("Speech Detected:  %d" %
                                   self.speech_detected)
            self._formatter.output("SPL:       % 6.2f dB" % self.spl)
            self._formatter.output("SPL mid:   % 6.2f dB" % self.spl_mid)
            self._formatter.output("SPL max:   % 6.2f dB" % self.spl_max)
            self._formatter.output("SPL delta: % 6.2f dB" % self.delta_spl)
        except (ValueError, AttributeError) as err:
            self._formatter.output("ED Analysis failed: %s" % err)
        self._formatter.section_end()
        return None

    def __repr__(self):
        """str: String representation of key ED variables."""
        return str(self)

    def __str__(self):
        """str: String representation of key ED variables."""
        rstr = "ED object at 0x%08X:\n" % self.obj.address
        rstr += "  Speech Detected:  %d\n" % self.speech_detected
        rstr += "  SPL:       % .2f\n" % self.spl
        rstr += "  SPL mid:   % .2f dB\n" % self.spl_mid
        rstr += "  SPL max:   % .2f dB\n" % self.spl_max
        rstr += "  SPL delta: %  2.2f dB\n" % self.delta_spl
        return rstr
