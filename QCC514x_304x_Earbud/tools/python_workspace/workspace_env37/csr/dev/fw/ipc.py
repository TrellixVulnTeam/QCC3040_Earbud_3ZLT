############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.model.interface import Group, Table, Code
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.env.env_helpers import _Variable
from csr.dwarf.read_dwarf import DwarfNoSymbol
from csr.wheels.bitsandbobs import bytes_to_dwords
from .structs import IAppsStructHandler

class IPC(FirmwareComponent):
    """
    Provide debugging access to the IPC mechanism
    """

    @staticmethod
    def create(fw_env, core):
        """
        Create an IPC object if possible.  If we're running against old firmware
        that doesn't support it, then it won't be.  We detect this by the
        failure to find key symbols in the firmware.
        """
        try:
            return IPC(fw_env, core)
        except DwarfNoSymbol:
            return None

    def buf_msg_decoder(self, msg_buf, ring_entry, start_index, msg):
        """
        Decoder to register for IPC's BUFFER_MSGs with the BufferMsg struct
        handler class
        """
        return self._decode_msg(msg, join=False)
        
    def buf_decoder(self, data):
        return self._decode(data, join=False)
    
    def __init__(self, fw_env, core):
        
        FirmwareComponent.__init__(self, fw_env, core)

        self._ipc_msg_names_to_ids = fw_env.enums["IPC_SIGNAL_ID"]
        self._ipc_msg_ids_to_names = dict((id, name) for (name, id) in 
                                          self._ipc_msg_names_to_ids.items())
        self._ipc_struct = self.env.gbl.ipc_data

        self._buffer_type = self._ipc_struct.send.deref.typename
        self._send_buf = IAppsStructHandler.handler_factory(self._buffer_type)(
                                              core,
                                              self._ipc_struct.send.deref)
        self._recv_buf = IAppsStructHandler.handler_factory(self._buffer_type)(
                                              core,
                                              self._ipc_struct.recv.deref)

        try:
            self._send_buf.set_msg_decoder(self.buf_msg_decoder)
            self._recv_buf.set_msg_decoder(self.buf_msg_decoder)
        except AttributeError:
            self._send_buf.set_decoder(self.buf_decoder)
            self._recv_buf.set_decoder(self.buf_decoder)

        # We need to look up trap definitions info in P1's DWARF
        try:
            self._p1_dwarf = self._core.subsystem.p1.fw.env.dwarf
        except AttributeError:
            self._p1_dwarf = None

    @property
    def title(self):
        return "IPC"
        
    def show_recv(self):
        """
        Decode recent entries in the IPC receive buffer
        """
        self._recv_buf.display()

    def show_send(self):
        """
        Decode recent entries in the IPC send buffer
        """
        self._send_buf.display()

    def _generate_report_body_elements(self):
        if self._buffer_type == "BUFFER_MSG":
            return self._generate_buffer_msg_report_body_elements()
        else:
            return self._generate_buffer_report_body_elements()

    def _generate_buffer_msg_report_body_elements(self):
        """
        Decode all the recent IPC messages that are still visible
        """
        reports = []
        # 1. Grab messages
        for dir in ("send", "recv"):
            ipc_dir = Group(dir)

            grp_cleared = Group("cleared")
            ipc_dir.append(grp_cleared)
            buf = getattr(self, "_%s_buf" % dir)
            for msg in buf.still_mapped_msgs[2]:
                grp_cleared.append(Code(self._decode_msg(msg, join=True)))

            grp_read = Group("read")
            ipc_dir.append(grp_read)
            for msg in buf.read_msgs[2]:
                grp_read.append(Code(self._decode_msg(msg, join=True)))

            grp_unread = Group("unread")
            ipc_dir.append(grp_unread)
            for msg in buf.unread_msgs[2]:
                grp_unread.append(Code(self._decode_msg(msg, join=True)))

            reports.append(ipc_dir)
        return reports

    def _generate_buffer_report_body_elements(self):
        """
        Decode all the recent IPC messages that are still visible
        """
        reports = []
        # 1. Grab messages
        for dir in ("send", "recv"):
            ipc_dir = Group(dir)
            buf = getattr(self, "_%s_buf" % dir)

            grp_cleared = Group("cleared")
            ipc_dir.append(grp_cleared)
            grp_cleared.append(Code(self._decode(buf.cleared_and_mapped, join=True)))

            grp_read = Group("read")
            ipc_dir.append(grp_read)
            grp_read.append(Code(self._decode(buf.read, join=True)))

            grp_unread = Group("unread")
            ipc_dir.append(grp_unread)
            grp_unread.append(Code(self._decode(buf.unread, join=True)))

            reports.append(ipc_dir)
        return reports

    def _looks_like_ipc_message(self, data):
        # IPC messages are at least 8 bytes long, signal ID + length
        if len(data) < 8:
            return False
        
        header = self._as_prim_header(data)
        return header.id.value in self._ipc_msg_ids_to_names and \
               header.length_bytes.value <= len(data) and \
               header.length_bytes.value >= 8  and \
               (header.length_bytes.value % 4) == 0

    def _looks_like_ipc_message_chain(self, data):
        messages = 0
        while self._looks_like_ipc_message(data):
            header = self._as_prim_header(data)
            data = data[header.length_bytes.value:]
            messages += 1
            
        return len(data) == 0 and messages > 0

    def _decode(self, data, join=False):
        """
        Interpret the supplied list of bytes as an array of IPC primitives
        that may not necessarily start from the beginning of the list.
        """
        while data and not self._looks_like_ipc_message_chain(data):
            # This doesn't look like the start of an IPC message
            # drop 4 bytes (IPC messages are 4 byte aligned) and continue
            # searching.
            data = data[4:]

        display_lines = []
        while data:
            header = self._as_prim_header(data)
            encoded_msg = data[0:header.length_bytes.value]
            decoded_msg = self._decode_msg(encoded_msg, join=True)
            display_lines.append(decoded_msg)
            data = data[header.length_bytes.value:]
            
        if join:
            return "\n".join(display_lines)
        return display_lines

    def _as_prim_header(self, data):
        return _Variable.create_from_type(
                            self.env.types["IPC_HEADER"],
                            0, data, self._core.info.layout_info,
                            ptd_to_space=False)
    
    def _msg_id_from_msg(self, data):
        return self._core.info.layout_info.deserialise(data[0:4])
        
    def _msg_name_to_struct_name(self, msg_name):
        struct_name = msg_name.replace("_SIGNAL_ID", "")
        if struct_name in ("IPC_TEST_TUNNEL_PRIM",):
            struct_name = "IPC_TUNNELLED_PRIM_OUTBAND"
        elif struct_name in ("IPC_PFREE", "IPC_SFREE"):
            struct_name = "IPC_FREE"
        elif struct_name in ("IPC_BLUESTACK_PRIM_RECEIVED",):
            struct_name = "IPC_BLUESTACK_PRIM"
        elif struct_name in ("IPC_APP_SINK_MSG",
                             "IPC_APP_SINK_SOURCE_MSG",
                             "IPC_APP_MSG",
                             "IPC_P1_DEEP_SLEEP_MSG",
                             "IPC_APP_MSG_TO_HANDLER"):
            struct_name += "_PRIM"
        elif struct_name == "IPC_XIO_RSP":
            struct_name = "IPC_BOOL_RSP"
        elif struct_name == "IPC_TRAP_API_VERSION":
            struct_name = "IPC_TRAP_API_VERSION_INFO"
        elif struct_name == "IPC_IPC_LEAVE_RECV_BUFFER_PAGES_MAPPED":
            struct_name = "IPC_SIGNAL"
        elif struct_name == "IPC_FAULT":
            struct_name = "IPC_FAULT_PANIC"
        elif struct_name.startswith("IPC_PIO_"):
            struct_name = "IPC_PIO"

        try:
            type = self.env.types[struct_name]
        except DwarfNoSymbol:
            # Probably indicates a response type that is derived from the
            # trap's return value rather than its name
            
            if self._p1_dwarf is None:
                # We *could* look up the info via the XML but if p1's firmware
                # isn't available we're probably not bothered about IPC so
                # there seems little point
                raise
            
            # Construct the trap function's name
            func_name_cmpts = (struct_name.replace("IPC_","")
                                          .replace("_RSP","")
                                          .split("_"))
            name = ""
            for cmpt in func_name_cmpts:
                name += (cmpt[0] + cmpt[1:].lower())
            try:
                func = self._p1_dwarf.get_function(name)
                if func.return_type is not None:
                    struct_name = "IPC_%s_RSP" % (func.return_type.struct_dict[
                                                     "type_name"].upper())
                else:
                    struct_name = "IPC_VOID_RSP"
                type = self.env.types[struct_name]
            except DwarfNoSymbol:
                struct_name = None
        return struct_name
    
    def _as_prim(self, msg):
        """
        Interpret the supplied list of bytes as an IPC primitive
        """
        msg_id = self._msg_id_from_msg(msg)
        try:
            message_name = self._ipc_msg_ids_to_names[msg_id]
            struct_name = self._msg_name_to_struct_name(message_name)
            type = self.env.types[struct_name]
            return _Variable.create_from_type(
                            type,
                            0, msg, self._core.info.layout_info,
                            ptd_to_space=False)
        except (KeyError, DwarfNoSymbol):
            pass
        return None
    
    def _decode_msg(self, msg, join=False):
        decoded_list = self._decode_msg_to_list(msg)
        if join:
            return "\n".join(decoded_list)
        return decoded_list
    
    def _decode_msg_to_list(self, msg):
        """
        Interpret the supplied list of bytes as an IPC primitive
        """
        msg_id = self._msg_id_from_msg(msg)
        if msg_id is None:
            return ["%s <Not decoded>" % msg]
 
        message_name = self._ipc_msg_ids_to_names.get(msg_id)
        if message_name is None:
            return ["%s <Not decoded>" % msg]
        
        struct_name = self._msg_name_to_struct_name(message_name)
        if struct_name is None:
            return ["%s %s" % (message_name, msg[4:])]

        type = self.env.types[struct_name]
        prim = _Variable.create_from_type(
                            type,
                            0, msg, self._core.info.layout_info,
                            ptd_to_space=False)

        return prim.display("IPC msg", "", [], [])
    
    def mmu_handles(self):
        """
        Returns the MMU handles owned by this module
        """
        results = []
        for dir in ("send", "recv"):
            bufdir = self.env.gv["ipc_data"][dir]
            try:
                # Newer FW versions use BUFFER rather than BUFFER_MSG
                handle = bufdir.deref["handle"]
            except KeyError:
                handle = bufdir.deref["buf"]["handle"]
            handles = [handle.value & 0xff]
            results.append(["ipc_data ({})".format(dir), handles])

        return results
